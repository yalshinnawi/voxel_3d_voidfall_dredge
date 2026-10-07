#include "level_shapes.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <unordered_map>

namespace Voidfall {

LevelGenerator::LevelGenerator(int sector_index, uint32_t seed, int vertical_chunks)
    : m_sector_index(sector_index), m_seed(seed), m_world_h(vertical_chunks > 1 ? vertical_chunks * 32 : WORLD_HEIGHT)
{
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

const RoomPlacement* LevelGenerator::get_room_at_grid(int gx, int gz) const {
    if (gx < 0 || gx >= m_grid_w || gz < 0 || gz >= m_grid_d) return nullptr;
    int idx = m_grid_room_indices[gx][gz];
    if (idx >= 0 && idx < static_cast<int>(m_rooms.size())) {
        return &m_rooms[idx];
    }
    return nullptr;
}

const RoomPlacement* LevelGenerator::get_room_at_position(const glm::vec3& pos) const {
    for (const auto& room : m_rooms) {
        if (pos.x >= static_cast<float>(room.center.x - room.half_width) &&
            pos.x <= static_cast<float>(room.center.x + room.half_width) &&
            pos.z >= static_cast<float>(room.center.z - room.half_depth) &&
            pos.z <= static_cast<float>(room.center.z + room.half_depth)) {
            return &room;
        }
    }
    return nullptr;
}

glm::vec3 LevelGenerator::spawn_position() const {
    const RoomPlacement* r = get_room_at_grid(0, 0);
    if (r) {
        return glm::vec3(r->center.x, static_cast<float>(r->floor_y + 1) + 0.1f, r->center.z);
    }
    return glm::vec3(16.0f, 5.1f, 16.0f);
}

glm::vec3 LevelGenerator::extraction_position() const {
    const RoomPlacement* r = get_room_at_grid(m_grid_w - 1, m_grid_d - 1);
    if (r) {
        return glm::vec3(r->center.x, static_cast<float>(r->floor_y + 1), r->center.z);
    }
    return glm::vec3(static_cast<float>(world_width() - 16), 5.0f, static_cast<float>(world_depth() - 16));
}

bool LevelGenerator::is_in_bounds(int x, int y, int z) const {
    return (x >= 2 && x <= world_width() - 3 &&
            z >= 2 && z <= world_depth() - 3 &&
            y >= 1 && y <= m_world_h - 5);
}

void LevelGenerator::create_single_room_test_layout(RoomShapeType type) {
    m_rooms.clear();
    m_corridors.clear();
    m_grid_w = 3;
    m_grid_d = 3;
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
    generate_luminaries();
}

void LevelGenerator::create_corridor_test_layout() {
    m_rooms.clear();
    m_corridors.clear();
    m_grid_w = 3;
    m_grid_d = 3;
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
    if (gx < 0 || gx >= m_grid_w || gz < 0 || gz >= m_grid_d) return;
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

    // 1. Determine randomised massive & soaring vertical room locations per sector
    // Sector 1 (3x3): 1 massive room at central grand chamber (1, 1)
    // Sector 2 (4x4): 2-3 massive rooms randomly chosen from interior slots {(1,1), (1,2), (2,1), (2,2)}
    // Sector 3 (5x5): 4-6 massive rooms randomly chosen from the 9 interior slots (1..3, 1..3)
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
    for (int i = 0; i < std::min(target_massive_count, static_cast<int>(candidate_slots.size())); ++i) {
        is_massive_slot[candidate_slots[i].first][candidate_slots[i].second] = true;
    }

    // 2. Build Room Archetype Pools with Weighted Probabilistic Selection & Seed-Based Flavor Variation
    struct ArchetypeWeight {
        RoomShapeType type;
        int weight;
        int max_count;
    };

    std::vector<ArchetypeWeight> archetype_weights;
    std::vector<RoomShapeType> guaranteed_types;

    if (m_sector_index <= 1) {
        // Sector 1: Natural caverns, geodes, quarries, grottos, crystal cathedrals, vast hollows & silos
        archetype_weights = {
            {RoomShapeType::ColossalVaultedDredgeCathedral, 16, 2},
            {RoomShapeType::BioluminescentFirmamentAbyss,   16, 2},
            {RoomShapeType::MiningPillarHall,               16, 2},
            {RoomShapeType::TerracedQuarry,                 15, 2},
            {RoomShapeType::CrystallineGeode,               15, 2},
            {RoomShapeType::SubterraneanAquiferOasis,       15, 2},
            {RoomShapeType::CyclopeanExcavationSilo,         14, 2},
            {RoomShapeType::PrismaticCrystalCathedral,      14, 2},
            {RoomShapeType::BioluminescentGlowwormGrotto,   14, 2},
            {RoomShapeType::FungoidBioGrotto,               13, 2},
            {RoomShapeType::CrumblingArchCanyon,            12, 2},
            {RoomShapeType::AncientTitanNecropolis,         10, 2}
        };
        // Guarantee at least 1 soaring vertical cathedral/firmament and 1 resource/crystalline cavern
        guaranteed_types.push_back((rng() % 2 == 0) ? RoomShapeType::ColossalVaultedDredgeCathedral : RoomShapeType::BioluminescentFirmamentAbyss);
        guaranteed_types.push_back((rng() % 2 == 0) ? RoomShapeType::MiningPillarHall : RoomShapeType::CrystallineGeode);
    } else if (m_sector_index == 2) {
        // Sector 2: Industrial silos, tectonic sinkholes, cathedrals, magma foundries, toxic miasma, ancient titan ruins
        archetype_weights = {
            {RoomShapeType::ColossalVaultedDredgeCathedral, 14, 2},
            {RoomShapeType::TectonicAbyssalSinkhole,        14, 2},
            {RoomShapeType::CyclopeanExcavationSilo,        14, 2},
            {RoomShapeType::BioluminescentFirmamentAbyss,   13, 2},
            {RoomShapeType::ColossalAbyssalChasm,           13, 2},
            {RoomShapeType::IndustrialVaultBunker,          12, 2},
            {RoomShapeType::MoltenMagmaFoundry,             12, 2},
            {RoomShapeType::ToxicMiasmaSwamp,               12, 2},
            {RoomShapeType::AncientTitanNecropolis,         12, 2},
            {RoomShapeType::PrismaticCrystalCathedral,      11, 2},
            {RoomShapeType::FaultLineCrevasse,              11, 2},
            {RoomShapeType::PrecursorCoolantReservoir,      11, 2},
            {RoomShapeType::MagmaCalderaLake,               10, 2},
            {RoomShapeType::SubterraneanAquiferOasis,       10, 2},
            {RoomShapeType::SpikeTrenchArena,               10, 2},
            {RoomShapeType::CrumblingArchCanyon,            10, 2},
            {RoomShapeType::LaserDefenseFoundry,            10, 2},
            {RoomShapeType::BioluminescentGlowwormGrotto,    9, 2},
            {RoomShapeType::FungoidBioGrotto,                8, 2}
        };
        // Guarantee at least 1 vast vertical landmark and 1 industrial/hazard arena
        guaranteed_types.push_back((rng() % 2 == 0) ? RoomShapeType::ColossalVaultedDredgeCathedral : RoomShapeType::TectonicAbyssalSinkhole);
        RoomShapeType s2_hazards[] = {
            RoomShapeType::MoltenMagmaFoundry,
            RoomShapeType::ToxicMiasmaSwamp,
            RoomShapeType::ColossalAbyssalChasm,
            RoomShapeType::CyclopeanExcavationSilo
        };
        guaranteed_types.push_back(s2_hazards[rng() % 4]);
    } else {
        // Sector 3+: Tectonic sinkholes, vaulted cathedrals, abyssal chasms, radioactive sanctuaries, void rifts
        archetype_weights = {
            {RoomShapeType::TectonicAbyssalSinkhole,        16, 3},
            {RoomShapeType::ColossalVaultedDredgeCathedral, 15, 3},
            {RoomShapeType::ColossalAbyssalChasm,           15, 3},
            {RoomShapeType::BioluminescentFirmamentAbyss,   14, 3},
            {RoomShapeType::CyclopeanExcavationSilo,        14, 3},
            {RoomShapeType::VoidSingularityRift,            14, 3},
            {RoomShapeType::RadioactiveCoreSanctuary,       14, 3},
            {RoomShapeType::AbyssalVerticalChasm,           13, 3},
            {RoomShapeType::AncientTitanNecropolis,         13, 2},
            {RoomShapeType::ToxicMiasmaSwamp,               12, 2},
            {RoomShapeType::MoltenMagmaFoundry,             12, 2},
            {RoomShapeType::PrecursorCoolantReservoir,      12, 2},
            {RoomShapeType::PrismaticCrystalCathedral,      12, 2},
            {RoomShapeType::MagmaCalderaLake,               10, 2},
            {RoomShapeType::SpikeTrenchArena,               10, 2},
            {RoomShapeType::LaserDefenseFoundry,            10, 2},
            {RoomShapeType::IndustrialVaultBunker,           8, 2},
            {RoomShapeType::FaultLineCrevasse,               8, 2},
            {RoomShapeType::SubterraneanAquiferOasis,        8, 2}
        };
        // Guarantee Sector 3 core archetypes + hazard
        guaranteed_types.push_back((rng() % 2 == 0) ? RoomShapeType::TectonicAbyssalSinkhole : RoomShapeType::ColossalAbyssalChasm);
        guaranteed_types.push_back(RoomShapeType::VoidSingularityRift);
        RoomShapeType s3_hazards[] = {
            RoomShapeType::MoltenMagmaFoundry,
            RoomShapeType::ToxicMiasmaSwamp,
            RoomShapeType::AncientTitanNecropolis,
            RoomShapeType::ColossalVaultedDredgeCathedral
        };
        guaranteed_types.push_back(s3_hazards[rng() % 4]);
    }

    std::vector<RoomShapeType> available_pool;
    std::unordered_map<int, int> archetype_counts;

    for (RoomShapeType g : guaranteed_types) {
        available_pool.push_back(g);
        archetype_counts[static_cast<int>(g)]++;
    }

    int total_variable_slots = (m_grid_w * m_grid_d) - 2;
    while (static_cast<int>(available_pool.size()) < total_variable_slots) {
        int total_weight = 0;
        for (const auto& aw : archetype_weights) {
            int current_cnt = archetype_counts[static_cast<int>(aw.type)];
            if (current_cnt < aw.max_count) {
                total_weight += aw.weight;
            }
        }
        if (total_weight <= 0) {
            available_pool.push_back(m_sector_index >= 3 ? RoomShapeType::VoidSingularityRift :
                                     (m_sector_index == 2 ? RoomShapeType::IndustrialVaultBunker : RoomShapeType::ColossalVaultedDredgeCathedral));
            continue;
        }

        int roll = static_cast<int>(rng() % total_weight);
        int accum = 0;
        for (const auto& aw : archetype_weights) {
            int current_cnt = archetype_counts[static_cast<int>(aw.type)];
            if (current_cnt < aw.max_count) {
                accum += aw.weight;
                if (roll < accum) {
                    available_pool.push_back(aw.type);
                    archetype_counts[static_cast<int>(aw.type)]++;
                    break;
                }
            }
        }
    }
    std::shuffle(available_pool.begin(), available_pool.end(), rng);

    // 3. Instantiate W x D Grid Rooms with Dynamic Sizing & Clustered Floor Assignment (Sprint 1: Item 3.5)
    int pool_idx = 0;
    std::vector<RoomPlacement> grid_rooms(m_grid_w * m_grid_d);
    std::vector<std::pair<int, int>> regular_slots;

    for (int gx = 0; gx < m_grid_w; ++gx) {
        for (int gz = 0; gz < m_grid_d; ++gz) {
            int idx = gx * m_grid_d + gz;
            RoomPlacement& room = grid_rooms[idx];
            room.grid_x = gx;
            room.grid_z = gz;

            if (gx == 0 && gz == 0) {
                room.type = RoomShapeType::SpawnStagingCavern;
                room.floor_level = 0;
                room.floor_y = 4;
                room.ceiling_y = 25;
            } else if (gx == m_grid_w - 1 && gz == m_grid_d - 1) {
                room.type = RoomShapeType::ExtractionLandingBay;
                room.floor_level = 2;
                room.floor_y = 4;
                room.ceiling_y = 25;
            } else {
                if (pool_idx < static_cast<int>(available_pool.size())) {
                    room.type = available_pool[pool_idx++];
                } else {
                    room.type = (m_sector_index >= 3) ? RoomShapeType::VoidSingularityRift :
                                ((m_sector_index == 2) ? RoomShapeType::IndustrialVaultBunker : RoomShapeType::ColossalVaultedDredgeCathedral);
                }

                if (is_massive_slot[gx][gz] ||
                    room.type == RoomShapeType::ColossalVaultedDredgeCathedral ||
                    room.type == RoomShapeType::TectonicAbyssalSinkhole ||
                    room.type == RoomShapeType::CyclopeanExcavationSilo ||
                    room.type == RoomShapeType::BioluminescentFirmamentAbyss ||
                    room.type == RoomShapeType::AbyssalVerticalChasm ||
                    room.type == RoomShapeType::VoidSingularityRift ||
                    room.type == RoomShapeType::RadioactiveCoreSanctuary ||
                    room.type == RoomShapeType::ColossalAbyssalChasm ||
                    room.type == RoomShapeType::PrismaticCrystalCathedral ||
                    room.type == RoomShapeType::AncientTitanNecropolis ||
                    room.type == RoomShapeType::SubterraneanAquiferOasis) {
                    room.is_massive = true;
                    // Mega-cavern status for key massive chambers or colossal chasms
                    if (is_massive_slot[gx][gz] && (room.type == RoomShapeType::ColossalVaultedDredgeCathedral ||
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
                    room.floor_level = 2;
                    room.floor_y = 4;
                    room.ceiling_y = 25;
                } else {
                    regular_slots.push_back({gx, gz});
                }
            }
        }
    }

    // Assign organic floor clusters for regular rooms (Elevation Biomes)
    if (!regular_slots.empty()) {
        int anchor0_idx = static_cast<int>(rng() % regular_slots.size());
        auto anchor0 = regular_slots[anchor0_idx];
        auto anchor1 = regular_slots[0];
        int max_dist = -1;
        for (const auto& s : regular_slots) {
            int d = std::abs(s.first - anchor0.first) + std::abs(s.second - anchor0.second);
            if (d > max_dist) {
                max_dist = d;
                anchor1 = s;
            }
        }

        int count_floor0 = 0;
        int count_floor1 = 0;

        for (const auto& s : regular_slots) {
            int gx = s.first;
            int gz = s.second;
            int r_idx = gx * m_grid_d + gz;
            RoomPlacement& room = grid_rooms[r_idx];

            int d0 = std::abs(gx - anchor0.first) + std::abs(gz - anchor0.second);
            int d1 = std::abs(gx - anchor1.first) + std::abs(gz - anchor1.second);
            int jitter = static_cast<int>(rng() % 3) - 1; // -1, 0, +1

            if (gx == anchor0.first && gz == anchor0.second) {
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
                // High-Vault Atrium variation: 40% chance of soaring 20-24m ceiling even in regular rooms!
                uint32_t ceil_roll = hash_coord(gx, gz, m_seed, 881) % 100;
                if (ceil_roll < 40) {
                    room.ceiling_y = 20 + static_cast<int>(rng() % 5); // 20 to 24m high ceiling!
                } else {
                    room.ceiling_y = 13 + static_cast<int>(rng() % 4); // 13 to 16m ceiling
                }
                count_floor0++;
            } else {
                room.floor_level = 1;
                room.floor_y = 14;
                room.ceiling_y = 25;
                count_floor1++;
            }
        }

        // Invariant guard: ensure at least one Floor 0 and one Floor 1 exist
        if (count_floor0 == 0 && !regular_slots.empty()) {
            int r_idx = anchor0.first * m_grid_d + anchor0.second;
            grid_rooms[r_idx].floor_level = 0;
            grid_rooms[r_idx].floor_y = 4;
            grid_rooms[r_idx].ceiling_y = 13;
        }
        if (count_floor1 == 0 && !regular_slots.empty()) {
            int r_idx = anchor1.first * m_grid_d + anchor1.second;
            grid_rooms[r_idx].floor_level = 1;
            grid_rooms[r_idx].floor_y = 14;
            grid_rooms[r_idx].ceiling_y = 25;
        }
    }

    // Finalize room geometry, dimensions & center placement
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
                // Massive cavern horizontal expanse (17x17 to 19x19), leaving 3-4 block corridor clearance
                room.half_width = 8 + static_cast<int>(rng() % 2); // 8 to 9
                room.half_depth = 8 + static_cast<int>(rng() % 2); // 8 to 9
                if (rng() % 2 == 0) std::swap(room.half_width, room.half_depth);
            } else {
                int sz = static_cast<int>(rng() % 3);
                if (sz == 0) {
                    // Small tight niche / outpost (guarantees has_small)
                    room.half_width = 5 + static_cast<int>(rng() % 2);  // 5 to 6
                    room.half_depth = 5 + static_cast<int>(rng() % 3);  // 5 to 7
                } else if (sz == 1) {
                    // Medium chamber
                    room.half_width = 7 + static_cast<int>(rng() % 2);  // 7 to 8
                    room.half_depth = 7 + static_cast<int>(rng() % 3);  // 7 to 9
                } else {
                    // Large hall / cavern
                    room.half_width = 8 + static_cast<int>(rng() % 2);  // 8 to 9
                    room.half_depth = 8 + static_cast<int>(rng() % 2);  // 8 to 9
                }
                // Organic aspect ratio asymmetry: swap width and depth randomly
                if (rng() % 2 == 0) {
                    std::swap(room.half_width, room.half_depth);
                }
            }

            // Grid-aligned room centers guarantee straight corridor linkages and unobstructed doorways
            room.center = glm::ivec3(grid_coords[gx], 0, grid_coords[gz]);

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
            if (rA_idx < 0 || rB_idx < 0) continue;
            RoomPlacement& roomA = m_rooms[rA_idx];
            RoomPlacement& roomB = m_rooms[rB_idx];

            CorridorPlacement c;
            c.is_x_axis = true;
            c.is_diagonal = false;

            int start_x = roomA.center.x + roomA.half_width - 1;
            int end_x   = roomB.center.x - roomB.half_width + 1;
            if (start_x >= end_x) {
                start_x = roomA.center.x + 3;
                end_x   = roomB.center.x - 3;
            }
            if (roomA.floor_y != roomB.floor_y) {
                int min_run = std::abs(roomB.floor_y - roomA.floor_y) + 2;
                while ((end_x - start_x) < min_run) {
                    if (start_x > roomA.center.x + 1) start_x--;
                    else if (end_x < roomB.center.x - 1) end_x++;
                    else break;
                }
            }
            c.start_pos = glm::ivec3(start_x, roomA.floor_y, roomA.center.z);
            c.end_pos   = glm::ivec3(end_x,   roomB.floor_y, roomB.center.z);

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

                if (rng() % 100 < 30) {
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
            if (rA_idx < 0 || rB_idx < 0) continue;
            RoomPlacement& roomA = m_rooms[rA_idx];
            RoomPlacement& roomB = m_rooms[rB_idx];

            CorridorPlacement c;
            c.is_x_axis = false;
            c.is_diagonal = false;

            int start_z = roomA.center.z + roomA.half_depth - 1;
            int end_z   = roomB.center.z - roomB.half_depth + 1;
            if (start_z >= end_z) {
                start_z = roomA.center.z + 3;
                end_z   = roomB.center.z - 3;
            }
            if (roomA.floor_y != roomB.floor_y) {
                int min_run = std::abs(roomB.floor_y - roomA.floor_y) + 2;
                while ((end_z - start_z) < min_run) {
                    if (start_z > roomA.center.z + 1) start_z--;
                    else if (end_z < roomB.center.z - 1) end_z++;
                    else break;
                }
            }
            c.start_pos = glm::ivec3(roomA.center.x, roomA.floor_y, start_z);
            c.end_pos   = glm::ivec3(roomB.center.x, roomB.floor_y, end_z);

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

                if (rng() % 100 < 30) {
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
            // Sector 1: guaranteed diagonal artery from (0,0) to (1,1) and (1,1) to (2,2)
            bool should_connect = (m_sector_index <= 1) ? true : (rng() % 100 < 55);
            if (!should_connect) continue;

            int rA_idx = m_grid_room_indices[gx][gz];
            int rB_idx = m_grid_room_indices[gx + 1][gz + 1];
            if (rA_idx < 0 || rB_idx < 0) continue;
            RoomPlacement& roomA = m_rooms[rA_idx];
            RoomPlacement& roomB = m_rooms[rB_idx];

            CorridorPlacement c;
            c.is_diagonal = true;
            c.is_x_axis = false;

            c.start_pos = glm::ivec3(roomA.center.x + roomA.half_width - 1, roomA.floor_y, roomA.center.z + roomA.half_depth - 1);
            c.end_pos   = glm::ivec3(roomB.center.x - roomB.half_width + 1, roomB.floor_y, roomB.center.z - roomB.half_depth + 1);

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
            // Prevent crossing diagonals: if the NE diagonal in this 2x2 cell is already connected, skip SE
            int ne_origin_idx = m_grid_room_indices[gx][gz - 1];
            if (ne_origin_idx >= 0 && m_rooms[ne_origin_idx].connected_ne) continue;

            bool should_connect = (rng() % 100 < 40);
            if (!should_connect) continue;

            int rA_idx = m_grid_room_indices[gx][gz];
            int rB_idx = m_grid_room_indices[gx + 1][gz - 1];
            if (rA_idx < 0 || rB_idx < 0) continue;
            RoomPlacement& roomA = m_rooms[rA_idx];
            RoomPlacement& roomB = m_rooms[rB_idx];

            CorridorPlacement c;
            c.is_diagonal = true;
            c.is_x_axis = false;

            c.start_pos = glm::ivec3(roomA.center.x + roomA.half_width - 1, roomA.floor_y, roomA.center.z - roomA.half_depth + 1);
            c.end_pos   = glm::ivec3(roomB.center.x - roomB.half_width + 1, roomB.floor_y, roomB.center.z + roomB.half_depth - 1);

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

    generate_luminaries();
}

void LevelGenerator::generate_luminaries() {
    m_luminaries.clear();
    m_luminaries.reserve(64);

    for (const auto& room : m_rooms) {
        switch (room.type) {
            case RoomShapeType::CrystallineGeode: {
                CavernLuminary core;
                core.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 3.0f), room.center.z);
                core.base_color = glm::vec3(0.42f, 0.18f, 0.78f);
                core.base_radius = 15.0f;
                core.base_intensity = 1.7f;
                core.type = LuminaryType::VoiditeCrystalGeode;
                core.pulse_speed = 1.8f;
                core.pulse_depth = 0.18f;
                core.name = "Voidite Geode Heart";
                m_luminaries.push_back(core);

                CavernLuminary high_cluster;
                high_cluster.position = glm::vec3(room.center.x + 3.5f, static_cast<float>(room.ceiling_y - 4.0f), room.center.z - 3.5f);
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
                core.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 3.0f), room.center.z);
                core.base_color = glm::vec3(0.12f, 0.98f, 0.35f);
                core.base_radius = 28.0f;
                core.base_intensity = 3.8f;
                core.type = LuminaryType::RadioactiveCore;
                core.pulse_speed = 4.0f;
                core.pulse_depth = 0.25f;
                core.flicker_rate = 12.0f;
                core.name = "Radioactive Core Monolith";
                m_luminaries.push_back(core);

                for (int i = 0; i < 2; ++i) {
                    float angle = glm::radians(i * 180.0f + 45.0f);
                    CavernLuminary vent;
                    vent.position = glm::vec3(room.center.x + std::cos(angle) * 4.5f, static_cast<float>(room.floor_y + 0.8f), room.center.z + std::sin(angle) * 4.5f);
                    vent.base_color = glm::vec3(0.20f, 0.88f, 0.25f);
                    vent.base_radius = 12.0f;
                    vent.base_intensity = 1.6f;
                    vent.type = LuminaryType::RadioactiveCore;
                    vent.flicker_rate = 18.0f;
                    vent.name = "Radioactive Moat Fissure";
                    m_luminaries.push_back(vent);
                }
                break;
            }

            case RoomShapeType::FaultLineCrevasse: {
                CavernLuminary magma1;
                magma1.position = glm::vec3(room.center.x - 2.5f, static_cast<float>(room.floor_y - 0.2f), room.center.z - 2.5f);
                magma1.base_color = glm::vec3(1.0f, 0.40f, 0.05f);
                magma1.base_radius = 20.0f;
                magma1.base_intensity = 2.8f;
                magma1.type = LuminaryType::ThermalMagmaVent;
                magma1.pulse_speed = 3.2f;
                magma1.name = "Molten Magma Rift";
                m_luminaries.push_back(magma1);

                CavernLuminary magma2;
                magma2.position = glm::vec3(room.center.x + 2.5f, static_cast<float>(room.floor_y - 0.2f), room.center.z + 2.5f);
                magma2.base_color = glm::vec3(1.0f, 0.50f, 0.08f);
                magma2.base_radius = 18.0f;
                magma2.base_intensity = 2.5f;
                magma2.type = LuminaryType::ThermalMagmaVent;
                magma2.name = "Thermal Crevasse Vent";
                m_luminaries.push_back(magma2);
                break;
            }

            case RoomShapeType::IndustrialVaultBunker: {
                CavernLuminary beacon;
                beacon.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 5.5f), room.center.z);
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
                CavernLuminary seam;
                seam.position = glm::vec3(room.center.x + 4.0f, static_cast<float>(room.floor_y + 4.0f), room.center.z + 4.0f);
                seam.base_color = glm::vec3(0.18f, 0.82f, 0.98f);
                seam.base_radius = 18.0f;
                seam.base_intensity = 2.2f;
                seam.type = LuminaryType::BioluminescentLedge;
                seam.name = "Phosphorescent Crystal Seam";
                m_luminaries.push_back(seam);
                break;
            }

            case RoomShapeType::TerracedQuarry: {
                CavernLuminary quarry;
                quarry.position = glm::vec3(room.center.x - 3.0f, static_cast<float>(room.floor_y + 3.0f), room.center.z + 3.0f);
                quarry.base_color = glm::vec3(0.95f, 0.85f, 0.40f);
                quarry.base_radius = 16.0f;
                quarry.base_intensity = 1.8f;
                quarry.type = LuminaryType::BioluminescentLedge;
                quarry.name = "Excavation Pit Glint";
                m_luminaries.push_back(quarry);
                break;
            }

            case RoomShapeType::AbyssalVerticalChasm: {
                CavernLuminary ledge;
                ledge.position = glm::vec3(room.center.x + 4.5f, static_cast<float>(room.floor_y + 10.0f), room.center.z);
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
                guide.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 1.2f), room.center.z);
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
                lava_core.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 0.5f), room.center.z);
                lava_core.base_color = glm::vec3(1.0f, 0.38f, 0.04f);
                lava_core.base_radius = 24.0f;
                lava_core.base_intensity = 3.4f;
                lava_core.type = LuminaryType::ThermalMagmaVent;
                lava_core.pulse_speed = 2.8f;
                lava_core.pulse_depth = 0.22f;
                lava_core.name = "Caldera Magma Heart";
                m_luminaries.push_back(lava_core);

                CavernLuminary vent1;
                vent1.position = glm::vec3(room.center.x + 4.0f, static_cast<float>(room.floor_y + 1.5f), room.center.z - 4.0f);
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
                warning.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 4.5f), room.center.z);
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
                void_pulse.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 5.0f), room.center.z);
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
                high_aura.position = glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 3.0f), room.center.z);
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
                spore.position = glm::vec3(room.center.x + 3.0f, static_cast<float>(room.floor_y + 5.0f), room.center.z + 3.0f);
                spore.base_color = glm::vec3(0.15f, 0.95f, 0.60f);
                spore.base_radius = 18.0f;
                spore.base_intensity = 2.4f;
                spore.type = LuminaryType::FungoidSporeGlow;
                spore.pulse_speed = 1.4f;
                spore.pulse_depth = 0.15f;
                spore.name = "Phosphorescent Spore Bloom";
                m_luminaries.push_back(spore);

                CavernLuminary cap;
                cap.position = glm::vec3(room.center.x - 3.0f, static_cast<float>(room.floor_y + 8.0f), room.center.z - 3.0f);
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
                strobe.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 6.0f), room.center.z);
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
                canyon.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 8.0f), room.center.z);
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
                pool_glow.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 1.2f), room.center.z);
                pool_glow.base_color = glm::vec3(0.08f, 0.92f, 0.96f);
                pool_glow.base_radius = 24.0f;
                pool_glow.base_intensity = 3.0f;
                pool_glow.type = LuminaryType::AquiferOasisGlow;
                pool_glow.pulse_speed = 1.3f;
                pool_glow.pulse_depth = 0.15f;
                pool_glow.name = "Aquifer Oasis Pool Luminary";
                m_luminaries.push_back(pool_glow);

                CavernLuminary waterfall_mist;
                waterfall_mist.position = glm::vec3(room.center.x - room.half_width + 2.0f, static_cast<float>(room.floor_y + 4.0f), room.center.z);
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
                bridge_light.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 9.5f), room.center.z);
                bridge_light.base_color = glm::vec3(0.95f, 0.85f, 0.40f);
                bridge_light.base_radius = 22.0f;
                bridge_light.base_intensity = 2.8f;
                bridge_light.type = LuminaryType::CorridorBulkheadLight;
                bridge_light.name = "Suspension Bridge Lantern";
                m_luminaries.push_back(bridge_light);

                CavernLuminary pit_beacon;
                pit_beacon.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 2.0f), room.center.z);
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
                hearth.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 1.2f), room.center.z);
                hearth.base_color = glm::vec3(1.0f, 0.38f, 0.05f);
                hearth.base_radius = 28.0f;
                hearth.base_intensity = 3.8f;
                hearth.type = LuminaryType::ThermalMagmaVent;
                hearth.pulse_speed = 3.0f;
                hearth.pulse_depth = 0.25f;
                hearth.name = "Magma Foundry Smelter";
                m_luminaries.push_back(hearth);

                CavernLuminary flume;
                flume.position = glm::vec3(room.center.x + 4.0f, static_cast<float>(room.floor_y + 4.0f), room.center.z - 4.0f);
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
                miasma.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 2.0f), room.center.z);
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
                spore.position = glm::vec3(room.center.x - 3.5f, static_cast<float>(room.floor_y + 4.0f), room.center.z + 3.5f);
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
                monolith.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 6.0f), room.center.z);
                monolith.base_color = glm::vec3(0.88f, 0.35f, 0.98f);
                monolith.base_radius = 32.0f;
                monolith.base_intensity = 4.2f;
                monolith.type = LuminaryType::PrismaticCrystalRadiance;
                monolith.pulse_speed = 1.6f;
                monolith.pulse_depth = 0.22f;
                monolith.name = "Prismatic Cathedral Core";
                m_luminaries.push_back(monolith);

                CavernLuminary spire;
                spire.position = glm::vec3(room.center.x + 4.0f, static_cast<float>(room.floor_y + 9.0f), room.center.z + 4.0f);
                spire.base_color = glm::vec3(0.20f, 0.85f, 0.98f);
                spire.base_radius = 20.0f;
                spire.base_intensity = 2.8f;
                spire.type = LuminaryType::PrismaticCrystalRadiance;
                spire.name = "Cathedral Spire Prism";
                m_luminaries.push_back(spire);
                break;
            }

            case RoomShapeType::AncientTitanNecropolis: {
                CavernLuminary titan_rib;
                titan_rib.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 6.0f), room.center.z);
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
                starry_canopy.position = glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 2.0f), room.center.z);
                starry_canopy.base_color = glm::vec3(0.15f, 0.92f, 0.82f);
                starry_canopy.base_radius = 26.0f;
                starry_canopy.base_intensity = 3.4f;
                starry_canopy.type = LuminaryType::BioluminescentLedge;
                starry_canopy.pulse_speed = 2.0f;
                starry_canopy.pulse_depth = 0.25f;
                starry_canopy.name = "Glowworm Canopy Stars";
                m_luminaries.push_back(starry_canopy);

                CavernLuminary mirror_pool;
                mirror_pool.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 1.0f), room.center.z);
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
                coolant.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 2.0f), room.center.z);
                coolant.base_color = glm::vec3(0.08f, 0.70f, 0.98f);
                coolant.base_radius = 24.0f;
                coolant.base_intensity = 3.2f;
                coolant.type = LuminaryType::IndustrialVaultBeacon;
                coolant.pulse_speed = 3.8f;
                coolant.pulse_depth = 0.22f;
                coolant.name = "Precursor Coolant Reservoir";
                m_luminaries.push_back(coolant);

                CavernLuminary strobe;
                strobe.position = glm::vec3(room.center.x - 4.0f, static_cast<float>(room.floor_y + 4.5f), room.center.z + 4.0f);
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
                high_vault.position = glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 2.5f), room.center.z);
                high_vault.base_color = glm::vec3(0.85f, 0.72f, 0.98f); // Radiant celestial violet / gold
                high_vault.base_radius = 28.0f;
                high_vault.base_intensity = 3.6f;
                high_vault.type = LuminaryType::GrandVaultRadiance;
                high_vault.pulse_speed = 1.6f;
                high_vault.pulse_depth = 0.20f;
                high_vault.name = "Cathedral Vault Chandelier";
                m_luminaries.push_back(high_vault);

                CavernLuminary dais_beacon;
                dais_beacon.position = glm::vec3(room.center.x + static_cast<float>(room.half_width - 3), static_cast<float>(room.floor_y + 2.0f), room.center.z);
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
                thermal_rift.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 0.8f), room.center.z);
                thermal_rift.base_color = glm::vec3(1.0f, 0.38f, 0.06f); // Searing thermal rift
                thermal_rift.base_radius = 26.0f;
                thermal_rift.base_intensity = 3.4f;
                thermal_rift.type = LuminaryType::ThermalMagmaVent;
                thermal_rift.pulse_speed = 3.0f;
                thermal_rift.name = "Abyssal Sinkhole Fissure";
                m_luminaries.push_back(thermal_rift);

                CavernLuminary rim_beacon;
                rim_beacon.position = glm::vec3(room.center.x - static_cast<float>(room.half_width - 3), static_cast<float>(room.floor_y + 16.0f), room.center.z);
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
                floodlight.position = glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 2.0f), room.center.z);
                floodlight.base_color = glm::vec3(1.0f, 0.76f, 0.20f); // Warm sodium industrial amber
                floodlight.base_radius = 28.0f;
                floodlight.base_intensity = 3.8f;
                floodlight.type = LuminaryType::CyclopeanFloodlight;
                floodlight.pulse_speed = 4.2f;
                floodlight.pulse_depth = 0.25f;
                floodlight.name = "Cyclopean Crane Floodlight";
                m_luminaries.push_back(floodlight);

                CavernLuminary mid_ring;
                mid_ring.position = glm::vec3(room.center.x + 4.5f, static_cast<float>(room.floor_y + 9.0f), room.center.z - 4.5f);
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
                canopy_firmament.position = glm::vec3(room.center.x, static_cast<float>(room.ceiling_y - 2.5f), room.center.z);
                canopy_firmament.base_color = glm::vec3(0.20f, 0.85f, 0.95f); // Celestial starlight cyan
                canopy_firmament.base_radius = 32.0f;
                canopy_firmament.base_intensity = 3.2f;
                canopy_firmament.type = LuminaryType::FirmamentStarlight;
                canopy_firmament.pulse_speed = 1.4f;
                canopy_firmament.pulse_depth = 0.15f;
                canopy_firmament.name = "Firmament Star Canopy";
                m_luminaries.push_back(canopy_firmament);

                CavernLuminary aquifer_glow;
                aquifer_glow.position = glm::vec3(room.center.x - 2.0f, static_cast<float>(room.floor_y + 1.2f), room.center.z + 2.0f);
                aquifer_glow.base_color = glm::vec3(0.12f, 0.95f, 0.80f);
                aquifer_glow.base_radius = 18.0f;
                aquifer_glow.base_intensity = 2.0f;
                aquifer_glow.type = LuminaryType::AquiferOasisGlow;
                aquifer_glow.name = "Firmament Reflecting Pool";
                m_luminaries.push_back(aquifer_glow);
                break;
            }

            case RoomShapeType::SpawnStagingCavern:
            default: {
                CavernLuminary spawn_beacon;
                spawn_beacon.position = glm::vec3(room.center.x, static_cast<float>(room.floor_y + 3.0f), room.center.z);
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

    // Corridor lights
    for (const auto& c : m_corridors) {
        if (c.has_bulkhead_door) {
            CavernLuminary blk_light;
            glm::vec3 mid = (glm::vec3(c.start_pos) + glm::vec3(c.end_pos)) * 0.5f;
            blk_light.position = glm::vec3(mid.x, static_cast<float>(c.floor_y + 2.5f), mid.z);
            blk_light.base_color = glm::vec3(0.95f, 0.60f, 0.18f);
            blk_light.base_radius = 10.0f;
            blk_light.base_intensity = 1.4f;
            blk_light.type = LuminaryType::CorridorBulkheadLight;
            blk_light.pulse_speed = 3.5f;
            blk_light.pulse_depth = 0.18f;
            blk_light.name = "Bulkhead Clearance Light";
            m_luminaries.push_back(blk_light);
        }
    }
}

