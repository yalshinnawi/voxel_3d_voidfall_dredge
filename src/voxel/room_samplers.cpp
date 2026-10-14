#include "level_shapes.hpp"
#include "room_carving_utils.hpp"
#include "packed_vertex.hpp"
#include "voxel_types.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

namespace Voidfall {

void LevelGenerator::sample_spawn_cavern(const RoomPlacement &room, int x,
                                         int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));
  float sin2 = std::sin(2.0f * angle);

  // Natural curved cavern envelope: rounded corners (r ~ 7.6 at diagonals),
  // expanding to r ~ 8.35 at cardinal points
  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 101) - 0.5f) * 0.30f);

  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
    return;
  }

  // Floor at y = 4
  if (y == room.floor_y) {
    if (r_horiz <= 3.6f) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED}; // Central Delver drop pad
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // Training mineral niche in northeast lobe (dx in [3, 5], dz in [3, 5])
  if (dx >= 3 && dx <= 5 && dz >= 3 && dz <= 5 && y >= room.floor_y + 1 &&
      y <= room.floor_y + 3) {
    if (y == room.floor_y + 2 && dx == 4 && dz == 4) {
      out = Voxel{MAT_TITANIUM, 0};
    } else if (y == room.floor_y + 1 && (dx == 3 || dz == 3)) {
      out = Voxel{MAT_TITANIUM, 0};
    } else {
      out = Voxel{MAT_FRACTURED_GRANITE, 0};
    }
    return;
  }

  // Smooth arched natural dome ceiling
  float r_span = std::max(
      4.0f, static_cast<float>(std::min(room.half_width, room.half_depth)));
  float norm_dist = r_horiz / r_span;
  int arch_height =
      static_cast<int>(static_cast<float>(room.ceiling_y) - norm_dist * 8.0f);
  if (y > arch_height) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  // Hanging stalactites from ceiling anchored into arch
  uint32_t stal_h = hash_coord(x, 0, z, 41);
  if ((stal_h % 9 == 0) && r_horiz >= 2.5f) {
    int stal_len = 1 + static_cast<int>(stal_h % 3);
    if (y >= arch_height - stal_len && y <= arch_height) {
      out = Voxel{MAT_FRACTURED_GRANITE, VOXEL_FLAG_ANCHORED};
      return;
    }
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_pillar_hall(const RoomPlacement &room, int x, int y,
                                        int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  // Vaulted cathedral envelope with side alcoves
  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 102) - 0.5f) * 0.30f);

  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
    return;
  }

  if (y == room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  uint32_t room_hash = hash_coord(room.center.x, room.center.z, 0, 401);
  int p_dist_x = 3 + static_cast<int>(room_hash % 2);
  int p_dist_z = 3 + static_cast<int>((room_hash >> 1) % 2);

  // 4 thick structural columns with climbable steps
  bool is_pillar_x = (std::abs(dx) >= p_dist_x && std::abs(dx) <= p_dist_x + 1);
  bool is_pillar_z = (std::abs(dz) >= p_dist_z && std::abs(dz) <= p_dist_z + 1);

  if (is_pillar_x && is_pillar_z && y < room.ceiling_y) {
    if ((x > room.center.x) && (y % 6 == 0)) {
      out = Voxel{MAT_TITANIUM, 0}; // Structural reinforcement bands
    } else if (y % 3 == 1) {
      out = Voxel{MAT_TITANIUM, 0}; // Climbable structural rungs
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // Elevated parkour arch bridge (relative to room.floor_y)
  int bridge_y = room.floor_y + 4 + static_cast<int>((room_hash >> 2) % 2);
  bool bridge_align_x = ((room_hash >> 3) % 2 == 1);
  bool is_bridge = false;
  if (bridge_align_x) {
    is_bridge = (y == bridge_y && std::abs(dz) <= 1 &&
                 std::abs(dx) <= room.half_width);
  } else {
    is_bridge = (y == bridge_y && std::abs(dx) <= 1 &&
                 std::abs(dz) <= room.half_depth);
  }

  // Arch piers grounding the bridge to the cavern floor at intervals
  bool is_bridge_pier = false;
  if (bridge_y <= room.ceiling_y - 6 && y >= room.floor_y && y <= bridge_y) {
    if (bridge_align_x && std::abs(dz) <= 1 && (std::abs(dx) == 4 || std::abs(dx) >= room.half_width - 2)) {
      is_bridge_pier = true;
    } else if (!bridge_align_x && std::abs(dx) <= 1 && (std::abs(dz) == 4 || std::abs(dz) >= room.half_depth - 2)) {
      is_bridge_pier = true;
    }
  }

  if ((is_bridge || is_bridge_pier) && bridge_y <= room.ceiling_y - 6) {
    if (dx == 0 && dz == 0 && y == bridge_y) {
      out = Voxel{MAT_TITANIUM, 0}; // Central bridge keystone
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, static_cast<uint8_t>(is_bridge_pier ? VOXEL_FLAG_ANCHORED : 0)};
    }
    return;
  }

  // Arched cathedral nave ceiling
  float r_base = static_cast<float>(std::min(room.half_width, room.half_depth));
  float factor = (r_horiz * r_horiz) / (r_base * r_base);
  int arch_y =
      static_cast<int>(static_cast<float>(room.ceiling_y - 1) - factor * 7.0f);
  if (y >= arch_y) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_crystalline_geode(const RoomPlacement &room, int x,
                                              int y, int z, Voxel &out) const {
  float dx_f = static_cast<float>(x - room.center.x);
  float dz_f = static_cast<float>(z - room.center.z);
  float dy_f = static_cast<float>(y - room.floor_y);

  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float angle = std::atan2(dz_f, dx_f);
  float eff_rad = compute_room_wall_radius(room, dx_f, dz_f, angle, 0.0f);

  // True 3D hollow geode sphere/ellipsoid
  float r_horiz_sq = (dx_f * dx_f + dz_f * dz_f) / (eff_rad * eff_rad);
  float r_vert_sq = ((dy_f - 9.0f) * (dy_f - 9.0f)) / (10.0f * 10.0f);
  float r_norm = r_horiz_sq + r_vert_sq;

  if (r_norm > 1.05f) {
    check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
    return;
  }

  if (y <= room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Doorway clearance check near connected corridor entrances
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Crystalline shell lining around geode inner cavity wall
  if (r_norm >= 0.86f) {
    if ((hash_coord(x, y, z, 77) % 4 == 0) && (y >= room.floor_y + 2)) {
      out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
    } else {
      out = Voxel{MAT_FRACTURED_GRANITE, 0};
    }
    return;
  }

  uint32_t geode_hash = hash_coord(room.center.x, room.center.z, 0, 619);
  float target_rad = 4.2f + static_cast<float>(geode_hash % 3) * 0.4f;

  // Parkour ring stepping stones: natural granite rock pinnacles rising from basin floor
  float rad = std::sqrt(dx_f * dx_f + dz_f * dz_f);
  int step_y1 = room.floor_y + 3;
  int step_y2 = room.floor_y + 6;
  uint32_t step_h = hash_coord(x, 0, z, 99);
  if (std::abs(rad - target_rad) <= 0.85f && (step_h % 5 == 0)) {
    int target_top = (step_h % 2 == 0) ? step_y1 : step_y2;
    if (y >= room.floor_y && y <= target_top) {
      out = (y == target_top) ? Voxel{MAT_FRACTURED_GRANITE, 0}
                              : Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
      return;
    }
  }

  // Central natural basalt geological rock pinnacle rising from basin floor
  int spire_dx = static_cast<int>((geode_hash >> 2) % 3) - 1; // -1, 0, 1
  int spire_dz = static_cast<int>((geode_hash >> 4) % 3) - 1; // -1, 0, 1
  if (std::abs(dx - spire_dx) <= 1 && std::abs(dz - spire_dz) <= 1 &&
      y <= room.floor_y + 5) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_terraced_quarry(const RoomPlacement &room, int x,
                                            int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 104) - 0.5f) * 0.30f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
    return;
  }

  // Doorway connection paths always step down to floor y=room.floor_y
  bool is_doorway_axis =
      (std::abs(dx) <= 2 && (room.connected_north || room.connected_south)) ||
      (std::abs(dz) <= 2 && (room.connected_east || room.connected_west));

  // Concentric stepped circular amphitheater tiers
  int terrace_floor = room.floor_y;
  if (!is_doorway_axis) {
    if (r_horiz > 6.0f) {
      terrace_floor = room.floor_y + 4; // Tier 2
    } else if (r_horiz > 3.8f) {
      terrace_floor = room.floor_y + 2; // Tier 1
    } else {
      terrace_floor = room.floor_y; // Basin
    }

    // Spiral ramp carved in quadrant determined by seed hash
    uint32_t q = hash_coord(room.center.x, room.center.z, 0, 883) % 4;
    bool in_ramp = false;
    if (q == 0)
      in_ramp = (angle > 0.35f && angle < 1.55f); // NE
    else if (q == 1)
      in_ramp = (angle > 1.90f && angle < 3.05f); // NW
    else if (q == 2)
      in_ramp = (angle > -2.75f && angle < -1.60f); // SW
    else
      in_ramp = (angle > -1.20f && angle < -0.10f); // SE

    if (in_ramp && r_horiz >= 3.4f && r_horiz <= 6.4f) {
      terrace_floor = room.floor_y + static_cast<int>((r_horiz - 3.4f) * 1.33f);
    }
  }

  if (y <= terrace_floor) {
    if (y == terrace_floor &&
        (r_horiz <= 1.2f || (std::abs(r_horiz - 3.8f) < 0.6f &&
                             (hash_coord(x, y, z, 42) % 4 == 0)))) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    } else if (y == terrace_floor && (hash_coord(x, y, z, 88) % 5 == 0)) {
      out = Voxel{MAT_TITANIUM, 0};
    } else {
      out = Voxel{MAT_FRACTURED_GRANITE, 0};
    }
    return;
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_vault_bunker(const RoomPlacement &room, int x,
                                         int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  int abs_x = std::abs(dx);
  int abs_z = std::abs(dz);
  int manhattan = abs_x + abs_z;

  // 45-degree chamfered octagonal bulkhead perimeter (keeps cardinal paths
  // open, cuts diagonal corners)
  bool is_outer_wall = (abs_x > room.half_width || abs_z > room.half_depth ||
                        manhattan >= (room.half_width + room.half_depth - 2));
  if (is_outer_wall) {
    if (is_doorway_floor(room, dx, dz, y)) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (is_doorway_air(room, dx, dz, y)) {
      out = Voxel{MAT_AIR, 0};
      return;
    }
    // Reinforced portal frames around doorway edges
    bool is_portal_frame = (abs_x == room.half_width && abs_z <= 3) ||
                           (abs_z == room.half_depth && abs_x <= 3);
    if (is_portal_frame && y <= room.floor_y + 5) {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    }
    return;
  }

  if (y == room.floor_y) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Elevated industrial catwalk mezzanine at y = 9 along perimeter
  bool is_catwalk = (y == 9) && (abs_x >= 4 || abs_z >= 4) && (manhattan <= 13);
  if (is_catwalk) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
    return;
  }

  // Catwalk mezzanine access and structural support pillars at corners
  if ((abs_x == 4 && abs_z == 4) && y >= room.floor_y && y <= 9) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Central research salvage console at (0, 0)
  if (abs_x <= 1 && abs_z <= 1 && y <= room.floor_y + 2) {
    if (y == room.floor_y + 2 && dx == 0 && dz == 0) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    } else {
      out = Voxel{MAT_TITANIUM, 0};
    }
    return;
  }

  if (y >= room.ceiling_y - 3) {
    out = Voxel{static_cast<uint8_t>(((x + z) % 3 == 0) ? MAT_TITANIUM
                                                        : MAT_VOLCANIC_BASALT),
                0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_fault_crevasse(const RoomPlacement &room, int x,
                                           int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 106) - 0.5f) * 0.30f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  // Sinuous jagged crevasse path
  float fissure_center = std::sin(static_cast<float>(dz) * 0.38f) * 2.2f;
  float dist_to_fissure = std::abs(static_cast<float>(dx) - fissure_center);
  bool is_perimeter = (std::abs(dz) >= room.half_depth - 2 || std::abs(dx) >= room.half_width - 2);
  bool in_crevasse = (dist_to_fissure <= 1.8f) && !is_perimeter;
  bool is_bridge = std::abs(dz) <= 1; // Natural stone bridge across fissure

  int fissure_bed = (room.floor_y > 6) ? 4 : std::min(room.floor_y, 4);

  if (in_crevasse) {
    if (is_bridge) {
      // Natural stone bridge across fissure at room.floor_y
      if (y == room.floor_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
      }
      if (y == room.floor_y - 1 && room.floor_y > fissure_bed) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Bridge arch thickness
        return;
      }
      if (y < room.floor_y - 1 && y > fissure_bed) {
        out = Voxel{MAT_AIR, 0}; // Deep plunge beneath bridge
        return;
      }
      if (y <= fissure_bed) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Bed beneath bridge
        return;
      }
    } else {
      // Inside open crevasse abyss
      if (y < fissure_bed) {
        out = Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
        return;
      }
      if (y == fissure_bed) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Fissure floor
        return;
      }
      if (y == fissure_bed + 1 && room.floor_y > 6) {
        // Toxic gas fissure pockets or glowing voidite veins at fissure bottom
        uint32_t gh = hash_coord(x, y, z, 77);
        if (gh % 5 == 0) {
          out = Voxel{MAT_TOXIC_GAS, 0};
        } else if (gh % 7 == 0) {
          out = Voxel{MAT_TITANIUM, 0};
        } else {
          out = Voxel{MAT_AIR, 0};
        }
        return;
      }
      // Wall climbing crag footholds for scaling back up if fallen into fissure
      if (room.floor_y > 6 && dist_to_fissure >= 1.3f && y <= room.floor_y) {
        if ((y == 7 || y == room.floor_y) && ((std::abs(dz) + x) % 3 == 0)) {
          out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Wall crag foothold
          return;
        }
      }
      if (y > fissure_bed && y < room.ceiling_y - 3) {
        out = Voxel{MAT_AIR, 0};
        return;
      }
    }
  }

  if (y < room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Solid rock outside crevasse
    return;
  }

  if (y == room.floor_y) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  // High stalactites for grappling across the crevasse
  if (y >= room.ceiling_y - 5 && is_bridge && std::abs(dx) <= 1) {
    out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    return;
  }

  if (y >= room.ceiling_y - 3) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_abyssal_chasm(const RoomPlacement &room, int x,
                                          int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 107) - 0.5f) * 0.30f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Staggered spiral parkour ledges along circular chasm walls
  bool is_ledge_1 =
      (y == 7 && r_horiz >= 4.5f && angle >= -0.5f && angle <= 1.2f);
  bool is_ledge_2 =
      (y == 11 && r_horiz >= 4.5f && angle >= 1.0f && angle <= 2.6f);
  bool is_ledge_3 =
      (y == 15 && r_horiz >= 4.5f && (angle >= 2.4f || angle <= -2.4f));
  bool is_ledge_4 =
      (y == 19 && r_horiz >= 4.5f && angle >= -2.6f && angle <= -0.8f);

  if (is_ledge_1 || is_ledge_2 || is_ledge_3 || is_ledge_4) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Center hanging crystal stalactite (grapple target at y = 14..22)
  if (std::abs(dx) <= 1 && std::abs(dz) <= 1 && y >= 14 &&
      y <= room.ceiling_y - 2) {
    if (y == 14) {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    }
    return;
  }

  // Chasm bottom bed is solid basalt at y = 4 (bedrock below)
  if (y <= 4) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Perimeter entrance walkway at room.floor_y
  bool is_perimeter = (r_horiz >= room.half_width - 2.0f);
  if (is_perimeter && y == room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_radioactive_sanctuary(const RoomPlacement &room,
                                                  int x, int y, int z,
                                                  Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 108) - 0.5f) * 0.30f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  // Floor with radioactive moat
  if (y == room.floor_y) {
    bool is_step = (std::abs(dx) <= 1 || std::abs(dz) <= 1);
    if (r_horiz >= 2.6f && r_horiz <= 5.5f && !is_step) {
      out = Voxel{MAT_RADIOACTIVE_ORE,
                  VOXEL_FLAG_EMISSIVE}; // Toxic radioactive moat
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // Central altar with colossal Voidite Monolith
  int monolith_top = std::min(room.floor_y + 5, room.ceiling_y - 4);
  if (r_horiz <= 2.2f && y >= room.floor_y + 1 && y <= monolith_top) {
    out = Voxel{MAT_RADIOACTIVE_ORE, VOXEL_FLAG_EMISSIVE};
    return;
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_extraction_bay(const RoomPlacement &room, int x,
                                           int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float abs_x = static_cast<float>(std::abs(dx));
  float abs_z = static_cast<float>(std::abs(dz));
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));

  // Fortified hexagonal touchdown perimeter adapted to room dimensions
  float max_hw = static_cast<float>(room.half_width);
  float max_hd = static_cast<float>(room.half_depth);
  bool is_outer =
      (abs_x > (max_hw + 0.2f) || (abs_z + 0.45f * abs_x) > (max_hd + 1.8f));
  if (is_outer) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  // Floor at room.floor_y
  if (y == room.floor_y) {
    if (r_horiz <= 3.2f) {
      out =
          Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED}; // Central touchdown cradle
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // Defensive holdout perimeter barricades for the 40s holdout timer
  bool is_barricade = (r_horiz >= 4.2f && r_horiz <= 5.8f) &&
                      (y == room.floor_y + 1) &&
                      (std::abs(dx) >= 3 || std::abs(dz) >= 3);
  if (is_barricade) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
    return;
  }

  // High vertical extraction launch shaft in the center for the evac pod
  if (r_horiz <= 3.2f && y <= room.ceiling_y + 1) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Dome ceiling around the shaft
  if (y >= room.ceiling_y - 3) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

// ─────────────────────────────────────────────────────────────
// NEW ADVANCED ROOM ARCHETYPES (Parkour, Hazards, & Traversal)
// ─────────────────────────────────────────────────────────────

void LevelGenerator::sample_magma_caldera(const RoomPlacement &room, int x,
                                          int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 110) - 0.5f) * 0.30f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  bool is_entry = is_doorway_corridor(room, dx, dz, 3) ||
                  is_doorway_floor(room, dx, dz, y) ||
                  is_doorway_air(room, dx, dz, y);

  uint32_t cal_hash = hash_coord(room.center.x, room.center.z, 0, 997);
  int grid_phase_x = static_cast<int>(cal_hash % 2);
  int grid_phase_z = static_cast<int>((cal_hash >> 1) % 2);

  // Caldera lake basin radius - strictly inside room with safe perimeter
  // walkway & rim
  float r_basin_max =
      std::max(2.5f, static_cast<float>(room.half_width) - 3.4f);

  // Stepping stone pillars across the molten lake at grid points
  bool is_stepping_stone = ((std::abs(dx) + grid_phase_x) % 3 == 0 &&
                            (std::abs(dz) + grid_phase_z) % 3 == 0) &&
                           (r_horiz <= (r_basin_max - 0.8f));

  // Central basalt chimney rising from caldera bottom at (0, 0)
  if (r_horiz <= 1.2f && y <= room.floor_y + 4) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Watertight Caldera Basin containment:
  // Inside the basin hollow (r_horiz <= r_basin_max)
  if (r_horiz <= r_basin_max) {
    if (y == room.floor_y) {
      if (is_entry || is_stepping_stone) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
      } else {
        out = Voxel{MAT_THERMITE_SLAG,
                    VOXEL_FLAG_EMISSIVE}; // Sealed molten lava lake
      }
      return;
    }
    if (y == room.floor_y + 1 && is_stepping_stone) {
      out = Voxel{MAT_VOLCANIC_BASALT,
                  0}; // Stepping stone rising above lava surface
      return;
    }
  }

  // Basalt retaining rim / lip enclosing the caldera basin where bounding walls
  // exceed liquid surface height
  if (r_horiz > r_basin_max && r_horiz <= r_basin_max + 1.2f && !is_entry) {
    if (y == room.floor_y || y == room.floor_y + 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Raised basalt bounding wall
      return;
    }
  }

  // Natural floor for perimeter walkways and doorway entrances
  if (y == room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Natural basalt arch bridging the caldera (orientation toggles NE-SW or
  // NW-SE)
  bool arch_diag_ne = ((cal_hash >> 2) % 2 == 0);
  int arch_h = room.floor_y + 3 + static_cast<int>((cal_hash >> 3) % 2);
  bool is_arch = false;
  if (arch_diag_ne) {
    is_arch = (std::abs(dx - dz) <= 1 && y == arch_h && r_horiz <= 6.0f);
  } else {
    is_arch = (std::abs(dx + dz) <= 1 && y == arch_h && r_horiz <= 6.0f);
  }

  if (is_arch) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_spike_trench(const RoomPlacement &room, int x,
                                         int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  int abs_x = std::abs(dx);
  int abs_z = std::abs(dz);

  // Diamond / octagonal perimeter
  if (abs_x + abs_z > (room.half_width + room.half_depth - 3)) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Safe perimeter walkway / doorway entrance ledges around room walls
  bool is_perimeter =
      (abs_x >= room.half_width - 2 || abs_z >= room.half_depth - 2);
  if (is_perimeter) {
    if (y < room.floor_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }
    if (y == room.floor_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }
    if (y >= room.floor_y + 1 && y <= room.floor_y + 3) {
      out = Voxel{MAT_AIR, 0};
      return;
    }
  }

  int pit_bottom = (room.floor_y > 6) ? 4 : std::min(room.floor_y, 4);

  if (room.floor_y > 6) {
    // Deep sunken chasm pit plunge
    if (y < pit_bottom) {
      out = Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (y == pit_bottom) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }

    uint32_t trench_hash = hash_coord(room.center.x, room.center.z, 0, 521);
    int spike_parity = static_cast<int>(trench_hash % 2);

    // Lethal glowing crimson obsidian spikes across bottom of pit at y = 5
    bool is_spike = (y == pit_bottom + 1) &&
                    (abs_x >= 2 && abs_x <= room.half_width - 3) &&
                    (abs_z >= 2 && abs_z <= room.half_depth - 3) &&
                    ((abs_x + abs_z) % 2 == spike_parity);
    if (is_spike) {
      out = Voxel{MAT_OBSIDIAN_SPIKES, VOXEL_FLAG_EMISSIVE};
      return;
    }

    // Central safe reward island column
    if (abs_x <= 1 && abs_z <= 1) {
      if (y <= room.floor_y) {
        if (y == room.floor_y) {
          out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
        } else {
          out = Voxel{MAT_TITANIUM, 0};
        }
        return;
      }
    }

    // High balance beam bridges spanning at room.floor_y across X and/or Z axes
    int bridge_mode = static_cast<int>((trench_hash >> 1) % 3);
    bool is_bridge_x = (bridge_mode != 2) && (abs_z == 0) &&
                       y == room.floor_y && (abs_x <= room.half_width);
    bool is_bridge_z = (bridge_mode != 1) && (abs_x == 0) &&
                       y == room.floor_y && (abs_z <= room.half_depth);
    bool is_bridge_gap =
        (abs_x >= 3 && abs_x <= 4) || (abs_z >= 3 && abs_z <= 4);

    bool is_bridge_pier = (is_bridge_x || is_bridge_z) && !is_bridge_gap &&
                          (abs_x == 2 || abs_x >= room.half_width - 3 || abs_z == 2 || abs_z >= room.half_depth - 3) &&
                          y >= pit_bottom && y <= room.floor_y;
    if (is_bridge_pier) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      return;
    }

    if ((is_bridge_x || is_bridge_z) && !is_bridge_gap) {
      out = Voxel{MAT_TITANIUM, 0};
      return;
    }

    // Corner ladder rungs for climbing back out of pit
    bool is_corner_crag =
        (abs_x == room.half_width - 3 && abs_z == room.half_depth - 3);
    if (is_corner_crag && y <= room.floor_y) {
      out = Voxel{MAT_TITANIUM, 0};
      return;
    }

    if (y > pit_bottom && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return;
    }
  } else {
    // Fallback shallow mode for isolated room unit tests at y = 4
    if (y == room.floor_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }

    uint32_t trench_hash = hash_coord(room.center.x, room.center.z, 0, 521);
    int spike_parity = static_cast<int>(trench_hash % 2);

    bool is_spike = (y == room.floor_y + 1) &&
                    (abs_x >= 2 && abs_x <= room.half_width - 3) &&
                    (abs_z >= 2 && abs_z <= room.half_depth - 3) &&
                    ((abs_x + abs_z) % 2 == spike_parity);
    if (is_spike) {
      out = Voxel{MAT_OBSIDIAN_SPIKES, VOXEL_FLAG_EMISSIVE};
      return;
    }

    int bridge_mode = static_cast<int>((trench_hash >> 1) % 3);
    bool is_bridge_x = (bridge_mode != 2) && (abs_z == 0) &&
                       y == room.floor_y + 2 && (abs_x <= room.half_width);
    bool is_bridge_z = (bridge_mode != 1) && (abs_x == 0) &&
                       y == room.floor_y + 2 && (abs_z <= room.half_depth);
    bool is_bridge_gap =
        (abs_x >= 3 && abs_x <= 4) || (abs_z >= 3 && abs_z <= 4);

    bool is_bridge_pier = (is_bridge_x || is_bridge_z) && !is_bridge_gap &&
                          (abs_x == 2 || abs_x >= room.half_width - 3 || abs_z == 2 || abs_z >= room.half_depth - 3) &&
                          y >= room.floor_y && y <= room.floor_y + 2;
    if (is_bridge_pier) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      return;
    }

    if ((is_bridge_x || is_bridge_z) && !is_bridge_gap) {
      out = Voxel{MAT_TITANIUM, 0};
      return;
    }

    if (abs_x <= 1 && abs_z <= 1 && y <= room.floor_y + 2) {
      if (y == room.floor_y + 2) {
        out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      } else {
        out = Voxel{MAT_TITANIUM, 0};
      }
      return;
    }
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_void_singularity(const RoomPlacement &room, int x,
                                             int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 112) - 0.5f) * 0.30f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  // Perimeter boundary ledge at room floor
  bool is_perimeter = (r_horiz >= (room.half_width - 2.5f));

  // In the center rift, the bedrock is torn away into the void except for anchored spire columns
  if (y <= room.floor_y) {
    if (is_perimeter) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }
    // Anchored volcanic basalt spire columns grounding platforms and monolith into bedrock foundation
    bool plat1_col = (std::abs(dx - 3) <= 1 && std::abs(dz + 3) <= 1);
    bool plat2_col = (std::abs(dx + 3) <= 1 && std::abs(dz - 3) <= 1);
    bool plat3_col = (std::abs(dx - 3) <= 1 && std::abs(dz - 2) <= 1);
    bool plat4_col = (std::abs(dx + 3) <= 1 && std::abs(dz + 3) <= 1);
    bool mono_col = (r_horiz <= 1.5f);
    if ((plat1_col || plat2_col || plat3_col || plat4_col || mono_col) && y >= 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, static_cast<uint8_t>((y <= 3) ? VOXEL_FLAG_ANCHORED : 0)};
      return;
    }
    // Void rift core plunges into the bottomless abyss
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Obsidian stepping platforms supported by anchored rock spires from foundation
  bool plat1 =
      (std::abs(dx - 3) <= 1 && std::abs(dz + 3) <= 1 && y <= room.floor_y + 2);
  bool plat2 =
      (std::abs(dx + 3) <= 1 && std::abs(dz - 3) <= 1 && y <= room.floor_y + 4);
  bool plat3 =
      (std::abs(dx - 3) <= 1 && std::abs(dz - 2) <= 1 && y <= room.floor_y + 7);
  bool plat4 = (std::abs(dx + 3) <= 1 && std::abs(dz + 3) <= 1 &&
                y <= room.floor_y + 10);

  if (plat1 || plat2 || plat3 || plat4) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Colossal Void Singularity Monolith anchored into bedrock base
  if (r_horiz <= 1.5f && y <= room.floor_y + 9) {
    out = Voxel{MAT_OBSIDIAN_SPIKES, static_cast<uint8_t>((y <= 3) ? VOXEL_FLAG_ANCHORED : 0)};
    return;
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_fungoid_grotto(const RoomPlacement &room, int x,
                                           int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  // Bulbous organic spore grotto
  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 113) - 0.5f) * 0.30f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
    return;
  }

  if (y == room.floor_y) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  uint32_t grotto_hash = hash_coord(room.center.x, room.center.z, 0, 771);
  int stem1_x = 3 + static_cast<int>(grotto_hash % 2);
  int stem1_z = 3 + static_cast<int>((grotto_hash >> 1) % 2);
  int stem2_x = -3 - static_cast<int>((grotto_hash >> 2) % 2);
  int stem2_z = -3 - static_cast<int>((grotto_hash >> 3) % 2);
  int stem3_x = -3 - static_cast<int>((grotto_hash >> 4) % 2);
  int stem3_z = 3 + static_cast<int>((grotto_hash >> 5) % 2);

  int cap1_y = room.floor_y + 4 + static_cast<int>((grotto_hash >> 6) % 2);
  int cap2_y = room.floor_y + 7 + static_cast<int>((grotto_hash >> 7) % 2);
  int cap3_y = room.floor_y + 10;

  // Giant tiered mushroom caps for vertical parkour jumping:
  // Cap 1 (North-East): stem at (stem1_x, stem1_z)
  int d1 = (dx - stem1_x) * (dx - stem1_x) + (dz - stem1_z) * (dz - stem1_z);
  if (d1 <= 4 && y == cap1_y) {
    out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE}; // Phosphorescent cap
    return;
  }
  if (dx == stem1_x && dz == stem1_z && y <= cap1_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Mushroom stem
    return;
  }

  // Cap 2 (South-West): stem at (stem2_x, stem2_z)
  int d2 = (dx - stem2_x) * (dx - stem2_x) + (dz - stem2_z) * (dz - stem2_z);
  if (d2 <= 4 && y == cap2_y) {
    out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
    return;
  }
  if (dx == stem2_x && dz == stem2_z && y <= cap2_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Cap 3 (North-West): stem at (stem3_x, stem3_z)
  int d3 = (dx - stem3_x) * (dx - stem3_x) + (dz - stem3_z) * (dz - stem3_z);
  if (d3 <= 4 && y == cap3_y && room.ceiling_y > room.floor_y + 11) {
    out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
    return;
  }
  if (dx == stem3_x && dz == stem3_z && y <= cap3_y &&
      room.ceiling_y > room.floor_y + 11) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Spore pockets and gas fissures at floor perimeter
  if (y == room.floor_y + 1 && r_horiz >= (room.half_width - 2.5f) &&
      (hash_coord(x, y, z, 55) % 6 == 0)) {
    out = Voxel{MAT_GAS, 0};
    return;
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_laser_foundry(const RoomPlacement &room, int x,
                                          int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  int abs_x = std::abs(dx);
  int abs_z = std::abs(dz);

  // Industrial chamfered facility
  if (abs_x > room.half_width || abs_z > room.half_depth ||
      (abs_x + abs_z) > (room.half_width + room.half_depth - 3)) {
    check_doorway_or_solid(room, dx, dz, y, MAT_INDUSTRIAL_BULKHEAD, out);
    return;
  }

  // Floor
  bool is_entry = is_doorway_corridor(room, dx, dz, 3);
  if (is_entry && y == room.floor_y) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Watertight molten metal flume basin
  int flume_max_x = room.half_width - 3;
  if (!is_entry && abs_z <= 1 && abs_x >= 2 && abs_x <= flume_max_x) {
    if (y == room.floor_y) {
      out = Voxel{MAT_THERMITE_SLAG, VOXEL_FLAG_EMISSIVE};
      return;
    }
  }

  // Raised industrial bulkhead safety curb enclosing the flume (bounding walls
  // exceed liquid height)
  bool is_flume_curb =
      ((abs_z == 2 && abs_x <= flume_max_x + 1) ||
       (abs_x == flume_max_x + 1 && abs_z <= 2) || (abs_x <= 1 && abs_z <= 2));
  if (is_flume_curb) {
    if (y == room.floor_y) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (y == room.floor_y + 1 && !is_entry) {
      out = Voxel{MAT_INDUSTRIAL_BULKHEAD,
                  0}; // Raised safety curb exceeding liquid surface
      return;
    }
  }

  if (y == room.floor_y) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // High elevated industrial gantry catwalk at y = floor_y + 4 along perimeter
  bool is_catwalk =
      !is_entry &&
      (y == room.floor_y + 4) && (abs_x >= 4 || abs_z >= 4) &&
      (abs_x <= room.half_width && abs_z <= room.half_depth);
  if (is_catwalk) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
    return;
  }

  // Catwalk bridge across the molten flume at x = 0
  if (y == room.floor_y + 4 && abs_x <= 1) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
    return;
  }

  // Structural pylon columns supporting catwalk from floor
  bool is_catwalk_pylon = ((abs_x == 4 && abs_z == 4) ||
                           (abs_x == flume_max_x && abs_z == 4) ||
                           (abs_x == 0 && abs_z == 2)) &&
                          y >= room.floor_y && y <= room.floor_y + 4;
  if (is_catwalk_pylon) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Overhead industrial crane beam at y = room.ceiling_y - 4
  if (y == room.ceiling_y - 4 && (abs_x == 0 || abs_z == 0)) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Security beacon pylons flanking doors grounded to floor
  uint32_t pylon_h = hash_coord(x, 0, z, 12);
  if ((abs_x == 4 || abs_z == 4) && (pylon_h % 4 == 0) &&
      y >= room.floor_y && y <= room.floor_y + 2) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  if (y >= room.ceiling_y - 3) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_crumbling_canyon(const RoomPlacement &room, int x,
                                             int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 115) - 0.5f) * 0.30f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  // Sinuous canyon gorge cutting along Z axis
  float gorge_center = std::sin(static_cast<float>(dz) * 0.38f) * 1.6f;
  float dist_to_gorge = std::abs(static_cast<float>(dx) - gorge_center);
  bool is_perimeter = (std::abs(dz) >= room.half_depth - 2 || std::abs(dx) >= room.half_width - 2);
  bool in_gorge = (dist_to_gorge <= 2.8f) && !is_perimeter;

  int canyon_bed = (room.floor_y > 6) ? 4 : std::min(room.floor_y, 4);

  if (room.floor_y > 6) {
    // Three natural stone bridges spanning the gorge with structural arch piers down to bed
    bool bridge_south =
        in_gorge && (dz >= -5 && dz <= -4) &&
        ((y == room.floor_y) || (y >= canyon_bed && y <= room.floor_y && (dist_to_gorge >= 2.0f || dz == -5)));
    bool bridge_center =
        in_gorge && (std::abs(dz) <= 1) &&
        ((y == room.floor_y + 3) || (y >= canyon_bed && y <= room.floor_y + 3 && (dist_to_gorge >= 2.0f || dz == 0)));
    bool bridge_north =
        in_gorge && (dz >= 4 && dz <= 5) &&
        ((y == room.floor_y) || (y >= canyon_bed && y <= room.floor_y && (dist_to_gorge >= 2.0f || dz == 4)));

    if (bridge_south || bridge_center || bridge_north) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }

    if (in_gorge) {
      if (y < canyon_bed) {
        out = Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
        return;
      }
      if (y == canyon_bed) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
      }
      // Wall climbing shelves along the canyon flanks grounded to gorge walls
      if (dist_to_gorge >= 2.0f && y <= room.floor_y) {
        if ((y == canyon_bed + 2 || y == room.floor_y) && ((std::abs(dz) + 2) % 3 == 0)) {
          out = Voxel{MAT_VOLCANIC_BASALT, 0};
          return;
        }
      }
      // Hanging stalactites from ceiling (grapple anchors)
      if (y >= room.ceiling_y - 4 && dist_to_gorge <= 1.0f &&
          (std::abs(dz) % 4 == 0)) {
        out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
        return;
      }
      if (y > canyon_bed && y < room.ceiling_y - 2) {
        out = Voxel{MAT_AIR, 0};
        return;
      }
    }

    // Carved entrance walkways connecting West/East and North/South corridors
    // into gorge
    bool is_rim_walkway = is_perimeter || (dist_to_gorge > 2.8f && std::abs(dz) <= 2) ||
                          (std::abs(dz) > 3 && dist_to_gorge <= 2.0f);
    if (is_rim_walkway) {
      if (y < room.floor_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
      }
      if (y == room.floor_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
      }
      if (y > room.floor_y && y < room.ceiling_y - 2) {
        out = Voxel{MAT_AIR, 0};
        return;
      }
    }

    // Canyon high rim cliffs in flanking corners
    if (y <= room.floor_y + 2) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }
  } else {
    // Fallback for isolated single-room unit test at floor_y = 4 with grounded arch piers
    bool bridge_south =
        in_gorge && (dz >= -5 && dz <= -4) &&
        ((y == room.floor_y + 3) || (y >= room.floor_y && y <= room.floor_y + 3 && (dist_to_gorge >= 2.0f || dz == -5)));
    bool bridge_center =
        in_gorge && (std::abs(dz) <= 1) &&
        ((y == room.floor_y + 6) || (y >= room.floor_y && y <= room.floor_y + 6 && (dist_to_gorge >= 2.0f || dz == 0)));
    bool bridge_north =
        in_gorge && (dz >= 4 && dz <= 5) &&
        ((y == room.floor_y + 4) || (y >= room.floor_y && y <= room.floor_y + 4 && (dist_to_gorge >= 2.0f || dz == 4)));

    if (bridge_south || bridge_center || bridge_north) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }

    if (in_gorge) {
      if (y <= room.floor_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
      }
      if (y >= room.ceiling_y - 4 && dist_to_gorge <= 1.0f &&
          (std::abs(dz) % 4 == 0)) {
        out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
        return;
      }
      out = Voxel{MAT_AIR, 0};
      return;
    }

    bool is_rim_walkway = (dist_to_gorge > 2.8f && std::abs(dz) <= 2) ||
                          (std::abs(dz) > 3 && dist_to_gorge <= 2.0f);
    if (is_rim_walkway) {
      if (y == room.floor_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
      } else {
        out = Voxel{MAT_AIR, 0};
      }
      return;
    }

    if (y <= room.floor_y + 2) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

