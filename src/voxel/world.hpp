#pragma once
#include "chunk.hpp"
#include "greedy_mesher.hpp"
#include "level_shapes.hpp"
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
    void generate_world(int sector_index, uint32_t seed = 0);
    void set_level_generator(std::unique_ptr<LevelGenerator> gen);
    int sector_index() const { return m_sector_index; }

    Chunk* get_chunk(const ChunkPos& pos);
    const Chunk* get_chunk(const ChunkPos& pos) const;

    Chunk* get_or_create_chunk(const ChunkPos& pos);

    Voxel get_voxel(int world_x, int world_y, int world_z) const;
    bool set_voxel(int world_x, int world_y, int world_z, Voxel v, bool mark_neighbors = true);
    bool set_block_with_flags(const glm::ivec3& pos, uint8_t mat, uint8_t flags);
    bool SetBlockWithFlags(const glm::ivec3& pos, uint8_t mat, uint8_t flags) {
        return set_block_with_flags(pos, mat, flags);
    }
    uint8_t get_block_flags(const glm::ivec3& pos) const;
    uint8_t GetBlockFlags(const glm::ivec3& pos) const { return get_block_flags(pos); }

    bool is_solid(const glm::ivec3& pos) const;
    bool is_solid(int world_x, int world_y, int world_z) const;
    bool IsSolid(const glm::ivec3& pos) const { return is_solid(pos); }
    bool IsSolid(int world_x, int world_y, int world_z) const { return is_solid(world_x, world_y, world_z); }

    float get_highest_solid_surface(int x, int z) const;

    // Raycast through voxel grid (DDA algorithm)
    RaycastHit raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance = 12.0f) const;

    // Updates chunk generation & meshing around a target position
    void update(const glm::vec3& viewer_pos, int render_distance = 3);

    // Upload newly meshed chunks to GPU (called on render thread)
    void upload_dirty_chunks();

    // Synchronously meshes and uploads all chunks immediately (for staging/testing)
    void force_mesh_all_sync();

    const std::unordered_map<ChunkPos, std::shared_ptr<Chunk>, ChunkPosHash>& chunks() const {
        return m_chunks;
    }

    const LevelGenerator* level_generator() const { return m_level_gen.get(); }

    // Spatial proximity radiation query (samples active radioactive ores & vents)
    float query_radiation_proximity(const glm::vec3& pos, float max_radius = 12.0f) const;

private:
    void generate_chunk_terrain(Chunk& chunk);
    float sample_cavern_noise(float x, float y, float z) const;

    // Worker thread meshing queue
    void worker_thread_loop();
    void queue_chunk_for_meshing(const ChunkPos& pos);

    uint32_t m_seed{1337};
    int m_sector_index{1};
    glm::vec3 m_noise_offset{0.0f};
    std::unique_ptr<LevelGenerator> m_level_gen;
    mutable std::mutex m_world_mutex;
    std::unordered_map<ChunkPos, std::shared_ptr<Chunk>, ChunkPosHash> m_chunks;
    std::vector<glm::ivec3> m_radioactive_sources;

    // Background meshing thread pool
    std::vector<std::thread> m_workers;
    std::queue<ChunkPos> m_mesh_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::atomic<bool> m_running{true};
};

} // namespace Voidfall
