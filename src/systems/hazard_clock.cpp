#include "hazard_clock.hpp"
#include <algorithm>
#include <cmath>

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
        m_tremor_interval = 90.0f;
        m_radiation_rate = 0.35f;
        m_geological_creep = 0.16f;
        m_stress_decay_rate = 2.0f;
    }
    m_tremor_timer = m_tremor_interval;
}

void HazardClock::reset() {
    m_radiation_level = 0.0f;
    m_tremor_timer = m_tremor_interval;
    m_seismic_stress = 0.0f;
    m_rocks_mined = 0;
    m_bullet_hits = 0;
    m_warning_phase = false;
    m_warning_timer = 0.0f;
    m_is_tremoring = false;
    m_tremor_elapsed = 0.0f;
    m_tremor_intensity = 0.0f;
}

void HazardClock::force_tremor(float intensity) {
    m_warning_phase = false;
    m_warning_timer = 0.0f;
    m_is_tremoring = true;
    m_tremor_elapsed = 0.0f;
    m_tremor_intensity = intensity;
    m_seismic_stress = 0.0f;
    m_tremor_timer = 0.0f;
    if (m_on_tremor) {
        m_on_tremor(m_tremor_intensity);
    }
}

void HazardClock::add_seismic_stress(float amount) {
    m_seismic_stress = std::clamp(m_seismic_stress + amount, 0.0f, 100.0f);
}

void HazardClock::trigger_tremor_sequence() {
    if (m_warning_phase || m_is_tremoring) return;

    m_warning_phase = true;
    m_warning_timer = m_warning_duration;
    if (m_on_tremor_warning) {
        m_on_tremor_warning();
    }
}

void HazardClock::on_rock_mined(uint8_t material_id) {
    float gain = 1.6f;
    // Material hardness calibrates tectonic fracture vibration
    if (material_id == 6) { // MAT_VOIDITE_CRYSTAL
        gain = 1.8f;
    } else if (material_id == 3 || material_id == 7) { // MAT_TITANIUM, MAT_RADIOACTIVE_ORE
        gain = 2.6f;
    } else if (material_id == 9) { // MAT_REINFORCED_VAULT_DOOR
        gain = 6.5f;
    }

    add_seismic_stress(gain);
    m_rocks_mined++;

    if (!m_warning_phase && !m_is_tremoring) {
        // Exceeding 65% stress grants progressive probability of fault slip per rock mined
        if (m_seismic_stress >= 65.0f) {
            float roll = static_cast<float>(rand() % 1000) / 10.0f; // 0..100%
            float trigger_chance = (m_seismic_stress - 65.0f) * 0.40f;
            if (roll < trigger_chance || m_seismic_stress >= 100.0f) {
                trigger_tremor_sequence();
            }
        }
    }
}

void HazardClock::on_weapon_impact(float stress_amount) {
    add_seismic_stress(stress_amount);
    m_bullet_hits++;

    if (!m_warning_phase && !m_is_tremoring) {
        if (m_seismic_stress >= 65.0f) {
            float roll = static_cast<float>(rand() % 1000) / 10.0f;
            float trigger_chance = (m_seismic_stress - 65.0f) * 0.35f;
            if (roll < trigger_chance || m_seismic_stress >= 100.0f) {
                trigger_tremor_sequence();
            }
        }
    }
}

void HazardClock::on_explosive_detonation(float stress_amount) {
    add_seismic_stress(stress_amount);

    if (!m_warning_phase && !m_is_tremoring) {
        if (m_seismic_stress >= 50.0f) {
            float roll = static_cast<float>(rand() % 1000) / 10.0f;
            float trigger_chance = (m_seismic_stress - 45.0f) * 0.65f;
            if (roll < trigger_chance || m_seismic_stress >= 100.0f) {
                trigger_tremor_sequence();
            }
        }
    }
}

void HazardClock::update(float dt) {
    // 1. Escalate radiation over time
    m_radiation_level = std::min(100.0f, m_radiation_level + m_radiation_rate * dt);

    // 2. Pre-tremor warning phase (sirens & acoustic spall before ceiling collapse)
    if (m_warning_phase) {
        m_warning_timer -= dt;
        if (m_warning_timer <= 0.0f) {
            float leftover_dt = -m_warning_timer;
            m_warning_phase = false;
            m_is_tremoring = true;
            m_tremor_elapsed = leftover_dt;
            m_tremor_intensity = 0.85f + (m_seismic_stress / 100.0f) * 0.85f;
            if (m_on_tremor) {
                m_on_tremor(m_tremor_intensity);
            }
            if (m_tremor_elapsed >= m_tremor_duration) {
                m_is_tremoring = false;
                m_seismic_stress = 0.0f; // Complete tectonic pressure relief
                m_rocks_mined = 0;
                m_bullet_hits = 0;
                m_tremor_timer = m_tremor_interval;
                m_tremor_intensity = 0.0f;
            }
        }
        return;
    }

    // 3. Active Tremor Phase
    if (m_is_tremoring) {
        m_tremor_elapsed += dt;
        if (m_tremor_elapsed >= m_tremor_duration) {
            m_is_tremoring = false;
            m_seismic_stress = 0.0f; // Complete tectonic pressure relief
            m_rocks_mined = 0;
            m_bullet_hits = 0;
            m_tremor_timer = m_tremor_interval;
            m_tremor_intensity = 0.0f;
        }
        return;
    }

    // 4. Natural Dissipation and Baseline Geological Creep
    float net_change = (m_geological_creep - m_stress_decay_rate) * dt;
    m_seismic_stress = std::clamp(m_seismic_stress + net_change, 0.0f, 100.0f);

    // Guaranteed trigger at 100% structural strain
    if (m_seismic_stress >= 100.0f) {
        trigger_tremor_sequence();
    }

    // Keep estimated tremor timer synchronized for HUD/telemetry
    m_tremor_timer = std::max(0.0f, (100.0f - m_seismic_stress) * 0.50f);
}

} // namespace Voidfall
