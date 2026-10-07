#pragma once

#include <algorithm>
#include <vector>
#include <glm/glm.hpp>

namespace Voidfall {

/// 3D Acoustic Noise Event emitted by player or subterranean actions
struct AcousticNoiseEvent {
    glm::vec3 position{0.0f};
    float db{0.0f};             // Sound intensity in decibels [0 - 100 dB]
    float radius{0.0f};         // Maximum audible propagation radius in meters
    float age{0.0f};            // Time elapsed since emission
    float lifetime{0.5f};       // Propagation and linger duration in cavern

    bool is_expired() const { return age >= lifetime; }
};

/// Subterranean acoustic stealth, acoustic profile, and noise tracking system.
/// Monitors delver noise generation, action acoustic emissions, crouch stance suppression,
/// and natural acoustic decay.
class StealthSystem {
public:
    static StealthSystem& instance();

    StealthSystem() = default;

    /// Current accumulated acoustic noise level [0.0 - 100.0]
    float GetCurrentNoise() const { return m_noise; }
    float noise() const { return m_noise; }

    /// Add an acoustic impulse to the stealth meter
    void AddNoise(float amount) {
        if (amount <= 0.0f) return;
        m_noise = std::clamp(m_noise + amount, 0.0f, 100.0f);
    }
    void add_noise(float amount) { AddNoise(amount); }

    void SetNoise(float n) {
        m_noise = std::clamp(n, 0.0f, 100.0f);
    }
    void set_noise(float n) { SetNoise(n); }

    /// Step acoustic dissipation and expire lingering acoustic events
    void Update(float dt, bool isCrouched = false);
    void update(float dt, bool isCrouched = false) { Update(dt, isCrouched); }

    /// Locomotion noise attenuation: 0.4x while crouched, 1.0x while standing
    float GetLocomotionNoiseMultiplier(bool isCrouched) const {
        return isCrouched ? 0.40f : 1.0f;
    }

    /// Stealth sound emission dampening: sounds emitted in stealth stance
    /// have heavily reduced noise impact (-65%) and smaller audible radius (-55%)
    static constexpr float STEALTH_SOUND_INTENSITY_MUL = 0.35f;
    static constexpr float STEALTH_SOUND_RADIUS_MUL    = 0.45f;

    /// Standard Action Acoustic Impulses & Hearing Radii
    static constexpr float NOISE_CROUCH_WALK_DB       = 0.0f;
    static constexpr float NOISE_CROUCH_WALK_RADIUS   = 0.0f;

    static constexpr float NOISE_NORMAL_WALK_DB       = 15.0f;
    static constexpr float NOISE_NORMAL_WALK_RADIUS   = 10.0f;

    static constexpr float NOISE_MINING_DRILL_DB      = 55.0f;
    static constexpr float NOISE_MINING_DRILL_RADIUS  = 28.0f;

    static constexpr float NOISE_WEAPON_FIRING_DB     = 75.0f;
    static constexpr float NOISE_WEAPON_FIRING_RADIUS = 40.0f;

    static constexpr float NOISE_SATCHEL_BLAST_DB     = 100.0f;
    static constexpr float NOISE_SATCHEL_BLAST_RADIUS = 1000.0f; // Entire sector coverage

    /// Acoustic Enemy Detection Thresholds
    static constexpr float THRESHOLD_INVESTIGATE_DB   = 30.0f;
    static constexpr float THRESHOLD_PURSUIT_DB       = 70.0f;

    /// Emit an acoustic event into the subterranean acoustic profile system
    void EmitAcousticEvent(const glm::vec3& pos, float db, float radius, float lifetime = 0.5f);

    /// Action-specific emission methods matching Acoustic Profile specs
    void EmitCrouch(const glm::vec3& pos = glm::vec3(0.0f));
    void EmitWalk(const glm::vec3& pos);
    void EmitDrill(const glm::vec3& pos);
    void EmitSatchelDetonation(const glm::vec3& pos);

    // Snake_case aliases
    void emit_acoustic_event(const glm::vec3& pos, float db, float radius, float lifetime = 0.5f) { EmitAcousticEvent(pos, db, radius, lifetime); }
    void emit_crouch(const glm::vec3& pos = glm::vec3(0.0f)) { EmitCrouch(pos); }
    void emit_walk(const glm::vec3& pos) { EmitWalk(pos); }
    void emit_drill(const glm::vec3& pos) { EmitDrill(pos); }
    void emit_satchel_detonation(const glm::vec3& pos) { EmitSatchelDetonation(pos); }

    const std::vector<AcousticNoiseEvent>& GetAcousticEvents() const { return m_acoustic_events; }
    const std::vector<AcousticNoiseEvent>& recent_acoustic_events() const { return m_acoustic_events; }
    void ClearAcousticEvents() { m_acoustic_events.clear(); }

    /// Sneak attack critical hit multiplier on unalerted enemies (3.0x damage)
    static constexpr float SNEAK_ATTACK_CRIT_MULTIPLIER = 3.0f;

    float GetStealthIntensityMultiplier(bool isCrouched) const {
        return isCrouched ? STEALTH_SOUND_INTENSITY_MUL : 1.0f;
    }

    float GetStealthRadiusMultiplier(bool isCrouched) const {
        return isCrouched ? STEALTH_SOUND_RADIUS_MUL : 1.0f;
    }

    float GetCriticalMultiplier() const {
        return SNEAK_ATTACK_CRIT_MULTIPLIER;
    }

    void reset() {
        m_noise = 0.0f;
        m_acoustic_events.clear();
    }

private:
    float m_noise{0.0f};
    std::vector<AcousticNoiseEvent> m_acoustic_events;
};

using AcousticProfile = StealthSystem;

} // namespace Voidfall
