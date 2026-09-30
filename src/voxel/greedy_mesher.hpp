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

private:
    static Voxel sample_voxel(
        const Chunk& chunk,
        const NeighborChunkGetter& get_neighbor,
        int x, int y, int z
    );

    // Vertex Ambient Occlusion (returns 0..3)
    // 0 = fully occluded corner (darkest)
    // 3 = unoccluded corner (brightest)
    static uint8_t compute_vertex_ao(
        bool side1_solid,
        bool side2_solid,
        bool corner_solid
    );
};

} // namespace Voidfall
