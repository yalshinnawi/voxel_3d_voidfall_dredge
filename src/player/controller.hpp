#pragma once
#include "../core/window.hpp"
#include "../voxel/world.hpp"
#include "../net/packet_types.hpp"
#include "loadout.hpp"
#include "character_class.hpp"
#include "upgrades.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <algorithm>
#include <vector>

namespace Voidfall {

struct GrappleHook {
    bool active{false};
    glm::vec3 anchor_point{0.0f};
    float rest_length{0.0f};
    float max_length{45.0f};
    float stiffness{120.0f};
    float reel_speed{14.0f};
    bool just_fired{false};
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
    void Update(float dt);
    void update(float dt) { Update(dt); }
    void Update(float dt, World& world);
    void update(float dt, World& world) { Update(dt, world); }

    void ResolveAxisCollision(int axis, const glm::vec3& half_extents);
    void ResolveAxisCollision(int axis, const glm::vec3& half_extents, World& world);

    void place_bulkhead(World& world, const glm::ivec3& place_pos);
    void PlaceBulkhead(World& world, const glm::ivec3& place_pos);
    void clamp_to_surface(const World& world);

    static constexpr float EYE_HEIGHT_STAND = 1.65f;
    static constexpr float EYE_HEIGHT_CROUCH = 0.95f;

    bool is_grounded() const { return m_on_ground; }
    bool isGrounded() const { return m_on_ground; }

    bool is_crouching() const { return m_is_crouching; }
    bool IsCrouched() const { return m_is_crouching; }
    void set_crouching(bool c) { ProcessStanceChange(c); }
    void SetCrouched(bool c) { ProcessStanceChange(c); }
    void ProcessStanceChange(bool crouched);

    float eye_height() const { return m_eyeHeight; }
    float GetCurrentEyeHeight() const { return m_eyeHeight; }
    glm::vec3 eye_position() const {
        float vault_boost = (m_vault_timer > 0.0f) ? (0.12f * std::sin((m_vault_timer / 0.35f) * 3.14159f)) : 0.0f;
        return m_position + glm::vec3(0.0f, m_eyeHeight - half_extents().y + vault_boost, 0.0f);
    }
    glm::vec3 half_extents() const { return m_is_crouching ? glm::vec3(0.3f, 0.55f, 0.3f) : glm::vec3(0.3f, 0.9f, 0.3f); }
    glm::vec3 GetHalfExtents() const { return half_extents(); }

    RaycastHit QueryRaycastTarget(const World& world, float max_dist = 50.0f) const;

    glm::mat4 get_view_matrix() const;
    const glm::vec3& position() const { return m_position; }
    void set_position(const glm::vec3& pos) { m_position = pos; }
    const glm::vec3& velocity() const { return m_velocity; }
    void set_velocity(const glm::vec3& vel) { m_velocity = vel; }
    glm::vec3& velocity_mut() { return m_velocity; }
    bool is_penetrating_solid(const World& world) const;

    struct CameraView {
        glm::vec3 Front;
        glm::vec3 Position;
        glm::vec3 Up;
    };
    CameraView camera() const { return CameraView{m_front, eye_position(), m_up}; }

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
    void set_direction(const glm::vec3& dir) {
        glm::vec3 n = glm::normalize(dir);
        float pitch_deg = glm::degrees(std::asin(std::clamp(n.y, -1.0f, 1.0f)));
        float yaw_deg = glm::degrees(std::atan2(n.z, n.x));
        set_look_angles(yaw_deg, pitch_deg);
    }

    const GrappleHook& grapple() const { return m_grapple; }
    GrappleHook& grapple_mut() { return m_grapple; }
    const ExoStatus& exo() const { return m_exo; }
    ExoStatus& exo_mut() { return m_exo; }

    ToolSlot active_tool() const { return m_active_tool; }
    void set_active_tool(ToolSlot tool) {
        m_active_tool = tool;
        if (m_active_tool != ToolSlot::CombatWeapon) {
            m_reload_timer = 0.0f;
            m_is_aiming = false;
            m_zoom_progress = 0.0f;
        }
    }
    void cycle_tool_forward();
    void cycle_tool_backward();

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

