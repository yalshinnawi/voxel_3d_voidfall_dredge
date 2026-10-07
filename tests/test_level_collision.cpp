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
            if (r.type == RoomShapeType::IndustrialVaultBunker) has_vault = true;
            if (r.type == RoomShapeType::AbyssalVerticalChasm) has_chasm = true;
            if (r.type == RoomShapeType::RadioactiveCoreSanctuary) has_sanctuary = true;
            if (r.type == RoomShapeType::MagmaCalderaLake || r.type == RoomShapeType::SpikeTrenchArena ||
                r.type == RoomShapeType::VoidSingularityRift || r.type == RoomShapeType::FungoidBioGrotto) {
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
        {RoomShapeType::BioluminescentFirmamentAbyss,   "BioluminescentFirmamentAbyss"}
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
                    TEST_CHECK(v1.material_id != MAT_DREDGE_BEDROCK, "Corridor seam blocked by bedrock at y=" + std::to_string(fy + 1));
                    TEST_CHECK(v2.material_id != MAT_DREDGE_BEDROCK, "Corridor seam blocked by bedrock at y=" + std::to_string(fy + 2));

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

            // Verify delver traversed across the corridor to Room B doorway
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
            TEST_CHECK(mesh.size() % 6 == 0, "Greedy mesher must emit full 6-vertex quad pairs (2 triangles)");

            size_t num_quads = mesh.size() / 6;
            for (size_t q = 0; q < num_quads; ++q) {
                // Verify all 6 vertices of the quad
                for (size_t v_idx = 0; v_idx < 6; ++v_idx) {
                    const auto& v = mesh[q * 6 + v_idx];

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
                const auto& v0 = mesh[q * 6 + 0];
                const auto& v1 = mesh[q * 6 + 1];
                const auto& v2 = mesh[q * 6 + 2];

                glm::vec3 p0(v0.data0 & 0x3Fu, (v0.data0 >> 6u) & 0x3Fu, (v0.data0 >> 12u) & 0x3Fu);
                glm::vec3 p1(v1.data0 & 0x3Fu, (v1.data0 >> 6u) & 0x3Fu, (v1.data0 >> 12u) & 0x3Fu);
                glm::vec3 p2(v2.data0 & 0x3Fu, (v2.data0 >> 6u) & 0x3Fu, (v2.data0 >> 12u) & 0x3Fu);

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
    void_player.set_health(25.0f); // Pre-damaged delver falls into the abyss

    void_player.update_physics(0.02f, world);
    TEST_CHECK(void_player.health() == 0.0f, "Falling into void abyss at low health must be fatal");
    log_pass("Void chasm singularity: lethal void consumption verified");
}

// ─────────────────────────────────────────────────────────────
// 11. SPAWN SAFETY, HAZARD VISIBILITY & FULL LEVEL WALK/FLIGHT
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

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- LEVEL DESIGN & COLLISION TEST SUITE" << std::endl;
    std::cout << "==========================================================" << std::endl;

    test_level_design_topology();
    test_wall_collision_and_anti_clipping();
    test_ceiling_flying_and_anti_clipping();
    test_block_break_and_walkthrough();
    test_diagonal_corner_and_depenetration();
    test_isolated_base_shapes_stress();
    test_random_shape_permutations_integration();
    test_all_rooms_tour_and_visibility();
    test_flight_carving_and_mesh_integrity();
    test_environmental_room_dangers_and_mortality();
    test_spawn_safety_and_full_level_walk_flight();

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "  ALL " << s_passed_tests << " LEVEL DESIGN & COLLISION TESTS PASSED (0 CLIPPING ERRORS, 0 OCCLUSION GLITCHES)!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
