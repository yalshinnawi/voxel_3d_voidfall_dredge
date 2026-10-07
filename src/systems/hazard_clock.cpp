#include "hazard_clock.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Voidfall {

HazardClock::HazardClock() {
}

void HazardClock::set_sector_parameters(int sector) {
    if (sector == 1) {
        m_tremor_interval = 135.0f; // Calmer early cavern: tremors do not happen too soon
        m_radiation_rate = 0.20f;
        m_geological_creep = 0.08f;
        m_stress_decay_rate = 2.4f;
    } else if (sector == 3) {
        m_tremor_interval = 55.0f;
        m_radiation_rate = 0.60f;
        m_geological_creep = 0.35f;
        m_stress_decay_rate = 1.4f;
    } else {
        // Sector 2 (Volatile Fault): Moderate escalation
        m_tremor_interval = 105.0f;
        m_radiation_rate = 0.25f;
        m_geological_creep = 0.12f;
        m_stress_decay_rate = 2.1f;
    }
    m_tremor_timer = m_tremor_interval;
}

void HazardClock::reset() {
    m_radiation_level = 0.0f;
    m_tremor_timer = m_tremor_interval;
    m_zones.clear();
    m_player_pos = glm::vec3(0.0f);
    m_rocks_mined = 0;
    m_bullet_hits = 0;
    m_warning_phase = false;
    m_warning_timer = 0.0f;
    m_is_tremoring = false;
    m_tremor_elapsed = 0.0f;
    m_tremor_intensity = 0.0f;
    m_active_epicenter = glm::vec3(0.0f);
    m_active_radius = TREMOR_RADIUS;
}

SeismicZone& HazardClock::get_or_create_zone(const glm::vec3& pos) {
    int cx = static_cast<int>(std::floor(pos.x / ZONE_SIZE));
    int cz = static_cast<int>(std::floor(pos.z / ZONE_SIZE));
    uint64_t key = compute_zone_key(cx, cz);
    auto it = m_zones.find(key);
    if (it != m_zones.end()) {
        return it->second;
    }
    SeismicZone new_zone;
    new_zone.cell_x = cx;
    new_zone.cell_z = cz;
    new_zone.epicenter = pos;
    new_zone.stress = 0.0f;
    new_zone.rocks_mined = 0;
    new_zone.bullet_hits = 0;
    new_zone.damage_weight = 0.0f;
    auto [inserted_it, _] = m_zones.emplace(key, new_zone);
    return inserted_it->second;
}

float HazardClock::seismic_stress_at(const glm::vec3& pos) const {
    int cx = static_cast<int>(std::floor(pos.x / ZONE_SIZE));
    int cz = static_cast<int>(std::floor(pos.z / ZONE_SIZE));
    uint64_t key = compute_zone_key(cx, cz);
    auto it = m_zones.find(key);
    if (it != m_zones.end()) {
        return it->second.stress;
    }
    return 0.0f;
}

float HazardClock::highest_seismic_stress() const {
    float max_s = 0.0f;
    for (const auto& [key, zone] : m_zones) {
        if (zone.stress > max_s) {
            max_s = zone.stress;
        }
    }
    return max_s;
}

const SeismicZone* HazardClock::highest_stress_zone() const {
    const SeismicZone* best = nullptr;
    float max_s = -1.0f;
    for (const auto& [key, zone] : m_zones) {
        if (zone.stress > max_s) {
            max_s = zone.stress;
            best = &zone;
        }
    }
    return best;
}

float HazardClock::seismic_stress() const {
    if (m_zones.empty()) {
        return 0.0f;
    }
    if (m_zones.size() == 1) {
        return m_zones.begin()->second.stress;
    }

    float local_stress = seismic_stress_at(m_player_pos);
    if (local_stress > 0.0f) {
        return local_stress;
    }

    // Proximity falloff if the player is within range of an excavation zone
    for (const auto& [key, zone] : m_zones) {
        if (zone.stress > 0.0f) {
            float dist = glm::distance(m_player_pos, zone.epicenter);
            if (dist < ZONE_SIZE) {
                return zone.stress * (1.0f - dist / ZONE_SIZE);
            }
        }
    }
    return 0.0f;
}

float HazardClock::active_tremor_proximity(const glm::vec3& pos) const {
    if (!m_is_tremoring && !m_warning_phase) return 0.0f;
    float dist = glm::distance(pos, m_active_epicenter);
    if (dist >= m_active_radius || m_active_radius <= 0.001f) return 0.0f;
    return std::clamp(1.0f - (dist / m_active_radius), 0.0f, 1.0f);
}

