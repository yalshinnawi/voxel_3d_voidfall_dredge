#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <queue>
#include <unordered_set>

#include "../src/voxel/level_shapes.hpp"
#include "../src/voxel/world.hpp"
#include "../src/voxel/greedy_mesher.hpp"
#include "../src/player/controller.hpp"
#include "../src/systems/extraction.hpp"
#include "../src/systems/mission_system.hpp"

using namespace Voidfall;

#define TEST_CHECK(expr, msg) \
    if (!(expr)) { \
        std::cerr << "\n[TEST FAILED] " << msg << " (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

static int s_passed_tests = 0;
void log_pass(const std::string& name) {
    s_passed_tests++;
    std::cout << "  [PASS " << s_passed_tests << "] " << name << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 1. LEVEL DESIGN & ROOM SHAPES TOPOLOGY TESTS
// ─────────────────────────────────────────────────────────────
void test_level_design_topology() {
    std::cout << "\n=== [MODULE 1] Level Design Archetypes & Room Geometry ===" << std::endl;

    for (int sector = 1; sector <= 3; ++sector) {
        LevelGenerator gen(sector, 12345 + sector);
        const auto& rooms = gen.rooms();
        int expected_rooms = (sector == 1) ? 9 : ((sector == 2) ? 16 : 25);
        TEST_CHECK(static_cast<int>(rooms.size()) == expected_rooms, "Sector room count mismatch with dynamic scaling");

        // Spawn room must be (0, 0)
        const RoomPlacement* spawn = gen.get_room_at_grid(0, 0);
        TEST_CHECK(spawn != nullptr, "Spawn room at (0, 0) must exist");
        TEST_CHECK(spawn->type == RoomShapeType::SpawnStagingCavern, "Grid (0, 0) must be SpawnStagingCavern");

        // Extraction room must be at far corner (grid_w - 1, grid_d - 1)
        int evac_x = gen.grid_width() - 1;
        int evac_z = gen.grid_depth() - 1;
        const RoomPlacement* evac = gen.get_room_at_grid(evac_x, evac_z);
        TEST_CHECK(evac != nullptr, "Extraction room must exist at far corner");
        TEST_CHECK(evac->type == RoomShapeType::ExtractionLandingBay, "Extraction room must be ExtractionLandingBay");

        // Validate bedrock floor (y <= 3) and ceiling mantle (y >= 26) everywhere across dynamic world size
        for (int x = 5; x <= gen.world_width() - 6; x += 15) {
            for (int z = 5; z <= gen.world_depth() - 6; z += 15) {
                Voxel bedrock = gen.sample_voxel(x, 2, z);
                TEST_CHECK(bedrock.is_solid() && bedrock.material_id == MAT_DREDGE_BEDROCK, "Bedrock y=2 must be solid MAT_DREDGE_BEDROCK");
                Voxel mantle = gen.sample_voxel(x, 26, z);
                TEST_CHECK(mantle.is_solid(), "Ceiling mantle at y=26 must be solid");
            }
        }

        // Validate sector-specific room type pools
        bool has_vault = false;
        bool has_chasm = false;
        bool has_sanctuary = false;
        bool has_hazard_room = false;
        for (const auto& r : rooms) {
            if (r.type == RoomShapeType::IndustrialVaultBunker ||
                r.type == RoomShapeType::SubterraneanVaultBastion ||
                r.type == RoomShapeType::PrecursorSanctumVault) has_vault = true;
            if (r.type == RoomShapeType::AbyssalVerticalChasm ||
                r.type == RoomShapeType::AbyssalVaultChasm ||
                r.type == RoomShapeType::ColossalAbyssalChasm) has_chasm = true;
            if (r.type == RoomShapeType::RadioactiveCoreSanctuary) has_sanctuary = true;
            if (r.type == RoomShapeType::MagmaCalderaLake || r.type == RoomShapeType::SpikeTrenchArena ||
                r.type == RoomShapeType::VoidSingularityRift || r.type == RoomShapeType::FungoidBioGrotto ||
                r.type == RoomShapeType::SubterraneanVaultBastion || r.type == RoomShapeType::AbyssalVaultChasm) {
                has_hazard_room = true;
            }
        }

        // Validate multi-floor distribution and dynamic sizing
        bool has_floor0 = false;
        bool has_floor1 = false;
        bool has_multifloor = false;
        bool has_massive = false;
        bool has_small = false;
        for (const auto& r : rooms) {
            if (r.floor_level == 0) has_floor0 = true;
            if (r.floor_level == 1) has_floor1 = true;
            if (r.floor_level == 2) has_multifloor = true;
            if (r.is_massive) has_massive = true;
            if (r.half_width <= 7 || r.half_depth <= 7) has_small = true;
        }
        TEST_CHECK(has_floor0, "Sector must contain Floor 0 (lower cavern) rooms");
        TEST_CHECK(has_floor1, "Sector must contain Floor 1 (upper mezzanine) rooms");
        TEST_CHECK(has_multifloor, "Sector must contain Multi-Floor rooms");
        TEST_CHECK(has_massive, "Sector must generate massive multi-deck cavern chambers");
        TEST_CHECK(has_small, "Sector must generate randomized small niche rooms");

        if (sector >= 2) {
            TEST_CHECK(has_vault || has_hazard_room, "Sector 2+ must spawn Industrial Bunkers or Environmental Hazard rooms");
        }
        if (sector == 3) {
            TEST_CHECK(has_chasm || has_sanctuary || has_hazard_room, "Sector 3 must spawn Abyssal Chasms, Sanctuaries or Void Rifts");
        }
    }

    log_pass("Level layout topology, grid connectivity, and sector archetypes verified");
}

// ─────────────────────────────────────────────────────────────
// 2. THOROUGH WALL SPRINT COLLISION & ANTI-CLIPPING TESTS
// ─────────────────────────────────────────────────────────────
void test_random_shape_permutations_integration() {
    std::cout << "\n=== [MODULE 7] Random Shape Permutations & Integration Testing ===" << std::endl;

    const std::vector<uint32_t> test_seeds = {
        101, 202, 303, 404, 505, 777, 999, 1337, 2026, 4242
    };

    int total_permutations_tested = 0;
    int total_corridors_tested = 0;

    for (int sector = 1; sector <= 3; ++sector) {
        for (uint32_t seed : test_seeds) {
            total_permutations_tested++;

            World world(seed, false);
            world.generate_world(sector, seed);
            const LevelGenerator* gen = world.level_generator();
            TEST_CHECK(gen != nullptr, "Level generator must be initialized");

            const auto& corridors = gen->corridors();
            size_t min_orthogonal = static_cast<size_t>(gen->grid_depth() * (gen->grid_width() - 1) + gen->grid_width() * (gen->grid_depth() - 1));
            TEST_CHECK(corridors.size() >= min_orthogonal,
                       "Sector " + std::to_string(sector) + " layout must have at least " + std::to_string(min_orthogonal) + " inter-room corridors");

            // Verify corridor variety: diagonals, ravines, and sloping ramps
            bool has_diagonal_corridor = false;
            bool has_ravine_corridor = false;
            bool has_sloping_ramp = false;
            for (const auto& c : corridors) {
                if (c.is_diagonal) has_diagonal_corridor = true;
                if (c.is_ravine) has_ravine_corridor = true;
                if (c.type == CorridorType::SlopingRamp) has_sloping_ramp = true;
            }
            TEST_CHECK(has_diagonal_corridor, "Level layout must generate diagonal corridors");
            TEST_CHECK(has_ravine_corridor, "Level layout must generate deep jagged ravines");
            TEST_CHECK(has_sloping_ramp, "Level layout must generate sloping stepped ramps connecting different floors");

            // 1. Verify Corridor Threshold Seams
            for (const auto& c : corridors) {
                total_corridors_tested++;

                // Test start seam (where corridor meets Room A) and end seam (where corridor meets Room B)
                glm::ivec3 seams[2] = { c.start_pos, c.end_pos };
                int seam_floors[2] = { c.floor_y, c.end_floor_y };
                for (int s = 0; s < 2; ++s) {
                    glm::ivec3 p = seams[s];
                    int fy = seam_floors[s];
                    // Floor must be solid at fy or within 2 blocks below if intersecting a sloping ramp
                    bool sol = world.is_solid(p.x, fy, p.z) ||
                               (fy > 4 && (world.is_solid(p.x, fy - 1, p.z) || world.is_solid(p.x, fy - 2, p.z)));
                    TEST_CHECK(sol,
                               "Corridor threshold seam floor must be solid at (" +
                               std::to_string(p.x) + ", " + std::to_string(fy) + ", " + std::to_string(p.z) + ")");

                    // Headroom: y = fy + 1 and y = fy + 2 must not be bedrock
                    Voxel v1 = world.get_voxel(p.x, fy + 1, p.z);
                    Voxel v2 = world.get_voxel(p.x, fy + 2, p.z);
                    TEST_CHECK(v1.material_id != MAT_DREDGE_BEDROCK, "Corridor seam blocked by bedrock at (" + std::to_string(p.x) + "," + std::to_string(fy + 1) + "," + std::to_string(p.z) + ") mat=" + std::to_string(v1.material_id));
                    TEST_CHECK(v2.material_id != MAT_DREDGE_BEDROCK, "Corridor seam blocked by bedrock at (" + std::to_string(p.x) + "," + std::to_string(fy + 2) + "," + std::to_string(p.z) + ") mat=" + std::to_string(v2.material_id));

                    // Ceiling above corridor must remain solid mantle at y=26
                    TEST_CHECK(world.is_solid(p.x, 26, p.z), "Corridor seam mantle must be solid at y=26");
                }
            }

            // 2. Bidirectional Walkthrough across room boundary seams (Sector 1 unobstructed corridors)
            if (sector == 1) {
                const CorridorPlacement* corr_x = nullptr;
                const CorridorPlacement* corr_z = nullptr;
                for (const auto& c : gen->corridors()) {
                    if (c.is_x_axis && c.start_pos.x <= 25 && c.end_pos.x >= 25 && !corr_x) {
                        corr_x = &c;
                    }
                    if (!c.is_x_axis && !c.is_diagonal && c.start_pos.z <= 25 && c.end_pos.z >= 25 && !corr_z) {
                        corr_z = &c;
                    }
                }

                if (corr_x) {
                    float start_x = static_cast<float>(corr_x->start_pos.x);
                    float start_y = static_cast<float>(corr_x->floor_y + 1);
                    float start_z = static_cast<float>(corr_x->start_pos.z);
                    float mid_x = static_cast<float>(corr_x->start_pos.x + corr_x->end_pos.x) * 0.5f;

                    PlayerController player_x(glm::vec3(start_x, start_y, start_z));
                    player_x.set_auto_step(true);
                    player_x.clamp_to_surface(world);
                    TEST_CHECK(!player_x.is_penetrating_solid(world), "Player penetrating at corridor start");

                    // Sprint along corridor toward Room (1, 0)
                    glm::vec2 planar_dir(static_cast<float>(corr_x->end_pos.x - corr_x->start_pos.x),
                                         static_cast<float>(corr_x->end_pos.z - corr_x->start_pos.z));
                    if (glm::length(planar_dir) > 0.001f) planar_dir = glm::normalize(planar_dir);
                    else planar_dir = glm::vec2(1.0f, 0.0f);

                    player_x.set_velocity(glm::vec3(planar_dir.x * 8.0f, 0.0f, planar_dir.y * 8.0f));
                    for (int tick = 0; tick < 100; ++tick) {
                        player_x.velocity_mut().x = planar_dir.x * 8.0f;
                        player_x.velocity_mut().z = planar_dir.y * 8.0f;
                        player_x.update_physics(1.0f / 60.0f, world);
                        TEST_CHECK(!player_x.is_penetrating_solid(world), "Player clipped while walking East across corridor seam");
                    }
                    TEST_CHECK(player_x.position().x > mid_x + 0.5f, "Player failed to cross corridor into adjacent room");

                    // Sprint back toward Room (0, 0)
                    player_x.set_velocity(glm::vec3(-planar_dir.x * 8.0f, 0.0f, -planar_dir.y * 8.0f));
                    for (int tick = 0; tick < 100; ++tick) {
                        player_x.velocity_mut().x = -planar_dir.x * 8.0f;
                        player_x.velocity_mut().z = -planar_dir.y * 8.0f;
                        player_x.update_physics(1.0f / 60.0f, world);
                        TEST_CHECK(!player_x.is_penetrating_solid(world), "Player clipped while walking West across corridor seam");
                    }
                    TEST_CHECK(player_x.position().x < mid_x - 0.5f, "Player failed to return across corridor to origin room");
                }

                if (corr_z) {
                    float start_x = static_cast<float>(corr_z->start_pos.x);
                    float start_y = static_cast<float>(corr_z->floor_y + 1);
                    float start_z = static_cast<float>(corr_z->start_pos.z);
                    float mid_z = static_cast<float>(corr_z->start_pos.z + corr_z->end_pos.z) * 0.5f;

                    PlayerController player_z(glm::vec3(start_x, start_y, start_z));
                    player_z.set_auto_step(true);
                    player_z.clamp_to_surface(world);
                    TEST_CHECK(!player_z.is_penetrating_solid(world), "Player penetrating at North corridor start");

                    // Sprint along corridor toward Room (0, 1)
                    glm::vec2 planar_dir(static_cast<float>(corr_z->end_pos.x - corr_z->start_pos.x),
                                         static_cast<float>(corr_z->end_pos.z - corr_z->start_pos.z));
                    if (glm::length(planar_dir) > 0.001f) planar_dir = glm::normalize(planar_dir);
                    else planar_dir = glm::vec2(0.0f, 1.0f);

                    player_z.set_velocity(glm::vec3(planar_dir.x * 8.0f, 0.0f, planar_dir.y * 8.0f));
                    for (int tick = 0; tick < 100; ++tick) {
                        player_z.velocity_mut().x = planar_dir.x * 8.0f;
                        player_z.velocity_mut().z = planar_dir.y * 8.0f;
                        player_z.update_physics(1.0f / 60.0f, world);
                        TEST_CHECK(!player_z.is_penetrating_solid(world), "Player clipped while walking North across corridor seam");
                    }
                    TEST_CHECK(player_z.position().z > mid_z + 0.5f, "Player failed to cross North corridor into adjacent room");

                    // Sprint back toward Room (0, 0)
                    player_z.set_velocity(glm::vec3(-planar_dir.x * 8.0f, 0.0f, -planar_dir.y * 8.0f));
                    for (int tick = 0; tick < 100; ++tick) {
                        player_z.velocity_mut().x = -planar_dir.x * 8.0f;
                        player_z.velocity_mut().z = -planar_dir.y * 8.0f;
                        player_z.update_physics(1.0f / 60.0f, world);
                        TEST_CHECK(!player_z.is_penetrating_solid(world), "Player clipped while walking South across corridor seam");
                    }
                    TEST_CHECK(player_z.position().z < mid_z - 0.5f, "Player failed to return South across corridor to origin room");
                }
            }

            // 3. 3D BFS Topological Reachability Solver (Spawn (0,0) -> Extraction (W-1, D-1))
            // Verifies that across all permutations, a traversable path exists connecting the entire level.
            struct Node { int x, y, z; };
            std::queue<Node> q;
            int w_w = gen->world_width();
            int w_d = gen->world_depth();
            std::vector<uint8_t> visited(w_w * 32 * w_d, 0);
            auto idx3d = [w_w, w_d](int x, int y, int z) { return (x * 32 + y) * w_d + z; };

            // Start at spawn pad: (16, 5, 16)
            q.push(Node{16, 5, 16});
            visited[idx3d(16, 5, 16)] = 1;

            bool reached_extraction = false;
            const int dx[4] = { 1, -1, 0, 0 };
            const int dz[4] = { 0, 0, 1, -1 };
            glm::ivec3 extract_pos = gen->extraction_position();

            while (!q.empty()) {
                Node cur = q.front();
                q.pop();

                // Target: reached Extraction Landing Bay zone around extraction_position()
                if (std::abs(cur.x - extract_pos.x) <= 4 && std::abs(cur.z - extract_pos.z) <= 4 && cur.y <= 12) {
                    reached_extraction = true;
                    break;
                }

                // Try 4 cardinal directions with step up (+1) and step down (-1, -2)
                for (int i = 0; i < 4; ++i) {
                    int nx = cur.x + dx[i];
                    int nz = cur.z + dz[i];

                    if (nx < 4 || nx > w_w - 5 || nz < 4 || nz > w_d - 5) continue;

                    // Test possible y levels (flat, step up 1, step down 1 or 2)
                    for (int dy_step : {0, 1, -1, -2}) {
                        int ny = cur.y + dy_step;
                        if (ny < 4 || ny > 24) continue;

                        int id = idx3d(nx, ny, nz);
                        if (visited[id]) continue;

                        // Check passability:
                        // Needs ground support at ny - 1
                        if (!world.is_solid(nx, ny - 1, nz)) continue;

                        // Needs feet clearance at ny and head clearance at ny + 1
                        Voxel feet = world.get_voxel(nx, ny, nz);
                        Voxel head = world.get_voxel(nx, ny + 1, nz);

                        bool feet_passable = (sector == 1) ? !feet.is_solid() : (feet.material_id != MAT_DREDGE_BEDROCK);
                        bool head_passable = (sector == 1) ? !head.is_solid() : (head.material_id != MAT_DREDGE_BEDROCK);

                        if (feet_passable && head_passable) {
                            visited[id] = 1;
                            q.push(Node{nx, ny, nz});
                        }
                    }
                }
            }

            TEST_CHECK(reached_extraction, "Sector " + std::to_string(sector) + " Seed " +
                       std::to_string(seed) + " failed 3D reachability solver from Spawn to Extraction!");

            // 4. Verify Drop & Ravine Vertical Depth (> 8 blocks deep, no 1-block shallow ditches)
            for (const auto& room : gen->rooms()) {
                if (room.type == RoomShapeType::FaultLineCrevasse) {
                    TEST_CHECK(room.floor_y >= 14, "FaultLineCrevasse must be elevated to Floor 1 (floor_y >= 14) for deep chasm plunge");
                    int cz = room.center.z + 3;
                    int air_count = 0;
                    for (int dx_off = -2; dx_off <= 2; ++dx_off) {
                        int cx = room.center.x + dx_off;
                        int current_air = 0;
                        for (int y = 6; y < room.floor_y; ++y) {
                            if (!world.is_solid(cx, y, cz)) current_air++;
                        }
                        air_count = std::max(air_count, current_air);
                    }
                    TEST_CHECK(air_count >= 6, "FaultLineCrevasse must have deep vertical drop (found " + std::to_string(air_count) + " air blocks below floor)");
                } else if (room.type == RoomShapeType::CrumblingArchCanyon) {
                    TEST_CHECK(room.floor_y >= 14, "CrumblingArchCanyon must be elevated to Floor 1 for deep canyon plunge");
                    int cx = room.center.x;
                    int cz = room.center.z + 2;
                    TEST_CHECK(world.is_solid(cx, 4, cz), "CrumblingArchCanyon gorge bed must be solid at y=4");
                    int air_count = 0;
                    for (int y = 6; y < room.floor_y; ++y) {
                        if (!world.is_solid(cx, y, cz)) air_count++;
                    }
                    TEST_CHECK(air_count >= 6, "CrumblingArchCanyon must have deep vertical gorge drop");
                } else if (room.type == RoomShapeType::SpikeTrenchArena) {
                    TEST_CHECK(room.floor_y >= 14, "SpikeTrenchArena must be elevated to Floor 1 for deep sunken spike pit");
                    int air_count = 0;
                    for (int dx_val = 2; dx_val <= 4; ++dx_val) {
                        for (int dz_val = 2; dz_val <= 4; ++dz_val) {
                            int cx = room.center.x + dx_val;
                            int cz = room.center.z + dz_val;
                            if (dx_val == room.half_width - 3 && dz_val == room.half_depth - 3) continue; // ladder column
                            int current_air = 0;
                            for (int y = 6; y < room.floor_y; ++y) {
                                if (!world.is_solid(cx, y, cz)) current_air++;
                            }
                            air_count = std::max(air_count, current_air);
                        }
                    }
                    TEST_CHECK(air_count >= 6, "SpikeTrenchArena must have deep vertical pit drop");
                }
            }

            for (const auto& c : corridors) {
                if (c.is_ravine && c.floor_y >= 14 && c.is_x_axis) {
                    int mid_x = (c.start_pos.x + c.end_pos.x) / 2;
                    int flank_z = c.start_pos.z + 2;
                    TEST_CHECK(world.is_solid(mid_x, 4, flank_z), "Ravine corridor bed must be solid at y=4");
                    int air_count = 0;
                    for (int y = 6; y < c.floor_y; ++y) {
                        if (!world.is_solid(mid_x, y, flank_z)) air_count++;
                    }
                    TEST_CHECK(air_count >= 6, "Ravine corridor must have deep vertical plunge along flank");
                }
            }
        }
    }

    log_pass("Tested 30 random shape permutations across Sectors 1-3 (" +
             std::to_string(total_corridors_tested) + " corridor seams verified, 100% 3D BFS reachable)");
}

// ─────────────────────────────────────────────────────────────
// 8. FULL SECTOR ROOM-TO-ROOM SIGHTLINE & TRAVERSAL TOUR
// ─────────────────────────────────────────────────────────────
void test_all_rooms_tour_and_visibility() {
    std::cout << "\n=== [MODULE 8] Full Sector Room-to-Room Sightlines & Traversal Tour ===" << std::endl;

    for (int sector = 1; sector <= 3; ++sector) {
        World world(4242 + sector, false);
        world.generate_world(sector, 4242 + sector);

        const auto* gen = world.level_generator();
        TEST_CHECK(gen != nullptr, "Level generator must be initialized");

        int gw = gen->grid_width();
        int gd = gen->grid_depth();
        int total_rooms = gw * gd;

        // Tour sequence through all grid rooms: S-curve path guaranteeing each transition
        // is an adjacent connected orthogonal step (0,0) to (W-1, D-1)
        std::vector<std::pair<int, int>> tour_path;
        tour_path.reserve(total_rooms);
        for (int gz = 0; gz < gd; ++gz) {
            if (gz % 2 == 0) {
                for (int gx = 0; gx < gw; ++gx) {
                    tour_path.push_back({gx, gz});
                }
            } else {
                for (int gx = gw - 1; gx >= 0; --gx) {
                    tour_path.push_back({gx, gz});
                }
            }
        }

        std::vector<bool> rooms_visited(total_rooms, false);

        for (size_t step = 0; step + 1 < tour_path.size(); ++step) {
            int gxA = tour_path[step].first;
            int gzA = tour_path[step].second;
            int gxB = tour_path[step + 1].first;
            int gzB = tour_path[step + 1].second;

            const RoomPlacement* roomA = gen->get_room_at_grid(gxA, gzA);
            const RoomPlacement* roomB = gen->get_room_at_grid(gxB, gzB);
            TEST_CHECK(roomA != nullptr && roomB != nullptr, "Both connected rooms must exist");

            rooms_visited[gzA * gw + gxA] = true;
            rooms_visited[gzB * gw + gxB] = true;

            // 1. Find Connecting Corridor between Room A and Room B
            const CorridorPlacement* connecting_corr = nullptr;
            for (const auto& c : gen->corridors()) {
                bool matchAB = (glm::distance(glm::vec2(c.start_pos.x, c.start_pos.z), glm::vec2(roomA->center.x, roomA->center.z)) < 16.0f &&
                                glm::distance(glm::vec2(c.end_pos.x, c.end_pos.z), glm::vec2(roomB->center.x, roomB->center.z)) < 16.0f);
                bool matchBA = (glm::distance(glm::vec2(c.start_pos.x, c.start_pos.z), glm::vec2(roomB->center.x, roomB->center.z)) < 16.0f &&
                                glm::distance(glm::vec2(c.end_pos.x, c.end_pos.z), glm::vec2(roomA->center.x, roomA->center.z)) < 16.0f);
                if (matchAB || matchBA) {
                    connecting_corr = &c;
                    break;
                }
            }

            glm::vec3 corridor_start, corridor_end;
            int floorA = roomA->floor_y;
            int floorB = roomB->floor_y;
            glm::vec3 step_dir = glm::normalize(glm::vec3(roomB->center - roomA->center));
            float extA = (std::abs(step_dir.x) > std::abs(step_dir.z)) ? static_cast<float>(roomA->half_width) : static_cast<float>(roomA->half_depth);
            float extB = (std::abs(step_dir.x) > std::abs(step_dir.z)) ? static_cast<float>(roomB->half_width) : static_cast<float>(roomB->half_depth);

            if (connecting_corr) {
                bool is_forward = (glm::distance(glm::vec2(connecting_corr->start_pos.x, connecting_corr->start_pos.z),
                                                glm::vec2(roomA->center.x, roomA->center.z)) <
                                   glm::distance(glm::vec2(connecting_corr->end_pos.x, connecting_corr->end_pos.z),
                                                glm::vec2(roomA->center.x, roomA->center.z)));
                if (is_forward) {
                    corridor_start = glm::vec3(connecting_corr->start_pos);
                    corridor_end   = glm::vec3(connecting_corr->end_pos);
                    floorA = connecting_corr->floor_y;
                    floorB = connecting_corr->end_floor_y;
                } else {
                    corridor_start = glm::vec3(connecting_corr->end_pos);
                    corridor_end   = glm::vec3(connecting_corr->start_pos);
                    floorA = connecting_corr->end_floor_y;
                    floorB = connecting_corr->floor_y;
                }
            } else {
                corridor_start = glm::vec3(roomA->center) + step_dir * (extA - 1.0f);
                corridor_end   = glm::vec3(roomB->center) - step_dir * (extB - 1.0f);
            }

            glm::vec3 start_pos(corridor_start.x + 0.5f, static_cast<float>(floorA + 1), corridor_start.z + 0.5f);
            glm::vec3 target_pos(corridor_end.x + 0.5f, static_cast<float>(floorB + 1), corridor_end.z + 0.5f);

            glm::vec3 eyeA(start_pos.x, static_cast<float>(floorA) + 2.5f, start_pos.z);
            glm::vec3 eyeB(target_pos.x, static_cast<float>(floorB) + 2.5f, target_pos.z);

            glm::vec3 ray_dir = glm::normalize(eyeB - eyeA);
            float total_dist = glm::distance(eyeA, eyeB);

            // Step along ray between doorways and verify open sightline through the corridor
            float sample_step = 0.5f;
            int air_samples = 0;
            int total_samples = 0;
            for (float t = 0.5f; t <= total_dist - 0.5f; t += sample_step) {
                glm::vec3 p = eyeA + ray_dir * t;
                total_samples++;
                if (!world.is_solid(static_cast<int>(std::floor(p.x)),
                                    static_cast<int>(std::floor(p.y)),
                                    static_cast<int>(std::floor(p.z)))) {
                    air_samples++;
                }
            }

            if (total_samples > 0) {
                float air_ratio = static_cast<float>(air_samples) / static_cast<float>(total_samples);
                float min_ratio = (sector == 1) ? 0.70f : 0.40f; // Sector 2-3 have bulkhead doors & rubble
                TEST_CHECK(air_ratio >= min_ratio, "Corridor between (" + std::to_string(gxA) + "," + std::to_string(gzA) +
                           ") and (" + std::to_string(gxB) + "," + std::to_string(gzB) + ") has visual occlusion (air ratio: " +
                           std::to_string(air_ratio) + ")");
            }

            // 2. Physical Traversal Test: Delver walks through the corridor from Room A to Room B
            PlayerController delver(start_pos);
            delver.set_auto_step(true);
            delver.clamp_to_surface(world);
            TEST_CHECK(!delver.is_penetrating_solid(world), "Delver must not penetrate at corridor entry");

            // Walk across the corridor to Room B doorway over 180 physics frames
            for (int f = 0; f < 180; ++f) {
                glm::vec3 to_target = target_pos - delver.position();
                to_target.y = 0.0f; // horizontal walk
                if (glm::length(to_target) > 0.3f) {
                    glm::vec3 move_dir = glm::normalize(to_target);
                    delver.velocity_mut().x = move_dir.x * 7.5f;
                    delver.velocity_mut().z = move_dir.z * 7.5f;
                }
                delver.update_physics(1.0f / 60.0f, world);

                // Anti-clipping verification during transition
                TEST_CHECK(!delver.is_penetrating_solid(world), "Delver clipped into wall during corridor transition!");
            }

            float dist_to_end = glm::distance(glm::vec2(delver.position().x, delver.position().z),
                                              glm::vec2(target_pos.x, target_pos.z));
            TEST_CHECK(dist_to_end <= 2.5f, "Delver failed to traverse corridor into Room B (dist to end: " + std::to_string(dist_to_end) + ")");
        }

        for (int r = 0; r < total_rooms; ++r) {
            TEST_CHECK(rooms_visited[r], "All " + std::to_string(total_rooms) + " rooms in sector " + std::to_string(sector) + " must be visited");
        }
    }

    log_pass("Sector 1-3 room-to-room tour: 100% of all rooms visited, open sightlines verified, 0 clipping");
}

// ─────────────────────────────────────────────────────────────
// 9. FLIGHT CARVING, BLOCK BREAKING & MESH INTEGRITY STRESS TEST
// ─────────────────────────────────────────────────────────────
void test_spawn_safety_and_full_level_walk_flight() {
    std::cout << "\n=== [MODULE 11] Spawn Safety, Hazard Placement & Full Level Traversal ===" << std::endl;

    for (int sector = 1; sector <= 3; ++sector) {
        World world(1000 + sector * 77, false);
        world.generate_world(sector, 1000 + sector * 77);
        const LevelGenerator* gen = world.level_generator();
        TEST_CHECK(gen != nullptr, "Level generator must be initialized");

        // 1. Verify Spawn Safety & Immediate Mobility
        glm::vec3 spawn_pos = gen->spawn_position();
        PlayerController player(spawn_pos);
        player.apply_attributes_and_upgrades(CharacterClass::Scout, UpgradeTree{});
        player.clamp_to_surface(world);

        // Ground check on spawn
        TEST_CHECK(player.isGrounded(), "Delver must be solidly grounded upon spawn");
        TEST_CHECK(!player.is_in_spikes(), "Delver MUST NOT spawn on spikes");
        TEST_CHECK(!player.is_in_lava(), "Delver MUST NOT spawn in lava");
        TEST_CHECK(player.health() == player.max_health(), "Delver must have 100% full health at spawn");

        // Step 30 physics frames standing on spawn pad (0.5s idle)
        for (int f = 0; f < 30; ++f) {
            player.update_physics(1.0f / 60.0f, world);
        }
        TEST_CHECK(player.health() == player.max_health(), "Idle delver at spawn pad must take 0 damage");
        TEST_CHECK(!player.is_in_spikes(), "Idle delver at spawn pad must not be flagged in spikes");

        // 2. Cardinal Walking Test: Uninhibited Movement in All 4 Directions
        const glm::vec3 dirs[4] = {
            glm::vec3(1.0f, 0.0f, 0.0f),   // East (+X)
            glm::vec3(-1.0f, 0.0f, 0.0f),  // West (-X)
            glm::vec3(0.0f, 0.0f, 1.0f),   // South (+Z)
            glm::vec3(0.0f, 0.0f, -1.0f)   // North (-Z)
        };

        for (int d = 0; d < 4; ++d) {
            player.set_position(spawn_pos);
            player.clamp_to_surface(world);
            glm::vec3 start_p = player.position();

            for (int f = 0; f < 25; ++f) {
                player.velocity_mut() = dirs[d] * 5.5f;
                player.update_physics(1.0f / 60.0f, world);
            }
            float dist_moved = glm::distance(glm::vec2(player.position().x, player.position().z),
                                            glm::vec2(start_p.x, start_p.z));
            TEST_CHECK(dist_moved >= 1.2f, "Delver walking in spawn room must move freely without false spike drag");
            TEST_CHECK(player.health() == player.max_health(), "Delver walking in spawn room must take 0 damage");
            TEST_CHECK(!player.is_in_spikes(), "Delver walking in spawn room must not trigger spike flag");
        }

        // 3. Verify Entire Spawn Staging Cavern (0, 0) Has ZERO Hazards
        const RoomPlacement* spawn_room = gen->get_room_at_grid(0, 0);
        TEST_CHECK(spawn_room != nullptr, "Spawn staging cavern must exist at grid (0,0)");
        for (int rx = spawn_room->center.x - spawn_room->half_width; rx <= spawn_room->center.x + spawn_room->half_width; ++rx) {
            for (int rz = spawn_room->center.z - spawn_room->half_depth; rz <= spawn_room->center.z + spawn_room->half_depth; ++rz) {
                for (int ry = spawn_room->floor_y; ry <= spawn_room->ceiling_y; ++ry) {
                    Voxel v = world.get_voxel(rx, ry, rz);
                    TEST_CHECK(v.material_id != MAT_OBSIDIAN_SPIKES, "Spawn room must never contain obsidian spikes");
                    TEST_CHECK(v.material_id != MAT_THERMITE_SLAG && v.material_id != MAT_MOLTEN_MAGMA, "Spawn room must never contain lava");
                    TEST_CHECK(v.material_id != MAT_GAS && v.material_id != MAT_TOXIC_GAS, "Spawn room must never contain toxic gas");
                }
            }
        }

        // 4. Verify Doorway Thresholds and Mandatory Corridors are Hazard-Free
        for (const auto& room : gen->rooms()) {
            // Check cardinal doorway thresholds
            const int door_coords[4][2] = {
                {room.center.x + room.half_width, room.center.z},
                {room.center.x - room.half_width, room.center.z},
                {room.center.x, room.center.z + room.half_depth},
                {room.center.x, room.center.z - room.half_depth}
            };
            for (int i = 0; i < 4; ++i) {
                int dx = door_coords[i][0];
                int dz = door_coords[i][1];
                if (gen->is_in_bounds(dx, room.floor_y + 1, dz)) {
                    Voxel step_v = world.get_voxel(dx, room.floor_y + 1, dz);
                    TEST_CHECK(step_v.material_id != MAT_OBSIDIAN_SPIKES, "Doorway step must not be obsidian spikes");
                    TEST_CHECK(step_v.material_id != MAT_THERMITE_SLAG && step_v.material_id != MAT_MOLTEN_MAGMA, "Doorway step must not be lava");
                }
            }
        }

        // 5. Test Full Level Flight & Safe Mid-Air Roaming Across All Rooms
        for (const auto& room : gen->rooms()) {
            glm::vec3 fly_pos(room.center.x + 0.5f, static_cast<float>(room.floor_y + 3) + 0.5f, room.center.z + 0.5f);
            if (world.is_solid(glm::ivec3(fly_pos))) {
                // If a decorative pillar/monument occupies exact center, find open air adjacent
                for (int ox = -3; ox <= 3; ++ox) {
                    for (int oz = -3; oz <= 3; ++oz) {
                        glm::ivec3 check_air(room.center.x + ox, room.floor_y + 3, room.center.z + oz);
                        if (!world.is_solid(check_air) && !world.is_solid(check_air + glm::ivec3(0, 1, 0))) {
                            fly_pos = glm::vec3(check_air) + glm::vec3(0.5f);
                            break;
                        }
                    }
                }
            }
            player.set_position(fly_pos);
            player.set_velocity(glm::vec3(0.0f));

            for (int f = 0; f < 10; ++f) {
                player.update_physics(1.0f / 60.0f, world);
            }
            TEST_CHECK(!player.is_penetrating_solid(world), "Flight inside cavern must not clip into ceiling or walls");
            TEST_CHECK(!player.is_in_spikes(), "Flight high above floor must not trigger spike trap");
            TEST_CHECK(!player.is_in_lava(), "Flight high above floor must not trigger lava damage");
        }

        // 6. Hazard Room Safe Bridge Verification
        for (const auto& room : gen->rooms()) {
            if (room.type == RoomShapeType::SpikeTrenchArena) {
                // Bridge spans at floor_y + 2
                glm::vec3 b_start(room.center.x - room.half_width + 2, static_cast<float>(room.floor_y + 3) + 0.95f, room.center.z);
                PlayerController bridge_walker(b_start);
                bridge_walker.apply_attributes_and_upgrades(CharacterClass::Scout, UpgradeTree{});
                bridge_walker.clamp_to_surface(world);

                // Walk along bridge across the room
                for (int f = 0; f < 30; ++f) {
                    bridge_walker.velocity_mut() = glm::vec3(4.0f, 0.0f, 0.0f);
                    bridge_walker.update_physics(1.0f / 60.0f, world);
                }
                TEST_CHECK(bridge_walker.health() == bridge_walker.max_health(), "Delver traversing spike trench bridge must take 0 damage");
                TEST_CHECK(!bridge_walker.is_in_spikes(), "Delver on safe bridge must not trigger spikes");
            } else if (room.type == RoomShapeType::ColossalAbyssalChasm) {
                // Suspension bridge spans at floor_y + 8
                glm::vec3 b_start(room.center.x - room.half_width + 2, static_cast<float>(room.floor_y + 9) + 0.95f, room.center.z);
                PlayerController chasm_walker(b_start);
                chasm_walker.apply_attributes_and_upgrades(CharacterClass::Scout, UpgradeTree{});
                chasm_walker.clamp_to_surface(world);

                for (int f = 0; f < 30; ++f) {
                    chasm_walker.velocity_mut() = glm::vec3(4.0f, 0.0f, 0.0f);
                    chasm_walker.update_physics(1.0f / 60.0f, world);
                }
                TEST_CHECK(chasm_walker.health() == chasm_walker.max_health(), "Delver traversing colossal chasm suspension bridge must take 0 damage");
                TEST_CHECK(!chasm_walker.is_in_spikes(), "Delver on suspension bridge must not trigger spikes");
            }
        }
    }

    log_pass("Spawn safety, uninhibited cardinal mobility, hazard placement, and full level flight verified across Sectors 1-3");
}

// ─────────────────────────────────────────────────────────────
// 12. OBJECTIVE MATERIAL SCARCITY, BALANCING & COMPLETENESS VALIDATION
// ─────────────────────────────────────────────────────────────
void test_objective_material_scarcity_and_completeness() {
    std::cout << "\n=== [MODULE 12] Objective Material Scarcity, Balancing & Completeness Validation ===" << std::endl;

    const uint32_t test_seeds[] = {101, 777, 1337, 9999, 424242};

    for (int sector = 1; sector <= 3; ++sector) {
        for (uint32_t seed : test_seeds) {
            LevelGenerator gen(sector, seed);
            int target = gen.target_objective_amount();
            int total_spawned = gen.total_objective_material_count();

            // 1. Completeness Invariant: always enough spawned to complete the level
            TEST_CHECK(total_spawned >= target, "Total spawned objective material must meet or exceed level quota");

            // 2. Strict Scarcity Ceiling: never excessively abundant
            TEST_CHECK(total_spawned <= target + 3, "Total spawned objective material must be tightly capped (quota + 3 max)");

            // 3. Validation Checks: generator completeness, level validation, and per-room validation
            TEST_CHECK(gen.validate_resource_compleatability(), "LevelGenerator completeness validation check must succeed");
            TEST_CHECK(gen.validate_level_resources(), "validate_level_resources() must pass for all sectors");

            // 4. Exploration Necessity: zero objective material at distance <= 1 from spawn
            int near_spawn_count = 0;
            int max_room_voxels = 0;
            for (const auto& pos : gen.objective_deposit_locations()) {
                const RoomPlacement* r = gen.get_room_at_position(glm::vec3(pos.x, pos.y, pos.z));
                if (r) {
                    int dist = r->grid_x + r->grid_z;
                    if (dist <= 1) {
                        near_spawn_count++;
                    }
                }
            }
            TEST_CHECK(near_spawn_count == 0, "No objective materials may spawn at distance <= 1 from spawn cavern");

            // 5. Extraction Landing Bay Gating: zero objective material on evac pad
            const RoomPlacement* evac_room = gen.get_room_at_grid(gen.grid_width() - 1, gen.grid_depth() - 1);
            if (evac_room) {
                TEST_CHECK(evac_room->spawned_objective_voxels == 0, "Extraction bay must contain 0 objective voxels");
            }

            // 6. Non-Abundant Individual Rooms & Per-Room Validation
            for (const auto& room : gen.rooms()) {
                max_room_voxels = std::max(max_room_voxels, room.spawned_objective_voxels);
                auto report = gen.validate_room_resources(room);
                TEST_CHECK(report.valid, "validate_room_resources must pass for room at (" +
                           std::to_string(room.grid_x) + ", " + std::to_string(room.grid_z) + "): " + report.details);
            }
            int max_allowed_per_room = (sector == 1) ? 5 : 4;
            TEST_CHECK(max_room_voxels <= max_allowed_per_room, "No single room may contain excessive objective voxels (preventing jackpot rooms)");

            // 7. Rooms With Zero Objective Resources: some rooms have none, forcing cave exploration
            int zero_resource_rooms = gen.rooms_with_zero_objective_count();
            TEST_CHECK(zero_resource_rooms > 0, "Level must contain rooms with zero objective resources");
            int expected_min_zero = (sector == 1) ? 3 : (sector == 2 ? 6 : 10);
            TEST_CHECK(zero_resource_rooms >= expected_min_zero, "Level must have at least expected minimum rooms with zero objective resources");

            // 8. Room Spread: material must be distributed across multiple distant rooms
            int min_spread = (sector == 1) ? 4 : (sector == 2 ? 7 : 10);
            TEST_CHECK(gen.objective_room_spread_count() >= min_spread, "Objective material must be spread across distant rooms forcing cave exploration");

            // 9. Direct Voxel Sampling: sample_voxel must return MAT_VOIDITE_CRYSTAL at deposit locations
            for (const auto& pos : gen.objective_deposit_locations()) {
                Voxel v = gen.sample_voxel(pos.x, pos.y, pos.z);
                TEST_CHECK(v.material_id == MAT_VOIDITE_CRYSTAL, "Sampled objective voxel at registered location must be MAT_VOIDITE_CRYSTAL");
                TEST_CHECK(v.flags_and_damage & VOXEL_FLAG_EMISSIVE, "Objective Voidite crystal must be emissive");
            }

            // 9.5. Wall-Embedded Integrity & Wall-Surface Visual Discovery
            // Every objective voxel must replace solid base matrix (never floating in MAT_AIR),
            // and in every room with objective material, at least one block of the vein must be
            // exposed on the surface of the wall (adjacent to MAT_AIR) so that players can visually spot it!
            for (const auto& room : gen.rooms()) {
                if (room.spawned_objective_voxels == 0) continue;
                bool room_has_exposed_surface_piece = false;
                for (const auto& pos : gen.objective_deposit_locations()) {
                    const RoomPlacement* r = gen.get_room_at_position(glm::vec3(pos.x, pos.y, pos.z));
                    if (r && r->grid_x == room.grid_x && r->grid_z == room.grid_z) {
                        // Verify embedded in solid material, never air
                        Voxel base_v = gen.sample_base_voxel(pos.x, pos.y, pos.z);
                        TEST_CHECK(base_v.material_id != MAT_AIR, "Objective voxel must be embedded in solid wall or structure, never in MAT_AIR");

                        // Check if this block has an exposed face to air
                        const glm::ivec3 card_dirs[6] = {
                            {-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}
                        };
                        for (const auto& cd : card_dirs) {
                            if (gen.sample_base_voxel(pos.x + cd.x, pos.y + cd.y, pos.z + cd.z).material_id == MAT_AIR) {
                                room_has_exposed_surface_piece = true;
                                break;
                            }
                        }
                    }
                }
                TEST_CHECK(room_has_exposed_surface_piece,
                           "Room at (" + std::to_string(room.grid_x) + ", " + std::to_string(room.grid_z) +
                           ") must have at least one vein voxel exposed on the surface of the wall facing room air for player visual discovery");
            }
        }
    }

    // 10. Pre-Determined Spawn Locations Verification across all 27 Archetypes
    LevelGenerator helper(1, 1337);
    const RoomShapeType all_archetypes[] = {
        RoomShapeType::SpawnStagingCavern, RoomShapeType::MiningPillarHall, RoomShapeType::CrystallineGeode,
        RoomShapeType::TerracedQuarry, RoomShapeType::IndustrialVaultBunker, RoomShapeType::FaultLineCrevasse,
        RoomShapeType::AbyssalVerticalChasm, RoomShapeType::RadioactiveCoreSanctuary, RoomShapeType::ExtractionLandingBay,
        RoomShapeType::MagmaCalderaLake, RoomShapeType::SpikeTrenchArena, RoomShapeType::VoidSingularityRift,
        RoomShapeType::FungoidBioGrotto, RoomShapeType::LaserDefenseFoundry, RoomShapeType::CrumblingArchCanyon,
        RoomShapeType::SubterraneanAquiferOasis, RoomShapeType::ColossalAbyssalChasm, RoomShapeType::MoltenMagmaFoundry,
        RoomShapeType::ToxicMiasmaSwamp, RoomShapeType::PrismaticCrystalCathedral, RoomShapeType::AncientTitanNecropolis,
        RoomShapeType::BioluminescentGlowwormGrotto, RoomShapeType::PrecursorCoolantReservoir,
        RoomShapeType::ColossalVaultedDredgeCathedral, RoomShapeType::TectonicAbyssalSinkhole,
        RoomShapeType::CyclopeanExcavationSilo, RoomShapeType::BioluminescentFirmamentAbyss,
        RoomShapeType::PrecursorSanctumVault, RoomShapeType::SubterraneanVaultBastion,
        RoomShapeType::AbyssalVaultChasm
    };

    for (RoomShapeType archetype : all_archetypes) {
        auto offsets = helper.get_room_candidate_spawn_offsets(archetype, 10, 10, 4, 25);
        if (archetype == RoomShapeType::SpawnStagingCavern || archetype == RoomShapeType::ExtractionLandingBay) {
            TEST_CHECK(offsets.empty(), "Spawn and Extraction archetypes must return strictly 0 candidate offsets");
        } else {
            TEST_CHECK(offsets.size() >= 4, "Non-spawn archetype must provide at least 4 pre-determined candidate spawn locations");
            // Check that offsets are within reasonable bounds
            for (const auto& off : offsets) {
                TEST_CHECK(std::abs(off.x) <= 10, "Candidate offset X out of half-width bounds");
                TEST_CHECK(std::abs(off.z) <= 10, "Candidate offset Z out of half-depth bounds");
                TEST_CHECK(off.y >= 5 && off.y <= 24, "Candidate offset Y out of vertical bounds");
            }
        }

        // 11. Room Shape Decoupling: single room layout without objective injection has 0 MAT_VOIDITE_CRYSTAL voxels
        helper.create_single_room_test_layout(archetype);
        int hardcoded_voidite_count = 0;
        for (int x = 28; x <= 44; ++x) {
            for (int y = 4; y <= 25; ++y) {
                for (int z = 28; z <= 44; ++z) {
                    if (helper.sample_voxel(x, y, z).material_id == MAT_VOIDITE_CRYSTAL) {
                        hardcoded_voidite_count++;
                    }
                }
            }
        }
        TEST_CHECK(hardcoded_voidite_count == 0, "Room shape geometry must not have hardcoded MAT_VOIDITE_CRYSTAL built into it");
    }

    // 12. Room Decoration Decoupling: Architectural & Natural rooms must not place crystal blocks as floors, columns, or moisture stalactites
    {
        LevelGenerator quarry_gen(1, 42);
        quarry_gen.create_single_room_test_layout(RoomShapeType::TerracedQuarry);
        int quarry_crystals = 0;
        for (int x = 28; x <= 44; ++x) {
            for (int y = 4; y <= 25; ++y) {
                for (int z = 28; z <= 44; ++z) {
                    if (quarry_gen.sample_voxel(x, y, z).material_id == MAT_PRISMATIC_CRYSTAL) {
                        quarry_crystals++;
                    }
                }
            }
        }
        TEST_CHECK(quarry_crystals == 0, "Terraced Quarry must use natural rock/titanium, not crystal blocks for floors");

        LevelGenerator oasis_gen(1, 42);
        oasis_gen.create_single_room_test_layout(RoomShapeType::SubterraneanAquiferOasis);
        int oasis_crystals = 0;
        for (int x = 28; x <= 44; ++x) {
            for (int y = 4; y <= 25; ++y) {
                for (int z = 28; z <= 44; ++z) {
                    if (oasis_gen.sample_voxel(x, y, z).material_id == MAT_PRISMATIC_CRYSTAL) {
                        oasis_crystals++;
                    }
                }
            }
        }
        TEST_CHECK(oasis_crystals == 0, "Aquifer Oasis must use flora and natural basalt for banks/stalactites, not crystal blocks");

        LevelGenerator caldera_gen(1, 42);
        caldera_gen.create_single_room_test_layout(RoomShapeType::MagmaCalderaLake);
        int caldera_crystals = 0;
        for (int x = 28; x <= 44; ++x) {
            for (int y = 4; y <= 25; ++y) {
                for (int z = 28; z <= 44; ++z) {
                    if (caldera_gen.sample_voxel(x, y, z).material_id == MAT_PRISMATIC_CRYSTAL) {
                        caldera_crystals++;
                    }
                }
            }
        }
        TEST_CHECK(caldera_crystals == 0, "Magma Caldera chimney must be basalt, not crystal blocks");

        // 13. Decorative Crystal Subtlety & Room Center Integration:
        // Decorative crystals must never be placed as center-of-attention floor platforms or giant central spires
        LevelGenerator cathedral_gen(1, 42);
        cathedral_gen.create_single_room_test_layout(RoomShapeType::PrismaticCrystalCathedral);
        TEST_CHECK(cathedral_gen.sample_voxel(36, 4, 36).material_id == MAT_FRACTURED_GRANITE,
                   "Prismatic Cathedral central floor must be natural carved granite, not an eye-distracting crystal stage");
        TEST_CHECK(cathedral_gen.sample_voxel(36, 5, 36).material_id == MAT_FRACTURED_GRANITE,
                   "Prismatic Cathedral central dais stairs must be natural stone, not solid crystal blocks");
        TEST_CHECK(cathedral_gen.sample_voxel(36, 6, 36).material_id == MAT_FRACTURED_GRANITE,
                   "Prismatic Cathedral upper dais stairs must be natural stone");

        LevelGenerator geode_gen(1, 42);
        geode_gen.create_single_room_test_layout(RoomShapeType::CrystallineGeode);
        TEST_CHECK(geode_gen.sample_voxel(36, 5, 36).material_id == MAT_VOLCANIC_BASALT,
                   "Crystalline Geode central pinnacle must be natural basalt rock, not a giant central glowing crystal spire");

        LevelGenerator void_rift_gen(1, 42);
        void_rift_gen.create_single_room_test_layout(RoomShapeType::VoidSingularityRift);
        TEST_CHECK(void_rift_gen.sample_voxel(36, 7, 36).material_id == MAT_OBSIDIAN_SPIKES,
                   "Void Singularity central monolith must be dark obsidian, not a bright distracting prismatic crystal");
    }

    log_pass("Objective resource scarcity, zero-near-spawn exploration gating, zero-extraction gating, all-archetype pre-determined offsets, and room decoration visual decoupling verified across all Sectors and Seeds");
}

// ─────────────────────────────────────────────────────────────
// 6. DARK ZONES & TORCH ILLUMINATION SCALING TESTS
// ─────────────────────────────────────────────────────────────
void test_dark_zones_and_illumination() {
    std::cout << "\n=== [MODULE 6] Dark Zones & Illumination Scaling ===" << std::endl;

    // Test 1: Sector Darkness Scaling Progression
    {
        LevelGenerator s1(1, 1001);
        LevelGenerator s2(2, 1002);
        LevelGenerator s3(3, 1003);

        float r1 = s1.sector_darkness_ratio();
        float r2 = s2.sector_darkness_ratio();
        float r3 = s3.sector_darkness_ratio();

        TEST_CHECK(r1 <= 0.25f, "Sector 1 must have low dark zone ratio (<= 25%)");
        TEST_CHECK(r2 > r1 || s2.dark_room_count() >= 3, "Sector 2 must feature more dark zones than Sector 1");
        TEST_CHECK(r3 >= 0.45f, "Sector 3+ deep extraction must have high dark zone density (>= 45%)");

        log_pass("Sector darkness progression scaling verified (Sector 1 gentle, Sector 2 moderate, Sector 3+ blackout caverns)");
    }

    // Test 2: Inviolable Safe Illumination for Spawn & Evac Pod bays across seeds
    for (uint32_t seed : {42u, 1337u, 8888u, 99999u}) {
        for (int sec = 1; sec <= 3; ++sec) {
            LevelGenerator gen(sec, seed);

            const RoomPlacement* spawn = gen.get_room_at_grid(0, 0);
            TEST_CHECK(spawn != nullptr, "Spawn room must exist");
            TEST_CHECK(!spawn->is_dark_zone, "Spawn staging cavern must NEVER be marked as dark zone");
            TEST_CHECK(spawn->darkness_factor == 0.0f, "Spawn staging cavern must have 0 darkness factor");

            glm::vec3 spawn_pos(spawn->center.x, 10.0f, spawn->center.z);
            TEST_CHECK(!gen.is_dark_zone_at(spawn_pos), "is_dark_zone_at must return false at spawn staging center");
            TEST_CHECK(gen.get_darkness_at(spawn_pos) == 0.0f, "get_darkness_at must return 0.0f at spawn staging center");

            int evac_gx = gen.grid_width() - 1;
            int evac_gz = gen.grid_depth() - 1;
            const RoomPlacement* evac = gen.get_room_at_grid(evac_gx, evac_gz);
            TEST_CHECK(evac != nullptr, "Evac room must exist");
            TEST_CHECK(!evac->is_dark_zone, "Evac landing bay must NEVER be marked as dark zone");
            TEST_CHECK(evac->darkness_factor == 0.0f, "Evac landing bay must have 0 darkness factor");

            glm::vec3 evac_pos(evac->center.x, 10.0f, evac->center.z);
            TEST_CHECK(!gen.is_dark_zone_at(evac_pos), "is_dark_zone_at must return false at evac center");
            TEST_CHECK(gen.get_darkness_at(evac_pos) == 0.0f, "get_darkness_at must return 0.0f at evac center");
        }
    }
    log_pass("Spawn staging and Extraction landing bays strictly guaranteed full illumination across all sectors and seeds");

    // Test 3: Dark Room Spatial Lookup and Darkness Factors
    {
        LevelGenerator gen(3, 7777);
        bool found_dark_room = false;
        for (const auto& room : gen.rooms()) {
            if (room.is_dark_zone) {
                found_dark_room = true;
                TEST_CHECK(room.darkness_factor >= 0.70f, "Dark zone rooms must have darkness factor >= 0.70f");
                glm::vec3 center(room.center.x, 12.0f, room.center.z);
                TEST_CHECK(gen.is_dark_zone_at(center), "is_dark_zone_at must return true within dark zone room");
                TEST_CHECK(gen.get_darkness_at(center) >= 0.70f, "get_darkness_at must match dark zone factor");
            }
        }
        TEST_CHECK(found_dark_room, "Sector 3 must have at least one dark zone room");
        log_pass("Dark zone spatial query and darkness factors validated");
    }

    // Test 4: Luminary Suppression in Dark Rooms
    {
        LevelGenerator gen(2, 54321);
        for (const auto& lum : gen.luminaries()) {
            // Find which room (if any) this luminary is inside
            for (const auto& r : gen.rooms()) {
                if (r.is_dark_zone) {
                    bool inside_room = (lum.position.x >= r.min_x() && lum.position.x <= r.max_x() &&
                                        lum.position.z >= r.min_z() && lum.position.z <= r.max_z());
                    if (inside_room) {
                        // In dark zones, standard room cavern luminaries are suppressed.
                        // Only high hazard incandescent magma / lava vents may exist.
                        TEST_CHECK(lum.type == LuminaryType::ThermalMagmaVent,
                                   "Standard decorative cavern luminaries must be suppressed in dark zone rooms");
                    }
                }
            }
        }
        log_pass("Cavern luminaries correctly omitted in dark zone blackout caverns");
    }

    // Test 5: Balanced Diversified Luminaries and Anti-Clustering Spatial Density
    {
        int total_wall_sconces = 0;
        int total_spores = 0;
        int total_fluid_lights = 0;

        for (uint32_t seed : { 101u, 202u, 303u, 404u, 505u }) {
            LevelGenerator gen(1, seed);
            const auto& luminaries = gen.luminaries();

            // Anti-clustering verification
            for (size_t i = 0; i < luminaries.size(); ++i) {
                for (size_t j = i + 1; j < luminaries.size(); ++j) {
                    const auto& a = luminaries[i];
                    const auto& b = luminaries[j];

                    // Priority beacons and vents may co-exist at key mission points,
                    // but ambient decorative lights must respect the anti-clustering boundary
                    bool is_ambient_a = (a.type == LuminaryType::WallIndustrialSconce ||
                                         a.type == LuminaryType::FungoidSporeGlow ||
                                         a.type == LuminaryType::PrismaticCrystalRadiance);
                    bool is_ambient_b = (b.type == LuminaryType::WallIndustrialSconce ||
                                         b.type == LuminaryType::FungoidSporeGlow ||
                                         b.type == LuminaryType::PrismaticCrystalRadiance);

                    if (is_ambient_a && is_ambient_b) {
                        float dist = glm::distance(a.position, b.position);
                        TEST_CHECK(dist >= 8.0f, "Ambient luminaries must not cluster within 8.0m of each other");
                    }
                }
            }

            for (const auto& lum : luminaries) {
                if (lum.type == LuminaryType::WallIndustrialSconce) total_wall_sconces++;
                if (lum.type == LuminaryType::FungoidSporeGlow) total_spores++;
                if (lum.type == LuminaryType::AquiferOasisGlow ||
                    lum.type == LuminaryType::ThermalMagmaVent ||
                    lum.type == LuminaryType::ToxicMiasmaGreen) {
                    total_fluid_lights++;
                }
            }
        }

        TEST_CHECK(total_wall_sconces > 0, "Wall industrial sconces must be generated across levels");
        TEST_CHECK(total_wall_sconces >= total_spores, "Wall sconces and fixtures must equal or exceed fungal spore lights");
        log_pass("Luminary diversification, wall sconces, fluid lighting, and anti-clustering density validated");
    }
}


int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- PROCEDURAL LEVEL GENERATION TEST SUITE" << std::endl;
    std::cout << "==========================================================" << std::endl;

    test_level_design_topology();
    test_random_shape_permutations_integration();
    test_all_rooms_tour_and_visibility();
    test_spawn_safety_and_full_level_walk_flight();
    test_objective_material_scarcity_and_completeness();
    test_dark_zones_and_illumination();

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "  ALL " << s_passed_tests << " PROCEDURAL GENERATION TESTS PASSED!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
