#pragma once
#include <cstdint>
#include <glm/glm.hpp>

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
    MAT_COUNT
};

// Aliases for gameplay and surveying systems
constexpr uint8_t MAT_VOIDITE = MAT_VOIDITE_CRYSTAL;
constexpr uint8_t MAT_TITANIUM = MAT_INDUSTRIAL_BULKHEAD;
constexpr uint8_t MAT_BULKHEAD = MAT_INDUSTRIAL_BULKHEAD;
constexpr uint8_t MAT_VAULT_DOOR = MAT_REINFORCED_VAULT_DOOR;
constexpr uint8_t MAT_RADIOACTIVE = MAT_RADIOACTIVE_ORE;
constexpr uint8_t MAT_GRANITE = MAT_FRACTURED_GRANITE;
constexpr uint8_t MAT_BASALT = MAT_VOLCANIC_BASALT;

// 16-bit packed voxel state
#pragma pack(push, 1)
struct Voxel {
    uint8_t material_id{MAT_AIR};
    // Flags:
    // bit 0-3: Damage tier (0..15)
    // bit 4:   Anchored / structural node
    // bit 5:   Active emissive
    // bit 6:   Surveyed / pinged highlight
    // bit 7:   Reserved
    uint8_t flags_and_damage{0};

    inline bool is_solid() const { return material_id != MAT_AIR; }
    inline bool is_anchored() const { return (flags_and_damage & 0x10) != 0 || material_id == MAT_DREDGE_BEDROCK || material_id == MAT_REINFORCED_VAULT_DOOR; }
    inline uint8_t damage() const { return flags_and_damage & 0x0F; }
    inline void set_damage(uint8_t d) { flags_and_damage = (flags_and_damage & 0xF0) | (d & 0x0F); }
    inline bool is_highlighted() const { return (flags_and_damage & 0x40) != 0; }
    inline void set_highlighted(bool h) {
        if (h) flags_and_damage |= 0x40;
        else   flags_and_damage &= ~0x40;
    }
};
#pragma pack(pop)
static_assert(sizeof(Voxel) == 2, "Voxel struct must be exactly 2 bytes (16-bit packed)");

// 8-byte bit-packed vertex format for GPU greedy meshing
#pragma pack(push, 1)
struct PackedVoxelVertex {
    // uint32_t data0:
    // [0..4]   x (5 bits: 0..31)
    // [5..9]   y (5 bits: 0..31)
    // [10..14] z (5 bits: 0..31)
    // [15..17] normal_idx (3 bits: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z)
    // [18..19] ao (2 bits: 0..3 baked ambient occlusion)
    // [20..27] tex_layer (8 bits: 0..255 texture array layer ID)
    // [28..31] aux / pbr_flags (4 bits: e.g. metallic/roughness modifier)
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
        v.data0 = (x & 0x1Fu) |
                  ((y & 0x1Fu) << 5) |
                  ((z & 0x1Fu) << 10) |
                  ((normal_idx & 0x7u) << 15) |
                  ((ao & 0x3u) << 18) |
                  ((tex_layer & 0xFFu) << 20) |
                  ((aux & 0xFu) << 28);

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
