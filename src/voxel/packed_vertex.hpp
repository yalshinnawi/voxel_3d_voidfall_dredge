#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include "voxel_types.hpp"

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
constexpr uint8_t MAT_GRANITE = MAT_FRACTURED_GRANITE;
constexpr uint8_t MAT_BASALT = MAT_VOLCANIC_BASALT;
constexpr uint8_t MAT_WATER = MAT_CRYSTAL_AQUIFER;
constexpr uint8_t MAT_AQUIFER = MAT_CRYSTAL_AQUIFER;
constexpr uint8_t MAT_FLORA = MAT_BIOLUMINESCENT_FLORA;
constexpr uint8_t MAT_MOSS = MAT_BIOLUMINESCENT_FLORA;
constexpr uint8_t MAT_CRYSTAL = MAT_PRISMATIC_CRYSTAL;
constexpr uint8_t MAT_TOXIC_GAS = MAT_GAS;
constexpr uint8_t MAT_MOLTEN_MAGMA = MAT_THERMITE_SLAG;
constexpr uint8_t MAT_LAVA = MAT_THERMITE_SLAG;
constexpr uint8_t MAT_SPIKES = MAT_OBSIDIAN_SPIKES;
constexpr uint8_t MAT_PRECURSOR_STONE = MAT_DREDGE_BEDROCK;

// Voxel bit flags
constexpr uint8_t VOXEL_FLAG_ANCHORED      = 0x10;
constexpr uint8_t VOXEL_FLAG_EMISSIVE      = 0x20;
constexpr uint8_t VOXEL_FLAG_SURVEYED      = 0x40;
constexpr uint8_t VOXEL_FLAG_PLAYER_PLACED = 0x80;

// 16-bit packed voxel state
#pragma pack(push, 1)
struct Voxel {
    uint8_t material_id{MAT_AIR};
    // Flags:
    // bit 0-3: Damage tier (0..15)
    // bit 4:   Anchored / structural node (0x10)
    // bit 5:   Active emissive (0x20)
    // bit 6:   Surveyed / pinged highlight (0x40)
    // bit 7:   Player-placed block metadata (0x80)
    uint8_t flags_and_damage{0};

    inline bool is_liquid() const {
        return material_id == MAT_THERMITE_SLAG || material_id == MAT_CRYSTAL_AQUIFER;
    }
    inline bool is_solid() const {
        return material_id != MAT_AIR && material_id != MAT_GAS && material_id != MAT_VOLATILE_SMOKE && !is_liquid();
    }
    inline bool is_renderable() const {
        return material_id != MAT_AIR && material_id != MAT_GAS && material_id != MAT_VOLATILE_SMOKE;
    }
    inline bool is_anchored() const { return (!is_ramp() && (flags_and_damage & VOXEL_FLAG_ANCHORED) != 0) || material_id == MAT_DREDGE_BEDROCK || material_id == MAT_REINFORCED_VAULT_DOOR; }
    inline uint8_t damage() const { return flags_and_damage & 0x0F; }
    inline void set_damage(uint8_t d) { flags_and_damage = (flags_and_damage & 0xF0) | (d & 0x0F); }
    inline bool is_highlighted() const { return !is_ramp() && (flags_and_damage & VOXEL_FLAG_SURVEYED) != 0; }
    inline void set_highlighted(bool h) {
        if (h) flags_and_damage |= VOXEL_FLAG_SURVEYED;
        else   flags_and_damage &= ~VOXEL_FLAG_SURVEYED;
    }
    inline bool is_player_placed() const { return (flags_and_damage & VOXEL_FLAG_PLAYER_PLACED) != 0; }
    inline void set_player_placed(bool p) {
        if (p) flags_and_damage |= VOXEL_FLAG_PLAYER_PLACED;
        else   flags_and_damage &= ~VOXEL_FLAG_PLAYER_PLACED;
    }

