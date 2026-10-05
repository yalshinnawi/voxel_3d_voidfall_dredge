#include "swarm_manager.hpp"

namespace Voidfall {

SwarmManager& SwarmManager::instance() {
    static SwarmManager s_instance;
    return s_instance;
}

SwarmManager::SwarmManager()
    : m_agitation(0.0f)
    , m_state(AgitationState::CALM)
    , m_enrageTimer(0.0f)
    , m_maxAgitation(1.0f)
    , m_active_wave_hostiles(0)
{
}

void SwarmManager::AddAgitation(float amount) {
    if (m_state == AgitationState::ENRAGED) {
        // Locked at full while enraged
        m_agitation = 1.0f;
        return;
    }
    if (m_state == AgitationState::COOLDOWN) {
        // During cooldown, allow player to fight/shoot without instantly resetting to ENRAGED.
        // Cap agitation so player has recuperation breathing room.
        m_agitation = std::min(0.60f, m_agitation + amount * 0.4f);
        return;
    }
    SetAgitation(m_agitation + amount);
}

void SwarmManager::SetAgitation(float val) {
    m_agitation = std::clamp(val, 0.0f, m_maxAgitation);
    if (m_agitation >= 1.0f) {
        m_agitation = 1.0f;
        if (m_state != AgitationState::ENRAGED && m_state != AgitationState::COOLDOWN) {
            m_state = AgitationState::ENRAGED;
            m_enrageTimer = ENRAGE_DURATION;
            if (m_on_enrage_alert) m_on_enrage_alert();
            if (m_on_spawn_wave) m_on_spawn_wave();
        }
    } else if (m_state != AgitationState::ENRAGED && m_state != AgitationState::COOLDOWN) {
        if (m_agitation >= 0.40f) {
            m_state = AgitationState::STIRRED;
        } else {
            m_state = AgitationState::CALM;
        }
    }
}

void SwarmManager::EndEnrage() {
    if (m_state == AgitationState::ENRAGED) {
        m_state = AgitationState::COOLDOWN;
        m_enrageTimer = 0.0f;
        m_active_wave_hostiles = 0;
        m_agitation = 0.0f;
        if (m_on_enrage_ended) m_on_enrage_ended();
    }
}

void SwarmManager::on_hostile_killed() {
    if (m_active_wave_hostiles > 0) {
        m_active_wave_hostiles--;
    }
    if (m_active_wave_hostiles <= 0 && m_state == AgitationState::ENRAGED) {
        EndEnrage();
    }
}

void SwarmManager::Update(float dt) {
    if (m_state == AgitationState::ENRAGED) {
        // Meter stays at full until timer expires or all swarm hostiles defeated
        m_agitation = 1.0f;
        if (m_enrageTimer > 0.0f) {
            m_enrageTimer -= dt;
        }
        // After enrage timer expires
        if (m_enrageTimer <= 0.0f) {
            EndEnrage();
        }
    } else if (m_state == AgitationState::COOLDOWN) {
        // Gradually decay agitation back toward 0.0f over 15.0s
        float decayRate = 1.0f / COOLDOWN_DURATION;
        m_agitation = std::max(0.0f, m_agitation - decayRate * dt);
        if (m_agitation <= 0.0f) {
            m_agitation = 0.0f;
            m_state = AgitationState::CALM;
        }
    } else {
        if (m_agitation >= 1.0f) {
            m_agitation = 1.0f;
            m_state = AgitationState::ENRAGED;
            m_enrageTimer = ENRAGE_DURATION;
            if (m_on_enrage_alert) m_on_enrage_alert();
            if (m_on_spawn_wave) m_on_spawn_wave();
        } else if (m_agitation >= 0.40f) {
            m_state = AgitationState::STIRRED;
        } else {
            m_state = AgitationState::CALM;
        }
    }
}

void SwarmManager::reset() {
    m_agitation = 0.0f;
    m_state = AgitationState::CALM;
    m_enrageTimer = 0.0f;
    m_active_wave_hostiles = 0;
}

} // namespace Voidfall