void HazardClock::force_tremor(float intensity, const std::optional<glm::vec3>& pos) {
    m_warning_phase = false;
    m_warning_timer = 0.0f;
    m_is_tremoring = true;
    m_tremor_elapsed = 0.0f;
    m_tremor_intensity = intensity;
    m_tremor_timer = 0.0f;

    if (pos.has_value()) {
        m_active_epicenter = *pos;
    } else {
        const SeismicZone* highest = highest_stress_zone();
        m_active_epicenter = highest ? highest->epicenter : m_player_pos;
    }
    m_active_radius = TREMOR_RADIUS;

    if (m_on_spatial_tremor) {
        m_on_spatial_tremor(m_tremor_intensity, m_active_epicenter, m_active_radius);
    }
    if (m_on_legacy_tremor) {
        m_on_legacy_tremor(m_tremor_intensity);
    }
}

void HazardClock::trigger_tremor_sequence_at(const glm::vec3& epicenter, float intensity) {
    if (m_warning_phase || m_is_tremoring || m_tremor_cooldown_timer > 0.0f) return;

    m_warning_phase = true;
    m_warning_timer = m_warning_duration;
    m_active_epicenter = epicenter;
    m_active_radius = TREMOR_RADIUS;
    m_tremor_intensity = intensity;

    if (m_on_spatial_warning) {
        m_on_spatial_warning(m_active_epicenter, m_active_radius);
    }
    if (m_on_legacy_warning) {
        m_on_legacy_warning();
    }
}

void HazardClock::trigger_tremor_sequence() {
    const SeismicZone* highest = highest_stress_zone();
    glm::vec3 epicenter = highest ? highest->epicenter : m_player_pos;
    trigger_tremor_sequence_at(epicenter);
}

void HazardClock::add_seismic_stress(const glm::vec3& pos, float amount) {
    SeismicZone& zone = get_or_create_zone(pos);
    zone.damage_weight += amount;
    if (zone.stress <= 0.01f) {
        zone.epicenter = pos;
    } else {
        zone.epicenter = (zone.epicenter * zone.stress + pos * amount) / (zone.stress + amount);
    }
    zone.stress = std::clamp(zone.stress + amount, 0.0f, 100.0f);

    if (!m_warning_phase && !m_is_tremoring && m_tremor_cooldown_timer <= 0.0f && zone.stress >= 100.0f) {
        trigger_tremor_sequence_at(zone.epicenter);
    }
}

void HazardClock::add_seismic_stress(float amount) {
    add_seismic_stress(m_player_pos, amount);
}

void HazardClock::on_rock_mined(const glm::vec3& pos, uint8_t material_id) {
    float gain = 1.6f;
    if (material_id == 6) { // MAT_VOIDITE_CRYSTAL
        gain = 1.8f;
    } else if (material_id == 3 || material_id == 7) { // MAT_TITANIUM, MAT_RADIOACTIVE_ORE
        gain = 2.6f;
    } else if (material_id == 9) { // MAT_REINFORCED_VAULT_DOOR
        gain = 6.5f;
    }

    SeismicZone& zone = get_or_create_zone(pos);
    zone.damage_weight += gain;
    if (zone.stress <= 0.01f) {
        zone.epicenter = pos;
    } else {
        zone.epicenter = (zone.epicenter * zone.stress + pos * gain) / (zone.stress + gain);
    }
    zone.stress = std::clamp(zone.stress + gain, 0.0f, 100.0f);
    zone.rocks_mined++;
    m_rocks_mined++;

    if (!m_warning_phase && !m_is_tremoring && m_tremor_cooldown_timer <= 0.0f) {
        if (zone.stress >= 65.0f) {
            float roll = static_cast<float>(rand() % 1000) / 10.0f; // 0..100%
            float trigger_chance = (zone.stress - 65.0f) * 0.40f;
            if (roll < trigger_chance || zone.stress >= 100.0f) {
                trigger_tremor_sequence_at(zone.epicenter);
            }
        }
    }
}

void HazardClock::on_rock_mined(uint8_t material_id) {
    on_rock_mined(m_player_pos, material_id);
}

void HazardClock::on_weapon_impact(const glm::vec3& pos, float stress_amount) {
    SeismicZone& zone = get_or_create_zone(pos);
    zone.damage_weight += stress_amount;
    if (zone.stress <= 0.01f) {
        zone.epicenter = pos;
    } else {
        zone.epicenter = (zone.epicenter * zone.stress + pos * stress_amount) / (zone.stress + stress_amount);
    }
    zone.stress = std::clamp(zone.stress + stress_amount, 0.0f, 100.0f);
    zone.bullet_hits++;
    m_bullet_hits++;

    if (!m_warning_phase && !m_is_tremoring && m_tremor_cooldown_timer <= 0.0f) {
        if (zone.stress >= 50.0f) {
            float roll = static_cast<float>(rand() % 1000) / 10.0f;
            float trigger_chance = (zone.stress - 50.0f) * 0.55f;
            if (roll < trigger_chance || zone.stress >= 100.0f) {
                trigger_tremor_sequence_at(zone.epicenter);
            }
        }
    }
}

