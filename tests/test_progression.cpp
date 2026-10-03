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

    // Test 2: Exponential Coin Cost Formula, Level Gating & Upgrade Purchasing
    {
        std::cout << "[Test 2] Testing Coin Cost Formula & Level Gating..." << std::endl;
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

        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "Initial tier not 0");
        // Level 1: Can purchase Tier 1 (requires Level 1)
        CHECK(tree.can_purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium), "Cannot purchase tier 1 at level 1");
        bool bought = tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);
        CHECK(bought, "Purchase tier 1 failed");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 1, "Tier not 1 after purchase");
        CHECK(coins == 900, "Coins not deducted correctly (1000 - 100)");

        // At Level 1, cannot purchase Tier 2 (requires Level 2)
        CHECK(!tree.can_purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium), "Should not allow Tier 2 at Level 1");
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

        std::cout << " -> Upgrade purchasing, level gating (Tier N requires Lv N), and coin deduction verified." << std::endl;
    }

    // Test 3: Respec Feature (85% Coins refund)
    {
        std::cout << "[Test 3] Testing Respec with 85% Coins Refund..." << std::endl;
        UpgradeTree tree;
        int coins = 2000;
        int voidite = 100;
        int titanium = 100;
        int player_level = 5;

        // Buy 3 tiers of DrillSpeed (100 + 160 + 256 = 516 Coins)
        tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);
        tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);
        tree.purchase(UpgradeType::DrillSpeed, player_level, coins, voidite, titanium);

        // Buy 1 tier of ReinforcedPlating (100 Coins)
        tree.purchase(UpgradeType::ReinforcedPlating, player_level, coins, voidite, titanium);

        int total_spent = tree.get_total_spent_coins();
        CHECK(total_spent == 616, "Total spent Coins mismatch");

        int refunded = 0;
        tree.respec(refunded);
        int expected_refund = static_cast<int>(std::round(616 * 0.85f)); // 524 Coins
        CHECK(refunded == expected_refund, "Refund amount mismatch");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "DrillSpeed tier not reset");
        CHECK(tree.get_tier(UpgradeType::ReinforcedPlating) == 0, "ReinforcedPlating tier not reset");

        std::cout << " -> Respec refunded exactly 85% (" << refunded << " / " << total_spent << " Coins) and reset tiers." << std::endl;
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

    // Test 5: Player Controller Attribute & Damage Mitigation Injection
    {
        std::cout << "[Test 5] Testing Controller Attribute Injection & Damage Reduction..." << std::endl;
        PlayerController player(glm::vec3(0.0f, 10.0f, 0.0f));

        // Inject Vanguard with tier 2 Reinforced Plating (+30 HP, +20% cave-in reduction)
        UpgradeTree upg;
        upg.reinforcedPlatingTier = 2;
        upg.thrusterTankTier = 2;
        player.apply_attributes_and_upgrades(CharacterClass::Vanguard, upg);

        // Vanguard base 135 HP + 2 * 15 = 165 HP
        CHECK(std::abs(player.max_health() - 165.0f) < 0.001f, "Max health mismatch");
        CHECK(std::abs(player.health() - 165.0f) < 0.001f, "Health mismatch");
        // Thruster tank 100 * (1 + 2 * 0.20) = 140
        CHECK(std::abs(player.exo().max_power - 140.0f) < 0.001f, "Max power mismatch");

        // Falling debris damage mitigation:
        // Vanguard base 50% + tier 2 (2 * 10% = 20%) = 70% reduction!
        float raw_debris_damage = 50.0f;
        float taken = player.take_damage(raw_debris_damage, true);
        // 50 * (1 - 0.70) = 15.0 damage
        CHECK(std::abs(taken - 15.0f) < 0.01f, "Debris damage taken mismatch");
        CHECK(std::abs(player.health() - 150.0f) < 0.01f, "Health after debris mismatch");

        std::cout << " -> Vanguard 50% + Plating 20% debris damage mitigation verified: 50 dmg -> 15 dmg taken." << std::endl;
    }

    // Test 6: Player Level Progression, Level-Up Coins Bonus, Sector & Class Gating
    {
        std::cout << "[Test 6] Testing Player Level Progression, Sector/Class Gating & Coin Upgrades..." << std::endl;
        UserProfile profile;
        profile.total_exp = 0;
        profile.total_coins = 200;
        profile.total_voidite = 0;
        profile.total_titanium = 0;

        // Baseline Level 1 (0 EXP)
        CHECK(profile.get_player_level() == 1, "Initial level should be 1");
        CHECK(profile.is_sector_unlocked(1), "Sector 1 must be unlocked at Level 1");
        CHECK(!profile.is_sector_unlocked(2), "Sector 2 should be locked at Level 1");
        CHECK(!profile.is_sector_unlocked(3), "Sector 3 should be locked at Level 1");

        CHECK(profile.is_class_unlocked(CharacterClass::Demolitionist), "Demolitionist must be unlocked at Level 1");
        CHECK(!profile.is_class_unlocked(CharacterClass::Vanguard), "Vanguard should be locked at Level 1");
        CHECK(!profile.is_class_unlocked(CharacterClass::Scout), "Scout should be locked at Level 1");

        // Add 350 EXP -> reaches Level 2 (+150 bonus coins)
        int lvls = 0, bonus = 0;
        bool leveled_up = profile.add_exp(350, lvls, bonus);
        CHECK(leveled_up, "Should have leveled up to Level 2");
        CHECK(lvls == 1, "Should have gained 1 level");
        CHECK(bonus == 150, "Level up bonus should be +150 coins");
        CHECK(profile.get_player_level() == 2, "Level should now be 2");
        CHECK(profile.total_coins == 350, "Coins should be 200 + 150 = 350");

        // Level 2 Unlocks: Sector 2 and Vanguard!
        CHECK(profile.is_sector_unlocked(2), "Sector 2 should be unlocked at Level 2");
        CHECK(!profile.is_sector_unlocked(3), "Sector 3 should still be locked at Level 2");
        CHECK(profile.is_class_unlocked(CharacterClass::Vanguard), "Vanguard should be unlocked at Level 2");
        CHECK(!profile.is_class_unlocked(CharacterClass::Scout), "Scout should still be locked at Level 2");

        // Add 400 more EXP (total 750) -> reaches Level 3 (+150 bonus coins)
        leveled_up = profile.add_exp(400, lvls, bonus);
        CHECK(leveled_up, "Should have leveled up to Level 3");
        CHECK(profile.get_player_level() == 3, "Level should now be 3");
        CHECK(profile.total_coins == 500, "Coins should be 350 + 150 = 500");
        CHECK(profile.is_class_unlocked(CharacterClass::Scout), "Scout should be unlocked at Level 3");

        // Add 600 more EXP (total 1350) -> reaches Level 4 (+150 bonus coins)
        profile.add_exp(600, lvls, bonus);
        CHECK(profile.get_player_level() == 4, "Level should now be 4");
        CHECK(profile.is_sector_unlocked(3), "Sector 3 should be unlocked at Level 4");

        // Grant materials to test Coin-based purchasing and respec
        profile.grant_resources(0, 0, 50, 50);

        // Purchase DrillSpeed (100 coins)
        bool p1 = profile.upgrades.purchase(UpgradeType::DrillSpeed, profile.get_player_level(), profile.total_coins, profile.total_voidite, profile.total_titanium);
        CHECK(p1, "Purchase DrillSpeed Tier 1 failed");
        CHECK(profile.upgrades.drillSpeedTier == 1, "DrillSpeed tier not 1");
        CHECK(profile.total_coins == 550, "Coins deduction mismatch (650 - 100)");

        // Purchase ThrusterTank (100 coins)
        bool p2 = profile.upgrades.purchase(UpgradeType::ThrusterTank, profile.get_player_level(), profile.total_coins, profile.total_voidite, profile.total_titanium);
        CHECK(p2, "Purchase ThrusterTank Tier 1 failed");
        CHECK(profile.upgrades.thrusterTankTier == 1, "ThrusterTank tier not 1");
        CHECK(profile.total_coins == 450, "Coins deduction mismatch (550 - 100)");

        int total_spent = profile.upgrades.get_total_spent_coins();
        CHECK(total_spent == 200, "Total spent mismatch");

        // Execute respec (85% recovery: 200 * 0.85 = 170 Coins)
        int refunded = 0;
        profile.upgrades.respec(refunded);
        CHECK(refunded == 170, "Refund calculation mismatch");
        profile.total_coins += refunded;
        CHECK(profile.total_coins == 620, "Total Coins after respec refund mismatch (450 + 170)");

        CHECK(profile.upgrades.drillSpeedTier == 0, "DrillSpeed tier not reset to 0");
        CHECK(profile.upgrades.thrusterTankTier == 0, "ThrusterTank tier not reset to 0");

        std::cout << " -> Level progression (Lv 1-4), level-up coin bonuses, sector/class gating, and coin-based respec verified." << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "ALL PROGRESSION TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
