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
    // Cost(tier) = BaseCoin * 1.6^tier (100 -> 160 -> 256 -> 410 -> 656 Coins)
    static const int s_coin_costs[MAX_TIER] = {100, 160, 256, 410, 656};
    if (current_tier >= 0 && current_tier < MAX_TIER) {
        return s_coin_costs[current_tier];
    }
    float cost = 100.0f * std::pow(1.6f, static_cast<float>(current_tier));
    return static_cast<int>(std::round(cost));
}

int UpgradeTree::get_voidite_cost(int current_tier) {
    if (current_tier >= MAX_TIER) return 0;
    return 4 * (current_tier + 1); // 4, 8, 12, 16, 20
}

int UpgradeTree::get_titanium_cost(int current_tier) {
    if (current_tier >= MAX_TIER) return 0;
    return 3 * (current_tier + 1); // 3, 6, 9, 12, 15
}

bool UpgradeTree::can_purchase(UpgradeType type, int player_level, int current_coins, int current_voidite, int current_titanium) const {
    int tier = get_tier(type);
    if (tier >= MAX_TIER) return false;
    
    // Level gating: Tier 1 requires Level 1, Tier 2 requires Level 2, etc.
    int req_level = get_required_level_for_tier(tier);
    if (player_level < req_level) return false;

    int coin_needed = get_coin_cost(tier);
    int voidite_needed = get_voidite_cost(tier);
    int titanium_needed = get_titanium_cost(tier);

    return (current_coins >= coin_needed && current_voidite >= voidite_needed && current_titanium >= titanium_needed);
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

int UpgradeTree::respec(int& out_refunded_coins) {
    int total_spent = get_total_spent_coins();
    out_refunded_coins = static_cast<int>(std::round(total_spent * 0.85f)); // 85% refund

    drillSpeedTier = 0;
    drillDurabilityTier = 0;
    thrusterTankTier = 0;
    kineticDynamoTier = 0;
    sonarFrequencyTier = 0;
    reinforcedPlatingTier = 0;

    return out_refunded_coins;
}

UpgradeInfo UpgradeTree::get_info(UpgradeType type) {
    switch (type) {
        case UpgradeType::DrillSpeed:
            return {
                type,
                "Subterranean Drill Velocity",
                "DRILL MATRIX",
                "+12% mining excavation rate per tier.",
                "%",
                12.0f
            };
        case UpgradeType::DrillDurability:
            return {
                type,
                "Spindle Heat Sinks & Spinup",
                "DRILL MATRIX",
                "-15% thermal buildup and accelerated torque spinup.",
                "%",
                15.0f
            };
        case UpgradeType::ThrusterTank:
            return {
                type,
                "Pressurized Jetpack Reservoir",
                "EXO-SUIT & MOBILITY",
                "+20% thruster fuel capacity and hover endurance.",
                "%",
                20.0f
            };
        case UpgradeType::KineticDynamo:
            return {
                type,
                "Kinetic Dynamo Converter",
                "EXO-SUIT & MOBILITY",
                "Movement and falls regenerate fuel +15% faster.",
                "%",
                15.0f
            };
        case UpgradeType::SonarFrequency:
            return {
                type,
                "Wide-Spectrum Sonar Transceiver",
                "SURVEYING & DEFENSE",
                "+2.0m scan pulse radius, -1.0s cooldown (Rank 2 unlocks HUD Rock Labels).",
                "m",
                2.0f
            };
        case UpgradeType::ReinforcedPlating:
            return {
                type,
                "Ablative Hazard Plating",
                "SURVEYING & DEFENSE",
                "+15 Max Suit Integrity HP and -10% cave-in impact damage.",
                " HP",
                15.0f
            };
        default:
            return {
                type,
                "Unknown Calibration",
                "GENERAL",
                "No description available.",
                "",
                0.0f
            };
    }
}

std::string UpgradeTree::get_stat_preview(UpgradeType type) const {
    int cur = get_tier(type);
    auto info = get_info(type);

    if (type == UpgradeType::SonarFrequency) {
        if (cur >= MAX_TIER) {
            return "MAXED (+10m, -5s cd, Labels)";
        }
        if (cur == 0) {
            return "+0m (10s cd) -> +2m (9s cd)";
        }
        if (cur == 1) {
            return "+2m -> +4m (Unlocks HUD Labels)";
        }
        return "+" + std::to_string(cur * 2) + "m (" + std::to_string(10 - cur) + "s cd) -> +" +
               std::to_string((cur + 1) * 2) + "m (" + std::to_string(10 - (cur + 1)) + "s cd)";
    }

    if (cur >= MAX_TIER) {
        float total = cur * info.baseStatBonusPerTier;
        return "MAXED (+" + std::to_string(static_cast<int>(total)) + info.statUnit + ")";
    }

    float cur_val = cur * info.baseStatBonusPerTier;
    float next_val = (cur + 1) * info.baseStatBonusPerTier;

    return "+" + std::to_string(static_cast<int>(cur_val)) + info.statUnit +
           " -> +" + std::to_string(static_cast<int>(next_val)) + info.statUnit;
}

} // namespace Voidfall
