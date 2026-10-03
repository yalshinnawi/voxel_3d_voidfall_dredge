#pragma once
#include <glm/glm.hpp>
#include <functional>
#include <algorithm>
#include <vector>
#include <string>

namespace Voidfall {

/// Type of acoustic disturbance emitted in the subterranean cavern
enum class SoundEventType : uint8_t {
    FootstepWalk,
    FootstepSprint,
    JumpTakeoff,
    JumpLanding,
    JetpackThruster,
    GrappleAction,
    DrillVibration,
    VoxelFracture,
    Gunshot,
    BulletImpact,
    DemolitionBlast,
    SonarPulse,
    BulkheadClang,
    DynamicDebrisCrash,
    SeismicTremor
};

/// 3D Spatial Sound Event emitted in world space for creature hearing & perception
struct SoundEvent {
    SoundEventType type{SoundEventType::FootstepWalk};
    glm::vec3 position{0.0f};       // Exact 3D origin in cavern
    float intensity{5.0f};          // Noise amount added to silence meter [0..100]
    float audible_radius{10.0f};    // Distance in meters subterranean creatures can hear this sound
    float age{0.0f};                // Seconds elapsed since emission
    float lifetime{0.45f};          // Acoustic echo linger duration

    bool is_expired() const { return age >= lifetime; }
};

/// Mining noise & subterranean acoustic accumulation system. Every footstep,
/// drill hit, demolition charge, gunshot, and sonar pulse emits spatial acoustic
/// waves that propagate through cavern air and rock strata. At threshold levels
/// (40%, 70%, 100%), enemy waves spawn. Standing still or crouching in dark
/// shadows rapidly dissipates noise.
///
/// Design inspiration: DRG swarm tension + Lethal Company & Alien: Isolation acoustic stealth.
class NoiseMeter {
public:
    /// Noise thresholds that trigger enemy wave spawns
    enum class AlertLevel : uint8_t {
        Silent   = 0, // 0–39%:  Subterranean calm, no active hunting
        Alerted  = 1, // 40–69%: Minor Void Stalker acoustic investigation
        Agitated = 2, // 70–99%: Aggressive Stalker pack + Burrower tracking
        Swarming = 3  // 100%:   Full subterranean apex swarm, resets to 60%
    };

    using WaveCallback = std::function<void(AlertLevel level, float noise_pct)>;

    NoiseMeter() = default;

    void update(float dt) {
        // Update lifetime of active acoustic sound events
        for (auto& s : m_recent_sounds) {
            s.age += dt;
        }
        m_recent_sounds.erase(
            std::remove_if(m_recent_sounds.begin(), m_recent_sounds.end(),
                           [](const SoundEvent& s) { return s.is_expired(); }),
            m_recent_sounds.end()
        );

        // Check threshold crossings before decay
        AlertLevel current = get_alert_level();
        if (current != m_last_alert && current > m_last_alert) {
            if (current == AlertLevel::Swarming) {
                // Full swarm: trigger wave then reset to 60% to prevent instant re-trigger
                if (m_on_wave) m_on_wave(AlertLevel::Swarming, noise_percent());
                m_noise = m_max_noise * 0.6f;
                m_swarm_count++;
            } else {
                if (m_on_wave) m_on_wave(current, noise_percent());
            }
        }

        // Natural decay: noise dissipates when quiet / stationary / crouching
        float decay_rate = m_is_crouching ? m_crouch_decay_rate : m_base_decay_rate;
        m_noise = std::max(0.0f, m_noise - decay_rate * dt);

        m_last_alert = get_alert_level();

        // Cooldown tracking for swarm re-triggers
        if (m_swarm_cooldown > 0.0f) {
            m_swarm_cooldown -= dt;
        }
    }

    /// Emit a 3D acoustic disturbance into the cavern
    void emit_sound(SoundEventType type, const glm::vec3& pos, float intensity, float audible_radius, float lifetime = 0.45f) {
        // Accumulate onto the global silence meter
        m_noise = std::min(m_max_noise, m_noise + intensity * m_noise_multiplier);

        // Track spatial sound event for creature acoustic AI
        m_recent_sounds.push_back({type, pos, intensity, audible_radius, 0.0f, lifetime});
    }

    /// Access all recent active sound events
    const std::vector<SoundEvent>& recent_sounds() const { return m_recent_sounds; }

    // ─── Specialized Action Sound Emitters ─────────────────────────────

