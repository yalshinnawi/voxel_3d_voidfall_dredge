#include "world.hpp"
#include "structural_check.hpp"
#include "fluid_sim.hpp"
#include "../graphics/surface_clutter.hpp"
#include "../graphics/particle_system.hpp"
#include "../core/save_system.hpp"
#include "../core/logger.hpp"
#include <glm/gtc/matrix_access.hpp>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <ctime>

namespace Voidfall {

static inline int floor_div(int a, int b) {
    if (b == CHUNK_SIZE) return a >> 5;
    int res = a / b;
    int rem = a % b;
    if (rem != 0 && ((a < 0) ^ (b < 0))) {
        res--;
    }
    return res;
}

static inline int floor_mod(int a, int b) {
    if (b == CHUNK_SIZE) return a & (CHUNK_SIZE - 1);
    int res = a % b;
    if (res < 0) {
        res += b;
    }
    return res;
}

World::World(uint32_t seed, bool enable_background_meshing)
    : m_seed(seed), m_enable_background_meshing(enable_background_meshing)
{
    FluidSim::SetActiveWorld(this);

    const char* env_headless = std::getenv("VOIDFALL_HEADLESS_TEST");
    if (env_headless && std::strcmp(env_headless, "1") == 0) {
        m_enable_background_meshing = false;
    }

    m_level_gen = std::make_unique<LevelGenerator>(1, seed);

    if (m_enable_background_meshing) {
        // Start background meshing worker threads
        unsigned int num_threads = std::max(2u, std::thread::hardware_concurrency() / 2);
        for (unsigned int i = 0; i < num_threads; ++i) {
            m_workers.emplace_back(&World::worker_thread_loop, this);
        }
    }
}

World::~World() {
    if (FluidSim::s_activeWorld == this) {
        FluidSim::SetActiveWorld(nullptr);
    }
    m_running.store(false);
    m_queue_cv.notify_all();
    for (auto& t : m_workers) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void World::set_seed(uint32_t seed) {
    generate_world(m_sector_index, seed);
}

void World::generate_world(int sector_index, uint32_t seed, int target_objective) {
    m_sector_index = sector_index;
    uint32_t effective_seed = (seed != 0) ? seed : SaveSystem::generate_random_seed(sector_index);
    m_seed = effective_seed;
    m_level_gen = std::make_unique<LevelGenerator>(sector_index, effective_seed, 1, target_objective);

    if (m_level_gen) {
        VF_LOG_INFO("World", "Generated sector " << sector_index << " (seed " << effective_seed << "): "
                    << "Objective Target=" << m_level_gen->target_objective_amount()
                    << ", Spawned=" << m_level_gen->total_objective_material_count()
                    << ", Room Spread=" << m_level_gen->objective_room_spread_count()
                    << ", Zero-Resource Rooms=" << m_level_gen->rooms_with_zero_objective_count()
                    << ", Completable=" << (m_level_gen->validate_resource_compleatability() ? "YES" : "NO"));
    }

    // Flush stale mesh queue requests
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        std::queue<ChunkPos> empty_queue;
        std::swap(m_mesh_queue, empty_queue);
        m_mesh_queued_set.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_upload_mutex);
        std::queue<std::unique_ptr<ChunkMeshData>> empty_queue;
        std::swap(m_pendingUploads, empty_queue);
        m_upload_queued_set.clear();
        m_pending_upload_map.clear();
    }

    std::vector<ChunkPos> to_mesh;
    {
        std::lock_guard<std::mutex> lock(m_world_mutex);
        m_chunks.clear();
        m_radioactive_sources.clear();
        m_toxic_gas_sources.clear();
        m_lava_sources.clear();
        m_aquifer_sources.clear();

        int chunk_count_x = (m_level_gen->world_width() + CHUNK_SIZE - 1) / CHUNK_SIZE;
        int chunk_count_y = (m_level_gen->world_height() + CHUNK_SIZE - 1) / CHUNK_SIZE;
        int chunk_count_z = (m_level_gen->world_depth() + CHUNK_SIZE - 1) / CHUNK_SIZE;
        for (int cz = 0; cz < chunk_count_z; ++cz) {
            for (int cy = 0; cy < chunk_count_y; ++cy) {
                for (int cx = 0; cx < chunk_count_x; ++cx) {
                    ChunkPos pos{cx, cy, cz};
                    auto chunk = std::make_shared<Chunk>(pos);
                    generate_chunk_terrain(*chunk);
                    m_chunks[pos] = std::move(chunk);
                    to_mesh.push_back(pos);
                }
            }
        }
    }

    m_playerSpawnPos = (m_level_gen) ? m_level_gen->spawn_position() : glm::vec3(16.0f, 5.1f, 16.0f);
    GenerateSectorStructures(sector_index);
    for (auto& [cpos, chunk] : m_chunks) {
        if (chunk) {
            ApplyTopologicalShapes(*chunk);
        }
    }
    ensure_structural_connectivity();
    for (auto& [cpos, chunk] : m_chunks) {
        if (chunk) {
            GenerateChunkClutter(*chunk);
        }
    }
    for (auto& [cpos, chunk] : m_chunks) {
        if (chunk) {
            OnChunkGenerated(chunk.get());
        }
    }
    settle_fluids_offline();
    PopulateFauna();
    m_playerSpawnYaw = calculate_spawn_yaw(m_playerSpawnPos);

    if (m_enable_background_meshing) {
        for (const auto& pos : to_mesh) {
            queue_chunk_for_meshing(pos);
        }
    }
}

constexpr float SPAWN_SAFE_RADIUS = 28.0f; // Minimum meters from insertion point

static inline bool IsSpawnPointSafe(const glm::vec3& candidatePos, const glm::vec3& playerSpawnPos) {
    return glm::distance(candidatePos, playerSpawnPos) >= SPAWN_SAFE_RADIUS;
}

void World::PopulateFauna() {
    m_active_entities.clear();
    if (!m_level_gen) return;

    const auto& rooms = m_level_gen->rooms();
    uint32_t next_id = 1;

    auto find_valid_room_spawn = [&](const glm::vec3& approx_pos) -> std::optional<glm::vec3> {
        int cx = static_cast<int>(std::floor(approx_pos.x));
        int cy = static_cast<int>(std::floor(approx_pos.y));
        int cz = static_cast<int>(std::floor(approx_pos.z));

        // Quick check exact position
        Voxel v_feet = get_voxel(cx, cy, cz);
        Voxel v_head = get_voxel(cx, cy + 1, cz);
        Voxel v_below = get_voxel(cx, cy - 1, cz);
        if (!v_feet.is_solid() && !v_head.is_solid() && v_below.is_solid() && IsSpawnPointSafe(approx_pos, m_playerSpawnPos)) {
            return approx_pos;
        }

        // Search in spiral around approx_pos within room
        for (int r = 1; r <= 6; ++r) {
            for (int dx = -r; dx <= r; ++dx) {
                for (int dz = -r; dz <= r; ++dz) {
                    if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                    for (int dy = 2; dy >= -3; --dy) {
                        int tx = cx + dx;
                        int ty = cy + dy;
                        int tz = cz + dz;
                        if (tx < 5 || tx > 66 || tz < 5 || tz > 66 || ty < 3 || ty > 25) continue;
                        Voxel f_v = get_voxel(tx, ty, tz);
                        Voxel h_v = get_voxel(tx, ty + 1, tz);
                        Voxel b_v = get_voxel(tx, ty - 1, tz);
                        if (!f_v.is_solid() && !h_v.is_solid() && b_v.is_solid()) {
                            glm::vec3 candidate(static_cast<float>(tx) + 0.5f,
                                                static_cast<float>(ty) + 0.1f,
                                                static_cast<float>(tz) + 0.5f);
                            if (IsSpawnPointSafe(candidate, m_playerSpawnPos)) {
                                return candidate;
                            }
                        }
                    }
                }
            }
        }
        return std::nullopt;
    };

    constexpr float MIN_FAUNA_SPAWN_SEPARATION = 8.0f;

    for (size_t i = 1; i < rooms.size(); ++i) { // Room 0 is player insertion arrival bay
        glm::vec3 room_center(
            static_cast<float>(rooms[i].center.x),
            static_cast<float>(rooms[i].floor_y) + 1.2f,
            static_cast<float>(rooms[i].center.z)
        );

        auto safe_pos = find_valid_room_spawn(room_center);
        if (!safe_pos) {
            continue;
        }

        // Enforce minimum spatial separation from all already-placed fauna entities
        bool too_close = false;
        for (const auto& existing : m_active_entities) {
            if (glm::distance(existing.pos, *safe_pos) < MIN_FAUNA_SPAWN_SEPARATION) {
                too_close = true;
                break;
            }
        }
        if (too_close) {
            continue;
        }

        StalkerRole role = (i % 2 == 0) ? StalkerRole::Melee : StalkerRole::Shooter;
        FaunaEntity e;
        e.id = next_id++;
        e.pos = *safe_pos;
        e.position = *safe_pos;
        e.state = AIState::ROOSTING;
        e.role = role;
        m_active_entities.push_back(e);

        if (m_sector_index >= 2 && (i % 2 == 0)) {
            glm::vec3 sec_candidate = *safe_pos + glm::vec3(8.5f, 0.0f, -8.5f);
            auto sec_safe = find_valid_room_spawn(sec_candidate);
            if (sec_safe) {
                bool sec_too_close = false;
                for (const auto& existing : m_active_entities) {
                    if (glm::distance(existing.pos, *sec_safe) < MIN_FAUNA_SPAWN_SEPARATION) {
                        sec_too_close = true;
                        break;
                    }
                }
                if (!sec_too_close) {
                    FaunaEntity e2;
                    e2.id = next_id++;
                    e2.pos = *sec_safe;
                    e2.position = *sec_safe;
                    e2.state = AIState::ROOSTING;
                    e2.role = StalkerRole::Melee;
                    m_active_entities.push_back(e2);
                }
            }
        }
    }

    // Procedural Precursor Vault Guardian / Boss Monster Spawn
    if (m_level_gen && m_level_gen->has_vault() && m_level_gen->vault_placement().has_guardian) {
        const auto& vp = m_level_gen->vault_placement();
        FaunaEntity guardian;
        guardian.id = next_id++;
        guardian.pos = vp.guardian_spawn_pos;
        guardian.position = vp.guardian_spawn_pos;
        guardian.state = AIState::ROOSTING;
        if (vp.guardian_role == VaultGuardianRole::ChitinGoliath) {
            guardian.role = StalkerRole::ChitinGoliath;
        } else if (vp.guardian_role == VaultGuardianRole::StalkerShooter) {
            guardian.role = StalkerRole::Shooter;
        } else {
            guardian.role = StalkerRole::Melee;
        }
        m_active_entities.push_back(guardian);
    }
}

