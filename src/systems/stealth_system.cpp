#include "stealth_system.hpp"

namespace Voidfall {

StealthSystem& StealthSystem::instance() {
    static StealthSystem s_instance;
    return s_instance;
}

void StealthSystem::Update(float dt, bool isCrouched) {
    float decayRate = isCrouched ? 8.0f : 4.0f;
    m_noise = std::max(0.0f, m_noise - decayRate * dt);

    for (auto& evt : m_acoustic_events) {
        evt.age += dt;
    }
    m_acoustic_events.erase(
        std::remove_if(m_acoustic_events.begin(), m_acoustic_events.end(),
                       [](const AcousticNoiseEvent& e) { return e.is_expired(); }),
        m_acoustic_events.end()
    );
}

void StealthSystem::EmitAcousticEvent(const glm::vec3& pos, float db, float radius, float lifetime) {
    m_acoustic_events.push_back({pos, db, radius, 0.0f, lifetime});
}

void StealthSystem::EmitCrouch(const glm::vec3& pos) {
    EmitAcousticEvent(pos, NOISE_CROUCH_WALK_DB, NOISE_CROUCH_WALK_RADIUS, 0.35f);
}

void StealthSystem::EmitWalk(const glm::vec3& pos) {
    EmitAcousticEvent(pos, NOISE_NORMAL_WALK_DB, NOISE_NORMAL_WALK_RADIUS, 0.35f);
}

void StealthSystem::EmitDrill(const glm::vec3& pos) {
    EmitAcousticEvent(pos, NOISE_MINING_DRILL_DB, NOISE_MINING_DRILL_RADIUS, 0.45f);
}

void StealthSystem::EmitSatchelDetonation(const glm::vec3& pos) {
    EmitAcousticEvent(pos, NOISE_SATCHEL_BLAST_DB, NOISE_SATCHEL_BLAST_RADIUS, 1.20f);
}

} // namespace Voidfall
