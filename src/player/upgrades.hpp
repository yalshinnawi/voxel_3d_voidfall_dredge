#pragma once
#include <string>
#include <vector>
#include <algorithm>

namespace Voidfall {

enum class UpgradeType : int {
    // Discipline 1: EXCAVATION (Drill Matrix)
    DrillSpeed = 0,          // Foundation: Excavation mining rate (Max 5)
    DrillDurability = 1,     // Specialization: Thermal heat sinks & cooling (Max 3)
    MicroCharges = 6,        // Capstone: Surgical blast & mineral extraction yield (Max 2)

    // Discipline 2: TRAVERSAL & SCAN (Exo-Suit Mobility & Sonar)
    ThrusterTank = 2,        // Foundation: Jetpack fuel capacity & hover endurance (Max 5)
    KineticDynamo = 3,       // Specialization: Sprint & fall kinetic recharge (Max 3)
    SonarFrequency = 4,      // Foundation/Tech: Wide-spectrum seismic transceiver (Max 2)

    // Discipline 3: DEFENSE (Bastion & Hazard Survival)
    ReinforcedPlating = 5,   // Foundation: Suit integrity HP & debris impact dampening (Max 5)
    KineticShielding = 7,    // Specialization: Shield capacity buffer & fast recharge (Max 3)
    Defibrillator = 8,       // Capstone: Emergency second wind revive & barrier (Max 2)

    // Discipline 4: STEALTH (Shadow Infiltration)
    AcousticDampening = 9,   // Foundation: Footstep & crouch acoustic noise suppression (Max 5)
    ShadowVeil = 10,         // Specialization: Light dispersion & reduced optical detection (Max 3)
    AmbushProtocol = 11,     // Capstone: Unalerted stealth strike damage & silent drill (Max 2)

    // Discipline 5: MELEE (Close Combat & Shock)
    PneumaticImpact = 12,    // Foundation: Quick melee shove damage & stagger impulse (Max 5)
    CleaveShockwave = 13,    // Specialization: Wide 90-degree cleave cone & shock stun (Max 3)
    BerserkerOverdrive = 14, // Capstone: Melee strike shield siphon & speed burst (Max 2)

    // Discipline 6: GUN DAMAGE (Ballistics & Precision Firepower)
    HyperVelocitySlugs = 15, // Foundation: Weapon projectile damage & penetration (Max 5)
    RapidCycling = 16,       // Specialization: Overclocked fire rate & rapid reload (Max 3)
    CriticalWeakpoint = 17,  // Capstone: Piercing weakpoint critical strike damage (Max 2)

    COUNT = 18
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
    InsufficientEXP,
    CampaignCapReached,
    InsufficientCoins = InsufficientEXP,
    InsufficientVoidite = InsufficientEXP,
    InsufficientTitanium = InsufficientEXP
};

enum class NodeWebState {
    GreyedOutLocked = 0, // Prerequisite not met: branch node is greyed out
    LevelGated = 1,      // Prerequisite met, but Delver Level is below requirement
    InsufficientEXP = 2, // Prerequisite met, Delver Level met, but lacks EXP
    Available = 3,       // Ready to acquire with EXP
    Purchased = 4,       // Active tier 1 to MaxTier-1
    Mastered = 5,        // Max tier mastered
    CampaignCapped = 6   // Prereq & level met, but player has reached 24 pts campaign cap
};

struct TierCostInfo {
    int tier{0};           // Target tier (1-5)
    int required_level{0}; // Required player level
    int exp_cost{0};       // Sole upgrade currency: EXP
    int coin_cost{0};      // Compatibility alias (matches exp_cost)
    int voidite_cost{0};   // 0 (EXP only)
    int titanium_cost{0};  // 0 (EXP only)
};

struct UpgradeInfo {
    UpgradeType type;
    std::string name;
    std::string category;
    std::string discipline;       // "EXCAVATION", "TRAVERSAL", "DEFENSE", "STEALTH", "MELEE", "GUN DAMAGE"
    std::string description;
    std::string statUnit;
    float baseStatBonusPerTier;
    bool is_root_node{true};
    std::vector<std::string> tier_subtitles;
    std::string milestone_perk;
    int max_tier{5};
};

struct UpgradeTree {
    // Discipline 1: EXCAVATION (Drill Matrix)
    int drillSpeedTier{0};          // Max 5: +12% mining excavation rate per tier
    int drillDurabilityTier{0};     // Max 3: -15% heat buildup / +20% cooling rate
    int microChargesTier{0};        // Max 2: +20% high-grade mineral extraction yield

    // Discipline 2: TRAVERSAL & SCAN (Exo-Suit Mobility & Sonar)
    int thrusterTankTier{0};        // Max 5: +20% jetpack fuel capacity per tier
    int kineticDynamoTier{0};       // Max 3: Sprinting/falling recharges fuel +15% faster
    int sonarFrequencyTier{0};      // Max 2: +2m scan radius, -1.0s CD (T2 unlocks HUD labels)

    // Discipline 3: DEFENSE (Bastion & Hazard Survival)
    int reinforcedPlatingTier{0};   // Max 5: +15 Max HP, -10% cave-in/debris damage
    int kineticShieldingTier{0};    // Max 3: +12 Max Shield, -0.5s shield recharge delay
    int defibrillatorTier{0};       // Max 2: Emergency second wind upon fatal damage

