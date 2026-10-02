#pragma once
#include "../core/window.hpp"
#include "../voxel/world.hpp"
#include "../net/packet_types.hpp"
#include "loadout.hpp"
#include "character_class.hpp"
#include "upgrades.hpp"
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
    void UpdatePhysics(float dt, World& world) { update_physics(dt, world); }
    void UpdatePhysics(float dt);

    void ResolveAxisCollision(int axis, const glm::vec3& half_extents);
    void ResolveAxisCollision(int axis, const glm::vec3& half_extents, World& world);

    void place_bulkhead(World& world, const glm::ivec3& place_pos);
    void PlaceBulkhead(World& world, const glm::ivec3& place_pos);
    void clamp_to_surface(const World& world);

    bool is_grounded() const { return m_on_ground; }
    bool isGrounded() const { return m_on_ground; }

    glm::mat4 get_view_matrix() const;
    const glm::vec3& position() const { return m_position; }
    void set_position(const glm::vec3& pos) { m_position = pos; }

    const glm::vec3& forward() const { return m_front; }
    const glm::vec3& right() const { return m_right; }
    const glm::vec3& up() const { return m_up; }

    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    void set_look_angles(float yaw, float pitch) {
        m_yaw = yaw;
        m_pitch = glm::clamp(pitch, -89.0f, 89.0f);
        update_camera_vectors();
    }

    const GrappleHook& grapple() const { return m_grapple; }
    GrappleHook& grapple_mut() { return m_grapple; }
    const ExoStatus& exo() const { return m_exo; }
    ExoStatus& exo_mut() { return m_exo; }

    ToolSlot active_tool() const { return m_active_tool; }
    void set_active_tool(ToolSlot tool) { m_active_tool = tool; }

    void MineBlock(float dt);
    void MineBlock(float dt, World& world);

    float crack_stage() const { return m_block_hardness > 0.0f ? glm::clamp(m_drillDamageAccumulator / m_block_hardness, 0.0f, 1.0f) : 0.0f; }
    float mine_progress() const { return crack_stage(); }
    float drill_damage_accumulator() const { return m_drillDamageAccumulator; }
    float target_block_damage() const { return m_drillDamageAccumulator; }
    float block_hardness() const { return m_block_hardness; }
    const glm::ivec3& target_block() const { return m_target_block; }
    const glm::ivec3& target_normal() const { return m_target_normal; }
    bool is_drilling() const { return (m_current_buttons & BTN_MINE_DRILL) != 0 && (m_active_tool == ToolSlot::MiningDrill); }
    void set_drilling(bool drilling) { if (drilling) m_current_buttons |= BTN_MINE_DRILL; else m_current_buttons &= ~BTN_MINE_DRILL; }
    RaycastHit get_look_target(const World& world, float max_dist = 5.0f) const;

    void set_character_class(CharacterClass cls);
    CharacterClass character_class() const { return m_char_attr.classType; }
    const CharacterAttributes& character_attributes() const { return m_char_attr; }

    void set_upgrades(const UpgradeTree& tree);
    const UpgradeTree& upgrades() const { return m_upgrades; }
    void apply_attributes_and_upgrades(const CharacterAttributes& attr, const UpgradeTree& upg);
    void apply_attributes_and_upgrades(CharacterClass cls, const UpgradeTree& upg);

    float health() const { return m_health; }
    float max_health() const { return m_max_health; }
    void set_health(float h) { m_health = glm::clamp(h, 0.0f, m_max_health); }
    float take_damage(float dmg, bool is_falling_debris = false);

    void set_reel_speed_multiplier(float mul) { m_reel_speed_multiplier = mul; }
    void set_thruster_regen_multiplier(float mul) { m_thruster_regen_multiplier = mul; }
    void set_drill_speed_multiplier(float mul) { m_drill_speed_multiplier = mul; }
    void set_allow_micro_charges(bool allow) { m_allow_micro_charges = allow; }

    void add_trauma(float t) { m_trauma = glm::clamp(m_trauma + t, 0.0f, 1.0f); }
    void AddTrauma(float t) { add_trauma(t); }
    float trauma() const { return m_trauma; }
    void set_trauma(float t) { m_trauma = glm::clamp(t, 0.0f, 1.0f); }

    void set_mouse_sensitivity(float sens) { m_mouse_sensitivity = sens; }
    float mouse_sensitivity() const { return m_mouse_sensitivity; }

    bool has_placed_charge() const { return m_has_placed_charge; }
    const glm::ivec3& placed_charge_pos() const { return m_placed_charge_pos; }
    const glm::ivec3& placed_charge_normal() const { return m_placed_charge_normal; }
    void clear_placed_charge() { m_has_placed_charge = false; }

    PlayerInputPacket build_input_packet(uint32_t tick, float dt) const;

    // Callbacks for gameplay actions
    using BlockBreakCallback = std::function<void(int x, int y, int z, const glm::ivec3& normal, uint8_t mat, uint8_t flags)>;
    using BlockPlaceCallback = std::function<void(int x, int y, int z, uint8_t mat)>;
    using BulkheadDismantleCallback = std::function<void(int x, int y, int z)>;
    using SonarCastCallback = std::function<void(const glm::vec3& origin)>;
    using ExplosiveBlastCallback = std::function<void(const glm::ivec3& origin, const glm::ivec3& dir, bool is_micro)>;
    using CanPlacePredicate = std::function<bool()>;
    using WarningCallback = std::function<void(const std::string&)>;

    void set_on_block_break(BlockBreakCallback cb) { m_on_block_break = std::move(cb); }
    void set_on_block_place(BlockPlaceCallback cb) { m_on_block_place = std::move(cb); }
    void set_on_bulkhead_dismantle(BulkheadDismantleCallback cb) { m_on_bulkhead_dismantle = std::move(cb); }
    void set_on_sonar_cast(SonarCastCallback cb) { m_on_sonar_cast = std::move(cb); }
    void set_on_explosive_blast(ExplosiveBlastCallback cb) { m_on_explosive_blast = std::move(cb); }
    void set_can_place_predicate(CanPlacePredicate pred) { m_can_place_predicate = std::move(pred); }
    void set_on_warning(WarningCallback cb) { m_on_warning = std::move(cb); }

