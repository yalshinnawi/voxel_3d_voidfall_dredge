#pragma once
#include <cstdint>
#include <string>
#include <glm/glm.hpp>
#include "character_class.hpp"

namespace Voidfall {

enum class ToolSlot : uint8_t {
    MiningDrill = 0,
    PlasmaCarbine = 1,      // Handheld Delver Combat Weapon (Class-specific firearm)
    CombatWeapon = 1,       // Alias for active class weapon slot
    IndustrialBulkhead = 2,
    DemolitionCharge = 3
};

enum class WeaponArchetype : uint8_t {
    MagmaScattergun = 0, // Demolitionist (Kaelen): Heavy close-range thermite buckshot
    PlasmaCarbine = 1,   // Vanguard (Rhodes): Tactical sustained rapid-pulse plasma rifle
    NeedlerRailgun = 2   // Scout (Vesper): High-velocity electromagnetic marksman rifle
};

struct WeaponStats {
    WeaponArchetype archetype{WeaponArchetype::PlasmaCarbine};
    std::string name{"Plasma Carbine"};
    std::string short_name{"CARB"};
    int max_ammo{16};
    float fire_rate{0.16f};          // Seconds between shots
    float reload_time{1.25f};        // Full reload cycle in seconds
    int pellets{1};                  // Projectiles spawned per trigger pull
    float spread{0.015f};            // Cone angle spread in radians
    float projectile_speed{52.0f};   // Velocity in m/s
    float projectile_lifetime{1.4f}; // Range/lifetime in seconds
    float damage_per_pellet{22.0f};  // Damage per projectile on impact
    float projectile_radius{0.18f};  // Visual tracer beam thickness
    glm::vec4 tracer_color{0.08f, 0.90f, 1.0f, 1.0f}; // Glow tracer color
    float noise_generation{0.12f};   // Noise added to seismic meter
    float trauma_kick{0.04f};        // Camera recoil trauma
    bool auto_recharge{false};       // Strictly false for all weapons: ammo never trickles, player must reload
    float zoom_fov_multiplier{0.65f}; // Target FOV multiplier when zooming / aiming down sights (e.g. 0.65 -> 65% of base FOV, 1.54x zoom)
    float ads_time{0.18f};            // Time to smoothly transition into full zoom (seconds)
};

struct PlayerPlasmaBolt {
    uint32_t id{0};
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec4 color{0.05f, 0.92f, 1.0f, 1.0f};
    float lifetime{1.8f};
    float damage{22.0f};
    float radius{0.06f};
    bool active{true};
};

inline WeaponStats get_weapon_stats_for_archetype(WeaponArchetype arch) {
    WeaponStats w;
    switch (arch) {
        case WeaponArchetype::MagmaScattergun:
            w.archetype = WeaponArchetype::MagmaScattergun;
            w.name = "Magma Scattergun";
            w.short_name = "SCAT";
            w.max_ammo = 6;
            w.fire_rate = 0.45f;
            w.reload_time = 1.6f;
            w.pellets = 5;
            w.spread = 0.085f;
            w.projectile_speed = 38.0f;
            w.projectile_lifetime = 0.65f;
            w.damage_per_pellet = 7.5f;
            w.projectile_radius = 0.12f;
            w.tracer_color = glm::vec4(1.0f, 0.55f, 0.08f, 1.0f);
            w.noise_generation = 0.22f;
            w.trauma_kick = 0.08f;
            w.auto_recharge = false;
            w.zoom_fov_multiplier = 0.78f; // ~1.28x magnification for close-quarters buckshot focus
            w.ads_time = 0.20f;
            break;

        case WeaponArchetype::PlasmaCarbine:
            w.archetype = WeaponArchetype::PlasmaCarbine;
            w.name = "Plasma Carbine";
            w.short_name = "CARB";
            w.max_ammo = 16;
            w.fire_rate = 0.16f;
            w.reload_time = 1.25f;
            w.pellets = 1;
            w.spread = 0.015f;
            w.projectile_speed = 52.0f;
            w.projectile_lifetime = 1.4f;
            w.damage_per_pellet = 22.0f;
            w.projectile_radius = 0.18f;
            w.tracer_color = glm::vec4(0.08f, 0.90f, 1.0f, 1.0f);
            w.noise_generation = 0.12f;
            w.trauma_kick = 0.04f;
            w.auto_recharge = false;
            w.zoom_fov_multiplier = 0.65f; // ~1.54x magnification for tactical carbine fire
            w.ads_time = 0.18f;
            break;

        case WeaponArchetype::NeedlerRailgun:
            w.archetype = WeaponArchetype::NeedlerRailgun;
            w.name = "Needler Railgun";
            w.short_name = "RAIL";
            w.max_ammo = 8;
            w.fire_rate = 0.32f;
            w.reload_time = 1.1f;
            w.pellets = 1;
            w.spread = 0.0f;
            w.projectile_speed = 110.0f;
            w.projectile_lifetime = 1.5f;
            w.damage_per_pellet = 42.0f;
            w.projectile_radius = 0.10f;
            w.tracer_color = glm::vec4(0.15f, 1.0f, 0.45f, 1.0f);
            w.noise_generation = 0.09f;
            w.trauma_kick = 0.05f;
            w.auto_recharge = false;
            w.zoom_fov_multiplier = 0.40f; // ~2.50x sniper marksman scope magnification
            w.ads_time = 0.22f;
            break;
    }
    return w;
}

inline WeaponArchetype get_class_weapon_archetype(CharacterClass cls) {
    switch (cls) {
        case CharacterClass::Demolitionist: return WeaponArchetype::MagmaScattergun;
        case CharacterClass::Vanguard:      return WeaponArchetype::PlasmaCarbine;
        case CharacterClass::Scout:         return WeaponArchetype::NeedlerRailgun;
        default:                            return WeaponArchetype::PlasmaCarbine;
    }
}

inline WeaponStats get_class_weapon_stats(CharacterClass cls) {
    return get_weapon_stats_for_archetype(get_class_weapon_archetype(cls));
}

struct DelverUpgrades {
    int drill_speed_level{0};      // each +20% mining rate
    int thruster_energy_level{0};  // each +25% thruster max/regen
    int sonar_range_level{0};      // each +5m sonar pulse range
    int max_bulkheads_level{0};    // increases max bulkheads capacity (+5 per level)
    int shield_plating_level{0};   // reinforced shield plating (+25% integrity / damage reduction)
};

struct SectorRecord {
    int highest_completion_rate{0};
    std::string best_badge{"UNEXPLORED"};
};

struct PlayerInventory {
    int voidite{0};
    int titanium{0};
    int salvage_parts{0};
    int demolition_charges{3};
    int bulkheads{15};
    int max_bulkheads{20};
    int scrap_metal{0};
    int granite_mined_count{0};
    int total_run_score{0};

