#include "level_shapes.hpp"
#include "room_carving_utils.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <cassert>

namespace Voidfall {

RoomCategory get_room_category(RoomShapeType type) {
  switch (type) {
    case RoomShapeType::SpawnStagingCavern:
    case RoomShapeType::ExtractionLandingBay:
      return RoomCategory::SafeTerminal;

    case RoomShapeType::MiningPillarHall:
    case RoomShapeType::CrystallineGeode:
    case RoomShapeType::PrismaticCrystalCathedral:
    case RoomShapeType::SubterraneanAquiferOasis:
    case RoomShapeType::BioluminescentFirmamentAbyss:
    case RoomShapeType::BioluminescentGlowwormGrotto:
      return RoomCategory::NaturalCrystalline;

    case RoomShapeType::FungoidBioGrotto:
    case RoomShapeType::ToxicMiasmaSwamp:
      return RoomCategory::BioOrganic;

    case RoomShapeType::IndustrialVaultBunker:
    case RoomShapeType::LaserDefenseFoundry:
    case RoomShapeType::PrecursorCoolantReservoir:
    case RoomShapeType::CyclopeanExcavationSilo:
    case RoomShapeType::TerracedQuarry:
    case RoomShapeType::PrecursorSanctumVault:
    case RoomShapeType::SubterraneanVaultBastion:
      return RoomCategory::PrecursorIndustrial;

    case RoomShapeType::MagmaCalderaLake:
    case RoomShapeType::MoltenMagmaFoundry:
      return RoomCategory::VolcanicThermal;

    case RoomShapeType::FaultLineCrevasse:
    case RoomShapeType::CrumblingArchCanyon:
    case RoomShapeType::SpikeTrenchArena:
    case RoomShapeType::AncientTitanNecropolis:
    case RoomShapeType::TectonicAbyssalSinkhole:
      return RoomCategory::TectonicChasm;

    case RoomShapeType::AbyssalVerticalChasm:
    case RoomShapeType::ColossalAbyssalChasm:
    case RoomShapeType::VoidSingularityRift:
    case RoomShapeType::RadioactiveCoreSanctuary:
    case RoomShapeType::ColossalVaultedDredgeCathedral:
    case RoomShapeType::AbyssalVaultChasm:
    default:
      return RoomCategory::CosmicVoidAbyss;
  }
}

const char* get_room_category_name(RoomCategory cat) {
  switch (cat) {
    case RoomCategory::SafeTerminal: return "SafeTerminal";
    case RoomCategory::NaturalCrystalline: return "NaturalCrystalline";
    case RoomCategory::BioOrganic: return "BioOrganic";
    case RoomCategory::PrecursorIndustrial: return "PrecursorIndustrial";
    case RoomCategory::VolcanicThermal: return "VolcanicThermal";
    case RoomCategory::TectonicChasm: return "TectonicChasm";
    case RoomCategory::CosmicVoidAbyss: return "CosmicVoidAbyss";
    default: return "Unknown";
  }
}

bool is_harsh_hazard_room(RoomShapeType type) {
  switch (type) {
    case RoomShapeType::MagmaCalderaLake:
    case RoomShapeType::MoltenMagmaFoundry:
    case RoomShapeType::RadioactiveCoreSanctuary:
    case RoomShapeType::VoidSingularityRift:
    case RoomShapeType::ColossalAbyssalChasm:
    case RoomShapeType::AbyssalVerticalChasm:
    case RoomShapeType::SpikeTrenchArena:
    case RoomShapeType::ToxicMiasmaSwamp:
    case RoomShapeType::SubterraneanVaultBastion:
    case RoomShapeType::AbyssalVaultChasm:
      return true;
    default:
      return false;
  }
}

bool is_extreme_spawn_hazard(RoomShapeType type) {
  switch (type) {
    case RoomShapeType::RadioactiveCoreSanctuary:
    case RoomShapeType::VoidSingularityRift:
    case RoomShapeType::MoltenMagmaFoundry:
    case RoomShapeType::MagmaCalderaLake:
    case RoomShapeType::ColossalAbyssalChasm:
    case RoomShapeType::AbyssalVerticalChasm:
    case RoomShapeType::AbyssalVaultChasm:
      return true;
    default:
      return false;
  }
}

bool is_vault_room_type(RoomShapeType type) {
  switch (type) {
    case RoomShapeType::AbyssalVaultChasm:
    case RoomShapeType::SubterraneanVaultBastion:
    case RoomShapeType::PrecursorSanctumVault:
    case RoomShapeType::IndustrialVaultBunker:
      return true;
    default:
      return false;
  }
}

bool is_extraction_climax_room(RoomShapeType type) {
  switch (type) {
    case RoomShapeType::ColossalVaultedDredgeCathedral:
    case RoomShapeType::TectonicAbyssalSinkhole:
    case RoomShapeType::CyclopeanExcavationSilo:
    case RoomShapeType::IndustrialVaultBunker:
    case RoomShapeType::LaserDefenseFoundry:
    case RoomShapeType::ColossalAbyssalChasm:
    case RoomShapeType::AncientTitanNecropolis:
    case RoomShapeType::PrismaticCrystalCathedral:
    case RoomShapeType::PrecursorSanctumVault:
    case RoomShapeType::SubterraneanVaultBastion:
    case RoomShapeType::AbyssalVaultChasm:
      return true;
    default:
      return false;
  }
}

bool is_colossal_or_massive_room(RoomShapeType type) {
  switch (type) {
    case RoomShapeType::ColossalVaultedDredgeCathedral:
    case RoomShapeType::TectonicAbyssalSinkhole:
    case RoomShapeType::CyclopeanExcavationSilo:
    case RoomShapeType::BioluminescentFirmamentAbyss:
    case RoomShapeType::AbyssalVerticalChasm:
    case RoomShapeType::VoidSingularityRift:
    case RoomShapeType::RadioactiveCoreSanctuary:
    case RoomShapeType::ColossalAbyssalChasm:
    case RoomShapeType::PrismaticCrystalCathedral:
    case RoomShapeType::AncientTitanNecropolis:
    case RoomShapeType::SubterraneanAquiferOasis:
    case RoomShapeType::MoltenMagmaFoundry:
      return true;
    default:
      return false;
  }
}

float get_room_adjacency_affinity(RoomShapeType typeA, RoomShapeType typeB) {
  if (typeA == typeB) {
    // Zero-tolerance rule: two rooms of the same type must never be adjacent!
    return -9999.0f;
  }

  RoomCategory catA = get_room_category(typeA);
  RoomCategory catB = get_room_category(typeB);

  // Spawn and extraction terminal affinities
  if (catA == RoomCategory::SafeTerminal || catB == RoomCategory::SafeTerminal) {
    if (typeA == RoomShapeType::SpawnStagingCavern || typeB == RoomShapeType::SpawnStagingCavern) {
      RoomShapeType other = (typeA == RoomShapeType::SpawnStagingCavern) ? typeB : typeA;
      if (is_extreme_spawn_hazard(other)) return -500.0f;
      if (other == RoomShapeType::MiningPillarHall || other == RoomShapeType::CrystallineGeode ||
          other == RoomShapeType::TerracedQuarry || other == RoomShapeType::SubterraneanAquiferOasis ||
          other == RoomShapeType::BioluminescentGlowwormGrotto) {
        return 15.0f;
      }
      return 5.0f;
    }
    if (typeA == RoomShapeType::ExtractionLandingBay || typeB == RoomShapeType::ExtractionLandingBay) {
      RoomShapeType other = (typeA == RoomShapeType::ExtractionLandingBay) ? typeB : typeA;
      if (is_extraction_climax_room(other)) return 18.0f;
      return 6.0f;
    }
  }

  float affinity = 0.0f;

  // Thematic contrast & harmony affinities:
  // 1. Thermal + Coolant / Water
  if ((catA == RoomCategory::VolcanicThermal && (typeB == RoomShapeType::PrecursorCoolantReservoir || typeB == RoomShapeType::SubterraneanAquiferOasis)) ||
      (catB == RoomCategory::VolcanicThermal && (typeA == RoomShapeType::PrecursorCoolantReservoir || typeA == RoomShapeType::SubterraneanAquiferOasis))) {
    affinity += 16.0f;
  }

  // 2. Volcanic + Basalt Tectonic
  if ((catA == RoomCategory::VolcanicThermal && catB == RoomCategory::TectonicChasm) ||
      (catB == RoomCategory::VolcanicThermal && catA == RoomCategory::TectonicChasm)) {
    affinity += 10.0f;
  }

  // 3. BioOrganic + Crystalline Water / Oasis
  if ((catA == RoomCategory::BioOrganic && (typeB == RoomShapeType::SubterraneanAquiferOasis || typeB == RoomShapeType::BioluminescentGlowwormGrotto || typeB == RoomShapeType::CrystallineGeode)) ||
      (catB == RoomCategory::BioOrganic && (typeA == RoomShapeType::SubterraneanAquiferOasis || typeA == RoomShapeType::BioluminescentGlowwormGrotto || typeA == RoomShapeType::CrystallineGeode))) {
    affinity += 14.0f;
  }

  // 4. Precursor Industrial + Excavation / Mining
  if ((catA == RoomCategory::PrecursorIndustrial && (typeB == RoomShapeType::MiningPillarHall || typeB == RoomShapeType::CyclopeanExcavationSilo || typeB == RoomShapeType::TerracedQuarry)) ||
      (catB == RoomCategory::PrecursorIndustrial && (typeA == RoomShapeType::MiningPillarHall || typeA == RoomShapeType::CyclopeanExcavationSilo || typeA == RoomShapeType::TerracedQuarry))) {
    affinity += 12.0f;
  }

  // 5. Tectonic Chasm + Ancient Titan / Deep Gorge
  if ((catA == RoomCategory::TectonicChasm && (typeB == RoomShapeType::AncientTitanNecropolis || typeB == RoomShapeType::CrumblingArchCanyon || typeB == RoomShapeType::TectonicAbyssalSinkhole)) ||
      (catB == RoomCategory::TectonicChasm && (typeA == RoomShapeType::AncientTitanNecropolis || typeA == RoomShapeType::CrumblingArchCanyon || typeA == RoomShapeType::TectonicAbyssalSinkhole))) {
    affinity += 12.0f;
  }

  // 6. Cosmic Void Abyss + Vaulted Cathedral / Firmament
  if ((catA == RoomCategory::CosmicVoidAbyss && (typeB == RoomShapeType::ColossalVaultedDredgeCathedral || typeB == RoomShapeType::BioluminescentFirmamentAbyss || typeB == RoomShapeType::PrismaticCrystalCathedral)) ||
      (catB == RoomCategory::CosmicVoidAbyss && (typeA == RoomShapeType::ColossalVaultedDredgeCathedral || typeA == RoomShapeType::BioluminescentFirmamentAbyss || typeA == RoomShapeType::PrismaticCrystalCathedral))) {
    affinity += 10.0f;
  }

  // 7. General Category Diversity Bonus
  if (catA != catB) {
    affinity += 4.0f;
  } else {
    affinity -= 3.0f;
  }

  // Harsh hazard clustering penalty
  if (is_harsh_hazard_room(typeA) && is_harsh_hazard_room(typeB)) {
    affinity -= 15.0f;
  }

  return affinity;
}

bool LevelGenerator::are_rooms_adjacent(int gx1, int gz1, int gx2, int gz2, bool allow_diagonal) const {
  int dx = std::abs(gx1 - gx2);
  int dz = std::abs(gz1 - gz2);
  if (allow_diagonal) {
    return (dx <= 1 && dz <= 1) && (dx + dz > 0);
  }
  return (dx + dz == 1);
}

int LevelGenerator::count_adjacent_same_type_violations() const {
  int violations = 0;
  for (int gx = 0; gx < m_grid_w; ++gx) {
    for (int gz = 0; gz < m_grid_d; ++gz) {
      const RoomPlacement *rA = get_room_at_grid(gx, gz);
      if (!rA) continue;

      if (gx + 1 < m_grid_w) {
        const RoomPlacement *rB = get_room_at_grid(gx + 1, gz);
        if (rB && rA->type == rB->type) {
          violations++;
        }
      }
      if (gz + 1 < m_grid_d) {
        const RoomPlacement *rB = get_room_at_grid(gx, gz + 1);
        if (rB && rA->type == rB->type) {
          violations++;
        }
      }
    }
  }
  return violations;
}

int LevelGenerator::count_diagonal_same_type_violations() const {
  int violations = 0;
  for (int gx = 0; gx < m_grid_w - 1; ++gx) {
    for (int gz = 0; gz < m_grid_d - 1; ++gz) {
      const RoomPlacement *rA = get_room_at_grid(gx, gz);
      const RoomPlacement *rNE = get_room_at_grid(gx + 1, gz + 1);
      if (rA && rNE && rA->type == rNE->type) {
        violations++;
      }
      const RoomPlacement *rSE = get_room_at_grid(gx + 1, gz);
      const RoomPlacement *rNW = get_room_at_grid(gx, gz + 1);
      if (rSE && rNW && rSE->type == rNW->type) {
        violations++;
      }
    }
  }
  return violations;
}

bool LevelGenerator::validate_room_adjacency_rules() const {
  return count_adjacent_same_type_violations() == 0;
}

LevelGenerator::LevelGenerator(int sector_index, uint32_t seed,
                               int vertical_chunks, int target_objective)
    : m_sector_index(sector_index), m_seed(seed),
      m_world_h(vertical_chunks > 1 ? vertical_chunks * 32 : WORLD_HEIGHT),
      m_target_objective(target_objective > 0 ? target_objective :
          ((sector_index <= 1) ? 25 : (sector_index == 2) ? 30 : (sector_index == 3) ? 45 : std::min(45 + (sector_index - 3) * 10, 150))) {
  // Sector-based level scaling:
  // Sector 1: 3x3 grid (9 rooms, 72x72 voxels)
  // Sector 2: 4x4 grid (16 rooms, 96x96 voxels)
  // Sector 3: 5x5 grid (25 rooms, 120x120 voxels)
  if (m_sector_index <= 1) {
    m_grid_w = 3;
    m_grid_d = 3;
  } else if (m_sector_index == 2) {
    m_grid_w = 4;
    m_grid_d = 4;
  } else {
    m_grid_w = 5;
    m_grid_d = 5;
  }

  for (int x = 0; x < MAX_GRID_SIZE; ++x) {
    for (int z = 0; z < MAX_GRID_SIZE; ++z) {
      m_grid_room_indices[x][z] = -1;
    }
  }
  generate_layout();
}

uint32_t LevelGenerator::hash_coord(int x, int y, int z, uint32_t salt) const {
  uint32_t h = m_seed ^ (salt * 2654435761u);
  h ^= static_cast<uint32_t>(x) * 73856093u;
  h ^= static_cast<uint32_t>(y) * 19349663u;
  h ^= static_cast<uint32_t>(z) * 83492791u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

float LevelGenerator::pseudo_rand(int x, int y, int z, uint32_t salt) const {
  uint32_t h = hash_coord(x, y, z, salt);
  return static_cast<float>(h & 0xFFFF) / 65535.0f;
}

const RoomPlacement *LevelGenerator::get_room_at_grid(int gx, int gz) const {
  if (gx < 0 || gx >= m_grid_w || gz < 0 || gz >= m_grid_d)
    return nullptr;
  int idx = m_grid_room_indices[gx][gz];
  if (idx >= 0 && idx < static_cast<int>(m_rooms.size())) {
    return &m_rooms[idx];
  }
  return nullptr;
}

const RoomPlacement *
LevelGenerator::get_room_at_position(const glm::vec3 &pos) const {
  for (const auto &room : m_rooms) {
    if (pos.x >= static_cast<float>(room.center.x - room.half_width) &&
        pos.x <= static_cast<float>(room.center.x + room.half_width) &&
        pos.z >= static_cast<float>(room.center.z - room.half_depth) &&
        pos.z <= static_cast<float>(room.center.z + room.half_depth)) {
      return &room;
    }
  }
  // Secondary check: Wall perimeter buffer (up to 2 voxels into room walls)
  for (const auto &room : m_rooms) {
    if (pos.x >= static_cast<float>(room.center.x - room.half_width - 2) &&
        pos.x <= static_cast<float>(room.center.x + room.half_width + 2) &&
        pos.z >= static_cast<float>(room.center.z - room.half_depth - 2) &&
        pos.z <= static_cast<float>(room.center.z + room.half_depth + 2)) {
      return &room;
    }
  }
  return nullptr;
}

glm::vec3 LevelGenerator::spawn_position() const {
  const RoomPlacement *r = get_room_at_grid(0, 0);
  if (r) {
    return glm::vec3(r->center.x, static_cast<float>(r->floor_y + 1) + 0.1f,
                     r->center.z);
  }
  return glm::vec3(16.0f, 5.1f, 16.0f);
}

glm::vec3 LevelGenerator::extraction_position() const {
  const RoomPlacement *r = get_room_at_grid(m_grid_w - 1, m_grid_d - 1);
  if (r) {
    return glm::vec3(r->center.x, static_cast<float>(r->floor_y + 1),
                     r->center.z);
  }
  return glm::vec3(static_cast<float>(world_width() - 16), 5.0f,
                   static_cast<float>(world_depth() - 16));
}

bool LevelGenerator::is_in_bounds(int x, int y, int z) const {
  return (x >= 2 && x <= world_width() - 3 && z >= 2 &&
          z <= world_depth() - 3 && y >= 1 && y <= m_world_h - 5);
}

bool LevelGenerator::is_objective_voxel(int x, int y, int z) const {
  if (x < 0 || x >= world_width() || y < 0 || y >= m_world_h || z < 0 || z >= world_depth()) {
    return false;
  }
  size_t idx = static_cast<size_t>(x) + static_cast<size_t>(world_width()) * (static_cast<size_t>(y) + static_cast<size_t>(m_world_h) * static_cast<size_t>(z));
  size_t byte_idx = idx >> 3;
  if (byte_idx >= m_objective_bitset.size()) {
    return false;
  }
  return (m_objective_bitset[byte_idx] & (1u << (idx & 7u))) != 0;
}

static inline bool is_mineable_solid(uint8_t mat) {
  return mat != MAT_AIR &&
         mat != MAT_DREDGE_BEDROCK &&
         mat != MAT_GAS &&
         mat != MAT_VOLATILE_SMOKE &&
         mat != MAT_CRYSTAL_AQUIFER &&
         mat != MAT_ACID &&
         mat != MAT_COOLANT &&
         mat != MAT_THERMITE_SLAG;
}

void LevelGenerator::add_objective_voxel(int x, int y, int z) {
  if (x <= 3 || x >= world_width() - 4 || y <= 3 || y >= m_world_h - 5 || z <= 3 || z >= world_depth() - 4) {
    return;
  }
  // Enforce zero floating crystals: only place objective crystals into solid mineable base voxels
  Voxel base = sample_base_voxel(x, y, z);
  if (!is_mineable_solid(base.material_id)) {
    return;
  }
  size_t idx = static_cast<size_t>(x) + static_cast<size_t>(world_width()) * (static_cast<size_t>(y) + static_cast<size_t>(m_world_h) * static_cast<size_t>(z));
  size_t byte_idx = idx >> 3;
  if (byte_idx >= m_objective_bitset.size()) {
    return;
  }
  if ((m_objective_bitset[byte_idx] & (1u << (idx & 7u))) == 0) {
    m_objective_bitset[byte_idx] |= (1u << (idx & 7u));
    m_objective_positions.push_back(glm::ivec3(x, y, z));
  }
}

bool LevelGenerator::find_wall_surface_voxel(const glm::ivec3& anchor, const RoomPlacement* room,
                                            glm::ivec3& out_surface, glm::ivec3& out_inward_normal) const {
  const RoomPlacement* r = room;
  if (!r) {
    r = get_room_at_position(glm::vec3(anchor.x, anchor.y, anchor.z));
  }

  int min_y = r ? (r->floor_y + 1) : 4;
  int max_y = r ? (r->ceiling_y - 1) : (m_world_h - 6);

  auto is_valid_surface = [&](const glm::ivec3& pos, glm::ivec3& norm) -> bool {
    if (pos.x <= 3 || pos.x >= world_width() - 4 || pos.y < min_y || pos.y > max_y ||
        pos.z <= 3 || pos.z >= world_depth() - 4) {
      return false;
    }
    if (is_objective_voxel(pos.x, pos.y, pos.z)) {
      return false;
    }
    uint8_t mat = sample_base_voxel(pos.x, pos.y, pos.z).material_id;
    if (!is_mineable_solid(mat)) {
      return false;
    }
    // Check if at least one cardinal neighbor is MAT_AIR (facing open space)
    const glm::ivec3 n_dirs[6] = {
      {-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}
    };
    for (const auto& nd : n_dirs) {
      glm::ivec3 np = pos + nd;
      if (np.x >= 0 && np.x < world_width() && np.y >= 0 && np.y < m_world_h &&
          np.z >= 0 && np.z < world_depth()) {
        if (sample_base_voxel(np.x, np.y, np.z).material_id == MAT_AIR) {
          norm = -nd; // Points into wall (away from air)
          return true;
        }
      }
    }
    return false;
  };

  glm::ivec3 norm(0);

  // 1. Direct anchor check
  if (is_valid_surface(anchor, norm)) {
    out_surface = anchor;
    out_inward_normal = norm;
    return true;
  }

  // 2. Local shell search around anchor (radius 1..4) for structural features (pillars, ledges, benches, alcoves)
  for (int radius = 1; radius <= 4; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
          if (std::abs(dx) != radius && std::abs(dy) != radius && std::abs(dz) != radius) {
            continue;
          }
          glm::ivec3 cand = anchor + glm::ivec3(dx, dy, dz);
          if (is_valid_surface(cand, norm)) {
            out_surface = cand;
            out_inward_normal = norm;
            return true;
          }
        }
      }
    }
  }

  // 3. Outward raycast from room center towards anchor and perimeter wall
  if (r) {
    int dx = anchor.x - r->center.x;
    int dz = anchor.z - r->center.z;
    if (dx == 0 && dz == 0) {
      dx = 1;
    }
    float len = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float step_x = static_cast<float>(dx) / len;
    float step_z = static_cast<float>(dz) / len;

    int y_clamp = glm::clamp(anchor.y, min_y, max_y);
    int max_steps = std::max(r->half_width, r->half_depth) + 4;

    for (int s = 1; s <= max_steps; ++s) {
      int cx = r->center.x + static_cast<int>(std::round(step_x * s));
      int cz = r->center.z + static_cast<int>(std::round(step_z * s));
      glm::ivec3 cand(cx, y_clamp, cz);
      if (is_valid_surface(cand, norm)) {
        out_surface = cand;
        out_inward_normal = norm;
        return true;
      }
    }

    // 4. Perimeter wall cardinal fallback
    const glm::ivec2 wall_dirs[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    for (const auto& wd : wall_dirs) {
      for (int py = min_y + 1; py <= std::min(min_y + 4, max_y); ++py) {
        for (int s = 2; s <= max_steps; ++s) {
          int cx = r->center.x + wd.x * s;
          int cz = r->center.z + wd.y * s;
          glm::ivec3 cand(cx, py, cz);
          if (is_valid_surface(cand, norm)) {
            out_surface = cand;
            out_inward_normal = norm;
            return true;
          }
        }
      }
    }
  }

  return false;
}

