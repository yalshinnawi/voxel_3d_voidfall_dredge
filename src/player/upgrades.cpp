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

int UpgradeTree::get_exp_cost(int current_tier) {
    if (current_tier >= MAX_TIER) return 0;
    // Cost(tier) = BaseEXP * 1.6^tier (100 -> 160 -> 256 -> 410 -> 656 EXP)
    static const int s_exp_costs[MAX_TIER] = {100, 160, 256, 410, 656};
    if (current_tier >= 0 && current_tier < MAX_TIER) {
        return s_exp_costs[current_tier];
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

bool UpgradeTree::can_purchase(UpgradeType type, int current_exp, int current_voidite, int current_titanium) const {
    int tier = get_tier(type);
    if (tier >= MAX_TIER) return false;
    int exp_needed = get_exp_cost(tier);
    int voidite_needed = get_voidite_cost(tier);
    int titanium_needed = get_titanium_cost(tier);

    return (current_exp >= exp_needed && current_voidite >= voidite_needed && current_titanium >= titanium_needed);
}

bool UpgradeTree::purchase(UpgradeType type, int& inout_exp, int& inout_voidite, int& inout_titanium) {
    if (!can_purchase(type, inout_exp, inout_voidite, inout_titanium)) return false;

    int tier = get_tier(type);
    int exp_cost = get_exp_cost(tier);
    int voidite_cost = get_voidite_cost(tier);
    int titanium_cost = get_titanium_cost(tier);

    inout_exp -= exp_cost;
    inout_voidite -= voidite_cost;
    inout_titanium -= titanium_cost;

    set_tier(type, tier + 1);
    return true;
}

int UpgradeTree::get_total_spent_exp() const {
    int total = 0;
    for (int t = 0; t < static_cast<int>(UpgradeType::COUNT); ++t) {
        int tier = get_tier(static_cast<UpgradeType>(t));
        for (int i = 0; i < tier; ++i) {
            total += get_exp_cost(i);
        }
    }
    return total;
}

int UpgradeTree::respec(int& out_refunded_exp) {
    int total_spent = get_total_spent_exp();
    out_refunded_exp = static_cast<int>(std::round(total_spent * 0.85f)); // 85% refund

    drillSpeedTier = 0;
    drillDurabilityTier = 0;
    thrusterTankTier = 0;
    kineticDynamoTier = 0;
    sonarFrequencyTier = 0;
    reinforcedPlatingTier = 0;

    return out_refunded_exp;
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
                "+2.0m scan pulse radius and -0.5s cooldown.",
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
