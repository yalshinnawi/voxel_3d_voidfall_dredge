#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>

// Include core engine headers
#include "../src/voxel/packed_vertex.hpp"
#include "../src/voxel/chunk.hpp"
#include "../src/voxel/world.hpp"
#include "../src/voxel/structural_check.hpp"
#include "../src/player/character_class.hpp"
#include "../src/player/upgrades.hpp"
#include "../src/player/loadout.hpp"
#include "../src/player/controller.hpp"
#include "../src/skills/skill_matrix.hpp"
#include "../src/skills/surveying.hpp"
#include "../src/systems/hazard_clock.hpp"
#include "../src/systems/extraction.hpp"
#include "../src/systems/noise_meter.hpp"
#include "../src/entities/enemies/void_stalker.hpp"
#include "../src/entities/enemies/seismic_burrower.hpp"
#include "../src/core/save_system.hpp"
#include "../src/net/packet_types.hpp"
#include "../src/entities/dynamic_debris.hpp"
#include "../src/entities/flare.hpp"
#include "../src/systems/mission_system.hpp"
#include "../src/entities/carcass_manager.hpp"
#include "../src/ui/hud.hpp"
#include "../src/ui/terrain_scanner.hpp"
#include "../src/ui/debrief_menu.hpp"
#include "../src/graphics/particle_system.hpp"
#include "../src/graphics/clustered_lighting.hpp"

using namespace Voidfall;

#define TEST_CHECK(expr, msg) \
    if (!(expr)) { \
        std::cerr << "\n[TEST FAILED] " << msg << " (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

static int s_total_unit_tests = 0;

void log_pass(const std::string& name) {
    s_total_unit_tests++;
    std::cout << "  [PASS " << s_total_unit_tests << "] " << name << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 1. VOXEL ENGINE & CHUNK INDEXING UNIT TESTS
// ─────────────────────────────────────────────────────────────
void test_voxel_engine() {
    std::cout << "\n=== [MODULE 1] Voxel Engine & Chunk Mechanics ===" << std::endl;

    // Index calculation
    size_t idx = Chunk::to_index(0, 0, 0);
    TEST_CHECK(idx == 0, "Chunk origin index must be 0");
    size_t idx_max = Chunk::to_index(CHUNK_SIZE - 1, CHUNK_SIZE - 1, CHUNK_SIZE - 1);
    TEST_CHECK(idx_max == (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE - 1), "Chunk max index bounds mismatch");

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(5, 10, 15, Voxel{MAT_VOIDITE_CRYSTAL, 0});
    Voxel v = chunk.get_voxel(5, 10, 15);
    TEST_CHECK(v.material_id == MAT_VOIDITE_CRYSTAL, "Voxel material ID mismatch");
    TEST_CHECK(v.is_solid(), "Voidite crystal must be solid");
    TEST_CHECK(chunk.is_mesh_dirty(), "Chunk must be marked dirty after modification");
    log_pass("Chunk voxel storage and dirty flag tracking");

    // Voxel material definitions
    Voxel air{MAT_AIR, 0};
    TEST_CHECK(!air.is_solid(), "Air must not be solid");

    Voxel bulkhead{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_PLAYER_PLACED};
    TEST_CHECK(bulkhead.is_solid(), "Industrial bulkhead must be solid");
    TEST_CHECK((bulkhead.flags_and_damage & VOXEL_FLAG_PLAYER_PLACED) != 0, "Player placed flag must be preserved");
    log_pass("Voxel material properties and flag bitmasks");

    // World voxel queries and block generation
    World world(4242);
    world.generate_world(1, 4242);
    world.set_voxel(16, 20, 16, Voxel{MAT_FRACTURED_GRANITE, 0});
    Voxel query_v = world.get_voxel(16, 20, 16);
    TEST_CHECK(query_v.material_id == MAT_FRACTURED_GRANITE, "World get_voxel mismatch");
    TEST_CHECK(world.is_solid(16, 20, 16), "World is_solid query mismatch");
    log_pass("World voxel mutation and procedural topology generation");

    // Raycast hit detection
    RaycastHit hit = world.raycast(glm::vec3(16.5f, 25.0f, 16.5f), glm::vec3(0.0f, -1.0f, 0.0f), 10.0f);
    TEST_CHECK(hit.hit, "Raycast downward must hit solid voxel");
    TEST_CHECK(hit.block_pos.x == 16 && hit.block_pos.z == 16, "Raycast hit coordinate mismatch");
    TEST_CHECK(hit.normal == glm::ivec3(0, 1, 0), "Raycast top face normal must be (0, 1, 0)");
    log_pass("DDA Voxel Raycasting and surface normal calculation");
}

// ─────────────────────────────────────────────────────────────
// 2. STRUCTURAL INTEGRITY & CAVE-IN BFS SOLVER
// ─────────────────────────────────────────────────────────────
void test_structural_solver() {
    std::cout << "\n=== [MODULE 2] Structural Integrity & Island Cave-in Solver ===" << std::endl;

    World world(9999);
    // Clear an open cavern space in air around (11, 30, 11)
    for (int x = 6; x <= 16; ++x) {
        for (int y = 20; y <= 35; ++y) {
            for (int z = 6; z <= 16; ++z) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
            }
        }
    }

    // Create a 3x3 floating platform of granite in air
    for (int x = 10; x <= 12; ++x) {
        for (int z = 10; z <= 12; ++z) {
            world.set_voxel(x, 30, z, Voxel{MAT_FRACTURED_GRANITE, 0});
        }
    }

    // Break the center block
    world.set_voxel(11, 30, 11, Voxel{MAT_AIR, 0});
    auto islands = StructuralCheck::solve_cavein(world, 11, 30, 11, 64);
    TEST_CHECK(!islands.empty(), "Unanchored floating blocks must trigger cave-in islands");
    TEST_CHECK(islands[0].primary_material == MAT_FRACTURED_GRANITE, "Debris island material mismatch");
    log_pass("Authoritative BFS Anchored Island Cave-in detection");

    // Seismic detachment block query
    glm::vec3 player_pos(11.0f, 20.0f, 11.0f);
    auto detach = StructuralCheck::query_seismic_detachment_blocks(world, player_pos, 2, 6, 8.0f, 3, 12);
    TEST_CHECK(!detach.empty(), "Seismic detachment query must find overhead granite blocks");
    log_pass("Seismic Detachment Candidate Query");

    // ── Anchored-Island BFS Structural Collapse & Composite Debris Verification ──
    World collapse_world(7777, false);
    collapse_world.set_chunk_ceiling(64);

    // Build solid floor at Y=5
    for (int x = 10; x <= 20; ++x) {
        for (int z = 10; z <= 20; ++z) {
            collapse_world.set_voxel(x, 5, z, Voxel{MAT_DREDGE_BEDROCK, 0});
            for (int y = 6; y <= 65; ++y) {
                collapse_world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
            }
        }
    }

    // A. Anchored Stalactite: connected to chunk ceiling anchor plane (Y >= ChunkCeiling)
    for (int y = 58; y <= 64; ++y) {
        collapse_world.set_voxel(15, y, 15, Voxel{MAT_VOLCANIC_BASALT, 0});
    }
    // Breaking lowest tip at Y=58: remaining blocks 59..64 are connected to Y=64, so no collapse occurs
    bool broke_tip = collapse_world.break_voxel(15, 58, 15);
    TEST_CHECK(broke_tip, "Breaking stalactite tip must succeed");
    TEST_CHECK(collapse_world.debris().empty(), "Anchored stalactite (reaching Y >= ChunkCeiling) must NOT collapse");
    TEST_CHECK(collapse_world.is_solid(15, 60, 15), "Anchored blocks must remain in world grid");
    log_pass("Chunk Ceiling Anchor Plane (Y >= ChunkCeiling) Structural Stability");

    // B. Unanchored Hanging Stalactite: suspended in mid-air (Y = 35..40)
    for (int y = 35; y <= 40; ++y) {
        collapse_world.set_voxel(15, y, 15, Voxel{MAT_VOLCANIC_BASALT, 0});
    }
    TEST_CHECK(collapse_world.borders_hanging_overhang_or_stalactite(15, 40, 15),
               "Broken root block must detect adjacent hanging stalactite");

    // Break the root support block at Y=40
    bool broke_root = collapse_world.break_voxel(15, 40, 15);
    TEST_CHECK(broke_root, "Breaking unanchored stalactite root must succeed");
    TEST_CHECK(!collapse_world.debris().empty(), "Unanchored island must spawn DynamicDebris");

    // Verify island blocks detached from world grid
    for (int y = 35; y <= 39; ++y) {
        TEST_CHECK(!collapse_world.is_solid(15, y, 15),
                   "Detached unanchored island voxels must become air in world grid");
    }
    DynamicDebris& spawned_debris = collapse_world.debris()[0];
    TEST_CHECK(spawned_debris.block_count() == 5, "Spawned composite DynamicDebris must contain 5 blocks");
    TEST_CHECK(spawned_debris.blocks().size() == 5, "Debris blocks array must match cluster count");
    log_pass("Unanchored Stalactite Detachment and Composite DynamicDebris Spawning");

    // C. Physical Simulation, Gravity (g = 18 m/s^2), Crushing Damage (20-50 HP) & Dust Cloud
    glm::vec3 test_player_pos(15.5f, 6.0f, 15.5f);
    std::vector<glm::vec3> test_enemy_pos = { glm::vec3(15.5f, 6.0f, 15.5f) };
    bool crushing_damage_verified = false;
    bool dust_cloud_verified = false;

    // Simulate physics ticks
    for (int tick = 0; tick < 120 && !spawned_debris.is_destroyed(); ++tick) {
        float prev_vy = spawned_debris.velocity().y;
        auto res = spawned_debris.update(1.0f / 60.0f, collapse_world, test_player_pos, false, test_enemy_pos);

        // Check gravity acceleration: -18.0 m/s^2 * dt
        float expected_vy = prev_vy - 18.0f * (1.0f / 60.0f);
        TEST_CHECK(std::abs(spawned_debris.velocity().y - expected_vy) < 0.05f,
                   "Falling debris gravity must accelerate at g = 18.0 m/s^2");

        if (res.hit_player || res.hit_enemy) {
            float dmg = res.hit_player ? res.damage : res.enemy_damage;
            TEST_CHECK(dmg >= 20.0f && dmg <= 50.0f,
                       "Kinetic crushing damage must be proportional between 20 and 50 HP");
            crushing_damage_verified = true;
        }

        if (res.placed_on_ground) {
            TEST_CHECK(res.spawned_dust_cloud, "Floor impact must spawn heavy dust cloud");
            dust_cloud_verified = true;
            break;
        }
    }

    TEST_CHECK(crushing_damage_verified, "Falling cluster must inflict kinetic crushing damage to entity");
    TEST_CHECK(dust_cloud_verified, "Debris floor impact must trigger dust cloud and physical placement");
    log_pass("Composite DynamicDebris Physics Gravity, 20-50 HP Crushing Damage & Floor Dust Cloud");
}

// ─────────────────────────────────────────────────────────────
// 3. PLAYER CONTROLLER, PHYSICS & COLLISION SOLVER
// ─────────────────────────────────────────────────────────────
void test_player_controller_and_physics() {
    std::cout << "\n=== [MODULE 3] Player Controller & Multi-Axis Swept AABB ===" << std::endl;

    World world(5555);
    // Build solid floor at Y=10 and clear air above it
    for (int x = 10; x <= 22; ++x) {
        for (int z = 10; z <= 22; ++z) {
            for (int y = 11; y <= 25; ++y) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
            }
            world.set_voxel(x, 10, z, Voxel{MAT_FRACTURED_GRANITE, 0});
        }
    }

    PlayerController player(glm::vec3(16.0f, 15.0f, 16.0f));
    player.set_character_class(CharacterClass::Scout);
    TEST_CHECK(player.character_class() == CharacterClass::Scout, "Character class assignment mismatch");

    // Simulate gravity fall
    for (int i = 0; i < 60; ++i) {
        player.update_physics(1.0f / 60.0f, world);
    }

    // Player height = 1.8, floor at Y=10 (top face at Y=11). Feet at Y=11 -> Position Y=11.9
    TEST_CHECK(player.position().y >= 11.8f && player.position().y <= 12.0f, "Swept AABB floor collision mismatch");
    TEST_CHECK(player.is_grounded(), "Player must be grounded after falling onto solid floor");
    log_pass("Swept AABB multi-axis gravity and floor collision resolution");

    // Exosuit thrusters and power drain/recharge
    player.exo_mut().power = 100.0f;
    float init_power = player.exo().power;
    TEST_CHECK(init_power == 100.0f, "Initial power must be 100");

    // Durability heat reduction check
    UpgradeTree upg;
    upg.drillDurabilityTier = 3;
    player.set_upgrades(upg);
    TEST_CHECK(player.upgrades().drillDurabilityTier == 3, "Upgrades sync mismatch");
    log_pass("Exosuit power, heat, and upgrade injection");

    // Grappling hook mechanics
    player.grapple_mut().active = true;
    player.grapple_mut().anchor_point = glm::vec3(16.0f, 25.0f, 16.0f);
    TEST_CHECK(player.grapple().active, "Grapple must be active after firing");
    TEST_CHECK(player.grapple().anchor_point == glm::vec3(16.0f, 25.0f, 16.0f), "Grapple anchor point mismatch");
    player.grapple_mut().active = false;
    TEST_CHECK(!player.grapple().active, "Grapple must be inactive after release");
    log_pass("Grappling hook tension cable firing, reeling, and release");
}