    // Weapon methods
    bool is_weapon_equipped() const { return m_active_tool == ToolSlot::CombatWeapon; }
    bool is_firing_weapon() const { return (m_current_buttons & BTN_MINE_DRILL) != 0 && (m_active_tool == ToolSlot::CombatWeapon); }
    bool is_aiming() const { return m_is_aiming && is_weapon_equipped(); }
    void set_aiming(bool aiming) { m_is_aiming = aiming; }
    float zoom_progress() const { return m_zoom_progress; }
    void set_zoom_progress(float progress) { m_zoom_progress = glm::clamp(progress, 0.0f, 1.0f); }
    float zoom_fov_multiplier() const { return m_weapon_stats.zoom_fov_multiplier; }
    float current_fov(float base_fov = 75.0f) const;
    int weapon_ammo() const { return m_weapon_ammo; }
    int weapon_max_ammo() const { return m_weapon_stats.max_ammo; }
    int carbine_ammo() const { return m_weapon_ammo; }
    int carbine_max_ammo() const { return m_weapon_stats.max_ammo; }
    WeaponArchetype weapon_archetype() const { return m_weapon_stats.archetype; }
    const WeaponStats& weapon_stats() const { return m_weapon_stats; }
    const std::string& weapon_name() const { return m_weapon_stats.name; }
    const std::string& weapon_short_name() const { return m_weapon_stats.short_name; }
    bool is_reloading() const { return m_reload_timer > 0.0f && m_weapon_ammo < m_weapon_stats.max_ammo && m_active_tool == ToolSlot::CombatWeapon; }
    float reload_timer() const { return std::max(0.0f, m_reload_timer); }
    float reload_progress() const { return (m_weapon_stats.reload_time > 0.0f) ? (1.0f - std::max(0.0f, m_reload_timer) / m_weapon_stats.reload_time) : 1.0f; }
    bool is_thruster_air_braking() const { return m_thruster_air_braking; }
    bool is_vaulting() const { return m_vault_timer > 0.0f; }
    void reload_weapon();
    bool try_fire_weapon(std::vector<PlayerPlasmaBolt>& out_bolts, float dt);
    bool try_fire_weapon(glm::vec3& out_origin, glm::vec3& out_dir, float dt);

    // Headlamp state
    bool is_headlamp_on() const { return m_headlamp_on; }
    void set_headlamp_on(bool on) { m_headlamp_on = on; }
    void toggle_headlamp() { m_headlamp_on = !m_headlamp_on; }

    // Defensive Quick Melee Shove mechanics (Key: V or Middle Click)
    bool is_melee_shoving() const { return m_melee_shove_timer > 0.0f; }
    float melee_shove_timer() const { return m_melee_shove_timer; }
    float melee_shove_cooldown() const { return m_melee_shove_cooldown; }
    float melee_shove_progress() const {
        return m_melee_shove_timer > 0.0f ? std::clamp(1.0f - (m_melee_shove_timer / 0.35f), 0.0f, 1.0f) : 0.0f;
    }
    void execute_melee_shove();

    using MeleeShoveCallback = std::function<void(const glm::vec3& cam_pos, const glm::vec3& cam_dir)>;
    void set_on_melee_shove(MeleeShoveCallback cb) { m_on_melee_shove = std::move(cb); }

    RaycastHit get_look_target(const World& world, float max_dist = 5.0f) const;

    void set_character_class(CharacterClass cls);
    CharacterClass character_class() const { return m_char_attr.classType; }
    const CharacterAttributes& character_attributes() const { return m_char_attr; }

    void set_upgrades(const UpgradeTree& tree);
    const UpgradeTree& upgrades() const { return m_upgrades; }
    void apply_attributes_and_upgrades(const CharacterAttributes& attr, const UpgradeTree& upg);
    void apply_attributes_and_upgrades(CharacterClass cls, const UpgradeTree& upg);