Voxel LevelGenerator::sample_voxel(int x, int y, int z) const {
    // 1. Bedrock base layer (bottom of world is solid impenetrable crust)
    if (y <= 3) {
        return Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
    }

    // 2. Ceiling mantle layer (top of world is solid rock mantle)
    if (y >= m_world_h - 6) {
        uint8_t ceiling_mat = (m_sector_index >= 2) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE;
        return Voxel{ceiling_mat, VOXEL_FLAG_ANCHORED};
    }

    // 3. World Perimeter Barrier (Outer solid bedrock bounding walls)
    if (x <= 3 || x >= world_width() - 4 || z <= 3 || z >= world_depth() - 4) {
        return Voxel{MAT_DREDGE_BEDROCK, VOXEL_FLAG_ANCHORED};
    }

    // 4. Default Host Rock Matrix per Sector
    uint8_t host_rock = MAT_FRACTURED_GRANITE;
    if (m_sector_index == 2) {
        host_rock = MAT_VOLCANIC_BASALT;
    } else if (m_sector_index >= 3) {
        host_rock = (y < 12) ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE;
    }

    Voxel voxel{host_rock, 0};

    // 5. Check if inside any Corridor
    for (const auto& c : m_corridors) {
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
            if (px < min_cx || px > max_cx || pz < min_cz || pz > max_cz) continue;

            float vx = bx - ax;
            float vz = bz - az;
            float len_sq = vx * vx + vz * vz;
            if (len_sq < 0.001f) continue;

            float t = glm::clamp(((px - ax) * vx + (pz - az) * vz) / len_sq, 0.0f, 1.0f);
            float qx = ax + t * vx;
            float qz = az + t * vz;
            float dist = std::sqrt((px - qx) * (px - qx) + (pz - qz) * (pz - qz));

            float eff_width = static_cast<float>(c.half_width);
            if (c.is_ravine) {
                eff_width += (pseudo_rand(x, y, z, 77) - 0.5f) * 1.5f;
            }

            if (dist <= eff_width) {
                int floor_y = static_cast<int>(std::round(static_cast<float>(c.floor_y) + t * static_cast<float>(c.end_floor_y - c.floor_y)));
                int ceil_y  = static_cast<int>(std::round(static_cast<float>(c.ceiling_y) + t * static_cast<float>(c.end_ceiling_y - c.ceiling_y)));

                if (y == floor_y) {
                    voxel = Voxel{MAT_VOLCANIC_BASALT, 0};
                    return voxel;
                }
                if (y > floor_y && y < ceil_y) {
                    voxel = Voxel{MAT_AIR, 0};
                    return voxel;
                }
                if (y == ceil_y) {
                    voxel = Voxel{c.is_ravine ? MAT_FRACTURED_GRANITE : MAT_INDUSTRIAL_BULKHEAD, 0};
                    return voxel;
                }
            }
        } else if (c.is_x_axis) {
            int min_x = std::min(c.start_pos.x, c.end_pos.x);
            int max_x = std::max(c.start_pos.x, c.end_pos.x);
            if (x >= min_x && x <= max_x) {
                float t = (max_x > min_x) ? static_cast<float>(x - min_x) / static_cast<float>(max_x - min_x) : 0.0f;
                if (c.start_pos.x > c.end_pos.x) t = 1.0f - t;
                float center_z = static_cast<float>(c.start_pos.z) + t * static_cast<float>(c.end_pos.z - c.start_pos.z);
                if (std::abs(static_cast<float>(z) - center_z) <= static_cast<float>(c.half_width)) {

                    int floor_y = static_cast<int>(std::round(static_cast<float>(c.floor_y) + t * static_cast<float>(c.end_floor_y - c.floor_y)));
                    int ceil_y  = static_cast<int>(std::round(static_cast<float>(c.ceiling_y) + t * static_cast<float>(c.end_ceiling_y - c.ceiling_y)));

                    if (y == floor_y) {
                        voxel = Voxel{MAT_VOLCANIC_BASALT, 0};
                        return voxel;
                    }
                    if (y > floor_y && y < ceil_y) {
                        int mid_x = (c.start_pos.x + c.end_pos.x) / 2;
                        if (c.has_bulkhead_door && x == mid_x) {
                            if (std::abs(static_cast<float>(z) - center_z) >= 1.8f || y >= ceil_y - 2) {
                                voxel = Voxel{MAT_REINFORCED_VAULT_DOOR, VOXEL_FLAG_ANCHORED};
                                return voxel;
                            }
                        }
                        if (c.has_rubble_collapse && (x == mid_x || x == mid_x + 1) && y <= floor_y + 1 && std::abs(static_cast<float>(z) - center_z) >= 1.8f) {
                            voxel = Voxel{MAT_FRACTURED_GRANITE, 0};
                            return voxel;
                        }

                        bool is_arch = (x % 6 == 0) || (x == c.start_pos.x) || (x == c.end_pos.x);
                        if (is_arch && (std::abs(static_cast<float>(z) - center_z) >= static_cast<float>(c.half_width) - 0.5f || y == ceil_y - 1)) {
                            voxel = Voxel{c.is_ravine ? MAT_FRACTURED_GRANITE : MAT_INDUSTRIAL_BULKHEAD, 0};
                            return voxel;
                        }

                        voxel = Voxel{MAT_AIR, 0};
                        return voxel;
                    }
                    if (y == ceil_y) {
                        voxel = Voxel{c.is_ravine ? MAT_FRACTURED_GRANITE : MAT_INDUSTRIAL_BULKHEAD, 0};
                        return voxel;
                    }
                }
            }
        } else { // Z axis
            int min_z = std::min(c.start_pos.z, c.end_pos.z);
            int max_z = std::max(c.start_pos.z, c.end_pos.z);
            if (z >= min_z && z <= max_z) {
                float t = (max_z > min_z) ? static_cast<float>(z - min_z) / static_cast<float>(max_z - min_z) : 0.0f;
                if (c.start_pos.z > c.end_pos.z) t = 1.0f - t;
                float center_x = static_cast<float>(c.start_pos.x) + t * static_cast<float>(c.end_pos.x - c.start_pos.x);
                if (std::abs(static_cast<float>(x) - center_x) <= static_cast<float>(c.half_width)) {

                    int floor_y = static_cast<int>(std::round(static_cast<float>(c.floor_y) + t * static_cast<float>(c.end_floor_y - c.floor_y)));
                    int ceil_y  = static_cast<int>(std::round(static_cast<float>(c.ceiling_y) + t * static_cast<float>(c.end_ceiling_y - c.ceiling_y)));

                    if (y == floor_y) {
                        voxel = Voxel{MAT_VOLCANIC_BASALT, 0};
                        return voxel;
                    }
                    if (y > floor_y && y < ceil_y) {
                        int mid_z = (c.start_pos.z + c.end_pos.z) / 2;
                        if (c.has_bulkhead_door && z == mid_z) {
                            if (std::abs(static_cast<float>(x) - center_x) >= 1.8f || y >= ceil_y - 2) {
                                voxel = Voxel{MAT_REINFORCED_VAULT_DOOR, VOXEL_FLAG_ANCHORED};
                                return voxel;
                            }
                        }
                        if (c.has_rubble_collapse && (z == mid_z || z == mid_z + 1) && y <= floor_y + 1 && std::abs(static_cast<float>(x) - center_x) >= 1.8f) {
                            voxel = Voxel{MAT_FRACTURED_GRANITE, 0};
                            return voxel;
                        }

                        bool is_arch = (z % 6 == 0) || (z == c.start_pos.z) || (z == c.end_pos.z);
                        if (is_arch && (std::abs(static_cast<float>(x) - center_x) >= static_cast<float>(c.half_width) - 0.5f || y == ceil_y - 1)) {
                            voxel = Voxel{c.is_ravine ? MAT_FRACTURED_GRANITE : MAT_INDUSTRIAL_BULKHEAD, 0};
                            return voxel;
                        }

                        voxel = Voxel{MAT_AIR, 0};
                        return voxel;
                    }
                    if (y == ceil_y) {
                        voxel = Voxel{c.is_ravine ? MAT_FRACTURED_GRANITE : MAT_INDUSTRIAL_BULKHEAD, 0};
                        return voxel;
                    }
                }
            }
        }
    }

    // 6. Check Grid Rooms
    for (const auto& room : m_rooms) {
        int dx = std::abs(x - room.center.x);
        int dz = std::abs(z - room.center.z);

        if (dx <= room.half_width && dz <= room.half_depth &&
            y >= room.floor_y && y <= room.ceiling_y) {

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
            }
            return voxel;
        }
    }

    // 7. Procedural Mineral Seam Generation in Host Rock Matrix (Sprint 2: Item 3.3)
    if (y >= 4 && y <= 25) {
        // Coherent 3D vein clustering via 3x2x3 cell hashing
        uint32_t vein_cluster = hash_coord(x / 3, y / 2, z / 3, 7331);
        int vein_chance = (m_sector_index >= 3) ? 6 : ((m_sector_index == 2) ? 5 : 4);
        if ((vein_cluster % 100) < static_cast<uint32_t>(vein_chance)) {
            uint32_t detail_hash = hash_coord(x, y, z, 9127);
            if ((detail_hash % 10) < 6) { // 60% density within cluster -> organic 3-6 block streaks
                uint32_t ore_roll = hash_coord(x / 2, y, z / 2, 4513) % 100;
                if (m_sector_index <= 1) {
                    if (ore_roll < 75) {
                        return Voxel{MAT_TITANIUM, 0};
                    } else {
                        return Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
                    }
                } else if (m_sector_index == 2) {
                    if (ore_roll < 45) {
                        return Voxel{MAT_TITANIUM, 0};
                    } else if (ore_roll < 85) {
                        return Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
                    } else {
                        return Voxel{MAT_RADIOACTIVE_ORE, VOXEL_FLAG_EMISSIVE};
                    }
                } else {
                    if (ore_roll < 30) {
                        return Voxel{MAT_TITANIUM, 0};
                    } else if (ore_roll < 65) {
                        return Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
                    } else {
                        return Voxel{MAT_RADIOACTIVE_ORE, VOXEL_FLAG_EMISSIVE};
                    }
                }
            }
        }
    }

    return voxel;
}

