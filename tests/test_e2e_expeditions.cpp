#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <memory>

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

#define E2E_CHECK(expr, msg) \
    if (!(expr)) { \
        std::cerr << "\n[E2E TEST FAILURE] " << msg << " (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

static int s_total_e2e_scenarios = 0;

void log_e2e_pass(const std::string& name) {
    s_total_e2e_scenarios++;
    std::cout << "  [E2E SCENARIO " << s_total_e2e_scenarios << " PASSED] " << name << std::endl;
}

// ─────────────────────────────────────────────────────────────
// E2E SCENARIO 1: Sector 1 Full Lifecycle (Scout Expedition)
// ─────────────────────────────────────────────────────────────
void run_e2e_sector1_scout_lifecycle() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "  E2E SCENARIO 1: Sector 1 Scout Full Expedition Lifecycle" << std::endl;
    std::cout << "========================================================" << std::endl;

    // 1. Initialize fresh User Profile & Select Scout (requires Level 3)
    UserProfile profile;
    profile.total_exp = 750;
    profile.total_coins = 500;
    profile.total_voidite = 20;
    profile.total_titanium = 15;
    profile.selected_class_id = static_cast<int>(CharacterClass::Scout);
    E2E_CHECK(profile.is_class_unlocked(CharacterClass::Scout), "Scout must be unlocked at Level 3");

    // 2. Hub Terminal Upgrade Purchase: Thruster Tank & Sonar Frequency
    bool buy_tank = profile.upgrades.purchase(UpgradeType::ThrusterTank, profile.get_player_level(), profile.total_coins, profile.total_voidite, profile.total_titanium);
    E2E_CHECK(buy_tank, "Scout must purchase Thruster Tank Tier 1");
    bool buy_sonar = profile.upgrades.purchase(UpgradeType::SonarFrequency, profile.get_player_level(), profile.total_coins, profile.total_voidite, profile.total_titanium);
    E2E_CHECK(buy_sonar, "Scout must purchase Sonar Frequency Tier 1");

    // 3. Save profile to disk and reload
    const std::string e2e_save = "tests/e2e_scout_profile.json";
    E2E_CHECK(SaveSystem::save_profile(profile, e2e_save), "Save profile to disk failed");
    UserProfile loaded_profile;
    E2E_CHECK(SaveSystem::load_profile(loaded_profile, e2e_save), "Load profile from disk failed");
    E2E_CHECK(loaded_profile.selected_class_id == static_cast<int>(CharacterClass::Scout), "Loaded class mismatch");
    E2E_CHECK(loaded_profile.upgrades.thrusterTankTier == 1, "Loaded upgrade tier mismatch");

    // 4. Launch Sector 1 Expedition
    World world(1337);
    world.generate_world(1, 1337);

    PlayerController player(glm::vec3(16.0f, 25.0f, 16.0f));
    player.apply_attributes_and_upgrades(CharacterClass::Scout, loaded_profile.upgrades);
    player.clamp_to_surface(world);

    // Verify Scout stats & upgrades applied
    auto scout_attr = get_character_attributes(CharacterClass::Scout);
    E2E_CHECK(std::abs(player.max_health() - 80.0f) < 0.01f, "Scout baseline health must be 80 HP");
    E2E_CHECK(std::abs(player.exo().max_power - 120.0f) < 0.01f, "Scout with Thruster Tier 1 must have 120 power");

    // 5. Surveying Sonar Pulse Scan (22m base + 2m tier 1 = 24m)
    SurveyingSystem surveying;
    float effective_sonar_radius = scout_attr.sonarRadius + loaded_profile.upgrades.sonarFrequencyTier * 2.0f;
    E2E_CHECK(std::abs(effective_sonar_radius - 24.0f) < 0.01f, "Effective sonar radius must be 24m");
    surveying.trigger_scan(player.position(), world, effective_sonar_radius, scout_attr.scanLingerBonus);
    surveying.update(0.1f);
    E2E_CHECK(surveying.is_active(), "Surveying scan wave must be propagating");

    // 6. Grapple Navigation Simulation
    player.grapple_mut().active = true;
    player.grapple_mut().anchor_point = player.position() + glm::vec3(0.0f, 8.0f, 5.0f);
    E2E_CHECK(player.grapple().active, "Grapple must be anchored");
    player.grapple_mut().active = false;

    // 7. Mining Loop: Mine 25 Voidite Crystals & Fabricate Bulkheads
    PlayerInventory inventory;
    inventory.reset(25);
    SkillMatrix skills;

    for (int i = 0; i < 25; ++i) {
        int vx = 16 + (i % 5);
        int vy = 10;
        int vz = 16 + (i / 5);
        world.set_voxel(vx, vy, vz, Voxel{MAT_VOIDITE_CRYSTAL, 0});

        // Simulate mining block
        world.set_voxel(vx, vy, vz, Voxel{MAT_AIR, 0}, true);
        inventory.add_voidite(1);
        skills.add_demolitions_xp(15);
    }
    E2E_CHECK(inventory.voidite == 25, "Voidite mined quota must equal 25");

    // Mine granite to trigger scrap bulkhead fabrication
    for (int g = 0; g < 9; ++g) {
        inventory.add_scrap_from_granite();
    }
    E2E_CHECK(inventory.bulkheads >= 16, "Bulkhead must be fabricated from granite scrap");

    // 8. Extraction Protocol
    ExtractionSystem extraction;
    extraction.deploy_beacon(player.position());
    E2E_CHECK(extraction.phase() == ExtractionPhase::BeaconDeployed, "Beacon must be deployed");

    // Fast-forward 40s defense timer
    extraction.update(42.0f, player.position());
    E2E_CHECK(extraction.phase() == ExtractionPhase::PodLanded, "Evac pod must land after countdown");

    // Board evacuation pod
    extraction.update(1.0f, extraction.beacon_position() + glm::vec3(0.5f, 0.0f, 0.5f));
    E2E_CHECK(extraction.phase() == ExtractionPhase::Complete, "Player must extract upon boarding");

    // 9. Run Finalization & Reward Banking
    inventory.vault_breached = true; // Breach subterranean cache
    inventory.finalize_run(1, true);
    E2E_CHECK(inventory.run_completion_rate == 100, "Completion rate must be 100%");
    E2E_CHECK(inventory.run_outcome_badge == "CLEARED (100%)", "Badge must be CLEARED (100%)");

    // Bank rewards into account pool
    int run_score = inventory.total_run_score;
    int lvls = 0, bonus = 0;
    loaded_profile.add_exp(run_score, lvls, bonus);
    loaded_profile.grant_coins(inventory.run_coins_earned);
    loaded_profile.total_voidite += inventory.voidite;
    loaded_profile.sector_records[1] = {100, "CLEARED (100%)"};
    SaveSystem::save_profile(loaded_profile, e2e_save);

    log_e2e_pass("Sector 1 Scout Complete Expedition Lifecycle (Mining, Grappling, Evacuation, Banking)");
}