    // Skill tree progression effects & player stat getters
    float effective_mine_speed() const {
        return m_char_attr.baseMineSpeed * (1.0f + m_upgrades.drillSpeedTier * 0.12f) * std::max(0.2f, m_drill_speed_multiplier);
    }
    float drill_heat_buildup_rate() const {
        return 20.0f * std::max(0.25f, 1.0f - m_upgrades.drillDurabilityTier * 0.15f);
    }
    float drill_heat_dissipation_rate() const {
        return 18.0f * (1.0f + m_upgrades.drillDurabilityTier * 0.20f);
    }
    float dynamo_multiplier(bool is_sprint_or_falling = false) const {
        float mult = 1.0f + m_upgrades.kineticDynamoTier * 0.15f;
        if (is_sprint_or_falling) mult += m_upgrades.kineticDynamoTier * 0.15f;
        return mult;
    }
    float sonar_radius() const {
        return m_char_attr.sonarRadius + (m_upgrades.sonarFrequencyTier * 2.0f);
    }
    bool can_identify_materials() const {
        return m_upgrades.can_identify_materials();
    }
    float debris_damage_reduction() const {
        return std::clamp(m_char_attr.fallingDamageReduction + (m_upgrades.reinforcedPlatingTier * 0.10f), 0.0f, 0.85f);
    }

    enum class DamageSource {
        Kinetic,         // Standard kinetic attack (screen shake trauma, red flash)
        EnemyAttack,     // Hostile creature claw maul / bite / spine (screen blood splatters + visceral flesh hit sound)
        FallImpact,      // High-speed landing (bone crack noise, trauma, NO blood splatters)
        FallingDebris,   // Heavy rock fall from ceiling / seismic activity (debris armor impact sound, NO blood splatters)
        Radiation,       // Ionizing radiation (ZERO screen shake, geiger clicking, player groaning)
        ToxicGas,        // Chemical/spore gas (minimal cough shudder, asphyxiation choking, on-screen particles)
        ThermalLava,     // Molten thermite slag (thermal sizzle, heat rise, burn flash)
        Spikes,          // Punji spike puncture (sharp crunch, recoil hop)
        VoidSingularity  // Cosmic gravitational void (gravitational distortion)
    };

    float health() const { return m_health; }
    float max_health() const { return m_max_health; }
    void set_health(float h) { m_health = glm::clamp(h, 0.0f, m_max_health); }
    float take_damage(float dmg, DamageSource source);
    float take_damage(float dmg, bool is_falling_debris = false);
    DamageSource last_damage_source() const { return m_last_damage_source; }

    bool has_insertion_shield() const { return m_insertionShieldTimer > 0.0f; }
    float insertion_shield_timer() const { return m_insertionShieldTimer; }
    void reset_insertion_shield(float duration = 4.0f) { m_insertionShieldTimer = duration; }

    void set_combat_inputs_paused(bool paused) { m_combat_inputs_paused = paused; }
    bool are_combat_inputs_paused() const { return m_combat_inputs_paused; }

    void set_reel_speed_multiplier(float mul) { m_reel_speed_multiplier = mul; }
    void set_thruster_regen_multiplier(float mul) { m_thruster_regen_multiplier = mul; }
    void set_drill_speed_multiplier(float mul) { m_drill_speed_multiplier = mul; }
    void set_allow_micro_charges(bool allow) { m_allow_micro_charges = allow; }
    void set_carry_weight_multiplier(float mul) { m_carry_weight_multiplier = glm::clamp(mul, 0.4f, 1.0f); }
    float carry_weight_multiplier() const { return m_carry_weight_multiplier; }

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

    uint16_t current_buttons() const { return m_current_buttons; }

    // Callbacks for gameplay actions
    using BlockBreakCallback = std::function<void(int x, int y, int z, const glm::ivec3& normal, uint8_t mat, uint8_t flags)>;
    using BlockPlaceCallback = std::function<void(int x, int y, int z, uint8_t mat)>;
    using BulkheadDismantleCallback = std::function<void(int x, int y, int z)>;
    using SonarCastCallback = std::function<void(const glm::vec3& origin)>;
    using ExplosiveBlastCallback = std::function<void(const glm::ivec3& origin, const glm::ivec3& dir, bool is_micro)>;
    using CanPlacePredicate = std::function<bool()>;
    using WarningCallback = std::function<void(const std::string&)>;
    using JumpCallback = std::function<void(const glm::vec3& pos)>;
    using LandCallback = std::function<void(const glm::vec3& pos, float impact_speed)>;
    /// Fired by take_damage() for every HP reduction regardless of source
    /// (fall, stalker, burrower, radiation, gas, lava, debris…).
    /// dmg = final HP removed after reductions; is_impact = true when kinetic/fall.
    using DamageCallback = std::function<void(float dmg, bool is_impact)>;
    using DamageSourceCallback = std::function<void(float dmg, DamageSource source)>;

