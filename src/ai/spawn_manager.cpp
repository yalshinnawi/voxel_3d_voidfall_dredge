#include "spawn_manager.hpp"
#include "../voxel/world.hpp"
#include "../voxel/packed_vertex.hpp"
#include "../core/logger.hpp"
#include <algorithm>
#include <cmath>

namespace Voidfall {

SpawnManager::SpawnManager() {
    reset(1);
}

void SpawnManager::reset(int selected_level, const glm::vec3& insertion_pod_pos) {
    m_level = selected_level;
    m_grace_timer = (m_level == 1) ? LEVEL_1_GRACE_PERIOD : 0.0f;
    m_elapsed_time = 0.0f;
    m_high_tier_ore_breached = false;
    m_insertion_pod_pos = insertion_pod_pos;
    m_pending_spawns.clear();
}

void SpawnManager::on_block_broken(uint8_t material_id) {
    if (material_id == MAT_VOIDITE_CRYSTAL || material_id == MAT_TITANIUM || material_id == MAT_RADIOACTIVE_ORE) {
        if (!m_high_tier_ore_breached && is_in_grace_period()) {
            m_high_tier_ore_breached = true;
            m_grace_timer = 0.0f;
            VF_LOG_INFO("SpawnManager", "High-tier ore extracted! Grace period breached early.");
        }
    }
}

int SpawnManager::max_allowed_enemies() const {
    if (is_in_grace_period()) {
        return 0;
    }
    if (m_elapsed_time < EARLY_RAMP_DURATION) {
        // Minutes 0-2: Max 1-2 scout enemies active
        return (m_level >= 2) ? 2 : 2;
    }
    // Mid/Late Phase: Scaled dynamic encounter capacity
    return 4 + (m_level >= 2 ? 2 : 0) + (m_level >= 3 ? 2 : 0);
}

bool SpawnManager::can_spawn(int current_active_enemies, const glm::vec3& candidate_pos) const {
    if (is_in_grace_period()) {
        return false;
    }

    // Enforce Safe Spawn Exclusion Radius: No enemies within 28m of insertion pod
    if (!IsSpawnPointSafe(candidate_pos, m_insertion_pod_pos)) {
        return false;
    }

    if (current_active_enemies >= max_allowed_enemies()) {
        return false;
    }

    return true;
}

glm::vec3 SpawnManager::FindSpawnPoint(const glm::vec3& near_pos, const World& /*world*/, const glm::vec3& player_spawn_pos) const {
    if (IsSpawnPointSafe(near_pos, player_spawn_pos)) {
        return near_pos;
    }
    glm::vec3 dir = near_pos - player_spawn_pos;
    if (glm::length(dir) < 0.001f) {
        dir = glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        dir = glm::normalize(dir);
    }
    return player_spawn_pos + dir * (SPAWN_SAFE_RADIUS + 4.0f);
}

bool SpawnManager::request_spawn(const glm::vec3& pos, const glm::vec3& normal, StalkerRole role) {
    TelegraphedSpawn s;
    s.position = pos;
    s.normal = normal;
    s.role = role;
    s.timer = 2.0f;
    s.duration = 2.0f;
    s.audio_cued = false;
    m_pending_spawns.push_back(s);
    return true;
}

bool SpawnManager::try_immediate_spawn(VoidStalkerManager& stalker_mgr, const glm::vec3& pos, StalkerRole role) {
    if (!can_spawn(stalker_mgr.active_count(), pos)) {
        return false;
    }
    stalker_mgr.spawn_stalker(pos, 1.0f, role);
    return true;
}

void SpawnManager::update(float dt, World& world, VoidStalkerManager& stalker_mgr, const glm::vec3& player_pos) {
    m_elapsed_time += dt;

    if (m_grace_timer > 0.0f) {
        m_grace_timer = std::max(0.0f, m_grace_timer - dt);
    }

    // Process telegraphed emergence spawns
    for (auto it = m_pending_spawns.begin(); it != m_pending_spawns.end();) {
        it->timer -= dt;

        // Trigger 2.0s audio cue on start
        if (!it->audio_cued) {
            it->audio_cued = true;
            if (m_on_audio_cue) {
                m_on_audio_cue(it->position);
            }
        }

        // Particle dust burst emission at the emergence voxel
        if (m_on_dust_burst) {
            m_on_dust_burst(it->position, it->normal);
        }

        if (it->timer <= 0.0f) {
            // Emerge entity into the world
            if (can_spawn(stalker_mgr.active_count(), it->position)) {
                stalker_mgr.spawn_stalker(it->position, 1.0f, it->role);
            }
            it = m_pending_spawns.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Voidfall