    /// Running / Walking movement sound (sprinting = loud, walking = subtle, crouching = silent)
    void add_movement_sound(const glm::vec3& pos, float dt, bool is_sprinting, bool is_crouching, bool is_grounded) {
        if (!is_grounded || is_crouching) return;
        if (is_sprinting) {
            // Sprinting footsteps: continuous noise + 14m audible radius
            emit_sound(SoundEventType::FootstepSprint, pos, 12.0f * dt, 14.0f, 0.35f);
        } else {
            // Normal walking: low noise + 6m audible radius
            emit_sound(SoundEventType::FootstepWalk, pos, 3.5f * dt, 6.0f, 0.25f);
        }
    }

    /// Jump takeoff acoustic burst
    void add_jump_sound(const glm::vec3& pos) {
        emit_sound(SoundEventType::JumpTakeoff, pos, 6.5f, 10.0f, 0.4f);
    }

    /// Landing impact on rock floor (proportional to fall impact velocity)
    void add_landing_sound(const glm::vec3& pos, float impact_speed) {
        float norm_speed = std::clamp(impact_speed / 18.0f, 0.2f, 1.5f);
        float intensity = 5.0f + 14.0f * norm_speed;
        float radius = 8.0f + 12.0f * norm_speed;
        emit_sound(SoundEventType::JumpLanding, pos, intensity, radius, 0.5f);
    }

    /// Jetpack thruster roaring burn in cavern
    void add_jetpack_sound(const glm::vec3& pos, float dt) {
        emit_sound(SoundEventType::JetpackThruster, pos, 18.0f * dt, 22.0f, 0.4f);
    }

    /// Grapple winch cable snap / reel
    void add_grapple_sound(const glm::vec3& pos, bool reeling) {
        float intensity = reeling ? 4.5f : 3.0f;
        float radius = reeling ? 11.0f : 8.0f;
        emit_sound(SoundEventType::GrappleAction, pos, intensity, radius, 0.35f);
    }

    /// Continuous drill grinding contact vibration
    void add_drill_sound(const glm::vec3& pos, float dt, float intensity = 14.0f) {
        emit_sound(SoundEventType::DrillVibration, pos, intensity * dt, 18.0f, 0.4f);
    }

    /// Voxel rock fracture / break noise
    void add_voxel_break_sound(const glm::vec3& pos, uint8_t mat) {
        float intensity = 6.0f;
        float radius = 16.0f;
        if (mat == 2) { intensity = 12.0f; radius = 22.0f; } // MAT_VOIDITE_CRYSTAL
        else if (mat == 3) { intensity = 9.0f; radius = 18.0f; } // MAT_VOLCANIC_BASALT
        else if (mat == 4) { intensity = 10.0f; radius = 19.0f; } // MAT_TITANIUM
        else if (mat == 8) { intensity = 24.0f; radius = 28.0f; } // MAT_REINFORCED_VAULT_DOOR
        emit_sound(SoundEventType::VoxelFracture, pos, intensity, radius, 0.6f);
    }

    /// Gunshot muzzle blast
    void add_gunshot_sound(const glm::vec3& muzzle_pos, int weapon_archetype = 0) {
        float intensity = 18.0f;
        float radius = 32.0f;
        if (weapon_archetype == 0) { intensity = 25.0f; radius = 38.0f; } // Magma Scattergun
        else if (weapon_archetype == 2) { intensity = 32.0f; radius = 45.0f; } // Needler Railgun
        emit_sound(SoundEventType::Gunshot, muzzle_pos, intensity, radius, 0.75f);
    }

    /// Bullet / plasma impact on solid voxel wall (creates acoustic distraction point!)
    void add_bullet_impact_sound(const glm::vec3& hit_pos, float stress = 2.0f) {
        float intensity = 4.0f * stress;
        float radius = 12.0f * std::clamp(stress, 0.5f, 2.0f);
        emit_sound(SoundEventType::BulletImpact, hit_pos, intensity, radius, 0.5f);
    }

    /// Demolition / Shaped charge detonation (massive concussive blastwave)
    void add_demolition_sound(const glm::vec3& pos, bool is_micro) {
        float intensity = is_micro ? 16.0f : 36.0f;
        float radius = is_micro ? 35.0f : 60.0f;
        emit_sound(SoundEventType::DemolitionBlast, pos, intensity, radius, 1.2f);
    }

    /// Active acoustic surveying radar pulse
    void add_sonar_sound(const glm::vec3& pos, float amount = 6.0f) {
        emit_sound(SoundEventType::SonarPulse, pos, amount, 28.0f, 0.8f);
    }

    /// Bulkhead placement or dismantling clang
    void add_bulkhead_sound(const glm::vec3& pos, bool place_or_remove = true) {
        emit_sound(SoundEventType::BulkheadClang, pos, place_or_remove ? 8.0f : 6.0f, 15.0f, 0.5f);
    }

