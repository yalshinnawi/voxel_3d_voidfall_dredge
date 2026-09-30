#pragma once
#include "../core/window.hpp"
#include "../voxel/world.hpp"
#include "../net/packet_types.hpp"
#include <glm/glm.hpp>
#include <functional>

namespace Voidfall {

struct GrappleHook {
    bool active{false};
    glm::vec3 anchor_point{0.0f};
    float rest_length{0.0f};
    float max_length{45.0f};
    float stiffness{120.0f};
    float reel_speed{14.0f};
};

struct ExoStatus {
    float power{100.0f};      // 0..100%
    float max_power{100.0f};
    float heat{0.0f};         // 0..100%
    float max_heat{100.0f};
    bool overheated{false};
    float integrity{100.0f};  // 0..100%
    float radiation{0.0f};    // 0..100%
};

class PlayerController {
public:
    PlayerController(const glm::vec3& start_pos = glm::vec3(16.0f, 25.0f, 16.0f));

    void handle_input(const Window& window, float dt);
    void update_physics(float dt, World& world);

    glm::mat4 get_view_matrix() const;
    const glm::vec3& position() const { return m_position; }
    void set_position(const glm::vec3& pos) { m_position = pos; }

    const glm::vec3& forward() const { return m_front; }
    const glm::vec3& right() const { return m_right; }
    const glm::vec3& up() const { return m_up; }

    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }

    const GrappleHook& grapple() const { return m_grapple; }
    const ExoStatus& exo() const { return m_exo; }
    ExoStatus& exo_mut() { return m_exo; }

    PlayerInputPacket build_input_packet(uint32_t tick, float dt) const;

    // Callbacks for gameplay actions
    using BlockBreakCallback = std::function<void(int x, int y, int z)>;
    using BlockPlaceCallback = std::function<void(int x, int y, int z, uint8_t mat)>;
    using SonarCastCallback = std::function<void(const glm::vec3& origin)>;

    void set_on_block_break(BlockBreakCallback cb) { m_on_block_break = std::move(cb); }
    void set_on_block_place(BlockPlaceCallback cb) { m_on_block_place = std::move(cb); }
    void set_on_sonar_cast(SonarCastCallback cb) { m_on_sonar_cast = std::move(cb); }

private:
    void update_camera_vectors();
    void resolve_voxel_collisions(World& world, glm::vec3& pos, glm::vec3& vel, float dt);

    glm::vec3 m_position;
    glm::vec3 m_velocity{0.0f};

    float m_yaw{-90.0f};
    float m_pitch{0.0f};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};

    bool m_on_ground{false};
    uint16_t m_current_buttons{0};

    // Suit status & equipment
    ExoStatus m_exo;
    GrappleHook m_grapple;

    // Mining / drilling state
    float m_mine_timer{0.0f};
    glm::ivec3 m_target_block{-1};

    // Action callbacks
    BlockBreakCallback m_on_block_break;
    BlockPlaceCallback m_on_block_place;
    SonarCastCallback m_on_sonar_cast;
};

} // namespace Voidfall
