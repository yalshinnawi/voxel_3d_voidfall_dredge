#include "controller.hpp"
#include "../systems/stealth_system.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Voidfall {

PlayerController::PlayerController(const glm::vec3& start_pos)
    : m_position(start_pos)
{
    update_camera_vectors();
}

void PlayerController::ProcessStanceChange(bool crouched) {
    if (m_is_crouching != crouched) {
        m_is_crouching = crouched;
        // Toggling or holding crouch must produce 0.0 acoustic impulse
        StealthSystem::instance().AddNoise(0.0f);
    }
}

RaycastHit PlayerController::QueryRaycastTarget(const World& world, float max_dist) const {
    return get_look_target(world, max_dist);
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
    glm::vec3 eye = eye_position();
    return glm::lookAt(eye, eye + m_front, m_up);
}

RaycastHit PlayerController::get_look_target(const World& world, float max_dist) const {
    glm::vec3 eye = eye_position();
    return world.raycast(eye, m_front, max_dist);
}

void PlayerController::handle_input(const Window& window, float dt) {
    m_current_buttons = 0;

    // Paused while inspecting tactical map overlay or interacting with focused UI
    if (m_combat_inputs_paused) {
        if (m_on_ground) {
            m_velocity.x = 0.0f;
            m_velocity.z = 0.0f;
        }
        return;
    }

    // 1. Mouse Look
    {
        glm::dvec2 mouse_delta = const_cast<Window&>(window).get_cursor_delta();
        float sens = m_mouse_sensitivity;
        if (m_zoom_progress > 0.0f && is_weapon_equipped()) {
            // Scale sensitivity proportionally with FOV zoom level for precise tactical aim
            float zoom_ratio = glm::mix(1.0f, m_weapon_stats.zoom_fov_multiplier, m_zoom_progress);
            sens *= zoom_ratio;
        }
        m_yaw   += static_cast<float>(mouse_delta.x) * sens;
        m_pitch += static_cast<float>(mouse_delta.y) * sens;
        m_pitch  = glm::clamp(m_pitch, -89.0f, 89.0f);
        update_camera_vectors();

        // Tool selection: 1 (Mining Drill), 2 (Combat Weapon), 3/4 (Demolition Charge)
        // Bulkhead is placed directly with Right Click while on Mining Drill (bulk tool slot removed)
        if (window.is_key_down(GLFW_KEY_1)) set_active_tool(ToolSlot::MiningDrill);
        if (window.is_key_down(GLFW_KEY_2)) set_active_tool(ToolSlot::CombatWeapon);
        if (window.is_key_down(GLFW_KEY_3) || window.is_key_down(GLFW_KEY_4)) set_active_tool(ToolSlot::DemolitionCharge);
    }

    // Mouse scroll wheel tool cycling: scroll down -> next weapon, scroll up -> prev weapon
    double scroll_y = const_cast<Window&>(window).get_scroll_delta_y();
    if (scroll_y != 0.0) {
        if (scroll_y < -0.1) {
            cycle_tool_forward();
        } else if (scroll_y > 0.1) {
            cycle_tool_backward();
        }
    }

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
    bool crouch_held = window.is_key_down(GLFW_KEY_LEFT_CONTROL);
    ProcessStanceChange(crouch_held);

    if (glm::length(wish_dir) > 0.001f) {
        wish_dir = glm::normalize(wish_dir);
    }

    float move_speed = ((m_current_buttons & BTN_SPRINT) ? 12.0f : 7.0f) * m_char_attr.moveSpeed * m_carry_weight_multiplier;
    if (m_is_crouching) {
        move_speed *= 0.55f;
    }
    if (m_is_in_lava) {
        move_speed *= 0.40f; // Trudging through viscous molten slag
    } else if (m_is_in_water) {
        move_speed *= 0.70f; // Wading through water pool
    }

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

    // 3. Jump, Exo-Suit Thrusters & Thruster Air-Brake
    m_thruster_air_braking = false;
    if (window.is_key_down(GLFW_KEY_SPACE)) {
        if (m_on_ground) {
            m_velocity.y = m_is_in_lava ? 4.5f : (m_is_in_water ? 6.5f : 8.5f);
            m_on_ground = false;
            m_current_buttons |= BTN_JUMP;
            if (m_on_jump) {
                m_on_jump(m_position);
            }
        } else if (m_is_in_liquid) {
            // Swimming / struggling upward in fluid when holding SPACE
            m_current_buttons |= BTN_JUMP;
            if (m_is_in_lava) {
                // Struggling upward against dense molten slag
                m_velocity.y = std::min(2.0f, m_velocity.y + 14.0f * dt);
            } else {
                // Swimming upward in clear water/coolant
                m_velocity.y = std::min(3.8f, m_velocity.y + 20.0f * dt);
            }
            if (!m_exo.overheated && m_exo.power > 3.0f) {
                // Thruster boost assistance in liquid
                m_velocity.y += 10.0f * dt;
                m_exo.power = std::max(0.0f, m_exo.power - 15.0f * dt);
                m_current_buttons |= BTN_THRUSTER;
            }
        } else if (!m_exo.overheated && m_exo.power > 3.0f) {
            // Thruster Air-Brake: Holding SPACE while falling at high downward velocity (vy < -12.0 m/s)
            // consumes jetpack fuel to cap terminal velocity at a survivable -5.0 m/s.
            if (m_velocity.y < -12.0f) {
                m_thruster_air_braking = true;
                m_velocity.y = std::max(-5.0f, m_velocity.y + 45.0f * dt);
                if (m_velocity.y > -5.0f) m_velocity.y = -5.0f;
                m_exo.power = std::max(0.0f, m_exo.power - 25.0f * dt);
                m_current_buttons |= BTN_THRUSTER;
            } else {
                // Jetpack vertical hover thrusters
                m_velocity.y += 18.0f * dt;
                m_velocity.y = glm::clamp(m_velocity.y, -4.0f, 9.0f);
                float overburden_drain = (m_carry_weight_multiplier < 0.99f) ? (1.0f + (1.0f - m_carry_weight_multiplier) * 1.5f) : 1.0f;
                m_exo.power -= 25.0f * dt * overburden_drain;
                float heat_buildup = drill_heat_buildup_rate();
                m_exo.heat += heat_buildup * dt;
                m_current_buttons |= BTN_THRUSTER;
            }
        }
    }

    // Passive heat dissipation & power regeneration with Kinetic Dynamo perk
    if (!(m_current_buttons & BTN_THRUSTER)) {
        m_exo.heat = std::max(0.0f, m_exo.heat - drill_heat_dissipation_rate() * dt);

        // Kinetic Dynamo: Movement and falling recharge fuel faster
        bool sprint_or_fall = (m_current_buttons & BTN_SPRINT) || (!m_on_ground && m_velocity.y < -1.0f);
        float dynamo_mult = dynamo_multiplier(sprint_or_fall);

        float regen_rate = 15.0f * m_thruster_regen_multiplier * dynamo_mult;
        m_exo.power = std::min(m_exo.max_power, m_exo.power + regen_rate * dt);
        if (m_exo.heat < 15.0f) {
            m_exo.overheated = false;
        }
    }
    if (m_exo.heat >= 100.0f) {
        m_exo.overheated = true;
    }

    // 4. Flashlight / Headlamp Toggle (F key, L key secondary)
    static bool f_pressed_last = false;
    bool f_down = window.is_key_down(GLFW_KEY_F) || window.is_key_down(GLFW_KEY_L);
    if (f_down && !f_pressed_last) {
        toggle_headlamp();
        if (m_on_warning) {
            m_on_warning(m_headlamp_on ? "FLASHLIGHT: ON [F]" : "FLASHLIGHT: OFF [F]");
        }
    }
    f_pressed_last = f_down;

    // 5. Grappling Hook Launch (G key)
    if (window.is_key_down(GLFW_KEY_G)) {
        m_current_buttons |= BTN_GRAPPLE_FIRE;
    }

    // Interact & Reel Grapple (E key: interacts with world objects, reels cable when grapple active)
    bool e_down = window.is_key_down(GLFW_KEY_E);
    if (e_down && !m_e_pressed_last) {
        if (m_on_interact) {
            m_on_interact();
        }
    }
    m_e_pressed_last = e_down;

    if (e_down) {
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
    bool raw_lmb = window.is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT);
    if (!raw_lmb) {
        m_require_lmb_release = false;
    }

    bool can_primary_action = !m_combat_inputs_paused && !is_shooting_paused();
    bool lmb = can_primary_action && raw_lmb;
    bool rmb = !m_combat_inputs_paused && window.is_mouse_button_down(GLFW_MOUSE_BUTTON_RIGHT);

    if (m_active_tool == ToolSlot::MiningDrill) {
        m_is_aiming = false;
        if (lmb) m_current_buttons |= BTN_MINE_DRILL;
        if (rmb) m_current_buttons |= BTN_PLACE_BLOCK;
    } else if (m_active_tool == ToolSlot::CombatWeapon) {
        m_is_aiming = rmb; // Holding Right-Click zooms in with firearm!
        if (lmb) m_current_buttons |= BTN_MINE_DRILL; // Triggers weapon fire
    } else if (m_active_tool == ToolSlot::IndustrialBulkhead) {
        m_is_aiming = false;
        if (lmb) m_current_buttons |= BTN_PLACE_BLOCK;
        if (rmb) m_current_buttons |= BTN_REMOVE_BULKHEAD;
    } else if (m_active_tool == ToolSlot::DemolitionCharge) {
        m_is_aiming = false;
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

    // 9. Throwable Chemical Flares (Key: T or Z)
    static bool flare_pressed_last = false;
    bool flare_down = window.is_key_down(GLFW_KEY_T) || window.is_key_down(GLFW_KEY_Z);
    if (flare_down && !flare_pressed_last) {
        if (m_flare_count > 0) {
            throw_flare();
        } else {
            if (m_on_warning) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "FLARES DEPLETED (RECHARGING %.1fs)", m_flare_recharge_timer);
                m_on_warning(std::string(buf));
            }
        }
    }
    flare_pressed_last = flare_down;

    // 10. Defensive Quick Melee Shove (Key: V or Middle Mouse)
    static bool v_pressed_last = false;
    bool v_down = window.is_key_down(GLFW_KEY_V) || window.is_mouse_button_down(GLFW_MOUSE_BUTTON_MIDDLE);
    if (v_down && !v_pressed_last) {
        execute_melee_shove();
    }
    v_pressed_last = v_down;
}

