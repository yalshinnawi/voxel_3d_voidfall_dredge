#include <iostream>
#include <cmath>
#include <cstdlib>
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

    // Test 2: Exponential Coin Cost Formula, Level Gating & Branch Prerequisite Progression
    {
        std::cout << "[Test 2] Testing Coin Cost Formula, Level Gating & Branch Prerequisites..." << std::endl;
        CHECK(UpgradeTree::get_coin_cost(0) == 100, "Tier 0 coin cost mismatch");
        CHECK(UpgradeTree::get_coin_cost(1) == 160, "Tier 1 coin cost mismatch");
        CHECK(UpgradeTree::get_coin_cost(2) == 256, "Tier 2 coin cost mismatch");
        CHECK(UpgradeTree::get_coin_cost(3) == 410, "Tier 3 coin cost mismatch");
        CHECK(UpgradeTree::get_coin_cost(4) == 656, "Tier 4 coin cost mismatch");
        CHECK(UpgradeTree::get_coin_cost(5) == 0, "Tier 5 (max) coin cost mismatch");

        std::cout << " -> Exponential coin costs matched: 100 -> 160 -> 256 -> 410 -> 656 Coins." << std::endl;

        UpgradeTree tree;
        int coins = 1000;
        int voidite = 100;
        int titanium = 100;
        int player_level = 1;

        // Verify Branch Prerequisite: DrillDurability requires DrillSpeed >= 1
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "Initial drill speed tier not 0");
        CHECK(!tree.can_purchase(UpgradeType::DrillDurability, player_level, coins, voidite, titanium),
              "DrillDurability must not be purchasable before DrillSpeed T1 prerequisite is unlocked");
        CHECK(tree.get_lock_reason(UpgradeType::DrillDurability, player_level, coins, voidite, titanium) == UpgradeLockReason::PrerequisiteLocked,
              "Lock reason must be PrerequisiteLocked");

        // Level 1: Can purchase Root DrillSpeed Tier 1 (requires Level 1)
        CHECK(tree.can_purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium), "Cannot purchase tier 1 at level 1");
        bool bought = tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);
        CHECK(bought, "Purchase tier 1 failed");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 1, "Tier not 1 after purchase");
        CHECK(coins == 900, "Coins not deducted correctly (1000 - 100)");
        CHECK(voidite == 96, "Voidite not deducted correctly (100 - 4)");
        CHECK(titanium == 97, "Titanium not deducted correctly (100 - 3)");

        // Now that DrillSpeed T1 is acquired, DrillDurability prerequisite is MET!
        CHECK(tree.can_purchase(UpgradeType::DrillDurability, player_level, coins, voidite, titanium),
              "DrillDurability should now be purchasable after prerequisite is met");

        // At Level 1, cannot purchase Tier 2 (requires Level 2)
        CHECK(!tree.can_purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium), "Should not allow Tier 2 at Level 1");
        CHECK(tree.get_lock_reason(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium) == UpgradeLockReason::LevelLocked,
              "Lock reason should be LevelLocked");
        bool bought_fail = tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);
        CHECK(!bought_fail, "Purchase Tier 2 should fail at Level 1");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 1, "Tier should remain 1");

        // Level up to Level 2: Now Tier 2 purchase succeeds!
        player_level = 2;
        CHECK(tree.can_purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium), "Cannot purchase Tier 2 at Level 2");
        bought = tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);
        CHECK(bought, "Purchase tier 2 failed at Level 2");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 2, "Tier not 2 after purchase");
        CHECK(coins == 740, "Coins not deducted correctly for tier 2 (900 - 160)");
        CHECK(voidite == 88, "Voidite not deducted correctly for tier 2 (96 - 8)");
        CHECK(titanium == 91, "Titanium not deducted correctly for tier 2 (97 - 6)");

        std::cout << " -> Branch prerequisites, level gating, and multi-resource deduction verified." << std::endl;
    }

    // Test 3: Respec Feature (100% Points & Materials Refund + Immediate Re-Upgrade Verification)
    {
        std::cout << "[Test 3] Testing Respec with 100% Full Refund & Re-Upgrade Capability..." << std::endl;
        UpgradeTree tree;
        int coins = 2000;
        int voidite = 100;
        int titanium = 100;
        int player_level = 5;

        // Buy 3 tiers of DrillSpeed:
        // T0: 100c, 4v, 3t
        // T1: 160c, 8v, 6t
        // T2: 256c, 12v, 9t
        tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);
        tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);
        tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);

        // Buy 1 tier of ReinforcedPlating (T0: 100c, 4v, 3t)
        tree.purchase(UpgradeType::ReinforcedPlating, player_level, coins, voidite, titanium);

        int total_spent_coins = tree.get_total_spent_coins();
        int total_spent_voidite = tree.get_total_spent_voidite();
        int total_spent_titanium = tree.get_total_spent_titanium();

        // 100 + 160 + 256 + 100 = 616 coins
        CHECK(total_spent_coins == 616, "Total spent Coins mismatch");
        // 4 + 8 + 12 + 4 = 28 voidite
        CHECK(total_spent_voidite == 28, "Total spent Voidite mismatch");
        // 3 + 6 + 9 + 3 = 21 titanium
        CHECK(total_spent_titanium == 21, "Total spent Titanium mismatch");

        // Verify remaining currency balances
        CHECK(coins == (2000 - 616), "Coins balance mismatch");
        CHECK(voidite == (100 - 28), "Voidite balance mismatch");
        CHECK(titanium == (100 - 21), "Titanium balance mismatch");

        // Execute Respec (100% full refund)
        int ref_coins = 0, ref_voidite = 0, ref_titanium = 0;
        tree.respec(ref_coins, ref_voidite, ref_titanium);

        CHECK(ref_coins == 616, "100% coins refund mismatch");
        CHECK(ref_voidite == 28, "100% voidite refund mismatch");
        CHECK(ref_titanium == 21, "100% titanium refund mismatch");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "DrillSpeed tier not reset to 0");
        CHECK(tree.get_tier(UpgradeType::ReinforcedPlating) == 0, "ReinforcedPlating tier not reset to 0");

        // Restore refunded currencies into player inventory
        coins += ref_coins;
        voidite += ref_voidite;
        titanium += ref_titanium;
        CHECK(coins == 2000, "Coins not restored to initial 2000");
        CHECK(voidite == 100, "Voidite not restored to initial 100");
        CHECK(titanium == 100, "Titanium not restored to initial 100");

        // Crucial test: Re-upgrade verification! The player can now re-spend points freely!
        bool re_p1 = tree.purchase(UpgradeType::ThrusterTank, player_level, coins, voidite, titanium);
        CHECK(re_p1, "Failed to re-upgrade into ThrusterTank after respec");
        CHECK(tree.get_tier(UpgradeType::ThrusterTank) == 1, "ThrusterTank tier not 1 after re-upgrade");

        bool re_p2 = tree.purchase(UpgradeType::KineticDynamo, player_level, coins, voidite, titanium);
        CHECK(re_p2, "Failed to re-upgrade into KineticDynamo after ThrusterTank T1 prerequisite met");
        CHECK(tree.get_tier(UpgradeType::KineticDynamo) == 1, "KineticDynamo tier not 1 after re-upgrade");

        std::cout << " -> Respec 100% full point/material recovery (616C, 28V, 21T) and immediate re-upgrade verified." << std::endl;
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

        // 7. Debris Damage Mitigation:
        // Vanguard base 50% + tier 2 (2 * 10% = 20%) = 70% reduction!
        float raw_debris_damage = 50.0f;
        float taken = player.take_damage(raw_debris_damage, true);
        CHECK(std::abs(taken - 15.0f) < 0.01f, "Debris damage taken mismatch");
        CHECK(std::abs(player.health() - 150.0f) < 0.01f, "Health after debris mismatch");

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

        // Grant materials to test multi-resource purchase and respec
        profile.grant_resources(0, 0, 50, 50);

        // Buy DrillSpeed Tier 1 (100 Coins, 4 Voidite, 3 Titanium)
        bool p1 = profile.upgrades.purchase(UpgradeType::DrillSpeed, profile.get_player_level(), profile.total_coins, profile.total_voidite, profile.total_titanium);
        CHECK(p1, "Purchase DrillSpeed Tier 1 failed");
        CHECK(profile.upgrades.drillSpeedTier == 1, "DrillSpeed tier not 1");
        CHECK(profile.total_coins == 250, "Coins deduction mismatch (350 - 100)");
        CHECK(profile.total_voidite == 46, "Voidite deduction mismatch (50 - 4)");
        CHECK(profile.total_titanium == 47, "Titanium deduction mismatch (50 - 3)");

        // Buy ThrusterTank Tier 1 (100 Coins, 4 Voidite, 3 Titanium)
        bool p2 = profile.upgrades.purchase(UpgradeType::ThrusterTank, profile.get_player_level(), profile.total_coins, profile.total_voidite, profile.total_titanium);
        CHECK(p2, "Purchase ThrusterTank Tier 1 failed");
        CHECK(profile.upgrades.thrusterTankTier == 1, "ThrusterTank tier not 1");
        CHECK(profile.total_coins == 150, "Coins deduction mismatch (250 - 100)");
        CHECK(profile.total_voidite == 42, "Voidite deduction mismatch (46 - 4)");
        CHECK(profile.total_titanium == 44, "Titanium deduction mismatch (47 - 3)");

        int total_spent = profile.upgrades.get_total_spent_coins();
        CHECK(total_spent == 200, "Total spent coins mismatch");

        // Execute Respec (100% full refund)
        int ref_coins = 0, ref_voidite = 0, ref_titanium = 0;
        profile.upgrades.respec(ref_coins, ref_voidite, ref_titanium);
        CHECK(ref_coins == 200, "Refund coins mismatch");
        CHECK(ref_voidite == 8, "Refund voidite mismatch");
        CHECK(ref_titanium == 6, "Refund titanium mismatch");

        profile.total_coins += ref_coins;
        profile.total_voidite += ref_voidite;
        profile.total_titanium += ref_titanium;

        CHECK(profile.total_coins == 350, "Total Coins after respec mismatch (should be restored to 350)");
        CHECK(profile.total_voidite == 50, "Total Voidite after respec mismatch (should be restored to 50)");
        CHECK(profile.total_titanium == 50, "Total Titanium after respec mismatch (should be restored to 50)");

        CHECK(profile.upgrades.drillSpeedTier == 0, "DrillSpeed tier not reset to 0");
        CHECK(profile.upgrades.thrusterTankTier == 0, "ThrusterTank tier not reset to 0");

        std::cout << " -> Profile level progression, level-up coins, and 100% full resource respec recovery verified." << std::endl;
    }

    // Test 7: Complete Tier Cost Matrix & Progression Roadmap
    {
        std::cout << "[Test 7] Testing 5-Tier Cost Matrix & Progression Roadmap..." << std::endl;

        // Verify exact cost info for each of the 5 tiers
        auto t1 = UpgradeTree::get_tier_cost_info(1);
        CHECK(t1.tier == 1 && t1.required_level == 1 && t1.coin_cost == 100 && t1.voidite_cost == 4 && t1.titanium_cost == 3, "Tier 1 cost info mismatch");

        auto t2 = UpgradeTree::get_tier_cost_info(2);
        CHECK(t2.tier == 2 && t2.required_level == 2 && t2.coin_cost == 160 && t2.voidite_cost == 8 && t2.titanium_cost == 6, "Tier 2 cost info mismatch");

        auto t3 = UpgradeTree::get_tier_cost_info(3);
        CHECK(t3.tier == 3 && t3.required_level == 3 && t3.coin_cost == 256 && t3.voidite_cost == 12 && t3.titanium_cost == 9, "Tier 3 cost info mismatch");

        auto t4 = UpgradeTree::get_tier_cost_info(4);
        CHECK(t4.tier == 4 && t4.required_level == 4 && t4.coin_cost == 410 && t4.voidite_cost == 16 && t4.titanium_cost == 12, "Tier 4 cost info mismatch");

        auto t5 = UpgradeTree::get_tier_cost_info(5);
        CHECK(t5.tier == 5 && t5.required_level == 5 && t5.coin_cost == 656 && t5.voidite_cost == 20 && t5.titanium_cost == 15, "Tier 5 cost info mismatch");

        // Verify Lock Reasons for clear UI messaging
        UpgradeTree test_tree;
        // Level locked
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillSpeed, 0, 1000, 100, 100) == UpgradeLockReason::LevelLocked, "Should be LevelLocked at Lv 0");
        // Prerequisite locked
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillDurability, 5, 1000, 100, 100) == UpgradeLockReason::PrerequisiteLocked, "Should be PrerequisiteLocked without DrillSpeed T1");
        // Insufficient coins
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillSpeed, 1, 50, 100, 100) == UpgradeLockReason::InsufficientCoins, "Should be InsufficientCoins");
        // Insufficient voidite
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillSpeed, 1, 100, 2, 100) == UpgradeLockReason::InsufficientVoidite, "Should be InsufficientVoidite");
        // Insufficient titanium
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillSpeed, 1, 100, 10, 1) == UpgradeLockReason::InsufficientTitanium, "Should be InsufficientTitanium");
        // Unlocked and ready
        CHECK(test_tree.get_lock_reason(UpgradeType::DrillSpeed, 1, 100, 10, 10) == UpgradeLockReason::Unlocked, "Should be Unlocked");

        std::cout << " -> All 5 Tier cost records (100->656 Coins, 4->20 Voidite, 3->15 Titanium) & lock reasons verified." << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "ALL PROGRESSION TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