void LevelGenerator::add_objective_deposit(const glm::ivec3& anchor, int deposit_size, RoomPlacement* room) {
  if (deposit_size <= 0) return;
  int initial_count = static_cast<int>(m_objective_positions.size());

  glm::ivec3 surface_pos;
  glm::ivec3 inward_norm;

  if (find_wall_surface_voxel(anchor, room, surface_pos, inward_norm)) {
    // 1. Add the visible surface block on the wall face
    add_objective_voxel(surface_pos.x, surface_pos.y, surface_pos.z);

    // 2. If deposit has multiple blocks, extend the vein inside the solid wall/structure
    if (deposit_size > 1) {
      glm::ivec3 t1, t2;
      if (inward_norm.x != 0) {
        t1 = glm::ivec3(0, 1, 0);
        t2 = glm::ivec3(0, 0, 1);
      } else if (inward_norm.z != 0) {
        t1 = glm::ivec3(0, 1, 0);
        t2 = glm::ivec3(1, 0, 0);
      } else {
        t1 = glm::ivec3(1, 0, 0);
        t2 = glm::ivec3(0, 0, 1);
      }

      const glm::ivec3 vein_offsets[] = {
        inward_norm,                          // 1 block deep into wall
        inward_norm + t1,                     // deep + tangent
        inward_norm - t1,                     // deep - tangent
        inward_norm + t2,                     // deep + tangent
        inward_norm - t2,                     // deep - tangent
        inward_norm * 2,                      // 2 blocks deep into wall
        inward_norm * 2 + t1,                 // 2 deep + tangent
        inward_norm * 2 + t2,                 // 2 deep + tangent
        t1,                                   // adjacent solid wall block
        -t1,                                  // adjacent solid wall block
        t2,                                   // adjacent solid wall block
        -t2,                                  // adjacent solid wall block
        inward_norm + glm::ivec3(0, -1, 0),   // deep + down
        inward_norm + glm::ivec3(0, 1, 0),    // deep + up
        inward_norm * 3                       // 3 blocks deep into wall
      };

      for (const auto& off : vein_offsets) {
        if ((static_cast<int>(m_objective_positions.size()) - initial_count) >= deposit_size) {
          break;
        }
        glm::ivec3 p = surface_pos + off;
        if (p.x <= 3 || p.x >= world_width() - 4 || p.y <= 3 || p.y >= m_world_h - 5 ||
            p.z <= 3 || p.z >= world_depth() - 4) {
          continue;
        }
        if (!is_mineable_solid(sample_base_voxel(p.x, p.y, p.z).material_id)) {
          continue; // Strictly NEVER place in air or fluid
        }
        add_objective_voxel(p.x, p.y, p.z);
      }
    }
  } else {
    // Fallback: If no surface could be resolved, never spawn floating blocks
    if (is_mineable_solid(sample_base_voxel(anchor.x, anchor.y, anchor.z).material_id)) {
      add_objective_voxel(anchor.x, anchor.y, anchor.z);
    }
  }

  int placed = static_cast<int>(m_objective_positions.size()) - initial_count;
  if (room && placed > 0) {
    room->objective_deposit_count++;
    room->spawned_objective_voxels += placed;
  }
}

int LevelGenerator::objective_room_spread_count() const {
  int count = 0;
  for (const auto& r : m_rooms) {
    if (r.spawned_objective_voxels > 0) {
      count++;
    }
  }
  return count;
}

std::vector<glm::ivec3> LevelGenerator::get_room_candidate_spawn_offsets(
    RoomShapeType type, int half_w, int half_d, int floor_y, int ceil_y) const {
  std::vector<glm::ivec3> offsets;

  // Staging arrival cavern and Extraction landing bay have 0 candidate nodes:
  // Strictly prevent near-spawn or extraction pad objective fulfillment
  if (type == RoomShapeType::SpawnStagingCavern ||
      type == RoomShapeType::ExtractionLandingBay) {
    return offsets;
  }

  int w = std::max(3, half_w - 3);
  int d = std::max(3, half_d - 3);
  int safe_ceil = std::max(floor_y + 4, ceil_y - 3);

  auto clamp_y = [&](int y_val) {
    return glm::clamp(y_val, floor_y + 2, safe_ceil);
  };

  switch (type) {
  case RoomShapeType::MiningPillarHall:
    // Pillar flanks and upper ledges on structural columns
    offsets.push_back(glm::ivec3(w / 2 + 1, clamp_y(floor_y + 2), d / 2 + 1));
    offsets.push_back(glm::ivec3(-w / 2 - 1, clamp_y(floor_y + 2), -d / 2 - 1));
    offsets.push_back(glm::ivec3(w / 2 + 1, clamp_y(floor_y + 5), -d / 2 - 1));
    offsets.push_back(glm::ivec3(-w / 2 - 1, clamp_y(floor_y + 5), d / 2 + 1));
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 2), d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 2), -d));
    break;

  case RoomShapeType::CrystallineGeode:
    // Spherical shell curvature crevices and perimeter ledges
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 3), d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 3), -d));
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 4), -d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 4), d));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 5), d - 1));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 5), -d + 1));
    break;

  case RoomShapeType::TerracedQuarry:
    // Stepped excavation benches and quarry face ledges
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 2), d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 2), -d));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 4), 0));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 4), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 6), d - 2));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 6), -d + 2));
    break;

  case RoomShapeType::IndustrialVaultBunker:
    // Mezzanine wall alcoves and lower machinery recesses
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 5), d - 2));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 5), -d + 2));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 2), 0));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 2), 0));
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 2), -d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 2), d));
    break;

  case RoomShapeType::FaultLineCrevasse:
    // Upper fault ledges and crevasse wall pockets
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 4), d));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 4), -d));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 2), -d));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 2), d));
    break;

  case RoomShapeType::AbyssalVerticalChasm:
    // Spiral ledge landings and high wall alcoves
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 4), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 8), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 12), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 16), -d + 2));
    break;

  case RoomShapeType::RadioactiveCoreSanctuary:
    // Outer boundary dry corners away from central core
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 2), d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 2), -d));
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 2), -d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 2), d));
    break;

  case RoomShapeType::MagmaCalderaLake:
    // Basalt shoreline outcroppings and elevated caldera wall niches
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 3), d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 3), -d));
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 3), -d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 3), d));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 5), d - 1));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 5), -d + 1));
    break;

  case RoomShapeType::SpikeTrenchArena:
    // Elevated catwalk corner pylons safely above punji spikes
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 5), d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 5), -d));
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 5), -d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 5), d));
    break;

  case RoomShapeType::VoidSingularityRift:
    // Perimeter obsidian platform bedrock anchor points
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 3), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 3), -d + 2));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 3), -d + 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 3), d - 2));
    break;

  case RoomShapeType::FungoidBioGrotto:
    // Fungal spore-shelf ledges and root crevices
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 3), d - 1));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 4), -d + 1));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 2), -d + 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 2), d - 2));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 6), d - 2));
    break;

  case RoomShapeType::LaserDefenseFoundry:
    // Overhead catwalk maintenance recesses and smelting flume corners
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 4), d - 2));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 4), -d + 2));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 2), -d + 1));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 2), d - 1));
    break;

  case RoomShapeType::CrumblingArchCanyon:
    // Arch abutment pockets and canyon rock walls
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 4), 0));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 4), 0));
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 2), d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 2), -d));
    break;

  case RoomShapeType::SubterraneanAquiferOasis:
    // Grotto rock niches behind cascades and upper dry bank ledges
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 3), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 3), -d + 2));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 5), -d + 2));
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 4), -d + 1));
    break;

  case RoomShapeType::ColossalAbyssalChasm:
    // Suspension bridge abutment alcoves and mid-shaft cliff ledges
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 10), 0));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 10), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 6), d - 2));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 16), -d + 2));
    break;

  case RoomShapeType::MoltenMagmaFoundry:
    // Cooling wall embrasures and basalt mezzanine corners
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 3), d - 2));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 3), -d + 2));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 6), 0));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 6), 0));
    break;

  case RoomShapeType::ToxicMiasmaSwamp:
    // Elevated stone hummock niches and root embrasures
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 3), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 3), -d + 2));
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 5), 0));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 5), 0));
    break;

  case RoomShapeType::PrismaticCrystalCathedral:
    // Nave corner alcoves, triforium wall pockets and apse niche
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 3), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 3), -d + 2));
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 7), 0));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 7), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 4), d - 2));
    break;

  case RoomShapeType::AncientTitanNecropolis:
    // Fossil strata rock pockets and ribcage base sockets
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 2), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 2), -d + 2));
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 5), 0));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 5), 0));
    break;

  case RoomShapeType::BioluminescentGlowwormGrotto:
    // Pool perimeter rock alcoves and stalactite shelf niches
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 2), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 2), -d + 2));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 6), d - 2));
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 4), -d + 2));
    break;

  case RoomShapeType::PrecursorCoolantReservoir:
    // Conduit wall recesses and upper valve manifold ledge
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 3), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 3), -d + 2));
    offsets.push_back(glm::ivec3(w - 1, clamp_y(floor_y + 6), 0));
    offsets.push_back(glm::ivec3(-w + 1, clamp_y(floor_y + 6), 0));
    break;

  case RoomShapeType::ColossalVaultedDredgeCathedral:
    // Gantry catwalk overlook, triforium wall recess, pillar flank niches
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 7), 0));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 7), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 12), d - 2));
    offsets.push_back(glm::ivec3(w - 3, clamp_y(floor_y + 3), d - 3));
    offsets.push_back(glm::ivec3(-w + 3, clamp_y(floor_y + 3), -d + 3));
    break;

  case RoomShapeType::TectonicAbyssalSinkhole:
    // Intermediate spiral terrace landings and rim overlook crevice
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 5), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 9), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 13), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 17), -d + 2));
    break;

  case RoomShapeType::CyclopeanExcavationSilo:
    // Lower maintenance ring niche and upper ring maintenance recesses
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 5), 0));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 5), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 11), d - 2));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 11), -d + 2));
    break;

  case RoomShapeType::BioluminescentFirmamentAbyss:
    // Stone arch bridge abutments and perimeter cliff alcoves
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 6), 0));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 6), 0));
    offsets.push_back(glm::ivec3(w - 3, clamp_y(floor_y + 3), d - 3));
    offsets.push_back(glm::ivec3(-w + 3, clamp_y(floor_y + 3), -d + 3));
    break;

  case RoomShapeType::PrecursorSanctumVault:
    // Flanking dais niches and precursor column recesses
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 3), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 3), d - 2));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 2), -d + 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 2), -d + 2));
    break;

  case RoomShapeType::SubterraneanVaultBastion:
    // Mezzanine security observation alcoves and slag trench flanks
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 5), 0));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 5), 0));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 2), d - 2));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 2), -d + 2));
    break;

  case RoomShapeType::AbyssalVaultChasm:
    // Chasm stepping monoliths and boss dais platform alcoves
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 2), d - 3));
    offsets.push_back(glm::ivec3(w - 2, clamp_y(floor_y + 2), 0));
    offsets.push_back(glm::ivec3(-w + 2, clamp_y(floor_y + 2), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 2), -d + 3));
    break;

  default:
    // Standard wall crevice fallback
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 2), d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 2), -d));
    offsets.push_back(glm::ivec3(-w, clamp_y(floor_y + 2), d));
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 2), -d));
    offsets.push_back(glm::ivec3(w, clamp_y(floor_y + 3), 0));
    offsets.push_back(glm::ivec3(0, clamp_y(floor_y + 3), d));
    break;
  }
  return offsets;
}

void LevelGenerator::place_objective_resources() {
  m_objective_positions.clear();
  size_t total_voxels = static_cast<size_t>(world_width()) * static_cast<size_t>(m_world_h) * static_cast<size_t>(world_depth());
  m_objective_bitset.assign((total_voxels + 7) / 8, 0);

  for (auto &room : m_rooms) {
    room.objective_deposit_count = 0;
    room.spawned_objective_voxels = 0;
  }

  if (m_rooms.empty()) return;

  if (m_rooms.size() == 1) {
    auto offsets = get_room_candidate_spawn_offsets(m_rooms[0].type, m_rooms[0].half_width, m_rooms[0].half_depth, m_rooms[0].floor_y, m_rooms[0].ceiling_y);
    if (!offsets.empty()) {
      glm::ivec3 anchor = m_rooms[0].center + offsets[0];
      add_objective_deposit(anchor, 2, &m_rooms[0]);
    }
    m_validation_passed = true;
    return;
  }

  // Tight scarcity budget: strictly quota + 2 crystals across the entire cave system.
  int target_spawn_goal = m_target_objective + 2;

  // Categorize candidate distant rooms:
  // Distance 0 (Spawn Cavern) and Distance 1 (Adjacent Rooms) receive ZERO objective deposits.
  // ExtractionLandingBay is also strictly EXCLUDED (zero objective deposits).
  std::vector<size_t> distant_rooms;
  for (size_t i = 0; i < m_rooms.size(); ++i) {
    int dist = m_rooms[i].grid_x + m_rooms[i].grid_z;
    if (dist >= 2 && m_rooms[i].type != RoomShapeType::ExtractionLandingBay) {
      distant_rooms.push_back(i);
    }
  }

  if (distant_rooms.empty()) {
    m_validation_passed = false;
    return;
  }

  // Sort distant rooms by distance descending: prioritize far corners and deepest reaches first
  std::sort(distant_rooms.begin(), distant_rooms.end(), [this](size_t a, size_t b) {
    int da = m_rooms[a].grid_x + m_rooms[a].grid_z;
    int db = m_rooms[b].grid_x + m_rooms[b].grid_z;
    if (da != db) return da > db;
    uint32_t ha = hash_coord(m_rooms[a].grid_x, 0, m_rooms[a].grid_z, 911);
    uint32_t hb = hash_coord(m_rooms[b].grid_x, 0, m_rooms[b].grid_z, 911);
    return ha < hb;
  });

  int default_dep_size = (m_sector_index <= 1) ? 3 : 2;
  int max_per_room = (m_sector_index <= 1) ? 5 : 4;

  for (size_t r_idx : distant_rooms) {
    if (total_objective_material_count() >= target_spawn_goal) break;

    auto offsets = get_room_candidate_spawn_offsets(m_rooms[r_idx].type, m_rooms[r_idx].half_width, m_rooms[r_idx].half_depth, m_rooms[r_idx].floor_y, m_rooms[r_idx].ceiling_y);
    if (offsets.empty()) continue;

    bool placed_in_room = true;
    while (m_rooms[r_idx].spawned_objective_voxels < max_per_room &&
           total_objective_material_count() < target_spawn_goal && placed_in_room) {
      placed_in_room = false;
      int start_off = (static_cast<int>(m_seed) + m_rooms[r_idx].objective_deposit_count * 2) % offsets.size();
      for (size_t try_i = 0; try_i < offsets.size(); ++try_i) {
        int off_idx = (start_off + try_i) % offsets.size();
        glm::ivec3 anchor = m_rooms[r_idx].center + offsets[off_idx];
        int remaining = target_spawn_goal - total_objective_material_count();
        int room_space = max_per_room - m_rooms[r_idx].spawned_objective_voxels;
        int dep_size = std::min({remaining, default_dep_size, room_space});
        if (dep_size <= 0) break;

        int before = total_objective_material_count();
        add_objective_deposit(anchor, dep_size, &m_rooms[r_idx]);
        if (total_objective_material_count() > before) {
          placed_in_room = true;
          break;
        }
      }
    }
  }
}

void LevelGenerator::validate_and_balance_objectives() {
  if (m_rooms.empty()) {
    m_validation_passed = false;
    return;
  }
  if (m_rooms.size() == 1) {
    m_validation_passed = true;
    return;
  }

  int target_min = m_target_objective;
  int allowed_max_room = (m_sector_index <= 1) ? 5 : 4;

  // 1. Completeness safety fallback:
  int safety_attempts = 0;
  while (total_objective_material_count() < target_min && safety_attempts++ < 30) {
    bool placed_any = false;
    for (size_t i = 0; i < m_rooms.size(); ++i) {
      int dist = m_rooms[i].grid_x + m_rooms[i].grid_z;
      if (dist >= 2 && m_rooms[i].type != RoomShapeType::ExtractionLandingBay &&
          m_rooms[i].spawned_objective_voxels < allowed_max_room) {
        auto offsets = get_room_candidate_spawn_offsets(m_rooms[i].type, m_rooms[i].half_width, m_rooms[i].half_depth, m_rooms[i].floor_y, m_rooms[i].ceiling_y);
        glm::ivec3 anchor = m_rooms[i].center;
        if (!offsets.empty()) {
          int off_idx = (m_rooms[i].objective_deposit_count + safety_attempts) % offsets.size();
          anchor += offsets[off_idx];
        } else {
          anchor += glm::ivec3(2 + (safety_attempts % 3), m_rooms[i].floor_y + 2, 2 + (safety_attempts % 3));
        }
        int before = total_objective_material_count();
        add_objective_deposit(anchor, 1, &m_rooms[i]);
        if (total_objective_material_count() > before) {
          placed_any = true;
          if (total_objective_material_count() >= target_min) break;
        }
      }
    }
    if (!placed_any) break;
  }

  // 2. Strict Scarcity Ceiling: remove any excess voxels above target + 3
  while (static_cast<int>(m_objective_positions.size()) > m_target_objective + 3) {
    glm::ivec3 p = m_objective_positions.back();
    m_objective_positions.pop_back();
    size_t idx = static_cast<size_t>(p.x) + static_cast<size_t>(world_width()) * (static_cast<size_t>(p.y) + static_cast<size_t>(m_world_h) * static_cast<size_t>(p.z));
    size_t byte_idx = idx >> 3;
    if (byte_idx < m_objective_bitset.size()) {
      m_objective_bitset[byte_idx] &= ~(1u << (idx & 7u));
    }
  }

  // 3. Re-tally per room counts
  for (auto &room : m_rooms) {
    room.spawned_objective_voxels = 0;
  }
  for (const auto &pos : m_objective_positions) {
    const RoomPlacement *r = get_room_at_position(glm::vec3(pos.x, pos.y, pos.z));
    if (r) {
      for (auto &mut_r : m_rooms) {
        if (mut_r.grid_x == r->grid_x && mut_r.grid_z == r->grid_z) {
          mut_r.spawned_objective_voxels++;
          break;
        }
      }
    }
  }

  m_validation_passed = validate_level_resources();
}

LevelGenerator::RoomValidationReport LevelGenerator::validate_room_resources(const RoomPlacement& room) const {
  RoomValidationReport report;
  report.spawned_voxels = room.spawned_objective_voxels;
  report.deposit_count = room.objective_deposit_count;
  int dist = room.grid_x + room.grid_z;
  report.is_near_spawn = (dist <= 1);
  int allowed_max = (m_sector_index <= 1) ? 5 : 4;

  if (report.is_near_spawn && room.spawned_objective_voxels > 0) {
    report.valid = false;
    report.details = "Near-spawn room contains objective materials (violates exploration gating)";
    return report;
  }
  if (room.type == RoomShapeType::ExtractionLandingBay && room.spawned_objective_voxels > 0) {
    report.valid = false;
    report.details = "Extraction landing bay contains objective materials (must be gathered in field)";
    return report;
  }
  if (room.spawned_objective_voxels > allowed_max) {
    report.valid = false;
    report.exceeds_capacity = true;
    report.details = "Room exceeds maximum objective voxel capacity of " + std::to_string(allowed_max);
    return report;
  }
  report.valid = true;
  report.details = "Room resource distribution valid";
  return report;
}

int LevelGenerator::rooms_with_zero_objective_count() const {
  int count = 0;
  for (const auto& r : m_rooms) {
    if (r.spawned_objective_voxels == 0) {
      count++;
    }
  }
  return count;
}

bool LevelGenerator::validate_level_resources() const {
  if (m_rooms.empty()) return false;
  if (m_rooms.size() == 1) return true;

  int total = total_objective_material_count();
  int target_min = m_target_objective;
  if (total < target_min || total > target_min + 3) {
    return false;
  }

  int zero_resource_rooms = 0;
  for (const auto& room : m_rooms) {
    auto report = validate_room_resources(room);
    if (!report.valid) {
      return false;
    }
    if (room.spawned_objective_voxels == 0) {
      zero_resource_rooms++;
    }
  }

  if (zero_resource_rooms == 0) {
    return false;
  }

  int min_spread = (m_grid_w <= 3) ? 4 : (m_grid_w == 4 ? 7 : 10);
  if (objective_room_spread_count() < std::min(min_spread, static_cast<int>(m_rooms.size()) - 3)) {
    return false;
  }

  return true;
}

bool LevelGenerator::validate_resource_compleatability() const {
  return m_validation_passed && validate_level_resources();
}

void LevelGenerator::create_single_room_test_layout(RoomShapeType type) {
  m_rooms.clear();
  m_corridors.clear();
  m_objective_positions.clear();
  m_grid_w = 3;
  m_grid_d = 3;
  size_t total_voxels = static_cast<size_t>(world_width()) * static_cast<size_t>(m_world_h) * static_cast<size_t>(world_depth());
  m_objective_bitset.assign((total_voxels + 7) / 8, 0);
  m_validation_passed = true;

  for (int x = 0; x < MAX_GRID_SIZE; ++x) {
    for (int z = 0; z < MAX_GRID_SIZE; ++z) {
      m_grid_room_indices[x][z] = -1;
    }
  }
  RoomPlacement room;
  room.grid_x = 1;
  room.grid_z = 1;
  room.center = glm::ivec3(36, 0, 36);
  room.half_width = 8;
  room.half_depth = 8;
  room.floor_y = 4;
  room.ceiling_y = 25;
  room.type = type;
  m_grid_room_indices[1][1] = 0;
  m_rooms.push_back(room);
  m_vault_placement = VaultPlacement{};
  generate_luminaries();
}