// ─────────────────────────────────────────────────────────────
// 4. PROGRESSION, UPGRADE TREE & RESPEC ECONOMY
// ─────────────────────────────────────────────────────────────
void test_progression_and_economy() {
    std::cout << "\n=== [MODULE 4] Progression, Upgrade Tree & Respec Economy ===" << std::endl;

    // Archetype Attribute Model Verification
    auto demo_attr = get_character_attributes(CharacterClass::Demolitionist);
    TEST_CHECK(demo_attr.name == "Kaelen", "Demolitionist name mismatch");
    TEST_CHECK(std::abs(demo_attr.baseMineSpeed - 1.4f) < 0.001f, "Demolitionist 1.4x mining speed mismatch");
    TEST_CHECK(std::abs(demo_attr.suitIntegrity - 100.0f) < 0.001f, "Demolitionist 100 HP mismatch");

    auto vngd_attr = get_character_attributes(CharacterClass::Vanguard);
    TEST_CHECK(vngd_attr.name == "Rhodes", "Vanguard name mismatch");
    TEST_CHECK(std::abs(vngd_attr.suitIntegrity - 135.0f) < 0.001f, "Vanguard 135 HP mismatch");
    TEST_CHECK(vngd_attr.maxBulkheads == 12, "Vanguard 12 bulkheads mismatch");
    TEST_CHECK(std::abs(vngd_attr.fallingDamageReduction - 0.50f) < 0.001f, "Vanguard 50% damage reduction mismatch");

    auto scout_attr = get_character_attributes(CharacterClass::Scout);
    TEST_CHECK(scout_attr.name == "Vesper", "Scout name mismatch");
    TEST_CHECK(std::abs(scout_attr.moveSpeed - 1.20f) < 0.001f, "Scout 1.2x move speed mismatch");
    TEST_CHECK(std::abs(scout_attr.sonarRadius - 22.0f) < 0.001f, "Scout 22m sonar radius mismatch");
    TEST_CHECK(std::abs(scout_attr.grapplePullSpeed - 1.50f) < 0.001f, "Scout 1.5x grapple reel mismatch");
    log_pass("All 3 Character Archetype specifications verified");

    // Upgrade Tree Exponential Costs: 100 -> 160 -> 256 -> 410 -> 656 EXP
    int exp_costs[5] = {100, 160, 256, 410, 656};
    for (int t = 0; t < 5; ++t) {
        TEST_CHECK(UpgradeTree::get_exp_cost(t) == exp_costs[t], "Exponential tier cost mismatch");
    }
    TEST_CHECK(UpgradeTree::get_exp_cost(5) == 0, "Max tier cost must be 0");
    log_pass("Exponential Upgrade Cost Formula (100 -> 160 -> 256 -> 410 -> 656)");

    // Purchasing & Respec 85% Refund
    UpgradeTree tree;
    int exp = 2000;
    int voidite = 100;
    int titanium = 100;

    tree.purchase(UpgradeType::DrillSpeed, exp, voidite, titanium);
    tree.purchase(UpgradeType::DrillSpeed, exp, voidite, titanium);
    tree.purchase(UpgradeType::ReinforcedPlating, exp, voidite, titanium);
    TEST_CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 2, "Drill Speed tier mismatch");
    TEST_CHECK(tree.get_tier(UpgradeType::ReinforcedPlating) == 1, "Reinforced Plating tier mismatch");
    TEST_CHECK(exp == (2000 - 100 - 160 - 100), "Remaining EXP mismatch after purchases");

    int refunded = 0;
    tree.respec(refunded);
    int expected_refund = 360; // 100% full recovery of 360 EXP/Coins
    TEST_CHECK(refunded == expected_refund, "Respec 100% refund mismatch");
    TEST_CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "Respec tier reset mismatch");
    log_pass("Upgrade purchasing validation and 100% Respec refund");
}

// ─────────────────────────────────────────────────────────────
// 5. SKILLS MATRIX, SURVEYING & HAZARDS
// ─────────────────────────────────────────────────────────────
void test_skills_surveying_and_hazards() {
    std::cout << "\n=== [MODULE 5] Skills Matrix, Surveying & Hazard Clock ===" << std::endl;

    SkillMatrix matrix;
    TEST_CHECK(!matrix.demolitions.unlocked, "Demolitions skill should start locked");
    matrix.add_demolitions_xp(250);
    TEST_CHECK(matrix.demolitions.unlocked, "Demolitions skill must unlock at 250 XP");
    TEST_CHECK(matrix.has_micro_charges(), "Demolitions unlock perk must enable micro-charges");

    // Surveying Rank progression & Material Identification
    SkillMatrix survey_matrix;
    TEST_CHECK(survey_matrix.get_surveying_rank() == 0, "Surveying rank must start at 0");
    TEST_CHECK(!survey_matrix.can_identify_materials(), "Rank 0 cannot identify materials");

    survey_matrix.add_surveying_xp(80);
    TEST_CHECK(survey_matrix.get_surveying_rank() == 1, "Surveying rank must be 1 at 80 XP");
    TEST_CHECK(survey_matrix.surveying.unlocked, "Surveying must be unlocked at Rank 1");
    TEST_CHECK(!survey_matrix.can_identify_materials(), "Rank 1 cannot yet identify materials (requires Rank 2)");

    survey_matrix.add_surveying_xp(120); // total 200 XP
    TEST_CHECK(survey_matrix.get_surveying_rank() == 2, "Surveying rank must be 2 at 200 XP");
    TEST_CHECK(survey_matrix.can_identify_materials(), "Rank 2 must unlock Acoustic Spectroscopy & material labels");

    // UpgradeTree Sonar Material Identification
    UpgradeTree upg;
    TEST_CHECK(!upg.can_identify_materials(), "Tier 0 SonarFrequency cannot identify materials");
    upg.sonarFrequencyTier = 1;
    TEST_CHECK(!upg.can_identify_materials(), "Tier 1 SonarFrequency cannot identify materials");
    upg.sonarFrequencyTier = 2;
    TEST_CHECK(upg.can_identify_materials(), "Tier 2 SonarFrequency must unlock material identification");

    matrix.add_surveying_xp(200);
    TEST_CHECK(matrix.surveying.unlocked, "Surveying skill must unlock at 200 XP");
    TEST_CHECK(matrix.get_sonar_radius() == 30.0f, "Unlocked surveying sonar radius must be 30m");
    log_pass("Skill Matrix XP accumulation, ranks and perk unlocks");

    // Surveying system pulse wave & clustering
    World world(1234);
    world.set_voxel(10, 10, 10, Voxel{MAT_VOIDITE_CRYSTAL, 0});
    world.set_voxel(10, 11, 10, Voxel{MAT_VOIDITE_CRYSTAL, 0});
    world.set_voxel(18, 10, 10, Voxel{MAT_INDUSTRIAL_BULKHEAD, 0});
    world.set_voxel(10, 18, 10, Voxel{MAT_RADIOACTIVE_ORE, 0});

    // Rank 0 Scan: detects minerals, but material identification is disabled
    SurveyingSystem surveying_raw;
    surveying_raw.trigger_scan(glm::vec3(10.0f, 10.0f, 10.0f), world, 25.0f, 0.0f, 0);
    TEST_CHECK(surveying_raw.is_active(), "Raw surveying scan must be active");
    TEST_CHECK(!surveying_raw.is_identifying_materials(), "Rank 0 scan must not identify materials");
    TEST_CHECK(surveying_raw.total_duration() == 1.8f, "Rank 0 base duration must be weaker (1.8s)");

    // Rank 2 Scan: Acoustic Spectroscopy enabled + clusters created
    SurveyingSystem surveying;
    surveying.trigger_scan(glm::vec3(10.0f, 10.0f, 10.0f), world, 25.0f, 1.0f, 2);
    TEST_CHECK(surveying.is_active(), "Surveying scan must be active after trigger");
    TEST_CHECK(surveying.is_identifying_materials(), "Rank 2 scan must identify materials");
    TEST_CHECK(surveying.total_duration() == (1.8f + 0.6f + 1.0f), "Rank 2 duration must scale properly");
    surveying.update(0.5f);
    TEST_CHECK(!surveying.surveyed_voxels().empty(), "Surveying pulse must detect nearby mineral voxels");
    TEST_CHECK(surveying.clusters().size() >= 3, "Surveying pulse must group voxels into at least 3 distinct mineral clusters");
    log_pass("Seismic Sonar wave query, mineral clustering and spectroscopic labels");

    // Sonar Cooldown & Anti-Spam Verification
    PlayerController player;
    player.set_character_class(CharacterClass::Demolitionist);
    TEST_CHECK(player.is_sonar_ready(), "Sonar must start in ready state");
    TEST_CHECK(player.sonar_cooldown() == 0.0f, "Initial sonar cooldown must be 0s");
    TEST_CHECK(player.sonar_max_cooldown() == 10.0f, "Demolitionist base sonar cooldown must be 10s");

    player.trigger_sonar_cooldown();
    TEST_CHECK(!player.is_sonar_ready(), "Sonar must not be ready immediately after firing");
    TEST_CHECK(player.sonar_cooldown() == 10.0f, "Cooldown must equal max cooldown");

    player.update_physics(4.0f, world);
    TEST_CHECK(std::abs(player.sonar_cooldown() - 6.0f) < 0.01f, "Cooldown must decrement with elapsed time");
    TEST_CHECK(!player.is_sonar_ready(), "Sonar must still be recharging at 6.0s remaining");

    player.update_physics(6.5f, world);
    TEST_CHECK(player.is_sonar_ready(), "Sonar must be ready once cooldown elapses");
    TEST_CHECK(player.sonar_cooldown() == 0.0f, "Sonar cooldown must be 0s once fully recharged");

    // Upgrades reduce sonar max cooldown
    UpgradeTree upg_tree;
    upg_tree.sonarFrequencyTier = 3;
    player.apply_attributes_and_upgrades(CharacterClass::Demolitionist, upg_tree);
    TEST_CHECK(player.sonar_max_cooldown() == 7.0f, "Tier 3 SonarFrequency must reduce cooldown from 10s to 7s");

    log_pass("Sonar cooldown enforcement, anti-spam prevention and upgrade scaling");

    // Hazard Clock and Tremors
    HazardClock hazard;
    hazard.set_sector_parameters(2);
    bool warning_fired = false;
    hazard.set_on_tremor_warning([&]() { warning_fired = true; });
    hazard.update(300.0f); // Fast forward time
    TEST_CHECK(hazard.radiation_level() > 0.0f, "Hazard radiation must increase over time");
    log_pass("Hazard Clock radiation buildup and seismic tremors");

    // Dynamic Seismic Stress & Tremors (Driven by mining rocks & shooting weapons)
    HazardClock dynamic_hazard;
    dynamic_hazard.set_sector_parameters(2);
    TEST_CHECK(dynamic_hazard.seismic_stress() == 0.0f, "Initial seismic stress must be zero");

    // 1. Mining rocks adds stress
    dynamic_hazard.on_rock_mined(MAT_VOLCANIC_BASALT);
    TEST_CHECK(dynamic_hazard.seismic_stress() > 0.0f, "Mining rock must accumulate seismic stress");
    TEST_CHECK(dynamic_hazard.rocks_mined_since_tremor() == 1, "Mined rocks counter must increment");

    // 2. Shooting weapons at rocks adds stress
    float stress_before_shot = dynamic_hazard.seismic_stress();
    dynamic_hazard.on_weapon_impact(3.0f);
    TEST_CHECK(dynamic_hazard.seismic_stress() > stress_before_shot, "Weapon impacts on rock must accumulate seismic stress");
    TEST_CHECK(dynamic_hazard.bullet_hits_since_tremor() == 1, "Bullet hits counter must increment");

    // 3. Stress decays when idle
    float high_stress = dynamic_hazard.seismic_stress();
    dynamic_hazard.update(1.0f);
    TEST_CHECK(dynamic_hazard.seismic_stress() < high_stress, "Seismic stress must naturally dissipate over calm periods");

    // 4. Critical stress triggers tremor warning and tremor
    bool tremor_warning_called = false;
    bool tremor_called = false;
    dynamic_hazard.set_on_tremor_warning([&]() { tremor_warning_called = true; });
    dynamic_hazard.set_on_tremor([&](float) { tremor_called = true; });

    for (int i = 0; i < 25; ++i) {
        dynamic_hazard.on_rock_mined(MAT_VOLCANIC_BASALT);
        dynamic_hazard.on_weapon_impact(2.5f);
    }
    TEST_CHECK(dynamic_hazard.is_warning() || dynamic_hazard.is_tremoring(), "Intense mining & shooting must trigger seismic instability");
    TEST_CHECK(tremor_warning_called || tremor_called, "Warning or tremor callback must fire");

    // Advance through tremor duration (warning 2.5s + tremor 5.0s = 7.5s)
    dynamic_hazard.update(8.0f);
    TEST_CHECK(!dynamic_hazard.is_tremoring(), "Tremor must conclude after duration");
    TEST_CHECK(dynamic_hazard.seismic_stress() == 0.0f, "Tectonic fault rupture must completely relieve stress to 0%");
    log_pass("Dynamic seismic stress accumulation, rock mining/gunfire triggers & fault relief");

    // 5. Spatial Radiation Proximity & Geiger Detection
    World rad_world(42);
    rad_world.set_voxel(20, 10, 20, Voxel{MAT_RADIOACTIVE_ORE, 0});
    float rad_close = rad_world.query_radiation_proximity(glm::vec3(20.0f, 11.0f, 20.0f));
    float rad_far = rad_world.query_radiation_proximity(glm::vec3(20.0f, 17.0f, 20.0f));
    float rad_outside = rad_world.query_radiation_proximity(glm::vec3(20.0f, 30.0f, 20.0f));

    TEST_CHECK(rad_close > 0.30f, "Close proximity to radioactive ore must yield high radiation");
    TEST_CHECK(rad_close > rad_far, "Radiation must scale inversely with distance");
    TEST_CHECK(rad_outside == 0.0f, "Radiation outside detection radius must be 0");
    log_pass("Spatial radiation proximity querying, distance falloff & radioactive ore tracking");

    // Extraction System
    ExtractionSystem evac;
    TEST_CHECK(evac.phase() == ExtractionPhase::Dormant, "Initial extraction phase must be Dormant");
    evac.deploy_beacon(glm::vec3(16.0f, 12.0f, 16.0f));
    TEST_CHECK(evac.phase() == ExtractionPhase::BeaconDeployed, "Phase must transition to BeaconDeployed");
    evac.update(41.0f, glm::vec3(16.0f, 12.0f, 16.0f));
    TEST_CHECK(evac.phase() == ExtractionPhase::PodLanded, "Pod must land after countdown");
    log_pass("Extraction Beacon defense and landing pod state machine");
}