    // Upgrades persist across expedition runs
    DelverUpgrades upgrades;

    // Dynamic level objective tracking
    bool vault_breached{false};
    bool relic_extracted{false};
    int target_voidite{25};
    int alphas_eliminated{0};
    int target_alphas{0}; // >0 in endless sectors requiring an alpha kill
    std::string custom_objective_text{""};

    // Mission status & completion tracking
    bool is_abandoned{false};
    bool suit_failed{false};
    bool has_banked_run{false};
    std::string run_outcome_badge{"UNEXPLORED"};
    int run_completion_rate{0};
    int run_coins_earned{0};

    // Stored persistent records for endless sectors (1-indexed up to MAX_SECTOR_RECORDS-1)
    static constexpr int MAX_SECTOR_RECORDS = 32;
    SectorRecord sector_records[MAX_SECTOR_RECORDS] = {};

    int get_max_bulkheads() const {
        return max_bulkheads + upgrades.max_bulkheads_level * 5;
    }

    float carry_weight() const {
        return static_cast<float>(voidite) * 1.0f +
               static_cast<float>(titanium) * 2.5f +
               static_cast<float>(salvage_parts) * 0.4f +
               static_cast<float>(bulkheads) * 2.0f +
               static_cast<float>(demolition_charges) * 1.5f +
               (relic_extracted ? 10.0f : 0.0f);
    }

    float max_carry_weight(int class_id) const {
        // 0: Demolitionist, 1: Vanguard, 2: Scout
        switch (class_id) {
            case 1: return 85.0f; // Vanguard heavy frame
            case 0: return 60.0f; // Demolitionist balanced
            case 2: return 42.0f; // Scout agile frame
            default: return 60.0f;
        }
    }

    bool is_overburdened(int class_id) const {
        return carry_weight() > max_carry_weight(class_id);
    }

    float overburden_penalty(int class_id) const {
        float cw = carry_weight();
        float mw = max_carry_weight(class_id);
        if (cw <= mw) return 1.0f;
        float excess = (cw - mw) / mw;
        return std::max(0.55f, 1.0f - excess * 0.45f);
    }

    void add_voidite(int amount) {
        voidite += amount;
        total_run_score += amount * 5;
    }

    void add_titanium(int amount) {
        titanium += amount;
        total_run_score += amount * 6;
    }

    void add_salvage(int amount) {
        salvage_parts += amount;
        total_run_score += amount * 2;
    }

    void add_relic() {
        relic_extracted = true;
        total_run_score += 250;
    }

    bool add_scrap_from_granite() {
        granite_mined_count++;
        if (granite_mined_count % 3 == 0) {
            scrap_metal++;
            if (scrap_metal >= 2 && bulkheads < get_max_bulkheads()) {
                scrap_metal -= 2;
                bulkheads++;
                return true; // +1 bulkhead fabricated
            }
        }
        return false;
    }

    bool has_bulkhead_material() const {
        return bulkheads > 0;
    }

    bool consume_bulkhead() {
        if (bulkheads > 0) {
            bulkheads--;
            return true;
        }
        return false;
    }

