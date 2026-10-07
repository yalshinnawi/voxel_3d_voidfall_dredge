#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include "../../systems/noise_meter.hpp"
#include "../../ai/aberrant_ai.hpp"
#include "../../graphics/animation_controller.hpp"

namespace Voidfall {

class World;

// StalkerState and AIState behavioral enums are defined in aberrant_ai.hpp

/// Archetype combat role for hostile subterranean stalkers
enum class StalkerRole : uint8_t {
    Melee,          // Crimson Red: Closes in, stalks, circles, and lunges for razor claw strikes. NEVER fires ranged projectiles.
    Shooter,        // Toxic Green: Skirmishes at distance, circles, and fires crystalline void spines. NEVER executes melee lunges.
    ChitinGoliath   // Heavy Carapace Breacher: Subterranean tank with 85% front armor deflection, rear weak points, satchel/stalactite weakness.
};

/// Projectile spine fired by Void Stalkers at mid-range
struct VoidSpikeProjectile {
    uint32_t id{0};
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float lifetime{2.5f};
    float damage{12.0f};
    bool active{true};
};

/// A single Void Stalker hostile entity. Clings to walls and ceilings,
/// stalks behind players, and attacks when the player enters line-of-sight
/// or creates acoustic disturbances.
///
/// Design: Subterranean predator with acute acoustic hearing and shadow vision.
/// Reacts to sound origins in 3D space. Gated by Line-of-Sight for direct attacks.
struct VoidStalker {
    uint32_t id{0};
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 target_pos{0.0f};    // Cached player position
    float yaw{0.0f};               // Facing direction (radians)

    StalkerRole role{StalkerRole::Melee}; // Dedicated combat role (Melee = Red, Shooter = Green)
    StalkerState state{StalkerState::Idle};
    float state_timer{0.0f};        // Time in current state
    float hp{40.0f};
    float max_hp{40.0f};
    float damage{18.0f};            // HP damage per successful lunge hit (Melee only)
    float attack_cooldown{0.0f};    // Prevents rapid double-hits
    float stun_timer{0.0f};
    float projectile_cooldown{1.2f};// Cooldown for ranged void spine attacks (Shooter only)
    float slash_fx_timer{0.0f};     // Visual claw swipe arc duration

    // Acoustic Sound & Investigation
    glm::vec3 investigation_target{0.0f}; // 3D position of heard noise disturbance
    float investigation_timer{0.0f};      // Time spent scanning sound origin
    bool has_sound_target{false};
    bool has_player_los{false};           // Current frame direct line-of-sight to player
    float lost_los_timer{0.0f};           // Seconds elapsed since player broke line-of-sight
    glm::vec3 last_seen_player_pos{0.0f}; // Last known player coordinates

    // Ambient Patrol & Roaming
    glm::vec3 patrol_anchor{0.0f};  // Home cavern chamber anchor point
    glm::vec3 patrol_waypoint{0.0f};// Current roaming destination
    float patrol_timer{0.0f};       // Timer to pick next prowl waypoint

    // Perception & Retaliation Pursuit
    float detection_range{18.0f};   // Range at which stalker notices player
    float lunge_range{4.8f};        // Range at which stalker commits to attack (Melee only)
    float flee_hp_threshold{12.0f}; // HP below which stalker flees
    bool is_in_light{false};        // Illuminated by headlamp cone
    bool is_pursuing_attacker{false}; // Aggressively pursuing player after being attacked
    float pursuit_lost_timer{0.0f};   // Time spent without direct contact while player is fleeing far away
    float pursuit_break_dist{32.0f};  // Distance beyond which escape timer increments
    float pursuit_break_time{12.0f};  // Required time player must stay away to lose enemy pursuit interest

    // Movement tuning
    float move_speed{4.8f};         // Base movement speed (m/s)
    float lunge_speed{14.0f};       // Attack lunge velocity (m/s)
    float circle_speed{3.6f};       // Circling speed around player
    float prowl_speed{1.8f};        // Ambient roaming prowl speed (m/s)

    // Visual & Kinematic Animation
    uint8_t material_id{7};         // Base material reference
    float scale{1.3f};              // Visual scale (massive, terrifying predator)
    float glow_phase{0.0f};         // Pulsing emissive glow animation
    float pitch{0.0f};              // Vertical tilt angle toward target (radians)
    float walk_cycle{0.0f};         // Articulated leg crawl cycle phase

