#include "void_stalker.hpp"
#include "../../voxel/world.hpp"
#include "../../core/logger.hpp"
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <random>

namespace Voidfall {

// ─── VoidStalkerManager ──────────────────────────────────────────────

void VoidStalkerManager::spawn_stalker(const glm::vec3& pos, float difficulty_mul, StalkerRole role) {
    VoidStalker s;
    s.id = m_next_id++;
    s.role = role;
    s.position = pos;
    s.patrol_anchor = pos;
    s.patrol_waypoint = pos;
    s.patrol_timer = 2.0f;
    s.hp = 40.0f * difficulty_mul;
    s.max_hp = s.hp;
    s.damage = 18.0f * difficulty_mul;
    s.state = StalkerState::Idle;
    s.state_timer = 0.0f;
    s.glow_phase = static_cast<float>(m_next_id) * 1.7f; // Offset pulsing
    s.screech_timer = 4.0f + static_cast<float>((m_next_id * 31) % 110) * 0.1f; // Staggered initial screech
    s.just_screeched = false;

    m_stalkers.push_back(s);
}

void VoidStalkerManager::spawn_ambient_stalker(const glm::vec3& room_center, const World& world) {
    static std::mt19937 rng(1337);
    std::uniform_real_distribution<float> offset_dist(-5.5f, 5.5f);

    glm::vec3 spawn_pos = room_center;
    for (int attempt = 0; attempt < 15; ++attempt) {
        float ox = offset_dist(rng);
        float oz = offset_dist(rng);
        glm::vec3 candidate = room_center + glm::vec3(ox, 0.0f, oz);
        candidate.x = std::clamp(candidate.x, 6.0f, 66.0f);
        candidate.z = std::clamp(candidate.z, 6.0f, 66.0f);

        int ix = static_cast<int>(std::floor(candidate.x));
        int iz = static_cast<int>(std::floor(candidate.z));

        // Find floor in room column
        for (int y = static_cast<int>(candidate.y) + 4; y >= static_cast<int>(candidate.y) - 3; --y) {
            if (world.get_voxel(ix, y, iz).material_id == MAT_AIR &&
                world.get_voxel(ix, y - 1, iz).material_id != MAT_AIR) {
                spawn_pos = glm::vec3(ix + 0.5f, static_cast<float>(y) + 0.1f, iz + 0.5f);
                break;
            }
        }
    }

    // Alternate ambient roles so caverns have both Red Melee and Green Shooter threats
    StalkerRole role = (m_next_id % 2 == 0) ? StalkerRole::Shooter : StalkerRole::Melee;
    spawn_stalker(spawn_pos, 1.0f, role);
    if (!m_stalkers.empty()) {
        m_stalkers.back().patrol_anchor = spawn_pos;
        m_stalkers.back().patrol_waypoint = spawn_pos;
    }
}

void VoidStalkerManager::spawn_wave(const glm::vec3& player_pos, int count, const World& world) {
    if (m_spawn_cooldown > 0.0f) return;

    for (int i = 0; i < count; ++i) {
        glm::vec3 spawn = find_spawn_pos(player_pos, world);
        // Tactical wave composition: Even index is Melee (Red), Odd index is Shooter (Green)
        StalkerRole role = (i % 2 == 0) ? StalkerRole::Melee : StalkerRole::Shooter;
        spawn_stalker(spawn, 1.0f, role);
    }
    m_spawn_cooldown = 8.0f; // Minimum 8s between spawned waves
    VF_LOG_INFO("VoidStalker", "Spawned wave of " << count << " stalkers (tactical Melee/Shooter mix) near player");
}

glm::vec3 VoidStalkerManager::find_spawn_pos(const glm::vec3& near, const World& world) {
    // Smart Spawning: Spawn 18–32m away from player, preferring positions with NO direct line-of-sight
    // (out from behind corners, rock pillars, or cavern bends) so enemies stalk in naturally
    static std::mt19937 rng(42);
    std::uniform_real_distribution<float> angle_dist(0.0f, glm::two_pi<float>());
    std::uniform_real_distribution<float> dist_range(18.0f, 32.0f);
    std::uniform_real_distribution<float> height_dist(-2.0f, 5.0f);

    glm::vec3 fallback_candidate = near + glm::vec3(22.0f, 1.0f, 22.0f);

    for (int attempt = 0; attempt < 25; ++attempt) {
        float angle = angle_dist(rng);
        float dist = dist_range(rng);
        float dy = height_dist(rng);

        glm::vec3 candidate(
            near.x + std::cos(angle) * dist,
            near.y + dy,
            near.z + std::sin(angle) * dist
        );

        // Clamp to sector bounds
        candidate.x = std::clamp(candidate.x, 5.0f, 67.0f);
        candidate.y = std::clamp(candidate.y, 4.0f, 25.0f);
        candidate.z = std::clamp(candidate.z, 5.0f, 67.0f);

        // Check that spawn pos is in air (not inside a wall)
        int ix = static_cast<int>(std::floor(candidate.x));
        int iy = static_cast<int>(std::floor(candidate.y));
        int iz = static_cast<int>(std::floor(candidate.z));

        Voxel v = world.get_voxel(ix, iy, iz);
        Voxel v_below = world.get_voxel(ix, iy - 1, iz);

        // Must be in air with solid below (so stalker has a surface)
        if (v.material_id == MAT_AIR && v_below.material_id != MAT_AIR) {
            bool los = has_line_of_sight(candidate + glm::vec3(0.0f, 0.5f, 0.0f), near + glm::vec3(0.0f, 1.5f, 0.0f), world);
            if (!los) {
                // Perfect: terrain-occluded spawn point!
                return candidate;
            }
            fallback_candidate = candidate;
        }
    }

    // Fallback: spawn at distance from player
    return fallback_candidate;
}

bool VoidStalkerManager::has_line_of_sight(const glm::vec3& from, const glm::vec3& to, const World& world) const {
    // Simple DDA ray march checking for solid voxels between two points
    glm::vec3 dir = to - from;
    float dist = glm::length(dir);
    if (dist < 0.1f) return true;
    dir /= dist;

    float step_size = 0.5f;
    int steps = static_cast<int>(dist / step_size);
    steps = std::min(steps, 60); // Cap iterations

    for (int i = 1; i < steps; ++i) {
        glm::vec3 sample = from + dir * (step_size * static_cast<float>(i));
        int sx = static_cast<int>(std::floor(sample.x));
        int sy = static_cast<int>(std::floor(sample.y));
        int sz = static_cast<int>(std::floor(sample.z));

        Voxel v = world.get_voxel(sx, sy, sz);
        if (v.material_id != MAT_AIR && v.material_id != MAT_GAS &&
            v.material_id != MAT_VOLATILE_SMOKE) {
            return false;
        }
    }
    return true;
}

bool VoidStalkerManager::is_in_headlamp(const glm::vec3& pos, const glm::vec3& player_pos,
                                         const glm::vec3& headlamp_dir, float inner_angle) const {
    glm::vec3 to_stalker = glm::normalize(pos - player_pos);
    float dot = glm::dot(headlamp_dir, to_stalker);
    float cone_cos = std::cos(glm::radians(inner_angle));
    return dot > cone_cos;
}

VoidStalkerManager::FrameResult VoidStalkerManager::update(
    float dt,
    const glm::vec3& player_pos,
    const glm::vec3& player_forward,
    const glm::vec3& headlamp_dir,
    bool headlamp_on,
    float noise_level_pct,
    bool player_is_drilling,
    const World& world,
    const std::vector<SoundEvent>& sound_events,
    bool player_is_crouching)
{
    FrameResult result;
    m_spawn_cooldown = std::max(0.0f, m_spawn_cooldown - dt);
    m_global_screech_cooldown = std::max(0.0f, m_global_screech_cooldown - dt);

    // Compute player velocity for predictive leading attacks
    glm::vec3 player_vel(0.0f);
    if (m_has_prev_player_pos && dt > 0.0001f) {
        player_vel = (player_pos - m_prev_player_pos) / dt;
        float speed = glm::length(player_vel);
        if (speed > 25.0f) {
            player_vel = (player_vel / speed) * 25.0f;
        }
    }
    m_prev_player_pos = player_pos;
    m_has_prev_player_pos = true;

    for (auto& s : m_stalkers) {
        // Reset per-frame one-shot notification flags for all entities
        s.just_screeched = false;
        s.just_chittered = false;
        s.just_spotted_player = false;
        s.just_heard_sound = false;
        s.just_lunged = false;
        s.just_hit_player = false;
        s.just_fired_spine = false;
        s.just_died = false;

        if (s.state == StalkerState::Dead) continue;

        s.state_timer += dt;
        s.glow_phase += dt * 2.0f;
        s.attack_cooldown = std::max(0.0f, s.attack_cooldown - dt);
        s.projectile_cooldown = std::max(0.0f, s.projectile_cooldown - dt);
        s.slash_fx_timer = std::max(0.0f, s.slash_fx_timer - dt);

        float dist_to_player = glm::distance(s.position, player_pos);
        glm::vec3 to_player = player_pos - s.position;
        float horiz_dist = std::hypot(to_player.x, to_player.z);
        if (horiz_dist > 0.05f) {
            s.pitch = std::atan2(to_player.y, horiz_dist);
        }

        // Animate procedural leg crawl cycle
        float current_speed = glm::length(s.velocity);
        if (current_speed > 0.1f) {
            s.walk_cycle += current_speed * dt * 2.8f;
        } else {
            s.walk_cycle += dt * 1.2f; // Subtle idle twitch
        }

        // Ambient Cavern Echo Screeches (subtle, rare & atmospheric - minimum 35s global cooldown)
        if (s.state != StalkerState::Dead && s.state != StalkerState::Stunned) {
            if (m_global_screech_cooldown <= 0.0f && dist_to_player > 18.0f && !s.is_in_light) {
                s.just_screeched = true;
                result.any_screech = true;
                m_global_screech_cooldown = 35.0f + static_cast<float>((s.id * 37) % 25);
                VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " emitted distant cavern echo screech (dist=" 
                            << dist_to_player << "m)");
            }
        }

        // ─── 1. Acoustic Sound Hearing Processing ────────────────────────
        // Check if any sound events occurred within hearing range
        const SoundEvent* best_sound = nullptr;
        float best_sound_dist = 999.0f;

        for (const auto& snd : sound_events) {
            float snd_dist = glm::distance(s.position, snd.position);
            // Solid rock dampens sound propagation by 35%
            bool sound_los = has_line_of_sight(s.position + glm::vec3(0.0f, 0.4f, 0.0f), snd.position, world);
            float effective_radius = snd.audible_radius * (sound_los ? 1.0f : 0.65f);

            if (snd_dist <= effective_radius && snd_dist < best_sound_dist) {
                best_sound_dist = snd_dist;
                best_sound = &snd;
            }
        }

        // Fallback acoustic detection for continuous drill grinding or high global noise
        if (!best_sound && (player_is_drilling || noise_level_pct > 30.0f)) {
            float effective_hearing_radius = player_is_drilling ? 20.0f : (10.0f + (noise_level_pct / 100.0f) * 14.0f);
            if (dist_to_player <= effective_hearing_radius) {
                // Synthetic continuous sound event centered at player
                s.investigation_target = player_pos;
                s.has_sound_target = true;
                s.just_heard_sound = true;
                result.any_heard_sound = true;
            }
        }

        if (best_sound) {
            s.investigation_target = best_sound->position;
            s.has_sound_target = true;
            s.just_heard_sound = true;
            result.any_heard_sound = true;
        }

        // ─── 2. Visual & Line-of-Sight Perception Processing ─────────────
        // Check headlamp cone illumination
        s.is_in_light = headlamp_on &&
                        dist_to_player < 16.0f &&
                        is_in_headlamp(s.position, player_pos, headlamp_dir, 28.0f);

        // Raycast Line of Sight from stalker eyes to delver torso
        bool los = has_line_of_sight(s.position + glm::vec3(0.0f, 0.4f, 0.0f), player_pos + glm::vec3(0.0f, 0.7f, 0.0f), world);
        s.has_player_los = los;

        // Visual detection range (shadow vs light vs stealth crouch)
        float visual_range = 10.0f;
        if (s.is_pursuing_attacker) {
            visual_range = s.pursuit_break_dist; // High alert predator pursuit tracking
        } else if (s.is_in_light) {
            visual_range = 18.0f; // Directly lit up by flashlight beam
        } else if (player_is_crouching) {
            visual_range = 5.2f;  // Delver crouching in dark shadows has high visual concealment
        } else if (noise_level_pct > 30.0f || player_is_drilling) {
            visual_range = 14.0f; // Agitated alertness
        }

        // Tactile proximity trigger: physically colliding within 2.2m senses warmth/contact
        bool tactile_proximity = (dist_to_player < 2.2f);
        bool can_see_player = (los && dist_to_player <= visual_range) || (los && s.is_in_light) || tactile_proximity;

        if (can_see_player) {
            s.last_seen_player_pos = player_pos;
            s.lost_los_timer = 0.0f;
        } else {
            s.lost_los_timer += dt;
        }

        // Retaliation pursuit tracking:
        // If enemy was attacked by player, it pursues the player. It only loses interest if the player
        // has fled far away (> pursuit_break_dist or prolonged broken LOS at distance) for a long time.
        if (s.is_pursuing_attacker) {
            if (dist_to_player > s.pursuit_break_dist || (!can_see_player && dist_to_player > 18.0f)) {
                s.pursuit_lost_timer += dt;
                if (s.pursuit_lost_timer >= s.pursuit_break_time) {
                    s.is_pursuing_attacker = false;
                    s.pursuit_lost_timer = 0.0f;
                    s.state = StalkerState::Idle;
                    s.state_timer = 0.0f;
                    s.patrol_anchor = s.position;
                    s.patrol_waypoint = s.position;
                    VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " lost interest after prolonged pursuit (player escaped for " 
                                << s.pursuit_break_time << "s, dist=" << dist_to_player << "m)");
                }
            } else {
                // Maintained proximity or visual contact: reset flee timeout
                s.pursuit_lost_timer = 0.0f;
            }
        }

