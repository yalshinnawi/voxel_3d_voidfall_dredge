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
    int sonarFrequencyTier{0};   // Max 5: +2m scan radius, -0.5s cooldown
    int reinforcedPlatingTier{0};// Max 5: +15 Max HP, -10% cave-in damage

    static constexpr int MAX_TIER = 5;

    int get_tier(UpgradeType type) const;
    void set_tier(UpgradeType type, int tier);

    static int get_exp_cost(int current_tier);
    static int get_voidite_cost(int current_tier);
    static int get_titanium_cost(int current_tier);

    bool can_purchase(UpgradeType type, int current_exp, int current_voidite, int current_titanium) const;
    bool purchase(UpgradeType type, int& inout_exp, int& inout_voidite, int& inout_titanium);

    int get_total_spent_exp() const;
    int respec(int& out_refunded_exp);

    static UpgradeInfo get_info(UpgradeType type);
    std::string get_stat_preview(UpgradeType type) const;
};

} // namespace Voidfall
