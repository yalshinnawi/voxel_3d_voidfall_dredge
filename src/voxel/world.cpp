#include "world.hpp"
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

void World::generate_world(int sector_index, uint32_t seed) {
    m_sector_index = sector_index;
    uint32_t effective_seed = (seed != 0) ? seed : (static_cast<uint32_t>(sector_index) * 2654435761u + static_cast<uint32_t>(time(nullptr)));
    m_seed = effective_seed;
    m_level_gen = std::make_unique<LevelGenerator>(sector_index, effective_seed);

    // Flush stale mesh queue requests
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        std::queue<ChunkPos> empty_queue;
        std::swap(m_mesh_queue, empty_queue);
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
    PopulateFauna();

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

    for (size_t i = 1; i < rooms.size(); ++i) { // Room 0 is player insertion arrival bay
        glm::vec3 room_center(
            static_cast<float>(rooms[i].center.x),
            static_cast<float>(rooms[i].floor_y) + 1.2f,
            static_cast<float>(rooms[i].center.z)
        );

        // Insertion Quarantine Sphere: discard candidate spawn point within 28m
        if (!IsSpawnPointSafe(room_center, m_playerSpawnPos)) {
            continue;
        }

        StalkerRole role = (i % 2 == 0) ? StalkerRole::Melee : StalkerRole::Shooter;
        FaunaEntity e;
        e.id = next_id++;
        e.pos = room_center;
        e.position = room_center;
        e.state = AIState::ROOSTING;
        e.role = role;
        m_active_entities.push_back(e);

        if (m_sector_index >= 2 && (i % 2 == 0)) {
            glm::vec3 sec_pos = room_center + glm::vec3(3.5f, 0.0f, -3.5f);
            if (IsSpawnPointSafe(sec_pos, m_playerSpawnPos)) {
                FaunaEntity e2;
                e2.id = next_id++;
                e2.pos = sec_pos;
                e2.position = sec_pos;
                e2.state = AIState::ROOSTING;
                e2.role = StalkerRole::Melee;
                m_active_entities.push_back(e2);
            }
        }
    }
}

void World::GenerateSectorStructures(int sector_index) {
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

    // Floating Relic Hyper-Core Pedestal in vault center
    set_voxel(vx, vy, vz, Voxel{MAT_PRECURSOR_STONE, VOXEL_FLAG_ANCHORED}, false);
    set_voxel(vx, vy + 1, vz, Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE}, false);
}

void World::set_level_generator(std::unique_ptr<LevelGenerator> gen) {
    // Flush stale mesh queue requests
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        std::queue<ChunkPos> empty_queue;
        std::swap(m_mesh_queue, empty_queue);
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
    PopulateFauna();

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

Chunk* World::get_or_create_chunk(const ChunkPos& pos) {
    std::lock_guard<std::mutex> lock(m_world_mutex);
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        return it->second.get();
    }

    auto chunk = std::make_shared<Chunk>(pos);
    generate_chunk_terrain(*chunk);
    Chunk* raw = chunk.get();
    m_chunks[pos] = std::move(chunk);

    queue_chunk_for_meshing(pos);
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

    queue_chunk_for_meshing(cpos);

    if (mark_neighbors) {
        if (lx == 0)              queue_chunk_for_meshing(ChunkPos{cpos.x - 1, cpos.y, cpos.z});
        if (lx == CHUNK_SIZE - 1) queue_chunk_for_meshing(ChunkPos{cpos.x + 1, cpos.y, cpos.z});
        if (ly == 0)              queue_chunk_for_meshing(ChunkPos{cpos.x, cpos.y - 1, cpos.z});
        if (ly == CHUNK_SIZE - 1) queue_chunk_for_meshing(ChunkPos{cpos.x, cpos.y + 1, cpos.z});
        if (lz == 0)              queue_chunk_for_meshing(ChunkPos{cpos.x, cpos.y, cpos.z - 1});
        if (lz == CHUNK_SIZE - 1) queue_chunk_for_meshing(ChunkPos{cpos.x, cpos.y, cpos.z + 1});
    }

    return true;
}

bool World::set_block_with_flags(const glm::ivec3& pos, uint8_t mat, uint8_t flags) {
    return set_voxel(pos.x, pos.y, pos.z, Voxel{mat, flags}, true);
}

uint8_t World::get_block_flags(const glm::ivec3& pos) const {
    return get_voxel(pos.x, pos.y, pos.z).flags_and_damage;
}

bool World::is_solid(const glm::ivec3& pos) const {
    if (pos.y < 0) return false;  // Out-of-bounds below is the lethal void singularity chasm, NOT solid!
    if (pos.y >= 26) return true; // Ceiling mantle layer is always solid

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
                } else if (v.material_id == MAT_CRYSTAL_AQUIFER || v.material_id == MAT_WATER || v.material_id == MAT_AQUIFER) {
                    m_aquifer_sources.push_back(glm::ivec3(wx, wy, wz));
                }
            }
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

void World::queue_chunk_for_meshing(const ChunkPos& pos) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_mesh_queue.push(pos);
    m_queue_cv.notify_one();
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

        std::vector<PackedVoxelVertex> mesh = GreedyMesher::generate_mesh(*chunk, get_neighbor);
        chunk->stage_mesh(std::move(mesh));
    }
}

void World::update(const glm::vec3& viewer_pos, int render_distance) {
    ChunkPos center{
        floor_div(static_cast<int>(viewer_pos.x), CHUNK_SIZE),
        floor_div(static_cast<int>(viewer_pos.y), CHUNK_SIZE),
        floor_div(static_cast<int>(viewer_pos.z), CHUNK_SIZE)
    };

    for (int dz = -render_distance; dz <= render_distance; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -render_distance; dx <= render_distance; ++dx) {
                ChunkPos p{center.x + dx, center.y + dy, center.z + dz};
                get_or_create_chunk(p);
            }
        }
    }
}

void World::upload_dirty_chunks() {
    std::lock_guard<std::mutex> lock(m_world_mutex);
    for (auto& [pos, chunk] : m_chunks) {
        chunk->upload_mesh();
    }
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
}

} // namespace Voidfall
