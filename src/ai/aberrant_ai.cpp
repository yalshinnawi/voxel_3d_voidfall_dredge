#include "aberrant_ai.hpp"
#include "../voxel/world.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <queue>

namespace Voidfall {

StalkerSurfaceState AberrantAI::classify_normal(const glm::vec3& normal, const glm::vec3& world_up) {
    float dot = glm::dot(normal, world_up);
    if (dot > 0.65f) { // 45-degree slopes (dot ~0.7071) and ramps classify as FLOOR
        return StalkerSurfaceState::FLOOR;
    }
    if (dot < -0.7f) {
        return StalkerSurfaceState::CEILING_CRAWLING;
    }
    return StalkerSurfaceState::WALL_CLIMBING;
}

bool AberrantAI::is_point_solid(const World& world, const glm::vec3& p) {
    int bx = static_cast<int>(std::floor(p.x));
    int by = static_cast<int>(std::floor(p.y));
    int bz = static_cast<int>(std::floor(p.z));
    Voxel v = world.get_voxel(bx, by, bz);
    if (!v.is_solid()) return false;
    VoxelShape s = v.shape();
    if (s == SHAPE_CUBE) return true;
    float u = std::clamp(p.x - static_cast<float>(bx), 0.0f, 1.0f);
    float w = std::clamp(p.z - static_cast<float>(bz), 0.0f, 1.0f);
    float local_y = p.y - static_cast<float>(by);
    float top_h = ShapeGeometry::GetSolidHeightAt(s, u, w);
    float base_h = ShapeGeometry::GetSolidBaseAt(s, u, w);
    return (local_y >= base_h && local_y < top_h);
}

float AberrantAI::get_voxel_surface_y(const World& world, int bx, int by, int bz, float px, float pz) {
    Voxel v = world.get_voxel(bx, by, bz);
    if (!v.is_solid()) return -1000.0f;
    VoxelShape s = v.shape();
    if (s == SHAPE_CUBE || s == SHAPE_SLAB_TOP) {
        return static_cast<float>(by + 1);
    }
    if (s == SHAPE_SLAB_BOTTOM) {
        return static_cast<float>(by) + 0.5f;
    }
    float u = std::clamp(px - static_cast<float>(bx), 0.0f, 1.0f);
    float w = std::clamp(pz - static_cast<float>(bz), 0.0f, 1.0f);
    return static_cast<float>(by) + ShapeGeometry::GetSolidHeightAt(s, u, w);
}

glm::vec3 AberrantAI::get_voxel_surface_normal(const World& world, int bx, int by, int bz) {
    Voxel v = world.get_voxel(bx, by, bz);
    if (!v.is_solid()) return glm::vec3(0.0f, 1.0f, 0.0f);
    VoxelShape s = v.shape();
    constexpr float INV_SQRT2 = 0.7071067811865475f;
    switch (s) {
        case SHAPE_RAMP_EAST:       return glm::vec3(-INV_SQRT2, INV_SQRT2, 0.0f);
        case SHAPE_RAMP_WEST:       return glm::vec3(INV_SQRT2, INV_SQRT2, 0.0f);
        case SHAPE_RAMP_SOUTH:      return glm::vec3(0.0f, INV_SQRT2, -INV_SQRT2);
        case SHAPE_RAMP_NORTH:      return glm::vec3(0.0f, INV_SQRT2, INV_SQRT2);
        case SHAPE_CORNER_OUTER_NE: return glm::normalize(glm::vec3(-INV_SQRT2, INV_SQRT2, INV_SQRT2));
        case SHAPE_CORNER_OUTER_NW: return glm::normalize(glm::vec3(INV_SQRT2, INV_SQRT2, INV_SQRT2));
        case SHAPE_CORNER_OUTER_SE: return glm::normalize(glm::vec3(-INV_SQRT2, INV_SQRT2, -INV_SQRT2));
        case SHAPE_CORNER_OUTER_SW: return glm::normalize(glm::vec3(INV_SQRT2, INV_SQRT2, -INV_SQRT2));
        case SHAPE_CORNER_INNER_NE: return glm::normalize(glm::vec3(-INV_SQRT2, INV_SQRT2, INV_SQRT2));
        case SHAPE_CORNER_INNER_NW: return glm::normalize(glm::vec3(INV_SQRT2, INV_SQRT2, INV_SQRT2));
        case SHAPE_CORNER_INNER_SE: return glm::normalize(glm::vec3(-INV_SQRT2, INV_SQRT2, -INV_SQRT2));
        case SHAPE_CORNER_INNER_SW: return glm::normalize(glm::vec3(INV_SQRT2, INV_SQRT2, -INV_SQRT2));
        default:                    return glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

bool AberrantAI::find_ground_surface(
    const World& world,
    const glm::vec3& pos,
    float max_step_up,
    float max_step_down,
    float* out_surface_y,
    glm::vec3* out_normal)
{
    int bx = static_cast<int>(std::floor(pos.x));
    int bz = static_cast<int>(std::floor(pos.z));
    int top_y = static_cast<int>(std::floor(pos.y + max_step_up));
    int bottom_y = static_cast<int>(std::floor(pos.y - max_step_down));
    top_y = std::clamp(top_y, 0, 31);
    bottom_y = std::clamp(bottom_y, 0, 31);

    for (int y = top_y; y >= bottom_y; --y) {
        Voxel v = world.get_voxel(bx, y, bz);
        if (v.is_solid()) {
            float surf_y = get_voxel_surface_y(world, bx, y, bz, pos.x, pos.z);
            if (surf_y <= pos.y + max_step_up + 0.05f) {
                // Headroom check: ensure standing on this surface has open air
                if (!is_point_solid(world, glm::vec3(pos.x, surf_y + 0.35f, pos.z)) &&
                    !is_point_solid(world, glm::vec3(pos.x, surf_y + 0.80f, pos.z))) {
                    if (out_surface_y) *out_surface_y = surf_y;
                    if (out_normal) *out_normal = get_voxel_surface_normal(world, bx, y, bz);
                    return true;
                }
            }
        }
    }
    return false;
}

SurfaceContactSample AberrantAI::sample_surface_normal(
    const glm::vec3& position,
    const World& world,
    float probe_distance,
    StalkerSurfaceState current_state,
    const glm::vec3* target_pos)
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
    bool has_wall = false;
    float wall_dist = probe_distance + 1.0f;
    glm::vec3 wall_normal(0.0f);

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
                if (dir.y < -0.5f) {
                    float surf_y = get_voxel_surface_y(world, ix, iy, iz, position.x, position.z);
                    if (position.y >= surf_y - 0.20f) {
                        float d = std::max(0.0f, position.y - surf_y);
                        glm::vec3 face_normal = get_voxel_surface_normal(world, ix, iy, iz);
                        has_down = true;
                        down_dist = d;
                        if (d < min_dist) {
                            min_dist = d;
                            best_normal = face_normal;
                            result.has_contact = true;
                        }
                        break;
                    }
                    continue;
                }

                // For horizontal / upward probes, verify point is actually in solid geometry
                if (!is_point_solid(world, p)) {
                    continue;
                }

                // Voxel boundary face normal points back toward entity
                glm::vec3 face_normal = -dir;

                // Calculate exact plane distance to the face
                float d = static_cast<float>(step) * step_size;
                if (dir.y > 0.5f) {
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

                if (i >= 2) { // Side walls
                    if (d < wall_dist) {
                        has_wall = true;
                        wall_dist = d;
                        wall_normal = face_normal;
                    }
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
    // 1. If currently on FLOOR (or transitioning from floor) and floor contact is valid within reachable ground distance (<= 1.45m),
    // prioritize FLOOR over ceiling to prevent flipping in gaps between top and bottom blocks.
    if ((current_state == StalkerSurfaceState::FLOOR || current_state == StalkerSurfaceState::TRANSITIONING) && has_down && down_dist <= 1.45f) {
        if (target_pos != nullptr && (target_pos->y > position.y + 1.5f) && has_wall && wall_dist <= 1.15f) {
            best_normal = wall_normal;
            min_dist = wall_dist;
            result.has_contact = true;
        } else if (target_pos != nullptr && (target_pos->y < position.y - 1.8f) && has_wall && wall_dist <= 1.15f && (!has_down || down_dist > 1.20f)) {
            // When target is below and stalker is next to a descending wall or drop edge, allow wall climbing descent!
            best_normal = wall_normal;
            min_dist = wall_dist;
            result.has_contact = true;
        } else {
            best_normal = glm::vec3(0.0f, 1.0f, 0.0f);
            min_dist = down_dist;
            result.has_contact = true;
        }
    } else if (current_state == StalkerSurfaceState::WALL_CLIMBING) {
        if (has_down && down_dist <= 1.45f && (target_pos == nullptr || target_pos->y <= position.y + 0.8f)) {
            // Smoothly step down onto floor when reaching ground level (only if target is not above)
            best_normal = glm::vec3(0.0f, 1.0f, 0.0f);
            min_dist = down_dist;
            result.has_contact = true;
        } else if (!has_wall && has_down && down_dist <= 2.2f) {
            // If detached from wall in open air above ground, smoothly land onto floor
            best_normal = glm::vec3(0.0f, 1.0f, 0.0f);
            min_dist = down_dist;
            result.has_contact = true;
        }
    } else if (current_state == StalkerSurfaceState::CEILING_CRAWLING) {
        if (has_down && down_dist <= 1.45f) {
            // Drop onto floor if close to ground level (prevents getting trapped ceiling crawling on floor or in low gaps)
            best_normal = glm::vec3(0.0f, 1.0f, 0.0f);
            min_dist = down_dist;
            result.has_contact = true;
        } else if (has_wall && wall_dist <= 0.80f) {
            // Ceiling stalker within reach of vertical wall: transfer to wall climbing to descend toward floor!
            if (target_pos == nullptr || target_pos->y < position.y - 1.2f) {
                best_normal = wall_normal;
                min_dist = wall_dist;
                result.has_contact = true;
            }
        }
    }

    if (result.has_contact) {
        result.contact_normal = glm::normalize(best_normal);
        result.distance = min_dist;
        result.surface_state = classify_normal(result.contact_normal);
    } else {
        result.contact_normal = (current_state == StalkerSurfaceState::CEILING_CRAWLING) ? glm::vec3(0.0f, -1.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        result.distance = probe_distance;
        result.surface_state = current_state;
    }

    return result;
}

glm::quat AberrantAI::calculate_orientation(
    const glm::vec3& velocity,
    const glm::vec3& contact_normal,
    const glm::vec3& fallback_forward)
{
    glm::vec3 n = (glm::length(contact_normal) > 0.001f && !std::isnan(contact_normal.x))
        ? glm::normalize(contact_normal)
        : glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 fwd_candidate = (!std::isnan(velocity.x) && glm::length(velocity) > 0.001f)
        ? velocity
        : fallback_forward;

    // Anti-jitter: If fallback_forward is valid and velocity is moving backward (e.g. knockback/recoil from weapon fire),
    // keep facing the forward direction instead of flipping 180 degrees.
    if (glm::length(fallback_forward) > 0.01f && !std::isnan(fallback_forward.x)) {
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

    if (glm::length(fwd_proj) < 0.01f || std::isnan(fwd_proj.x)) {
        // Fall back to projected previous facing direction
        if (glm::length(fallback_forward) > 0.01f && !std::isnan(fallback_forward.x)) {
            fwd_proj = fallback_forward - glm::dot(fallback_forward, n) * n;
        }
    }

    if (glm::length(fwd_proj) < 0.01f || std::isnan(fwd_proj.x)) {
        // Choose arbitrary tangent perpendicular to n
        glm::vec3 temp = (std::abs(n.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        fwd_proj = glm::cross(n, temp);
    }

    glm::vec3 forward = (glm::length(fwd_proj) > 0.001f && !std::isnan(fwd_proj.x))
        ? glm::normalize(fwd_proj)
        : glm::vec3(0.0f, 0.0f, 1.0f);
    // Right-handed basis: cross(n, forward) yields the +X vector in model space
    glm::vec3 cross_rf = glm::cross(n, forward);
    glm::vec3 right = (glm::length(cross_rf) > 0.001f && !std::isnan(cross_rf.x))
        ? glm::normalize(cross_rf)
        : glm::vec3(1.0f, 0.0f, 0.0f);

    // Construct orthonormal rotation matrix
    glm::mat3 rot_mat;
    rot_mat[0] = right;
    rot_mat[1] = n;
    rot_mat[2] = forward;

    glm::quat q = glm::quat_cast(rot_mat);
    if (std::isnan(q.x) || std::isnan(q.y) || std::isnan(q.z) || std::isnan(q.w) || glm::length(q) < 0.001f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    return glm::normalize(q);
}

glm::quat AberrantAI::slerp_rotation(
    const glm::quat& current,
    const glm::quat& target,
    float dt,
    float speed)
{
    if (std::isnan(current.x) || std::isnan(current.y) || std::isnan(current.z) || std::isnan(current.w) || glm::length(current) < 0.001f) {
        return target;
    }
    if (std::isnan(target.x) || std::isnan(target.y) || std::isnan(target.z) || std::isnan(target.w) || glm::length(target) < 0.001f) {
        return current;
    }

    glm::quat t = target;
    // Shortest path slerp
    if (glm::dot(current, t) < 0.0f) {
        t = -t;
    }
    float factor = std::clamp(dt * speed, 0.0f, 1.0f);
    glm::quat result = glm::slerp(current, t, factor);
    if (std::isnan(result.x) || std::isnan(result.y) || std::isnan(result.z) || std::isnan(result.w) || glm::length(result) < 0.001f) {
        return target;
    }
    return glm::normalize(result);
}

glm::vec3 AberrantAI::compute_surface_snapping_offset(
    StalkerSurfaceState state,
    const glm::vec3& normal)
{
    glm::vec3 n = (glm::length(normal) > 0.001f && !std::isnan(normal.x))
        ? glm::normalize(normal)
        : glm::vec3(0.0f, 1.0f, 0.0f);

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
    float player_radius,
    glm::vec3* enemy_vel)
{
    // True 3D Euclidean sphere standoff (including Y elevation, not just XZ plane!) of 2.35m
    glm::vec3 delta = enemy_pos - player_pos;
    float dist = glm::length(delta);
    float minRadius = std::max(2.35f, enemy_radius + player_radius);
    if (dist < minRadius) {
        glm::vec3 pushDir = (dist > 0.001f) ? (delta / dist) : glm::vec3(0.0f, 0.0f, 1.0f);
        float penetration = minRadius - dist;
        enemy_pos += pushDir * penetration; // Push enemy back along true 3D vector
        if (enemy_vel) {
            float inwardSpeed = glm::dot(*enemy_vel, -pushDir);
            if (inwardSpeed > 0.0f) {
                *enemy_vel += pushDir * inwardSpeed; // Zero out inward closing velocity
            }
        }
        return true;
    }
    return false;
}

bool AberrantAI::resolve_enemy_to_enemy_penetration(
    glm::vec3& pos_a,
    glm::vec3& vel_a,
    float radius_a,
    float weight_a,
    glm::vec3& pos_b,
    glm::vec3& vel_b,
    float radius_b,
    float weight_b,
    uint32_t id_a,
    uint32_t id_b)
{
    glm::vec3 delta = pos_a - pos_b;
    float vert_diff = std::abs(delta.y);
    if (vert_diff > 2.0f) {
        return false;
    }

    float horiz_dist_sq = delta.x * delta.x + delta.z * delta.z;
    float min_radius = radius_a + radius_b;

    if (horiz_dist_sq >= min_radius * min_radius) {
        return false;
    }

    float horiz_dist = std::sqrt(horiz_dist_sq);
    glm::vec3 push_dir(0.0f);
    float dist = horiz_dist;

    if (horiz_dist > 0.001f) {
        push_dir = glm::vec3(delta.x / horiz_dist, 0.0f, delta.z / horiz_dist);
    } else {
        // Exactly coincident horizontal coordinates! Generate deterministic non-zero separation angle
        float angle = static_cast<float>((id_a * 2654435761u ^ id_b * 2246822519u) % 6283) * 0.001f;
        push_dir = glm::vec3(std::cos(angle), 0.0f, std::sin(angle));
        dist = 0.0f;
    }

    float penetration = min_radius - dist;
    float total_weight = weight_a + weight_b;
    if (total_weight < 0.001f) total_weight = 2.0f;
    float frac_a = weight_b / total_weight;
    float frac_b = weight_a / total_weight;

    pos_a += push_dir * (penetration * frac_a);
    pos_b -= push_dir * (penetration * frac_b);

    // Tangential slip / crowd gliding:
    // Instead of completely freezing crowded entities, deflect velocity tangentially around each other
    glm::vec3 tangent(-push_dir.z, 0.0f, push_dir.x);
    float v_rel = glm::dot(vel_a - vel_b, push_dir);
    if (v_rel < 0.0f) {
        vel_a -= push_dir * (v_rel * frac_a);
        vel_b += push_dir * (v_rel * frac_b);

        // Add tangential circulation force so crowded enemies slide past each other smoothly
        float slip_speed = std::min(std::abs(v_rel) * 0.5f + 0.8f, 2.2f);
        float sign = ((id_a ^ id_b) & 1) ? 1.0f : -1.0f;
        vel_a += tangent * (slip_speed * sign * frac_a);
        vel_b -= tangent * (slip_speed * sign * frac_b);
    }
    return true;
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

AISpatialHash::AISpatialHash() {
    clear();
}

void AISpatialHash::clear() {
    std::fill_n(m_head, BUCKET_COUNT, -1);
    m_count = 0;
}

void AISpatialHash::insert(uint32_t id, const glm::vec3& position) {
    if (m_count >= MAX_ENTITIES) return;
    int idx = m_count++;
    m_entries[idx] = {id, position};
    int cx = static_cast<int>(std::floor(position.x * CELL_SIZE_INV));
    int cy = static_cast<int>(std::floor(position.y * CELL_SIZE_INV));
    int cz = static_cast<int>(std::floor(position.z * CELL_SIZE_INV));
    uint32_t h = hash_cell(cx, cy, cz);
    m_next[idx] = m_head[h];
    m_head[h] = idx;
}

glm::vec3 AISpatialHash::calculate_separation(
    uint32_t self_id,
    const glm::vec3& self_pos,
    float max_dist_sq,
    float separation_force) const
{
    int cx = static_cast<int>(std::floor(self_pos.x * CELL_SIZE_INV));
    int cy = static_cast<int>(std::floor(self_pos.y * CELL_SIZE_INV));
    int cz = static_cast<int>(std::floor(self_pos.z * CELL_SIZE_INV));

    glm::vec3 total_force(0.0f);

    for (int dz_cell = -1; dz_cell <= 1; ++dz_cell) {
        for (int dy_cell = -1; dy_cell <= 1; ++dy_cell) {
            for (int dx_cell = -1; dx_cell <= 1; ++dx_cell) {
                uint32_t h = hash_cell(cx + dx_cell, cy + dy_cell, cz + dz_cell);
                for (int idx = m_head[h]; idx != -1; idx = m_next[idx]) {
                    const auto& ent = m_entries[idx];
                    if (ent.id == self_id) continue;
                    float dx = self_pos.x - ent.position.x;
                    float dy = self_pos.y - ent.position.y;
                    float dz = self_pos.z - ent.position.z;
                    // Early-exit distance check: dx*dx + dy*dy + dz*dz > 16.0f skips separation math immediately
                    float dist_sq = dx * dx + dy * dy + dz * dz;
                    if (dist_sq > 16.0f || dist_sq > max_dist_sq || dist_sq <= 0.0001f) {
                        continue;
                    }
                    float safe_dist_sq = std::max(dist_sq, 0.36f);
                    float d = std::sqrt(dist_sq);
                    glm::vec3 sep_dir = glm::vec3(dx, dy, dz) / d;
                    total_force += (sep_dir / safe_dist_sq) * separation_force;

                    // Flocking tangential bypass: within 2.0m, add perpendicular lateral force to flow around peers
                    if (dist_sq < 4.0f) {
                        glm::vec3 tangent(-sep_dir.z, 0.0f, sep_dir.x);
                        float sign = ((self_id ^ ent.id) & 1) ? 1.0f : -1.0f;
                        total_force += tangent * (separation_force * 0.35f * sign);
                    }
                }
            }
        }
    }
    return total_force;
}

glm::vec3 AberrantAI::calculate_swarm_separation(
    const glm::vec3& self_pos,
    const std::vector<glm::vec3>& other_positions,
    float max_distance,
    float separation_force)
{
    glm::vec3 total_force(0.0f);
    const float max_dist_sq = std::min(16.0f, max_distance * max_distance);
    for (const auto& other_pos : other_positions) {
        float dx = self_pos.x - other_pos.x;
        float dy = self_pos.y - other_pos.y;
        float dz = self_pos.z - other_pos.z;
        // Early-exit distance check: dx*dx + dy*dy + dz*dz > 16.0f skips separation math immediately
        float dist_sq = dx * dx + dy * dy + dz * dz;
        if (dist_sq > 16.0f || dist_sq > max_dist_sq || dist_sq <= 0.0001f) {
            continue;
        }
        float safe_dist_sq = std::max(dist_sq, 0.36f);
        float d = std::sqrt(dist_sq);
        glm::vec3 sep_dir = glm::vec3(dx, dy, dz) / d;
        total_force += (sep_dir / safe_dist_sq) * separation_force;

        // Flocking tangential bypass: within 2.0m, add perpendicular lateral force
        if (dist_sq < 4.0f) {
            glm::vec3 tangent(-sep_dir.z, 0.0f, sep_dir.x);
            total_force += tangent * (separation_force * 0.35f);
        }
    }
    return total_force;
}

glm::vec3 AberrantAI::calculate_swarm_separation(
    const AISpatialHash& grid,
    uint32_t self_id,
    const glm::vec3& self_pos,
    float max_dist_sq,
    float separation_force)
{
    return grid.calculate_separation(self_id, self_pos, max_dist_sq, separation_force);
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

    // Direct visual contact -> AIState::PURSUIT with screech and swarm alert
    if (can_see_player) {
        if (current_state != AIState::PURSUIT && current_state != AIState::DYING && current_state != AIState::DEAD) {
            result.state_changed = true;
            result.new_state = AIState::PURSUIT;
            result.target_pos = player_pos;
            result.triggered_screech = true;
            result.triggered_swarm_alert = true;
        } else if (current_state == AIState::PURSUIT) {
            result.target_pos = player_pos;
        }
        return result;
    }

    // Acoustic disturbance > 30 dB -> transition to AIState::INVESTIGATING toward noise source (curious investigation)
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

bool AberrantAI::is_in_vision_cone(
    const glm::vec3& observer_pos,
    const glm::vec3& observer_fwd,
    const glm::vec3& target_pos,
    float max_angle_rad)
{
    glm::vec3 to_target = target_pos - observer_pos;
    float dist = glm::length(to_target);
    if (dist < 0.001f) {
        return true;
    }
    glm::vec3 dir_to_target = to_target / dist;
    glm::vec3 fwd = (glm::length(observer_fwd) > 0.001f) ? glm::normalize(observer_fwd) : glm::vec3(0.0f, 0.0f, 1.0f);
    float cos_angle = glm::dot(fwd, dir_to_target);
    return cos_angle >= std::cos(max_angle_rad);
}

glm::vec3 AberrantAI::find_descending_wall_direction(
    const glm::vec3& enemy_pos,
    const glm::vec3& target_pos,
    const World& world,
    float max_probe_dist)
{
    constexpr int NUM_DIRS = 24;
    float best_cost = 1e9f;
    glm::vec3 best_dir(0.0f);
    bool found_wall = false;

    int py = static_cast<int>(std::floor(enemy_pos.y));

    for (int i = 0; i < NUM_DIRS; ++i) {
        float angle = static_cast<float>(i) * (2.0f * glm::pi<float>() / static_cast<float>(NUM_DIRS));
        glm::vec3 dir(std::cos(angle), 0.0f, std::sin(angle));

        float hit_dist = max_probe_dist;
        bool hit = false;
        const float step_size = 0.4f;
        int max_steps = static_cast<int>(max_probe_dist / step_size);

        for (int s = 1; s <= max_steps; ++s) {
            glm::vec3 probe_pos = enemy_pos + dir * (static_cast<float>(s) * step_size);
            int px = static_cast<int>(std::floor(probe_pos.x));
            int pz = static_cast<int>(std::floor(probe_pos.z));

            // World boundary clamp
            if (px <= 4 || px >= 68 || pz <= 4 || pz >= 68) {
                hit_dist = static_cast<float>(s) * step_size;
                hit = true;
                break;
            }

            // Check wall obstacles at enemy crawl level and below (py, py - 1).
            // Do NOT check py + 1, which is the solid ceiling right above the monster!
            // Only consider py - 1 if the space directly underneath the enemy position has drop clearance,
            // preventing low gap / tunnel floors from being misclassified as walls.
            int e_x = static_cast<int>(std::floor(enemy_pos.x));
            int e_z = static_cast<int>(std::floor(enemy_pos.z));
            bool has_drop = (py - 1 >= 0) && !world.is_solid(glm::ivec3(e_x, py - 1, e_z));
            bool solid_at_level = world.is_solid(glm::ivec3(px, py, pz));
            bool solid_below = has_drop && (py - 1 >= 0) && world.is_solid(glm::ivec3(px, py - 1, pz));
            if (solid_at_level || solid_below) {
                hit_dist = static_cast<float>(s) * step_size;
                hit = true;
                break;
            }
        }

        if (hit) {
            glm::vec3 wall_pos = enemy_pos + dir * hit_dist;
            float d_to_wall = hit_dist;
            float d_wall_to_tgt = glm::distance(glm::vec2(wall_pos.x, wall_pos.z), glm::vec2(target_pos.x, target_pos.z));
            
            // Total cost: travel across ceiling to wall + distance from wall base to target
            float cost = d_to_wall * 1.0f + d_wall_to_tgt * 1.25f;

            if (cost < best_cost) {
                best_cost = cost;
                best_dir = dir;
                found_wall = true;
            }
        }
    }

    if (found_wall && glm::length(best_dir) > 0.001f) {
        return glm::normalize(best_dir);
    }

    glm::vec3 to_tgt_xz(target_pos.x - enemy_pos.x, 0.0f, target_pos.z - enemy_pos.z);
    if (glm::length(to_tgt_xz) > 0.1f) {
        return glm::normalize(to_tgt_xz);
    }
    return glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 AberrantAI::find_ascending_wall_direction(
    const glm::vec3& enemy_pos,
    const glm::vec3& target_pos,
    const World& world,
    float max_probe_dist)
{
    constexpr int NUM_DIRS = 24;
    float best_cost = 1e9f;
    glm::vec3 best_dir(0.0f);
    bool found_wall = false;

    int py = static_cast<int>(std::floor(enemy_pos.y));

    for (int i = 0; i < NUM_DIRS; ++i) {
        float angle = static_cast<float>(i) * (2.0f * glm::pi<float>() / static_cast<float>(NUM_DIRS));
        glm::vec3 dir(std::cos(angle), 0.0f, std::sin(angle));

        float hit_dist = max_probe_dist;
        bool hit = false;
        const float step_size = 0.4f;
        int max_steps = static_cast<int>(max_probe_dist / step_size);

        for (int s = 1; s <= max_steps; ++s) {
            glm::vec3 probe_pos = enemy_pos + dir * (static_cast<float>(s) * step_size);
            int px = static_cast<int>(std::floor(probe_pos.x));
            int pz = static_cast<int>(std::floor(probe_pos.z));

            // World boundary clamp (perimeter rock faces are climbable)
            if (px <= 4 || px >= 68 || pz <= 4 || pz >= 68) {
                hit_dist = static_cast<float>(s) * step_size;
                hit = true;
                break;
            }

            // Check if there is a vertical wall extending upward above floor level
            bool solid_at_level = world.is_solid(glm::ivec3(px, py, pz));
            bool solid_above_1  = (py + 1 < 32) && world.is_solid(glm::ivec3(px, py + 1, pz));
            bool solid_above_2  = (py + 2 < 32) && world.is_solid(glm::ivec3(px, py + 2, pz));

            if ((solid_at_level && solid_above_1) || (solid_above_1 && solid_above_2)) {
                hit_dist = static_cast<float>(s) * step_size;
                hit = true;
                break;
            }
        }

        if (hit) {
            glm::vec3 wall_pos = enemy_pos + dir * hit_dist;
            float d_to_wall = hit_dist;
            float d_wall_to_tgt = glm::distance(glm::vec2(wall_pos.x, wall_pos.z), glm::vec2(target_pos.x, target_pos.z));

            // Total cost: travel across floor to wall + distance from wall to elevated target
            float cost = d_to_wall * 1.0f + d_wall_to_tgt * 1.30f;

            if (cost < best_cost) {
                best_cost = cost;
                best_dir = dir;
                found_wall = true;
            }
        }
    }

    if (found_wall && glm::length(best_dir) > 0.001f) {
        return glm::normalize(best_dir);
    }

    glm::vec3 to_tgt_xz(target_pos.x - enemy_pos.x, 0.0f, target_pos.z - enemy_pos.z);
    if (glm::length(to_tgt_xz) > 0.1f) {
        return glm::normalize(to_tgt_xz);
    }
    return glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 AberrantAI::calculate_wall_traversal_direction(
    const glm::vec3& enemy_pos,
    const glm::vec3& target_pos,
    const glm::vec3& contact_normal)
{
    glm::vec3 n = glm::length(contact_normal) > 0.001f
        ? glm::normalize(contact_normal)
        : glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 to_tgt = target_pos - enemy_pos;
    glm::vec3 v_wall = to_tgt - glm::dot(to_tgt, n) * n;

    // Weight vertical descent/ascent when target is at a significantly different elevation
    if (target_pos.y < enemy_pos.y - 0.4f && v_wall.y < 0.0f) {
        float vert_drop = enemy_pos.y - target_pos.y;
        v_wall.y *= std::max(2.2f, 1.0f + vert_drop * 0.8f);
    } else if (target_pos.y > enemy_pos.y + 0.4f && v_wall.y > 0.0f) {
        float vert_rise = target_pos.y - enemy_pos.y;
        v_wall.y *= std::max(2.2f, 1.0f + vert_rise * 0.8f);
    }

    if (glm::length(v_wall) > 0.05f) {
        return glm::normalize(v_wall);
    }

    glm::vec3 down_wall = glm::vec3(0.0f, -1.0f, 0.0f) - glm::dot(glm::vec3(0.0f, -1.0f, 0.0f), n) * n;
    if (glm::length(down_wall) > 0.05f) {
        return glm::normalize(down_wall);
    }
    return glm::vec3(0.0f, -1.0f, 0.0f);
}

bool AberrantAI::is_point_lethal_hazard(
    const World& world,
    const glm::vec3& pos,
    float ground_probe_depth)
{
    int cx = static_cast<int>(std::floor(pos.x));
    int cy = static_cast<int>(std::floor(pos.y));
    int cz = static_cast<int>(std::floor(pos.z));
    Voxel v = world.get_voxel(cx, cy, cz);
    if (IsLethalHazard(v.material_id)) {
        return true;
    }

    // Check ground surface directly beneath pos
    float ground_y = 0.0f;
    if (find_ground_surface(world, pos, 0.5f, ground_probe_depth, &ground_y)) {
        int gx = static_cast<int>(std::floor(pos.x));
        int gy = static_cast<int>(std::floor(ground_y - 0.1f));
        int gz = static_cast<int>(std::floor(pos.z));
        Voxel v_ground = world.get_voxel(gx, gy, gz);
        if (IsLethalHazard(v_ground.material_id)) {
            return true;
        }
        int g_above_y = static_cast<int>(std::floor(ground_y + 0.1f));
        Voxel v_surf = world.get_voxel(gx, g_above_y, gz);
        if (IsLethalHazard(v_surf.material_id)) {
            return true;
        }
    }
    return false;
}

bool AberrantAI::has_hazard_between(
    const glm::vec3& from,
    const glm::vec3& to,
    const World& world,
    float step_size)
{
    glm::vec3 diff = to - from;
    float dist = glm::length(diff);
    if (dist < 0.1f) return false;
    glm::vec3 dir = diff / dist;

    for (float d = step_size; d < dist - 0.2f; d += step_size) {
        glm::vec3 pt = from + dir * d;
        if (is_point_lethal_hazard(world, pt, 2.0f)) {
            return true;
        }
    }
    return false;
}

glm::vec3 AberrantAI::steer_around_obstacles(
    const glm::vec3& enemy_pos,
    const glm::vec3& desired_dir,
    const glm::vec3& up_normal,
    const World& world,
    float probe_dist)
{
    if (glm::length(desired_dir) < 0.001f) return desired_dir;
    glm::vec3 fwd = glm::normalize(desired_dir);
    glm::vec3 up = glm::length(up_normal) > 0.001f ? glm::normalize(up_normal) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(up, fwd);
    if (glm::length(right) < 0.001f) return fwd;
    right = glm::normalize(right);

    glm::vec3 probe_origin = enemy_pos + up * 0.35f;

    auto is_blocked = [&](const glm::vec3& dir, float dist) -> bool {
        for (float d = 0.4f; d <= dist; d += 0.4f) {
            glm::vec3 pt = probe_origin + dir * d;
            if (is_point_solid(world, pt)) {
                // If on floor and the space directly above is open, this is a clamberable step / ramp / slab!
                if (up.y > 0.65f) {
                    int cx = static_cast<int>(std::floor(pt.x));
                    int cy = static_cast<int>(std::floor(pt.y));
                    int cz = static_cast<int>(std::floor(pt.z));
                    float s_y = get_voxel_surface_y(world, cx, cy, cz, pt.x, pt.z);
                    if (s_y - enemy_pos.y <= 1.35f) {
                        if (!is_point_solid(world, glm::vec3(pt.x, s_y + 0.35f, pt.z)) &&
                            !is_point_solid(world, glm::vec3(pt.x, s_y + 0.85f, pt.z))) {
                            // Check if this step is a lethal hazard (e.g. lava/acid slab or spikes)!
                            int sx = static_cast<int>(std::floor(pt.x));
                            int sy = static_cast<int>(std::floor(s_y - 0.1f));
                            int sz = static_cast<int>(std::floor(pt.z));
                            Voxel step_v = world.get_voxel(sx, sy, sz);
                            if (IsLethalHazard(step_v.material_id)) {
                                return true; // Fear clambering onto hazardous ledge
                            }
                            continue;
                        }
                    }
                } else if (std::abs(up.y) <= 0.65f && dir.y < -0.2f) {
                    // Descending wall climber probing toward floor: check if this is an open floor surface
                    int cx = static_cast<int>(std::floor(pt.x));
                    int cy = static_cast<int>(std::floor(pt.y));
                    int cz = static_cast<int>(std::floor(pt.z));
                    float s_y = get_voxel_surface_y(world, cx, cy, cz, pt.x, pt.z);
                    if (!is_point_solid(world, glm::vec3(pt.x, s_y + 0.35f, pt.z)) &&
                        !is_point_solid(world, glm::vec3(pt.x, s_y + 0.85f, pt.z))) {
                        Voxel floor_v = world.get_voxel(cx, cy, cz);
                        if (!IsLethalHazard(floor_v.material_id)) {
                            continue; // Valid floor landing zone, not an obstacle!
                        }
                    }
                }
                return true;
            }

            // Hazard fear check: point directly occupied by lethal hazard (lava, acid, spikes)
            int cx = static_cast<int>(std::floor(pt.x));
            int cy = static_cast<int>(std::floor(pt.y));
            int cz = static_cast<int>(std::floor(pt.z));
            Voxel v_at = world.get_voxel(cx, cy, cz);
            if (IsLethalHazard(v_at.material_id)) {
                return true; // Fears stepping into lethal fluid or spikes
            }

            // Check if walking on floor and ground beneath pt is a lethal hazard (e.g. stepping onto lava/acid/spikes)
            if (up.y > 0.65f) {
                float ground_y = 0.0f;
                if (find_ground_surface(world, pt, 1.35f, 2.0f, &ground_y)) {
                    int gx = static_cast<int>(std::floor(pt.x));
                    int gy = static_cast<int>(std::floor(ground_y - 0.1f));
                    int gz = static_cast<int>(std::floor(pt.z));
                    Voxel v_ground = world.get_voxel(gx, gy, gz);
                    if (IsLethalHazard(v_ground.material_id)) {
                        return true; // Fears walking across lava pit, acid pool, or spike trench!
                    }
                    int g_above_y = static_cast<int>(std::floor(ground_y + 0.1f));
                    Voxel v_surf = world.get_voxel(gx, g_above_y, gz);
                    if (IsLethalHazard(v_surf.material_id)) {
                        return true;
                    }
                }
            }

            // Top block / headroom check on floor: prevent entering a gap where head would hit top block
            if (up.y > 0.65f) {
                glm::vec3 pt_head = pt + up * 0.45f;
                if (is_point_solid(world, pt_head)) {
                    int cx = static_cast<int>(std::floor(pt_head.x));
                    int cy = static_cast<int>(std::floor(pt_head.y));
                    int cz = static_cast<int>(std::floor(pt_head.z));
                    float s_y = get_voxel_surface_y(world, cx, cy - 1, cz, pt_head.x, pt_head.z);
                    float ceil_bottom = static_cast<float>(cy);
                    // If vertical gap between ground and top block is under 0.70m, path is blocked
                    if (ceil_bottom - s_y < 0.70f) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    if (!is_blocked(fwd, probe_dist)) {
        return fwd;
    }

    // Direct path blocked! Check whiskered deviation angles left, right, and reverse
    const float angles[] = { 0.52f, -0.52f, 0.96f, -0.96f, 1.40f, -1.40f, 1.92f, -1.92f, 2.50f, -2.50f, 3.1415f };
    for (float ang : angles) {
        glm::vec3 test_dir = glm::normalize(fwd * std::cos(ang) + right * std::sin(ang));
        if (!is_blocked(test_dir, probe_dist * 0.9f)) {
            return test_dir;
        }
    }

    // Try shorter probe distance for tight corner turns
    for (float ang : angles) {
        glm::vec3 test_dir = glm::normalize(fwd * std::cos(ang) + right * std::sin(ang));
        if (!is_blocked(test_dir, probe_dist * 0.45f)) {
            return test_dir;
        }
    }

    // Completely obstructed in forward hemisphere: reverse away from dead-end
    return -fwd;
}

bool AberrantAI::resolve_voxel_collision(
    glm::vec3& pos,
    glm::vec3& velocity,
    float radius,
    float height,
    StalkerSurfaceState surface_state,
    const glm::vec3& contact_normal,
    const World& world)
{
    bool collided = false;

    // Sector bounds clamp
    pos.x = std::clamp(pos.x, 4.5f + radius, 67.5f - radius);
    pos.y = std::clamp(pos.y, 0.5f, 31.0f);
    pos.z = std::clamp(pos.z, 4.5f + radius, 67.5f - radius);

    int cx = static_cast<int>(std::floor(pos.x));
    int cy = static_cast<int>(std::floor(pos.y));
    int cz = static_cast<int>(std::floor(pos.z));

    // 0. GROUND SURFACE CONFORMING (FLOOR / TRANSITIONING):
    // Find walkable surface directly underneath or stepped onto by the entity (ramps, slabs, stairs)
    if (surface_state == StalkerSurfaceState::FLOOR || surface_state == StalkerSurfaceState::TRANSITIONING) {
        float ground_y = 0.0f;
        glm::vec3 ground_n(0.0f, 1.0f, 0.0f);
        if (find_ground_surface(world, pos, 1.35f, 1.5f, &ground_y, &ground_n)) {
            if (pos.y < ground_y) {
                pos.y = ground_y;
            } else if (pos.y <= ground_y + 1.25f && velocity.y <= 0.2f) {
                // Smooth slope, ramp, stair, and half-block step-down hugging
                pos.y = ground_y;
            }
        } else if (is_point_solid(world, pos) || is_point_solid(world, pos + glm::vec3(0.0f, 0.20f, 0.0f))) {
            // Failsafe: entity center embedded inside solid block without ground surface below - pop to top
            int cur_y = static_cast<int>(std::floor(pos.y));
            pos.y = static_cast<float>(cur_y + 1.0f);
        }

        // 0b. TOP BLOCK & CEILING HEADROOM CLAMP:
        // Prevent monster head from penetrating overhead top blocks or low ceiling slabs in gaps
        int head_vox_y = static_cast<int>(std::floor(pos.y + height));
        int cur_vox_y = static_cast<int>(std::floor(pos.y));
        for (int ty = cur_vox_y + 1; ty <= head_vox_y + 1; ++ty) {
            if (ty < 32 && world.is_solid(glm::ivec3(cx, ty, cz))) {
                float ceil_bottom_y = static_cast<float>(ty);
                if (pos.y + height >= ceil_bottom_y - 0.25f) {
                    if (velocity.y > 0.0f) velocity.y = 0.0f;
                }
                if (pos.y + height > ceil_bottom_y) {
                    collided = true;
                    float max_allowed_y = ceil_bottom_y - height;
                    if (pos.y > max_allowed_y) {
                        pos.y = max_allowed_y;
                    }
                }
                break;
            }
        }
    }

    // 1. KINEMATIC SURFACE & HORIZONTAL VOXEL COLLISION
    // Multi-point radial offsets: 4 cardinal + 4 diagonal
    const float d_r = radius * 0.7071f;
    const glm::vec2 offsets[8] = {
        glm::vec2( radius,  0.0f),
        glm::vec2(-radius,  0.0f),
        glm::vec2( 0.0f,    radius),
        glm::vec2( 0.0f,   -radius),
        glm::vec2( d_r,     d_r),
        glm::vec2(-d_r,     d_r),
        glm::vec2( d_r,    -d_r),
        glm::vec2(-d_r,    -d_r)
    };

    if (surface_state == StalkerSurfaceState::FLOOR || surface_state == StalkerSurfaceState::TRANSITIONING) {
        // Test horizontal perimeter at feet level and upper body/head level
        const float check_heights[2] = { 0.25f, std::min(height * 0.80f, 0.75f) };
        for (const auto& off : offsets) {
            for (float check_h : check_heights) {
                glm::vec3 check_pos = pos + glm::vec3(off.x, check_h, off.y);
                if (is_point_solid(world, check_pos)) {
                    int ix = static_cast<int>(std::floor(check_pos.x));
                    int iy = static_cast<int>(std::floor(check_pos.y));
                    int iz = static_cast<int>(std::floor(check_pos.z));

                    // Step clambering check:
                    // Only clamber upward if the obstacle probe is in the forward direction of motion!
                    // Probes behind or perpendicular must never pop pos.y upward to a higher block behind the entity.
                    glm::vec2 vel_xz(velocity.x, velocity.z);
                    float vel_len = glm::length(vel_xz);
                    bool is_forward_probe = (vel_len <= 0.05f) || (glm::dot(off, vel_xz) >= -0.05f * vel_len);

                    float step_surf_y = -1000.0f;
                    bool can_step = false;
                    for (int ty = iy + 1; ty >= iy - 1; --ty) {
                        Voxel tv = world.get_voxel(ix, ty, iz);
                        if (tv.is_solid()) {
                            float s_y = get_voxel_surface_y(world, ix, ty, iz, check_pos.x, check_pos.z);
                            float step_h = s_y - pos.y;
                            if (step_h > 0.0f && step_h <= 1.25f) {
                                if (!is_point_solid(world, glm::vec3(check_pos.x, s_y + 0.35f, check_pos.z)) &&
                                    !is_point_solid(world, glm::vec3(check_pos.x, s_y + height * 0.75f, check_pos.z))) {
                                    step_surf_y = s_y;
                                    can_step = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (can_step && is_forward_probe) {
                        pos.y = std::max(pos.y, step_surf_y);
                        continue; // Stepped onto block in front; do not treat as blocking wall!
                    }
                    if (can_step && !is_forward_probe) {
                        // Solid step is BEHIND or to the side of entity moving away from it: ignore to allow descent!
                        continue;
                    }

                    // Solid wall / top block: smooth closest-point AABB depenetration
                    collided = true;
                    float cx = std::clamp(pos.x, static_cast<float>(ix), static_cast<float>(ix + 1.0f));
                    float cz = std::clamp(pos.z, static_cast<float>(iz), static_cast<float>(iz + 1.0f));
                    float diff_x = pos.x - cx;
                    float diff_z = pos.z - cz;
                    float dist_sq = diff_x * diff_x + diff_z * diff_z;

                    if (dist_sq < radius * radius) {
                        float dist = std::sqrt(dist_sq);
                        if (dist > 0.0001f) {
                            float overlap = (radius + 0.005f) - dist;
                            glm::vec2 normal(diff_x / dist, diff_z / dist);
                            pos.x += normal.x * overlap;
                            pos.z += normal.y * overlap;
                            float vn = velocity.x * normal.x + velocity.z * normal.y;
                            if (vn < 0.0f) {
                                velocity.x -= vn * normal.x;
                                velocity.z -= vn * normal.y;
                            }
                        } else {
                            // Center penetrated inside voxel: push to closest boundary face
                            float d_left = pos.x - static_cast<float>(ix);
                            float d_right = static_cast<float>(ix + 1.0f) - pos.x;
                            float d_bottom = pos.z - static_cast<float>(iz);
                            float d_top = static_cast<float>(iz + 1.0f) - pos.z;
                            float min_d = std::min({d_left, d_right, d_bottom, d_top});
                            if (min_d == d_left) {
                                pos.x = static_cast<float>(ix) - radius - 0.005f;
                                if (velocity.x > 0.0f) velocity.x = 0.0f;
                            } else if (min_d == d_right) {
                                pos.x = static_cast<float>(ix + 1.0f) + radius + 0.005f;
                                if (velocity.x < 0.0f) velocity.x = 0.0f;
                            } else if (min_d == d_bottom) {
                                pos.z = static_cast<float>(iz) - radius - 0.005f;
                                if (velocity.z > 0.0f) velocity.z = 0.0f;
                            } else {
                                pos.z = static_cast<float>(iz + 1.0f) + radius + 0.005f;
                                if (velocity.z < 0.0f) velocity.z = 0.0f;
                            }
                        }
                    }
                }
            }
        }
    } else if (surface_state == StalkerSurfaceState::WALL_CLIMBING) {
        glm::vec3 n = glm::length(contact_normal) > 0.001f ? glm::normalize(contact_normal) : glm::vec3(0, 1, 0);
        float dot_v = glm::dot(velocity, n);
        if (dot_v < 0.0f) {
            velocity -= dot_v * n;
        }

        // Floor / ground contact check while climbing down:
        // Prevent burrowing into floor blocks beneath wall and ensure seamless floor arrival
        float floor_y = 0.0f;
        glm::vec3 floor_n(0.0f, 1.0f, 0.0f);
        if (find_ground_surface(world, pos, 0.5f, 1.35f, &floor_y, &floor_n)) {
            if (pos.y <= floor_y + 0.15f) {
                pos.y = floor_y;
                if (velocity.y < 0.0f) velocity.y = 0.0f;
            }
        }

        // Center check: smoothly depenetrate along normal
        if (is_point_solid(world, pos)) {
            collided = true;
            // If embedded inside a floor block at the base of the wall, pop up to floor
            if (find_ground_surface(world, pos, 0.8f, 1.5f, &floor_y, &floor_n) && pos.y < floor_y) {
                pos.y = floor_y;
                if (velocity.y < 0.0f) velocity.y = 0.0f;
            } else {
                pos += n * 0.35f;
            }
        }

        // Tangent boundary checks: prevent clipping into adjoining inside corners
        glm::vec3 tangent_right = glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f));
        if (glm::length(tangent_right) > 0.01f) {
            tangent_right = glm::normalize(tangent_right);
            for (float sign : { -1.0f, 1.0f }) {
                glm::vec3 check_side = pos + tangent_right * (radius * sign);
                if (is_point_solid(world, check_side)) {
                    collided = true;
                    pos -= tangent_right * (sign * 0.15f);
                }
            }
        }
    } else if (surface_state == StalkerSurfaceState::CEILING_CRAWLING) {
        if (velocity.y > 0.0f) velocity.y = 0.0f;

        if (is_point_solid(world, pos)) {
            collided = true;
            pos.y = static_cast<float>(cy) - 0.15f;
        }

        for (const auto& off : offsets) {
            glm::vec3 check_pos = pos + glm::vec3(off.x, 0.0f, off.y);
            if (is_point_solid(world, check_pos)) {
                collided = true;
                int ix = static_cast<int>(std::floor(check_pos.x));
                int iz = static_cast<int>(std::floor(check_pos.z));
                float cx = std::clamp(pos.x, static_cast<float>(ix), static_cast<float>(ix + 1.0f));
                float cz = std::clamp(pos.z, static_cast<float>(iz), static_cast<float>(iz + 1.0f));
                float diff_x = pos.x - cx;
                float diff_z = pos.z - cz;
                float dist_sq = diff_x * diff_x + diff_z * diff_z;
                if (dist_sq < radius * radius) {
                    float dist = std::sqrt(dist_sq);
                    if (dist > 0.0001f) {
                        float overlap = (radius + 0.005f) - dist;
                        pos.x += (diff_x / dist) * overlap;
                        pos.z += (diff_z / dist) * overlap;
                    } else {
                        float d_left = pos.x - static_cast<float>(ix);
                        float d_right = static_cast<float>(ix + 1.0f) - pos.x;
                        float d_bottom = pos.z - static_cast<float>(iz);
                        float d_top = static_cast<float>(iz + 1.0f) - pos.z;
                        float min_d = std::min({d_left, d_right, d_bottom, d_top});
                        if (min_d == d_left) pos.x = static_cast<float>(ix) - radius - 0.005f;
                        else if (min_d == d_right) pos.x = static_cast<float>(ix + 1.0f) + radius + 0.005f;
                        else if (min_d == d_bottom) pos.z = static_cast<float>(iz) - radius - 0.005f;
                        else pos.z = static_cast<float>(iz + 1.0f) + radius + 0.005f;
                    }
                }
            }
        }
    }

    // 2. EMERGENCY PENETRATION RECOVERY (Absolute last resort):
    // Only triggered if an entity is completely buried/encased inside solid rock on ALL probe points
    // (feet, center, AND head are ALL solid), e.g. when spawned inside solid rock or buried by cave-in.
    glm::vec3 feet_pos = pos + glm::vec3(0.0f, 0.15f, 0.0f);
    glm::vec3 center_pos = pos + glm::vec3(0.0f, std::max(0.35f, height * 0.5f), 0.0f);
    glm::vec3 head_pos = pos + glm::vec3(0.0f, std::max(0.65f, height * 0.85f), 0.0f);

    bool center_solid = is_point_solid(world, center_pos);
    bool head_solid = is_point_solid(world, head_pos);
    bool feet_solid = is_point_solid(world, feet_pos);

    if (feet_solid && center_solid && head_solid) {
        collided = true;
        bool found_air = false;
        glm::vec3 best_eject_pos = pos;
        float best_eject_dist_sq = 1e9f;

        int center_x = static_cast<int>(std::floor(center_pos.x));
        int center_y = static_cast<int>(std::floor(center_pos.y));
        int center_z = static_cast<int>(std::floor(center_pos.z));

        // Search upward first if open
        for (int dy = 1; dy <= 4; ++dy) {
            int ty = center_y + dy;
            if (ty < 26 && !is_point_solid(world, glm::vec3(static_cast<float>(center_x) + 0.5f, static_cast<float>(ty) + 0.05f, static_cast<float>(center_z) + 0.5f))) {
                best_eject_pos = glm::vec3(static_cast<float>(center_x) + 0.5f, static_cast<float>(ty) + 0.05f, static_cast<float>(center_z) + 0.5f);
                best_eject_dist_sq = static_cast<float>(dy * dy);
                found_air = true;
                break;
            }
        }

        if (!found_air) {
            for (int r = 1; r <= 4 && !found_air; ++r) {
                for (int dx = -r; dx <= r; ++dx) {
                    for (int dz = -r; dz <= r; ++dz) {
                        for (int dy = -1; dy <= 2; ++dy) {
                            if (std::max(std::abs(dx), std::abs(dz)) != r && std::abs(dy) != r) continue;
                            int tx = center_x + dx;
                            int ty = center_y + dy;
                            int tz = center_z + dz;
                            if (tx < 5 || tx > 67 || tz < 5 || tz > 67 || ty < 4 || ty > 25) continue;
                            glm::vec3 test_pt(static_cast<float>(tx) + 0.5f, static_cast<float>(ty) + 0.05f, static_cast<float>(tz) + 0.5f);
                            if (!is_point_solid(world, test_pt)) {
                                float dist_sq = static_cast<float>(dx * dx + dy * dy + dz * dz);
                                if (dist_sq < best_eject_dist_sq) {
                                    best_eject_dist_sq = dist_sq;
                                    best_eject_pos = test_pt;
                                    found_air = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (found_air) {
            pos = best_eject_pos;
            velocity = glm::vec3(0.0f);
        }
    }

    return collided;
}


std::vector<glm::vec3> AberrantAI::generate_zone_waypoints(
    const PatrolZone& zone,
    const World& world,
    int count
) {
    std::vector<glm::vec3> waypoints;
    waypoints.reserve(count + 1);

    // 1. Center waypoint
    glm::vec3 center = zone.center;
    float ground_y = 0.0f;
    glm::vec3 ground_norm(0.0f, 1.0f, 0.0f);
    if (find_ground_surface(world, center, 3.0f, 6.0f, &ground_y, &ground_norm)) {
        center.y = ground_y + 0.05f;
        if (!is_point_lethal_hazard(world, center)) {
            waypoints.push_back(center);
        }
    }

    // 2. Sample candidate positions distributed inside the zone
    float rx = zone.has_bounds ? (zone.bounds_max.x - zone.bounds_min.x) * 0.35f : zone.radius * 0.65f;
    float rz = zone.has_bounds ? (zone.bounds_max.z - zone.bounds_min.z) * 0.35f : zone.radius * 0.65f;
    rx = std::max(2.0f, rx);
    rz = std::max(2.0f, rz);

    // Diagonal/cardinal offset angles to form a loop
    const float angles[] = {
        0.785398f,         // +X, +Z (NE)
        2.356194f,         // -X, +Z (NW)
        3.926991f,         // -X, -Z (SW)
        5.497787f          // +X, -Z (SE)
    };

    for (int i = 0; i < count && i < 4; ++i) {
        float ang = angles[i];
        glm::vec3 cand = zone.center + glm::vec3(std::cos(ang) * rx, 0.0f, std::sin(ang) * rz);
        cand = zone.clamp_to_zone(cand);

        float gy = 0.0f;
        glm::vec3 gn(0.0f, 1.0f, 0.0f);
        if (find_ground_surface(world, cand, 3.0f, 6.0f, &gy, &gn)) {
            cand.y = gy + 0.05f;
            if (!is_point_lethal_hazard(world, cand)) {
                waypoints.push_back(cand);
            }
        }
    }

    // Fallback: If no waypoints could be grounded, provide zone center
    if (waypoints.empty()) {
        waypoints.push_back(zone.center);
    }

    return waypoints;
}

glm::vec3 AberrantAI::sample_wander_waypoint(
    const PatrolZone& zone,
    const glm::vec3& current_pos,
    const World& world,
    uint32_t seed_salt
) {
    float rx = zone.has_bounds ? (zone.bounds_max.x - zone.bounds_min.x) * 0.40f : zone.radius * 0.80f;
    float rz = zone.has_bounds ? (zone.bounds_max.z - zone.bounds_min.z) * 0.40f : zone.radius * 0.80f;
    rx = std::max(2.0f, rx);
    rz = std::max(2.0f, rz);

    for (int attempt = 0; attempt < 8; ++attempt) {
        uint32_t hash = seed_salt * 1664525u + static_cast<uint32_t>(attempt) * 1013904223u;
        float ang = static_cast<float>(hash % 360) * (glm::pi<float>() / 180.0f);
        float dist_frac = 0.35f + static_cast<float>((hash >> 8) % 65) * 0.01f;

        glm::vec3 cand = zone.center + glm::vec3(std::cos(ang) * rx * dist_frac, 0.0f, std::sin(ang) * rz * dist_frac);
        cand = zone.clamp_to_zone(cand);

        float gy = 0.0f;
        glm::vec3 gn(0.0f, 1.0f, 0.0f);
        if (find_ground_surface(world, cand, 3.0f, 6.0f, &gy, &gn)) {
            cand.y = gy + 0.05f;
            if (!is_point_lethal_hazard(world, cand)) {
                return cand;
            }
        }
    }

    return zone.clamp_to_zone(zone.center);
}

bool AberrantAI::has_line_of_sight(
    const glm::vec3& from,
    const glm::vec3& to,
    const World& world,
    float step_size,
    int max_steps)
{
    glm::vec3 dir = to - from;
    float dist = glm::length(dir);
    if (dist < 0.1f) return true;
    dir /= dist;

    int steps = static_cast<int>(dist / step_size);
    steps = std::min(steps, max_steps);

    for (int i = 1; i < steps; ++i) {
        glm::vec3 sample = from + dir * (step_size * static_cast<float>(i));
        if (is_point_solid(world, sample)) {
            return false;
        }
    }
    return true;
}

bool AberrantAI::is_path_obstructed_by_wall(
    const glm::vec3& from,
    const glm::vec3& to,
    const World& world)
{
    glm::vec3 from_torso = from + glm::vec3(0.0f, 0.45f, 0.0f);
    glm::vec3 to_torso = to + glm::vec3(0.0f, 0.50f, 0.0f);
    if (!has_line_of_sight(from_torso, to_torso, world)) {
        return true;
    }
    glm::vec3 from_low = from + glm::vec3(0.0f, 0.20f, 0.0f);
    glm::vec3 to_low = to + glm::vec3(0.0f, 0.20f, 0.0f);
    return !has_line_of_sight(from_low, to_low, world);
}

std::vector<glm::vec3> AberrantAI::find_ground_route_around_wall(
    const glm::vec3& start_pos,
    const glm::vec3& target_pos,
    const World& world,
    float max_range)
{
    std::vector<glm::vec3> empty_path;
    float direct_dist = glm::distance(glm::vec2(start_pos.x, start_pos.z),
                                      glm::vec2(target_pos.x, target_pos.z));
    if (direct_dist > max_range * 1.5f || direct_dist < 0.2f) {
        return empty_path;
    }

    int start_ix = static_cast<int>(std::floor(start_pos.x));
    int start_iz = static_cast<int>(std::floor(start_pos.z));
    int target_ix = static_cast<int>(std::floor(target_pos.x));
    int target_iz = static_cast<int>(std::floor(target_pos.z));

    int margin = std::clamp(static_cast<int>(direct_dist * 0.65f) + 6, 8, 20);
    int min_x = std::max(2, std::min(start_ix, target_ix) - margin);
    int max_x = std::min(world.world_width() - 3, std::max(start_ix, target_ix) + margin);
    int min_z = std::max(2, std::min(start_iz, target_iz) - margin);
    int max_z = std::min(world.world_depth() - 3, std::max(start_iz, target_iz) + margin);

    int grid_w = max_x - min_x + 1;
    int grid_d = max_z - min_z + 1;
    if (grid_w <= 0 || grid_d <= 0) return empty_path;

    int total_cells = grid_w * grid_d;
    if (total_cells > 6400) return empty_path;

    auto cell_index = [min_x, min_z, grid_w](int x, int z) -> int {
        return (z - min_z) * grid_w + (x - min_x);
    };

    struct PathNode {
        int parent_idx{-1};
        float g_cost{1e9f};
        float f_cost{1e9f};
        float ground_y{0.0f};
        uint8_t state{0}; // 0 = unvisited, 1 = open, 2 = closed
    };

    std::vector<PathNode> nodes(total_cells);

    int start_idx = cell_index(start_ix, start_iz);
    if (start_idx < 0 || start_idx >= total_cells) return empty_path;

    float start_gy = start_pos.y;
    find_ground_surface(world, glm::vec3(start_ix + 0.5f, start_pos.y, start_iz + 0.5f), 1.5f, 2.5f, &start_gy);

    auto heuristic = [target_ix, target_iz](int x, int z) -> float {
        int dx = x - target_ix;
        int dz = z - target_iz;
        return std::sqrt(static_cast<float>(dx * dx + dz * dz));
    };

    nodes[start_idx].g_cost = 0.0f;
    nodes[start_idx].ground_y = start_gy;
    nodes[start_idx].f_cost = heuristic(start_ix, start_iz);
    nodes[start_idx].state = 1;

    using QueueElement = std::pair<float, int>;
    std::priority_queue<QueueElement, std::vector<QueueElement>, std::greater<QueueElement>> open_set;
    open_set.push({nodes[start_idx].f_cost, start_idx});

    int goal_idx = -1;
    int max_expansions = 1200;
    int expansions = 0;

    const int dx[8] = { 1, -1, 0, 0,  1, -1,  1, -1 };
    const int dz[8] = { 0, 0, 1, -1,  1,  1, -1, -1 };
    const float step_costs[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.414f, 1.414f, 1.414f, 1.414f };

    while (!open_set.empty() && expansions++ < max_expansions) {
        auto [cur_f, cur_idx] = open_set.top();
        open_set.pop();

        if (nodes[cur_idx].state == 2) continue;
        nodes[cur_idx].state = 2; // Closed

        int cur_x = min_x + (cur_idx % grid_w);
        int cur_z = min_z + (cur_idx / grid_w);
        float cur_y = nodes[cur_idx].ground_y;
        glm::vec3 cur_pt(cur_x + 0.5f, cur_y, cur_z + 0.5f);

        // Check if reached target coordinate directly or within close proximity (<= 1.5m)
        float dist_sq = static_cast<float>((cur_x - target_ix) * (cur_x - target_ix) + (cur_z - target_iz) * (cur_z - target_iz));
        if (dist_sq <= 2.25f || (cur_x == target_ix && cur_z == target_iz)) {
            goal_idx = cur_idx;
            break;
        }

        // If not at start, and cur_pt has clear line of sight to target (wall has been bypassed!)
        if (cur_idx != start_idx && dist_sq <= (direct_dist + 4.0f) * (direct_dist + 4.0f)) {
            glm::vec3 cur_eye = cur_pt + glm::vec3(0.0f, 0.45f, 0.0f);
            glm::vec3 tgt_eye = target_pos + glm::vec3(0.0f, 0.5f, 0.0f);
            if (has_line_of_sight(cur_eye, tgt_eye, world) && !has_hazard_between(cur_pt, target_pos, world)) {
                goal_idx = cur_idx;
                break;
            }
        }

        for (int i = 0; i < 8; ++i) {
            int nx = cur_x + dx[i];
            int nz = cur_z + dz[i];

            if (nx < min_x || nx > max_x || nz < min_z || nz > max_z) continue;

            // Diagonal corner-cutting check: don't cut corners between two solid walls
            if (i >= 4) {
                int cx1 = cur_x + dx[i];
                int cz1 = cur_z;
                int cx2 = cur_x;
                int cz2 = cur_z + dz[i];
                int iy = static_cast<int>(std::floor(cur_y + 0.4f));
                if (world.is_solid(cx1, iy, cz1) && world.is_solid(cx2, iy, cz2)) {
                    continue;
                }
            }

            int n_idx = cell_index(nx, nz);
            if (nodes[n_idx].state == 2) continue;

            float ny = cur_y;
            if (!find_ground_surface(world, glm::vec3(nx + 0.5f, cur_y, nz + 0.5f), 1.35f, 1.8f, &ny)) {
                continue;
            }

            if (std::abs(ny - cur_y) > 1.35f) continue;

            // Headroom check (0.35m and 0.85m above ground)
            if (is_point_solid(world, glm::vec3(nx + 0.5f, ny + 0.35f, nz + 0.5f)) ||
                is_point_solid(world, glm::vec3(nx + 0.5f, ny + 0.85f, nz + 0.5f))) {
                continue;
            }

            // Hazard fear check
            if (is_point_lethal_hazard(world, glm::vec3(nx + 0.5f, ny + 0.1f, nz + 0.5f))) {
                continue;
            }

            float tentative_g = nodes[cur_idx].g_cost + step_costs[i] + std::abs(ny - cur_y) * 0.4f;
            if (tentative_g < nodes[n_idx].g_cost) {
                nodes[n_idx].parent_idx = cur_idx;
                nodes[n_idx].g_cost = tentative_g;
                nodes[n_idx].ground_y = ny;
                nodes[n_idx].f_cost = tentative_g + heuristic(nx, nz);
                nodes[n_idx].state = 1;
                open_set.push({nodes[n_idx].f_cost, n_idx});
            }
        }
    }

    if (goal_idx == -1) {
        return empty_path;
    }

    // Reconstruct raw path
    std::vector<glm::vec3> raw_path;
    int goal_x = min_x + (goal_idx % grid_w);
    int goal_z = min_z + (goal_idx / grid_w);
    if (goal_x != target_ix || goal_z != target_iz) {
        raw_path.push_back(target_pos);
    }

    int curr = goal_idx;
    while (curr != -1) {
        int x = min_x + (curr % grid_w);
        int z = min_z + (curr / grid_w);
        raw_path.push_back(glm::vec3(x + 0.5f, nodes[curr].ground_y + 0.05f, z + 0.5f));
        curr = nodes[curr].parent_idx;
    }
    std::reverse(raw_path.begin(), raw_path.end());

    if (raw_path.size() <= 1) {
        return raw_path;
    }

    // String-pulling / waypoint pruning to produce smooth direct waypoints around corners
    std::vector<glm::vec3> pruned;
    pruned.reserve(raw_path.size());
    pruned.push_back(raw_path[0]);

    size_t cur_pt = 0;
    while (cur_pt < raw_path.size() - 1) {
        size_t furthest = cur_pt + 1;
        for (size_t next = raw_path.size() - 1; next > cur_pt + 1; --next) {
            glm::vec3 from = raw_path[cur_pt] + glm::vec3(0.0f, 0.45f, 0.0f);
            glm::vec3 to = raw_path[next] + glm::vec3(0.0f, 0.45f, 0.0f);
            if (has_line_of_sight(from, to, world) && !has_hazard_between(raw_path[cur_pt], raw_path[next], world)) {
                furthest = next;
                break;
            }
        }
        pruned.push_back(raw_path[furthest]);
        cur_pt = furthest;
    }

    return pruned;
}

bool AberrantAI::is_vertical_climb_route_viable(
    const glm::vec3& enemy_pos,
    const glm::vec3& target_pos,
    const World& world,
    float max_wall_height)
{
    glm::vec3 to_target = target_pos - enemy_pos;
    float dist = glm::length(glm::vec2(to_target.x, to_target.z));
    if (dist < 1.0f || dist > 24.0f) return false;

    glm::vec3 fwd = glm::normalize(glm::vec3(to_target.x, 0.0f, to_target.z));

    float step = 0.5f;
    int steps = static_cast<int>(dist / step);
    glm::vec3 wall_hit_pos(0.0f);
    bool hit_wall = false;
    for (int i = 1; i < steps; ++i) {
        glm::vec3 pt = enemy_pos + glm::vec3(0.0f, 0.5f, 0.0f) + fwd * (static_cast<float>(i) * step);
        if (is_point_solid(world, pt)) {
            wall_hit_pos = pt;
            hit_wall = true;
            break;
        }
    }
    if (!hit_wall) return false;

    int wx = static_cast<int>(std::floor(wall_hit_pos.x));
    int wz = static_cast<int>(std::floor(wall_hit_pos.z));
    int base_y = static_cast<int>(std::floor(enemy_pos.y));

    for (int y = base_y + 1; y <= base_y + static_cast<int>(max_wall_height); ++y) {
        if (y >= 31) break;
        glm::vec3 wall_top_sample(wx + 0.5f, static_cast<float>(y) + 0.5f, wz + 0.5f);
        if (!is_point_solid(world, wall_top_sample)) {
            glm::vec3 wall_top_head(wx + 0.5f, static_cast<float>(y) + 1.5f, wz + 0.5f);
            if (!is_point_solid(world, wall_top_head)) {
                glm::vec3 past_wall = wall_hit_pos + fwd * 1.5f;
                int px = static_cast<int>(std::floor(past_wall.x));
                int pz = static_cast<int>(std::floor(past_wall.z));
                glm::vec3 landing_feet(px + 0.5f, static_cast<float>(y) + 0.5f, pz + 0.5f);
                glm::vec3 landing_head(px + 0.5f, static_cast<float>(y) + 1.5f, pz + 0.5f);
                if (!is_point_solid(world, landing_feet) && !is_point_solid(world, landing_head)) {
                    return true;
                }
            }
        }
    }

    return false;
}

AlternateRoute AberrantAI::find_alternate_route(
    const glm::vec3& enemy_pos,
    const glm::vec3& target_pos,
    const World& world,
    float max_search_range,
    bool can_climb_walls)
{
    AlternateRoute route;
    route.type = AlternateRouteType::None;
    route.is_valid = false;
    route.total_distance = 0.0f;

    // Direct path check: if not obstructed, no alternate route needed
    if (!is_path_obstructed_by_wall(enemy_pos, target_pos, world)) {
        return route;
    }

    // 1. Try ground detour route around the wall
    std::vector<glm::vec3> ground_waypoints = find_ground_route_around_wall(enemy_pos, target_pos, world, max_search_range);
    float ground_dist = 0.0f;
    for (size_t i = 1; i < ground_waypoints.size(); ++i) {
        ground_dist += glm::distance(ground_waypoints[i - 1], ground_waypoints[i]);
    }

    // 2. Check vertical climbing route
    bool climb_viable = can_climb_walls && is_vertical_climb_route_viable(enemy_pos, target_pos, world);
    float direct_dist = glm::distance(enemy_pos, target_pos);

    // If ground detour exists:
    if (!ground_waypoints.empty()) {
        if (!climb_viable || ground_dist < direct_dist * 2.2f) {
            route.type = AlternateRouteType::GroundDetour;
            route.waypoints = ground_waypoints;
            route.is_valid = true;
            route.total_distance = ground_dist;
            return route;
        }
    }

    // If ground route is blocked, but vertical climb is viable:
    if (climb_viable) {
        route.type = AlternateRouteType::VerticalClimb;
        route.is_valid = true;
        glm::vec3 to_tgt = target_pos - enemy_pos;
        glm::vec3 fwd = glm::normalize(glm::vec3(to_tgt.x, 0.0f, to_tgt.z));
        route.waypoints.push_back(enemy_pos);
        route.waypoints.push_back(enemy_pos + fwd * 1.5f);
        route.waypoints.push_back(target_pos);
        route.total_distance = direct_dist * 1.2f;
        return route;
    }

    // Fallback if ground route exists
    if (!ground_waypoints.empty()) {
        route.type = AlternateRouteType::GroundDetour;
        route.waypoints = ground_waypoints;
        route.is_valid = true;
        route.total_distance = ground_dist;
        return route;
    }

    return route;
}

} // namespace Voidfall

