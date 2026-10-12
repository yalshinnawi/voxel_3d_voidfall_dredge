#include "seismic_burrower.hpp"
#include "../../voxel/world.hpp"
#include "../../core/logger.hpp"
#include "../../ai/aberrant_ai.hpp"
#include <glm/gtc/constants.hpp>
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

    spawn_burrower(spawn_pos);
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
        b.just_roared = false;
        b.just_ground = false;
        b.just_chattered = false;
        b.just_breached = false;
        b.just_rammed = false;
        b.just_triggered_cavein = false;
        b.just_stunned = false;
        b.just_died = false;

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
        bool is_locked_on_player = b.is_pursuing_attacker ||
                                   b.state == BurrowerState::Charging ||
                                   b.state == BurrowerState::Breaching ||
                                   b.state == BurrowerState::Enraged ||
                                   (!player_is_crouching && dist_to_player < 18.0f);

        if (!is_locked_on_player && b.has_sound_target && b.state == BurrowerState::Burrowing) {
            active_target = b.sound_target;
            if (glm::distance(b.position, b.sound_target) < 3.0f) {
                b.has_sound_target = false;
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
                // Dormant burrower deep in rock: only wake if loud sound occurs, damaged/pursued, or physical collision
                float wake_dist = player_is_crouching ? 0.9f : 8.0f;
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

                // Move forward through ground (surging speed when pursuing attacker)
                float eff_burrow_speed = b.is_pursuing_attacker ? 4.5f : b.burrow_speed;
                glm::vec3 old_pos = b.position;
                float move_dist = eff_burrow_speed * dt;
                b.velocity = facing * eff_burrow_speed;
                b.position += b.velocity * dt;

                // Swept excavation along movement path from old_pos to new position
                for (float step = 0.0f; step <= move_dist + 0.8f; step += 0.8f) {
                    glm::ivec3 center = glm::ivec3(glm::floor(old_pos + facing * (step + 1.0f)));
                    for (int dx = -1; dx <= 1; ++dx) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dz = -1; dz <= 1; ++dz) {
                                glm::ivec3 p = center + glm::ivec3(dx, dy, dz);
                                Voxel v = world.get_voxel(p.x, p.y, p.z);
                                if (v.is_solid() && v.material_id != MAT_DREDGE_BEDROCK && v.material_id != MAT_INDUSTRIAL_BULKHEAD) {
                                    world.set_voxel(p.x, p.y, p.z, Voxel{MAT_AIR, 0}, true);
                                    result.excavated_voxels.push_back({p, v.material_id});
                                }
                            }
                        }
                    }
                }

                // If emerged into open cavern with line of sight to player, transition to Breaching!
                bool can_breach = dist_to_player < 14.0f;
                if (player_is_crouching && !b.is_pursuing_attacker && !b.has_sound_target) {
                    // When player is sneaking and burrower is unalerted, only breach on close physical contact or direct line-of-sight in front
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
                    // Obliterate any destructible voxels around the cutter head during kinetic charge!
                    glm::ivec3 charge_center = glm::ivec3(glm::floor(next_pos + facing * 1.0f));
                    for (int dx = -1; dx <= 1; ++dx) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dz = -1; dz <= 1; ++dz) {
                                glm::ivec3 p = charge_center + glm::ivec3(dx, dy, dz);
                                Voxel v = world.get_voxel(p.x, p.y, p.z);
                                if (v.is_solid() && v.material_id != MAT_DREDGE_BEDROCK && v.material_id != MAT_INDUSTRIAL_BULKHEAD) {
                                    world.set_voxel(p.x, p.y, p.z, Voxel{MAT_AIR, 0}, true);
                                    result.excavated_voxels.push_back({p, v.material_id});
                                    b.just_ground = true;
                                    result.any_grind = true;
                                }
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

            case BurrowerState::Dead:
                break;
        }

        // Clamp inside cavern bounds
        b.position.x = std::clamp(b.position.x, 2.0f, 62.0f);
        b.position.y = std::clamp(b.position.y, 2.0f, 62.0f);
        b.position.z = std::clamp(b.position.z, 2.0f, 62.0f);
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
        nearest->state = BurrowerState::Dead;
        nearest->just_died = true;
        nearest->is_pursuing_attacker = false;
        VF_LOG_INFO("SeismicBurrower", "Burrower " << nearest->id << " DESTROYED!");
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
        if (!b.is_dead()) count++;
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

} // namespace Voidfall