    // Surface Traversal, Rig Alignment & Quaternion Orientation
    StalkerSurfaceState surface_state{StalkerSurfaceState::FLOOR};
    StalkerSurfaceState target_surface_state{StalkerSurfaceState::FLOOR};
    StalkerSurfaceState prev_surface_state{StalkerSurfaceState::FLOOR};
    glm::vec3 m_targetUpVector{0.0f, 1.0f, 0.0f};
    glm::vec3 contact_normal{0.0f, 1.0f, 0.0f};
    glm::quat m_currentRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::quat m_targetRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 surface_offset{0.0f};
    float m_climbSpeedScalar{3.2f};
    float surface_transition_timer{0.2f};
    float surface_cooldown{0.0f};           // Debounce cooldown preventing rapid surface flipping/jitter
    float hit_surface_lock_timer{0.0f};     // Locks surface to FLOOR during hit flinch/stagger
    float stuck_timer{0.0f};                // Obstacle obstruction timer for anti-stuck steering
    glm::vec3 last_progress_pos{0.0f};      // Position checkpoint for movement progress verification
    StalkerAnimationController anim_controller;

    // Helper references & accessors
    const glm::quat& current_rotation() const { return m_currentRotation; }
    const glm::vec3& target_up_vector() const { return m_targetUpVector; }
    bool is_wall_climbing() const { return surface_state == StalkerSurfaceState::WALL_CLIMBING; }
    bool is_ceiling_crawling() const { return surface_state == StalkerSurfaceState::CEILING_CRAWLING; }
    bool is_transitioning() const { return surface_state == StalkerSurfaceState::TRANSITIONING || anim_controller.is_transitioning(); }

    // Audio cue indicators (for HUD/SFX system)
    float screech_timer{0.0f};      // Timer between ambient cavern echo screeches
    bool just_screeched{false};     // Emitted echoing screech this frame
    bool just_chittered{false};     // Emitted subtle chitinous clicking in the dark
    float chitter_timer{0.0f};      // Timer between subtle prowling clicks/chitters
    bool just_dug{false};           // Emitted rock drilling / digging sound this frame
    bool just_escaped{false};       // Successfully burrowed through wall and escaped
    float drill_timer{0.0f};        // Timer between drilling/digging rock sounds
    float burrow_duration{2.0f};    // Total time required to burrow through wall
    glm::vec3 burrow_dir{0.0f};     // Normal direction into rock wall while burrowing
    glm::vec3 burrow_entry{0.0f};   // World coordinates of borehole entry point on rock wall surface
    bool just_spotted_player{false};
    bool just_heard_sound{false};   // Reacted to a sound event this frame
    bool just_lunged{false};
    bool just_hit_player{false};
    bool just_fired_spine{false};
    bool just_died{false};

    // Death Animation Collapse & Ragdoll Settling
    float death_timer{0.0f};
    float death_duration{0.75f};
    glm::vec3 death_impulse{0.0f};
    glm::vec3 death_angular_velocity{0.0f};
    glm::vec3 death_hit_dir{0.0f};

    // Hit Reacts & Dynamic Archetype Modifiers
    float hit_flash_timer{0.0f};        // White emissive shader flash (0.08s)

    /// Returns true if the stalker should be removed from the world
    bool is_dead() const { return state == StalkerState::Dead; }
    bool is_dying() const { return state == StalkerState::Dying; }
    bool is_burrowing() const { return state == StalkerState::Burrowing; }
    bool is_escaping() const { return state == StalkerState::Burrowing; }
    StalkerState GetState() const { return state; }
    bool has_collision() const { return state != StalkerState::Dying && state != StalkerState::Dead && state != StalkerState::Burrowing; }
    bool has_attack_hitbox() const { return state != StalkerState::Dying && state != StalkerState::Dead && state != StalkerState::Burrowing; }
    bool IsCollisionEnabled() const { return has_collision(); }
    bool IsAttackHitboxEnabled() const { return has_attack_hitbox(); }
    bool is_melee() const { return role == StalkerRole::Melee; }
    bool is_shooter() const { return role == StalkerRole::Shooter; }
    bool is_goliath() const { return role == StalkerRole::ChitinGoliath; }

    /// Visual awareness classification for overhead indicators
    bool is_investigating_state() const {
        return !is_dead() && !is_dying() && (state == StalkerState::Investigating || state == StalkerState::Stunned);
    }

    bool is_engaged_state() const {
        return !is_dead() && !is_dying() && (state == StalkerState::Stalking ||
                                             state == StalkerState::Circling ||
                                             state == StalkerState::Lunging ||
                                             (role == StalkerRole::Shooter && has_player_los));
    }