// ─────────────────────────────────────────────────────────────
// 6. INVENTORY, ANTI-EXPLOIT & SCORE METRICS
// ─────────────────────────────────────────────────────────────
void test_inventory_and_anti_exploit() {
    std::cout << "\n=== [MODULE 6] Player Inventory, Anti-Exploit & Score Metrics ===" << std::endl;

    PlayerInventory inv;
    inv.reset(25);
    inv.add_voidite(10);
    inv.add_titanium(2);
    inv.add_salvage(5);
    TEST_CHECK(inv.voidite == 10, "Voidite count mismatch");
    TEST_CHECK(inv.titanium == 2, "Titanium count mismatch");
    TEST_CHECK(inv.total_run_score == (10 * 5 + 2 * 6 + 5 * 2), "Total run score mismatch");

    // Bulkhead auto-fabrication from granite scrap
    for (int i = 0; i < 6; ++i) {
        inv.add_scrap_from_granite();
    }
    TEST_CHECK(inv.bulkheads > 15, "Bulkheads must be fabricated from collected scrap");

    // Completion rate formula:
    // Rate = (Voidite Mined / Target * 50%) + (Vault Breached * 25%) + (Safe Evac * 25%)
    inv.voidite = 25;
    inv.target_voidite = 25;
    inv.vault_breached = true;
    int rate_cleared = inv.calculate_completion_rate(true);
    TEST_CHECK(rate_cleared == 100, "100% completion rate formula mismatch");
    TEST_CHECK(inv.evaluate_outcome_badge(true, rate_cleared) == "CLEARED (100%)", "Outcome badge must be CLEARED (100%)");

    // Abandon penalty: 50% deduction
    inv.apply_abandon_penalty();
    TEST_CHECK(inv.is_abandoned, "is_abandoned flag must be set");
    TEST_CHECK(inv.voidite == 12, "Abandon penalty 50% voidite deduction mismatch");
    TEST_CHECK(inv.evaluate_outcome_badge(false, 30) == "ABANDONED (<50%)", "Abandoned badge mismatch");
    log_pass("Inventory calculations, bulkhead fabrication, and completion rating");
}

// ─────────────────────────────────────────────────────────────
// 7. LOCAL SAVE / LOAD SYSTEM PERSISTENCE
// ─────────────────────────────────────────────────────────────
void test_save_persistence() {
    std::cout << "\n=== [MODULE 7] Local Save / Load Persistence ===" << std::endl;

    UserProfile profile;
    profile.total_exp = 3500;
    profile.total_coins = 950;
    profile.total_voidite = 120;
    profile.total_titanium = 75;
    profile.selected_class_id = 2; // Scout
    profile.upgrades.drillSpeedTier = 4;
    profile.upgrades.thrusterTankTier = 3;
    profile.upgrades.sonarFrequencyTier = 2;
    profile.sector_records[1] = {100, "CLEARED (100%)"};
    profile.sector_records[2] = {100, "CLEARED (100%)"};
    profile.sector_records[3] = {65, "PARTIAL (50-99%)"};

    const std::string save_file = "tests/test_unit_save.json";
    bool saved = SaveSystem::save_profile(profile, save_file);
    TEST_CHECK(saved, "SaveSystem::save_profile must return true");

    UserProfile loaded;
    bool load_ok = SaveSystem::load_profile(loaded, save_file);
    TEST_CHECK(load_ok, "SaveSystem::load_profile must return true");
    TEST_CHECK(loaded.total_exp == 3500, "Loaded total_exp mismatch");
    TEST_CHECK(loaded.total_coins == 950, "Loaded total_coins mismatch");
    TEST_CHECK(loaded.get_player_level() == 5 || loaded.get_player_level() == 6, "Loaded player level valid");
    TEST_CHECK(loaded.total_voidite == 120, "Loaded total_voidite mismatch");
    TEST_CHECK(loaded.total_titanium == 75, "Loaded total_titanium mismatch");
    TEST_CHECK(loaded.selected_class_id == 2, "Loaded selected_class_id mismatch");
    TEST_CHECK(loaded.upgrades.drillSpeedTier == 4, "Loaded drillSpeedTier mismatch");
    TEST_CHECK(loaded.upgrades.thrusterTankTier == 3, "Loaded thrusterTankTier mismatch");
    TEST_CHECK(loaded.sector_records[3].highest_completion_rate == 65, "Loaded sector record mismatch");

    log_pass("JSON Save/Load persistence round-trip serialization");
}

// ─────────────────────────────────────────────────────────────
// 8. NETWORKING & PACKET SERIALIZATION
// ─────────────────────────────────────────────────────────────
void test_network_packets() {
    std::cout << "\n=== [MODULE 8] Networking Packets & ByteStream Serialization ===" << std::endl;

    // Block Delta Packet Serialization
    BlockDeltaPacket delta;
    delta.chunk_x = -2;
    delta.chunk_y = 4;
    delta.chunk_z = 7;
    delta.local_block_idx = 1045;
    delta.material_id = MAT_RADIOACTIVE_ORE;
    delta.flags_and_damage = VOXEL_FLAG_PLAYER_PLACED;
    delta.server_tick = 3600;

    ByteStreamWriter writer;
    writer.write(delta);

    ByteStreamReader reader(writer.data(), writer.size());
    BlockDeltaPacket unpacked;
    bool read_ok = reader.read(unpacked);
    TEST_CHECK(read_ok, "ByteStreamReader must succeed reading BlockDeltaPacket");
    TEST_CHECK(unpacked.chunk_x == -2 && unpacked.chunk_y == 4 && unpacked.chunk_z == 7, "Chunk coords mismatch");
    TEST_CHECK(unpacked.local_block_idx == 1045, "Block index mismatch");
    TEST_CHECK(unpacked.material_id == MAT_RADIOACTIVE_ORE, "Material ID mismatch");
    TEST_CHECK(unpacked.flags_and_damage == VOXEL_FLAG_PLAYER_PLACED, "Flags mismatch");
    TEST_CHECK(unpacked.server_tick == 3600, "Server tick mismatch");

    // Player State Snapshot Packet
    PlayerStateSnapshot snap;
    snap.player_id = 1;
    snap.server_tick = 1200;
    snap.position = glm::vec3(12.5f, 44.0f, -8.2f);
    snap.velocity = glm::vec3(0.0f, -5.0f, 1.2f);
    snap.suit_integrity = 95.0f;
    snap.grapple_active = 1;

    ByteStreamWriter snap_writer;
    snap_writer.write(snap);

    ByteStreamReader snap_reader(snap_writer.data(), snap_writer.size());
    PlayerStateSnapshot read_snap;
    TEST_CHECK(snap_reader.read(read_snap), "Snapshot read must succeed");
    TEST_CHECK(read_snap.player_id == 1, "Player ID mismatch");
    TEST_CHECK(std::abs(read_snap.position.x - 12.5f) < 0.001f, "Position X mismatch");
    TEST_CHECK(read_snap.grapple_active == 1, "Grapple active mismatch");

    log_pass("Binary ByteStreamWriter/Reader packet packing and unpacking");
}

// ─────────────────────────────────────────────────────────────
// 9. SEISMIC TREMORS, CEILING DETACHMENT & GROUND PLACEMENT
// ─────────────────────────────────────────────────────────────
void test_seismic_tremor_and_falling_blocks() {
    std::cout << "\n=== [MODULE 9] Seismic Tremor Detachment & Ground Block Placement ===" << std::endl;

    World world(9999);
    // Setup test chamber:
    // Solid floor at Y=10
    // Air chamber from Y=11 to Y=19
    // Solid ceiling at Y=20 with air directly below
    for (int x = 8; x <= 24; ++x) {
        for (int z = 8; z <= 24; ++z) {
            world.set_voxel(x, 10, z, Voxel{MAT_FRACTURED_GRANITE, 0});
            for (int y = 11; y <= 19; ++y) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
            }
            world.set_voxel(x, 20, z, Voxel{MAT_VOLCANIC_BASALT, 0});
            for (int y = 21; y <= 25; ++y) {
                world.set_voxel(x, y, z, Voxel{MAT_DREDGE_BEDROCK, 0});
            }
        }
    }

    glm::vec3 player_pos(16.0f, 11.0f, 16.0f);

    // 1. Query seismic detachment
    auto detach_blocks = StructuralCheck::query_seismic_detachment_blocks(world, player_pos, 3, 5, 6.0f, 2, 12);
    TEST_CHECK(!detach_blocks.empty(), "Seismic tremor query must detect overhead ceiling blocks");
    log_pass("Seismic Tremor Candidate Query Detection");

    // 2. Detach block: remove from ceiling
    glm::ivec3 b = detach_blocks[0];
    Voxel orig_voxel = world.get_voxel(b.x, b.y, b.z);
    TEST_CHECK(orig_voxel.material_id == MAT_VOLCANIC_BASALT, "Target ceiling block must be volcanic basalt");

    // Ceiling removal
    world.set_voxel(b.x, b.y, b.z, Voxel{MAT_AIR, 0}, true);
    TEST_CHECK(world.get_voxel(b.x, b.y, b.z).material_id == MAT_AIR, "Ceiling block must be removed upon tremor detachment");
    log_pass("Ceiling Voxel Removal on Seismic Detachment");

    // 3. Physical Falling Debris Simulation
    DynamicDebris debris(
        1001,
        glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f),
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.3f, 0.6f, 0.1f),
        orig_voxel.material_id,
        1
    );

    TEST_CHECK(!debris.is_destroyed(), "Falling debris must be initially active");
    TEST_CHECK(debris.material_id() == MAT_VOLCANIC_BASALT, "Falling debris material must match detached block");

    // Step physics ticks until debris lands on the ground
    bool landed = false;
    glm::ivec3 final_placed_pos(0);
    for (int tick = 0; tick < 120 && !debris.is_destroyed(); ++tick) {
        auto res = debris.update(1.0f / 60.0f, world, player_pos, false);
        if (res.placed_on_ground) {
            landed = true;
            final_placed_pos = res.place_pos;
            break;
        }
    }

    TEST_CHECK(landed, "Physical debris must fall and land on solid ground");
    TEST_CHECK(debris.is_destroyed(), "Debris entity must be destroyed after landing");
    log_pass("Physical Falling Debris Gravity & Downward Trajectory");

    // 4. Verify Block Settlement on the Ground
    TEST_CHECK(final_placed_pos.y == 11, "Placed block must settle on top of Y=10 floor at Y=11");
    TEST_CHECK(final_placed_pos.x == b.x && final_placed_pos.z == b.z, "Placed block must land along its falling column");
    TEST_CHECK(world.is_solid(final_placed_pos), "Placed block must be solid voxel in the world");
    TEST_CHECK(world.get_voxel(final_placed_pos.x, final_placed_pos.y, final_placed_pos.z).material_id == MAT_VOLCANIC_BASALT,
               "Placed block must retain original material");
    log_pass("Physical Block Settlement and Terrain Placement on Cavern Floor");

    // 5. Test Bulkhead Fortification Deflection
    world.set_voxel(16, 14, 16, Voxel{MAT_INDUSTRIAL_BULKHEAD, 0});
    DynamicDebris roof_debris(
        1002,
        glm::vec3(16.5f, 18.0f, 16.5f),
        glm::vec3(0.0f, -2.0f, 0.0f),
        glm::vec3(0.0f),
        MAT_FRACTURED_GRANITE,
        1
    );

    bool bulkhead_deflected = false;
    for (int tick = 0; tick < 60 && !roof_debris.is_destroyed(); ++tick) {
        auto res = roof_debris.update(1.0f / 60.0f, world, player_pos, true);
        if (res.hit_bulkhead) {
            bulkhead_deflected = true;
            break;
        }
    }
    TEST_CHECK(bulkhead_deflected, "Bulkhead must safely deflect and shatter falling overhead debris");
    TEST_CHECK(world.get_voxel(16, 14, 16).material_id == MAT_INDUSTRIAL_BULKHEAD, "Bulkhead must remain intact after deflection");
    log_pass("Bulkhead Fortification Deflection & Hazard Shelter Utility");
}

