#pragma once
#include "chunk.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Voidfall {

struct BlockBreakFX {
    /// Return the authentic material and stage-dependent crack luminescence / color
    static glm::vec4 get_material_crack_color(uint8_t mat_id, int stage);

    /// Generate 3D crack line geometry precisely conforming to voxel shape (ramps, slabs, cubes)
    /// and material texture fracture patterns
    static void generate_block_crack_geometry(
        const glm::ivec3& voxel_pos,
        float progress,
        const glm::ivec3& face_norm,
        uint8_t mat_id,
        VoxelShape shape,
        std::vector<float>& out_lines,
        glm::vec4& out_color
    );
};

} // namespace Voidfall
