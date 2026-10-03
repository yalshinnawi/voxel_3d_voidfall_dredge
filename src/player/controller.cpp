#include "controller.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Voidfall {

PlayerController::PlayerController(const glm::vec3& start_pos)
    : m_position(start_pos)
{
    update_camera_vectors();
}

void PlayerController::update_camera_vectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_up    = glm::normalize(glm::cross(m_right, m_front));
}

glm::mat4 PlayerController::get_view_matrix() const {
    glm::vec3 eye = m_position + glm::vec3(0.0f, 0.7f, 0.0f);
    return glm::lookAt(eye, eye + m_front, m_up);
}

RaycastHit PlayerController::get_look_target(const World& world, float max_dist) const {
    glm::vec3 eye = m_position + glm::vec3(0.0f, 0.7f, 0.0f);
    glm::vec3 ray_origin = eye + m_front * 0.6f;
    return world.raycast(ray_origin, m_front, max_dist);
}

void PlayerController::handle_input(const Window& window, float dt) {
    m_current_buttons = 0;

    // 1. Mouse Look
    glm::dvec2 mouse_delta = const_cast<Window&>(window).get_cursor_delta();
    m_yaw   += static_cast<float>(mouse_delta.x) * m_mouse_sensitivity;
    m_pitch += static_cast<float>(mouse_delta.y) * m_mouse_sensitivity;
    m_pitch  = glm::clamp(m_pitch, -89.0f, 89.0f);
    update_camera_vectors();

    // Tool hotbar selection: 1 (Mining Drill), 2 (Combat Weapon), 3 (Industrial Bulkhead), 4 (Demolition Charge)
    if (window.is_key_down(GLFW_KEY_1)) m_active_tool = ToolSlot::MiningDrill;
    if (window.is_key_down(GLFW_KEY_2)) m_active_tool = ToolSlot::CombatWeapon;
    if (window.is_key_down(GLFW_KEY_3)) m_active_tool = ToolSlot::IndustrialBulkhead;
    if (window.is_key_down(GLFW_KEY_4)) m_active_tool = ToolSlot::DemolitionCharge;

    // Quick weapon draw / toggle: X key swaps between Mining Drill and Combat Weapon
    static bool x_down_last = false;
    bool x_down = window.is_key_down(GLFW_KEY_X);
    if (x_down && !x_down_last) {
        if (m_active_tool == ToolSlot::CombatWeapon) {
            m_active_tool = ToolSlot::MiningDrill;
        } else {
            m_active_tool = ToolSlot::CombatWeapon;
        }
    }
    x_down_last = x_down;

    // Reload weapon: R key when Combat Weapon is active
    static bool r_down_last = false;
    bool r_down = window.is_key_down(GLFW_KEY_R);
    if (r_down && !r_down_last) {
        if (m_active_tool == ToolSlot::CombatWeapon && m_weapon_ammo < m_weapon_stats.max_ammo && m_reload_timer <= 0.0f) {
            reload_weapon();
        }
    }
    r_down_last = r_down;

    // 2. Keyboard Movement with continuous bitset polling
    glm::vec3 wish_dir(0.0f);
    glm::vec3 flat_front = glm::normalize(glm::vec3(m_front.x, 0.0f, m_front.z));
    glm::vec3 flat_right = glm::normalize(glm::vec3(m_right.x, 0.0f, m_right.z));

    if (window.is_key_down(GLFW_KEY_W)) { wish_dir += flat_front; m_current_buttons |= BTN_FORWARD; }
    if (window.is_key_down(GLFW_KEY_S)) { wish_dir -= flat_front; m_current_buttons |= BTN_BACKWARD; }
    if (window.is_key_down(GLFW_KEY_A)) { wish_dir -= flat_right; m_current_buttons |= BTN_LEFT; }
    if (window.is_key_down(GLFW_KEY_D)) { wish_dir += flat_right; m_current_buttons |= BTN_RIGHT; }
    if (window.is_key_down(GLFW_KEY_LEFT_SHIFT)) { m_current_buttons |= BTN_SPRINT; }

    if (glm::length(wish_dir) > 0.001f) {
        wish_dir = glm::normalize(wish_dir);
    }

    float move_speed = ((m_current_buttons & BTN_SPRINT) ? 12.0f : 7.0f) * m_char_attr.moveSpeed * m_carry_weight_multiplier;
    if (m_on_ground) {
        m_velocity.x = wish_dir.x * move_speed;
        m_velocity.z = wish_dir.z * move_speed;
    } else {
        // Air control
        m_velocity.x += wish_dir.x * move_speed * 2.5f * dt;
        m_velocity.z += wish_dir.z * move_speed * 2.5f * dt;
        m_velocity.x *= (1.0f - 1.5f * dt);
        m_velocity.z *= (1.0f - 1.5f * dt);
    }

    // 3. Jump & Exo-Suit Thrusters
    if (window.is_key_down(GLFW_KEY_SPACE)) {
        if (m_on_ground) {
            m_velocity.y = 8.5f;
            m_on_ground = false;
            m_current_buttons |= BTN_JUMP;
            if (m_on_jump) {
                m_on_jump(m_position);
            }
        } else if (!m_exo.overheated && m_exo.power > 5.0f) {
            // Jetpack vertical hover thrusters
            m_velocity.y += 18.0f * dt;
            m_velocity.y = glm::clamp(m_velocity.y, -4.0f, 9.0f);
            float overburden_drain = (m_carry_weight_multiplier < 0.99f) ? (1.0f + (1.0f - m_carry_weight_multiplier) * 1.5f) : 1.0f;
            m_exo.power -= 25.0f * dt * overburden_drain;
            float heat_buildup = 20.0f * std::max(0.25f, 1.0f - m_upgrades.drillDurabilityTier * 0.15f);
            m_exo.heat += heat_buildup * dt;
            m_current_buttons |= BTN_THRUSTER;
        }
    }

    // Passive heat dissipation & power regeneration with Kinetic Dynamo perk
    if (!(m_current_buttons & BTN_THRUSTER)) {
        m_exo.heat = std::max(0.0f, m_exo.heat - 18.0f * dt);

        // Kinetic Dynamo: Movement and falling recharge fuel faster
        float dynamo_mult = 1.0f + m_upgrades.kineticDynamoTier * 0.15f;
        if ((m_current_buttons & BTN_SPRINT) || (!m_on_ground && m_velocity.y < -1.0f)) {
            dynamo_mult += m_upgrades.kineticDynamoTier * 0.15f;
        }

        float regen_rate = 15.0f * m_thruster_regen_multiplier * dynamo_mult;
        m_exo.power = std::min(m_exo.max_power, m_exo.power + regen_rate * dt);
        if (m_exo.heat < 15.0f) {
            m_exo.overheated = false;
        }
    }
    if (m_exo.heat >= 100.0f) {
        m_exo.overheated = true;
    }

    // 4. Grappling Hook (F key or Middle Mouse)
    if (window.is_key_down(GLFW_KEY_F) || window.is_mouse_button_down(GLFW_MOUSE_BUTTON_MIDDLE)) {
        m_current_buttons |= BTN_GRAPPLE_FIRE;
    }

    // 5. Reel Grapple (E key strictly reels grapple cable when active)
    if (window.is_key_down(GLFW_KEY_E)) {
        if (m_grapple.active) {
            m_current_buttons |= BTN_GRAPPLE_REEL;
            float effective_reel = m_grapple.reel_speed * m_reel_speed_multiplier * m_char_attr.grapplePullSpeed;
            m_grapple.rest_length = std::max(2.0f, m_grapple.rest_length - effective_reel * dt);
        }
    }

    // 6. Tool-specific mouse actions:
    // Slot 1 (Standard Excavator Drill):
    //   LMB: Mine/Drill single targeted voxel.
    //   RMB: Place Industrial Bulkhead (MAT_BULKHEAD) if bulkheads > 0.
    // Slot 2 (Reinforce / Builder Tool):
    //   LMB: Place Structural Support Bulkhead (MAT_BULKHEAD).
    //   RMB: Remove player-placed bulkhead cleanly without triggering cave-in checks.
    // Slot 3 (Demolition Shaped Charges):
    //   LMB: Deploy shaped charge on targeted surface.
    //   RMB: Detonate placed shaped charges (blasts 3x1x1 tunnel).
    bool lmb = window.is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT);
    bool rmb = window.is_mouse_button_down(GLFW_MOUSE_BUTTON_RIGHT);

    if (m_active_tool == ToolSlot::MiningDrill) {
        if (lmb) m_current_buttons |= BTN_MINE_DRILL;
        if (rmb) m_current_buttons |= BTN_PLACE_BLOCK;
    } else if (m_active_tool == ToolSlot::PlasmaCarbine) {
        if (lmb) m_current_buttons |= BTN_MINE_DRILL; // Triggers weapon fire
        if (rmb) m_current_buttons |= BTN_PLACE_BLOCK;
    } else if (m_active_tool == ToolSlot::IndustrialBulkhead) {
        if (lmb) m_current_buttons |= BTN_PLACE_BLOCK;
        if (rmb) m_current_buttons |= BTN_REMOVE_BULKHEAD;
    } else if (m_active_tool == ToolSlot::DemolitionCharge) {
        if (lmb) m_current_buttons |= BTN_SKILL_DEMO;       // Deploy shaped charge
        if (rmb) m_current_buttons |= BTN_DETONATE_CHARGE;   // Detonate placed charge
    }

    // 7. Surveying Sonar Pulse (Q key)
    static bool q_pressed_last = false;
    bool q_down = window.is_key_down(GLFW_KEY_Q);
    if (q_down && !q_pressed_last) {
        if (m_sonar_cooldown <= 0.0f) {
            m_current_buttons |= BTN_SKILL_SONAR;
            m_sonar_cooldown = m_sonar_max_cooldown;
            if (m_on_sonar_cast) {
                m_on_sonar_cast(m_position);
            }
        } else {
            if (m_on_warning) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "SONAR RECHARGING (%.1fs)", m_sonar_cooldown);
                m_on_warning(std::string(buf));
            }
        }
    }
    q_pressed_last = q_down;

    // 8. Class Tactical Ability (C key)
    static bool c_pressed_last = false;
    bool c_down = window.is_key_down(GLFW_KEY_C);
    if (c_down && !c_pressed_last) {
        if (m_tactical_cooldown <= 0.0f) {
            m_current_buttons |= BTN_TACTICAL_SKILL;
            m_tactical_cooldown = m_tactical_max_cooldown;
            if (m_on_tactical_ability) {
                m_on_tactical_ability(m_char_attr.classType, m_position, m_front);
            }
        } else {
            if (m_on_warning) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "TACTICAL RECHARGING (%.1fs)", m_tactical_cooldown);
                m_on_warning(std::string(buf));
            }
        }
    }
    c_pressed_last = c_down;
}

