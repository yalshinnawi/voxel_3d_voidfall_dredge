#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Voidfall {

enum class ClutterType : uint8_t {
    CeilingStalactite = 0,
    FloorStalagmiteRubble = 1,
    VoiditeCrystal = 2,    // Objective mineral cluster (vibrant violet amethyst spires)
    PrismaticCrystal = 3,  // Decorative diamond/calcite cluster (radiant cyan ice shards)
    Count = 4
};

struct ClutterInstance {
    glm::vec3 position{0.0f};     // World-space anchor for distance culling
    glm::mat4 model_matrix{1.0f}; // Transformed 3D orientation
    ClutterType type{ClutterType::CeilingStalactite};
    glm::vec4 color_tint{1.0f, 1.0f, 1.0f, 1.0f};
    float emissive_boost{1.0f};
    glm::ivec3 host_voxel{0};     // World-space coordinates of host block
    glm::ivec3 attach_normal{0, 1, 0}; // Direction towards attached surface face
};

} // namespace Voidfall
