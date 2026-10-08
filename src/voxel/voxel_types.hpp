#pragma once
#include <cstdint>

namespace Voidfall {

constexpr uint8_t VOXEL_FLAG_PLAYER_PLACED = 0x80;
constexpr uint8_t VOXEL_SHAPE_MASK         = 0x78; // Bits 3, 4, 5, 6
constexpr uint8_t VOXEL_DAMAGE_MASK        = 0x07; // Bits 0, 1, 2

enum VoxelShape : uint8_t {
    SHAPE_CUBE            = 0x00, // Standard 1x1x1 cube
    SHAPE_SLAB_BOTTOM     = 0x08, // 0.5m bottom slab (Y: [0.0, 0.5])
    SHAPE_SLAB_TOP        = 0x10, // 0.5m top slab (Y: [0.5, 1.0])
    SHAPE_RAMP_NORTH      = 0x18, // 45° slope rising toward -Z
    SHAPE_RAMP_SOUTH      = 0x20, // 45° slope rising toward +Z
    SHAPE_RAMP_EAST       = 0x28, // 45° slope rising toward +X
    SHAPE_RAMP_WEST       = 0x30, // 45° slope rising toward -X
    SHAPE_CORNER_OUTER_NE = 0x38, // Convex pyramid wedge (+X and -Z)
    SHAPE_CORNER_OUTER_NW = 0x40, // Convex pyramid wedge (-X and -Z)
    SHAPE_CORNER_OUTER_SE = 0x48, // Convex pyramid wedge (+X and +Z)
    SHAPE_CORNER_OUTER_SW = 0x50, // Convex pyramid wedge (-X and +Z)
    SHAPE_CORNER_INNER_NE = 0x58, // Concave valley wedge (+X and -Z valley)
    SHAPE_CORNER_INNER_NW = 0x60, // Concave valley wedge (-X and -Z valley)
    SHAPE_CORNER_INNER_SE = 0x68, // Concave valley wedge (+X and +Z valley)
    SHAPE_CORNER_INNER_SW = 0x70  // Concave valley wedge (-X and +Z valley)
};

// Directional aliases
constexpr VoxelShape SHAPE_RAMP_POS_X = SHAPE_RAMP_EAST;  // Rising toward +X
constexpr VoxelShape SHAPE_RAMP_NEG_X = SHAPE_RAMP_WEST;  // Rising toward -X
constexpr VoxelShape SHAPE_RAMP_POS_Z = SHAPE_RAMP_SOUTH; // Rising toward +Z
constexpr VoxelShape SHAPE_RAMP_NEG_Z = SHAPE_RAMP_NORTH; // Rising toward -Z

inline constexpr bool is_ramp_shape(VoxelShape shape) {
    return shape == SHAPE_RAMP_NORTH || shape == SHAPE_RAMP_SOUTH ||
           shape == SHAPE_RAMP_EAST  || shape == SHAPE_RAMP_WEST;
}

inline constexpr bool is_slab_shape(VoxelShape shape) {
    return shape == SHAPE_SLAB_BOTTOM || shape == SHAPE_SLAB_TOP;
}

inline constexpr bool is_corner_outer_shape(VoxelShape shape) {
    return shape == SHAPE_CORNER_OUTER_NE || shape == SHAPE_CORNER_OUTER_NW ||
           shape == SHAPE_CORNER_OUTER_SE || shape == SHAPE_CORNER_OUTER_SW;
}

inline constexpr bool is_corner_inner_shape(VoxelShape shape) {
    return shape == SHAPE_CORNER_INNER_NE || shape == SHAPE_CORNER_INNER_NW ||
           shape == SHAPE_CORNER_INNER_SE || shape == SHAPE_CORNER_INNER_SW;
}

inline constexpr bool is_corner_shape(VoxelShape shape) {
    return is_corner_outer_shape(shape) || is_corner_inner_shape(shape);
}

inline constexpr bool is_sub_block(VoxelShape shape) {
    return shape != SHAPE_CUBE;
}

} // namespace Voidfall
