#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include "voxel_types.hpp"

namespace Voidfall {

enum MaterialGrade : uint8_t {
    GRADE_COMMON = 1,   // Titanium Bulkhead Alloy, Flora
    GRADE_UNCOMMON = 2, // Voidite Crystal
    GRADE_RARE = 3,     // Radioactive Ore
    GRADE_EXOTIC = 4,   // Prismatic Crystal
    GRADE_MYTHIC = 5    // Astral Ore (Exotic deep-space mineral in later levels)
};

struct MaterialScarcityConfig {
    uint8_t material_id;
    uint8_t grade;                     // 1..5
    std::string name;
    int unlock_sector;                 // Sector where this material first begins to spawn
    float base_scarcity;               // Scarcity rate S in [0.0, 1.0] at unlock_sector (1.0 = rarest, 0.0 = ubiquitous)
    float scarcity_decay_per_sector;   // Scarcity decay per subsequent sector (becomes less scarce)
    float min_scarcity;                // Floor for scarcity in endless sectors
};

class MaterialScarcitySystem {
public:
    static const MaterialScarcitySystem& instance() {
        static MaterialScarcitySystem s_instance;
        return s_instance;
    }

    MaterialScarcitySystem() {
        initialize_default_registry();
    }

    void initialize_default_registry() {
        m_configs = {
            // Grade 1: Common Industrial Alloy
            {MAT_TITANIUM, GRADE_COMMON, "Titanium Core", 1, 0.30f, 0.04f, 0.02f},
            // Grade 1: Organic Bio-sample
            {MAT_BIOLUMINESCENT_FLORA, GRADE_COMMON, "Bioluminescent Flora", 1, 0.45f, 0.04f, 0.05f},
            // Grade 2: Refined Voidite Crystal
            {MAT_VOIDITE_CRYSTAL, GRADE_UNCOMMON, "Voidite Crystal", 1, 0.60f, 0.04f, 0.10f},
            // Grade 3: Hazardous Radioactive Ore
            {MAT_RADIOACTIVE_ORE, GRADE_RARE, "Radioactive Ore", 2, 0.75f, 0.04f, 0.20f},
            // Grade 4: Prismatic Gemstone
            {MAT_PRISMATIC_CRYSTAL, GRADE_EXOTIC, "Prismatic Crystal", 2, 0.85f, 0.03f, 0.35f},
            // Grade 5: Astral Ore (Exotic deep-space mineral unlocked in later sectors)
            {MAT_ASTRAL_ORE, GRADE_MYTHIC, "Astral Ore", 3, 0.92f, 0.02f, 0.50f}
        };
    }

    const std::vector<MaterialScarcityConfig>& configs() const {
        return m_configs;
    }

    // Scarcity rate in [0.0, 1.0]:
    // 1.0 = completely absent / maximum scarcity
    // 0.0 = completely ubiquitous
    float get_scarcity(uint8_t material_id, int sector) const {
        for (const auto& cfg : m_configs) {
            if (cfg.material_id == material_id) {
                if (sector < cfg.unlock_sector) {
                    return 1.0f; // Completely locked / maximum scarcity
                }
                int extra_sectors = sector - cfg.unlock_sector;
                float s = cfg.base_scarcity - cfg.scarcity_decay_per_sector * static_cast<float>(extra_sectors);
                return std::clamp(s, cfg.min_scarcity, 1.0f);
            }
        }
        return 1.0f;
    }

    // Abundance spawn weight: inversely proportional to scarcity
    float get_spawn_weight(uint8_t material_id, int sector) const {
        float scarcity = get_scarcity(material_id, sector);
        if (scarcity >= 1.0f) {
            return 0.0f;
        }
        return std::max(0.0f, 1.0f - scarcity);
    }

    // Samples a single ore deterministically for a vein in a given sector.
    // Guaranteed to only return one of the eligible ores unlocked for that sector.
    uint8_t sample_vein_ore(int sector, uint32_t seed_roll) const {
        float total_weight = 0.0f;
        for (const auto& cfg : m_configs) {
            total_weight += get_spawn_weight(cfg.material_id, sector);
        }

        if (total_weight <= 0.0f) {
            return MAT_TITANIUM;
        }

        float pick = static_cast<float>(seed_roll % 10000) / 10000.0f * total_weight;
        float accumulated = 0.0f;

        for (const auto& cfg : m_configs) {
            float w = get_spawn_weight(cfg.material_id, sector);
            if (w <= 0.0f) continue;
            accumulated += w;
            if (pick <= accumulated) {
                return cfg.material_id;
            }
        }

        return MAT_TITANIUM;
    }

private:
    std::vector<MaterialScarcityConfig> m_configs;
};

} // namespace Voidfall
