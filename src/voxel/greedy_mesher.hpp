#pragma once
#include "chunk.hpp"
#include <functional>
#include <vector>

namespace Voidfall {

class GreedyMesher {
public:
    using NeighborChunkGetter = std::function<const Chunk*(const ChunkPos&)>;

    // Generates a greedy mesh with baked vertex ambient occlusion
    static std::vector<PackedVoxelVertex> generate_mesh(
        const Chunk& chunk,
        const NeighborChunkGetter& get_neighbor = nullptr
    );

    // Queries face visibility between adjacent voxels.
    // If neighbor chunk is unloaded, treats boundary block as SOLID (MAT_GRANITE) to prevent void leaks.
    static bool is_face_visible(
        const Chunk& chunk,
        const NeighborChunkGetter& get_neighbor,
        int x, int y, int z,
        int nx, int ny, int nz
    );
    static bool IsFaceVisible(
        const Chunk& chunk,
        const NeighborChunkGetter& get_neighbor,
        int x, int y, int z,
        int nx, int ny, int nz
    ) {
        return is_face_visible(chunk, get_neighbor, x, y, z, nx, ny, nz);
    }

    // Vertex Ambient Occlusion (returns 0..3)
    // 0 = open (unoccluded corner)
    // 3 = fully occluded corner
    static uint8_t compute_vertex_ao(
        bool side1_solid,
        bool side2_solid,
        bool corner_solid
    );

private:
    static Voxel sample_voxel(
        const Chunk& chunk,
        const NeighborChunkGetter& get_neighbor,
        int x, int y, int z
    );
};

} // namespace Voidfall
