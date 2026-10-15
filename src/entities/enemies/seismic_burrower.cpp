#include "seismic_burrower.hpp"
#include "../carcass_manager.hpp"
#include "../../voxel/world.hpp"
#include "../../voxel/level_shapes.hpp"
#include "../../core/logger.hpp"
#include "../../ai/aberrant_ai.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace Voidfall {

void SeismicBurrowerManager::spawn_burrower(const glm::vec3& pos, float hp_multiplier, bool emerging) {
    SeismicBurrower b;
    b.id = m_next_id++;
    b.position = pos;
    b.target_pos = pos;
    b.hp = 120.0f * hp_multiplier;
    b.max_hp = b.hp;
    b.state = emerging ? BurrowerState::Breaching : BurrowerState::Burrowing;
    b.state_timer = 0.0f;
    b.yaw = 0.0f;
    b.pitch = 0.0f;
    b.roar_timer = 5.0f + static_cast<float>((b.id * 37) % 110) * 0.1f; // Staggered initial roar
    b.grind_timer = emerging ? 0.15f : (1.5f + static_cast<float>((b.id * 17) % 30) * 0.1f);
    b.chatter_timer = 2.0f + static_cast<float>((b.id * 29) % 35) * 0.1f;
    b.just_roared = emerging;
    b.just_ground = emerging;
    b.just_chattered = false;
    b.just_breached = emerging;
    m_burrowers.push_back(b);
    VF_LOG_INFO("SeismicBurrower", "Spawned Seismic Burrower " << b.id << " at ("
                << pos.x << ", " << pos.y << ", " << pos.z << ") with HP=" << b.hp
                << (emerging ? " (Emerging Breach)" : ""));
}

void SeismicBurrowerManager::spawn_emerging_burrower(const glm::vec3& pos, const glm::vec3& facing_dir, float hp_multiplier) {
    spawn_burrower(pos, hp_multiplier, true);
    if (!m_burrowers.empty()) {
        auto& b = m_burrowers.back();
        glm::vec3 fwd = facing_dir;
        if (glm::length(fwd) < 0.01f) fwd = glm::vec3(0.0f, 0.0f, 1.0f);
        else fwd = glm::normalize(fwd);
        b.yaw = std::atan2(fwd.x, fwd.z);
        b.pitch = -std::asin(std::clamp(fwd.y, -1.0f, 1.0f));
        b.just_breached = true;
        b.just_ground = true;
        b.just_roared = true;
    }
}

void SeismicBurrowerManager::spawn_dormant_burrower(const glm::vec3& pos, float hp_multiplier) {
    SeismicBurrower b;
    b.id = m_next_id++;
    b.position = pos;
    b.target_pos = pos;
    b.hp = 120.0f * hp_multiplier;
    b.max_hp = b.hp;
    b.state = BurrowerState::Dormant;
    b.state_timer = 0.0f;
    b.yaw = 0.0f;
    b.pitch = 0.0f;
    b.roar_timer = 14.0f + static_cast<float>((b.id * 37) % 110) * 0.1f;
    b.grind_timer = 2.0f;
    b.chatter_timer = 3.5f;
    b.just_roared = false;
    b.just_ground = false;
    b.just_chattered = false;
    m_burrowers.push_back(b);
    VF_LOG_INFO("SeismicBurrower", "Spawned Dormant Seismic Burrower " << b.id << " at ("
                << pos.x << ", " << pos.y << ", " << pos.z << ") with HP=" << b.hp);
}

void SeismicBurrowerManager::spawn_hazard_wave(const glm::vec3& player_pos, const World& world) {
    if (m_spawn_cooldown > 0.0f) return;

    // Pick a spawn point 14-20 meters away embedded in rock wall
    float angle = static_cast<float>(rand() % 360) * glm::pi<float>() / 180.0f;
    float dist = 14.0f + static_cast<float>(rand() % 6);
    glm::vec3 spawn_pos = player_pos + glm::vec3(std::cos(angle) * dist, 1.0f + (rand() % 4), std::sin(angle) * dist);

    glm::vec3 facing = glm::normalize(player_pos - spawn_pos);
    spawn_emerging_burrower(spawn_pos, facing);
    m_spawn_cooldown = 45.0f; // Limit frequency of burrower spawns
}