void World::GenerateSectorStructures(int sector_index) {
    if (m_level_gen && m_level_gen->has_vault()) {
        const auto& vp = m_level_gen->vault_placement();
        m_vault_door = VaultDoor(vp.door_pos, vp.door_height);
        m_vault_door.set_relic_position(vp.relic_pos);
        return;
    }

    bool should_generate = (sector_index >= 2) || ((m_seed % 100) < 30);
    if (!should_generate) {
        m_vault_door = VaultDoor{};
        return;
    }

    // Anchor vault into deep rock
    int vx = (sector_index == 1) ? 46 : (sector_index == 2 ? 40 : 48);
    int vy = 12;
    int vz = (sector_index == 1) ? 44 : (sector_index == 2 ? 48 : 38);

    // 7x5x7 sealed chamber:
    // x in [vx - 3, vx + 3] (7 blocks wide)
    // y in [vy, vy + 4] (5 blocks high)
    // z in [vz - 3, vz + 3] (7 blocks deep)
    for (int x = vx - 3; x <= vx + 3; ++x) {
        for (int y = vy; y <= vy + 4; ++y) {
            for (int z = vz - 3; z <= vz + 3; ++z) {
                // Do not seal or obstruct existing open corridors, doorways, or room walkways
                Voxel existing = get_voxel(x, y, z);
                if (existing.material_id == MAT_AIR ||
                    existing.material_id == MAT_VOLCANIC_BASALT ||
                    existing.material_id == MAT_INDUSTRIAL_BULKHEAD ||
                    existing.material_id == MAT_REINFORCED_VAULT_DOOR) {
                    continue;
                }

                bool is_boundary = (x == vx - 3 || x == vx + 3 ||
                                    y == vy || y == vy + 4 ||
                                    z == vz - 3 || z == vz + 3);
                if (is_boundary) {
                    set_voxel(x, y, z, Voxel{MAT_PRECURSOR_STONE, VOXEL_FLAG_ANCHORED}, false);
                } else {
                    set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }
    }

    // Sealed by 3-block-tall MAT_VAULT_DOOR at entry portal
    glm::ivec3 door_pos(vx - 3, vy + 1, vz);
    for (int dy = 0; dy < 3; ++dy) {
        set_voxel(door_pos.x, door_pos.y + dy, door_pos.z, Voxel{MAT_VAULT_DOOR, VOXEL_FLAG_ANCHORED}, false);
    }
    m_vault_door = VaultDoor(door_pos, 3);
    m_vault_door.set_relic_position(glm::ivec3(vx, vy + 1, vz));

    // Floating Relic Hyper-Core Pedestal in vault center (dynamic animated 3D model)
    set_voxel(vx, vy, vz, Voxel{MAT_PRECURSOR_STONE, VOXEL_FLAG_ANCHORED}, false);
    set_voxel(vx, vy + 1, vz, Voxel{MAT_PRISMATIC_CRYSTAL, static_cast<uint8_t>(VOXEL_FLAG_EMISSIVE | VOXEL_FLAG_CUSTOM_MODEL)}, false);
}

void World::set_level_generator(std::unique_ptr<LevelGenerator> gen) {
    // Flush stale mesh queue requests
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        std::queue<ChunkPos> empty_queue;
        std::swap(m_mesh_queue, empty_queue);
        m_mesh_queued_set.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_upload_mutex);
        std::queue<std::unique_ptr<ChunkMeshData>> empty_queue;
        std::swap(m_pendingUploads, empty_queue);
        m_upload_queued_set.clear();
        m_pending_upload_map.clear();
    }

    std::vector<ChunkPos> to_mesh;
    {
        std::lock_guard<std::mutex> lock(m_world_mutex);
        if (gen) {
            m_sector_index = gen->sector_index();
            m_seed = gen->seed();
        }
        m_level_gen = std::move(gen);
        m_chunks.clear();
        m_radioactive_sources.clear();
        m_toxic_gas_sources.clear();
        m_lava_sources.clear();
        m_aquifer_sources.clear();

        int chunk_count_x = m_level_gen ? ((m_level_gen->world_width() + CHUNK_SIZE - 1) / CHUNK_SIZE) : 3;
        int chunk_count_y = m_level_gen ? ((m_level_gen->world_height() + CHUNK_SIZE - 1) / CHUNK_SIZE) : 1;
        int chunk_count_z = m_level_gen ? ((m_level_gen->world_depth() + CHUNK_SIZE - 1) / CHUNK_SIZE) : 3;
        for (int cz = 0; cz < chunk_count_z; ++cz) {
            for (int cy = 0; cy < chunk_count_y; ++cy) {
                for (int cx = 0; cx < chunk_count_x; ++cx) {
                    ChunkPos pos{cx, cy, cz};
                    auto chunk = std::make_shared<Chunk>(pos);
                    generate_chunk_terrain(*chunk);
                    m_chunks[pos] = std::move(chunk);
                    to_mesh.push_back(pos);
                }
            }
        }
    }

    m_playerSpawnPos = (m_level_gen) ? m_level_gen->spawn_position() : glm::vec3(16.0f, 5.1f, 16.0f);
    GenerateSectorStructures(m_sector_index);
    for (auto& [cpos, chunk] : m_chunks) {
        if (chunk) {
            ApplyTopologicalShapes(*chunk);
        }
    }
    ensure_structural_connectivity();
    for (auto& [cpos, chunk] : m_chunks) {
        if (chunk) {
            OnChunkGenerated(chunk.get());
        }
    }
    settle_fluids_offline();
    PopulateFauna();
    m_playerSpawnYaw = calculate_spawn_yaw(m_playerSpawnPos);

    for (const auto& pos : to_mesh) {
        queue_chunk_for_meshing(pos);
    }
}

Chunk* World::get_chunk(const ChunkPos& pos) {
    std::lock_guard<std::mutex> lock(m_world_mutex);
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Chunk* World::get_chunk(const ChunkPos& pos) const {
    std::lock_guard<std::mutex> lock(m_world_mutex);
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

Chunk* World::GetChunkFromBlockPos(const glm::ivec3& blockPos) {
    int cx = floor_div(blockPos.x, CHUNK_SIZE);
    int cy = floor_div(blockPos.y, CHUNK_SIZE);
    int cz = floor_div(blockPos.z, CHUNK_SIZE);
    return get_chunk(ChunkPos{cx, cy, cz});
}

const Chunk* World::GetChunkFromBlockPos(const glm::ivec3& blockPos) const {
    int cx = floor_div(blockPos.x, CHUNK_SIZE);
    int cy = floor_div(blockPos.y, CHUNK_SIZE);
    int cz = floor_div(blockPos.z, CHUNK_SIZE);
    return get_chunk(ChunkPos{cx, cy, cz});
}

void World::OnChunkGenerated(Chunk* chunk) {
    if (!chunk || chunk->is_empty()) return;

    ChunkPos cpos = chunk->get_pos();
    int bx = cpos.x * CHUNK_SIZE;
    int by = cpos.y * CHUNK_SIZE;
    int bz = cpos.z * CHUNK_SIZE;

    const glm::ivec3 flowDirs[5] = {
        { 0,-1, 0},
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                Voxel vox = chunk->get_voxel(x, y, z);
                if (!vox.is_liquid() && !vox.is_waterlogged()) {
                    continue;
                }

                glm::ivec3 worldPos(bx + x, by + y, bz + z);
                bool hasAirNeighbor = false;
                for (const auto& dir : flowDirs) {
                    int nx = x + dir.x;
                    int ny = y + dir.y;
                    int nz = z + dir.z;
                    uint8_t nbMat = MAT_AIR;
                    if (Chunk::in_bounds(nx, ny, nz)) {
                        nbMat = chunk->get_voxel(nx, ny, nz).material_id;
                    } else {
                        nbMat = get_voxel(worldPos.x + dir.x, worldPos.y + dir.y, worldPos.z + dir.z).material_id;
                    }
                    if (nbMat == MAT_AIR) {
                        hasAirNeighbor = true;
                        break;
                    }
                }

                if (hasAirNeighbor) {
                    m_fluidSim.WakeFluid(worldPos);
                }
            }
        }
    }
}

Chunk* World::get_or_create_chunk(const ChunkPos& pos) {
    if (pos.y < 0 || pos.y > 16) {
        return nullptr;
    }

    Chunk* raw = nullptr;
    bool created = false;
    {
        std::lock_guard<std::mutex> lock(m_world_mutex);
        auto it = m_chunks.find(pos);
        if (it != m_chunks.end()) {
            return it->second.get();
        }

        auto chunk = std::make_shared<Chunk>(pos);
        generate_chunk_terrain(*chunk);
        raw = chunk.get();
        m_chunks[pos] = std::move(chunk);
        created = true;
    }

    if (created && raw) {
        ApplyTopologicalShapes(*raw);
        queue_chunk_for_meshing(pos);
        OnChunkGenerated(raw);
    }
    return raw;
}

Voxel World::get_voxel(int world_x, int world_y, int world_z) const {
    ChunkPos cpos{
        floor_div(world_x, CHUNK_SIZE),
        floor_div(world_y, CHUNK_SIZE),
        floor_div(world_z, CHUNK_SIZE)
    };

    const Chunk* chunk = get_chunk(cpos);
    if (!chunk) {
        if (m_level_gen) {
            return m_level_gen->sample_voxel(world_x, world_y, world_z);
        }
        return Voxel{MAT_AIR, 0};
    }

    int lx = floor_mod(world_x, CHUNK_SIZE);
    int ly = floor_mod(world_y, CHUNK_SIZE);
    int lz = floor_mod(world_z, CHUNK_SIZE);
    return chunk->get_voxel(lx, ly, lz);
}

bool World::set_voxel(int world_x, int world_y, int world_z, Voxel v, bool mark_neighbors) {
    ChunkPos cpos{
        floor_div(world_x, CHUNK_SIZE),
        floor_div(world_y, CHUNK_SIZE),
        floor_div(world_z, CHUNK_SIZE)
    };

    Chunk* chunk = get_or_create_chunk(cpos);
    if (!chunk) return false;

    int lx = floor_mod(world_x, CHUNK_SIZE);
    int ly = floor_mod(world_y, CHUNK_SIZE);
    int lz = floor_mod(world_z, CHUNK_SIZE);

    Voxel old_vox = chunk->get_voxel(lx, ly, lz);
    chunk->set_voxel(lx, ly, lz, v);

    // Track active radioactive ore sources dynamically
    bool old_is_rad = (old_vox.material_id == MAT_RADIOACTIVE_ORE || old_vox.material_id == MAT_RADIOACTIVE);
    bool new_is_rad = (v.material_id == MAT_RADIOACTIVE_ORE || v.material_id == MAT_RADIOACTIVE);
    if (old_is_rad != new_is_rad) {
        std::lock_guard<std::mutex> lock(m_world_mutex);
        if (old_is_rad && !new_is_rad) {
            auto it = std::find(m_radioactive_sources.begin(), m_radioactive_sources.end(), glm::ivec3(world_x, world_y, world_z));
            if (it != m_radioactive_sources.end()) {
                m_radioactive_sources.erase(it);
            }
        } else if (!old_is_rad && new_is_rad) {
            m_radioactive_sources.push_back(glm::ivec3(world_x, world_y, world_z));
        }
    }

    // Anchor Dependency & Auto-Despawn:
    // When a solid voxel breaks or is cleared to air/liquid, clear all decorative clutter anchored to its faces
    if (old_vox.is_solid() && (!v.is_solid() || v.material_id == MAT_AIR)) {
        clear_disconnected_clutter(glm::ivec3(world_x, world_y, world_z));

        // Dynamic Organic Crystal Spawning:
        // When a solid block breaks, any adjacent crystal deposit has its newly exposed surface sprout a 3D faceted cluster!
        const glm::ivec3 adj_dirs[6] = {
            { 1, 0, 0}, {-1, 0, 0},
            { 0, 1, 0}, { 0,-1, 0},
            { 0, 0, 1}, { 0, 0,-1}
        };
        for (int d = 0; d < 6; ++d) {
            glm::ivec3 n_pos = glm::ivec3(world_x, world_y, world_z) + adj_dirs[d];
            Voxel n_vox = get_voxel(n_pos.x, n_pos.y, n_pos.z);
            if (n_vox.material_id == MAT_VOIDITE_CRYSTAL || n_vox.material_id == MAT_PRISMATIC_CRYSTAL) {
                glm::ivec3 attach_norm = -adj_dirs[d];
                ChunkPos n_cpos{floor_div(n_pos.x, CHUNK_SIZE), floor_div(n_pos.y, CHUNK_SIZE), floor_div(n_pos.z, CHUNK_SIZE)};
                Chunk* n_chunk = get_chunk(n_cpos);
                if (n_chunk) {
                    bool already_has = false;
                    for (const auto& cl : n_chunk->clutter_instances()) {
                        if (cl.host_voxel == n_pos && cl.attach_normal == attach_norm) {
                            already_has = true;
                            break;
                        }
                    }
                    if (!already_has) {
                        ClutterInstance new_cl = SurfaceClutterSystem::create_crystal_instance(n_pos, attach_norm, n_vox.material_id, m_seed);
                        n_chunk->clutter_instances_mut().push_back(new_cl);
                    }
                }
            }
        }
    }

    // Event-Driven Fluid Wake-Up Hooks:
    if (v.material_id == MAT_AIR) {
        check_wake_fluid_around(glm::ivec3(world_x, world_y, world_z));
    } else if (IsLiquid(v.material_id) || v.is_waterlogged()) {
        WakeFluid(glm::ivec3(world_x, world_y, world_z));
    }

    chunk->MarkDirty();
    queue_chunk_for_meshing(cpos);

    if (mark_neighbors) {
        if (lx == 0)              { Chunk* c = get_chunk(ChunkPos{cpos.x - 1, cpos.y, cpos.z}); if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); } }
        if (lx == CHUNK_SIZE - 1) { Chunk* c = get_chunk(ChunkPos{cpos.x + 1, cpos.y, cpos.z}); if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); } }
        if (ly == 0)              { Chunk* c = get_chunk(ChunkPos{cpos.x, cpos.y - 1, cpos.z}); if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); } }
        if (ly == CHUNK_SIZE - 1) { Chunk* c = get_chunk(ChunkPos{cpos.x, cpos.y + 1, cpos.z}); if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); } }
        if (lz == 0)              { Chunk* c = get_chunk(ChunkPos{cpos.x, cpos.y, cpos.z - 1}); if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); } }
        if (lz == CHUNK_SIZE - 1) { Chunk* c = get_chunk(ChunkPos{cpos.x, cpos.y, cpos.z + 1}); if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); } }
    }

    return true;
}