// ─────────────────────────────────────────────────────────────
// ROOM CARVING & DOORWAY INTEGRATION HELPERS
// ─────────────────────────────────────────────────────────────

static inline bool is_doorway_floor(const RoomPlacement& room, int dx, int dz, int y) {
    if (y != room.floor_y) return false;
    if (room.connected_east  && dx > 0 && std::abs(dz) <= 2) return true;
    if (room.connected_west  && dx < 0 && std::abs(dz) <= 2) return true;
    if (room.connected_north && dz > 0 && std::abs(dx) <= 2) return true;
    if (room.connected_south && dz < 0 && std::abs(dx) <= 2) return true;
    if (room.connected_ne    && dx > 0 && dz > 0 && std::abs(dx - dz) <= 2) return true;
    if (room.connected_nw    && dx < 0 && dz > 0 && std::abs((-dx) - dz) <= 2) return true;
    if (room.connected_se    && dx > 0 && dz < 0 && std::abs(dx - (-dz)) <= 2) return true;
    if (room.connected_sw    && dx < 0 && dz < 0 && std::abs((-dx) - (-dz)) <= 2) return true;
    return false;
}

static inline bool is_doorway_air(const RoomPlacement& room, int dx, int dz, int y) {
    if (y <= room.floor_y || y > room.floor_y + 4) return false;
    if (room.connected_east  && dx > 0 && std::abs(dz) <= 2) return true;
    if (room.connected_west  && dx < 0 && std::abs(dz) <= 2) return true;
    if (room.connected_north && dz > 0 && std::abs(dx) <= 2) return true;
    if (room.connected_south && dz < 0 && std::abs(dx) <= 2) return true;
    if (room.connected_ne    && dx > 0 && dz > 0 && std::abs(dx - dz) <= 2) return true;
    if (room.connected_nw    && dx < 0 && dz > 0 && std::abs((-dx) - dz) <= 2) return true;
    if (room.connected_se    && dx > 0 && dz < 0 && std::abs(dx - (-dz)) <= 2) return true;
    if (room.connected_sw    && dx < 0 && dz < 0 && std::abs((-dx) - (-dz)) <= 2) return true;
    return false;
}