void LevelGenerator::create_corridor_test_layout() {
  m_rooms.clear();
  m_corridors.clear();
  m_objective_positions.clear();
  m_grid_w = 3;
  m_grid_d = 3;
  size_t total_voxels = static_cast<size_t>(world_width()) * static_cast<size_t>(m_world_h) * static_cast<size_t>(world_depth());
  m_objective_bitset.assign((total_voxels + 7) / 8, 0);
  m_validation_passed = true;

  for (int x = 0; x < MAX_GRID_SIZE; ++x) {
    for (int z = 0; z < MAX_GRID_SIZE; ++z) {
      m_grid_room_indices[x][z] = -1;
    }
  }
  // Room A at (16, 0, 36)
  RoomPlacement roomA;
  roomA.grid_x = 0;
  roomA.grid_z = 1;
  roomA.center = glm::ivec3(16, 0, 36);
  roomA.half_width = 8;
  roomA.half_depth = 8;
  roomA.floor_y = 4;
  roomA.ceiling_y = 16;
  roomA.type = RoomShapeType::IndustrialVaultBunker;
  roomA.connected_east = true;
  m_grid_room_indices[0][1] = 0;
  m_rooms.push_back(roomA);

  // Room B at (36, 0, 36)
  RoomPlacement roomB;
  roomB.grid_x = 1;
  roomB.grid_z = 1;
  roomB.center = glm::ivec3(36, 0, 36);
  roomB.half_width = 8;
  roomB.half_depth = 8;
  roomB.floor_y = 4;
  roomB.ceiling_y = 16;
  roomB.type = RoomShapeType::MiningPillarHall;
  roomB.connected_west = true;
  m_grid_room_indices[1][1] = 1;
  m_rooms.push_back(roomB);

  // Corridor between Room A and Room B
  CorridorPlacement corr;
  corr.start_pos = glm::ivec3(24, 4, 36);
  corr.end_pos = glm::ivec3(28, 4, 36);
  corr.floor_y = 4;
  corr.ceiling_y = 9;
  corr.half_width = 2;
  corr.is_x_axis = true;
  corr.has_bulkhead_door = true;
  m_corridors.push_back(corr);

  generate_luminaries();
}

void LevelGenerator::set_room_type(int gx, int gz, RoomShapeType type) {
  if (gx < 0 || gx >= m_grid_w || gz < 0 || gz >= m_grid_d)
    return;
  int idx = m_grid_room_indices[gx][gz];
  if (idx >= 0 && idx < static_cast<int>(m_rooms.size())) {
    m_rooms[idx].type = type;
    generate_luminaries();
  }
}