void PlayerController::update_physics(float dt, World& world) {
    m_current_world = &world;

    // 0. Update Combat Firearm cooldowns & passive capacitor recharge
    if (m_fire_cooldown > 0.0f) {
        m_fire_cooldown = std::max(0.0f, m_fire_cooldown - dt);
    }
    if (m_reload_timer > 0.0f) {
        m_reload_timer -= dt;
        if (m_reload_timer <= 0.0f) {
            m_reload_timer = 0.0f;
            m_weapon_ammo = m_weapon_stats.max_ammo;
        }
    } else if (m_weapon_stats.auto_recharge && m_weapon_ammo < m_weapon_stats.max_ammo) {
        if (m_recharge_delay > 0.0f) {
            m_recharge_delay -= dt;
        } else {
            // Passive recharge: 1 round every 0.35s when not firing
            static float recharge_accum = 0.0f;
            recharge_accum += dt;
            if (recharge_accum >= 0.35f) {
                recharge_accum = 0.0f;
                m_weapon_ammo++;
            }
        }
    }

    // 1. Grapple Fire & Tension Cable Dynamics
    // DDA raycast starts 0.6 units in front of the camera along the view vector to prevent self-collision
    if (m_current_buttons & BTN_GRAPPLE_FIRE) {
        if (!m_grapple.active) {
            glm::vec3 eye = m_position + glm::vec3(0.0f, 0.7f, 0.0f);
            glm::vec3 ray_origin = eye + m_front * 0.6f;
            RaycastHit hit = world.raycast(ray_origin, m_front, m_grapple.max_length);
            if (hit.hit && hit.voxel.is_solid()) {
                m_grapple.active = true;
                m_grapple.anchor_point = glm::vec3(hit.block_pos) + glm::vec3(0.5f);
                m_grapple.rest_length = glm::distance(m_position, m_grapple.anchor_point);
            }
        }
    } else {
        m_grapple.active = false;
    }

    if (m_grapple.active) {
        glm::vec3 to_anchor = m_grapple.anchor_point - m_position;
        float current_dist = glm::length(to_anchor);

        // Pull the player toward anchor point with tension
        if (current_dist > 0.1f) {
            float pull_speed = 28.0f * m_reel_speed_multiplier;
            m_velocity += glm::normalize(to_anchor) * (pull_speed * dt);
        }

        // Cancel grapple if within 1.5 units or if SPACE/jump is pressed, granting upward vault boost
        if (current_dist < 1.5f || (m_current_buttons & BTN_JUMP)) {
            m_grapple.active = false;
            m_velocity.y += 4.5f; // Upward vault boost
        }
    }

    // 2. Gravity
    if (!m_on_ground) {
        m_velocity.y -= 19.6f * dt;
    }

    // 3. Subterranean Voxel Collision Resolution (AABB vs Voxel Grid)
    resolve_voxel_collisions(world, m_position, m_velocity, dt);

    // 3b. Environmental Hazard Interaction (Lava, Spikes, Void Chasm)
    m_is_in_lava = false;
    m_is_in_spikes = false;
    if (m_spike_damage_timer > 0.0f) {
        m_spike_damage_timer -= dt;
    }

    int ground_x = static_cast<int>(std::floor(m_position.x));
    int ground_y = static_cast<int>(std::floor(m_position.y - 0.96f));
    int ground_z = static_cast<int>(std::floor(m_position.z));
    int body_y   = static_cast<int>(std::floor(m_position.y - 0.40f));
    Voxel underfoot = world.get_voxel(ground_x, ground_y, ground_z);
    Voxel lower_body = world.get_voxel(ground_x, body_y, ground_z);

    if (underfoot.material_id == MAT_THERMITE_SLAG || lower_body.material_id == MAT_THERMITE_SLAG) {
        m_is_in_lava = true;
        // Viscous fluid drag - severely hampers horizontal movement
        m_velocity.x *= std::max(0.0f, 1.0f - 8.5f * dt);
        m_velocity.z *= std::max(0.0f, 1.0f - 8.5f * dt);
        if (m_velocity.y < -3.0f) {
            m_velocity.y = -3.0f; // Buoyant terminal velocity in slag
        }

        float lava_dmg = 28.0f * dt;
        take_damage(lava_dmg, false);
        m_exo.heat = std::min(100.0f, m_exo.heat + 50.0f * dt);
        if (m_exo.heat >= 100.0f) {
            m_exo.overheated = true;
        }
        add_trauma(0.18f * dt);
        static float s_lava_warn = 0.0f;
        s_lava_warn += dt;
        if (s_lava_warn >= 0.8f) {
            s_lava_warn = 0.0f;
            if (m_on_warning) {
                if (m_health <= 0.0f) {
                    m_on_warning("VITAL SIGNS LOST: DELVER INCINERATED IN THERMITE SLAG");
                } else {
                    m_on_warning("CRITICAL: THERMAL BURNS / LAVA CONTACT (-28 HP/s)");
                }
            }
        }
    }

    if (underfoot.flags_and_damage == 0x0F || lower_body.flags_and_damage == 0x0F) {
        m_is_in_spikes = true;
        // Movement slowed to crawl while traversing puncture hazards
        m_velocity.x *= std::max(0.0f, 1.0f - 7.0f * dt);
        m_velocity.z *= std::max(0.0f, 1.0f - 7.0f * dt);

        if (m_spike_damage_timer <= 0.0f) {
            m_spike_damage_timer = 0.65f;
            take_damage(25.0f, false);
            add_trauma(0.45f);
            m_velocity.y = std::max(m_velocity.y, 2.2f); // Sharp hop recoil, not trampoline escape
            if (m_on_warning) {
                if (m_health <= 0.0f) {
                    m_on_warning("VITAL SIGNS LOST: DELVER IMPALED IN SPIKE TRENCH");
                } else {
                    m_on_warning("PUNCTURE HAZARD: SPIKE TRAP DETECTED (-25 HP)");
                }
            }
        }
    }

    if (m_position.y <= 1.8f) {
        take_damage(35.0f, false);
        add_trauma(0.60f);
        if (m_health > 0.0f) {
            m_position.y = 6.5f;
            m_velocity = glm::vec3(0.0f, 4.0f, 0.0f);
            if (m_on_warning) {
                m_on_warning("VOID RIFT: GRAVITATIONAL REPULSION (-35 HP)");
            }
        } else {
            m_velocity = glm::vec3(0.0f, -8.0f, 0.0f);
            if (m_on_warning) {
                m_on_warning("VITAL SIGNS LOST: DELVER CONSUMED BY VOID SINGULARITY");
            }
        }
    }

    // 4. Mining / Drilling Handling (Progressive cumulative damage with 1.0s decay)
    MineBlock(dt, world);

    // 5. Tool Actions & Cooldowns
    if (m_place_cooldown > 0.0f) {
        m_place_cooldown -= dt;
    }
    if (m_sonar_cooldown > 0.0f) {
        m_sonar_cooldown = std::max(0.0f, m_sonar_cooldown - dt);
    }
    if (m_tactical_cooldown > 0.0f) {
        m_tactical_cooldown = std::max(0.0f, m_tactical_cooldown - dt);
    }

    // Slot 3 LMB: Deploy Shaped Charge on targeted surface
    if (m_current_buttons & BTN_SKILL_DEMO) {
        if (m_place_cooldown <= 0.0f) {
            m_place_cooldown = 0.25f;
            glm::vec3 eye = m_position + glm::vec3(0.0f, 0.7f, 0.0f);
            glm::vec3 ray_origin = eye + m_front * 0.6f;
            RaycastHit hit = world.raycast(ray_origin, m_front, 7.5f);
            if (hit.hit && hit.voxel.material_id != MAT_DREDGE_BEDROCK) {
                m_placed_charge_pos = hit.block_pos;
                m_placed_charge_normal = hit.normal;
                m_has_placed_charge = true;
                if (m_on_warning) {
                    m_on_warning("SHAPED CHARGE DEPLOYED. PRESS [RMB] TO DETONATE TUNNEL.");
                }
            }
        }
    }

    // Slot 3 RMB: Detonate Placed Shaped Charge (blasts 3x1x1 tunnel)
    if (m_current_buttons & BTN_DETONATE_CHARGE) {
        if (m_place_cooldown <= 0.0f) {
            m_place_cooldown = 0.35f;
            if (m_has_placed_charge) {
                if (m_on_explosive_blast) {
                    m_on_explosive_blast(m_placed_charge_pos, m_placed_charge_normal, true);
                }
                m_has_placed_charge = false;
            } else {
                glm::vec3 eye = m_position + glm::vec3(0.0f, 0.7f, 0.0f);
                glm::vec3 ray_origin = eye + m_front * 0.6f;
                RaycastHit hit = world.raycast(ray_origin, m_front, 7.5f);
                if (hit.hit && hit.voxel.material_id != MAT_DREDGE_BEDROCK) {
                    if (m_on_explosive_blast) {
                        m_on_explosive_blast(hit.block_pos, hit.normal, true);
                    }
                }
            }
        }
    }

    // Slot 2 RMB: Remove Player-Placed Bulkhead cleanly without triggering cave-in checks
    if (m_current_buttons & BTN_REMOVE_BULKHEAD) {
        if (m_place_cooldown <= 0.0f) {
            m_place_cooldown = 0.25f;
            glm::vec3 eye = m_position + glm::vec3(0.0f, 0.7f, 0.0f);
            glm::vec3 ray_origin = eye + m_front * 0.6f;
            RaycastHit hit = world.raycast(ray_origin, m_front, 6.0f);
            if (hit.hit && (hit.voxel.material_id == MAT_INDUSTRIAL_BULKHEAD || hit.voxel.material_id == MAT_BULKHEAD)) {
                world.set_voxel(hit.block_pos.x, hit.block_pos.y, hit.block_pos.z, Voxel{MAT_AIR, 0}, true);
                if (m_on_bulkhead_dismantle) {
                    m_on_bulkhead_dismantle(hit.block_pos.x, hit.block_pos.y, hit.block_pos.z);
                }
                if (m_on_warning) {
                    m_on_warning("+1 BULKHEAD | 0 PTS");
                }
            }
        }
    }

    // Slot 1 RMB or Slot 2 LMB: Place Industrial Bulkhead
    if (m_current_buttons & BTN_PLACE_BLOCK) {
        if (m_place_cooldown <= 0.0f) {
            m_place_cooldown = 0.2f;

            if (m_can_place_predicate && !m_can_place_predicate()) {
                if (m_on_warning) {
                    m_on_warning("INSUFFICIENT BULKHEAD MATERIALS");
                }
            } else {
                glm::vec3 eye = m_position + glm::vec3(0.0f, 0.7f, 0.0f);
                glm::vec3 ray_origin = eye + m_front * 0.6f;
                RaycastHit hit = world.raycast(ray_origin, m_front, 6.0f);
                if (hit.hit) {
                    glm::ivec3 place_pos = hit.block_pos + hit.normal;

                    // AABB Self-Collision Check: verify place_pos does NOT intersect player bounding box
                    glm::vec3 player_min = m_position - glm::vec3(0.35f, 0.95f, 0.35f);
                    glm::vec3 player_max = m_position + glm::vec3(0.35f, 0.95f, 0.35f);
                    glm::vec3 block_min(place_pos);
                    glm::vec3 block_max = block_min + glm::vec3(1.0f);

                    bool overlaps = (player_min.x < block_max.x && player_max.x > block_min.x &&
                                     player_min.y < block_max.y && player_max.y > block_min.y &&
                                     player_min.z < block_max.z && player_max.z > block_min.z);

                    if (!overlaps) {
                        // Guarantee: face normal placement ONLY with VOXEL_FLAG_PLAYER_PLACED metadata
                        place_bulkhead(world, place_pos);
                    }
                }
            }
        }
    }
}

