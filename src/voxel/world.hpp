#pragma once
#include "chunk.hpp"
#include "greedy_mesher.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <atomic>

namespace Voidfall {

struct RaycastHit {
    bool hit{false};
    glm::ivec3 block_pos{0};
    ChunkPos chunk_pos{0, 0, 0};
    int local_x{0};
    int local_y{0};
    int local_z{0};
    glm::ivec3 normal{0};
    Voxel voxel{MAT_AIR, 0};
    float distance{0.0f};
};

class World {
public:
    explicit World(uint32_t seed = 1337);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    uint32_t seed() const { return m_seed; }
    void set_seed(uint32_t seed);

    Chunk* get_chunk(const ChunkPos& pos);
    const Chunk* get_chunk(const ChunkPos& pos) const;

    Chunk* get_or_create_chunk(const ChunkPos& pos);

    Voxel get_voxel(int world_x, int world_y, int world_z) const;
    bool set_voxel(int world_x, int world_y, int world_z, Voxel v, bool mark_neighbors = true);

    // Raycast through voxel grid (DDA algorithm)
    RaycastHit raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance = 12.0f) const;

    // Updates chunk generation & meshing around a target position
    void update(const glm::vec3& viewer_pos, int render_distance = 3);

    // Upload newly meshed chunks to GPU (called on render thread)
    void upload_dirty_chunks();

    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash>& chunks() const {
        return m_chunks;
    }

private:
    void generate_chunk_terrain(Chunk& chunk);
    float sample_cavern_noise(float x, float y, float z) const;

    // Worker thread meshing queue
    void worker_thread_loop();
    void queue_chunk_for_meshing(const ChunkPos& pos);

    uint32_t m_seed{1337};
    mutable std::mutex m_world_mutex;
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> m_chunks;

    // Background meshing thread pool
    std::vector<std::thread> m_workers;
    std::queue<ChunkPos> m_mesh_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::atomic<bool> m_running{true};
};

} // namespace Voidfall
