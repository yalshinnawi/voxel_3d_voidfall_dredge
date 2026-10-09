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
    static std::vector<PackedVoxelVertex> MeshChunk(
        const Chunk& chunk,
        const NeighborChunkGetter& get_neighbor = nullptr
    ) {
        return generate_mesh(chunk, get_neighbor);
    }

    // Dedicated liquid and waterlogged sub-block meshing pass
    static void mesh_liquid_pass(
        const Chunk& chunk,
        const NeighborChunkGetter& get_neighbor,
        std::vector<PackedVoxelVertex>& vertices
    );
    static inline void MeshLiquidPass(
        const Chunk& chunk,
        const NeighborChunkGetter& get_neighbor,
        std::vector<PackedVoxelVertex>& vertices
    ) {
        mesh_liquid_pass(chunk, get_neighbor, vertices);
    }

    // Evaluates Minecraft-style 4-corner averaged fluid height for a vertex column
    template<typename WorldLike>
    static float CalculateCornerHeight(const WorldLike& world, int cornerX, int y, int cornerZ) {
        float sumHeight = 0.0f;
        int count = 0;

        for (int dx = -1; dx <= 0; ++dx) {
            for (int dz = -1; dz <= 0; ++dz) {
                glm::ivec3 colPos(cornerX + dx, y, cornerZ + dz);
                float h = GetBlockFluidHeight(world, colPos);
                if (h >= 0.0f) {
                    if (h >= 1.0f) return 1.0f;
                    sumHeight += h;
                    count++;
                }
            }
        }
        return (count > 0) ? (sumHeight / static_cast<float>(count)) : 0.88f;
    }

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

    // Calculates the normalized diagonal normal for a ramp shape:
    // N = normalize(N_base + N_side)
    static glm::vec3 calculate_diagonal_normal(VoxelShape shape);
    static glm::vec3 get_diagonal_normal(VoxelShape shape) { return calculate_diagonal_normal(shape); }
    static glm::vec3 calculate_ramp_normal(VoxelShape shape) { return calculate_diagonal_normal(shape); }

private:
    static Voxel sample_voxel(
        const Chunk& chunk,
        const NeighborChunkGetter& get_neighbor,
        int x, int y, int z
    );
};

template<typename WorldLike>
inline float CalculateCornerHeight(const WorldLike& world, int cornerX, int y, int cornerZ) {
    return GreedyMesher::CalculateCornerHeight(world, cornerX, y, cornerZ);
}

} // namespace Voidfall