bool World::set_block_with_flags(const glm::ivec3& pos, uint8_t mat, uint8_t flags) {
    int cx = floor_div(pos.x, CHUNK_SIZE);
    int cy = floor_div(pos.y, CHUNK_SIZE);
    int cz = floor_div(pos.z, CHUNK_SIZE);
    ChunkPos cpos{cx, cy, cz};
    Chunk* chunk = get_or_create_chunk(cpos);
    if (!chunk) return false;

    int lx = floor_mod(pos.x, CHUNK_SIZE);
    int ly = floor_mod(pos.y, CHUNK_SIZE);
    int lz = floor_mod(pos.z, CHUNK_SIZE);

    chunk->set_voxel(lx, ly, lz, Voxel{mat, flags});
    chunk->MarkDirty();
    queue_chunk_for_meshing(cpos);

    // Cross-chunk mesh dirtying on boundaries
    if (lx == 0) {
        Chunk* c = get_chunk(ChunkPos{cx - 1, cy, cz});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    } else if (lx == CHUNK_SIZE - 1) {
        Chunk* c = get_chunk(ChunkPos{cx + 1, cy, cz});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    }
    if (ly == 0) {
        Chunk* c = get_chunk(ChunkPos{cx, cy - 1, cz});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    } else if (ly == CHUNK_SIZE - 1) {
        Chunk* c = get_chunk(ChunkPos{cx, cy + 1, cz});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    }
    if (lz == 0) {
        Chunk* c = get_chunk(ChunkPos{cx, cy, cz - 1});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    } else if (lz == CHUNK_SIZE - 1) {
        Chunk* c = get_chunk(ChunkPos{cx, cy, cz + 1});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    }

    return true;
}


bool World::SetBlock(int x, int y, int z, uint8_t mat, uint8_t flags) {
    bool res = set_voxel(x, y, z, Voxel{mat, flags}, true);
    if (mat == MAT_AIR) {
        glm::ivec3 pos(x, y, z);
        const glm::ivec3 offsets[6] = {
            { 1, 0, 0}, {-1, 0, 0},
            { 0, 1, 0}, { 0,-1, 0},
            { 0, 0, 1}, { 0, 0,-1}
        };
        for (const auto& offset : offsets) {
            glm::ivec3 nPos = pos + offset;
            uint8_t nMat = GetBlockMaterial(nPos);
            if (IsLiquid(nMat)) {
                m_fluidSim.WakeFluid(nPos);
            }
        }
    }
    return res;
}

bool World::SetBlock(const glm::ivec3& pos, uint8_t mat, uint8_t flags) {
    return SetBlock(pos.x, pos.y, pos.z, mat, flags);
}

bool World::set_block(int x, int y, int z, uint8_t mat, uint8_t flags) {
    return SetBlock(x, y, z, mat, flags);
}

bool World::set_block(const glm::ivec3& pos, uint8_t mat, uint8_t flags) {
    return SetBlock(pos.x, pos.y, pos.z, mat, flags);
}

uint8_t World::get_block_flags(const glm::ivec3& pos) const {
    return get_voxel(pos.x, pos.y, pos.z).flags_and_damage;
}

bool World::set_block_flags(const glm::ivec3& pos, uint8_t flags) {
    ChunkPos cpos{
        floor_div(pos.x, CHUNK_SIZE),
        floor_div(pos.y, CHUNK_SIZE),
        floor_div(pos.z, CHUNK_SIZE)
    };
    Chunk* chunk = get_or_create_chunk(cpos);
    if (!chunk) return false;
    int lx = floor_mod(pos.x, CHUNK_SIZE);
    int ly = floor_mod(pos.y, CHUNK_SIZE);
    int lz = floor_mod(pos.z, CHUNK_SIZE);
    chunk->SetFlags(lx, ly, lz, flags);
    chunk->MarkDirty();
    queue_chunk_for_meshing(cpos);

    // Cross-chunk mesh dirtying on boundaries
    if (lx == 0) {
        Chunk* c = get_chunk(ChunkPos{cpos.x - 1, cpos.y, cpos.z});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    } else if (lx == CHUNK_SIZE - 1) {
        Chunk* c = get_chunk(ChunkPos{cpos.x + 1, cpos.y, cpos.z});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    }
    if (ly == 0) {
        Chunk* c = get_chunk(ChunkPos{cpos.x, cpos.y - 1, cpos.z});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    } else if (ly == CHUNK_SIZE - 1) {
        Chunk* c = get_chunk(ChunkPos{cpos.x, cpos.y + 1, cpos.z});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    }
    if (lz == 0) {
        Chunk* c = get_chunk(ChunkPos{cpos.x, cpos.y, cpos.z - 1});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    } else if (lz == CHUNK_SIZE - 1) {
        Chunk* c = get_chunk(ChunkPos{cpos.x, cpos.y, cpos.z + 1});
        if (c) { c->MarkDirty(); queue_chunk_for_meshing(c->get_pos()); }
    }

    return true;
}

bool World::is_solid(const glm::ivec3& pos) const {
    if (pos.y < 0) return false;  // Out-of-bounds below is the lethal void singularity chasm, NOT solid!

    int cx = (pos.x < 0) ? ((pos.x - 31) / 32) : (pos.x / 32);
    int cy = (pos.y < 0) ? ((pos.y - 31) / 32) : (pos.y / 32);
    int cz = (pos.z < 0) ? ((pos.z - 31) / 32) : (pos.z / 32);

    const Chunk* chunk = get_chunk(ChunkPos{cx, cy, cz});
    if (chunk) {
        int lx = floor_mod(pos.x, CHUNK_SIZE);
        int ly = floor_mod(pos.y, CHUNK_SIZE);
        int lz = floor_mod(pos.z, CHUNK_SIZE);

        Voxel v = chunk->get_voxel(lx, ly, lz);
        return v.is_solid();
    }

    if (pos.y >= 26) return true; // Ceiling mantle layer is always solid when chunk is not present

    int world_w = m_level_gen ? m_level_gen->world_width() : LevelGenerator::WORLD_WIDTH;
    int world_d = m_level_gen ? m_level_gen->world_depth() : LevelGenerator::WORLD_DEPTH;

    // Enforce impenetrable outer sector perimeter bounds when no explicit chunk voxel is present
    if (pos.x <= 3 || pos.x >= world_w - 4 ||
        pos.z <= 3 || pos.z >= world_d - 4) {
        return true;
    }

    if (pos.y <= 3) return true;  // Uncarved bedrock base layer

    if (m_level_gen) {
        return m_level_gen->sample_voxel(pos.x, pos.y, pos.z).is_solid();
    }
    return true;
}

bool World::is_solid(int world_x, int world_y, int world_z) const {
    return is_solid(glm::ivec3(world_x, world_y, world_z));
}

bool World::is_liquid(const glm::ivec3& pos) const {
    if (pos.y < 0 || pos.y >= 26) return false;
    return get_voxel(pos.x, pos.y, pos.z).is_liquid();
}

bool World::is_liquid(int world_x, int world_y, int world_z) const {
    return is_liquid(glm::ivec3(world_x, world_y, world_z));
}

float World::get_highest_solid_surface(int x, int z) const {
    for (int y = 25; y >= 0; --y) {
        if (is_solid(glm::ivec3(x, y, z))) {
            if (!is_solid(glm::ivec3(x, y + 1, z)) && !is_solid(glm::ivec3(x, y + 2, z))) {
                return static_cast<float>(y + 1);
            }
        }
    }
    return 4.0f;
}

// Retained for backward compatibility
float World::sample_cavern_noise(float x, float y, float z) const {
    if (m_level_gen) {
        return m_level_gen->sample_voxel(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)).is_solid() ? 1.0f : -1.0f;
    }
    return 0.0f;
}

void World::generate_chunk_terrain(Chunk& chunk) {
    ChunkPos cpos = chunk.get_pos();
    int world_base_x = cpos.x * CHUNK_SIZE;
    int world_base_y = cpos.y * CHUNK_SIZE;
    int world_base_z = cpos.z * CHUNK_SIZE;

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                int wx = world_base_x + x;
                int wy = world_base_y + y;
                int wz = world_base_z + z;

                Voxel v = m_level_gen ? m_level_gen->sample_voxel(wx, wy, wz) : Voxel{MAT_DREDGE_BEDROCK, 0};
                chunk.set_voxel(x, y, z, v);

                if (v.material_id == MAT_RADIOACTIVE_ORE || v.material_id == MAT_RADIOACTIVE) {
                    m_radioactive_sources.push_back(glm::ivec3(wx, wy, wz));
                } else if (v.material_id == MAT_TOXIC_GAS || v.material_id == MAT_GAS) {
                    m_toxic_gas_sources.push_back(glm::ivec3(wx, wy, wz));
                } else if (v.material_id == MAT_MOLTEN_MAGMA || v.material_id == MAT_THERMITE_SLAG || v.material_id == MAT_LAVA) {
                    m_lava_sources.push_back(glm::ivec3(wx, wy, wz));
                } else if (v.material_id == MAT_CRYSTAL_AQUIFER || v.material_id == MAT_WATER || v.material_id == MAT_AQUIFER || v.material_id == MAT_COOLANT || v.material_id == MAT_ACID) {
                    m_aquifer_sources.push_back(glm::ivec3(wx, wy, wz));
                }
            }
        }
    }
}

Voxel World::get_voxel_unlocked(int world_x, int world_y, int world_z) const {
    ChunkPos cpos{
        floor_div(world_x, CHUNK_SIZE),
        floor_div(world_y, CHUNK_SIZE),
        floor_div(world_z, CHUNK_SIZE)
    };

    auto it = m_chunks.find(cpos);
    if (it == m_chunks.end() || !it->second) {
        if (m_level_gen) {
            return m_level_gen->sample_voxel(world_x, world_y, world_z);
        }
        return Voxel{MAT_AIR, 0};
    }

    int lx = floor_mod(world_x, CHUNK_SIZE);
    int ly = floor_mod(world_y, CHUNK_SIZE);
    int lz = floor_mod(world_z, CHUNK_SIZE);
    return it->second->get_voxel(lx, ly, lz);
}

void World::GenerateChunkClutter(Chunk& chunk) {
    chunk.set_clutter_instances(SurfaceClutterSystem::generate_chunk_clutter(
        chunk, m_seed, [this](int wx, int wy, int wz) { return get_voxel(wx, wy, wz); }
    ));
}

