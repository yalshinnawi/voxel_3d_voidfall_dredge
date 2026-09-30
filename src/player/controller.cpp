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
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

void PlayerController::handle_input(const Window& window, float dt) {
    m_current_buttons = 0;

    // 1. Mouse Look
    glm::dvec2 mouse_delta = const_cast<Window&>(window).get_cursor_delta();
    float sensitivity = 0.12f;
    m_yaw   += static_cast<float>(mouse_delta.x) * sensitivity;
    m_pitch += static_cast<float>(mouse_delta.y) * sensitivity;
    m_pitch  = glm::clamp(m_pitch, -89.0f, 89.0f);
    update_camera_vectors();

    // 2. Keyboard Movement
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

    float move_speed = (m_current_buttons & BTN_SPRINT) ? 12.0f : 7.0f;
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
            m_exo.heat  += 20.0f * dt;
            m_current_buttons |= BTN_THRUSTER;
        }
    }

    // Passive heat dissipation & power regeneration
    if (!(m_current_buttons & BTN_THRUSTER)) {
        m_exo.heat = std::max(0.0f, m_exo.heat - 18.0f * dt);
        m_exo.power = std::min(m_exo.max_power, m_exo.power + 15.0f * dt);
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

    // 5. Reel Grapple (E key)
    if (window.is_key_down(GLFW_KEY_E)) {
        m_current_buttons |= BTN_GRAPPLE_REEL;
        if (m_grapple.active) {
            m_grapple.rest_length = std::max(2.0f, m_grapple.rest_length - m_grapple.reel_speed * dt);
        }
    }

    // 6. Mining & Building buttons
    if (window.is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT)) {
        m_current_buttons |= BTN_MINE_DRILL;
    }
    if (window.is_mouse_button_down(GLFW_MOUSE_BUTTON_RIGHT)) {
        m_current_buttons |= BTN_PLACE_BLOCK;
    }

    // 7. Surveying Sonar Pulse (Q key)
    if (window.is_key_down(GLFW_KEY_Q)) {
        m_current_buttons |= BTN_SKILL_SONAR;
        if (m_on_sonar_cast) {
            m_on_sonar_cast(m_position);
        }
    }
}

