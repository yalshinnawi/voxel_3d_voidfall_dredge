#include "world.hpp"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <ctime>

namespace Voidfall {

static inline int floor_div(int a, int b) {
    int res = a / b;
    int rem = a % b;
    if (rem != 0 && ((a < 0) ^ (b < 0))) {
        res--;
    }
    return res;
}

static inline int floor_mod(int a, int b) {
    int res = a % b;
    if (res < 0) {
        res += b;
    }
    return res;
}

World::World(uint32_t seed)
    : m_seed(seed)
{
    // Start background meshing worker threads
    unsigned int num_threads = std::max(2u, std::thread::hardware_concurrency() / 2);
    for (unsigned int i = 0; i < num_threads; ++i) {
        m_workers.emplace_back(&World::worker_thread_loop, this);
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
    m_seed = seed;
    std::lock_guard<std::mutex> lock(m_world_mutex);
    m_chunks.clear();
}

void World::generate_world(int sector_index, uint32_t seed) {
    m_sector_index = sector_index;
    uint32_t effective_seed = (seed ^ (static_cast<uint32_t>(sector_index) * 2654435761u)) + static_cast<uint32_t>(time(nullptr));
    m_seed = effective_seed;
    m_noise_offset = glm::vec3(
        static_cast<float>(effective_seed % 10000),
        static_cast<float>((effective_seed / 10000) % 10000),
        static_cast<float>((effective_seed / 100) % 10000)
    );
    std::lock_guard<std::mutex> lock(m_world_mutex);
    m_chunks.clear();
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

    auto chunk = std::make_unique<Chunk>(pos);
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
    if (!chunk) return Voxel{MAT_AIR, 0};

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

    chunk->set_voxel(lx, ly, lz, v);
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

// 3D procedural noise synthesis for subterranean caverns
float World::sample_cavern_noise(float x, float y, float z) const {
    float ox = x + m_noise_offset.x;
    float oy = y + m_noise_offset.y;
    float oz = z + m_noise_offset.z;

    if (m_sector_index == 1) {
        // Broad open hollow chambers (smoother, lower frequency)
        float nx = ox * 0.025f;
        float ny = oy * 0.025f;
        float nz = oz * 0.025f;
        float n1 = std::sin(nx) * std::cos(ny) + std::sin(ny) * std::cos(nz) + std::sin(nz) * std::cos(nx);
        float n2 = (std::sin(nx * 1.8f + 1.2f) * std::cos(ny * 1.8f) +
                    std::sin(ny * 1.8f + 0.7f) * std::cos(nz * 1.8f)) * 0.4f;
        return n1 + n2;
    } else if (m_sector_index == 3) {
        // Narrow vertical fissures (elongated vertically, high horizontal frequency)
        float nx = ox * 0.07f;
        float ny = oy * 0.015f; // stretched vertically
        float nz = oz * 0.07f;
        float n1 = std::sin(nx) * std::cos(nz) + std::sin(nz * 1.4f) * std::cos(nx * 1.4f);
        float n2 = std::sin(nx * 2.1f + oy * 0.04f) * 0.5f;
        return n1 + n2;
    } else {
        // Sector 2 / default
        float nx = ox * 0.04f;
        float ny = oy * 0.04f;
        float nz = oz * 0.04f;
        float n1 = std::sin(nx) * std::cos(ny) + std::sin(ny) * std::cos(nz) + std::sin(nz) * std::cos(nx);
        float n2 = (std::sin(nx * 2.3f + 1.2f) * std::cos(ny * 2.3f) +
                    std::sin(ny * 2.3f + 0.7f) * std::cos(nz * 2.3f)) * 0.5f;
        float n3 = (std::sin(nx * 4.7f) * std::cos(nz * 4.7f)) * 0.25f;
        return n1 + n2 + n3;
    }
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

                // Bedrock base layer
                if (wy <= 0) {
                    chunk.set_voxel(x, y, z, Voxel{MAT_DREDGE_BEDROCK, 0x10});
                    continue;
                }

                // Guarantee open landing cavern around spawn point (16, 20, 16)
                float dist_to_spawn_sq = static_cast<float>((wx - 16)*(wx - 16) + (wy - 20)*(wy - 20) + (wz - 16)*(wz - 16));
                if (dist_to_spawn_sq < 64.0f && wy > 1) {
                    chunk.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                    continue;
                }

                float noise = sample_cavern_noise(static_cast<float>(wx), static_cast<float>(wy), static_cast<float>(wz));

                if (m_sector_index == 1) {
                    // Level 1 (Crystalline Caverns): Broad open hollow chambers, high MAT_VOIDITE crystal clusters
                    if (noise < 0.10f) {
                        chunk.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                        continue;
                    }

                    uint8_t mat = MAT_FRACTURED_GRANITE;
                    if (wy < 8) {
                        mat = MAT_VOLCANIC_BASALT;
                    }

                    // High voidite crystal clusters
                    float crystal_noise = std::sin((wx + m_noise_offset.x) * 0.16f) *
                                          std::cos((wy + m_noise_offset.y) * 0.16f) *
                                          std::sin((wz + m_noise_offset.z) * 0.16f);
                    if (crystal_noise > 0.58f) {
                        mat = MAT_VOIDITE_CRYSTAL;
                    } else if (crystal_noise < -0.85f) {
                        mat = MAT_TITANIUM;
                    }

                    chunk.set_voxel(x, y, z, Voxel{mat, 0});
                }
                else if (m_sector_index == 2) {
                    // Level 2 (Subterranean Vault): Linear corridors cut through dense basalt, embedded metal plates (MAT_TITANIUM), reinforced vault bulkheads (MAT_VAULT_DOOR)
                    int off_x = wx + static_cast<int>(m_noise_offset.x);
                    int off_z = wz + static_cast<int>(m_noise_offset.z);
                    int mod_x = floor_mod(off_x, 24);
                    int mod_z = floor_mod(off_z, 24);

                    bool is_corridor_x = (mod_z >= 10 && mod_z <= 14 && wy >= 4 && wy <= 16);
                    bool is_corridor_z = (mod_x >= 10 && mod_x <= 14 && wy >= 4 && wy <= 16);
                    bool is_corridor = is_corridor_x || is_corridor_z;

                    if (is_corridor) {
                        // Check for reinforced vault doors blocking corridor sections
                        bool is_door_x = is_corridor_x && (floor_mod(off_x, 48) == 0);
                        bool is_door_z = is_corridor_z && (floor_mod(off_z, 48) == 0);
                        if (is_door_x || is_door_z) {
                            chunk.set_voxel(x, y, z, Voxel{MAT_REINFORCED_VAULT_DOOR, 0x10});
                            continue;
                        }
                        chunk.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                        continue;
                    }

                    // Organic caverns
                    if (noise < -0.25f) {
                        chunk.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                        continue;
                    }

                    uint8_t mat = MAT_VOLCANIC_BASALT; // dense basalt

                    // Embedded metal plates / titanium lining
                    bool near_corridor_wall = (mod_z == 9 || mod_z == 15 || mod_x == 9 || mod_x == 15) && (wy >= 4 && wy <= 16);
                    if (near_corridor_wall) {
                        mat = MAT_TITANIUM;
                    } else {
                        float v_noise = std::sin((wx + m_noise_offset.x) * 0.20f) * std::sin((wz + m_noise_offset.z) * 0.20f);
                        if (v_noise > 0.72f) {
                            mat = MAT_TITANIUM;
                        } else if (v_noise < -0.80f) {
                            mat = MAT_VOIDITE_CRYSTAL;
                        }
                    }

                    chunk.set_voxel(x, y, z, Voxel{mat, 0});
                }
                else {
                    // Level 3 (Fault-Line Collapse): Narrow vertical fissures, sparse ground paths, frequent falling stalactites
                    float fissure = std::abs(std::sin((wx + m_noise_offset.x) * 0.08f) +
                                             std::cos((wz + m_noise_offset.z) * 0.08f));
                    if (fissure < 0.38f || noise < -0.22f) {
                        bool is_ledge = (wy % 12 == 1 && ((wx + wz) % 5 == 0));
                        if (!is_ledge) {
                            chunk.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                            continue;
                        }
                    }

                    uint8_t mat = MAT_FRACTURED_GRANITE;
                    if (wy < 14) {
                        mat = MAT_VOLCANIC_BASALT;
                    }

                    if (wy >= 18 && (wx % 6 == 0 && wz % 6 == 0)) {
                        mat = MAT_RADIOACTIVE_ORE;
                    } else {
                        float ore_noise = std::sin((wx + m_noise_offset.x) * 0.22f) * std::cos((wy + m_noise_offset.y) * 0.22f);
                        if (ore_noise > 0.75f) {
                            mat = MAT_VOIDITE_CRYSTAL;
                        } else if (ore_noise < -0.65f) {
                            mat = MAT_RADIOACTIVE_ORE;
                        }
                    }

                    chunk.set_voxel(x, y, z, Voxel{mat, 0});
                }
            }
        }
    }
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

        Chunk* chunk = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_world_mutex);
            auto it = m_chunks.find(target_pos);
            if (it != m_chunks.end()) {
                chunk = it->second.get();
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

} // namespace Voidfall