SeismicBurrowerManager::FrameResult SeismicBurrowerManager::update(
    float dt,
    const glm::vec3& player_pos,
    World& world,
    const std::vector<SoundEvent>& sound_events,
    bool player_is_crouching
) {
    FrameResult result;
    if (m_spawn_cooldown > 0.0f) {
        m_spawn_cooldown -= dt;
    }
    if (m_global_roar_cooldown > 0.0f) {
        m_global_roar_cooldown -= dt;
    }
    if (m_global_grind_cooldown > 0.0f) {
        m_global_grind_cooldown -= dt;
    }
    if (m_global_chatter_cooldown > 0.0f) {
        m_global_chatter_cooldown -= dt;
    }

    for (auto& b : m_burrowers) {
        glm::vec3 frame_start_pos = b.position;
        b.just_roared = false;
        b.just_ground = false;
        b.just_chattered = false;
        b.just_breached = false;
        b.just_rammed = false;
        b.just_triggered_cavein = false;
        b.just_stunned = false;
        b.just_died = false;
        b.just_stepped = false;

        if (b.is_dead()) continue;

        // Ambient Subterranean Roars echoing through caverns (Only when actively moving/burrowing, not dormant)
        if (b.state != BurrowerState::Dead && b.state != BurrowerState::Stunned && b.state != BurrowerState::Dormant) {
            b.roar_timer -= dt;
            if (b.roar_timer <= 0.0f && m_global_roar_cooldown <= 0.0f) {
                b.roar_timer = 14.0f + static_cast<float>((b.id * 43 + static_cast<int>(b.state_timer * 100)) % 140) * 0.1f;
                b.just_roared = true;
                result.any_roar = true;
                m_global_roar_cooldown = 8.0f;
                VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " emitted subterranean roar at ("
                            << b.position.x << ", " << b.position.y << ", " << b.position.z << ")");
            }
        }

        b.state_timer += dt;
        b.pulse_phase += dt * 3.0f;
        b.cutter_angle += dt * 12.0f; // Continuously spinning drill borer
        b.segment_wiggle += dt * 5.0f;

        if (b.attack_cooldown > 0.0f) b.attack_cooldown -= dt;
        if (b.shockwave_cooldown > 0.0f) b.shockwave_cooldown -= dt;

        // Acoustic hearing: Check if any heavy acoustic disturbances (drilling, explosives, footsteps) occurred
        if (b.sound_target_timer > 0.0f) {
            float sound_decay_rate = player_is_crouching ? 2.5f : 1.0f;
            b.sound_target_timer -= dt * sound_decay_rate;
            if (b.sound_target_timer <= 0.0f) {
                b.has_sound_target = false;
            }
        }

        float best_salience = 0.0f;
        for (const auto& snd : sound_events) {
            float snd_dist = glm::distance(b.position, snd.position);
            // Burrowers sense seismic ground vibrations up to 1.5x sound radius through solid rock
            if (snd_dist <= snd.audible_radius * 1.5f) {
                float salience = snd.intensity / std::max(1.0f, snd_dist);
                if (snd.type == SoundEventType::DemolitionBlast || snd.type == SoundEventType::SeismicTremor) {
                    salience *= 3.5f; // Heavy tectonic shocks heavily attract subterranean burrowers
                } else if (snd.type == SoundEventType::Gunshot) {
                    salience *= 3.0f; // Gunfire shocks command immediate seismic attention
                } else if (snd.type == SoundEventType::DrillVibration) {
                    salience *= 3.0f; // Mining drill vibrations strongly attract burrowers
                } else if (snd.type == SoundEventType::BulletImpact) {
                    salience *= 2.2f;
                }
                if (salience > best_salience) {
                    best_salience = salience;
                    b.sound_target = snd.position;
                    b.has_sound_target = true;
                    b.sound_target_timer = player_is_crouching ? 3.0f : 6.0f;
                }
            }
        }

        // Retaliation pursuit and escape tracking
        float dist_to_player = glm::distance(b.position, player_pos);
        if (b.is_pursuing_attacker) {
            // Relentless pursuit: Once set on killing the player, the burrower does NOT lose interest easily
            // It only breaks pursuit if the player flees to extreme distance (> 35m) for extended time (15s)
            if (dist_to_player > b.pursuit_break_dist) {
                b.pursuit_lost_timer += dt;
                if (b.pursuit_lost_timer >= b.pursuit_break_time) {
                    b.is_pursuing_attacker = false;
                    b.pursuit_lost_timer = 0.0f;
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " lost interest after player fled to distant cavern ("
                                << b.pursuit_break_time << "s, dist=" << dist_to_player << "m)");
                }
            } else {
                b.pursuit_lost_timer = 0.0f;
            }
        }

        // Target selection: If pursuing player, locked on, or player is detected within seismic proximity,
        // lock authoritatively onto player_pos!
        glm::vec3 active_target = player_pos;
        bool is_locked_on_player = (b.state == BurrowerState::Charging ||
                                    b.state == BurrowerState::Breaching ||
                                    b.state == BurrowerState::Enraged ||
                                    b.is_pursuing_attacker ||
                                    (!player_is_crouching && dist_to_player < 18.0f) ||
                                    (player_is_crouching && dist_to_player < 6.0f));

        if (!is_locked_on_player) {
            if (b.has_sound_target && b.state == BurrowerState::Burrowing) {
                active_target = b.sound_target;
                if (glm::distance(b.position, b.sound_target) < 3.0f) {
                    b.has_sound_target = false;
                }
            } else if (b.state == BurrowerState::Burrowing) {
                // Autonomous subterranean wandering within assigned territory zone
                b.patrol_timer -= dt;
                float dist_to_wp = glm::distance(b.position, b.patrol_waypoint);
                if (dist_to_wp < 4.0f || b.patrol_timer <= 0.0f) {
                    if (b.has_patrol_zone) {
                        b.patrol_waypoint = AberrantAI::sample_wander_waypoint(b.patrol_zone, b.position, world, b.id * 31 + static_cast<uint32_t>(b.state_timer * 10.0f));
                    } else {
                        PatrolZone default_z;
                        default_z.center = b.position;
                        default_z.radius = 16.0f;
                        default_z.has_bounds = false;
                        b.patrol_waypoint = AberrantAI::sample_wander_waypoint(default_z, b.position, world, b.id * 31 + static_cast<uint32_t>(b.state_timer * 10.0f));
                    }
                    b.patrol_timer = 12.0f + static_cast<float>((b.id * 19) % 80) * 0.1f;
                }
                active_target = b.patrol_waypoint;
            }
        }
        b.target_pos = active_target;

        glm::vec3 to_target = active_target - b.position;
        float dist_to_target = glm::length(to_target);
        glm::vec3 dir_to_target = (dist_to_target > 0.001f) ? glm::normalize(to_target) : glm::vec3(0, 0, 1);

        // Calculate facing angles
        float target_yaw = std::atan2(dir_to_target.x, dir_to_target.z);
        float target_pitch = -std::asin(std::clamp(dir_to_target.y, -1.0f, 1.0f));

        // Smooth angle tracking (amplified when aggressively pursuing / charging target)
        float eff_turn_rate = (b.is_pursuing_attacker || b.state == BurrowerState::Charging) ? 4.2f : b.turn_rate;
        if (dist_to_target < 10.0f) {
            eff_turn_rate = std::max(eff_turn_rate, 4.8f);
        }
        float yaw_diff = target_yaw - b.yaw;
        while (yaw_diff > glm::pi<float>()) yaw_diff -= 2.0f * glm::pi<float>();
        while (yaw_diff < -glm::pi<float>()) yaw_diff += 2.0f * glm::pi<float>();
        b.yaw += std::clamp(yaw_diff, -eff_turn_rate * dt, eff_turn_rate * dt);
        b.pitch += std::clamp(target_pitch - b.pitch, -eff_turn_rate * dt, eff_turn_rate * dt);

        // Accurate 3D forward facing orientation (pitch: -sin(pitch) points UP when target is above)
        glm::vec3 facing(
            std::sin(b.yaw) * std::cos(b.pitch),
            -std::sin(b.pitch),
            std::cos(b.yaw) * std::cos(b.pitch)
        );

        // Check if head position is inside solid rock or open air
        glm::ivec3 head_vox(
            static_cast<int>(std::floor(b.position.x)),
            static_cast<int>(std::floor(b.position.y)),
            static_cast<int>(std::floor(b.position.z))
        );
        bool in_solid = AberrantAI::is_point_solid(world, b.position);

        // State Machine
        switch (b.state) {
            case BurrowerState::Dormant: {
                // Dormant burrower deep in rock: wake if sound occurs, damaged/pursued, or player approaches within 6m
                float wake_dist = player_is_crouching ? 0.9f : 6.0f;
                if (b.has_sound_target || b.is_pursuing_attacker || dist_to_player < wake_dist) {
                    b.state = BurrowerState::Burrowing;
                    b.state_timer = 0.0f;
                    if (m_global_roar_cooldown <= 0.0f) {
                        b.just_roared = true;
                        result.any_roar = true;
                        m_global_roar_cooldown = 10.0f;
                    }
                    VF_LOG_INFO("SeismicBurrower", "Dormant Burrower " << b.id << " AWAKENED (sound=" << b.has_sound_target
                                << ", pursuing=" << b.is_pursuing_attacker << ", dist=" << dist_to_player << "m)!");
                }
                break;
            }

            case BurrowerState::Burrowing: {
                // Ground tremor / rumble based on proximity to player
                float rumble_dist = b.is_pursuing_attacker ? 26.0f : 20.0f;
                b.rumble_intensity = std::clamp(1.0f - (dist_to_player / rumble_dist), 0.0f, 1.0f);
                result.max_rumble = std::max(result.max_rumble, b.rumble_intensity);

                // Periodic drilling & digging noises as borer cutter spins through rock walls
                b.grind_timer -= dt;
                if (b.grind_timer <= 0.0f) {
                    b.grind_timer = 1.2f + static_cast<float>((b.id * 19) % 15) * 0.05f;
                    if (m_global_grind_cooldown <= 0.0f) {
                        b.just_ground = true;
                        result.any_grind = true;
                        m_global_grind_cooldown = 0.8f;
                    }
                }

                // Chattering clicks as cutter teeth and mandibles chatter against rock strata
                b.chatter_timer -= dt;
                if (b.chatter_timer <= 0.0f) {
                    b.chatter_timer = 3.0f + static_cast<float>((b.id * 23) % 25) * 0.1f;
                    if (m_global_chatter_cooldown <= 0.0f) {
                        b.just_chattered = true;
                        result.any_chatter = true;
                        m_global_chatter_cooldown = 2.0f;
                    }
                }

                // Subterranean hazard and impenetrable barrier avoidance (bedrock / bulkheads / hazards)
                glm::vec3 probe_pos = b.position + facing * 2.5f;
                glm::ivec3 pv(std::floor(probe_pos.x), std::floor(probe_pos.y), std::floor(probe_pos.z));
                Voxel pv_vox = world.get_voxel(pv.x, pv.y, pv.z);
                if (pv_vox.material_id == MAT_DREDGE_BEDROCK || pv_vox.material_id == MAT_INDUSTRIAL_BULKHEAD ||
                    IsLethalHazard(pv_vox.material_id) || pv_vox.material_id == MAT_COOLANT) {
                    b.yaw += 1.2f * dt * 4.0f;
                    b.pitch = std::clamp(b.pitch + 0.5f * dt * 4.0f, -0.6f, 0.6f);
                    facing = glm::vec3(
                        std::sin(b.yaw) * std::cos(b.pitch),
                        -std::sin(b.pitch),
                        std::cos(b.yaw) * std::cos(b.pitch)
                    );
                }

                // Anti-stuck displacement tracking
                b.stuck_checkpoint_timer += dt;
                if (b.stuck_checkpoint_timer >= 0.25f) {
                    float net_disp = glm::distance(b.position, b.stuck_checkpoint_pos);
                    if (net_disp < 0.10f) {
                        b.stuck_timer += b.stuck_checkpoint_timer;
                    } else {
                        b.stuck_timer = std::max(0.0f, b.stuck_timer - b.stuck_checkpoint_timer * 1.5f);
                    }
                    b.stuck_checkpoint_pos = b.position;
                    b.stuck_checkpoint_timer = 0.0f;
                }

                if (b.stuck_timer > 0.40f) {
                    // Turn away from impenetrable obstruction
                    b.yaw += 2.0f;
                    b.pitch = -b.pitch;
                    b.velocity = -b.velocity * 0.5f;
                    if (b.has_patrol_zone) {
                        b.patrol_waypoint = AberrantAI::sample_wander_waypoint(b.patrol_zone, b.position, world, b.id * 31 + static_cast<uint32_t>(b.state_timer * 10.0f));
                    }
                    b.patrol_timer = 12.0f;
                    b.stuck_timer = 0.0f;
                }

                // Move forward through ground (surging speed when pursuing attacker)
                float eff_burrow_speed = b.is_pursuing_attacker ? 4.5f : b.burrow_speed;
                glm::vec3 old_pos = b.position;
                float move_dist = eff_burrow_speed * dt;
                b.velocity = facing * eff_burrow_speed;
                b.position += b.velocity * dt;

                // Swept excavation along movement path from old_pos to new position (distance-throttled to avoid 60fps chunk remeshing storm)
                b.excavation_dist_accum += move_dist;
                if (b.excavation_dist_accum >= 0.5f) {
                    b.excavation_dist_accum = 0.0f;
                    std::unordered_set<ChunkPos, ChunkPosHash> burrow_affected_chunks;
                    for (float step = 0.0f; step <= move_dist + 0.8f; step += 0.8f) {
                        glm::ivec3 center = glm::ivec3(glm::floor(old_pos + facing * (step + 1.0f)));
                        for (int dx = -1; dx <= 1; ++dx) {
                            for (int dy = -1; dy <= 1; ++dy) {
                                for (int dz = -1; dz <= 1; ++dz) {
                                    glm::ivec3 p = center + glm::ivec3(dx, dy, dz);
                                    Voxel v = world.get_voxel(p.x, p.y, p.z);
                                    if (v.is_solid() && v.material_id != MAT_DREDGE_BEDROCK && v.material_id != MAT_INDUSTRIAL_BULKHEAD) {
                                        world.set_voxel(p.x, p.y, p.z, Voxel{MAT_AIR, 0}, false);
                                        if (result.excavated_voxels.size() < 16) {
                                            result.excavated_voxels.push_back({p, v.material_id});
                                        }

                                        int cx = p.x >> 5;
                                        int cy = p.y >> 5;
                                        int cz = p.z >> 5;
                                        burrow_affected_chunks.insert(ChunkPos{cx, cy, cz});
                                        int lx = p.x & 31;
                                        int ly = p.y & 31;
                                        int lz = p.z & 31;
                                        if (lx == 0) burrow_affected_chunks.insert(ChunkPos{cx - 1, cy, cz});
                                        if (lx == 31) burrow_affected_chunks.insert(ChunkPos{cx + 1, cy, cz});
                                        if (ly == 0) burrow_affected_chunks.insert(ChunkPos{cx, cy - 1, cz});
                                        if (ly == 31) burrow_affected_chunks.insert(ChunkPos{cx, cy + 1, cz});
                                        if (lz == 0) burrow_affected_chunks.insert(ChunkPos{cx, cy, cz - 1});
                                        if (lz == 31) burrow_affected_chunks.insert(ChunkPos{cx, cy, cz + 1});
                                    }
                                }
                            }
                        }
                    }
                    for (const auto& cpos : burrow_affected_chunks) {
                        Chunk* c = world.get_chunk(cpos);
                        if (c) {
                            c->MarkDirty();
                            world.queue_chunk_for_meshing(cpos);
                        }
                    }
                }

                // If emerged into open cavern with line of sight to player, transition to Breaching!
                bool can_breach = dist_to_player < 14.0f;
                if (player_is_crouching && !b.is_pursuing_attacker) {
                    // When player is sneaking and burrower is not actively pursuing an attacker,
                    // only breach on close physical contact or direct line-of-sight in front within 4.5m
                    float fwd_dot = glm::dot(facing, dir_to_target);
                    can_breach = (dist_to_player < 0.9f) || (dist_to_player <= 4.5f && fwd_dot >= 0.5f);
                }
                if (!in_solid && can_breach) {
                    b.state = BurrowerState::Breaching;
                    b.state_timer = 0.0f;
                    b.just_breached = true;
                    b.is_pursuing_attacker = true;
                    if (m_global_roar_cooldown <= 0.0f) {
                        b.just_roared = true; // Terrifying breach roar!
                        result.any_roar = true;
                        m_global_roar_cooldown = 8.0f;
                    }
                    result.any_breach = true;
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " BREACHED into open cavern!");
                }
                break;
            }

            case BurrowerState::Breaching: {
                // Physical rotary emergence into the cave
                b.velocity *= 0.85f;
                b.cutter_angle += dt * 36.0f; // High-RPM cutter drill spin during breach
                b.grind_timer -= dt;
                if (b.grind_timer <= 0.0f) {
                    b.grind_timer = 0.20f;
                    b.just_ground = true;
                    result.any_grind = true;
                }
                b.rumble_intensity = std::clamp(1.2f - (dist_to_player / 22.0f), 0.0f, 1.0f);
                result.max_rumble = std::max(result.max_rumble, b.rumble_intensity);

                if (b.state_timer > 1.2f) {
                    b.state = BurrowerState::Charging;
                    b.state_timer = 0.0f;
                    b.excavation_dist_accum = 1.0f; // Ready for immediate forward destruction
                    b.yaw = target_yaw;
                    b.pitch = target_pitch;
                    result.any_charge = true;
                    if (m_global_roar_cooldown <= 0.0f) {
                        b.just_roared = true;
                        result.any_roar = true;
                        m_global_roar_cooldown = 6.0f;
                    }
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " INITIATES KINETIC CHARGE!");
                }
                break;
            }

            case BurrowerState::Charging: {
                // High-speed open cavern ramming charge with swept collision & destruction
                b.cutter_angle += dt * 48.0f;
                b.rumble_intensity = std::clamp((1.0f - (dist_to_player / 28.0f)) * 1.5f, 0.0f, 1.0f);
                result.max_rumble = std::max(result.max_rumble, b.rumble_intensity);

                glm::vec3 old_pos = b.position;
                float move_dist = b.charge_speed * dt;
                glm::vec3 next_pos = b.position + facing * move_dist;

                // Swept collision check along path to prevent tunneling through bedrock or bulkheads
                bool hit_barrier = false;
                for (float t = 0.4f; t <= move_dist + 1.2f; t += 0.4f) {
                    glm::ivec3 check_p = glm::ivec3(glm::floor(old_pos + facing * t));
                    Voxel fv = world.get_voxel(check_p.x, check_p.y, check_p.z);
                    if (fv.material_id == MAT_DREDGE_BEDROCK || fv.material_id == MAT_INDUSTRIAL_BULKHEAD) {
                        hit_barrier = true;
                        break;
                    }
                }

                if (hit_barrier) {
                    b.state = BurrowerState::Stunned;
                    b.stun_timer = 3.0f;
                    b.just_stunned = true;
                    b.velocity = -facing * 2.5f; // Rebound recoil
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " SMASHED into reinforced bulkhead/bedrock -> STUNNED!");
                } else {
                    b.velocity = facing * b.charge_speed;
                    b.position = next_pos;

                    // DESTROY THINGS IN ITS PATH:
                    // Obliterate any destructible voxels around the cutter head during kinetic charge (distance-throttled)!
                    b.excavation_dist_accum += move_dist;
                    if (b.excavation_dist_accum >= 0.5f) {
                        b.excavation_dist_accum = 0.0f;
                        glm::ivec3 charge_center = glm::ivec3(glm::floor(next_pos + facing * 1.0f));
                        std::unordered_set<ChunkPos, ChunkPosHash> charge_affected_chunks;
                        for (int dx = -1; dx <= 1; ++dx) {
                            for (int dy = -1; dy <= 1; ++dy) {
                                for (int dz = -1; dz <= 1; ++dz) {
                                    glm::ivec3 p = charge_center + glm::ivec3(dx, dy, dz);
                                    Voxel v = world.get_voxel(p.x, p.y, p.z);
                                    if (v.is_solid() && v.material_id != MAT_DREDGE_BEDROCK && v.material_id != MAT_INDUSTRIAL_BULKHEAD) {
                                        world.set_voxel(p.x, p.y, p.z, Voxel{MAT_AIR, 0}, false);
                                        if (result.excavated_voxels.size() < 16) {
                                            result.excavated_voxels.push_back({p, v.material_id});
                                        }
                                        b.just_ground = true;
                                        result.any_grind = true;

                                        int cx = p.x >> 5;
                                        int cy = p.y >> 5;
                                        int cz = p.z >> 5;
                                        charge_affected_chunks.insert(ChunkPos{cx, cy, cz});
                                        int lx = p.x & 31;
                                        int ly = p.y & 31;
                                        int lz = p.z & 31;
                                        if (lx == 0) charge_affected_chunks.insert(ChunkPos{cx - 1, cy, cz});
                                        if (lx == 31) charge_affected_chunks.insert(ChunkPos{cx + 1, cy, cz});
                                        if (ly == 0) charge_affected_chunks.insert(ChunkPos{cx, cy - 1, cz});
                                        if (ly == 31) charge_affected_chunks.insert(ChunkPos{cx, cy + 1, cz});
                                        if (lz == 0) charge_affected_chunks.insert(ChunkPos{cx, cy, cz - 1});
                                        if (lz == 31) charge_affected_chunks.insert(ChunkPos{cx, cy, cz + 1});
                                    }
                                }
                            }
                        }
                        for (const auto& cpos : charge_affected_chunks) {
                            Chunk* c = world.get_chunk(cpos);
                            if (c) {
                                c->MarkDirty();
                                world.queue_chunk_for_meshing(cpos);
                            }
                        }
                    }

                    // Collision with player
                    if (dist_to_player < 2.5f && b.attack_cooldown <= 0.0f) {
                        b.attack_cooldown = 1.8f;
                        b.just_rammed = true;
                        result.any_ram_hit = true;
                        result.total_damage += b.ram_damage;
                        VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " RAMMED player (-" << b.ram_damage << " HP)!");
                    } else if (b.state_timer > 3.5f) {
                        // Charge expired, re-enter ground or rage
                        if (b.shockwave_cooldown <= 0.0f) {
                            b.state = BurrowerState::Enraged;
                            b.state_timer = 0.0f;
                            b.shockwave_cooldown = 8.0f;
                        } else {
                            b.state = BurrowerState::Burrowing;
                            b.state_timer = 0.0f;
                        }
                    }
                }
                break;
            }

            case BurrowerState::Stunned: {
                b.velocity *= 0.8f;
                b.stun_timer -= dt;
                if (b.stun_timer <= 0.0f) {
                    b.state = BurrowerState::Burrowing;
                    b.state_timer = 0.0f;
                }
                break;
            }

            case BurrowerState::Enraged: {
                // Slam ground, emitting shockwave that destabilizes ceiling
                b.velocity *= 0.5f;
                if (b.state_timer >= 1.0f) {
                    b.just_triggered_cavein = true;
                    result.any_cavein_triggered = true;
                    result.cavein_origins.push_back(head_vox);
                    b.state = BurrowerState::Burrowing;
                    b.state_timer = 0.0f;
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " TRIGGERED TECTONIC SHOCKWAVE / CAVE-IN!");
                }
                break;
            }

            case BurrowerState::Dying: {
                b.death_timer += dt;
                b.velocity.y -= 18.0f * dt;
                b.velocity.x *= std::max(0.0f, 1.0f - 2.5f * dt);
                b.velocity.z *= std::max(0.0f, 1.0f - 2.5f * dt);
                b.position += b.velocity * dt;

                float ground_y = 0.0f;
                if (AberrantAI::find_ground_surface(world, b.position, 0.8f, 2.0f, &ground_y)) {
                    if (b.position.y <= ground_y + 0.15f) {
                        b.position.y = ground_y + 0.05f;
                        b.velocity = glm::vec3(0.0f);
                    }
                }

                float remain = std::max(0.0f, 1.0f - (b.death_timer / b.death_duration));
                b.cutter_angle += dt * remain * 16.0f;
                b.yaw += b.death_angular_velocity.y * dt;
                b.pitch += b.death_angular_velocity.x * dt;

                if (b.death_timer >= b.death_duration) {
                    b.state = BurrowerState::Dead;
                    result.burrowers_killed++;

                    glm::mat4 m(1.0f);
                    m = glm::rotate(m, b.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                    m = glm::rotate(m, -b.pitch, glm::vec3(1.0f, 0.0f, 0.0f));
                    m = glm::rotate(m, 0.25f, glm::vec3(0.0f, 0.0f, 1.0f));
                    glm::quat death_rot = glm::quat_cast(m);

                    CarcassManager::instance().spawn_carcass(
                        b.position, death_rot, b.death_hit_dir,
                        StalkerRole::Melee, b.scale, CarcassType::SeismicBurrower
                    );
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " DIED -> Spawning physical carcass remains!");
                }
                break;
            }

            case BurrowerState::Dead:
                break;
        }

        // Environmental hazard perception and damage (Lava, Acid, Spikes)
        b.spike_damage_timer = std::max(0.0f, b.spike_damage_timer - dt);

        if (b.state != BurrowerState::Dead && b.state != BurrowerState::Dying) {
            glm::vec3 head_pos = b.position;
            glm::vec3 body_pos = b.position - b.forward() * 1.2f;
            glm::vec3 under_pos = b.position - glm::vec3(0.0f, 0.6f, 0.0f);

            Voxel v_head = world.get_voxel(static_cast<int>(std::floor(head_pos.x)),
                                           static_cast<int>(std::floor(head_pos.y)),
                                           static_cast<int>(std::floor(head_pos.z)));
            Voxel v_body = world.get_voxel(static_cast<int>(std::floor(body_pos.x)),
                                           static_cast<int>(std::floor(body_pos.y)),
                                           static_cast<int>(std::floor(body_pos.z)));
            Voxel v_under = world.get_voxel(static_cast<int>(std::floor(under_pos.x)),
                                            static_cast<int>(std::floor(under_pos.y)),
                                            static_cast<int>(std::floor(under_pos.z)));

            bool b_in_lava = (v_head.material_id == MAT_THERMITE_SLAG || v_head.material_id == MAT_LAVA || v_head.material_id == MAT_MOLTEN_MAGMA ||
                              v_body.material_id == MAT_THERMITE_SLAG || v_body.material_id == MAT_LAVA || v_body.material_id == MAT_MOLTEN_MAGMA ||
                              v_under.material_id == MAT_THERMITE_SLAG || v_under.material_id == MAT_LAVA || v_under.material_id == MAT_MOLTEN_MAGMA);

            bool b_in_acid = (v_head.material_id == MAT_ACID || v_body.material_id == MAT_ACID || v_under.material_id == MAT_ACID);

            bool b_in_coolant = (v_head.material_id == MAT_COOLANT || v_body.material_id == MAT_COOLANT || v_under.material_id == MAT_COOLANT);

            bool b_in_spikes = (v_head.material_id == MAT_OBSIDIAN_SPIKES || v_under.material_id == MAT_OBSIDIAN_SPIKES);

            b.is_in_lava = b_in_lava;
            b.is_in_acid = b_in_acid;
            b.is_in_coolant = b_in_coolant;
            b.is_in_hazard = b_in_lava || b_in_acid || b_in_coolant || b_in_spikes;

            if (b_in_lava) {
                float lava_dmg = 45.0f * dt;
                b.hp -= lava_dmg;
                if (m_global_roar_cooldown <= 0.0f) {
                    b.just_roared = true;
                    result.any_roar = true;
                    m_global_roar_cooldown = 4.0f;
                }
                if (b.state == BurrowerState::Charging) {
                    b.state = BurrowerState::Stunned;
                    b.stun_timer = 2.0f;
                    b.just_stunned = true;
                    b.velocity = -b.forward() * 3.0f;
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " PLUNGED into lava during charge -> STUNNED & INCINERATING!");
                }
                if (b.hp <= 0.0f) {
                    b.hp = 0.0f;
                    if (b.state != BurrowerState::Dying && b.state != BurrowerState::Dead) {
                        b.state = BurrowerState::Dying;
                        b.death_timer = 0.0f;
                        b.death_duration = 0.9f;
                        b.just_died = true;
                        b.death_hit_dir = glm::vec3(0.0f, 1.0f, 0.0f);
                        b.death_impulse = glm::vec3(0.0f, 2.0f, 0.0f);
                        b.velocity = b.death_impulse;
                        b.death_angular_velocity = glm::vec3(0.4f, 0.2f, 0.3f);
                        VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " INCINERATED in molten lava -> ENTERING DEATH THROES!");
                    }
                }
            } else if (b_in_acid) {
                float acid_dmg = 30.0f * dt;
                b.hp -= acid_dmg;
                if (b.state == BurrowerState::Charging) {
                    b.state = BurrowerState::Stunned;
                    b.stun_timer = 1.5f;
                    b.just_stunned = true;
                }
                if (b.hp <= 0.0f) {
                    b.hp = 0.0f;
                    if (b.state != BurrowerState::Dying && b.state != BurrowerState::Dead) {
                        b.state = BurrowerState::Dying;
                        b.death_timer = 0.0f;
                        b.death_duration = 0.9f;
                        b.just_died = true;
                        b.death_hit_dir = glm::vec3(0.0f, 1.0f, 0.0f);
                        b.death_impulse = glm::vec3(0.0f, 1.5f, 0.0f);
                        b.velocity = b.death_impulse;
                        b.death_angular_velocity = glm::vec3(0.3f, 0.4f, 0.2f);
                        VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " DISSOLVED in acid reservoir -> ENTERING DEATH THROES!");
                    }
                }
            } else if (b_in_coolant) {
                float coolant_dmg = 20.0f * dt;
                b.hp -= coolant_dmg;
                b.velocity *= 0.45f;
                if (b.state == BurrowerState::Charging) {
                    b.state = BurrowerState::Stunned;
                    b.stun_timer = 2.0f;
                    b.just_stunned = true;
                    b.velocity = -b.forward() * 1.5f;
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " PLUNGED into cryogenic coolant pool -> CHARGE STALLED & HYDRAULICS FROZEN!");
                }
                if (b.hp <= 0.0f) {
                    b.hp = 0.0f;
                    if (b.state != BurrowerState::Dying && b.state != BurrowerState::Dead) {
                        b.state = BurrowerState::Dying;
                        b.death_timer = 0.0f;
                        b.death_duration = 0.9f;
                        b.just_died = true;
                        b.death_hit_dir = glm::vec3(0.0f, 1.0f, 0.0f);
                        b.death_impulse = glm::vec3(0.0f, 1.0f, 0.0f);
                        b.velocity = b.death_impulse;
                        b.death_angular_velocity = glm::vec3(0.1f, 0.2f, 0.1f);
                        VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " FROZEN SOLID in cryogenic coolant pool -> ENTERING DEATH THROES!");
                    }
                }
            } else if (b_in_spikes) {
                if (b.spike_damage_timer <= 0.0f) {
                    b.spike_damage_timer = 1.0f;
                    b.hp -= 25.0f;
                    if (b.hp <= 0.0f) {
                        b.hp = 0.0f;
                        if (b.state != BurrowerState::Dying && b.state != BurrowerState::Dead) {
                            b.state = BurrowerState::Dying;
                            b.death_timer = 0.0f;
                            b.death_duration = 0.9f;
                            b.just_died = true;
                            b.death_hit_dir = glm::vec3(0.0f, 1.0f, 0.0f);
                            b.death_impulse = glm::vec3(0.0f, 2.0f, 0.0f);
                            b.velocity = b.death_impulse;
                            b.death_angular_velocity = glm::vec3(0.5f, 0.1f, 0.3f);
                            VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " IMPALED on obsidian spikes -> ENTERING DEATH THROES!");
                        }
                    }
                }
            }
        }

        // Enforce strict minimum 2.15m horizontal standoff to player camera
        if (b.state != BurrowerState::Dead && b.state != BurrowerState::Dying) {
            AberrantAI::resolve_player_penetration(b.position, player_pos, 1.45f, 0.70f);
        }

        // Clamp inside cavern bounds
        b.position.x = std::clamp(b.position.x, 2.0f, 62.0f);
        b.position.y = std::clamp(b.position.y, 2.0f, 62.0f);
        b.position.z = std::clamp(b.position.z, 2.0f, 62.0f);

        // Locomotion & Crawling Step Sound Generation (Segmented Crawler Tread Churn)
        if (b.state != BurrowerState::Dead && b.state != BurrowerState::Dying &&
            b.state != BurrowerState::Dormant && b.state != BurrowerState::Stunned) {
            float physical_disp = glm::distance(b.position, frame_start_pos);
            float physical_speed = physical_disp / std::max(0.0001f, dt);
            float speed = glm::length(b.velocity);

            // Check if entity is stuck against impenetrable rock or lacking physical progress
            bool is_stuck = (b.stuck_timer > 0.15f);
            if (is_stuck) {
                b.footstep_timer = 0.0f;
                b.just_stepped = false;
            } else if (speed >= 0.25f) {
                bool is_charging = (b.state == BurrowerState::Charging || b.state == BurrowerState::Enraged || speed >= 5.0f);
                b.step_is_run = is_charging;
                float interval = is_charging ? 0.30f : 0.48f;
                interval = std::clamp(interval / std::max(0.5f, speed / (is_charging ? 6.5f : 2.5f)), 0.22f, 0.55f);

                b.footstep_timer += dt;
                if (b.footstep_timer >= interval) {
                    b.footstep_timer = 0.0f;
                    b.just_stepped = true;
                    result.any_step = true;
                    if (is_charging) {
                        result.any_charge_step = true;
                    }

                    // Adaptive Proximity & Directional Gain (louder when closer and approaching)
                    float dist = glm::distance(b.position, player_pos);
                    float prox = std::clamp(1.0f - (dist - 3.0f) / 27.0f, 0.0f, 1.0f);
                    float gain_prox = 0.45f + 0.85f * std::pow(prox, 1.15f);

                    glm::vec3 to_player = player_pos - b.position;
                    float to_player_len = glm::length(to_player);
                    float approach = 0.0f;
                    if (to_player_len > 0.01f && speed > 0.1f) {
                        approach = std::max(0.0f, glm::dot(glm::normalize(b.velocity), to_player / to_player_len));
                    }
                    float gain_approach = 1.0f + 0.20f * approach;

                    // Subterranean serpentine slither: scale friction across cavern grit, tuned quieter
                    float base_vol = is_charging ? 0.45f : 0.26f;
                    b.step_volume = std::clamp(base_vol * gain_prox * gain_approach, 0.10f, is_charging ? 0.65f : 0.42f);

                    float pitch_base = is_charging ? 1.02f : 0.88f;
                    float jitter = 0.94f + 0.12f * std::fmod(std::abs(b.position.x * 9.1f + b.position.z * 13.7f), 1.0f);
                    b.step_pitch = pitch_base * jitter;
                }
            } else {
                b.footstep_timer = std::max(0.0f, b.footstep_timer - dt * 2.0f);
            }
        } else {
            b.footstep_timer = 0.0f;
        }
    }

    // Pairwise burrower-to-burrower depenetration (hard anti-overlap)
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0; i < m_burrowers.size(); ++i) {
            auto& b1 = m_burrowers[i];
            if (b1.is_dead() || b1.is_dying()) continue;
            float r1 = 0.95f;
            float w1 = 3.5f;

            for (size_t j = i + 1; j < m_burrowers.size(); ++j) {
                auto& b2 = m_burrowers[j];
                if (b2.is_dead() || b2.is_dying()) continue;
                float r2 = 0.95f;
                float w2 = 3.5f;

                AberrantAI::resolve_enemy_to_enemy_penetration(
                    b1.position, b1.velocity, r1, w1,
                    b2.position, b2.velocity, r2, w2,
                    b1.id, b2.id
                );
            }
        }
    }

    return result;
}