    inline VoxelShape shape() const {
        if (material_id == MAT_DREDGE_BEDROCK ||
            material_id == MAT_REINFORCED_VAULT_DOOR ||
            material_id == MAT_AIR ||
            material_id == MAT_GAS ||
            material_id == MAT_VOLATILE_SMOKE ||
            material_id == MAT_THERMITE_SLAG ||
            material_id == MAT_CRYSTAL_AQUIFER ||
            material_id == MAT_VOIDITE_CRYSTAL ||
            material_id == MAT_RADIOACTIVE_ORE ||
            material_id == MAT_PRISMATIC_CRYSTAL ||
            material_id == MAT_BIOLUMINESCENT_FLORA ||
            material_id == MAT_OBSIDIAN_SPIKES) {
            return SHAPE_CUBE;
        }
        uint8_t s = flags_and_damage & VOXEL_SHAPE_MASK;
        if (s == SHAPE_RAMP_POS_X || s == SHAPE_RAMP_NEG_X ||
            s == SHAPE_RAMP_POS_Z || s == SHAPE_RAMP_NEG_Z) {
            return static_cast<VoxelShape>(s);
        }
        return SHAPE_CUBE;
    }
    inline void set_shape(VoxelShape s) {
        flags_and_damage = (flags_and_damage & 0x0F) | (static_cast<uint8_t>(s) & VOXEL_SHAPE_MASK);
    }
    inline bool is_ramp() const {
        return is_ramp_shape(shape());
    }
};
#pragma pack(pop)
static_assert(sizeof(Voxel) == 2, "Voxel struct must be exactly 2 bytes (16-bit packed)");

// 8-byte bit-packed vertex format for GPU greedy meshing
#pragma pack(push, 1)
struct PackedVoxelVertex {
    // uint32_t data0:
    // [0..5]   x (6 bits: 0..63, holds 0..32)
    // [6..11]  y (6 bits: 0..63, holds 0..32)
    // [12..17] z (6 bits: 0..63, holds 0..32)
    // [18..20] normal_idx (3 bits: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z)
    // [21..22] ao (2 bits: 0..3 baked ambient occlusion)
    // [23..30] tex_layer (8 bits: 0..255 texture array layer ID)
    // [31]     aux / surveyed_flag (1 bit: 0..1)
    uint32_t data0;

    // uint32_t data1:
    // [0..5]   u_dim (6 bits: greedy quad width 1..32)
    // [6..11]  v_dim (6 bits: greedy quad height 1..32)
    // [12..13] corner_idx (2 bits: 0..3 quad corner)
    // [14..17] damage_tier (4 bits: 0..15 crack overlay)
    // [18..25] emission_intensity (8 bits: 0..255)
    // [26..31] reserved / extra
    uint32_t data1;

    static inline PackedVoxelVertex encode(
        uint32_t x, uint32_t y, uint32_t z,
        uint32_t normal_idx, uint32_t ao, uint32_t tex_layer,
        uint32_t u_dim, uint32_t v_dim, uint32_t corner_idx,
        uint32_t damage = 0, uint32_t emission = 0, uint32_t aux = 0
    ) {
        PackedVoxelVertex v;
        v.data0 = (x & 0x3Fu) |
                  ((y & 0x3Fu) << 6) |
                  ((z & 0x3Fu) << 12) |
                  ((normal_idx & 0x7u) << 18) |
                  ((ao & 0x3u) << 21) |
                  ((tex_layer & 0xFFu) << 23) |
                  ((aux & 0x1u) << 31);

        v.data1 = (u_dim & 0x3Fu) |
                  ((v_dim & 0x3Fu) << 6) |
                  ((corner_idx & 0x3u) << 12) |
                  ((damage & 0xFu) << 14) |
                  ((emission & 0xFFu) << 18);
        return v;
    }
};
#pragma pack(pop)
static_assert(sizeof(PackedVoxelVertex) == 8, "PackedVoxelVertex must be exactly 8 bytes");

} // namespace Voidfall