int World::clear_disconnected_clutter(const glm::ivec3& host_block) {
    int total_removed = 0;
    std::vector<ClutterInstance> removed_props;

    int cx = floor_div(host_block.x, CHUNK_SIZE);
    int cy = floor_div(host_block.y, CHUNK_SIZE);
    int cz = floor_div(host_block.z, CHUNK_SIZE);

    ChunkPos cpos{cx, cy, cz};
    Chunk* chunk = get_chunk(cpos);
    if (chunk) {
        total_removed += chunk->remove_clutter_hosted_at(host_block, &removed_props);
    }

    // Check adjacent neighbor chunks if the host block is at a chunk boundary
    int lx = floor_mod(host_block.x, CHUNK_SIZE);
    int ly = floor_mod(host_block.y, CHUNK_SIZE);
    int lz = floor_mod(host_block.z, CHUNK_SIZE);

    const glm::ivec3 neighbor_dirs[6] = {
        {-1, 0, 0}, { 1, 0, 0},
        { 0,-1, 0}, { 0, 1, 0},
        { 0, 0,-1}, { 0, 0, 1}
    };
    bool is_border[6] = {
        lx == 0,
        lx == CHUNK_SIZE - 1,
        ly == 0,
        ly == CHUNK_SIZE - 1,
        lz == 0,
        lz == CHUNK_SIZE - 1
    };

    for (int d = 0; d < 6; ++d) {
        if (is_border[d]) {
            Chunk* n_chunk = get_chunk(ChunkPos{cx + neighbor_dirs[d].x, cy + neighbor_dirs[d].y, cz + neighbor_dirs[d].z});
            if (n_chunk) {
                total_removed += n_chunk->remove_clutter_hosted_at(host_block, &removed_props);
            }
        }
    }

    // Auto-Despawn: Instantly clear disconnected surface props with a small break-particle burst
    for (const auto& prop : removed_props) {
        ParticleSystem::SpawnClutterBreakDebris(prop.position, prop.type, prop.color_tint);
    }

    return total_removed;
}

int World::cleanup_all_disconnected_clutter(Chunk& chunk) {
    auto& instances = chunk.clutter_instances_mut();
    if (instances.empty()) return 0;
    int removed_count = 0;
    std::vector<ClutterInstance> removed;
    auto it = std::remove_if(instances.begin(), instances.end(), [&](const ClutterInstance& inst) {
        Voxel v = get_voxel(inst.host_voxel.x, inst.host_voxel.y, inst.host_voxel.z);
        if (!v.is_solid() || v.material_id == MAT_AIR) {
            removed.push_back(inst);
            removed_count++;
            return true;
        }
        return false;
    });
    instances.erase(it, instances.end());
    for (const auto& prop : removed) {
        ParticleSystem::SpawnClutterBreakDebris(prop.position, prop.type, prop.color_tint);
    }
    return removed_count;
}

int World::ensure_structural_connectivity() {
    return StructuralCheck::ensure_world_connectivity(*this);
}

int World::count_unanchored_blocks() const {
    return StructuralCheck::count_unanchored_blocks(*this);
}

int World::count_disconnected_clutter() const {
    int count = 0;
    for (const auto& [cpos, chunk] : m_chunks) {
        if (!chunk) continue;
        for (const auto& inst : chunk->clutter_instances()) {
            Voxel v = get_voxel(inst.host_voxel.x, inst.host_voxel.y, inst.host_voxel.z);
            if (!v.is_solid() || v.material_id == MAT_AIR) {
                count++;
            }
        }
    }
    return count;
}

void World::ApplyTopologicalShapes(Chunk& chunk) {
    if (chunk.is_empty()) return;
    ChunkPos cpos = chunk.get_pos();
    int base_x = cpos.x * CHUNK_SIZE;
    int base_y = cpos.y * CHUNK_SIZE;
    int base_z = cpos.z * CHUNK_SIZE;

    auto get_vox = [&](int lx, int ly, int lz) -> Voxel {
        if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE && lz >= 0 && lz < CHUNK_SIZE) {
            return chunk.get_voxel(lx, ly, lz);
        }
        int wx = base_x + lx;
        int wy = base_y + ly;
        int wz = base_z + lz;
        if (wy < 0) {
            return Voxel{MAT_DREDGE_BEDROCK, 0};
        }
        return get_voxel_unlocked(wx, wy, wz);
    };

    auto is_stone_floor = [](const Voxel& v) {
        return v.is_solid() && (v.material_id == MAT_FRACTURED_GRANITE ||
                                v.material_id == MAT_VOLCANIC_BASALT ||
                                v.material_id == MAT_DREDGE_BEDROCK ||
                                v.material_id == MAT_PRECURSOR_STONE);
    };

    // Pass 1: Floor Step & Terracing Shape Assignment
    for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
            for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                Voxel cur = chunk.get_voxel(lx, ly, lz);
                if (!is_stone_floor(cur)) continue;

                Voxel above = get_vox(lx, ly + 1, lz);
                if (above.is_solid()) continue; // Must have air above

                Voxel below = get_vox(lx, ly - 1, lz);
                if (!below.is_solid() || below.shape() != SHAPE_CUBE) continue; // Solid Base Support

                // Step candidate checks along each horizontal direction
                // East (+X):
                bool step_east = get_vox(lx + 1, ly, lz).is_solid() && get_vox(lx + 1, ly, lz).shape() == SHAPE_CUBE &&
                                 !get_vox(lx + 1, ly + 1, lz).is_solid() &&
                                 !get_vox(lx - 1, ly, lz).is_solid() &&
                                 get_vox(lx - 1, ly - 1, lz).is_solid() &&
                                 !get_vox(lx, ly + 1, lz).is_solid();

                // West (-X):
                bool step_west = get_vox(lx - 1, ly, lz).is_solid() && get_vox(lx - 1, ly, lz).shape() == SHAPE_CUBE &&
                                 !get_vox(lx - 1, ly + 1, lz).is_solid() &&
                                 !get_vox(lx + 1, ly, lz).is_solid() &&
                                 get_vox(lx + 1, ly - 1, lz).is_solid() &&
                                 !get_vox(lx, ly + 1, lz).is_solid();

                // South (+Z):
                bool step_south = get_vox(lx, ly, lz + 1).is_solid() && get_vox(lx, ly, lz + 1).shape() == SHAPE_CUBE &&
                                  !get_vox(lx, ly + 1, lz + 1).is_solid() &&
                                  !get_vox(lx, ly, lz - 1).is_solid() &&
                                  get_vox(lx, ly - 1, lz - 1).is_solid() &&
                                  !get_vox(lx, ly + 1, lz).is_solid();

                // North (-Z):
                bool step_north = get_vox(lx, ly, lz - 1).is_solid() && get_vox(lx, ly, lz - 1).shape() == SHAPE_CUBE &&
                                  !get_vox(lx, ly + 1, lz - 1).is_solid() &&
                                  !get_vox(lx, ly, lz + 1).is_solid() &&
                                  get_vox(lx, ly - 1, lz + 1).is_solid() &&
                                  !get_vox(lx, ly + 1, lz).is_solid();

                int step_count = (step_east ? 1 : 0) + (step_west ? 1 : 0) +
                                 (step_south ? 1 : 0) + (step_north ? 1 : 0);

                if (cur.shape() == SHAPE_CUBE) {
                    if (step_count == 1) {
                        if (step_east) cur.set_shape(SHAPE_RAMP_EAST);
                        else if (step_west) cur.set_shape(SHAPE_RAMP_WEST);
                        else if (step_south) cur.set_shape(SHAPE_RAMP_SOUTH);
                        else if (step_north) cur.set_shape(SHAPE_RAMP_NORTH);
                        chunk.set_voxel(lx, ly, lz, cur);
                    } else if (step_count > 1) {
                        // Terracing via half-slabs on multi-step perimeter edges
                        cur.set_shape(SHAPE_SLAB_BOTTOM);
                        chunk.set_voxel(lx, ly, lz, cur);
                    }
                }
            }
        }
    }

    // Pass 2: Strict Invariant Enforcement & Smoothing Pass
    for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
            for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                Voxel cur = chunk.get_voxel(lx, ly, lz);
                if (!cur.is_solid()) continue;

                // 1. Solid Base Support:
                // A ground slope or slab may ONLY exist if (x, y-1, z) is a solid, non-sloped cube (SHAPE_CUBE).
                if (cur.is_ramp() || cur.is_slab()) {
                    Voxel below = get_vox(lx, ly - 1, lz);
                    if (!below.is_solid() || below.shape() != SHAPE_CUBE) {
                        cur.set_shape(SHAPE_CUBE);
                        chunk.set_voxel(lx, ly, lz, cur);
                        continue;
                    }
                }

                // 2. Headroom Check:
                // Ramps and slabs cannot exist if there is a solid block directly above them
                if (cur.is_ramp() || cur.is_slab()) {
                    Voxel above = get_vox(lx, ly + 1, lz);
                    if (above.is_solid()) {
                        cur.set_shape(SHAPE_CUBE);
                        chunk.set_voxel(lx, ly, lz, cur);
                        continue;
                    }
                }

                // 3. No Opposing Sawtooth Peaks:
                // Never generate opposing ramps in adjacent cells
                if (cur.shape() == SHAPE_RAMP_EAST) {
                    Voxel east_n = get_vox(lx + 1, ly, lz);
                    if (east_n.shape() == SHAPE_RAMP_WEST) {
                        cur.set_shape(SHAPE_SLAB_BOTTOM);
                        chunk.set_voxel(lx, ly, lz, cur);
                        if (lx + 1 < CHUNK_SIZE) {
                            east_n.set_shape(SHAPE_SLAB_BOTTOM);
                            chunk.set_voxel(lx + 1, ly, lz, east_n);
                        }
                    }
                } else if (cur.shape() == SHAPE_RAMP_WEST) {
                    Voxel west_n = get_vox(lx - 1, ly, lz);
                    if (west_n.shape() == SHAPE_RAMP_EAST) {
                        cur.set_shape(SHAPE_SLAB_BOTTOM);
                        chunk.set_voxel(lx, ly, lz, cur);
                        if (lx - 1 >= 0) {
                            west_n.set_shape(SHAPE_SLAB_BOTTOM);
                            chunk.set_voxel(lx - 1, ly, lz, west_n);
                        }
                    }
                } else if (cur.shape() == SHAPE_RAMP_SOUTH) {
                    Voxel south_n = get_vox(lx, ly, lz + 1);
                    if (south_n.shape() == SHAPE_RAMP_NORTH) {
                        cur.set_shape(SHAPE_SLAB_BOTTOM);
                        chunk.set_voxel(lx, ly, lz, cur);
                        if (lz + 1 < CHUNK_SIZE) {
                            south_n.set_shape(SHAPE_SLAB_BOTTOM);
                            chunk.set_voxel(lx, ly, lz + 1, south_n);
                        }
                    }
                } else if (cur.shape() == SHAPE_RAMP_NORTH) {
                    Voxel north_n = get_vox(lx, ly, lz - 1);
                    if (north_n.shape() == SHAPE_RAMP_SOUTH) {
                        cur.set_shape(SHAPE_SLAB_BOTTOM);
                        chunk.set_voxel(lx, ly, lz, cur);
                        if (lz - 1 >= 0) {
                            north_n.set_shape(SHAPE_SLAB_BOTTOM);
                            chunk.set_voxel(lx, ly, lz - 1, north_n);
                        }
                    }
                }
            }
        }
    }

    // Pass 3: Liquid-Contact Sub-Block Waterlogging Pass
    // Ensure any natural ramps or slabs in contact with initially placed world liquids
    // are correctly marked waterlogged so fluids seamlessly flow and contour over them.
    bool changed = true;
    for (int iter = 0; iter < 4 && changed; ++iter) {
        changed = false;
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                    Voxel cur = chunk.get_voxel(lx, ly, lz);
                    if (!cur.is_ramp() && !cur.is_slab()) continue;

                    // Check if directly beneath fluid
                    Voxel above = get_vox(lx, ly + 1, lz);
                    bool submerged = above.is_liquid() || above.is_waterlogged();

                    // Check lateral liquid contact
                    bool lateral_liquid = false;
                    int max_liquid_lvl = 0;
                    const glm::ivec3 dirs[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
                    for (const auto& d : dirs) {
                        Voxel nb = get_vox(lx + d.x, ly, lz + d.z);
                        if (nb.is_liquid() || nb.is_waterlogged()) {
                            lateral_liquid = true;
                            int lvl = GetFluidLevel(nb.flags_and_damage);
                            max_liquid_lvl = std::max(max_liquid_lvl, lvl);
                        }
                    }

                    // Check upper step contact (liquid on elevated ledge feeding down onto ramp)
                    bool upper_liquid = false;
                    glm::ivec3 high_dir(0);
                    if (cur.shape() == SHAPE_RAMP_EAST)  high_dir = glm::ivec3( 1, 0, 0);
                    if (cur.shape() == SHAPE_RAMP_WEST)  high_dir = glm::ivec3(-1, 0, 0);
                    if (cur.shape() == SHAPE_RAMP_SOUTH) high_dir = glm::ivec3( 0, 0, 1);
                    if (cur.shape() == SHAPE_RAMP_NORTH) high_dir = glm::ivec3( 0, 0,-1);
                    if (high_dir != glm::ivec3(0)) {
                        Voxel nb_high = get_vox(lx + high_dir.x, ly + 1, lz + high_dir.z);
                        if (nb_high.is_liquid() || nb_high.is_waterlogged()) {
                            upper_liquid = true;
                            int lvl = GetFluidLevel(nb_high.flags_and_damage);
                            max_liquid_lvl = std::max(max_liquid_lvl, lvl);
                        }
                    }

                    if (submerged || lateral_liquid || upper_liquid) {
                        uint8_t old_flags = cur.flags_and_damage;
                        cur.flags_and_damage |= VOXEL_FLAG_WATERLOGGED;
                        int final_lvl = submerged ? 5 : max_liquid_lvl;
                        if (final_lvl < 1) final_lvl = 1;
                        if (final_lvl > 5) final_lvl = 5;
                        cur.flags_and_damage = (cur.flags_and_damage & ~VOXEL_DAMAGE_MASK) | (final_lvl & VOXEL_DAMAGE_MASK);
                        if (cur.flags_and_damage != old_flags) {
                            chunk.set_voxel(lx, ly, lz, cur);
                            changed = true;
                        }
                    }
                }
            }
        }
    }
}