void LevelGenerator::generate_layout() {
  m_rooms.clear();
  m_corridors.clear();

  std::mt19937 rng(m_seed);

  // Grid cell centers: 16, 36, 56, 76, 96 (spacing = 20)
  std::vector<int> grid_coords;
  for (int i = 0; i < m_grid_w; ++i) {
    grid_coords.push_back(16 + i * 20);
  }

  // 1. Determine randomised massive & soaring vertical room locations per
  // sector Sector 1 (3x3): 1 massive room at central grand chamber (1, 1)
  // Sector 2 (4x4): 2-3 massive rooms randomly chosen from interior slots
  // {(1,1), (1,2), (2,1), (2,2)} Sector 3 (5x5): 4-6 massive rooms randomly
  // chosen from the 9 interior slots (1..3, 1..3)
  int target_massive_count = 1;
  if (m_sector_index == 2) {
    target_massive_count = 2 + static_cast<int>(rng() % 2);
  } else if (m_sector_index >= 3) {
    target_massive_count = 4 + static_cast<int>(rng() % 3);
  }

  std::vector<std::pair<int, int>> candidate_slots;
  for (int gx = 1; gx < m_grid_w - 1; ++gx) {
    for (int gz = 1; gz < m_grid_d - 1; ++gz) {
      candidate_slots.push_back({gx, gz});
    }
  }
  std::shuffle(candidate_slots.begin(), candidate_slots.end(), rng);

  bool is_massive_slot[MAX_GRID_SIZE][MAX_GRID_SIZE] = {};
  for (int i = 0; i < std::min(target_massive_count,
                               static_cast<int>(candidate_slots.size()));
       ++i) {
    is_massive_slot[candidate_slots[i].first][candidate_slots[i].second] = true;
  }

  // 2. Build Room Archetype Pools with Weighted Probabilistic Selection &
  // Seed-Based Flavor Variation
  struct ArchetypeWeight {
    RoomShapeType type;
    int weight;
    int max_count;
  };

  std::vector<ArchetypeWeight> archetype_weights;
  std::vector<RoomShapeType> guaranteed_types;

  if (m_sector_index <= 1) {
    // Sector 1: Natural caverns, geodes, quarries, grottos, crystal cathedrals,
    // vast hollows, silos & precursor sanctum vaults
    archetype_weights = {{RoomShapeType::ColossalVaultedDredgeCathedral, 16, 2},
                         {RoomShapeType::BioluminescentFirmamentAbyss, 16, 2},
                         {RoomShapeType::PrecursorSanctumVault, 16, 2},
                         {RoomShapeType::MiningPillarHall, 16, 2},
                         {RoomShapeType::TerracedQuarry, 15, 2},
                         {RoomShapeType::CrystallineGeode, 15, 2},
                         {RoomShapeType::SubterraneanAquiferOasis, 15, 2},
                         {RoomShapeType::CyclopeanExcavationSilo, 14, 2},
                         {RoomShapeType::PrismaticCrystalCathedral, 14, 2},
                         {RoomShapeType::BioluminescentGlowwormGrotto, 14, 2},
                         {RoomShapeType::FungoidBioGrotto, 13, 2},
                         {RoomShapeType::CrumblingArchCanyon, 12, 2},
                         {RoomShapeType::AncientTitanNecropolis, 10, 2}};
    // Guarantee at least 1 soaring vertical cathedral/firmament and 1
    // resource/crystalline cavern
    guaranteed_types.push_back(
        (rng() % 2 == 0) ? RoomShapeType::ColossalVaultedDredgeCathedral
                         : RoomShapeType::BioluminescentFirmamentAbyss);
    guaranteed_types.push_back((rng() % 2 == 0)
                                   ? RoomShapeType::MiningPillarHall
                                   : RoomShapeType::PrecursorSanctumVault);
  } else if (m_sector_index == 2) {
    // Sector 2: Industrial silos, tectonic sinkholes, cathedrals, magma
    // foundries, toxic miasma, ancient titan ruins, vault bastions
    archetype_weights = {{RoomShapeType::ColossalVaultedDredgeCathedral, 14, 2},
                         {RoomShapeType::TectonicAbyssalSinkhole, 14, 2},
                         {RoomShapeType::SubterraneanVaultBastion, 14, 2},
                         {RoomShapeType::CyclopeanExcavationSilo, 14, 2},
                         {RoomShapeType::BioluminescentFirmamentAbyss, 13, 2},
                         {RoomShapeType::ColossalAbyssalChasm, 13, 2},
                         {RoomShapeType::IndustrialVaultBunker, 12, 2},
                         {RoomShapeType::PrecursorSanctumVault, 12, 2},
                         {RoomShapeType::MoltenMagmaFoundry, 12, 2},
                         {RoomShapeType::ToxicMiasmaSwamp, 12, 2},
                         {RoomShapeType::AncientTitanNecropolis, 12, 2},
                         {RoomShapeType::PrismaticCrystalCathedral, 11, 2},
                         {RoomShapeType::FaultLineCrevasse, 11, 2},
                         {RoomShapeType::PrecursorCoolantReservoir, 11, 2},
                         {RoomShapeType::MagmaCalderaLake, 10, 2},
                         {RoomShapeType::SubterraneanAquiferOasis, 10, 2},
                         {RoomShapeType::SpikeTrenchArena, 10, 2},
                         {RoomShapeType::CrumblingArchCanyon, 10, 2},
                         {RoomShapeType::LaserDefenseFoundry, 10, 2},
                         {RoomShapeType::BioluminescentGlowwormGrotto, 9, 2},
                         {RoomShapeType::FungoidBioGrotto, 8, 2}};
    // Guarantee at least 1 vast vertical landmark and 1 industrial/hazard arena
    guaranteed_types.push_back(
        (rng() % 2 == 0) ? RoomShapeType::ColossalVaultedDredgeCathedral
                         : RoomShapeType::TectonicAbyssalSinkhole);
    // Guarantee industrial bunker/bastion or hazard room for sector requirements
    guaranteed_types.push_back((rng() % 2 == 0) ? RoomShapeType::SubterraneanVaultBastion
                                                : RoomShapeType::MoltenMagmaFoundry);
    RoomShapeType s2_hazards[] = {RoomShapeType::MagmaCalderaLake,
                                  RoomShapeType::ToxicMiasmaSwamp,
                                  RoomShapeType::ColossalAbyssalChasm,
                                  RoomShapeType::SpikeTrenchArena};
    guaranteed_types.push_back(s2_hazards[rng() % 4]);
  } else {
    // Sector 3+: Tectonic sinkholes, vaulted cathedrals, abyssal chasms,
    // radioactive sanctuaries, void rifts, abyssal vault chasms
    archetype_weights = {{RoomShapeType::TectonicAbyssalSinkhole, 16, 3},
                         {RoomShapeType::ColossalVaultedDredgeCathedral, 15, 3},
                         {RoomShapeType::AbyssalVaultChasm, 16, 3},
                         {RoomShapeType::ColossalAbyssalChasm, 15, 3},
                         {RoomShapeType::BioluminescentFirmamentAbyss, 14, 3},
                         {RoomShapeType::CyclopeanExcavationSilo, 14, 3},
                         {RoomShapeType::VoidSingularityRift, 14, 3},
                         {RoomShapeType::RadioactiveCoreSanctuary, 14, 3},
                         {RoomShapeType::SubterraneanVaultBastion, 13, 2},
                         {RoomShapeType::AbyssalVerticalChasm, 13, 3},
                         {RoomShapeType::AncientTitanNecropolis, 13, 2},
                         {RoomShapeType::ToxicMiasmaSwamp, 12, 2},
                         {RoomShapeType::MoltenMagmaFoundry, 12, 2},
                         {RoomShapeType::PrecursorCoolantReservoir, 12, 2},
                         {RoomShapeType::PrismaticCrystalCathedral, 12, 2},
                         {RoomShapeType::MagmaCalderaLake, 10, 2},
                         {RoomShapeType::SpikeTrenchArena, 10, 2},
                         {RoomShapeType::LaserDefenseFoundry, 10, 2},
                         {RoomShapeType::IndustrialVaultBunker, 8, 2},
                         {RoomShapeType::FaultLineCrevasse, 8, 2},
                         {RoomShapeType::SubterraneanAquiferOasis, 8, 2}};
    // Guarantee Sector 3 core archetypes + hazard
    guaranteed_types.push_back((rng() % 2 == 0)
                                   ? RoomShapeType::TectonicAbyssalSinkhole
                                   : RoomShapeType::ColossalAbyssalChasm);
    guaranteed_types.push_back(RoomShapeType::VoidSingularityRift);
    guaranteed_types.push_back((rng() % 2 == 0)
                                   ? RoomShapeType::AbyssalVerticalChasm
                                   : RoomShapeType::RadioactiveCoreSanctuary);
    RoomShapeType s3_hazards[] = {
        RoomShapeType::MoltenMagmaFoundry, RoomShapeType::ToxicMiasmaSwamp,
        RoomShapeType::AncientTitanNecropolis,
        RoomShapeType::ColossalVaultedDredgeCathedral};
    guaranteed_types.push_back(s3_hazards[rng() % 4]);
  }

  // 3. Grid Room Instantiation with Constraint-Driven Adjacency Solver & Thematic Affinities
  std::vector<RoomPlacement> grid_rooms(m_grid_w * m_grid_d);
  std::vector<std::pair<int, int>> regular_slots;
  std::unordered_map<int, int> placed_counts;

  // Initialize all grid rooms
  for (int gx = 0; gx < m_grid_w; ++gx) {
    for (int gz = 0; gz < m_grid_d; ++gz) {
      int idx = gx * m_grid_d + gz;
      grid_rooms[idx].grid_x = gx;
      grid_rooms[idx].grid_z = gz;
      grid_rooms[idx].type = static_cast<RoomShapeType>(-1); // Sentinel unassigned
    }
  }

  // Fixed Anchor Rooms: Spawn at (0, 0), Extraction at (grid_w-1, grid_d-1)
  {
    RoomPlacement &spawn_room = grid_rooms[0];
    spawn_room.type = RoomShapeType::SpawnStagingCavern;
    spawn_room.floor_level = 0;
    spawn_room.floor_y = 4;
    spawn_room.ceiling_y = 25;
    placed_counts[static_cast<int>(RoomShapeType::SpawnStagingCavern)]++;

    RoomPlacement &evac_room =
        grid_rooms[(m_grid_w - 1) * m_grid_d + (m_grid_d - 1)];
    evac_room.type = RoomShapeType::ExtractionLandingBay;
    evac_room.floor_level = 2;
    evac_room.floor_y = 4;
    evac_room.ceiling_y = 25;
    placed_counts[static_cast<int>(RoomShapeType::ExtractionLandingBay)]++;
  }

  // Gather variable slots and order them by wavefront (Manhattan distance from spawn)
  std::vector<std::pair<int, int>> variable_slots;
  for (int gx = 0; gx < m_grid_w; ++gx) {
    for (int gz = 0; gz < m_grid_d; ++gz) {
      if ((gx == 0 && gz == 0) ||
          (gx == m_grid_w - 1 && gz == m_grid_d - 1))
        continue;
      variable_slots.push_back({gx, gz});
    }
  }

  // Sort slots by Manhattan distance from (0, 0) with deterministic hash tie-breaking
  std::sort(variable_slots.begin(), variable_slots.end(),
            [&](const std::pair<int, int> &a, const std::pair<int, int> &b) {
              int distA = a.first + a.second;
              int distB = b.first + b.second;
              if (distA != distB)
                return distA < distB;
              uint32_t hA = hash_coord(a.first, a.second, m_seed, 55);
              uint32_t hB = hash_coord(b.first, b.second, m_seed, 55);
              return hA < hB;
            });

  for (const auto &slot : variable_slots) {
    int gx = slot.first;
    int gz = slot.second;
    int idx = gx * m_grid_d + gz;
    RoomPlacement &room = grid_rooms[idx];

    // Identify already-placed orthogonal and diagonal neighbors
    std::vector<RoomShapeType> placed_ortho_types;
    std::vector<RoomShapeType> placed_diag_types;

    const int ortho_dx[] = {1, -1, 0, 0};
    const int ortho_dz[] = {0, 0, 1, -1};
    for (int d = 0; d < 4; ++d) {
      int nx = gx + ortho_dx[d];
      int nz = gz + ortho_dz[d];
      if (nx >= 0 && nx < m_grid_w && nz >= 0 && nz < m_grid_d) {
        RoomShapeType nt = grid_rooms[nx * m_grid_d + nz].type;
        if (static_cast<int>(nt) != -1) {
          placed_ortho_types.push_back(nt);
        }
      }
    }

    const int diag_dx[] = {1, 1, -1, -1};
    const int diag_dz[] = {1, -1, 1, -1};
    for (int d = 0; d < 4; ++d) {
      int nx = gx + diag_dx[d];
      int nz = gz + diag_dz[d];
      if (nx >= 0 && nx < m_grid_w && nz >= 0 && nz < m_grid_d) {
        RoomShapeType nt = grid_rooms[nx * m_grid_d + nz].type;
        if (static_cast<int>(nt) != -1) {
          placed_diag_types.push_back(nt);
        }
      }
    }

    bool is_spawn_adjacent = (std::abs(gx - 0) + std::abs(gz - 0) <= 1);
    bool is_extraction_adjacent =
        (std::abs(gx - (m_grid_w - 1)) + std::abs(gz - (m_grid_d - 1)) <= 1);
    bool is_massive = is_massive_slot[gx][gz];

    struct CandidateScore {
      RoomShapeType type;
      float weight;
    };
    std::vector<CandidateScore> valid_candidates;

    auto evaluate_candidate = [&](const ArchetypeWeight &aw, int pass) -> float {
      RoomShapeType t = aw.type;

      // RULE 1: STRICT - Two rooms of the same type are NEVER next to each other!
      for (RoomShapeType ont : placed_ortho_types) {
        if (t == ont)
          return -1.0f; // STRICTLY FORBIDDEN
      }

      // Diagonal same-type prohibition in Pass 1
      if (pass == 1) {
        for (RoomShapeType dnt : placed_diag_types) {
          if (t == dnt)
            return -1.0f;
        }
      }

      // Prohibit terminal types
      if (t == RoomShapeType::SpawnStagingCavern ||
          t == RoomShapeType::ExtractionLandingBay) {
        return -1.0f;
      }

      // Spawn proximity safety: Never place extreme hazards or dedicated vault chambers adjacent to spawn!
      if (is_spawn_adjacent && (is_extreme_spawn_hazard(t) || is_vault_room_type(t))) {
        return -1.0f;
      }

      // Global count limits
      int cur_count = placed_counts[static_cast<int>(t)];
      int max_allowed = aw.max_count;
      if (pass >= 2)
        max_allowed += 1;
      if (cur_count >= max_allowed && pass < 3) {
        return -1.0f;
      }

      // Harsh hazard limit: No two harsh hazards of the same category, and limit harsh hazard clustering
      if (is_harsh_hazard_room(t)) {
        int harsh_count = 0;
        RoomCategory my_cat = get_room_category(t);
        for (RoomShapeType ont : placed_ortho_types) {
          if (is_harsh_hazard_room(ont)) {
            harsh_count++;
            if (get_room_category(ont) == my_cat) {
              return -1.0f; // Cannot place 2 harsh hazards of the same category adjacent!
            }
          }
        }
        if (harsh_count >= 1 && pass == 1) {
          return -1.0f; // Limit to max 1 adjacent harsh hazard in Pass 1
        }
      }

      // Category clustering limit in Pass 1:
      if (pass == 1) {
        int same_cat_count = 0;
        RoomCategory my_cat = get_room_category(t);
        for (RoomShapeType ont : placed_ortho_types) {
          if (get_room_category(ont) == my_cat) {
            same_cat_count++;
          }
        }
        if (same_cat_count >= 2) {
          return -1.0f; // Don't surround by 2+ rooms of the same category
        }
      }

      // Calculate weight with contextual adjacency affinities
      float w = static_cast<float>(aw.weight);

      // Priority for guaranteed types not yet placed
      for (RoomShapeType gt : guaranteed_types) {
        if (t == gt && cur_count == 0) {
          w += 40.0f;
          break;
        }
      }

      // Diversity / novelty bonus for rooms not yet present in the level
      if (cur_count == 0) {
        w += 14.0f;
      }

      // Massive slot preference
      if (is_massive && is_colossal_or_massive_room(t)) {
        w += 16.0f;
      }

      // Extraction climax affinity
      if (is_extraction_adjacent && is_extraction_climax_room(t)) {
        w += 15.0f;
      }

      // Contextual adjacency affinity with already-placed neighbors!
      for (RoomShapeType ont : placed_ortho_types) {
        w += get_room_adjacency_affinity(t, ont);
      }
      for (RoomShapeType dnt : placed_diag_types) {
        w += 0.4f * get_room_adjacency_affinity(t, dnt);
      }

      return std::max(1.0f, w);
    };

    // Try Pass 1 (Strict), then Pass 2 (Relaxed), then Pass 3 (Guarantee)
    for (int pass = 1; pass <= 3 && valid_candidates.empty(); ++pass) {
      for (const auto &aw : archetype_weights) {
        float score = evaluate_candidate(aw, pass);
        if (score > 0.0f) {
          valid_candidates.push_back({aw.type, score});
        }
      }
    }

    // Safety fallback: if somehow still empty, pick any type not equal to any placed orthogonal neighbor
    if (valid_candidates.empty()) {
      for (const auto &aw : archetype_weights) {
        bool match = false;
        for (RoomShapeType ont : placed_ortho_types) {
          if (aw.type == ont) {
            match = true;
            break;
          }
        }
        if (!match && aw.type != RoomShapeType::SpawnStagingCavern &&
            aw.type != RoomShapeType::ExtractionLandingBay) {
          valid_candidates.push_back({aw.type, static_cast<float>(aw.weight)});
        }
      }
    }

    // Weighted random selection
    float total_w = 0.0f;
    for (const auto &c : valid_candidates)
      total_w += c.weight;
    float roll = std::uniform_real_distribution<float>(0.0f, total_w)(rng);
    RoomShapeType chosen = valid_candidates.front().type;
    float accum = 0.0f;
    for (const auto &c : valid_candidates) {
      accum += c.weight;
      if (roll <= accum) {
        chosen = c.type;
        break;
      }
    }

    room.type = chosen;
    placed_counts[static_cast<int>(chosen)]++;
  }

  // Classify vertical tiers, massive rooms and regular slots
  for (int gx = 0; gx < m_grid_w; ++gx) {
    for (int gz = 0; gz < m_grid_d; ++gz) {
      int idx = gx * m_grid_d + gz;
      RoomPlacement &room = grid_rooms[idx];
      if ((gx == 0 && gz == 0) ||
          (gx == m_grid_w - 1 && gz == m_grid_d - 1)) {
        continue;
      }

      if (is_massive_slot[gx][gz] || is_colossal_or_massive_room(room.type)) {
        room.is_massive = true;
        // Mega-cavern status for key massive chambers or colossal chasms
        if (is_massive_slot[gx][gz] &&
            (room.type == RoomShapeType::ColossalVaultedDredgeCathedral ||
             room.type == RoomShapeType::TectonicAbyssalSinkhole ||
             room.type == RoomShapeType::CyclopeanExcavationSilo ||
             room.type == RoomShapeType::BioluminescentFirmamentAbyss ||
             room.type == RoomShapeType::ColossalAbyssalChasm ||
             room.type == RoomShapeType::PrismaticCrystalCathedral ||
             room.type == RoomShapeType::AncientTitanNecropolis ||
             room.type == RoomShapeType::SubterraneanAquiferOasis ||
             m_sector_index >= 2)) {
          room.is_mega = true;
        }
        if (room.type == RoomShapeType::FaultLineCrevasse ||
            room.type == RoomShapeType::CrumblingArchCanyon ||
            room.type == RoomShapeType::SpikeTrenchArena) {
          room.floor_level = 1;
          room.floor_y = 14;
        } else {
          room.floor_level = 2;
          room.floor_y = 4;
        }
        room.ceiling_y = 25;
      } else {
        regular_slots.push_back({gx, gz});
      }
    }
  }

  // Assign organic floor clusters for regular rooms (Elevation Biomes)
  if (!regular_slots.empty()) {
    int anchor0_idx = static_cast<int>(rng() % regular_slots.size());
    for (size_t i = 0; i < regular_slots.size(); ++i) {
      int candidate_idx =
          regular_slots[i].first * m_grid_d + regular_slots[i].second;
      if (grid_rooms[candidate_idx].type != RoomShapeType::FaultLineCrevasse &&
          grid_rooms[candidate_idx].type !=
              RoomShapeType::CrumblingArchCanyon &&
          grid_rooms[candidate_idx].type != RoomShapeType::SpikeTrenchArena) {
        anchor0_idx = static_cast<int>(i);
        break;
      }
    }
    auto anchor0 = regular_slots[anchor0_idx];
    auto anchor1 = regular_slots[0];
    int max_dist = -1;
    for (const auto &s : regular_slots) {
      int d = std::abs(s.first - anchor0.first) +
              std::abs(s.second - anchor0.second);
      if (d > max_dist) {
        max_dist = d;
        anchor1 = s;
      }
    }

    int count_floor0 = 0;
    int count_floor1 = 0;

    for (const auto &s : regular_slots) {
      int gx = s.first;
      int gz = s.second;
      int r_idx = gx * m_grid_d + gz;
      RoomPlacement &room = grid_rooms[r_idx];

      int d0 = std::abs(gx - anchor0.first) + std::abs(gz - anchor0.second);
      int d1 = std::abs(gx - anchor1.first) + std::abs(gz - anchor1.second);
      int jitter = static_cast<int>(rng() % 3) - 1; // -1, 0, +1

      if (room.type == RoomShapeType::FaultLineCrevasse ||
          room.type == RoomShapeType::CrumblingArchCanyon ||
          room.type == RoomShapeType::SpikeTrenchArena) {
        // Drop, canyon, and sunken trench chambers are always elevated to Floor
        // 1 (y=14) so their chasms, crevasses, and pits plunge 10 blocks deep
        // down to bedrock crust (y=4), eliminating shallow 1-block ditches!
        room.floor_level = 1;
      } else if (gx == anchor0.first && gz == anchor0.second) {
        room.floor_level = 0;
      } else if (gx == anchor1.first && gz == anchor1.second) {
        room.floor_level = 1;
      } else if ((d0 - d1 + jitter) <= 0) {
        room.floor_level = 0;
      } else {
        room.floor_level = 1;
      }

      if (room.floor_level == 0) {
        room.floor_y = 4;
        // High-Vault Atrium variation: 40% chance of soaring 20-24m ceiling
        // even in regular rooms!
        uint32_t ceil_roll = hash_coord(gx, gz, m_seed, 881) % 100;
        if (ceil_roll < 40) {
          room.ceiling_y =
              20 + static_cast<int>(rng() % 5); // 20 to 24m high ceiling!
        } else {
          room.ceiling_y =
              13 + static_cast<int>(rng() % 4); // 13 to 16m ceiling
        }
        count_floor0++;
      } else {
        room.floor_level = 1;
        room.floor_y = 14;
        room.ceiling_y = 25;
        count_floor1++;
      }
    }

    // Invariant guard: ensure at least one Floor 0 exists among regular
    // non-drop rooms
    if (count_floor0 == 0 && !regular_slots.empty()) {
      for (const auto &s : regular_slots) {
        int r_candidate = s.first * m_grid_d + s.second;
        if (grid_rooms[r_candidate].type != RoomShapeType::FaultLineCrevasse &&
            grid_rooms[r_candidate].type !=
                RoomShapeType::CrumblingArchCanyon &&
            grid_rooms[r_candidate].type != RoomShapeType::SpikeTrenchArena) {
          grid_rooms[r_candidate].floor_level = 0;
          grid_rooms[r_candidate].floor_y = 4;
          grid_rooms[r_candidate].ceiling_y = 13;
          count_floor0++;
          break;
        }
      }
    }
    if (count_floor1 == 0 && !regular_slots.empty()) {
      int r_idx = anchor1.first * m_grid_d + anchor1.second;
      grid_rooms[r_idx].floor_level = 1;
      grid_rooms[r_idx].floor_y = 14;
      grid_rooms[r_idx].ceiling_y = 25;
    }
  }

  // Enforce elevation diversity: ensure at least one orthogonally connected pair of rooms
  // has differing floor_y (e.g., floor_y = 4 and floor_y = 14) so a sloping ramp is guaranteed!
  bool has_elevation_transition = false;
  for (int gz = 0; gz < m_grid_d && !has_elevation_transition; ++gz) {
    for (int gx = 0; gx < m_grid_w - 1; ++gx) {
      if (grid_rooms[gx * m_grid_d + gz].floor_y != grid_rooms[(gx + 1) * m_grid_d + gz].floor_y) {
        has_elevation_transition = true;
        break;
      }
    }
  }
  for (int gx = 0; gx < m_grid_w && !has_elevation_transition; ++gx) {
    for (int gz = 0; gz < m_grid_d - 1; ++gz) {
      if (grid_rooms[gx * m_grid_d + gz].floor_y != grid_rooms[gx * m_grid_d + gz + 1].floor_y) {
        has_elevation_transition = true;
        break;
      }
    }
  }

  if (!has_elevation_transition) {
    int target_gx = 0;
    int target_gz = 1;
    if (m_grid_d <= 1) {
      target_gx = 1;
      target_gz = 0;
    }
    int t_idx = target_gx * m_grid_d + target_gz;
    grid_rooms[t_idx].floor_level = 1;
    grid_rooms[t_idx].floor_y = 14;
    grid_rooms[t_idx].ceiling_y = 25;
  }

  // Finalize room geometry, dimensions & center placement
  bool has_small_generated = false;
  int fallback_small_room_idx = -1;
  for (int gx = 0; gx < m_grid_w; ++gx) {
    for (int gz = 0; gz < m_grid_d; ++gz) {
      int idx = gx * m_grid_d + gz;
      RoomPlacement room = grid_rooms[idx];

      // Room Dimensions & Non-Square Variation:
      if (gx == 0 && gz == 0) {
        // Preserve standard spawn staging cavern bounds
        room.half_width = 8;
        room.half_depth = 8;
      } else if (gx == m_grid_w - 1 && gz == m_grid_d - 1) {
        // Preserve extraction landing bay bounds
        room.half_width = 8;
        room.half_depth = 8;
      } else if (room.is_mega || room.is_massive) {
        // Massive cavern horizontal expanse (17x17 to 19x19), leaving 3-4 block
        // corridor clearance
        room.half_width = 8 + static_cast<int>(rng() % 2); // 8 to 9
        room.half_depth = 8 + static_cast<int>(rng() % 2); // 8 to 9
        if (rng() % 2 == 0)
          std::swap(room.half_width, room.half_depth);
      } else if (room.type == RoomShapeType::SpikeTrenchArena ||
                 room.type == RoomShapeType::FaultLineCrevasse ||
                 room.type == RoomShapeType::CrumblingArchCanyon) {
        // Chasms, canyons and trench arenas need medium-to-large expanse for
        // pits and bridges
        room.half_width = 7 + static_cast<int>(rng() % 2); // 7 to 8
        room.half_depth = 7 + static_cast<int>(rng() % 2); // 7 to 8
      } else {
        int sz = static_cast<int>(rng() % 3);
        if (!has_small_generated) {
          sz = 0;
          has_small_generated = true;
        }
        if (sz == 0) {
          // Small tight niche / outpost (guarantees has_small)
          room.half_width = 5 + static_cast<int>(rng() % 2); // 5 to 6
          room.half_depth = 5 + static_cast<int>(rng() % 3); // 5 to 7
        } else if (sz == 1) {
          // Medium chamber
          room.half_width = 7 + static_cast<int>(rng() % 2); // 7 to 8
          room.half_depth = 7 + static_cast<int>(rng() % 3); // 7 to 9
        } else {
          // Large hall / cavern
          room.half_width = 8 + static_cast<int>(rng() % 2); // 8 to 9
          room.half_depth = 8 + static_cast<int>(rng() % 2); // 8 to 9
        }
        // Organic aspect ratio asymmetry: swap width and depth randomly
        if (rng() % 2 == 0) {
          std::swap(room.half_width, room.half_depth);
        }
      }

      // Grid-aligned room centers guarantee straight corridor linkages and
      // unobstructed doorways
      room.center = glm::ivec3(grid_coords[gx], 0, grid_coords[gz]);

      // Enforce clearance with outer bedrock perimeter walls (x <= 3, x >=
      // world_width() - 4)
      room.half_width = std::min(
          room.half_width,
          std::min(room.center.x - 4, world_width() - 5 - room.center.x));
      room.half_depth = std::min(
          room.half_depth,
          std::min(room.center.z - 4, world_depth() - 5 - room.center.z));

      // Determine Dark Zone status based on Sector & Room Type:
      // Spawn and Extraction are always illuminated
      if ((gx == 0 && gz == 0) || (gx == m_grid_w - 1 && gz == m_grid_d - 1)) {
        room.is_dark_zone = false;
        room.darkness_factor = 0.0f;
      } else {
        uint32_t dark_hash = hash_coord(gx, gz, m_seed, 941);
        int dark_roll = static_cast<int>(dark_hash % 100);

        if (m_sector_index <= 1) {
          // Sector 1 (Perimeter Drift): Mostly operational lighting (~14% unpowered corner pockets)
          if (dark_roll < 14) {
            room.is_dark_zone = true;
            room.darkness_factor = 0.80f;
          } else {
            room.is_dark_zone = false;
            room.darkness_factor = 0.0f;
          }
        } else if (m_sector_index == 2) {
          // Sector 2 (Volatile Fault): Moderate infrastructure degradation (~30-45% dark zones)
          bool is_hazard_room = is_harsh_hazard_room(room.type) ||
                                room.type == RoomShapeType::FaultLineCrevasse ||
                                room.type == RoomShapeType::CrumblingArchCanyon ||
                                room.type == RoomShapeType::SpikeTrenchArena;
          int threshold = is_hazard_room ? 45 : 30;
          if (dark_roll < threshold) {
            room.is_dark_zone = true;
            room.darkness_factor = 0.85f + 0.10f * static_cast<float>((dark_hash >> 8) % 100) / 100.0f;
          } else {
            room.is_dark_zone = false;
            room.darkness_factor = 0.0f;
          }
        } else {
          // Sector 3+ (Void Cradle & deep late sectors): Deep abyssal blackouts (~45-65% dark zones)
          bool is_abyss_or_hazard = room.type == RoomShapeType::AbyssalVerticalChasm ||
                                    room.type == RoomShapeType::ColossalAbyssalChasm ||
                                    room.type == RoomShapeType::TectonicAbyssalSinkhole ||
                                    room.type == RoomShapeType::VoidSingularityRift ||
                                    room.type == RoomShapeType::AncientTitanNecropolis ||
                                    is_harsh_hazard_room(room.type);
          int threshold = is_abyss_or_hazard ? 65 : 45;
          if (dark_roll < threshold) {
            room.is_dark_zone = true;
            room.darkness_factor = 0.90f + 0.08f * static_cast<float>((dark_hash >> 8) % 100) / 100.0f;
          } else {
            room.is_dark_zone = false;
            room.darkness_factor = 0.0f;
          }
        }
      }

      int room_idx = static_cast<int>(m_rooms.size());
      m_grid_room_indices[gx][gz] = room_idx;
      m_rooms.push_back(room);
    }
  }

  // 3. Connect Rooms with Guaranteed Reachability & Diverse Corridors
  // Primary Orthogonal Corridors (Horizontal X axis)
  for (int gz = 0; gz < m_grid_d; ++gz) {
    for (int gx = 0; gx < m_grid_w - 1; ++gx) {
      int rA_idx = m_grid_room_indices[gx][gz];
      int rB_idx = m_grid_room_indices[gx + 1][gz];
      if (rA_idx < 0 || rB_idx < 0)
        continue;
      RoomPlacement &roomA = m_rooms[rA_idx];
      RoomPlacement &roomB = m_rooms[rB_idx];

      CorridorPlacement c;
      c.is_x_axis = true;
      c.is_diagonal = false;

      int start_x = roomA.center.x + roomA.half_width - 1;
      int end_x = roomB.center.x - roomB.half_width + 1;
      if (start_x >= end_x) {
        start_x = roomA.center.x + 3;
        end_x = roomB.center.x - 3;
      }
      if (roomA.floor_y != roomB.floor_y) {
        int min_run = std::abs(roomB.floor_y - roomA.floor_y) + 2;
        while ((end_x - start_x) < min_run) {
          if (start_x > roomA.center.x + 1)
            start_x--;
          else if (end_x < roomB.center.x - 1)
            end_x++;
          else
            break;
        }
      }
      c.start_pos = glm::ivec3(start_x, roomA.floor_y, roomA.center.z);
      c.end_pos = glm::ivec3(end_x, roomB.floor_y, roomB.center.z);

      if (roomA.floor_y != roomB.floor_y) {
        // Stepped ramp / staircase transitioning between floors
        c.type = CorridorType::SlopingRamp;
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomB.floor_y;
        c.ceiling_y = roomA.floor_y + 8;
        c.end_ceiling_y = roomB.floor_y + 8;
        c.half_width = 3;
      } else {
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomA.floor_y;
        c.ceiling_y = (roomA.floor_y <= 4 && roomB.floor_y <= 4) ? 13 : 23;
        c.end_ceiling_y = c.ceiling_y;

        if (roomA.floor_y >= 8 || roomB.floor_y >= 8 || rng() % 100 < 30) {
          // Wide jagged ravine
          c.type = CorridorType::Ravine;
          c.is_ravine = true;
          c.half_width = 4;
        } else {
          c.type = CorridorType::Standard;
          c.half_width = 2 + static_cast<int>(rng() % 2); // 2 to 3
        }
      }

      if (m_sector_index >= 2 && (rng() % 100 < 20)) {
        c.has_bulkhead_door = true;
      } else if (m_sector_index >= 2 && (rng() % 100 < 15)) {
        c.has_rubble_collapse = true;
      }

      m_corridors.push_back(c);
      roomA.connected_east = true;
      roomB.connected_west = true;
    }
  }

  // Primary Orthogonal Corridors (Vertical Z axis)
  for (int gx = 0; gx < m_grid_w; ++gx) {
    for (int gz = 0; gz < m_grid_d - 1; ++gz) {
      int rA_idx = m_grid_room_indices[gx][gz];
      int rB_idx = m_grid_room_indices[gx][gz + 1];
      if (rA_idx < 0 || rB_idx < 0)
        continue;
      RoomPlacement &roomA = m_rooms[rA_idx];
      RoomPlacement &roomB = m_rooms[rB_idx];

      CorridorPlacement c;
      c.is_x_axis = false;
      c.is_diagonal = false;

      int start_z = roomA.center.z + roomA.half_depth - 1;
      int end_z = roomB.center.z - roomB.half_depth + 1;
      if (start_z >= end_z) {
        start_z = roomA.center.z + 3;
        end_z = roomB.center.z - 3;
      }
      if (roomA.floor_y != roomB.floor_y) {
        int min_run = std::abs(roomB.floor_y - roomA.floor_y) + 2;
        while ((end_z - start_z) < min_run) {
          if (start_z > roomA.center.z + 1)
            start_z--;
          else if (end_z < roomB.center.z - 1)
            end_z++;
          else
            break;
        }
      }
      c.start_pos = glm::ivec3(roomA.center.x, roomA.floor_y, start_z);
      c.end_pos = glm::ivec3(roomB.center.x, roomB.floor_y, end_z);

      if (roomA.floor_y != roomB.floor_y) {
        c.type = CorridorType::SlopingRamp;
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomB.floor_y;
        c.ceiling_y = roomA.floor_y + 8;
        c.end_ceiling_y = roomB.floor_y + 8;
        c.half_width = 3;
      } else {
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomA.floor_y;
        c.ceiling_y = (roomA.floor_y <= 4 && roomB.floor_y <= 4) ? 13 : 23;
        c.end_ceiling_y = c.ceiling_y;

        if (roomA.floor_y >= 8 || roomB.floor_y >= 8 || rng() % 100 < 30) {
          c.type = CorridorType::Ravine;
          c.is_ravine = true;
          c.half_width = 4;
        } else {
          c.type = CorridorType::Standard;
          c.half_width = 2 + static_cast<int>(rng() % 2);
        }
      }

      if (m_sector_index >= 3 && (rng() % 100 < 20)) {
        c.has_bulkhead_door = true;
      } else if (m_sector_index >= 2 && (rng() % 100 < 15)) {
        c.has_rubble_collapse = true;
      }

      m_corridors.push_back(c);
      roomA.connected_north = true;
      roomB.connected_south = true;
    }
  }

  // 4. Diagonal Corridors & Natural Ravines
  // North-East Diagonal (+X, +Z)
  for (int gx = 0; gx < m_grid_w - 1; ++gx) {
    for (int gz = 0; gz < m_grid_d - 1; ++gz) {
      // Sector 1: guaranteed diagonal artery from (0,0) to (1,1) and (1,1) to
      // (2,2)
      bool should_connect = (m_sector_index <= 1) ? true : (rng() % 100 < 55);
      if (!should_connect)
        continue;

      int rA_idx = m_grid_room_indices[gx][gz];
      int rB_idx = m_grid_room_indices[gx + 1][gz + 1];
      if (rA_idx < 0 || rB_idx < 0)
        continue;
      RoomPlacement &roomA = m_rooms[rA_idx];
      RoomPlacement &roomB = m_rooms[rB_idx];

      CorridorPlacement c;
      c.is_diagonal = true;
      c.is_x_axis = false;

      c.start_pos = glm::ivec3(std::clamp(roomA.center.x + roomA.half_width - 1,
                                          4, world_width() - 5),
                               roomA.floor_y,
                               std::clamp(roomA.center.z + roomA.half_depth - 1,
                                          4, world_depth() - 5));
      c.end_pos = glm::ivec3(std::clamp(roomB.center.x - roomB.half_width + 1,
                                        4, world_width() - 5),
                             roomB.floor_y,
                             std::clamp(roomB.center.z - roomB.half_depth + 1,
                                        4, world_depth() - 5));

      if (roomA.floor_y != roomB.floor_y) {
        c.type = CorridorType::SlopingRamp;
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomB.floor_y;
        c.ceiling_y = roomA.floor_y + 8;
        c.end_ceiling_y = roomB.floor_y + 8;
        c.half_width = 3;
      } else if (rng() % 100 < 50) {
        c.type = CorridorType::Ravine;
        c.is_ravine = true;
        c.half_width = 4;
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomA.floor_y;
        c.ceiling_y = (roomA.floor_y <= 4 && roomB.floor_y <= 4) ? 13 : 23;
        c.end_ceiling_y = c.ceiling_y;
      } else {
        c.type = CorridorType::Diagonal;
        c.half_width = 3;
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomA.floor_y;
        c.ceiling_y = (roomA.floor_y <= 4 && roomB.floor_y <= 4) ? 13 : 23;
        c.end_ceiling_y = c.ceiling_y;
      }

      m_corridors.push_back(c);
      roomA.connected_ne = true;
      roomB.connected_sw = true;
    }
  }

  // South-East Diagonal (+X, -Z)
  for (int gx = 0; gx < m_grid_w - 1; ++gx) {
    for (int gz = 1; gz < m_grid_d; ++gz) {
      // Prevent crossing diagonals: if the NE diagonal in this 2x2 cell is
      // already connected, skip SE
      int ne_origin_idx = m_grid_room_indices[gx][gz - 1];
      if (ne_origin_idx >= 0 && m_rooms[ne_origin_idx].connected_ne)
        continue;

      bool should_connect = (rng() % 100 < 40);
      if (!should_connect)
        continue;

      int rA_idx = m_grid_room_indices[gx][gz];
      int rB_idx = m_grid_room_indices[gx + 1][gz - 1];
      if (rA_idx < 0 || rB_idx < 0)
        continue;
      RoomPlacement &roomA = m_rooms[rA_idx];
      RoomPlacement &roomB = m_rooms[rB_idx];

      CorridorPlacement c;
      c.is_diagonal = true;
      c.is_x_axis = false;

      c.start_pos = glm::ivec3(std::clamp(roomA.center.x + roomA.half_width - 1,
                                          4, world_width() - 5),
                               roomA.floor_y,
                               std::clamp(roomA.center.z - roomA.half_depth + 1,
                                          4, world_depth() - 5));
      c.end_pos = glm::ivec3(std::clamp(roomB.center.x - roomB.half_width + 1,
                                        4, world_width() - 5),
                             roomB.floor_y,
                             std::clamp(roomB.center.z + roomB.half_depth - 1,
                                        4, world_depth() - 5));

      if (roomA.floor_y != roomB.floor_y) {
        c.type = CorridorType::SlopingRamp;
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomB.floor_y;
        c.ceiling_y = roomA.floor_y + 8;
        c.end_ceiling_y = roomB.floor_y + 8;
        c.half_width = 3;
      } else if (rng() % 100 < 50) {
        c.type = CorridorType::Ravine;
        c.is_ravine = true;
        c.half_width = 4;
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomA.floor_y;
        c.ceiling_y = (roomA.floor_y <= 4 && roomB.floor_y <= 4) ? 13 : 23;
        c.end_ceiling_y = c.ceiling_y;
      } else {
        c.type = CorridorType::Diagonal;
        c.half_width = 3;
        c.floor_y = roomA.floor_y;
        c.end_floor_y = roomA.floor_y;
        c.ceiling_y = (roomA.floor_y <= 4 && roomB.floor_y <= 4) ? 13 : 23;
        c.end_ceiling_y = c.ceiling_y;
      }

      m_corridors.push_back(c);
      roomA.connected_se = true;
      roomB.connected_nw = true;
    }
  }

  // 5. Procedural Random Fluid Structure Placement across Caverns, Halls & Chasms
  // Generates diverse contained fluid structures:
  // - ContainedPool: Natural circular/oval basin with decorative rim
  // - SteppingStonePool: Grotto pool with central island and hop-across stepping stones
  // - WallCascade: Small fall / wall niche cascade pouring down into splash basin
  // - CascadingTwinPools: Tiered double terrace pools with weir overflow step
  // - HazardTrench: Dangerous room obstacle trench with crossing stepping stones
  // - DropChasmSump: Plunge pool / hazard sump at the bottom of large drop shafts
  int placed_fluid_count = 0;
  std::vector<int> eligible_room_indices;

  for (size_t r_i = 0; r_i < m_rooms.size(); ++r_i) {
    auto &room = m_rooms[r_i];
    // Exclude arrival pad, extraction bay, and chambers that already feature fixed specialized pools
    if (room.type == RoomShapeType::SpawnStagingCavern ||
        room.type == RoomShapeType::ExtractionLandingBay ||
        room.type == RoomShapeType::SubterraneanAquiferOasis ||
        room.type == RoomShapeType::MagmaCalderaLake ||
        room.type == RoomShapeType::MoltenMagmaFoundry ||
        room.type == RoomShapeType::PrecursorCoolantReservoir ||
        room.type == RoomShapeType::BioluminescentGlowwormGrotto ||
        room.type == RoomShapeType::BioluminescentFirmamentAbyss ||
        room.type == RoomShapeType::VoidSingularityRift ||
        room.type == RoomShapeType::SpikeTrenchArena ||
        room.type == RoomShapeType::FaultLineCrevasse ||
        room.type == RoomShapeType::CrumblingArchCanyon ||
        room.type == RoomShapeType::TectonicAbyssalSinkhole ||
        room.type == RoomShapeType::PrecursorSanctumVault ||
        room.type == RoomShapeType::SubterraneanVaultBastion ||
        room.type == RoomShapeType::AbyssalVaultChasm ||
        room.has_vault) {
      continue;
    }

    // Only place on bedrock ground (floor_y == 4) to guarantee impenetrable bedrock crust underneath and prevent leaks
    if (room.floor_y != 4) {
      continue;
    }

    eligible_room_indices.push_back(static_cast<int>(r_i));

    uint32_t fluid_roll = hash_coord(room.grid_x, room.grid_z, m_seed, 9431) % 100;
    if (fluid_roll < 70) { // 70% chance per eligible room
      std::vector<std::pair<int, int>> safe_quadrants;
      if (!room.connected_ne) safe_quadrants.push_back({1, 1});
      if (!room.connected_nw) safe_quadrants.push_back({-1, 1});
      if (!room.connected_se) safe_quadrants.push_back({1, -1});
      if (!room.connected_sw) safe_quadrants.push_back({-1, -1});
      if (safe_quadrants.empty()) {
        safe_quadrants.push_back({1, 1});
      }

      uint32_t q_idx = hash_coord(room.grid_x, room.grid_z, m_seed, 7719) % safe_quadrants.size();
      auto [sx, sz] = safe_quadrants[q_idx];

      float off_x = std::clamp(static_cast<float>(room.half_width) - 4.5f, 2.0f, 4.0f);
      float off_z = std::clamp(static_cast<float>(room.half_depth) - 4.5f, 2.0f, 4.0f);
      float r_pool = (room.half_width >= 7 && room.half_depth >= 7) ? 1.4f : 0.9f;

      room.has_random_fluid = true;
      room.random_fluid_offset_x = static_cast<float>(sx) * off_x;
      room.random_fluid_offset_z = static_cast<float>(sz) * off_z;
      room.random_fluid_radius = r_pool;
      room.random_fluid_length = (room.half_width >= 8 && room.half_depth >= 8) ? 6.0f : 4.5f;
      room.fluid_orientation = static_cast<int>(hash_coord(room.grid_x, room.grid_z, m_seed, 6113) % 2);

      // Select diverse fluid structure archetype
      uint32_t struct_roll = hash_coord(room.grid_x, room.grid_z, m_seed, 5521) % 100;
      if (room.type == RoomShapeType::AbyssalVerticalChasm ||
          room.type == RoomShapeType::ColossalAbyssalChasm ||
          room.type == RoomShapeType::TectonicAbyssalSinkhole) {
        room.fluid_structure_type = FluidStructureType::DropChasmSump;
        room.random_fluid_offset_x = 0.0f;
        room.random_fluid_offset_z = 0.0f;
        room.random_fluid_radius = std::clamp(static_cast<float>(room.half_width) - 4.5f, 2.5f, 4.5f);
      } else if (room.type == RoomShapeType::IndustrialVaultBunker ||
                 room.type == RoomShapeType::LaserDefenseFoundry ||
                 room.type == RoomShapeType::TerracedQuarry ||
                 room.type == RoomShapeType::ToxicMiasmaSwamp) {
        if (struct_roll < 55) {
          room.fluid_structure_type = FluidStructureType::HazardTrench;
        } else if (struct_roll < 80) {
          room.fluid_structure_type = FluidStructureType::WallCascade;
        } else {
          room.fluid_structure_type = FluidStructureType::ContainedPool;
        }
      } else {
        if (struct_roll < 25) {
          room.fluid_structure_type = FluidStructureType::WallCascade;
        } else if (struct_roll < 50) {
          room.fluid_structure_type = FluidStructureType::CascadingTwinPools;
        } else if (struct_roll < 72) {
          room.fluid_structure_type = FluidStructureType::HazardTrench;
        } else if (struct_roll < 88) {
          room.fluid_structure_type = FluidStructureType::SteppingStonePool;
        } else {
          room.fluid_structure_type = FluidStructureType::ContainedPool;
        }
      }

      // Determine thematic fluid material based on sector and room archetype
      uint32_t mat_roll = hash_coord(room.grid_x, room.grid_z, m_seed, 1047) % 100;
      if (m_sector_index <= 1) {
        if (mat_roll < 75) {
          room.random_fluid_material = MAT_CRYSTAL_AQUIFER;
          room.random_fluid_rim_material = (mat_roll % 2 == 0) ? MAT_BIOLUMINESCENT_FLORA : MAT_VOLCANIC_BASALT;
        } else {
          room.random_fluid_material = MAT_COOLANT;
          room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
        }
      } else if (m_sector_index == 2) {
        if (mat_roll < 45) {
          room.random_fluid_material = MAT_THERMITE_SLAG;
          room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
        } else if (mat_roll < 75) {
          room.random_fluid_material = MAT_COOLANT;
          room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
        } else {
          room.random_fluid_material = MAT_CRYSTAL_AQUIFER;
          room.random_fluid_rim_material = MAT_BIOLUMINESCENT_FLORA;
        }
      } else {
        if (mat_roll < 40) {
          room.random_fluid_material = MAT_ACID;
          room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
        } else if (mat_roll < 70) {
          room.random_fluid_material = MAT_THERMITE_SLAG;
          room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
        } else if (mat_roll < 85) {
          room.random_fluid_material = MAT_COOLANT;
          room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
        } else {
          room.random_fluid_material = MAT_CRYSTAL_AQUIFER;
          room.random_fluid_rim_material = MAT_BIOLUMINESCENT_FLORA;
        }
      }

      // Room archetype flavor overrides
      if (room.type == RoomShapeType::ToxicMiasmaSwamp) {
        room.random_fluid_material = MAT_ACID;
        room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
      } else if (room.type == RoomShapeType::RadioactiveCoreSanctuary) {
        room.random_fluid_material = MAT_ACID;
        room.random_fluid_rim_material = MAT_RADIOACTIVE_ORE;
      } else if (room.type == RoomShapeType::IndustrialVaultBunker || room.type == RoomShapeType::LaserDefenseFoundry) {
        room.random_fluid_material = (mat_roll < 50) ? MAT_COOLANT : MAT_THERMITE_SLAG;
        room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
      }

      placed_fluid_count++;
    }
  }

  // Guarantee at least 2 rooms with random fluids per level across all seeds
  if (placed_fluid_count < 2 && !eligible_room_indices.empty()) {
    for (int r_idx : eligible_room_indices) {
      if (placed_fluid_count >= 2) break;
      auto &room = m_rooms[r_idx];
      if (room.has_random_fluid) continue;

      std::vector<std::pair<int, int>> safe_quadrants;
      if (!room.connected_ne) safe_quadrants.push_back({1, 1});
      if (!room.connected_nw) safe_quadrants.push_back({-1, 1});
      if (!room.connected_se) safe_quadrants.push_back({1, -1});
      if (!room.connected_sw) safe_quadrants.push_back({-1, -1});
      if (safe_quadrants.empty()) {
        safe_quadrants.push_back({1, 1});
      }

      uint32_t q_idx = hash_coord(room.grid_x, room.grid_z, m_seed, 4423) % safe_quadrants.size();
      auto [sx, sz] = safe_quadrants[q_idx];

      float off_x = std::clamp(static_cast<float>(room.half_width) - 3.5f, 2.5f, 5.0f);
      float off_z = std::clamp(static_cast<float>(room.half_depth) - 3.5f, 2.5f, 5.0f);
      float r_pool = (room.half_width >= 7 && room.half_depth >= 7) ? 1.4f : 0.9f;

      room.has_random_fluid = true;
      room.random_fluid_offset_x = static_cast<float>(sx) * off_x;
      room.random_fluid_offset_z = static_cast<float>(sz) * off_z;
      room.random_fluid_radius = r_pool;
      room.random_fluid_length = 5.0f;
      room.fluid_orientation = (r_idx % 2);

      uint32_t struct_roll = hash_coord(room.grid_x, room.grid_z, m_seed, 8831) % 100;
      if (room.type == RoomShapeType::AbyssalVerticalChasm ||
          room.type == RoomShapeType::ColossalAbyssalChasm ||
          room.type == RoomShapeType::TectonicAbyssalSinkhole) {
        room.fluid_structure_type = FluidStructureType::DropChasmSump;
        room.random_fluid_offset_x = 0.0f;
        room.random_fluid_offset_z = 0.0f;
        room.random_fluid_radius = std::clamp(static_cast<float>(room.half_width) - 4.5f, 2.5f, 4.5f);
      } else {
        if (struct_roll < 30) {
          room.fluid_structure_type = FluidStructureType::WallCascade;
        } else if (struct_roll < 60) {
          room.fluid_structure_type = FluidStructureType::CascadingTwinPools;
        } else if (struct_roll < 80) {
          room.fluid_structure_type = FluidStructureType::HazardTrench;
        } else {
          room.fluid_structure_type = FluidStructureType::ContainedPool;
        }
      }

      if (m_sector_index <= 1) {
        room.random_fluid_material = MAT_CRYSTAL_AQUIFER;
        room.random_fluid_rim_material = MAT_BIOLUMINESCENT_FLORA;
      } else if (m_sector_index == 2) {
        room.random_fluid_material = (r_idx % 2 == 0) ? MAT_THERMITE_SLAG : MAT_COOLANT;
        room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
      } else {
        room.random_fluid_material = (r_idx % 2 == 0) ? MAT_ACID : MAT_THERMITE_SLAG;
        room.random_fluid_rim_material = MAT_VOLCANIC_BASALT;
      }

      placed_fluid_count++;
    }
  }

  place_objective_resources();
  validate_and_balance_objectives();
  configure_vault_placement(rng);
  generate_luminaries();
}