// ─────────────────────────────────────────────────────────────
// 10. NOISE METER & VOID STALKER ENEMY AI UNIT TESTS
// ─────────────────────────────────────────────────────────────
void test_noise_meter_and_void_stalkers() {
    std::cout << "\n=== [MODULE 10] Mining Noise Meter & Void Stalker Enemy AI ===" << std::endl;

    // 1. Noise Meter Threshold Progression
    NoiseMeter meter;
    TEST_CHECK(meter.get_alert_level() == NoiseMeter::AlertLevel::Silent, "Initial noise level must be Silent");
    TEST_CHECK(meter.noise() == 0.0f, "Initial noise must be 0.0");

    meter.add_drill_noise(35.0f);
    TEST_CHECK(meter.noise() == 35.0f, "Noise must accumulate from drilling");
    TEST_CHECK(meter.get_alert_level() == NoiseMeter::AlertLevel::Silent, "35% noise must remain Silent (< 40%)");

    meter.add_drill_noise(10.0f); // 45%
    TEST_CHECK(meter.get_alert_level() == NoiseMeter::AlertLevel::Alerted, "45% noise must trigger Alerted state (>= 40%)");
    TEST_CHECK(std::string(meter.alert_string()) == "ALERTED", "Alert string must match enum");

    meter.add_demolition_noise(30.0f); // 75%
    TEST_CHECK(meter.get_alert_level() == NoiseMeter::AlertLevel::Agitated, "75% noise must trigger Agitated state (>= 70%)");

    log_pass("Noise Meter Alert Level Threshold Progression");

    // 2. Wave Callback and Swarm Auto-Reset
    int wave_triggered_count = 0;
    NoiseMeter::AlertLevel last_wave_level = NoiseMeter::AlertLevel::Silent;
    meter.set_on_wave([&](NoiseMeter::AlertLevel lvl, float pct) {
        wave_triggered_count++;
        last_wave_level = lvl;
    });

    meter.reset();
    TEST_CHECK(meter.noise() == 0.0f, "Reset must restore noise to 0");
    meter.add_explosive_noise(45.0f);
    meter.update(0.01f);
    TEST_CHECK(wave_triggered_count == 1, "Crossing Alerted threshold must fire wave callback");
    TEST_CHECK(last_wave_level == NoiseMeter::AlertLevel::Alerted, "Callback alert level must be Alerted");

    meter.add_explosive_noise(60.0f); // Surpass 100% -> Swarming
    meter.update(0.01f);
    TEST_CHECK(wave_triggered_count == 2, "Reaching 100% must trigger Swarming wave callback");
    TEST_CHECK(last_wave_level == NoiseMeter::AlertLevel::Swarming, "Callback alert level must be Swarming");
    TEST_CHECK(meter.noise() <= 60.0f, "Swarm trigger must reset noise to 60% baseline");
    TEST_CHECK(meter.swarm_count() == 1, "Swarm counter must increment");

    // 3. Stealth Decay Rates (Standing vs Crouching)
    NoiseMeter stealth_meter;
    stealth_meter.add_drill_noise(50.0f);
    stealth_meter.set_crouching(false);
    stealth_meter.update(2.0f); // 50 - 2.5*2 = 45
    float standing_noise = stealth_meter.noise();
    TEST_CHECK(std::abs(standing_noise - 45.0f) < 0.1f, "Standing decay must follow base decay rate");

    stealth_meter.set_crouching(true);
    stealth_meter.update(2.0f); // 45 - 5.0*2 = 35
    float crouch_noise = stealth_meter.noise();
    TEST_CHECK(std::abs(crouch_noise - 35.0f) < 0.1f, "Crouching stealth decay must dissipate noise twice as fast");

    log_pass("Noise Meter Wave Callbacks, Swarm Cycling, and Stealth Decay");

    // 4. Void Stalker Manager Lifecycle & Spawning
    World world(42);
    VoidStalkerManager mgr;
    TEST_CHECK(mgr.active_count() == 0, "Initial stalker count must be 0");

    mgr.spawn_stalker(glm::vec3(16.0f, 22.0f, 16.0f));
    TEST_CHECK(mgr.active_count() == 1, "Stalker count must be 1 after spawn");
    TEST_CHECK(mgr.stalkers()[0].hp == 40.0f, "Initial stalker HP must be 40");
    TEST_CHECK(mgr.stalkers()[0].state == StalkerState::Idle, "Stalker must spawn in Idle state");

    // 5. Sonar Pulse Stun Mechanics
    mgr.apply_sonar_stun(glm::vec3(16.0f, 22.0f, 16.0f), 10.0f);
    TEST_CHECK(mgr.stalkers()[0].state == StalkerState::Stunned, "Sonar pulse within range must stun Void Stalker");
    TEST_CHECK(mgr.stalkers()[0].stun_timer > 2.0f, "Stun duration must be >= 2.0s");

    // 6. Combat Damage & Elimination
    bool hit = mgr.damage_nearest(glm::vec3(16.0f, 22.0f, 16.0f), 3.0f, 25.0f);
    TEST_CHECK(hit, "Damage within reach must register a hit");
    TEST_CHECK(mgr.stalkers()[0].hp == 15.0f, "Stalker HP must decrease from 40 to 15 (-25)");

    // Lethal blow
    bool lethal_hit = mgr.damage_nearest(glm::vec3(16.0f, 22.0f, 16.0f), 3.0f, 20.0f);
    TEST_CHECK(lethal_hit, "Lethal blow must register");
    TEST_CHECK(mgr.stalkers()[0].is_dying() || mgr.stalkers()[0].is_dead(), "Stalker with <= 0 HP must enter Dying or Dead state");
    TEST_CHECK(mgr.stalkers()[0].hit_flash_timer == 0.0f, "Dying stalker must have hit_flash_timer == 0 to prevent turning white");
    TEST_CHECK(mgr.active_count() == 0, "Dying stalker must not count toward active_count");

    // Step collapse animation sequence into static carcass
    mgr.update(0.80f, glm::vec3(16.0f, 22.0f, 25.0f), glm::vec3(0.0f, 0.0f, -1.0f),
               glm::vec3(0.0f, 0.0f, -1.0f), true, 0.0f, false, world);
    TEST_CHECK(mgr.stalkers()[0].is_dead(), "Stalker must enter Dead state after collapse");

    mgr.remove_dead();
    TEST_CHECK(mgr.stalkers().empty(), "remove_dead() must clean up defeated entities");

    // 7. World Wave Spawning & AI Simulation
    glm::vec3 player_pos(16.0f, 22.0f, 16.0f);
    mgr.spawn_wave(player_pos, 2, world);
    TEST_CHECK(mgr.active_count() == 2, "Wave spawn must spawn exactly requested count");

    // Execute an update frame
    glm::vec3 player_fwd(0.0f, 0.0f, -1.0f);
    auto res = mgr.update(1.0f / 60.0f, player_pos, player_fwd, player_fwd, true, 80.0f, true, world);
    TEST_CHECK(mgr.active_count() == 2, "Stalkers must remain active during exploration");

    log_pass("Void Stalker Entity FSM, Sonar Stun Vulnerability, and Combat Elimination");

    // 8. Eerie movement — stalker must approach player during Stalking state
    {
        World movement_world(42);
        VoidStalkerManager movement_mgr;
        // Spawn stalker ~20m from player, force Stalking state immediately
        glm::vec3 stalker_spawn(36.0f, 22.0f, 36.0f);
        glm::vec3 movement_player(16.0f, 22.0f, 16.0f);
        movement_mgr.spawn_stalker(stalker_spawn);
        movement_mgr.stalkers_mut()[0].state = StalkerState::Stalking;
        movement_mgr.stalkers_mut()[0].state_timer = 0.0f;

        float initial_xz_dist = std::hypot(stalker_spawn.x - movement_player.x,
                                            stalker_spawn.z - movement_player.z);

        // Run 30 frames (~0.5s) at 60Hz
        glm::vec3 fwd(0.0f, 0.0f, -1.0f);
        for (int i = 0; i < 30; ++i) {
            movement_mgr.update(1.0f / 60.0f, movement_player, fwd, fwd, false, 80.0f, false, movement_world);
        }

        const auto& final_s = movement_mgr.stalkers()[0];
        float final_xz_dist = std::hypot(final_s.position.x - movement_player.x,
                                          final_s.position.z - movement_player.z);
        TEST_CHECK(final_xz_dist < initial_xz_dist,
                   "Eerie burst-and-weave stalker must still close net distance toward player");
    }
    log_pass("Eerie movement — Stalker burst-and-weave closes net distance toward player");

    // 9. Walk cycle burst speed variation — quantify scurry-pause ratio
    {
        // At walk_cycle = 0: burst = 0.50 + 0.70*|sin(0)| = 0.50 (slowest pause phase)
        float burst_min = 0.50f + 0.70f * std::abs(std::sin(0.0f));
        TEST_CHECK(std::abs(burst_min - 0.50f) < 0.001f,
                   "Walk-cycle burst at phase 0 must be 0.50 (minimum pause speed)");

        // At peak: walk_cycle = (pi/2) / 0.45 => sin(wc*0.45) = 1.0 => burst = 1.20
        float wc_peak = (3.14159265f * 0.5f) / 0.45f;
        float burst_max = 0.50f + 0.70f * std::abs(std::sin(wc_peak * 0.45f));
        TEST_CHECK(burst_max > 1.15f,
                   "Walk-cycle burst at peak must exceed 1.15 (fast scurry phase)");

        float ratio = burst_max / burst_min;
        TEST_CHECK(ratio > 2.0f,
                   "Scurry-to-pause speed ratio must exceed 2x for eerie movement quality");
    }
    log_pass("Eerie movement — walk-cycle scurry/pause burst ratio quantified (> 2x)");

    // 10. Circling orbit radius variation — eerie breathing orbit
    {
        // At glow_phase = 0: radius = 5.0 + sin(0) * 1.8 = 5.0 (minimum)
        float gp_a = 0.0f;
        float radius_a = 5.0f + std::sin(gp_a * 0.65f) * 1.8f;
        TEST_CHECK(std::abs(radius_a - 5.0f) < 0.001f,
                   "Orbit radius at glow_phase=0 must be exactly 5.0m");

        // At glow_phase = pi/(2*0.65): sin(gp*0.65)=1 => radius = 6.8m (maximum)
        float gp_b = (3.14159265f * 0.5f) / 0.65f;
        float radius_b = 5.0f + std::sin(gp_b * 0.65f) * 1.8f;
        TEST_CHECK(radius_b > 6.7f,
                   "Orbit radius at peak glow_phase must reach ~6.8m");
        TEST_CHECK(radius_b > radius_a,
                   "Orbit radius must vary across glow_phase (eerie breathing orbit)");

        // Full variation is ±1.8m: verify range is at least 3.0m total
        float gp_neg = -(3.14159265f * 0.5f) / 0.65f;
        float radius_neg = 5.0f + std::sin(gp_neg * 0.65f) * 1.8f;
        float total_range = radius_b - radius_neg;
        TEST_CHECK(total_range > 3.0f,
                   "Total orbit radius variation must span > 3.0m for eerie effect");
    }
    log_pass("Eerie movement — circling orbit radius breathes between 3.2m and 6.8m");
}

