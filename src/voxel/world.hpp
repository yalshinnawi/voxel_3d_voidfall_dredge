#pragma once
#include "chunk.hpp"
#include "greedy_mesher.hpp"
#include "level_shapes.hpp"
#include "../entities/enemies/void_stalker.hpp"
#include "../entities/vault_door.hpp"
#include "../entities/dynamic_debris.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <atomic>

namespace Voidfall {

struct UnanchoredIsland;

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

struct FaunaEntity {
    glm::vec3 pos{0.0f};
    glm::vec3 position{0.0f};
    AIState state{AIState::ROOSTING};
    StalkerRole role{StalkerRole::Melee};
    uint32_t id{0};
};

struct EntityManager {
    static constexpr size_t MAX_ACTIVE_DEBRIS = 36;
};

class World {
public:
    explicit World(uint32_t seed = 1337, bool enable_background_meshing = true);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    uint32_t seed() const { return m_seed; }
    void set_seed(uint32_t seed);
    void generate_world(int sector_index, uint32_t seed = 0);
    void set_level_generator(std::unique_ptr<LevelGenerator> gen);
    int sector_index() const { return m_sector_index; }

    void PopulateFauna();
    void GenerateSectorStructures(int sector_index);

    const std::vector<FaunaEntity>& GetActiveEntities() const { return m_active_entities; }
    std::vector<FaunaEntity>& GetActiveEntities() { return m_active_entities; }

    const glm::vec3& GetPlayerSpawnPos() const { return m_playerSpawnPos; }
    void SetPlayerSpawnPos(const glm::vec3& p) { m_playerSpawnPos = p; }

    const VaultDoor& vault_door() const { return m_vault_door; }
    VaultDoor& vault_door_mut() { return m_vault_door; }

    Chunk* get_chunk(const ChunkPos& pos);
    const Chunk* get_chunk(const ChunkPos& pos) const;

    Chunk* get_or_create_chunk(const ChunkPos& pos);

    Voxel get_voxel(int world_x, int world_y, int world_z) const;
    bool set_voxel(int world_x, int world_y, int world_z, Voxel v, bool mark_neighbors = true);
    bool set_block_with_flags(const glm::ivec3& pos, uint8_t mat, uint8_t flags);
    bool SetBlockWithFlags(const glm::ivec3& pos, uint8_t mat, uint8_t flags) {
        return set_block_with_flags(pos, mat, flags);
    }
    bool SetBlock(int x, int y, int z, uint8_t mat, uint8_t flags = 0);
    bool SetBlock(const glm::ivec3& pos, uint8_t mat, uint8_t flags = 0);
    bool set_block(int x, int y, int z, uint8_t mat, uint8_t flags = 0);
    bool set_block(const glm::ivec3& pos, uint8_t mat, uint8_t flags = 0);
    uint8_t get_block_flags(const glm::ivec3& pos) const;
    uint8_t GetBlockFlags(const glm::ivec3& pos) const { return get_block_flags(pos); }

    bool is_solid(const glm::ivec3& pos) const;
    bool is_solid(int world_x, int world_y, int world_z) const;
    bool IsSolid(const glm::ivec3& pos) const { return is_solid(pos); }
    bool IsSolid(int world_x, int world_y, int world_z) const { return is_solid(world_x, world_y, world_z); }

    bool is_liquid(const glm::ivec3& pos) const;
    bool is_liquid(int world_x, int world_y, int world_z) const;
    bool IsLiquid(const glm::ivec3& pos) const { return is_liquid(pos); }
    bool IsLiquid(int world_x, int world_y, int world_z) const { return is_liquid(world_x, world_y, world_z); }

    float get_highest_solid_surface(int x, int z) const;

    // Raycast through voxel grid (DDA algorithm)
    RaycastHit raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance = 12.0f) const;

    // Mesh upload throttling
    static constexpr size_t MAX_CHUNK_UPLOADS_PER_FRAME = 2;

    // Updates chunk generation & meshing around a target position, and throttles GPU uploads
    void update(const glm::vec3& viewer_pos, int render_distance = 3);
    void update();

    // PascalCase aliases
    void Update(const glm::vec3& viewer_pos, int render_distance = 3) { update(viewer_pos, render_distance); }
    void Update() { update(); }

