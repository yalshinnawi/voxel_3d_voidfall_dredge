#pragma once

#include <algorithm>

namespace Voidfall {

/// Subterranean acoustic stealth and noise tracking system.
/// Monitors delver noise generation, crouch stance suppression,
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

    /// Step acoustic dissipation
    void Update(float dt, bool isCrouched = false) {
        float decayRate = isCrouched ? 8.0f : 4.0f;
        m_noise = std::max(0.0f, m_noise - decayRate * dt);
    }
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
    static constexpr float NOISE_SATCHEL_BLAST_RADIUS = 64.0f;

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
    }

private:
    float m_noise{0.0f};
};

} // namespace Voidfall
