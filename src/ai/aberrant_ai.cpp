#include "aberrant_ai.hpp"
#include "../voxel/world.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

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
        } else {
            best_normal = glm::vec3(0.0f, 1.0f, 0.0f);
            min_dist = down_dist;
            result.has_contact = true;
        }
    } else if (current_state == StalkerSurfaceState::WALL_CLIMBING) {
        if (has_down && down_dist <= 0.65f && (target_pos == nullptr || target_pos->y <= position.y + 0.5f)) {
            // Smoothly step down onto floor when reaching ground level (only if target is not above)
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
                    total_force += (glm::vec3(dx, dy, dz) / dist_sq) * separation_force;
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
        total_force += (glm::vec3(dx, dy, dz) / dist_sq) * separation_force;
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

    if (glm::length(v_wall) > 0.05f) {
        return glm::normalize(v_wall);
    }

    glm::vec3 down_wall = glm::vec3(0.0f, -1.0f, 0.0f) - glm::dot(glm::vec3(0.0f, -1.0f, 0.0f), n) * n;
    if (glm::length(down_wall) > 0.05f) {
        return glm::normalize(down_wall);
    }
    return glm::vec3(0.0f, -1.0f, 0.0f);
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
                            continue;
                        }
                    }
                }
                return true;
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

    return right;
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
    pos.y = std::clamp(pos.y, 4.5f, 26.0f);
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
            } else if (pos.y <= ground_y + 0.65f) {
                // Smooth slope, ramp, and half-block hugging
                pos.y = ground_y;
            }
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

    // 1. ABSOLUTE PENETRATION RECOVERY:
    // If the entity is genuinely embedded inside a solid rock wall (feet, center, or head are in solid blocks),
    // eject immediately to nearest open air so enemies never remain trapped inside walls.
    glm::vec3 feet_pos = pos + glm::vec3(0.0f, 0.15f, 0.0f);
    glm::vec3 center_pos = pos + glm::vec3(0.0f, std::max(0.35f, height * 0.5f), 0.0f);
    glm::vec3 head_pos = pos + glm::vec3(0.0f, std::max(0.65f, height * 0.85f), 0.0f);

    bool center_solid = is_point_solid(world, center_pos);
    bool head_solid = is_point_solid(world, head_pos);
    bool feet_solid = is_point_solid(world, feet_pos);

    if (center_solid || (feet_solid && head_solid)) {
        collided = true;
        bool found_air = false;
        glm::vec3 best_eject_pos = pos;
        float best_eject_dist_sq = 1e9f;

        // Check upwards only if overhead space is open (not trapped under a top block / ceiling)
        int center_x = static_cast<int>(std::floor(center_pos.x));
        int center_y = static_cast<int>(std::floor(center_pos.y));
        int center_z = static_cast<int>(std::floor(center_pos.z));
        bool ceiling_solid = (center_y + 1 < 32) && world.is_solid(glm::ivec3(center_x, center_y + 1, center_z));
        if (!ceiling_solid) {
            for (int dy = 1; dy <= 4; ++dy) {
                int ty = center_y + dy;
                if (ty < 26 && !is_point_solid(world, glm::vec3(static_cast<float>(center_x) + 0.5f, static_cast<float>(ty) + 0.05f, static_cast<float>(center_z) + 0.5f))) {
                    best_eject_pos = glm::vec3(static_cast<float>(center_x) + 0.5f, static_cast<float>(ty) + 0.05f, static_cast<float>(center_z) + 0.5f);
                    best_eject_dist_sq = static_cast<float>(dy * dy);
                    found_air = true;
                    break;
                }
            }
        }

        if (!found_air) {
            for (int r = 1; r <= 5 && !found_air; ++r) {
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

                    // Step clambering check: ONLY allowed if collision was at feet level (check_h < 0.35f)
                    // and upper clearance is open!
                    float step_surf_y = -1000.0f;
                    bool can_step = false;
                    if (check_h < 0.35f) {
                        for (int ty = iy + 1; ty >= iy - 1; --ty) {
                            Voxel tv = world.get_voxel(ix, ty, iz);
                            if (tv.is_solid()) {
                                float s_y = get_voxel_surface_y(world, ix, ty, iz, check_pos.x, check_pos.z);
                                float step_h = s_y - pos.y;
                                if (step_h > 0.0f && step_h <= 1.35f) {
                                    if (!is_point_solid(world, glm::vec3(check_pos.x, s_y + 0.35f, check_pos.z)) &&
                                        !is_point_solid(world, glm::vec3(check_pos.x, s_y + height * 0.75f, check_pos.z))) {
                                        step_surf_y = s_y;
                                        can_step = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if (can_step) {
                        pos.y = std::max(pos.y, step_surf_y);
                        continue;
                    }

                    // Solid wall / top block: push out along penetrating axis
                    collided = true;
                    float center_vx = static_cast<float>(ix) + 0.5f;
                    float center_vz = static_cast<float>(iz) + 0.5f;
                    float dx = pos.x - center_vx;
                    float dz = pos.z - center_vz;
                    if (std::abs(dx) > std::abs(dz)) {
                        pos.x = static_cast<float>(ix) + (dx > 0.0f ? (1.0f + radius + 0.01f) : (-radius - 0.01f));
                        if (dx > 0.0f) velocity.x = std::max(0.0f, velocity.x);
                        else velocity.x = std::min(0.0f, velocity.x);
                    } else {
                        pos.z = static_cast<float>(iz) + (dz > 0.0f ? (1.0f + radius + 0.01f) : (-radius - 0.01f));
                        if (dz > 0.0f) velocity.z = std::max(0.0f, velocity.z);
                        else velocity.z = std::min(0.0f, velocity.z);
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

        // Center check
        if (is_point_solid(world, pos)) {
            collided = true;
            pos += n * 0.35f;
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
                float center_vx = static_cast<float>(ix) + 0.5f;
                float center_vz = static_cast<float>(iz) + 0.5f;
                float dx = pos.x - center_vx;
                float dz = pos.z - center_vz;
                if (std::abs(dx) > std::abs(dz)) {
                    pos.x = static_cast<float>(ix) + (dx > 0.0f ? (1.0f + radius + 0.01f) : (-radius - 0.01f));
                } else {
                    pos.z = static_cast<float>(iz) + (dz > 0.0f ? (1.0f + radius + 0.01f) : (-radius - 0.01f));
                }
            }
        }
    }

    return collided;
}

} // namespace Voidfall

