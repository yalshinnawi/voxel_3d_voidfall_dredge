#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <vector>
#include "../systems/stealth_system.hpp"

namespace Voidfall {

class World;

/// Behavioral states for the subterranean creature AI finite state machine
enum class StalkerState : uint8_t {
    Idle = 0,           // Clinging to wall/ceiling, ambient prowling in cavern
    Investigating,      // Scurrying towards heard sound event origin (sniffing/clicking in dark)
    Stalking,           // Pursuing player while in visual contact or direct combat range
    Circling,           // Circling around player at mid-range before attack
    Lunging,            // Committed attack leap toward player (Melee only)
    Stunned,            // Temporarily disabled (sonar pulse, bright light)
    Fleeing,            // Retreating after taking critical damage
    Burrowing,          // Escaping by burrowing through cavern rock walls
    Dying,              // Death animation collapse & tumbling before carcass transition
    Dead,               // Marked for removal
    Roosting,           // Initial dormant state, clamped to surfaces with closed eyes/retracted limbs

    // Uppercase aliases for compatibility with AIState enum conventions
    IDLE = Idle,
    ROOSTING = Roosting,
    INVESTIGATING = Investigating,
    STALKING = Stalking,
    PURSUIT = Stalking,
    Pursuit = Stalking,
    CIRCLING = Circling,
    LUNGING = Lunging,
    STUNNED = Stunned,
    FLEEING = Fleeing,
    BURROWING = Burrowing,
    ESCAPING = Burrowing,
    DYING = Dying,
    DEAD = Dead
};

using AIState = StalkerState;

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

/// Result of evaluating acoustic and visual perception against an entity
struct PerceptionEvaluation {
    bool state_changed{false};
    AIState new_state{AIState::ROOSTING};
    glm::vec3 target_pos{0.0f};
    float received_noise_db{0.0f};
    bool triggered_screech{false};
    bool triggered_swarm_alert{false};
};

/// AI utilities for aberrant surface attachment, 6-direction raycast sampling,
/// contact normal classification, smooth transform orientation, and acoustic perception.
class AberrantAI {
public:
    /// Acoustic Hearing & Detection Thresholds
    static constexpr float THRESHOLD_INVESTIGATE_DB = 30.0f;
    static constexpr float THRESHOLD_PURSUIT_DB     = 70.0f;

    /// Calculates acoustic noise received in dB from a sound source at distance
    static float calculate_received_noise_db(
        const glm::vec3& listener_pos,
        const glm::vec3& noise_pos,
        float emitted_db,
        float audible_radius,
        bool has_los = true
    );

    /// Evaluates enemy hearing and detection against a single noise source and visual sightlines:
    /// 1. If player is spotted directly or noise reaches > 70 dB -> AIState::PURSUIT with screech and swarm alert.
    /// 2. If noise reaches > 30 dB -> AIState::INVESTIGATING toward noise source.
    /// 3. Otherwise, state is unchanged (remains in current state, e.g. AIState::ROOSTING).
    static PerceptionEvaluation evaluate_hearing(
        AIState current_state,
        const glm::vec3& enemy_pos,
        bool can_see_player,
        const glm::vec3& player_pos,
        const glm::vec3& noise_pos,
        float emitted_db,
        float audible_radius,
        bool has_los = true
    );

    /// Evaluates multiple acoustic events (such as from StealthSystem) and visual sightlines
    static PerceptionEvaluation evaluate_acoustic_and_visual_perception(
        AIState current_state,
        const glm::vec3& enemy_pos,
        bool can_see_player,
        const glm::vec3& player_pos,
        const std::vector<AcousticNoiseEvent>& acoustic_events,
        bool has_los = true
    );
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
        float probe_distance = 1.4f,
        StalkerSurfaceState current_state = StalkerSurfaceState::FLOOR,
        const glm::vec3* target_pos = nullptr
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

    /// Calculates pairwise repulsive separation force between swarm entities:
    /// F_sep = sum((pos_i - pos_j) / ||pos_i - pos_j||^2) * 8.0f
    static glm::vec3 calculate_swarm_separation(
        const glm::vec3& self_pos,
        const std::vector<glm::vec3>& other_positions,
        float max_distance = 6.0f,
        float separation_force = 8.0f
    );

    /// When crawling on the ceiling and target is below, finds the optimal
    /// horizontal heading towards an adjacent wall that allows descending to the floor.
    static glm::vec3 find_descending_wall_direction(
        const glm::vec3& enemy_pos,
        const glm::vec3& target_pos,
        const World& world,
        float max_probe_dist = 24.0f
    );

    /// Calculates 3D traversal direction along a climbing wall (tangent plane) towards target,
    /// enabling vertical climbing up/down the wall as well as horizontal wall-crawling.
    static glm::vec3 calculate_wall_traversal_direction(
        const glm::vec3& enemy_pos,
        const glm::vec3& target_pos,
        const glm::vec3& contact_normal
    );

    /// Samples forward and angled whiskers to steer smoothly around voxel obstacles on surfaces
    static glm::vec3 steer_around_obstacles(
        const glm::vec3& enemy_pos,
        const glm::vec3& desired_dir,
        const glm::vec3& up_normal,
        const World& world,
        float probe_dist = 1.6f
    );

    /// Robust swept voxel collision resolving against walls/geometry.
    /// Clamps movement to prevent clipping/moving through walls, while allowing 1-block step clambering
    /// on floors when headroom is clear.
    static bool resolve_voxel_collision(
        glm::vec3& pos,
        glm::vec3& velocity,
        float radius,
        float height,
        StalkerSurfaceState surface_state,
        const glm::vec3& contact_normal,
        const World& world
    );
};

} // namespace Voidfall
