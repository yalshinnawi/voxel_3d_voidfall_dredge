#include "seismic_burrower.hpp"
#include "../../voxel/world.hpp"
#include "../../core/logger.hpp"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

namespace Voidfall {

void SeismicBurrowerManager::spawn_burrower(const glm::vec3& pos, float hp_multiplier) {
    SeismicBurrower b;
    b.id = m_next_id++;
    b.position = pos;
    b.target_pos = pos;
    b.hp = 120.0f * hp_multiplier;
    b.max_hp = b.hp;
    b.state = BurrowerState::Burrowing;
    b.state_timer = 0.0f;
    b.yaw = 0.0f;
    b.pitch = 0.0f;
    b.roar_timer = 5.0f + static_cast<float>((b.id * 37) % 110) * 0.1f; // Staggered initial roar
    b.grind_timer = 1.5f + static_cast<float>((b.id * 17) % 30) * 0.1f;
    b.just_roared = false;
    b.just_ground = false;
    m_burrowers.push_back(b);
    VF_LOG_INFO("SeismicBurrower", "Spawned Seismic Burrower " << b.id << " at ("
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
    const std::vector<SoundEvent>& sound_events
) {
    FrameResult result;
    if (m_spawn_cooldown > 0.0f) {
        m_spawn_cooldown -= dt;
    }

    for (auto& b : m_burrowers) {
        b.just_roared = false;
        b.just_ground = false;
        b.just_breached = false;
        b.just_rammed = false;
        b.just_triggered_cavein = false;
        b.just_stunned = false;
        b.just_died = false;

        if (b.is_dead()) continue;

        // Ambient Subterranean Roars echoing through caverns
        if (b.state != BurrowerState::Dead && b.state != BurrowerState::Stunned) {
            b.roar_timer -= dt;
            if (b.roar_timer <= 0.0f) {
                b.roar_timer = 13.0f + static_cast<float>((b.id * 43 + static_cast<int>(b.state_timer * 100)) % 140) * 0.1f;
                b.just_roared = true;
                result.any_roar = true;
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
            b.sound_target_timer -= dt;
            if (b.sound_target_timer <= 0.0f) {
                b.has_sound_target = false;
            }
        }

        for (const auto& snd : sound_events) {
            float snd_dist = glm::distance(b.position, snd.position);
            // Burrowers sense seismic ground vibrations up to 1.5x sound radius through solid rock
            if (snd_dist <= snd.audible_radius * 1.5f) {
                b.sound_target = snd.position;
                b.has_sound_target = true;
                b.sound_target_timer = 5.0f;
            }
        }

        // Retaliation pursuit and escape tracking
        float dist_to_player = glm::distance(b.position, player_pos);
        if (b.is_pursuing_attacker) {
            if (dist_to_player > b.pursuit_break_dist) {
                b.pursuit_lost_timer += dt;
                if (b.pursuit_lost_timer >= b.pursuit_break_time) {
                    b.is_pursuing_attacker = false;
                    b.pursuit_lost_timer = 0.0f;
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " lost interest after player fled for "
                                << b.pursuit_break_time << "s (dist=" << dist_to_player << "m)");
                }
            } else {
                b.pursuit_lost_timer = 0.0f;
            }
        }

        // Target selection: If pursuing an attacker, lock relentlessly onto player_pos.
        // Otherwise, if burrowing in rock, steer towards latest sound disturbance; else towards player.
        glm::vec3 active_target = player_pos;
        if (!b.is_pursuing_attacker && b.has_sound_target && b.state == BurrowerState::Burrowing) {
            active_target = b.sound_target;
        }
        b.target_pos = active_target;

        glm::vec3 to_target = active_target - b.position;
        float dist_to_target = glm::length(to_target);
        glm::vec3 dir_to_target = (dist_to_target > 0.001f) ? glm::normalize(to_target) : glm::vec3(0, 0, 1);

        // Calculate facing angles
        float target_yaw = std::atan2(dir_to_target.x, dir_to_target.z);
        float target_pitch = -std::asin(std::clamp(dir_to_target.y, -1.0f, 1.0f));

        // Smooth angle tracking
        float yaw_diff = target_yaw - b.yaw;
        while (yaw_diff > glm::pi<float>()) yaw_diff -= 2.0f * glm::pi<float>();
        while (yaw_diff < -glm::pi<float>()) yaw_diff += 2.0f * glm::pi<float>();
        b.yaw += std::clamp(yaw_diff, -b.turn_rate * dt, b.turn_rate * dt);
        b.pitch += std::clamp(target_pitch - b.pitch, -b.turn_rate * dt, b.turn_rate * dt);

        glm::vec3 facing(
            std::sin(b.yaw) * std::cos(-b.pitch),
            -std::sin(-b.pitch),
            std::cos(b.yaw) * std::cos(-b.pitch)
        );

        // Check if head voxel is inside solid rock or open air
        glm::ivec3 head_vox(
            static_cast<int>(std::floor(b.position.x)),
            static_cast<int>(std::floor(b.position.y)),
            static_cast<int>(std::floor(b.position.z))
        );
        bool in_solid = world.is_solid(head_vox);

        // State Machine
        switch (b.state) {
            case BurrowerState::Dormant: {
                if (dist_to_player < 22.0f) {
                    b.state = BurrowerState::Burrowing;
                    b.state_timer = 0.0f;
                }
                break;
            }

            case BurrowerState::Burrowing: {
                b.rumble_intensity = std::clamp(1.0f - (dist_to_player / 20.0f), 0.0f, 1.0f);
                result.max_rumble = std::max(result.max_rumble, b.rumble_intensity);

                // Periodic tooth grinding as cutter spins through bedrock
                b.grind_timer -= dt;
                if (b.grind_timer <= 0.0f) {
                    b.grind_timer = 2.4f + static_cast<float>((b.id * 19) % 20) * 0.1f;
                    b.just_ground = true;
                    result.any_grind = true;
                }

                // Move forward through ground
                b.velocity = facing * b.burrow_speed;
                b.position += b.velocity * dt;

                // Excavate soft terrain in front of cutter head
                glm::ivec3 center = glm::ivec3(glm::floor(b.position + facing * 1.0f));
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

                // If emerged into open cavern with line of sight to player, transition to Breaching!
                if (!in_solid && dist_to_player < 14.0f) {
                    b.state = BurrowerState::Breaching;
                    b.state_timer = 0.0f;
                    b.just_breached = true;
                    b.just_roared = true; // Terrifying breach roar!
                    result.any_breach = true;
                    result.any_roar = true;
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " BREACHED into open cavern with ROAR!");
                }
                break;
            }

            case BurrowerState::Breaching: {
                // Short dramatic pause while emerging into the cave
                b.velocity *= 0.85f;
                if (b.state_timer > 0.8f) {
                    b.state = BurrowerState::Charging;
                    b.state_timer = 0.0f;
                    b.yaw = target_yaw;
                    b.pitch = target_pitch;
                    result.any_charge = true;
                    VF_LOG_INFO("SeismicBurrower", "Burrower " << b.id << " INITIATES KINETIC CHARGE!");
                }
                break;
            }

            case BurrowerState::Charging: {
                // High-speed open cavern ramming charge with swept collision check
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

bool SeismicBurrowerManager::damage_nearest(const glm::vec3& origin, float radius, float damage, bool is_explosive) {
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
    nearest->hp -= effective_damage;

    VF_LOG_INFO("SeismicBurrower", "Burrower " << nearest->id << " took " << effective_damage
                << " damage (is_explosive=" << is_explosive << "), remaining HP=" << nearest->hp);

    if (nearest->hp <= 0.0f) {
        nearest->hp = 0.0f;
        nearest->state = BurrowerState::Dead;
        nearest->just_died = true;
        nearest->is_pursuing_attacker = false;
        VF_LOG_INFO("SeismicBurrower", "Burrower " << nearest->id << " DESTROYED!");
    } else {
        // Retaliation pursuit: Awaken and lock onto attacker
        nearest->is_pursuing_attacker = true;
        nearest->pursuit_lost_timer = 0.0f;
        if (nearest->state == BurrowerState::Dormant) {
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
}

} // namespace Voidfall
