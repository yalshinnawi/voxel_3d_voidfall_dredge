#pragma once
#include <cstdint>
#include <string>

namespace Voidfall {

enum class ToolSlot : uint8_t {
    MiningDrill = 0,
    IndustrialBulkhead = 1,
    DemolitionCharge = 2
};

struct PlayerInventory {
    int voidite{0};
    int titanium{0};
    int salvage_parts{0};
    int demolition_charges{3};
    int bulkheads{15};
    int total_run_score{0};

    // Level objective tracking
    bool vault_breached{false};
    bool relic_extracted{false};
    int target_voidite{25};

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

    bool has_bulkhead_material() const {
        return bulkheads > 0 || titanium >= 1;
    }

    bool consume_bulkhead() {
        if (bulkheads > 0) {
            bulkheads--;
            return true;
        }
        if (titanium >= 1) {
            titanium--;
            return true;
        }
        return false;
    }

    void reset(int voidite_goal = 25) {
        voidite = 0;
        titanium = 0;
        salvage_parts = 0;
        demolition_charges = 3;
        bulkheads = 15;
        total_run_score = 0;
        vault_breached = false;
        relic_extracted = false;
        target_voidite = voidite_goal;
    }
};

} // namespace Voidfall