void PlayerController::place_bulkhead(World& world, const glm::ivec3& place_pos) {
    world.set_block_with_flags(place_pos, MAT_BULKHEAD, VOXEL_FLAG_PLAYER_PLACED);
    if (m_on_block_place) {
        m_on_block_place(place_pos.x, place_pos.y, place_pos.z, MAT_BULKHEAD);
    }
}

void PlayerController::PlaceBulkhead(World& world, const glm::ivec3& place_pos) {
    place_bulkhead(world, place_pos);
}

void PlayerController::clamp_to_surface(const World& world) {
    // Search downwards starting from current y position down to 0 for immediate walkable floor
    int start_y = std::min(25, static_cast<int>(std::floor(m_position.y + 1.0f)));
    for (int y = start_y; y >= 0; --y) {
        glm::ivec3 check_pos(static_cast<int>(std::floor(m_position.x)), y, static_cast<int>(std::floor(m_position.z)));
        if (world.is_solid(check_pos)) {
            if (!world.is_solid(check_pos + glm::ivec3(0, 1, 0)) &&
                !world.is_solid(check_pos + glm::ivec3(0, 2, 0))) {
                m_position.y = static_cast<float>(y + 1) + 0.95f;
                return;
            }
        }
    }
    // If not found below current y (e.g. spawned below an upper floor), scan from top of cavern (y=25) downward
    for (int y = 25; y >= 0; --y) {
        glm::ivec3 check_pos(static_cast<int>(std::floor(m_position.x)), y, static_cast<int>(std::floor(m_position.z)));
        if (world.is_solid(check_pos)) {
            if (!world.is_solid(check_pos + glm::ivec3(0, 1, 0)) &&
                !world.is_solid(check_pos + glm::ivec3(0, 2, 0))) {
                m_position.y = static_cast<float>(y + 1) + 0.95f;
                return;
            }
        }
    }
    float surface = world.get_highest_solid_surface(static_cast<int>(m_position.x), static_cast<int>(m_position.z));
    m_position.y = surface + 0.95f;
}

