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

    /// Calculates local model anchor/pivot offset based on surface angle
    /// so the mesh legs stay flush with voxel faces without clipping.
    static glm::vec3 compute_surface_snapping_offset(
        StalkerSurfaceState state,
        const glm::vec3& normal
    );
};

} // namespace Voidfall
