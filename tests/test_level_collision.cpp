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
void test_wall_collision_and_anti_clipping() {
    std::cout << "\n=== [MODULE 2] Wall Collision & Anti-Clipping Stress Test ===" << std::endl;

    World world(7777, false);
    world.generate_world(2, 7777); // Sector 2 has vault bunkers, granites, basalts

    const auto& rooms = world.level_generator()->rooms();
    for (size_t r_idx = 0; r_idx < rooms.size(); ++r_idx) {
        const auto& room = rooms[r_idx];

        glm::vec3 sprint_dirs[4] = {
            glm::vec3(15.0f, 0.0f, 0.0f),   // Sprint East
            glm::vec3(-15.0f, 0.0f, 0.0f),  // Sprint West
            glm::vec3(0.0f, 0.0f, 15.0f),   // Sprint North
            glm::vec3(0.0f, 0.0f, -15.0f)   // Sprint South
        };

        for (int d = 0; d < 4; ++d) {
            PlayerController player(glm::vec3(room.center.x, 20.0f, room.center.z));
            player.clamp_to_surface(world);
            TEST_CHECK(!player.is_penetrating_solid(world), "Player initial clamped spawn must not penetrate");

            // Sprint directly for 60 physics frames (1 second at 15 m/s)
            for (int f = 0; f < 60; ++f) {
                if (sprint_dirs[d].x != 0.0f) player.velocity_mut().x = sprint_dirs[d].x;
                if (sprint_dirs[d].z != 0.0f) player.velocity_mut().z = sprint_dirs[d].z;
                player.update_physics(1.0f / 60.0f, world);

                // ZERO-TOLERANCE ASSERTION: Player must NEVER be inside a solid block
                bool penetrating = player.is_penetrating_solid(world);
                if (penetrating) {
                    std::cerr << "Penetration detected in room " << r_idx << " (type " << static_cast<int>(room.type)
                              << ") at frame " << f << " pos: " << player.position().x << ", " << player.position().y << ", " << player.position().z << std::endl;
                }
                TEST_CHECK(!penetrating, "Player clipped into solid block during high-speed sprint!");
            }
        }
    }

    // Outer Sector Perimeter Wall Stress Test: drive player at 20 m/s into outer world boundaries
    struct PerimeterTest {
        glm::vec3 start;
        glm::vec3 vel;
        float min_limit;
        float max_limit;
        bool is_x;
    };

    float max_x_bound = static_cast<float>(world.level_generator()->world_width()) - 3.5f;
    float max_z_bound = static_cast<float>(world.level_generator()->world_depth()) - 3.5f;
    int gw = world.level_generator()->grid_width();
    int gd = world.level_generator()->grid_depth();

    const RoomPlacement* r_west  = world.level_generator()->get_room_at_grid(0, 0);
    const RoomPlacement* r_east  = world.level_generator()->get_room_at_grid(gw - 1, 0);
    const RoomPlacement* r_south = world.level_generator()->get_room_at_grid(0, 0);
    const RoomPlacement* r_north = world.level_generator()->get_room_at_grid(0, gd - 1);

    std::vector<PerimeterTest> perim_tests = {
        { glm::vec3(r_west->center.x - r_west->half_width + 2.0f, 20.0f, r_west->center.z),  glm::vec3(-20.0f, 0.0f, 0.0f), 3.5f, max_x_bound, true },  // West perimeter
        { glm::vec3(r_east->center.x + r_east->half_width - 2.0f, 20.0f, r_east->center.z), glm::vec3(20.0f, 0.0f, 0.0f),  3.5f, max_x_bound, true },  // East perimeter
        { glm::vec3(r_south->center.x, 20.0f, r_south->center.z - r_south->half_depth + 2.0f),  glm::vec3(0.0f, 0.0f, -20.0f), 3.5f, max_z_bound, false }, // South perimeter
        { glm::vec3(r_north->center.x, 20.0f, r_north->center.z + r_north->half_depth - 2.0f), glm::vec3(0.0f, 0.0f, 20.0f),  3.5f, max_z_bound, false }  // North perimeter
    };

    for (const auto& pt : perim_tests) {
        PlayerController p(pt.start);
        p.clamp_to_surface(world);

        for (int f = 0; f < 60; ++f) {
            p.set_velocity(pt.vel);
            p.update_physics(1.0f / 60.0f, world);
            bool pen = p.is_penetrating_solid(world);
            if (pen) {
                std::cerr << "Perim test fail: start=(" << pt.start.x << "," << pt.start.y << "," << pt.start.z
                          << ") pos=(" << p.position().x << "," << p.position().y << "," << p.position().z << ") frame=" << f << std::endl;
            }
            TEST_CHECK(!pen, "Player clipped into outer perimeter bedrock wall!");
        }

        if (pt.is_x) {
            TEST_CHECK(p.position().x >= pt.min_limit && p.position().x <= pt.max_limit, "Player penetrated outer X perimeter!");
        } else {
            TEST_CHECK(p.position().z >= pt.min_limit && p.position().z <= pt.max_limit, "Player penetrated outer Z perimeter!");
        }
    }

    log_pass("High-speed wall sprinting across all 9 rooms: 0 clipping, 0 tunneling");
}