    using CanDeployChargePredicate = std::function<bool()>;
    using ChargePlacedCallback = std::function<void(const glm::ivec3& pos, const glm::ivec3& normal)>;

    void set_on_block_break(BlockBreakCallback cb) { m_on_block_break = std::move(cb); }
    void set_on_block_place(BlockPlaceCallback cb) { m_on_block_place = std::move(cb); }
    void set_on_bulkhead_dismantle(BulkheadDismantleCallback cb) { m_on_bulkhead_dismantle = std::move(cb); }
    void set_on_sonar_cast(SonarCastCallback cb) { m_on_sonar_cast = std::move(cb); }
    void set_on_explosive_blast(ExplosiveBlastCallback cb) { m_on_explosive_blast = std::move(cb); }
    void set_can_place_predicate(CanPlacePredicate pred) { m_can_place_predicate = std::move(pred); }
    void set_can_deploy_charge_predicate(CanDeployChargePredicate pred) { m_can_deploy_charge_predicate = std::move(pred); }
    void set_on_charge_placed(ChargePlacedCallback cb) { m_on_charge_placed = std::move(cb); }
    void set_on_warning(WarningCallback cb) { m_on_warning = std::move(cb); }
    void set_on_jump(JumpCallback cb) { m_on_jump = std::move(cb); }
    void set_on_land(LandCallback cb) { m_on_land = std::move(cb); }
    void set_on_damage(DamageCallback cb) { m_on_damage = std::move(cb); }
    void set_on_damage_source(DamageSourceCallback cb) { m_on_damage_source = std::move(cb); }

    // Weapon reload callback
    using WeaponReloadCallback = std::function<void(CharacterClass cls, float reload_time)>;
    void set_on_weapon_reload(WeaponReloadCallback cb) { m_on_weapon_reload = std::move(cb); }

    // Sonar cooldown & state
    float sonar_cooldown() const { return m_sonar_cooldown; }
    float sonar_max_cooldown() const { return m_sonar_max_cooldown; }
    bool is_sonar_ready() const { return m_sonar_cooldown <= 0.0f; }
    void set_sonar_max_cooldown(float cd) { m_sonar_max_cooldown = cd; }
    void trigger_sonar_cooldown() { m_sonar_cooldown = m_sonar_max_cooldown; }
    void reset_sonar_cooldown() { m_sonar_cooldown = 0.0f; }
    float sonar_recharge_progress() const { return m_sonar_max_cooldown > 0.0f ? std::clamp(1.0f - (m_sonar_cooldown / m_sonar_max_cooldown), 0.0f, 1.0f) : 1.0f; }

    // Tactical class ability cooldown & state
    using TacticalAbilityCallback = std::function<void(CharacterClass cls, const glm::vec3& pos, const glm::vec3& dir)>;
    void set_on_tactical_ability(TacticalAbilityCallback cb) { m_on_tactical_ability = std::move(cb); }

    float tactical_cooldown() const { return m_tactical_cooldown; }
    float tactical_max_cooldown() const { return m_tactical_max_cooldown; }
    bool is_tactical_ready() const { return m_tactical_cooldown <= 0.0f; }
    void set_tactical_max_cooldown(float cd) { m_tactical_max_cooldown = cd; }
    void trigger_tactical_cooldown(float cd = -1.0f) { m_tactical_cooldown = (cd > 0.0f) ? cd : m_tactical_max_cooldown; }
    void reset_tactical_cooldown() { m_tactical_cooldown = 0.0f; }
    float tactical_recharge_progress() const { return m_tactical_max_cooldown > 0.0f ? std::clamp(1.0f - (m_tactical_cooldown / m_tactical_max_cooldown), 0.0f, 1.0f) : 1.0f; }

    // Chemical Flares (Key: T or Z)
    static constexpr int MAX_FLARES = 3;
    static constexpr float FLARE_RECHARGE_TIME = 15.0f;

    using FlareThrownCallback = std::function<void(const glm::vec3& origin, const glm::vec3& dir, CharacterClass cls)>;
    void set_on_flare_thrown(FlareThrownCallback cb) { m_on_flare_thrown = std::move(cb); }

