#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <string>
#include "void_stalker.hpp"
#include "../../systems/noise_meter.hpp"

namespace Voidfall {

class World;

/// Behavioral states for the Seismic Burrower finite state machine
enum class BurrowerState : uint8_t {
    Dormant,    // Deep in bedrock, awaiting seismic disturbance / hazard escalation
    Burrowing,  // Tunneling through rock toward player or extraction beacon
    Breaching,  // Breaking through cavern wall into open space
    Charging,   // Direct kinetic ram charge at target
    Stunned,    // Collided with bedrock/bulkhead or blasted by explosives
    Enraged,    // Emitting tectonic shockwave to destabilize ceiling voxels
    Dead        // Slumped in cavern, ready for cleanup
};

/// A massive subterranean borer that excavates soft voxels, triggers cave-ins,
/// and rams delvers with devastating kinetic force.
///
/// Design: High-threat enemy emerging in Phase 3/4 and during extraction holdouts.
/// Weak to Demolitionist explosive charges (takes 2.5x explosive damage).
struct SeismicBurrower {
    uint32_t id{0};
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 target_pos{0.0f};      // Target position (player or beacon)
    float yaw{0.0f};                 // Horizontal facing angle
    float pitch{0.0f};               // Vertical facing angle
    float cutter_angle{0.0f};        // Rotating drill head angle

    BurrowerState state{BurrowerState::Burrowing};
    float state_timer{0.0f};
    float hp{120.0f};
    float max_hp{120.0f};
    float ram_damage{24.0f};         // Heavy kinetic ram damage
    float attack_cooldown{0.0f};
    float stun_timer{0.0f};
    float shockwave_cooldown{6.0f};  // Cooldown for ceiling collapse shockwave

    // Movement & Tunneling
    float burrow_speed{3.2f};        // Underground excavation speed
    float charge_speed{9.5f};        // Open cavern charge speed
    float turn_rate{2.2f};           // Radians per second
    float scale{1.75f};              // Massive size compared to stalker
    float rumble_intensity{0.0f};    // Low-frequency tremor intensity

    // Visual animation
    float pulse_phase{0.0f};
    float segment_wiggle{0.0f};

    // Audio & Roar indicators
    float roar_timer{0.0f};          // Timer between deep subterranean tectonic roars
    float grind_timer{0.0f};         // Timer between cutter teeth grinding pulses
    float chatter_timer{0.0f};       // Timer between subterranean chitinous mandible/cutter chatter clicks
    bool just_roared{false};         // Emitted deep subterranean roar this frame
    bool just_ground{false};         // Emitted rock tooth grinding this frame
    bool just_chattered{false};      // Emitted chattering clicks this frame

    // Acoustic sound tracking
    glm::vec3 sound_target{0.0f};
    bool has_sound_target{false};
    float sound_target_timer{0.0f};

    // Retaliation Pursuit
    bool is_pursuing_attacker{false}; // Aggressively tunneling/pursuing player after taking damage
    float pursuit_lost_timer{0.0f};   // Timer incremented when player is far away without contact
    float pursuit_break_dist{35.0f};  // Separation distance beyond which player is considered fleeing
    float pursuit_break_time{15.0f};  // Time player must remain far away before burrower ceases pursuit

    // Frame events
    bool just_breached{false};
    bool just_rammed{false};
    bool just_triggered_cavein{false};
    bool just_stunned{false};
    bool just_died{false};

    bool is_dead() const { return state == BurrowerState::Dead; }

