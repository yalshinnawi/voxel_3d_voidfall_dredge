#include "upgrades.hpp"
#include <cmath>
#include <algorithm>

namespace Voidfall {

int UpgradeTree::get_tier(UpgradeType type) const {
    switch (type) {
        case UpgradeType::DrillSpeed:        return drillSpeedTier;
        case UpgradeType::DrillDurability:   return drillDurabilityTier;
        case UpgradeType::ThrusterTank:      return thrusterTankTier;
        case UpgradeType::KineticDynamo:     return kineticDynamoTier;
        case UpgradeType::SonarFrequency:    return sonarFrequencyTier;
        case UpgradeType::ReinforcedPlating: return reinforcedPlatingTier;
        default: return 0;
    }
}

void UpgradeTree::set_tier(UpgradeType type, int tier) {
    int clamped = std::clamp(tier, 0, MAX_TIER);
    switch (type) {
        case UpgradeType::DrillSpeed:        drillSpeedTier = clamped; break;
        case UpgradeType::DrillDurability:   drillDurabilityTier = clamped; break;
        case UpgradeType::ThrusterTank:      thrusterTankTier = clamped; break;
        case UpgradeType::KineticDynamo:     kineticDynamoTier = clamped; break;
        case UpgradeType::SonarFrequency:    sonarFrequencyTier = clamped; break;
        case UpgradeType::ReinforcedPlating: reinforcedPlatingTier = clamped; break;
        default: break;
    }
}

int UpgradeTree::get_coin_cost(int current_tier) {
    if (current_tier >= MAX_TIER) return 0;
    // Exponential Coin Curve: 100 -> 160 -> 256 -> 410 -> 656 Coins
    static const int s_coin_costs[MAX_TIER] = {100, 160, 256, 410, 656};
    if (current_tier >= 0 && current_tier < MAX_TIER) {
        return s_coin_costs[current_tier];
    }
    float cost = 100.0f * std::pow(1.6f, static_cast<float>(current_tier));
    return static_cast<int>(std::round(cost));
}

int UpgradeTree::get_voidite_cost(int current_tier) {
    if (current_tier >= MAX_TIER) return 0;
    return 4 * (current_tier + 1); // Tier 1: 4, Tier 2: 8, Tier 3: 12, Tier 4: 16, Tier 5: 20
}

int UpgradeTree::get_titanium_cost(int current_tier) {
    if (current_tier >= MAX_TIER) return 0;
    return 3 * (current_tier + 1); // Tier 1: 3, Tier 2: 6, Tier 3: 9, Tier 4: 12, Tier 5: 15
}

TierCostInfo UpgradeTree::get_tier_cost_info(int target_tier) {
    int tier_idx = std::clamp(target_tier - 1, 0, MAX_TIER - 1);
    return TierCostInfo{
        target_tier,
        get_required_level_for_tier(tier_idx),
        get_coin_cost(tier_idx),
        get_voidite_cost(tier_idx),
        get_titanium_cost(tier_idx)
    };
}

UpgradePrerequisite UpgradeTree::get_prerequisite(UpgradeType type) {
    switch (type) {
        case UpgradeType::DrillSpeed:
            return UpgradePrerequisite{false, UpgradeType::DrillSpeed, 0, "None", "Foundational excavation rotary bit."};
        case UpgradeType::DrillDurability:
            return UpgradePrerequisite{true, UpgradeType::DrillSpeed, 1, "Drill Velocity Tier 1", "Requires basic bit stabilization before thermal cooling channels can be routed."};
        case UpgradeType::ThrusterTank:
            return UpgradePrerequisite{false, UpgradeType::ThrusterTank, 0, "None", "Foundational jetpack fuel reservoir."};
        case UpgradeType::KineticDynamo:
            return UpgradePrerequisite{true, UpgradeType::ThrusterTank, 1, "Thruster Reservoir Tier 1", "Requires active jetpack tank manifold to interface kinetic recovery."};
        case UpgradeType::ReinforcedPlating:
            return UpgradePrerequisite{false, UpgradeType::ReinforcedPlating, 0, "None", "Foundational suit integrity plating."};
        case UpgradeType::SonarFrequency:
            return UpgradePrerequisite{false, UpgradeType::SonarFrequency, 0, "None", "Foundational seismic surveying transceiver."};
        default:
            return UpgradePrerequisite{false, UpgradeType::DrillSpeed, 0, "None", ""};
    }
}

UpgradeLockReason UpgradeTree::get_lock_reason(UpgradeType type, int player_level, int coins, int voidite, int titanium) const {
    int tier = get_tier(type);
    if (tier >= MAX_TIER) return UpgradeLockReason::MaxTier;

    int req_level = get_required_level_for_tier(tier);
    if (player_level < req_level) return UpgradeLockReason::LevelLocked;

    auto prereq = get_prerequisite(type);
    if (prereq.has_prerequisite && get_tier(prereq.required_type) < prereq.required_tier) {
        return UpgradeLockReason::PrerequisiteLocked;
    }

    int coin_needed = get_coin_cost(tier);
    int void_needed = get_voidite_cost(tier);
    int tit_needed = get_titanium_cost(tier);

    if (coins < coin_needed) return UpgradeLockReason::InsufficientCoins;
    if (voidite < void_needed) return UpgradeLockReason::InsufficientVoidite;
    if (titanium < tit_needed) return UpgradeLockReason::InsufficientTitanium;

    return UpgradeLockReason::Unlocked;
}

std::string UpgradeTree::get_lock_reason_string(UpgradeType type, int player_level, int coins, int voidite, int titanium) const {
    int tier = get_tier(type);
    UpgradeLockReason reason = get_lock_reason(type, player_level, coins, voidite, titanium);
    switch (reason) {
        case UpgradeLockReason::MaxTier:
            return "MAX TIER MASTERED";
        case UpgradeLockReason::LevelLocked:
            return "REQUIRES DELVER LEVEL " + std::to_string(get_required_level_for_tier(tier));
        case UpgradeLockReason::PrerequisiteLocked: {
            auto prereq = get_prerequisite(type);
            return "REQUIRES: " + prereq.required_name;
        }
        case UpgradeLockReason::InsufficientCoins: {
            int need = get_coin_cost(tier) - coins;
            return "LACKING COINS (Need " + std::to_string(need) + " more)";
        }
        case UpgradeLockReason::InsufficientVoidite: {
            int need = get_voidite_cost(tier) - voidite;
            return "LACKING VOIDITE (Need " + std::to_string(need) + " more)";
        }
        case UpgradeLockReason::InsufficientTitanium: {
            int need = get_titanium_cost(tier) - titanium;
            return "LACKING TITANIUM (Need " + std::to_string(need) + " more)";
        }
        case UpgradeLockReason::Unlocked:
        default:
            return "READY TO ACQUIRE";
    }
}

bool UpgradeTree::can_purchase(UpgradeType type, int player_level, int current_coins, int current_voidite, int current_titanium) const {
    return get_lock_reason(type, player_level, current_coins, current_voidite, current_titanium) == UpgradeLockReason::Unlocked;
}

bool UpgradeTree::purchase(UpgradeType type, int player_level, int& inout_coins, int& inout_voidite, int& inout_titanium) {
    if (!can_purchase(type, player_level, inout_coins, inout_voidite, inout_titanium)) return false;

    int tier = get_tier(type);
    int coin_cost = get_coin_cost(tier);
    int voidite_cost = get_voidite_cost(tier);
    int titanium_cost = get_titanium_cost(tier);

    inout_coins -= coin_cost;
    inout_voidite -= voidite_cost;
    inout_titanium -= titanium_cost;

    set_tier(type, tier + 1);
    return true;
}

int UpgradeTree::get_total_spent_coins() const {
    int total = 0;
    for (int t = 0; t < static_cast<int>(UpgradeType::COUNT); ++t) {
        int tier = get_tier(static_cast<UpgradeType>(t));
        for (int i = 0; i < tier; ++i) {
            total += get_coin_cost(i);
        }
    }
    return total;
}

int UpgradeTree::get_total_spent_voidite() const {
    int total = 0;
    for (int t = 0; t < static_cast<int>(UpgradeType::COUNT); ++t) {
        int tier = get_tier(static_cast<UpgradeType>(t));
        for (int i = 0; i < tier; ++i) {
            total += get_voidite_cost(i);
        }
    }
    return total;
}

int UpgradeTree::get_total_spent_titanium() const {
    int total = 0;
    for (int t = 0; t < static_cast<int>(UpgradeType::COUNT); ++t) {
        int tier = get_tier(static_cast<UpgradeType>(t));
        for (int i = 0; i < tier; ++i) {
            total += get_titanium_cost(i);
        }
    }
    return total;
}

void UpgradeTree::respec(int& out_refunded_coins, int& out_refunded_voidite, int& out_refunded_titanium) {
    // 100% full recovery of all spent Coins, Voidite, and Titanium
    out_refunded_coins = get_total_spent_coins();
    out_refunded_voidite = get_total_spent_voidite();
    out_refunded_titanium = get_total_spent_titanium();

    drillSpeedTier = 0;
    drillDurabilityTier = 0;
    thrusterTankTier = 0;
    kineticDynamoTier = 0;
    sonarFrequencyTier = 0;
    reinforcedPlatingTier = 0;
}

int UpgradeTree::respec(int& out_refunded_coins) {
    int dummy_v = 0;
    int dummy_t = 0;
    respec(out_refunded_coins, dummy_v, dummy_t);
    return out_refunded_coins;
}

UpgradeInfo UpgradeTree::get_info(UpgradeType type) {
    switch (type) {
        case UpgradeType::DrillSpeed:
            return UpgradeInfo{
                type,
                "Subterranean Drill Velocity",
                "DRILL MATRIX",
                "EXCAVATION",
                "+12% mining excavation rate per tier.",
                "%",
                12.0f,
                true,
                {"Carbide Rotary Bit", "Tungsten Flute Teeth", "Diamond Core Sintering", "High-RPM Vibro-Overclock", "Subterranean Voidbreaker"},
                "Tier 5: Excavation rate reaches +60% maximum drill overdrive."
            };
        case UpgradeType::DrillDurability:
            return UpgradeInfo{
                type,
                "Spindle Heat Sinks & Spinup",
                "DRILL MATRIX",
                "EXCAVATION",
                "-15% thermal buildup and +20% passive heat cooling rate per tier.",
                "%",
                15.0f,
                false,
                {"Conductive Thermal Fins", "Cryo-Coolant Conduit", "Ceramic Heat Shrouds", "Active Vapor Chamber", "Zero-Thermal Overdrive"},
                "Requires Drill Velocity T1. Reduces heat buildup by up to 75%."
            };
        case UpgradeType::ThrusterTank:
            return UpgradeInfo{
                type,
                "Pressurized Jetpack Reservoir",
                "EXO-SUIT & MOBILITY",
                "TRAVERSAL",
                "+20% thruster fuel capacity and hover endurance per tier.",
                "%",
                20.0f,
                true,
                {"Auxiliary Fuel Bladder", "Carbon Filament Tank", "Dual-Chamber Injector", "Cryo-Propellant Density", "Orbital Vector Thrusters"},
                "Doubles maximum thruster power at Tier 5 (+100% capacity)."
            };
        case UpgradeType::KineticDynamo:
            return UpgradeInfo{
                type,
                "Kinetic Dynamo Converter",
                "EXO-SUIT & MOBILITY",
                "TRAVERSAL",
                "Sprinting and falling regenerates thruster fuel +15% faster per tier.",
                "%",
                15.0f,
                false,
                {"Inertial Piezo-Gels", "Impact Recovery Harness", "High-Density Capacitor", "Superconducting Dynamo", "Perpetual Kinetic Flywheel"},
                "Requires Thruster Reservoir T1. Recharges fuel mid-sprint and during descents."
            };
        case UpgradeType::ReinforcedPlating:
            return UpgradeInfo{
                type,
                "Ablative Hazard Plating",
                "SURVEYING & DEFENSE",
                "DEFENSE",
                "+15 Max Suit Integrity HP and -10% cave-in impact damage per tier.",
                " HP",
                15.0f,
                true,
                {"Reinforced Composite Weave", "Ablative Ceramic Tiles", "Titanium Skeleton Ribs", "Reactive Impact Dampeners", "Vanguard Bastion Carapace"},
                "+75 Suit Integrity and up to 50% cave-in damage mitigation."
            };
        case UpgradeType::SonarFrequency:
            return UpgradeInfo{
                type,
                "Wide-Spectrum Sonar Transceiver",
                "SURVEYING & DEFENSE",
                "DEFENSE",
                "+2.0m scan pulse radius and -1.0s cooldown per tier.",
                "m",
                2.0f,
                true,
                {"Wideband Acoustic Pinger", "Acoustic Spectroscopy", "Deep Resonance Sub-Pulse", "Harmonic Geo-Penetration", "Omni-Seismic Overdrive"},
                "Tier 2 unlocks Acoustic Spectroscopy (HUD Rock Labels)! Expands pulse radius up to +10m."
            };
        default:
            return UpgradeInfo{
                type,
                "Unknown Calibration",
                "GENERAL",
                "GENERAL",
                "No description available.",
                "",
                0.0f,
                true,
                {},
                ""
            };
    }
}

std::string UpgradeTree::get_current_stat_string(UpgradeType type) const {
    int cur = get_tier(type);
    auto info = get_info(type);
    if (type == UpgradeType::SonarFrequency) {
        if (cur == 0) return "+0m Radius (10.0s CD)";
        std::string s = "+" + std::to_string(cur * 2) + "m Radius (" + std::to_string(10 - cur) + ".0s CD)";
        if (cur >= 2) s += " [HUD Labels Active]";
        return s;
    }
    if (type == UpgradeType::DrillDurability) {
        if (cur == 0) return "0% Heat Reduction, 0% Cooling";
        return "-" + std::to_string(cur * 15) + "% Heat, +" + std::to_string(cur * 20) + "% Cool";
    }
    if (type == UpgradeType::ReinforcedPlating) {
        if (cur == 0) return "+0 HP, 0% Debris Reduction";
        return "+" + std::to_string(cur * 15) + " HP, -" + std::to_string(cur * 10) + "% Debris Dmg";
    }
    if (cur == 0) return "+0" + info.statUnit;
    return "+" + std::to_string(static_cast<int>(cur * info.baseStatBonusPerTier)) + info.statUnit;
}

std::string UpgradeTree::get_next_stat_string(UpgradeType type) const {
    int cur = get_tier(type);
    if (cur >= MAX_TIER) return "MAX RANK MASTERED";
    auto info = get_info(type);
    int next_tier = cur + 1;
    if (type == UpgradeType::SonarFrequency) {
        std::string s = "+" + std::to_string(next_tier * 2) + "m Radius (" + std::to_string(10 - next_tier) + ".0s CD)";
        if (next_tier == 2) s += " [Unlocks HUD Labels!]";
        return s;
    }
    if (type == UpgradeType::DrillDurability) {
        return "-" + std::to_string(next_tier * 15) + "% Heat, +" + std::to_string(next_tier * 20) + "% Cool";
    }
    if (type == UpgradeType::ReinforcedPlating) {
        return "+" + std::to_string(next_tier * 15) + " HP, -" + std::to_string(next_tier * 10) + "% Debris Dmg";
    }
    return "+" + std::to_string(static_cast<int>(next_tier * info.baseStatBonusPerTier)) + info.statUnit;
}

std::string UpgradeTree::get_stat_preview(UpgradeType type) const {
    int cur = get_tier(type);
    if (cur >= MAX_TIER) {
        return "MAXED (" + get_current_stat_string(type) + ")";
    }
    return get_current_stat_string(type) + " -> " + get_next_stat_string(type);
}

} // namespace Voidfall
