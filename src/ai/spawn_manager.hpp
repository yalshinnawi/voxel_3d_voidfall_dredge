#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <functional>
#include "../entities/enemies/void_stalker.hpp"

namespace Voidfall {

class World;
class HazardClock;

/// Structure tracking a telegraphed enemy spawn emergence
struct TelegraphedSpawn {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    StalkerRole role{StalkerRole::Melee};
    float timer{2.0f};          // 2.0s emergence warning countdown
    float duration{2.0f};
    bool audio_cued{false};     // Sound played at beginning of telegraph
};

/// Manages early encounter pacing, initial level grace period, insertion pod exclusion zone,
/// progressive threat ramping, and pre-spawn environmental telegraphs (dust particles & audio).
class SpawnManager {
public:
    static constexpr float LEVEL_1_GRACE_PERIOD  = 45.0f; // Seconds before combat spawns trigger in Sector 1
    static constexpr float LEVEL_2_GRACE_PERIOD  = 35.0f; // Seconds before combat spawns trigger in Sector 2
    static constexpr float LEVEL_3_GRACE_PERIOD  = 25.0f; // Seconds before combat spawns trigger in Sector 3
    static constexpr float SPAWN_SAFE_RADIUS     = 28.0f; // Minimum meters from insertion point
    static constexpr float SAFE_EXCLUSION_RADIUS = 28.0f; // Minimum Euclidean meters from insertion pod
    static constexpr float EARLY_RAMP_DURATION   = 120.0f; // Minutes 0-2 threat cap window

    static bool IsSpawnPointSafe(const glm::vec3& candidatePos, const glm::vec3& playerSpawnPos) {
        return glm::distance(candidatePos, playerSpawnPos) >= SPAWN_SAFE_RADIUS;
    }

    /// Enforces insertion quarantine sphere when picking runtime spawn points
    glm::vec3 FindSpawnPoint(const glm::vec3& near_pos, const World& world, const glm::vec3& player_spawn_pos) const;
    glm::vec3 FindSpawnPoint(const glm::vec3& candidatePos, const glm::vec3& playerSpawnPos) const {
        return FindSpawnPoint(candidatePos, *(const World*)nullptr, playerSpawnPos);
    }

    SpawnManager();

    /// Reset state for a new expedition run
    void reset(int selected_level = 1, const glm::vec3& insertion_pod_pos = glm::vec3(36.0f, 10.0f, 36.0f));

    /// Step pacing timers, check grace period, and update telegraph emergence countdowns
    void update(float dt, World& world, VoidStalkerManager& stalker_mgr, const glm::vec3& player_pos);

    /// Mining hook: Mining high-tier crystal or titanium immediately breaches the grace period
    void on_block_broken(uint8_t material_id);

    /// Determines if a combat spawn is permitted based on grace period, exclusion radius, and threat caps
    bool can_spawn(int current_active_enemies, const glm::vec3& candidate_pos) const;

    /// Request a telegraphed spawn (2.0s warning before emerging)
    bool request_spawn(const glm::vec3& pos, const glm::vec3& normal, StalkerRole role = StalkerRole::Melee);

    /// Immediate spawn bypass (for tests or scripted events)
    bool try_immediate_spawn(VoidStalkerManager& stalker_mgr, const glm::vec3& pos, StalkerRole role = StalkerRole::Melee);

    // Callbacks for visual and audio telegraphs
    using DustCallback = std::function<void(const glm::vec3& pos, const glm::vec3& normal)>;
    using AudioCallback = std::function<void(const glm::vec3& pos)>;
    void set_on_dust_burst(DustCallback cb) { m_on_dust_burst = std::move(cb); }
    void set_on_audio_cue(AudioCallback cb) { m_on_audio_cue = std::move(cb); }

    // State queries
    bool is_in_grace_period() const { return m_grace_timer > 0.0f && !m_high_tier_ore_breached; }
    float grace_timer() const { return m_grace_timer; }
    float elapsed_time() const { return m_elapsed_time; }
    int max_allowed_enemies() const;
    int level() const { return m_level; }
    const glm::vec3& insertion_pod_pos() const { return m_insertion_pod_pos; }
    const std::vector<TelegraphedSpawn>& pending_spawns() const { return m_pending_spawns; }

private:
    int m_level{1};
    float m_grace_timer{LEVEL_1_GRACE_PERIOD};
    float m_elapsed_time{0.0f};
    bool m_high_tier_ore_breached{false};
    glm::vec3 m_insertion_pod_pos{36.0f, 10.0f, 36.0f};

    std::vector<TelegraphedSpawn> m_pending_spawns;

    DustCallback m_on_dust_burst;
    AudioCallback m_on_audio_cue;
};

} // namespace Voidfall
