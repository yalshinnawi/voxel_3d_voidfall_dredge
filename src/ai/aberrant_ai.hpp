#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

namespace Voidfall {

class World;

/// Explicit traversal state for aberrant subterranean crawlers
enum class StalkerSurfaceState : uint8_t {
    FLOOR,
    WALL_CLIMBING,
    CEILING_CRAWLING,
    TRANSITIONING
};

/// Result of probing voxel geometry along orthogonal directions
struct SurfaceContactSample {
    glm::vec3 contact_normal{0.0f, 1.0f, 0.0f};
    StalkerSurfaceState surface_state{StalkerSurfaceState::FLOOR};
    float distance{1.0f};
    bool has_contact{false};
};

/// AI utilities for aberrant surface attachment, 6-direction raycast sampling,
/// contact normal classification, and smooth transform orientation.
class AberrantAI {
public:
    /// Classifies contact normal into FLOOR, WALL_CLIMBING, or CEILING_CRAWLING based on:
    /// - Floor:   n · up > 0.7
    /// - Wall:    |n · up| <= 0.7
    /// - Ceiling: n · up < -0.7
    static StalkerSurfaceState classify_normal(const glm::vec3& normal, const glm::vec3& world_up = glm::vec3(0.0f, 1.0f, 0.0f));

    /// Samples geometry along the 6 orthogonal axes (+Y, -Y, +X, -X, +Z, -Z)
    /// to locate solid voxels and find primary contact normal n_contact.
    static SurfaceContactSample sample_surface_normal(
        const glm::vec3& position,
        const World& world,
        float probe_distance = 1.4f
    );

    /// Computes target orientation quaternion from velocity v and contact normal n:
    /// forward = normalize(v - (v · n)n)
    /// right   = cross(forward, n)
    static glm::quat calculate_orientation(
        const glm::vec3& velocity,
        const glm::vec3& contact_normal,
        const glm::vec3& fallback_forward
    );

    /// Smoothly slerps current rotation towards target rotation without snapping:
    /// m_currentRotation = glm::slerp(m_currentRotation, m_targetRotation, dt * 8.0f)
    static glm::quat slerp_rotation(
        const glm::quat& current,
        const glm::quat& target,
        float dt,
        float speed = 8.0f
    );

    /// Minimum engagement parameters
    static constexpr float MIN_STRIKE_DISTANCE    = 1.75f; // Distance maintained at end of lunge
    static constexpr float MELEE_STRIKE_REACH     = 2.20f; // Range where melee damage registers
    static constexpr float ENEMY_PLAYER_REPULSION  = 14.0f; // Soft repulsion force to prevent phasing

    /// Calculates local model anchor/pivot offset based on surface angle
    /// so the mesh legs stay flush with voxel faces without clipping.
    static glm::vec3 compute_surface_snapping_offset(
        StalkerSurfaceState state,
        const glm::vec3& normal
    );

    /// Hard capsule-capsule penetration resolution against the player
    static bool resolve_player_penetration(
        glm::vec3& enemy_pos,
        const glm::vec3& player_pos,
        float enemy_radius = 0.5f,
        float player_radius = 0.35f
    );

    /// Compute lunge target offset maintaining stand-off distance
    static glm::vec3 calculate_lunge_target(
        const glm::vec3& enemy_pos,
        const glm::vec3& player_pos,
        float stand_off = MIN_STRIKE_DISTANCE
    );

    /// Cancel forward momentum along attack approach vector
    static void cancel_forward_momentum(
        glm::vec3& velocity,
        const glm::vec3& attack_dir
    );
};

} // namespace Voidfall