// ─────────────────────────────────────────────────────────────
// 3. CEILING FLYING, JETPACK THRUSTERS & GRAPPLE STRESS TEST
// ─────────────────────────────────────────────────────────────
void test_ceiling_flying_and_anti_clipping() {
    std::cout << "\n=== [MODULE 3] Ceiling Collision, Thrusters & Grapple Reel ===" << std::endl;

    for (int sector = 1; sector <= 3; ++sector) {
        World world(8888 + sector, false);
        world.generate_world(sector, 8888 + sector);

        const auto& rooms = world.level_generator()->rooms();
        for (const auto& room : rooms) {
            glm::vec3 spawn_pos(room.center.x, 22.0f, room.center.z);
            PlayerController player(spawn_pos);
            player.clamp_to_surface(world);

            // 1. Vertical Jetpack Thruster Burn straight into ceiling
            for (int f = 0; f < 100; ++f) {
                player.velocity_mut().y = 12.0f; // Max upward hover velocity
                player.update_physics(1.0f / 60.0f, world);

                // Assert zero clipping inside ceiling solid voxels
                bool penetrating = player.is_penetrating_solid(world);
                if (penetrating) {
                    std::cerr << "Ceiling penetration: sector " << sector << ", room type " << static_cast<int>(room.type)
                              << ", frame " << f << " pos: " << player.position().x << ", " << player.position().y << ", " << player.position().z << std::endl;
                    // Check voxels overlapping box
                    glm::vec3 box_min = player.position() - glm::vec3(0.3f, 0.9f, 0.3f);
                    glm::vec3 box_max = player.position() + glm::vec3(0.3f, 0.9f, 0.3f);
                    for (int cy = (int)floor(box_min.y); cy <= (int)floor(box_max.y); ++cy) {
                        for (int cx = (int)floor(box_min.x); cx <= (int)floor(box_max.x); ++cx) {
                            for (int cz = (int)floor(box_min.z); cz <= (int)floor(box_max.z); ++cz) {
                                if (world.is_solid(glm::ivec3(cx, cy, cz))) {
                                    std::cerr << "  Solid block at " << cx << ", " << cy << ", " << cz << std::endl;
                                }
                            }
                        }
                    }
                }
                TEST_CHECK(!penetrating, "Player clipped into ceiling solid block during thruster ascent!");

                // Assert player never pops on top of the world mantle
                TEST_CHECK(player.position().y < 26.0f, "Player popped above ceiling mantle onto roof!");
            }

            // 2. Upward Grapple Reel into ceiling
            // Find lowest solid ceiling voxel above player
            int ceiling_hit_y = -1;
            for (int y = static_cast<int>(player.position().y); y <= 27; ++y) {
                if (world.is_solid(glm::ivec3(room.center.x, y, room.center.z))) {
                    ceiling_hit_y = y;
                    break;
                }
            }

            if (ceiling_hit_y > 0) {
                PlayerController grapple_player(player.position());
                grapple_player.grapple_mut().active = true;
                grapple_player.grapple_mut().anchor_point = glm::vec3(room.center.x, static_cast<float>(ceiling_hit_y), room.center.z);

                // Reel up rapidly with extreme tension (up to 30 m/s pull)
                for (int f = 0; f < 60; ++f) {
                    grapple_player.velocity_mut().y = 25.0f;
                    grapple_player.update_physics(1.0f / 60.0f, world);
                    TEST_CHECK(!grapple_player.is_penetrating_solid(world), "Grapple reeled player through ceiling block!");
                    TEST_CHECK(grapple_player.position().y < 26.0f, "Grapple launched player over ceiling mantle!");
                }
            }
        }
    }

    log_pass("Vertical thruster burns and ceiling grapple pulls: 0 ceiling clipping, 0 roof popping");
}

// ─────────────────────────────────────────────────────────────
// 4. BLOCK BREAKING, HOLE TRAVERSAL & SOLIDITY TESTS
// ─────────────────────────────────────────────────────────────
void test_block_break_and_walkthrough() {
    std::cout << "\n=== [MODULE 4] Block Breaking & Broken Block Traversal ===" << std::endl;

    World world(5555, false);
    // Clear an open room in air from x=10..30, y=5..15, z=10..30
    for (int x = 10; x <= 30; ++x) {
        for (int y = 5; y <= 15; ++y) {
            for (int z = 10; z <= 30; ++z) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
            }
        }
    }

    // Build a solid floor at y = 5
    for (int x = 10; x <= 30; ++x) {
        for (int z = 10; z <= 30; ++z) {
            world.set_voxel(x, 5, z, Voxel{MAT_VOLCANIC_BASALT, 0});
        }
    }

    // Build a solid dividing wall at z = 20 from x = 10..30, y = 6..12
    for (int x = 10; x <= 30; ++x) {
        for (int y = 6; y <= 12; ++y) {
            world.set_voxel(x, y, 20, Voxel{MAT_FRACTURED_GRANITE, 0});
        }
    }

    // 1. Player standing at (20.5, 6.9, 18), moving North (+Z) into solid wall at z = 20
    PlayerController player(glm::vec3(20.5f, 6.9f, 18.0f));
    player.set_velocity(glm::vec3(0.0f, 0.0f, 10.0f));

    for (int f = 0; f < 30; ++f) {
        player.velocity_mut().z = 10.0f;
        player.update_physics(1.0f / 60.0f, world);
        TEST_CHECK(!player.is_penetrating_solid(world), "Player clipped into solid test wall!");
    }

    // Must be blocked at z < 20.0 - 0.3 = 19.7
    TEST_CHECK(player.position().z <= 19.71f, "Player walked through solid dividing wall!");

    // 2. Break a 2-block-high doorway at (20, 6, 20) [feet] and (20, 7, 20) [head]
    // Set active tool to MiningDrill, face directly at (20, 7, 20)
    player.set_active_tool(ToolSlot::MiningDrill);
    player.set_position(glm::vec3(20.5f, 6.9f, 19.0f)); // 1 meter away from wall
    player.set_look_angles(-90.0f, 0.0f); // Looking forward (+Z)

    // Simulate mining drill operation
    world.set_voxel(20, 6, 20, Voxel{MAT_AIR, 0}, true);
    world.set_voxel(20, 7, 20, Voxel{MAT_AIR, 0}, true);

    TEST_CHECK(!world.is_solid(glm::ivec3(20, 6, 20)), "Broken foot block must be non-solid AIR");
    TEST_CHECK(!world.is_solid(glm::ivec3(20, 7, 20)), "Broken head block must be non-solid AIR");

    // Surrounding wall blocks must remain solid!
    TEST_CHECK(world.is_solid(glm::ivec3(19, 6, 20)), "Neighbor wall at x=19 must remain solid");
    TEST_CHECK(world.is_solid(glm::ivec3(21, 6, 20)), "Neighbor wall at x=21 must remain solid");
    TEST_CHECK(world.is_solid(glm::ivec3(20, 8, 20)), "Lintel block above doorway at y=8 must remain solid");

    // 3. Now walk forward through the carved doorway centered at x = 20.5
    player.set_velocity(glm::vec3(0.0f, 0.0f, 8.0f));
    for (int f = 0; f < 30; ++f) {
        player.velocity_mut().z = 8.0f;
        player.update_physics(1.0f / 60.0f, world);
        TEST_CHECK(!player.is_penetrating_solid(world), "Player clipped while walking through carved doorway!");
    }

    // Player must now be on the other side of the wall! (z > 20.5)
    TEST_CHECK(player.position().z > 20.5f, "Player failed to walk through carved doorway!");

    // 4. Stand inside the doorway at (20.5, 6.9, 20.0) and try to walk sideways (+X) into the jamb at x=21
    player.set_position(glm::vec3(20.5f, 6.9f, 20.0f));
    player.set_velocity(glm::vec3(8.0f, 0.0f, 0.0f));
    for (int f = 0; f < 30; ++f) {
        player.velocity_mut().x = 8.0f;
        player.update_physics(1.0f / 60.0f, world);
        TEST_CHECK(!player.is_penetrating_solid(world), "Player clipped into doorway side jamb!");
    }
    TEST_CHECK(player.position().x <= 20.71f, "Player walked into solid doorway side jamb!");

    log_pass("Block breaking mechanics: broken voxels become traversable air, neighbor blocks remain solid");
}