// ─────────────────────────────────────────────────────────────
// NEW EXPANDED ROOM ARCHETYPES (OASIS, CHASMS, FOUNDRIES, GROTTOS)
// ─────────────────────────────────────────────────────────────

void LevelGenerator::sample_aquifer_oasis(const RoomPlacement &room, int x,
                                          int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 201) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  bool is_entry = is_doorway_corridor(room, dx, dz, 3);
  if (is_entry && y == room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Floor and water pools
  // Watertight Oasis Aquifer Basin
  float pool_r = std::max(2.5f, static_cast<float>(room.half_width) - 4.0f);
  bool is_island = ((std::abs(dx) <= 1 && std::abs(dz) <= 1) ||
                    (std::abs(dx % 3) == 0 && std::abs(dz % 3) == 0));

  // Inside the aquifer basin hollow
  if (r_horiz <= pool_r) {
    if (y == room.floor_y) {
      if (is_island || is_entry) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
      } else {
        out = Voxel{MAT_CRYSTAL_AQUIFER, 0}; // Liquid crystal water pool
      }
      return;
    }
    if (y == room.floor_y + 1 && is_island && !is_entry) {
      out = Voxel{MAT_VOLCANIC_BASALT,
                  0}; // Stepping stone rising above water surface
      return;
    }
  }

  // Verdant shoreline retaining rim enclosing the aquifer where bounding walls
  // exceed water level
  if (r_horiz > pool_r && r_horiz <= pool_r + 1.2f && !is_entry) {
    if (y == room.floor_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }
    if (y == room.floor_y + 1) {
      out = Voxel{MAT_BIOLUMINESCENT_FLORA,
                  VOXEL_FLAG_EMISSIVE}; // Raised mossy shoreline bank
      return;
    }
  }

  // Floor of surrounding cavern banks
  if (y == room.floor_y) {
    uint32_t florah = hash_coord(x, y, z, 303);
    if ((florah % 10) < 6) {
      out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // Bioluminescent flora moss jutting out around the oasis banks (strictly avoid
  // blocking doorways)
  if (y == room.floor_y + 1 &&
      r_horiz >= static_cast<float>(room.half_width) - 3.0f &&
      r_horiz <= static_cast<float>(room.half_width) - 1.5f) {
    if (!is_doorway_air(room, dx, dz, y) &&
        (hash_coord(x, y, z, 404) % 10) == 0) {
      out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
      return;
    }
  }

  // High domed ceiling with moisture rock stalactites
  if (y >= room.ceiling_y - 2) {
    if (y == room.ceiling_y - 2 &&
        (std::abs(dx % 4) == 0 && std::abs(dz % 4) == 0)) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_colossal_abyssal_chasm(const RoomPlacement &room,
                                                   int x, int y, int z,
                                                   Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 202) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Chasm floor at y == room.floor_y:
  // Deadly spiked abyss pit bed!
  if (y == room.floor_y) {
    // Safe perimeter walkway / doorway entrance ledges around room walls
    if (std::abs(dx) >= room.half_width - 3 ||
        std::abs(dz) >= room.half_depth - 3) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }
    // In the center is a lethal punji spike trench
    if (r_horiz <= static_cast<float>(room.half_width) - 3.0f) {
      out = Voxel{MAT_OBSIDIAN_SPIKES,
                  VOXEL_FLAG_EMISSIVE}; // Deadly punji spike trench
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // High-tension suspension bridge spanning across the chasm at y =
  // room.floor_y + 8, grounded by titanium structural pylons and abutments down to floor
  bool on_bridge_pylon = (std::abs(dz) == 2) && (std::abs(dx) == 6 || std::abs(dx) == 0) &&
                         (y >= room.floor_y && y <= room.floor_y + 8);
  bool on_bridge_abutment = (std::abs(dz) <= 2) && (std::abs(dx) >= room.half_width - 3) &&
                            (y >= room.floor_y && y <= room.floor_y + 8);
  bool on_bridge = (std::abs(dz) <= 2 && y == room.floor_y + 8) || on_bridge_pylon || on_bridge_abutment;
  bool on_railing = (std::abs(dz) == 2 && y == room.floor_y + 9);
  if (on_bridge) {
    out = Voxel{MAT_TITANIUM, 0};
    return;
  }
  if (on_railing) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
    return;
  }

  // Doorway entrance ledges at room.floor_y on the perimeter so corridors
  // connect safely
  bool is_doorway_walkway = (std::abs(dx) >= room.half_width - 2 ||
                             std::abs(dz) >= room.half_depth - 2) &&
                            (y == room.floor_y);
  if (is_doorway_walkway) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Hanging grapple stalactites from ceiling
  if (y >= room.ceiling_y - 4 &&
      (std::abs(dx % 5) == 0 && std::abs(dz % 5) == 0)) {
    out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    return;
  }

  if (y >= room.ceiling_y - 1) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_molten_magma_foundry(const RoomPlacement &room,
                                                 int x, int y, int z,
                                                 Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 203) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  bool is_entry = is_doorway_corridor(room, dx, dz, 3) ||
                  is_doorway_floor(room, dx, dz, y) ||
                  is_doorway_air(room, dx, dz, y);

  // Magma lake & smelting flumes at floor level
  // Watertight central smelting cauldron basin
  float crucible_r = 3.6f;
  if (r_horiz <= crucible_r) {
    if (y == room.floor_y) {
      if (is_entry) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
      } else {
        out = Voxel{MAT_MOLTEN_MAGMA,
                    VOXEL_FLAG_EMISSIVE}; // Contained molten magma pool
      }
      return;
    }
  }

  // Raised basalt crucible rim where bounding walls exceed magma surface
  if (r_horiz > crucible_r && r_horiz <= crucible_r + 1.2f && !is_entry) {
    if (y == room.floor_y || y == room.floor_y + 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Raised bounding wall lip
      return;
    }
  }

  if (y == room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Elevated titanium industrial catwalk at y == room.floor_y + 3 with grounded support columns
  bool catwalk_perimeter =
      (r_horiz >= 4.0f && r_horiz <= 6.0f) && (y == room.floor_y + 3);
  bool catwalk_cross =
      (std::abs(dx) <= 1 || std::abs(dz) <= 1) && (y == room.floor_y + 3);
  bool catwalk_posts = ((std::abs(dx) == 2 && std::abs(dz) == 2) ||
                        (std::abs(dx) == 5 && std::abs(dz) == 0) ||
                        (std::abs(dx) == 0 && std::abs(dz) == 5)) &&
                       (y >= room.floor_y && y <= room.floor_y + 3);
  if (catwalk_perimeter || catwalk_cross || catwalk_posts) {
    out = Voxel{MAT_TITANIUM, 0};
    return;
  }

  // Smelting columns & exhaust chimneys
  if ((std::abs(dx) == 4 && std::abs(dz) == 4) && y < room.ceiling_y - 2) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
    return;
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_toxic_miasma_swamp(const RoomPlacement &room, int x,
                                               int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 204) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Low swamp floor: damp moss & sludge
  if (y == room.floor_y) {
    if ((hash_coord(x, y, z, 505) % 10) < 5) {
      out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // Pockets of heavy toxic gas lingering in the lower 2 meters (y = floor_y + 1
  // and floor_y + 2) Sinuous pockets visible across the room!
  if ((y == room.floor_y + 1 || y == room.floor_y + 2) &&
      r_horiz <= static_cast<float>(room.half_width) - 3.0f) {
    // Natural meandering gas pockets
    float gas_wave = std::sin(static_cast<float>(dx) * 0.5f) *
                     std::cos(static_cast<float>(dz) * 0.5f);
    if (gas_wave > -0.2f && (std::abs(dx) > 1 || std::abs(dz) > 1)) {
      out = Voxel{MAT_TOXIC_GAS, VOXEL_FLAG_EMISSIVE};
      return;
    }
  }

  // Elevated winding root bridge at y = room.floor_y + 3 with grounded root trunks down to floor
  float root_path = std::sin(static_cast<float>(dx) * 0.4f) * 2.2f;
  bool on_root_pillar = (std::abs(static_cast<float>(dz) - root_path) <= 0.8f) &&
                        (std::abs(dx) % 4 == 0) && (y >= room.floor_y && y <= room.floor_y + 3);
  bool on_root_abutment = (std::abs(static_cast<float>(dz) - root_path) <= 1.2f) &&
                          (std::abs(dx) >= room.half_width - 2) && (y >= room.floor_y && y <= room.floor_y + 3);
  bool on_root = ((std::abs(static_cast<float>(dz) - root_path) <= 1.2f) && (y == room.floor_y + 3)) ||
                 on_root_pillar || on_root_abutment;
  if (on_root) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  // Overhanging fungal spore arches from ceiling (2D hash to hang continuously)
  if (y >= room.ceiling_y - 3 &&
      (std::abs(dx % 4) == 0 || std::abs(dz % 4) == 0)) {
    if ((hash_coord(x, 0, z, 606) % 10) < 4) {
      out = Voxel{MAT_VOLATILE_SMOKE, VOXEL_FLAG_EMISSIVE};
      return;
    }
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_prismatic_crystal_cathedral(
    const RoomPlacement &room, int x, int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 205) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Floor at y == room.floor_y: carved granite floor
  if (y == room.floor_y) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  // Stepped central dais stairs (natural carved granite)
  if (y == room.floor_y + 1 && r_horiz <= 2.5f) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }
  if (y == room.floor_y + 2 && r_horiz <= 1.2f) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  // 4 Colossal Hexagonal Stone Columns extending floor to ceiling with naturally integrated mineral veins
  int col_offset_x = std::max(2, room.half_width / 2);
  int col_offset_z = std::max(2, room.half_depth / 2);
  bool is_col1 =
      (std::abs(dx - col_offset_x) <= 1 && std::abs(dz - col_offset_z) <= 1);
  bool is_col2 =
      (std::abs(dx + col_offset_x) <= 1 && std::abs(dz - col_offset_z) <= 1);
  bool is_col3 =
      (std::abs(dx - col_offset_x) <= 1 && std::abs(dz + col_offset_z) <= 1);
  bool is_col4 =
      (std::abs(dx + col_offset_x) <= 1 && std::abs(dz + col_offset_z) <= 1);

  if (is_col1 || is_col2 || is_col3 || is_col4) {
    if (y <= room.ceiling_y - 2) {
      // Natural crystal seams integrated into upper column fluting and recesses
      if (y >= room.floor_y + 7 && y <= room.ceiling_y - 4 &&
          (hash_coord(x, y, z, 73) % 4 == 0)) {
        out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
      } else {
        out = Voxel{MAT_FRACTURED_GRANITE, 0};
      }
      return;
    }
  }

  // Elevated stone arch bridges linking the pillars at y = room.floor_y + 7
  bool bridge_x =
      (y == room.floor_y + 7 &&
       (std::abs(dz - col_offset_z) <= 1 || std::abs(dz + col_offset_z) <= 1) &&
       std::abs(dx) <= col_offset_x);
  bool bridge_z =
      (y == room.floor_y + 7 &&
       (std::abs(dx - col_offset_x) <= 1 || std::abs(dx + col_offset_x) <= 1) &&
       std::abs(dz) <= col_offset_z);
  if (bridge_x || bridge_z) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  // Vaulted cathedral ceiling with ribbed stone arches and subtle crystal accents
  if (y >= room.ceiling_y - 2) {
    if ((std::abs(dx) % 4 == 0) && (std::abs(dz) % 4 == 0)) {
      out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
    } else {
      out = Voxel{MAT_FRACTURED_GRANITE, 0};
    }
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_ancient_titan_necropolis(const RoomPlacement &room,
                                                     int x, int y, int z,
                                                     Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 206) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Sunken excavation pit floor at y = room.floor_y
  if (y == room.floor_y) {
    if (std::abs(dx) <= 2 && std::abs(dz) <= room.half_depth - 3) {
      out = Voxel{MAT_TITANIUM, 0}; // Excavated mineral seam
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // Giant Skeletal Ribcage Arches spanning across Z axis: 5 massive bone arches
  bool on_rib_ring =
      (std::abs(dz) % 3 == 0) && (std::abs(dz) <= room.half_depth - 2);
  if (on_rib_ring && y > room.floor_y && y <= room.ceiling_y - 3) {
    float max_arch_h = static_cast<float>(room.ceiling_y - room.floor_y - 4);
    float arch_span = static_cast<float>(std::max(3, room.half_width - 2));
    float norm_x = std::abs(static_cast<float>(dx)) / arch_span;
    if (norm_x <= 1.0f) {
      float expected_y =
          static_cast<float>(room.floor_y + 1) +
          max_arch_h * std::sqrt(std::max(0.0f, 1.0f - norm_x * norm_x));
      if (std::abs(static_cast<float>(y) - expected_y) <= 1.0f) {
        out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED}; // Titan bone matrix
        return;
      }
    }
  }

  // Ancient excavation scaffolding along east/west walls at y = room.floor_y + 4
  // Supported by vertical scaffold posts down to the floor
  bool is_scaffolding_post = (std::abs(dx) == room.half_width - 2) && (std::abs(dz) % 4 == 0) &&
                             (y >= room.floor_y && y <= room.floor_y + 4);
  if (((std::abs(dx) == room.half_width - 2) && (y == room.floor_y + 4)) || is_scaffolding_post) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
    return;
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_bioluminescent_glowworm_grotto(
    const RoomPlacement &room, int x, int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 207) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  bool is_entry = is_doorway_corridor(room, dx, dz, 3);
  if (is_entry && y == room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Subterranean reflecting pool at floor level
  // Watertight subterranean reflecting pool basin
  float pool_r = std::max(2.5f, static_cast<float>(room.half_width) - 4.2f);
  if (r_horiz <= pool_r) {
    if (y == room.floor_y) {
      if (is_entry) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
      } else {
        out = Voxel{MAT_CRYSTAL_AQUIFER, 0}; // Contained reflecting water pool
      }
      return;
    }
  }

  // Raised mossy shoreline rim where bounding walls exceed water surface height
  if (r_horiz > pool_r && r_horiz <= pool_r + 1.2f && !is_entry) {
    if (y == room.floor_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return;
    }
    if (y == room.floor_y + 1) {
      out = Voxel{MAT_BIOLUMINESCENT_FLORA,
                  VOXEL_FLAG_EMISSIVE}; // Raised shoreline lip
      return;
    }
  }

  if (y == room.floor_y) {
    uint32_t florah = hash_coord(x, y, z, 707);
    if ((florah % 10) < 5) {
      out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // Starry night glowworm canopy ceiling
  if (y == room.ceiling_y - 2) {
    uint32_t star_hash = hash_coord(x, y, z, 888);
    if ((star_hash % 10) < 5) {
      // Densely clustered bioluminescent glow points
      out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
      return;
    }
  }

  if (y >= room.ceiling_y - 1) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_precursor_coolant_reservoir(
    const RoomPlacement &room, int x, int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 208) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_TITANIUM, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Floor and twin coolant reservoirs
  bool is_entry = is_doorway_corridor(room, dx, dz, 3);
  if (is_entry && y == room.floor_y) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Watertight twin coolant reservoir basins
  float d_pool1 = std::sqrt(static_cast<float>((dx - 4) * (dx - 4) + dz * dz));
  float d_pool2 = std::sqrt(static_cast<float>((dx + 4) * (dx + 4) + dz * dz));

  if ((d_pool1 <= 2.2f || d_pool2 <= 2.2f) && !is_entry) {
    if (y == room.floor_y) {
      out = Voxel{MAT_CRYSTAL_AQUIFER,
                  0}; // Purified liquid coolant in sealed basin
      return;
    }
  }

  // Raised basalt coaming where bounding walls exceed coolant height
  bool is_curb = ((d_pool1 > 2.2f && d_pool1 <= 3.2f) ||
                  (d_pool2 > 2.2f && d_pool2 <= 3.2f)) &&
                 !is_entry;
  if (is_curb) {
    if (y == room.floor_y) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (y == room.floor_y + 1) {
      out = Voxel{MAT_VOLCANIC_BASALT,
                  0}; // Raised safety coaming exceeding liquid surface
      return;
    }
  }

  if (y == room.floor_y) {
    out = Voxel{MAT_TITANIUM, 0}; // Reinforced steel plating
    return;
  }

  // Overhead high-pressure coolant pipelines running along Z axis over
  // reservoir pools at dx = +/- 4, y = room.floor_y + 5, supported by vertical stanchions
  bool pipeline_stanchion = (std::abs(dx - 4) == 0 || std::abs(dx + 4) == 0) &&
                            (std::abs(dz) % 4 == 0) && (y >= room.floor_y && y <= room.floor_y + 5);
  if (((std::abs(dx - 4) <= 1 || std::abs(dx + 4) <= 1) && y == room.floor_y + 5) || pipeline_stanchion) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Control consoles and reinforced catwalks
  if (std::abs(dz) >= room.half_depth - 2 &&
      (y == room.floor_y + 1 || y == room.floor_y + 2)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_vaulted_dredge_cathedral(const RoomPlacement &room,
                                                     int x, int y, int z,
                                                     Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 301) - 0.5f) * 0.30f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Floor at room.floor_y:
  if (y == room.floor_y) {
    // Sunken nave center aisle (dz == 0 or +/- 1)
    if (std::abs(dz) <= 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    } else if (std::abs(dz) <= 4) {
      out = Voxel{MAT_TITANIUM, 0}; // Titanium aisle trim
    } else {
      out = Voxel{MAT_FRACTURED_GRANITE, 0};
    }
    return;
  }

  // Elevated altar dais at positive X end
  if (dx >= room.half_width - 3 && std::abs(dz) <= 3 && y == room.floor_y + 1) {
    if (dx == room.half_width - 2 && dz == 0) {
      out = Voxel{MAT_PRECURSOR_STONE,
                  0}; // Ancient ceremonial precursor monolith
    } else {
      out = Voxel{MAT_TITANIUM, 0}; // Stepped dais
    }
    return;
  }

  // High observation gantry / catwalk at y = room.floor_y + 15 (y ~ 19)
  // spanning East-West, grounded by structural titanium pylons and abutments down to floor
  bool is_catwalk_pylon = (std::abs(dz) == 2) && (std::abs(dx) % 6 == 0) &&
                          (y >= room.floor_y && y <= room.floor_y + 15);
  bool is_catwalk_abutment = (std::abs(dz) <= 2) && (std::abs(dx) >= room.half_width - 3) &&
                             (y >= room.floor_y && y <= room.floor_y + 15);
  bool is_catwalk = (y == room.floor_y + 15 && std::abs(dz) <= 2) || is_catwalk_pylon || is_catwalk_abutment;
  if (is_catwalk) {
    out = Voxel{MAT_TITANIUM, 0}; // Catwalk bridge deck & pylons
    return;
  }
  if (y == room.floor_y + 16 && (std::abs(dz) == 2)) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0}; // Catwalk safety railings
    return;
  }

  // Flanking buttress columns along north/south walls (dz = +/- (half_depth -
  // 2)) every 4 blocks along X
  if (std::abs(dz) >= room.half_depth - 3 && (std::abs(dx) % 4 == 0) &&
      y < room.ceiling_y - 1) {
    if (y % 4 == 0) {
      out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    }
    return;
  }

  // Ribbed cathedral vault arches overhead at y >= room.ceiling_y - 2 (every 4
  // blocks in X)
  if (y >= room.ceiling_y - 2 && (std::abs(dx) % 4 == 0)) {
    out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Grand stalactites hanging from the vault roof (2D hash to hang continuously)
  int stal_len = (hash_coord(x, 0, z, 311) % 13 == 0 && r_horiz >= 3.0f) ? 3 : 0;
  if (stal_len > 0 && y >= room.ceiling_y - stal_len) {
    out = Voxel{MAT_FRACTURED_GRANITE, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Vault ceiling mantle
  if (y >= room.ceiling_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_tectonic_sinkhole(const RoomPlacement &room, int x,
                                              int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 302) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Deep abyss bed at y == room.floor_y:
  // Watertight glowing molten fissure basin in central trough
  if (std::abs(dz) <= 1 && r_horiz <= 3.2f) {
    if (y == room.floor_y) {
      out = Voxel{MAT_MOLTEN_MAGMA,
                  VOXEL_FLAG_EMISSIVE}; // Sealed molten magma fissure
      return;
    }
  }

  // Raised basalt fissure rim where bounding walls exceed magma height
  if (r_horiz <= 4.2f &&
      (std::abs(dz) == 2 || (std::abs(dz) <= 1 && r_horiz > 3.2f))) {
    if (y == room.floor_y || y == room.floor_y + 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Raised bounding wall rim
      return;
    }
  }

  if (y == room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Stepped spiral terraces descending from rim grounded to abyss bed:
  // Outer terrace at y = room.floor_y + 14 (rim shelf) grounded to bounding walls
  bool is_outer_shelf =
      (r_horiz >= static_cast<float>(room.half_width) - 2.8f) &&
      (y >= room.floor_y + 10 && y <= room.floor_y + 14);
  if (is_outer_shelf) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  // Mid terrace at y = room.floor_y + 8 with solid rock buttress down to floor
  bool is_mid_shelf = (r_horiz >= 5.5f && r_horiz <= 7.5f) &&
                      (angle > -1.0f && angle < 2.0f) &&
                      (y >= room.floor_y && y <= room.floor_y + 8);
  if (is_mid_shelf) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Central stepping spire monolith rising from abyss bed to y = room.floor_y +
  // 10
  if (r_horiz <= 2.2f && y <= room.floor_y + 10) {
    if (y == room.floor_y + 10) {
      out = Voxel{MAT_TITANIUM, 0}; // Monolith lookout cap
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    }
    return;
  }

  // High tension cable bridge crossing the sinkhole at y = room.floor_y + 14
  // along dz = 0, supported by central monolith tower and rim abutments
  bool is_bridge_tower = (r_horiz <= 1.5f && y >= room.floor_y + 10 && y <= room.floor_y + 14) ||
                         (r_horiz >= static_cast<float>(room.half_width) - 3.0f && std::abs(dz) <= 2 &&
                          y >= room.floor_y && y <= room.floor_y + 14);
  if ((std::abs(dz) <= 2 && y == room.floor_y + 14) || is_bridge_tower) {
    out = Voxel{MAT_TITANIUM, 0};
    return;
  }
  if ((std::abs(dz) == 2) && y == room.floor_y + 15) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0}; // Guardrail
    return;
  }

  // Hanging stalactites from ceiling (2D hash to hang continuously)
  int stal_len = (hash_coord(x, 0, z, 321) % 11 == 0 && r_horiz >= 2.5f) ? 3 : 0;
  if (stal_len > 0 && y >= room.ceiling_y - stal_len) {
    out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    return;
  }

  if (y >= room.ceiling_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_cyclopean_silo(const RoomPlacement &room, int x,
                                           int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 303) - 0.5f) * 0.20f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_INDUSTRIAL_BULKHEAD, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_TITANIUM, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  // Floor at y == room.floor_y: Heavy titanium dredge excavation basin
  if (y == room.floor_y) {
    if (r_horiz <= 3.2f) {
      out =
          Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED}; // Excavation hopper center
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    }
    return;
  }

  // Mid-level maintenance ring catwalk at y = room.floor_y + 8 (y ~ 12)
  float r_ring_min = std::max(4.0f, static_cast<float>(room.half_width) - 3.5f);
  float r_ring_max = static_cast<float>(room.half_width) - 0.5f;
  if (y == room.floor_y + 8 && r_horiz >= r_ring_min && r_horiz <= r_ring_max) {
    out = Voxel{MAT_TITANIUM, 0};
    return;
  }
  if (y == room.floor_y + 9 && (std::abs(r_horiz - r_ring_min) <= 0.6f)) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0}; // Mid-level inner safety railing
    return;
  }

  // High-level observation ring catwalk at y = room.floor_y + 16 (y ~ 20)
  if (y == room.floor_y + 16 && r_horiz >= r_ring_min &&
      r_horiz <= r_ring_max) {
    out = Voxel{MAT_TITANIUM, 0};
    return;
  }
  if (y == room.floor_y + 17 && (std::abs(r_horiz - r_ring_min) <= 0.6f)) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0}; // High-level inner safety railing
    return;
  }

  // Vertical structural guide columns & conduit pipes along cardinal perimeter
  // walls
  if ((std::abs(dx) <= 1 || std::abs(dz) <= 1) &&
      r_horiz >= static_cast<float>(room.half_width) - 2.0f &&
      y < room.ceiling_y) {
    if (y % 4 == 2) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    } else {
      out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
    }
    return;
  }

  // Overhead heavy crane girder at y = room.ceiling_y - 2 spanning X axis
  // across diameter
  if (y == room.ceiling_y - 2 && std::abs(dz) <= 1) {
    if (dx == 0 && dz == 0) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_EMISSIVE}; // Crane hoist mount
    } else {
      out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
    }
    return;
  }

  if (y >= room.ceiling_y) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_firmament_abyss(const RoomPlacement &room, int x,
                                            int y, int z, Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
  float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

  float r_wall = compute_room_wall_radius(
      room, static_cast<float>(dx), static_cast<float>(dz), angle,
      (pseudo_rand(x, y, z, 304) - 0.5f) * 0.35f);
  if (r_horiz > r_wall) {
    check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
    return;
  }

  if (is_doorway_floor(room, dx, dz, y)) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }
  if (is_doorway_air(room, dx, dz, y)) {
    out = Voxel{MAT_AIR, 0};
    return;
  }

  bool is_entry = is_doorway_corridor(room, dx, dz, 3);
  if (is_entry && y == room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return;
  }

  // Undulating natural floor with crystal aquifer reflecting pools at y ==
  // room.floor_y Watertight crystal aquifer reflecting pool basin
  float d_pool =
      std::sqrt(static_cast<float>((dx - 2) * (dx - 2) + (dz + 2) * (dz + 2)));
  if (d_pool <= 2.8f && !is_entry) {
    if (y == room.floor_y) {
      out = Voxel{MAT_CRYSTAL_AQUIFER,
                  0}; // Glowing crystal water pool in sealed basin
      return;
    }
  }

  // Raised basalt rim where bounding walls exceed water height
  if (d_pool > 2.8f && d_pool <= 3.8f && !is_entry) {
    if (y == room.floor_y || y == room.floor_y + 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Raised basalt bounding rim
      return;
    }
  }

  if (y == room.floor_y) {
    if (r_horiz <= 4.0f) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
    } else {
      out = Voxel{MAT_FRACTURED_GRANITE, 0};
    }
    return;
  }

  // High natural stone arch bridge soaring across the cavern at y =
  // room.floor_y + 15 (y ~ 19) Spans diagonally from SW to NE (dx ~ dz)
  // Supported by massive stone piers and wall abutments down to floor
  bool arch_abutment = (std::abs(dx - dz) <= 1) &&
                       (std::abs(dx) >= room.half_width - 3 || std::abs(dz) >= room.half_depth - 3) &&
                       (y >= room.floor_y && y <= room.floor_y + 15);
  bool arch_piers = (std::abs(dx - dz) <= 1) && (std::abs(dx) % 5 == 0) &&
                    (y >= room.floor_y && y <= room.floor_y + 15);
  bool on_stone_arch = ((std::abs(dx - dz) <= 1) && (y == room.floor_y + 15)) ||
                       arch_abutment || arch_piers;
  if (on_stone_arch) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  // Starry Celestial Firmament embedded in the high vault ceiling at y >=
  // room.ceiling_y - 2
  if (y >= room.ceiling_y - 2) {
    uint32_t star_hash = hash_coord(x, y, z, 555);
    if (y == room.ceiling_y - 2 && (star_hash % 9 == 0)) {
      // Emissive star crystal embedded in dark rock ceiling
      out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
      return;
    }
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  if (y >= room.ceiling_y) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

// ─────────────────────────────────────────────────────────────
// PRECURSOR VAULT PROCEDURAL PLACEMENT & OBSTACLES
// ─────────────────────────────────────────────────────────────

void LevelGenerator::configure_vault_placement(std::mt19937 &rng) {
  m_vault_placement = VaultPlacement{};
  if (m_rooms.empty()) return;

  glm::vec3 spawn_pos = spawn_position();

  auto get_door_and_chamber = [&](const RoomPlacement &r, int dx, int dz,
                                  glm::ivec3 &door, glm::ivec3 &chamber) -> bool {
    int door_x = r.center.x + (dx != 0 ? dx * (r.half_width - 1) : 0);
    int door_z = r.center.z + (dz != 0 ? dz * (r.half_depth - 1) : 0);
    if (r.connected_north && r.connected_south && r.connected_east && r.connected_west) {
      if (dz != 0) {
        door_x = (door_x + 4 <= world_width() - 8) ? (door_x + 4) : (door_x - 4);
      } else if (dx != 0) {
        door_z = (door_z + 4 <= world_depth() - 8) ? (door_z + 4) : (door_z - 4);
      }
    }
    int door_y = r.floor_y + (r.type == RoomShapeType::PrecursorSanctumVault ? 1 : 0);

    // Chamber center extends 4 blocks behind door in direction (dx, dz)
    int chamber_cx = door_x + dx * 4;
    int chamber_cz = door_z + dz * 4;

    // Chamber extends +-3 blocks from center. Must stay strictly within bedrock borders:
    // [4, world_width() - 5] and [4, world_depth() - 5]
    if (chamber_cx - 3 < 4 || chamber_cx + 3 > world_width() - 5) return false;
    if (chamber_cz - 3 < 4 || chamber_cz + 3 > world_depth() - 5) return false;

    // Door must also be within world interior
    if (door_x < 4 || door_x > world_width() - 5) return false;
    if (door_z < 4 || door_z > world_depth() - 5) return false;

    door = glm::ivec3(door_x, door_y, door_z);
    chamber = glm::ivec3(chamber_cx, door_y, chamber_cz);
    return true;
  };

  const int test_dirs[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};

  auto has_unconnected_clear_wall = [&](const RoomPlacement &r) -> bool {
    const bool conn[4] = {r.connected_north, r.connected_south, r.connected_east, r.connected_west};
    glm::ivec3 d, c;
    for (int k = 0; k < 4; ++k) {
      if (!conn[k] && get_door_and_chamber(r, test_dirs[k][0], test_dirs[k][1], d, c)) {
        return true;
      }
    }
    return false;
  };

  auto has_any_clear_wall = [&](const RoomPlacement &r) -> bool {
    glm::ivec3 d, c;
    for (int k = 0; k < 4; ++k) {
      if (get_door_and_chamber(r, test_dirs[k][0], test_dirs[k][1], d, c)) {
        return true;
      }
    }
    return false;
  };

  // Precursor Vaults and Boss Monsters must never generate so close to player spawn!
  // Enforce minimum spatial separation from spawn room (0,0) and its immediate neighbors.
  auto is_safe_from_spawn = [&](const RoomPlacement &r) -> bool {
    // Never place in spawn room (0,0)
    if (r.grid_x == 0 && r.grid_z == 0) return false;
    // Avoid extraction landing bay if possible
    if (r.grid_x == m_grid_w - 1 && r.grid_z == m_grid_d - 1) return false;
    // Reject spawn-adjacent rooms (Manhattan distance <= 1)
    if ((std::abs(r.grid_x - 0) + std::abs(r.grid_z - 0)) <= 1) return false;
    // Reject rooms closer than 26m from spawn center
    float d = glm::distance(glm::vec2(r.center.x, r.center.z), glm::vec2(spawn_pos.x, spawn_pos.z));
    return d >= 26.0f;
  };

  // 1. Identify best host room
  int best_room_idx = -1;
  int best_score = -1;

  // First priority: Dedicated vault room types with an unconnected outer wall AND safe from spawn.
  // Prioritize the deepest room furthest from spawn!
  for (size_t i = 1; i < m_rooms.size() - 1; ++i) {
    if (!is_safe_from_spawn(m_rooms[i])) continue;
    RoomShapeType t = m_rooms[i].type;
    if (is_vault_room_type(t) && has_unconnected_clear_wall(m_rooms[i])) {
      int score = (m_rooms[i].grid_x + m_rooms[i].grid_z) * 100 +
                  static_cast<int>(glm::distance(glm::vec2(m_rooms[i].center.x, m_rooms[i].center.z),
                                                glm::vec2(spawn_pos.x, spawn_pos.z)));
      if (score > best_score) {
        best_score = score;
        best_room_idx = static_cast<int>(i);
      }
    }
  }

  // Second priority: Deepest room away from spawn with an unconnected outer wall and safe from spawn
  if (best_room_idx < 0) {
    best_score = -1;
    for (size_t i = 1; i < m_rooms.size() - 1; ++i) {
      if (!is_safe_from_spawn(m_rooms[i])) continue;
      if (!has_unconnected_clear_wall(m_rooms[i])) continue;
      int score = (m_rooms[i].grid_x + m_rooms[i].grid_z) * 100 +
                  static_cast<int>(glm::distance(glm::vec2(m_rooms[i].center.x, m_rooms[i].center.z),
                                                glm::vec2(spawn_pos.x, spawn_pos.z)));
      if (score > best_score) {
        best_score = score;
        best_room_idx = static_cast<int>(i);
      }
    }
  }

  // Third priority: Any dedicated vault room safe from spawn with any clear wall
  if (best_room_idx < 0) {
    best_score = -1;
    for (size_t i = 1; i < m_rooms.size() - 1; ++i) {
      if (!is_safe_from_spawn(m_rooms[i])) continue;
      RoomShapeType t = m_rooms[i].type;
      if (is_vault_room_type(t) && has_any_clear_wall(m_rooms[i])) {
        int score = (m_rooms[i].grid_x + m_rooms[i].grid_z) * 100 +
                    static_cast<int>(glm::distance(glm::vec2(m_rooms[i].center.x, m_rooms[i].center.z),
                                                  glm::vec2(spawn_pos.x, spawn_pos.z)));
        if (score > best_score) {
          best_score = score;
          best_room_idx = static_cast<int>(i);
        }
      }
    }
  }

  // Fourth priority: Any room safe from spawn with any clear wall
  if (best_room_idx < 0) {
    best_score = -1;
    for (size_t i = 1; i < m_rooms.size() - 1; ++i) {
      if (!is_safe_from_spawn(m_rooms[i])) continue;
      if (!has_any_clear_wall(m_rooms[i])) continue;
      int score = (m_rooms[i].grid_x + m_rooms[i].grid_z) * 100 +
                  static_cast<int>(glm::distance(glm::vec2(m_rooms[i].center.x, m_rooms[i].center.z),
                                                glm::vec2(spawn_pos.x, spawn_pos.z)));
      if (score > best_score) {
        best_score = score;
        best_room_idx = static_cast<int>(i);
      }
    }
  }

  // Fallback for tiny layouts or test harnesses with <= 2 rooms
  if (best_room_idx < 0) {
    for (size_t i = 1; i < m_rooms.size(); ++i) {
      if (has_any_clear_wall(m_rooms[i])) {
        best_room_idx = static_cast<int>(i);
        break;
      }
    }
  }
  if (best_room_idx < 0 && m_rooms.size() > 1) {
    best_room_idx = 1;
  }
  if (best_room_idx < 0 && !m_rooms.empty()) {
    best_room_idx = 0;
  }

  if (best_room_idx < 0 || best_room_idx >= static_cast<int>(m_rooms.size())) {
    return;
  }

  RoomPlacement &host = m_rooms[best_room_idx];
  host.has_vault = true;

  VaultPlacement vp;
  vp.exists = true;
  vp.room_index = best_room_idx;
  vp.room_type = host.type;
  vp.door_height = 3;

  // 2. Select wall orientation: prefer unconnected wall with clearance, else any wall with clearance
  int dir_x = 0;
  int dir_z = 0;
  glm::ivec3 chosen_door(0);
  glm::ivec3 chosen_chamber(0);
  bool placed = false;

  const bool host_conn[4] = {host.connected_north, host.connected_south, host.connected_east, host.connected_west};
  for (int k = 0; k < 4; ++k) {
    if (!host_conn[k] && get_door_and_chamber(host, test_dirs[k][0], test_dirs[k][1], chosen_door, chosen_chamber)) {
      dir_x = test_dirs[k][0];
      dir_z = test_dirs[k][1];
      placed = true;
      break;
    }
  }

  if (!placed) {
    for (int k = 0; k < 4; ++k) {
      if (get_door_and_chamber(host, test_dirs[k][0], test_dirs[k][1], chosen_door, chosen_chamber)) {
        dir_x = test_dirs[k][0];
        dir_z = test_dirs[k][1];
        placed = true;
        break;
      }
    }
  }

  if (!placed) {
    // Ultimate boundary fallback for very tight rooms
    dir_x = 0;
    dir_z = (host.center.z + host.half_depth + 7 <= world_depth() - 5) ? 1 : -1;
    int door_x = host.center.x;
    int door_z = host.center.z + dir_z * (host.half_depth - 1);
    door_x = std::clamp(door_x, 5, world_width() - 6);
    door_z = std::clamp(door_z, 5, world_depth() - 6);
    int door_y = host.floor_y + (host.type == RoomShapeType::PrecursorSanctumVault ? 1 : 0);
    int chamber_cx = door_x + dir_x * 4;
    int chamber_cz = door_z + dir_z * 4;
    chosen_door = glm::ivec3(door_x, door_y, door_z);
    chosen_chamber = glm::ivec3(chamber_cx, door_y, chamber_cz);
  }

  int door_y = chosen_door.y;
  vp.door_pos = chosen_door;
  vp.approach_pos = glm::ivec3(chosen_door.x - dir_x * 2, door_y, chosen_door.z - dir_z * 2);

  vp.chamber_center = chosen_chamber;
  vp.chamber_min = glm::ivec3(chosen_chamber.x - 3, host.floor_y, chosen_chamber.z - 3);
  vp.chamber_max = glm::ivec3(chosen_chamber.x + 3, host.floor_y + 4, chosen_chamber.z + 3);
  vp.relic_pos = glm::ivec3(chosen_chamber.x, door_y + 1, chosen_chamber.z);

  // 3. Level-specific Obstacles and Guardians
  if (m_sector_index <= 1) {
    vp.obstacle_type = VaultObstacleType::AquiferSteppingStones;
    vp.has_guardian = true;
    vp.is_boss_guardian = false;
    vp.guardian_role = VaultGuardianRole::StalkerMelee;
    vp.guardian_spawn_pos = glm::vec3(vp.approach_pos.x + (dir_z != 0 ? 3.0f : 0.0f),
                                      static_cast<float>(door_y),
                                      vp.approach_pos.z + (dir_x != 0 ? 3.0f : 0.0f));
  } else if (m_sector_index == 2) {
    vp.obstacle_type = VaultObstacleType::HazardTrenchSlag;
    vp.has_guardian = true;
    vp.is_boss_guardian = false;
    vp.guardian_role = (rng() % 2 == 0) ? VaultGuardianRole::StalkerMelee : VaultGuardianRole::StalkerShooter;
    vp.guardian_spawn_pos = glm::vec3(vp.approach_pos.x - (dir_z != 0 ? 3.0f : 0.0f),
                                      static_cast<float>(door_y),
                                      vp.approach_pos.z - (dir_x != 0 ? 3.0f : 0.0f));
  } else {
    vp.obstacle_type = VaultObstacleType::ChasmAbyssRadiation;
    vp.has_guardian = true;
    vp.is_boss_guardian = true; // Boss Monster: Chitin Goliath
    vp.guardian_role = VaultGuardianRole::ChitinGoliath;
    vp.guardian_spawn_pos = glm::vec3(vp.approach_pos.x, static_cast<float>(door_y), vp.approach_pos.z);
  }

  // Strict Boss Monster and Vault Guardian Spawn Separation Invariant:
  // Boss monsters must NEVER be close to spawn (>= MIN_BOSS_SPAWN_DISTANCE = 35.0m),
  // and vault sentry guardians must NEVER be within MIN_VAULT_SPAWN_SEPARATION (30.0m).
  if (vp.has_guardian && m_rooms.size() > 2) {
    float req_separation = vp.is_boss_guardian ? MIN_BOSS_SPAWN_DISTANCE : MIN_VAULT_SPAWN_SEPARATION;
    float current_dist = glm::distance(vp.guardian_spawn_pos, spawn_pos);
    if (current_dist < req_separation) {
      glm::vec3 offset_dir = vp.guardian_spawn_pos - spawn_pos;
      offset_dir.y = 0.0f;
      if (glm::length(offset_dir) < 0.001f) {
        offset_dir = glm::vec3(0.0f, 0.0f, 1.0f);
      }
      offset_dir = glm::normalize(offset_dir);
      vp.guardian_spawn_pos = spawn_pos + offset_dir * (req_separation + 2.0f);
      vp.guardian_spawn_pos.y = static_cast<float>(door_y);
    }
  }

  m_vault_placement = vp;
  host.vault = vp;
}

bool LevelGenerator::sample_vault_structure(int x, int y, int z, Voxel &out) const {
  if (!m_vault_placement.exists) return false;

  const auto &vp = m_vault_placement;

  // 1. Door portal check (3 blocks tall)
  if (x == vp.door_pos.x && z == vp.door_pos.z &&
      y >= vp.door_pos.y && y < vp.door_pos.y + vp.door_height) {
    out = Voxel{MAT_REINFORCED_VAULT_DOOR, VOXEL_FLAG_ANCHORED};
    return true;
  }

  // 2. Door portal archway lintel and pillars
  bool is_door_column = (y >= vp.door_pos.y && y <= vp.door_pos.y + vp.door_height) &&
      (((vp.door_pos.x != vp.chamber_center.x) && (x == vp.door_pos.x) && (z == vp.door_pos.z - 1 || z == vp.door_pos.z + 1)) ||
       ((vp.door_pos.z != vp.chamber_center.z) && (z == vp.door_pos.z) && (x == vp.door_pos.x - 1 || x == vp.door_pos.x + 1)));
  bool is_door_lintel = (y == vp.door_pos.y + vp.door_height) && (x == vp.door_pos.x && z == vp.door_pos.z);
  if (is_door_column || is_door_lintel) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return true;
  }

  // 3. Inner vault chamber bounds check
  if (x >= vp.chamber_min.x && x <= vp.chamber_max.x &&
      y >= vp.chamber_min.y && y <= vp.chamber_max.y &&
      z >= vp.chamber_min.z && z <= vp.chamber_max.z) {

    // Outer chamber shell (walls, floor, ceiling)
    bool is_boundary = (x == vp.chamber_min.x || x == vp.chamber_max.x ||
                        y == vp.chamber_min.y || y == vp.chamber_max.y ||
                        z == vp.chamber_min.z || z == vp.chamber_max.z);
    if (is_boundary) {
      if (y == vp.chamber_min.y) {
        out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      } else {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
      }
      return true;
    }

    // Pedestal underneath the relic hyper-core
    if (x == vp.relic_pos.x && z == vp.relic_pos.z && y == vp.relic_pos.y - 1) {
      out = Voxel{MAT_PRECURSOR_STONE, VOXEL_FLAG_ANCHORED};
      return true;
    }
    // Relic Hyper-Core (rendered as dynamic animated 3D precursor relic)
    if (x == vp.relic_pos.x && z == vp.relic_pos.z && y == vp.relic_pos.y) {
      out = Voxel{MAT_PRISMATIC_CRYSTAL, static_cast<uint8_t>(VOXEL_FLAG_EMISSIVE | VOXEL_FLAG_CUSTOM_MODEL)};
      return true;
    }
    // Flanking treasure caches embedded in inner walls
    if ((x == vp.chamber_min.x + 1 || x == vp.chamber_max.x - 1) &&
        (z == vp.chamber_center.z) && (y == vp.relic_pos.y)) {
      out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
      return true;
    }

    out = Voxel{MAT_AIR, 0};
    return true;
  }

  return false;
}

void LevelGenerator::sample_precursor_sanctum_vault(const RoomPlacement &room,
                                                    int x, int y, int z,
                                                    Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  int abs_x = std::abs(dx);
  int abs_z = std::abs(dz);
  int manhattan = abs_x + abs_z;

  // Vaulted octagonal sanctuary perimeter with chamfered corners
  bool is_outer_wall = (abs_x > room.half_width || abs_z > room.half_depth ||
                        manhattan >= (room.half_width + room.half_depth - 2));
  if (is_outer_wall) {
    if (is_doorway_floor(room, dx, dz, y)) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (is_doorway_air(room, dx, dz, y)) {
      out = Voxel{MAT_AIR, 0};
      return;
    }
    bool is_portal_frame = (abs_x == room.half_width && abs_z <= 3) ||
                           (abs_z == room.half_depth && abs_x <= 3);
    if (is_portal_frame && y <= room.floor_y + 5) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    } else {
      out = Voxel{MAT_PRECURSOR_STONE, VOXEL_FLAG_ANCHORED};
    }
    return;
  }

  // Precursor Columns with Prismatic Crystal Capitals
  bool is_column = ((abs_x == 4 && (dz == -3 || dz == 3)) ||
                    (abs_x == 3 && dz == 0)) && (y >= room.floor_y && y <= room.ceiling_y - 2);
  if (is_column) {
    if (y == room.ceiling_y - 2 || y == room.floor_y + 1) {
      out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
    } else {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    }
    return;
  }

  // Elevated North Dais leading up to the Vault Portal (y = room.floor_y + 1)
  if (dz >= 2 && y == room.floor_y + 1) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Aquifer Moat & Stepping Stones (Level 1 Obstacle)
  bool in_moat_span = (dz == 0 || dz == 1) && (abs_x <= room.half_width - 2);
  if (in_moat_span) {
    if (abs_x <= 1 && y == room.floor_y) {
      out = Voxel{MAT_PRECURSOR_STONE, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (y == room.floor_y) {
      out = Voxel{MAT_BIOLUMINESCENT_FLORA, 0};
      return;
    }
    if (y == room.floor_y - 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
      return;
    }
  }

  // Room floor
  if (y == room.floor_y) {
    uint8_t floor_mat = ((abs_x + abs_z) % 2 == 0) ? MAT_TITANIUM : MAT_PRECURSOR_STONE;
    out = Voxel{floor_mat, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Ribbed Vault Arch Ceiling
  if (y >= room.ceiling_y - 3) {
    if (dz % 4 == 0 || abs_x % 4 == 0) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    } else {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    }
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_subterranean_vault_bastion(const RoomPlacement &room,
                                                       int x, int y, int z,
                                                       Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  int abs_x = std::abs(dx);
  int abs_z = std::abs(dz);
  int manhattan = abs_x + abs_z;

  // Reinforced bulkhead fortified perimeter
  bool is_outer_wall = (abs_x > room.half_width || abs_z > room.half_depth ||
                        manhattan >= (room.half_width + room.half_depth - 2));
  if (is_outer_wall) {
    if (is_doorway_floor(room, dx, dz, y)) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (is_doorway_air(room, dx, dz, y)) {
      out = Voxel{MAT_AIR, 0};
      return;
    }
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Elevated Security Mezzanine Catwalk at y = room.floor_y + 5
  int catwalk_y = room.floor_y + 5;
  bool is_catwalk_ring = (y == catwalk_y) && (abs_x >= room.half_width - 3 || abs_z >= room.half_depth - 3);
  if (is_catwalk_ring) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Support pillars for mezzanine
  if ((abs_x == room.half_width - 3 && abs_z == room.half_depth - 3) &&
      y >= room.floor_y && y <= catwalk_y) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Fortified Spike Trench / Hazardous Defense Field (Level 2 Obstacle)
  bool in_hazard_trench = (dz >= -1 && dz <= 1) && (abs_x <= room.half_width - 2);
  if (in_hazard_trench) {
    if (abs_x <= 1 && y == room.floor_y) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (y == room.floor_y) {
      out = Voxel{MAT_OBSIDIAN_SPIKES, 0};
      return;
    }
    if (y == room.floor_y - 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
      return;
    }
  }

  // Room floor
  if (y == room.floor_y) {
    out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Ceiling industrial gantry
  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_abyssal_vault_chasm(const RoomPlacement &room,
                                                int x, int y, int z,
                                                Voxel &out) const {
  int dx = x - room.center.x;
  int dz = z - room.center.z;
  int abs_x = std::abs(dx);
  int abs_z = std::abs(dz);

  int upper_y = room.floor_y;
  int chasm_bottom = 4;

  // Perimeter bounding basalt walls
  if (abs_x >= room.half_width || abs_z >= room.half_depth) {
    if (is_doorway_floor(room, dx, dz, y)) {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (is_doorway_air(room, dx, dz, y)) {
      out = Voxel{MAT_AIR, 0};
      return;
    }
    // Irradiated ore veins embedded directly into the cliff wall
    if ((y >= chasm_bottom + 1 && y <= upper_y - 2) && ((x + z + y) % 5 == 0)) {
      out = Voxel{MAT_RADIOACTIVE_ORE, VOXEL_FLAG_ANCHORED | VOXEL_FLAG_EMISSIVE};
      return;
    }
    out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Arrival ledge at South (dz <= -room.half_depth + 3)
  bool is_arrival_ledge = (dz <= -room.half_depth + 3) && (abs_x <= room.half_width);
  if (is_arrival_ledge && y == upper_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Boss Dais & Vault Platform at North (dz >= room.half_depth - 5)
  bool is_boss_dais = (dz >= room.half_depth - 5) && (abs_x <= 5);
  if (is_boss_dais) {
    if (y == upper_y) {
      out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
      return;
    }
    if (y >= chasm_bottom && y < upper_y && abs_x <= 4 && dz >= room.half_depth - 4) {
      out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
      return;
    }
  }

  // Floating Monolithic Stepping Platforms Spanning the Void (Level 3 Obstacle)
  bool is_stepping_island = (y == upper_y) &&
      (((dz >= -4 && dz <= -2) && (abs_x <= 2)) ||
       ((dz >= -1 && dz <= 1) && (abs_x <= 2)) ||
       ((dz >= 2 && dz <= 4) && (abs_x <= 2)));
  if (is_stepping_island) {
    out = Voxel{MAT_PRECURSOR_STONE, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Chasm Bottom: Molten Magma Lake & Punji Spikes
  if (y == chasm_bottom) {
    if (abs_x % 3 == 0) {
      out = Voxel{MAT_OBSIDIAN_SPIKES, VOXEL_FLAG_ANCHORED};
    } else {
      out = Voxel{MAT_THERMITE_SLAG, VOXEL_FLAG_EMISSIVE};
    }
    return;
  }

  // Solid bedrock floor underneath chasm
  if (y < chasm_bottom) {
    out = Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
    return;
  }

  // Ceiling
  if (y >= room.ceiling_y - 2) {
    out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
    return;
  }

  out = Voxel{MAT_AIR, 0};
}
} // namespace Voidfall
