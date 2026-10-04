#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include "../ai/aberrant_ai.hpp"

namespace Voidfall {

/// Distinct pose metrics applied to articulated Stalker carapace, limbs, and head
struct StalkerPoseParameters {
    float carapace_offset_y{0.0f};      // Height adjustment relative to anchor
    float carapace_pitch{0.0f};         // Torso tilt angle
    float front_claw_reach{1.0f};       // Reach multiplier for front scythe claws
    float front_claw_pitch_offset{0.0f};// Pitch offset extending claw upward/forward
    float front_claw_yaw{0.60f};        // Lateral spread of front claws
    float limb_splay_multiplier{1.0f};  // Lateral splay width of mid & rear legs
    float femur_pitch_offset{0.0f};     // Hip pitch adjustment
    float tibia_pitch_offset{0.0f};     // Claw angle to grip surface
    float head_pitch_offset{0.0f};      // Downward-arching neck/head tracking
};

/// Animation controller managing state-based locomotion, 0.2s crossfade transitions,
/// distinct surface poses, and velocity-proportional play rate modulation.
class StalkerAnimationController {
public:
    static constexpr float BLEND_WINDOW = 0.2f; // 0.2s crossfade window

    StalkerAnimationController();

    /// Updates transition weights and walk cycle phase over time dt
    void update(float dt, StalkerSurfaceState target_state, const glm::vec3& velocity, float climb_speed_scalar = 3.2f);

    /// Modulate leg cycle speed proportionally to linear climbing velocity:
    /// animPlayRate = glm::length(m_velocity) * m_climbSpeedScalar
    float calculate_play_rate(const glm::vec3& velocity, float climb_speed_scalar = 3.2f) const;

    /// Computes the blended pose parameters according to current crossfaded weights
    StalkerPoseParameters compute_blended_pose(float player_pitch = 0.0f) const;

    // Weight accessors
    float floor_weight() const { return m_floor_weight; }
    float wall_weight() const { return m_wall_weight; }
    float ceiling_weight() const { return m_ceiling_weight; }

    StalkerSurfaceState current_state() const { return m_current_state; }
    StalkerSurfaceState target_state() const { return m_target_state; }
    bool is_transitioning() const { return m_transition_timer < BLEND_WINDOW; }
    float transition_progress() const { return std::clamp(m_transition_timer / BLEND_WINDOW, 0.0f, 1.0f); }

    /// Force immediate state change without crossfade (useful for testing or initial spawn)
    void set_immediate_state(StalkerSurfaceState state);

    // Preset pose definitions
    static StalkerPoseParameters get_floor_pose();
    static StalkerPoseParameters get_wall_climb_pose();
    static StalkerPoseParameters get_ceiling_inversion_pose(float player_pitch = 0.0f);

private:
    StalkerSurfaceState m_current_state{StalkerSurfaceState::FLOOR};
    StalkerSurfaceState m_target_state{StalkerSurfaceState::FLOOR};

    float m_floor_weight{1.0f};
    float m_wall_weight{0.0f};
    float m_ceiling_weight{0.0f};

    float m_transition_timer{0.2f};
};

} // namespace Voidfall