        // === FSM State Transitions & Behavior ===
        switch (s.state) {

        case StalkerState::Idle: {
            // Priority 1: Visual contact -> Switch to combat tracking (Stalking or Ambush Lunge)
            if (can_see_player) {
                if (dist_to_player < 4.2f && s.role == StalkerRole::Melee) {
                    s.state = StalkerState::Lunging;
                    s.state_timer = 0.0f;
                    s.just_lunged = true;
                    result.any_lunge = true;
                    VF_LOG_INFO("VoidStalker", "Melee Stalker " << s.id << " AMBUSH LUNGES at close player (dist=" << dist_to_player << "m)!");
                } else {
                    s.state = StalkerState::Stalking;
                    s.state_timer = 0.0f;
                    s.target_pos = player_pos;
                    s.just_spotted_player = true;
                    result.any_spotted = true;
                    VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " SPOTTED player (dist=" << dist_to_player << "m, in_light=" << s.is_in_light << ")");
                }
                break;
            }

            // Priority 2: Heard a sound event -> Transition to Investigating sound location!
            if (s.has_sound_target && s.just_heard_sound) {
                s.state = StalkerState::Investigating;
                s.state_timer = 0.0f;
                s.investigation_timer = 3.5f;
                s.target_pos = s.investigation_target;
                VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " HEARD sound disturbance -> Investigating (" 
                            << s.investigation_target.x << ", " << s.investigation_target.y << ", " << s.investigation_target.z << ")");
                break;
            }

