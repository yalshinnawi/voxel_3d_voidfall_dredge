#pragma once
#include "level_shapes.hpp"
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace Voidfall {

// ROOM CARVING & DOORWAY INTEGRATION HELPERS
// ─────────────────────────────────────────────────────────────

static inline bool is_doorway_floor(const RoomPlacement &room, int dx, int dz,
                                    int y) {
  if (y != room.floor_y)
    return false;
  if (room.connected_east && dx > 0 && std::abs(dz) <= 2)
    return true;
  if (room.connected_west && dx < 0 && std::abs(dz) <= 2)
    return true;
  if (room.connected_north && dz > 0 && std::abs(dx) <= 2)
    return true;
  if (room.connected_south && dz < 0 && std::abs(dx) <= 2)
    return true;
  if (room.connected_ne && dx > 0 && dz > 0 && std::abs(dx - dz) <= 2)
    return true;
  if (room.connected_nw && dx < 0 && dz > 0 && std::abs((-dx) - dz) <= 2)
    return true;
  if (room.connected_se && dx > 0 && dz < 0 && std::abs(dx - (-dz)) <= 2)
    return true;
  if (room.connected_sw && dx < 0 && dz < 0 && std::abs((-dx) - (-dz)) <= 2)
    return true;
  return false;
}

static inline bool is_doorway_air(const RoomPlacement &room, int dx, int dz,
                                  int y) {
  if (y <= room.floor_y || y > room.floor_y + 4)
    return false;
  if (room.connected_east && dx > 0 && std::abs(dz) <= 2)
    return true;
  if (room.connected_west && dx < 0 && std::abs(dz) <= 2)
    return true;
  if (room.connected_north && dz > 0 && std::abs(dx) <= 2)
    return true;
  if (room.connected_south && dz < 0 && std::abs(dx) <= 2)
    return true;
  if (room.connected_ne && dx > 0 && dz > 0 && std::abs(dx - dz) <= 2)
    return true;
  if (room.connected_nw && dx < 0 && dz > 0 && std::abs((-dx) - dz) <= 2)
    return true;
  if (room.connected_se && dx > 0 && dz < 0 && std::abs(dx - (-dz)) <= 2)
    return true;
  if (room.connected_sw && dx < 0 && dz < 0 && std::abs((-dx) - (-dz)) <= 2)
    return true;
  return false;
}

static inline bool is_doorway_corridor(const RoomPlacement &room, int dx,
                                       int dz, int buffer = 4) {
  if (room.connected_east && dx > 0 && std::abs(dz) <= buffer)
    return true;
  if (room.connected_west && dx < 0 && std::abs(dz) <= buffer)
    return true;
  if (room.connected_north && dz > 0 && std::abs(dx) <= buffer)
    return true;
  if (room.connected_south && dz < 0 && std::abs(dx) <= buffer)
    return true;
  if ((room.connected_north || room.connected_south) && std::abs(dx) <= buffer)
    return true;
  if ((room.connected_east || room.connected_west) && std::abs(dz) <= buffer)
    return true;
  if (room.connected_ne && dx > 0 && dz > 0 && std::abs(dx - dz) <= buffer)
    return true;
  if (room.connected_nw && dx < 0 && dz > 0 && std::abs((-dx) - dz) <= buffer)
    return true;
  if (room.connected_se && dx > 0 && dz < 0 && std::abs(dx - (-dz)) <= buffer)
    return true;
  if (room.connected_sw && dx < 0 && dz < 0 &&
      std::abs((-dx) - (-dz)) <= buffer)
    return true;
  return false;
}

static inline float compute_room_wall_radius(const RoomPlacement &room,
                                             float dx, float dz, float angle,
                                             float noise, int y = -1) {
  float rx = static_cast<float>(room.half_width);
  float rz = static_cast<float>(room.half_depth);
  float cos_a = std::cos(angle);
  float sin_a = std::sin(angle);
  float denom = std::sqrt((rz * cos_a) * (rz * cos_a) +
                          (rx * sin_a) * (rx * sin_a) + 0.001f);
  float r_base = (rx * rz) / denom;
  float sin2 = std::sin(2.0f * angle);
  float r = r_base * (1.02f - 0.08f * (sin2 * sin2)) + noise;

  if (room.type == RoomShapeType::IndustrialVaultBunker ||
      room.type == RoomShapeType::LaserDefenseFoundry ||
      room.type == RoomShapeType::PrecursorCoolantReservoir ||
      room.type == RoomShapeType::CyclopeanExcavationSilo) {
    return r;
  }

  // 1. Seeded Asymmetric Harmonics (Non-Repetitiveness across rooms)
  uint32_t r_seed = static_cast<uint32_t>(room.center.x * 73856093 ^ room.center.z * 19349663 ^ static_cast<int>(room.type) * 83492791);
  float phase1 = static_cast<float>(r_seed & 0xFF) * (6.2831853f / 255.0f);
  float phase2 = static_cast<float>((r_seed >> 8) & 0xFF) * (6.2831853f / 255.0f);
  float phase3 = static_cast<float>((r_seed >> 16) & 0xFF) * (6.2831853f / 255.0f);

  float asymmetry = std::sin(3.0f * angle + phase1) * 0.45f +
                    std::sin(5.0f * angle + phase2) * 0.28f +
                    std::cos(7.0f * angle + phase3) * 0.16f;

  // 2. 3D Vertical Undulation & Strata Overhangs
  float vertical_undulation = 0.0f;
  if (y >= 0) {
    float norm_y = static_cast<float>(y - room.floor_y) / std::max(1.0f, static_cast<float>(room.ceiling_y - room.floor_y));
    float belly_swell = std::sin(norm_y * 3.14159f) * 0.42f;
    float strata_ripple = std::sin(static_cast<float>(y) * 0.58f + angle * 2.2f + phase2) * 0.52f +
                          std::cos(static_cast<float>(y) * 0.32f - angle * 3.1f + phase1) * 0.38f;
    vertical_undulation = belly_swell + strata_ripple;
  }

  // 3. Multi-Voxel Organic Rock Cragginess & Natural Niches
  int wx = room.center.x + static_cast<int>(dx);
  int wz = room.center.z + static_cast<int>(dz);
  uint32_t crag_h = (static_cast<uint32_t>(wx) * 73856093u) ^
                    (static_cast<uint32_t>(std::max(0, y)) * 19349663u) ^
                    (static_cast<uint32_t>(wz) * 83492791u);
  float crag = (static_cast<float>(crag_h % 1000) / 1000.0f - 0.5f) * 1.1f;

  uint32_t macro_cell = ((static_cast<uint32_t>(wx / 4) * 49979687u) ^
                         (static_cast<uint32_t>(std::max(0, y) / 3) * 982451653u) ^
                         (static_cast<uint32_t>(wz / 4) * 231181u));
  if ((macro_cell % 100) < 12) {
    crag += 0.80f; // Natural wall alcove expands cavern outwards
  } else if ((macro_cell % 100) > 88) {
    crag -= 0.70f; // Natural rock spur projects into cavern
  }

  r += asymmetry + vertical_undulation + crag;

  if (is_doorway_corridor(room, static_cast<int>(dx), static_cast<int>(dz), 3)) {
    r = std::max(r, r_base * 0.95f);
  }

  float min_r = std::min(rx, rz) * 0.72f;
  float max_r = std::max(rx, rz) * 1.08f;
  return std::clamp(r, min_r, max_r);
}

static inline void check_doorway_or_solid(const RoomPlacement &room, int dx,
                                          int dz, int y, uint8_t host_mat,
                                          Voxel &out) {
  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Industrial rooms retain clean metallic bulkhead construction with occasional titanium framing
  if (host_mat == MAT_INDUSTRIAL_BULKHEAD ||
      room.type == RoomShapeType::IndustrialVaultBunker ||
      room.type == RoomShapeType::LaserDefenseFoundry ||
      room.type == RoomShapeType::PrecursorCoolantReservoir ||
      room.type == RoomShapeType::CyclopeanExcavationSilo) {
    int wx = room.center.x + dx;
    int wz = room.center.z + dz;
    uint32_t ind_h = (static_cast<uint32_t>(wx) * 31337u) ^ (static_cast<uint32_t>(y) * 1013904223u) ^ (static_cast<uint32_t>(wz) * 107u);
    if ((ind_h % 100) < 8) {
      out = Voxel{MAT_TITANIUM, 0};
    } else {
      out = Voxel{host_mat, 0};
    }
    return;
  }

  // 1. Natural Geological Strata Layers (Wavy Sedimentary Rock Bands)
  int wx = room.center.x + dx;
  int wz = room.center.z + dz;
  float wavy_y = static_cast<float>(y) + std::sin(static_cast<float>(wx) * 0.22f) * 1.1f + std::cos(static_cast<float>(wz) * 0.22f) * 0.9f;
  int band = static_cast<int>(std::floor(wavy_y + 100.0f)) % 7;
  uint8_t varied_mat = host_mat;
  if (band == 0 || band == 1) {
    varied_mat = MAT_VOLCANIC_BASALT; // Dark volcanic basalt band
  } else if (band >= 2 && band <= 4) {
    varied_mat = MAT_FRACTURED_GRANITE; // Lighter fractured granite band
  } else if (band == 5 && y <= 8) {
    varied_mat = MAT_DREDGE_BEDROCK; // Dense ancient bedrock stratum in deep crust
  }

  // 2. High-Frequency Mottled Rock Inclusions (Crumbly vs Tough Rock)
  uint32_t patch_h = (static_cast<uint32_t>(wx) * 73856093u) ^
                     (static_cast<uint32_t>(y) * 19349663u) ^
                     (static_cast<uint32_t>(wz) * 83492791u);
  if ((patch_h % 100) < 14) {
    varied_mat = (varied_mat == MAT_FRACTURED_GRANITE) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE;
  }

  // 3. Moisture & Cave Lichen/Moss at Wall Bases
  if (y == room.floor_y + 1 || y == room.floor_y + 2) {
    if (room.type == RoomShapeType::FungoidBioGrotto ||
        room.type == RoomShapeType::SubterraneanAquiferOasis ||
        room.type == RoomShapeType::BioluminescentGlowwormGrotto ||
        room.type == RoomShapeType::BioluminescentFirmamentAbyss ||
        room.type == RoomShapeType::CrystallineGeode ||
        room.has_random_fluid) {
      if (((patch_h >> 4) % 100) < 24) {
        varied_mat = MAT_BIOLUMINESCENT_FLORA;
      }
    }
  }

  out = Voxel{varied_mat, 0};
}

// ─────────────────────────────────────────────────────────────
// SPECIFIC ROOM SHAPE CARVERS (Organic, Realistic & Non-Square)
// ─────────────────────────────────────────────────────────────


} // namespace Voidfall