void World::detect_natural_floor_steps() {
    std::lock_guard<std::mutex> lock(m_world_mutex);
    if (m_chunks.empty()) return;

    struct RampCandidate {
        ChunkPos cpos;
        int lx, ly, lz;
        VoxelShape shape;
    };
    std::vector<RampCandidate> candidates;

    auto is_stone_floor = [](const Voxel& v) {
        return v.is_solid() && (v.material_id == MAT_FRACTURED_GRANITE ||
                                v.material_id == MAT_VOLCANIC_BASALT ||
                                v.material_id == MAT_DREDGE_BEDROCK);
    };

    for (const auto& [cpos, chunk] : m_chunks) {
        if (!chunk || chunk->is_empty()) continue;
        int base_x = cpos.x * CHUNK_SIZE;
        int base_y = cpos.y * CHUNK_SIZE;
        int base_z = cpos.z * CHUNK_SIZE;

        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                int wy = base_y + ly;
                if (wy <= 3 || wy >= 30) continue;

                for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                    Voxel cur = chunk->get_voxel(lx, ly, lz);
                    if (!is_stone_floor(cur)) continue;

                    int wx = base_x + lx;
                    int wz = base_z + lz;

                    // Must have air directly above
                    Voxel above = get_voxel_unlocked(wx, wy + 1, wz);
                    if (above.is_solid()) continue;

                    // Detect directional natural floor step where an adjacent solid neighbor rises at Y + 1
                    bool step_pos_x = get_voxel_unlocked(wx + 1, wy + 1, wz).is_solid() &&
                                      get_voxel_unlocked(wx + 1, wy, wz).is_solid() &&
                                      !get_voxel_unlocked(wx + 1, wy + 2, wz).is_solid() &&
                                      !get_voxel_unlocked(wx - 1, wy + 1, wz).is_solid();

                    bool step_neg_x = get_voxel_unlocked(wx - 1, wy + 1, wz).is_solid() &&
                                      get_voxel_unlocked(wx - 1, wy, wz).is_solid() &&
                                      !get_voxel_unlocked(wx - 1, wy + 2, wz).is_solid() &&
                                      !get_voxel_unlocked(wx + 1, wy + 1, wz).is_solid();

                    bool step_pos_z = get_voxel_unlocked(wx, wy + 1, wz + 1).is_solid() &&
                                      get_voxel_unlocked(wx, wy, wz + 1).is_solid() &&
                                      !get_voxel_unlocked(wx, wy + 2, wz + 1).is_solid() &&
                                      !get_voxel_unlocked(wx, wy + 1, wz - 1).is_solid();

                    bool step_neg_z = get_voxel_unlocked(wx, wy + 1, wz - 1).is_solid() &&
                                      get_voxel_unlocked(wx, wy, wz - 1).is_solid() &&
                                      !get_voxel_unlocked(wx, wy + 2, wz - 1).is_solid() &&
                                      !get_voxel_unlocked(wx, wy + 1, wz + 1).is_solid();

                    int step_count = (step_pos_x ? 1 : 0) + (step_neg_x ? 1 : 0) +
                                     (step_pos_z ? 1 : 0) + (step_neg_z ? 1 : 0);

                    if (step_count == 1) {
                        VoxelShape shape = SHAPE_CUBE;
                        if (step_pos_x) shape = SHAPE_RAMP_POS_X;
                        else if (step_neg_x) shape = SHAPE_RAMP_NEG_X;
                        else if (step_pos_z) shape = SHAPE_RAMP_POS_Z;
                        else if (step_neg_z) shape = SHAPE_RAMP_NEG_Z;

                        candidates.push_back({cpos, lx, ly, lz, shape});
                    }
                }
            }
        }
    }

    for (const auto& c : candidates) {
        auto it = m_chunks.find(c.cpos);
        if (it != m_chunks.end() && it->second) {
            Voxel v = it->second->get_voxel(c.lx, c.ly, c.lz);
            v.set_shape(c.shape);
            it->second->set_voxel(c.lx, c.ly, c.lz, v);
        }
    }
}

float World::query_radiation_proximity(const glm::vec3& pos, float max_radius) const {
    std::lock_guard<std::mutex> lock(m_world_mutex);
    if (m_radioactive_sources.empty()) return 0.0f;

    float total_intensity = 0.0f;
    float max_r2 = max_radius * max_radius;

    for (const auto& src : m_radioactive_sources) {
        glm::vec3 src_center = glm::vec3(src) + glm::vec3(0.5f);
        glm::vec3 diff = pos - src_center;
        float d2 = glm::dot(diff, diff);
        if (d2 < max_r2) {
            float dist = std::sqrt(d2);
            float falloff = std::max(0.0f, 1.0f - (dist / max_radius));
            total_intensity += falloff * falloff;
        }
    }

    // Normalized intensity scaling: standing 1 block away from an ore cluster yields ~0.75-1.0
    return std::clamp(total_intensity * 0.45f, 0.0f, 1.0f);
}

// Fast DDA (Digital Differential Analyzer) voxel raycasting
RaycastHit World::raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance) const {
    RaycastHit hit;
    hit.hit = false;

    glm::vec3 dir = glm::normalize(direction);
    glm::ivec3 block_pos = glm::floor(origin);

    glm::ivec3 step(
        (dir.x > 0.0f) ? 1 : ((dir.x < 0.0f) ? -1 : 0),
        (dir.y > 0.0f) ? 1 : ((dir.y < 0.0f) ? -1 : 0),
        (dir.z > 0.0f) ? 1 : ((dir.z < 0.0f) ? -1 : 0)
    );

    glm::vec3 t_delta(
        (step.x != 0) ? std::abs(1.0f / dir.x) : 1e30f,
        (step.y != 0) ? std::abs(1.0f / dir.y) : 1e30f,
        (step.z != 0) ? std::abs(1.0f / dir.z) : 1e30f
    );

    glm::vec3 t_max(
        (step.x > 0) ? (block_pos.x + 1.0f - origin.x) * t_delta.x : (origin.x - block_pos.x) * t_delta.x,
        (step.y > 0) ? (block_pos.y + 1.0f - origin.y) * t_delta.y : (origin.y - block_pos.y) * t_delta.y,
        (step.z > 0) ? (block_pos.z + 1.0f - origin.z) * t_delta.z : (origin.z - block_pos.z) * t_delta.z
    );

    float current_dist = 0.0f;
    glm::ivec3 normal(0);

    while (current_dist <= max_distance) {
        Voxel v = get_voxel(block_pos.x, block_pos.y, block_pos.z);
        if (v.is_solid()) {
            hit.hit = true;
            hit.block_pos = block_pos;
            hit.chunk_pos = ChunkPos{
                floor_div(block_pos.x, CHUNK_SIZE),
                floor_div(block_pos.y, CHUNK_SIZE),
                floor_div(block_pos.z, CHUNK_SIZE)
            };
            hit.local_x = floor_mod(block_pos.x, CHUNK_SIZE);
            hit.local_y = floor_mod(block_pos.y, CHUNK_SIZE);
            hit.local_z = floor_mod(block_pos.z, CHUNK_SIZE);
            hit.normal = normal;
            hit.voxel = v;
            hit.distance = current_dist;
            return hit;
        }

        if (t_max.x < t_max.y) {
            if (t_max.x < t_max.z) {
                block_pos.x += step.x;
                current_dist = t_max.x;
                t_max.x += t_delta.x;
                normal = glm::ivec3(-step.x, 0, 0);
            } else {
                block_pos.z += step.z;
                current_dist = t_max.z;
                t_max.z += t_delta.z;
                normal = glm::ivec3(0, 0, -step.z);
            }
        } else {
            if (t_max.y < t_max.z) {
                block_pos.y += step.y;
                current_dist = t_max.y;
                t_max.y += t_delta.y;
                normal = glm::ivec3(0, -step.y, 0);
            } else {
                block_pos.z += step.z;
                current_dist = t_max.z;
                t_max.z += t_delta.z;
                normal = glm::ivec3(0, 0, -step.z);
            }
        }
    }

    return hit;
}

float World::calculate_spawn_yaw(const glm::vec3& spawn_pos) const {
    // Eye level for clearance raycasting
    glm::vec3 eye = spawn_pos + glm::vec3(0.0f, 1.6f, 0.0f);

    // If level generator is present and has corridors leading from spawn room (grid 0, 0),
    // prioritize open corridors with genuine clearance
    if (m_level_gen) {
        const auto* r = m_level_gen->get_room_at_grid(0, 0);
        if (r) {
            float best_corr_clearance = -1.0f;
            glm::vec3 best_corr_dir(0.0f);
            for (const auto& c : m_level_gen->corridors()) {
                glm::vec3 start(static_cast<float>(c.start_pos.x), static_cast<float>(c.floor_y + 1), static_cast<float>(c.start_pos.z));
                glm::vec3 end(static_cast<float>(c.end_pos.x), static_cast<float>(c.floor_y + 1), static_cast<float>(c.end_pos.z));
                float dist_s = glm::distance(glm::vec2(spawn_pos.x, spawn_pos.z), glm::vec2(start.x, start.z));
                float dist_e = glm::distance(glm::vec2(spawn_pos.x, spawn_pos.z), glm::vec2(end.x, end.z));

                glm::vec3 candidate_target(0.0f);
                if (dist_s < 24.0f) {
                    candidate_target = end;
                } else if (dist_e < 24.0f) {
                    candidate_target = start;
                }

                if (glm::length(candidate_target) > 0.1f) {
                    glm::vec3 dir = candidate_target - spawn_pos;
                    dir.y = 0.0f;
                    if (glm::length(dir) > 0.01f) {
                        dir = glm::normalize(dir);
                        RaycastHit hit = raycast(eye, dir, 48.0f);
                        float clearance = hit.hit ? hit.distance : 48.0f;
                        if (clearance > best_corr_clearance) {
                            best_corr_clearance = clearance;
                            best_corr_dir = dir;
                        }
                    }
                }
            }
            if (best_corr_clearance > 6.0f) {
                return glm::degrees(std::atan2(best_corr_dir.z, best_corr_dir.x));
            }
        }
    }

    // Cast 32 radial horizontal rays to locate the open grotto or corridor
    constexpr int NUM_RAYS = 32;
    float distances[NUM_RAYS];
    for (int i = 0; i < NUM_RAYS; ++i) {
        float angle = static_cast<float>(i) * (2.0f * 3.14159265358979323846f / static_cast<float>(NUM_RAYS));
        glm::vec3 dir(std::cos(angle), 0.0f, std::sin(angle));
        RaycastHit hit = raycast(eye, dir, 48.0f);
        distances[i] = hit.hit ? hit.distance : 48.0f;
    }

    // Weighted 3-tap smoothing kernel (0.25, 0.5, 0.25) to favor wide corridors/grottos over narrow alcoves
    float best_score = -1.0f;
    int best_idx = 0;
    for (int i = 0; i < NUM_RAYS; ++i) {
        int prev = (i - 1 + NUM_RAYS) % NUM_RAYS;
        int next = (i + 1) % NUM_RAYS;
        float score = distances[prev] * 0.25f + distances[i] * 0.5f + distances[next] * 0.25f;
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    float best_angle = static_cast<float>(best_idx) * (2.0f * 3.14159265358979323846f / static_cast<float>(NUM_RAYS));
    glm::vec3 best_dir(std::cos(best_angle), 0.0f, std::sin(best_angle));
    return glm::degrees(std::atan2(best_dir.z, best_dir.x));
}

void World::queue_chunk_for_meshing(const ChunkPos& pos) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    if (m_mesh_queued_set.insert(pos).second) {
        m_mesh_queue.push(pos);
        m_queue_cv.notify_one();
    }
}

