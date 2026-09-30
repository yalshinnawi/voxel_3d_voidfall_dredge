#pragma once
#include "packed_vertex.hpp"
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
        return static_cast<size_t>(x) +
               static_cast<size_t>(y) * CHUNK_SIZE +
               static_cast<size_t>(z) * CHUNK_SIZE_SQ;
    }

    static inline void from_index(size_t idx, int& x, int& y, int& z) {
        x = static_cast<int>(idx % CHUNK_SIZE);
        y = static_cast<int>((idx / CHUNK_SIZE) % CHUNK_SIZE);
        z = static_cast<int>(idx / CHUNK_SIZE_SQ);
    }

    static inline bool in_bounds(int x, int y, int z) {
        return x >= 0 && x < CHUNK_SIZE &&
               y >= 0 && y < CHUNK_SIZE &&
               z >= 0 && z < CHUNK_SIZE;
    }

    Voxel get_voxel(int x, int y, int z) const;
    void set_voxel(int x, int y, int z, Voxel v);
    Voxel get_voxel_idx(size_t idx) const;
    void set_voxel_idx(size_t idx, Voxel v);

    const Voxel* raw_voxels() const { return m_voxels.data(); }
    Voxel* raw_voxels_mut() { return m_voxels.data(); }

    bool is_empty() const { return m_solid_count == 0; }
    bool is_fully_solid() const { return m_solid_count == CHUNK_VOLUME; }
    size_t solid_count() const { return m_solid_count; }

    bool is_mesh_dirty() const { return m_dirty_mesh.load(); }
    void mark_mesh_dirty() { m_dirty_mesh.store(true); }
    void clear_mesh_dirty() { m_dirty_mesh.store(false); }

    bool is_structural_dirty() const { return m_dirty_structural.load(); }
    void mark_structural_dirty() { m_dirty_structural.store(true); }
    void clear_structural_dirty() { m_dirty_structural.store(false); }

    // Upload pending vertex mesh to GPU buffers (must be called on main render thread)
    void upload_mesh();
    void render() const;

    // Direct buffer staging (produced by GreedyMesher worker threads)
    void stage_mesh(std::vector<PackedVoxelVertex>&& vertices);

    size_t vertex_count() const { return m_uploaded_vertex_count; }

private:
    ChunkPos m_pos;
    std::vector<Voxel> m_voxels;
    size_t m_solid_count{0};

    std::atomic<bool> m_dirty_mesh{true};
    std::atomic<bool> m_dirty_structural{false};

    // Staging mesh (populated by mesher worker threads)
    std::mutex m_stage_mutex;
    std::vector<PackedVoxelVertex> m_staged_vertices;
    bool m_has_staged_mesh{false};

    // GPU resources
    unsigned int m_vao{0};
    unsigned int m_vbo{0};
    size_t m_uploaded_vertex_count{0};
    bool m_gpu_initialized{false};
};

} // namespace Voidfall