void PlayerController::execute_melee_shove() {
    if (m_melee_shove_cooldown > 0.0f) return;
    m_melee_shove_cooldown = 0.8f;
    m_melee_shove_timer = 0.35f;
    add_trauma(0.15f);
    if (m_on_melee_shove) {
        m_on_melee_shove(eye_position(), m_front);
    }
}

void PlayerController::throw_flare() {
    if (m_flare_count <= 0) return;
    m_flare_count--;
    if (m_flare_recharge_timer <= 0.0f) {
        m_flare_recharge_timer = m_flare_max_recharge;
    }
    if (m_on_flare_thrown) {
        m_on_flare_thrown(eye_position(), m_front, m_char_attr.classType);
    }
}

void PlayerController::update_physics(float dt, World& world) {
    m_current_world = &world;

    m_insertionShieldTimer = std::max(0.0f, m_insertionShieldTimer - dt);
    if (m_spawn_shoot_pause_timer > 0.0f) {
        m_spawn_shoot_pause_timer = std::max(0.0f, m_spawn_shoot_pause_timer - dt);
    }

    // Flare recharge progression (15.0s per flare, capacity 3)
    if (m_flare_count < MAX_FLARES) {
        m_flare_recharge_timer -= dt;
        if (m_flare_recharge_timer <= 0.0f) {
            m_flare_count++;
            m_flare_recharge_timer = (m_flare_count < MAX_FLARES) ? m_flare_max_recharge : 0.0f;
        }
    } else {
        m_flare_recharge_timer = 0.0f;
    }

    // Breadcrumb trail placement (every 12m of horizontal distance traveled)
    float dist_moved = glm::distance(glm::vec3(m_position.x, 0.0f, m_position.z),
                                     glm::vec3(m_last_breadcrumb_pos.x, 0.0f, m_last_breadcrumb_pos.z));
    if (dist_moved > 0.01f && dist_moved < 50.0f) {
        m_dist_since_breadcrumb += dist_moved;
        m_last_breadcrumb_pos = m_position;
        if (m_dist_since_breadcrumb >= 12.0f) {
            m_breadcrumbs.push_back(m_position);
            m_dist_since_breadcrumb = 0.0f;
            if (m_breadcrumbs.size() > 128) {
                m_breadcrumbs.erase(m_breadcrumbs.begin());
            }
        }
    }

    // Smoothly interpolate first-person camera eye offset between standing and crouching
    float targetEyeHeight = m_is_crouching ? EYE_HEIGHT_CROUCH : EYE_HEIGHT_STAND;
    m_eyeHeight = glm::mix(m_eyeHeight, targetEyeHeight, 1.0f - std::exp(-18.0f * dt));
    if (std::abs(m_eyeHeight - targetEyeHeight) < 0.03f) {
        m_eyeHeight = targetEyeHeight;
    }

    if (m_vault_timer > 0.0f) {
        m_vault_timer = std::max(0.0f, m_vault_timer - dt);
    }

    // 0. Update Combat Firearm cooldowns & reload timer
    if (m_fire_cooldown > 0.0f) {
        m_fire_cooldown = std::max(0.0f, m_fire_cooldown - dt);
    }
    m_melee_shove_cooldown = std::max(0.0f, m_melee_shove_cooldown - dt);
    m_melee_shove_timer = std::max(0.0f, m_melee_shove_timer - dt);
    if (m_active_tool != ToolSlot::CombatWeapon || m_weapon_ammo >= m_weapon_stats.max_ammo) {
        m_reload_timer = 0.0f;
    } else if (m_reload_timer > 0.0f) {
        m_reload_timer -= dt;
        if (m_reload_timer <= 0.0f) {
            m_reload_timer = 0.0f;
            m_weapon_ammo = m_weapon_stats.max_ammo;
        }
    }

    // Smooth Aim-Down-Sights (ADS) zoom progression
    float ads_speed = 1.0f / std::max(0.04f, m_weapon_stats.ads_time);
    if (m_is_aiming && is_weapon_equipped()) {
        m_zoom_progress = std::min(1.0f, m_zoom_progress + dt * ads_speed);
    } else {
        m_zoom_progress = std::max(0.0f, m_zoom_progress - dt * ads_speed);
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
                m_grapple.just_fired = true;
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

    // 2a. Liquid & Environmental Hazard Spatial Query (AABB vs World Voxels)
    glm::vec3 box_min = m_position - half_extents();
    glm::vec3 box_max = m_position + half_extents();

    int min_bx = static_cast<int>(std::floor(box_min.x + 0.05f));
    int max_bx = static_cast<int>(std::floor(box_max.x - 0.05f));
    int min_by = static_cast<int>(std::floor(box_min.y));
    int max_by = static_cast<int>(std::floor(box_max.y));
    int min_bz = static_cast<int>(std::floor(box_min.z + 0.05f));
    int max_bz = static_cast<int>(std::floor(box_max.z - 0.05f));

    bool in_lava = false;
    bool in_water = false;
    float max_liquid_surface = box_min.y;

    for (int y = min_by; y <= max_by; ++y) {
        for (int x = min_bx; x <= max_bx; ++x) {
            for (int z = min_bz; z <= max_bz; ++z) {
                Voxel v = world.get_voxel(x, y, z);
                if (v.material_id == MAT_THERMITE_SLAG || v.material_id == MAT_MOLTEN_MAGMA || v.material_id == MAT_LAVA) {
                    in_lava = true;
                    max_liquid_surface = std::max(max_liquid_surface, static_cast<float>(y + 1));
                } else if (v.material_id == MAT_CRYSTAL_AQUIFER || v.material_id == MAT_WATER || v.material_id == MAT_AQUIFER) {
                    in_water = true;
                    max_liquid_surface = std::max(max_liquid_surface, static_cast<float>(y + 1));
                }
            }
        }
    }

    // Also check immediate underfoot for wading contact
    if (!in_lava && !in_water) {
        int foot_y = static_cast<int>(std::floor(box_min.y - 0.05f));
        for (int x = min_bx; x <= max_bx; ++x) {
            for (int z = min_bz; z <= max_bz; ++z) {
                Voxel v = world.get_voxel(x, foot_y, z);
                if (v.material_id == MAT_THERMITE_SLAG || v.material_id == MAT_MOLTEN_MAGMA || v.material_id == MAT_LAVA) {
                    in_lava = true;
                    max_liquid_surface = std::max(max_liquid_surface, static_cast<float>(foot_y + 1));
                } else if (v.material_id == MAT_CRYSTAL_AQUIFER || v.material_id == MAT_WATER || v.material_id == MAT_AQUIFER) {
                    in_water = true;
                    max_liquid_surface = std::max(max_liquid_surface, static_cast<float>(foot_y + 1));
                }
            }
        }
    }

    m_is_in_lava = in_lava;
    m_is_in_water = in_water;
    m_is_in_liquid = in_lava || in_water;
    if (m_is_in_liquid) {
        float player_height = 2.0f * half_extents().y;
        m_liquid_submersion = std::clamp((max_liquid_surface - box_min.y) / player_height, 0.0f, 1.0f);
        m_current_liquid_material = in_lava ? MAT_THERMITE_SLAG : MAT_CRYSTAL_AQUIFER;
    } else {
        m_liquid_submersion = 0.0f;
        m_current_liquid_material = MAT_AIR;
    }

    // 2b. Gravity & Fluid Dynamics
    if (m_is_in_lava) {
        // High density molten thermite / magma
        // Plunge braking: rapidly decelerate high fall velocities towards slow terminal sinking speed (-1.5 m/s)
        if (m_velocity.y < -1.5f) {
            m_velocity.y = glm::mix(m_velocity.y, -1.5f, std::min(1.0f, 12.0f * dt));
        } else if (!m_on_ground) {
            // Gentle buoyant sinking gravity in thick molten rock
            m_velocity.y -= 5.0f * dt;
            if (m_velocity.y < -1.5f) {
                m_velocity.y = -1.5f;
            }
        }
        // Viscous fluid drag - dampens horizontal movement
        m_velocity.x *= std::max(0.0f, 1.0f - 8.5f * dt);
        m_velocity.z *= std::max(0.0f, 1.0f - 8.5f * dt);
    } else if (m_is_in_water) {
        // Subterranean crystal aquifer water / coolant
        // Plunge braking towards buoyant terminal velocity (-3.0 m/s)
        if (m_velocity.y < -3.0f) {
            m_velocity.y = glm::mix(m_velocity.y, -3.0f, std::min(1.0f, 8.0f * dt));
        } else if (!m_on_ground) {
            m_velocity.y -= 7.5f * dt;
            if (m_velocity.y < -3.0f) {
                m_velocity.y = -3.0f;
            }
        }
        // Water fluid drag
        m_velocity.x *= std::max(0.0f, 1.0f - 2.8f * dt);
        m_velocity.z *= std::max(0.0f, 1.0f - 2.8f * dt);
    } else {
        // Standard in-air gravity
        if (!m_on_ground) {
            m_velocity.y -= 19.6f * dt;
        }
    }

    // Step-Up & Ramp Motion: Query ramp under feet and adjust ground projection vector
    UpdateMovement(dt, world);

    // 3. Subterranean Voxel Collision Resolution (AABB vs Voxel Grid)
    resolve_voxel_collisions(world, m_position, m_velocity, dt);

    // 3b. Environmental Hazard Interaction (Lava, Spikes, Void Chasm)
    m_is_in_spikes = false;
    if (m_spike_damage_timer > 0.0f) {
        m_spike_damage_timer -= dt;
    }

    if (m_is_in_lava) {
        float lava_dmg = 28.0f * dt;
        take_damage(lava_dmg, DamageSource::ThermalLava);
        m_exo.heat = std::min(100.0f, m_exo.heat + 50.0f * dt);
        if (m_exo.heat >= 100.0f) {
            m_exo.overheated = true;
        }
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

    if (m_is_in_water) {
        // Water immersion: rapid exo cooling and buoyant fluid drag
        m_exo.heat = std::max(0.0f, m_exo.heat - 75.0f * dt);
        m_exo.overheated = false;
    }

    int center_x = static_cast<int>(std::floor(m_position.x));
    int under_y  = static_cast<int>(std::floor(box_min.y - 0.05f));
    int center_z = static_cast<int>(std::floor(m_position.z));
    int torso_y  = static_cast<int>(std::floor(m_position.y));
    Voxel underfoot = world.get_voxel(center_x, under_y, center_z);
    Voxel lower_body = world.get_voxel(center_x, torso_y, center_z);

    if (underfoot.material_id == MAT_TOXIC_GAS || lower_body.material_id == MAT_TOXIC_GAS) {
        take_damage(22.0f * dt, DamageSource::ToxicGas);
    }

    bool is_spikes_underfoot = (underfoot.material_id == MAT_OBSIDIAN_SPIKES || lower_body.material_id == MAT_OBSIDIAN_SPIKES);
    if (is_spikes_underfoot) {
        m_is_in_spikes = true;
        // Movement slowed to crawl while traversing puncture hazards
        m_velocity.x *= std::max(0.0f, 1.0f - 7.0f * dt);
        m_velocity.z *= std::max(0.0f, 1.0f - 7.0f * dt);

        if (m_spike_damage_timer <= 0.0f) {
            m_spike_damage_timer = 0.65f;
            take_damage(25.0f, DamageSource::Spikes);
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

    if (m_position.y <= 1.8f && !m_on_ground) {
        float foot_y = m_position.y - half_extents().y;
        int check_bx = static_cast<int>(std::floor(m_position.x));
        int check_bz = static_cast<int>(std::floor(m_position.z));
        int check_by = static_cast<int>(std::floor(foot_y));
        if (!world.is_solid(glm::ivec3(check_bx, check_by, check_bz)) &&
            !world.is_solid(glm::ivec3(check_bx, check_by - 1, check_bz))) {
            take_damage(35.0f, DamageSource::VoidSingularity);
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
    }

    // 3c. Suit Nanite Field Stabilizer (Out-of-combat triage recuperation up to 75% max health)
    m_time_since_damage += dt;
    if (m_time_since_damage >= 12.0f && m_health > 0.0f && m_health < m_max_health * 0.75f) {
        float heal_amount = 2.5f * dt;
        m_health = std::min(m_max_health * 0.75f, m_health + heal_amount);
        m_exo.integrity = (m_max_health > 0.0f) ? (m_health / m_max_health * 100.0f) : 0.0f;
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

    // Slot 4 LMB: Deploy Shaped Satchel Charge on targeted surface
    if (m_current_buttons & BTN_SKILL_DEMO) {
        if (m_place_cooldown <= 0.0f) {
            m_place_cooldown = 0.30f;
            if (m_has_placed_charge) {
                if (m_on_warning) {
                    m_on_warning("SATCHEL CHARGE ALREADY PLANTED. PRESS [RMB] TO DETONATE.");
                }
            } else {
                glm::vec3 eye = m_position + glm::vec3(0.0f, 0.7f, 0.0f);
                glm::vec3 ray_origin = eye + m_front * 0.6f;
                RaycastHit hit = world.raycast(ray_origin, m_front, 7.5f);
                if (hit.hit && hit.voxel.material_id != MAT_DREDGE_BEDROCK) {
                    if (m_can_deploy_charge_predicate && !m_can_deploy_charge_predicate()) {
                        if (m_on_warning) {
                            m_on_warning("OUT OF DEMOLITION SATCHEL CHARGES");
                        }
                    } else {
                        m_placed_charge_pos = hit.block_pos;
                        m_placed_charge_normal = hit.normal;
                        m_has_placed_charge = true;
                        if (m_on_charge_placed) {
                            m_on_charge_placed(hit.block_pos, hit.normal);
                        }
                        if (m_on_warning) {
                            m_on_warning("SATCHEL CHARGE PLANTED. PRESS [RMB] TO DETONATE.");
                        }
                    }
                } else if (!hit.hit) {
                    if (m_on_warning) {
                        m_on_warning("OUT OF RANGE TO PLANT SATCHEL CHARGE");
                    }
                }
            }
        }
    }

    // Slot 4 RMB: Detonate Placed Satchel Charge via remote detonator
    if (m_current_buttons & BTN_DETONATE_CHARGE) {
        if (m_place_cooldown <= 0.0f) {
            m_place_cooldown = 0.35f;
            if (m_has_placed_charge) {
                if (m_on_explosive_blast) {
                    m_on_explosive_blast(m_placed_charge_pos, m_placed_charge_normal, false);
                }
                m_has_placed_charge = false;
                if (m_on_warning) {
                    m_on_warning("SATCHEL CHARGE DETONATED! CREATURES ALERTED!");
                }
            } else {
                if (m_on_warning) {
                    m_on_warning("NO ACTIVE CHARGE PLANTED. PRESS [LMB] TO PLANT SATCHEL CHARGE.");
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

static inline float calculate_ramp_surface_y(VoxelShape shape, int bx, int by, int bz, float px, float pz) {
    float u = glm::clamp(px - static_cast<float>(bx), 0.0f, 1.0f);
    float w = glm::clamp(pz - static_cast<float>(bz), 0.0f, 1.0f);
    float local_h = 0.0f;
    switch (shape) {
        case SHAPE_RAMP_POS_X: local_h = u; break;          // Slope rising toward +X
        case SHAPE_RAMP_NEG_X: local_h = 1.0f - u; break;   // Slope rising toward -X
        case SHAPE_RAMP_POS_Z: local_h = w; break;          // Slope rising toward +Z
        case SHAPE_RAMP_NEG_Z: local_h = 1.0f - w; break;   // Slope rising toward -Z
        default: local_h = 1.0f; break;
    }
    return static_cast<float>(by) + local_h;
}

static inline glm::vec3 get_ramp_normal(VoxelShape shape) {
    constexpr float INV_SQRT2 = 0.7071067811865475f;
    switch (shape) {
        case SHAPE_RAMP_POS_X:
            return glm::vec3(-INV_SQRT2, INV_SQRT2, 0.0f);
        case SHAPE_RAMP_NEG_X:
            return glm::vec3(INV_SQRT2, INV_SQRT2, 0.0f);
        case SHAPE_RAMP_POS_Z:
            return glm::vec3(0.0f, INV_SQRT2, -INV_SQRT2);
        case SHAPE_RAMP_NEG_Z:
            return glm::vec3(0.0f, INV_SQRT2, INV_SQRT2);
        default:
            return glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

void PlayerController::clamp_to_surface(const World& world) {
    // Search downwards starting from current y position down to 0 for immediate walkable floor
    float h_y = half_extents().y;
    int start_y = std::min(25, static_cast<int>(std::floor(m_position.y + 1.0f)));
    for (int y = start_y; y >= 0; --y) {
        glm::ivec3 check_pos(static_cast<int>(std::floor(m_position.x)), y, static_cast<int>(std::floor(m_position.z)));
        if (world.is_solid(check_pos)) {
            if (!world.is_solid(check_pos + glm::ivec3(0, 1, 0)) &&
                !world.is_solid(check_pos + glm::ivec3(0, 2, 0))) {
                m_position.y = static_cast<float>(y + 1) + h_y;
                m_velocity.y = 0.0f;
                m_on_ground = true;
                m_isGrounded = true;
                // Ensure player is oriented toward open grotto/corridor rather than facing solid rock wall
                RaycastHit hit = world.raycast(eye_position(), m_front, 1.5f);
                if (hit.hit) {
                    align_spawn_yaw(world);
                }
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
                m_position.y = static_cast<float>(y + 1) + h_y;
                m_velocity.y = 0.0f;
                m_on_ground = true;
                m_isGrounded = true;
                RaycastHit hit = world.raycast(eye_position(), m_front, 1.5f);
                if (hit.hit) {
                    align_spawn_yaw(world);
                }
                return;
            }
        }
    }
    float surface = world.get_highest_solid_surface(static_cast<int>(m_position.x), static_cast<int>(m_position.z));
    m_position.y = surface + h_y;
    m_velocity.y = 0.0f;
    m_on_ground = true;
    m_isGrounded = true;
    RaycastHit hit = world.raycast(eye_position(), m_front, 1.5f);
    if (hit.hit) {
        align_spawn_yaw(world);
    }
}

void PlayerController::align_spawn_yaw(const World& world) {
    float best_yaw = world.calculate_spawn_yaw(m_position);
    set_look_angles(best_yaw, 0.0f);
}

void PlayerController::UpdatePhysics(float dt) {
    float targetEyeHeight = m_is_crouching ? EYE_HEIGHT_CROUCH : EYE_HEIGHT_STAND;
    m_eyeHeight = glm::mix(m_eyeHeight, targetEyeHeight, 1.0f - std::exp(-18.0f * dt));
    if (std::abs(m_eyeHeight - targetEyeHeight) < 0.03f) {
        m_eyeHeight = targetEyeHeight;
    }
    if (m_current_world) {
        update_physics(dt, *m_current_world);
    } else {
        // In headless / worldless test runs, update player cooldowns and timers
        if (m_vault_timer > 0.0f) {
            m_vault_timer = std::max(0.0f, m_vault_timer - dt);
        }
        if (m_spawn_shoot_pause_timer > 0.0f) {
            m_spawn_shoot_pause_timer = std::max(0.0f, m_spawn_shoot_pause_timer - dt);
        }
        if (m_fire_cooldown > 0.0f) {
            m_fire_cooldown = std::max(0.0f, m_fire_cooldown - dt);
        }
        m_melee_shove_cooldown = std::max(0.0f, m_melee_shove_cooldown - dt);
        m_melee_shove_timer = std::max(0.0f, m_melee_shove_timer - dt);
        if (m_active_tool != ToolSlot::CombatWeapon || m_weapon_ammo >= m_weapon_stats.max_ammo) {
            m_reload_timer = 0.0f;
        } else if (m_reload_timer > 0.0f) {
            m_reload_timer = std::max(0.0f, m_reload_timer - dt);
            if (m_reload_timer == 0.0f) {
                m_weapon_ammo = m_weapon_stats.max_ammo;
            }
        }

        // Smooth Aim-Down-Sights (ADS) zoom progression in offline/test physics
        float ads_speed = 1.0f / std::max(0.04f, m_weapon_stats.ads_time);
        if (m_is_aiming && is_weapon_equipped()) {
            m_zoom_progress = std::min(1.0f, m_zoom_progress + dt * ads_speed);
        } else {
            m_zoom_progress = std::max(0.0f, m_zoom_progress - dt * ads_speed);
        }
    }
}

void PlayerController::Update(float dt) {
    UpdatePhysics(dt);
    StealthSystem::instance().Update(dt, m_is_crouching);
}

void PlayerController::Update(float dt, World& world) {
    m_current_world = &world;
    UpdateMovement(dt, world);
    UpdatePhysics(dt, world);
    StealthSystem::instance().Update(dt, m_is_crouching);
}

void PlayerController::UpdateMovement(float dt) {
    if (m_current_world) {
        UpdateMovement(dt, *m_current_world);
    }
}

void PlayerController::UpdateMovement(float dt, World& world) {
    m_current_world = &world;
    float h_y = half_extents().y;
    float foot_y = m_position.y - h_y;

    int bx = static_cast<int>(std::floor(m_position.x));
    int bz = static_cast<int>(std::floor(m_position.z));
    int by_foot = static_cast<int>(std::floor(foot_y));

    // 1. Query the shape flag of the voxel directly beneath the player's feet
    VoxelShape shape = SHAPE_CUBE;
    int ramp_bx = bx;
    int ramp_by = by_foot;
    int ramp_bz = bz;

    // Check voxel at feet level and voxel immediately below/above feet at player center
    Voxel v_curr = world.get_voxel(bx, by_foot, bz);
    Voxel v_below = world.get_voxel(bx, by_foot - 1, bz);
    Voxel v_above = world.get_voxel(bx, by_foot + 1, bz);

    if (v_above.is_ramp()) {
        shape = v_above.shape();
        ramp_by = by_foot + 1;
    } else if (v_curr.is_ramp()) {
        shape = v_curr.shape();
        ramp_by = by_foot;
    } else if (v_below.is_ramp()) {
        shape = v_below.shape();
        ramp_by = by_foot - 1;
    } else {
        // Also check footprint along movement direction or foot extents
        int check_bx = bx;
        int check_bz = bz;
        if (m_velocity.x > 0.1f) check_bx = static_cast<int>(std::floor(m_position.x + half_extents().x));
        else if (m_velocity.x < -0.1f) check_bx = static_cast<int>(std::floor(m_position.x - half_extents().x));

        if (m_velocity.z > 0.1f) check_bz = static_cast<int>(std::floor(m_position.z + half_extents().z));
        else if (m_velocity.z < -0.1f) check_bz = static_cast<int>(std::floor(m_position.z - half_extents().z));

        Voxel v_lead_above = world.get_voxel(check_bx, by_foot + 1, check_bz);
        Voxel v_lead_curr = world.get_voxel(check_bx, by_foot, check_bz);
        Voxel v_lead_below = world.get_voxel(check_bx, by_foot - 1, check_bz);
        if (v_lead_above.is_ramp()) {
            shape = v_lead_above.shape();
            ramp_bx = check_bx;
            ramp_by = by_foot + 1;
            ramp_bz = check_bz;
        } else if (v_lead_curr.is_ramp()) {
            shape = v_lead_curr.shape();
            ramp_bx = check_bx;
            ramp_by = by_foot;
            ramp_bz = check_bz;
        } else if (v_lead_below.is_ramp()) {
            shape = v_lead_below.shape();
            ramp_bx = check_bx;
            ramp_by = by_foot - 1;
            ramp_bz = check_bz;
        }
    }

    // 2. If standing on a SHAPE_RAMP_* block, adjust ground projection vector along the 45° slope normal
    if (is_ramp_shape(shape)) {
        float surf_y = calculate_ramp_surface_y(shape, ramp_bx, ramp_by, ramp_bz, m_position.x, m_position.z);
        float diff = foot_y - surf_y;

        // Player is considered standing on the ramp if feet are close to the slope surface
        bool jumping = (m_current_buttons & BTN_JUMP) != 0 || m_velocity.y > 6.0f;
        if (!jumping && diff >= -0.35f && diff <= 0.45f) {
            // Elevate player smoothly to surface
            m_position.y = surf_y + h_y;
            m_on_ground = true;
            m_isGrounded = true;

            glm::vec3 normal = get_ramp_normal(shape);

            // Project horizontal velocity along the 45° slope tangent:
            // v_slope = v - (v . n_ramp) * n_ramp
            glm::vec3 v_slope = m_velocity - glm::dot(m_velocity, normal) * normal;
            if (v_slope.y >= 0.0f) {
                m_velocity.y = v_slope.y;
            } else if (normal.y > 0.001f) {
                m_velocity.y = -(m_velocity.x * normal.x + m_velocity.z * normal.z) / normal.y;
            }
        }
    } else if (m_on_ground && !(m_current_buttons & BTN_JUMP)) {
        if (m_velocity.y > 0.0f) {
            m_velocity.y = 0.0f;
        }
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
                    Voxel vox = world.get_voxel(x, y, z);
                    if (vox.is_ramp()) {
                        float surf_y = calculate_ramp_surface_y(vox.shape(), x, y, z, m_position.x, m_position.z);
                        if (box_min.y >= surf_y - 0.02f) {
                            continue;
                        }
                    } else if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                        float surf_y = static_cast<float>(y) + 0.5f;
                        if (box_min.y >= surf_y - 0.02f) {
                            continue;
                        }
                    } else if (vox.shape() == SHAPE_SLAB_TOP) {
                        if (box_max.y <= static_cast<float>(y) + 0.5f || box_min.y >= static_cast<float>(y + 1) - 0.02f) {
                            continue;
                        }
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

void PlayerController::apply_fall_impact(float impact_speed) {
    if (m_is_in_liquid) {
        impact_speed *= 0.25f; // Viscous liquid cushions kinetic fall impact
    }
    if (impact_speed > 2.5f && m_on_land) {
        m_on_land(m_position, impact_speed);
    }
    if (impact_speed > 13.0f) {
        float vex = impact_speed - 13.0f;
        float raw_dmg = vex * (3.2f + 0.12f * vex);
        float applied = take_damage(raw_dmg, DamageSource::FallImpact);
        add_trauma(std::min(impact_speed * 0.04f, 0.85f));
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
                    Voxel vox = world.get_voxel(x, y, z);
                    if (vox.is_ramp()) {
                        float surf_y = calculate_ramp_surface_y(vox.shape(), x, y, z, m_position.x, m_position.z);
                        if (box_min.y >= surf_y - 0.02f) {
                            continue;
                        }
                        if (box_min.y >= surf_y - 0.60f) {
                            m_position.y = surf_y + half_extents.y;
                            if (m_velocity.y < 0.0f) {
                                m_velocity.y = 0.0f;
                            }
                            m_on_ground = true;
                            m_isGrounded = true;
                            box_min = m_position - half_extents;
                            box_max = m_position + half_extents;
                            continue;
                        }
                    } else if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                        float surf_y = static_cast<float>(y) + 0.5f;
                        if (box_min.y >= surf_y - 0.02f) {
                            continue;
                        }
                        if (box_min.y >= surf_y - 0.60f) {
                            m_position.y = surf_y + half_extents.y;
                            if (m_velocity.y < 0.0f) {
                                m_velocity.y = 0.0f;
                            }
                            m_on_ground = true;
                            m_isGrounded = true;
                            box_min = m_position - half_extents;
                            box_max = m_position + half_extents;
                            continue;
                        }
                    } else if (vox.shape() == SHAPE_SLAB_TOP) {
                        float surf_y = static_cast<float>(y + 1);
                        if (box_min.y >= surf_y - 0.02f || box_max.y <= static_cast<float>(y) + 0.5f) {
                            continue;
                        }
                        if (box_min.y >= surf_y - 0.60f) {
                            m_position.y = surf_y + half_extents.y;
                            if (m_velocity.y < 0.0f) {
                                m_velocity.y = 0.0f;
                            }
                            m_on_ground = true;
                            m_isGrounded = true;
                            box_min = m_position - half_extents;
                            box_max = m_position + half_extents;
                            continue;
                        }
                    }

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
    int min_wall_y = static_cast<int>(std::floor(box_min.y + 0.15f));
    int max_by = static_cast<int>(std::floor(box_max.y - 0.002f));
    int min_bz = static_cast<int>(std::floor(box_min.z + 0.002f));
    int max_bz = static_cast<int>(std::floor(box_max.z - 0.002f));

    if (axis == 1) { // 1. Y Axis (Gravity, jumping, floors, ceilings)
        bool handled_floor = false;
        int check_y_start = static_cast<int>(std::floor(box_min.y + 0.15f));
        int check_y_end = static_cast<int>(std::floor(box_min.y - 0.15f));

        for (int check_y = check_y_start; check_y >= check_y_end; --check_y) {
            for (int x = min_bx; x <= max_bx; ++x) {
                for (int z = min_bz; z <= max_bz; ++z) {
                    Voxel vox = world.get_voxel(x, check_y, z);
                    if (!vox.is_solid()) continue;

                    if (vox.is_ramp()) {
                        float surf_y = calculate_ramp_surface_y(vox.shape(), x, check_y, z, m_position.x, m_position.z);
                        if (box_min.y <= surf_y + 0.12f && box_min.y >= surf_y - 0.45f) {
                            m_position.y = surf_y + half_extents.y;
                            m_on_ground = true;
                            m_isGrounded = true;
                            glm::vec3 n = get_ramp_normal(vox.shape());
                            glm::vec3 v_slope = m_velocity - glm::dot(m_velocity, n) * n;
                            if (v_slope.y >= 0.0f) {
                                m_velocity.y = v_slope.y;
                            } else {
                                m_velocity.y = 0.0f;
                            }
                            handled_floor = true;
                            break;
                        }
                    } else if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                        float surf_y = static_cast<float>(check_y) + 0.5f;
                        if (box_min.y <= surf_y + 0.12f && box_min.y >= surf_y - 0.45f) {
                            m_position.y = surf_y + half_extents.y;
                            if (m_velocity.y < 0.0f) m_velocity.y = 0.0f;
                            m_on_ground = true;
                            m_isGrounded = true;
                            handled_floor = true;
                            break;
                        }
                    } else if (vox.shape() == SHAPE_SLAB_TOP) {
                        float surf_y = static_cast<float>(check_y + 1);
                        if (box_min.y <= surf_y + 0.12f && box_min.y >= surf_y - 0.45f) {
                            m_position.y = surf_y + half_extents.y;
                            if (m_velocity.y < 0.0f) m_velocity.y = 0.0f;
                            m_on_ground = true;
                            m_isGrounded = true;
                            handled_floor = true;
                            break;
                        }
                    } else if (vox.shape() == SHAPE_CUBE && m_velocity.y <= 0.05f) {
                        float surf_y = static_cast<float>(check_y + 1);
                        if (box_min.y <= surf_y + 0.12f && box_min.y >= surf_y - 0.45f) {
                            float impact_speed = -m_velocity.y;
                            if (impact_speed > 0.0f) apply_fall_impact(impact_speed);
                            m_position.y = surf_y + half_extents.y;
                            m_velocity.y = 0.0f;
                            m_on_ground = true;
                            m_isGrounded = true;
                            handled_floor = true;
                            break;
                        }
                    }
                }
                if (handled_floor) break;
            }
            if (handled_floor) break;
        }

        if (!handled_floor && m_velocity.y > 0.0f) {
            int check_ceil_y = static_cast<int>(std::floor(box_max.y));
            for (int x = min_bx; x <= max_bx; ++x) {
                for (int z = min_bz; z <= max_bz; ++z) {
                    if (world.is_solid(glm::ivec3(x, check_ceil_y, z))) {
                        Voxel vox = world.get_voxel(x, check_ceil_y, z);
                        if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                            if (box_max.y >= static_cast<float>(check_ceil_y)) {
                                float target_ceiling = static_cast<float>(check_ceil_y);
                                m_position.y = target_ceiling - half_extents.y - 0.001f;
                                m_velocity.y = 0.0f;
                                return;
                            }
                            continue;
                        }
                        if (vox.shape() == SHAPE_SLAB_TOP) {
                            if (box_max.y >= static_cast<float>(check_ceil_y) + 0.5f) {
                                float target_ceiling = static_cast<float>(check_ceil_y) + 0.5f;
                                m_position.y = target_ceiling - half_extents.y - 0.001f;
                                m_velocity.y = 0.0f;
                                return;
                            }
                            continue;
                        }
                        float target_ceiling = static_cast<float>(check_ceil_y);
                        m_position.y = target_ceiling - half_extents.y - 0.001f;
                        m_velocity.y = 0.0f;
                        return;
                    }
                }
            }
        }
    } else if (axis == 0) { // 2. X Axis (Walls)
        auto try_mantle_x = [&](int check_x) -> bool {
            if (!(m_current_buttons & BTN_FORWARD) || m_vault_timer > 0.0f) return false;
            bool has_edge = false;
            for (int z = min_bz; z <= max_bz; ++z) {
                if (world.is_solid(glm::ivec3(check_x, max_by, z)) ||
                    world.is_solid(glm::ivec3(check_x, min_wall_y + 1, z))) {
                    has_edge = true;
                    break;
                }
            }
            if (!has_edge) return false;
            for (int z = min_bz; z <= max_bz; ++z) {
                if (world.is_solid(glm::ivec3(check_x, max_by + 1, z)) ||
                    world.is_solid(glm::ivec3(check_x, max_by + 2, z))) {
                    return false;
                }
            }
            // Pull player vertically over the ledge (+1.8m) with smooth vault motion
            m_position.y += 1.8f;
            m_velocity.y = std::max(2.5f, m_velocity.y);
            m_vault_timer = 0.35f;
            m_on_ground = true;
            m_isGrounded = true;
            return true;
        };

        if (m_velocity.x > 0.0f) {
            int check_x = static_cast<int>(std::floor(box_max.x));
            bool has_collision = false;
            int highest_solid_y = -1;
            float step_surf_y = 0.0f;
            for (int y = min_wall_y; y <= max_by; ++y) {
                for (int z = min_bz; z <= max_bz; ++z) {
                    Voxel vox = world.get_voxel(check_x, y, z);
                    if (vox.is_solid()) {
                        if (vox.is_ramp()) {
                            float surf_y = calculate_ramp_surface_y(vox.shape(), check_x, y, z, m_position.x, m_position.z);
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y <= surf_y + 0.08f) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    glm::vec3 n = get_ramp_normal(vox.shape());
                                    glm::vec3 v_slope = m_velocity - glm::dot(m_velocity, n) * n;
                                    if (v_slope.y >= 0.0f) {
                                        m_velocity.y = v_slope.y;
                                    }
                                }
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                            float surf_y = static_cast<float>(y) + 0.5f;
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y < surf_y) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                }
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_SLAB_TOP) {
                            float surf_y = static_cast<float>(y + 1);
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y < surf_y) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                }
                                continue;
                            }
                            if (box_max.y < static_cast<float>(y) + 0.5f) {
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_CUBE) {
                            Voxel v_above = world.get_voxel(check_x, y + 1, z);
                            if (v_above.is_ramp()) {
                                float ramp_s = calculate_ramp_surface_y(v_above.shape(), check_x, y + 1, z, m_position.x, m_position.z);
                                if (box_min.y >= static_cast<float>(y + 1) - 0.60f) {
                                    m_position.y = std::max(m_position.y, ramp_s + half_extents.y);
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    continue;
                                }
                            } else if (!v_above.is_solid()) {
                                float top_s = static_cast<float>(y + 1);
                                if (box_min.y >= top_s - 0.60f) {
                                    m_position.y = top_s + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                    continue;
                                }
                            }
                        }
                        has_collision = true;
                        if (y > highest_solid_y) {
                            highest_solid_y = y;
                            if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                                step_surf_y = static_cast<float>(y) + 0.5f;
                            } else {
                                step_surf_y = static_cast<float>(y + 1);
                            }
                        }
                    }
                }
            }
            if (has_collision) {
                float step_height = step_surf_y - box_min.y;
                bool can_step = (m_on_ground || m_isGrounded) && (highest_solid_y != -1) &&
                                (step_height > 0.0f && step_height <= 1.35f);
                if (can_step) {
                    for (int z = min_bz; z <= max_bz; ++z) {
                        if (world.is_solid(glm::ivec3(check_x, highest_solid_y + 1, z)) ||
                            world.is_solid(glm::ivec3(check_x, highest_solid_y + 2, z))) {
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
                    m_position.y = step_surf_y + half_extents.y;
                    m_on_ground = true;
                    m_isGrounded = true;
                    m_velocity.y = 0.0f;
                } else if (try_mantle_x(check_x)) {
                    // Mantled successfully over ledge
                } else {
                    m_position.x = static_cast<float>(check_x) - half_extents.x - 0.001f;
                    m_velocity.x = 0.0f;
                    return;
                }
            }
        } else if (m_velocity.x < 0.0f) {
            int check_x = static_cast<int>(std::floor(box_min.x));
            bool has_collision = false;
            int highest_solid_y = -1;
            float step_surf_y = 0.0f;
            for (int y = min_wall_y; y <= max_by; ++y) {
                for (int z = min_bz; z <= max_bz; ++z) {
                    Voxel vox = world.get_voxel(check_x, y, z);
                    if (vox.is_solid()) {
                        if (vox.is_ramp()) {
                            float surf_y = calculate_ramp_surface_y(vox.shape(), check_x, y, z, m_position.x, m_position.z);
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y <= surf_y + 0.08f) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    glm::vec3 n = get_ramp_normal(vox.shape());
                                    glm::vec3 v_slope = m_velocity - glm::dot(m_velocity, n) * n;
                                    if (v_slope.y >= 0.0f) {
                                        m_velocity.y = v_slope.y;
                                    }
                                }
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                            float surf_y = static_cast<float>(y) + 0.5f;
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y < surf_y) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                }
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_SLAB_TOP) {
                            float surf_y = static_cast<float>(y + 1);
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y < surf_y) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                }
                                continue;
                            }
                            if (box_max.y < static_cast<float>(y) + 0.5f) {
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_CUBE) {
                            Voxel v_above = world.get_voxel(check_x, y + 1, z);
                            if (v_above.is_ramp()) {
                                float ramp_s = calculate_ramp_surface_y(v_above.shape(), check_x, y + 1, z, m_position.x, m_position.z);
                                if (box_min.y >= static_cast<float>(y + 1) - 0.60f) {
                                    m_position.y = std::max(m_position.y, ramp_s + half_extents.y);
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    continue;
                                }
                            } else if (!v_above.is_solid()) {
                                float top_s = static_cast<float>(y + 1);
                                if (box_min.y >= top_s - 0.60f) {
                                    m_position.y = top_s + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                    continue;
                                }
                            }
                        }
                        has_collision = true;
                        if (y > highest_solid_y) {
                            highest_solid_y = y;
                            if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                                step_surf_y = static_cast<float>(y) + 0.5f;
                            } else {
                                step_surf_y = static_cast<float>(y + 1);
                            }
                        }
                    }
                }
            }
            if (has_collision) {
                float step_height = step_surf_y - box_min.y;
                bool can_step = (m_on_ground || m_isGrounded) && (highest_solid_y != -1) &&
                                (step_height > 0.0f && step_height <= 1.35f);
                if (can_step) {
                    for (int z = min_bz; z <= max_bz; ++z) {
                        if (world.is_solid(glm::ivec3(check_x, highest_solid_y + 1, z)) ||
                            world.is_solid(glm::ivec3(check_x, highest_solid_y + 2, z))) {
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
                    m_position.y = step_surf_y + half_extents.y;
                    m_on_ground = true;
                    m_isGrounded = true;
                    m_velocity.y = 0.0f;
                } else if (try_mantle_x(check_x)) {
                    // Mantled successfully over ledge
                } else {
                    m_position.x = static_cast<float>(check_x + 1) + half_extents.x + 0.001f;
                    m_velocity.x = 0.0f;
                    return;
                }
            }
        }
    } else if (axis == 2) { // 3. Z Axis (Walls)
        auto try_mantle_z = [&](int check_z) -> bool {
            if (!(m_current_buttons & BTN_FORWARD) || m_vault_timer > 0.0f) return false;
            bool has_edge = false;
            for (int x = min_bx; x <= max_bx; ++x) {
                if (world.is_solid(glm::ivec3(x, max_by, check_z)) ||
                    world.is_solid(glm::ivec3(x, min_wall_y + 1, check_z))) {
                    has_edge = true;
                    break;
                }
            }
            if (!has_edge) return false;
            for (int x = min_bx; x <= max_bx; ++x) {
                if (world.is_solid(glm::ivec3(x, max_by + 1, check_z)) ||
                    world.is_solid(glm::ivec3(x, max_by + 2, check_z))) {
                    return false;
                }
            }
            // Pull player vertically over the ledge (+1.8m) with smooth vault motion
            m_position.y += 1.8f;
            m_velocity.y = std::max(2.5f, m_velocity.y);
            m_vault_timer = 0.35f;
            m_on_ground = true;
            m_isGrounded = true;
            return true;
        };

        if (m_velocity.z > 0.0f) {
            int check_z = static_cast<int>(std::floor(box_max.z));
            bool has_collision = false;
            int highest_solid_y = -1;
            float step_surf_y = 0.0f;
            for (int y = min_wall_y; y <= max_by; ++y) {
                for (int x = min_bx; x <= max_bx; ++x) {
                    Voxel vox = world.get_voxel(x, y, check_z);
                    if (vox.is_solid()) {
                        if (vox.is_ramp()) {
                            float surf_y = calculate_ramp_surface_y(vox.shape(), x, y, check_z, m_position.x, m_position.z);
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y <= surf_y + 0.08f) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    glm::vec3 n = get_ramp_normal(vox.shape());
                                    glm::vec3 v_slope = m_velocity - glm::dot(m_velocity, n) * n;
                                    if (v_slope.y >= 0.0f) {
                                        m_velocity.y = v_slope.y;
                                    }
                                }
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                            float surf_y = static_cast<float>(y) + 0.5f;
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y < surf_y) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                }
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_SLAB_TOP) {
                            float surf_y = static_cast<float>(y + 1);
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y < surf_y) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                }
                                continue;
                            }
                            if (box_max.y < static_cast<float>(y) + 0.5f) {
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_CUBE) {
                            Voxel v_above = world.get_voxel(x, y + 1, check_z);
                            if (v_above.is_ramp()) {
                                float ramp_s = calculate_ramp_surface_y(v_above.shape(), x, y + 1, check_z, m_position.x, m_position.z);
                                if (box_min.y >= static_cast<float>(y + 1) - 0.60f) {
                                    m_position.y = std::max(m_position.y, ramp_s + half_extents.y);
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    continue;
                                }
                            } else if (!v_above.is_solid()) {
                                float top_s = static_cast<float>(y + 1);
                                if (box_min.y >= top_s - 0.60f) {
                                    m_position.y = top_s + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                    continue;
                                }
                            }
                        }
                        has_collision = true;
                        if (y > highest_solid_y) {
                            highest_solid_y = y;
                            if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                                step_surf_y = static_cast<float>(y) + 0.5f;
                            } else {
                                step_surf_y = static_cast<float>(y + 1);
                            }
                        }
                    }
                }
            }
            if (has_collision) {
                float step_height = step_surf_y - box_min.y;
                bool can_step = (m_on_ground || m_isGrounded) && (highest_solid_y != -1) &&
                                (step_height > 0.0f && step_height <= 1.35f);
                if (can_step) {
                    for (int x = min_bx; x <= max_bx; ++x) {
                        if (world.is_solid(glm::ivec3(x, highest_solid_y + 1, check_z)) ||
                            world.is_solid(glm::ivec3(x, highest_solid_y + 2, check_z))) {
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
                    m_position.y = step_surf_y + half_extents.y;
                    m_on_ground = true;
                    m_isGrounded = true;
                    m_velocity.y = 0.0f;
                } else if (try_mantle_z(check_z)) {
                    // Mantled successfully over ledge
                } else {
                    m_position.z = static_cast<float>(check_z) - half_extents.z - 0.001f;
                    m_velocity.z = 0.0f;
                    return;
                }
            }
        } else if (m_velocity.z < 0.0f) {
            int check_z = static_cast<int>(std::floor(box_min.z));
            bool has_collision = false;
            int highest_solid_y = -1;
            float step_surf_y = 0.0f;
            for (int y = min_wall_y; y <= max_by; ++y) {
                for (int x = min_bx; x <= max_bx; ++x) {
                    Voxel vox = world.get_voxel(x, y, check_z);
                    if (vox.is_solid()) {
                        if (vox.is_ramp()) {
                            float surf_y = calculate_ramp_surface_y(vox.shape(), x, y, check_z, m_position.x, m_position.z);
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y <= surf_y + 0.08f) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    glm::vec3 n = get_ramp_normal(vox.shape());
                                    glm::vec3 v_slope = m_velocity - glm::dot(m_velocity, n) * n;
                                    if (v_slope.y >= 0.0f) {
                                        m_velocity.y = v_slope.y;
                                    }
                                }
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                            float surf_y = static_cast<float>(y) + 0.5f;
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y < surf_y) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                }
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_SLAB_TOP) {
                            float surf_y = static_cast<float>(y + 1);
                            if (box_min.y >= surf_y - 0.60f) {
                                if (box_min.y < surf_y) {
                                    m_position.y = surf_y + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                }
                                continue;
                            }
                            if (box_max.y < static_cast<float>(y) + 0.5f) {
                                continue;
                            }
                        } else if (vox.shape() == SHAPE_CUBE) {
                            Voxel v_above = world.get_voxel(x, y + 1, check_z);
                            if (v_above.is_ramp()) {
                                float ramp_s = calculate_ramp_surface_y(v_above.shape(), x, y + 1, check_z, m_position.x, m_position.z);
                                if (box_min.y >= static_cast<float>(y + 1) - 0.60f) {
                                    m_position.y = std::max(m_position.y, ramp_s + half_extents.y);
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    continue;
                                }
                            } else if (!v_above.is_solid()) {
                                float top_s = static_cast<float>(y + 1);
                                if (box_min.y >= top_s - 0.60f) {
                                    m_position.y = top_s + half_extents.y;
                                    m_on_ground = true;
                                    m_isGrounded = true;
                                    m_velocity.y = 0.0f;
                                    continue;
                                }
                            }
                        }
                        has_collision = true;
                        if (y > highest_solid_y) {
                            highest_solid_y = y;
                            if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                                step_surf_y = static_cast<float>(y) + 0.5f;
                            } else {
                                step_surf_y = static_cast<float>(y + 1);
                            }
                        }
                    }
                }
            }
            if (has_collision) {
                float step_height = step_surf_y - box_min.y;
                bool can_step = (m_on_ground || m_isGrounded) && (highest_solid_y != -1) &&
                                (step_height > 0.0f && step_height <= 1.35f);
                if (can_step) {
                    for (int x = min_bx; x <= max_bx; ++x) {
                        if (world.is_solid(glm::ivec3(x, highest_solid_y + 1, check_z)) ||
                            world.is_solid(glm::ivec3(x, highest_solid_y + 2, check_z))) {
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
                    m_position.y = step_surf_y + half_extents.y;
                    m_on_ground = true;
                    m_isGrounded = true;
                    m_velocity.y = 0.0f;
                } else if (try_mantle_z(check_z)) {
                    // Mantled successfully over ledge
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
    glm::vec3 half_extents = this->half_extents();

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

    // Ground support probe: ensure m_on_ground stays true while standing, walking, or ascending on floor/ramp
    float foot_y = m_position.y - half_extents.y;
    int ground_y_high = static_cast<int>(std::floor(foot_y + 0.15f));
    int ground_y_low  = static_cast<int>(std::floor(foot_y - 0.15f));
    int min_x = static_cast<int>(std::floor(m_position.x - 0.28f));
    int max_x = static_cast<int>(std::floor(m_position.x + 0.28f));
    int min_z = static_cast<int>(std::floor(m_position.z - 0.28f));
    int max_z = static_cast<int>(std::floor(m_position.z + 0.28f));

    for (int gy = ground_y_high; gy >= ground_y_low; --gy) {
        for (int x = min_x; x <= max_x; ++x) {
            for (int z = min_z; z <= max_z; ++z) {
                if (world.is_solid(glm::ivec3(x, gy, z))) {
                    Voxel vox = world.get_voxel(x, gy, z);
                    float surf_y = static_cast<float>(gy + 1);
                    if (vox.is_ramp()) {
                        surf_y = calculate_ramp_surface_y(vox.shape(), x, gy, z, m_position.x, m_position.z);
                    } else if (vox.shape() == SHAPE_SLAB_BOTTOM) {
                        surf_y = static_cast<float>(gy) + 0.5f;
                    }

                    if (std::abs(foot_y - surf_y) <= 0.18f || (foot_y >= surf_y - 0.18f && foot_y <= surf_y + 0.05f)) {
                        m_on_ground = true;
                        m_isGrounded = true;
                        if (!vox.is_ramp() && m_velocity.y < 0.0f) {
                            apply_fall_impact(-m_velocity.y);
                            m_velocity.y = 0.0f;
                            m_position.y = surf_y + half_extents.y;
                        }
                        break;
                    }
                }
            }
            if (m_on_ground) break;
        }
        if (m_on_ground) break;
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
        glm::vec3 eye = eye_position();
        RaycastHit hit = world.raycast(eye, m_front, 6.5f);

        if (hit.hit && hit.voxel.material_id != MAT_DREDGE_BEDROCK &&
            hit.voxel.material_id != MAT_REINFORCED_VAULT_DOOR &&
            hit.voxel.material_id != MAT_VAULT_DOOR &&
            hit.voxel.material_id != MAT_PRECURSOR_STONE) {
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
            float effective_speed = effective_mine_speed();
            m_drillDamageAccumulator += effective_speed * dt;
            m_target_block_damage = m_drillDamageAccumulator;
            m_mine_timer = m_drillDamageAccumulator;
            m_target_time_to_break = m_block_hardness;

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
                m_drillDamageAccumulator = 0.0f;
                m_target_block_damage = 0.0f;
                m_mine_timer = 0.0f;
                m_target_block = glm::ivec3(-1);
            }
        } else {
            // Player looked away or hit bedrock: decay back to 0 over 0.8s
            if (m_drillDamageAccumulator > 0.0f) {
                float decay_rate = (m_block_hardness > 0.0f ? m_block_hardness : 1.0f) / 0.8f;
                m_drillDamageAccumulator = std::max(0.0f, m_drillDamageAccumulator - decay_rate * dt);
                m_target_block_damage = m_drillDamageAccumulator;
                m_mine_timer = m_drillDamageAccumulator;
                if (m_drillDamageAccumulator <= 0.0f) {
                    m_target_block = glm::ivec3(-1);
                }
            }
        }
    } else {
        // Player released LMB: decay target block damage back to 0 over 0.8s
        if (m_drillDamageAccumulator > 0.0f) {
            float decay_rate = (m_block_hardness > 0.0f ? m_block_hardness : 1.0f) / 0.8f;
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

float PlayerController::take_damage(float dmg, DamageSource source) {
    if (m_insertionShieldTimer > 0.0f) {
        return 0.0f; // Suit takes 0 damage while Drop Pod Recall Matrix is active
    }

    if (source == DamageSource::FallingDebris) {
        float reduction = debris_damage_reduction();
        dmg *= (1.0f - reduction);
    }
    if (dmg <= 0.0f) return 0.0f;
    m_health = std::max(0.0f, m_health - dmg);
    m_exo.integrity = (m_max_health > 0.0f) ? (m_health / m_max_health * 100.0f) : 0.0f;
    m_last_damage_source = source;
    m_time_since_damage = 0.0f;

    // Apply trauma screen shake based on specific damage scenario:
    // - Radiation causes invisible cellular breakdown -> ZERO screen shaking! (Telegraphed by Geiger counter & groaning)
    // - Toxic gas causes respiratory distress -> subtle cough spasm (0.02f), NOT violent screen shaking!
    // - Thermal lava causes burn distress -> mild heat tremor
    // - Enemy attack -> visceral physical trauma
    // - Kinetic / Falling Debris / Fall Impact / Spikes -> punchy camera trauma
    if (source == DamageSource::Radiation) {
        // Zero screen shaking for radiation
    } else if (source == DamageSource::ToxicGas) {
        add_trauma(0.02f);
    } else if (source == DamageSource::ThermalLava) {
        float trauma_to_add = std::clamp(0.10f + dmg * 0.015f, 0.10f, 0.35f);
        add_trauma(trauma_to_add);
    } else if (source == DamageSource::EnemyAttack) {
        float trauma_to_add = std::clamp(0.30f + dmg * 0.025f, 0.30f, 0.95f);
        add_trauma(trauma_to_add);
    } else {
        float trauma_to_add = std::clamp(0.25f + dmg * 0.025f, 0.25f, 0.95f);
        add_trauma(trauma_to_add);
    }

    if (m_on_damage_source) {
        m_on_damage_source(dmg, source);
    }
    if (m_on_damage) {
        bool is_impact = (source == DamageSource::FallingDebris || source == DamageSource::FallImpact);
        m_on_damage(dmg, is_impact);
    }
    return dmg;
}

float PlayerController::take_damage(float dmg, bool is_falling_debris) {
    return take_damage(dmg, is_falling_debris ? DamageSource::FallingDebris : DamageSource::Kinetic);
}

void PlayerController::reload_weapon() {
    if (m_reload_timer > 0.0f || m_weapon_ammo >= m_weapon_stats.max_ammo) return;
    m_reload_timer = m_weapon_stats.reload_time;
    if (m_on_weapon_reload) {
        m_on_weapon_reload(m_char_attr.classType, m_weapon_stats.reload_time);
    }
}

bool PlayerController::try_fire_weapon(std::vector<PlayerPlasmaBolt>& out_bolts, float dt) {
    if (m_active_tool != ToolSlot::CombatWeapon) return false;
    if (is_shooting_paused()) return false;
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

    // Unified Camera Center Raycast & Viewmodel Convergence Matrix:
    glm::vec3 eye = eye_position();
    float right_offset = glm::mix(0.10f, 0.02f, m_zoom_progress);
    glm::vec3 base_origin = eye + m_front * 0.35f + m_right * right_offset - m_up * 0.06f;

    // 1. Raycast forward from eye along camera front to find 3D world impact point
    glm::vec3 p_target = eye + m_front * 50.0f;
    if (m_current_world) {
        RaycastHit hit = m_current_world->raycast(eye, m_front, 50.0f);
        if (hit.hit) {
            p_target = eye + m_front * hit.distance;
        }
    }

    // 2. Aim projectile trajectory from muzzle to P_target
    glm::vec3 shot_dir = glm::normalize(p_target - base_origin);

    float effective_spread = m_weapon_stats.spread;
    if (is_aiming()) {
        effective_spread *= 0.5f; // 50% tighter spread cone when aiming down sights
    }

    for (int p = 0; p < m_weapon_stats.pellets; ++p) {
        glm::vec3 fire_dir = shot_dir;
        if (effective_spread > 0.0f) {
            float rx = ((rand() % 1000) / 500.0f - 1.0f) * effective_spread;
            float ry = ((rand() % 1000) / 500.0f - 1.0f) * effective_spread;
            fire_dir = glm::normalize(shot_dir + m_right * rx + m_up * ry);
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

void PlayerController::cycle_tool_forward() {
    if (m_active_tool == ToolSlot::MiningDrill) {
        set_active_tool(ToolSlot::CombatWeapon);
    } else if (m_active_tool == ToolSlot::CombatWeapon) {
        set_active_tool(ToolSlot::DemolitionCharge);
    } else {
        set_active_tool(ToolSlot::MiningDrill);
    }
}

void PlayerController::cycle_tool_backward() {
    if (m_active_tool == ToolSlot::MiningDrill) {
        set_active_tool(ToolSlot::DemolitionCharge);
    } else if (m_active_tool == ToolSlot::DemolitionCharge) {
        set_active_tool(ToolSlot::CombatWeapon);
    } else {
        set_active_tool(ToolSlot::MiningDrill);
    }
}

float PlayerController::current_fov(float base_fov) const {
    if (m_zoom_progress <= 0.0f || !is_weapon_equipped()) {
        return base_fov;
    }
    float target_fov = base_fov * m_weapon_stats.zoom_fov_multiplier;
    return glm::mix(base_fov, target_fov, m_zoom_progress);
}

} // namespace Voidfall