// ─────────────────────────────────────────────────────────────
// 5. DIAGONAL CORNER & STATIC DEPENETRATION TESTS
// ─────────────────────────────────────────────────────────────
void test_diagonal_corner_and_depenetration() {
    std::cout << "\n=== [MODULE 5] Diagonal Corner Sliding & Static Depenetration ===" << std::endl;

    World world(6666, false);
    // Create an inner 90-degree corner at (15, y, 15) with walls at x >= 15 and z >= 15
    for (int x = 10; x <= 20; ++x) {
        for (int y = 5; y <= 12; ++y) {
            for (int z = 10; z <= 20; ++z) {
                if (y == 5) {
                    world.set_voxel(x, y, z, Voxel{MAT_VOLCANIC_BASALT, 0});
                } else if (x >= 15 || z >= 15) {
                    world.set_voxel(x, y, z, Voxel{MAT_VOLCANIC_BASALT, 0});
                } else {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                }
            }
        }
    }

    // Sprint diagonally into the corner (vx = 12, vz = 12)
    PlayerController player(glm::vec3(12.0f, 6.9f, 12.0f));
    player.set_velocity(glm::vec3(12.0f, 0.0f, 12.0f));

    for (int f = 0; f < 60; ++f) {
        player.velocity_mut().x = 12.0f;
        player.velocity_mut().z = 12.0f;
        player.update_physics(1.0f / 60.0f, world);
        TEST_CHECK(!player.is_penetrating_solid(world), "Player clipped into diagonal corner intersection!");
    }

    TEST_CHECK(player.position().x <= 14.71f, "Player penetrated corner X boundary");
    TEST_CHECK(player.position().z <= 14.71f, "Player penetrated corner Z boundary");

    // Static depenetration recovery: forcibly embed player 0.25m into the wall
    player.set_position(glm::vec3(14.95f, 6.9f, 12.0f));
    TEST_CHECK(player.is_penetrating_solid(world), "Forced embed must initially register penetration");

    // Run 1 physics tick (or depenetrate pass)
    player.update_physics(1.0f / 60.0f, world);
    TEST_CHECK(!player.is_penetrating_solid(world), "Depenetration must safely eject player to free air");
    TEST_CHECK(player.position().x <= 14.71f, "Depenetration pushed player in wrong direction!");

    log_pass("Corner sliding and static depenetration recovery verified with zero clipping");
}

