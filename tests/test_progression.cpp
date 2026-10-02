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
        CHECK(std::abs(vngd.suitIntegrity - 160.0f) < 0.001f, "Vanguard suit integrity mismatch");
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

    // Test 2: Exponential Cost Formula & Upgrade Purchasing
    {
        std::cout << "[Test 2] Testing EXP Cost Formula..." << std::endl;
        CHECK(UpgradeTree::get_exp_cost(0) == 100, "Tier 0 cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(1) == 160, "Tier 1 cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(2) == 256, "Tier 2 cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(3) == 410, "Tier 3 cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(4) == 656, "Tier 4 cost mismatch");
        CHECK(UpgradeTree::get_exp_cost(5) == 0, "Tier 5 (max) cost mismatch");

        std::cout << " -> Exponential costs matched: 100 -> 160 -> 256 -> 410 -> 656 EXP." << std::endl;

        UpgradeTree tree;
        int exp = 1000;
        int voidite = 100;
        int titanium = 100;

        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "Initial tier not 0");
        CHECK(tree.can_purchase(UpgradeType::DrillSpeed, exp, voidite, titanium), "Cannot purchase tier 1");
        bool bought = tree.purchase(UpgradeType::DrillSpeed, exp, voidite, titanium);
        CHECK(bought, "Purchase tier 1 failed");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 1, "Tier not 1 after purchase");
        CHECK(exp == 900, "EXP not deducted correctly"); // 1000 - 100

        // Purchase tier 2
        bought = tree.purchase(UpgradeType::DrillSpeed, exp, voidite, titanium);
        CHECK(bought, "Purchase tier 2 failed");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 2, "Tier not 2 after purchase");
        CHECK(exp == 740, "EXP not deducted correctly for tier 2"); // 900 - 160

        std::cout << " -> Upgrade purchasing and currency deduction verified." << std::endl;
    }

    // Test 3: Respec Feature (85% refund)
    {
        std::cout << "[Test 3] Testing Respec with 85% Refund..." << std::endl;
        UpgradeTree tree;
        int exp = 2000;
        int voidite = 100;
        int titanium = 100;

        // Buy 3 tiers of DrillSpeed (100 + 160 + 256 = 516 EXP)
        tree.purchase(UpgradeType::DrillSpeed, exp, voidite, titanium);
        tree.purchase(UpgradeType::DrillSpeed, exp, voidite, titanium);
        tree.purchase(UpgradeType::DrillSpeed, exp, voidite, titanium);

        // Buy 1 tier of ReinforcedPlating (100 EXP)
        tree.purchase(UpgradeType::ReinforcedPlating, exp, voidite, titanium);

        int total_spent = tree.get_total_spent_exp();
        CHECK(total_spent == 616, "Total spent EXP mismatch");

        int refunded = 0;
        tree.respec(refunded);
        int expected_refund = static_cast<int>(std::round(616 * 0.85f)); // 524 EXP
        CHECK(refunded == expected_refund, "Refund amount mismatch");
        CHECK(tree.get_tier(UpgradeType::DrillSpeed) == 0, "DrillSpeed tier not reset");
        CHECK(tree.get_tier(UpgradeType::ReinforcedPlating) == 0, "ReinforcedPlating tier not reset");

        std::cout << " -> Respec refunded exactly 85% (" << refunded << " / " << total_spent << " EXP) and reset tiers." << std::endl;
    }

    // Test 4: Save & Load Persistence
    {
        std::cout << "[Test 4] Testing JSON Save / Load Persistence..." << std::endl;
        UserProfile p_write;
        p_write.total_exp = 1450;
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

        // Vanguard base 160 HP + 2 * 15 = 190 HP
        CHECK(std::abs(player.max_health() - 190.0f) < 0.001f, "Max health mismatch");
        CHECK(std::abs(player.health() - 190.0f) < 0.001f, "Health mismatch");
        // Thruster tank 100 * (1 + 2 * 0.20) = 140
        CHECK(std::abs(player.exo().max_power - 140.0f) < 0.001f, "Max power mismatch");

        // Falling debris damage mitigation:
        // Vanguard base 50% + tier 2 (2 * 10% = 20%) = 70% reduction!
        float raw_debris_damage = 50.0f;
        float taken = player.take_damage(raw_debris_damage, true);
        // 50 * (1 - 0.70) = 15.0 damage
        CHECK(std::abs(taken - 15.0f) < 0.01f, "Debris damage taken mismatch");
        CHECK(std::abs(player.health() - 175.0f) < 0.01f, "Health after debris mismatch");

        std::cout << " -> Vanguard 50% + Plating 20% debris damage mitigation verified: 50 dmg -> 15 dmg taken." << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "ALL PROGRESSION TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