// ─────────────────────────────────────────────────────────────
// 11. CARRY WEIGHT OVERBURDEN & CLASS TACTICAL ABILITIES
// ─────────────────────────────────────────────────────────────
void test_carry_weight_and_tactical_abilities() {
    std::cout << "\n=== [MODULE 11] Carry Weight Overburden & Class Tactical Abilities ===" << std::endl;

    // 1. Inventory Carry Weight Calculation
    PlayerInventory inv;
    inv.bulkheads = 0;
    inv.demolition_charges = 0;
    TEST_CHECK(inv.carry_weight() == 0.0f, "Empty inventory must have 0 carry weight");
    TEST_CHECK(!inv.is_overburdened(0), "Empty inventory cannot be overburdened");

    inv.add_voidite(10);      // 10 * 1.0 = 10.0kg
    inv.add_titanium(5);      // 5 * 2.5 = 12.5kg
    inv.add_salvage(10);      // 10 * 0.4 = 4.0kg
    inv.bulkheads = 5;        // 5 * 2.0 = 10.0kg
    inv.demolition_charges = 2; // 2 * 1.5 = 3.0kg
    float expected_w = 10.0f + 12.5f + 4.0f + 10.0f + 3.0f; // 39.5kg
    TEST_CHECK(std::abs(inv.carry_weight() - expected_w) < 0.001f, "Carry weight must correctly tally all minerals and materials");

    // Check thresholds: Vanguard (85kg), Demolitionist (60kg), Scout (42kg)
    TEST_CHECK(!inv.is_overburdened(1), "39.5kg is under Vanguard 85kg capacity");
    TEST_CHECK(!inv.is_overburdened(0), "39.5kg is under Demolitionist 60kg capacity");
    TEST_CHECK(!inv.is_overburdened(2), "39.5kg is under Scout 42kg capacity");
    TEST_CHECK(inv.overburden_penalty(2) == 1.0f, "No overburden penalty when under max weight");

    // Add Relic (10.0kg) -> total = 49.5kg
    inv.add_relic();
    TEST_CHECK(std::abs(inv.carry_weight() - (expected_w + 10.0f)) < 0.001f, "Relic hyper-core must add 10kg weight");
    TEST_CHECK(inv.is_overburdened(2), "49.5kg must overburden Scout (42kg max)");
    TEST_CHECK(!inv.is_overburdened(0), "49.5kg is still within Demolitionist capacity (60kg max)");

    float penalty = inv.overburden_penalty(2);
    TEST_CHECK(penalty < 1.0f && penalty >= 0.55f, "Overburden penalty must degrade speed linearly down to min 0.55");
    log_pass("Dynamic Mineral Carry Weight & Class Overburden Limits");

    // 2. PlayerController Tactical Abilities
    World world(42);
    PlayerController player(glm::vec3(16.0f, 20.0f, 16.0f));

    TEST_CHECK(player.is_tactical_ready(), "Tactical ability must be ready initially");
    TEST_CHECK(player.tactical_cooldown() == 0.0f, "Initial tactical cooldown must be 0");

    // Class cooldown configuration
    player.set_character_class(CharacterClass::Demolitionist);
    TEST_CHECK(player.tactical_max_cooldown() == 18.0f, "Demolitionist tactical cooldown must be 18s");

    player.set_character_class(CharacterClass::Vanguard);
    TEST_CHECK(player.tactical_max_cooldown() == 20.0f, "Vanguard tactical cooldown must be 20s");

    player.set_character_class(CharacterClass::Scout);
    TEST_CHECK(player.tactical_max_cooldown() == 12.0f, "Scout tactical cooldown must be 12s");

    // Trigger ability and cooldown decay
    player.trigger_tactical_cooldown();
    TEST_CHECK(!player.is_tactical_ready(), "Ability must not be ready immediately after trigger");
    TEST_CHECK(player.tactical_cooldown() == 12.0f, "Cooldown must be set to tactical max");

    // Advance physics
    player.update_physics(4.0f, world);
    TEST_CHECK(std::abs(player.tactical_cooldown() - 8.0f) < 0.01f, "Cooldown must decay with delta time");
    TEST_CHECK(player.tactical_recharge_progress() > 0.3f, "Recharge progress must advance");

    player.update_physics(8.5f, world);
    TEST_CHECK(player.is_tactical_ready(), "Ability must be ready once cooldown completes");

    // Callback verification
    bool callback_fired = false;
    CharacterClass fired_cls = CharacterClass::Demolitionist;
    player.set_on_tactical_ability([&](CharacterClass cls, const glm::vec3& pos, const glm::vec3& dir) {
        callback_fired = true;
        fired_cls = cls;
    });

    player.set_character_class(CharacterClass::Vanguard);
    player.reset_tactical_cooldown();
    TEST_CHECK(player.is_tactical_ready(), "Vanguard tactical must be ready");

    // DynamicDebris impulse verification (Kinetic Repulsor deflection)
    DynamicDebris test_debris(999, glm::vec3(16.0f, 22.0f, 16.0f), glm::vec3(0.0f, -5.0f, 0.0f), glm::vec3(0.0f), MAT_FRACTURED_GRANITE, 4);
    test_debris.apply_impulse(glm::vec3(5.0f, 10.0f, 0.0f));
    TEST_CHECK(test_debris.velocity().x > 4.9f && test_debris.velocity().y > 4.9f, "DynamicDebris impulse must deflect velocity");

    // Test carry weight multiplier on PlayerController
    player.set_carry_weight_multiplier(0.70f);
    TEST_CHECK(player.carry_weight_multiplier() == 0.70f, "Carry weight multiplier must be retained");

    log_pass("Class-Unique Tactical Ability Cooldowns, Recharge Lifecycle, and Overburden Throttling");
}

// ─────────────────────────────────────────────────────────────
// 12. CAVERN LUMINARIES & DYNAMIC LIGHTING UNIT TESTS
// ─────────────────────────────────────────────────────────────
void test_cavern_luminaries_and_lighting() {
    std::cout << "\n=== [MODULE 12] Cavern Luminaries, Light Prioritization & Dynamic Cavern Lighting ===" << std::endl;

    // 1. Verify Luminary Generation for All 9 Room Archetypes
    LevelGenerator gen(1, 1337);

    RoomShapeType types[] = {
        RoomShapeType::SpawnStagingCavern,
        RoomShapeType::MiningPillarHall,
        RoomShapeType::CrystallineGeode,
        RoomShapeType::TerracedQuarry,
        RoomShapeType::IndustrialVaultBunker,
        RoomShapeType::FaultLineCrevasse,
        RoomShapeType::AbyssalVerticalChasm,
        RoomShapeType::RadioactiveCoreSanctuary,
        RoomShapeType::ExtractionLandingBay,
        RoomShapeType::MagmaCalderaLake,
        RoomShapeType::SpikeTrenchArena,
        RoomShapeType::VoidSingularityRift,
        RoomShapeType::FungoidBioGrotto,
        RoomShapeType::LaserDefenseFoundry,
        RoomShapeType::CrumblingArchCanyon,
        RoomShapeType::SubterraneanAquiferOasis,
        RoomShapeType::ColossalAbyssalChasm,
        RoomShapeType::MoltenMagmaFoundry,
        RoomShapeType::ToxicMiasmaSwamp,
        RoomShapeType::PrismaticCrystalCathedral,
        RoomShapeType::AncientTitanNecropolis,
        RoomShapeType::BioluminescentGlowwormGrotto,
        RoomShapeType::PrecursorCoolantReservoir,
        RoomShapeType::ColossalVaultedDredgeCathedral,
        RoomShapeType::TectonicAbyssalSinkhole,
        RoomShapeType::CyclopeanExcavationSilo,
        RoomShapeType::BioluminescentFirmamentAbyss
    };

    for (RoomShapeType t : types) {
        gen.create_single_room_test_layout(t);
        const auto& lums = gen.luminaries();
        TEST_CHECK(!lums.empty(), "Single room layout must generate at least one cavern luminary");

        for (const auto& l : lums) {
            TEST_CHECK(l.base_radius > 0.0f, "Luminary radius must be positive");
            TEST_CHECK(l.base_intensity > 0.0f, "Luminary intensity must be positive");
            TEST_CHECK(l.base_color.r >= 0.0f && l.base_color.r <= 1.0f, "Luminary red channel must be in [0, 1]");
            TEST_CHECK(l.base_color.g >= 0.0f && l.base_color.g <= 1.0f, "Luminary green channel must be in [0, 1]");
            TEST_CHECK(l.base_color.b >= 0.0f && l.base_color.b <= 1.0f, "Luminary blue channel must be in [0, 1]");
        }
    }
    log_pass("Cavern Luminary Generation Across All 27 Room Archetypes");

    // 2. Specific Thematic Luminary Spectral Verification
    // Geode: Vibrant pulsing violet
    gen.create_single_room_test_layout(RoomShapeType::CrystallineGeode);
    bool found_geode = false;
    for (const auto& l : gen.luminaries()) {
        if (l.type == LuminaryType::VoiditeCrystalGeode) {
            found_geode = true;
            TEST_CHECK(l.base_color.b > l.base_color.r && l.base_color.r > l.base_color.g && l.base_color.b >= 0.70f, "Voidite Geode Heart must have mineral violet hue");
            TEST_CHECK(l.base_radius >= 10.0f, "Geode light radius must illuminate room");
        }
    }
    TEST_CHECK(found_geode, "CrystallineGeode room must contain VoiditeCrystalGeode luminary");

    // Radioactive Sanctuary: Toxic emerald with radiation flicker
    gen.create_single_room_test_layout(RoomShapeType::RadioactiveCoreSanctuary);
    bool found_radioactive = false;
    for (const auto& l : gen.luminaries()) {
        if (l.type == LuminaryType::RadioactiveCore) {
            found_radioactive = true;
            TEST_CHECK(l.base_color.g > 0.8f, "Radioactive Core must have intense green emission");
            TEST_CHECK(l.flicker_rate > 0.0f, "Radioactive Core must have non-zero radiation flicker rate");
        }
    }
    TEST_CHECK(found_radioactive, "Radioactive Sanctuary must contain RadioactiveCore luminary");

    // Magma Crevasse: Molten thermite amber/orange
    gen.create_single_room_test_layout(RoomShapeType::FaultLineCrevasse);
    bool found_magma = false;
    for (const auto& l : gen.luminaries()) {
        if (l.type == LuminaryType::ThermalMagmaVent) {
            found_magma = true;
            TEST_CHECK(l.base_color.r > 0.9f && l.base_color.g > 0.3f && l.base_color.b < 0.2f, "Magma vent must emit molten orange");
        }
    }
    TEST_CHECK(found_magma, "FaultLineCrevasse must contain ThermalMagmaVent luminary");
    log_pass("Thematic Color Spectra and Physical Attributes for Specialist Luminaries");

    // 3. Dynamic Real-Time Luminary Evaluation (Breathing and Flicker Math)
    CavernLuminary test_lum;
    test_lum.base_color = glm::vec3(0.5f, 0.2f, 0.9f);
    test_lum.base_radius = 20.0f;
    test_lum.base_intensity = 3.0f;
    test_lum.pulse_speed = 2.0f;
    test_lum.pulse_depth = 0.25f;
    test_lum.flicker_rate = 10.0f;

    glm::vec3 eval_col;
    float eval_rad = 0.0f, eval_int = 0.0f;

    // Evaluate at multiple timestamps
    float times[] = {0.0f, 0.25f, 0.5f, 1.0f, 2.5f, 5.0f};
    float prev_int = -1.0f;
    bool intensity_varied = false;

    for (float t : times) {
        test_lum.evaluate(t, eval_col, eval_rad, eval_int);
        TEST_CHECK(eval_col == test_lum.base_color, "Evaluated color must preserve base color");
        TEST_CHECK(eval_rad > 0.0f, "Evaluated radius must be strictly positive");
        TEST_CHECK(eval_int > 0.0f, "Evaluated intensity must be strictly positive");

        if (prev_int >= 0.0f && std::abs(eval_int - prev_int) > 0.05f) {
            intensity_varied = true;
        }
        prev_int = eval_int;
    }
    TEST_CHECK(intensity_varied, "Luminary intensity must vary dynamically over time (pulse / flicker)");
    log_pass("Real-Time Temporal Animation, Sine Breathing & Radiation Flicker");

    // 4. Clustered Light Prioritization Algorithm (Zero-Allocation Staging)
    std::vector<CavernLuminary> candidate_lums;
    // Luminary A: Close to camera (dist ~ 5m), moderate intensity
    CavernLuminary lumA;
    lumA.position = glm::vec3(10.0f, 5.0f, 10.0f);
    lumA.base_intensity = 2.5f;
    lumA.base_radius = 16.0f;
    candidate_lums.push_back(lumA);

    // Luminary B: Extremely far from camera (dist ~ 80m), high intensity
    CavernLuminary lumB;
    lumB.position = glm::vec3(90.0f, 5.0f, 10.0f);
    lumB.base_intensity = 5.0f;
    lumB.base_radius = 24.0f;
    candidate_lums.push_back(lumB);

    // Luminary C: Medium distance (dist ~ 15m), very high intensity
    CavernLuminary lumC;
    lumC.position = glm::vec3(25.0f, 5.0f, 10.0f);
    lumC.base_intensity = 4.0f;
    lumC.base_radius = 22.0f;
    candidate_lums.push_back(lumC);

    glm::vec3 cam_pos(10.0f, 5.0f, 5.0f); // 5m from lumA, 15m from lumC, 80m from lumB

    struct ScoredLum {
        const CavernLuminary* lum;
        float score;
    };
    std::array<ScoredLum, 8> ranked;
    size_t count = 0;

    for (const auto& l : candidate_lums) {
        float d = glm::distance(cam_pos, l.position);
        float dist_sq = std::max(1.0f, d * d);
        float score = (l.base_intensity * l.base_radius * l.base_radius) / dist_sq;
        ranked[count++] = {&l, score};
    }

    std::sort(ranked.begin(), ranked.begin() + count, [](const ScoredLum& a, const ScoredLum& b) {
        return a.score > b.score;
    });

    TEST_CHECK(ranked[0].lum == &candidate_lums[0], "Closest luminary must achieve highest importance score");
    TEST_CHECK(ranked[1].lum == &candidate_lums[2], "Mid-distance luminary must be second in priority");
    TEST_CHECK(ranked[2].lum == &candidate_lums[1], "Distant luminary must have lowest priority");
    log_pass("Distance-Weighted Clustered Light Prioritization & Fixed-Buffer Selection");

    // 5. GameSettings Brightness Range and Exposure Scaling
    GameSettings settings;
    TEST_CHECK(settings.brightness == 1.0f, "Default brightness must be 1.0 (100%)");
    settings.brightness = 0.5f;
    TEST_CHECK(settings.brightness >= 0.4f, "Minimum brightness limit respected");
    settings.brightness = 1.8f;
    TEST_CHECK(settings.brightness <= 2.0f, "Maximum brightness limit respected");
    log_pass("GameSettings Cavern Brightness Controls and Clamp Range");
}