    /// Dynamic falling debris impact on floor
    void add_debris_crash_sound(const glm::vec3& pos, size_t block_count) {
        float intensity = std::min(28.0f, 6.0f + static_cast<float>(block_count) * 1.5f);
        float radius = std::min(35.0f, 12.0f + static_cast<float>(block_count) * 2.0f);
        emit_sound(SoundEventType::DynamicDebrisCrash, pos, intensity, radius, 0.7f);
    }

    /// Tectonic seismic tremor rumble
    void add_seismic_sound(const glm::vec3& epicenter, float intensity) {
        emit_sound(SoundEventType::SeismicTremor, epicenter, intensity * 15.0f, 65.0f, 1.0f);
    }

    // ─── Backward-Compatible Legacy Methods ────────────────────────────

    void add_drill_vibration(float dt, float intensity = 14.0f) {
        m_noise = std::min(m_max_noise, m_noise + intensity * dt * m_noise_multiplier);
    }

    void add_drill_noise(float amount = 5.0f) {
        m_noise = std::min(m_max_noise, m_noise + amount * m_noise_multiplier);
    }

    void add_bulk_mine_noise(float amount = 12.0f) {
        m_noise = std::min(m_max_noise, m_noise + amount * m_noise_multiplier);
    }

    void add_demolition_noise(float amount = 28.0f) {
        m_noise = std::min(m_max_noise, m_noise + amount * m_noise_multiplier);
    }

    void add_sonar_noise(float amount = 6.0f) {
        m_noise = std::min(m_max_noise, m_noise + amount * m_noise_multiplier);
    }

    void add_explosive_noise(float amount = 35.0f) {
        m_noise = std::min(m_max_noise, m_noise + amount * m_noise_multiplier);
    }

    /// Get current noise level [0..max_noise]
    float noise() const { return m_noise; }
    float max_noise() const { return m_max_noise; }
    float noise_percent() const { return (m_max_noise > 0.0f) ? (m_noise / m_max_noise * 100.0f) : 0.0f; }

    AlertLevel get_alert_level() const {
        float pct = noise_percent();
        if (pct >= 99.5f) return AlertLevel::Swarming;
        if (pct >= 70.0f)  return AlertLevel::Agitated;
        if (pct >= 40.0f)  return AlertLevel::Alerted;
        return AlertLevel::Silent;
    }

    /// HUD helper: returns a string description of current alert
    const char* alert_string() const {
        switch (get_alert_level()) {
            case AlertLevel::Silent:   return "SILENT";
            case AlertLevel::Alerted:  return "ALERTED";
            case AlertLevel::Agitated: return "AGITATED";
            case AlertLevel::Swarming: return "SWARMING";
        }
        return "UNKNOWN";
    }

    /// HUD color based on alert level (r, g, b, a)
    void alert_color(float& r, float& g, float& b) const {
        switch (get_alert_level()) {
            case AlertLevel::Silent:   r = 0.0f; g = 0.94f; b = 1.0f; return; // Cyan
            case AlertLevel::Alerted:  r = 1.0f; g = 0.70f; b = 0.0f; return; // Amber
            case AlertLevel::Agitated: r = 1.0f; g = 0.30f; b = 0.0f; return; // Orange-Red
            case AlertLevel::Swarming: r = 1.0f; g = 0.05f; b = 0.05f; return;// Bright Red
        }
        r = 0.7f; g = 0.7f; b = 0.7f;
    }

    int swarm_count() const { return m_swarm_count; }
    bool is_crouching() const { return m_is_crouching; }
    void set_crouching(bool crouch) { m_is_crouching = crouch; }
    void set_noise_multiplier(float mul) { m_noise_multiplier = mul; }
    void set_on_wave(WaveCallback cb) { m_on_wave = std::move(cb); }

    /// Reset for new expedition
    void reset() {
        m_noise = 0.0f;
        m_last_alert = AlertLevel::Silent;
        m_swarm_count = 0;
        m_swarm_cooldown = 0.0f;
        m_recent_sounds.clear();
    }

private:
    float m_noise{0.0f};
    float m_max_noise{100.0f};
    float m_base_decay_rate{2.5f};      // Dissipation when standing still
    float m_crouch_decay_rate{5.0f};    // Twice as fast dissipation when crouching (stealth)
    float m_noise_multiplier{1.0f};     // Difficulty scaling
    bool m_is_crouching{false};

    AlertLevel m_last_alert{AlertLevel::Silent};
    int m_swarm_count{0};
    float m_swarm_cooldown{0.0f};

    std::vector<SoundEvent> m_recent_sounds;
    WaveCallback m_on_wave;
};

} // namespace Voidfall
