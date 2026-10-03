#pragma once
#include <string>
#include <vector>

namespace Voidfall {

enum class UpgradeType : int {
    DrillSpeed = 0,
    DrillDurability = 1,
    ThrusterTank = 2,
    KineticDynamo = 3,
    SonarFrequency = 4,
    ReinforcedPlating = 5,
    COUNT = 6
};

struct UpgradeInfo {
    UpgradeType type;
    std::string name;
    std::string category;
    std::string description;
    std::string statUnit;
    float baseStatBonusPerTier;
};

struct UpgradeTree {
    // Drill Matrix
    int drillSpeedTier{0};       // Max 5: +12% mining speed per tier
    int drillDurabilityTier{0};  // Max 5: -15% heat buildup / faster spinup
    
    // Exo-Suit & Mobility
    int thrusterTankTier{0};     // Max 5: +20% jetpack fuel capacity
    int kineticDynamoTier{0};    // Max 5: Sprinting/falling recharges fuel +15% faster
    
    // Surveying & Defense
    int sonarFrequencyTier{0};   // Max 5: +2m scan radius, -1.0s cooldown (Tier 2 unlocks HUD material labels)
    int reinforcedPlatingTier{0};// Max 5: +15 Max HP, -10% cave-in damage

    bool can_identify_materials() const { return sonarFrequencyTier >= 2; }

    static constexpr int MAX_TIER = 5;

    int get_tier(UpgradeType type) const;
    void set_tier(UpgradeType type, int tier);

    static int get_coin_cost(int current_tier);
    static int get_exp_cost(int current_tier) { return get_coin_cost(current_tier); } // Legacy / compatibility alias
    static int get_voidite_cost(int current_tier);
    static int get_titanium_cost(int current_tier);
    static int get_required_level_for_tier(int current_tier) { return current_tier + 1; }

    bool can_purchase(UpgradeType type, int player_level, int current_coins, int current_voidite, int current_titanium) const;
    bool can_purchase(UpgradeType type, int current_coins, int current_voidite, int current_titanium) const {
        return can_purchase(type, 999, current_coins, current_voidite, current_titanium);
    }

    bool purchase(UpgradeType type, int player_level, int& inout_coins, int& inout_voidite, int& inout_titanium);
    bool purchase(UpgradeType type, int& inout_coins, int& inout_voidite, int& inout_titanium) {
        return purchase(type, 999, inout_coins, inout_voidite, inout_titanium);
    }

    int get_total_spent_coins() const;
    int get_total_spent_exp() const { return get_total_spent_coins(); } // Legacy alias
    int respec(int& out_refunded_coins);

    static UpgradeInfo get_info(UpgradeType type);
    std::string get_stat_preview(UpgradeType type) const;
};

} // namespace Voidfall