void World::worker_thread_loop() {
    while (m_running.load()) {
        ChunkPos target_pos;
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_queue_cv.wait(lock, [this]() {
                return !m_running.load() || !m_mesh_queue.empty();
            });

            if (!m_running.load() && m_mesh_queue.empty()) {
                return;
            }

            target_pos = m_mesh_queue.front();
            m_mesh_queue.pop();
            m_mesh_queued_set.erase(target_pos);
        }

        std::shared_ptr<Chunk> chunk;
        {
            std::lock_guard<std::mutex> lock(m_world_mutex);
            auto it = m_chunks.find(target_pos);
            if (it != m_chunks.end()) {
                chunk = it->second;
            }
        }

        if (!chunk) continue;

        auto get_neighbor = [this](const ChunkPos& npos) -> const Chunk* {
            std::lock_guard<std::mutex> lock(m_world_mutex);
            auto it = m_chunks.find(npos);
            if (it != m_chunks.end()) {
                return it->second.get();
            }
            return nullptr;
        };

        uint32_t start_ver = chunk->mesh_version();
        std::vector<PackedVoxelVertex> mesh = GreedyMesher::generate_mesh(*chunk, get_neighbor);
        auto mesh_data = std::make_unique<ChunkMeshData>();
        mesh_data->pos = target_pos;
        mesh_data->chunk = chunk.get();
        mesh_data->chunk_ref = chunk;
        mesh_data->vertices = std::move(mesh);
        mesh_data->version = start_ver;

        {
            std::lock_guard<std::mutex> lock(m_upload_mutex);
            // Only queue upload if the chunk was not modified again during meshing
            if (chunk->mesh_version() == start_ver) {
                auto it = m_pending_upload_map.find(target_pos);
                if (it != m_pending_upload_map.end() && it->second) {
                    it->second->vertices = std::move(mesh_data->vertices);
                    it->second->version = mesh_data->version;
                } else {
                    m_upload_queued_set.insert(target_pos);
                    ChunkMeshData* raw = mesh_data.get();
                    m_pending_upload_map[target_pos] = raw;
                    m_pendingUploads.push(std::move(mesh_data));
                }
            }
        }
    }
}

void World::push_pending_upload(std::unique_ptr<ChunkMeshData> data) {
    if (!data) return;
    std::lock_guard<std::mutex> lock(m_upload_mutex);
    auto it = m_pending_upload_map.find(data->pos);
    if (it != m_pending_upload_map.end() && it->second) {
        it->second->vertices = std::move(data->vertices);
        it->second->version = data->version;
    } else {
        m_upload_queued_set.insert(data->pos);
        ChunkMeshData* raw = data.get();
        m_pending_upload_map[data->pos] = raw;
        m_pendingUploads.push(std::move(data));
    }
}

void World::queue_chunk_for_upload(const ChunkPos& pos) {
    std::lock_guard<std::mutex> lock(m_upload_mutex);
    if (m_upload_queued_set.insert(pos).second) {
        auto data = std::make_unique<ChunkMeshData>();
        data->pos = pos;
        {
            std::lock_guard<std::mutex> wlock(m_world_mutex);
            auto it = m_chunks.find(pos);
            if (it != m_chunks.end() && it->second) {
                data->chunk = it->second.get();
                data->chunk_ref = it->second;
                data->version = it->second->mesh_version();
            }
        }
        ChunkMeshData* raw = data.get();
        m_pending_upload_map[pos] = raw;
        m_pendingUploads.push(std::move(data));
    }
}

size_t World::upload_queue_size() const {
    std::lock_guard<std::mutex> lock(m_upload_mutex);
    return m_pendingUploads.size();
}

size_t World::mesh_queue_size() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_queue_mutex));
    return m_mesh_queue.size();
}

size_t World::upload_mesh_queue(size_t max_uploads) {
    if (max_uploads == 0) return 0;

    size_t uploadsDone = 0;

    // 1. Drain up to max_uploads completed chunks from the worker upload queue
    {
        std::lock_guard<std::mutex> lock(m_upload_mutex);
        while (!m_pendingUploads.empty() && uploadsDone < max_uploads) {
            auto& front = m_pendingUploads.front();
            if (front) {
                m_upload_queued_set.erase(front->pos);
                m_pending_upload_map.erase(front->pos);
                front->UploadToGPU();
            }
            m_pendingUploads.pop();
            uploadsDone++;
        }
    }

    // 2. Fallback check: if upload queue is exhausted but there are still staged chunks in m_chunks,
    // upload up to the remaining per-frame budget
    if (uploadsDone < max_uploads) {
        std::lock_guard<std::mutex> lock(m_world_mutex);
        for (auto& [pos, chunk] : m_chunks) {
            if (uploadsDone >= max_uploads) break;
            if (chunk && chunk->has_staged_mesh()) {
                chunk->upload_mesh();
                uploadsDone++;
            }
        }
    }

    return uploadsDone;
}

static_assert(World::MAX_CHUNK_UPLOADS_PER_FRAME == 2, "World::MAX_CHUNK_UPLOADS_PER_FRAME must be 2");
static_assert(World::MAX_ACTIVE_DEBRIS == 36, "World::MAX_ACTIVE_DEBRIS must be 36");
static_assert(EntityManager::MAX_ACTIVE_DEBRIS == 36, "EntityManager::MAX_ACTIVE_DEBRIS must be 36");

void World::update() {
    size_t uploadsDone = 0;
    {
        std::lock_guard<std::mutex> lock(m_upload_mutex);
        while (!m_pendingUploads.empty() && uploadsDone < MAX_CHUNK_UPLOADS_PER_FRAME) {
            auto& front = m_pendingUploads.front();
            if (front) {
                m_upload_queued_set.erase(front->pos);
                m_pending_upload_map.erase(front->pos);
                front->UploadToGPU();
            }
            m_pendingUploads.pop();
            uploadsDone++;
        }
    }
    m_uploads_this_frame = uploadsDone;
}

void World::update(const glm::vec3& viewer_pos, int render_distance) {
    // Amortize OpenGL driver overhead: upload at most MAX_CHUNK_UPLOADS_PER_FRAME buffers per frame
    size_t uploadsDone = 0;
    {
        std::lock_guard<std::mutex> lock(m_upload_mutex);
        while (!m_pendingUploads.empty() && uploadsDone < MAX_CHUNK_UPLOADS_PER_FRAME) {
            auto& front = m_pendingUploads.front();
            if (front) {
                m_upload_queued_set.erase(front->pos);
                m_pending_upload_map.erase(front->pos);
                front->UploadToGPU();
            }
            m_pendingUploads.pop();
            uploadsDone++;
        }
    }
    m_uploads_this_frame = uploadsDone;

    int max_cx = m_level_gen ? ((m_level_gen->world_width() + CHUNK_SIZE - 1) / CHUNK_SIZE) : 8;
    int max_cy = std::max((m_chunk_ceiling + CHUNK_SIZE - 1) / CHUNK_SIZE,
                          m_level_gen ? ((m_level_gen->world_height() + CHUNK_SIZE - 1) / CHUNK_SIZE) : 1);
    int max_cz = m_level_gen ? ((m_level_gen->world_depth() + CHUNK_SIZE - 1) / CHUNK_SIZE) : 8;

    ChunkPos center{
        floor_div(static_cast<int>(viewer_pos.x), CHUNK_SIZE),
        floor_div(static_cast<int>(viewer_pos.y), CHUNK_SIZE),
        floor_div(static_cast<int>(viewer_pos.z), CHUNK_SIZE)
    };

    for (int dz = -render_distance; dz <= render_distance; ++dz) {
        int cz = center.z + dz;
        if (cz < 0 || cz >= max_cz) continue;

        for (int dy = -render_distance; dy <= render_distance; ++dy) {
            int cy = center.y + dy;
            if (cy < 0 || cy >= max_cy) continue;

            for (int dx = -render_distance; dx <= render_distance; ++dx) {
                int cx = center.x + dx;
                if (cx < 0 || cx >= max_cx) continue;

                get_or_create_chunk(ChunkPos{cx, cy, cz});
            }
        }
    }
}

size_t World::upload_dirty_chunks(size_t max_uploads) {
    size_t uploaded = 0;
    if (m_uploads_this_frame < MAX_CHUNK_UPLOADS_PER_FRAME) {
        size_t allowed = std::min(max_uploads, MAX_CHUNK_UPLOADS_PER_FRAME - m_uploads_this_frame);
        uploaded = upload_mesh_queue(allowed);
        m_uploads_this_frame += uploaded;
    }
    return uploaded;
}

void World::force_mesh_all_sync() {
    std::lock_guard<std::mutex> lock(m_world_mutex);
    auto get_neighbor = [this](const ChunkPos& npos) -> const Chunk* {
        auto it = m_chunks.find(npos);
        if (it != m_chunks.end()) {
            return it->second.get();
        }
        return nullptr;
    };
    for (auto& [pos, chunk] : m_chunks) {
        if (chunk) {
            std::vector<PackedVoxelVertex> mesh = GreedyMesher::generate_mesh(*chunk, get_neighbor);
            chunk->stage_mesh(std::move(mesh));
            chunk->upload_mesh();
        }
    }
    // Flush upload queue to prevent redundant uploads
    {
        std::lock_guard<std::mutex> lock(m_upload_mutex);
        std::queue<std::unique_ptr<ChunkMeshData>> empty_queue;
        std::swap(m_pendingUploads, empty_queue);
        m_upload_queued_set.clear();
        m_pending_upload_map.clear();
    }
}

void World::sync_remesh_chunks(const std::unordered_set<ChunkPos, ChunkPosHash>& chunk_positions) {
    if (chunk_positions.empty()) return;

    auto get_neighbor = [this](const ChunkPos& npos) -> const Chunk* {
        std::lock_guard<std::mutex> lock(m_world_mutex);
        auto it = m_chunks.find(npos);
        if (it != m_chunks.end()) {
            return it->second.get();
        }
        return nullptr;
    };

    std::vector<std::shared_ptr<Chunk>> chunks_to_mesh;
    chunks_to_mesh.reserve(chunk_positions.size());

    {
        std::lock_guard<std::mutex> lock(m_world_mutex);
        for (const auto& pos : chunk_positions) {
            auto it = m_chunks.find(pos);
            if (it != m_chunks.end() && it->second) {
                it->second->increment_mesh_version();
                it->second->clear_mesh_dirty();
                chunks_to_mesh.push_back(it->second);
            }
        }
    }

    // Remesh each chunk synchronously and upload to GPU immediately
    for (auto& chunk : chunks_to_mesh) {
        std::vector<PackedVoxelVertex> mesh = GreedyMesher::generate_mesh(*chunk, get_neighbor);
        chunk->stage_mesh(std::move(mesh));
        chunk->upload_mesh();
    }

    // Erase these chunk positions from pending upload tracking
    {
        std::lock_guard<std::mutex> lock(m_upload_mutex);
        for (const auto& pos : chunk_positions) {
            m_upload_queued_set.erase(pos);
            m_pending_upload_map.erase(pos);
        }
    }
}