            // Ambient prowling & roaming around patrol anchor
            s.patrol_timer -= dt;
            if (s.patrol_timer <= 0.0f) {
                s.patrol_timer = 3.0f + static_cast<float>((s.id * 17) % 30) * 0.1f;
                float angle = static_cast<float>((s.id * 73 + static_cast<int>(s.state_timer * 10)) % 360) * (glm::pi<float>() / 180.0f);
                float radius = 3.0f + static_cast<float>((s.id * 31) % 40) * 0.1f;
                glm::vec3 wp = s.patrol_anchor + glm::vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
                wp.x = std::clamp(wp.x, 6.0f, 66.0f);
                wp.z = std::clamp(wp.z, 6.0f, 66.0f);
                s.patrol_waypoint = wp;
            }

            // Eerie stop-and-scan prowl: scurry in bursts, pause to sense surroundings
            glm::vec3 to_wp = s.patrol_waypoint - s.position;
            float wp_dist = std::hypot(to_wp.x, to_wp.z);
            if (wp_dist > 0.4f) {
                glm::vec3 prowl_dir = glm::normalize(glm::vec3(to_wp.x, 0.0f, to_wp.z));
                float prowl_burst = std::max(0.08f, 0.5f + 0.7f * std::sin(s.walk_cycle * 0.6f));
                s.velocity = prowl_dir * s.prowl_speed * prowl_burst;
                s.position += s.velocity * dt;
                s.yaw = std::atan2(prowl_dir.x, prowl_dir.z);
            } else {
                s.velocity = glm::vec3(0.0f);
            }
            s.position.y += std::sin(s.walk_cycle * 2.2f) * 0.010f * dt;
            break;
        }

        case StalkerState::Investigating: {
            // Priority 1: Gained direct Line of Sight to player while searching -> Attack!
            if (can_see_player) {
                if (dist_to_player < 4.2f && s.role == StalkerRole::Melee) {
                    s.state = StalkerState::Lunging;
                    s.state_timer = 0.0f;
                    s.just_lunged = true;
                    result.any_lunge = true;
                    VF_LOG_INFO("VoidStalker", "Melee Stalker " << s.id << " SPOTTED player while investigating and LUNGED!");
                } else {
                    s.state = StalkerState::Stalking;
                    s.state_timer = 0.0f;
                    s.target_pos = player_pos;
                    s.just_spotted_player = true;
                    result.any_spotted = true;
                    VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " SPOTTED player during sound investigation!");
                }
                break;
            }

            // Priority 2: Newer sound heard -> Redirect investigation
            if (s.just_heard_sound) {
                s.investigation_timer = 3.5f;
                s.target_pos = s.investigation_target;
            }

            // Move towards sound disturbance location
            glm::vec3 to_snd = s.investigation_target - s.position;
            float snd_dist = std::hypot(to_snd.x, to_snd.z);

            if (snd_dist > 1.0f) {
                glm::vec3 move_dir = glm::normalize(glm::vec3(to_snd.x, 0.0f, to_snd.z));
                float burst = 0.60f + 0.65f * std::abs(std::sin(s.walk_cycle * 0.8f));
                s.velocity.x = move_dir.x * s.move_speed * 0.85f * burst;
                s.velocity.z = move_dir.z * s.move_speed * 0.85f * burst;
                s.position.x += s.velocity.x * dt;
                s.position.z += s.velocity.z * dt;
                s.yaw = std::atan2(move_dir.x, move_dir.z);
            } else {
                // Reached sound origin! Stand, twitch, tilt pitch, chitter suspiciously
                s.velocity = glm::vec3(0.0f);
                s.investigation_timer -= dt;

                s.chitter_timer -= dt;
                if (s.chitter_timer <= 0.0f) {
                    s.chitter_timer = 2.5f + static_cast<float>((s.id * 19) % 20) * 0.1f;
                    s.just_chittered = true;
                    result.any_chitter = true;
                }

                // Nothing found and silence resumed
                if (s.investigation_timer <= 0.0f) {
                    if (s.is_pursuing_attacker) {
                        // Persistently hunting attacker: resume stalking towards player's last direction
                        s.state = StalkerState::Stalking;
                        s.state_timer = 0.0f;
                        s.target_pos = can_see_player ? player_pos : s.last_seen_player_pos;
                        VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " continuing relentless pursuit of attacker!");
                    } else {
                        s.patrol_anchor = s.position;
                        s.patrol_waypoint = s.position;
                        s.state = StalkerState::Idle;
                        s.state_timer = 0.0f;
                        s.has_sound_target = false;
                        VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " finished investigating sound (nothing found) -> returning to Idle");
                    }
                }
            }
            break;
        }

        case StalkerState::Stalking: {
            s.target_pos = can_see_player ? player_pos : s.last_seen_player_pos;

            // Subtle, eerie chitinous mandible clicks & chittering when stalking in the dark
            if (!s.is_in_light && dist_to_player >= 5.0f && dist_to_player <= 16.0f) {
                s.chitter_timer -= dt;
                if (s.chitter_timer <= 0.0f) {
                    s.chitter_timer = 4.0f + static_cast<float>((s.id * 19) % 35) * 0.1f;
                    s.just_chittered = true;
                    result.any_chitter = true;
                }
            }

            // Lost Line-of-Sight for sustained duration (> 4.5s) without hearing fresh sound
            if (!can_see_player && s.lost_los_timer > 4.5f) {
                s.investigation_target = s.last_seen_player_pos;
                s.investigation_timer = 3.0f;
                s.target_pos = s.investigation_target;
                s.state = StalkerState::Investigating;
                s.state_timer = 0.0f;
                VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " lost line-of-sight to player -> searching last known location");
                break;
            }

            // Close range (< 6.8m) combat reactions:
            if (dist_to_player < 6.8f && can_see_player) {
                if (s.role == StalkerRole::Melee) {
                    s.state = StalkerState::Lunging;
                    s.state_timer = 0.0f;
                    s.just_lunged = true;
                    result.any_lunge = true;
                    VF_LOG_INFO("VoidStalker", "Melee Stalker " << s.id << " ENRAGES and LUNGES at close player!");
                    break;
                } else {
                    // Shooter keeps distance and never lunges
                    s.state = StalkerState::Circling;
                    s.state_timer = 0.0f;
                    break;
                }
            }

            // Lost interest if very far away (bypassed if pursuing an attacker who damaged us)
            if (!s.is_pursuing_attacker && dist_to_player > s.detection_range * 1.6f) {
                s.state = StalkerState::Idle;
                s.state_timer = 0.0f;
                break;
            }

            // Headlamp light: ONLY flee if at critical health (<= 12 HP)
            if (s.is_in_light) {
                if (s.hp <= s.flee_hp_threshold) {
                    s.state = StalkerState::Fleeing;
                    s.state_timer = 0.0f;
                    VF_LOG_INFO("VoidStalker", "Wounded Stalker " << s.id << " retreats from light");
                    break;
                }
                // Healthy Melee: illuminated within 9m -> provokes an aggressive lunge attack!
                if (dist_to_player < 9.0f && s.role == StalkerRole::Melee && can_see_player) {
                    s.state = StalkerState::Lunging;
                    s.state_timer = 0.0f;
                    s.just_lunged = true;
                    result.any_lunge = true;
                    VF_LOG_INFO("VoidStalker", "Melee Stalker " << s.id << " PROVOKED by headlamp beam! Lunging into light!");
                    break;
                }
            }

            // Ranged Attack: Launch crystalline void spine if at mid-range and has LOS (Shooters ONLY!)
            if (s.role == StalkerRole::Shooter && s.projectile_cooldown <= 0.0f && dist_to_player > 4.5f && dist_to_player < 14.0f &&
                has_line_of_sight(s.position + glm::vec3(0, 0.4f, 0), player_pos, world)) {
                glm::vec3 spawn_pt = s.position + glm::vec3(0.0f, 0.4f, 0.0f);
                float flight_time = dist_to_player / 15.0f;
                glm::vec3 predicted_pos = player_pos + player_vel * (flight_time * 0.75f);
                glm::vec3 aim_dir = glm::normalize(predicted_pos + glm::vec3(0.0f, 0.5f, 0.0f) - spawn_pt);
                m_projectiles.push_back({
                    m_next_proj_id++,
                    spawn_pt + aim_dir * 0.4f,
                    aim_dir * 15.0f,
                    2.5f,
                    10.0f,
                    true
                });
                s.projectile_cooldown = 2.4f;
                s.just_fired_spine = true;
                result.any_projectile_fired = true;
                VF_LOG_INFO("VoidStalker", "Shooter Stalker " << s.id << " FIRED predictive crystalline void spine volley at player!");
            }

            // Eerie burst-and-weave pursuit towards target_pos
            glm::vec3 to_tgt_xz(s.target_pos.x - s.position.x, 0.0f, s.target_pos.z - s.position.z);
            float horiz_to_tgt = glm::length(to_tgt_xz);
            if (horiz_to_tgt > 0.1f) {
                glm::vec3 dir_xz = to_tgt_xz / horiz_to_tgt;
                glm::vec3 perp(-dir_xz.z, 0.0f, dir_xz.x);
                float burst = 0.50f + 0.70f * std::abs(std::sin(s.walk_cycle * 0.45f));
                float weave = 0.45f * std::sin(s.walk_cycle * 1.2f);
                glm::vec3 move_xz = glm::normalize(dir_xz + perp * weave);
                s.velocity.x = move_xz.x * s.move_speed * burst;
                s.velocity.z = move_xz.z * s.move_speed * burst;
                s.position.x += s.velocity.x * dt;
                s.position.z += s.velocity.z * dt;
                s.yaw = std::atan2(move_xz.x, move_xz.z);
            }

            // Transition to circling at mid-range (6.8m to 12.0m) if in open sight
            if (can_see_player && dist_to_player <= 12.0f && dist_to_player >= 6.8f) {
                s.state = StalkerState::Circling;
                s.state_timer = 0.0f;
            }

            // Immediate lunge if within lunge range (Melee ONLY!)
            if (can_see_player && s.role == StalkerRole::Melee && dist_to_player <= s.lunge_range) {
                s.state = StalkerState::Lunging;
                s.state_timer = 0.0f;
                s.just_lunged = true;
                result.any_lunge = true;
                VF_LOG_INFO("VoidStalker", "Melee Stalker " << s.id << " LUNGING at close player!");
            }
            break;
        }

        case StalkerState::Circling: {
            // If player approaches within 5.8m:
            if (dist_to_player < 5.8f && can_see_player) {
                if (s.role == StalkerRole::Melee) {
                    s.state = StalkerState::Lunging;
                    s.state_timer = 0.0f;
                    s.just_lunged = true;
                    result.any_lunge = true;
                    VF_LOG_INFO("VoidStalker", "Melee Stalker " << s.id << " springs aggressive ambush from circling orbit!");
                    break;
                } else {
                    if (dist_to_player < 4.0f) {
                        s.state = StalkerState::Fleeing;
                        s.state_timer = 0.0f;
                        break;
                    }
                }
            }

            // Only flee if low HP
            if (s.is_in_light && s.hp <= s.flee_hp_threshold) {
                s.state = StalkerState::Fleeing;
                s.state_timer = 0.0f;
                break;
            }

            // Ranged Attack while circling (Shooters ONLY):
            if (s.role == StalkerRole::Shooter && s.projectile_cooldown <= 0.0f && dist_to_player > 4.0f && dist_to_player < 14.0f &&
                has_line_of_sight(s.position + glm::vec3(0, 0.4f, 0), player_pos, world)) {
                glm::vec3 spawn_pt = s.position + glm::vec3(0.0f, 0.4f, 0.0f);
                float flight_time = dist_to_player / 15.0f;
                glm::vec3 predicted_pos = player_pos + player_vel * (flight_time * 0.75f);
                glm::vec3 aim_dir = glm::normalize(predicted_pos + glm::vec3(0.0f, 0.5f, 0.0f) - spawn_pt);
                m_projectiles.push_back({
                    m_next_proj_id++,
                    spawn_pt + aim_dir * 0.4f,
                    aim_dir * 15.0f,
                    2.5f,
                    10.0f,
                    true
                });
                s.projectile_cooldown = 2.2f;
                s.just_fired_spine = true;
                result.any_projectile_fired = true;
                VF_LOG_INFO("VoidStalker", "Shooter Stalker " << s.id << " FIRED predictive void spine from flanking circle!");
            }

            // If line of sight lost for > 3.0s, break circling and search
            if (!can_see_player && s.lost_los_timer > 3.0f) {
                s.state = StalkerState::Investigating;
                s.investigation_target = s.last_seen_player_pos;
                s.investigation_timer = 3.5f;
                s.state_timer = 0.0f;
                break;
            }

            // Eerie variable-radius orbit with height undulation (wall-climber pattern)
            float orbit_radius = 5.0f + std::sin(s.glow_phase * 0.65f) * 1.8f;
            float orbit_angle  = s.glow_phase * 0.85f;
            glm::vec3 orbit_offset(
                std::cos(orbit_angle) * orbit_radius,
                std::sin(s.glow_phase * 1.1f) * 0.75f,
                std::sin(orbit_angle) * orbit_radius
            );
            glm::vec3 target = player_pos + orbit_offset;
            glm::vec3 dir = glm::normalize(target - s.position);
            float orbit_burst = 0.65f + 0.50f * std::abs(std::sin(s.walk_cycle * 0.40f));
            s.velocity = dir * s.circle_speed * orbit_burst;
            s.position += s.velocity * dt;
            s.yaw = std::atan2((player_pos.x - s.position.x), (player_pos.z - s.position.z));

            // Lunge conditions: player drilling, or circled for 1.8s (Melee ONLY!)
            bool should_lunge = (s.role == StalkerRole::Melee) && (
                                (player_is_drilling && dist_to_player < s.lunge_range * 1.5f) ||
                                (s.state_timer > 1.8f && dist_to_player < s.lunge_range * 1.3f));

            if (should_lunge && can_see_player) {
                s.state = StalkerState::Lunging;
                s.state_timer = 0.0f;
                s.just_lunged = true;
                result.any_lunge = true;
                VF_LOG_INFO("VoidStalker", "Melee Stalker " << s.id << " committed to LUNGE from flanking circle!");
            }

            if (dist_to_player > 13.0f) {
                s.state = StalkerState::Stalking;
                s.state_timer = 0.0f;
            }
            break;
        }

        case StalkerState::Lunging: {
            if (s.role != StalkerRole::Melee) {
                s.state = StalkerState::Circling;
                break;
            }
            s.slash_fx_timer = 0.5f;

            glm::vec3 to_target = player_pos - s.position;
            float target_dist = glm::length(to_target);
            glm::vec3 dir = (target_dist > 0.05f) ? (to_target / target_dist) : glm::vec3(0.0f, 0.0f, 1.0f);
            float lunge_ramp = (s.state_timer < 0.12f)
                ? (s.state_timer / 0.12f) * (s.state_timer / 0.12f)
                : 1.0f;
            s.velocity = dir * s.lunge_speed * lunge_ramp;
            s.position += s.velocity * dt;
            s.yaw = std::atan2(dir.x, dir.z);

            // Collision check — did we reach the player?
            if (dist_to_player < 1.6f && s.attack_cooldown <= 0.0f) {
                result.total_damage += s.damage;
                s.just_hit_player = true;
                result.any_melee_hit = true;
                s.attack_cooldown = 1.0f;
                VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " SLASHED player (-" << s.damage << " HP)!");

                if (s.hp <= s.flee_hp_threshold) {
                    s.state = StalkerState::Fleeing;
                    s.state_timer = 0.0f;
                } else {
                    s.state = StalkerState::Circling;
                    s.state_timer = 0.0f;
                }
            }

            // Lunge timeout — overshot or completed
            if (s.state_timer > 1.2f) {
                s.state = StalkerState::Circling;
                s.state_timer = 0.0f;
            }
            break;
        }

        case StalkerState::Stunned: {
            s.stun_timer -= dt;
            s.velocity = glm::vec3(0.0f);
            if (s.stun_timer <= 0.0f) {
                s.state = StalkerState::Fleeing;
                s.state_timer = 0.0f;
            }
            break;
        }

        case StalkerState::Fleeing: {
            glm::vec3 flee_dir = glm::normalize(s.position - player_pos);
            flee_dir.x += std::sin(s.glow_phase * 3.0f) * 0.3f;
            flee_dir.z += std::cos(s.glow_phase * 3.0f) * 0.3f;
            if (glm::length(flee_dir) > 0.1f) {
                flee_dir = glm::normalize(flee_dir);
            }
            s.velocity = flee_dir * s.move_speed * 1.5f;
            s.position += s.velocity * dt;
            s.yaw = std::atan2(flee_dir.x, flee_dir.z);

            if (s.hp <= s.flee_hp_threshold && s.state_timer > 5.0f) {
                s.state = StalkerState::Dead;
                s.just_died = true;
                result.stalkers_killed++;
            }

            if (s.state_timer > 3.0f && s.hp > s.flee_hp_threshold) {
                s.state = StalkerState::Stalking;
                s.state_timer = 0.0f;
            }
            break;
        }

        case StalkerState::Dead:
            break;
        }

        // Clamp position to sector bounds
        s.position.x = std::clamp(s.position.x, 4.5f, 67.5f);
        s.position.y = std::clamp(s.position.y, 4.5f, 26.0f);
        s.position.z = std::clamp(s.position.z, 4.5f, 67.5f);

        // Simple gravity: stalker drops if in air
        int sx = static_cast<int>(std::floor(s.position.x));
        int sy = static_cast<int>(std::floor(s.position.y - 0.1f));
        int sz = static_cast<int>(std::floor(s.position.z));
        Voxel below = world.get_voxel(sx, sy, sz);
        if (below.material_id == MAT_AIR) {
            s.position.y -= 4.0f * dt; // Gravity
        }

        // Don't clip into solid blocks
        int cx = static_cast<int>(std::floor(s.position.x));
        int cy = static_cast<int>(std::floor(s.position.y));
        int cz = static_cast<int>(std::floor(s.position.z));
        Voxel current_v = world.get_voxel(cx, cy, cz);
        if (current_v.material_id != MAT_AIR && current_v.material_id != MAT_GAS &&
            current_v.material_id != MAT_VOLATILE_SMOKE) {
            s.position.y = static_cast<float>(cy) + 1.1f;
        }
    }

    // === Update Active Void Spine Projectiles ===
    for (auto& p : m_projectiles) {
        if (!p.active) continue;
        p.lifetime -= dt;
        if (p.lifetime <= 0.0f) {
            p.active = false;
            continue;
        }

        p.position += p.velocity * dt;

        // Voxel collision check (blocks or industrial bulkheads block spines!)
        int vx = static_cast<int>(std::floor(p.position.x));
        int vy = static_cast<int>(std::floor(p.position.y));
        int vz = static_cast<int>(std::floor(p.position.z));
        Voxel vox = world.get_voxel(vx, vy, vz);
        if (vox.material_id != MAT_AIR && vox.material_id != MAT_GAS && vox.material_id != MAT_VOLATILE_SMOKE) {
            p.active = false;
            continue;
        }

        // Player collision check
        float p_dist = glm::distance(p.position, player_pos + glm::vec3(0.0f, 0.5f, 0.0f));
        if (p_dist < 1.1f) {
            p.active = false;
            result.total_damage += p.damage;
            VF_LOG_INFO("VoidStalker", "Void spine IMPALED player (-" << p.damage << " HP)!");
        }
    }

    // Clean up inactive projectiles
    m_projectiles.erase(
        std::remove_if(m_projectiles.begin(), m_projectiles.end(), [](const VoidSpikeProjectile& p) {
            return !p.active;
        }),
        m_projectiles.end()
    );

    return result;
}