void PlayerController::UpdatePhysics(float dt) {
    if (m_current_world) {
        update_physics(dt, *m_current_world);
    }
}

void PlayerController::ResolveAxisCollision(int axis, const glm::vec3& half_extents) {
    if (m_current_world) {
        resolve_axis_collision(axis, half_extents, *m_current_world);
    }
}

void PlayerController::ResolveAxisCollision(int axis, const glm::vec3& half_extents, World& world) {
    m_current_world = &world;
    resolve_axis_collision(axis, half_extents, world);
}

bool PlayerController::is_penetrating_solid(const World& world) const {
    glm::vec3 half_extents(0.3f, 0.9f, 0.3f);
    glm::vec3 box_min = m_position - half_extents;
    glm::vec3 box_max = m_position + half_extents;

    int min_x = static_cast<int>(std::floor(box_min.x + 0.002f));
    int max_x = static_cast<int>(std::floor(box_max.x - 0.002f));
    int min_y = static_cast<int>(std::floor(box_min.y + 0.002f));
    int max_y = static_cast<int>(std::floor(box_max.y - 0.002f));
    int min_z = static_cast<int>(std::floor(box_min.z + 0.002f));
    int max_z = static_cast<int>(std::floor(box_max.z - 0.002f));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            for (int z = min_z; z <= max_z; ++z) {
                if (world.is_solid(glm::ivec3(x, y, z))) {
                    return true;
                }
            }
        }
    }
    return false;
}

