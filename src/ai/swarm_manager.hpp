#pragma once

#include <cstdint>
#include <algorithm>
#include <functional>

namespace Voidfall {

/// Subterranean aberrant threat agitation levels
enum class AgitationState : uint8_t {
    CALM,        // 0.0 - 0.4: Ambient patrol, low spawn rate
    STIRRED,     // 0.4 - 0.85: Scouts investigate player noise
    ENRAGED,     // 1.0: Active swarm ambush / lockdown
    COOLDOWN     // Post-assault decaying state
};

/// Manages the aberrant agitation loop state machine, swarm locks,
/// coordinated assault waves, and speed multipliers.
class SwarmManager {
public:
    static constexpr float ENRAGE_DURATION  = 30.0f; // Seconds locked in ENRAGED swarm state
    static constexpr float COOLDOWN_DURATION = 15.0f; // Seconds to decay agitation back to 0.0

    static SwarmManager& instance();

    SwarmManager();

    /// Step agitation timer, handle enrage lock, and post-assault cooldown decay
    void Update(float dt);
    void update(float dt) { Update(dt); }

    /// Add agitation impulse [0.0 - 1.0 scale]
    void AddAgitation(float amount);
    void add_agitation(float amount) { AddAgitation(amount); }

    /// Set agitation directly [0.0 - 1.0]
    void SetAgitation(float val);
    void set_agitation(float val) { SetAgitation(val); }

    float GetAgitation() const { return m_agitation; }
    float agitation() const { return m_agitation; }

    AgitationState GetState() const { return m_state; }
    AgitationState state() const { return m_state; }

    float GetEnrageTimer() const { return m_enrageTimer; }
    float enrage_timer() const { return m_enrageTimer; }

    bool IsEnraged() const { return m_state == AgitationState::ENRAGED; }
    bool is_enraged() const { return IsEnraged(); }

    /// Enemy movement speed multiplier (boosted 25% during ENRAGED swarm lockdown)
    float GetSpeedMultiplier() const { return IsEnraged() ? 1.25f : 1.0f; }

    int active_wave_hostiles() const { return m_active_wave_hostiles; }
    void set_active_wave_hostiles(int count) { m_active_wave_hostiles = count; }

    /// Force end enrage immediately (e.g. after defeating swarm hostiles)
    void EndEnrage();
    void end_enrage() { EndEnrage(); }

    /// Notify that an active swarm hostile has been defeated
    void on_hostile_killed();

    using AlertCallback = std::function<void()>;
    using WaveCallback = std::function<void()>;
    using EnrageEndedCallback = std::function<void()>;

    void SetOnEnrageAlert(AlertCallback cb) { m_on_enrage_alert = std::move(cb); }
    void SetOnSpawnWave(WaveCallback cb) { m_on_spawn_wave = std::move(cb); }
    void SetOnEnrageEnded(EnrageEndedCallback cb) { m_on_enrage_ended = std::move(cb); }

    void reset();
    void Reset() { reset(); }

public:
    float m_agitation{0.0f};
    AgitationState m_state{AgitationState::CALM};
    float m_enrageTimer{0.0f};
    float m_maxAgitation{1.0f};
    int m_active_wave_hostiles{0};

    AlertCallback m_on_enrage_alert;
    WaveCallback m_on_spawn_wave;
    EnrageEndedCallback m_on_enrage_ended;
};

} // namespace Voidfall
