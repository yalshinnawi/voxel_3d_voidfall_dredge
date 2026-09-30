#include "hazard_clock.hpp"
#include <algorithm>
#include <cmath>

namespace Voidfall {

HazardClock::HazardClock() {
}

void HazardClock::set_sector_parameters(int sector) {
    if (sector == 1) {
        m_tremor_interval = 70.0f;
        m_radiation_rate = 0.25f;
    } else if (sector == 3) {
        m_tremor_interval = 22.0f;
        m_radiation_rate = 0.65f;
    } else {
        m_tremor_interval = 45.0f;
        m_radiation_rate = 0.40f;
    }
    m_tremor_timer = m_tremor_interval;
}

void HazardClock::update(float dt) {
    // 1. Escalate radiation over time
    m_radiation_level = std::min(100.0f, m_radiation_level + m_radiation_rate * dt);

    // Higher radiation speeds up tremor intervals
    float interval_scale = 1.0f - (m_radiation_level / 100.0f) * 0.5f;
    float current_interval = m_tremor_interval * interval_scale;

    m_tremor_timer -= dt;

    // Trigger pre-tremor warning 0.75s before ceiling detachment
    if (!m_warning_fired && !m_is_tremoring && m_tremor_timer <= 0.75f) {
        m_warning_fired = true;
        if (m_on_tremor_warning) {
            m_on_tremor_warning();
        }
    }

    if (!m_is_tremoring && m_tremor_timer <= 0.0f) {
        m_is_tremoring = true;
        m_tremor_elapsed = 0.0f;
        m_tremor_intensity = 0.8f + (m_radiation_level / 100.0f) * 1.2f;
        if (m_on_tremor) {
            m_on_tremor(m_tremor_intensity);
        }
    }

    if (m_is_tremoring) {
        m_tremor_elapsed += dt;
        if (m_tremor_elapsed >= m_tremor_duration) {
            m_is_tremoring = false;
            m_warning_fired = false;
            m_tremor_timer = current_interval;
            m_tremor_intensity = 0.0f;
        }
    }
}

} // namespace Voidfall
