#pragma once
#include <cstdint>

namespace Voidfall {

enum VoxelShape : uint8_t {
    SHAPE_CUBE          = 0x00,
    SHAPE_RAMP_POS_X    = 0x10, // Slope rising toward +X
    SHAPE_RAMP_NEG_X    = 0x20, // Slope rising toward -X
    SHAPE_RAMP_POS_Z    = 0x30, // Slope rising toward +Z
    SHAPE_RAMP_NEG_Z    = 0x40, // Slope rising toward -Z
};

constexpr uint8_t VOXEL_SHAPE_MASK = 0xF0;

inline constexpr bool is_ramp_shape(VoxelShape shape) {
    return shape == SHAPE_RAMP_POS_X || shape == SHAPE_RAMP_NEG_X ||
           shape == SHAPE_RAMP_POS_Z || shape == SHAPE_RAMP_NEG_Z;
}

} // namespace Voidfall