void LevelGenerator::generate_luminaries() {
  m_luminaries.clear();
  m_luminaries.reserve(64);

  for (const auto &room : m_rooms) {
    if (room.is_dark_zone) {
      // In dark zones, standard bright cavern luminaries are omitted completely,
      // leaving the subterranean chamber in pitch-black void darkness where torches are required!
      continue;
    }

    switch (room.type) {
    case RoomShapeType::CrystallineGeode: {
      CavernLuminary core;
      core.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 3.0f),
                    room.center.z);
      core.base_color = glm::vec3(0.42f, 0.18f, 0.78f);
      core.base_radius = 15.0f;
      core.base_intensity = 1.7f;
      core.type = LuminaryType::VoiditeCrystalGeode;
      core.pulse_speed = 1.8f;
      core.pulse_depth = 0.18f;
      core.name = "Voidite Geode Heart";
      m_luminaries.push_back(core);

      CavernLuminary high_cluster;
      high_cluster.position = glm::vec3(
          room.center.x + 3.5f, static_cast<float>(room.ceiling_y - 4.0f),
          room.center.z - 3.5f);
      high_cluster.base_color = glm::vec3(0.48f, 0.22f, 0.82f);
      high_cluster.base_radius = 11.0f;
      high_cluster.base_intensity = 1.2f;
      high_cluster.type = LuminaryType::VoiditeCrystalGeode;
      high_cluster.name = "Crystalline Stalactite";
      m_luminaries.push_back(high_cluster);
      break;
    }

    case RoomShapeType::RadioactiveCoreSanctuary: {
      CavernLuminary core;
      core.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 3.0f),
                    room.center.z);
      core.base_color = glm::vec3(0.12f, 0.98f, 0.35f);
      core.base_radius = 5.5f;
      core.base_intensity = 1.5f;
      core.type = LuminaryType::RadioactiveCore;
      core.pulse_speed = 4.0f;
      core.pulse_depth = 0.25f;
      core.flicker_rate = 12.0f;
      core.name = "Radioactive Core Monolith";
      m_luminaries.push_back(core);

      for (int i = 0; i < 2; ++i) {
        float angle = glm::radians(i * 180.0f + 45.0f);
        CavernLuminary vent;
        vent.position = glm::vec3(room.center.x + std::cos(angle) * 4.5f,
                                  static_cast<float>(room.floor_y + 0.8f),
                                  room.center.z + std::sin(angle) * 4.5f);
        vent.base_color = glm::vec3(0.20f, 0.88f, 0.25f);
        vent.base_radius = 4.8f;
        vent.base_intensity = 0.65f;
        vent.type = LuminaryType::RadioactiveCore;
        vent.flicker_rate = 18.0f;
        vent.name = "Radioactive Moat Fissure";
        m_luminaries.push_back(vent);
      }
      break;
    }

    case RoomShapeType::FaultLineCrevasse: {
      CavernLuminary magma1;
      magma1.position =
          glm::vec3(room.center.x - 2.5f, 5.0f, room.center.z - 2.5f);
      magma1.base_color = glm::vec3(1.0f, 0.40f, 0.05f);
      magma1.base_radius = 24.0f;
      magma1.base_intensity = 3.2f;
      magma1.type = LuminaryType::ThermalMagmaVent;
      magma1.pulse_speed = 3.2f;
      magma1.name = "Molten Magma Rift";
      m_luminaries.push_back(magma1);

      CavernLuminary magma2;
      magma2.position =
          glm::vec3(room.center.x + 2.5f, 5.0f, room.center.z + 2.5f);
      magma2.base_color = glm::vec3(0.20f, 0.90f, 0.35f);
      magma2.base_radius = 5.5f;
      magma2.base_intensity = 1.1f;
      magma2.type = LuminaryType::RadioactiveCore;
      magma2.name = "Thermal Crevasse Vent";
      m_luminaries.push_back(magma2);
      break;
    }

    case RoomShapeType::IndustrialVaultBunker: {
      CavernLuminary beacon;
      beacon.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 5.5f),
                    room.center.z);
      beacon.base_color = glm::vec3(1.0f, 0.72f, 0.12f);
      beacon.base_radius = 16.0f;
      beacon.base_intensity = 2.4f;
      beacon.type = LuminaryType::IndustrialVaultBeacon;
      beacon.pulse_speed = 5.0f;
      beacon.pulse_depth = 0.35f;
      beacon.name = "Vault Security Strobe";
      m_luminaries.push_back(beacon);
      break;
    }

    case RoomShapeType::MiningPillarHall: {
      CavernLuminary worklight;
      worklight.position = glm::vec3(room.center.x + 4.0f,
                                     static_cast<float>(room.floor_y + 4.0f),
                                     room.center.z + 4.0f);
      worklight.base_color = glm::vec3(0.98f, 0.74f, 0.32f); // Industrial halogen amber
      worklight.base_radius = 18.0f;
      worklight.base_intensity = 2.2f;
      worklight.type = LuminaryType::WallIndustrialSconce;
      worklight.name = "Pillar Industrial Worklight";
      m_luminaries.push_back(worklight);
      break;
    }

    case RoomShapeType::TerracedQuarry: {
      CavernLuminary quarry;
      quarry.position = glm::vec3(room.center.x - 3.0f,
                                  static_cast<float>(room.floor_y + 3.0f),
                                  room.center.z + 3.0f);
      quarry.base_color = glm::vec3(0.96f, 0.78f, 0.35f); // Sodium quarry floodlight
      quarry.base_radius = 16.0f;
      quarry.base_intensity = 2.0f;
      quarry.type = LuminaryType::WallIndustrialSconce;
      quarry.name = "Quarry Stanchion Worklight";
      m_luminaries.push_back(quarry);
      break;
    }

    case RoomShapeType::AbyssalVerticalChasm: {
      CavernLuminary ledge;
      ledge.position =
          glm::vec3(room.center.x + 4.5f,
                    static_cast<float>(room.floor_y + 10.0f), room.center.z);
      ledge.base_color = glm::vec3(0.25f, 0.70f, 1.0f);
      ledge.base_radius = 20.0f;
      ledge.base_intensity = 2.2f;
      ledge.type = LuminaryType::BioluminescentLedge;
      ledge.name = "Chasm Abyssal Lantern";
      m_luminaries.push_back(ledge);
      break;
    }

    case RoomShapeType::ExtractionLandingBay: {
      CavernLuminary guide;
      guide.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 1.2f),
                    room.center.z);
      guide.base_color = glm::vec3(0.10f, 0.95f, 0.75f);
      guide.base_radius = 18.0f;
      guide.base_intensity = 2.4f;
      guide.type = LuminaryType::ExtractionGuideBeacon;
      guide.name = "Landing Bay Touchdown Guide";
      m_luminaries.push_back(guide);
      break;
    }

    case RoomShapeType::MagmaCalderaLake: {
      CavernLuminary lava_core;
      lava_core.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 0.5f),
                    room.center.z);
      lava_core.base_color = glm::vec3(1.0f, 0.38f, 0.04f);
      lava_core.base_radius = 24.0f;
      lava_core.base_intensity = 3.4f;
      lava_core.type = LuminaryType::ThermalMagmaVent;
      lava_core.pulse_speed = 2.8f;
      lava_core.pulse_depth = 0.22f;
      lava_core.name = "Caldera Magma Heart";
      m_luminaries.push_back(lava_core);

      CavernLuminary vent1;
      vent1.position = glm::vec3(room.center.x + 4.0f,
                                 static_cast<float>(room.floor_y + 1.5f),
                                 room.center.z - 4.0f);
      vent1.base_color = glm::vec3(1.0f, 0.55f, 0.06f);
      vent1.base_radius = 14.0f;
      vent1.base_intensity = 2.0f;
      vent1.type = LuminaryType::ThermalMagmaVent;
      vent1.name = "Caldera Shore Vent";
      m_luminaries.push_back(vent1);
      break;
    }

    case RoomShapeType::SpikeTrenchArena: {
      CavernLuminary warning;
      warning.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 4.5f),
                    room.center.z);
      warning.base_color = glm::vec3(1.0f, 0.22f, 0.10f);
      warning.base_radius = 16.0f;
      warning.base_intensity = 2.2f;
      warning.type = LuminaryType::IndustrialVaultBeacon;
      warning.pulse_speed = 3.5f;
      warning.pulse_depth = 0.25f;
      warning.name = "Spike Trench Hazard Beacon";
      m_luminaries.push_back(warning);
      break;
    }

    case RoomShapeType::VoidSingularityRift: {
      CavernLuminary void_pulse;
      void_pulse.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 5.0f),
                    room.center.z);
      void_pulse.base_color = glm::vec3(0.58f, 0.10f, 0.95f);
      void_pulse.base_radius = 28.0f;
      void_pulse.base_intensity = 3.8f;
      void_pulse.type = LuminaryType::VoidSingularityPulse;
      void_pulse.pulse_speed = 3.0f;
      void_pulse.pulse_depth = 0.30f;
      void_pulse.flicker_rate = 8.0f;
      void_pulse.name = "Void Singularity Heart";
      m_luminaries.push_back(void_pulse);

      CavernLuminary high_aura;
      high_aura.position =
          glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 3.0f),
                    room.center.z);
      high_aura.base_color = glm::vec3(0.40f, 0.05f, 0.85f);
      high_aura.base_radius = 16.0f;
      high_aura.base_intensity = 1.8f;
      high_aura.type = LuminaryType::VoidSingularityPulse;
      high_aura.name = "Void Abyss Crown";
      m_luminaries.push_back(high_aura);
      break;
    }

    case RoomShapeType::FungoidBioGrotto: {
      CavernLuminary spore;
      spore.position = glm::vec3(room.center.x + 3.0f,
                                 static_cast<float>(room.floor_y + 5.0f),
                                 room.center.z + 3.0f);
      spore.base_color = glm::vec3(0.15f, 0.95f, 0.60f);
      spore.base_radius = 18.0f;
      spore.base_intensity = 2.4f;
      spore.type = LuminaryType::FungoidSporeGlow;
      spore.pulse_speed = 1.4f;
      spore.pulse_depth = 0.15f;
      spore.name = "Phosphorescent Spore Bloom";
      m_luminaries.push_back(spore);

      CavernLuminary cap;
      cap.position = glm::vec3(room.center.x - 3.0f,
                               static_cast<float>(room.floor_y + 8.0f),
                               room.center.z - 3.0f);
      cap.base_color = glm::vec3(0.10f, 0.80f, 0.95f);
      cap.base_radius = 15.0f;
      cap.base_intensity = 2.0f;
      cap.type = LuminaryType::FungoidSporeGlow;
      cap.name = "Mushroom Cap Luminary";
      m_luminaries.push_back(cap);
      break;
    }

    case RoomShapeType::LaserDefenseFoundry: {
      CavernLuminary strobe;
      strobe.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 6.0f),
                    room.center.z);
      strobe.base_color = glm::vec3(1.0f, 0.15f, 0.08f);
      strobe.base_radius = 20.0f;
      strobe.base_intensity = 3.0f;
      strobe.type = LuminaryType::LaserSecurityStrobe;
      strobe.pulse_speed = 6.0f;
      strobe.pulse_depth = 0.35f;
      strobe.flicker_rate = 16.0f;
      strobe.name = "Foundry Security Strobe";
      m_luminaries.push_back(strobe);
      break;
    }

    case RoomShapeType::CrumblingArchCanyon: {
      CavernLuminary canyon;
      canyon.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 8.0f),
                    room.center.z);
      canyon.base_color = glm::vec3(0.35f, 0.70f, 0.98f);
      canyon.base_radius = 22.0f;
      canyon.base_intensity = 2.2f;
      canyon.type = LuminaryType::BioluminescentLedge;
      canyon.pulse_speed = 1.6f;
      canyon.name = "Canyon Arch Lantern";
      m_luminaries.push_back(canyon);
      break;
    }

    case RoomShapeType::SubterraneanAquiferOasis: {
      CavernLuminary pool_glow;
      pool_glow.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 1.2f),
                    room.center.z);
      pool_glow.base_color = glm::vec3(0.08f, 0.92f, 0.96f);
      pool_glow.base_radius = 24.0f;
      pool_glow.base_intensity = 3.0f;
      pool_glow.type = LuminaryType::AquiferOasisGlow;
      pool_glow.pulse_speed = 1.3f;
      pool_glow.pulse_depth = 0.15f;
      pool_glow.name = "Aquifer Oasis Pool Luminary";
      m_luminaries.push_back(pool_glow);

      CavernLuminary waterfall_mist;
      waterfall_mist.position =
          glm::vec3(room.center.x - room.half_width + 2.0f,
                    static_cast<float>(room.floor_y + 4.0f), room.center.z);
      waterfall_mist.base_color = glm::vec3(0.18f, 0.98f, 0.55f);
      waterfall_mist.base_radius = 16.0f;
      waterfall_mist.base_intensity = 2.2f;
      waterfall_mist.type = LuminaryType::AquiferOasisGlow;
      waterfall_mist.name = "Waterfall Mist Flora Glow";
      m_luminaries.push_back(waterfall_mist);
      break;
    }

    case RoomShapeType::ColossalAbyssalChasm: {
      CavernLuminary bridge_light;
      bridge_light.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 9.5f),
                    room.center.z);
      bridge_light.base_color = glm::vec3(0.95f, 0.85f, 0.40f);
      bridge_light.base_radius = 22.0f;
      bridge_light.base_intensity = 2.8f;
      bridge_light.type = LuminaryType::CorridorBulkheadLight;
      bridge_light.name = "Suspension Bridge Lantern";
      m_luminaries.push_back(bridge_light);

      CavernLuminary pit_beacon;
      pit_beacon.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 2.0f),
                    room.center.z);
      pit_beacon.base_color = glm::vec3(0.95f, 0.20f, 0.12f);
      pit_beacon.base_radius = 28.0f;
      pit_beacon.base_intensity = 3.4f;
      pit_beacon.type = LuminaryType::IndustrialVaultBeacon;
      pit_beacon.pulse_speed = 2.4f;
      pit_beacon.pulse_depth = 0.28f;
      pit_beacon.name = "Abyssal Chasm Spike Warning";
      m_luminaries.push_back(pit_beacon);
      break;
    }

    case RoomShapeType::MoltenMagmaFoundry: {
      CavernLuminary hearth;
      hearth.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 1.2f),
                    room.center.z);
      hearth.base_color = glm::vec3(1.0f, 0.38f, 0.05f);
      hearth.base_radius = 28.0f;
      hearth.base_intensity = 3.8f;
      hearth.type = LuminaryType::ThermalMagmaVent;
      hearth.pulse_speed = 3.0f;
      hearth.pulse_depth = 0.25f;
      hearth.name = "Magma Foundry Smelter";
      m_luminaries.push_back(hearth);

      CavernLuminary flume;
      flume.position = glm::vec3(room.center.x + 4.0f,
                                 static_cast<float>(room.floor_y + 4.0f),
                                 room.center.z - 4.0f);
      flume.base_color = glm::vec3(1.0f, 0.65f, 0.12f);
      flume.base_radius = 16.0f;
      flume.base_intensity = 2.4f;
      flume.type = LuminaryType::ThermalMagmaVent;
      flume.name = "Foundry Slag Flume";
      m_luminaries.push_back(flume);
      break;
    }

    case RoomShapeType::ToxicMiasmaSwamp: {
      CavernLuminary miasma;
      miasma.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 2.0f),
                    room.center.z);
      miasma.base_color = glm::vec3(0.22f, 0.95f, 0.18f);
      miasma.base_radius = 24.0f;
      miasma.base_intensity = 3.0f;
      miasma.type = LuminaryType::ToxicMiasmaGreen;
      miasma.pulse_speed = 1.8f;
      miasma.pulse_depth = 0.30f;
      miasma.flicker_rate = 7.0f;
      miasma.name = "Toxic Miasma Core Vent";
      m_luminaries.push_back(miasma);

      CavernLuminary spore;
      spore.position = glm::vec3(room.center.x - 3.5f,
                                 static_cast<float>(room.floor_y + 4.0f),
                                 room.center.z + 3.5f);
      spore.base_color = glm::vec3(0.40f, 0.85f, 0.15f);
      spore.base_radius = 16.0f;
      spore.base_intensity = 2.0f;
      spore.type = LuminaryType::ToxicMiasmaGreen;
      spore.name = "Swamp Spore Vent";
      m_luminaries.push_back(spore);
      break;
    }

    case RoomShapeType::PrismaticCrystalCathedral: {
      CavernLuminary monolith;
      monolith.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 6.0f),
                    room.center.z);
      monolith.base_color = glm::vec3(0.28f, 0.75f, 0.88f);
      monolith.base_radius = 20.0f;
      monolith.base_intensity = 1.6f;
      monolith.type = LuminaryType::PrismaticCrystalRadiance;
      monolith.pulse_speed = 1.2f;
      monolith.pulse_depth = 0.15f;
      monolith.name = "Prismatic Cathedral Ambient Glow";
      m_luminaries.push_back(monolith);

      CavernLuminary spire;
      spire.position = glm::vec3(room.center.x + 4.0f,
                                 static_cast<float>(room.floor_y + 9.0f),
                                 room.center.z + 4.0f);
      spire.base_color = glm::vec3(0.22f, 0.78f, 0.90f);
      spire.base_radius = 15.0f;
      spire.base_intensity = 1.2f;
      spire.type = LuminaryType::PrismaticCrystalRadiance;
      spire.name = "Cathedral Spire Prism";
      m_luminaries.push_back(spire);
      break;
    }

    case RoomShapeType::AncientTitanNecropolis: {
      CavernLuminary titan_rib;
      titan_rib.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 6.0f),
                    room.center.z);
      titan_rib.base_color = glm::vec3(0.92f, 0.65f, 0.18f);
      titan_rib.base_radius = 26.0f;
      titan_rib.base_intensity = 3.2f;
      titan_rib.type = LuminaryType::TitanFossilAura;
      titan_rib.pulse_speed = 1.0f;
      titan_rib.pulse_depth = 0.18f;
      titan_rib.name = "Titan Necropolis Fossil Glow";
      m_luminaries.push_back(titan_rib);
      break;
    }

    case RoomShapeType::BioluminescentGlowwormGrotto: {
      CavernLuminary starry_canopy;
      starry_canopy.position =
          glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 2.0f),
                    room.center.z);
      starry_canopy.base_color = glm::vec3(0.15f, 0.92f, 0.82f);
      starry_canopy.base_radius = 26.0f;
      starry_canopy.base_intensity = 3.4f;
      starry_canopy.type = LuminaryType::BioluminescentLedge;
      starry_canopy.pulse_speed = 2.0f;
      starry_canopy.pulse_depth = 0.25f;
      starry_canopy.name = "Glowworm Canopy Stars";
      m_luminaries.push_back(starry_canopy);

      CavernLuminary mirror_pool;
      mirror_pool.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 1.0f),
                    room.center.z);
      mirror_pool.base_color = glm::vec3(0.10f, 0.75f, 0.95f);
      mirror_pool.base_radius = 20.0f;
      mirror_pool.base_intensity = 2.2f;
      mirror_pool.type = LuminaryType::AquiferOasisGlow;
      mirror_pool.name = "Grotto Mirror Pool";
      m_luminaries.push_back(mirror_pool);
      break;
    }

    case RoomShapeType::PrecursorCoolantReservoir: {
      CavernLuminary coolant;
      coolant.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 2.0f),
                    room.center.z);
      coolant.base_color = glm::vec3(0.08f, 0.70f, 0.98f);
      coolant.base_radius = 24.0f;
      coolant.base_intensity = 3.2f;
      coolant.type = LuminaryType::IndustrialVaultBeacon;
      coolant.pulse_speed = 3.8f;
      coolant.pulse_depth = 0.22f;
      coolant.name = "Precursor Coolant Reservoir";
      m_luminaries.push_back(coolant);

      CavernLuminary strobe;
      strobe.position = glm::vec3(room.center.x - 4.0f,
                                  static_cast<float>(room.floor_y + 4.5f),
                                  room.center.z + 4.0f);
      strobe.base_color = glm::vec3(0.98f, 0.65f, 0.10f);
      strobe.base_radius = 16.0f;
      strobe.base_intensity = 2.6f;
      strobe.type = LuminaryType::LaserSecurityStrobe;
      strobe.flicker_rate = 14.0f;
      strobe.name = "Reservoir Leak Warning";
      m_luminaries.push_back(strobe);
      break;
    }

    case RoomShapeType::ColossalVaultedDredgeCathedral: {
      CavernLuminary high_vault;
      high_vault.position =
          glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 2.5f),
                    room.center.z);
      high_vault.base_color =
          glm::vec3(0.85f, 0.72f, 0.98f); // Radiant celestial violet / gold
      high_vault.base_radius = 28.0f;
      high_vault.base_intensity = 3.6f;
      high_vault.type = LuminaryType::GrandVaultRadiance;
      high_vault.pulse_speed = 1.6f;
      high_vault.pulse_depth = 0.20f;
      high_vault.name = "Cathedral Vault Chandelier";
      m_luminaries.push_back(high_vault);

      CavernLuminary dais_beacon;
      dais_beacon.position =
          glm::vec3(room.center.x + static_cast<float>(room.half_width - 3),
                    static_cast<float>(room.floor_y + 2.0f), room.center.z);
      dais_beacon.base_color = glm::vec3(0.55f, 0.25f, 0.95f);
      dais_beacon.base_radius = 18.0f;
      dais_beacon.base_intensity = 2.4f;
      dais_beacon.type = LuminaryType::VoiditeCrystalGeode;
      dais_beacon.name = "Ceremonial Dais Monolith";
      m_luminaries.push_back(dais_beacon);
      break;
    }

    case RoomShapeType::TectonicAbyssalSinkhole: {
      CavernLuminary thermal_rift;
      thermal_rift.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 0.8f),
                    room.center.z);
      thermal_rift.base_color =
          glm::vec3(1.0f, 0.38f, 0.06f); // Searing thermal rift
      thermal_rift.base_radius = 26.0f;
      thermal_rift.base_intensity = 3.4f;
      thermal_rift.type = LuminaryType::ThermalMagmaVent;
      thermal_rift.pulse_speed = 3.0f;
      thermal_rift.name = "Abyssal Sinkhole Fissure";
      m_luminaries.push_back(thermal_rift);

      CavernLuminary rim_beacon;
      rim_beacon.position =
          glm::vec3(room.center.x - static_cast<float>(room.half_width - 3),
                    static_cast<float>(room.floor_y + 16.0f), room.center.z);
      rim_beacon.base_color = glm::vec3(0.18f, 0.82f, 0.95f);
      rim_beacon.base_radius = 20.0f;
      rim_beacon.base_intensity = 2.2f;
      rim_beacon.type = LuminaryType::BioluminescentLedge;
      rim_beacon.name = "Sinkhole Precipice Overlook";
      m_luminaries.push_back(rim_beacon);
      break;
    }

    case RoomShapeType::CyclopeanExcavationSilo: {
      CavernLuminary floodlight;
      floodlight.position =
          glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 2.0f),
                    room.center.z);
      floodlight.base_color =
          glm::vec3(1.0f, 0.76f, 0.20f); // Warm sodium industrial amber
      floodlight.base_radius = 28.0f;
      floodlight.base_intensity = 3.8f;
      floodlight.type = LuminaryType::CyclopeanFloodlight;
      floodlight.pulse_speed = 4.2f;
      floodlight.pulse_depth = 0.25f;
      floodlight.name = "Cyclopean Crane Floodlight";
      m_luminaries.push_back(floodlight);

      CavernLuminary mid_ring;
      mid_ring.position = glm::vec3(room.center.x + 4.5f,
                                    static_cast<float>(room.floor_y + 9.0f),
                                    room.center.z - 4.5f);
      mid_ring.base_color = glm::vec3(0.95f, 0.55f, 0.12f);
      mid_ring.base_radius = 16.0f;
      mid_ring.base_intensity = 2.0f;
      mid_ring.type = LuminaryType::IndustrialVaultBeacon;
      mid_ring.name = "Silo Maintenance Ring Beacon";
      m_luminaries.push_back(mid_ring);
      break;
    }

    case RoomShapeType::BioluminescentFirmamentAbyss: {
      CavernLuminary canopy_firmament;
      canopy_firmament.position =
          glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 2.5f),
                    room.center.z);
      canopy_firmament.base_color =
          glm::vec3(0.20f, 0.85f, 0.95f); // Celestial starlight cyan
      canopy_firmament.base_radius = 32.0f;
      canopy_firmament.base_intensity = 3.2f;
      canopy_firmament.type = LuminaryType::FirmamentStarlight;
      canopy_firmament.pulse_speed = 1.4f;
      canopy_firmament.pulse_depth = 0.15f;
      canopy_firmament.name = "Firmament Star Canopy";
      m_luminaries.push_back(canopy_firmament);

      CavernLuminary aquifer_glow;
      aquifer_glow.position = glm::vec3(room.center.x - 2.0f,
                                        static_cast<float>(room.floor_y + 1.2f),
                                        room.center.z + 2.0f);
      aquifer_glow.base_color = glm::vec3(0.12f, 0.95f, 0.80f);
      aquifer_glow.base_radius = 18.0f;
      aquifer_glow.base_intensity = 2.0f;
      aquifer_glow.type = LuminaryType::AquiferOasisGlow;
      aquifer_glow.name = "Firmament Reflecting Pool";
      m_luminaries.push_back(aquifer_glow);
      break;
    }

    case RoomShapeType::PrecursorSanctumVault: {
      CavernLuminary sanctum;
      sanctum.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 4.0f),
                    room.center.z);
      sanctum.base_color = glm::vec3(0.55f, 0.25f, 0.95f); // Celestial radiant violet/gold
      sanctum.base_radius = 22.0f;
      sanctum.base_intensity = 2.8f;
      sanctum.type = LuminaryType::GrandVaultRadiance;
      sanctum.pulse_speed = 1.6f;
      sanctum.pulse_depth = 0.20f;
      sanctum.name = "Sanctum Relic Radiance";
      m_luminaries.push_back(sanctum);
      break;
    }

    case RoomShapeType::SubterraneanVaultBastion: {
      CavernLuminary bastion;
      bastion.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 5.0f),
                    room.center.z);
      bastion.base_color = glm::vec3(1.0f, 0.65f, 0.10f); // Industrial hazard amber
      bastion.base_radius = 18.0f;
      bastion.base_intensity = 2.5f;
      bastion.type = LuminaryType::IndustrialVaultBeacon;
      bastion.flicker_rate = 6.0f;
      bastion.name = "Vault Bastion Security Strobe";
      m_luminaries.push_back(bastion);
      break;
    }

    case RoomShapeType::AbyssalVaultChasm: {
      CavernLuminary chasm_core;
      chasm_core.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 3.0f),
                    room.center.z + 3.0f);
      chasm_core.base_color = glm::vec3(0.85f, 0.15f, 0.90f); // Abyssal purple radiance
      chasm_core.base_radius = 28.0f;
      chasm_core.base_intensity = 3.4f;
      chasm_core.type = LuminaryType::GrandVaultRadiance;
      chasm_core.pulse_speed = 2.2f;
      chasm_core.pulse_depth = 0.25f;
      chasm_core.name = "Abyssal Vault Core Beacon";
      m_luminaries.push_back(chasm_core);

      CavernLuminary lava_vent;
      lava_vent.position =
          glm::vec3(room.center.x, 5.0f, room.center.z - 3.0f);
      lava_vent.base_color = glm::vec3(1.0f, 0.35f, 0.05f); // Molten thermite orange
      lava_vent.base_radius = 20.0f;
      lava_vent.base_intensity = 2.8f;
      lava_vent.type = LuminaryType::ThermalMagmaVent;
      lava_vent.name = "Chasm Sump Slag Lake";
      m_luminaries.push_back(lava_vent);
      break;
    }

    case RoomShapeType::SpawnStagingCavern:
    default: {
      CavernLuminary spawn_beacon;
      spawn_beacon.position =
          glm::vec3(room.center.x, static_cast<float>(room.floor_y + 3.0f),
                    room.center.z);
      spawn_beacon.base_color = glm::vec3(0.95f, 0.70f, 0.30f);
      spawn_beacon.base_radius = 14.0f;
      spawn_beacon.base_intensity = 1.6f;
      spawn_beacon.type = LuminaryType::CorridorBulkheadLight;
      spawn_beacon.name = "Staging Arrival Beacon";
      m_luminaries.push_back(spawn_beacon);
      break;
    }
    }
  }

  // Dedicated Precursor Relic Chamber Luminary
  if (m_vault_placement.exists) {
    CavernLuminary relic_lum;
    relic_lum.position = glm::vec3(
        static_cast<float>(m_vault_placement.relic_pos.x) + 0.5f,
        static_cast<float>(m_vault_placement.relic_pos.y) + 0.5f,
        static_cast<float>(m_vault_placement.relic_pos.z) + 0.5f);
    relic_lum.base_color = glm::vec3(0.60f, 0.25f, 1.0f); // Radiant celestial violet
    relic_lum.base_radius = 18.0f;
    relic_lum.base_intensity = 3.0f;
    relic_lum.type = LuminaryType::GrandVaultRadiance;
    relic_lum.pulse_speed = 2.5f;
    relic_lum.pulse_depth = 0.30f;
    relic_lum.name = "Precursor Relic Hyper-Core Beacon";
    m_luminaries.push_back(relic_lum);
  }

  // Corridor lights
  for (size_t ci = 0; ci < m_corridors.size(); ++ci) {
    const auto &c = m_corridors[ci];
    glm::vec3 mid = (glm::vec3(c.start_pos) + glm::vec3(c.end_pos)) * 0.5f;

    // Check if midpoint falls inside any dark zone bounding box
    bool in_dark_zone = false;
    for (const auto &r : m_rooms) {
      if (r.is_dark_zone && mid.x >= r.min_x() && mid.x <= r.max_x() &&
          mid.z >= r.min_z() && mid.z <= r.max_z()) {
        in_dark_zone = true;
        break;
      }
    }
    if (in_dark_zone || is_dark_zone_at(mid)) {
      continue;
    }

    if (c.has_bulkhead_door) {
      CavernLuminary blk_light;
      blk_light.position =
          glm::vec3(mid.x, static_cast<float>(c.floor_y + 2.5f), mid.z);
      blk_light.base_color = glm::vec3(0.95f, 0.60f, 0.18f);
      blk_light.base_radius = 10.0f;
      blk_light.base_intensity = 1.4f;
      blk_light.type = LuminaryType::CorridorBulkheadLight;
      blk_light.pulse_speed = 3.5f;
      blk_light.pulse_depth = 0.18f;
      blk_light.name = "Bulkhead Clearance Light";
      m_luminaries.push_back(blk_light);
    } else {
      // Connecting tunnels: Corridors are primarily dark and unlit (~75%) to maintain tension
      // and provide natural dark zones between cavern chambers.
      // The remaining ~25% feature diversified fixtures: wall-mounted sconces, crevice crystal glints,
      // or rare bioluminescent spore nodes (instead of repetitive mushroom lights).
      uint32_t corridor_hash = static_cast<uint32_t>((ci * 7919 + m_seed * 104729) ^ 0x5bd1e995);
      uint32_t roll = corridor_hash % 100;
      if (roll >= 75) {
        CavernLuminary tunnel_light;
        if (roll < 85) {
          // Wall-mounted industrial sconce / contractor survey lamp
          tunnel_light.position = glm::vec3(mid.x, static_cast<float>(c.floor_y + 2.2f), mid.z);
          tunnel_light.base_color = glm::vec3(0.98f, 0.70f, 0.25f); // Warm halogen tungsten amber
          tunnel_light.base_radius = 8.5f;
          tunnel_light.base_intensity = 1.25f;
          tunnel_light.type = LuminaryType::WallIndustrialSconce;
          tunnel_light.pulse_speed = 0.5f;
          tunnel_light.pulse_depth = 0.08f;
          tunnel_light.name = "Corridor Wall Sconce";
          m_luminaries.push_back(tunnel_light);
        } else if (roll < 94) {
          // Subterranean crystal crevice glint (amethyst/cyan mineral radiance)
          tunnel_light.position = glm::vec3(mid.x, static_cast<float>(c.floor_y + 1.5f), mid.z);
          tunnel_light.base_color = ((corridor_hash >> 4) % 2 == 0)
              ? glm::vec3(0.35f, 0.72f, 0.98f) // Deep cyan prism
              : glm::vec3(0.78f, 0.35f, 0.95f); // Resonant amethyst
          tunnel_light.base_radius = 7.5f;
          tunnel_light.base_intensity = 1.15f;
          tunnel_light.type = LuminaryType::PrismaticCrystalRadiance;
          tunnel_light.pulse_speed = 1.2f;
          tunnel_light.pulse_depth = 0.15f;
          tunnel_light.name = "Crevice Crystal Glint";
          m_luminaries.push_back(tunnel_light);
        } else {
          // Rare bioluminescent spore node (only 6% of corridors)
          tunnel_light.position = glm::vec3(mid.x, static_cast<float>(c.floor_y + 1.2f), mid.z);
          tunnel_light.base_color = glm::vec3(0.18f, 0.85f, 0.62f); // Bioluminescent fungal teal
          tunnel_light.base_radius = 8.0f;
          tunnel_light.base_intensity = 1.05f;
          tunnel_light.type = LuminaryType::FungoidSporeGlow;
          tunnel_light.pulse_speed = 1.6f;
          tunnel_light.pulse_depth = 0.22f;
          tunnel_light.name = "Corridor Spore Luminary";
          m_luminaries.push_back(tunnel_light);
        }
      }
    }
  }

  // Fluid structure luminaries for procedural water, coolant, magma, and acid structures
  for (const auto &room : m_rooms) {
    if (room.has_random_fluid && (!room.is_dark_zone || room.random_fluid_material == MAT_THERMITE_SLAG || room.random_fluid_material == MAT_LAVA)) {
      CavernLuminary pool_lum;
      if (room.fluid_structure_type == FluidStructureType::DropChasmSump) {
        pool_lum.position = glm::vec3(
            static_cast<float>(room.center.x),
            5.2f,
            static_cast<float>(room.center.z));
        pool_lum.base_radius = 20.0f;
      } else if (room.fluid_structure_type == FluidStructureType::WallCascade) {
        pool_lum.position = glm::vec3(
            static_cast<float>(room.center.x) + room.random_fluid_offset_x,
            static_cast<float>(room.floor_y) + 2.5f,
            static_cast<float>(room.center.z) + room.random_fluid_offset_z);
        pool_lum.base_radius = 16.0f;
      } else {
        pool_lum.position = glm::vec3(
            static_cast<float>(room.center.x) + room.random_fluid_offset_x,
            static_cast<float>(room.floor_y) + 1.2f,
            static_cast<float>(room.center.z) + room.random_fluid_offset_z);
        pool_lum.base_radius = 16.0f;
      }

      if (room.random_fluid_material == MAT_CRYSTAL_AQUIFER ||
          room.random_fluid_material == MAT_WATER) {
        pool_lum.base_color = glm::vec3(0.12f, 0.88f, 0.95f);
        pool_lum.base_intensity = 2.2f;
        pool_lum.type = LuminaryType::AquiferOasisGlow;
        pool_lum.pulse_speed = 1.8f;
        pool_lum.pulse_depth = 0.18f;
        if (room.fluid_structure_type == FluidStructureType::WallCascade) {
          pool_lum.name = "Subterranean Cascade Shimmer";
        } else if (room.fluid_structure_type == FluidStructureType::DropChasmSump) {
          pool_lum.name = "Abyssal Aquifer Sump Shimmer";
        } else {
          pool_lum.name = "Subterranean Spring Shimmer";
        }
      } else if (room.random_fluid_material == MAT_THERMITE_SLAG ||
                 room.random_fluid_material == MAT_MOLTEN_MAGMA ||
                 room.random_fluid_material == MAT_LAVA) {
        pool_lum.base_color = glm::vec3(1.0f, 0.42f, 0.05f);
        pool_lum.base_intensity = 3.0f;
        pool_lum.type = LuminaryType::ThermalMagmaVent;
        pool_lum.pulse_speed = 2.6f;
        pool_lum.pulse_depth = 0.22f;
        if (room.fluid_structure_type == FluidStructureType::HazardTrench) {
          pool_lum.name = "Molten Slag Hazard Trench Glow";
        } else if (room.fluid_structure_type == FluidStructureType::DropChasmSump) {
          pool_lum.name = "Abyssal Caldera Pit Glow";
        } else {
          pool_lum.name = "Molten Slag Pool Glow";
        }
      } else if (room.random_fluid_material == MAT_COOLANT) {
        pool_lum.base_color = glm::vec3(0.35f, 0.75f, 1.0f);
        pool_lum.base_intensity = 2.4f;
        pool_lum.type = LuminaryType::BioluminescentLedge;
        pool_lum.pulse_speed = 1.5f;
        pool_lum.pulse_depth = 0.15f;
        if (room.fluid_structure_type == FluidStructureType::WallCascade) {
          pool_lum.name = "Cryogenic Coolant Sluice Glow";
        } else if (room.fluid_structure_type == FluidStructureType::HazardTrench) {
          pool_lum.name = "Cryogenic Coolant Trench Glow";
        } else {
          pool_lum.name = "Cryogenic Coolant Reservoir Glow";
        }
      } else if (room.random_fluid_material == MAT_ACID) {
        pool_lum.base_color = glm::vec3(0.20f, 0.95f, 0.15f);
        pool_lum.base_intensity = 2.8f;
        pool_lum.type = LuminaryType::ToxicMiasmaGreen;
        pool_lum.pulse_speed = 2.0f;
        pool_lum.pulse_depth = 0.25f;
        pool_lum.flicker_rate = 6.0f;
        if (room.fluid_structure_type == FluidStructureType::HazardTrench) {
          pool_lum.name = "Corrosive Acid Trench Glow";
        } else if (room.fluid_structure_type == FluidStructureType::DropChasmSump) {
          pool_lum.name = "Abyssal Acid Sump Glow";
        } else {
          pool_lum.name = "Corrosive Acid Pool Glow";
        }
      }
      m_luminaries.push_back(pool_lum);
    }
  }

  // =========================================================================
  // Spatial Lighting Density & Area Balancing Pass
  // =========================================================================
  // 1. Minimum distance / anti-clustering filter: Prevent unnatural luminary clumping
  //    within 8.5m of each other (prioritize critical beacons and fluid sources).
  // 2. Fluid luminary focal precedence: In rooms containing glowing fluids (lava,
  //    acid, coolant, aquifer water), suppress secondary point lights within 12m
  //    so the fluid pool serves as the dramatic primary illumination.
  // 3. Dark zone preservation: Ensure rooms flagged is_dark_zone contain no
  //    non-hazard luminaries, guaranteeing genuine tension-filled dark regions.
  // 4. Starvation prevention: Ensure non-dark rooms have at least one illumination
  //    source so areas without fluids or landmarks are not accidentally pitch-black.
  std::vector<CavernLuminary> balanced_luminaries;
  balanced_luminaries.reserve(m_luminaries.size());

  // First pass: add all essential structural & fluid luminaries
  for (const auto &lum : m_luminaries) {
    bool is_priority = (lum.type == LuminaryType::ThermalMagmaVent ||
                        lum.type == LuminaryType::AquiferOasisGlow ||
                        lum.type == LuminaryType::ToxicMiasmaGreen ||
                        lum.type == LuminaryType::CorridorBulkheadLight ||
                        lum.type == LuminaryType::IndustrialVaultBeacon ||
                        lum.type == LuminaryType::GrandVaultRadiance);

    // If inside a dark zone, strictly filter out non-essential luminaries
    if (is_dark_zone_at(lum.position)) {
      if (lum.type != LuminaryType::ThermalMagmaVent &&
          lum.type != LuminaryType::GrandVaultRadiance &&
          lum.type != LuminaryType::IndustrialVaultBeacon) {
        continue; // Maintain authentic darkness in designated dark zones
      }
    }

    if (is_priority) {
      balanced_luminaries.push_back(lum);
    }
  }

  // Second pass: add decorative & ambient lights with anti-clustering checks
  for (const auto &lum : m_luminaries) {
    bool is_priority = (lum.type == LuminaryType::ThermalMagmaVent ||
                        lum.type == LuminaryType::AquiferOasisGlow ||
                        lum.type == LuminaryType::ToxicMiasmaGreen ||
                        lum.type == LuminaryType::CorridorBulkheadLight ||
                        lum.type == LuminaryType::IndustrialVaultBeacon ||
                        lum.type == LuminaryType::GrandVaultRadiance);
    if (is_priority) continue; // Already processed

    if (is_dark_zone_at(lum.position)) {
      continue; // Dark zone preservation
    }

    // Check distance against already approved luminaries
    bool too_close = false;
    for (const auto &accepted : balanced_luminaries) {
      float d = glm::distance(lum.position, accepted.position);
      // Extra clearance near fluid pools so the radiant liquid is the focal feature
      float min_dist = (accepted.type == LuminaryType::ThermalMagmaVent ||
                        accepted.type == LuminaryType::AquiferOasisGlow ||
                        accepted.type == LuminaryType::ToxicMiasmaGreen) ? 12.0f : 8.5f;
      if (d < min_dist) {
        too_close = true;
        break;
      }
    }

    if (!too_close) {
      balanced_luminaries.push_back(lum);
    }
  }

  // Third pass: non-dark room starvation check
  // If an unflagged room ended up with 0 luminaries, add a single balanced sconce
  for (const auto &room : m_rooms) {
    if (room.is_dark_zone) continue; // Intentionally dark

    bool has_light = false;
    for (const auto &lum : balanced_luminaries) {
      if (lum.position.x >= room.min_x() - 2.0f && lum.position.x <= room.max_x() + 2.0f &&
          lum.position.z >= room.min_z() - 2.0f && lum.position.z <= room.max_z() + 2.0f) {
        has_light = true;
        break;
      }
    }

    if (!has_light) {
      CavernLuminary fill_light;
      fill_light.position = glm::vec3(
          static_cast<float>(room.center.x),
          static_cast<float>(room.floor_y + 2.4f),
          static_cast<float>(room.center.z));
      fill_light.base_color = glm::vec3(0.98f, 0.72f, 0.28f); // Warm halogen sconce
      fill_light.base_radius = 12.0f;
      fill_light.base_intensity = 1.35f;
      fill_light.type = LuminaryType::WallIndustrialSconce;
      fill_light.pulse_speed = 0.5f;
      fill_light.pulse_depth = 0.08f;
      fill_light.name = "Chamber Wall Worklight";
      balanced_luminaries.push_back(fill_light);
    }
  }

  m_luminaries = std::move(balanced_luminaries);
}

