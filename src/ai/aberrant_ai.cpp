#include "aberrant_ai.hpp"
#include "../voxel/world.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Voidfall {

StalkerSurfaceState AberrantAI::classify_normal(const glm::vec3& normal, const glm::vec3& world_up) {
    float dot = glm::dot(normal, world_up);
    if (dot > 0.7f) {
        return StalkerSurfaceState::FLOOR;
    }
    if (dot < -0.7f) {
        return StalkerSurfaceState::CEILING_CRAWLING;
    }
    return StalkerSurfaceState::WALL_CLIMBING;
}

SurfaceContactSample AberrantAI::sample_surface_normal(
    const glm::vec3& position,
    const World& world,
    float probe_distance,
    StalkerSurfaceState current_state)
{
    SurfaceContactSample result;
    result.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
    result.surface_state = StalkerSurfaceState::FLOOR;
    result.distance = probe_distance;
    result.has_contact = false;

    // 6 orthogonal probe directions
    const glm::vec3 directions[6] = {
        glm::vec3( 0.0f, -1.0f,  0.0f), // Down (floor)
        glm::vec3( 0.0f,  1.0f,  0.0f), // Up (ceiling)
        glm::vec3(-1.0f,  0.0f,  0.0f), // West wall
        glm::vec3( 1.0f,  0.0f,  0.0f), // East wall
        glm::vec3( 0.0f,  0.0f, -1.0f), // North wall
        glm::vec3( 0.0f,  0.0f,  1.0f)  // South wall
    };

    float min_dist = probe_distance + 1.0f;
    glm::vec3 best_normal(0.0f, 1.0f, 0.0f);
    bool has_down = false;
    float down_dist = probe_distance + 1.0f;

    for (int i = 0; i < 6; ++i) {
        const glm::vec3& dir = directions[i];
        
        // Raymarch up to probe_distance in small steps
        const float step_size = 0.15f;
        const int max_steps = static_cast<int>(probe_distance / step_size);

        for (int step = 1; step <= max_steps; ++step) {
            glm::vec3 p = position + dir * (static_cast<float>(step) * step_size);
            int ix = static_cast<int>(std::floor(p.x));
            int iy = static_cast<int>(std::floor(p.y));
            int iz = static_cast<int>(std::floor(p.z));

            // Upward probe (ceiling) must be strictly above the entity's current voxel level
            int cur_y = static_cast<int>(std::floor(position.y));
            if (dir.y > 0.5f && iy <= cur_y) {
                continue;
            }

            Voxel vox = world.get_voxel(ix, iy, iz);
            if (vox.material_id != MAT_AIR && vox.material_id != MAT_GAS && vox.material_id != MAT_VOLATILE_SMOKE) {
                // Voxel boundary face normal points back toward entity
                glm::vec3 face_normal = -dir;

                // Calculate exact plane distance to the face
                float d = static_cast<float>(step) * step_size;
                if (dir.y < -0.5f) {
                    d = position.y - (static_cast<float>(iy) + 1.0f);
                } else if (dir.y > 0.5f) {
                    d = static_cast<float>(iy) - position.y;
                } else if (dir.x < -0.5f) {
                    d = position.x - (static_cast<float>(ix) + 1.0f);
                } else if (dir.x > 0.5f) {
                    d = static_cast<float>(ix) - position.x;
                } else if (dir.z < -0.5f) {
                    d = position.z - (static_cast<float>(iz) + 1.0f);
                } else if (dir.z > 0.5f) {
                    d = static_cast<float>(iz) - position.z;
                }
                d = std::max(0.0f, d);

                if (i == 0) { // Down
                    has_down = true;
                    down_dist = d;
                }

                if (d < min_dist) {
                    min_dist = d;
                    best_normal = face_normal;
                    result.has_contact = true;
                }
                break;
            }
        }
    }

    // Surface hysteresis selection:
    // If currently on FLOOR (or transitioning from floor) and floor contact is valid within reachable ground distance (<= 1.25m),
    // prioritize FLOOR over adjacent side walls to prevent rapid 60Hz surface flipping/jitter.
    if ((current_state == StalkerSurfaceState::FLOOR || current_state == StalkerSurfaceState::TRANSITIONING) && has_down && down_dist <= 1.25f) {
        best_normal = glm::vec3(0.0f, 1.0f, 0.0f);
        min_dist = down_dist;
        result.has_contact = true;
    } else if (current_state == StalkerSurfaceState::WALL_CLIMBING && has_down && down_dist <= 0.55f) {
        // Smoothly step down onto floor when reaching ground level
        best_normal = glm::vec3(0.0f, 1.0f, 0.0f);
        min_dist = down_dist;
        result.has_contact = true;
    }

    if (result.has_contact) {
        result.contact_normal = glm::normalize(best_normal);
        result.distance = min_dist;
        result.surface_state = classify_normal(result.contact_normal);
    } else {
        result.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
        result.distance = probe_distance;
        result.surface_state = StalkerSurfaceState::FLOOR;
    }

    return result;
}