// ─────────────────────────────────────────────────────────────
// 6. ISOLATED BASE SHAPES STRESS TESTING (ALL 9 ARCHETYPES)
// ─────────────────────────────────────────────────────────────
void test_isolated_base_shapes_stress() {
    std::cout << "\n=== [MODULE 6] Isolated Base Shapes Stress & Boundary Testing ===" << std::endl;

    const std::vector<std::pair<RoomShapeType, std::string>> all_shapes = {
        {RoomShapeType::SpawnStagingCavern,       "SpawnStagingCavern"},
        {RoomShapeType::MiningPillarHall,         "MiningPillarHall"},
        {RoomShapeType::CrystallineGeode,         "CrystallineGeode"},
        {RoomShapeType::TerracedQuarry,           "TerracedQuarry"},
        {RoomShapeType::IndustrialVaultBunker,    "IndustrialVaultBunker"},
        {RoomShapeType::FaultLineCrevasse,        "FaultLineCrevasse"},
        {RoomShapeType::AbyssalVerticalChasm,     "AbyssalVerticalChasm"},
        {RoomShapeType::RadioactiveCoreSanctuary, "RadioactiveCoreSanctuary"},
        {RoomShapeType::ExtractionLandingBay,      "ExtractionLandingBay"},
        {RoomShapeType::MagmaCalderaLake,         "MagmaCalderaLake"},
        {RoomShapeType::SpikeTrenchArena,         "SpikeTrenchArena"},
        {RoomShapeType::VoidSingularityRift,      "VoidSingularityRift"},
        {RoomShapeType::FungoidBioGrotto,         "FungoidBioGrotto"},
        {RoomShapeType::LaserDefenseFoundry,      "LaserDefenseFoundry"},
        {RoomShapeType::CrumblingArchCanyon,      "CrumblingArchCanyon"},
        {RoomShapeType::SubterraneanAquiferOasis,     "SubterraneanAquiferOasis"},
        {RoomShapeType::ColossalAbyssalChasm,         "ColossalAbyssalChasm"},
        {RoomShapeType::MoltenMagmaFoundry,           "MoltenMagmaFoundry"},
        {RoomShapeType::ToxicMiasmaSwamp,             "ToxicMiasmaSwamp"},
        {RoomShapeType::PrismaticCrystalCathedral,    "PrismaticCrystalCathedral"},
        {RoomShapeType::AncientTitanNecropolis,       "AncientTitanNecropolis"},
        {RoomShapeType::BioluminescentGlowwormGrotto, "BioluminescentGlowwormGrotto"},
        {RoomShapeType::PrecursorCoolantReservoir,    "PrecursorCoolantReservoir"},
        {RoomShapeType::ColossalVaultedDredgeCathedral, "ColossalVaultedDredgeCathedral"},
        {RoomShapeType::TectonicAbyssalSinkhole,        "TectonicAbyssalSinkhole"},
        {RoomShapeType::CyclopeanExcavationSilo,        "CyclopeanExcavationSilo"},
        {RoomShapeType::BioluminescentFirmamentAbyss,   "BioluminescentFirmamentAbyss"},
        {RoomShapeType::PrecursorSanctumVault,          "PrecursorSanctumVault"},
        {RoomShapeType::SubterraneanVaultBastion,       "SubterraneanVaultBastion"},
        {RoomShapeType::AbyssalVaultChasm,               "AbyssalVaultChasm"}
    };

    for (const auto& [shape_type, shape_name] : all_shapes) {
        World world(9000, false);
        auto gen = std::make_unique<LevelGenerator>(2, 9000);
        gen->create_single_room_test_layout(shape_type);
        world.set_level_generator(std::move(gen));

        // 1. Structural Invariants Check
        // Bedrock floor integrity (VoidSingularityRift has open void rift in center)
        if (shape_type != RoomShapeType::VoidSingularityRift) {
            for (int x = 28; x <= 44; x += 2) {
                for (int z = 28; z <= 44; z += 2) {
                    TEST_CHECK(world.is_solid(x, 2, z), "Shape " + shape_name + " failed bedrock solid check at y=2");
                    TEST_CHECK(world.get_voxel(x, 2, z).material_id == MAT_DREDGE_BEDROCK,
                               "Shape " + shape_name + " must have MAT_DREDGE_BEDROCK at y=2");
                    TEST_CHECK(world.is_solid(x, 26, z), "Shape " + shape_name + " failed mantle solid check at y=26");
                }
            }
        }

        // Perimeter sealing check (outside room box at x=26, x=46, z=26, z=46 must be solid host rock)
        for (int y = 4; y <= 25; y += 3) {
            TEST_CHECK(world.is_solid(26, y, 36), "Shape " + shape_name + " must be sealed on West exterior border");
            TEST_CHECK(world.is_solid(46, y, 36), "Shape " + shape_name + " must be sealed on East exterior border");
            TEST_CHECK(world.is_solid(36, y, 26), "Shape " + shape_name + " must be sealed on South exterior border");
            TEST_CHECK(world.is_solid(36, y, 46), "Shape " + shape_name + " must be sealed on North exterior border");
        }

        // 2. Locate a safe, open starting position on the floor
        glm::vec3 safe_pos(36.5f, 15.0f, 36.5f);
        bool found_safe = false;
        for (int ox = 0; ox <= 5 && !found_safe; ++ox) {
            for (int oz = 0; oz <= 5 && !found_safe; ++oz) {
                float tx = 36.5f + static_cast<float>(ox);
                float tz = 36.5f + static_cast<float>(oz);
                PlayerController test_p(glm::vec3(tx, 20.0f, tz));
                test_p.clamp_to_surface(world);
                if (!test_p.is_penetrating_solid(world) && test_p.position().y >= 5.0f && test_p.position().y <= 24.0f) {
                    safe_pos = test_p.position();
                    found_safe = true;
                }
            }
        }
        TEST_CHECK(found_safe, "Shape " + shape_name + " must have a walkable floor tile near center");

        PlayerController player(safe_pos);
        TEST_CHECK(!player.is_penetrating_solid(world), "Player spawned penetrating geometry in shape " + shape_name);

        // 3. Horizontal Sprinting Stress in 8 compass directions
        const glm::vec3 sprint_dirs[8] = {
            glm::vec3(15.0f, 0.0f, 0.0f),
            glm::vec3(-15.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 15.0f),
            glm::vec3(0.0f, 0.0f, -15.0f),
            glm::vec3(11.0f, 0.0f, 11.0f),
            glm::vec3(-11.0f, 0.0f, 11.0f),
            glm::vec3(11.0f, 0.0f, -11.0f),
            glm::vec3(-11.0f, 0.0f, -11.0f)
        };

        for (int d = 0; d < 8; ++d) {
            player.set_position(safe_pos);
            player.set_velocity(sprint_dirs[d]);

            for (int tick = 0; tick < 45; ++tick) {
                player.velocity_mut() = sprint_dirs[d];
                player.update_physics(1.0f / 60.0f, world);
                TEST_CHECK(!player.is_penetrating_solid(world),
                           "Shape " + shape_name + " clipped during high-speed horizontal sprint dir=" + std::to_string(d));
            }

            // Player must be contained within room boundary (never penetrated host rock border)
            TEST_CHECK(player.position().x >= 27.2f && player.position().x <= 44.8f,
                       "Shape " + shape_name + " tunneled through X outer boundary");
            TEST_CHECK(player.position().z >= 27.2f && player.position().z <= 44.8f,
                       "Shape " + shape_name + " tunneled through Z outer boundary");
        }

        // 4. Ceiling Thruster / Grapple Vertical Stress Test
        player.set_position(safe_pos);
        player.set_velocity(glm::vec3(0.0f, 25.0f, 0.0f));
        for (int tick = 0; tick < 40; ++tick) {
            player.velocity_mut().y = 25.0f;
            player.update_physics(1.0f / 60.0f, world);
            TEST_CHECK(!player.is_penetrating_solid(world),
                       "Shape " + shape_name + " clipped into ceiling during vertical thruster ascent");
        }
        TEST_CHECK(player.position().y < 25.5f, "Shape " + shape_name + " player clipped into or above mantle layer");

        // 5. Downward Gravity / Floor Fall Check
        player.set_velocity(glm::vec3(0.0f, -35.0f, 0.0f));
        for (int tick = 0; tick < 45; ++tick) {
            player.velocity_mut().y = -35.0f;
            player.update_physics(1.0f / 60.0f, world);
            TEST_CHECK(!player.is_penetrating_solid(world),
                       "Shape " + shape_name + " clipped into floor during downward impact");
        }
        TEST_CHECK(player.position().y >= 4.0f, "Shape " + shape_name + " player fell through bedrock floor");
    }

    log_pass("All 27 base room shapes thoroughly stress tested individually with 0 clipping/tunneling errors");
}