private:
    void update_camera_vectors();
    void resolve_voxel_collisions(World& world, glm::vec3& pos, glm::vec3& vel, float dt);
    void resolve_axis_collision(int axis, const glm::vec3& half_extents, World& world);

    glm::vec3 m_position;
    glm::vec3 m_velocity{0.0f};

    float m_yaw{-90.0f};
    float m_pitch{0.0f};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};

    bool m_on_ground{false};
    bool m_isGrounded{false};
    World* m_current_world{nullptr};
    uint16_t m_current_buttons{0};

    // Character Archetype & Meta-Upgrades
    CharacterAttributes m_char_attr{get_character_attributes(CharacterClass::Demolitionist)};
    UpgradeTree m_upgrades;
    float m_health{100.0f};
    float m_max_health{100.0f};

    // Suit status & equipment
    ExoStatus m_exo;
    GrappleHook m_grapple;
    ToolSlot m_active_tool{ToolSlot::MiningDrill};

    // Progression skill modifiers
    float m_reel_speed_multiplier{1.0f};
    float m_thruster_regen_multiplier{1.0f};
    float m_drill_speed_multiplier{1.0f};
    bool m_allow_micro_charges{false};

    // Shaped charge deployment state
    bool m_has_placed_charge{false};
    glm::ivec3 m_placed_charge_pos{0};
    glm::ivec3 m_placed_charge_normal{0, 1, 0};

    // Mining / drilling state
    float m_drillDamageAccumulator{0.0f};
    float m_target_block_damage{0.0f};
    float m_block_hardness{0.6f};
    float m_mine_timer{0.0f};
    float m_target_time_to_break{0.6f};
    glm::ivec3 m_target_block{-1};
    glm::ivec3 m_target_normal{0, 1, 0};

    // Placement cooldown debounce (0.2s)
    float m_place_cooldown{0.0f};

    // Camera trauma / screen shake (clamped 0..1)
    float m_trauma{0.0f};

    // Look sensitivity
    float m_mouse_sensitivity{0.12f};

    // Action callbacks
    BlockBreakCallback m_on_block_break;
    BlockPlaceCallback m_on_block_place;
    BulkheadDismantleCallback m_on_bulkhead_dismantle;
    SonarCastCallback m_on_sonar_cast;
    ExplosiveBlastCallback m_on_explosive_blast;
    CanPlacePredicate m_can_place_predicate;
    WarningCallback m_on_warning;
};

} // namespace Voidfall
