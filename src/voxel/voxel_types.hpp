#pragma once
#include <cstdint>

namespace Voidfall {

// Standard Material Tiers for Voidfall: Dredge
enum MaterialID : uint8_t {
    MAT_AIR = 0,
    MAT_FRACTURED_GRANITE = 1,
    MAT_VOLCANIC_BASALT = 2,
    MAT_VOIDITE_CRYSTAL = 3,       // Emissive purple/void crystal
    MAT_INDUSTRIAL_BULKHEAD = 4,   // Metallic subterranean vault alloy
    MAT_REINFORCED_VAULT_DOOR = 5, // Structural anchor (indestructible / anchor point)
    MAT_THERMITE_SLAG = 6,         // Heated molten slag from demolition charges
    MAT_RADIOACTIVE_ORE = 7,       // Glowing green toxic mineral
    MAT_DREDGE_BEDROCK = 8,        // Deep planetary crust
    MAT_GAS = 9,
    MAT_VOLATILE_SMOKE = 10,
    MAT_CRYSTAL_AQUIFER = 11,      // Clear subterranean aquifer water / cascade pool
    MAT_BIOLUMINESCENT_FLORA = 12, // Subterranean glowing moss, fungi, and oasis flora
    MAT_PRISMATIC_CRYSTAL = 13,    // Translucent radiant prismatic crystal spires
    MAT_OBSIDIAN_SPIKES = 14,      // Lethal needle-sharp obsidian punji spikes
    MAT_COUNT = 15
};

// Aliases for gameplay and surveying systems
constexpr uint8_t MAT_VOIDITE = MAT_VOIDITE_CRYSTAL;
constexpr uint8_t MAT_TITANIUM = MAT_INDUSTRIAL_BULKHEAD;
constexpr uint8_t MAT_BULKHEAD = MAT_INDUSTRIAL_BULKHEAD;
constexpr uint8_t MAT_VAULT_DOOR = MAT_REINFORCED_VAULT_DOOR;
constexpr uint8_t MAT_RADIOACTIVE = MAT_RADIOACTIVE_ORE;
constexpr uint8_t MAT_RADIOACTIVE_SLUDGE = MAT_RADIOACTIVE_ORE;
constexpr uint8_t MAT_GRANITE = MAT_FRACTURED_GRANITE;
constexpr uint8_t MAT_BASALT = MAT_VOLCANIC_BASALT;
constexpr uint8_t MAT_WATER = MAT_CRYSTAL_AQUIFER;
constexpr uint8_t MAT_AQUIFER = MAT_CRYSTAL_AQUIFER;
constexpr uint8_t MAT_COOLANT = MAT_CRYSTAL_AQUIFER;
constexpr uint8_t MAT_ACID = MAT_THERMITE_SLAG;
constexpr uint8_t MAT_FLORA = MAT_BIOLUMINESCENT_FLORA;
constexpr uint8_t MAT_MOSS = MAT_BIOLUMINESCENT_FLORA;
constexpr uint8_t MAT_CRYSTAL = MAT_PRISMATIC_CRYSTAL;
constexpr uint8_t MAT_TOXIC_GAS = MAT_GAS;
constexpr uint8_t MAT_MOLTEN_MAGMA = MAT_THERMITE_SLAG;
constexpr uint8_t MAT_LAVA = MAT_THERMITE_SLAG;
constexpr uint8_t MAT_SPIKES = MAT_OBSIDIAN_SPIKES;
constexpr uint8_t MAT_PRECURSOR_STONE = MAT_DREDGE_BEDROCK;

inline bool IsLiquid(uint8_t mat) {
    return mat == MAT_WATER || 
           mat == MAT_ACID || 
           mat == MAT_COOLANT || 
           mat == MAT_THERMITE_SLAG || 
           mat == MAT_LAVA || 
           mat == MAT_RADIOACTIVE_SLUDGE;
}

constexpr uint8_t VOXEL_FLAG_PLAYER_PLACED = 0x80;
constexpr uint8_t VOXEL_SHAPE_MASK         = 0x78; // Bits 3, 4, 5, 6
constexpr uint8_t VOXEL_FLAG_WATERLOGGED   = 0x04; // Set when liquid occupies a slab or ramp
constexpr uint8_t VOXEL_DAMAGE_MASK        = 0x07; // Bits 0, 1, 2
constexpr uint8_t VOXEL_FLUID_LEVEL_MASK   = 0x07; // Bits 0, 1, 2: fluid level [1..5]
constexpr uint8_t VOXEL_FLUID_LEVEL_SOURCE = 5;
constexpr uint8_t VOXEL_FLUID_LEVEL_MIN    = 1;

inline int GetFluidLevel(uint8_t flags) {
    int lvl = flags & 0x07;
    return (lvl == 0) ? 5 : lvl; // 0 or 5 is a permanent source block
}

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