    /// Returns true if the stalker is completely unalerted (unaware of player / sounds)
    bool is_unalerted() const {
        return !is_dead() && !is_dying() &&
               (state == StalkerState::Idle || state == StalkerState::Roosting) &&
               !is_pursuing_attacker &&
               !has_sound_target;
    }

    /// Returns the active emissive eye/core color based on behavioral state and role
    glm::vec4 get_eye_color() const {
        if (state == StalkerState::Roosting) {
            return glm::vec4(0.05f, 0.05f, 0.05f, 0.2f); // Dormant closed eyes
        }

        if (state == StalkerState::Stunned) {
            return glm::vec4(0.2f, 0.90f, 1.0f, 1.0f); // Electric cyan shock
        }

        if (role == StalkerRole::ChitinGoliath) {
            return glm::vec4(1.0f, 0.65f, 0.05f, 1.0f); // Molten amber-gold heavy core
        } else if (role == StalkerRole::Shooter) {
            // Dedicated Toxic Emerald Green for Shooter Archetype
            switch (state) {
                case StalkerState::Idle:          return glm::vec4(0.15f, 0.85f, 0.25f, 1.0f); // Neon toxic green idle
                case StalkerState::Investigating: return glm::vec4(0.55f, 0.95f, 0.10f, 1.0f); // Chartreuse acoustic alert
                case StalkerState::Stalking:      return glm::vec4(0.10f, 1.0f, 0.30f, 1.0f);  // Toxic emerald stalking
                case StalkerState::Circling:      return glm::vec4(0.20f, 1.0f, 0.25f, 1.0f);  // Intense glowing neon green
                case StalkerState::Fleeing:       return glm::vec4(0.65f, 1.0f, 0.15f, 1.0f);  // Lime-chartreuse evasion
                case StalkerState::Burrowing:     return glm::vec4(0.85f, 1.0f, 0.20f, 1.0f);  // Electric lime burrowing
                default:                          return glm::vec4(0.18f, 0.92f, 0.30f, 1.0f); // Toxic green
            }
        } else {
            // Dedicated Predatory Blood Crimson Red for Melee Archetype
            switch (state) {
                case StalkerState::Idle:          return glm::vec4(0.85f, 0.08f, 0.12f, 1.0f); // Predatory dark crimson red
                case StalkerState::Investigating: return glm::vec4(1.0f, 0.55f, 0.05f, 1.0f);  // Amber acoustic alert
                case StalkerState::Stalking:      return glm::vec4(0.95f, 0.08f, 0.08f, 1.0f); // Blazing crimson red
                case StalkerState::Lunging:       return glm::vec4(1.0f, 0.02f, 0.02f, 1.0f);  // Radiant blood red lunge
                case StalkerState::Circling:      return glm::vec4(1.0f, 0.20f, 0.12f, 1.0f);  // Crimson-orange circling
                case StalkerState::Fleeing:       return glm::vec4(1.0f, 0.55f, 0.10f, 1.0f);  // Amber retreat
                case StalkerState::Burrowing:     return glm::vec4(1.0f, 0.65f, 0.15f, 1.0f);  // Intense amber burrowing
                default:                          return glm::vec4(0.85f, 0.08f, 0.08f, 1.0f); // Predatory red
            }
        }
    }
};

/// Visual awareness state for overhead in-world indicators
enum class EnemyAwarenessMarkerType : uint8_t {
    None = 0,
    YellowExclamation, // Investigating / sound heard -> Yellow "!"
    RedTriangle        // Engaged / attack mode -> Red Triangle
};

inline EnemyAwarenessMarkerType get_awareness_marker_type(const VoidStalker& s) {
    if (s.is_engaged_state()) return EnemyAwarenessMarkerType::RedTriangle;
    if (s.is_investigating_state()) return EnemyAwarenessMarkerType::YellowExclamation;
    return EnemyAwarenessMarkerType::None;
}

/// Manages all active Void Stalkers in the expedition cavern.
/// Handles spawning, AI updates, collision with player, and rendering data.
class VoidStalkerManager {
public:
    VoidStalkerManager() = default;