void PlayerController::update_physics(float dt, World& world) {
    // 1. Grapple Fire & Tension Cable Dynamics
    if (m_current_buttons & BTN_GRAPPLE_FIRE) {
        if (!m_grapple.active) {
            RaycastHit hit = world.raycast(m_position, m_front, m_grapple.max_length);
            if (hit.hit) {
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
        float current_len = glm::length(to_anchor);
        if (current_len > 0.1f) {
            glm::vec3 cable_dir = to_anchor / current_len;
            if (current_len > m_grapple.rest_length) {
                float extension = current_len - m_grapple.rest_length;
                float tension = extension * m_grapple.stiffness;
                m_velocity += cable_dir * (tension * dt);
            }
        }
    }

    // 2. Gravity
    if (!m_on_ground) {
        m_velocity.y -= 19.6f * dt;
    }

    // 3. Subterranean Voxel Collision Resolution (AABB vs Voxel Grid)
    resolve_voxel_collisions(world, m_position, m_velocity, dt);

    // 4. Mining / Drilling Handling
    if (m_current_buttons & BTN_MINE_DRILL) {
        RaycastHit hit = world.raycast(m_position, m_front, 6.5f);
        if (hit.hit && hit.voxel.material_id != MAT_DREDGE_BEDROCK) {
            if (hit.block_pos != m_target_block) {
                m_target_block = hit.block_pos;
                m_mine_timer = 0.0f;
            }

            m_mine_timer += dt;
            float time_to_break = (hit.voxel.material_id == MAT_VOIDITE_CRYSTAL) ? 0.35f :
                                  (hit.voxel.material_id == MAT_INDUSTRIAL_BULKHEAD) ? 1.2f : 0.6f;

            if (m_mine_timer >= time_to_break) {
                world.set_voxel(hit.block_pos.x, hit.block_pos.y, hit.block_pos.z, Voxel{MAT_AIR, 0}, true);
                if (m_on_block_break) {
                    m_on_block_break(hit.block_pos.x, hit.block_pos.y, hit.block_pos.z);
                }
                m_mine_timer = 0.0f;
                m_target_block = glm::ivec3(-1);
            }
        }
    } else {
        m_mine_timer = 0.0f;
    }

    // 5. Block Placement
    if (m_current_buttons & BTN_PLACE_BLOCK) {
        RaycastHit hit = world.raycast(m_position, m_front, 6.0f);
        if (hit.hit) {
            glm::ivec3 place_pos = hit.block_pos + hit.normal;
            // Prevent placing inside player bounding box
            glm::vec3 player_min = m_position - glm::vec3(0.35f, 1.6f, 0.35f);
            glm::vec3 player_max = m_position + glm::vec3(0.35f, 0.2f, 0.35f);
            glm::vec3 block_min(place_pos);
            glm::vec3 block_max = block_min + glm::vec3(1.0f);

            bool overlaps = (player_min.x < block_max.x && player_max.x > block_min.x &&
                             player_min.y < block_max.y && player_max.y > block_min.y &&
                             player_min.z < block_max.z && player_max.z > block_min.z);

            if (!overlaps) {
                world.set_voxel(place_pos.x, place_pos.y, place_pos.z, Voxel{MAT_INDUSTRIAL_BULKHEAD, 0}, true);
                if (m_on_block_place) {
                    m_on_block_place(place_pos.x, place_pos.y, place_pos.z, MAT_INDUSTRIAL_BULKHEAD);
                }
            }
        }
    }
}

void PlayerController::resolve_voxel_collisions(World& world, glm::vec3& pos, glm::vec3& vel, float dt) {
    const float radius = 0.35f;
    const float feet_offset = 1.6f;
    const float head_offset = 0.2f;
    m_on_ground = false;

    // 1. Move & Resolve X
    pos.x += vel.x * dt;
    {
        int min_x = static_cast<int>(std::floor(pos.x - radius));
        int max_x = static_cast<int>(std::floor(pos.x + radius));
        int min_y = static_cast<int>(std::floor(pos.y - feet_offset + 0.05f));
        int max_y = static_cast<int>(std::floor(pos.y + head_offset - 0.05f));
        int min_z = static_cast<int>(std::floor(pos.z - radius));
        int max_z = static_cast<int>(std::floor(pos.z + radius));

        if (vel.x > 0.0f) {
            for (int y = min_y; y <= max_y; ++y) {
                for (int z = min_z; z <= max_z; ++z) {
                    if (world.get_voxel(max_x, y, z).is_solid()) {
                        pos.x = static_cast<float>(max_x) - radius - 0.001f;
                        vel.x = 0.0f;
                        goto resolved_x;
                    }
                }
            }
        } else if (vel.x < 0.0f) {
            for (int y = min_y; y <= max_y; ++y) {
                for (int z = min_z; z <= max_z; ++z) {
                    if (world.get_voxel(min_x, y, z).is_solid()) {
                        pos.x = static_cast<float>(min_x + 1) + radius + 0.001f;
                        vel.x = 0.0f;
                        goto resolved_x;
                    }
                }
            }
        }
    }
resolved_x:

    // 2. Move & Resolve Z
    pos.z += vel.z * dt;
    {
        int min_x = static_cast<int>(std::floor(pos.x - radius));
        int max_x = static_cast<int>(std::floor(pos.x + radius));
        int min_y = static_cast<int>(std::floor(pos.y - feet_offset + 0.05f));
        int max_y = static_cast<int>(std::floor(pos.y + head_offset - 0.05f));
        int min_z = static_cast<int>(std::floor(pos.z - radius));
        int max_z = static_cast<int>(std::floor(pos.z + radius));

        if (vel.z > 0.0f) {
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    if (world.get_voxel(x, y, max_z).is_solid()) {
                        pos.z = static_cast<float>(max_z) - radius - 0.001f;
                        vel.z = 0.0f;
                        goto resolved_z;
                    }
                }
            }
        } else if (vel.z < 0.0f) {
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    if (world.get_voxel(x, y, min_z).is_solid()) {
                        pos.z = static_cast<float>(min_z + 1) + radius + 0.001f;
                        vel.z = 0.0f;
                        goto resolved_z;
                    }
                }
            }
        }
    }
resolved_z:

    // 3. Move & Resolve Y (Ceiling and Floor)
    pos.y += vel.y * dt;
    {
        int min_x = static_cast<int>(std::floor(pos.x - radius + 0.05f));
        int max_x = static_cast<int>(std::floor(pos.x + radius - 0.05f));
        int min_y = static_cast<int>(std::floor(pos.y - feet_offset));
        int max_y = static_cast<int>(std::floor(pos.y + head_offset));
        int min_z = static_cast<int>(std::floor(pos.z - radius + 0.05f));
        int max_z = static_cast<int>(std::floor(pos.z + radius - 0.05f));

        if (vel.y > 0.0f) {
            // Moving UP (Jetpack / Jump): check top ceiling blocks
            for (int x = min_x; x <= max_x; ++x) {
                for (int z = min_z; z <= max_z; ++z) {
                    if (world.get_voxel(x, max_y, z).is_solid()) {
                        pos.y = static_cast<float>(max_y) - head_offset - 0.001f;
                        vel.y = 0.0f;
                        goto resolved_y;
                    }
                }
            }
        } else if (vel.y <= 0.0f) {
            // Moving DOWN (Gravity): check bottom feet floor blocks
            for (int x = min_x; x <= max_x; ++x) {
                for (int z = min_z; z <= max_z; ++z) {
                    if (world.get_voxel(x, min_y, z).is_solid()) {
                        pos.y = static_cast<float>(min_y + 1) + feet_offset;
                        vel.y = 0.0f;
                        m_on_ground = true;
                        goto resolved_y;
                    }
                }
            }
        }
    }
resolved_y:
    ;
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

} // namespace Voidfall