void PlayerController::apply_fall_impact(float impact_speed) {
    if (impact_speed > 2.5f && m_on_land) {
        m_on_land(m_position, impact_speed);
    }
    if (impact_speed > 13.0f) {
        float vex = impact_speed - 13.0f;
        float raw_dmg = vex * (3.2f + 0.12f * vex);
        float applied = take_damage(raw_dmg, true);
        add_trauma(std::min(impact_speed * 0.035f, 0.6f));
        if (m_on_warning) {
            if (m_health <= 0.0f) {
                m_on_warning("FATAL IMPACT: KINETIC COMPRESSION BLUNT FORCE TRAUMA (-" + std::to_string(static_cast<int>(applied)) + " HP)");
            } else {
                m_on_warning("HARD IMPACT: SUIT DAMAGED (-" + std::to_string(static_cast<int>(applied)) + " HP)");
            }
        }
    }
}

void PlayerController::depenetrate(const glm::vec3& half_extents, World& world) {
    glm::vec3 box_min = m_position - half_extents;
    glm::vec3 box_max = m_position + half_extents;

    int min_x = static_cast<int>(std::floor(box_min.x + 0.002f));
    int max_x = static_cast<int>(std::floor(box_max.x - 0.002f));
    int min_y = static_cast<int>(std::floor(box_min.y + 0.002f));
    int max_y = static_cast<int>(std::floor(box_max.y - 0.002f));
    int min_z = static_cast<int>(std::floor(box_min.z + 0.002f));
    int max_z = static_cast<int>(std::floor(box_max.z - 0.002f));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            for (int z = min_z; z <= max_z; ++z) {
                if (world.is_solid(glm::ivec3(x, y, z))) {
                    float push_px = (static_cast<float>(x + 1) - box_min.x);
                    float push_nx = (box_max.x - static_cast<float>(x));
                    float push_py = (static_cast<float>(y + 1) - box_min.y);
                    float push_ny = (box_max.y - static_cast<float>(y));
                    float push_pz = (static_cast<float>(z + 1) - box_min.z);
                    float push_nz = (box_max.z - static_cast<float>(z));

                    float min_push = std::min({push_px, push_nx, push_py, push_ny, push_pz, push_nz});
                    if (min_push == push_py) {
                        m_position.y = static_cast<float>(y + 1) + half_extents.y;
                        if (m_velocity.y < 0.0f) {
                            apply_fall_impact(-m_velocity.y);
                            m_velocity.y = 0.0f;
                        }
                        m_on_ground = true;
                        m_isGrounded = true;
                    } else if (min_push == push_ny) {
                        m_position.y = static_cast<float>(y) - half_extents.y - 0.001f;
                        if (m_velocity.y > 0.0f) m_velocity.y = 0.0f;
                    } else if (min_push == push_px) {
                        m_position.x = static_cast<float>(x + 1) + half_extents.x + 0.001f;
                        if (m_velocity.x < 0.0f) m_velocity.x = 0.0f;
                    } else if (min_push == push_nx) {
                        m_position.x = static_cast<float>(x) - half_extents.x - 0.001f;
                        if (m_velocity.x > 0.0f) m_velocity.x = 0.0f;
                    } else if (min_push == push_pz) {
                        m_position.z = static_cast<float>(z + 1) + half_extents.z + 0.001f;
                        if (m_velocity.z < 0.0f) m_velocity.z = 0.0f;
                    } else if (min_push == push_nz) {
                        m_position.z = static_cast<float>(z) - half_extents.z - 0.001f;
                        if (m_velocity.z > 0.0f) m_velocity.z = 0.0f;
                    }

                    box_min = m_position - half_extents;
                    box_max = m_position + half_extents;
                }
            }
        }
    }
}