    /// Spawn a new stalker with noise-gated difficulty and dedicated combat role
    void spawn_stalker(const glm::vec3& pos, float difficulty_mul = 1.0f, StalkerRole role = StalkerRole::Melee);
    void spawn_shooter(const glm::vec3& pos, float difficulty_mul = 1.0f) { spawn_stalker(pos, difficulty_mul, StalkerRole::Shooter); }
    void spawn_melee(const glm::vec3& pos, float difficulty_mul = 1.0f) { spawn_stalker(pos, difficulty_mul, StalkerRole::Melee); }
    void spawn_goliath(const glm::vec3& pos, float difficulty_mul = 1.0f) { spawn_stalker(pos, difficulty_mul, StalkerRole::ChitinGoliath); }
    void spawn_roosting(const glm::vec3& pos, StalkerRole role = StalkerRole::Melee, float difficulty_mul = 1.0f);

    /// Spawn an ambient prowling stalker inhabiting a distant cavern chamber
    void spawn_ambient_stalker(const glm::vec3& room_center, const World& world);

    /// Spawn a wave of stalkers around the player (called by NoiseMeter)
    void spawn_wave(const glm::vec3& player_pos, int count, const World& world);

    /// Update all stalker AI, physics, and collision with player
    struct FrameResult {
        float total_damage{0.0f};        // Sum of damage dealt to player this frame
        int stalkers_killed{0};
        int stalkers_stunned{0};
        int stalkers_escaped{0};         // Count of stalkers that burrowed through walls and escaped
        bool any_lunge{false};           // True if any stalker lunged this frame
        bool any_spotted{false};         // True if any stalker spotted player
        bool any_heard_sound{false};     // True if any stalker reacted to an acoustic sound event
        bool any_projectile_fired{false};// True if any spine projectile was launched
        bool any_melee_hit{false};       // True if a melee attack landed
        bool any_screech{false};         // True if any stalker emitted an echoing screech
        bool any_chitter{false};         // True if any stalker clicked/chittered in the dark
        bool any_digging{false};         // True if any stalker made drilling/digging noises into walls
        std::vector<glm::vec3> digging_positions;
    };

    FrameResult update(float dt, const glm::vec3& player_pos,
                       const glm::vec3& player_forward,
                       const glm::vec3& headlamp_dir,
                       bool headlamp_on,
                       float noise_level_pct,
                       bool player_is_drilling,
                       const World& world,
                       const std::vector<SoundEvent>& sound_events = {},
                       bool player_is_crouching = false);

    /// Apply sonar pulse stun to all stalkers within radius
    void apply_sonar_stun(const glm::vec3& origin, float radius);

    /// Defensive melee shove / bash attack in camera cone
    int apply_melee_shove(const glm::vec3& camera_pos, const glm::vec3& camera_dir,
                          float range = 2.5f, float min_cos = 0.65f, float damage = 15.0f);

    /// Apply damage to nearest stalker from player attack (drill hit, explosion, sneak attack, gunshot)
    bool damage_nearest(const glm::vec3& origin, float radius, float damage,
                         bool allow_crit = false, bool* out_is_crit = nullptr,
                         float* out_damage_dealt = nullptr,
                         const glm::vec3* attacker_pos = nullptr,
                         const glm::vec3* shot_direction = nullptr);

    /// Get all active stalkers (for rendering)
    const std::vector<VoidStalker>& stalkers() const { return m_stalkers; }
    std::vector<VoidStalker>& stalkers_mut() { return m_stalkers; }

    /// Get all active void spine projectiles (for rendering)
    const std::vector<VoidSpikeProjectile>& projectiles() const { return m_projectiles; }
    std::vector<VoidSpikeProjectile>& projectiles_mut() { return m_projectiles; }

    /// Count active (non-dead) stalkers
    int active_count() const;

    /// Cleanup dead stalkers
    void remove_dead();

    /// Reset for new expedition
    void reset();

    /// Check line-of-sight between stalker and player
    bool has_line_of_sight(const glm::vec3& from, const glm::vec3& to, const World& world) const;

private:
    /// Find a valid spawn position near target (on a wall/ceiling)
    glm::vec3 find_spawn_pos(const glm::vec3& near, const World& world);

    /// Check if position is in headlamp cone
    bool is_in_headlamp(const glm::vec3& pos, const glm::vec3& player_pos,
                        const glm::vec3& headlamp_dir, float inner_angle = 18.0f) const;

    std::vector<VoidStalker> m_stalkers;
    std::vector<VoidSpikeProjectile> m_projectiles;
    uint32_t m_next_id{1};
    uint32_t m_next_proj_id{1};
    float m_spawn_cooldown{0.0f};
    float m_global_screech_cooldown{25.0f}; // Prevents constant ear fatigue; screeches are rare & terrifying
    glm::vec3 m_prev_player_pos{0.0f};
    bool m_has_prev_player_pos{false};
};

} // namespace Voidfall