    // Discipline 4: STEALTH (Shadow Infiltration)
    int acousticDampeningTier{0};   // Max 5: -20% crouch/footstep noise radius per tier
    int shadowVeilTier{0};          // Max 3: -15% enemy optical detection range per tier
    int ambushProtocolTier{0};      // Max 2: +35% damage against unalerted hostiles per tier

    // Discipline 5: MELEE (Close Combat & Shock)
    int pneumaticImpactTier{0};     // Max 5: +30% quick melee shove damage & stagger
    int cleaveShockwaveTier{0};     // Max 3: 90-degree cleave arc & stun duration
    int berserkerOverdriveTier{0};  // Max 2: Melee strike siphons +12 shield & speed burst

    // Discipline 6: GUN DAMAGE (Ballistics & Precision Firepower)
    int hyperVelocitySlugsTier{0};  // Max 5: +12% firearm projectile damage per tier
    int rapidCyclingTier{0};        // Max 3: +8% fire rate & +10% reload speed per tier
    int criticalWeakpointTier{0};   // Max 2: +35% critical damage on monster weakpoints

    bool can_identify_materials() const { return sonarFrequencyTier >= 2; }

    static constexpr int MAX_TIER = 5;
    static constexpr int CAMPAIGN_POINT_CAP = 24;  // Max points assignable before beating the campaign
    static constexpr int TOTAL_TREE_POINTS = 60;   // Exactly 60 points across the full 6-branch web

    static int get_max_tier(UpgradeType type);

    int get_tier(UpgradeType type) const;
    void set_tier(UpgradeType type, int tier);

    int get_total_assigned_points() const;

    static int get_exp_cost(int current_tier);
    static int get_coin_cost(int current_tier) { return get_exp_cost(current_tier); }
    static int get_voidite_cost(int current_tier) { (void)current_tier; return 0; }
    static int get_titanium_cost(int current_tier) { (void)current_tier; return 0; }
    static int get_required_level_for_tier(int current_tier) { return current_tier + 1; }

    static TierCostInfo get_tier_cost_info(int target_tier);
    static UpgradePrerequisite get_prerequisite(UpgradeType type);

    // EXP-only purchase check
    bool can_purchase(UpgradeType type, int player_level, int current_exp, bool is_game_completed = false) const;
    bool can_purchase(UpgradeType type, int current_exp, bool is_game_completed = false) const {
        return can_purchase(type, 999, current_exp, is_game_completed);
    }
    // Backward compatibility overloads
    bool can_purchase(UpgradeType type, int player_level, int current_exp, int current_voidite, int current_titanium) const {
        (void)current_voidite; (void)current_titanium;
        return can_purchase(type, player_level, current_exp, false);
    }
    bool can_purchase(UpgradeType type, int current_exp, int current_voidite, int current_titanium) const {
        (void)current_voidite; (void)current_titanium;
        return can_purchase(type, 999, current_exp, false);
    }

    UpgradeLockReason get_lock_reason(UpgradeType type, int player_level, int current_exp, bool is_game_completed = false) const;
    UpgradeLockReason get_lock_reason(UpgradeType type, int player_level, int current_exp, int voidite, int titanium) const {
        (void)voidite; (void)titanium;
        return get_lock_reason(type, player_level, current_exp, false);
    }

    std::string get_lock_reason_string(UpgradeType type, int player_level, int current_exp, bool is_game_completed = false) const;
    std::string get_lock_reason_string(UpgradeType type, int player_level, int current_exp, int voidite, int titanium) const {
        (void)voidite; (void)titanium;
        return get_lock_reason_string(type, player_level, current_exp, false);
    }

    // EXP-only purchase
    bool purchase(UpgradeType type, int player_level, int& inout_exp, bool is_game_completed = false);
    bool purchase(UpgradeType type, int& inout_exp, bool is_game_completed = false) {
        return purchase(type, 999, inout_exp, is_game_completed);
    }
    // Backward compatibility overloads
    bool purchase(UpgradeType type, int player_level, int& inout_exp, int& inout_voidite, int& inout_titanium) {
        (void)inout_voidite; (void)inout_titanium;
        return purchase(type, player_level, inout_exp, false);
    }
    bool purchase(UpgradeType type, int& inout_exp, int& inout_voidite, int& inout_titanium) {
        (void)inout_voidite; (void)inout_titanium;
        return purchase(type, 999, inout_exp, false);
    }

    int get_total_spent_exp() const;
    int get_total_spent_coins() const { return get_total_spent_exp(); }
    int get_total_spent_voidite() const { return 0; }
    int get_total_spent_titanium() const { return 0; }

    // 100% full refund on respec so spent EXP is returned and players can freely re-allocate
    int respec(int& out_refunded_exp);
    void respec(int& out_refunded_exp, int& out_refunded_voidite, int& out_refunded_titanium) {
        out_refunded_voidite = 0;
        out_refunded_titanium = 0;
        respec(out_refunded_exp);
    }

    // Skill Tree Web state helper
    NodeWebState get_node_web_state(UpgradeType type, int player_level, int current_exp, bool is_game_completed = false) const;

    static UpgradeInfo get_info(UpgradeType type);
    std::string get_stat_preview(UpgradeType type) const;
    std::string get_current_stat_string(UpgradeType type) const;
    std::string get_next_stat_string(UpgradeType type) const;
};

} // namespace Voidfall