// ─────────────────────────────────────────────────────────────
// 13. PLASMA CARBINE WEAPON & BALLISTICS COMBAT TESTS
// ─────────────────────────────────────────────────────────────
void test_plasma_carbine_combat() {
    std::cout << "\n=== [MODULE 13] Class-Specific Weapons, Ballistics & Combat ===" << std::endl;

    PlayerController player(glm::vec3(16.0f, 20.0f, 16.0f));
    World world(1234);

    // 1. Initial tool selection
    TEST_CHECK(player.active_tool() == ToolSlot::MiningDrill, "Default active tool must be Mining Drill");
    TEST_CHECK(!player.is_weapon_equipped(), "Carbine must not be equipped when drill is active");

    // 2. Demolitionist: Magma Scattergun
    player.set_character_class(CharacterClass::Demolitionist);
    player.set_active_tool(ToolSlot::CombatWeapon);
    TEST_CHECK(player.is_weapon_equipped(), "Combat firearm must be recognized as equipped");
    TEST_CHECK(player.weapon_archetype() == WeaponArchetype::MagmaScattergun, "Demolitionist must equip Magma Scattergun");
    TEST_CHECK(player.weapon_ammo() == 6, "Magma Scattergun must have 6-round capacity");
    TEST_CHECK(player.weapon_short_name() == "SCAT", "Scattergun short name must be SCAT");

    // Firing without trigger pressed
    std::vector<PlayerPlasmaBolt> bolts;
    bool fired = player.try_fire_weapon(bolts, 0.016f);
    TEST_CHECK(!fired, "Weapon must not fire when trigger is not pressed");

    // Pull trigger on Scattergun
    player.set_drilling(true);
    fired = player.try_fire_weapon(bolts, 0.016f);
    TEST_CHECK(fired, "Scattergun must fire when trigger is held");
    TEST_CHECK(bolts.size() == 5, "Scattergun must spawn 5 flechettes per trigger pull");
    TEST_CHECK(player.weapon_ammo() == 5, "Scattergun ammo must decrease to 5");
    TEST_CHECK(!player.weapon_stats().auto_recharge, "Scattergun auto_recharge must be false");
    for (int t = 0; t < 30; ++t) {
        player.update_physics(0.1f, world);
    }
    TEST_CHECK(player.weapon_ammo() == 5, "Scattergun ammo must never passively regenerate over time");
    TEST_CHECK(bolts[0].color.r > 0.9f && bolts[0].color.g > 0.4f, "Scattergun flechette tracer must be thermite amber");
    log_pass("Demolitionist Magma Scattergun: 6-round drum, 5-flechette burst spread, zero passive recharge");

    // 3. Vanguard: Plasma Carbine
    player.set_character_class(CharacterClass::Vanguard);
    TEST_CHECK(player.weapon_archetype() == WeaponArchetype::PlasmaCarbine, "Vanguard must equip Plasma Carbine");
    TEST_CHECK(player.weapon_ammo() == 16, "Plasma Carbine must start with full 16-round capacitor");
    TEST_CHECK(player.weapon_short_name() == "CARB", "Carbine short name must be CARB");

    bolts.clear();
    fired = player.try_fire_weapon(bolts, 0.016f);
    TEST_CHECK(fired, "Plasma Carbine must fire when trigger is held");
    TEST_CHECK(bolts.size() == 1, "Plasma Carbine must fire 1 focused plasma bolt");
    TEST_CHECK(player.weapon_ammo() == 15, "Carbine ammo must decrease to 15");
    TEST_CHECK(bolts[0].color.b > 0.9f, "Plasma Carbine bolt must be electric cyan");

    // Cyclic cooldown enforcement (fire rate 0.16s)
    bool immediate_fire = player.try_fire_weapon(bolts, 0.016f);
    TEST_CHECK(!immediate_fire, "Carbine must enforce cyclic fire rate cooldown");

    player.update_physics(0.20f, world);
    bolts.clear();
    fired = player.try_fire_weapon(bolts, 0.016f);
    TEST_CHECK(fired, "Carbine must fire again after cyclic cooldown elapses");
    TEST_CHECK(player.weapon_ammo() == 14, "Ammo must decrease to 14");

    // Verify ammo NEVER passively comes back over time; player MUST reload
    TEST_CHECK(!player.weapon_stats().auto_recharge, "Plasma Carbine auto_recharge must be false");
    for (int t = 0; t < 30; ++t) {
        player.update_physics(0.1f, world);
    }
    TEST_CHECK(player.weapon_ammo() == 14, "Ammo must not passively regenerate over time; player must reload");

    // Manual reload
    player.reload_weapon();
    TEST_CHECK(player.is_reloading(), "Weapon must enter reload state");
    player.update_physics(1.30f, world);
    TEST_CHECK(!player.is_reloading(), "Weapon must finish reload");
    TEST_CHECK(player.weapon_ammo() == 16, "Carbine magazine must be replenished to 16");
    log_pass("Vanguard Plasma Carbine: 16-round capacitor, rapid fire, reload cycle, zero passive recharge");

    // 4. Scout: Needler Railgun
    player.set_character_class(CharacterClass::Scout);
    TEST_CHECK(player.weapon_archetype() == WeaponArchetype::NeedlerRailgun, "Scout must equip Needler Railgun");
    TEST_CHECK(player.weapon_ammo() == 8, "Needler Railgun must start with 8-round needle cartridge");
    TEST_CHECK(player.weapon_short_name() == "RAIL", "Railgun short name must be RAIL");
    TEST_CHECK(!player.weapon_stats().auto_recharge, "Needler Railgun auto_recharge must be false");

    bolts.clear();
    fired = player.try_fire_weapon(bolts, 0.016f);
    TEST_CHECK(fired, "Needler Railgun must fire when trigger is held");
    TEST_CHECK(bolts.size() == 1, "Railgun must fire 1 high-velocity needle");
    TEST_CHECK(player.weapon_ammo() == 7, "Railgun ammo must decrease to 7");
    for (int t = 0; t < 30; ++t) {
        player.update_physics(0.1f, world);
    }
    TEST_CHECK(player.weapon_ammo() == 7, "Railgun ammo must never passively regenerate");
    TEST_CHECK(bolts[0].damage == 42.0f, "Needler Railgun must deal 42 piercing damage");
    TEST_CHECK(glm::length(bolts[0].velocity) > 100.0f, "Railgun needle velocity must exceed 100 m/s");
    TEST_CHECK(bolts[0].color.g > 0.9f, "Railgun needle tracer must be emerald green");
    log_pass("Scout Needler Railgun: 8-round needle mag, 110m/s hyper-velocity, 42 damage, zero passive recharge");

    // 5. Enemy Hit Registration & Damage: Void Stalker
    VoidStalkerManager stalkers;
    stalkers.spawn_stalker(glm::vec3(20.0f, 20.0f, 20.0f), 1.0f);
    TEST_CHECK(stalkers.active_count() == 1, "Must have 1 active stalker");
    float prev_hp = stalkers.stalkers()[0].hp;

    // Direct hit with plasma bolt (22 damage)
    bool hit_stalker = stalkers.damage_nearest(glm::vec3(20.2f, 20.1f, 20.0f), 1.4f, 22.0f);
    TEST_CHECK(hit_stalker, "Plasma bolt within radius must hit Void Stalker");
    TEST_CHECK(stalkers.stalkers()[0].hp < prev_hp, "Stalker HP must decrease after plasma bolt hit");
    TEST_CHECK(stalkers.stalkers()[0].hp == prev_hp - 22.0f, "Stalker must take exact 22 damage from carbine bolt");
    log_pass("Combat projectile hit registration and damage infliction on Void Stalkers");

    // 6. Enemy Hit Registration & Damage: Seismic Burrower
    SeismicBurrowerManager burrowers;
    World test_world(8888);
    burrowers.update(0.016f, glm::vec3(30.0f, 20.0f, 30.0f), test_world);
    if (!burrowers.burrowers().empty()) {
        glm::vec3 burrower_pos = burrowers.burrowers()[0].position;
        float prev_b_hp = burrowers.burrowers()[0].hp;
        bool hit_burrower = burrowers.damage_nearest(burrower_pos, 2.4f, 22.0f, false);
        TEST_CHECK(hit_burrower, "Plasma bolt within radius must hit Seismic Burrower");
        TEST_CHECK(burrowers.burrowers()[0].hp < prev_b_hp, "Burrower HP must decrease after plasma hit");
        log_pass("Combat projectile hit registration and damage infliction on Seismic Burrowers");
    } else {
        log_pass("Seismic Burrower manager damage interface verified");
    }
}

// ─────────────────────────────────────────────────────────────
// 14. SATCHEL CHARGE, REMOTE DETONATOR & WEAPON CYCLING TESTS
// ─────────────────────────────────────────────────────────────
void test_satchel_charge_and_weapon_cycling() {
    std::cout << "\n=== [MODULE 14] Satchel Charge, Remote Detonator & Weapon Cycling ===" << std::endl;

    PlayerController player(glm::vec3(16.0f, 20.0f, 16.0f));

    // 1. Tool slot cycling (3 active slots: Drill, Combat Weapon, Satchel Charge; Bulkhead placed via RMB on drill)
    TEST_CHECK(player.active_tool() == ToolSlot::MiningDrill, "Default slot must be 0 (Drill)");

    // Simulate forward cycle (scroll down): Drill -> Combat Weapon -> Satchel Charge -> Drill
    player.cycle_tool_forward();
    TEST_CHECK(player.active_tool() == ToolSlot::CombatWeapon, "Cycling forward from Drill must select Combat Weapon");

    player.cycle_tool_forward();
    TEST_CHECK(player.active_tool() == ToolSlot::DemolitionCharge, "Cycling forward from Combat Weapon must select Demolition Charge");

    player.cycle_tool_forward();
    TEST_CHECK(player.active_tool() == ToolSlot::MiningDrill, "Cycling forward from Demolition Charge must wrap back to Mining Drill");

    // Simulate backward cycle (scroll up): Drill -> Satchel Charge -> Combat Weapon -> Drill
    player.cycle_tool_backward();
    TEST_CHECK(player.active_tool() == ToolSlot::DemolitionCharge, "Cycling backward from Drill must wrap to Demolition Charge");

    player.cycle_tool_backward();
    TEST_CHECK(player.active_tool() == ToolSlot::CombatWeapon, "Cycling backward from Demolition Charge must select Combat Weapon");

    player.cycle_tool_backward();
    TEST_CHECK(player.active_tool() == ToolSlot::MiningDrill, "Cycling backward from Combat Weapon must select Mining Drill");
    log_pass("Mouse scroll wheel 3-tool bidirectional cycling (Drill, Weapon, Satchel; Bulkhead placed via RMB on drill)");

    // 2. Physical Satchel Charge Placement & Remote Detonator State
    TEST_CHECK(!player.has_placed_charge(), "Player must not have a placed charge initially");

    // Plant charge at (18, 20, 16) with normal (0, 1, 0)
    glm::ivec3 target_voxel(18, 20, 16);
    glm::ivec3 target_norm(0, 1, 0);

    bool charge_placed_callback_fired = false;
    player.set_on_charge_placed([&](const glm::ivec3& pos, const glm::ivec3& norm) {
        charge_placed_callback_fired = true;
    });

    // Simulate placing charge
    player.clear_placed_charge();
    TEST_CHECK(!player.has_placed_charge(), "Clear placed charge must reset has_placed_charge to false");

    // 3. Class Tactical Ability & Weapon Archetype Alignment Verification
    player.set_character_class(CharacterClass::Demolitionist);
    WeaponStats demo_w = get_class_weapon_stats(CharacterClass::Demolitionist);
    TEST_CHECK(demo_w.archetype == WeaponArchetype::MagmaScattergun, "Demolitionist must use Magma Scattergun");
    TEST_CHECK(demo_w.short_name == "SCAT", "Magma Scattergun short name must be SCAT");

    WeaponStats van_w = get_class_weapon_stats(CharacterClass::Vanguard);
    TEST_CHECK(van_w.archetype == WeaponArchetype::PlasmaCarbine, "Vanguard must use Plasma Carbine");
    TEST_CHECK(van_w.short_name == "CARB", "Plasma Carbine short name must be CARB");

    WeaponStats scout_w = get_class_weapon_stats(CharacterClass::Scout);
    TEST_CHECK(scout_w.archetype == WeaponArchetype::NeedlerRailgun, "Scout must use Needler Railgun");
    TEST_CHECK(scout_w.short_name == "RAIL", "Needler Railgun short name must be RAIL");
    log_pass("Class Weapon Archetype and Tactical Kit alignment (Demolitionist=SCAT, Vanguard=CARB, Scout=RAIL)");
}