bool SeismicBurrowerManager::damage_nearest(const glm::vec3& origin, float radius, float damage,
                                            bool is_explosive, bool allow_crit,
                                            bool* out_is_crit, float* out_damage_dealt,
                                            const glm::vec3* attacker_pos,
                                            const glm::vec3* shot_direction) {
    SeismicBurrower* nearest = nullptr;
    float min_dist = radius;

    for (auto& b : m_burrowers) {
        if (b.is_dead()) continue;
        float d = glm::distance(origin, b.position);
        if (d < min_dist) {
            min_dist = d;
            nearest = &b;
        }
    }

    if (!nearest) return false;

    // Vulnerability: Explosive damage deals 2.5x damage!
    float effective_damage = is_explosive ? (damage * 2.5f) : damage;
    bool is_crit = false;
    if (allow_crit && nearest->is_unalerted()) {
        is_crit = true;
        effective_damage *= StealthSystem::SNEAK_ATTACK_CRIT_MULTIPLIER;
        VF_LOG_INFO("SeismicBurrower", "SNEAK ATTACK CRITICAL HIT on unalerted Burrower " << nearest->id
                    << "! Base=" << damage << " -> Crit=" << effective_damage);
    }
    if (out_is_crit) *out_is_crit = is_crit;
    if (out_damage_dealt) *out_damage_dealt = effective_damage;

    nearest->hp -= effective_damage;

    VF_LOG_INFO("SeismicBurrower", "Burrower " << nearest->id << " took " << effective_damage
                << " damage (is_explosive=" << is_explosive << ", is_crit=" << is_crit << "), remaining HP=" << nearest->hp);

    if (nearest->hp <= 0.0f) {
        nearest->hp = 0.0f;
        if (nearest->state != BurrowerState::Dying && nearest->state != BurrowerState::Dead) {
            nearest->state = BurrowerState::Dying;
            nearest->death_timer = 0.0f;
            nearest->death_duration = 0.9f;
            nearest->just_died = true;
            nearest->is_pursuing_attacker = false;
            glm::vec3 hit_dir = (shot_direction != nullptr && glm::length(*shot_direction) > 0.001f)
                ? glm::normalize(*shot_direction)
                : ((attacker_pos != nullptr && glm::distance(nearest->position, *attacker_pos) > 0.001f)
                   ? glm::normalize(nearest->position - *attacker_pos)
                   : glm::vec3(0.0f, 0.0f, 1.0f));
            nearest->death_hit_dir = hit_dir;
            nearest->death_impulse = hit_dir * 3.5f + glm::vec3(0.0f, 2.0f, 0.0f);
            nearest->velocity = nearest->death_impulse;
            nearest->death_angular_velocity = glm::vec3(
                ((rand() % 100) - 50) * 0.04f,
                ((rand() % 100) - 50) * 0.06f,
                ((rand() % 100) - 50) * 0.04f
            );
            VF_LOG_INFO("SeismicBurrower", "Burrower " << nearest->id << " FATAL HIT -> ENTERING DEATH THROES!");
        }
    } else {
        // Resolve actual position of attacker
        glm::vec3 actual_attacker_pos = (attacker_pos != nullptr) ? *attacker_pos : origin;
        if (attacker_pos == nullptr && glm::distance(origin, nearest->position) < 0.5f) {
            glm::vec3 facing(std::sin(nearest->yaw), 0.0f, std::cos(nearest->yaw));
            actual_attacker_pos = nearest->position - facing * 5.0f;
        }

        // Retaliation pursuit: Awaken and lock onto attacker
        nearest->is_pursuing_attacker = true;
        nearest->pursuit_lost_timer = 0.0f;
        nearest->target_pos = actual_attacker_pos;

        // Immediately orient toward attacker
        glm::vec3 to_attacker = actual_attacker_pos - nearest->position;
        if (glm::length(glm::vec2(to_attacker.x, to_attacker.z)) > 0.1f) {
            nearest->yaw = std::atan2(to_attacker.x, to_attacker.z);
        }
        float horiz_d = std::hypot(to_attacker.x, to_attacker.z);
        if (horiz_d > 0.1f) {
            nearest->pitch = -std::asin(std::clamp(to_attacker.y / std::max(0.1f, glm::length(to_attacker)), -1.0f, 1.0f));
        }

        if (nearest->hp <= 35.0f) {
            // Critically damaged -> Never retreat or scare! Enrages into relentless berserk attack!
            if (nearest->state != BurrowerState::Charging && nearest->state != BurrowerState::Enraged) {
                nearest->state = BurrowerState::Enraged;
                nearest->state_timer = 0.0f;
                nearest->just_roared = true;
                VF_LOG_INFO("SeismicBurrower", "Wounded Burrower " << nearest->id << " (HP=" << nearest->hp
                            << ") DOES NOT SCARE OR RETREAT - ENRAGED BERSERK PURSUIT!");
            }
        } else if (nearest->state == BurrowerState::Dormant) {
            nearest->state = BurrowerState::Burrowing;
            nearest->state_timer = 0.0f;
            nearest->just_roared = true;
            VF_LOG_INFO("SeismicBurrower", "Dormant Burrower " << nearest->id << " AWAKENED by player attack and begins pursuit!");
        }

        if (is_explosive) {
            // Explosive concussion stuns the burrower!
            nearest->state = BurrowerState::Stunned;
            nearest->stun_timer = 2.5f;
            nearest->just_stunned = true;
        }
    }

    return true;
}

