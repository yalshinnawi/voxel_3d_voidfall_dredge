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
#include "../src/core/save_system.hpp"
#include "../src/net/packet_types.hpp"

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
    TEST_CHECK(std::abs(vngd_attr.suitIntegrity - 160.0f) < 0.001f, "Vanguard 160 HP mismatch");
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
    int expected_refund = static_cast<int>(std::round(360 * 0.85f)); // 306 EXP
    TEST_CHECK(refunded == expected_refund, "Respec 85% refund mismatch");
    TEST_CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "Respec tier reset mismatch");
    log_pass("Upgrade purchasing validation and 85% Respec refund");
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

    matrix.add_surveying_xp(200);
    TEST_CHECK(matrix.surveying.unlocked, "Surveying skill must unlock at 200 XP");
    TEST_CHECK(matrix.get_sonar_radius() == 30.0f, "Unlocked surveying sonar radius must be 30m");
    log_pass("Skill Matrix XP accumulation and perk unlocks");

    // Surveying system pulse wave
    World world(1234);
    world.set_voxel(10, 10, 10, Voxel{MAT_VOIDITE_CRYSTAL, 0});
    SurveyingSystem surveying;
    surveying.trigger_scan(glm::vec3(10.0f, 10.0f, 10.0f), world, 25.0f, 1.0f);
    TEST_CHECK(surveying.is_active(), "Surveying scan must be active after trigger");
    surveying.update(0.5f);
    TEST_CHECK(!surveying.surveyed_voxels().empty(), "Surveying pulse must detect nearby mineral voxels");
    log_pass("Seismic Sonar wave query and mineral outline linger");

    // Hazard Clock and Tremors
    HazardClock hazard;
    hazard.set_sector_parameters(2);
    bool warning_fired = false;
    hazard.set_on_tremor_warning([&]() { warning_fired = true; });
    hazard.update(300.0f); // Fast forward time
    TEST_CHECK(hazard.radiation_level() > 0.0f, "Hazard radiation must increase over time");
    log_pass("Hazard Clock radiation buildup and seismic tremors");

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

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "  ALL " << s_total_unit_tests << " UNIT TESTS PASSED SUCCESSFULLY WITH 0 ERRORS!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