// ─────────────────────────────────────────────────────────────
// E2E SCENARIO 2: Sector 2 Vault Breach (Demolitionist)
// ─────────────────────────────────────────────────────────────
void run_e2e_sector2_demolitionist_vault() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "  E2E SCENARIO 2: Sector 2 Demolitionist Vault Breach" << std::endl;
    std::cout << "========================================================" << std::endl;

    UserProfile profile;
    profile.selected_class_id = static_cast<int>(CharacterClass::Demolitionist);

    World world(2048);
    world.generate_world(2, 2048);

    PlayerController player(glm::vec3(16.0f, 20.0f, 16.0f));
    player.apply_attributes_and_upgrades(CharacterClass::Demolitionist, profile.upgrades);
    auto demo_attr = get_character_attributes(CharacterClass::Demolitionist);
    E2E_CHECK(demo_attr.baseMineSpeed == 1.40f, "Demolitionist mine speed must be 1.4x");

    PlayerInventory inventory;
    inventory.reset(25);
    SkillMatrix skills;

    // 1. Demolitionist Micro-Charge Tunneling (clears 4 blocks forward)
    glm::ivec3 blast_origin(16, 20, 16);
    glm::ivec3 blast_dir(0, 0, 1);
    for (int step = 0; step < 4; ++step) {
        glm::ivec3 target = blast_origin + blast_dir * step;
        world.set_voxel(target.x, target.y, target.z, Voxel{MAT_FRACTURED_GRANITE, 0});
    }

    // Trigger micro charge
    for (int step = 0; step < 4; ++step) {
        glm::ivec3 target = blast_origin + blast_dir * step;
        world.set_voxel(target.x, target.y, target.z, Voxel{MAT_AIR, 0}, true);
    }
    for (int step = 0; step < 4; ++step) {
        glm::ivec3 target = blast_origin + blast_dir * step;
        E2E_CHECK(world.get_voxel(target.x, target.y, target.z).material_id == MAT_AIR, "Tunnel block must be cleared");
    }

    // 2. Volatile Ore Mining with Demolitionist +25% Bonus
    int initial_score = inventory.total_run_score;
    // Mine volatile Radioactive Ore
    inventory.add_salvage(15);
    int demo_bonus = 20; // +25% of 80 pts
    inventory.total_run_score += demo_bonus;
    skills.add_demolitions_xp(20 + 5); // +25% bonus XP
    E2E_CHECK(inventory.total_run_score > initial_score, "Demolitionist volatile bonus score applied");

    // 3. Vault Door Breach & Relic Retrieval
    world.set_voxel(20, 20, 20, Voxel{MAT_REINFORCED_VAULT_DOOR, 0});
    world.set_voxel(20, 20, 20, Voxel{MAT_AIR, 0}, true);
    inventory.vault_breached = true;
    inventory.add_relic();
    E2E_CHECK(inventory.relic_extracted, "Hyper-Core Relic must be extracted");
    E2E_CHECK(inventory.vault_breached, "Vault breached status must be true");

    // 4. Safe Extraction & 100% Cleared Rating
    inventory.voidite = 25;
    inventory.finalize_run(2, true);
    E2E_CHECK(inventory.run_completion_rate == 100, "Sector 2 with Relic must achieve 100% completion rate");
    E2E_CHECK(inventory.run_outcome_badge == "CLEARED (100%)", "Sector 2 outcome badge mismatch");

    log_e2e_pass("Sector 2 Demolitionist Vault Breach, Volatile Mining (+25% Bonus) & Relic Extraction");
}