void PlayerController::resolve_axis_collision(int axis, const glm::vec3& half_extents, World& world) {
    glm::vec3 box_min = m_position - half_extents;
    glm::vec3 box_max = m_position + half_extents;

    int min_bx = static_cast<int>(std::floor(box_min.x + 0.002f));
    int max_bx = static_cast<int>(std::floor(box_max.x - 0.002f));
    int min_by = static_cast<int>(std::floor(box_min.y + 0.002f));
    int max_by = static_cast<int>(std::floor(box_max.y - 0.002f));
    int min_bz = static_cast<int>(std::floor(box_min.z + 0.002f));
    int max_bz = static_cast<int>(std::floor(box_max.z - 0.002f));

    if (axis == 1) { // 1. Y Axis (Gravity, jumping, floors, ceilings)
        if (m_velocity.y <= 0.0f) {
            int check_y = static_cast<int>(std::floor(box_min.y));
            bool landed = false;
            for (int x = min_bx; x <= max_bx; ++x) {
                for (int z = min_bz; z <= max_bz; ++z) {
                    if (world.is_solid(glm::ivec3(x, check_y, z))) {
                        float impact_speed = -m_velocity.y;
                        if (impact_speed > 0.0f) apply_fall_impact(impact_speed);
                        float target_floor = static_cast<float>(check_y + 1);
                        m_position.y = target_floor + half_extents.y;
                        m_velocity.y = 0.0f;
                        m_on_ground = true;
                        m_isGrounded = true;
                        landed = true;
                        break;
                    }
                }
                if (landed) break;
            }
            if (!landed) {
                // If resting right on top of a solid floor block (within 0.05 margin)
                int floor_below = static_cast<int>(std::floor(box_min.y - 0.02f));
                for (int x = min_bx; x <= max_bx; ++x) {
                    for (int z = min_bz; z <= max_bz; ++z) {
                        if (world.is_solid(glm::ivec3(x, floor_below, z))) {
                            if (box_min.y <= static_cast<float>(floor_below + 1) + 0.05f) {
                                m_position.y = static_cast<float>(floor_below + 1) + half_extents.y;
                                m_velocity.y = 0.0f;
                                m_on_ground = true;
                                m_isGrounded = true;
                                return;
                            }
                        }
                    }
                }
            }
        } else if (m_velocity.y > 0.0f) {
            int check_y = static_cast<int>(std::floor(box_max.y));
            for (int x = min_bx; x <= max_bx; ++x) {
                for (int z = min_bz; z <= max_bz; ++z) {
                    if (world.is_solid(glm::ivec3(x, check_y, z))) {
                        float target_ceiling = static_cast<float>(check_y);
                        m_position.y = target_ceiling - half_extents.y - 0.001f;
                        m_velocity.y = 0.0f;
                        return;
                    }
                }
            }
        }
    } else if (axis == 0) { // 2. X Axis (Walls)
        if (m_velocity.x > 0.0f) {
            int check_x = static_cast<int>(std::floor(box_max.x));
            bool has_collision = false;
            bool only_step = true;
            for (int y = min_by; y <= max_by; ++y) {
                for (int z = min_bz; z <= max_bz; ++z) {
                    if (world.is_solid(glm::ivec3(check_x, y, z))) {
                        has_collision = true;
                        if (y > min_by) {
                            only_step = false;
                        }
                    }
                }
            }
            if (has_collision) {
                bool can_step = m_on_ground && only_step;
                if (can_step) {
                    for (int z = min_bz; z <= max_bz; ++z) {
                        if (world.is_solid(glm::ivec3(check_x, min_by + 1, z)) ||
                            world.is_solid(glm::ivec3(check_x, min_by + 2, z))) {
                            can_step = false;
                            break;
                        }
                    }
                    if (can_step) {
                        for (int x = min_bx; x <= max_bx; ++x) {
                            for (int z = min_bz; z <= max_bz; ++z) {
                                if (world.is_solid(glm::ivec3(x, max_by + 1, z))) {
                                    can_step = false;
                                    break;
                                }
                            }
                            if (!can_step) break;
                        }
                    }
                }
                if (can_step) {
                    m_position.y += 1.0f;
                    m_on_ground = true;
                    m_isGrounded = true;
                } else {
                    m_position.x = static_cast<float>(check_x) - half_extents.x - 0.001f;
                    m_velocity.x = 0.0f;
                    return;
                }
            }
        } else if (m_velocity.x < 0.0f) {
            int check_x = static_cast<int>(std::floor(box_min.x));
            bool has_collision = false;
            bool only_step = true;
            for (int y = min_by; y <= max_by; ++y) {
                for (int z = min_bz; z <= max_bz; ++z) {
                    if (world.is_solid(glm::ivec3(check_x, y, z))) {
                        has_collision = true;
                        if (y > min_by) {
                            only_step = false;
                        }
                    }
                }
            }
            if (has_collision) {
                bool can_step = m_on_ground && only_step;
                if (can_step) {
                    for (int z = min_bz; z <= max_bz; ++z) {
                        if (world.is_solid(glm::ivec3(check_x, min_by + 1, z)) ||
                            world.is_solid(glm::ivec3(check_x, min_by + 2, z))) {
                            can_step = false;
                            break;
                        }
                    }
                    if (can_step) {
                        for (int x = min_bx; x <= max_bx; ++x) {
                            for (int z = min_bz; z <= max_bz; ++z) {
                                if (world.is_solid(glm::ivec3(x, max_by + 1, z))) {
                                    can_step = false;
                                    break;
                                }
                            }
                            if (!can_step) break;
                        }
                    }
                }
                if (can_step) {
                    m_position.y += 1.0f;
                    m_on_ground = true;
                    m_isGrounded = true;
                } else {
                    m_position.x = static_cast<float>(check_x + 1) + half_extents.x + 0.001f;
                    m_velocity.x = 0.0f;
                    return;
                }
            }
        }
    } else if (axis == 2) { // 3. Z Axis (Walls)
        if (m_velocity.z > 0.0f) {
            int check_z = static_cast<int>(std::floor(box_max.z));
            bool has_collision = false;
            bool only_step = true;
            for (int y = min_by; y <= max_by; ++y) {
                for (int x = min_bx; x <= max_bx; ++x) {
                    if (world.is_solid(glm::ivec3(x, y, check_z))) {
                        has_collision = true;
                        if (y > min_by) {
                            only_step = false;
                        }
                    }
                }
            }
            if (has_collision) {
                bool can_step = m_on_ground && only_step;
                if (can_step) {
                    for (int x = min_bx; x <= max_bx; ++x) {
                        if (world.is_solid(glm::ivec3(x, min_by + 1, check_z)) ||
                            world.is_solid(glm::ivec3(x, min_by + 2, check_z))) {
                            can_step = false;
                            break;
                        }
                    }
                    if (can_step) {
                        for (int x = min_bx; x <= max_bx; ++x) {
                            for (int z = min_bz; z <= max_bz; ++z) {
                                if (world.is_solid(glm::ivec3(x, max_by + 1, z))) {
                                    can_step = false;
                                    break;
                                }
                            }
                            if (!can_step) break;
                        }
                    }
                }
                if (can_step) {
                    m_position.y += 1.0f;
                    m_on_ground = true;
                    m_isGrounded = true;
                } else {
                    m_position.z = static_cast<float>(check_z) - half_extents.z - 0.001f;
                    m_velocity.z = 0.0f;
                    return;
                }
            }
        } else if (m_velocity.z < 0.0f) {
            int check_z = static_cast<int>(std::floor(box_min.z));
            bool has_collision = false;
            bool only_step = true;
            for (int y = min_by; y <= max_by; ++y) {
                for (int x = min_bx; x <= max_bx; ++x) {
                    if (world.is_solid(glm::ivec3(x, y, check_z))) {
                        has_collision = true;
                        if (y > min_by) {
                            only_step = false;
                        }
                    }
                }
            }
            if (has_collision) {
                bool can_step = m_on_ground && only_step;
                if (can_step) {
                    for (int x = min_bx; x <= max_bx; ++x) {
                        if (world.is_solid(glm::ivec3(x, min_by + 1, check_z)) ||
                            world.is_solid(glm::ivec3(x, min_by + 2, check_z))) {
                            can_step = false;
                            break;
                        }
                    }
                    if (can_step) {
                        for (int x = min_bx; x <= max_bx; ++x) {
                            for (int z = min_bz; z <= max_bz; ++z) {
                                if (world.is_solid(glm::ivec3(x, max_by + 1, z))) {
                                    can_step = false;
                                    break;
                                }
                            }
                            if (!can_step) break;
                        }
                    }
                }
                if (can_step) {
                    m_position.y += 1.0f;
                    m_on_ground = true;
                    m_isGrounded = true;
                } else {
                    m_position.z = static_cast<float>(check_z + 1) + half_extents.z + 0.001f;
                    m_velocity.z = 0.0f;
                    return;
                }
            }
        }
    }
}

