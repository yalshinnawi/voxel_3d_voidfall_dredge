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

    // Tool hotbar selection: 1, 2, 3
    if (window.is_key_down(GLFW_KEY_1)) m_active_tool = ToolSlot::MiningDrill;
    if (window.is_key_down(GLFW_KEY_2)) m_active_tool = ToolSlot::IndustrialBulkhead;
    if (window.is_key_down(GLFW_KEY_3)) m_active_tool = ToolSlot::DemolitionCharge;

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

    float move_speed = ((m_current_buttons & BTN_SPRINT) ? 12.0f : 7.0f) * m_char_attr.moveSpeed;
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
        } else if (!m_exo.overheated && m_exo.power > 5.0f) {
            // Jetpack vertical hover thrusters
            m_velocity.y += 18.0f * dt;
            m_velocity.y = glm::clamp(m_velocity.y, -4.0f, 9.0f);
            m_exo.power -= 25.0f * dt;
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
        m_current_buttons |= BTN_SKILL_SONAR;
        if (m_on_sonar_cast) {
            m_on_sonar_cast(m_position);
        }
    }
    q_pressed_last = q_down;
}

void PlayerController::update_physics(float dt, World& world) {
    m_current_world = &world;

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

    // 4. Mining / Drilling Handling (Progressive cumulative damage with 1.0s decay)
    MineBlock(dt, world);

    // 5. Tool Actions & Cooldowns
    if (m_place_cooldown > 0.0f) {
        m_place_cooldown -= dt;
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
    float surface = world.get_highest_solid_surface(static_cast<int>(m_position.x), static_cast<int>(m_position.z));
    m_position.y = surface + 1.1f;
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

void PlayerController::resolve_axis_collision(int axis, const glm::vec3& half_extents, World& world) {
    glm::vec3 box_min = m_position - half_extents;
    glm::vec3 box_max = m_position + half_extents;

    int min_bx = static_cast<int>(std::floor(box_min.x + 0.001f));
    int max_bx = static_cast<int>(std::floor(box_max.x - 0.001f));
    int min_by = static_cast<int>(std::floor(box_min.y + 0.001f));
    int max_by = static_cast<int>(std::floor(box_max.y - 0.001f));
    int min_bz = static_cast<int>(std::floor(box_min.z + 0.001f));
    int max_bz = static_cast<int>(std::floor(box_max.z - 0.001f));

    if (axis == 1) { // 1. Y Axis (Gravity, jumping, floors, ceilings)
        if (m_velocity.y < 0.0f) {
            for (int y = max_by; y >= min_by; --y) {
                for (int x = min_bx; x <= max_bx; ++x) {
                    for (int z = min_bz; z <= max_bz; ++z) {
                        if (world.is_solid(glm::ivec3(x, y, z))) {
                            // On floor contact: land on top of block (y + 1.0f)
                            float target_floor = static_cast<float>(y + 1);
                            m_position.y = target_floor + half_extents.y;
                            m_velocity.y = 0.0f;
                            m_on_ground = true;
                            m_isGrounded = true;
                            return;
                        }
                    }
                }
            }
        } else if (m_velocity.y > 0.0f) {
            for (int y = min_by; y <= max_by; ++y) {
                for (int x = min_bx; x <= max_bx; ++x) {
                    for (int z = min_bz; z <= max_bz; ++z) {
                        if (world.is_solid(glm::ivec3(x, y, z))) {
                            // Hit ceiling
                            float target_ceiling = static_cast<float>(y);
                            m_position.y = target_ceiling - half_extents.y - 0.001f;
                            m_velocity.y = 0.0f;
                            return;
                        }
                    }
                }
            }
        }
    } else if (axis == 0) { // 2. X Axis (Walls)
        if (m_velocity.x > 0.0f) {
            for (int x = min_bx; x <= max_bx; ++x) {
                for (int y = min_by; y <= max_by; ++y) {
                    for (int z = min_bz; z <= max_bz; ++z) {
                        if (world.is_solid(glm::ivec3(x, y, z))) {
                            m_position.x = static_cast<float>(x) - half_extents.x - 0.001f;
                            m_velocity.x = 0.0f;
                            return;
                        }
                    }
                }
            }
        } else if (m_velocity.x < 0.0f) {
            for (int x = max_bx; x >= min_bx; --x) {
                for (int y = min_by; y <= max_by; ++y) {
                    for (int z = min_bz; z <= max_bz; ++z) {
                        if (world.is_solid(glm::ivec3(x, y, z))) {
                            m_position.x = static_cast<float>(x + 1) + half_extents.x + 0.001f;
                            m_velocity.x = 0.0f;
                            return;
                        }
                    }
                }
            }
        }
    } else if (axis == 2) { // 3. Z Axis (Walls)
        if (m_velocity.z > 0.0f) {
            for (int z = min_bz; z <= max_bz; ++z) {
                for (int y = min_by; y <= max_by; ++y) {
                    for (int x = min_bx; x <= max_bx; ++x) {
                        if (world.is_solid(glm::ivec3(x, y, z))) {
                            m_position.z = static_cast<float>(z) - half_extents.z - 0.001f;
                            m_velocity.z = 0.0f;
                            return;
                        }
                    }
                }
            }
        } else if (m_velocity.z < 0.0f) {
            for (int z = max_bz; z >= min_bz; --z) {
                for (int y = min_by; y <= max_by; ++y) {
                    for (int x = min_bx; x <= max_bx; ++x) {
                        if (world.is_solid(glm::ivec3(x, y, z))) {
                            m_position.z = static_cast<float>(z + 1) + half_extents.z + 0.001f;
                            m_velocity.z = 0.0f;
                            return;
                        }
                    }
                }
            }
        }
    }
}

void PlayerController::resolve_voxel_collisions(World& world, glm::vec3& pos, glm::vec3& vel, float dt) {
    m_on_ground = false;
    m_isGrounded = false;
    glm::vec3 half_extents(0.3f, 0.9f, 0.3f);

    // 1. Resolve Y Axis (Gravity, jumping, floors, ceilings)
    m_position.y += m_velocity.y * dt;
    resolve_axis_collision(1, half_extents, world);

    // 2. Resolve X Axis (Walls)
    m_position.x += m_velocity.x * dt;
    resolve_axis_collision(0, half_extents, world);

    // 3. Resolve Z Axis (Walls)
    m_position.z += m_velocity.z * dt;
    resolve_axis_collision(2, half_extents, world);

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
                    m_velocity.y = 0.0f;
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

} // namespace Voidfall