void HazardClock::on_weapon_impact(float stress_amount) {
    on_weapon_impact(m_player_pos, stress_amount);
}

void HazardClock::on_explosive_detonation(const glm::vec3& pos, float stress_amount) {
    SeismicZone& zone = get_or_create_zone(pos);
    zone.damage_weight += stress_amount;
    if (zone.stress <= 0.01f) {
        zone.epicenter = pos;
    } else {
        zone.epicenter = (zone.epicenter * zone.stress + pos * stress_amount) / (zone.stress + stress_amount);
    }
    zone.stress = std::clamp(zone.stress + stress_amount, 0.0f, 100.0f);

    if (!m_warning_phase && !m_is_tremoring && m_tremor_cooldown_timer <= 0.0f) {
        if (zone.stress >= 50.0f) {
            float roll = static_cast<float>(rand() % 1000) / 10.0f;
            float trigger_chance = (zone.stress - 45.0f) * 0.65f;
            if (roll < trigger_chance || zone.stress >= 100.0f) {
                trigger_tremor_sequence_at(zone.epicenter);
            }
        }
    }
}

void HazardClock::on_explosive_detonation(float stress_amount) {
    on_explosive_detonation(m_player_pos, stress_amount);
}

void HazardClock::finish_tremor() {
    m_is_tremoring = false;
    m_tremor_timer = m_tremor_interval;
    m_tremor_intensity = 0.0f;
    m_rocks_mined = 0;
    m_bullet_hits = 0;
    m_tremor_cooldown_timer = 40.0f; // 40-second refractory window to prevent repetitive tremors

    // Relieve tectonic strain in epicenter zone and damp neighboring boundary zones
    int cx = static_cast<int>(std::floor(m_active_epicenter.x / ZONE_SIZE));
    int cz = static_cast<int>(std::floor(m_active_epicenter.z / ZONE_SIZE));
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            uint64_t key = compute_zone_key(cx + dx, cz + dz);
            auto it = m_zones.find(key);
            if (it != m_zones.end()) {
                if (dx == 0 && dz == 0) {
                    it->second.stress = 0.0f;
                    it->second.rocks_mined = 0;
                    it->second.bullet_hits = 0;
                    it->second.damage_weight = 0.0f;
                } else {
                    it->second.stress *= 0.40f;
                }
            }
        }
    }
}

void HazardClock::update(float dt) {
    m_tremor_cooldown_timer = std::max(0.0f, m_tremor_cooldown_timer - dt);

    // 1. Escalate radiation over time
    m_radiation_level = std::min(100.0f, m_radiation_level + m_radiation_rate * dt);

    // 2. Pre-tremor warning phase (acoustic spall before ceiling collapse in affected zone)
    if (m_warning_phase) {
        m_warning_timer -= dt;
        if (m_warning_timer <= 0.0f) {
            float leftover_dt = -m_warning_timer;
            m_warning_phase = false;
            m_is_tremoring = true;
            m_tremor_elapsed = leftover_dt;

            float zone_stress = seismic_stress_at(m_active_epicenter);
            m_tremor_intensity = 0.85f + (zone_stress / 100.0f) * 0.85f;

            if (m_on_spatial_tremor) {
                m_on_spatial_tremor(m_tremor_intensity, m_active_epicenter, m_active_radius);
            }
            if (m_on_legacy_tremor) {
                m_on_legacy_tremor(m_tremor_intensity);
            }

            if (m_tremor_elapsed >= m_tremor_duration) {
                finish_tremor();
            }
        }
        return;
    }

    // 3. Active Tremor Phase
    if (m_is_tremoring) {
        m_tremor_elapsed += dt;
        if (m_tremor_elapsed >= m_tremor_duration) {
            finish_tremor();
        }
        return;
    }

    // 4. Natural Dissipation and Baseline Geological Creep per zone
    float net_change = (m_geological_creep - m_stress_decay_rate) * dt;
    uint64_t critical_zone_key = 0;
    bool found_critical = false;

    for (auto& [key, zone] : m_zones) {
        zone.stress = std::clamp(zone.stress + net_change, 0.0f, 100.0f);
        if (!found_critical && zone.stress >= 100.0f) {
            critical_zone_key = key;
            found_critical = true;
        }
    }

    if (found_critical && m_tremor_cooldown_timer <= 0.0f) {
        auto it = m_zones.find(critical_zone_key);
        if (it != m_zones.end()) {
            trigger_tremor_sequence_at(it->second.epicenter);
        }
    }

    // Keep estimated tremor timer synchronized for HUD telemetry based on highest zone stress
    float max_stress = highest_seismic_stress();
    m_tremor_timer = std::max(0.0f, (100.0f - max_stress) * 0.50f);
}

} // namespace Voidfall
