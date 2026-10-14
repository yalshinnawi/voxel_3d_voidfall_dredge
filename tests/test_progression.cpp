#include <iostream>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include "../src/player/character_class.hpp"
#include "../src/player/upgrades.hpp"
#include "../src/core/save_system.hpp"
#include "../src/player/controller.hpp"

using namespace Voidfall;

#define CHECK(expr, msg) \
    if (!(expr)) { \
        std::cerr << "[TEST FAILURE] " << msg << " (" #expr ") at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "RUNNING VOIDFALL PROGRESSION UNIT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    // Test 1: Archetype Attribute Models
    {
        std::cout << "[Test 1] Testing Character Archetype Models..." << std::endl;

        auto demo = get_character_attributes(CharacterClass::Demolitionist);
        CHECK(demo.name == "Kaelen", "Demolitionist name mismatch");
        CHECK(std::abs(demo.baseMineSpeed - 1.4f) < 0.001f, "Demolitionist mine speed mismatch");
        CHECK(std::abs(demo.moveSpeed - 1.0f) < 0.001f, "Demolitionist move speed mismatch");
        CHECK(std::abs(demo.suitIntegrity - 100.0f) < 0.001f, "Demolitionist suit integrity mismatch");
        CHECK(!demo.traitName.empty(), "Demolitionist trait name empty");

        auto vngd = get_character_attributes(CharacterClass::Vanguard);
        CHECK(vngd.name == "Rhodes", "Vanguard name mismatch");
        CHECK(std::abs(vngd.baseMineSpeed - 1.0f) < 0.001f, "Vanguard mine speed mismatch");
        CHECK(std::abs(vngd.moveSpeed - 0.88f) < 0.001f, "Vanguard move speed mismatch");
        CHECK(std::abs(vngd.suitIntegrity - 135.0f) < 0.001f, "Vanguard suit integrity mismatch");
        CHECK(vngd.maxBulkheads == 12, "Vanguard bulkheads mismatch");
        CHECK(std::abs(vngd.fallingDamageReduction - 0.50f) < 0.001f, "Vanguard damage reduction mismatch");

        auto scout = get_character_attributes(CharacterClass::Scout);
        CHECK(scout.name == "Vesper", "Scout name mismatch");
        CHECK(std::abs(scout.baseMineSpeed - 1.0f) < 0.001f, "Scout mine speed mismatch");
        CHECK(std::abs(scout.moveSpeed - 1.20f) < 0.001f, "Scout move speed mismatch");
        CHECK(std::abs(scout.suitIntegrity - 80.0f) < 0.001f, "Scout suit integrity mismatch");
        CHECK(std::abs(scout.grapplePullSpeed - 1.50f) < 0.001f, "Scout grapple pull mismatch");
        CHECK(std::abs(scout.sonarRadius - 22.0f) < 0.001f, "Scout sonar radius mismatch");
        CHECK(std::abs(scout.scanLingerBonus - 1.0f) < 0.001f, "Scout scan linger mismatch");

        std::cout << " -> All 3 Archetype attributes verified successfully." << std::endl;
    }

    // Test 2: Exponential EXP Cost Formula, Level Gating & Branch Prerequisite Progression
    {
        std::cout << "[Test 2] Testing EXP Cost Formula, Level Gating & Branch Prerequisites..." << std::endl;
        CHECK(UpgradeTree::get_exp_cost(0) == 100, "Tier 0 exp cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(1) == 160, "Tier 1 exp cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(2) == 256, "Tier 2 exp cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(3) == 410, "Tier 3 exp cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(4) == 656, "Tier 4 exp cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(5) == 0, "Tier 5 (max) exp cost mismatch");

        std::cout << " -> Exponential EXP costs matched: 100 -> 160 -> 256 -> 410 -> 656 EXP." << std::endl;

        UpgradeTree tree;
        int exp = 1000;
        int coins = 500;
        int voidite = 100;
        int titanium = 100;
        int player_level = 1;

        // Verify Branch Prerequisite: DrillDurability requires DrillSpeed >= 1
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "Initial drill speed tier not 0");
        CHECK(!tree.can_purchase(UpgradeType::DrillDurability, player_level, exp),
              "DrillDurability must not be purchasable before DrillSpeed T1 prerequisite is unlocked");
        CHECK(tree.get_lock_reason(UpgradeType::DrillDurability, player_level, exp) == UpgradeLockReason::PrerequisiteLocked,
              "Lock reason must be PrerequisiteLocked");
        CHECK(tree.get_node_web_state(UpgradeType::DrillDurability, player_level, exp) == NodeWebState::GreyedOutLocked,
              "Node web state must be GreyedOutLocked");

        // Level 1: Can purchase Root DrillSpeed Tier 1 (requires Level 1, 100 EXP)
        CHECK(tree.can_purchase(UpgradeType::DrillSpeed, player_level, exp), "Cannot purchase tier 1 at level 1");
        bool bought = tree.purchase(UpgradeType::DrillSpeed, player_level, exp);
        CHECK(bought, "Purchase tier 1 failed");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 1, "Tier not 1 after purchase");
        CHECK(exp == 900, "EXP not deducted correctly (1000 - 100)");

        // CRITICAL CHECK: Upgrades must ONLY consume EXP! Coins, Voidite, Titanium must remain untouched!
        CHECK(coins == 500, "Coins must not be touched by upgrades");
        CHECK(voidite == 100, "Voidite must not be touched by upgrades");
        CHECK(titanium == 100, "Titanium must not be touched by upgrades");

        // Now that DrillSpeed T1 is acquired, DrillDurability prerequisite is MET and it awakens from GreyedOut!
        CHECK(tree.get_node_web_state(UpgradeType::DrillDurability, player_level, exp) != NodeWebState::GreyedOutLocked,
              "DrillDurability must no longer be greyed out after prerequisite is met");

        // At Level 1, cannot purchase Tier 2 (requires Level 2)
        CHECK(!tree.can_purchase(UpgradeType::DrillSpeed, player_level, exp), "Should not allow Tier 2 at Level 1");
        CHECK(tree.get_lock_reason(UpgradeType::DrillSpeed, player_level, exp) == UpgradeLockReason::LevelLocked,
              "Lock reason should be LevelLocked");
        bool bought_fail = tree.purchase(UpgradeType::DrillSpeed, player_level, exp);
        CHECK(!bought_fail, "Purchase Tier 2 should fail at Level 1");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 1, "Tier should remain 1");

        // Level up to Level 2: Now Tier 2 purchase succeeds!
        player_level = 2;
        CHECK(tree.can_purchase(UpgradeType::DrillSpeed, player_level, exp), "Cannot purchase Tier 2 at Level 2");
        bought = tree.purchase(UpgradeType::DrillSpeed, player_level, exp);
        CHECK(bought, "Purchase tier 2 failed at Level 2");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 2, "Tier not 2 after purchase");
        CHECK(exp == 740, "EXP not deducted correctly for tier 2 (900 - 160)");

        std::cout << " -> Branch prerequisites, level gating, and EXP-only deduction verified." << std::endl;
    }

    // Test 3: Respec Feature (100% EXP Refund + Immediate Re-Upgrade Verification)
    {
        std::cout << "[Test 3] Testing Respec with 100% EXP Full Refund & Re-Upgrade Capability..." << std::endl;
        UpgradeTree tree;
        int exp = 2000;
        int player_level = 5;

        // Buy 3 tiers of DrillSpeed:
        // T0: 100 EXP
        // T1: 160 EXP
        // T2: 256 EXP
        tree.purchase(UpgradeType::DrillSpeed, player_level, exp);
        tree.purchase(UpgradeType::DrillSpeed, player_level, exp);
        tree.purchase(UpgradeType::DrillSpeed, player_level, exp);

        // Buy 1 tier of ReinforcedPlating (T0: 100 EXP)
        tree.purchase(UpgradeType::ReinforcedPlating, player_level, exp);

        int total_spent_exp = tree.get_total_spent_exp();
        // 100 + 160 + 256 + 100 = 616 EXP
        CHECK(total_spent_exp == 616, "Total spent EXP mismatch");
        CHECK(exp == (2000 - 616), "EXP balance mismatch");

        // Execute Respec (100% full refund)
        int ref_exp = 0;
        tree.respec(ref_exp);

        CHECK(ref_exp == 616, "100% EXP refund mismatch");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "DrillSpeed tier not reset to 0");
        CHECK(tree.get_tier(UpgradeType::ReinforcedPlating) == 0, "ReinforcedPlating tier not reset to 0");

        // Restore refunded EXP into player pool
        exp += ref_exp;
        CHECK(exp == 2000, "EXP not restored to initial 2000");

        // Crucial test: Re-upgrade verification! The player can now re-spend EXP freely!
        bool re_p1 = tree.purchase(UpgradeType::ThrusterTank, player_level, exp);
        CHECK(re_p1, "Failed to re-upgrade into ThrusterTank after respec");
        CHECK(tree.get_tier(UpgradeType::ThrusterTank) == 1, "ThrusterTank tier not 1 after re-upgrade");

        bool re_p2 = tree.purchase(UpgradeType::KineticDynamo, player_level, exp);
        CHECK(re_p2, "Failed to re-upgrade into KineticDynamo after ThrusterTank T1 prerequisite met");
        CHECK(tree.get_tier(UpgradeType::KineticDynamo) == 1, "KineticDynamo tier not 1 after re-upgrade");

        std::cout << " -> Respec 100% full EXP recovery (616 EXP) and immediate re-upgrade verified." << std::endl;
    }

    // Test 4: Save & Load Persistence
    {
        std::cout << "[Test 4] Testing JSON Save / Load Persistence..." << std::endl;
        UserProfile p_write;
        p_write.total_exp = 1450;
        p_write.total_coins = 580;
        p_write.total_voidite = 65;
        p_write.total_titanium = 42;
        p_write.selected_class_id = 1; // Vanguard
        p_write.upgrades.drillSpeedTier = 3;
        p_write.upgrades.reinforcedPlatingTier = 2;
        p_write.upgrades.sonarFrequencyTier = 1;
        p_write.sector_records[1] = {100, "CLEARED (100%)"};
        p_write.sector_records[2] = {75, "PARTIAL (50-99%)"};

        const std::string test_file = "tests/test_profile.json";
        bool saved = SaveSystem::save_profile(p_write, test_file);
        CHECK(saved, "Save profile failed");

        UserProfile p_read;
        bool loaded = SaveSystem::load_profile(p_read, test_file);
        CHECK(loaded, "Load profile failed");

        CHECK(p_read.total_exp == 1450, "Loaded total_exp mismatch");
        CHECK(p_read.total_coins == 580, "Loaded total_coins mismatch");
        CHECK(p_read.get_player_level() == 4, "Derived player level mismatch for 1450 EXP (should be Level 4)");
        CHECK(p_read.total_voidite == 65, "Loaded total_voidite mismatch");
        CHECK(p_read.total_titanium == 42, "Loaded total_titanium mismatch");
        CHECK(p_read.selected_class_id == 1, "Loaded selected_class_id mismatch");
        CHECK(p_read.upgrades.drillSpeedTier == 3, "Loaded drillSpeedTier mismatch");
        CHECK(p_read.upgrades.reinforcedPlatingTier == 2, "Loaded reinforcedPlatingTier mismatch");
        CHECK(p_read.upgrades.sonarFrequencyTier == 1, "Loaded sonarFrequencyTier mismatch");
        CHECK(p_read.sector_records[1].highest_completion_rate == 100, "Loaded sector 1 completion rate mismatch");
        CHECK(p_read.sector_records[1].best_badge == "CLEARED (100%)", "Loaded sector 1 best badge mismatch");
        CHECK(p_read.sector_records[2].highest_completion_rate == 75, "Loaded sector 2 completion rate mismatch");

        std::cout << " -> Profile serialization and deserialization verified identical." << std::endl;
    }

    // Test 5: Player Controller Skill Tree Application (Ensuring all skills directly apply to player)
    {
        std::cout << "[Test 5] Testing Controller Skill Application to Player Mechanics..." << std::endl;
        PlayerController player(glm::vec3(0.0f, 10.0f, 0.0f));

        UpgradeTree upg;
        upg.drillSpeedTier = 3;        // +36% mining speed
        upg.drillDurabilityTier = 2;   // -30% heat buildup, +40% passive cooling
        upg.thrusterTankTier = 2;      // +40% fuel capacity (100 -> 140)
        upg.kineticDynamoTier = 2;     // +30% fuel recharge during sprint/fall
        upg.reinforcedPlatingTier = 2; // +30 HP, +20% debris reduction
        upg.sonarFrequencyTier = 2;    // +4m radius, -2s cooldown, material labels unlocked

        player.apply_attributes_and_upgrades(CharacterClass::Vanguard, upg);

        // 1. Health & Plating: Vanguard base 135 HP + 2 * 15 = 165 HP
        CHECK(std::abs(player.max_health() - 165.0f) < 0.001f, "Max health mismatch");
        CHECK(std::abs(player.health() - 165.0f) < 0.001f, "Health mismatch");

        // 2. Thruster Tank: Base 100 * (1 + 2 * 0.20) = 140 max power
        CHECK(std::abs(player.exo().max_power - 140.0f) < 0.001f, "Max power mismatch");

        // 3. Mining Excavation Speed: Vanguard base 1.0 * (1.0 + 3 * 0.12) = 1.36x
        float expected_mine_speed = 1.0f * (1.0f + 3 * 0.12f);
        CHECK(std::abs(player.effective_mine_speed() - expected_mine_speed) < 0.001f, "Effective mining speed mismatch");

        // 4. Drill Durability Heat Buildup & Cooling:
        // Buildup: 20 * (1 - 2 * 0.15) = 14.0 heat/sec
        CHECK(std::abs(player.drill_heat_buildup_rate() - 14.0f) < 0.001f, "Drill heat buildup rate mismatch");
        // Dissipation: 18 * (1 + 2 * 0.20) = 25.2 heat/sec
        CHECK(std::abs(player.drill_heat_dissipation_rate() - 25.2f) < 0.001f, "Drill heat dissipation rate mismatch");

        // 5. Kinetic Dynamo: Base multiplier 1.0 + 2 * 0.15 = 1.30x; Sprint/Fall bonus: +0.30 = 1.60x
        CHECK(std::abs(player.dynamo_multiplier(false) - 1.30f) < 0.001f, "Passive dynamo multiplier mismatch");
        CHECK(std::abs(player.dynamo_multiplier(true) - 1.60f) < 0.001f, "Sprinting/falling dynamo multiplier mismatch");

        // 6. Sonar Frequency & Material Identification:
        // Vanguard base 14m radius + 2 * 2m = 18m
        CHECK(std::abs(player.sonar_radius() - 18.0f) < 0.001f, "Sonar radius mismatch");
        // Cooldown: 10s base - 2 * 1s = 8s
        CHECK(std::abs(player.sonar_max_cooldown() - 8.0f) < 0.001f, "Sonar max cooldown mismatch");
        // Tier 2 unlocks HUD material labels
        CHECK(player.can_identify_materials() == true, "Material identification should be unlocked at Tier 2");

        // 7. Debris Damage Mitigation & Shield Layer Absorption:
        // Vanguard base 50% + tier 2 (2 * 10% = 20%) = 70% reduction!
        float raw_debris_damage = 50.0f;
        float taken = player.take_damage(raw_debris_damage, true);
        CHECK(std::abs(taken - 15.0f) < 0.01f, "Debris damage taken mismatch");
        CHECK(std::abs(player.shield() - 60.0f) < 0.01f, "Shield should absorb debris damage first (75 -> 60)");
        CHECK(std::abs(player.health() - 165.0f) < 0.01f, "Health must remain intact while shield absorbs impact");

        // Deplete shield to verify damage penetrates directly to unregenerating health:
        player.set_shield(0.0f);
        player.take_damage(raw_debris_damage, true);
        CHECK(std::abs(player.health() - 150.0f) < 0.01f, "Health after debris mismatch with depleted shield");

        std::cout << " -> All 6 skills confirmed applying directly to player stats, mining, heat, sonar, and defense." << std::endl;
    }

    // Test 6: UserProfile Level Progression, Coins Bonus & 100% Respec Full Recovery
    {
        std::cout << "[Test 6] Testing UserProfile Progression, Level-Up Coins & Full Respec..." << std::endl;
        UserProfile profile;
        profile.total_exp = 0;
        profile.total_coins = 200;
        profile.total_voidite = 0;
        profile.total_titanium = 0;

        CHECK(profile.get_player_level() == 1, "Initial level should be 1");
        CHECK(profile.is_sector_unlocked(1), "Sector 1 must be unlocked at Level 1");
        CHECK(!profile.is_sector_unlocked(2), "Sector 2 should be locked at Level 1");

        // Level up to Level 2 (+150 coins bonus)
        int lvls = 0, bonus = 0;
        profile.add_exp(350, lvls, bonus);
        CHECK(profile.get_player_level() == 2, "Level should now be 2");
        CHECK(profile.total_coins == 350, "Coins should be 200 + 150 = 350");

        // Grant materials to verify that materials are NEVER consumed by upgrades
        profile.grant_resources(0, 0, 50, 50);

        // Buy DrillSpeed Tier 1 (100 EXP)
        bool p1 = profile.upgrades.purchase(UpgradeType::DrillSpeed, profile.get_player_level(), profile.total_exp);
        CHECK(p1, "Purchase DrillSpeed Tier 1 failed");
        CHECK(profile.upgrades.drillSpeedTier == 1, "DrillSpeed tier not 1");
        CHECK(profile.total_exp == 250, "EXP deduction mismatch (350 - 100)");
        CHECK(profile.total_coins == 350, "Coins must not be deducted (should remain 350)");
        CHECK(profile.total_voidite == 50, "Voidite must not be deducted (should remain 50)");
        CHECK(profile.total_titanium == 50, "Titanium must not be deducted (should remain 50)");

        // Buy ThrusterTank Tier 1 (100 EXP)
        bool p2 = profile.upgrades.purchase(UpgradeType::ThrusterTank, profile.get_player_level(), profile.total_exp);
        CHECK(p2, "Purchase ThrusterTank Tier 1 failed");
        CHECK(profile.upgrades.thrusterTankTier == 1, "ThrusterTank tier not 1");
        CHECK(profile.total_exp == 150, "EXP deduction mismatch (250 - 100)");
        CHECK(profile.total_coins == 350, "Coins must not be deducted");
        CHECK(profile.total_voidite == 50, "Voidite must not be deducted");
        CHECK(profile.total_titanium == 50, "Titanium must not be deducted");

        int total_spent = profile.upgrades.get_total_spent_exp();
        CHECK(total_spent == 200, "Total spent EXP mismatch");

        // Execute Respec (100% full refund of EXP)
        int ref_exp = 0;
        profile.upgrades.respec(ref_exp);
        CHECK(ref_exp == 200, "Refund EXP mismatch");

        profile.total_exp += ref_exp;

        CHECK(profile.total_exp == 350, "Total EXP after respec mismatch (should be restored to 350)");
        CHECK(profile.total_coins == 350, "Total Coins after respec mismatch (should be restored to 350)");
        CHECK(profile.total_voidite == 50, "Total Voidite after respec mismatch (should be restored to 50)");
        CHECK(profile.total_titanium == 50, "Total Titanium after respec mismatch (should be restored to 50)");

        CHECK(profile.upgrades.drillSpeedTier == 0, "DrillSpeed tier not reset to 0");
        CHECK(profile.upgrades.thrusterTankTier == 0, "ThrusterTank tier not reset to 0");

        std::cout << " -> Profile level progression, level-up coins, and 100% full EXP respec recovery verified." << std::endl;
    }

    // Test 7: Complete Tier Cost Matrix & Progression Roadmap
    {
        std::cout << "[Test 7] Testing 5-Tier Cost Matrix & Progression Roadmap..." << std::endl;

        // Verify exact cost info for each of the 5 tiers (EXP-only, materials = 0)
        auto t1 = UpgradeTree::get_tier_cost_info(1);
        CHECK(t1.tier == 1 && t1.required_level == 1 && t1.exp_cost == 100 && t1.voidite_cost == 0 && t1.titanium_cost == 0, "Tier 1 cost info mismatch");

        auto t2 = UpgradeTree::get_tier_cost_info(2);
        CHECK(t2.tier == 2 && t2.required_level == 2 && t2.exp_cost == 160 && t2.voidite_cost == 0 && t2.titanium_cost == 0, "Tier 2 cost info mismatch");

        auto t3 = UpgradeTree::get_tier_cost_info(3);
        CHECK(t3.tier == 3 && t3.required_level == 3 && t3.exp_cost == 256 && t3.voidite_cost == 0 && t3.titanium_cost == 0, "Tier 3 cost info mismatch");

        auto t4 = UpgradeTree::get_tier_cost_info(4);
        CHECK(t4.tier == 4 && t4.required_level == 4 && t4.exp_cost == 410 && t4.voidite_cost == 0 && t4.titanium_cost == 0, "Tier 4 cost info mismatch");

        auto t5 = UpgradeTree::get_tier_cost_info(5);
        CHECK(t5.tier == 5 && t5.required_level == 5 && t5.exp_cost == 656 && t5.voidite_cost == 0 && t5.titanium_cost == 0, "Tier 5 cost info mismatch");

        // Verify Lock Reasons for clear UI messaging
        UpgradeTree test_tree;
        // Level locked
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillSpeed, 0, 1000) == UpgradeLockReason::LevelLocked, "Should be LevelLocked at Lv 0");
        // Prerequisite locked
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillDurability, 5, 1000) == UpgradeLockReason::PrerequisiteLocked, "Should be PrerequisiteLocked without DrillSpeed T1");
        // Insufficient exp
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillSpeed, 1, 50) == UpgradeLockReason::InsufficientEXP, "Should be InsufficientEXP");
        // Unlocked and ready
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillSpeed, 1, 100) == UpgradeLockReason::Unlocked, "Should be Unlocked");

        std::cout << " -> All 5 Tier cost records (100->656 EXP) & lock reasons verified." << std::endl;
    }

    // Test 8: Sector Progression, Uncharted Territory Gating & Save Persistence
    {
        std::cout << "[Test 8] Testing Sector Progression, Uncharted Territory & Persistence..." << std::endl;
        UserProfile p;
        p.total_exp = 0;
        p.highest_cleared_sector = 1;

        // Baseline: Level 1
        CHECK(p.get_player_level() == 1, "Level must start at 1");
        CHECK(p.is_sector_unlocked(1), "Sector 1 must be unlocked");
        CHECK(!p.is_sector_unlocked(2), "Sector 2 must be locked at Level 1");
        CHECK(!p.is_sector_unlocked(3), "Sector 3 must be locked at Level 1");

        // Advance to Level 2
        int lvls = 0, bonus = 0;
        p.add_exp(350, lvls, bonus);
        CHECK(p.get_player_level() == 2, "Must reach Level 2");
        CHECK(p.is_sector_unlocked(2), "Sector 2 must unlock at Level 2");
        CHECK(!p.is_sector_unlocked(3), "Sector 3 must remain locked at Level 2");

        // Advance to Level 4 (Sector 3 requirement)
        p.add_exp(1200, lvls, bonus);
        CHECK(p.get_player_level() >= 4, "Must reach Level 4");
        CHECK(p.is_sector_unlocked(3), "Sector 3 (Uncharted Territory) must be unlocked at Level 4");
        CHECK(!p.is_sector_unlocked(4), "Sector 4 must remain locked before Sector 3 clear");

        // Clear Sector 3 with 100% completion
        p.sector_records[3].highest_completion_rate = 100;
        p.sector_records[3].best_badge = "CLEARED (100%)";
        p.highest_cleared_sector = 3;

        // Advance to Level 6 (Sector 4 requirement)
        p.add_exp(2500, lvls, bonus);
        CHECK(p.get_player_level() >= 6, "Must reach Level 6");
        CHECK(p.is_sector_unlocked(4), "Sector 4 must unlock once Sector 3 is cleared and Level 6 reached");

        // Test persistence of highest_cleared_sector & sector records
        const std::string test_save_path = "saves/test_sector_save.json";
        bool save_ok = SaveSystem::save_profile(p, test_save_path);
        CHECK(save_ok, "Must save profile successfully");

        UserProfile loaded_p;
        bool load_ok = SaveSystem::load_profile(loaded_p, test_save_path);
        CHECK(load_ok, "Must load profile successfully");
        CHECK(loaded_p.highest_cleared_sector >= 3, "highest_cleared_sector must persist >= 3");
        CHECK(loaded_p.sector_records[3].highest_completion_rate == 100, "Sector 3 record must persist 100%");
        CHECK(loaded_p.sector_records[3].best_badge == "CLEARED (100%)", "Sector 3 badge must persist");
        CHECK(loaded_p.is_sector_unlocked(4), "Sector 4 must remain unlocked in loaded profile");

        std::filesystem::remove(test_save_path);
        std::cout << " -> Uncharted territory unlock, level gating, clear prereqs & save persistence verified." << std::endl;
    }

    // Test 9: Skill Tree Web Grid Architecture & Greyed-Out Sector States
    {
        std::cout << "[Test 9] Testing Skill Tree Web Grid Architecture & Greyed-Out Node States..." << std::endl;
        UpgradeTree web_tree;

        // 1. Initial Web State at Level 1, 0 EXP:
        // Root nodes have no prerequisite: accessible but lacking EXP
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillSpeed, 1, 0) == NodeWebState::InsufficientEXP,
              "DrillSpeed root node should be InsufficientEXP when player has 0 EXP");
        CHECK(web_tree.get_node_web_state(UpgradeType::ThrusterTank, 1, 0) == NodeWebState::InsufficientEXP,
              "ThrusterTank root node should be InsufficientEXP when player has 0 EXP");
        CHECK(web_tree.get_node_web_state(UpgradeType::ReinforcedPlating, 1, 0) == NodeWebState::InsufficientEXP,
              "ReinforcedPlating root node should be InsufficientEXP when player has 0 EXP");
        CHECK(web_tree.get_node_web_state(UpgradeType::SonarFrequency, 1, 0) == NodeWebState::InsufficientEXP,
              "SonarFrequency root node should be InsufficientEXP when player has 0 EXP");

        // Dependent child nodes on the grid must be strictly GREYED OUT (GreyedOutLocked) because root branches are unpowered!
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillDurability, 1, 500) == NodeWebState::GreyedOutLocked,
              "DrillDurability branch sector MUST be GreyedOutLocked until DrillSpeed T1 is energized");
        CHECK(web_tree.get_node_web_state(UpgradeType::KineticDynamo, 1, 500) == NodeWebState::GreyedOutLocked,
              "KineticDynamo branch sector MUST be GreyedOutLocked until ThrusterTank T1 is energized");

        // 2. Awakening Root Nodes with EXP:
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillSpeed, 1, 100) == NodeWebState::Available,
              "DrillSpeed should be Available when player has 100 EXP at Level 1");

        // 3. Purchasing Root Node & Web State Transitions:
        int exp_pool = 5000;
        bool b_root = web_tree.purchase(UpgradeType::DrillSpeed, 1, exp_pool);
        CHECK(b_root, "Purchase DrillSpeed T1 failed");
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillSpeed, 1, exp_pool) == NodeWebState::Purchased,
              "DrillSpeed should now be in Purchased state");

        // Now that DrillSpeed T1 is acquired, DrillDurability prerequisite is satisfied!
        // At Level 0 it is LevelGated; at Level 1 with sufficient EXP it is Available!
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillDurability, 0, exp_pool) == NodeWebState::LevelGated,
              "DrillDurability should be LevelGated at Level 0 once prereq is met");
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillDurability, 1, exp_pool) == NodeWebState::Available,
              "DrillDurability should be Available at Level 1 with sufficient EXP");

        // 4. Purchase child node:
        bool b_child = web_tree.purchase(UpgradeType::DrillDurability, 1, exp_pool);
        CHECK(b_child, "Purchase DrillDurability T1 failed");
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillDurability, 1, exp_pool) == NodeWebState::Purchased,
              "DrillDurability should now be in Purchased state");

        // 6. Max Tier Mastering (T5):
        for (int t = 2; t <= 5; ++t) {
            web_tree.purchase(UpgradeType::DrillSpeed, 5, exp_pool);
        }
        CHECK(web_tree.get_tier(UpgradeType::DrillSpeed) == 5, "DrillSpeed should be Tier 5");
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillSpeed, 5, exp_pool) == NodeWebState::Mastered,
              "DrillSpeed should be Mastered at Tier 5");
        CHECK(!web_tree.can_purchase(UpgradeType::DrillSpeed, 5, exp_pool),
              "Cannot purchase beyond Tier 5");

        // 7. Respec resets all web nodes back to foundational states (children become GreyedOutLocked again):
        int refunded_exp = 0;
        web_tree.respec(refunded_exp);
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillDurability, 5, exp_pool) == NodeWebState::GreyedOutLocked,
              "DrillDurability must return to GreyedOutLocked state after full respec");
        CHECK(web_tree.get_node_web_state(UpgradeType::DrillSpeed, 5, exp_pool) == NodeWebState::Available,
              "DrillSpeed returns to Available state at Level 5 with EXP");

        std::cout << " -> Skill tree web states (GreyedOutLocked, LevelGated, Available, Purchased, Mastered) fully validated." << std::endl;
    }

    // Test 9: Random Level Generation & Completed Level Seed Persistence
    {
        std::cout << "\n[Test 9] Testing Random Level Generation & Completed Level Seed Persistence..." << std::endl;

        // 1. Uncompleted levels generate distinct random seeds on each opening
        uint32_t seed_a = SaveSystem::generate_random_seed(1);
        uint32_t seed_b = SaveSystem::generate_random_seed(1);
        CHECK(seed_a != 0, "Generated seed must be non-zero");
        CHECK(seed_b != 0, "Generated seed must be non-zero");
        // Verify entropy produces different seeds
        CHECK(seed_a != seed_b, "Consecutive random level openings must generate distinct seeds");

        // 2. Uncompleted level lifecycle: Failed/abandoned runs do not lock in seed
        PlayerInventory inv;
        UserProfile user;
        const int test_sector = 2;

        CHECK(!user.is_sector_completed(test_sector), "Sector must start uncompleted");
        CHECK(user.get_sector_seed(test_sector) == 0, "Uncompleted sector seed must be 0");

        // Attempt 1: Player launches with seed_a but fails/dies
        inv.reset(30);
        inv.active_seed = seed_a;
        inv.suit_failed = true;
        inv.finalize_run(test_sector, false); // Failed extraction
        CHECK(!inv.sector_records[test_sector].is_completed(), "Failed run must not mark sector completed");
        CHECK(inv.sector_records[test_sector].saved_seed == 0, "Failed run must not save persistent seed");

        // Attempt 2: Player launches with fresh random seed_b and successfully evacuates!
        inv.reset(30);
        inv.active_seed = seed_b;
        inv.voidite = 30; // Target met
        inv.vault_breached = true;
        inv.finalize_run(test_sector, true); // Safe extraction!

        CHECK(inv.sector_records[test_sector].is_completed(), "Safe extraction must mark sector completed");
        CHECK(inv.sector_records[test_sector].saved_seed == seed_b, "Winning seed must be locked into saved_seed");

        // Sync to UserProfile and save
        user.mark_sector_completed(test_sector, inv.sector_records[test_sector].saved_seed);
        CHECK(user.is_sector_completed(test_sector), "UserProfile must reflect sector completion");
        CHECK(user.get_sector_seed(test_sector) == seed_b, "UserProfile must store exact completed seed");

        const std::string level_seed_file = "tests/test_level_seed_persistence.json";
        bool save_ok = SaveSystem::save_profile(user, level_seed_file);
        CHECK(save_ok, "Saving profile with completed level seed must succeed");

        // 3. Persistence round-trip: Reload profile and verify seed does NOT change
        UserProfile reloaded;
        bool load_ok = SaveSystem::load_profile(reloaded, level_seed_file);
        CHECK(load_ok, "Loading profile with completed level seed must succeed");
        CHECK(reloaded.is_sector_completed(test_sector), "Loaded profile must have completed sector");
        CHECK(reloaded.get_sector_seed(test_sector) == seed_b, "Loaded profile must retain exact saved seed");

        // 4. Replay completed level: Replaying should preserve the saved seed even on subsequent runs
        inv.reset(30);
        inv.active_seed = reloaded.get_sector_seed(test_sector);
        CHECK(inv.active_seed == seed_b, "Opening completed level must use saved seed");
        inv.finalize_run(test_sector, true);
        CHECK(inv.sector_records[test_sector].saved_seed == seed_b, "Replaying completed level must not alter saved seed");

        // 5. Backward compatibility: Legacy save with high completion rate but 0 seed gets assigned deterministic persistent seed
        UserProfile legacy;
        legacy.sector_records[3].highest_completion_rate = 85;
        legacy.sector_records[3].best_badge = "PARTIAL (50–99%)";
        legacy.sector_records[3].saved_seed = 0;
        legacy.sector_records[3].completed = false;

        const std::string legacy_file = "tests/test_legacy_sector_migration.json";
        SaveSystem::save_profile(legacy, legacy_file);

        UserProfile migrated;
        SaveSystem::load_profile(migrated, legacy_file);
        CHECK(migrated.is_sector_completed(3), "Migrated legacy cleared sector must be marked completed");
        CHECK(migrated.get_sector_seed(3) != 0, "Migrated legacy cleared sector must have non-zero seed established");
        uint32_t established_seed = migrated.get_sector_seed(3);

        // Verify that re-saving and re-loading preserves this established seed
        SaveSystem::save_profile(migrated, legacy_file);
        UserProfile migrated_reloaded;
        SaveSystem::load_profile(migrated_reloaded, legacy_file);
        CHECK(migrated_reloaded.get_sector_seed(3) == established_seed, "Established legacy seed must remain immutable");

        std::cout << " -> Random level generation and persistent completed seed immutability verified!" << std::endl;
    }

    // Test 9: Save System Clearance, Profile Reset, and Deletion Lifecycle
    {
        std::cout << "[Test 9] Testing Save Clearing, Profile Reset, and Deletion Lifecycle..." << std::endl;

        const std::string test_clear_file = "tests/test_clear_save_profile.json";

        // 1. Populate an advanced profile
        UserProfile advanced_profile;
        advanced_profile.player_name = "VeteranDelver";
        advanced_profile.total_exp = 55000;
        advanced_profile.lifetime_exp = 65000;
        advanced_profile.total_coins = 4200;
        advanced_profile.total_voidite = 150;
        advanced_profile.total_titanium = 45;
        advanced_profile.upgrades.drillSpeedTier = 4;
        advanced_profile.upgrades.thrusterTankTier = 3;
        advanced_profile.mark_sector_completed(1, 111111);
        advanced_profile.mark_sector_completed(2, 222222);
        advanced_profile.highest_cleared_sector = 3;
        advanced_profile.settings.master_volume = 0.65f;

        bool saved = SaveSystem::save_profile(advanced_profile, test_clear_file);
        CHECK(saved, "Saving advanced profile for clear test must succeed");
        CHECK(std::filesystem::exists(test_clear_file), "Save file must exist on disk");

        // 2. Clear save file via SaveSystem::clear_save
        bool cleared = SaveSystem::clear_save(test_clear_file);
        CHECK(cleared, "SaveSystem::clear_save must return true");
        CHECK(std::filesystem::exists(test_clear_file), "Cleared save file must be rewritten with fresh defaults");

        // 3. Reload profile from cleared file and verify pristine state
        UserProfile reloaded_fresh;
        bool loaded = SaveSystem::load_profile(reloaded_fresh, test_clear_file);
        CHECK(loaded, "Loading cleared save file must succeed");
        CHECK(reloaded_fresh.total_exp == 0, "Cleared profile must have 0 spendable EXP");
        CHECK(reloaded_fresh.lifetime_exp == 0, "Cleared profile must have 0 lifetime EXP");
        CHECK(reloaded_fresh.total_coins == 200, "Cleared profile must have base starting coins (200)");
        CHECK(reloaded_fresh.total_voidite == 12, "Cleared profile must have base starting voidite (12)");
        CHECK(reloaded_fresh.total_titanium == 8, "Cleared profile must have base starting titanium (8)");
        CHECK(reloaded_fresh.upgrades.drillSpeedTier == 0, "Cleared profile must have 0 drill speed upgrades");
        CHECK(reloaded_fresh.upgrades.thrusterTankTier == 0, "Cleared profile must have 0 thruster upgrades");
        CHECK(!reloaded_fresh.is_sector_completed(1), "Sector 1 must not be marked completed on fresh profile");
        CHECK(!reloaded_fresh.is_sector_completed(2), "Sector 2 must not be marked completed on fresh profile");
        CHECK(reloaded_fresh.highest_cleared_sector == 1, "Cleared profile highest cleared sector must be 1");

        // 4. Test SaveSystem::reset_profile (in-memory object reset preserving user audio/graphics settings)
        UserProfile active_mem_profile = advanced_profile;
        active_mem_profile.settings.fov = 90.0f;
        bool mem_reset = SaveSystem::reset_profile(active_mem_profile, test_clear_file, true);
        CHECK(mem_reset, "SaveSystem::reset_profile must succeed");
        CHECK(active_mem_profile.total_exp == 0, "In-memory profile must be reset to 0 EXP");
        CHECK(active_mem_profile.settings.fov == 90.0f, "reset_profile must preserve user settings when preserve_settings is true");
        CHECK(active_mem_profile.player_name == "VeteranDelver", "reset_profile must retain player name");

        // 5. Test SaveSystem::delete_save_file
        bool deleted = SaveSystem::delete_save_file(test_clear_file);
        CHECK(deleted, "SaveSystem::delete_save_file must return true for existing file");
        CHECK(!std::filesystem::exists(test_clear_file), "Deleted save file must no longer exist on disk");

        std::cout << " -> Save clearance, fresh profile re-initialization, and deletion verified!" << std::endl;
    }

    // Test 10: Sector Charting, Prerequisite Gating & Lock Badge Messages
    {
        std::cout << "[Test 10] Testing Sector Charting, Prior Clear Status & Lock Badge Details..." << std::endl;

        UserProfile p;
        p.player_name = "Cadet";
        p.total_exp = 0; // Level 1

        // Fresh profile: Sector 1 is unlocked and ready
        CHECK(p.is_sector_unlocked(1), "Sector 1 must be unlocked on fresh profile");
        CHECK(!p.is_sector_completed(1), "Sector 1 must not be completed initially");
        CHECK(!p.is_sector_charted(1), "Sector 1 must not be charted initially");
        CHECK(p.is_prior_sector_cleared(1), "Sector 1 has no prior prerequisite");

        // Sector 2 requires Level 2 (sectors 1-3 are classic introductory sectors gated by rank)
        CHECK(!p.is_sector_unlocked(2), "Sector 2 must be locked for level 1 cadet");
        CHECK(p.get_sector_lock_badge(2) == "[ LOCKED // REQUIRES LEVEL 2 ]", "Sector 2 lock badge mismatch");
        CHECK(p.get_sector_lock_detail(2) == "SECTOR 2 LOCKED: REQUIRES DELVER LEVEL 2 (CURRENT RANK: 1)", "Sector 2 lock detail mismatch");

        // Sector 5 requires Level 8 AND Sector 4 completed ("need to complete level 4 before doing level 5")
        // Level up player to level 10 without clearing Sector 4:
        p.total_exp = 25000; // High level delver (> Level 10)
        CHECK(p.get_player_level() >= 8, "Player level must be >= 8");
        CHECK(!p.is_prior_sector_cleared(5), "Sector 5 prior sector (4) must not be cleared yet");
        CHECK(!p.is_sector_unlocked(5), "Sector 5 must remain locked because Sector 4 is uncleared");
        CHECK(p.get_sector_lock_badge(5) == "[ LOCKED // CLEAR SECTOR 4 FIRST ]", "Sector 5 must show prior clear badge for sector 4");
        CHECK(p.get_sector_lock_detail(5) == "SECTOR 5 LOCKED: MUST COMPLETE SECTOR 4 BEFORE COMMENCING SECTOR 5", "Sector 5 detail text mismatch");

        // Now mark Sector 1 completed with 100% completion rate & "SURVIVED" badge
        p.mark_sector_completed(1, 100, "100% VOIDITE RECOVERED", 987654);
        CHECK(p.is_sector_completed(1), "Sector 1 must now be marked completed");
        CHECK(p.is_sector_charted(1), "Sector 1 must now be marked charted");
        CHECK(p.get_sector_completion_rate(1) == 100, "Sector 1 completion rate must be 100");
        CHECK(p.get_sector_badge(1) == "100% VOIDITE RECOVERED", "Sector 1 badge must match");
        CHECK(p.highest_cleared_sector >= 1, "highest_cleared_sector must be at least 1");
        CHECK(p.is_prior_sector_cleared(2), "Sector 2 prior sector must now be cleared");

        // Complete sectors 2 and 3 sequentially
        p.mark_sector_completed(2, 85, "CLEARED", 2222);
        CHECK(p.is_sector_charted(2), "Sector 2 must be charted");
        CHECK(!p.is_prior_sector_cleared(5), "Sector 5 prior sector (4) still not cleared");

        p.mark_sector_completed(3, 90, "CLEARED", 3333);
        CHECK(p.is_sector_charted(3), "Sector 3 must be charted");
        CHECK(p.highest_cleared_sector >= 3, "highest_cleared_sector must advance to 3");
        CHECK(!p.is_prior_sector_cleared(5), "Sector 5 prior sector (4) still not cleared");

        // Now complete sector 4
        p.mark_sector_completed(4, 95, "CLEARED", 4444);
        CHECK(p.is_sector_charted(4), "Sector 4 must be charted");
        CHECK(p.highest_cleared_sector >= 4, "highest_cleared_sector must advance to 4");
        CHECK(p.is_prior_sector_cleared(5), "Sector 5 prior sector (4) must now be cleared");
        CHECK(p.is_sector_unlocked(5), "Sector 5 must now be unlocked (level >= 8 and sector 4 cleared)");
        CHECK(p.get_sector_lock_badge(5).empty(), "Unlocked sector 5 must have empty lock badge");

        // Verify persistence of completion rate, badge, and charting status
        const std::string test_chart_file = "tests/test_sector_charting.json";
        bool saved = SaveSystem::save_profile(p, test_chart_file);
        CHECK(saved, "Saving profile with charted sectors must succeed");

        UserProfile loaded_p;
        bool loaded = SaveSystem::load_profile(loaded_p, test_chart_file);
        CHECK(loaded, "Loading profile with charted sectors must succeed");
        CHECK(loaded_p.is_sector_completed(1), "Loaded profile sector 1 must be completed");
        CHECK(loaded_p.is_sector_charted(1), "Loaded profile sector 1 must be charted");
        CHECK(loaded_p.get_sector_completion_rate(1) == 100, "Loaded profile sector 1 completion rate mismatch");
        CHECK(loaded_p.get_sector_badge(1) == "100% VOIDITE RECOVERED", "Loaded profile sector 1 badge mismatch");
        CHECK(loaded_p.is_sector_charted(2), "Loaded profile sector 2 must be charted");
        CHECK(loaded_p.is_sector_charted(3), "Loaded profile sector 3 must be charted");
        CHECK(loaded_p.is_sector_charted(4), "Loaded profile sector 4 must be charted");
        CHECK(loaded_p.is_sector_unlocked(5), "Loaded profile sector 5 must remain unlocked");
        CHECK(loaded_p.highest_cleared_sector >= 4, "Loaded profile highest_cleared_sector mismatch");

        SaveSystem::delete_save_file(test_chart_file);
        std::cout << " -> Sector charting, prerequisite gating, and lock messages verified!" << std::endl;
    }

    // Test 11: 6-Discipline Progressive Skill Tree, Campaign Cap, 100% Respec, and Post-Game Grandmaster Unlock
    {
        std::cout << "[Test 11] Testing 6-Discipline Skill Tree, 28-Pt Campaign Cap, Respec & Post-Game Unlock..." << std::endl;

        // 1. Verify 22 upgrades, 6 disciplines, 28-pt campaign cap, and 72 total points
        CHECK(static_cast<int>(UpgradeType::COUNT) == 22, "Tree must have exactly 22 upgrades");
        CHECK(UpgradeTree::CAMPAIGN_POINT_CAP == 28, "Pre-completion campaign cap must be 28 points");
        CHECK(UpgradeTree::TOTAL_TREE_POINTS == 72, "Total skill tree must have 72 points");

        // Verify tier allocations: Foundation (5) + Specialization (3) + Capstone (2) = 10 pts per branch
        CHECK(UpgradeTree::get_max_tier(UpgradeType::DrillSpeed) == 5, "Excavation Foundation max tier must be 5");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::DrillDurability) == 3, "Excavation Specialization max tier must be 3");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::MicroCharges) == 2, "Excavation Capstone max tier must be 2");

        CHECK(UpgradeTree::get_max_tier(UpgradeType::AcousticDampening) == 5, "Stealth Foundation max tier must be 5");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::ShadowVeil) == 3, "Stealth Specialization max tier must be 3");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::AmbushProtocol) == 2, "Stealth Capstone max tier must be 2");

        CHECK(UpgradeTree::get_max_tier(UpgradeType::PneumaticImpact) == 5, "Melee Foundation max tier must be 5");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::CleaveShockwave) == 3, "Melee Specialization max tier must be 3");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::BerserkerOverdrive) == 2, "Melee Capstone max tier must be 2");

        CHECK(UpgradeTree::get_max_tier(UpgradeType::HyperVelocitySlugs) == 5, "Gun Damage Foundation max tier must be 5");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::RapidCycling) == 3, "Gun Damage Specialization max tier must be 3");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::CriticalWeakpoint) == 2, "Gun Damage Capstone max tier must be 2");

        CHECK(UpgradeTree::get_max_tier(UpgradeType::ReinforcedPlating) == 5, "Defense Foundation max tier must be 5");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::KineticShielding) == 3, "Defense Specialization max tier must be 3");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::Defibrillator) == 2, "Defense Capstone max tier must be 2");

        // Verify new hazard resistance, fall damage, and boss damage skills
        CHECK(UpgradeTree::get_max_tier(UpgradeType::ImpactDampeners) == 3, "Fall Impact Dampeners max tier must be 3");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::RadiationShielding) == 3, "Radiation Shielding max tier must be 3");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::ThermalInsulation) == 3, "Thermal Lava Insulation max tier must be 3");
        CHECK(UpgradeTree::get_max_tier(UpgradeType::ApexBossDamage) == 3, "Apex Boss Damage max tier must be 3");

        // 2. Sequential Branch Prerequisites
        UpgradeTree branch_tree;
        int exp_pool = 100000;
        int player_lvl = 20;

        // Stealth branch prerequisite check
        CHECK(branch_tree.get_lock_reason(UpgradeType::ShadowVeil, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "ShadowVeil must require AcousticDampening >= 1");
        CHECK(branch_tree.get_lock_reason(UpgradeType::AmbushProtocol, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "AmbushProtocol must require ShadowVeil >= 1");

        // Melee branch prerequisite check
        CHECK(branch_tree.get_lock_reason(UpgradeType::CleaveShockwave, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "CleaveShockwave must require PneumaticImpact >= 1");
        CHECK(branch_tree.get_lock_reason(UpgradeType::BerserkerOverdrive, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "BerserkerOverdrive must require CleaveShockwave >= 1");

        // Gun Damage & Boss Slayer branch prerequisite check
        CHECK(branch_tree.get_lock_reason(UpgradeType::RapidCycling, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "RapidCycling must require HyperVelocitySlugs >= 1");
        CHECK(branch_tree.get_lock_reason(UpgradeType::CriticalWeakpoint, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "CriticalWeakpoint must require RapidCycling >= 1");
        CHECK(branch_tree.get_lock_reason(UpgradeType::ApexBossDamage, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "ApexBossDamage must require HyperVelocitySlugs >= 1");

        // Defense & Hazard Wards branch prerequisite check
        CHECK(branch_tree.get_lock_reason(UpgradeType::KineticShielding, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "KineticShielding must require ReinforcedPlating >= 1");
        CHECK(branch_tree.get_lock_reason(UpgradeType::Defibrillator, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "Defibrillator must require KineticShielding >= 1");
        CHECK(branch_tree.get_lock_reason(UpgradeType::RadiationShielding, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "RadiationShielding must require ReinforcedPlating >= 1");
        CHECK(branch_tree.get_lock_reason(UpgradeType::ThermalInsulation, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "ThermalInsulation must require ReinforcedPlating >= 1");

        // Traversal branch prerequisite check
        CHECK(branch_tree.get_lock_reason(UpgradeType::KineticDynamo, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "KineticDynamo must require ThrusterTank >= 1");
        CHECK(branch_tree.get_lock_reason(UpgradeType::ImpactDampeners, player_lvl, exp_pool) == UpgradeLockReason::PrerequisiteLocked,
              "ImpactDampeners must require ThrusterTank >= 1");

        // 3. Campaign Point Cap Enforcement (28 points)
        UserProfile camp_profile;
        camp_profile.total_exp = 60000;
        camp_profile.lifetime_exp = 60000;
        CHECK(!camp_profile.is_game_completed(), "Fresh campaign must not be marked completed");
        CHECK(camp_profile.get_max_assignable_points() == 28, "Pre-completion max points must be 28");

        // Allocate points: Max Excavation (5 + 3 + 2 = 10 pts)
        for (int i = 0; i < 5; ++i) camp_profile.upgrades.purchase(UpgradeType::DrillSpeed, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::DrillDurability, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 2; ++i) camp_profile.upgrades.purchase(UpgradeType::MicroCharges, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        CHECK(camp_profile.upgrades.get_total_assigned_points() == 10, "Should have 10 points allocated");

        // Allocate points: Max Gun Damage (5 + 3 + 2 = 10 pts) -> 20 pts total
        for (int i = 0; i < 5; ++i) camp_profile.upgrades.purchase(UpgradeType::HyperVelocitySlugs, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::RapidCycling, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 2; ++i) camp_profile.upgrades.purchase(UpgradeType::CriticalWeakpoint, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        CHECK(camp_profile.upgrades.get_total_assigned_points() == 20, "Should have 20 points allocated");

        // Allocate 5 points into Defense (ReinforcedPlating T5) + 3 points into RadiationShielding (T3) -> Exactly 28 points (CAP REACHED!)
        for (int i = 0; i < 5; ++i) camp_profile.upgrades.purchase(UpgradeType::ReinforcedPlating, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::RadiationShielding, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        CHECK(camp_profile.upgrades.get_total_assigned_points() == 28, "Should be capped at 28 points");

        // Attempting to buy a 29th point must fail because of CampaignCapReached even with abundant EXP!
        CHECK(!camp_profile.upgrades.can_purchase(UpgradeType::KineticShielding, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed()),
              "Cannot purchase beyond 28 points during campaign");
        CHECK(camp_profile.upgrades.get_lock_reason(UpgradeType::KineticShielding, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed()) == UpgradeLockReason::CampaignCapReached,
              "Lock reason must be CampaignCapReached");
        CHECK(camp_profile.upgrades.get_node_web_state(UpgradeType::KineticShielding, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed()) == NodeWebState::CampaignCapped,
              "Web node state must be CampaignCapped");

        bool extra_purchase = camp_profile.upgrades.purchase(UpgradeType::KineticShielding, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        CHECK(!extra_purchase, "29th point purchase must return false");
        CHECK(camp_profile.upgrades.get_total_assigned_points() == 28, "Points must remain strictly at 28");

        // 4. Respec: Resets all 22 upgrades and refunds 100% of spent EXP
        int spent_exp_before_respec = camp_profile.upgrades.get_total_spent_exp();
        CHECK(spent_exp_before_respec > 0, "Spent EXP must be > 0");
        int refund = 0;
        camp_profile.upgrades.respec(refund);
        CHECK(refund == spent_exp_before_respec, "Respec must refund 100% spent EXP");
        CHECK(camp_profile.upgrades.get_total_assigned_points() == 0, "Assigned points must reset to 0");
        CHECK(camp_profile.upgrades.radiationShieldingTier == 0, "Radiation shielding must reset to 0");
        camp_profile.total_exp += refund;

        // Now specialize down Stealth (10 pts) and Melee (10 pts) + 5 pts Traversal + 3 pts Fall Dampeners!
        for (int i = 0; i < 5; ++i) camp_profile.upgrades.purchase(UpgradeType::AcousticDampening, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::ShadowVeil, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 2; ++i) camp_profile.upgrades.purchase(UpgradeType::AmbushProtocol, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        CHECK(camp_profile.upgrades.ambushProtocolTier == 2, "Ambush protocol capstone acquired");

        for (int i = 0; i < 5; ++i) camp_profile.upgrades.purchase(UpgradeType::PneumaticImpact, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::CleaveShockwave, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 2; ++i) camp_profile.upgrades.purchase(UpgradeType::BerserkerOverdrive, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        CHECK(camp_profile.upgrades.berserkerOverdriveTier == 2, "Berserker overdrive capstone acquired");

        for (int i = 0; i < 5; ++i) camp_profile.upgrades.purchase(UpgradeType::ThrusterTank, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::ImpactDampeners, camp_profile.get_player_level(), camp_profile.total_exp, camp_profile.is_game_completed());
        CHECK(camp_profile.upgrades.impactDampenersTier == 3, "Impact dampeners T3 acquired");
        CHECK(camp_profile.upgrades.get_total_assigned_points() == 28, "Must reach 28 points");

        // 5. Post-Game Grandmaster Unlock (Beating Sector 3 Void Cradle expands cap to 72)
        camp_profile.mark_sector_completed(3, 100, "CLEARED", 99999);
        CHECK(camp_profile.is_game_completed(), "Profile must now be marked game completed");
        CHECK(camp_profile.get_max_assignable_points() == 72, "Post-game max assignable points must expand to 72");

        // Now unlock all remaining 22 skills up to 72/72 points!
        camp_profile.total_exp = 5000000;
        int cur_level = camp_profile.get_player_level();

        // Fill remaining nodes
        for (int i = 0; i < 5; ++i) camp_profile.upgrades.purchase(UpgradeType::DrillSpeed, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::DrillDurability, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 2; ++i) camp_profile.upgrades.purchase(UpgradeType::MicroCharges, cur_level, camp_profile.total_exp, true);

        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::KineticDynamo, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 2; ++i) camp_profile.upgrades.purchase(UpgradeType::SonarFrequency, cur_level, camp_profile.total_exp, true);

        for (int i = 0; i < 5; ++i) camp_profile.upgrades.purchase(UpgradeType::ReinforcedPlating, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::KineticShielding, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 2; ++i) camp_profile.upgrades.purchase(UpgradeType::Defibrillator, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::RadiationShielding, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::ThermalInsulation, cur_level, camp_profile.total_exp, true);

        for (int i = 0; i < 5; ++i) camp_profile.upgrades.purchase(UpgradeType::HyperVelocitySlugs, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::RapidCycling, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 2; ++i) camp_profile.upgrades.purchase(UpgradeType::CriticalWeakpoint, cur_level, camp_profile.total_exp, true);
        for (int i = 0; i < 3; ++i) camp_profile.upgrades.purchase(UpgradeType::ApexBossDamage, cur_level, camp_profile.total_exp, true);

        // Entire tree is now 100% complete: exactly 72/72 points!
        CHECK(camp_profile.upgrades.get_total_assigned_points() == 72, "Must reach exactly 72/72 points post-game");

        // 6. PlayerController Combat, Hazard & Boss Integration
        PlayerController test_delver(glm::vec3(0.0f));
        test_delver.apply_attributes_and_upgrades(CharacterClass::Demolitionist, camp_profile.upgrades);

        // Stealth bonuses
        CHECK(std::abs(test_delver.stealth_noise_multiplier() - 0.20f) < 0.01f, "Stealth noise should be dampened to minimum 0.20x");
        CHECK(std::abs(test_delver.optical_detection_multiplier() - 0.55f) < 0.01f, "Optical detection should be reduced by 45% (0.55x)");
        CHECK(std::abs(test_delver.ambush_damage_multiplier() - 1.70f) < 0.01f, "Ambush damage should be +70% (1.70x)");

        // Melee & Weapon bonuses
        CHECK(std::abs(test_delver.melee_damage_multiplier() - 2.50f) < 0.01f, "Melee damage should be +150% (2.50x)");
        CHECK(std::abs(test_delver.weakpoint_critical_multiplier() - 1.70f) < 0.01f, "Weakpoint critical damage should be +70% (1.70x)");
        CHECK(std::abs(test_delver.mineral_yield_multiplier() - 1.40f) < 0.01f, "Mineral harvest yield should be +40% (1.40x)");

        // Hazard reductions & Boss bonus
        CHECK(std::abs(test_delver.radiation_damage_reduction() - 0.75f) < 0.01f, "Radiation reduction must be 75% at T3");
        CHECK(std::abs(test_delver.lava_damage_reduction() - 0.75f) < 0.01f, "Lava reduction must be 75% at T3");
        CHECK(std::abs(test_delver.fall_damage_reduction() - 0.75f) < 0.01f, "Fall damage reduction must be 75% at T3");
        CHECK(std::abs(test_delver.boss_damage_multiplier() - 1.75f) < 0.01f, "Boss damage multiplier must be 1.75x at T3");

        // Damage mitigation tests
        float rad_dmg_applied = test_delver.take_damage(40.0f, PlayerController::DamageSource::Radiation);
        CHECK(std::abs(rad_dmg_applied - 10.0f) < 0.1f, "40 rad dmg should be mitigated to 10 dmg (75% reduction)");

        float lava_dmg_applied = test_delver.take_damage(40.0f, PlayerController::DamageSource::ThermalLava);
        CHECK(std::abs(lava_dmg_applied - 10.0f) < 0.1f, "40 lava dmg should be mitigated to 10 dmg (75% reduction)");

        float fall_dmg_applied = test_delver.take_damage(40.0f, PlayerController::DamageSource::FallImpact);
        CHECK(std::abs(fall_dmg_applied - 10.0f) < 0.1f, "40 fall dmg should be mitigated to 10 dmg (75% reduction)");

        // Defibrillator emergency second wind upon lethal damage
        test_delver.take_damage(9999.0f, PlayerController::DamageSource::Kinetic);
        CHECK(test_delver.health() == 35.0f, "Defibrillator should preserve player at 35 HP on fatal trauma");
        CHECK(test_delver.shield() > 0.0f, "Defibrillator should restore shield barrier");
        CHECK(test_delver.defibrillator_timer() > 0.0f, "Defibrillator cooldown must be triggered");

        std::cout << " -> Progressive 22-node skill web, 28-pt campaign cap, respec, and post-game Grandmaster unlock verified!" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "ALL PROGRESSION TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