void SeismicBurrowerManager::apply_stun(const glm::vec3& origin, float radius, float duration) {
    for (auto& b : m_burrowers) {
        if (b.is_dead()) continue;
        if (glm::distance(origin, b.position) <= radius) {
            b.state = BurrowerState::Stunned;
            b.stun_timer = duration;
            b.just_stunned = true;
        }
    }
}

int SeismicBurrowerManager::active_count() const {
    int count = 0;
    for (const auto& b : m_burrowers) {
        if (!b.is_dead() && !b.is_dying()) count++;
    }
    return count;
}

void SeismicBurrowerManager::remove_dead() {
    m_burrowers.erase(
        std::remove_if(m_burrowers.begin(), m_burrowers.end(),
            [](const SeismicBurrower& b) { return b.is_dead(); }),
        m_burrowers.end()
    );
}

void SeismicBurrowerManager::reset() {
    m_burrowers.clear();
    m_next_id = 1001;
    m_spawn_cooldown = 0.0f;
    m_global_roar_cooldown = 0.0f;
    m_global_grind_cooldown = 0.0f;
    m_global_chatter_cooldown = 0.0f;
}

void SeismicBurrowerManager::assign_patrol_zones_from_world(const World& world) {
    if (!world.level_generator()) return;
    for (auto& b : m_burrowers) {
        const RoomPlacement* room = world.level_generator()->get_room_at_position(b.position);
        if (room) {
            PatrolZone zone;
            zone.zone_id = room->grid_x * 10 + room->grid_z;
            zone.center = glm::vec3(room->center);
            zone.bounds_min = glm::vec3(
                static_cast<float>(room->center.x - room->half_width + 1.0f),
                static_cast<float>(room->floor_y - 2),
                static_cast<float>(room->center.z - room->half_depth + 1.0f)
            );
            zone.bounds_max = glm::vec3(
                static_cast<float>(room->center.x + room->half_width - 1.0f),
                static_cast<float>(room->ceiling_y + 2),
                static_cast<float>(room->center.z + room->half_depth - 1.0f)
            );
            zone.has_bounds = true;
            zone.radius = static_cast<float>(std::max(room->half_width, room->half_depth)) + 4.0f;
            zone.floor_y = room->floor_y;
            zone.ceiling_y = room->ceiling_y;
            b.set_patrol_zone(zone);
        }
    }
}

} // namespace Voidfall
