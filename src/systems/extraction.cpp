#include "extraction.hpp"
#include <cmath>
#include <iostream>

namespace Voidfall {

ExtractionSystem::ExtractionSystem() {
}

void ExtractionSystem::deploy_beacon(const glm::vec3& beacon_pos) {
    if (m_phase == ExtractionPhase::Dormant) {
        m_phase = ExtractionPhase::BeaconDeployed;
        m_beacon_pos = beacon_pos;
        m_countdown = 40.0f;
        std::cout << "[Extraction] Beacon deployed at (" << beacon_pos.x << ", "
                  << beacon_pos.y << ", " << beacon_pos.z << ")! 40-Second evacuation defense initiated!" << std::endl;
    }
}

void ExtractionSystem::update(float dt, const glm::vec3& player_pos) {
    if (m_phase == ExtractionPhase::BeaconDeployed) {
        m_countdown -= dt;

        // Pulsating red siren flare
        m_siren_pulse = std::sin(m_countdown * 8.0f) * 0.5f + 0.5f;

        if (m_countdown <= 0.0f) {
            m_phase = ExtractionPhase::PodLanded;
            std::cout << "[Extraction] Evac Pod has touched down! Board immediately within 3 meters!" << std::endl;
        }
    } else if (m_phase == ExtractionPhase::PodLanded) {
        float dist_to_pod = glm::distance(player_pos, m_beacon_pos);
        if (dist_to_pod < 3.0f) {
            m_phase = ExtractionPhase::Complete;
            std::cout << "[Extraction] Squad extraction successful! Returning to Orbital Hub." << std::endl;
            if (m_on_complete) {
                m_on_complete();
            }
        }
    }
}

} // namespace Voidfall