static inline void check_doorway_or_solid(const RoomPlacement& room, int dx, int dz, int y, uint8_t host_mat, Voxel& out) {
    if (is_doorway_floor(room, dx, dz, y)) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }
    if (is_doorway_air(room, dx, dz, y)) {
        out = Voxel{MAT_AIR, 0};
        return;
    }
    out = Voxel{host_mat, 0};
}

static inline float compute_room_wall_radius(const RoomPlacement& room, float dx, float dz, float angle, float noise) {
    float rx = static_cast<float>(room.half_width);
    float rz = static_cast<float>(room.half_depth);
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    float denom = std::sqrt((rz * cos_a) * (rz * cos_a) + (rx * sin_a) * (rx * sin_a) + 0.001f);
    float r_base = (rx * rz) / denom;
    float sin2 = std::sin(2.0f * angle);
    return r_base * (1.02f - 0.08f * (sin2 * sin2)) + noise;
}

// ─────────────────────────────────────────────────────────────
// SPECIFIC ROOM SHAPE CARVERS (Organic, Realistic & Non-Square)
// ─────────────────────────────────────────────────────────────

void LevelGenerator::sample_spawn_cavern(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));
    float sin2 = std::sin(2.0f * angle);

    // Natural curved cavern envelope: rounded corners (r ~ 7.6 at diagonals), expanding to r ~ 8.35 at cardinal points
    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 101) - 0.5f) * 0.30f);

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
    if (dx >= 3 && dx <= 5 && dz >= 3 && dz <= 5 && y >= room.floor_y + 1 && y <= room.floor_y + 3) {
        if (y == room.floor_y + 2 && dx == 4 && dz == 4) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else if (y == room.floor_y + 1 && (dx == 3 || dz == 3)) {
            out = Voxel{MAT_TITANIUM, 0};
        } else {
            out = Voxel{MAT_FRACTURED_GRANITE, 0};
        }
        return;
    }

    // Smooth arched natural dome ceiling
    float r_span = std::max(4.0f, static_cast<float>(std::min(room.half_width, room.half_depth)));
    float norm_dist = r_horiz / r_span;
    int arch_height = static_cast<int>(static_cast<float>(room.ceiling_y) - norm_dist * 8.0f);
    if (y > arch_height) {
        out = Voxel{MAT_FRACTURED_GRANITE, 0};
        return;
    }

    // Hanging stalactites from ceiling at y >= 19
    if (y >= 19 && (hash_coord(x, y, z, 41) % 9 == 0) && r_horiz >= 2.5f) {
        out = Voxel{MAT_FRACTURED_GRANITE, VOXEL_FLAG_ANCHORED};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_pillar_hall(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    // Vaulted cathedral envelope with side alcoves
    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 102) - 0.5f) * 0.30f);

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
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
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
        is_bridge = (y == bridge_y && std::abs(dz) <= 1 && std::abs(dx) <= (room.half_width - 3));
    } else {
        is_bridge = (y == bridge_y && std::abs(dx) <= 1 && std::abs(dz) <= (room.half_depth - 3));
    }

    if (is_bridge && bridge_y <= room.ceiling_y - 6) {
        if (dx == 0 && dz == 0) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE}; // Central bridge prize
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        }
        return;
    }

    // Arched cathedral nave ceiling
    float r_base = static_cast<float>(std::min(room.half_width, room.half_depth));
    float factor = (r_horiz * r_horiz) / (r_base * r_base);
    int arch_y = static_cast<int>(static_cast<float>(room.ceiling_y - 1) - factor * 7.0f);
    if (y >= arch_y) {
        out = Voxel{MAT_FRACTURED_GRANITE, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_crystalline_geode(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    float dx_f = static_cast<float>(x - room.center.x);
    float dz_f = static_cast<float>(z - room.center.z);
    float dy_f = static_cast<float>(y - room.floor_y);

    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float angle = std::atan2(dz_f, dx_f);
    float eff_rad = compute_room_wall_radius(room, dx_f, dz_f, angle, 0.0f);

    // True 3D hollow geode sphere/ellipsoid
    float r_horiz_sq = (dx_f * dx_f + dz_f * dz_f) / (eff_rad * eff_rad);
    float r_vert_sq  = ((dy_f - 9.0f) * (dy_f - 9.0f)) / (10.0f * 10.0f);
    float r_norm     = r_horiz_sq + r_vert_sq;

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

    // Crystalline shell lining around geode inner surface
    if (r_norm >= 0.86f) {
        if ((hash_coord(x, y, z, 77) % 3 == 0) && (y >= room.floor_y + 2)) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_FRACTURED_GRANITE, 0};
        }
        return;
    }

    uint32_t geode_hash = hash_coord(room.center.x, room.center.z, 0, 619);
    float target_rad = 4.2f + static_cast<float>(geode_hash % 3) * 0.4f;

    // Parkour ring stepping stones suspended relative to room.floor_y
    float rad = std::sqrt(dx_f * dx_f + dz_f * dz_f);
    int step_y1 = room.floor_y + 3;
    int step_y2 = room.floor_y + 6;
    if ((y == step_y1 || y == step_y2) && std::abs(rad - target_rad) <= 0.85f && (hash_coord(x, y, z, 99) % 3 == 0)) {
        out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        return;
    }

    // Central glowing geode crystal spire rising from basin
    int spire_dx = static_cast<int>((geode_hash >> 2) % 3) - 1; // -1, 0, 1
    int spire_dz = static_cast<int>((geode_hash >> 4) % 3) - 1; // -1, 0, 1
    if (std::abs(dx - spire_dx) <= 1 && std::abs(dz - spire_dz) <= 1 && y <= room.floor_y + 5) {
        out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_terraced_quarry(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 104) - 0.5f) * 0.30f);
    if (r_horiz > r_wall) {
        check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
        return;
    }

    // Doorway connection paths always step down to floor y=room.floor_y
    bool is_doorway_axis = (std::abs(dx) <= 2 && (room.connected_north || room.connected_south)) ||
                           (std::abs(dz) <= 2 && (room.connected_east  || room.connected_west));

    // Concentric stepped circular amphitheater tiers
    int terrace_floor = room.floor_y;
    if (!is_doorway_axis) {
        if (r_horiz > 6.0f) {
            terrace_floor = room.floor_y + 4; // Tier 2
        } else if (r_horiz > 3.8f) {
            terrace_floor = room.floor_y + 2; // Tier 1
        } else {
            terrace_floor = room.floor_y;     // Basin
        }

        // Spiral ramp carved in quadrant determined by seed hash
        uint32_t q = hash_coord(room.center.x, room.center.z, 0, 883) % 4;
        bool in_ramp = false;
        if (q == 0) in_ramp = (angle > 0.35f && angle < 1.55f); // NE
        else if (q == 1) in_ramp = (angle > 1.90f && angle < 3.05f); // NW
        else if (q == 2) in_ramp = (angle > -2.75f && angle < -1.60f); // SW
        else in_ramp = (angle > -1.20f && angle < -0.10f); // SE

        if (in_ramp && r_horiz >= 3.4f && r_horiz <= 6.4f) {
            terrace_floor = room.floor_y + static_cast<int>((r_horiz - 3.4f) * 1.33f);
        }
    }

    if (y <= terrace_floor) {
        if (y == terrace_floor && (r_horiz <= 1.2f || (std::abs(r_horiz - 3.8f) < 0.6f && (hash_coord(x, y, z, 42) % 4 == 0)))) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
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

void LevelGenerator::sample_vault_bunker(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    int abs_x = std::abs(dx);
    int abs_z = std::abs(dz);
    int manhattan = abs_x + abs_z;

    // 45-degree chamfered octagonal bulkhead perimeter (keeps cardinal paths open, cuts diagonal corners)
    bool is_outer_wall = (abs_x > room.half_width || abs_z > room.half_depth || manhattan >= (room.half_width + room.half_depth - 2));
    if (is_outer_wall) {
        if (is_doorway_floor(room, dx, dz, y)) {
            out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
            return;
        }
        if (is_doorway_air(room, dx, dz, y)) {
            out = Voxel{MAT_AIR, 0};
            return;
        }
        // Reinforced vault portal frames around doorway edges
        bool is_portal_frame = (abs_x == room.half_width && abs_z <= 3) ||
                               (abs_z == room.half_depth && abs_x <= 3);
        if (is_portal_frame && y <= room.floor_y + 5) {
            out = Voxel{MAT_REINFORCED_VAULT_DOOR, VOXEL_FLAG_ANCHORED};
        } else {
            out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
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

    // Catwalk access ladder pillar at (-4, -4)
    if (dx == -4 && dz == -4 && y <= 9) {
        out = Voxel{MAT_TITANIUM, 0};
        return;
    }

    // Central research salvage console at (0, 0)
    if (abs_x <= 1 && abs_z <= 1 && y <= room.floor_y + 2) {
        if (y == room.floor_y + 2 && dx == 0 && dz == 0) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_TITANIUM, 0};
        }
        return;
    }

    if (y >= room.ceiling_y - 3) {
        out = Voxel{static_cast<uint8_t>(((x + z) % 3 == 0) ? MAT_TITANIUM : MAT_VOLCANIC_BASALT), 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_fault_crevasse(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 106) - 0.5f) * 0.30f);
    if (r_horiz > r_wall) {
        check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
        return;
    }

    // Sinuous jagged crevasse path
    float fissure_center = std::sin(static_cast<float>(dz) * 0.38f) * 2.2f;
    float dist_to_fissure = std::abs(static_cast<float>(dx) - fissure_center);
    bool in_crevasse = dist_to_fissure <= 1.8f;
    bool is_bridge = std::abs(dz) <= 1; // Natural stone bridge across fissure

    if (in_crevasse && !is_bridge) {
        if (y < room.floor_y) {
            if (y == room.floor_y - 1 && (x % 3 == 0)) {
                out = Voxel{MAT_GAS, 0}; // Toxic escaping gas pockets
            } else if (y == room.floor_y - 1) {
                out = Voxel{MAT_THERMITE_SLAG, VOXEL_FLAG_EMISSIVE}; // Molten fissure bottom
            } else {
                out = Voxel{MAT_VOLCANIC_BASALT, 0};
            }
            return;
        }
        out = Voxel{MAT_AIR, 0};
        return;
    }

    if (y == room.floor_y) {
        out = Voxel{static_cast<uint8_t>(is_bridge ? MAT_VOLCANIC_BASALT : MAT_FRACTURED_GRANITE), 0};
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

void LevelGenerator::sample_abyssal_chasm(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 107) - 0.5f) * 0.30f);
    if (r_horiz > r_wall) {
        check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
        return;
    }

    // Staggered spiral parkour ledges along circular chasm walls
    bool is_ledge_1 = (y == 7  && r_horiz >= 4.5f && angle >= -0.5f && angle <= 1.2f);
    bool is_ledge_2 = (y == 11 && r_horiz >= 4.5f && angle >= 1.0f  && angle <= 2.6f);
    bool is_ledge_3 = (y == 15 && r_horiz >= 4.5f && (angle >= 2.4f || angle <= -2.4f));
    bool is_ledge_4 = (y == 19 && r_horiz >= 4.5f && angle >= -2.6f && angle <= -0.8f);

    if (is_ledge_1 || is_ledge_2 || is_ledge_3 || is_ledge_4) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    // Center hanging crystal stalactite (grapple target at y = 14..22)
    if (std::abs(dx) <= 1 && std::abs(dz) <= 1 && y >= 14 && y <= room.ceiling_y - 2) {
        if (y == 14) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
        }
        return;
    }

    if (y == room.floor_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    if (y >= room.ceiling_y - 2) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_radioactive_sanctuary(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 108) - 0.5f) * 0.30f);
    if (r_horiz > r_wall) {
        check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
        return;
    }

    // Floor with radioactive moat
    if (y == room.floor_y) {
        bool is_step = (std::abs(dx) <= 1 || std::abs(dz) <= 1);
        if (r_horiz >= 2.6f && r_horiz <= 5.5f && !is_step) {
            out = Voxel{MAT_RADIOACTIVE_ORE, VOXEL_FLAG_EMISSIVE}; // Toxic radioactive moat
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        }
        return;
    }

    // Central altar with colossal Voidite Monolith
    int monolith_top = std::min(room.floor_y + 5, room.ceiling_y - 4);
    if (r_horiz <= 2.2f && y >= room.floor_y + 1 && y <= monolith_top) {
        out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        return;
    }

    if (y >= room.ceiling_y - 2) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_extraction_bay(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float abs_x = static_cast<float>(std::abs(dx));
    float abs_z = static_cast<float>(std::abs(dz));
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));

    // Fortified hexagonal touchdown perimeter adapted to room dimensions
    float max_hw = static_cast<float>(room.half_width);
    float max_hd = static_cast<float>(room.half_depth);
    bool is_outer = (abs_x > (max_hw + 0.2f) || (abs_z + 0.45f * abs_x) > (max_hd + 1.8f));
    if (is_outer) {
        check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
        return;
    }

    // Floor at room.floor_y
    if (y == room.floor_y) {
        if (r_horiz <= 3.2f) {
            out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED}; // Central touchdown cradle
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        }
        return;
    }

    // Defensive holdout perimeter barricades for the 40s holdout timer
    bool is_barricade = (r_horiz >= 4.2f && r_horiz <= 5.8f) && (y == room.floor_y + 1) &&
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