    // Upload newly meshed chunks to GPU (called on render thread, throttled by MAX_CHUNK_UPLOADS_PER_FRAME)
    size_t upload_dirty_chunks(size_t max_uploads = MAX_CHUNK_UPLOADS_PER_FRAME);
    size_t upload_mesh_queue(size_t max_uploads = MAX_CHUNK_UPLOADS_PER_FRAME);
    void queue_chunk_for_upload(const ChunkPos& pos);
    size_t upload_queue_size() const;
    size_t pending_upload_count() const { return upload_queue_size(); }

    // Synchronously meshes and uploads all chunks immediately (for staging/testing)
    void force_mesh_all_sync();

    const std::unordered_map<ChunkPos, std::shared_ptr<Chunk>, ChunkPosHash>& chunks() const {
        return m_chunks;
    }

    const LevelGenerator* level_generator() const { return m_level_gen.get(); }

    // Spatial proximity radiation query (samples active radioactive ores & vents)
    float query_radiation_proximity(const glm::vec3& pos, float max_radius = 12.0f) const;

    const std::vector<glm::ivec3>& radioactive_sources() const { return m_radioactive_sources; }
    const std::vector<glm::ivec3>& toxic_gas_sources() const { return m_toxic_gas_sources; }
    const std::vector<glm::ivec3>& lava_sources() const { return m_lava_sources; }
    const std::vector<glm::ivec3>& aquifer_sources() const { return m_aquifer_sources; }

    // Anchored-Island BFS structural collapse system
    static constexpr int DEFAULT_CHUNK_CEILING = 64;
    int chunk_ceiling() const { return m_chunk_ceiling; }
    void set_chunk_ceiling(int c) { m_chunk_ceiling = c; }

    bool borders_hanging_overhang_or_stalactite(int x, int y, int z) const;
    bool break_voxel(int world_x, int world_y, int world_z);
    std::vector<UnanchoredIsland> solve_structural_collapse(int x, int y, int z, size_t max_depth = 64);

    // Dynamic Debris Entity Management
    static constexpr size_t MAX_ACTIVE_DEBRIS = 36;
    std::vector<DynamicDebris>& debris() { return m_debris; }
    const std::vector<DynamicDebris>& debris() const { return m_debris; }
    void add_debris(DynamicDebris&& d);
    void spawn_debris(DynamicDebris&& d) { add_debris(std::move(d)); }
    void spawn_tremor_debris(const glm::vec3& pos, uint8_t mat_id = MAT_FRACTURED_GRANITE, const glm::vec3& vel = glm::vec3(0.0f, -2.0f, 0.0f));
    void clear_debris() { m_debris.clear(); m_next_debris_id = 1; }
    void update_debris(float dt, const glm::vec3& player_pos = glm::vec3(-9999.0f), bool is_player_sheltered = false);

private:
    void generate_chunk_terrain(Chunk& chunk);
    float sample_cavern_noise(float x, float y, float z) const;

    // Worker thread meshing queue
    void worker_thread_loop();
    void queue_chunk_for_meshing(const ChunkPos& pos);

    uint32_t m_seed{1337};
    int m_sector_index{1};
    int m_chunk_ceiling{DEFAULT_CHUNK_CEILING};
    uint32_t m_next_debris_id{1};
    std::vector<DynamicDebris> m_debris;
    glm::vec3 m_noise_offset{0.0f};
    std::unique_ptr<LevelGenerator> m_level_gen;
    mutable std::mutex m_world_mutex;
    std::unordered_map<ChunkPos, std::shared_ptr<Chunk>, ChunkPosHash> m_chunks;
    std::vector<glm::ivec3> m_radioactive_sources;
    std::vector<glm::ivec3> m_toxic_gas_sources;
    std::vector<glm::ivec3> m_lava_sources;
    std::vector<glm::ivec3> m_aquifer_sources;

    glm::vec3 m_playerSpawnPos{16.0f, 5.1f, 16.0f};
    std::vector<FaunaEntity> m_active_entities;
    VaultDoor m_vault_door;

    bool is_background_meshing_enabled() const { return m_enable_background_meshing; }

    // Background meshing thread pool
    bool m_enable_background_meshing{true};
    std::vector<std::thread> m_workers;
    std::queue<ChunkPos> m_mesh_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::atomic<bool> m_running{true};

    // Mesh GPU upload queue
    mutable std::mutex m_upload_mutex;
    std::queue<ChunkPos> m_upload_queue;
    std::unordered_set<ChunkPos, ChunkPosHash> m_upload_queued_set;
    size_t m_uploads_this_frame{0};
};

} // namespace Voidfall