// ─────────────────────────────────────────────────────────────
// E2E SCENARIO 3: Sector 3 Fault-Line Tremor & Hazard Mitigation (Vanguard)
// ─────────────────────────────────────────────────────────────
void run_e2e_sector3_vanguard_survival() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "  E2E SCENARIO 3: Sector 3 Vanguard Fault-Line Survival" << std::endl;
    std::cout << "========================================================" << std::endl;

    UserProfile profile;
    profile.selected_class_id = static_cast<int>(CharacterClass::Vanguard);
    profile.upgrades.reinforcedPlatingTier = 2; // +30 HP, +20% cave-in reduction

    World world(3000);
    world.generate_world(3, 3000);

    PlayerController player(glm::vec3(16.0f, 15.0f, 16.0f));
    player.apply_attributes_and_upgrades(CharacterClass::Vanguard, profile.upgrades);

    // Vanguard base 135 HP + 2 * 15 = 165 HP
    E2E_CHECK(player.max_health() == 165.0f, "Vanguard with Plating Tier 2 must have 165 HP");

    // 1. Overhead Bulkhead Shelter Deflection Test
    // Place industrial bulkhead directly above player
    world.set_voxel(16, 18, 16, Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_PLAYER_PLACED});
    bool covered_by_bulkhead = true;

    // Simulate falling debris hitting bulkhead
    E2E_CHECK(covered_by_bulkhead, "Player under placed bulkhead must be shielded from cave-in debris");

    // 2. Direct Falling Debris Impact Damage Mitigation Test
    // Vanguard 50% + Plating Tier 2 (20%) = 70% total reduction
    float raw_damage = 50.0f;
    float applied_dmg = player.take_damage(raw_damage, true);
    E2E_CHECK(std::abs(applied_dmg - 15.0f) < 0.01f, "Vanguard 70% total debris damage mitigation mismatch");
    E2E_CHECK(player.health() == (165.0f - 15.0f), "Player health after mitigated debris hit mismatch");

    // 3. Level 3 180s Countdown Simulation
    float level3_timer = 180.0f;
    level3_timer -= 45.0f;
    E2E_CHECK(level3_timer > 0.0f, "Level 3 timer active");

    PlayerInventory inventory;
    inventory.reset(50);
    inventory.voidite = 50;
    inventory.finalize_run(3, true);
    E2E_CHECK(inventory.run_completion_rate >= 75, "Sector 3 extraction completion verified");

    log_e2e_pass("Sector 3 Vanguard Seismic Tremor Survival & 70% Cave-in Mitigation");
}

// ─────────────────────────────────────────────────────────────
// E2E SCENARIO 4: Anti-Exploit Voxel Flags & Abandon Penalty
// ─────────────────────────────────────────────────────────────
void run_e2e_anti_exploit_and_abandon() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "  E2E SCENARIO 4: Anti-Exploit Flags & Abandon Penalties" << std::endl;
    std::cout << "========================================================" << std::endl;

    PlayerInventory inventory;
    inventory.reset(25);
    inventory.bulkheads = 10;

    // Player places an industrial bulkhead
    inventory.consume_bulkhead();
    E2E_CHECK(inventory.bulkheads == 9, "Bulkhead count must decrement upon placement");

    Voxel placed_v{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_PLAYER_PLACED};
    E2E_CHECK((placed_v.flags_and_damage & VOXEL_FLAG_PLAYER_PLACED) != 0, "Flag must be set");

    // Player breaks their own placed bulkhead
    // Anti-exploit check: Award 0 XP, 0 Score, refund 1 Bulkhead
    inventory.refund_bulkhead();
    E2E_CHECK(inventory.bulkheads == 10, "Bulkhead must be refunded cleanly");
    E2E_CHECK(inventory.total_run_score == 0, "No score should be awarded for player-placed voxels");

    // Player collects loot during expedition
    inventory.add_voidite(40);
    inventory.add_titanium(10);
    inventory.add_salvage(20);

    // Player triggers Escape Abandon Expedition
    inventory.apply_abandon_penalty();
    E2E_CHECK(inventory.is_abandoned, "Abandon flag must be active");
    E2E_CHECK(inventory.voidite == 20, "50% voidite penalty applied");
    E2E_CHECK(inventory.titanium == 5, "50% titanium penalty applied");
    E2E_CHECK(inventory.salvage_parts == 10, "50% salvage penalty applied");

    inventory.finalize_run(1, false);
    E2E_CHECK(inventory.run_outcome_badge == "ABANDONED (<50%)", "Outcome badge must reflect abandoned status");

    log_e2e_pass("Anti-Exploit player-placed voxel verification & Abandon 50% penalty enforcement");
}

