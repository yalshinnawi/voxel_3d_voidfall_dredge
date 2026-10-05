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
    float probe_distance)
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

                if (d < min_dist) {
                    min_dist = d;
                    best_normal = face_normal;
                    result.has_contact = true;
                }
                break;
            }
        }
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

} // namespace Voidfall