// ─────────────────────────────────────────────────────────────
// 7. INTEGRATION TESTS: RANDOM SHAPE PERMUTATIONS & CONNECTIVITY
// ─────────────────────────────────────────────────────────────
void test_flight_carving_and_mesh_integrity() {
    std::cout << "\n=== [MODULE 9] Flight Carving, Block Breaking & Mesh Integrity Stress Test ===" << std::endl;

    World world(9999, false);
    world.generate_world(1, 9999);

    // Initial flight spawn at elevated altitude
    PlayerController flyer(glm::vec3(12.0f, 15.0f, 12.0f));

    // Carve a continuous diagonal tunnel across multiple chunks (Chunk (0,0) to Chunk (1,1))
    // Trajectory spans X: 12 -> 50, Z: 12 -> 50, crossing X=32 and Z=32 chunk boundaries
    int carved_count = 0;
    const int num_frames = 120;
    for (int f = 0; f < num_frames; ++f) {
        float t = static_cast<float>(f) / static_cast<float>(num_frames);
        glm::vec3 flight_pos(12.0f + t * 38.0f,
                             15.0f + std::sin(t * 3.14159f * 2.0f) * 4.0f,
                             12.0f + t * 38.0f);
        flyer.set_position(flight_pos);

        // Mine/carve a 3x3x3 spherical cavity around flyer's mining tool position
        glm::ivec3 carve_center(static_cast<int>(std::floor(flight_pos.x)),
                                static_cast<int>(std::floor(flight_pos.y)),
                                static_cast<int>(std::floor(flight_pos.z)));

        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    glm::ivec3 bp = carve_center + glm::ivec3(dx, dy, dz);
                    if (bp.y > 3 && bp.y < 26) {
                        Voxel vox = world.get_voxel(bp.x, bp.y, bp.z);
                        if (vox.is_solid() && vox.material_id != MAT_DREDGE_BEDROCK) {
                            world.set_voxel(bp.x, bp.y, bp.z, Voxel{MAT_AIR, 0}, true);
                            carved_count++;
                        }
                    }
                }
            }
        }
    }

    TEST_CHECK(carved_count > 50, "Stress test must have carved at least 50 blocks along diagonal flight path");

    // Neighbor getter lambda for synchronous GreedyMesher verification
    auto neighbor_getter = [&world](const ChunkPos& npos) -> const Chunk* {
        return world.get_chunk(npos);
    };

    // Meshing & Geometry Integrity Verification across all chunks
    int total_quads_verified = 0;
    for (int cz = 0; cz < 3; ++cz) {
        for (int cx = 0; cx < 3; ++cx) {
            ChunkPos cpos{cx, 0, cz};
            const Chunk* chunk = world.get_chunk(cpos);
            TEST_CHECK(chunk != nullptr, "Chunk at (" + std::to_string(cx) + "," + std::to_string(cz) + ") must exist");

            // Generate greedy mesh with full neighbor context
            std::vector<PackedVoxelVertex> mesh = GreedyMesher::generate_mesh(*chunk, neighbor_getter);
            TEST_CHECK(mesh.size() % 3 == 0, "Greedy mesher must emit complete triangles (mesh.size() % 3 == 0)");

            size_t num_tris = mesh.size() / 3;
            for (size_t t = 0; t < num_tris; ++t) {
                // Verify all 3 vertices of each triangle
                for (size_t v_idx = 0; v_idx < 3; ++v_idx) {
                    const auto& v = mesh[t * 3 + v_idx];

                    // Unpack 6-bit coordinates
                    uint32_t lx = (v.data0 & 0x3Fu);
                    uint32_t ly = ((v.data0 >> 6u) & 0x3Fu);
                    uint32_t lz = ((v.data0 >> 12u) & 0x3Fu);
                    uint32_t norm_idx = ((v.data0 >> 18u) & 0x7u);
                    uint32_t ao = ((v.data0 >> 21u) & 0x3u);
                    uint32_t tex_layer = ((v.data0 >> 23u) & 0xFFu);

                    // Assert coordinate bounds: [0..32]
                    TEST_CHECK(lx <= 32, "Vertex localX out of bounds: " + std::to_string(lx));
                    TEST_CHECK(ly <= 32, "Vertex localY out of bounds: " + std::to_string(ly));
                    TEST_CHECK(lz <= 32, "Vertex localZ out of bounds: " + std::to_string(lz));

                    // Assert valid normal index
                    TEST_CHECK(norm_idx <= 5, "Invalid normal index: " + std::to_string(norm_idx));

                    // Assert valid AO
                    TEST_CHECK(ao <= 3, "Invalid AO: " + std::to_string(ao));

                    // Assert valid texture layer (not air, not corrupted)
                    TEST_CHECK(tex_layer > 0 && tex_layer < 255, "Invalid texture layer ID: " + std::to_string(tex_layer));

                    // Unpack data1 quad dimensions
                    uint32_t u_dim = (v.data1 & 0x3Fu);
                    uint32_t v_dim = ((v.data1 >> 6u) & 0x3Fu);
                    uint32_t corner_idx = ((v.data1 >> 12u) & 0x3u);

                    TEST_CHECK(u_dim >= 1 && u_dim <= 32, "Invalid u_dim: " + std::to_string(u_dim));
                    TEST_CHECK(v_dim >= 1 && v_dim <= 32, "Invalid v_dim: " + std::to_string(v_dim));
                    TEST_CHECK(corner_idx <= 3, "Invalid corner_idx: " + std::to_string(corner_idx));
                }

                // Check for non-degenerate triangles (no zero area)
                const auto& v0 = mesh[t * 3 + 0];
                const auto& v1 = mesh[t * 3 + 1];
                const auto& v2 = mesh[t * 3 + 2];

                glm::vec3 p0 = v0.position();
                glm::vec3 p1 = v1.position();
                glm::vec3 p2 = v2.position();

                float tri_area = glm::length(glm::cross(p1 - p0, p2 - p0)) * 0.5f;
                TEST_CHECK(tri_area > 0.001f, "Degenerate zero-area triangle detected in greedy mesh quad!");

                total_quads_verified++;
            }
        }
    }

    log_pass("Flight carving (" + std::to_string(carved_count) + " blocks mined across chunks): " +
             std::to_string(total_quads_verified) + " quads verified, 0 corrupted vertices, 0 degenerate faces");
}

