#include "animation_controller.hpp"
#include <algorithm>
#include <cmath>

namespace Voidfall {

StalkerAnimationController::StalkerAnimationController()
    : m_current_state(StalkerSurfaceState::FLOOR)
    , m_target_state(StalkerSurfaceState::FLOOR)
    , m_floor_weight(1.0f)
    , m_wall_weight(0.0f)
    , m_ceiling_weight(0.0f)
    , m_transition_timer(BLEND_WINDOW)
{
}

void StalkerAnimationController::set_immediate_state(StalkerSurfaceState state) {
    if (state == StalkerSurfaceState::TRANSITIONING) {
        state = StalkerSurfaceState::FLOOR;
    }
    m_current_state = state;
    m_target_state = state;
    m_transition_timer = BLEND_WINDOW;

    m_floor_weight   = (state == StalkerSurfaceState::FLOOR) ? 1.0f : 0.0f;
    m_wall_weight    = (state == StalkerSurfaceState::WALL_CLIMBING) ? 1.0f : 0.0f;
    m_ceiling_weight = (state == StalkerSurfaceState::CEILING_CRAWLING) ? 1.0f : 0.0f;
}

float StalkerAnimationController::calculate_play_rate(const glm::vec3& velocity, float climb_speed_scalar) const {
    float speed = glm::length(velocity);
    float rate = speed * climb_speed_scalar;
    // Maintain subtle breathing/twitch when stationary so the predator looks alive
    return (speed > 0.05f) ? rate : 0.8f;
}

void StalkerAnimationController::update(
    float dt,
    StalkerSurfaceState target_state,
    const glm::vec3& velocity,
    float climb_speed_scalar)
{
    // Ignore direct requests to set state to TRANSITIONING; transition is managed by timer
    if (target_state != StalkerSurfaceState::TRANSITIONING && target_state != m_target_state) {
        m_target_state = target_state;
        m_transition_timer = 0.0f;
        m_current_state = StalkerSurfaceState::TRANSITIONING;
    }

    if (m_transition_timer < BLEND_WINDOW) {
        m_transition_timer += dt;
        float progress = std::clamp(m_transition_timer / BLEND_WINDOW, 0.0f, 1.0f);

        // Compute destination weights for target state
        float dest_floor   = (m_target_state == StalkerSurfaceState::FLOOR) ? 1.0f : 0.0f;
        float dest_wall    = (m_target_state == StalkerSurfaceState::WALL_CLIMBING) ? 1.0f : 0.0f;
        float dest_ceiling = (m_target_state == StalkerSurfaceState::CEILING_CRAWLING) ? 1.0f : 0.0f;

        // Smoothstep interpolation for seamless acceleration/deceleration
        float s = progress * progress * (3.0f - 2.0f * progress);

        // Blend weights smoothly
        m_floor_weight   = glm::mix(m_floor_weight, dest_floor, s);
        m_wall_weight    = glm::mix(m_wall_weight, dest_wall, s);
        m_ceiling_weight = glm::mix(m_ceiling_weight, dest_ceiling, s);

        // Normalize weights
        float total = m_floor_weight + m_wall_weight + m_ceiling_weight;
        if (total > 0.0001f) {
            m_floor_weight   /= total;
            m_wall_weight    /= total;
            m_ceiling_weight /= total;
        }

        if (progress >= 1.0f) {
            m_current_state = m_target_state;
            m_floor_weight   = dest_floor;
            m_wall_weight    = dest_wall;
            m_ceiling_weight = dest_ceiling;
        }
    }
}

StalkerPoseParameters StalkerAnimationController::get_floor_pose() {
    StalkerPoseParameters p;
    p.carapace_offset_y       = 0.0f;
    p.carapace_pitch          = 0.0f;
    p.front_claw_reach        = 1.0f;
    p.front_claw_pitch_offset = 0.0f;
    p.front_claw_yaw          = 0.60f;
    p.limb_splay_multiplier   = 1.0f;
    p.femur_pitch_offset      = 0.0f;
    p.tibia_pitch_offset      = 0.0f;
    p.head_pitch_offset       = 0.0f;
    return p;
}

StalkerPoseParameters StalkerAnimationController::get_wall_climb_pose() {
    StalkerPoseParameters p;
    // Flatten carapace closer to the surface plane (lower profile)
    p.carapace_offset_y       = -0.12f;
    p.carapace_pitch          = -0.06f;
    // Extend front claw reach forward & up the wall
    p.front_claw_reach        = 1.55f;
    p.front_claw_pitch_offset = -0.38f;
    p.front_claw_yaw          = 0.90f;
    // Splay limbs wider along wall normal
    p.limb_splay_multiplier   = 1.45f;
    p.femur_pitch_offset      = 0.28f;
    p.tibia_pitch_offset      = -0.32f;
    // Head tilted outward to scan cavern floor
    p.head_pitch_offset       = -0.18f;
    return p;
}

StalkerPoseParameters StalkerAnimationController::get_ceiling_inversion_pose(float player_pitch) {
    StalkerPoseParameters p;
    // Hanging flush under ceiling
    p.carapace_offset_y       = -0.08f;
    p.carapace_pitch          = 0.04f;
    p.front_claw_reach        = 1.25f;
    p.front_claw_pitch_offset = -0.20f;
    p.front_claw_yaw          = 1.15f;
    // Fully spread limbs anchored to ceiling voxels
    p.limb_splay_multiplier   = 1.65f;
    p.femur_pitch_offset      = 0.38f;
    p.tibia_pitch_offset      = -0.48f;
    // Downward-arching predatory neck/head tracking toward player
    p.head_pitch_offset       = -0.85f - std::max(0.0f, player_pitch * 0.4f);
    return p;
}

StalkerPoseParameters StalkerAnimationController::compute_blended_pose(float player_pitch) const {
    StalkerPoseParameters floor_p   = get_floor_pose();
    StalkerPoseParameters wall_p    = get_wall_climb_pose();
    StalkerPoseParameters ceiling_p = get_ceiling_inversion_pose(player_pitch);

    StalkerPoseParameters result;
    result.carapace_offset_y =
        floor_p.carapace_offset_y * m_floor_weight +
        wall_p.carapace_offset_y * m_wall_weight +
        ceiling_p.carapace_offset_y * m_ceiling_weight;

    result.carapace_pitch =
        floor_p.carapace_pitch * m_floor_weight +
        wall_p.carapace_pitch * m_wall_weight +
        ceiling_p.carapace_pitch * m_ceiling_weight;

    result.front_claw_reach =
        floor_p.front_claw_reach * m_floor_weight +
        wall_p.front_claw_reach * m_wall_weight +
        ceiling_p.front_claw_reach * m_ceiling_weight;

    result.front_claw_pitch_offset =
        floor_p.front_claw_pitch_offset * m_floor_weight +
        wall_p.front_claw_pitch_offset * m_wall_weight +
        ceiling_p.front_claw_pitch_offset * m_ceiling_weight;

    result.front_claw_yaw =
        floor_p.front_claw_yaw * m_floor_weight +
        wall_p.front_claw_yaw * m_wall_weight +
        ceiling_p.front_claw_yaw * m_ceiling_weight;

    result.limb_splay_multiplier =
        floor_p.limb_splay_multiplier * m_floor_weight +
        wall_p.limb_splay_multiplier * m_wall_weight +
        ceiling_p.limb_splay_multiplier * m_ceiling_weight;

    result.femur_pitch_offset =
        floor_p.femur_pitch_offset * m_floor_weight +
        wall_p.femur_pitch_offset * m_wall_weight +
        ceiling_p.femur_pitch_offset * m_ceiling_weight;

    result.tibia_pitch_offset =
        floor_p.tibia_pitch_offset * m_floor_weight +
        wall_p.tibia_pitch_offset * m_wall_weight +
        ceiling_p.tibia_pitch_offset * m_ceiling_weight;

    result.head_pitch_offset =
        floor_p.head_pitch_offset * m_floor_weight +
        wall_p.head_pitch_offset * m_wall_weight +
        ceiling_p.head_pitch_offset * m_ceiling_weight;

    return result;
}

} // namespace Voidfall