void PlayerController::resolve_voxel_collisions(World& world, glm::vec3& pos, glm::vec3& vel, float dt) {
    m_on_ground = false;
    m_isGrounded = false;
    glm::vec3 half_extents(0.3f, 0.9f, 0.3f);

    float max_speed = std::max({std::abs(m_velocity.x), std::abs(m_velocity.y), std::abs(m_velocity.z)});
    int steps = std::max(1, static_cast<int>(std::ceil(max_speed * dt / 0.12f)));
    steps = std::min(steps, 16); // up to 16 sub-steps for high-speed grapple & thrusters
    float sub_dt = dt / static_cast<float>(steps);

    for (int step = 0; step < steps; ++step) {
        // 1. Resolve Y Axis (Gravity, jumping, floors, ceilings)
        m_position.y += m_velocity.y * sub_dt;
        resolve_axis_collision(1, half_extents, world);

        // 2. Resolve X Axis (Walls)
        m_position.x += m_velocity.x * sub_dt;
        resolve_axis_collision(0, half_extents, world);

        // 3. Resolve Z Axis (Walls)
        m_position.z += m_velocity.z * sub_dt;
        resolve_axis_collision(2, half_extents, world);

        // 4. Depenetrate static overlaps
        depenetrate(half_extents, world);
    }

    // Ground support probe: ensure m_on_ground stays true while standing or walking on floor
    if (m_velocity.y <= 0.05f) {
        int ground_y = static_cast<int>(std::floor(m_position.y - half_extents.y - 0.05f));
        int min_x = static_cast<int>(std::floor(m_position.x - 0.28f));
        int max_x = static_cast<int>(std::floor(m_position.x + 0.28f));
        int min_z = static_cast<int>(std::floor(m_position.z - 0.28f));
        int max_z = static_cast<int>(std::floor(m_position.z + 0.28f));

        for (int x = min_x; x <= max_x; ++x) {
            for (int z = min_z; z <= max_z; ++z) {
                if (world.is_solid(glm::ivec3(x, ground_y, z))) {
                    m_on_ground = true;
                    m_isGrounded = true;
                    if (m_velocity.y < 0.0f) {
                        apply_fall_impact(-m_velocity.y);
                        m_velocity.y = 0.0f;
                    }
                    m_position.y = static_cast<float>(ground_y + 1) + half_extents.y;
                    break;
                }
            }
            if (m_on_ground) break;
        }
    }

    pos = m_position;
    vel = m_velocity;
}

PlayerInputPacket PlayerController::build_input_packet(uint32_t tick, float dt) const {
    PlayerInputPacket pkt;
    pkt.client_tick = tick;
    pkt.sequence_id = tick;
    pkt.move_dir = m_front;
    pkt.look_angles = glm::vec2(m_pitch, m_yaw);
    pkt.buttons = m_current_buttons;
    pkt.delta_time = dt;
    return pkt;
}

void PlayerController::MineBlock(float dt) {
    if (m_current_world) {
        MineBlock(dt, *m_current_world);
    }
}

void PlayerController::MineBlock(float dt, World& world) {
    bool is_mining = (m_current_buttons & BTN_MINE_DRILL) != 0 && (m_active_tool == ToolSlot::MiningDrill);

    if (is_mining) {
        glm::vec3 eye = m_position + glm::vec3(0.0f, 0.7f, 0.0f);
        glm::vec3 ray_origin = eye + m_front * 0.6f;
        RaycastHit hit = world.raycast(ray_origin, m_front, 6.5f);

        if (hit.hit && hit.voxel.material_id != MAT_DREDGE_BEDROCK) {
            if (hit.block_pos != m_target_block) {
                m_target_block = hit.block_pos;
                m_target_normal = hit.normal;
                m_drillDamageAccumulator = 0.0f;
                m_target_block_damage = 0.0f;
                m_mine_timer = 0.0f;

                // Calibrate block hardness according to material
                m_block_hardness = (hit.voxel.material_id == MAT_VOIDITE_CRYSTAL) ? 0.35f :
                                   (hit.voxel.material_id == MAT_FRACTURED_GRANITE) ? 0.45f :
                                   (hit.voxel.material_id == MAT_RADIOACTIVE_ORE) ? 0.80f :
                                   (hit.voxel.material_id == MAT_INDUSTRIAL_BULKHEAD) ? 1.20f :
                                   (hit.voxel.material_id == MAT_REINFORCED_VAULT_DOOR) ? 2.50f : 0.60f;
            }

            // Track cumulative drill progress per block factoring baseMineSpeed and drillSpeedTier
            float effective_mine_speed = m_char_attr.baseMineSpeed * (1.0f + m_upgrades.drillSpeedTier * 0.12f) * std::max(0.2f, m_drill_speed_multiplier);
            m_drillDamageAccumulator += effective_mine_speed * dt;
            m_target_block_damage = m_drillDamageAccumulator;
            m_mine_timer = m_drillDamageAccumulator;
            m_target_time_to_break = m_block_hardness;

            // Micro-trauma camera vibration while drill bit bites into rock
            add_trauma(0.015f * dt);

            float progress = m_block_hardness > 0.0f ? (m_drillDamageAccumulator / m_block_hardness) : 1.0f;
            if (progress >= 1.0f) {
                Voxel target_vox = world.get_voxel(hit.block_pos.x, hit.block_pos.y, hit.block_pos.z);
                uint8_t old_mat = target_vox.material_id;
                uint8_t old_flags = target_vox.flags_and_damage;
                glm::ivec3 break_pos = hit.block_pos;
                glm::ivec3 break_norm = hit.normal;
                world.set_voxel(break_pos.x, break_pos.y, break_pos.z, Voxel{MAT_AIR, 0}, true);
                if (m_on_block_break) {
                    m_on_block_break(break_pos.x, break_pos.y, break_pos.z, break_norm, old_mat, old_flags);
                }
                add_trauma(0.035f); // Punchy release impulse upon block shatter
                m_drillDamageAccumulator = 0.0f;
                m_target_block_damage = 0.0f;
                m_mine_timer = 0.0f;
                m_target_block = glm::ivec3(-1);
            }
        } else {
            // Player looked away or hit bedrock: decay back to 0
            if (m_drillDamageAccumulator > 0.0f) {
                float decay_rate = (m_block_hardness > 0.0f ? m_block_hardness : 1.0f) * 2.0f;
                m_drillDamageAccumulator = std::max(0.0f, m_drillDamageAccumulator - decay_rate * dt);
                m_target_block_damage = m_drillDamageAccumulator;
                m_mine_timer = m_drillDamageAccumulator;
                if (m_drillDamageAccumulator <= 0.0f) {
                    m_target_block = glm::ivec3(-1);
                }
            }
        }
    } else {
        // Player released LMB: decay target block damage back to 0
        if (m_drillDamageAccumulator > 0.0f) {
            float decay_rate = (m_block_hardness > 0.0f ? m_block_hardness : 1.0f) * 2.0f;
            m_drillDamageAccumulator = std::max(0.0f, m_drillDamageAccumulator - decay_rate * dt);
            m_target_block_damage = m_drillDamageAccumulator;
            m_mine_timer = m_drillDamageAccumulator;
            if (m_drillDamageAccumulator <= 0.0f) {
                m_target_block = glm::ivec3(-1);
            }
        }
    }
}