void World::sync_remesh_box(const glm::ivec3& min_block, const glm::ivec3& max_block) {
    std::unordered_set<ChunkPos, ChunkPosHash> affected_chunks;
    int min_cx = floor_div(min_block.x - 1, CHUNK_SIZE);
    int max_cx = floor_div(max_block.x + 1, CHUNK_SIZE);
    int min_cy = floor_div(min_block.y - 1, CHUNK_SIZE);
    int max_cy = floor_div(max_block.y + 1, CHUNK_SIZE);
    int min_cz = floor_div(min_block.z - 1, CHUNK_SIZE);
    int max_cz = floor_div(max_block.z + 1, CHUNK_SIZE);

    for (int cz = min_cz; cz <= max_cz; ++cz) {
        for (int cy = min_cy; cy <= max_cy; ++cy) {
            for (int cx = min_cx; cx <= max_cx; ++cx) {
                affected_chunks.insert(ChunkPos{cx, cy, cz});
            }
        }
    }

    sync_remesh_chunks(affected_chunks);
}

void World::set_frustum_planes(const std::array<glm::vec4, 6>& planes) {
    m_frustum_planes = planes;
    m_has_frustum = true;
}

void World::set_camera_frustum(const glm::mat4& view_proj) {
    glm::vec4 row0 = glm::row(view_proj, 0);
    glm::vec4 row1 = glm::row(view_proj, 1);
    glm::vec4 row2 = glm::row(view_proj, 2);
    glm::vec4 row3 = glm::row(view_proj, 3);

    m_frustum_planes[0] = row3 + row0; // Left
    m_frustum_planes[1] = row3 - row0; // Right
    m_frustum_planes[2] = row3 + row1; // Bottom
    m_frustum_planes[3] = row3 - row1; // Top
    m_frustum_planes[4] = row3 + row2; // Near
    m_frustum_planes[5] = row3 - row2; // Far

    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(m_frustum_planes[i]));
        if (len > 1e-6f) {
            m_frustum_planes[i] /= len;
        }
    }
    m_has_frustum = true;
}

bool World::is_box_in_frustum(const glm::vec3& min_pt, const glm::vec3& max_pt) const {
    if (!m_has_frustum) return true;
    for (int i = 0; i < 6; ++i) {
        glm::vec3 p(
            (m_frustum_planes[i].x > 0.0f) ? max_pt.x : min_pt.x,
            (m_frustum_planes[i].y > 0.0f) ? max_pt.y : min_pt.y,
            (m_frustum_planes[i].z > 0.0f) ? max_pt.z : min_pt.z
        );
        if (glm::dot(glm::vec3(m_frustum_planes[i]), p) + m_frustum_planes[i].w < 0.0f) {
            return false;
        }
    }
    return true;
}