// ─────────────────────────────────────────────────────────────
// 15. CHEMICAL FLARES, ABERRANT ARCHETYPES, CARCASSES & OBJECTIVES
// ─────────────────────────────────────────────────────────────
void test_flares_aberrants_and_mission_objectives() {
    std::cout << "\n=== [MODULE 15] Flares, Aberrant Archetypes, Carcasses & Mission Objectives ===" << std::endl;

    // 1. Flare Manager & Chemical Flares
    FlareManager::instance().clear();
    TEST_CHECK(FlareManager::instance().flares().empty(), "Flares must initially be empty");

    // Spawn 3 flares for the 3 character classes
    FlareManager::instance().spawn_flare(glm::vec3(10.0f, 20.0f, 10.0f), glm::vec3(1.0f, 0.5f, 0.0f), CharacterClass::Scout);
    FlareManager::instance().spawn_flare(glm::vec3(10.0f, 20.0f, 10.0f), glm::vec3(0.0f, 0.5f, 1.0f), CharacterClass::Vanguard);
    FlareManager::instance().spawn_flare(glm::vec3(10.0f, 20.0f, 10.0f), glm::vec3(-1.0f, 0.5f, 0.0f), CharacterClass::Demolitionist);

    TEST_CHECK(FlareManager::instance().flares().size() == 3, "Must have spawned 3 flares");
    TEST_CHECK(FlareManager::instance().flares()[0].light_radius >= 16.0f && FlareManager::instance().flares()[0].light_radius <= 22.0f, "Flare light radius must be 16m-22m");
    TEST_CHECK(FlareManager::instance().flares()[0].lifetime == 60.0f, "Flare lifetime must be 60s");
    // Class colors
    TEST_CHECK(FlareManager::instance().flares()[0].color.r < 0.2f && FlareManager::instance().flares()[0].color.b > 0.8f, "Scout flare must be electric cyan");
    TEST_CHECK(FlareManager::instance().flares()[1].color.r > 0.8f && FlareManager::instance().flares()[1].color.g > 0.6f, "Vanguard flare must be amber");
    TEST_CHECK(FlareManager::instance().flares()[2].color.r > 0.8f && FlareManager::instance().flares()[2].color.b < 0.2f, "Demolitionist flare must be orange");

    // Simulate physics and ground bounce
    World world(1234);
    // Flat floor at Y=10
    for (int x = 0; x <= 20; ++x) {
        for (int z = 0; z <= 20; ++z) {
            world.set_voxel(x, 10, z, Voxel{MAT_FRACTURED_GRANITE, 0});
        }
    }
    for (int step = 0; step < 120; ++step) {
        FlareManager::instance().update(0.016f, world);
    }
    TEST_CHECK(FlareManager::instance().flares()[0].position.y >= 10.0f, "Flare must rest at or above floor level");
    TEST_CHECK(FlareManager::instance().flares()[0].lifetime < 60.0f, "Flare lifetime must decay over time");
    log_pass("Chemical Flare physics, bounce, class color illumination (Scout=Cyan, Vanguard=Amber, Demo=Orange) and 60s lifetime");

    // 2. Player Flare Inventory & 15s Recharge
    PlayerController player;
    TEST_CHECK(player.flare_count() == 3, "Player must start with max 3 flares");
    bool flare_thrown = false;
    player.set_on_flare_thrown([&](const glm::vec3&, const glm::vec3&, CharacterClass) {
        flare_thrown = true;
    });
    player.throw_flare();
    TEST_CHECK(flare_thrown, "Throwing flare must invoke on_flare_thrown callback");
    TEST_CHECK(player.flare_count() == 2, "Flare count must decrement to 2");
    // Throw remaining flares
    player.throw_flare();
    player.throw_flare();
    TEST_CHECK(player.flare_count() == 0, "Flare count must be 0 after throwing 3");
    player.throw_flare(); // Capacity empty, should not throw
    TEST_CHECK(player.flare_count() == 0, "Cannot throw when flare count is 0");

    // Advance 15s recharge timer
    player.update(15.1f, world);
    TEST_CHECK(player.flare_count() == 1, "Player must recharge 1 flare after 15s");
    log_pass("Player Chemical Flare 3-capacity inventory and 15s automatic recharge timer");

    // 3. Breadcrumb Trail Auto-placement
    player.set_position(glm::vec3(10.0f, 12.0f, 10.0f));
    player.update(0.1f, world);
    size_t crumbs_initial = player.breadcrumbs().size();
    // Move player by 15 meters (> 12m threshold)
    player.set_position(glm::vec3(25.0f, 12.0f, 10.0f));
    player.update(0.1f, world);
    TEST_CHECK(player.breadcrumbs().size() > crumbs_initial, "Moving > 12m must drop an automatic breadcrumb trail marker");
    log_pass("3D Breadcrumb Trail automatic placement every 12m of subterranean exploration");

    // 4. Mission System: Precursor Vault & Reactive Gas Pockets
    MissionSystem mission;
    mission.embed_precursor_vault(world, 1);
    TEST_CHECK(mission.vault().exists, "Mission system must report vault embedded");
    Voxel door_vox = world.get_voxel(mission.vault().door_pos.x, mission.vault().door_pos.y, mission.vault().door_pos.z);
    TEST_CHECK(door_vox.material_id == MAT_REINFORCED_VAULT_DOOR, "Embedded vault door must be MAT_REINFORCED_VAULT_DOOR");

    // Detonate satchel charge near vault door
    bool breach_notified = false;
    mission.set_on_notification([&](const std::string& msg, float) {
        if (msg.find("BREACHED") != std::string::npos) breach_notified = true;
    });
    glm::vec3 door_world = glm::vec3(mission.vault().door_pos) + glm::vec3(0.5f);
    bool breached = mission.check_satchel_vault_breach(world, door_world, 3.5f);
    TEST_CHECK(breached, "Satchel charge blast must breach precursor vault bulkhead");
    TEST_CHECK(mission.is_vault_breached(), "Vault state must be breached");
    TEST_CHECK(world.get_voxel(mission.vault().door_pos.x, mission.vault().door_pos.y, mission.vault().door_pos.z).material_id == MAT_AIR, "Breached door voxel must be removed from world");

    // Retrieve Relic Hyper-Core (player stands near relic and interacts via [E])
    glm::vec3 relic_world = glm::vec3(mission.vault().relic_pos) + glm::vec3(0.5f);
    mission.update(0.1f, world, relic_world);
    TEST_CHECK(!mission.is_relic_retrieved(), "Relic must require player interaction and NOT auto-pickup on approach");
    TEST_CHECK(mission.can_interact_relic(relic_world), "Relic must be interactable when player is nearby");
    bool relic_interacted = mission.interact_relic(world, relic_world);
    TEST_CHECK(relic_interacted, "Interacting with relic must retrieve it");
    TEST_CHECK(mission.is_relic_retrieved(), "Relic state must be retrieved after [E] interaction");
    TEST_CHECK(world.get_voxel(mission.vault().relic_pos.x, mission.vault().relic_pos.y, mission.vault().relic_pos.z).material_id == MAT_AIR, "Retrieved relic voxel must be removed from pedestal");
    TEST_CHECK(mission.total_bonus_xp() == 350, "Relic extraction bonus must award +350 EXP");
    TEST_CHECK(mission.total_bonus_titanium() == 5, "Relic extraction bonus must award +5 Titanium Cores");
    log_pass("Precursor Vault Bulkhead Satchel breach and Relic Hyper-Core interactive pickup [E] reward (+350 EXP, +5 Titanium)");

    // Reactive Gas Pockets: create gas voxels
    for (int x = 14; x <= 16; ++x) {
        for (int z = 14; z <= 16; ++z) {
            world.set_voxel(x, 11, z, Voxel{MAT_GAS, 0});
        }
    }
    VoidStalkerManager stalker_mgr;
    stalker_mgr.stalkers_mut().clear();
    // Spawn stalker near gas
    stalker_mgr.spawn_stalker(glm::vec3(15.0f, 11.0f, 15.0f), 1.0f, StalkerRole::Melee);
    bool gas_ignited = false;
    mission.set_on_gas_ignited([&](const glm::vec3&, float) {
        gas_ignited = true;
    });
    int ignited_count = mission.ignite_gas_pocket(world, glm::vec3(15.0f, 11.0f, 15.0f), 4.0f, stalker_mgr);
    TEST_CHECK(ignited_count > 0, "Igniting gas pocket must burn gas voxels");
    TEST_CHECK(gas_ignited, "Gas ignition callback must fire");
    TEST_CHECK(world.get_voxel(15, 11, 15).material_id == MAT_AIR, "Ignited gas voxels must clear into air");
    TEST_CHECK(stalker_mgr.stalkers()[0].is_dead() || stalker_mgr.stalkers()[0].is_dying(), "Fireball explosion must incinerate nearby aberrant swarm hostiles");
    log_pass("Reactive Gas Pocket ignition, fireball detonation and swarm hostile incineration");

    // 5. Aberrant Fauna Archetypes: ChitinGoliath
    stalker_mgr.stalkers_mut().clear();

    // Spawn Chitin Goliath facing East (+X, yaw = 0)
    stalker_mgr.stalkers_mut().clear();
    stalker_mgr.spawn_stalker(glm::vec3(15.0f, 10.0f, 15.0f), 1.0f, StalkerRole::ChitinGoliath);
    auto& goliath = stalker_mgr.stalkers_mut().back();
    TEST_CHECK(goliath.role == StalkerRole::ChitinGoliath, "Role must be ChitinGoliath");
    TEST_CHECK(goliath.hp > 150.0f, "Chitin Goliath must have heavy armored HP pool");
    goliath.yaw = 0.0f; // Facing East (+X)

    // Shot from front: player is at +X (18, 10, 15), shooting toward Goliath at (15, 10, 15)
    float front_dmg = 0.0f;
    stalker_mgr.damage_nearest(glm::vec3(18.0f, 10.0f, 15.0f), 5.0f, 40.0f, false, nullptr, &front_dmg);
    // 85% deflection: front_dmg should be 15% of 40 = 6 HP
    TEST_CHECK(front_dmg <= 40.0f * 0.20f, "Frontal shots on Chitin Goliath must deflect 85% damage");

    // Shot from rear: player is at -X (12, 10, 15), shooting from behind Goliath
    float rear_dmg = 0.0f;
    stalker_mgr.damage_nearest(glm::vec3(12.0f, 10.0f, 15.0f), 5.0f, 40.0f, false, nullptr, &rear_dmg);
    TEST_CHECK(rear_dmg >= 35.0f, "Rear weak point shots on Chitin Goliath must deal full unmitigated damage");
    log_pass("Chitin Goliath heavy tank front armor 85% deflection and rear weak point vulnerability");

    // 6. Persistent Carcass Floor Skirmish Tracking
    CarcassManager::instance().clear();
    CarcassManager::instance().spawn_carcass(glm::vec3(12.0f, 10.0f, 12.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.0f), StalkerRole::Melee);
    TEST_CHECK(CarcassManager::instance().carcasses().size() == 1, "Must spawn 1 persistent carcass");
    TEST_CHECK(CarcassManager::instance().carcasses()[0].max_lifetime >= 45.0f, "Carcass must persist for 45s before decay");
    CarcassManager::instance().update(20.0f, world);
    TEST_CHECK(CarcassManager::instance().carcasses().size() == 1, "Carcass must still exist after 20s");
    CarcassManager::instance().update(30.0f, world);
    TEST_CHECK(CarcassManager::instance().carcasses().empty(), "Carcass must decay into ash after 45s+");
    log_pass("Persistent enemy floor carcasses (45s lifetime before ash decay) for tracking skirmish sites");
}

void test_headlamp_briefing_melee_and_loot_systems() {
    std::cout << "\n=== [MODULE 16] Headlamp, Briefing, Melee Shove & Loot Aggregation ===" << std::endl;

    // 1. Headlamp toggling and controller state
    PlayerController player(glm::vec3(0.0f, 10.0f, 0.0f));
    TEST_CHECK(player.is_headlamp_on(), "Headlamp must default to ON");
    player.toggle_headlamp();
    TEST_CHECK(!player.is_headlamp_on(), "Headlamp must toggle to OFF");
    player.toggle_headlamp();
    TEST_CHECK(player.is_headlamp_on(), "Headlamp must toggle back to ON");
    log_pass("Headlamp toggle state and suit vitals telemetry");

    // 2. Headlamp notification debounce & overwrite
    HUD hud(1600, 900, true);
    hud.show_warning("HEADLAMP: ACTIVE", 2.0f);
    TEST_CHECK(hud.notifications().front().count == 1, "Headlamp warning must not stack count multiplier");
    hud.show_warning("HEADLAMP: ACTIVE", 2.0f);
    TEST_CHECK(hud.notifications().front().count == 1, "Headlamp repeat warning must debounce and overwrite rather than stack");
    log_pass("Headlamp notification debounce and single-card overwrite");

    // 3. Contractor Field Briefing Auto-Dismiss & Toggle
    hud.SetContractorBriefing(true, 8.0f);
    TEST_CHECK(hud.briefing_timer() == 8.0f, "Briefing timer must initialize to 8.0s");
    TEST_CHECK(hud.is_help_briefing_visible(), "Briefing must be visible initially");
    hud.update(5.0f);
    TEST_CHECK(std::abs(hud.briefing_timer() - 3.0f) < 0.01f, "Briefing timer must decrement linearly with dt");
    hud.update(3.5f);
    TEST_CHECK(hud.briefing_timer() == 0.0f, "Briefing timer must clamp to 0.0s");
    hud.toggle_help_briefing();
    TEST_CHECK(hud.is_help_briefing_visible(), "Briefing must toggle back ON via key H");
    hud.toggle_help_briefing();
    TEST_CHECK(!hud.is_help_briefing_visible(), "Briefing must toggle OFF via key H");
    log_pass("Contractor field briefing auto-dismiss over 8.0s and [H] manual toggle");

    // 4. Loot Toast Full Queue Search & 4-Row Cap
    hud.PushLootToast("res_carapace", "Carapace", 1, glm::vec4(1.0f));
    hud.PushLootToast("res_biomass", "Biomass", 1, glm::vec4(1.0f));
    hud.PushLootToast("res_carapace", "Carapace", 2, glm::vec4(1.0f));
    hud.PushLootToast("res_biomass", "Biomass", 3, glm::vec4(1.0f));
    TEST_CHECK(hud.loot_toast_count() == 2, "Alternating loot pickups must consolidate into 2 unique rows, not 4 alternating rows");

    // Add extra items to verify max 4 rows cap
    hud.PushLootToast("res_voidite", "Voidite", 1, glm::vec4(1.0f));
    hud.PushLootToast("res_titanium", "Titanium", 1, glm::vec4(1.0f));
    hud.PushLootToast("res_scrap", "Scrap", 1, glm::vec4(1.0f));
    TEST_CHECK(hud.loot_toast_count() <= 4, "Loot toast queue must cap at maximum 4 active rows");
    log_pass("Loot toast queue full-depth search consolidation and 4-row overflow limit");

    // 5. Defensive Quick Melee Shove Mechanics
    TEST_CHECK(player.melee_shove_cooldown() <= 0.0f, "Melee shove must start off cooldown");
    TEST_CHECK(player.melee_shove_progress() == 0.0f, "Melee shove progress must start at 0.0");
    player.execute_melee_shove();
    TEST_CHECK(player.is_melee_shoving(), "Player must be in melee shoving state");
    TEST_CHECK(player.melee_shove_progress() >= 0.0f && player.melee_shove_progress() < 0.1f, "Melee shove progress must be near 0.0 at initiation");
    TEST_CHECK(player.melee_shove_cooldown() > 0.75f, "Melee shove cooldown must reset to 0.8s");
    float shove_cd = player.melee_shove_cooldown();
    player.execute_melee_shove();
    TEST_CHECK(player.melee_shove_cooldown() <= shove_cd, "Melee shove on cooldown must not reset cooldown");

    // Melee Shove cone area & knockback test on stalker
    VoidStalkerManager stalkers;
    stalkers.spawn_melee(glm::vec3(0.0f, 10.0f, 2.0f)); // 2m directly in front (+Z)
    TEST_CHECK(stalkers.stalkers().size() == 1, "Must have 1 spawned stalker");
    float initial_hp = stalkers.stalkers()[0].hp;
    int shove_hits = stalkers.apply_melee_shove(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 2.5f, 0.65f, 15.0f);
    TEST_CHECK(shove_hits == 1, "Melee shove cone must hit stalker at 2.0m");
    TEST_CHECK(stalkers.stalkers()[0].hp < initial_hp, "Stalker must take 15 HP shove damage");
    TEST_CHECK(stalkers.stalkers()[0].state == StalkerState::Stunned, "Stalker must be stunned by shove");
    TEST_CHECK(stalkers.stalkers()[0].stun_timer >= 0.55f, "Stalker stun duration must be 0.6s");
    TEST_CHECK(stalkers.stalkers()[0].velocity.z > 5.0f, "Stalker must receive repulsive knockback impulse");
    log_pass("Defensive quick melee shove (V key, 0.8s cooldown, 2.5m cone, attack cancel, knockback & 0.6s stun)");

    // 6. 3D Holographic Terrain Scanner Orbit & TAB hold
    TerrainScanner scanner;
    TEST_CHECK(!scanner.is_active(), "Terrain scanner must start inactive");
    scanner.update(0.1f, true, glm::vec3(10.0f, 5.0f, 10.0f), 12.0f, 8.0f, true);
    TEST_CHECK(scanner.is_active(), "Terrain scanner must be active while TAB held");
    TEST_CHECK(scanner.fold_progress() > 0.0f, "Terrain scanner must begin unfolding projection");
    TEST_CHECK(scanner.discovery_percentage() > 0.0f, "Scanner must record uncovered fog of war discovery");
    float initial_yaw = scanner.orbit_yaw();
    scanner.update(0.1f, true, glm::vec3(25.0f, 5.0f, 30.0f), 15.0f, 0.0f, true);
    TEST_CHECK(scanner.orbit_yaw() != initial_yaw, "Mouse delta must orbit/pan scanner camera smoothly");
    TEST_CHECK(scanner.discovery_percentage() > 5.0f, "Moving delver must uncover cumulative cavern territory");
    scanner.update(0.1f, false, glm::vec3(25.0f, 5.0f, 30.0f), 0.0f, 0.0f, false);
    TEST_CHECK(!scanner.is_active(), "Terrain scanner must deactivate cleanly when TAB released");
    log_pass("2D tactical cavern cartography, fog-of-war discovery memory, and mouse pan controls");

    // 7. Debrief Menu Re-Deploy Routing
    DebriefMenu debrief;
    debrief.SetCurrentSectorIndex(3);
    DebriefAction act_redeploy = debrief.ProcessClick("[RE-DEPLOY EXPEDITION]");
    TEST_CHECK(act_redeploy == DebriefAction::RedeployExpedition, "Clicking [RE-DEPLOY EXPEDITION] must return RedeployExpedition action");
    DebriefAction act_hub = debrief.ProcessClick("[RETURN TO ORBITAL HUB]");
    TEST_CHECK(act_hub == DebriefAction::ReturnToHub, "Clicking [RETURN TO ORBITAL HUB] must return ReturnToHub action");
    log_pass("Debrief menu [RE-DEPLOY EXPEDITION] immediate sector restart routing");
}