void PlayerController::set_character_class(CharacterClass cls) {
    m_char_attr = get_character_attributes(cls);
    apply_attributes_and_upgrades(m_char_attr, m_upgrades);
}

void PlayerController::set_upgrades(const UpgradeTree& tree) {
    m_upgrades = tree;
    apply_attributes_and_upgrades(m_char_attr, m_upgrades);
}

void PlayerController::apply_attributes_and_upgrades(const CharacterAttributes& attr, const UpgradeTree& upg) {
    m_char_attr = attr;
    m_upgrades = upg;

    m_max_health = attr.suitIntegrity + (upg.reinforcedPlatingTier * 15.0f);
    m_health = m_max_health;
    m_exo.max_power = 100.0f * (1.0f + upg.thrusterTankTier * 0.20f);
    m_exo.power = m_exo.max_power;
    m_exo.integrity = 100.0f;
    m_sonar_max_cooldown = std::max(4.0f, attr.sonarCooldown - (upg.sonarFrequencyTier * 1.0f));
    m_tactical_max_cooldown = (attr.classType == CharacterClass::Demolitionist) ? 18.0f :
                              (attr.classType == CharacterClass::Vanguard) ? 20.0f : 12.0f;

    m_weapon_stats = get_class_weapon_stats(attr.classType);
    m_weapon_ammo = m_weapon_stats.max_ammo;
    m_reload_timer = 0.0f;
    m_fire_cooldown = 0.0f;
    m_recharge_delay = 0.0f;
}

void PlayerController::apply_attributes_and_upgrades(CharacterClass cls, const UpgradeTree& upg) {
    apply_attributes_and_upgrades(get_character_attributes(cls), upg);
}

float PlayerController::take_damage(float dmg, bool is_falling_debris) {
    if (is_falling_debris) {
        float reduction = m_char_attr.fallingDamageReduction + (m_upgrades.reinforcedPlatingTier * 0.10f);
        reduction = std::clamp(reduction, 0.0f, 0.85f);
        dmg *= (1.0f - reduction);
    }
    m_health = std::max(0.0f, m_health - dmg);
    m_exo.integrity = (m_max_health > 0.0f) ? (m_health / m_max_health * 100.0f) : 0.0f;
    add_trauma(dmg * 0.02f);
    return dmg;
}

void PlayerController::reload_weapon() {
    if (m_reload_timer > 0.0f || m_weapon_ammo >= m_weapon_stats.max_ammo) return;
    m_reload_timer = m_weapon_stats.reload_time;
}

bool PlayerController::try_fire_weapon(std::vector<PlayerPlasmaBolt>& out_bolts, float dt) {
    if (m_active_tool != ToolSlot::CombatWeapon) return false;
    if (m_reload_timer > 0.0f) return false;
    if (m_fire_cooldown > 0.0f) return false;
    if ((m_current_buttons & BTN_MINE_DRILL) == 0) return false;

    if (m_weapon_ammo <= 0) {
        reload_weapon();
        return false;
    }

    m_weapon_ammo--;
    m_fire_cooldown = m_weapon_stats.fire_rate;
    m_recharge_delay = 1.5f;

    // Projectile spawn position: eye level offset forward & slightly right
    glm::vec3 base_origin = m_position + glm::vec3(0.0f, 0.45f, 0.0f) + m_front * 0.45f + m_right * 0.12f;

    for (int p = 0; p < m_weapon_stats.pellets; ++p) {
        glm::vec3 fire_dir = m_front;
        if (m_weapon_stats.spread > 0.0f) {
            float rx = ((rand() % 1000) / 500.0f - 1.0f) * m_weapon_stats.spread;
            float ry = ((rand() % 1000) / 500.0f - 1.0f) * m_weapon_stats.spread;
            fire_dir = glm::normalize(m_front + m_right * rx + m_up * ry);
        }

        PlayerPlasmaBolt bolt;
        bolt.position = base_origin;
        bolt.velocity = fire_dir * m_weapon_stats.projectile_speed;
        bolt.lifetime = m_weapon_stats.projectile_lifetime;
        bolt.damage = m_weapon_stats.damage_per_pellet;
        bolt.color = m_weapon_stats.tracer_color;
        bolt.radius = m_weapon_stats.projectile_radius;
        bolt.active = true;
        out_bolts.push_back(bolt);
    }

    if (m_weapon_ammo == 0) {
        reload_weapon();
    }

    return true;
}

bool PlayerController::try_fire_weapon(glm::vec3& out_origin, glm::vec3& out_dir, float dt) {
    std::vector<PlayerPlasmaBolt> temp;
    if (try_fire_weapon(temp, dt) && !temp.empty()) {
        out_origin = temp[0].position;
        out_dir = glm::normalize(temp[0].velocity);
        return true;
    }
    return false;
}

} // namespace Voidfall
