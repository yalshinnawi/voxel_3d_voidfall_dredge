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
#include <array>
#include <functional>
#include <thread>
#include <queue>
#include <deque>
#include <condition_variable>
#include <mutex>
#include <atomic>

namespace Voidfall {

class Renderer;
struct UnanchoredIsland;

struct ChunkMeshData {
    ChunkPos pos{0, 0, 0};
    Chunk* chunk{nullptr};
    std::shared_ptr<Chunk> chunk_ref{nullptr};
    std::vector<PackedVoxelVertex> vertices;

    void UploadToGPU() {
        Chunk* c = chunk ? chunk : chunk_ref.get();
        if (c) {
            if (!vertices.empty()) {
                c->stage_mesh(std::move(vertices));
            }
            c->upload_mesh();
        }
    }
};

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
    inline static size_t active_debris_count{0};
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
    void detect_natural_floor_steps();
    void ApplyTopologicalShapes(Chunk& chunk);
    void apply_topological_shapes(Chunk& chunk) { ApplyTopologicalShapes(chunk); }
    void GenerateChunkClutter(Chunk& chunk);
    void generate_chunk_clutter(Chunk& chunk) { GenerateChunkClutter(chunk); }

    const std::vector<FaunaEntity>& GetActiveEntities() const { return m_active_entities; }
    std::vector<FaunaEntity>& GetActiveEntities() { return m_active_entities; }

    const glm::vec3& GetPlayerSpawnPos() const { return m_playerSpawnPos; }
    void SetPlayerSpawnPos(const glm::vec3& p) { m_playerSpawnPos = p; }
    float GetPlayerSpawnYaw() const { return m_playerSpawnYaw; }
    void SetPlayerSpawnYaw(float yaw) { m_playerSpawnYaw = yaw; }
    float calculate_spawn_yaw(const glm::vec3& spawn_pos) const;

    const VaultDoor& vault_door() const { return m_vault_door; }
    VaultDoor& vault_door_mut() { return m_vault_door; }

    Chunk* get_chunk(const ChunkPos& pos);
    const Chunk* get_chunk(const ChunkPos& pos) const;

    Chunk* GetChunkFromBlockPos(const glm::ivec3& blockPos);
    const Chunk* GetChunkFromBlockPos(const glm::ivec3& blockPos) const;

    Chunk* get_or_create_chunk(const ChunkPos& pos);
    void OnChunkGenerated(Chunk* chunk);

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
    bool IsLiquid(uint8_t mat) const { return Voidfall::IsLiquid(mat); }

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
    void push_pending_upload(std::unique_ptr<ChunkMeshData> data);
    size_t upload_queue_size() const;
    size_t pending_upload_count() const { return upload_queue_size(); }
    std::queue<std::unique_ptr<ChunkMeshData>>& pending_uploads() { return m_pendingUploads; }
    const std::queue<std::unique_ptr<ChunkMeshData>>& pending_uploads() const { return m_pendingUploads; }

    // Pending GPU upload staging queue (thread-safe)
    std::queue<std::unique_ptr<ChunkMeshData>> m_pendingUploads;

    // Frustum and Vertical Culling & Rendering
    void set_frustum_planes(const std::array<glm::vec4, 6>& planes);
    void set_camera_frustum(const glm::mat4& view_proj);
    bool is_box_in_frustum(const glm::vec3& min_pt, const glm::vec3& max_pt) const;
    bool is_vertical_occluded(const ChunkPos& chunkPos, int playerChunkY) const;
    bool is_chunk_occluded_vertically(const ChunkPos& chunkPos, int playerChunkY) const {
        return is_vertical_occluded(chunkPos, playerChunkY);
    }

    void render();
    void render(const glm::vec3& player_pos);
    void render(const std::function<void(const Chunk&)>& draw_fn, const glm::vec3& player_pos = glm::vec3(0.0f));

    // PascalCase aliases
    void Render() { render(); }
    void Render(const glm::vec3& player_pos) { render(player_pos); }
    void Render(const std::function<void(const Chunk&)>& draw_fn, const glm::vec3& player_pos = glm::vec3(0.0f)) {
        render(draw_fn, player_pos);
    }

    // Template overloads for external renderer objects (e.g. Renderer)
    template <typename TRenderer>
    requires (!std::is_same_v<std::decay_t<TRenderer>, glm::vec3>)
    void Render(TRenderer& renderer, const glm::vec3& camera_pos, const glm::vec3& player_pos) {
        set_frustum_planes(renderer.frustum_planes());
        render([&renderer, &player_pos](const Chunk& chunk) {
            renderer.render_chunk(chunk);
            renderer.render_chunk_clutter(chunk, player_pos);
        }, player_pos);
    }