void LevelGenerator::sample_magma_caldera(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 110) - 0.5f) * 0.30f);
    if (r_horiz > r_wall) {
        check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
        return;
    }

    bool is_entry = is_doorway_floor(room, dx, dz, y) || is_doorway_air(room, dx, dz, y);

    uint32_t cal_hash = hash_coord(room.center.x, room.center.z, 0, 997);
    int grid_phase_x = static_cast<int>(cal_hash % 2);
    int grid_phase_z = static_cast<int>((cal_hash >> 1) % 2);

    // Stepping stone pillars across the molten lake at grid points
    bool is_stepping_stone = ((std::abs(dx) + grid_phase_x) % 3 == 0 && (std::abs(dz) + grid_phase_z) % 3 == 0) && (r_horiz <= (room.half_width - 2));

    // Floor layer: Searing molten thermite slag lake
    if (y == room.floor_y) {
        if (is_entry || is_stepping_stone || r_horiz > (room.half_width - 1.5f)) {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        } else {
            out = Voxel{MAT_THERMITE_SLAG, VOXEL_FLAG_EMISSIVE}; // Hot molten lava!
        }
        return;
    }

    // Stepping stones rise 1 voxel above lava for parkour hopping
    if (y == room.floor_y + 1 && is_stepping_stone) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    // Natural basalt arch bridging the caldera (orientation toggles NE-SW or NW-SE)
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

    // Central lava geyser chimney at (0, 0)
    if (r_horiz <= 1.2f && y <= room.floor_y + 4) {
        if (y == room.floor_y + 4) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_THERMITE_SLAG, VOXEL_FLAG_EMISSIVE};
        }
        return;
    }

    if (y >= room.ceiling_y - 2) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_spike_trench(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
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
    if (abs_x >= room.half_width - 2 || abs_z >= room.half_depth - 2) {
        if (y == room.floor_y) {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
            return;
        }
        if (y >= room.floor_y + 1 && y <= room.floor_y + 3) {
            out = Voxel{MAT_AIR, 0};
            return;
        }
    }

    // Pit floor at room.floor_y
    if (y == room.floor_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    uint32_t trench_hash = hash_coord(room.center.x, room.center.z, 0, 521);
    int spike_parity = static_cast<int>(trench_hash % 2);

    // Spikes strictly confined to interior recessed pit away from perimeter walkways and bridges
    bool is_spike = (y == room.floor_y + 1) &&
                    (abs_x >= 2 && abs_x <= room.half_width - 3) &&
                    (abs_z >= 2 && abs_z <= room.half_depth - 3) &&
                    ((abs_x + abs_z) % 2 == spike_parity);
    if (is_spike) {
        out = Voxel{MAT_OBSIDIAN_SPIKES, VOXEL_FLAG_EMISSIVE}; // Lethal glowing crimson obsidian spikes
        return;
    }

    // High balance beam bridges spanning at floor_y + 2 across X and/or Z axes
    int bridge_mode = static_cast<int>((trench_hash >> 1) % 3); // 0: both, 1: X only, 2: Z only
    bool is_bridge_x = (bridge_mode != 2) && (abs_z == 0) && y == room.floor_y + 2 && (abs_x <= room.half_width - 2);
    bool is_bridge_z = (bridge_mode != 1) && (abs_x == 0) && y == room.floor_y + 2 && (abs_z <= room.half_depth - 2);
    bool is_bridge_gap = (abs_x >= 3 && abs_x <= 4) || (abs_z >= 3 && abs_z <= 4);

    if ((is_bridge_x || is_bridge_z) && !is_bridge_gap) {
        out = Voxel{MAT_TITANIUM, 0};
        return;
    }

    // Central safe reward island
    if (abs_x <= 1 && abs_z <= 1 && y <= room.floor_y + 2) {
        if (y == room.floor_y + 2) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_TITANIUM, 0};
        }
        return;
    }

    if (y >= room.ceiling_y - 2) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_void_singularity(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 112) - 0.5f) * 0.30f);
    if (r_horiz > r_wall) {
        check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
        return;
    }

    // Perimeter boundary ledge at room floor
    bool is_perimeter = (r_horiz >= (room.half_width - 2.5f));

    // In the center rift, the bedrock is completely torn away into empty void!
    if (y <= room.floor_y) {
        if (is_perimeter) {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
            return;
        }
        // Void rift core plunges into the bottomless abyss
        out = Voxel{MAT_AIR, 0};
        return;
    }

    // Floating zero-gravity obsidian stepping platforms suspended in the void
    bool plat1 = (std::abs(dx - 3) <= 1 && std::abs(dz + 3) <= 1 && y == room.floor_y + 2);
    bool plat2 = (std::abs(dx + 3) <= 1 && std::abs(dz - 3) <= 1 && y == room.floor_y + 4);
    bool plat3 = (std::abs(dx - 3) <= 1 && std::abs(dz - 2) <= 1 && y == room.floor_y + 7);
    bool plat4 = (std::abs(dx + 3) <= 1 && std::abs(dz + 3) <= 1 && y == room.floor_y + 10);

    if (plat1 || plat2 || plat3 || plat4) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    // Colossal floating Void Singularity Monolith suspended in center (y = floor_y + 3 to floor_y + 9)
    if (r_horiz <= 1.5f && y >= room.floor_y + 3 && y <= room.floor_y + 9) {
        out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        return;
    }

    if (y >= room.ceiling_y - 2) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_fungoid_grotto(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    // Bulbous organic spore grotto
    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 113) - 0.5f) * 0.30f);
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
        out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE}; // Phosphorescent cap
        return;
    }
    if (dx == stem1_x && dz == stem1_z && y <= cap1_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0}; // Mushroom stem
        return;
    }

    // Cap 2 (South-West): stem at (stem2_x, stem2_z)
    int d2 = (dx - stem2_x) * (dx - stem2_x) + (dz - stem2_z) * (dz - stem2_z);
    if (d2 <= 4 && y == cap2_y) {
        out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        return;
    }
    if (dx == stem2_x && dz == stem2_z && y <= cap2_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    // Cap 3 (North-West): stem at (stem3_x, stem3_z)
    int d3 = (dx - stem3_x) * (dx - stem3_x) + (dz - stem3_z) * (dz - stem3_z);
    if (d3 <= 4 && y == cap3_y && room.ceiling_y > room.floor_y + 11) {
        out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        return;
    }
    if (dx == stem3_x && dz == stem3_z && y <= cap3_y && room.ceiling_y > room.floor_y + 11) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    // Spore pockets and gas fissures at floor perimeter
    if (y == room.floor_y + 1 && r_horiz >= (room.half_width - 2.5f) && (hash_coord(x, y, z, 55) % 6 == 0)) {
        out = Voxel{MAT_GAS, 0};
        return;
    }

    if (y >= room.ceiling_y - 2) {
        out = Voxel{MAT_FRACTURED_GRANITE, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_laser_foundry(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    int abs_x = std::abs(dx);
    int abs_z = std::abs(dz);

    // Industrial chamfered facility
    if (abs_x > room.half_width || abs_z > room.half_depth || (abs_x + abs_z) > (room.half_width + room.half_depth - 3)) {
        check_doorway_or_solid(room, dx, dz, y, MAT_INDUSTRIAL_BULKHEAD, out);
        return;
    }

    // Floor
    if (y == room.floor_y) {
        // Molten metal drainage trench cutting along X axis
        if (abs_z <= 1) {
            out = Voxel{MAT_THERMITE_SLAG, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
        }
        return;
    }

    // High elevated industrial gantry catwalk at y = floor_y + 4 along perimeter
    bool is_catwalk = (y == room.floor_y + 4) && (abs_x >= 4 || abs_z >= 4) && (abs_x <= room.half_width - 1 && abs_z <= room.half_depth - 1);
    if (is_catwalk) {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
        return;
    }

    // Catwalk bridge across the molten flume at x = 0
    if (y == room.floor_y + 4 && abs_x <= 1) {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
        return;
    }

    // Overhead industrial crane beam at y = room.ceiling_y - 4
    if (y == room.ceiling_y - 4 && (abs_x == 0 || abs_z == 0)) {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
        return;
    }

    // Security beacon pylons flanking doors
    if ((abs_x == 4 || abs_z == 4) && y <= room.floor_y + 2 && (hash_coord(x, y, z, 12) % 4 == 0)) {
        out = Voxel{MAT_TITANIUM, 0};
        return;
    }

    if (y >= room.ceiling_y - 3) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_crumbling_canyon(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 115) - 0.5f) * 0.30f);
    if (r_horiz > r_wall) {
        check_doorway_or_solid(room, dx, dz, y, MAT_VOLCANIC_BASALT, out);
        return;
    }

    // Sinuous canyon gorge cutting along Z axis
    float gorge_center = std::sin(static_cast<float>(dz) * 0.38f) * 1.6f;
    float dist_to_gorge = std::abs(static_cast<float>(dx) - gorge_center);
    bool in_gorge = (dist_to_gorge <= 2.8f);

    // Three natural stone bridges spanning the gorge
    bool bridge_south  = in_gorge && (dz >= -5 && dz <= -4) && (y == room.floor_y + 3);
    bool bridge_center = in_gorge && (std::abs(dz) <= 1)    && (y == room.floor_y + 6);
    bool bridge_north  = in_gorge && (dz >= 4 && dz <= 5)   && (y == room.floor_y + 4);

    if (bridge_south || bridge_center || bridge_north) {
        if (bridge_center && std::abs(dx) <= 1) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        }
        return;
    }

    if (in_gorge) {
        if (y <= room.floor_y) {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
            return;
        }
        // Hanging stalactites from ceiling (grapple anchors)
        if (y >= room.ceiling_y - 4 && dist_to_gorge <= 1.0f && (std::abs(dz) % 4 == 0)) {
            out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
            return;
        }
        out = Voxel{MAT_AIR, 0};
        return;
    }

    // Carved entrance walkways connecting West/East and North/South corridors into gorge
    bool is_rim_walkway = (dist_to_gorge > 2.8f && std::abs(dz) <= 2) || (std::abs(dz) > 3 && dist_to_gorge <= 2.0f);
    if (is_rim_walkway) {
        if (y == room.floor_y) {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        } else {
            out = Voxel{MAT_AIR, 0};
        }
        return;
    }

    // Canyon high rim cliffs in flanking corners
    if (y <= room.floor_y + 2) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
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

void LevelGenerator::sample_aquifer_oasis(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 201) - 0.5f) * 0.35f);
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

    // Floor and water pools
    if (y == room.floor_y) {
        // Central crystal aquifer pool
        if (r_horiz <= static_cast<float>(room.half_width) - 3.2f) {
            // Stepping stones and islands
            if ((std::abs(dx) <= 1 && std::abs(dz) <= 1) || (std::abs(dx % 3) == 0 && std::abs(dz % 3) == 0)) {
                out = Voxel{MAT_VOLCANIC_BASALT, 0};
            } else {
                out = Voxel{MAT_CRYSTAL_AQUIFER, 0}; // Liquid crystal water pool
            }
        } else {
            // Verdant oasis shoreline banks: lush bioluminescent moss & soil
            uint32_t florah = hash_coord(x, y, z, 303);
            if ((florah % 10) < 6) {
                out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
            } else {
                out = Voxel{MAT_VOLCANIC_BASALT, 0};
            }
        }
        return;
    }

    // Cascading subterranean waterfall pouring along the West wall
    if (!room.connected_west && dx <= -room.half_width + 2 && std::abs(dz) <= 2 && y > room.floor_y && y <= room.ceiling_y - 3) {
        out = Voxel{MAT_CRYSTAL_AQUIFER, 0}; // Cascading vertical water stream
        return;
    }

    // Prismatic crystals jutting out around the oasis banks (strictly avoid blocking doorways)
    if (y == room.floor_y + 1 && r_horiz >= static_cast<float>(room.half_width) - 3.0f && r_horiz <= static_cast<float>(room.half_width) - 1.5f) {
        if (!is_doorway_air(room, dx, dz, y) && (hash_coord(x, y, z, 404) % 10) == 0) {
            out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
            return;
        }
    }

    // High domed ceiling with moisture stalactites
    if (y >= room.ceiling_y - 2) {
        if (y == room.ceiling_y - 2 && (std::abs(dx % 4) == 0 && std::abs(dz % 4) == 0)) {
            out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
            return;
        }
        out = Voxel{MAT_FRACTURED_GRANITE, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_colossal_abyssal_chasm(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 202) - 0.5f) * 0.35f);
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
        if (std::abs(dx) >= room.half_width - 3 || std::abs(dz) >= room.half_depth - 3) {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
            return;
        }
        // In the center is a lethal punji spike trench
        if (r_horiz <= static_cast<float>(room.half_width) - 3.0f) {
            out = Voxel{MAT_OBSIDIAN_SPIKES, VOXEL_FLAG_EMISSIVE}; // Deadly punji spike trench
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        }
        return;
    }

    // High-tension suspension bridge spanning across the chasm at y = room.floor_y + 8
    // Spans East-West (along X axis) with 3m wide clear central walkway and perimeter railings at dz = +/- 2
    bool on_bridge = (std::abs(dz) <= 2 && y == room.floor_y + 8);
    bool on_railing = (std::abs(dz) == 2 && y == room.floor_y + 9);
    if (on_bridge) {
        out = Voxel{MAT_TITANIUM, 0};
        return;
    }
    if (on_railing) {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
        return;
    }

    // Doorway entrance ledges at room.floor_y on the perimeter so corridors connect safely
    bool is_doorway_walkway = (std::abs(dx) >= room.half_width - 2 || std::abs(dz) >= room.half_depth - 2) && (y == room.floor_y);
    if (is_doorway_walkway) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    // Hanging grapple stalactites from ceiling
    if (y >= room.ceiling_y - 4 && (std::abs(dx % 5) == 0 && std::abs(dz % 5) == 0)) {
        out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
        return;
    }

    if (y >= room.ceiling_y - 1) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_molten_magma_foundry(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 203) - 0.5f) * 0.35f);
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

    // Magma lake & smelting flumes at floor level
    if (y == room.floor_y) {
        // Criss-crossing magma channels
        bool is_magma_channel = (std::abs(dx) <= 2 || std::abs(dz) <= 2) && r_horiz <= static_cast<float>(room.half_width) - 2.5f;
        if (is_magma_channel) {
            out = Voxel{MAT_MOLTEN_MAGMA, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        }
        return;
    }

    // Elevated titanium industrial catwalk at y == room.floor_y + 3
    bool catwalk_perimeter = (r_horiz >= 4.0f && r_horiz <= 6.0f) && (y == room.floor_y + 3);
    bool catwalk_cross = (std::abs(dx) <= 1 || std::abs(dz) <= 1) && (y == room.floor_y + 3);
    if (catwalk_perimeter || catwalk_cross) {
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

void LevelGenerator::sample_toxic_miasma_swamp(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 204) - 0.5f) * 0.35f);
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

    // Pockets of heavy toxic gas lingering in the lower 2 meters (y = floor_y + 1 and floor_y + 2)
    // Sinuous pockets visible across the room!
    if ((y == room.floor_y + 1 || y == room.floor_y + 2) && r_horiz <= static_cast<float>(room.half_width) - 3.0f) {
        // Natural meandering gas pockets
        float gas_wave = std::sin(static_cast<float>(dx) * 0.5f) * std::cos(static_cast<float>(dz) * 0.5f);
        if (gas_wave > -0.2f && (std::abs(dx) > 1 || std::abs(dz) > 1)) {
            out = Voxel{MAT_TOXIC_GAS, VOXEL_FLAG_EMISSIVE};
            return;
        }
    }

    // Elevated winding root bridge at y = room.floor_y + 3 allowing delvers to navigate over gas
    float root_path = std::sin(static_cast<float>(dx) * 0.4f) * 2.2f;
    bool on_root = (std::abs(static_cast<float>(dz) - root_path) <= 1.2f) && (y == room.floor_y + 3);
    if (on_root) {
        out = Voxel{MAT_FRACTURED_GRANITE, 0};
        return;
    }

    // Overhanging fungal spore arches from ceiling
    if (y >= room.ceiling_y - 3 && (std::abs(dx % 4) == 0 || std::abs(dz % 4) == 0)) {
        if ((hash_coord(x, y, z, 606) % 10) < 4) {
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

void LevelGenerator::sample_prismatic_crystal_cathedral(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 205) - 0.5f) * 0.35f);
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

    // Floor at y == room.floor_y:
    if (y == room.floor_y) {
        // Polished crystal dais at center
        if (r_horiz <= 3.5f) {
            out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_FRACTURED_GRANITE, 0};
        }
        return;
    }

    // Stepped central dais stairs (y = floor_y + 1 and floor_y + 2)
    if (y == room.floor_y + 1 && r_horiz <= 2.5f) {
        out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        return;
    }
    if (y == room.floor_y + 2 && r_horiz <= 1.2f) {
        out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        return;
    }

    // 4 Colossal Hexagonal Prismatic Crystal Columns extending floor to ceiling
    int col_offset_x = std::max(2, room.half_width / 2);
    int col_offset_z = std::max(2, room.half_depth / 2);
    bool is_col1 = (std::abs(dx - col_offset_x) <= 1 && std::abs(dz - col_offset_z) <= 1);
    bool is_col2 = (std::abs(dx + col_offset_x) <= 1 && std::abs(dz - col_offset_z) <= 1);
    bool is_col3 = (std::abs(dx - col_offset_x) <= 1 && std::abs(dz + col_offset_z) <= 1);
    bool is_col4 = (std::abs(dx + col_offset_x) <= 1 && std::abs(dz + col_offset_z) <= 1);

    if (is_col1 || is_col2 || is_col3 || is_col4) {
        if (y <= room.ceiling_y - 2) {
            out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
            return;
        }
    }

    // Elevated crystal bridges linking the pillars at y = room.floor_y + 7
    bool bridge_x = (y == room.floor_y + 7 && (std::abs(dz - col_offset_z) <= 1 || std::abs(dz + col_offset_z) <= 1) && std::abs(dx) <= col_offset_x);
    bool bridge_z = (y == room.floor_y + 7 && (std::abs(dx - col_offset_x) <= 1 || std::abs(dx + col_offset_x) <= 1) && std::abs(dz) <= col_offset_z);
    if (bridge_x || bridge_z) {
        out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        return;
    }

    // Vaulted cathedral ceiling with ribbed arches
    if (y >= room.ceiling_y - 2) {
        if ((std::abs(dx) % 3 == 0) || (std::abs(dz) % 3 == 0)) {
            out = Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_FRACTURED_GRANITE, 0};
        }
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_ancient_titan_necropolis(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 206) - 0.5f) * 0.35f);
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
    bool on_rib_ring = (std::abs(dz) % 3 == 0) && (std::abs(dz) <= room.half_depth - 2);
    if (on_rib_ring && y > room.floor_y && y <= room.ceiling_y - 3) {
        float max_arch_h = static_cast<float>(room.ceiling_y - room.floor_y - 4);
        float arch_span = static_cast<float>(std::max(3, room.half_width - 2));
        float norm_x = std::abs(static_cast<float>(dx)) / arch_span;
        if (norm_x <= 1.0f) {
            float expected_y = static_cast<float>(room.floor_y + 1) + max_arch_h * std::sqrt(std::max(0.0f, 1.0f - norm_x * norm_x));
            if (std::abs(static_cast<float>(y) - expected_y) <= 1.0f) {
                out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED}; // Titan bone matrix
                return;
            }
        }
    }

    // Ancient excavation scaffolding along east/west walls at y = room.floor_y + 4
    if ((std::abs(dx) == room.half_width - 2) && (y == room.floor_y + 4)) {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
        return;
    }

    if (y >= room.ceiling_y - 2) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_bioluminescent_glowworm_grotto(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 207) - 0.5f) * 0.35f);
    if (r_horiz > r_wall) {
        check_doorway_or_solid(room, dx, dz, y, MAT_FRACTURED_GRANITE, out);
        return;
    }

    // Subterranean reflecting pool at floor level
    if (y == room.floor_y) {
        if (r_horiz <= static_cast<float>(room.half_width) - 3.0f) {
            // Serene reflecting water pool
            out = Voxel{MAT_CRYSTAL_AQUIFER, 0};
        } else {
            // Winding mossy shoreline
            uint32_t florah = hash_coord(x, y, z, 707);
            if ((florah % 10) < 5) {
                out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
            } else {
                out = Voxel{MAT_VOLCANIC_BASALT, 0};
            }
        }
        return;
    }

    // Starry night glowworm canopy ceiling
    if (y == room.ceiling_y - 2) {
        uint32_t star_hash = hash_coord(x, y, z, 888);
        if ((star_hash % 10) < 4) {
            // Densely clustered bioluminescent glow points
            out = Voxel{MAT_BIOLUMINESCENT_FLORA, VOXEL_FLAG_EMISSIVE};
            return;
        } else if ((star_hash % 20) == 7) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
            return;
        }
    }

    if (y >= room.ceiling_y - 1) {
        out = Voxel{MAT_FRACTURED_GRANITE, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_precursor_coolant_reservoir(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 208) - 0.5f) * 0.35f);
    if (r_horiz > r_wall) {
        check_doorway_or_solid(room, dx, dz, y, MAT_INDUSTRIAL_BULKHEAD, out);
        return;
    }

    // Floor and twin coolant reservoirs
    if (y == room.floor_y) {
        float d_pool1 = std::sqrt(static_cast<float>((dx - 4) * (dx - 4) + dz * dz));
        float d_pool2 = std::sqrt(static_cast<float>((dx + 4) * (dx + 4) + dz * dz));
        if (d_pool1 <= 2.8f || d_pool2 <= 2.8f) {
            out = Voxel{MAT_CRYSTAL_AQUIFER, 0}; // Purified liquid coolant
        } else {
            out = Voxel{MAT_TITANIUM, 0}; // Reinforced steel plating
        }
        return;
    }

    // Overhead high-pressure coolant pipelines running along Z axis over reservoir pools at dx = +/- 4, y = room.floor_y + 5
    if ((std::abs(dx - 4) <= 1 || std::abs(dx + 4) <= 1) && y == room.floor_y + 5) {
        out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
        return;
    }

    // Ruptured pipeline leak spray pouring into eastern coolant pool
    if (dx == 4 && dz == 0 && y > room.floor_y && y < room.floor_y + 5) {
        out = Voxel{MAT_CRYSTAL_AQUIFER, 0}; // Liquid coolant spray stream
        return;
    }

    // Control consoles and reinforced blast catwalks
    if (std::abs(dz) >= room.half_depth - 2 && (y == room.floor_y + 1 || y == room.floor_y + 2)) {
        out = Voxel{MAT_REINFORCED_VAULT_DOOR, 0};
        return;
    }

    if (y >= room.ceiling_y - 2) {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_vaulted_dredge_cathedral(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 301) - 0.5f) * 0.30f);
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
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE}; // Central ceremonial Voidite monolith
        } else {
            out = Voxel{MAT_TITANIUM, 0}; // Stepped dais
        }
        return;
    }

    // High observation gantry / catwalk at y = room.floor_y + 15 (y ~ 19) spanning East-West
    if (y == room.floor_y + 15 && std::abs(dz) <= 2) {
        out = Voxel{MAT_TITANIUM, 0}; // Catwalk bridge deck
        return;
    }
    if (y == room.floor_y + 16 && (std::abs(dz) == 2)) {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0}; // Catwalk safety railings
        return;
    }

    // Flanking buttress columns along north/south walls (dz = +/- (half_depth - 2)) every 4 blocks along X
    if (std::abs(dz) >= room.half_depth - 3 && (std::abs(dx) % 4 == 0) && y < room.ceiling_y - 1) {
        if (y % 4 == 0) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
        }
        return;
    }

    // Ribbed cathedral vault arches overhead at y >= room.ceiling_y - 2 (every 4 blocks in X)
    if (y >= room.ceiling_y - 2 && (std::abs(dx) % 4 == 0)) {
        out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
        return;
    }

    // Grand stalactites hanging from the vault roof
    if (y >= room.ceiling_y - 3 && (hash_coord(x, y, z, 311) % 13 == 0) && r_horiz >= 3.0f) {
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

void LevelGenerator::sample_tectonic_sinkhole(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 302) - 0.5f) * 0.35f);
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
    if (y == room.floor_y) {
        // Glowing molten fissure in the central trough
        if (std::abs(dz) <= 1 && r_horiz <= static_cast<float>(room.half_width) - 2.5f) {
            out = Voxel{MAT_MOLTEN_MAGMA, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        }
        return;
    }

    // Stepped spiral terraces descending from rim:
    // Outer terrace at y = room.floor_y + 14 (rim shelf)
    bool is_outer_shelf = (r_horiz >= static_cast<float>(room.half_width) - 2.8f) && (y == room.floor_y + 14);
    if (is_outer_shelf) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }
    // Mid terrace at y = room.floor_y + 8
    bool is_mid_shelf = (r_horiz >= 5.5f && r_horiz <= 7.5f) && (angle > -1.0f && angle < 2.0f) && (y == room.floor_y + 8);
    if (is_mid_shelf) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    // Central stepping spire monolith rising from abyss bed to y = room.floor_y + 10
    if (r_horiz <= 2.2f && y <= room.floor_y + 10) {
        if (y == room.floor_y + 10) {
            out = Voxel{MAT_TITANIUM, 0}; // Monolith lookout cap
        } else if (y % 4 == 0) {
            out = Voxel{MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
        } else {
            out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
        }
        return;
    }

    // High tension cable bridge crossing the sinkhole at y = room.floor_y + 14 along dz = 0
    if (std::abs(dz) <= 2 && y == room.floor_y + 14) {
        out = Voxel{MAT_TITANIUM, 0};
        return;
    }
    if ((std::abs(dz) == 2) && y == room.floor_y + 15) {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0}; // Guardrail
        return;
    }

    // Hanging stalactites from ceiling
    if (y >= room.ceiling_y - 3 && (hash_coord(x, y, z, 321) % 11 == 0) && r_horiz >= 2.5f) {
        out = Voxel{MAT_VOLCANIC_BASALT, VOXEL_FLAG_ANCHORED};
        return;
    }

    if (y >= room.ceiling_y) {
        out = Voxel{MAT_VOLCANIC_BASALT, 0};
        return;
    }

    out = Voxel{MAT_AIR, 0};
}

