#pragma once
#include <cstdint>
#include <string>

namespace Voidfall {

enum class ToolSlot : uint8_t {
    MiningDrill = 0,
    IndustrialBulkhead = 1,
    DemolitionCharge = 2
};

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

    // Level objective tracking
    bool vault_breached{false};
    bool relic_extracted{false};
    int target_voidite{25};

    // Mission status & completion tracking
    bool is_abandoned{false};
    bool suit_failed{false};
    std::string run_outcome_badge{"UNEXPLORED"};
    int run_completion_rate{0};

    // Stored persistent records for Sector 1, Sector 2, Sector 3 (1-indexed)
    SectorRecord sector_records[4] = {
        {0, "UNEXPLORED"},
        {0, "UNEXPLORED"},
        {0, "UNEXPLORED"},
        {0, "UNEXPLORED"}
    };

    int get_max_bulkheads() const {
        return max_bulkheads + upgrades.max_bulkheads_level * 5;
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

    // Formula: Rate = (Voidite Mined / Target Voidite * 50%) + (Vault Breached * 25%) + (Safe Evac * 25%)
    int calculate_completion_rate(bool safe_evac) const {
        float voidite_ratio = (target_voidite > 0) ? std::min(1.0f, static_cast<float>(voidite) / static_cast<float>(target_voidite)) : 1.0f;
        float rate = (voidite_ratio * 50.0f) + (vault_breached ? 25.0f : 0.0f) + (safe_evac ? 25.0f : 0.0f);
        int final_rate = static_cast<int>(std::round(rate));
        return (final_rate < 0) ? 0 : (final_rate > 100) ? 100 : final_rate;
    }

    std::string evaluate_outcome_badge(bool safe_evac, int completion_rate) const {
        if (is_abandoned) return "EXPEDITION ABANDONED";
        if (suit_failed) return "M.I.A. (SUIT FAILURE)";
        if (safe_evac && completion_rate >= 100) return "SECTOR CLEARED (100%)";
        if (safe_evac && completion_rate >= 50) return "PARTIAL EXTRACTION (" + std::to_string(completion_rate) + "%)";
        return "EXPEDITION FAILED";
    }

    void finalize_run(int sector, bool safe_evac) {
        run_completion_rate = calculate_completion_rate(safe_evac);
        run_outcome_badge = evaluate_outcome_badge(safe_evac, run_completion_rate);

        if (sector >= 1 && sector <= 3) {
            if (run_completion_rate > sector_records[sector].highest_completion_rate) {
                sector_records[sector].highest_completion_rate = run_completion_rate;
            }
            if (run_outcome_badge == "SECTOR CLEARED (100%)" || sector_records[sector].best_badge == "UNEXPLORED") {
                sector_records[sector].best_badge = run_outcome_badge;
            } else if (run_outcome_badge.find("PARTIAL") != std::string::npos && sector_records[sector].best_badge != "SECTOR CLEARED (100%)") {
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
        vault_breached = false;
        relic_extracted = false;
        is_abandoned = false;
        suit_failed = false;
        run_outcome_badge = "UNEXPLORED";
        run_completion_rate = 0;
        target_voidite = voidite_goal;
    }
};

} // namespace Voidfall