glm::quat AberrantAI::calculate_orientation(
    const glm::vec3& velocity,
    const glm::vec3& contact_normal,
    const glm::vec3& fallback_forward)
{
    glm::vec3 n = glm::length(contact_normal) > 0.001f
        ? glm::normalize(contact_normal)
        : glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 fwd_candidate = velocity;

    // Anti-jitter: If fallback_forward is valid and velocity is moving backward (e.g. knockback/recoil from weapon fire),
    // keep facing the forward direction instead of flipping 180 degrees.
    if (glm::length(fallback_forward) > 0.01f) {
        glm::vec3 fwd_ref = fallback_forward - glm::dot(fallback_forward, n) * n;
        if (glm::length(fwd_ref) > 0.01f) {
            glm::vec3 v_tangent = fwd_candidate - glm::dot(fwd_candidate, n) * n;
            if (glm::length(v_tangent) < 0.01f || glm::dot(glm::normalize(v_tangent), glm::normalize(fwd_ref)) < -0.2f) {
                fwd_candidate = fallback_forward;
            }
        }
    }

    // Project velocity onto tangent plane: fwd = v - (v · n)n
    glm::vec3 fwd_proj = fwd_candidate - glm::dot(fwd_candidate, n) * n;

    if (glm::length(fwd_proj) < 0.01f) {
        // Fall back to projected previous facing direction
        fwd_proj = fallback_forward - glm::dot(fallback_forward, n) * n;
    }

    if (glm::length(fwd_proj) < 0.01f) {
        // Choose arbitrary tangent perpendicular to n
        glm::vec3 temp = (std::abs(n.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        fwd_proj = glm::cross(n, temp);
    }

    glm::vec3 forward = glm::normalize(fwd_proj);
    // Right-handed basis: cross(n, forward) yields the +X vector in model space
    glm::vec3 right = glm::normalize(glm::cross(n, forward));

    // Construct orthonormal rotation matrix
    glm::mat3 rot_mat;
    rot_mat[0] = right;
    rot_mat[1] = n;
    rot_mat[2] = forward;

    glm::quat q = glm::quat_cast(rot_mat);
    return glm::normalize(q);
}

glm::quat AberrantAI::slerp_rotation(
    const glm::quat& current,
    const glm::quat& target,
    float dt,
    float speed)
{
    glm::quat t = target;
    // Shortest path slerp
    if (glm::dot(current, t) < 0.0f) {
        t = -t;
    }
    float factor = std::clamp(dt * speed, 0.0f, 1.0f);
    glm::quat result = glm::slerp(current, t, factor);
    return glm::normalize(result);
}

glm::vec3 AberrantAI::compute_surface_snapping_offset(
    StalkerSurfaceState state,
    const glm::vec3& normal)
{
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0.0f, 1.0f, 0.0f);

    float clearance = 0.05f; // Floor clearance
    if (state == StalkerSurfaceState::WALL_CLIMBING) {
        clearance = 0.18f; // Flush low-profile wall attachment
    } else if (state == StalkerSurfaceState::CEILING_CRAWLING) {
        clearance = 0.22f; // Flush inverted ceiling gripping offset
    }

    return n * clearance;
}

bool AberrantAI::resolve_player_penetration(
    glm::vec3& enemy_pos,
    const glm::vec3& player_pos,
    float enemy_radius,
    float player_radius)
{
    glm::vec3 delta = enemy_pos - player_pos;
    delta.y = 0.0f; // Horizontal separation plane
    float dist = glm::length(delta);
    float minRadius = enemy_radius + player_radius; // e.g., 0.5m + 0.35m = 0.85m
    if (dist < minRadius) {
        glm::vec3 pushDir = (dist > 0.001f) ? (delta / dist) : glm::vec3(0.0f, 0.0f, 1.0f);
        float penetration = minRadius - dist;
        enemy_pos += pushDir * penetration; // Push enemy back out into attackable view
        return true;
    }
    return false;
}

glm::vec3 AberrantAI::calculate_lunge_target(
    const glm::vec3& enemy_pos,
    const glm::vec3& player_pos,
    float stand_off)
{
    glm::vec3 to_player = player_pos - enemy_pos;
    float dist = glm::length(to_player);
    glm::vec3 dir = (dist > 0.001f) ? (to_player / dist) : glm::vec3(0.0f, 0.0f, 1.0f);
    return player_pos - dir * stand_off;
}

void AberrantAI::cancel_forward_momentum(
    glm::vec3& velocity,
    const glm::vec3& attack_dir)
{
    glm::vec3 dir = (glm::length(attack_dir) > 0.001f) ? glm::normalize(attack_dir) : glm::vec3(0.0f);
    float forward_proj = glm::dot(velocity, dir);
    if (forward_proj > 0.0f) {
        velocity -= dir * forward_proj;
    }
}

glm::vec3 AberrantAI::calculate_swarm_separation(
    const glm::vec3& self_pos,
    const std::vector<glm::vec3>& other_positions,
    float max_distance,
    float separation_force)
{
    glm::vec3 total_force(0.0f);
    for (const auto& other_pos : other_positions) {
        glm::vec3 diff = self_pos - other_pos;
        float dist_sq = glm::dot(diff, diff);
        if (dist_sq > 0.0001f && dist_sq < max_distance * max_distance) {
            total_force += (diff / dist_sq) * separation_force;
        }
    }
    return total_force;
}

float AberrantAI::calculate_received_noise_db(
    const glm::vec3& listener_pos,
    const glm::vec3& noise_pos,
    float emitted_db,
    float audible_radius,
    bool has_los)
{
    if (emitted_db <= 0.0f || audible_radius <= 0.0f) {
        return 0.0f;
    }
    float dist = glm::distance(listener_pos, noise_pos);
    if (dist > audible_radius) {
        return 0.0f;
    }
    float falloff = 1.0f - (dist / audible_radius);
    float los_factor = has_los ? 1.0f : 0.80f; // rock strata acoustic transmission factor
    return std::max(0.0f, emitted_db * falloff * los_factor);
}

PerceptionEvaluation AberrantAI::evaluate_hearing(
    AIState current_state,
    const glm::vec3& enemy_pos,
    bool can_see_player,
    const glm::vec3& player_pos,
    const glm::vec3& noise_pos,
    float emitted_db,
    float audible_radius,
    bool has_los)
{
    PerceptionEvaluation result;
    result.new_state = current_state;
    result.target_pos = enemy_pos;

    // Calculate noise arriving at enemy location
    float received_db = calculate_received_noise_db(enemy_pos, noise_pos, emitted_db, audible_radius, has_los);
    result.received_noise_db = received_db;

    // Rule 4: If player is spotted directly OR noise exceeds 70 dB -> AIState::PURSUIT with screech and swarm alert
    if (can_see_player || received_db > THRESHOLD_PURSUIT_DB) {
        if (current_state != AIState::PURSUIT && current_state != AIState::DYING && current_state != AIState::DEAD) {
            result.state_changed = true;
            result.new_state = AIState::PURSUIT;
            result.target_pos = can_see_player ? player_pos : noise_pos;
            result.triggered_screech = true;
            result.triggered_swarm_alert = true;
        } else if (current_state == AIState::PURSUIT) {
            result.target_pos = can_see_player ? player_pos : noise_pos;
        }
        return result;
    }

    // Rule 3: When noise reaching an enemy exceeds 30 dB -> transition to AIState::INVESTIGATING toward noise source
    if (received_db > THRESHOLD_INVESTIGATE_DB) {
        if (current_state == AIState::ROOSTING || current_state == AIState::IDLE) {
            result.state_changed = true;
            result.new_state = AIState::INVESTIGATING;
            result.target_pos = noise_pos;
            result.triggered_screech = false;
            result.triggered_swarm_alert = false;
        } else if (current_state == AIState::INVESTIGATING) {
            // Already investigating -> redirect to louder or new noise disturbance
            result.new_state = AIState::INVESTIGATING;
            result.target_pos = noise_pos;
        }
        return result;
    }

    // Otherwise, noise <= 30 dB (e.g. Crouch 0 dB, Walk 15 dB):
    // Stays in current state (e.g. AIState::ROOSTING dormant)
    return result;
}

PerceptionEvaluation AberrantAI::evaluate_acoustic_and_visual_perception(
    AIState current_state,
    const glm::vec3& enemy_pos,
    bool can_see_player,
    const glm::vec3& player_pos,
    const std::vector<AcousticNoiseEvent>& acoustic_events,
    bool has_los)
{
    PerceptionEvaluation best_result;
    best_result.new_state = current_state;
    best_result.target_pos = enemy_pos;

    // Direct visual spot immediately takes top priority: Rule 4
    if (can_see_player) {
        return evaluate_hearing(current_state, enemy_pos, true, player_pos, player_pos, 0.0f, 0.0f, true);
    }

    float max_db = 0.0f;
    const AcousticNoiseEvent* loudest_event = nullptr;

    for (const auto& evt : acoustic_events) {
        if (evt.is_expired()) continue;
        float rx_db = calculate_received_noise_db(enemy_pos, evt.position, evt.db, evt.radius, has_los);
        if (rx_db > max_db) {
            max_db = rx_db;
            loudest_event = &evt;
        }
    }

    if (loudest_event) {
        return evaluate_hearing(
            current_state, enemy_pos, false, player_pos,
            loudest_event->position, loudest_event->db, loudest_event->radius, has_los
        );
    }

    return best_result;
}

} // namespace Voidfall