    template <typename TRenderer>
    requires (!std::is_same_v<std::decay_t<TRenderer>, glm::vec3>)
    void Render(TRenderer& renderer, const glm::vec3& camera_or_player_pos) {
        Render(renderer, camera_or_player_pos, camera_or_player_pos);
    }

    template <typename TRenderer>
    requires (!std::is_same_v<std::decay_t<TRenderer>, glm::vec3>)
    void Render(TRenderer& renderer) {
        Render(renderer, m_playerSpawnPos, m_playerSpawnPos);
    }

    size_t rendered_chunks_count() const { return m_rendered_chunks_count; }
    size_t culled_chunks_count() const { return m_culled_chunks_count; }

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
    size_t active_debris_count() const { return m_debris.size(); }
    void add_debris(DynamicDebris&& d);
    void spawn_debris(DynamicDebris&& d) { add_debris(std::move(d)); }
    void spawn_tremor_debris(const glm::vec3& pos, uint8_t mat_id = MAT_FRACTURED_GRANITE, const glm::vec3& vel = glm::vec3(0.0f, -2.0f, 0.0f));
    void clear_debris();
    void despawn_or_revoxelize_oldest_debris(bool force_revoxelize = false);
    void remove_destroyed_debris();
    void update_debris(float dt, const glm::vec3& player_pos = glm::vec3(-9999.0f), bool is_player_sheltered = false);

    // Event-Driven Fluid Simulation Engine (Active Queue Architecture)
    std::deque<glm::ivec3> m_activeFluids;
    std::unordered_set<uint64_t> m_activeFluidSet;
    float m_fluidAccumulator{0.0f};
    std::unordered_set<Chunk*> m_fluidDirtyChunks;

    static inline uint64_t PackCoord(const glm::ivec3& p) {
        return (uint64_t(p.x & 0x1FFFFF) << 42) | (uint64_t(p.y & 0x1FFFFF) << 21) | uint64_t(p.z & 0x1FFFFF);
    }
    static inline uint64_t PackPos(const glm::ivec3& pos) { return PackCoord(pos); }

    void WakeFluid(const glm::ivec3& pos);
    void wake_fluid(const glm::ivec3& pos) { WakeFluid(pos); }
    void PushActiveFluid(const glm::ivec3& pos) { WakeFluid(pos); }
    void check_wake_fluid_around(const glm::ivec3& pos);

    uint8_t GetBlockMaterial(const glm::ivec3& pos) const;
    uint8_t get_block_material(const glm::ivec3& pos) const { return GetBlockMaterial(pos); }

    void set_fluid_cell(const glm::ivec3& pos, uint8_t mat, uint8_t level, bool is_waterlogged = false);
    void set_waterlogged_cell(const glm::ivec3& pos, bool state);
    void flush_fluid_dirty_chunks();
    void update_fluids(float dt);
    void UpdateFluids(float dt) { update_fluids(dt); }

    void SimulateFluidCell(const glm::ivec3& pos);
    void SimulateFluidCell(const glm::ivec3& pos, std::unordered_set<Chunk*>& dirtyChunks);
    bool SimulateAirCell(const glm::ivec3& pos);
    bool SimulateAirCell(const glm::ivec3& pos, std::unordered_set<Chunk*>& dirtyChunks);

    size_t active_fluid_count() const { return m_activeFluids.size(); }
    size_t ActiveFluidCount() const { return active_fluid_count(); }

    bool DestroyBlock(const glm::ivec3& pos);

    void Update(float dt) {
        update_debris(dt);
        update_fluids(dt);
        upload_mesh_queue(MAX_CHUNK_UPLOADS_PER_FRAME);
    }

private:
    void generate_chunk_terrain(Chunk& chunk);
    Voxel get_voxel_unlocked(int world_x, int world_y, int world_z) const;
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
    float m_playerSpawnYaw{0.0f};
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
    std::unordered_set<ChunkPos, ChunkPosHash> m_upload_queued_set;
    size_t m_uploads_this_frame{0};

    // Camera view frustum culling
    std::array<glm::vec4, 6> m_frustum_planes{};
    bool m_has_frustum{false};
    size_t m_rendered_chunks_count{0};
    size_t m_culled_chunks_count{0};
};

} // namespace Voidfall