    /// Forward direction vector based on velocity heading or pitch/yaw orientation
    glm::vec3 forward() const {
        if (glm::length(velocity) > 0.05f) {
            return glm::normalize(velocity);
        }
        return glm::vec3(std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    }

    /// Visual awareness classification for overhead indicators
    bool is_investigating_state() const {
        return !is_dead() && (state == BurrowerState::Burrowing);
    }

    bool is_engaged_state() const {
        return !is_dead() && (state == BurrowerState::Breaching ||
                              state == BurrowerState::Charging ||
                              state == BurrowerState::Enraged);
    }

    /// Returns true if the burrower is completely unalerted (dormant or unaware of sound/player)
    bool is_unalerted() const {
        return !is_dead() &&
               (state == BurrowerState::Dormant ||
                (!is_engaged_state() && !has_sound_target && !is_pursuing_attacker));
    }

    /// Returns emissive slag/eye color based on state
    glm::vec4 get_core_color() const {
        switch (state) {
            case BurrowerState::Enraged:   return glm::vec4(1.0f, 0.15f, 0.05f, 1.0f); // Blazing magma red
            case BurrowerState::Charging:  return glm::vec4(1.0f, 0.45f, 0.05f, 1.0f); // Molten amber slag
            case BurrowerState::Breaching: return glm::vec4(1.0f, 0.70f, 0.10f, 1.0f); // Electric fracture yellow
            case BurrowerState::Stunned:   return glm::vec4(0.20f, 0.75f, 1.0f, 1.0f); // Cool electric blue
            case BurrowerState::Burrowing:
            default:                       return glm::vec4(0.95f, 0.35f, 0.05f, 1.0f); // Warm subterranean orange
        }
    }
};

inline EnemyAwarenessMarkerType get_awareness_marker_type(const SeismicBurrower& b) {
    if (b.is_engaged_state()) return EnemyAwarenessMarkerType::RedTriangle;
    if (b.is_investigating_state()) return EnemyAwarenessMarkerType::YellowExclamation;
    return EnemyAwarenessMarkerType::None;
}

/// Excavated block delta emitted when the burrower eats terrain
struct ExcavatedVoxel {
    glm::ivec3 pos;
    uint8_t original_mat;
};

/// Manages all active Seismic Burrowers in the cavern
class SeismicBurrowerManager {
public:
    SeismicBurrowerManager() = default;

    /// Spawn a burrower near target (usually deep in rock wall)
    void spawn_burrower(const glm::vec3& pos, float hp_multiplier = 1.0f);

    /// Spawn burrower during hazard Phase 3/4 or holdout
    void spawn_hazard_wave(const glm::vec3& player_pos, const World& world);

    struct FrameResult {
        float total_damage{0.0f};
        int burrowers_killed{0};
        bool any_breach{false};
        bool any_charge{false};
        bool any_ram_hit{false};
        bool any_cavein_triggered{false};
        bool any_roar{false};
        bool any_grind{false};
        bool any_chatter{false};
        float max_rumble{0.0f};
        std::vector<ExcavatedVoxel> excavated_voxels;
        std::vector<glm::ivec3> cavein_origins;
    };

    /// Update burrower AI, tunneling physics, voxel destruction, and combat
    FrameResult update(float dt, const glm::vec3& player_pos, World& world,
                       const std::vector<SoundEvent>& sound_events = {},
                       bool player_is_crouching = false);

    /// Apply damage to nearest burrower (with optional explosive multiplier and sneak attack crit)
    bool damage_nearest(const glm::vec3& origin, float radius, float damage,
                        bool is_explosive = false, bool allow_crit = false,
                        bool* out_is_crit = nullptr, float* out_damage_dealt = nullptr);

    /// Apply stun to burrowers near an origin (e.g. bulkhead collision or concussion blast)
    void apply_stun(const glm::vec3& origin, float radius, float duration = 2.5f);

    const std::vector<SeismicBurrower>& burrowers() const { return m_burrowers; }
    std::vector<SeismicBurrower>& burrowers_mut() { return m_burrowers; }

    int active_count() const;
    void remove_dead();
    void reset();

private:
    std::vector<SeismicBurrower> m_burrowers;
    uint32_t m_next_id{1001};
    float m_spawn_cooldown{0.0f};
};

} // namespace Voidfall