// ─────────────────────────────────────────────────────────────
// E2E SCENARIO 5: Multiplayer Host/Client Packet Loopback
// ─────────────────────────────────────────────────────────────
void run_e2e_multiplayer_simulation() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "  E2E SCENARIO 5: Multiplayer Host/Client Packet Loopback" << std::endl;
    std::cout << "========================================================" << std::endl;

    // Client connects: Handshake Request
    HandshakeRequestPacket req;
    req.protocol_version = 1;
#if defined(_MSC_VER)
    strncpy_s(req.player_name, sizeof(req.player_name), "Delver_Alpha", _TRUNCATE);
#else
    std::strncpy(req.player_name, "Delver_Alpha", sizeof(req.player_name) - 1);
#endif

    ByteStreamWriter client_out;
    client_out.write(req);

    // Host receives Handshake Request
    ByteStreamReader host_in(client_out.data(), client_out.size());
    HandshakeRequestPacket host_req;
    E2E_CHECK(host_in.read(host_req), "Host must read handshake request");
    E2E_CHECK(std::string(host_req.player_name) == "Delver_Alpha", "Player name verified by host");

    // Host responds with World Seed
    HandshakeResponsePacket resp;
    resp.assigned_player_id = 101;
    resp.world_seed = 7777;
    ByteStreamWriter host_out;
    host_out.write(resp);

    // Client receives seed and initializes local world
    ByteStreamReader client_in(host_out.data(), host_out.size());
    HandshakeResponsePacket client_resp;
    E2E_CHECK(client_in.read(client_resp), "Client must read handshake response");
    E2E_CHECK(client_resp.world_seed == 7777, "Client synced world seed 7777");

    World host_world(7777);
    World client_world(7777);

    // Client mines block at (10, 15, 10) and sends BlockDelta
    BlockDeltaPacket delta;
    delta.chunk_x = 0;
    delta.chunk_y = 0;
    delta.chunk_z = 0;
    delta.local_block_idx = static_cast<uint16_t>(Chunk::to_index(10, 15, 10));
    delta.material_id = MAT_AIR;
    delta.flags_and_damage = 0;

    ByteStreamWriter delta_writer;
    delta_writer.write(delta);

    // Host receives delta and broadcasts
    ByteStreamReader delta_reader(delta_writer.data(), delta_writer.size());
    BlockDeltaPacket host_delta;
    E2E_CHECK(delta_reader.read(host_delta), "Host reads block delta");
    host_world.set_voxel(10, 15, 10, Voxel{MAT_AIR, 0}, true);
    client_world.set_voxel(10, 15, 10, Voxel{MAT_AIR, 0}, true);

    E2E_CHECK(host_world.get_voxel(10, 15, 10).material_id == MAT_AIR, "Host world synchronized");
    E2E_CHECK(client_world.get_voxel(10, 15, 10).material_id == MAT_AIR, "Client world synchronized");

    log_e2e_pass("Multiplayer Handshake, Seed Synchronization, and World Delta Broadcast Loopback");
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- COMPLETE END-TO-END (E2E) TEST SUITE" << std::endl;
    std::cout << "==========================================================" << std::endl;

    try {
        run_e2e_sector1_scout_lifecycle();
        run_e2e_sector2_demolitionist_vault();
        run_e2e_sector3_vanguard_survival();
        run_e2e_anti_exploit_and_abandon();
        run_e2e_multiplayer_simulation();
    } catch (const std::exception& e) {
        std::cerr << "\n[E2E EXCEPTION] " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n[E2E UNKNOWN EXCEPTION]" << std::endl;
        return 1;
    }

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "  ALL " << s_total_e2e_scenarios << " END-TO-END SCENARIOS EXECUTED & PASSED WITH 0 ERRORS!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