static inline bool sample_ravine_corridor(int x, int y, int z, float t,
                                          float lat_dist, float eff_width,
                                          int floor_y, int ceil_y, Voxel &out) {
  int ravine_bed = 4;
  bool is_threshold = (t < 0.16f || t > 0.84f);
  bool is_bridge = (lat_dist <= 1.25f);

  if (is_threshold || is_bridge) {
    if (y == floor_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y == floor_y - 1 && floor_y > ravine_bed) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Bridge arch thickness
      return true;
    }
    if (y < floor_y - 1 && y > ravine_bed && !is_threshold) {
      out = Voxel{MAT_AIR, 0}; // Open drop beneath central bridge
      return true;
    }
    if (y == ravine_bed) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y > floor_y && y < ceil_y) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  } else {
    // Flanking deep ravine chasm
    if (y < ravine_bed) {
      out = Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
      return true;
    }
    if (y == ravine_bed) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    // Stepped rubble ramps at corridor ends for climbing out
    if (floor_y > ravine_bed) {
      float climb_t = std::min(t - 0.16f, 0.84f - t);
      if (climb_t < 0.12f) {
        int crag_y = ravine_bed +
                     static_cast<int>(
                         std::round((1.0f - climb_t / 0.12f) *
                                    static_cast<float>(floor_y - ravine_bed)));
        if (y <= crag_y) {
          out = Voxel{MAT_VOLCANIC_BASALT, 0};
          return true;
        }
      }
    }
    if (y > ravine_bed && y < ceil_y) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  if (y == ceil_y) {
    out = Voxel{MAT_FRACTURED_GRANITE, 0};
    return true;
  }

  return false;
}