// ─────────────────────────────────────────────────────────────
// 10. ENVIRONMENTAL ROOM DANGERS & MORTALITY TESTS
// ─────────────────────────────────────────────────────────────
void test_environmental_room_dangers_and_mortality() {
    std::cout << "\n=== [MODULE 10] Environmental Room Dangers & Lethality Test ===" << std::endl;

    World world(8888, false);
    // Create an isolated testbed with bedrock base and open air above
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            world.set_voxel(x, 0, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
            world.set_voxel(x, 1, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
            for (int y = 2; y <= 25; ++y) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
            }
        }
    }

    // 1. Lava Pit Danger & Thermal Fatality
    // Fill a 6x6 test pit with MAT_THERMITE_SLAG at y=2
    for (int x = 4; x <= 9; ++x) {
        for (int z = 4; z <= 9; ++z) {
            world.set_voxel(x, 2, z, Voxel{MAT_THERMITE_SLAG, 0}, false);
        }
    }

    PlayerController lava_player(glm::vec3(6.5f, 3.0f, 6.5f));
    lava_player.apply_attributes_and_upgrades(CharacterClass::Scout, UpgradeTree{}); // 90 HP base
    lava_player.set_shield(0.0f);
    std::string last_warning;
    lava_player.set_on_warning([&](const std::string& msg) { last_warning = msg; });

    // Step 0.5s inside lava
    for (int step = 0; step < 50; ++step) {
        lava_player.update_physics(0.01f, world);
    }
    TEST_CHECK(lava_player.is_in_lava(), "Player standing in molten slag must register is_in_lava()");
    TEST_CHECK(lava_player.health() < 90.0f, "Player in lava must take thermal damage");
    TEST_CHECK(lava_player.exo().heat > 0.0f, "Exo-suit heat must increase in lava");

    // Viscous drag verification: give player horizontal velocity and assert it is dragged down rapidly
    lava_player.set_velocity(glm::vec3(8.0f, 0.0f, 8.0f));
    for (int step = 0; step < 20; ++step) {
        lava_player.update_physics(0.01f, world);
    }
    float horiz_speed = glm::length(glm::vec2(lava_player.velocity().x, lava_player.velocity().z));
    TEST_CHECK(horiz_speed < 2.0f, "Lava viscous drag must rapidly arrest horizontal delver movement");

    // Submersion until death: ~3.5 seconds
    while (lava_player.health() > 0.0f) {
        lava_player.update_physics(0.05f, world);
    }
    TEST_CHECK(lava_player.health() == 0.0f, "Extended lava immersion must result in fatal delver incineration");
    TEST_CHECK(lava_player.exo().integrity == 0.0f, "Suit integrity must be 0% upon fatality");
    log_pass("Lava hazard: viscous drag, heat accumulation, and lethal incineration verified");

    // 2. Spike Trench Puncture Danger & Trapped Fatality
    // Fill a 4x4 pit with obsidian spikes (MAT_OBSIDIAN_SPIKES)
    for (int x = 12; x <= 16; ++x) {
        for (int z = 12; z <= 16; ++z) {
            world.set_voxel(x, 2, z, Voxel{MAT_OBSIDIAN_SPIKES, 0}, false);
        }
    }

    PlayerController spike_player(glm::vec3(14.0f, 3.0f, 14.0f));
    spike_player.apply_attributes_and_upgrades(CharacterClass::Scout, UpgradeTree{});
    spike_player.set_shield(0.0f);
    spike_player.set_on_warning([&](const std::string& msg) { last_warning = msg; });

    // Single step on spikes
    spike_player.update_physics(0.02f, world);
    TEST_CHECK(spike_player.is_in_spikes(), "Player on spike voxels must register is_in_spikes()");
    TEST_CHECK(spike_player.health() <= 65.0f, "Spike trap must deal 25 puncture damage on trigger");
    TEST_CHECK(spike_player.spike_damage_timer() > 0.4f, "Spike debounce timer must be set to prevent instant death");

    // Trapped in spikes: continue stepping for 3.0s until dead
    while (spike_player.health() > 0.0f) {
        spike_player.update_physics(0.05f, world);
    }
    TEST_CHECK(spike_player.health() == 0.0f, "Delver trapped in spike pit must perish from periodic impalement");
    log_pass("Spike trap hazard: puncture damage, debounce timing, and trapped lethality verified");

    // 3. Catastrophic Fall Impact Damage
    // Explicitly carve an open 3x3 vertical test shaft: air from y=5 to y=22, floor at y=4
    for (int x = 19; x <= 21; ++x) {
        for (int z = 19; z <= 21; ++z) {
            world.set_voxel(x, 4, z, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
            for (int y = 5; y <= 22; ++y) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
            }
        }
    }

    // Fall 1: High cavern drop (y=21 down to floor y=4.9)
    PlayerController fall_player1(glm::vec3(20.0f, 21.0f, 20.0f));
    fall_player1.apply_attributes_and_upgrades(CharacterClass::Scout, UpgradeTree{});
    fall_player1.set_shield(0.0f);
    int step_count = 0;
    while (!fall_player1.is_grounded() && step_count < 150) {
        fall_player1.update_physics(0.016f, world);
        step_count++;
    }
    float hp_after_fall = fall_player1.health();
    TEST_CHECK(fall_player1.is_grounded(), "Player must land on the floor");
    TEST_CHECK(hp_after_fall < 80.0f, "High drop must deal substantial fall damage");
    TEST_CHECK(hp_after_fall > 0.0f && hp_after_fall <= 55.0f, "High drop should leave Scout heavily injured but alive (~20-55 HP)");

    // Fall 2: Fatal fall with downward explosive propulsion / pre-damaged delver
    PlayerController fall_player2(glm::vec3(20.0f, 21.0f, 20.0f));
    fall_player2.apply_attributes_and_upgrades(CharacterClass::Scout, UpgradeTree{});
    fall_player2.set_shield(0.0f);
    fall_player2.set_velocity(glm::vec3(0.0f, -20.0f, 0.0f)); // concussive downward blast velocity
    fall_player2.set_on_warning([&](const std::string& msg) { last_warning = msg; });
    int step_count2 = 0;
    while (!fall_player2.is_grounded() && fall_player2.health() > 0.0f && step_count2 < 150) {
        fall_player2.update_physics(0.016f, world);
        step_count2++;
    }
    TEST_CHECK(fall_player2.health() == 0.0f, "High-velocity vertical impact must be fatal to delver");
    log_pass("Fall damage: survivable high jump damage and fatal high-velocity impact verified");

    // 4. Void Abyss Singularity Hazard & Fatal Fall
    for (int x = 20; x <= 24; ++x) {
        for (int z = 20; z <= 24; ++z) {
            for (int y = 0; y <= 5; ++y) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
            }
        }
    }

    PlayerController void_player(glm::vec3(22.0f, 1.5f, 22.0f));
    void_player.apply_attributes_and_upgrades(CharacterClass::Scout, UpgradeTree{});
    void_player.set_shield(0.0f);
    void_player.set_health(25.0f); // Pre-damaged delver falls into the abyss

    void_player.update_physics(0.02f, world);
    TEST_CHECK(void_player.health() == 0.0f, "Falling into void abyss at low health must be fatal");
    log_pass("Void chasm singularity: lethal void consumption verified");
}