void VoidStalkerManager::apply_sonar_stun(const glm::vec3& origin, float radius) {
    for (auto& s : m_stalkers) {
        if (s.state == StalkerState::Dead) continue;
        float dist = glm::distance(s.position, origin);
        if (dist < radius) {
            s.state = StalkerState::Stunned;
            s.state_timer = 0.0f;
            s.stun_timer = 3.0f; // 3 second stun
            s.velocity = glm::vec3(0.0f);
            VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " STUNNED by sonar pulse (dist=" << dist << "m)");
        }
    }
}

bool VoidStalkerManager::damage_nearest(const glm::vec3& origin, float radius, float damage) {
    float best_dist = radius;
    VoidStalker* nearest = nullptr;

    for (auto& s : m_stalkers) {
        if (s.state == StalkerState::Dead) continue;
        float dist = glm::distance(s.position, origin);
        if (dist < best_dist) {
            best_dist = dist;
            nearest = &s;
        }
    }

    if (nearest) {
        nearest->hp -= damage;
        if (nearest->hp <= 0.0f) {
            nearest->state = StalkerState::Dead;
            nearest->just_died = true;
            VF_LOG_INFO("VoidStalker", "Stalker " << nearest->id << " KILLED (damage=" << damage << ")");
        } else if (nearest->hp <= nearest->flee_hp_threshold) {
            nearest->state = StalkerState::Fleeing;
            nearest->state_timer = 0.0f;
            VF_LOG_INFO("VoidStalker", "Stalker " << nearest->id << " critically injured (HP=" << nearest->hp << ") -> FLEEING!");
        } else {
            // Activate persistent retaliation pursuit against the attacking player
            nearest->is_pursuing_attacker = true;
            nearest->pursuit_lost_timer = 0.0f;
            nearest->target_pos = origin;

            if (nearest->role == StalkerRole::Melee) {
                // Healthy Melee -> ENRAGES into counter-attack lunge!
                nearest->state = StalkerState::Lunging;
                nearest->state_timer = 0.0f;
                nearest->just_lunged = true;
                VF_LOG_INFO("VoidStalker", "Melee Stalker " << nearest->id << " ENRAGED by player damage (HP=" << nearest->hp << ") -> COUNTER-ATTACKING & PURSUING!");
            } else {
                // Healthy Shooter -> Repositions to flanking circle to return fire with void spines!
                nearest->state = StalkerState::Circling;
                nearest->state_timer = 0.0f;
                nearest->projectile_cooldown = 0.2f;
                VF_LOG_INFO("VoidStalker", "Shooter Stalker " << nearest->id << " REPOSITIONS after damage to return fire and pursue!");
            }
        }
        return true;
    }
    return false;
}

int VoidStalkerManager::active_count() const {
    int count = 0;
    for (const auto& s : m_stalkers) {
        if (s.state != StalkerState::Dead) ++count;
    }
    return count;
}

void VoidStalkerManager::remove_dead() {
    m_stalkers.erase(
        std::remove_if(m_stalkers.begin(), m_stalkers.end(),
            [](const VoidStalker& s) { return s.state == StalkerState::Dead; }),
        m_stalkers.end()
    );
}

void VoidStalkerManager::reset() {
    m_stalkers.clear();
    m_projectiles.clear();
    m_next_id = 1;
    m_next_proj_id = 1;
    m_spawn_cooldown = 0.0f;
    m_global_screech_cooldown = 25.0f;
    m_has_prev_player_pos = false;
    m_prev_player_pos = glm::vec3(0.0f);
}

} // namespace Voidfall