bool World::is_vertical_occluded(const ChunkPos& chunkPos, int playerChunkY) const {
    int dy = chunkPos.y - playerChunkY;
    if (std::abs(dy) <= 3) {
        return false;
    }

    int ceiling_chunk_y = (m_chunk_ceiling + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (dy > 0 && chunkPos.y >= ceiling_chunk_y) {
        return true;
    }
    if (dy < 0 && chunkPos.y < 0) {
        return true;
    }

    int step = (dy > 0) ? 1 : -1;
    for (int y = playerChunkY + step; y != chunkPos.y; y += step) {
        ChunkPos mid_pos{chunkPos.x, y, chunkPos.z};
        auto it = m_chunks.find(mid_pos);
        if (it != m_chunks.end() && it->second) {
            if (it->second->is_fully_solid()) {
                return true;
            }
            if (dy > 0 && y >= ceiling_chunk_y) {
                return true;
            }
            if (dy < 0 && y < 0) {
                return true;
            }
        } else {
            if ((dy > 0 && y >= ceiling_chunk_y) || (dy < 0 && y < 0)) {
                return true;
            }
        }
    }

    return false;
}

void World::render() {
    render(m_playerSpawnPos);
}

void World::render(const glm::vec3& player_pos) {
    render([](const Chunk& chunk) {
        chunk.render();
    }, player_pos);
}

void World::render(const std::function<void(const Chunk&)>& draw_fn, const glm::vec3& player_pos) {
    m_rendered_chunks_count = 0;
    m_culled_chunks_count = 0;
    int playerChunkY = floor_div(static_cast<int>(player_pos.y), CHUNK_SIZE);

    std::lock_guard<std::mutex> lock(m_world_mutex);
    for (const auto& [pos, chunk] : m_chunks) {
        if (!chunk || (chunk->vertex_count() == 0 && !chunk->has_staged_mesh())) {
            m_culled_chunks_count++;
            continue;
        }

        // Vertical occlusion culling
        if (std::abs(pos.y - playerChunkY) > 3 && is_vertical_occluded(pos, playerChunkY)) {
            m_culled_chunks_count++;
            continue;
        }

        // Camera view frustum culling
        glm::vec3 min_pt = chunk->get_world_pos();
        glm::vec3 max_pt = min_pt + glm::vec3(static_cast<float>(CHUNK_SIZE));
        if (m_has_frustum && !is_box_in_frustum(min_pt, max_pt)) {
            m_culled_chunks_count++;
            continue;
        }

        if (draw_fn) {
            draw_fn(*chunk);
        } else {
            chunk->render();
        }
        m_rendered_chunks_count++;
    }
}

bool World::borders_hanging_overhang_or_stalactite(int x, int y, int z) const {
    const glm::ivec3 neighbor_dirs[6] = {
        glm::ivec3( 1,  0,  0),
        glm::ivec3(-1,  0,  0),
        glm::ivec3( 0,  1,  0),
        glm::ivec3( 0, -1,  0),
        glm::ivec3( 0,  0,  1),
        glm::ivec3( 0,  0, -1)
    };

    for (const auto& dir : neighbor_dirs) {
        int nx = x + dir.x;
        int ny = y + dir.y;
        int nz = z + dir.z;

        if (is_solid(nx, ny, nz)) {
            // Stalactite hanging below or overhang with empty space beneath
            if (dir.y < 0 || !is_solid(nx, ny - 1, nz)) {
                return true;
            }
        }
    }
    return false;
}

std::vector<UnanchoredIsland> World::solve_structural_collapse(int x, int y, int z, size_t max_depth) {
    return StructuralCheck::solve_cavein(*this, x, y, z, max_depth);
}

bool World::break_voxel(int world_x, int world_y, int world_z) {
    Voxel v = get_voxel(world_x, world_y, world_z);
    if (!v.is_solid() || v.is_anchored()) {
        return false;
    }

    // Set destroyed voxel to air
    set_voxel(world_x, world_y, world_z, Voxel{MAT_AIR, 0}, true);
    check_wake_fluid_around(glm::ivec3(world_x, world_y, world_z));

    // If it borders a hanging ceiling overhang or stalactite, perform bounded BFS
    if (borders_hanging_overhang_or_stalactite(world_x, world_y, world_z)) {
        auto islands = solve_structural_collapse(world_x, world_y, world_z, 64);
        for (const auto& island : islands) {
            uint32_t did = m_next_debris_id++;
            glm::vec3 vel(0.0f, -1.5f, 0.0f);
            glm::vec3 rot(0.2f, 0.5f, 0.1f);
            DynamicDebris deb(
                did,
                island.center_of_mass,
                vel,
                rot,
                island.blocks,
                island.primary_material,
                this
            );
            add_debris(std::move(deb));
        }
    }
    return true;
}

void World::despawn_or_revoxelize_oldest_debris(bool force_revoxelize) {
    if (m_debris.empty()) return;
    auto& oldest = m_debris.front();
    if (force_revoxelize || oldest.is_sleeping()) {
        oldest.re_voxelize_blocks(*this);
    }
    m_debris.erase(m_debris.begin());
    EntityManager::active_debris_count = m_debris.size();
}

void World::add_debris(DynamicDebris&& d) {
    while (m_debris.size() >= MAX_ACTIVE_DEBRIS) {
        // Recycle / despawn or re-voxelize oldest active debris entity
        despawn_or_revoxelize_oldest_debris();
    }
    d.set_world(this);
    m_debris.push_back(std::move(d));
    EntityManager::active_debris_count = m_debris.size();
}

void World::clear_debris() {
    m_debris.clear();
    m_next_debris_id = 1;
    EntityManager::active_debris_count = 0;
}

void World::remove_destroyed_debris() {
    for (auto it = m_debris.begin(); it != m_debris.end();) {
        if (it->is_destroyed()) {
            it = m_debris.erase(it);
        } else {
            ++it;
        }
    }
    EntityManager::active_debris_count = m_debris.size();
}

void World::spawn_tremor_debris(const glm::vec3& pos, uint8_t mat_id, const glm::vec3& vel) {
    uint32_t did = m_next_debris_id++;
    DynamicDebris deb(did, pos, vel, glm::vec3(0.2f, 0.5f, 0.1f), mat_id, 1, this);
    add_debris(std::move(deb));
}

void World::update_debris(float dt, const glm::vec3& player_pos, bool is_player_sheltered) {
    for (auto it = m_debris.begin(); it != m_debris.end();) {
        it->update(dt, *this, player_pos, is_player_sheltered);
        if (it->is_destroyed()) {
            it = m_debris.erase(it);
        } else {
            ++it;
        }
    }
    EntityManager::active_debris_count = m_debris.size();
}

bool World::DestroyBlock(const glm::ivec3& pos) {
    bool broke = break_voxel(pos.x, pos.y, pos.z);
    if (!broke) {
        SetBlock(pos, MAT_AIR);
        broke = true;
    }
    const glm::ivec3 offsets[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };
    for (const auto& offset : offsets) {
        glm::ivec3 neighbor = pos + offset;
        uint8_t mat = GetBlockMaterial(neighbor);
        if (IsLiquid(mat)) {
            m_fluidSim.WakeFluid(neighbor);
        }
    }
    return broke;
}

uint8_t World::GetBlockMaterial(const glm::ivec3& pos) const {
    return get_voxel(pos.x, pos.y, pos.z).material_id;
}

void World::check_wake_fluid_around(const glm::ivec3& pos) {
    const glm::ivec3 offsets[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };
    for (const auto& offset : offsets) {
        glm::ivec3 neighbor = pos + offset;
        uint8_t mat = GetBlockMaterial(neighbor);
        Voxel nv = get_voxel(neighbor.x, neighbor.y, neighbor.z);
        if (IsLiquid(mat) || nv.is_waterlogged()) {
            m_fluidSim.WakeFluid(neighbor);
        }
    }
}

void World::set_fluid_cell(const glm::ivec3& pos, uint8_t mat, uint8_t level, bool is_waterlogged) {
    int cx = floor_div(pos.x, CHUNK_SIZE);
    int cy = floor_div(pos.y, CHUNK_SIZE);
    int cz = floor_div(pos.z, CHUNK_SIZE);
    ChunkPos cpos{cx, cy, cz};
    Chunk* chunk = get_or_create_chunk(cpos);
    if (!chunk) return;

    int lx = floor_mod(pos.x, CHUNK_SIZE);
    int ly = floor_mod(pos.y, CHUNK_SIZE);
    int lz = floor_mod(pos.z, CHUNK_SIZE);

    Voxel v;
    v.material_id = mat;
    v.flags_and_damage = (level & 0x07);
    if (is_waterlogged) {
        v.flags_and_damage |= VOXEL_FLAG_WATERLOGGED;
    }
    chunk->set_voxel(lx, ly, lz, v);
    chunk->MarkDirty();

    // Mesher Throttling: collect modified chunks in m_fluidDirtyChunks
    m_fluidDirtyChunks.insert(chunk);
    if (lx == 0)              { Chunk* c = get_chunk(ChunkPos{cx - 1, cy, cz}); if (c) m_fluidDirtyChunks.insert(c); }
    if (lx == CHUNK_SIZE - 1) { Chunk* c = get_chunk(ChunkPos{cx + 1, cy, cz}); if (c) m_fluidDirtyChunks.insert(c); }
    if (ly == 0)              { Chunk* c = get_chunk(ChunkPos{cx, cy - 1, cz}); if (c) m_fluidDirtyChunks.insert(c); }
    if (ly == CHUNK_SIZE - 1) { Chunk* c = get_chunk(ChunkPos{cx, cy + 1, cz}); if (c) m_fluidDirtyChunks.insert(c); }
    if (lz == 0)              { Chunk* c = get_chunk(ChunkPos{cx, cy, cz - 1}); if (c) m_fluidDirtyChunks.insert(c); }
    if (lz == CHUNK_SIZE - 1) { Chunk* c = get_chunk(ChunkPos{cx, cy, cz + 1}); if (c) m_fluidDirtyChunks.insert(c); }
}

void World::set_waterlogged_cell(const glm::ivec3& pos, bool state) {
    int cx = floor_div(pos.x, CHUNK_SIZE);
    int cy = floor_div(pos.y, CHUNK_SIZE);
    int cz = floor_div(pos.z, CHUNK_SIZE);
    ChunkPos cpos{cx, cy, cz};
    Chunk* chunk = get_or_create_chunk(cpos);
    if (!chunk) return;

    int lx = floor_mod(pos.x, CHUNK_SIZE);
    int ly = floor_mod(pos.y, CHUNK_SIZE);
    int lz = floor_mod(pos.z, CHUNK_SIZE);

    chunk->SetWaterlogged(lx, ly, lz, state);
    chunk->MarkDirty();

    m_fluidDirtyChunks.insert(chunk);
    if (lx == 0)              { Chunk* c = get_chunk(ChunkPos{cx - 1, cy, cz}); if (c) m_fluidDirtyChunks.insert(c); }
    if (lx == CHUNK_SIZE - 1) { Chunk* c = get_chunk(ChunkPos{cx + 1, cy, cz}); if (c) m_fluidDirtyChunks.insert(c); }
    if (ly == 0)              { Chunk* c = get_chunk(ChunkPos{cx, cy - 1, cz}); if (c) m_fluidDirtyChunks.insert(c); }
    if (ly == CHUNK_SIZE - 1) { Chunk* c = get_chunk(ChunkPos{cx, cy + 1, cz}); if (c) m_fluidDirtyChunks.insert(c); }
    if (lz == 0)              { Chunk* c = get_chunk(ChunkPos{cx, cy, cz - 1}); if (c) m_fluidDirtyChunks.insert(c); }
    if (lz == CHUNK_SIZE - 1) { Chunk* c = get_chunk(ChunkPos{cx, cy, cz + 1}); if (c) m_fluidDirtyChunks.insert(c); }
}

void World::flush_fluid_dirty_chunks() {
    for (Chunk* c : m_fluidDirtyChunks) {
        if (c) {
            c->MarkDirty();
            queue_chunk_for_meshing(c->get_pos());
        }
    }
    m_fluidDirtyChunks.clear();
}

void World::settle_fluids_offline(int max_ticks) {
    constexpr float TICK_INTERVAL = 0.0833f;
    int ticks = 0;
    while ((!m_fluidSim.m_activeFluids.empty() || !m_fluidSim.m_scheduledSpreads.empty()) && ticks < max_ticks) {
        m_fluidSim.Update(TICK_INTERVAL, *this);
        ticks++;
    }

    // Flush any pending scheduled spreads into world blocks immediately
    while (!m_fluidSim.m_scheduledSpreads.empty()) {
        auto upd = m_fluidSim.m_scheduledSpreads.front();
        m_fluidSim.m_scheduledSpreads.pop_front();
        std::unordered_set<Chunk*> dirtyChunks;
        m_fluidSim.TrySpreadToNeighbor(*this, upd.pos, upd.material, upd.level, dirtyChunks, true);
    }

    // Sleep on Spawn: Ensure spawn queues are completely empty so liquids cost 0 ms CPU until disturbed
    m_fluidSim.m_activeFluids.clear();
    m_fluidSim.m_activeFluidSet.clear();
    m_fluidSim.m_scheduledSpreads.clear();
    m_fluidSim.m_scheduledSpreadMap.clear();
    m_fluidSim.m_accumulator = 0.0f;
    m_fluidDirtyChunks.clear();
}

int World::count_material(uint8_t mat_id) const {
    std::lock_guard<std::mutex> lock(m_world_mutex);
    int total = 0;
    for (const auto& [cpos, chunk] : m_chunks) {
        if (!chunk || chunk->is_empty()) continue;
        const Voxel* voxels = chunk->raw_voxels();
        for (size_t i = 0; i < CHUNK_VOLUME; ++i) {
            if (voxels[i].material_id == mat_id) {
                total++;
            }
        }
    }
    return total;
}

std::vector<glm::ivec3> World::query_material_positions(uint8_t mat_id, size_t max_results) const {
    std::lock_guard<std::mutex> lock(m_world_mutex);
    std::vector<glm::ivec3> results;
    results.reserve(std::min(max_results, size_t(128)));

    for (const auto& [cpos, chunk] : m_chunks) {
        if (!chunk || chunk->is_empty()) continue;
        const Voxel* voxels = chunk->raw_voxels();
        int base_x = cpos.x * CHUNK_SIZE;
        int base_y = cpos.y * CHUNK_SIZE;
        int base_z = cpos.z * CHUNK_SIZE;

        for (size_t i = 0; i < CHUNK_VOLUME; ++i) {
            if (voxels[i].material_id == mat_id) {
                int lx, ly, lz;
                Chunk::from_index(i, lx, ly, lz);
                results.push_back(glm::ivec3(base_x + lx, base_y + ly, base_z + lz));
                if (results.size() >= max_results) {
                    return results;
                }
            }
        }
    }
    return results;
}

void World::verify_and_seed_objective_ores(int target_astral) {
    if (target_astral <= 0) {
        return;
    }

    int current_astral = count_material(MAT_ASTRAL_ORE);
    int required_quota = target_astral + 2; // Guarantee objective completion + extra margin
    if (current_astral >= required_quota) {
        VF_LOG_INFO("World", "Objective ore verification passed: found " << current_astral
                             << " Astral Ore blocks (quota=" << target_astral << ")");
        return;
    }

    int needed = required_quota - current_astral;
    VF_LOG_INFO("World", "Objective ore verification: spawning " << needed
                         << " guaranteed Astral Ore deposits (current=" << current_astral
                         << ", quota=" << target_astral << ")");

    int placed = 0;
    const glm::ivec3 dirs[6] = {
        { 1, 0, 0 }, {-1, 0, 0 },
        { 0, 1, 0 }, { 0,-1, 0 },
        { 0, 0, 1 }, { 0, 0,-1 }
    };

    if (!m_level_gen || m_level_gen->rooms().empty()) {
        // Fallback for custom or direct test worlds: scan existing chunks for solid blocks adjacent to air
        for (const auto& [cpos, chunk] : m_chunks) {
            if (!chunk) continue;
            for (int ly = 1; ly < CHUNK_SIZE - 1 && placed < needed; ++ly) {
                for (int lz = 1; lz < CHUNK_SIZE - 1 && placed < needed; ++lz) {
                    for (int lx = 1; lx < CHUNK_SIZE - 1 && placed < needed; ++lx) {
                        Voxel v = chunk->get_voxel(lx, ly, lz);
                        if (v.material_id == MAT_AIR || v.material_id == MAT_DREDGE_BEDROCK ||
                            v.material_id == MAT_ASTRAL_ORE) {
                            continue;
                        }
                        // Check if adjacent to air
                        bool near_air = false;
                        for (int d = 0; d < 6; ++d) {
                            Voxel n = chunk->get_voxel(lx + dirs[d].x, ly + dirs[d].y, lz + dirs[d].z);
                            if (n.material_id == MAT_AIR) {
                                near_air = true;
                                break;
                            }
                        }
                        if (near_air) {
                            int wx = cpos.x * CHUNK_SIZE + lx;
                            int wy = cpos.y * CHUNK_SIZE + ly;
                            int wz = cpos.z * CHUNK_SIZE + lz;
                            set_block(wx, wy, wz, MAT_ASTRAL_ORE, VOXEL_FLAG_EMISSIVE);
                            placed++;
                        }
                    }
                }
            }
        }
        // If still needed (e.g. all-air test world), spawn guaranteed ore clusters directly
        if (placed < needed) {
            for (int i = placed; i < needed; ++i) {
                int wx = 10 + (i % 5) * 2;
                int wy = 10 + (i / 5) * 2;
                int wz = 10;
                set_block(wx, wy, wz, MAT_ASTRAL_ORE, VOXEL_FLAG_EMISSIVE);
                placed++;
            }
        }
        VF_LOG_INFO("World", "Objective ore verification complete (fallback): seeded " << placed
                             << " Astral Ore blocks. Total now: " << count_material(MAT_ASTRAL_ORE));
        return;
    }

    const auto& rooms = m_level_gen->rooms();
    std::vector<size_t> distant_rooms;
    for (size_t i = 0; i < rooms.size(); ++i) {
        int dist = rooms[i].grid_x + rooms[i].grid_z;
        if (dist >= 2 && rooms[i].type != RoomShapeType::ExtractionLandingBay) {
            distant_rooms.push_back(i);
        }
    }
    if (distant_rooms.empty()) {
        for (size_t i = 1; i < rooms.size(); ++i) {
            distant_rooms.push_back(i);
        }
    }

    size_t room_idx_cursor = 0;

    while (placed < needed && !distant_rooms.empty()) {
        size_t r_idx = distant_rooms[room_idx_cursor % distant_rooms.size()];
        room_idx_cursor++;
        const auto& room = rooms[r_idx];

        // Search for a solid, mineable rock face exposed to open air
        bool placed_in_room = false;
        int min_x = std::max(4, room.center.x - room.half_width);
        int max_x = std::min(world_width() - 5, room.center.x + room.half_width);
        int min_z = std::max(4, room.center.z - room.half_depth);
        int max_z = std::min(world_depth() - 5, room.center.z + room.half_depth);
        int min_y = std::max(4, room.floor_y + 1);
        int max_y = std::min(world_height() - 5, room.ceiling_y - 1);

        for (int y = min_y; y <= max_y && !placed_in_room; ++y) {
            for (int z = min_z; z <= max_z && !placed_in_room; ++z) {
                for (int x = min_x; x <= max_x && !placed_in_room; ++x) {
                    Voxel current = get_voxel(x, y, z);
                    if (!current.is_solid() || current.material_id == MAT_DREDGE_BEDROCK ||
                        current.material_id == MAT_VOIDITE_CRYSTAL ||
                        current.material_id == MAT_ASTRAL_ORE ||
                        current.material_id == MAT_VAULT_DOOR ||
                        current.material_id == MAT_REINFORCED_VAULT_DOOR) {
                        continue;
                    }

                    // Check if adjacent to air so it's visibly exposed and reachable
                    bool near_air = false;
                    for (int d = 0; d < 6; ++d) {
                        Voxel n = get_voxel(x + dirs[d].x, y + dirs[d].y, z + dirs[d].z);
                        if (n.material_id == MAT_AIR) {
                            near_air = true;
                            break;
                        }
                    }

                    if (near_air) {
                        set_block(x, y, z, MAT_ASTRAL_ORE, VOXEL_FLAG_EMISSIVE);
                        placed++;
                        placed_in_room = true;
                    }
                }
            }
        }

        // Failsafe exit if entire scan yielded no room spot
        if (room_idx_cursor > distant_rooms.size() * 3 && !placed_in_room) {
            break;
        }
    }

    VF_LOG_INFO("World", "Objective ore verification complete: seeded " << placed
                         << " Astral Ore blocks. Total now: " << count_material(MAT_ASTRAL_ORE));
}

} // namespace Voidfall