Voxel LevelGenerator::sample_voxel(int x, int y, int z, bool* out_is_vein) const {
  if (out_is_vein) {
    *out_is_vein = false;
  }
  // 1. Bedrock base layer (bottom of world is solid impenetrable crust)
  if (y <= 3) {
    return Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
  }

  // 2. Ceiling mantle layer (top of world is solid rock mantle)
  if (y >= m_world_h - 6) {
    uint8_t ceiling_mat =
        (m_sector_index >= 2) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE;
    return Voxel{ceiling_mat, VOXEL_FLAG_ANCHORED};
  }

  // 3. World Perimeter Barrier (Outer solid bedrock bounding walls)
  if (x <= 3 || x >= world_width() - 4 || z <= 3 || z >= world_depth() - 4) {
    return Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
  }

  // 3.5. Objective Material Injection (Guaranteed balanced, scarce placement)
  if (is_objective_voxel(x, y, z)) {
    return Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
  }

  return sample_base_voxel(x, y, z, out_is_vein);
}

Voxel LevelGenerator::sample_base_voxel(int x, int y, int z, bool* out_is_vein) const {
  if (out_is_vein) {
    *out_is_vein = false;
  }
  // 1. Bedrock base layer (bottom of world is solid impenetrable crust)
  if (y <= 3) {
    return Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
  }

  // 2. Ceiling mantle layer (top of world is solid rock mantle)
  if (y >= m_world_h - 6) {
    uint8_t ceiling_mat =
        (m_sector_index >= 2) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE;
    return Voxel{ceiling_mat, VOXEL_FLAG_ANCHORED};
  }

  // 3. World Perimeter Barrier (Outer solid bedrock bounding walls)
  if (x <= 3 || x >= world_width() - 4 || z <= 3 || z >= world_depth() - 4) {
    return Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
  }

  // 3.8. Precursor Vault Chamber & Portal Integration
  Voxel vault_vox{MAT_AIR, 0};
  if (sample_vault_structure(x, y, z, vault_vox)) {
    return vault_vox;
  }

  // 4. Default Host Rock Matrix per Sector with Organic Geological Strata
  uint8_t host_rock = MAT_FRACTURED_GRANITE;
  if (m_sector_index == 2) {
    host_rock = MAT_VOLCANIC_BASALT;
  } else if (m_sector_index >= 3) {
    host_rock = (y < 12) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE;
  }
  // Geological Strata Bands running through the planetary bedrock
  float wavy_host_y = static_cast<float>(y) + std::sin(static_cast<float>(x) * 0.22f) * 1.1f + std::cos(static_cast<float>(z) * 0.22f) * 0.9f;
  int host_band = static_cast<int>(std::floor(wavy_host_y + 100.0f)) % 7;
  if (host_band == 0 || host_band == 1) {
    host_rock = MAT_VOLCANIC_BASALT;
  } else if (host_band >= 2 && host_band <= 4) {
    host_rock = MAT_FRACTURED_GRANITE;
  } else if (host_band == 5 && y <= 8) {
    host_rock = MAT_DREDGE_BEDROCK;
  }
  uint32_t host_patch = (static_cast<uint32_t>(x) * 73856093u) ^ (static_cast<uint32_t>(y) * 19349663u) ^ (static_cast<uint32_t>(z) * 83492791u);
  if ((host_patch % 100) < 12) {
    host_rock = (host_rock == MAT_FRACTURED_GRANITE) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE;
  }

  Voxel voxel{host_rock, 0};

  auto is_in_chasm_abyss = [&](int qx, int qy, int qz) -> bool {
    for (const auto &r : m_rooms) {
      if (r.type == RoomShapeType::FaultLineCrevasse ||
          r.type == RoomShapeType::CrumblingArchCanyon ||
          r.type == RoomShapeType::SpikeTrenchArena ||
          r.type == RoomShapeType::AbyssalVerticalChasm ||
          r.type == RoomShapeType::ColossalAbyssalChasm ||
          r.type == RoomShapeType::TectonicAbyssalSinkhole) {
        if (std::abs(qx - r.center.x) <= r.half_width - 2 &&
            std::abs(qz - r.center.z) <= r.half_depth - 2 &&
            qy < r.floor_y) {
          return true;
        }
      }
    }
    return false;
  };

  auto is_in_any_room_interior = [&](int qx, int qz) -> bool {
    for (const auto &r : m_rooms) {
      if (std::abs(qx - r.center.x) <= r.half_width - 2 &&
          std::abs(qz - r.center.z) <= r.half_depth - 2) {
        return true;
      }
    }
    return false;
  };

  // 5. Check if inside any Corridor
  for (const auto &c : m_corridors) {
    if (c.is_diagonal) {
        float ax = static_cast<float>(c.start_pos.x);
        float az = static_cast<float>(c.start_pos.z);
        float bx = static_cast<float>(c.end_pos.x);
        float bz = static_cast<float>(c.end_pos.z);
        float px = static_cast<float>(x);
        float pz = static_cast<float>(z);

        float min_cx = std::min(ax, bx) - static_cast<float>(c.half_width) - 1.0f;
        float max_cx = std::max(ax, bx) + static_cast<float>(c.half_width) + 1.0f;
        float min_cz = std::min(az, bz) - static_cast<float>(c.half_width) - 1.0f;
        float max_cz = std::max(az, bz) + static_cast<float>(c.half_width) + 1.0f;
        if (px < min_cx || px > max_cx || pz < min_cz || pz > max_cz)
          continue;

        float vx = bx - ax;
        float vz = bz - az;
        float len_sq = vx * vx + vz * vz;
        if (len_sq < 0.001f)
          continue;

        float t =
            glm::clamp(((px - ax) * vx + (pz - az) * vz) / len_sq, 0.0f, 1.0f);
        float qx = ax + t * vx;
        float qz = az + t * vz;
        float dist = std::sqrt((px - qx) * (px - qx) + (pz - qz) * (pz - qz));

        float eff_width = static_cast<float>(c.half_width);
        if (c.is_ravine) {
          eff_width += (pseudo_rand(x, y, z, 77) - 0.5f) * 1.5f;
        } else {
          eff_width += (pseudo_rand(x, y, z, 77) - 0.5f) * 0.85f;
        }
        eff_width = std::max(1.8f, eff_width);

        if (dist <= eff_width) {
          int floor_y = static_cast<int>(
              std::round(static_cast<float>(c.floor_y) +
                         t * static_cast<float>(c.end_floor_y - c.floor_y)));
          int ceil_y = static_cast<int>(
              std::round(static_cast<float>(c.ceiling_y) +
                         t * static_cast<float>(c.end_ceiling_y - c.ceiling_y)));

          if (c.is_ravine) {
            if (sample_ravine_corridor(x, y, z, t, dist, eff_width, floor_y,
                                       ceil_y, voxel)) {
              return voxel;
            }
          }

          if (y == floor_y || (c.type == CorridorType::SlopingRamp && y == floor_y - 1)) {
            if (!is_in_chasm_abyss(x, y, z)) {
              voxel = Voxel{MAT_VOLCANIC_BASALT, 0};
              return voxel;
            }
          }
          if (y > floor_y && y < ceil_y) {
            voxel = Voxel{MAT_AIR, 0};
            return voxel;
          }
          if (y == ceil_y || (c.type == CorridorType::SlopingRamp && y == ceil_y + 1)) {
            if (!is_in_any_room_interior(x, z)) {
              uint8_t ceil_mat = c.is_ravine ? MAT_FRACTURED_GRANITE
                                             : ((m_sector_index >= 2) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE);
              voxel = Voxel{ceil_mat, 0};
              return voxel;
            }
          }
        }
      } else if (c.is_x_axis) {
        int min_x = std::min(c.start_pos.x, c.end_pos.x);
        int max_x = std::max(c.start_pos.x, c.end_pos.x);
        if (x >= min_x && x <= max_x) {
          float t = (max_x > min_x) ? static_cast<float>(x - min_x) /
                                          static_cast<float>(max_x - min_x)
                                    : 0.0f;
          if (c.start_pos.x > c.end_pos.x)
            t = 1.0f - t;
          float center_z = static_cast<float>(c.start_pos.z) +
                           t * static_cast<float>(c.end_pos.z - c.start_pos.z);
          float lat_dist = std::abs(static_cast<float>(z) - center_z);
          float eff_width = static_cast<float>(c.half_width);
          if (c.is_ravine) {
            eff_width += (pseudo_rand(x, y, z, 88) - 0.5f) * 1.5f;
          } else {
            eff_width += (pseudo_rand(x, y, z, 88) - 0.5f) * 0.85f;
          }
          eff_width = std::max(1.8f, eff_width);

          if (lat_dist <= eff_width) {

            int floor_y = static_cast<int>(
                std::round(static_cast<float>(c.floor_y) +
                           t * static_cast<float>(c.end_floor_y - c.floor_y)));
            int ceil_y = static_cast<int>(std::round(
                static_cast<float>(c.ceiling_y) +
                t * static_cast<float>(c.end_ceiling_y - c.ceiling_y)));

            if (c.is_ravine) {
              if (sample_ravine_corridor(x, y, z, t, lat_dist,
                                         eff_width,
                                         floor_y, ceil_y, voxel)) {
                return voxel;
              }
            }

            if (y == floor_y || (c.type == CorridorType::SlopingRamp && y == floor_y - 1)) {
              if (!is_in_chasm_abyss(x, y, z)) {
                voxel = Voxel{MAT_VOLCANIC_BASALT, 0};
                return voxel;
              }
            }
            if (y > floor_y && y < ceil_y) {
              int mid_x = (c.start_pos.x + c.end_pos.x) / 2;
              if (c.has_bulkhead_door && x == mid_x) {
                if (lat_dist >= 1.8f || y >= ceil_y - 2) {
                  voxel = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
                  return voxel;
                }
              }
              if (c.has_rubble_collapse && (x == mid_x || x == mid_x + 1) &&
                  y <= floor_y + 1 && lat_dist >= 1.8f) {
                voxel = Voxel{MAT_FRACTURED_GRANITE, 0};
                return voxel;
              }

              bool is_arch =
                  (x % 6 == 0) || (x == c.start_pos.x) || (x == c.end_pos.x);
              if (is_arch &&
                  (lat_dist >= eff_width - 0.5f ||
                   y == ceil_y - 1)) {
                if (!is_in_any_room_interior(x, z)) {
                  uint8_t arch_mat = c.is_ravine ? MAT_FRACTURED_GRANITE
                                                 : ((m_sector_index >= 2) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE);
                  voxel = Voxel{arch_mat, 0};
                  return voxel;
                }
              }

              voxel = Voxel{MAT_AIR, 0};
              return voxel;
            }
            if (y == ceil_y || (c.type == CorridorType::SlopingRamp && y == ceil_y + 1)) {
              if (!is_in_any_room_interior(x, z)) {
                uint8_t ceil_mat = c.is_ravine ? MAT_FRACTURED_GRANITE
                                               : ((m_sector_index >= 2) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE);
                voxel = Voxel{ceil_mat, 0};
                return voxel;
              }
            }
          }
        }
      } else { // Z axis
        int min_z = std::min(c.start_pos.z, c.end_pos.z);
        int max_z = std::max(c.start_pos.z, c.end_pos.z);
        if (z >= min_z && z <= max_z) {
          float t = (max_z > min_z) ? static_cast<float>(z - min_z) /
                                          static_cast<float>(max_z - min_z)
                                    : 0.0f;
          if (c.start_pos.z > c.end_pos.z)
            t = 1.0f - t;
          float center_x = static_cast<float>(c.start_pos.x) +
                           t * static_cast<float>(c.end_pos.x - c.start_pos.x);
          float lat_dist = std::abs(static_cast<float>(x) - center_x);
          float eff_width = static_cast<float>(c.half_width);
          if (c.is_ravine) {
            eff_width += (pseudo_rand(x, y, z, 99) - 0.5f) * 1.5f;
          } else {
            eff_width += (pseudo_rand(x, y, z, 99) - 0.5f) * 0.85f;
          }
          eff_width = std::max(1.8f, eff_width);

          if (lat_dist <= eff_width) {

            int floor_y = static_cast<int>(
                std::round(static_cast<float>(c.floor_y) +
                           t * static_cast<float>(c.end_floor_y - c.floor_y)));
            int ceil_y = static_cast<int>(std::round(
                static_cast<float>(c.ceiling_y) +
                t * static_cast<float>(c.end_ceiling_y - c.ceiling_y)));

            if (c.is_ravine) {
              if (sample_ravine_corridor(x, y, z, t, lat_dist,
                                         eff_width,
                                         floor_y, ceil_y, voxel)) {
                return voxel;
              }
            }

            if (y == floor_y || (c.type == CorridorType::SlopingRamp && y == floor_y - 1)) {
              if (!is_in_chasm_abyss(x, y, z)) {
                voxel = Voxel{MAT_VOLCANIC_BASALT, 0};
                return voxel;
              }
            }
            if (y > floor_y && y < ceil_y) {
              int mid_z = (c.start_pos.z + c.end_pos.z) / 2;
              if (c.has_bulkhead_door && z == mid_z) {
                if (lat_dist >= 1.8f || y >= ceil_y - 2) {
                  voxel = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
                  return voxel;
                }
              }
              if (c.has_rubble_collapse && (z == mid_z || z == mid_z + 1) &&
                  y <= floor_y + 1 && lat_dist >= 1.8f) {
                voxel = Voxel{MAT_FRACTURED_GRANITE, 0};
                return voxel;
              }

              bool is_arch =
                  (z % 6 == 0) || (z == c.start_pos.z) || (z == c.end_pos.z);
              if (is_arch &&
                  (lat_dist >= eff_width - 0.5f ||
                   y == ceil_y - 1)) {
                if (!is_in_any_room_interior(x, z)) {
                  uint8_t arch_mat = c.is_ravine ? MAT_FRACTURED_GRANITE
                                                 : ((m_sector_index >= 2) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE);
                  voxel = Voxel{arch_mat, 0};
                  return voxel;
                }
              }

              voxel = Voxel{MAT_AIR, 0};
              return voxel;
            }
            if (y == ceil_y || (c.type == CorridorType::SlopingRamp && y == ceil_y + 1)) {
              if (!is_in_any_room_interior(x, z)) {
                uint8_t ceil_mat = c.is_ravine ? MAT_FRACTURED_GRANITE
                                               : ((m_sector_index >= 2) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE);
                voxel = Voxel{ceil_mat, 0};
                return voxel;
              }
            }
          }
        }
      }
    }

  // 6. Check Grid Rooms
  for (const auto &room : m_rooms) {
    int dx = std::abs(x - room.center.x);
    int dz = std::abs(z - room.center.z);

    int min_y = room.floor_y;
    if (room.type == RoomShapeType::FaultLineCrevasse ||
        room.type == RoomShapeType::CrumblingArchCanyon ||
        room.type == RoomShapeType::SpikeTrenchArena ||
        room.type == RoomShapeType::AbyssalVerticalChasm ||
        room.type == RoomShapeType::ColossalAbyssalChasm ||
        room.type == RoomShapeType::TectonicAbyssalSinkhole) {
      min_y = 4;
    } else if (room.type == RoomShapeType::VoidSingularityRift) {
      min_y = 1;
    }
    if (room.has_random_fluid) {
      min_y = std::min(min_y, room.floor_y - 1);
    }

    if (dx <= room.half_width && dz <= room.half_depth && y >= min_y &&
        y <= room.ceiling_y) {

      if (room.has_random_fluid && sample_random_fluid_structure(room, x, y, z, voxel)) {
        return voxel;
      }

      if (y < room.floor_y &&
          room.type != RoomShapeType::FaultLineCrevasse &&
          room.type != RoomShapeType::CrumblingArchCanyon &&
          room.type != RoomShapeType::SpikeTrenchArena &&
          room.type != RoomShapeType::AbyssalVerticalChasm &&
          room.type != RoomShapeType::ColossalAbyssalChasm &&
          room.type != RoomShapeType::TectonicAbyssalSinkhole &&
          room.type != RoomShapeType::VoidSingularityRift) {
        voxel = Voxel{MAT_VOLCANIC_BASALT, 0};
        return voxel;
      }

      switch (room.type) {
      case RoomShapeType::SpawnStagingCavern:
        sample_spawn_cavern(room, x, y, z, voxel);
        break;
      case RoomShapeType::MiningPillarHall:
        sample_pillar_hall(room, x, y, z, voxel);
        break;
      case RoomShapeType::CrystallineGeode:
        sample_crystalline_geode(room, x, y, z, voxel);
        break;
      case RoomShapeType::TerracedQuarry:
        sample_terraced_quarry(room, x, y, z, voxel);
        break;
      case RoomShapeType::IndustrialVaultBunker:
        sample_vault_bunker(room, x, y, z, voxel);
        break;
      case RoomShapeType::FaultLineCrevasse:
        sample_fault_crevasse(room, x, y, z, voxel);
        break;
      case RoomShapeType::AbyssalVerticalChasm:
        sample_abyssal_chasm(room, x, y, z, voxel);
        break;
      case RoomShapeType::RadioactiveCoreSanctuary:
        sample_radioactive_sanctuary(room, x, y, z, voxel);
        break;
      case RoomShapeType::ExtractionLandingBay:
        sample_extraction_bay(room, x, y, z, voxel);
        break;
      case RoomShapeType::MagmaCalderaLake:
        sample_magma_caldera(room, x, y, z, voxel);
        break;
      case RoomShapeType::SpikeTrenchArena:
        sample_spike_trench(room, x, y, z, voxel);
        break;
      case RoomShapeType::VoidSingularityRift:
        sample_void_singularity(room, x, y, z, voxel);
        break;
      case RoomShapeType::FungoidBioGrotto:
        sample_fungoid_grotto(room, x, y, z, voxel);
        break;
      case RoomShapeType::LaserDefenseFoundry:
        sample_laser_foundry(room, x, y, z, voxel);
        break;
      case RoomShapeType::CrumblingArchCanyon:
        sample_crumbling_canyon(room, x, y, z, voxel);
        break;
      case RoomShapeType::SubterraneanAquiferOasis:
        sample_aquifer_oasis(room, x, y, z, voxel);
        break;
      case RoomShapeType::ColossalAbyssalChasm:
        sample_colossal_abyssal_chasm(room, x, y, z, voxel);
        break;
      case RoomShapeType::MoltenMagmaFoundry:
        sample_molten_magma_foundry(room, x, y, z, voxel);
        break;
      case RoomShapeType::ToxicMiasmaSwamp:
        sample_toxic_miasma_swamp(room, x, y, z, voxel);
        break;
      case RoomShapeType::PrismaticCrystalCathedral:
        sample_prismatic_crystal_cathedral(room, x, y, z, voxel);
        break;
      case RoomShapeType::AncientTitanNecropolis:
        sample_ancient_titan_necropolis(room, x, y, z, voxel);
        break;
      case RoomShapeType::BioluminescentGlowwormGrotto:
        sample_bioluminescent_glowworm_grotto(room, x, y, z, voxel);
        break;
      case RoomShapeType::PrecursorCoolantReservoir:
        sample_precursor_coolant_reservoir(room, x, y, z, voxel);
        break;
      case RoomShapeType::ColossalVaultedDredgeCathedral:
        sample_vaulted_dredge_cathedral(room, x, y, z, voxel);
        break;
      case RoomShapeType::TectonicAbyssalSinkhole:
        sample_tectonic_sinkhole(room, x, y, z, voxel);
        break;
      case RoomShapeType::CyclopeanExcavationSilo:
        sample_cyclopean_silo(room, x, y, z, voxel);
        break;
      case RoomShapeType::BioluminescentFirmamentAbyss:
        sample_firmament_abyss(room, x, y, z, voxel);
        break;
      case RoomShapeType::PrecursorSanctumVault:
        sample_precursor_sanctum_vault(room, x, y, z, voxel);
        break;
      case RoomShapeType::SubterraneanVaultBastion:
        sample_subterranean_vault_bastion(room, x, y, z, voxel);
        break;
      case RoomShapeType::AbyssalVaultChasm:
        sample_abyssal_vault_chasm(room, x, y, z, voxel);
        break;
      }

      // If room hosts designated vault and has obstacle in approach zone
      if (room.has_vault && m_vault_placement.exists) {
        const auto &vp = m_vault_placement;
        int app_dist_x = std::abs(x - vp.approach_pos.x);
        int app_dist_z = std::abs(z - vp.approach_pos.z);
        if (app_dist_x <= 2 && app_dist_z <= 2) {
          if (vp.obstacle_type == VaultObstacleType::AquiferSteppingStones) {
            // Sector 1: Precursor crystal moat with stepping stone pathway
            if (y == room.floor_y) {
              if (x == vp.approach_pos.x || z == vp.approach_pos.z) {
                voxel = Voxel{MAT_PRECURSOR_STONE, VOXEL_FLAG_ANCHORED};
              } else {
                voxel = Voxel{MAT_BIOLUMINESCENT_FLORA, 0};
              }
            }
          } else if (vp.obstacle_type == VaultObstacleType::HazardTrenchSlag) {
            // Sector 2: Obsidian spike hazard field & titanium walkway
            if (y == room.floor_y) {
              if (x == vp.approach_pos.x || z == vp.approach_pos.z) {
                voxel = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
              } else {
                voxel = Voxel{MAT_OBSIDIAN_SPIKES, 0};
              }
            }
          } else if (vp.obstacle_type == VaultObstacleType::ChasmAbyssRadiation) {
            // Sector 3: Irradiated core field & basalt stepping island
            if (y == room.floor_y) {
              if (x == vp.approach_pos.x || z == vp.approach_pos.z) {
                voxel = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
              } else {
                voxel = Voxel{MAT_RADIOACTIVE_ORE, VOXEL_FLAG_EMISSIVE};
              }
            }
          }
        }
      }

      return voxel;
    }
  }

  // 7. Procedural Mineral Seam Generation in Host Rock Matrix (Sprint 2:
  // Item 3.3)
  if (y >= 4 && y <= 25) {
    // Coherent 3D vein clustering via 3x2x3 cell hashing
    int cell_x = x / 3;
    int cell_y = y / 2;
    int cell_z = z / 3;
    uint32_t vein_cluster = hash_coord(cell_x, cell_y, cell_z, 7331);
    int vein_chance =
        (m_sector_index >= 3) ? 6 : ((m_sector_index == 2) ? 5 : 4);
    if ((vein_cluster % 100) < static_cast<uint32_t>(vein_chance)) {
      uint32_t detail_hash = hash_coord(x, y, z, 9127);
      if ((detail_hash % 10) <
          6) { // 60% density within cluster -> organic 3-6 block streaks
        // CRITICAL INVARIANT: In a given vein, only that specific ore is allowed to spawn there!
        // Deterministically roll the vein material ONCE per cell_x, cell_y, cell_z cluster:
        uint32_t vein_seed = hash_coord(cell_x, cell_y, cell_z, 4513);
        uint8_t ore_mat = sample_vein_material(vein_seed);
        uint8_t flags = (ore_mat == MAT_TITANIUM) ? 0 : VOXEL_FLAG_EMISSIVE;
        if (out_is_vein) {
          *out_is_vein = true;
        }
        return Voxel{ore_mat, flags};
      }
    }
  }

  return voxel;
}

