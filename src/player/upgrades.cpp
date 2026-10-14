#include "upgrades.hpp"
#include <cmath>
#include <algorithm>

namespace Voidfall {

int UpgradeTree::get_max_tier(UpgradeType type) {
    switch (type) {
        // Foundation root nodes (Max 5)
        case UpgradeType::DrillSpeed:
        case UpgradeType::ThrusterTank:
        case UpgradeType::ReinforcedPlating:
        case UpgradeType::AcousticDampening:
        case UpgradeType::PneumaticImpact:
        case UpgradeType::HyperVelocitySlugs:
            return 5;

        // Specialization nodes (Max 3)
        case UpgradeType::DrillDurability:
        case UpgradeType::KineticDynamo:
        case UpgradeType::ImpactDampeners:
        case UpgradeType::RadiationShielding:
        case UpgradeType::ThermalInsulation:
        case UpgradeType::KineticShielding:
        case UpgradeType::ShadowVeil:
        case UpgradeType::CleaveShockwave:
        case UpgradeType::RapidCycling:
        case UpgradeType::ApexBossDamage:
            return 3;

        // Capstone nodes & Tech nodes (Max 2)
        case UpgradeType::MicroCharges:
        case UpgradeType::SonarFrequency:
        case UpgradeType::Defibrillator:
        case UpgradeType::AmbushProtocol:
        case UpgradeType::BerserkerOverdrive:
        case UpgradeType::CriticalWeakpoint:
            return 2;

        default:
            return 5;
    }
}

int UpgradeTree::get_tier(UpgradeType type) const {
    switch (type) {
        // Excavation
        case UpgradeType::DrillSpeed:        return drillSpeedTier;
        case UpgradeType::DrillDurability:   return drillDurabilityTier;
        case UpgradeType::MicroCharges:      return microChargesTier;

        // Traversal & Scan
        case UpgradeType::ThrusterTank:      return thrusterTankTier;
        case UpgradeType::KineticDynamo:     return kineticDynamoTier;
        case UpgradeType::ImpactDampeners:   return impactDampenersTier;
        case UpgradeType::SonarFrequency:    return sonarFrequencyTier;

        // Defense & Hazards
        case UpgradeType::ReinforcedPlating: return reinforcedPlatingTier;
        case UpgradeType::RadiationShielding:return radiationShieldingTier;
        case UpgradeType::ThermalInsulation: return thermalInsulationTier;
        case UpgradeType::KineticShielding:  return kineticShieldingTier;
        case UpgradeType::Defibrillator:     return defibrillatorTier;

        // Stealth
        case UpgradeType::AcousticDampening: return acousticDampeningTier;
        case UpgradeType::ShadowVeil:        return shadowVeilTier;
        case UpgradeType::AmbushProtocol:    return ambushProtocolTier;

        // Melee
        case UpgradeType::PneumaticImpact:   return pneumaticImpactTier;
        case UpgradeType::CleaveShockwave:   return cleaveShockwaveTier;
        case UpgradeType::BerserkerOverdrive:return berserkerOverdriveTier;

        // Gun Damage & Apex Hunting
        case UpgradeType::HyperVelocitySlugs:return hyperVelocitySlugsTier;
        case UpgradeType::RapidCycling:      return rapidCyclingTier;
        case UpgradeType::ApexBossDamage:    return apexBossDamageTier;
        case UpgradeType::CriticalWeakpoint: return criticalWeakpointTier;

        default: return 0;
    }
}

void UpgradeTree::set_tier(UpgradeType type, int tier) {
    int max_t = get_max_tier(type);
    int clamped = std::clamp(tier, 0, max_t);
    switch (type) {
        // Excavation
        case UpgradeType::DrillSpeed:        drillSpeedTier = clamped; break;
        case UpgradeType::DrillDurability:   drillDurabilityTier = clamped; break;
        case UpgradeType::MicroCharges:      microChargesTier = clamped; break;

        // Traversal & Scan
        case UpgradeType::ThrusterTank:      thrusterTankTier = clamped; break;
        case UpgradeType::KineticDynamo:     kineticDynamoTier = clamped; break;
        case UpgradeType::ImpactDampeners:   impactDampenersTier = clamped; break;
        case UpgradeType::SonarFrequency:    sonarFrequencyTier = clamped; break;

        // Defense & Hazards
        case UpgradeType::ReinforcedPlating: reinforcedPlatingTier = clamped; break;
        case UpgradeType::RadiationShielding:radiationShieldingTier = clamped; break;
        case UpgradeType::ThermalInsulation: thermalInsulationTier = clamped; break;
        case UpgradeType::KineticShielding:  kineticShieldingTier = clamped; break;
        case UpgradeType::Defibrillator:     defibrillatorTier = clamped; break;

        // Stealth
        case UpgradeType::AcousticDampening: acousticDampeningTier = clamped; break;
        case UpgradeType::ShadowVeil:        shadowVeilTier = clamped; break;
        case UpgradeType::AmbushProtocol:    ambushProtocolTier = clamped; break;

        // Melee
        case UpgradeType::PneumaticImpact:   pneumaticImpactTier = clamped; break;
        case UpgradeType::CleaveShockwave:   cleaveShockwaveTier = clamped; break;
        case UpgradeType::BerserkerOverdrive:berserkerOverdriveTier = clamped; break;

        // Gun Damage & Apex Hunting
        case UpgradeType::HyperVelocitySlugs:hyperVelocitySlugsTier = clamped; break;
        case UpgradeType::RapidCycling:      rapidCyclingTier = clamped; break;
        case UpgradeType::ApexBossDamage:    apexBossDamageTier = clamped; break;
        case UpgradeType::CriticalWeakpoint: criticalWeakpointTier = clamped; break;

        default: break;
    }
}

int UpgradeTree::get_total_assigned_points() const {
    int total = 0;
    for (int t = 0; t < static_cast<int>(UpgradeType::COUNT); ++t) {
        total += get_tier(static_cast<UpgradeType>(t));
    }
    return total;
}

int UpgradeTree::get_exp_cost(int current_tier) {
    if (current_tier >= MAX_TIER) return 0;
    // Exponential EXP Curve: 100 -> 160 -> 256 -> 410 -> 656 EXP
    static const int s_exp_costs[MAX_TIER] = {100, 160, 256, 410, 656};
    if (current_tier >= 0 && current_tier < MAX_TIER) {
        return s_exp_costs[current_tier];
    }
    float cost = 100.0f * std::pow(1.6f, static_cast<float>(current_tier));
    return static_cast<int>(std::round(cost));
}

TierCostInfo UpgradeTree::get_tier_cost_info(int target_tier) {
    int tier_idx = std::clamp(target_tier - 1, 0, MAX_TIER - 1);
    int cost = get_exp_cost(tier_idx);
    return TierCostInfo{
        target_tier,
        get_required_level_for_tier(tier_idx),
        cost,
        cost, // coin_cost alias
        0,    // voidite_cost
        0     // titanium_cost
    };
}

UpgradePrerequisite UpgradeTree::get_prerequisite(UpgradeType type) {
    switch (type) {
        // EXCAVATION
        case UpgradeType::DrillSpeed:
            return UpgradePrerequisite{false, UpgradeType::DrillSpeed, 0, "None", "Foundational excavation rotary bit."};
        case UpgradeType::DrillDurability:
            return UpgradePrerequisite{true, UpgradeType::DrillSpeed, 1, "Drill Velocity Tier 1", "Requires basic bit stabilization before thermal cooling channels can be routed."};
        case UpgradeType::MicroCharges:
            return UpgradePrerequisite{true, UpgradeType::DrillDurability, 1, "Heat Sinks Tier 1", "Requires thermal dissipation conduit to mount directional micro-charges."};

        // TRAVERSAL & SCAN
        case UpgradeType::ThrusterTank:
            return UpgradePrerequisite{false, UpgradeType::ThrusterTank, 0, "None", "Foundational jetpack fuel reservoir."};
        case UpgradeType::KineticDynamo:
            return UpgradePrerequisite{true, UpgradeType::ThrusterTank, 1, "Thruster Reservoir Tier 1", "Requires active jetpack tank manifold to interface kinetic recovery."};
        case UpgradeType::ImpactDampeners:
            return UpgradePrerequisite{true, UpgradeType::ThrusterTank, 1, "Thruster Reservoir Tier 1", "Requires jetpack thrusters to coordinate inertial impact retro-burns."};
        case UpgradeType::SonarFrequency:
            return UpgradePrerequisite{false, UpgradeType::SonarFrequency, 0, "None", "Foundational seismic surveying transceiver."};

        // DEFENSE & HAZARD RESISTANCES
        case UpgradeType::ReinforcedPlating:
            return UpgradePrerequisite{false, UpgradeType::ReinforcedPlating, 0, "None", "Foundational suit integrity plating."};
        case UpgradeType::RadiationShielding:
            return UpgradePrerequisite{true, UpgradeType::ReinforcedPlating, 1, "Ablative Plating Tier 1", "Requires chassis hardpoints to install lead-polymorphic radiation shielding."};
        case UpgradeType::ThermalInsulation:
            return UpgradePrerequisite{true, UpgradeType::ReinforcedPlating, 1, "Ablative Plating Tier 1", "Requires suit armor base before thermite slag lava mesh can be laminated."};
        case UpgradeType::KineticShielding:
            return UpgradePrerequisite{true, UpgradeType::ReinforcedPlating, 1, "Ablative Plating Tier 1", "Requires chassis hardpoints to mount kinetic shield buffer capacitors."};
        case UpgradeType::Defibrillator:
            return UpgradePrerequisite{true, UpgradeType::KineticShielding, 1, "Kinetic Shielding Tier 1", "Requires shield power grid to route emergency second wind resuscitation pulse."};

        // STEALTH
        case UpgradeType::AcousticDampening:
            return UpgradePrerequisite{false, UpgradeType::AcousticDampening, 0, "None", "Foundational acoustic vibration dampeners for boots and knee servos."};
        case UpgradeType::ShadowVeil:
            return UpgradePrerequisite{true, UpgradeType::AcousticDampening, 1, "Acoustic Dampening Tier 1", "Requires sound suppression base before light-scattering mesh can calibrate."};
        case UpgradeType::AmbushProtocol:
            return UpgradePrerequisite{true, UpgradeType::ShadowVeil, 1, "Shadow Veil Tier 1", "Requires camouflage distortion field to coordinate silent ambush strikes."};

        // MELEE
        case UpgradeType::PneumaticImpact:
            return UpgradePrerequisite{false, UpgradeType::PneumaticImpact, 0, "None", "Foundational pneumatic piston boosters for quick melee shove strikes."};
        case UpgradeType::CleaveShockwave:
            return UpgradePrerequisite{true, UpgradeType::PneumaticImpact, 1, "Pneumatic Impact Tier 1", "Requires pneumatic ram reinforcement to sweep wide 90-degree cleave arcs."};
        case UpgradeType::BerserkerOverdrive:
            return UpgradePrerequisite{true, UpgradeType::CleaveShockwave, 1, "Cleave Shockwave Tier 1", "Requires shock arc capacitors to siphon kinetic feedback into shield & speed."};

        // GUN DAMAGE & APEX HUNTING
        case UpgradeType::HyperVelocitySlugs:
            return UpgradePrerequisite{false, UpgradeType::HyperVelocitySlugs, 0, "None", "Foundational accelerated magnetic coil rails for firearm ballistic damage."};
        case UpgradeType::RapidCycling:
            return UpgradePrerequisite{true, UpgradeType::HyperVelocitySlugs, 1, "Hyper-Velocity Slugs Tier 1", "Requires barrel stabilization to support rapid bolt cycling & fast reloads."};
        case UpgradeType::ApexBossDamage:
            return UpgradePrerequisite{true, UpgradeType::HyperVelocitySlugs, 1, "Hyper-Velocity Slugs Tier 1", "Requires magnetic rail velocity to punch through Chitin Goliath boss armor."};
        case UpgradeType::CriticalWeakpoint:
            return UpgradePrerequisite{true, UpgradeType::RapidCycling, 1, "Rapid Cycling Tier 1", "Requires high-frequency cycling to target void monster cranial weakpoints."};

        default:
            return UpgradePrerequisite{false, UpgradeType::DrillSpeed, 0, "None", ""};
    }
}

UpgradeLockReason UpgradeTree::get_lock_reason(UpgradeType type, int player_level, int current_exp, bool is_game_completed) const {
    int tier = get_tier(type);
    int max_tier = get_max_tier(type);
    if (tier >= max_tier) return UpgradeLockReason::MaxTier;

    auto prereq = get_prerequisite(type);
    if (prereq.has_prerequisite && get_tier(prereq.required_type) < prereq.required_tier) {
        return UpgradeLockReason::PrerequisiteLocked;
    }

    // Campaign progression cap: before completing all sectors, assignable points are capped at 24
    if (!is_game_completed && get_total_assigned_points() >= CAMPAIGN_POINT_CAP) {
        return UpgradeLockReason::CampaignCapReached;
    }

    int req_level = get_required_level_for_tier(tier);
    if (player_level < req_level) return UpgradeLockReason::LevelLocked;

    int exp_needed = get_exp_cost(tier);
    if (current_exp < exp_needed) return UpgradeLockReason::InsufficientEXP;

    return UpgradeLockReason::Unlocked;
}

std::string UpgradeTree::get_lock_reason_string(UpgradeType type, int player_level, int current_exp, bool is_game_completed) const {
    int tier = get_tier(type);
    UpgradeLockReason reason = get_lock_reason(type, player_level, current_exp, is_game_completed);
    switch (reason) {
        case UpgradeLockReason::MaxTier:
            return "MAX TIER MASTERED";
        case UpgradeLockReason::PrerequisiteLocked: {
            auto prereq = get_prerequisite(type);
            return "GREYED OUT (REQUIRES: " + prereq.required_name + ")";
        }
        case UpgradeLockReason::CampaignCapReached:
            return "CAMPAIGN CAP (24/24 PTS) REACHED. RESPEC TO SPECIALIZE IN OTHER BRANCHES OR CLEAR SECTOR 3 TO UNLOCK GRANDMASTER (60 PTS)";
        case UpgradeLockReason::LevelLocked:
            return "REQUIRES DELVER LEVEL " + std::to_string(get_required_level_for_tier(tier));
        case UpgradeLockReason::InsufficientEXP: {
            int need = get_exp_cost(tier) - current_exp;
            return "LACKING EXP (Need " + std::to_string(need) + " more)";
        }
        case UpgradeLockReason::Unlocked:
        default:
            return "READY TO ACQUIRE";
    }
}

bool UpgradeTree::can_purchase(UpgradeType type, int player_level, int current_exp, bool is_game_completed) const {
    return get_lock_reason(type, player_level, current_exp, is_game_completed) == UpgradeLockReason::Unlocked;
}

bool UpgradeTree::purchase(UpgradeType type, int player_level, int& inout_exp, bool is_game_completed) {
    if (!can_purchase(type, player_level, inout_exp, is_game_completed)) return false;

    int tier = get_tier(type);
    int exp_cost = get_exp_cost(tier);

    inout_exp -= exp_cost;
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
    // 100% full recovery of all spent EXP
    out_refunded_exp = get_total_spent_exp();

    drillSpeedTier = 0;
    drillDurabilityTier = 0;
    microChargesTier = 0;

    thrusterTankTier = 0;
    kineticDynamoTier = 0;
    sonarFrequencyTier = 0;

    reinforcedPlatingTier = 0;
    kineticShieldingTier = 0;
    defibrillatorTier = 0;

    acousticDampeningTier = 0;
    shadowVeilTier = 0;
    ambushProtocolTier = 0;

    pneumaticImpactTier = 0;
    cleaveShockwaveTier = 0;
    berserkerOverdriveTier = 0;

    hyperVelocitySlugsTier = 0;
    rapidCyclingTier = 0;
    criticalWeakpointTier = 0;

    impactDampenersTier = 0;
    radiationShieldingTier = 0;
    thermalInsulationTier = 0;
    apexBossDamageTier = 0;

    return out_refunded_exp;
}

NodeWebState UpgradeTree::get_node_web_state(UpgradeType type, int player_level, int current_exp, bool is_game_completed) const {
    int cur = get_tier(type);
    int max_t = get_max_tier(type);
    if (cur >= max_t) return NodeWebState::Mastered;

    auto prereq = get_prerequisite(type);
    if (prereq.has_prerequisite && get_tier(prereq.required_type) < prereq.required_tier) {
        return NodeWebState::GreyedOutLocked;
    }

    if (!is_game_completed && get_total_assigned_points() >= CAMPAIGN_POINT_CAP) {
        return NodeWebState::CampaignCapped;
    }

    if (cur > 0) {
        return NodeWebState::Purchased;
    }

    int req_lvl = get_required_level_for_tier(cur);
    if (player_level < req_lvl) {
        return NodeWebState::LevelGated;
    }

    int exp_needed = get_exp_cost(cur);
    if (current_exp < exp_needed) {
        return NodeWebState::InsufficientEXP;
    }

    return NodeWebState::Available;
}

UpgradeInfo UpgradeTree::get_info(UpgradeType type) {
    switch (type) {
        // EXCAVATION (DRILL MATRIX)
        case UpgradeType::DrillSpeed:
            return UpgradeInfo{
                type,
                "Subterranean Drill Velocity",
                "DRILL SURGE",
                "+12% MINING SPD",
                "<drill>",
                "DRILL MATRIX",
                "EXCAVATION",
                "+12% mining excavation rate per tier.",
                "%",
                12.0f,
                true,
                {"Carbide Rotary Bit", "Tungsten Flute Teeth", "Diamond Core Sintering", "High-RPM Vibro-Overclock", "Subterranean Voidbreaker"},
                "Tier 5: Excavation rate reaches +60% maximum drill overdrive.",
                5,
                -180.0f, -90.0f
            };
        case UpgradeType::DrillDurability:
            return UpgradeInfo{
                type,
                "Spindle Heat Sinks & Spinup",
                "HEAT SINKS",
                "-15% HEAT / +20% COOL",
                "<flare>",
                "DRILL MATRIX",
                "EXCAVATION",
                "-15% thermal buildup and +20% passive heat cooling rate per tier.",
                "%",
                15.0f,
                false,
                {"Conductive Thermal Fins", "Cryo-Coolant Conduit", "Active Vapor Chamber"},
                "Requires Drill Velocity T1. Reduces heat buildup by up to 45%.",
                3,
                -330.0f, -150.0f
            };
        case UpgradeType::MicroCharges:
            return UpgradeInfo{
                type,
                "Surgical Micro-Charges",
                "MICRO BLAST",
                "+20% ORE YIELD",
                "<salvage>",
                "DRILL MATRIX",
                "EXCAVATION",
                "+20% high-grade ore and rare mineral extraction yield per tier.",
                "%",
                20.0f,
                false,
                {"Directional Micro-Blasting", "Resonance Cavitation Core"},
                "Requires Heat Sinks T1. Harvests +40% mineral yield from pristine veins.",
                2,
                -470.0f, -220.0f
            };

        // TRAVERSAL & SCAN (EXO-SUIT & SONAR)
        case UpgradeType::ThrusterTank:
            return UpgradeInfo{
                type,
                "Pressurized Jetpack Reservoir",
                "FUEL TANK",
                "+20% JETPACK FUEL",
                "<evac>",
                "EXO-SUIT & MOBILITY",
                "TRAVERSAL",
                "+20% thruster fuel capacity and hover endurance per tier.",
                "%",
                20.0f,
                true,
                {"Auxiliary Fuel Bladder", "Carbon Filament Tank", "Dual-Chamber Injector", "Cryo-Propellant Density", "Orbital Vector Thrusters"},
                "Doubles maximum thruster power at Tier 5 (+100% capacity).",
                5,
                -180.0f, 90.0f
            };
        case UpgradeType::KineticDynamo:
            return UpgradeInfo{
                type,
                "Kinetic Dynamo Converter",
                "DYNAMO COIL",
                "+15% FUEL RECOVERY",
                "<scrap>",
                "EXO-SUIT & MOBILITY",
                "TRAVERSAL",
                "Sprinting and falling regenerates thruster fuel +15% faster per tier.",
                "%",
                15.0f,
                false,
                {"Inertial Piezo-Gels", "High-Density Capacitor", "Perpetual Kinetic Flywheel"},
                "Requires Thruster Reservoir T1. Recharges fuel mid-sprint and during descents.",
                3,
                -320.0f, 60.0f
            };
        case UpgradeType::ImpactDampeners:
            return UpgradeInfo{
                type,
                "Inertial Impact Dampeners",
                "FALL DAMPENER",
                "-25% FALL DAMAGE",
                "<shield>",
                "EXO-SUIT & MOBILITY",
                "TRAVERSAL",
                "Reduces high-speed landing fall impact damage by -25% per tier.",
                "%",
                25.0f,
                false,
                {"Pneumatic Heel Shocks", "Magneto-Rheological Struts", "Apex Ground-Pound Dissipator"},
                "Requires Thruster Reservoir T1. Reduces lethal drop fall damage by up to 75%.",
                3,
                -320.0f, 170.0f
            };
        case UpgradeType::SonarFrequency:
            return UpgradeInfo{
                type,
                "Wide-Spectrum Sonar Transceiver",
                "SEISMIC EYE",
                "+2m RAD & HUD LABELS",
                "<sonar>",
                "SURVEYING & TECH",
                "TRAVERSAL",
                "+2.0m scan pulse radius and -1.0s cooldown per tier.",
                "m",
                2.0f,
                true,
                {"Wideband Acoustic Pinger", "Acoustic Spectroscopy"},
                "Tier 2 unlocks Acoustic Spectroscopy (HUD Rock & Mineral Labels)!",
                2,
                -200.0f, 240.0f
            };

        // DEFENSE & HAZARD RESISTANCES (BASTION & WARDS)
        case UpgradeType::ReinforcedPlating:
            return UpgradeInfo{
                type,
                "Ablative Hazard Plating",
                "ARMOR PLATING",
                "+15 HP / -10% DEBRIS",
                "<shield>",
                "SURVIVAL & DEFENSE",
                "DEFENSE",
                "+15 Max Suit Integrity HP and -10% cave-in impact damage per tier.",
                " HP",
                15.0f,
                true,
                {"Reinforced Composite Weave", "Ablative Ceramic Tiles", "Titanium Skeleton Ribs", "Reactive Dampeners", "Bastion Carapace"},
                "+75 Suit Integrity and up to 50% cave-in damage mitigation.",
                5,
                180.0f, -90.0f
            };
        case UpgradeType::RadiationShielding:
            return UpgradeInfo{
                type,
                "Lead-Polymorphic Rad Shield",
                "RAD SHIELD",
                "-25% RAD EXPOSURE",
                "<hazard>",
                "SURVIVAL & DEFENSE",
                "DEFENSE",
                "-25% ionizing radiation accumulation rate and radiation damage per tier.",
                "%",
                25.0f,
                false,
                {"Bismuth Polymer Lining", "Thorium-Isotope Dampeners", "Apex Rad-Hardened Chassis"},
                "Requires Ablative Plating T1. Cuts environmental and radioactive ore damage by up to 75%.",
                3,
                320.0f, -50.0f
            };
        case UpgradeType::ThermalInsulation:
            return UpgradeInfo{
                type,
                "Thermite Slag Lava Mesh",
                "LAVA MESH",
                "-25% LAVA BURN DMG",
                "<flare>",
                "SURVIVAL & DEFENSE",
                "DEFENSE",
                "-25% molten lava contact and thermal thermite burn damage per tier.",
                "%",
                25.0f,
                false,
                {"Aerogel Thermal Barrier", "Zirconia Heat Shield", "Subterranean Magma Coating"},
                "Requires Ablative Plating T1. Provides up to 75% protection against lethal magma floors.",
                3,
                320.0f, -160.0f
            };
        case UpgradeType::KineticShielding:
            return UpgradeInfo{
                type,
                "Kinetic Shield Buffer",
                "KINETIC SHIELD",
                "+12 SHIELD / -0.5s CD",
                "<shield>",
                "SURVIVAL & DEFENSE",
                "DEFENSE",
                "+12 Max Shield capacity and -0.5s shield recharge delay per tier.",
                " SP",
                12.0f,
                false,
                {"Overclocked Capacitors", "Rapid Harmonic Shunt", "Superconducting Shield Core"},
                "Requires Ablative Plating T1. Expands shield pool by +36 and accelerates regen.",
                3,
                200.0f, -230.0f
            };
        case UpgradeType::Defibrillator:
            return UpgradeInfo{
                type,
                "Emergency Defibrillator",
                "SECOND WIND",
                "REVIVE ON FATAL",
                "<health>",
                "SURVIVAL & DEFENSE",
                "DEFENSE",
                "Survives lethal damage, restoring 35 HP with invulnerability buffer.",
                "s CD",
                30.0f,
                false,
                {"Adrenaline Surge Resuscitator", "Immortal Core Overdrive"},
                "Requires Kinetic Shielding T1. Defies death with a second wind (90s-120s cooldown).",
                2,
                380.0f, -250.0f
            };

        // STEALTH (SHADOW INFILTRATION)
        case UpgradeType::AcousticDampening:
            return UpgradeInfo{
                type,
                "Acoustic Dampening Soles",
                "SILENT TREAD",
                "-20% NOISE RADIUS",
                "<audio>",
                "STEALTH INFILTRATION",
                "STEALTH",
                "-20% crouch and footstep vibration noise radius per tier.",
                "%",
                20.0f,
                true,
                {"Silicone Sole Lining", "Pneumatic Bellows", "Acoustic Honeycomb Pads", "Vibration Null-Mesh", "Silent Phantom Tread"},
                "Reduces footstep and crouch vibrations by up to 80% to evade stalkers.",
                5,
                0.0f, 170.0f
            };
        case UpgradeType::ShadowVeil:
            return UpgradeInfo{
                type,
                "Shadow Veil Distortion",
                "SHADOW VEIL",
                "-15% ENEMY SIGHT",
                "<warning>",
                "STEALTH INFILTRATION",
                "STEALTH",
                "-15% enemy optical detection range and sightline awareness per tier.",
                "%",
                15.0f,
                false,
                {"Prismatic Light Diffuser", "Photonic Absorption Skin", "Void Cloak Carapace"},
                "Requires Acoustic Dampening T1. Reduces enemy sightline detection by up to 45%.",
                3,
                0.0f, 290.0f
            };
        case UpgradeType::AmbushProtocol:
            return UpgradeInfo{
                type,
                "Ambush Strike Protocol",
                "AMBUSH STRIKE",
                "+35% SNEAK ATK DMG",
                "<skull>",
                "STEALTH INFILTRATION",
                "STEALTH",
                "+35% damage against unalerted or surprised hostiles per tier.",
                "%",
                35.0f,
                false,
                {"Surprise Kinetic Shunt", "Lethal Shadow Core"},
                "Requires Shadow Veil T1. Delivers devastating +70% ambush critical strikes.",
                2,
                0.0f, 410.0f
            };

        // MELEE (CLOSE COMBAT & SHOCK)
        case UpgradeType::PneumaticImpact:
            return UpgradeInfo{
                type,
                "Pneumatic Shove Piston",
                "PNEUMATIC RAM",
                "+30% MELEE SHOVE DMG",
                "<scrap>",
                "CLOSE COMBAT",
                "MELEE",
                "+30% quick melee shove impact damage and +25% knockback per tier.",
                "%",
                30.0f,
                true,
                {"High-Pressure Actuator", "Tungsten Ram Cap", "Hydraulic Overpressure", "Kinetic Shock Piston", "Titan Breaker Ram"},
                "Melee shove strikes deal up to +150% damage and launch hostiles backward.",
                5,
                0.0f, -170.0f
            };
        case UpgradeType::CleaveShockwave:
            return UpgradeInfo{
                type,
                "Wide Cleave Shockwave",
                "CLEAVE WAVE",
                "90° ARC & SHOCK STUN",
                "<flare>",
                "CLOSE COMBAT",
                "MELEE",
                "Expands melee shove to 90-degree cleave cone and stuns hostiles +0.5s per tier.",
                "s",
                0.5f,
                false,
                {"Flared Concussion Horn", "Vibrational Sweep Rotor", "Sonic Cleave Stun Wave"},
                "Requires Pneumatic Shove T1. Stuns swarms and knocks back all frontal enemies.",
                3,
                0.0f, -290.0f
            };
        case UpgradeType::BerserkerOverdrive:
            return UpgradeInfo{
                type,
                "Berserker Siphon Overdrive",
                "SIPHON STRIKE",
                "MELEE SHIELD SIPHON",
                "<shield>",
                "CLOSE COMBAT",
                "MELEE",
                "Landing melee strikes siphons +12 Shield and grants +15% Speed for 3s per tier.",
                " SP",
                12.0f,
                false,
                {"Kinetic Siphon Manifold", "Adrenaline Blood-Pump"},
                "Requires Cleave Shockwave T1. Siphons up to +24 Shield and grants +30% combat speed.",
                2,
                0.0f, -410.0f
            };

        // GUN DAMAGE & APEX COMBAT (BALLISTICS & BOSS SLAYERS)
        case UpgradeType::HyperVelocitySlugs:
            return UpgradeInfo{
                type,
                "Hyper-Velocity Slugs",
                "HYPER SLUGS",
                "+12% BALLISTIC DMG",
                "<crosshair>",
                "PRECISION BALLISTICS",
                "GUN DAMAGE",
                "+12% firearm ballistic projectile damage and armor piercing per tier.",
                "%",
                12.0f,
                true,
                {"Hardened Carbide Slugs", "Dense Depleted Core", "Magnetic Rail Accelerator", "High-Charge Plasma Jacket", "Void-Piercing Flechettes"},
                "Boosts carbine and shotgun projectile damage by up to +60%.",
                5,
                180.0f, 90.0f
            };
        case UpgradeType::RapidCycling:
            return UpgradeInfo{
                type,
                "Rapid Action Bolt Cycling",
                "RAPID BOLT",
                "+8% RATE / +10% RELOAD",
                "<scrap>",
                "PRECISION BALLISTICS",
                "GUN DAMAGE",
                "+8% weapon fire rate and +10% magazine reload speed per tier.",
                "%",
                8.0f,
                false,
                {"Lightweight Bolt Carrier", "Assisted Mag Release", "Hair-Trigger Actuator"},
                "Requires Hyper-Velocity Slugs T1. Accelerates fire rate (+24%) and reloads (+30%).",
                3,
                310.0f, 60.0f
            };
        case UpgradeType::ApexBossDamage:
            return UpgradeInfo{
                type,
                "Apex Goliath Slayer",
                "BOSS SLAYER",
                "+20% APEX BOSS DMG",
                "<skull>",
                "PRECISION BALLISTICS",
                "GUN DAMAGE",
                "+20% damage dealt to Apex Bosses (Chitin Goliath & Vault Guardians) per tier.",
                "%",
                20.0f,
                false,
                {"Carapace Fracture Core", "High-Energy Penetrator", "Apex Extinction Ammo"},
                "Requires Hyper-Velocity Slugs T1. Shreds boss health pools by up to +60%.",
                3,
                310.0f, 170.0f
            };
        case UpgradeType::CriticalWeakpoint:
            return UpgradeInfo{
                type,
                "Cranial Weakpoint Analysis",
                "CRIT PIERCER",
                "+35% WEAKPOINT CRIT",
                "<crosshair>",
                "PRECISION BALLISTICS",
                "GUN DAMAGE",
                "+35% critical strike damage when targeting monster heads & weakpoints per tier.",
                "%",
                35.0f,
                false,
                {"Optronic Target Telemetry", "Vulnerability Core Seeker"},
                "Requires Rapid Cycling T1. Deals +70% lethal critical damage on headshots.",
                2,
                450.0f, 130.0f
            };

        default:
            return UpgradeInfo{
                type,
                "Unknown Calibration",
                "MODULE",
                "CALIBRATED",
                "<gear>",
                "GENERAL",
                "GENERAL",
                "No description available.",
                "",
                0.0f,
                true,
                {},
                "",
                5,
                0.0f, 0.0f
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
    if (type == UpgradeType::ImpactDampeners) {
        if (cur == 0) return "0% Fall Reduction";
        return "-" + std::to_string(cur * 25) + "% Fall Impact Damage";
    }
    if (type == UpgradeType::RadiationShielding) {
        if (cur == 0) return "0% Rad Resistance";
        return "-" + std::to_string(cur * 25) + "% Radiation Exposure & Dmg";
    }
    if (type == UpgradeType::ThermalInsulation) {
        if (cur == 0) return "0% Lava Resistance";
        return "-" + std::to_string(cur * 25) + "% Molten Lava Burn Dmg";
    }
    if (type == UpgradeType::ApexBossDamage) {
        if (cur == 0) return "+0% Boss Damage";
        return "+" + std::to_string(cur * 20) + "% Damage to Chitin Goliath";
    }
    if (type == UpgradeType::Defibrillator) {
        if (cur == 0) return "Inactive";
        return "Second Wind Active (" + std::to_string(150 - cur * 30) + "s CD)";
    }
    if (type == UpgradeType::AcousticDampening) {
        if (cur == 0) return "0% Noise Reduction";
        return "-" + std::to_string(cur * 20) + "% Vibration Noise";
    }
    if (type == UpgradeType::ShadowVeil) {
        if (cur == 0) return "0% Optical Reduction";
        return "-" + std::to_string(cur * 15) + "% Detection Range";
    }
    if (cur == 0) return "+0" + info.statUnit;
    return "+" + std::to_string(static_cast<int>(cur * info.baseStatBonusPerTier)) + info.statUnit;
}

std::string UpgradeTree::get_next_stat_string(UpgradeType type) const {
    int cur = get_tier(type);
    int max_t = get_max_tier(type);
    if (cur >= max_t) return "MAX RANK MASTERED";
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
    if (type == UpgradeType::ImpactDampeners) {
        return "-" + std::to_string(next_tier * 25) + "% Fall Impact Damage";
    }
    if (type == UpgradeType::RadiationShielding) {
        return "-" + std::to_string(next_tier * 25) + "% Radiation Exposure & Dmg";
    }
    if (type == UpgradeType::ThermalInsulation) {
        return "-" + std::to_string(next_tier * 25) + "% Molten Lava Burn Dmg";
    }
    if (type == UpgradeType::ApexBossDamage) {
        return "+" + std::to_string(next_tier * 20) + "% Damage to Chitin Goliath";
    }
    if (type == UpgradeType::Defibrillator) {
        return "Second Wind Active (" + std::to_string(150 - next_tier * 30) + "s CD)";
    }
    if (type == UpgradeType::AcousticDampening) {
        return "-" + std::to_string(next_tier * 20) + "% Vibration Noise";
    }
    if (type == UpgradeType::ShadowVeil) {
        return "-" + std::to_string(next_tier * 15) + "% Detection Range";
    }
    return "+" + std::to_string(static_cast<int>(next_tier * info.baseStatBonusPerTier)) + info.statUnit;
}

std::string UpgradeTree::get_stat_preview(UpgradeType type) const {
    int cur = get_tier(type);
    int max_t = get_max_tier(type);
    if (cur >= max_t) {
        return "MAXED (" + get_current_stat_string(type) + ")";
    }
    return get_current_stat_string(type) + " -> " + get_next_stat_string(type);
}

} // namespace Voidfall