    bool refund_bulkhead() {
        if (bulkheads < get_max_bulkheads()) {
            bulkheads++;
            return true;
        }
        return false;
    }

    bool drop_voidite(int amount = 1) {
        if (voidite >= amount && amount > 0) {
            voidite -= amount;
            total_run_score = std::max(0, total_run_score - amount * 5);
            return true;
        }
        return false;
    }

    bool drop_titanium(int amount = 1) {
        if (titanium >= amount && amount > 0) {
            titanium -= amount;
            total_run_score = std::max(0, total_run_score - amount * 6);
            return true;
        }
        return false;
    }

    bool drop_salvage(int amount = 1) {
        if (salvage_parts >= amount && amount > 0) {
            salvage_parts -= amount;
            total_run_score = std::max(0, total_run_score - amount * 2);
            return true;
        }
        return false;
    }

    bool drop_bulkhead(int amount = 1) {
        if (bulkheads >= amount && amount > 0) {
            bulkheads -= amount;
            return true;
        }
        return false;
    }

    bool drop_demolition_charge(int amount = 1) {
        if (demolition_charges >= amount && amount > 0) {
            demolition_charges -= amount;
            return true;
        }
        return false;
    }

    // Formula: Rate = (Voidite Mined / Target Voidite * 50%) + (Vault Breached * 25%) + (Safe Evac * 25%)
    int calculate_completion_rate(bool safe_evac) const {
        float voidite_ratio = (target_voidite > 0) ? std::min(1.0f, static_cast<float>(voidite) / static_cast<float>(target_voidite)) : 1.0f;
        float rate = (voidite_ratio * 50.0f) + (vault_breached ? 25.0f : 0.0f) + (safe_evac ? 25.0f : 0.0f);
        int final_rate = static_cast<int>(std::round(rate));
        return (final_rate < 0) ? 0 : (final_rate > 100) ? 100 : final_rate;
    }

    std::string evaluate_outcome_badge(bool safe_evac, int completion_rate) const {
        if (is_abandoned) return "ABANDONED (<50%)";
        if (suit_failed) return "M.I.A. (SUIT FAILURE)";
        if (safe_evac && completion_rate >= 100) return "CLEARED (100%)";
        if (safe_evac && completion_rate >= 50) return "PARTIAL (50–99%)";
        if (completion_rate < 50) return "ABANDONED (<50%)";
        return "EXPEDITION FAILED";
    }

    void finalize_run(int sector, bool safe_evac) {
        run_completion_rate = calculate_completion_rate(safe_evac);
        run_outcome_badge = evaluate_outcome_badge(safe_evac, run_completion_rate);

        // Hybrid coin earning: base extraction stipend + completion rate bonus + harvested minerals + relic
        int base_payout = safe_evac ? 60 : 20;
        int completion_bonus = run_completion_rate; // 0 to 100 coins
        int mineral_coins = voidite * 2 + titanium * 3 + salvage_parts * 1;
        int relic_bonus = relic_extracted ? 100 : 0;
        run_coins_earned = base_payout + completion_bonus + mineral_coins + relic_bonus;
        if (is_abandoned) {
            run_coins_earned = std::max(0, run_coins_earned / 2);
        }

        if (sector >= 1 && sector < MAX_SECTOR_RECORDS) {
            if (run_completion_rate > sector_records[sector].highest_completion_rate) {
                sector_records[sector].highest_completion_rate = run_completion_rate;
            }
            if (run_outcome_badge == "CLEARED (100%)" || sector_records[sector].best_badge == "UNEXPLORED") {
                sector_records[sector].best_badge = run_outcome_badge;
            } else if (run_outcome_badge.rfind("PARTIAL", 0) == 0 && sector_records[sector].best_badge != "CLEARED (100%)") {
                sector_records[sector].best_badge = run_outcome_badge;
            } else if (sector_records[sector].best_badge == "UNEXPLORED") {
                sector_records[sector].best_badge = run_outcome_badge;
            }
        }
    }

    void apply_abandon_penalty() {
        is_abandoned = true;
        voidite = std::max(0, voidite / 2);
        titanium = std::max(0, titanium / 2);
        salvage_parts = std::max(0, salvage_parts / 2);
        scrap_metal = std::max(0, scrap_metal / 2);
        run_coins_earned = std::max(0, run_coins_earned / 2);
    }

    void reset(int voidite_goal = 25) {
        voidite = 0;
        titanium = 0;
        salvage_parts = 0;
        demolition_charges = 3;
        bulkheads = 15;
        scrap_metal = 0;
        granite_mined_count = 0;
        total_run_score = 0;
        run_coins_earned = 0;
        vault_breached = false;
        relic_extracted = false;
        is_abandoned = false;
        suit_failed = false;
        has_banked_run = false;
        run_outcome_badge = "UNEXPLORED";
        run_completion_rate = 0;
        target_voidite = voidite_goal;
    }
};

} // namespace Voidfall
