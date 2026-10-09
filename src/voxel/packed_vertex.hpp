#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include "voxel_types.hpp"

namespace Voidfall {



// Voxel bit flags
constexpr uint8_t VOXEL_FLAG_ANCHORED      = 0x10;
constexpr uint8_t VOXEL_FLAG_EMISSIVE      = 0x20;
constexpr uint8_t VOXEL_FLAG_SURVEYED      = 0x40;

// 16-bit packed voxel state
#pragma pack(push, 1)
struct Voxel {
    uint8_t material_id{MAT_AIR};
    // Flags:
    // bit 0-2: Damage tier (0..7) or fluid level (1..5)
    // bit 2:   Waterlogged flag (0x04) when occupied by liquid on slab/ramp
    // bit 4:   Anchored / structural node (0x10)
    // bit 5:   Active emissive (0x20)
    // bit 6:   Surveyed / pinged highlight (0x40)
    // bit 7:   Player-placed block metadata (0x80)
    uint8_t flags_and_damage{0};

    inline bool is_liquid() const {
        return IsLiquid(material_id);
    }
    inline void set_waterlogged(bool w) {
        if (w) flags_and_damage |= VOXEL_FLAG_WATERLOGGED;
        else   flags_and_damage &= ~VOXEL_FLAG_WATERLOGGED;
    }
    inline uint8_t fluid_level() const {
        return static_cast<uint8_t>(GetFluidLevel(flags_and_damage));
    }
    inline void set_fluid_level(uint8_t lvl) {
        flags_and_damage = (flags_and_damage & ~VOXEL_FLUID_LEVEL_MASK) | (lvl & VOXEL_FLUID_LEVEL_MASK);
    }
    inline bool is_solid() const {
        return material_id != MAT_AIR && material_id != MAT_GAS && material_id != MAT_VOLATILE_SMOKE && !is_liquid();
    }
    inline bool is_renderable() const {
        return material_id != MAT_AIR && material_id != MAT_GAS && material_id != MAT_VOLATILE_SMOKE;
    }
    inline bool is_anchored() const { return (!is_ramp() && (flags_and_damage & VOXEL_FLAG_ANCHORED) != 0) || material_id == MAT_DREDGE_BEDROCK || material_id == MAT_REINFORCED_VAULT_DOOR; }
    inline uint8_t damage() const { return flags_and_damage & VOXEL_DAMAGE_MASK; }
    inline void set_damage(uint8_t d) { flags_and_damage = (flags_and_damage & ~VOXEL_DAMAGE_MASK) | (d & VOXEL_DAMAGE_MASK); }
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
        return static_cast<VoxelShape>(flags_and_damage & VOXEL_SHAPE_MASK);
    }
    inline void set_shape(VoxelShape s) {
        flags_and_damage = (flags_and_damage & ~VOXEL_SHAPE_MASK) | (static_cast<uint8_t>(s) & VOXEL_SHAPE_MASK);
    }
    inline bool is_ramp() const {
        return is_ramp_shape(shape());
    }
    inline bool is_slab() const {
        return is_slab_shape(shape());
    }
    inline bool is_corner() const {
        return is_corner_shape(shape());
    }
    inline bool is_waterlogged() const {
        return (shape() != SHAPE_CUBE) && ((flags_and_damage & VOXEL_FLAG_WATERLOGGED) != 0);
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
    // [26]     sub_y_half (1 bit: 0 or 1, offsets Y by -0.5 for half-slabs)
    // [27]     water_slope_offset (1 bit: offsets Y by +0.04 for sloped water surface)
    // [28..30] water_fluid_level (3 bits: fluid level [1..5], offsets Y by 1.0 - liquidHeight)
    // [31]     water_vertical_flow (1 bit: vertical step skirt / waterfall curtain)
    uint32_t data1;

    static inline PackedVoxelVertex encode(
        uint32_t x, uint32_t y, uint32_t z,
        uint32_t normal_idx, uint32_t ao, uint32_t tex_layer,
        uint32_t u_dim, uint32_t v_dim, uint32_t corner_idx,
        uint32_t damage = 0, uint32_t emission = 0, uint32_t aux = 0,
        uint32_t sub_y_half = 0, uint32_t water_offset = 0,
        uint32_t water_recess = 0, uint32_t water_fluid_level = 0,
        uint32_t water_vertical_flow = 0
    ) {
        PackedVoxelVertex v;
        v.data0 = (x & 0x3Fu) |
                  ((y & 0x3Fu) << 6) |
                  ((z & 0x3Fu) << 12) |
                  ((normal_idx & 0x7u) << 18) |
                  ((ao & 0x3u) << 21) |
                  ((tex_layer & 0xFFu) << 23) |
                  ((aux & 0x1u) << 31);

        uint32_t fluid_lvl = water_fluid_level;
        if (fluid_lvl == 0 && water_recess != 0) {
            fluid_lvl = 5;
        }

        v.data1 = (u_dim & 0x3Fu) |
                  ((v_dim & 0x3Fu) << 6) |
                  ((corner_idx & 0x3u) << 12) |
                  ((damage & 0xFu) << 14) |
                  ((emission & 0xFFu) << 18) |
                  ((sub_y_half & 0x1u) << 26) |
                  ((water_offset & 0x1u) << 27) |
                  ((fluid_lvl & 0x7u) << 28) |
                  ((water_vertical_flow & 0x1u) << 31);
        return v;
    }

    static inline PackedVoxelVertex encode_smooth_fluid(
        uint32_t x, uint32_t y, uint32_t z,
        float corner_height,
        const glm::vec2& flow_dir,
        uint32_t mat_id = MAT_WATER,
        uint32_t corner_idx = 0
    ) {
        PackedVoxelVertex v;
        v.data0 = (x & 0x3Fu) |
                  ((y & 0x3Fu) << 6) |
                  ((z & 0x3Fu) << 12) |
                  ((2u & 0x7u) << 18) | // normal_idx = 2 (+Y)
                  (0u << 21) |          // ao = 0
                  ((mat_id & 0xFFu) << 23);

        uint32_t height_fixed = static_cast<uint32_t>(std::clamp(std::round(corner_height * 127.0f), 0.0f, 127.0f));
        uint32_t flow_packed = 0;
        if (glm::length(flow_dir) >= 0.001f) {
            float angle = std::atan2(flow_dir.y, flow_dir.x);
            float norm_angle = (angle + 3.1415926535f) / (2.0f * 3.1415926535f);
            flow_packed = 1 + static_cast<uint32_t>(std::clamp(std::round(norm_angle * 30.0f), 0.0f, 30.0f));
        }

        v.data1 = (1u & 0x3Fu) |               // u_dim = 1 (bits 0..5)
                  ((1u & 0x3Fu) << 6) |         // v_dim = 1 (bits 6..11)
                  ((corner_idx & 0x3u) << 12) | // corner_idx = 0..3 (bits 12..13)
                  ((height_fixed & 0x7Fu) << 14) | // 7-bit corner height (bits 14..20)
                  ((flow_packed & 0x1Fu) << 21) |  // 5-bit flow dir (bits 21..25)
                  // bits 26 (sub_y_half), 27 (water_offset) are strictly 0
                  (6u << 28);                   // fluid_lvl = 6 (bits 28..30)
        return v;
    }

    inline bool is_vertical_flow() const {
        return ((data1 >> 31) & 0x1u) != 0;
    }

    inline glm::vec3 position() const {
        float px = static_cast<float>(data0 & 0x3Fu);
        uint32_t fluid_lvl = (data1 >> 28) & 0x7u;
        float pz = static_cast<float>((data0 >> 12) & 0x3Fu);
        if (fluid_lvl == 6) {
            float cornerH = static_cast<float>((data1 >> 14) & 0x7Fu) / 127.0f;
            float py = static_cast<float>((data0 >> 6) & 0x3Fu) + cornerH;
            return glm::vec3(px, py, pz);
        }
        float recess = 0.0f;
        if (fluid_lvl > 0) {
            float liquidHeight = GetFluidHeight(static_cast<int>(fluid_lvl));
            recess = 1.0f - liquidHeight;
        }
        float py = static_cast<float>((data0 >> 6) & 0x3Fu)
                   - (((data1 >> 26) & 0x1u) ? 0.5f : 0.0f)
                   + (((data1 >> 27) & 0x1u) ? 0.04f : 0.0f)
                   - recess;
        return glm::vec3(px, py, pz);
    }
};
#pragma pack(pop)
static_assert(sizeof(PackedVoxelVertex) == 8, "PackedVoxelVertex must be exactly 8 bytes");

} // namespace Voidfall
