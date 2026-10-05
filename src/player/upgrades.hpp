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

struct UpgradePrerequisite {
    bool has_prerequisite{false};
    UpgradeType required_type{UpgradeType::DrillSpeed};
    int required_tier{1};
    std::string required_name;
    std::string description;
};

enum class UpgradeLockReason {
    Unlocked = 0,
    MaxTier,
    LevelLocked,
    PrerequisiteLocked,
    InsufficientCoins,
    InsufficientVoidite,
    InsufficientTitanium
};

struct TierCostInfo {
    int tier{0};           // Target tier (1-5)
    int required_level{0}; // Required player level
    int coin_cost{0};
    int voidite_cost{0};
    int titanium_cost{0};
};

struct UpgradeInfo {
    UpgradeType type;
    std::string name;
    std::string category;
    std::string discipline;       // e.g. "EXCAVATION", "TRAVERSAL", "DEFENSE"
    std::string description;
    std::string statUnit;
    float baseStatBonusPerTier;
    bool is_root_node{true};
    std::vector<std::string> tier_subtitles;
    std::string milestone_perk;
};

struct UpgradeTree {
    // Drill Matrix (Discipline: EXCAVATION)
    int drillSpeedTier{0};       // Max 5: +12% mining speed per tier (Root node)
    int drillDurabilityTier{0};  // Max 5: -15% heat buildup / +20% cooling rate (Requires DrillSpeed T1)
    
    // Exo-Suit & Mobility (Discipline: TRAVERSAL)
    int thrusterTankTier{0};     // Max 5: +20% jetpack fuel capacity (Root node)
    int kineticDynamoTier{0};    // Max 5: Sprinting/falling recharges fuel +15% faster (Requires ThrusterTank T1)
    
    // Surveying & Defense (Discipline: SURVIVAL & DEFENSE)
    int reinforcedPlatingTier{0};// Max 5: +15 Max HP, -10% cave-in damage (Root node)
    int sonarFrequencyTier{0};   // Max 5: +2m scan radius, -1.0s cooldown (Requires ReinforcedPlating T1; T2 unlocks HUD material labels)

    bool can_identify_materials() const { return sonarFrequencyTier >= 2; }

    static constexpr int MAX_TIER = 5;

    int get_tier(UpgradeType type) const;
    void set_tier(UpgradeType type, int tier);

    static int get_coin_cost(int current_tier);
    static int get_exp_cost(int current_tier) { return get_coin_cost(current_tier); } // Legacy / compatibility alias
    static int get_voidite_cost(int current_tier);
    static int get_titanium_cost(int current_tier);
    static int get_required_level_for_tier(int current_tier) { return current_tier + 1; }

    static TierCostInfo get_tier_cost_info(int target_tier);
    static UpgradePrerequisite get_prerequisite(UpgradeType type);

    bool can_purchase(UpgradeType type, int player_level, int current_coins, int current_voidite, int current_titanium) const;
    bool can_purchase(UpgradeType type, int current_coins, int current_voidite, int current_titanium) const {
        return can_purchase(type, 999, current_coins, current_voidite, current_titanium);
    }

    UpgradeLockReason get_lock_reason(UpgradeType type, int player_level, int coins, int voidite, int titanium) const;
    std::string get_lock_reason_string(UpgradeType type, int player_level, int coins, int voidite, int titanium) const;

    bool purchase(UpgradeType type, int player_level, int& inout_coins, int& inout_voidite, int& inout_titanium);
    bool purchase(UpgradeType type, int& inout_coins, int& inout_voidite, int& inout_titanium) {
        return purchase(type, 999, inout_coins, inout_voidite, inout_titanium);
    }

    int get_total_spent_coins() const;
    int get_total_spent_exp() const { return get_total_spent_coins(); } // Legacy alias
    int get_total_spent_voidite() const;
    int get_total_spent_titanium() const;

    // 100% full refund on respec so points and resources are returned and players can re-upgrade
    void respec(int& out_refunded_coins, int& out_refunded_voidite, int& out_refunded_titanium);
    int respec(int& out_refunded_coins); // Compatibility overload (100% refund)

    static UpgradeInfo get_info(UpgradeType type);
    std::string get_stat_preview(UpgradeType type) const;
    std::string get_current_stat_string(UpgradeType type) const;
    std::string get_next_stat_string(UpgradeType type) const;
};

} // namespace Voidfall
