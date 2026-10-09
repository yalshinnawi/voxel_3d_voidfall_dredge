#pragma once
#include "packed_vertex.hpp"
#include "voxel_types.hpp"
#include "clutter_types.hpp"
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

namespace Voidfall {

constexpr int CHUNK_SIZE = 32;
constexpr int CHUNK_SIZE_SQ = CHUNK_SIZE * CHUNK_SIZE;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

struct ChunkPos {
    int x{0};
    int y{0};
    int z{0};

    bool operator==(const ChunkPos& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const ChunkPos& o) const { return !(*this == o); }
    bool operator<(const ChunkPos& o) const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
};

struct ChunkPosHash {
    std::size_t operator()(const ChunkPos& p) const noexcept {
        // High quality spatial 3D hash
        std::size_t h = 2166136261u;
        h = (h ^ static_cast<std::size_t>(p.x)) * 16777619u;
        h = (h ^ static_cast<std::size_t>(p.y)) * 16777619u;
        h = (h ^ static_cast<std::size_t>(p.z)) * 16777619u;
        return h;
    }
};

class Chunk {
public:
    explicit Chunk(const ChunkPos& pos);
    ~Chunk();

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;

    const ChunkPos& get_pos() const { return m_pos; }
    glm::vec3 get_world_pos() const {
        return glm::vec3(m_pos.x * CHUNK_SIZE, m_pos.y * CHUNK_SIZE, m_pos.z * CHUNK_SIZE);
    }

    static inline size_t to_index(int x, int y, int z) {
        return (static_cast<size_t>(x) & 31u) |
               ((static_cast<size_t>(y) & 31u) << 5) |
               ((static_cast<size_t>(z) & 31u) << 10);
    }

    static inline void from_index(size_t idx, int& x, int& y, int& z) {
        x = static_cast<int>(idx & 31u);
        y = static_cast<int>((idx >> 5) & 31u);
        z = static_cast<int>((idx >> 10) & 31u);
    }

    static inline bool in_bounds(int x, int y, int z) {
        return ((x | y | z) & ~(CHUNK_SIZE - 1)) == 0;
    }

    inline Voxel get_voxel(int x, int y, int z) const {
        if (!in_bounds(x, y, z)) {
            return Voxel{MAT_AIR, 0};
        }
        return m_voxels[to_index(x, y, z)];
    }

    void set_voxel(int x, int y, int z, Voxel v);

    inline uint8_t GetFlags(int x, int y, int z) const {
        if (!in_bounds(x, y, z)) return 0;
        return m_voxels[to_index(x, y, z)].flags_and_damage;
    }

    inline void SetFlags(int x, int y, int z, uint8_t flags) {
        if (!in_bounds(x, y, z)) return;
        m_voxels[to_index(x, y, z)].flags_and_damage = flags;
        mark_mesh_dirty();
        mark_structural_dirty();
    }

    inline uint8_t get_flags(int x, int y, int z) const { return GetFlags(x, y, z); }
    inline void set_flags(int x, int y, int z, uint8_t flags) { SetFlags(x, y, z, flags); }

    inline VoxelShape GetShape(int x, int y, int z) const {
        return static_cast<VoxelShape>(GetFlags(x, y, z) & VOXEL_SHAPE_MASK);
    }

    inline void SetShape(int x, int y, int z, VoxelShape shape) {
        uint8_t current = GetFlags(x, y, z);
        SetFlags(x, y, z, (current & ~VOXEL_SHAPE_MASK) | static_cast<uint8_t>(shape));
    }

    inline VoxelShape get_shape(int x, int y, int z) const { return GetShape(x, y, z); }
    inline void set_shape(int x, int y, int z, VoxelShape shape) { SetShape(x, y, z, shape); }

    inline bool IsLiquid(uint8_t mat) const {
        return mat == MAT_WATER || 
               mat == MAT_ACID || 
               mat == MAT_COOLANT || 
               mat == MAT_THERMITE_SLAG || 
               mat == MAT_LAVA || 
               mat == MAT_RADIOACTIVE_SLUDGE;
    }
    inline bool IsWaterlogged(int x, int y, int z) const {
        return (GetFlags(x, y, z) & VOXEL_FLAG_WATERLOGGED) != 0;
    }
    inline void SetWaterlogged(int x, int y, int z, bool state) {
        uint8_t flags = GetFlags(x, y, z);
        SetFlags(x, y, z, state ? (flags | VOXEL_FLAG_WATERLOGGED) : (flags & ~VOXEL_FLAG_WATERLOGGED));
    }
    inline bool is_waterlogged(int x, int y, int z) const { return IsWaterlogged(x, y, z); }
    inline void set_waterlogged(int x, int y, int z, bool state) { SetWaterlogged(x, y, z, state); }

    inline Voxel get_voxel_idx(size_t idx) const {
        if (idx < m_voxels.size()) {
            return m_voxels[idx];
        }
        return Voxel{MAT_AIR, 0};
    }

    void set_voxel_idx(size_t idx, Voxel v);

    const Voxel* raw_voxels() const { return m_voxels.data(); }
    Voxel* raw_voxels_mut() { return m_voxels.data(); }

    bool is_empty() const { return m_solid_count == 0; }
    bool is_fully_solid() const { return m_solid_count == CHUNK_VOLUME; }
    size_t solid_count() const { return m_solid_count; }

    bool is_mesh_dirty() const { return m_dirty_mesh.load(); }
    void mark_mesh_dirty() { m_dirty_mesh.store(true); }
    void MarkDirty() { mark_mesh_dirty(); }
    void clear_mesh_dirty() { m_dirty_mesh.store(false); }

    bool is_structural_dirty() const { return m_dirty_structural.load(); }
    void mark_structural_dirty() { m_dirty_structural.store(true); }
    void clear_structural_dirty() { m_dirty_structural.store(false); }

    // Upload pending vertex mesh to GPU buffers (must be called on main render thread)
    void upload_mesh();
    void render() const;

    // Direct buffer staging (produced by GreedyMesher worker threads)
    void stage_mesh(std::vector<PackedVoxelVertex>&& vertices);

    bool has_staged_mesh() const { return m_has_staged_mesh.load(std::memory_order_acquire); }
    size_t vertex_count() const { return m_uploaded_vertex_count; }

    const std::vector<ClutterInstance>& clutter_instances() const { return m_clutter_instances; }
    std::vector<ClutterInstance>& clutter_instances_mut() { return m_clutter_instances; }
    void set_clutter_instances(std::vector<ClutterInstance> instances) { m_clutter_instances = std::move(instances); }

private:
    ChunkPos m_pos;
    std::vector<Voxel> m_voxels;
    size_t m_solid_count{0};
    std::vector<ClutterInstance> m_clutter_instances;

    std::atomic<bool> m_dirty_mesh{true};
    std::atomic<bool> m_dirty_structural{false};

    // Staging mesh (populated by mesher worker threads)
    std::mutex m_stage_mutex;
    std::vector<PackedVoxelVertex> m_staged_vertices;
    std::atomic<bool> m_has_staged_mesh{false};

    // GPU resources
    unsigned int m_vao{0};
    unsigned int m_vbo{0};
    size_t m_uploaded_vertex_count{0};
    bool m_gpu_initialized{false};
};

} // namespace Voidfall