// ─────────────────────────────────────────────────────────────

bool LevelGenerator::sample_random_fluid_structure(
    const RoomPlacement &room, int x, int y, int z, Voxel &out) const {
  if (!room.has_random_fluid)
    return false;

  int r_dx = x - room.center.x;
  int r_dz = z - room.center.z;
  // Strictly prevent fluids from encroaching into cardinal doorway corridors or center thoroughfares
  if ((std::abs(r_dx) <= 2 && (r_dz >= room.half_depth - 2 || r_dz <= -room.half_depth + 2)) ||
      (std::abs(r_dz) <= 2 && (r_dx >= room.half_width - 2 || r_dx <= -room.half_width + 2)) ||
      (r_dx == 0 && r_dz == 0)) {
    return false;
  }

  switch (room.fluid_structure_type) {
  case FluidStructureType::ContainedPool:
    return sample_fluid_contained_pool(room, x, y, z, out);
  case FluidStructureType::SteppingStonePool:
    return sample_fluid_stepping_stone_pool(room, x, y, z, out);
  case FluidStructureType::WallCascade:
    return sample_fluid_wall_cascade(room, x, y, z, out);
  case FluidStructureType::CascadingTwinPools:
    return sample_fluid_cascading_twin_pools(room, x, y, z, out);
  case FluidStructureType::HazardTrench:
    return sample_fluid_hazard_trench(room, x, y, z, out);
  case FluidStructureType::DropChasmSump:
    return sample_fluid_drop_chasm_sump(room, x, y, z, out);
  default:
    return sample_fluid_contained_pool(room, x, y, z, out);
  }
}

bool LevelGenerator::sample_fluid_contained_pool(const RoomPlacement &room,
                                                int x, int y, int z,
                                                Voxel &out) const {
  float p_dx = static_cast<float>(x) -
               (static_cast<float>(room.center.x) + room.random_fluid_offset_x);
  float p_dz = static_cast<float>(z) -
               (static_cast<float>(room.center.z) + room.random_fluid_offset_z);
  float dist = std::sqrt(p_dx * p_dx + p_dz * p_dz);
  float r_pool = room.random_fluid_radius;
  float r_rim = r_pool + 1.2f;

  if (dist > r_rim) {
    return false;
  }

  // Underneath the basin: solid basalt bed providing complete vertical support
  if (y == room.floor_y - 1) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return true;
  }

  if (y < room.floor_y - 1) {
    return false;
  }

  // Inside the fluid pool
  if (dist <= r_pool) {
    if (y == room.floor_y) {
      out = Voxel{room.random_fluid_material, 0};
      return true;
    }
    if (y > room.floor_y && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Raised retaining rim enclosing the pool
  if (dist > r_pool && dist <= r_rim) {
    if (y == room.floor_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y == room.floor_y + 1) {
      out = Voxel{room.random_fluid_rim_material, 0};
      return true;
    }
    if (y > room.floor_y + 1 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  return false;
}

bool LevelGenerator::sample_fluid_stepping_stone_pool(
    const RoomPlacement &room, int x, int y, int z, Voxel &out) const {
  float p_dx = static_cast<float>(x) -
               (static_cast<float>(room.center.x) + room.random_fluid_offset_x);
  float p_dz = static_cast<float>(z) -
               (static_cast<float>(room.center.z) + room.random_fluid_offset_z);
  float dist = std::sqrt(p_dx * p_dx + p_dz * p_dz);
  float r_pool = std::max(2.1f, room.random_fluid_radius + 0.6f);
  float r_rim = r_pool + 1.2f;

  if (dist > r_rim) {
    return false;
  }

  if (y == room.floor_y - 1) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return true;
  }

  if (y < room.floor_y - 1) {
    return false;
  }

  // Central island with natural rock mound
  if (dist <= 0.75f) {
    if (y >= room.floor_y && y <= room.floor_y + 2) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y > room.floor_y + 2 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Stepping stone hopping path leading to island
  bool is_stone = (p_dx >= 1.0f && p_dx <= 1.7f && std::abs(p_dz) <= 0.5f);
  if (is_stone) {
    if (y == room.floor_y || y == room.floor_y + 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y > room.floor_y + 1 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Inside fluid pool
  if (dist <= r_pool) {
    if (y == room.floor_y) {
      out = Voxel{room.random_fluid_material, 0};
      return true;
    }
    if (y > room.floor_y && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Raised retaining rim
  if (dist > r_pool && dist <= r_rim) {
    if (y == room.floor_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y == room.floor_y + 1) {
      out = Voxel{room.random_fluid_rim_material, 0};
      return true;
    }
    if (y > room.floor_y + 1 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  return false;
}

bool LevelGenerator::sample_fluid_wall_cascade(const RoomPlacement &room, int x,
                                              int y, int z, Voxel &out) const {
  float c_x = static_cast<float>(room.center.x) + room.random_fluid_offset_x;
  float c_z = static_cast<float>(room.center.z) + room.random_fluid_offset_z;
  float p_dx = static_cast<float>(x) - c_x;
  float p_dz = static_cast<float>(z) - c_z;
  float dist_basin = std::sqrt(p_dx * p_dx + p_dz * p_dz);
  float r_pool = 1.4f;
  float r_rim = r_pool + 1.2f;

  bool along_x = (std::abs(room.random_fluid_offset_x) >= std::abs(room.random_fluid_offset_z));
  float normal = along_x ? p_dx : p_dz;
  float tangent = along_x ? p_dz : p_dx;
  if ((along_x && room.random_fluid_offset_x < 0) || (!along_x && room.random_fluid_offset_z < 0)) {
    normal = -normal;
  }

  // Basin bed
  if (y == room.floor_y - 1 && dist_basin <= r_rim) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return true;
  }
  if (y < room.floor_y - 1) {
    return false;
  }

  // Decorative Wall Cascade Fountain Structure at the wall side
  int cascade_top_y = std::min(room.floor_y + 3, room.ceiling_y - 3);
  bool in_wall_flume = (normal >= 0.5f && normal <= 2.2f && std::abs(tangent) <= 1.2f);
  bool in_source_spout = (normal >= 0.8f && normal <= 1.8f && std::abs(tangent) <= 0.6f);

  if (in_wall_flume && y >= room.floor_y + 1 && y <= cascade_top_y) {
    if (in_source_spout && y == cascade_top_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return true;
  }

  // Plunge pool basin at room.floor_y
  if (dist_basin <= r_rim) {
    if (dist_basin <= r_pool) {
      if (y == room.floor_y) {
        out = Voxel{room.random_fluid_material, 0};
        return true;
      }
      if (y > room.floor_y && y < room.ceiling_y - 2 && !in_wall_flume) {
        out = Voxel{MAT_AIR, 0};
        return true;
      }
    }
    if (dist_basin > r_pool && dist_basin <= r_rim) {
      if (y == room.floor_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return true;
      }
      if (y == room.floor_y + 1 && !in_wall_flume) {
        out = Voxel{room.random_fluid_rim_material, 0};
        return true;
      }
      if (y > room.floor_y + 1 && y < room.ceiling_y - 2 && !in_wall_flume) {
        out = Voxel{MAT_AIR, 0};
        return true;
      }
    }
  }

  return false;
}

bool LevelGenerator::sample_fluid_cascading_twin_pools(
    const RoomPlacement &room, int x, int y, int z, Voxel &out) const {
  float c1_x = static_cast<float>(room.center.x) + room.random_fluid_offset_x;
  float c1_z = static_cast<float>(room.center.z) + room.random_fluid_offset_z;
  float c2_x = c1_x + (room.fluid_orientation == 0 ? (room.random_fluid_offset_x > 0 ? -2.2f : 2.2f) : 0.0f);
  float c2_z = c1_z + (room.fluid_orientation == 1 ? (room.random_fluid_offset_z > 0 ? -2.2f : 2.2f) : 0.0f);

  float d1 = std::sqrt((x - c1_x) * (x - c1_x) + (z - c1_z) * (z - c1_z));
  float d2 = std::sqrt((x - c2_x) * (x - c2_x) + (z - c2_z) * (z - c2_z));
  float r_pool = 1.3f;
  float r_rim = r_pool + 1.2f;

  if (d1 > r_rim && d2 > r_rim) {
    return false;
  }

  // Bed under both twin basins
  if (y == room.floor_y - 1) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return true;
  }
  if (y < room.floor_y - 1) {
    return false;
  }

  bool in_pool_1 = (d1 <= r_pool);
  bool in_pool_2 = (d2 <= r_pool);

  // Twin pool liquid surfaces at room.floor_y
  if (in_pool_1 || in_pool_2) {
    if (y == room.floor_y) {
      out = Voxel{room.random_fluid_material, 0};
      return true;
    }
    if (y > room.floor_y && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Retaining perimeter rim / dividing weir ridge between basins
  if (y == room.floor_y) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return true;
  }
  if (y == room.floor_y + 1) {
    out = Voxel{room.random_fluid_rim_material, 0};
    return true;
  }
  if (y > room.floor_y + 1 && y < room.ceiling_y - 2) {
    out = Voxel{MAT_AIR, 0};
    return true;
  }

  return false;
}

bool LevelGenerator::sample_fluid_hazard_trench(const RoomPlacement &room,
                                                int x, int y, int z,
                                                Voxel &out) const {
  float c_x = static_cast<float>(room.center.x) + room.random_fluid_offset_x;
  float c_z = static_cast<float>(room.center.z) + room.random_fluid_offset_z;
  float dx = static_cast<float>(x) - c_x;
  float dz = static_cast<float>(z) - c_z;

  bool along_x = (room.fluid_orientation == 0);
  float u = along_x ? dx : dz;
  float v = along_x ? dz : dx;

  float half_len = std::clamp(room.random_fluid_length * 0.5f, 2.0f, 3.0f);
  float half_w = 1.1f;
  float rim_len = half_len + 1.2f;
  float rim_w = half_w + 1.2f;

  if (std::abs(u) > rim_len || std::abs(v) > rim_w) {
    return false;
  }

  if (y == room.floor_y - 1) {
    out = Voxel{MAT_VOLCANIC_BASALT, 0};
    return true;
  }
  if (y < room.floor_y - 1) {
    return false;
  }

  // Midpoint crossing stepping stone bridge
  bool is_bridge_step = (std::abs(u) <= 0.6f && std::abs(v) <= half_w);
  if (is_bridge_step) {
    if (y == room.floor_y || y == room.floor_y + 1) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y > room.floor_y + 1 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Inside fluid hazard trench
  if (std::abs(u) <= half_len && std::abs(v) <= half_w) {
    if (y == room.floor_y) {
      out = Voxel{room.random_fluid_material, 0};
      return true;
    }
    if (y > room.floor_y && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Retaining curbs enclosing trench
  if (std::abs(u) <= rim_len && std::abs(v) <= rim_w) {
    if (y == room.floor_y) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y == room.floor_y + 1) {
      out = Voxel{room.random_fluid_rim_material, 0};
      return true;
    }
    if (y > room.floor_y + 1 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  return false;
}

bool LevelGenerator::sample_fluid_drop_chasm_sump(const RoomPlacement &room,
                                                  int x, int y, int z,
                                                  Voxel &out) const {
  float p_dx = static_cast<float>(x - room.center.x);
  float p_dz = static_cast<float>(z - room.center.z);
  float dist = std::sqrt(p_dx * p_dx + p_dz * p_dz);
  float r_sump = std::clamp(static_cast<float>(room.half_width) - 4.5f, 2.5f, 4.5f);
  float r_rim = r_sump + 1.2f;

  if (dist > r_rim) {
    return false;
  }

  if (y <= 3) {
    out = Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
    return true;
  }

  // Center drop landing island under stalactite axis (guarantees center is solid!)
  if (dist <= 0.85f) {
    if (y == 4 || y == 5) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y == 6) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y > 6 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Stepping stone island
  bool is_stepping_stone = (p_dx >= 1.5f && p_dx <= 2.3f && std::abs(p_dz) <= 0.6f);
  if (is_stepping_stone) {
    if (y == 4 || y == 5) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y > 5 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Fluid basin at y == 4
  if (dist <= r_sump) {
    if (y == 4) {
      out = Voxel{room.random_fluid_material, 0};
      return true;
    }
    if (y > 4 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  // Perimeter retaining curb / walkway
  if (dist > r_sump && dist <= r_rim) {
    if (y == 4 || y == 5) {
      out = Voxel{MAT_VOLCANIC_BASALT, 0};
      return true;
    }
    if (y > 5 && y < room.ceiling_y - 2) {
      out = Voxel{MAT_AIR, 0};
      return true;
    }
  }

  return false;
}

bool LevelGenerator::is_dark_zone_at(const glm::vec3 &pos) const {
  const RoomPlacement *room = get_room_at_position(pos);
  if (room) {
    return room->is_dark_zone;
  }
  // If not strictly inside a room, find the nearest room
  float min_dist_sq = 1e9f;
  const RoomPlacement *nearest = nullptr;
  for (const auto &r : m_rooms) {
    float dx = pos.x - static_cast<float>(r.center.x);
    float dz = pos.z - static_cast<float>(r.center.z);
    float d2 = dx * dx + dz * dz;
    if (d2 < min_dist_sq) {
      min_dist_sq = d2;
      nearest = &r;
    }
  }
  return nearest ? nearest->is_dark_zone : false;
}

float LevelGenerator::get_darkness_at(const glm::vec3 &pos) const {
  const RoomPlacement *room = get_room_at_position(pos);
  if (room) {
    return room->darkness_factor;
  }
  // In corridors or transitional rock, blend smoothly between nearby rooms
  float total_weight = 0.0f;
  float blended_darkness = 0.0f;
  for (const auto &r : m_rooms) {
    float dx = pos.x - static_cast<float>(r.center.x);
    float dz = pos.z - static_cast<float>(r.center.z);
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < 40.0f) {
      float w = std::max(0.0f, 1.0f - dist / 40.0f);
      w *= w;
      blended_darkness += r.darkness_factor * w;
      total_weight += w;
    }
  }
  if (total_weight > 0.0001f) {
    return std::clamp(blended_darkness / total_weight, 0.0f, 1.0f);
  }
  return (m_sector_index >= 3) ? 0.85f : (m_sector_index == 2 ? 0.40f : 0.0f);
}

int LevelGenerator::dark_room_count() const {
  int count = 0;
  for (const auto &r : m_rooms) {
    if (r.is_dark_zone) {
      count++;
    }
  }
  return count;
}

float LevelGenerator::sector_darkness_ratio() const {
  if (m_rooms.empty()) return 0.0f;
  return static_cast<float>(dark_room_count()) / static_cast<float>(m_rooms.size());
}


} // namespace Voidfall