    int flare_count() const { return m_flare_count; }
    int max_flares() const { return MAX_FLARES; }
    float flare_recharge_timer() const { return m_flare_recharge_timer; }
    float flare_recharge_progress() const {
        return (m_flare_count < MAX_FLARES && m_flare_max_recharge > 0.0f)
            ? std::clamp(1.0f - (m_flare_recharge_timer / m_flare_max_recharge), 0.0f, 1.0f)
            : 1.0f;
    }
    bool can_throw_flare() const { return m_flare_count > 0; }
    void throw_flare();

    // Breadcrumb Trail (placed every 12m of travel)
    const std::vector<glm::vec3>& breadcrumbs() const { return m_breadcrumbs; }
    void clear_breadcrumbs() { m_breadcrumbs.clear(); m_dist_since_breadcrumb = 0.0f; }

private:
    void update_camera_vectors();
    void resolve_voxel_collisions(World& world, glm::vec3& pos, glm::vec3& vel, float dt);
    void resolve_axis_collision(int axis, const glm::vec3& half_extents, World& world);
    void depenetrate(const glm::vec3& half_extents, World& world);

    glm::vec3 m_position;
    glm::vec3 m_velocity{0.0f};

    float m_yaw{-90.0f};
    float m_pitch{0.0f};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};

    bool m_on_ground{false};
    bool m_isGrounded{false};
    bool m_is_crouching{false};
    float m_eyeHeight{1.65f};
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
    float m_carry_weight_multiplier{1.0f};

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

    // Class-specific combat firearm state
    WeaponStats m_weapon_stats{get_class_weapon_stats(CharacterClass::Demolitionist)};
    int m_weapon_ammo{6};
    float m_fire_cooldown{0.0f};
    float m_reload_timer{0.0f};
    float m_recharge_delay{0.0f};
    bool m_thruster_air_braking{false};
    float m_vault_timer{0.0f};
    bool m_is_aiming{false};
    float m_zoom_progress{0.0f};

    // Sonar pulse cooldown (seconds)
    float m_sonar_cooldown{0.0f};
    float m_sonar_max_cooldown{10.0f};

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
    CanDeployChargePredicate m_can_deploy_charge_predicate;
    ChargePlacedCallback m_on_charge_placed;
    CanPlacePredicate m_can_place_predicate;
    WarningCallback m_on_warning;
    TacticalAbilityCallback m_on_tactical_ability;
    JumpCallback m_on_jump;
    LandCallback m_on_land;
    DamageCallback m_on_damage;  // Notifies HUD/application on any HP loss
    DamageSourceCallback m_on_damage_source;
    WeaponReloadCallback m_on_weapon_reload;
    DamageSource m_last_damage_source{DamageSource::Kinetic};

    float m_tactical_cooldown{0.0f};
    float m_tactical_max_cooldown{15.0f};

    // Chemical Flare Inventory & Recharge
    int m_flare_count{3};
    float m_flare_recharge_timer{0.0f};
    float m_flare_max_recharge{15.0f};
    FlareThrownCallback m_on_flare_thrown;

    // Breadcrumb Trail
    std::vector<glm::vec3> m_breadcrumbs;
    glm::vec3 m_last_breadcrumb_pos{16.0f, 25.0f, 16.0f};
    float m_dist_since_breadcrumb{0.0f};

    // Environmental room hazard states
    bool m_is_in_lava{false};
    bool m_is_in_spikes{false};
    float m_spike_damage_timer{0.0f};

    // Headlamp & Defensive Melee Shove state
    bool m_headlamp_on{true};
    float m_melee_shove_cooldown{0.0f};
    float m_melee_shove_timer{0.0f};
    MeleeShoveCallback m_on_melee_shove;

    // Insertion Pod Breach Shielding & Combat Pause
    float m_insertionShieldTimer{0.0f};
    bool m_combat_inputs_paused{false};

public:
    void apply_fall_impact(float impact_speed);
    bool is_in_lava() const { return m_is_in_lava; }
    bool is_in_spikes() const { return m_is_in_spikes; }
    float spike_damage_timer() const { return m_spike_damage_timer; }
};

} // namespace Voidfall