// ─────────────────────────────────────────────────────────────
// 11. SPAWN SAFETY, HAZARD VISIBILITY & FULL LEVEL WALK/FLIGHT
// ─────────────────────────────────────────────────────────────
void test_extraction_pod_grounding_clearance_and_rigidity() {
    std::cout << "\n=== [MODULE 15] Extraction Pod Ground Placement, Landing Clearance & Rigidity ===" << std::endl;

    World world(42, false);
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            world.set_voxel(x, 4, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
        }
    }

    // 1. Valid Landing Site: Open space with ground at y=4 -> ground surface at 5.0f
    glm::vec3 open_lz(16.0f, 6.0f, 16.0f);
    float ground_y = 0.0f;
    std::string reason;
    bool clear1 = ExtractionSystem::check_landing_clearance(world, open_lz, ground_y, &reason);
    TEST_CHECK(clear1, "Open landing zone must pass clearance check");
    TEST_CHECK(std::abs(ground_y - 5.0f) < 0.001f, "Ground level must be exact top of floor voxel (5.0f)");

    // 2. Low Ceiling Obstruction: add ceiling at y=7 (only 2 blocks clearance above ground y=4)
    World cramped_world(42, false);
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            cramped_world.set_voxel(x, 4, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
            cramped_world.set_voxel(x, 7, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
        }
    }
    bool clear_low = ExtractionSystem::check_landing_clearance(cramped_world, open_lz, ground_y, &reason);
    TEST_CHECK(!clear_low, "Low ceiling (y=7) must fail landing clearance");
    TEST_CHECK(reason.find("LOW CEILING") != std::string::npos || reason.find("OBSTRUCTED") != std::string::npos,
        "Clearance failure reason must indicate low ceiling or obstruction");

    // 3. Wall Block in Footprint: place a boulder at (16, 5, 16) right in landing column
    World blocked_world(42, false);
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            blocked_world.set_voxel(x, 4, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
        }
    }
    blocked_world.set_voxel(16, 5, 16, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
    bool clear_boulder = ExtractionSystem::check_landing_clearance(blocked_world, open_lz, ground_y, &reason);
    TEST_CHECK(!clear_boulder, "Obstruction block inside landing footprint must fail clearance");

    // 4. Ground-Level Beacon & Pod Landing: verify pod sits at ground level (not floating)
    ExtractionSystem evac;
    glm::vec3 pod_pos(16.0f, ground_y, 16.0f);
    evac.deploy_beacon(pod_pos, 40.0f);
    TEST_CHECK(evac.beacon_position().y == 5.0f, "Beacon position Y must be grounded at 5.0f");

    // 5. Rigid Pod Collider & Anti-Penetration Tests
    PlayerController player;
    player.set_position(glm::vec3(16.0f, 5.9f, 10.0f));

    // A) Pod is descending / landed and closed (ramp = 0.0)
    player.set_pod_collider(pod_pos, 1.0f, 0.0f, true);

    // Try to run straight into the center of the pod from the south (-Z)
    player.set_position(glm::vec3(16.0f, 5.9f, 15.5f));
    player.set_velocity(glm::vec3(0.0f, 0.0f, 5.0f));
    bool collided_back = player.resolve_extraction_pod_collision(pod_pos, 1.0f, 0.0f, true);
    TEST_CHECK(collided_back, "Player inside pod bounds must register collision");
    float dist_back = glm::distance(glm::vec2(player.position().x, player.position().z), glm::vec2(pod_pos.x, pod_pos.z));
    TEST_CHECK(dist_back >= 1.75f, "Player must be depenetrated to at least MIN_COLLISION_DIST (>= 1.75m)");
    TEST_CHECK(player.velocity().z <= 0.0f, "Inward velocity into pod must be eliminated");

    // Camera eye position check: distance to outer wall must be > near clipping plane (0.10m)
    float eye_to_wall = dist_back - 1.45f;
    TEST_CHECK(eye_to_wall >= 0.30f, "Eye position must maintain at least 0.30m margin outside hull (near plane 0.10m)");

    // B) Try to penetrate from the side (+X)
    player.set_position(glm::vec3(16.8f, 5.9f, 16.0f));
    player.set_velocity(glm::vec3(-4.0f, 0.0f, 0.0f));
    bool collided_side = player.resolve_extraction_pod_collision(pod_pos, 1.0f, 0.0f, true);
    TEST_CHECK(collided_side, "Player entering from side must collide");
    float dist_side = glm::distance(glm::vec2(player.position().x, player.position().z), glm::vec2(pod_pos.x, pod_pos.z));
    TEST_CHECK(dist_side >= 1.75f, "Side collision must depenetrate player to >= 1.75m");
    TEST_CHECK(player.velocity().x >= 0.0f, "Inward X velocity must be canceled");

    // C) Boarding Ramp Test (+Z direction, ramp extended to 1.0)
    player.set_pod_collider(pod_pos, 1.0f, 1.0f, true);
    player.set_position(glm::vec3(16.0f, 5.9f, 18.5f));
    player.set_velocity(glm::vec3(0.0f, -2.0f, -3.0f));
    bool ramp_col = player.resolve_extraction_pod_collision(pod_pos, 1.0f, 1.0f, true);
    TEST_CHECK(ramp_col, "Ramp interaction must register");
    TEST_CHECK(player.is_grounded(), "Player on ramp must be grounded");
    TEST_CHECK(player.position().y >= 5.0f + 0.9f, "Player feet must be supported on inclined ramp deck");

    // D) Ramp Handrails: Player trying to walk off sides of ramp at |dx| > 0.52m is constrained
    player.set_position(glm::vec3(16.8f, 5.9f, 18.5f));
    player.resolve_extraction_pod_collision(pod_pos, 1.0f, 1.0f, true);
    TEST_CHECK(player.position().x <= 16.0f + 0.55f, "Ramp handrail must constrain player within ramp walkway width");

    // E) Roof Support: Player lands on top of pod (y = 5.0f + 4.20f = 9.20f)
    player.set_position(glm::vec3(16.0f, 9.20f + 0.9f + 0.1f, 16.0f));
    player.set_velocity(glm::vec3(0.0f, -5.0f, 0.0f));
    bool roof_col = player.resolve_extraction_pod_collision(pod_pos, 1.0f, 1.0f, true);
    TEST_CHECK(roof_col, "Top deck of pod must collide and support player");
    TEST_CHECK(player.is_grounded(), "Player standing on pod roof must be grounded");
    TEST_CHECK(player.velocity().y >= 0.0f, "Downward velocity on pod roof must be stopped");

    log_pass("Extraction Pod grounded landing, space clearance checks & rigid physical collision verified");
}

// ─────────────────────────────────────────────────────────────
// 16. STRUCTURAL CONNECTIVITY & FLOATING BLOCK PREVENTION TEST
// ─────────────────────────────────────────────────────────────

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- LEVEL COLLISION AND PHYSICS TEST SUITE" << std::endl;
    std::cout << "==========================================================" << std::endl;

    test_wall_collision_and_anti_clipping();
    test_ceiling_flying_and_anti_clipping();
    test_block_break_and_walkthrough();
    test_diagonal_corner_and_depenetration();
    test_isolated_base_shapes_stress();
    test_flight_carving_and_mesh_integrity();
    test_environmental_room_dangers_and_mortality();
    test_extraction_pod_grounding_clearance_and_rigidity();

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "  ALL " << s_passed_tests << " LEVEL COLLISION AND PHYSICS TESTS PASSED!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