void LevelGenerator::sample_cyclopean_silo(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 303) - 0.5f) * 0.20f);
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
            out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED}; // Excavation hopper center
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
    if (y == room.floor_y + 16 && r_horiz >= r_ring_min && r_horiz <= r_ring_max) {
        out = Voxel{MAT_TITANIUM, 0};
        return;
    }
    if (y == room.floor_y + 17 && (std::abs(r_horiz - r_ring_min) <= 0.6f)) {
        out = Voxel{MAT_INDUSTRIAL_BULKHEAD, 0}; // High-level inner safety railing
        return;
    }

    // Vertical structural guide columns & conduit pipes along cardinal perimeter walls
    if ((std::abs(dx) <= 1 || std::abs(dz) <= 1) && r_horiz >= static_cast<float>(room.half_width) - 2.0f && y < room.ceiling_y) {
        if (y % 4 == 2) {
            out = Voxel{MAT_TITANIUM, VOXEL_FLAG_ANCHORED};
        } else {
            out = Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED};
        }
        return;
    }

    // Overhead heavy crane girder at y = room.ceiling_y - 2 spanning X axis across diameter
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

void LevelGenerator::sample_firmament_abyss(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
    int dx = x - room.center.x;
    int dz = z - room.center.z;
    float r_horiz = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    float angle = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

    float r_wall = compute_room_wall_radius(room, static_cast<float>(dx), static_cast<float>(dz), angle, (pseudo_rand(x, y, z, 304) - 0.5f) * 0.35f);
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

    // Undulating natural floor with crystal aquifer reflecting pools at y == room.floor_y
    if (y == room.floor_y) {
        float d_pool = std::sqrt(static_cast<float>((dx - 2) * (dx - 2) + (dz + 2) * (dz + 2)));
        if (d_pool <= 3.8f) {
            out = Voxel{MAT_CRYSTAL_AQUIFER, 0}; // Glowing crystal water pool
        } else if (r_horiz <= 4.0f) {
            out = Voxel{MAT_VOLCANIC_BASALT, 0};
        } else {
            out = Voxel{MAT_FRACTURED_GRANITE, 0};
        }
        return;
    }

    // High natural stone arch bridge soaring across the cavern at y = room.floor_y + 15 (y ~ 19)
    // Spans diagonally from SW to NE (dx ~ dz)
    bool on_stone_arch = (std::abs(dx - dz) <= 1 && y == room.floor_y + 15);
    if (on_stone_arch) {
        out = Voxel{MAT_FRACTURED_GRANITE, 0};
        return;
    }

    // Starry Celestial Firmament embedded in the high vault ceiling at y >= room.ceiling_y - 2
    if (y >= room.ceiling_y - 2) {
        uint32_t star_hash = hash_coord(x, y, z, 555);
        if (y == room.ceiling_y - 2 && (star_hash % 9 == 0)) {
            // Emissive star crystal embedded in dark rock ceiling
            out = Voxel{(star_hash % 2 == 0) ? MAT_PRISMATIC_CRYSTAL : MAT_VOIDITE_CRYSTAL, VOXEL_FLAG_EMISSIVE};
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

} // namespace Voidfall