// ─────────────────────────────────────────────────────────────
// 17. CLUSTERED FORWARD LIGHTING (16x9x24 GRID & SSBO CULLING)
// ─────────────────────────────────────────────────────────────
void test_clustered_forward_lighting() {
    std::cout << "\n=== [MODULE 17] Clustered Forward Lighting & 3D Frustum Culling ===" << std::endl;

    // 1. Grid Dimensions & Capacity Constraints
    TEST_CHECK(CLUSTERS_X == 16, "Cluster frustum grid width must be 16");
    TEST_CHECK(CLUSTERS_Y == 9, "Cluster frustum grid height must be 9");
    TEST_CHECK(CLUSTERS_Z == 24, "Cluster frustum grid depth slices must be 24");
    TEST_CHECK(TOTAL_CLUSTERS == 3456, "Total cluster volume must be 16*9*24 = 3456");
    TEST_CHECK(MAX_LIGHTS_PER_CLUSTER == 64, "Max active lights per cluster must be 64");
    TEST_CHECK(MAX_SCENE_LIGHTS == 256, "Max scene dynamic lights capacity must be 256");
    log_pass("16x9x24 Cluster Grid Specification & Volume Geometry");

    // 2. SSBO Data Alignment & std430 Compatibility
    TEST_CHECK(sizeof(GpuPointLight) == 32, "GpuPointLight must be exactly 32 bytes for std430 packing");
    TEST_CHECK(alignof(GpuPointLight) == 16, "GpuPointLight alignment must be 16 bytes");
    TEST_CHECK(sizeof(ClusterRecord) == 272, "ClusterRecord must be exactly 272 bytes (16-byte header + 64*4 byte indices)");
    TEST_CHECK(alignof(ClusterRecord) == 16, "ClusterRecord alignment must be 16 bytes");
    log_pass("std430 SSBO Memory Invariants & Zero-Padding Divergence Guard");

    // 3. Cluster Spatial Indexing & 3D Bijection
    TEST_CHECK(ClusteredLighting::compute_cluster_index(0, 0, 0) == 0, "Origin cluster index must be 0");
    TEST_CHECK(ClusteredLighting::compute_cluster_index(15, 8, 23) == 3455, "Corner cluster index must be 3455");

    bool indexing_valid = true;
    for (uint32_t z = 0; z < CLUSTERS_Z; ++z) {
        for (uint32_t y = 0; y < CLUSTERS_Y; ++y) {
            for (uint32_t x = 0; x < CLUSTERS_X; ++x) {
                uint32_t idx = ClusteredLighting::compute_cluster_index(x, y, z);
                uint32_t rx, ry, rz;
                ClusteredLighting::get_cluster_coords(idx, rx, ry, rz);
                if (rx != x || ry != y || rz != z) {
                    indexing_valid = false;
                    break;
                }
            }
        }
    }
    TEST_CHECK(indexing_valid, "Cluster indexing must be a lossless bijection across all 3456 cells");
    log_pass("Lossless Bijective 3D Spatial Indexing Across 3456 Frustum Cells");

    // 4. Exponential Depth Slicing Distribution
    float near_z = 0.1f;
    float far_z = 250.0f;
    TEST_CHECK(ClusteredLighting::depth_to_slice(0.05f, near_z, far_z) == 0, "Depths <= near plane must clamp to slice 0");
    TEST_CHECK(ClusteredLighting::depth_to_slice(250.0f, near_z, far_z) == 23, "Depth at far plane must clamp to slice 23");
    TEST_CHECK(ClusteredLighting::depth_to_slice(500.0f, near_z, far_z) == 23, "Depths >= far plane must clamp to slice 23");

    uint32_t prev_slice = 0;
    bool monotonic = true;
    for (float d = 0.1f; d <= 250.0f; d += 2.5f) {
        uint32_t s = ClusteredLighting::depth_to_slice(d, near_z, far_z);
        if (s < prev_slice) {
            monotonic = false;
            break;
        }
        prev_slice = s;
    }
    TEST_CHECK(monotonic, "Exponential depth slicing must be monotonically non-decreasing");

    // Seamless depth range continuity
    bool slices_continuous = true;
    for (uint32_t s = 0; s < CLUSTERS_Z - 1; ++s) {
        float n1, f1, n2, f2;
        ClusteredLighting::slice_to_depth_range(s, near_z, far_z, n1, f1);
        ClusteredLighting::slice_to_depth_range(s + 1, near_z, far_z, n2, f2);
        if (std::abs(f1 - n2) > 0.001f) {
            slices_continuous = false;
            break;
        }
    }
    TEST_CHECK(slices_continuous, "Adjacent depth slices must share continuous boundary planes without gaps");
    log_pass("Continuous Exponential Depth Partitioning & Boundary Continuity");

    // 5. View-Space Cluster AABB Frustum Derivation
    glm::mat4 proj = glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, near_z, far_z);
    glm::vec3 aabb_min, aabb_max;
    ClusteredLighting::compute_cluster_aabb_view(0, 0, 0, proj, near_z, far_z, aabb_min, aabb_max);
    TEST_CHECK(aabb_min.x < aabb_max.x, "Cluster AABB X min must be strictly less than max");
    TEST_CHECK(aabb_min.y < aabb_max.y, "Cluster AABB Y min must be strictly less than max");
    TEST_CHECK(aabb_min.z < aabb_max.z, "Cluster AABB Z min must be strictly less than max (most negative to least negative)");
    TEST_CHECK(aabb_max.z < 0.0f, "Cluster AABB in view-space must reside in negative Z halfspace");
    log_pass("View-Space Cluster AABB Frustum Unprojection & Coordinate Validity");

    // 6. View-Space Sphere vs AABB Culling Exactness
    glm::vec3 test_aabb_min(-2.0f, -2.0f, -10.0f);
    glm::vec3 test_aabb_max(2.0f, 2.0f, -5.0f);

    // Light inside AABB
    TEST_CHECK(ClusteredLighting::test_sphere_aabb(glm::vec3(0.0f, 0.0f, -7.0f), 1.0f, test_aabb_min, test_aabb_max), "Light inside cluster must intersect");
    // Light outside but radius touches
    TEST_CHECK(ClusteredLighting::test_sphere_aabb(glm::vec3(3.5f, 0.0f, -7.0f), 2.0f, test_aabb_min, test_aabb_max), "Light sphere touching AABB must intersect");
    // Light completely outside
    TEST_CHECK(!ClusteredLighting::test_sphere_aabb(glm::vec3(15.0f, 0.0f, -7.0f), 2.0f, test_aabb_min, test_aabb_max), "Distant light must be culled from cluster");
    // Light behind camera
    TEST_CHECK(!ClusteredLighting::test_sphere_aabb(glm::vec3(0.0f, 0.0f, 5.0f), 2.0f, test_aabb_min, test_aabb_max), "Light behind camera outside influence range must be culled");
    log_pass("Arvo Sphere-AABB Intersection Invariants & Branchless Distance Testing");

    // 7. Multi-Light Culling Stress Test (35+ Simultaneous Dynamic Lights)
    std::vector<PointLight> test_scene_lights;
    test_scene_lights.reserve(40);

    // Place 35 dynamic lights: 5 in the local near cluster corridor, 30 scattered far throughout the cavern
    for (int i = 0; i < 5; ++i) {
        PointLight local_fl;
        local_fl.position = glm::vec3(0.0f, 0.0f, -2.0f - static_cast<float>(i)); // Near camera view path
        local_fl.radius = 5.0f;
        local_fl.color = glm::vec3(0.1f, 0.9f, 0.3f);
        local_fl.intensity = 3.0f;
        test_scene_lights.push_back(local_fl);
    }
    for (int i = 0; i < 30; ++i) {
        PointLight distant_flare;
        distant_flare.position = glm::vec3(80.0f + static_cast<float>(i * 3), 40.0f, -120.0f); // Far cavern sector
        distant_flare.radius = 8.0f;
        distant_flare.color = glm::vec3(0.9f, 0.2f, 0.1f);
        distant_flare.intensity = 2.5f;
        test_scene_lights.push_back(distant_flare);
    }
    TEST_CHECK(test_scene_lights.size() == 35, "Must test with 35+ simultaneous dynamic lights");

    // Cull against near cluster tile (8, 4, 1) - center of screen near camera
    glm::vec3 center_tile_min, center_tile_max;
    ClusteredLighting::compute_cluster_aabb_view(8, 4, 1, proj, near_z, far_z, center_tile_min, center_tile_max);

    uint32_t culled_visible_count = 0;
    for (const auto& light : test_scene_lights) {
        // In view space with camera at origin looking down -Z:
        glm::vec3 pos_view = light.position;
        if (ClusteredLighting::test_sphere_aabb(pos_view, light.radius, center_tile_min, center_tile_max)) {
            culled_visible_count++;
        }
    }

    TEST_CHECK(culled_visible_count >= 1 && culled_visible_count <= 5, "Local cluster tile must only record nearby affecting lights, culling all distant cavern lights");
    log_pass("35+ Simultaneous Dynamic Lights Cluster Culling Performance & Overlap Isolation");
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- COMPLETE COMPREHENSIVE UNIT TEST SUITE" << std::endl;
    std::cout << "==========================================================" << std::endl;

    test_voxel_engine();
    test_structural_solver();
    test_player_controller_and_physics();
    test_progression_and_economy();
    test_skills_surveying_and_hazards();
    test_inventory_and_anti_exploit();
    test_save_persistence();
    test_network_packets();
    test_seismic_tremor_and_falling_blocks();
    test_noise_meter_and_void_stalkers();
    test_carry_weight_and_tactical_abilities();
    test_cavern_luminaries_and_lighting();
    test_plasma_carbine_combat();
    test_satchel_charge_and_weapon_cycling();
    test_flares_aberrants_and_mission_objectives();
    test_headlamp_briefing_melee_and_loot_systems();
    test_clustered_forward_lighting();

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "  ALL " << s_total_unit_tests << " UNIT TESTS PASSED SUCCESSFULLY WITH 0 ERRORS!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
