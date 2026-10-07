#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../src/ai/aberrant_ai.hpp"
#include "../src/ai/spawn_manager.hpp"
#include "../src/entities/carcass_manager.hpp"
#include "../src/entities/enemies/void_stalker.hpp"
#include "../src/entities/enemies/seismic_burrower.hpp"
#include "../src/ui/hud.hpp"
#include "../src/player/controller.hpp"
#include "../src/systems/noise_meter.hpp"
#include "../src/systems/stealth_system.hpp"
#include "../src/audio/audio_engine.hpp"
#include "../src/voxel/world.hpp"

using namespace Voidfall;

inline std::ostream& operator<<(std::ostream& os, StalkerSurfaceState s) {
    switch (s) {
        case StalkerSurfaceState::FLOOR: return os << "FLOOR";
        case StalkerSurfaceState::WALL_CLIMBING: return os << "WALL_CLIMBING";
        case StalkerSurfaceState::CEILING_CRAWLING: return os << "CEILING_CRAWLING";
        case StalkerSurfaceState::TRANSITIONING: return os << "TRANSITIONING";
    }
    return os << static_cast<int>(s);
}

#define ASSERT_TRUE(expr) \
    if (!(expr)) { \
        std::cerr << "[TEST FAILED] Assertion failed: " #expr " at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_FALSE(expr) \
    if (expr) { \
        std::cerr << "[TEST FAILED] Assertion failed (expected false): " #expr " at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") > " #b " (" << (b) << ") at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_GE(a, b) \
    if (!((a) >= (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") >= " #b " (" << (b) << ") at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_LE(a, b) \
    if (!((a) <= (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") <= " #b " (" << (b) << ") at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_EQ(a, b) \
    if (!((a) == (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") == " #b " (" << (b) << ") at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_NEAR(a, b, eps) \
    if (std::abs((a) - (b)) > (eps)) { \
        std::cerr << "[TEST FAILED] |" #a " - " #b "| (" << std::abs((a) - (b)) << ") <= " #eps " at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

// ─── TEST 1: CombatCollisionTest, EnemyLungeMaintainsStandOffDistance ─────────
void test_combat_collision_lunge_stand_off() {
    std::cout << "[TEST 1] CombatCollisionTest.EnemyLungeMaintainsStandOffDistance..." << std::endl;

    glm::vec3 player_pos(0.0f, 0.0f, 0.0f);
    float player_radius = 0.35f;
    float enemy_radius = 0.50f;
    float combined_hull = player_radius + enemy_radius; // 0.85m

    World world;
    VoidStalkerManager manager;
    manager.spawn_melee(glm::vec3(4.5f, 0.0f, 0.0f));
    ASSERT_EQ(manager.active_count(), 1);

    auto& stalkers = const_cast<std::vector<VoidStalker>&>(manager.stalkers());
    VoidStalker& stalker = stalkers[0];
    stalker.state = StalkerState::Lunging;
    stalker.state_timer = 0.0f;

    // Simulate leap physics over the full 0.65s leap duration (60 steps @ 0.016s)
    constexpr float dt = 0.016f;
    for (int step = 0; step < 60; ++step) {
        manager.update(dt, player_pos, glm::vec3(1.0f, 0.0f, 0.0f),
                       glm::vec3(1.0f, 0.0f, 0.0f), true, 0.0f, false, world);

        float current_dist = glm::distance(manager.stalkers()[0].position, player_pos);

        // Distance must maintain stand-off distance within tolerance (-0.1m)
        ASSERT_GE(current_dist, AberrantAI::MIN_STRIKE_DISTANCE - 0.10f);

        // Enemy must NEVER penetrate player hull (0.85m)
        ASSERT_GE(current_dist, combined_hull);
    }

    std::cout << "  -> Passed: Stalker maintained stand-off distance (final dist="
              << glm::distance(stalker.position, player_pos) << "m >= "
              << AberrantAI::MIN_STRIKE_DISTANCE - 0.10f << "m, hull=" << combined_hull << "m)" << std::endl;
}

// ─── TEST 2: CarcassSystemTest, EntityLeavesCarcassOnDeath ────────────────────
void test_carcass_system_leaves_carcass_on_death() {
    std::cout << "[TEST 2] CarcassSystemTest.EntityLeavesCarcassOnDeath..." << std::endl;

    CarcassManager::instance().clear();
    ASSERT_EQ(CarcassManager::instance().GetActiveCount(), 0);

    World world;
    // Set up a solid floor below the stalker at y=19 so it settles on y=20
    for (int x = 14; x <= 18; ++x) {
        for (int z = 14; z <= 18; ++z) {
            world.set_voxel(x, 19, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
            world.set_voxel(x, 20, z, Voxel{MAT_AIR, 0}, false);
            world.set_voxel(x, 21, z, Voxel{MAT_AIR, 0}, false);
        }
    }

    VoidStalkerManager manager;
    manager.spawn_melee(glm::vec3(16.0f, 21.0f, 16.0f));
    ASSERT_EQ(manager.active_count(), 1);

    // Apply lethal damage (100 HP) to the Stalker
    bool damaged = manager.damage_nearest(glm::vec3(16.0f, 21.0f, 16.0f), 2.0f, 100.0f);
    ASSERT_TRUE(damaged);

    // Active AI entity count decreases by 1 immediately upon fatal damage
    ASSERT_EQ(manager.active_count(), 0);

    // Step collapse animation sequence (0.75s collapse into static carcass)
    manager.update(0.80f, glm::vec3(16.0f, 21.0f, 25.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                   glm::vec3(0.0f, 0.0f, -1.0f), true, 0.0f, false, world);

    // CarcassManager active count increments by 1
    ASSERT_EQ(CarcassManager::instance().GetActiveCount(), 1);

    // Step physics forward 2.0 seconds so carcass settles to solid floor voxels
    for (int i = 0; i < 60; ++i) {
        CarcassManager::instance().update(0.033f, world);
    }

    const auto& carcasses = CarcassManager::instance().carcasses();
    ASSERT_EQ(carcasses.size(), 1);

    // Verify carcass settled and went to sleep
    ASSERT_TRUE(carcasses[0].is_sleeping);
    ASSERT_TRUE(carcasses[0].isSleeping());

    // Harvest testing: delvers cannot mine monsters/carcasses for chitin
    int carapace = 0;
    int biomass = 0;
    bool harvested = CarcassManager::instance().harvest_nearest(carcasses[0].position, 2.5f, 20.0f, carapace, biomass);
    ASSERT_FALSE(harvested);
    ASSERT_EQ(carapace, 0);
    ASSERT_EQ(biomass, 0);

    std::cout << "  -> Passed: Slain stalker left sleeping physical carcass; monsters cannot be mined for chitin"
              << std::endl;
}

// ─── TEST 3: SpawnPacingTest, InitialGracePeriodEnforced ──────────────────────
void test_spawn_pacing_initial_grace_period() {
    std::cout << "[TEST 3] SpawnPacingTest.InitialGracePeriodEnforced..." << std::endl;

    World world;
    VoidStalkerManager stalker_mgr;
    SpawnManager spawn_mgr;

    glm::vec3 pod_pos(36.0f, 10.0f, 36.0f);
    spawn_mgr.reset(1, pod_pos);

    ASSERT_TRUE(spawn_mgr.is_in_grace_period());
    ASSERT_EQ(spawn_mgr.level(), 1);

    // Step simulation across t in [0.0, 40.0s]
    float current_time = 0.0f;
    while (current_time <= 40.0f) {
        constexpr float dt = 1.0f;
        spawn_mgr.update(dt, world, stalker_mgr, pod_pos);
        current_time += dt;

        // Active enemy count must remain strictly 0 during the initial 45s grace period
        ASSERT_EQ(stalker_mgr.active_count(), 0);
        ASSERT_TRUE(spawn_mgr.is_in_grace_period());

        // Attempting to spawn inside the 24m exclusion zone must be rejected
        ASSERT_FALSE(spawn_mgr.can_spawn(0, pod_pos + glm::vec3(10.0f, 0.0f, 0.0f)));
    }

    std::cout << "  -> Passed: Zero combat spawns triggered across t=[0..40s], grace period intact (remaining="
              << spawn_mgr.grace_timer() << "s)" << std::endl;
}

// ─── TEST 4: AberrantAITest, SurfaceAttachmentAndNormalAlignment ──────────────
void test_aberrant_ai_surface_attachment_and_normal_alignment() {
    std::cout << "[TEST 4] AberrantAITest.SurfaceAttachmentAndNormalAlignment..." << std::endl;

    World world;

    // Sub-case A: Entity on Wall with outward normal n = (1, 0, 0)
    // Place solid voxel at (15, 20, 16). Air at (16, 20, 16).
    world.set_voxel(15, 20, 16, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
    world.set_voxel(16, 20, 16, Voxel{MAT_AIR, 0}, false);

    glm::vec3 wall_pos(16.2f, 20.0f, 16.0f);
    glm::vec3 wall_normal(1.0f, 0.0f, 0.0f);

    for (int step = 0; step < 10; ++step) {
        SurfaceContactSample sample = AberrantAI::sample_surface_normal(wall_pos, world, 1.4f);
        ASSERT_TRUE(sample.has_contact);
        ASSERT_NEAR(sample.contact_normal.x, wall_normal.x, 0.01f);
        ASSERT_NEAR(sample.contact_normal.y, wall_normal.y, 0.01f);
        ASSERT_NEAR(sample.contact_normal.z, wall_normal.z, 0.01f);
        ASSERT_EQ(sample.surface_state, StalkerSurfaceState::WALL_CLIMBING);

        glm::quat q = AberrantAI::calculate_orientation(glm::vec3(0.0f, 1.0f, 0.0f), sample.contact_normal, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::vec3 up_dir = q * glm::vec3(0.0f, 1.0f, 0.0f);
        float alignment_err = glm::length(up_dir - wall_normal);
        ASSERT_LE(alignment_err, 0.05f);
    }

    // Sub-case B: Entity on Ceiling with outward normal n = (0, -1, 0)
    // Place solid voxel at (16, 25, 16). Air at (16, 24, 16) and surrounding neighborhood.
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            world.set_voxel(16 + dx, 24, 16 + dz, Voxel{MAT_AIR, 0}, false);
            if (dx != 0 || dz != 0) {
                world.set_voxel(16 + dx, 25, 16 + dz, Voxel{MAT_AIR, 0}, false);
            }
        }
    }
    world.set_voxel(16, 25, 16, Voxel{MAT_VOLCANIC_BASALT, 0}, false);

    glm::vec3 ceiling_pos(16.0f, 24.2f, 16.0f);
    glm::vec3 ceiling_normal(0.0f, -1.0f, 0.0f);

    for (int step = 0; step < 10; ++step) {
        SurfaceContactSample sample = AberrantAI::sample_surface_normal(ceiling_pos, world, 1.4f);
        ASSERT_TRUE(sample.has_contact);
        ASSERT_NEAR(sample.contact_normal.x, ceiling_normal.x, 0.01f);
        ASSERT_NEAR(sample.contact_normal.y, ceiling_normal.y, 0.01f);
        ASSERT_NEAR(sample.contact_normal.z, ceiling_normal.z, 0.01f);
        ASSERT_EQ(sample.surface_state, StalkerSurfaceState::CEILING_CRAWLING);

        glm::quat q = AberrantAI::calculate_orientation(glm::vec3(1.0f, 0.0f, 0.0f), sample.contact_normal, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::vec3 up_dir = q * glm::vec3(0.0f, 1.0f, 0.0f);
        float alignment_err = glm::length(up_dir - ceiling_normal);
        ASSERT_LE(alignment_err, 0.05f);
    }

    std::cout << "  -> Passed: Entity attached to wall & ceiling; orientation aligns within eps < 0.05 across 10 steps" << std::endl;
}

// ─── TEST 5: PlayerStanceTest, CrouchCameraInterpolation ──────────────────────
void test_player_stance_crouch_interpolation() {
    std::cout << "[TEST 5] PlayerStanceTest.CrouchCameraInterpolation..." << std::endl;

    PlayerController player(glm::vec3(16.0f, 20.0f, 16.0f));

    // Standing initial state
    ASSERT_FALSE(player.is_crouching());
    ASSERT_NEAR(player.eye_height(), PlayerController::EYE_HEIGHT_STAND, 0.001f);
    ASSERT_NEAR(player.half_extents().y, 0.90f, 0.001f);

    // Engage crouch state
    player.set_crouching(true);
    ASSERT_TRUE(player.is_crouching());

    // Step physics forward 0.2s
    player.UpdatePhysics(0.20f);

    // Assert eye height equals Y_crouch (0.95m)
    ASSERT_NEAR(player.eye_height(), PlayerController::EYE_HEIGHT_CROUCH, 0.001f);

    // Assert bounding box extents shrink to crouching size (0.3m, 0.55m, 0.3m)
    glm::vec3 crouch_extents = player.half_extents();
    ASSERT_NEAR(crouch_extents.x, 0.30f, 0.001f);
    ASSERT_NEAR(crouch_extents.y, 0.55f, 0.001f);
    ASSERT_NEAR(crouch_extents.z, 0.30f, 0.001f);

    std::cout << "  -> Passed: Eye height smoothly interpolated to " << player.eye_height()
              << "m and bounding half_extents.y shrunk to " << crouch_extents.y << "m" << std::endl;
}

// ─── TEST 6: StealthMeterTest, NoiseDecayAndBounds ────────────────────────────
void test_stealth_meter_noise_decay_and_bounds() {
    std::cout << "[TEST 6] StealthMeterTest.NoiseDecayAndBounds..." << std::endl;

    // Noise meter with max_noise = 1.0f and decay_rate = 0.5f/s so 1.0 reaches 0.0 in exactly 2.0s
    NoiseMeter meter(1.0f, 0.5f);

    // Inject noise spike (1.0)
    meter.inject_noise(1.0f);
    ASSERT_NEAR(meter.noise(), 1.0f, 0.001f);

    float prev_noise = meter.noise();

    // Step simulation forward 2.0s in 20 steps of 0.1s with no actions
    for (int step = 0; step < 20; ++step) {
        meter.update(0.1f);
        float current_noise = meter.noise();

        // Must decay monotonically (current <= prev)
        ASSERT_LE(current_noise, prev_noise);

        // Must stay bounded within [0.0, 1.0]
        ASSERT_GE(current_noise, 0.0f);
        ASSERT_LE(current_noise, 1.0f);

        prev_noise = current_noise;
    }

    // After 2.0s at rate 0.5/s, noise must have decayed cleanly to 0.0
    ASSERT_NEAR(meter.noise(), 0.0f, 0.001f);

    std::cout << "  -> Passed: Noise spike (1.0) decayed monotonically to "
              << meter.noise() << " within bounds [0.0, 1.0] over 2.0s" << std::endl;
}

// ─── TEST 7: EnemyAwarenessMarkersTest, InvestigatingAndEngagedVisualIndicators ─────
void test_enemy_awareness_markers_investigating_and_engaged_states() {
    std::cout << "[TEST 7] EnemyAwarenessMarkersTest.InvestigatingAndEngagedVisualIndicators..." << std::endl;

    // 1. Void Stalker State Classification
    VoidStalker s;
    s.position = glm::vec3(12.0f, 5.0f, 12.0f);
    s.hp = 40.0f;
    s.scale = 1.3f;
    s.state = StalkerState::Idle;

    // Ambient / Idle -> None
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(s)), static_cast<int>(EnemyAwarenessMarkerType::None));
    ASSERT_FALSE(s.is_investigating_state());
    ASSERT_FALSE(s.is_engaged_state());

    // Acoustic Investigation -> YellowExclamation
    s.state = StalkerState::Investigating;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(s)), static_cast<int>(EnemyAwarenessMarkerType::YellowExclamation));
    ASSERT_TRUE(s.is_investigating_state());
    ASSERT_FALSE(s.is_engaged_state());

    // Stunned -> YellowExclamation alert
    s.state = StalkerState::Stunned;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(s)), static_cast<int>(EnemyAwarenessMarkerType::YellowExclamation));
    ASSERT_TRUE(s.is_investigating_state());

    // Engaged / Combat States -> RedTriangle
    s.state = StalkerState::Stalking;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(s)), static_cast<int>(EnemyAwarenessMarkerType::RedTriangle));
    ASSERT_FALSE(s.is_investigating_state());
    ASSERT_TRUE(s.is_engaged_state());

    s.state = StalkerState::Circling;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(s)), static_cast<int>(EnemyAwarenessMarkerType::RedTriangle));
    ASSERT_TRUE(s.is_engaged_state());

    s.state = StalkerState::Lunging;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(s)), static_cast<int>(EnemyAwarenessMarkerType::RedTriangle));
    ASSERT_TRUE(s.is_engaged_state());

    // Shooter in direct LOS targeting player -> RedTriangle
    s.state = StalkerState::Idle;
    s.role = StalkerRole::Shooter;
    s.has_player_los = true;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(s)), static_cast<int>(EnemyAwarenessMarkerType::RedTriangle));
    ASSERT_TRUE(s.is_engaged_state());

    // Dead / Dying -> None
    s.state = StalkerState::Dying;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(s)), static_cast<int>(EnemyAwarenessMarkerType::None));

    s.state = StalkerState::Dead;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(s)), static_cast<int>(EnemyAwarenessMarkerType::None));

    // 2. Seismic Burrower State Classification
    SeismicBurrower b;
    b.position = glm::vec3(15.0f, 2.0f, 15.0f);
    b.scale = 1.75f;
    b.state = BurrowerState::Burrowing;

    // Subterranean borer digging near player -> YellowExclamation
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(b)), static_cast<int>(EnemyAwarenessMarkerType::YellowExclamation));
    ASSERT_TRUE(b.is_investigating_state());
    ASSERT_FALSE(b.is_engaged_state());

    // Breaching / Charging / Enraged -> RedTriangle
    b.state = BurrowerState::Breaching;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(b)), static_cast<int>(EnemyAwarenessMarkerType::RedTriangle));
    ASSERT_TRUE(b.is_engaged_state());

    b.state = BurrowerState::Charging;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(b)), static_cast<int>(EnemyAwarenessMarkerType::RedTriangle));

    b.state = BurrowerState::Enraged;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(b)), static_cast<int>(EnemyAwarenessMarkerType::RedTriangle));

    b.state = BurrowerState::Dead;
    ASSERT_EQ(static_cast<int>(get_awareness_marker_type(b)), static_cast<int>(EnemyAwarenessMarkerType::None));

    // 3. HUD Awareness Marker World Projection & LOS computation
    HUD hud(1600, 900, /*headless=*/true);
    World world;
    world.generate_world(1, 1337);

    // Carve clear space for test
    for (int x = 5; x <= 20; ++x) {
        for (int y = 0; y <= 15; ++y) {
            for (int z = 5; z <= 20; ++z) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
            }
        }
    }

    std::vector<VoidStalker> stalkers;
    VoidStalker s1;
    s1.position = glm::vec3(10.0f, 5.0f, 10.0f);
    s1.state = StalkerState::Investigating;
    s1.scale = 1.3f;
    s1.hp = 40.0f;
    stalkers.push_back(s1);

    glm::vec3 cam_pos_los(10.0f, 5.0f, 6.0f); // 4m away inside carved air room (direct line of sight)
    auto markers_los = hud.compute_awareness_markers(cam_pos_los, world, &stalkers, nullptr);
    ASSERT_EQ(markers_los.size(), 1);
    ASSERT_EQ(markers_los[0].marker_type, static_cast<int>(EnemyAwarenessMarkerType::YellowExclamation));
    ASSERT_TRUE(markers_los[0].has_los);
    ASSERT_NEAR(markers_los[0].dist, 4.0f, 1.0f);
    ASSERT_GE(markers_los[0].world_pos.y, s1.position.y + 1.0f);

    // Occluded behind rock wall (at z = 0, separated by solid stone wall from z=0 to z=4)
    glm::vec3 cam_pos_wall(10.0f, 5.0f, 0.0f);
    auto markers_wall = hud.compute_awareness_markers(cam_pos_wall, world, &stalkers, nullptr);
    ASSERT_EQ(markers_wall.size(), 1);
    ASSERT_EQ(markers_wall[0].marker_type, static_cast<int>(EnemyAwarenessMarkerType::YellowExclamation));
    ASSERT_FALSE(markers_wall[0].has_los); // Successfully detected occlusion!

    // Transition to attack / engaged mode
    stalkers[0].state = StalkerState::Stalking;
    auto markers_engaged = hud.compute_awareness_markers(cam_pos_los, world, &stalkers, nullptr);
    ASSERT_EQ(markers_engaged.size(), 1);
    ASSERT_EQ(markers_engaged[0].marker_type, static_cast<int>(EnemyAwarenessMarkerType::RedTriangle));

    // Clean elimination -> 0 markers
    stalkers[0].state = StalkerState::Dead;
    auto markers_dead = hud.compute_awareness_markers(cam_pos_los, world, &stalkers, nullptr);
    ASSERT_EQ(markers_dead.size(), 0);

    std::cout << "  -> Passed: Overhead awareness indicators correctly classify Yellow '!' and Red Triangle states without screen spam." << std::endl;
}
// ─── TEST 8: Weapon reload cycle, callback, audio cue (silent) ───────────────
void test_weapon_reload_cycle_and_audio() {
    std::cout << "[TEST 8] WeaponReload.CycleCallbackAndSilentAudio..." << std::endl;

    World world;
    world.generate_world(1, 1337);

    const CharacterClass classes[3] = { CharacterClass::Demolitionist, CharacterClass::Vanguard, CharacterClass::Scout };
    const SoundCue expected_cues[3] = { SoundCue::ScattergunReload, SoundCue::PlasmaCarbineReload, SoundCue::RailgunReload };

    AudioEngine audio;
    audio.init(false); // Headless: zero hardware output

    for (int c = 0; c < 3; ++c) {
        PlayerController player(glm::vec3(16.0f, 20.0f, 16.0f));
        player.set_character_class(classes[c]);
        player.set_active_tool(ToolSlot::CombatWeapon);

        int cb_count = 0;
        float cb_time = 0.0f;
        player.set_on_weapon_reload([&](CharacterClass, float t) { cb_count++; cb_time = t; });

        // Full ammo: reload must be a no-op (no animation, no sound)
        player.reload_weapon();
        ASSERT_FALSE(player.is_reloading());
        ASSERT_EQ(cb_count, 0);

        // Spend one round
        player.set_drilling(true);
        std::vector<PlayerPlasmaBolt> bolts;
        ASSERT_TRUE(player.try_fire_weapon(bolts, 0.016f));
        player.set_drilling(false);
        int spent_ammo = player.weapon_ammo();
        ASSERT_TRUE(spent_ammo < player.weapon_max_ammo());

        player.reload_weapon();
        ASSERT_TRUE(player.is_reloading());
        ASSERT_EQ(cb_count, 1);
        ASSERT_NEAR(cb_time, player.weapon_stats().reload_time, 0.0001f);

        // Second request mid-reload must not retrigger sound
        player.reload_weapon();
        ASSERT_EQ(cb_count, 1);

        // Firing blocked while reloading
        player.set_drilling(true);
        std::vector<PlayerPlasmaBolt> blocked;
        ASSERT_FALSE(player.try_fire_weapon(blocked, 0.016f));
        player.set_drilling(false);

        // Progress is monotonic 0 -> 1, ammo only refilled on completion
        float last_progress = player.reload_progress();
        float total = 0.0f;
        const float dt = 0.05f;
        const float reload_time = player.weapon_stats().reload_time;
        while (player.is_reloading() && total < reload_time + 1.0f) {
            player.update_physics(dt, world);
            total += dt;
            if (player.is_reloading()) {
                ASSERT_GE(player.reload_progress(), last_progress);
                ASSERT_EQ(player.weapon_ammo(), spent_ammo);
                last_progress = player.reload_progress();
            }
        }
        ASSERT_FALSE(player.is_reloading());
        ASSERT_NEAR(total, reload_time, dt + 0.001f);
        ASSERT_EQ(player.weapon_ammo(), player.weapon_max_ammo());

        // Audio: cue exists, produces finite non-silent, ear-safe audio and releases cleanly
        audio.stop_all(true);
        audio.play_weapon_reload(static_cast<int>(player.weapon_archetype()));
        ASSERT_EQ(audio.active_voice_count(), 1);
        auto samples = audio.render_offline_samples(reload_time + 0.3f);
        float energy = 0.0f, peak = 0.0f, tail = 0.0f;
        for (size_t i = 0; i < samples.size(); ++i) {
            float s = samples[i];
            ASSERT_TRUE(std::isfinite(s));
            energy += s * s;
            peak = std::max(peak, std::abs(s));
            if (i + 2205 >= samples.size()) tail = std::max(tail, std::abs(s));
        }
        ASSERT_TRUE(energy > 0.01f);
        ASSERT_LE(peak, 0.95f);
        ASSERT_LE(tail, 0.01f);
        ASSERT_EQ(audio.active_voice_count(), 0);

        // Archetype maps to the expected cue
        audio.stop_all(true);
        audio.play_weapon_reload(static_cast<int>(player.weapon_archetype()));
        bool found = false;
        for (const auto& v : audio.voices()) if (v.active && v.cue == expected_cues[c]) found = true;
        ASSERT_TRUE(found);
    }
    audio.shutdown();

    std::cout << "  -> Passed: reload timing, callback, fire-lock, ammo refill, and silent per-archetype audio verified." << std::endl;
}

// ─── TEST 9: Viewmodel reload state plumbing (headless-safe) ─────────────────
void test_viewmodel_reload_state_defaults() {
    std::cout << "[TEST 9] ViewModel reload animation state..." << std::endl;
    // Reload animation poses are computed inside ViewModel::render (needs GL context);
    // verify here that the progress contract it consumes is bounded and monotonic.
    PlayerController player(glm::vec3(16.0f, 20.0f, 16.0f));
    ASSERT_FALSE(player.is_reloading());
    float p = player.reload_progress();
    ASSERT_GE(p, 0.0f);
    ASSERT_LE(p, 1.0f);
    std::cout << "  -> Passed." << std::endl;
}

// ─── TEST 10: Viewmodel melee animation contracts for each weapon ─────────────
void test_viewmodel_melee_animations_all_weapons() {
    std::cout << "[TEST 10] ViewModel melee animation states for Gun, Demo, and Mining Drill..." << std::endl;
    PlayerController player(glm::vec3(16.0f, 20.0f, 16.0f));

    // Initially off cooldown and not meleeing
    ASSERT_FALSE(player.is_melee_shoving());
    ASSERT_NEAR(player.melee_shove_progress(), 0.0f, 0.001f);
    ASSERT_LE(player.melee_shove_cooldown(), 0.0f);

    // Test melee execution across all 3 weapon slots
    ToolSlot test_tools[] = {
        ToolSlot::CombatWeapon,    // Gun (Carbine/Scattergun/Railgun)
        ToolSlot::DemolitionCharge,// Demo (Tactical Clacker)
        ToolSlot::MiningDrill      // Mining Drill Rig
    };

    for (ToolSlot tool : test_tools) {
        player.set_active_tool(tool);
        ASSERT_TRUE(player.active_tool() == tool);

        // Reset cooldowns for test
        player.update(1.0f);

        // Execute melee shove
        player.execute_melee_shove();
        ASSERT_TRUE(player.is_melee_shoving());
        ASSERT_NEAR(player.melee_shove_progress(), 0.0f, 0.05f);
        ASSERT_TRUE(player.melee_shove_cooldown() > 0.70f);

        // Advance to mid-strike impact window (approx 0.175s)
        player.update(0.175f);
        ASSERT_TRUE(player.is_melee_shoving());
        float mid_p = player.melee_shove_progress();
        ASSERT_GE(mid_p, 0.40f);
        ASSERT_LE(mid_p, 0.60f);

        // Advance past completion (remaining 0.20s -> total 0.375s > 0.35s)
        player.update(0.20f);
        ASSERT_FALSE(player.is_melee_shoving());
        ASSERT_NEAR(player.melee_shove_progress(), 0.0f, 0.001f);
    }

    std::cout << "  -> Passed: All 3 weapon classes (Gun, Demo, Drill) correctly execute melee animation lifecycle." << std::endl;
}

// ─── TEST 11: Acoustic Profile System & Enemy Hearing Detection ─────────────
void test_acoustic_profile_and_enemy_hearing_detection() {
    std::cout << "[TEST 11] AcousticProfile.EnemyHearingAndDetection..." << std::endl;

    // 1. Verify player action acoustic noise emission profiles
    auto& stealth = StealthSystem::instance();
    stealth.reset();

    // Crouch: 0 dB (radius 0m)
    stealth.EmitCrouch(glm::vec3(5.0f, 0.0f, 5.0f));
    const auto& events_crouch = stealth.GetAcousticEvents();
    ASSERT_EQ(events_crouch.size(), 1);
    ASSERT_NEAR(events_crouch[0].db, StealthSystem::NOISE_CROUCH_WALK_DB, 0.001f);
    ASSERT_NEAR(events_crouch[0].radius, StealthSystem::NOISE_CROUCH_WALK_RADIUS, 0.001f);
    ASSERT_EQ(events_crouch[0].db, 0.0f);
    ASSERT_EQ(events_crouch[0].radius, 0.0f);

    // Walk: 15 dB (radius 10m)
    stealth.reset();
    stealth.EmitWalk(glm::vec3(5.0f, 0.0f, 5.0f));
    const auto& events_walk = stealth.GetAcousticEvents();
    ASSERT_EQ(events_walk.size(), 1);
    ASSERT_NEAR(events_walk[0].db, 15.0f, 0.001f);
    ASSERT_NEAR(events_walk[0].radius, 10.0f, 0.001f);

    // Drill: 55 dB (radius 28m)
    stealth.reset();
    stealth.EmitDrill(glm::vec3(5.0f, 0.0f, 5.0f));
    const auto& events_drill = stealth.GetAcousticEvents();
    ASSERT_EQ(events_drill.size(), 1);
    ASSERT_NEAR(events_drill[0].db, 55.0f, 0.001f);
    ASSERT_NEAR(events_drill[0].radius, 28.0f, 0.001f);

    // Satchel Detonation: 100 dB (radius 1000m - entire sector)
    stealth.reset();
    stealth.EmitSatchelDetonation(glm::vec3(5.0f, 0.0f, 5.0f));
    const auto& events_satchel = stealth.GetAcousticEvents();
    ASSERT_EQ(events_satchel.size(), 1);
    ASSERT_NEAR(events_satchel[0].db, 100.0f, 0.001f);
    ASSERT_GE(events_satchel[0].radius, 1000.0f);

    // 2. Pre-spawned enemies initialize in AIState::ROOSTING dormant state on cavern walls/ceilings
    VoidStalkerManager mgr;
    mgr.spawn_roosting(glm::vec3(10.0f, 5.0f, 10.0f), StalkerRole::Melee);
    mgr.spawn_roosting(glm::vec3(12.0f, 8.0f, 10.0f), StalkerRole::Shooter);

    ASSERT_EQ(mgr.stalkers().size(), 2);
    const auto& stalker_wall = mgr.stalkers()[0];
    const auto& stalker_ceiling = mgr.stalkers()[1];

    // Dormant state: AIState::ROOSTING (StalkerState::Roosting), zero velocity
    ASSERT_EQ(static_cast<int>(stalker_wall.state), static_cast<int>(AIState::ROOSTING));
    ASSERT_EQ(static_cast<int>(stalker_ceiling.state), static_cast<int>(AIState::ROOSTING));
    ASSERT_NEAR(glm::length(stalker_wall.velocity), 0.0f, 0.001f);
    ASSERT_NEAR(glm::length(stalker_ceiling.velocity), 0.0f, 0.001f);
    ASSERT_EQ(stalker_wall.surface_state, StalkerSurfaceState::WALL_CLIMBING);
    ASSERT_EQ(stalker_ceiling.surface_state, StalkerSurfaceState::CEILING_CRAWLING);

    // 3. When noise reaching an enemy exceeds 30 dB, transition to AIState::INVESTIGATING toward noise source
    World world;
    world.generate_world(1, 1337);
    // Carve spacious cavern
    for (int x = 0; x <= 40; ++x) {
        for (int y = 0; y <= 20; ++y) {
            for (int z = 0; z <= 40; ++z) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
            }
        }
    }
    // Place solid rock separating wall at x = 14..16 to block visual line-of-sight while allowing acoustic transmission
    for (int x = 14; x <= 16; ++x) {
        for (int y = 0; y <= 20; ++y) {
            for (int z = 0; z <= 40; ++z) {
                world.set_voxel(x, y, z, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
            }
        }
    }

    glm::vec3 hidden_player(18.0f, 5.0f, 10.0f); // 8m away behind solid stone wall (no direct visual sightline)
    glm::vec3 fwd(0.0f, 0.0f, 1.0f);

    // Sub-test A: Walking noise (15 dB, 10m radius) at 8m through stone -> arriving noise is ~2.4 dB (< 30 dB)
    // Stalker should remain in ROOSTING dormant state
    stealth.reset();
    stealth.EmitWalk(hidden_player);

    mgr.update(0.05f, hidden_player, fwd, fwd, false, 0.0f, false, world);
    ASSERT_EQ(static_cast<int>(mgr.stalkers()[0].state), static_cast<int>(AIState::ROOSTING));

    // Sub-test B: Mining Drill (55 dB, 28m radius) at 8m through stone -> arriving noise is ~31.4 dB (> 30 dB, < 70 dB)
    // Stalker must transition to AIState::INVESTIGATING toward noise source
    stealth.reset();
    stealth.EmitDrill(hidden_player);

    auto res_drill = mgr.update(0.05f, hidden_player, fwd, fwd, false, 0.0f, false, world);
    ASSERT_EQ(static_cast<int>(mgr.stalkers()[0].state), static_cast<int>(AIState::INVESTIGATING));
    ASSERT_NEAR(mgr.stalkers()[0].target_pos.x, hidden_player.x, 0.01f);
    ASSERT_NEAR(mgr.stalkers()[0].target_pos.z, hidden_player.z, 0.01f);
    ASSERT_FALSE(res_drill.any_screech); // No screech on sub-70 dB investigation

    // 4. If noise exceeds 70 dB OR player is spotted directly -> transition to AIState::PURSUIT with screech and swarm alert
    // Reset stalkers back to roosting for satchel blast test
    mgr.stalkers_mut()[0].state = StalkerState::Roosting;
    mgr.stalkers_mut()[1].state = StalkerState::Roosting;
    stealth.reset();
    stealth.EmitSatchelDetonation(hidden_player); // 8m away through stone, 100 dB detonation -> ~79.4 dB

    auto res_blast = mgr.update(0.05f, hidden_player, fwd, fwd, false, 0.0f, false, world);
    // Both stalkers should be in PURSUIT (StalkerState::Stalking) due to > 70 dB and swarm alert
    ASSERT_EQ(static_cast<int>(mgr.stalkers()[0].state), static_cast<int>(AIState::PURSUIT));
    ASSERT_EQ(static_cast<int>(mgr.stalkers()[1].state), static_cast<int>(AIState::PURSUIT));
    ASSERT_TRUE(res_blast.any_screech); // Screech emitted!

    // Also test direct visual spotting while roosting -> transition to AIState::PURSUIT
    mgr.stalkers_mut()[0].state = StalkerState::Roosting;
    glm::vec3 direct_player_pos(10.0f, 5.0f, 14.0f); // 4m away in direct line-of-sight on same side of wall
    stealth.reset();
    auto res_spotted = mgr.update(0.05f, direct_player_pos, fwd, fwd, false, 0.0f, false, world);
    ASSERT_EQ(static_cast<int>(mgr.stalkers()[0].state), static_cast<int>(AIState::PURSUIT));
    ASSERT_TRUE(res_spotted.any_spotted);
    ASSERT_TRUE(res_spotted.any_screech);

    stealth.reset();
    std::cout << "  -> Passed: Acoustic Profile emissions, dormant roosting, >30 dB investigation, >70 dB/visual pursuit with screech verified." << std::endl;
}

// ─── MAIN ────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << "RUNNING COMBAT, OMNIDIRECTIONAL AI & CARCASS REGRESSION TESTS" << std::endl;
    std::cout << "============================================================" << std::endl;

    test_combat_collision_lunge_stand_off();
    test_carcass_system_leaves_carcass_on_death();
    test_spawn_pacing_initial_grace_period();
    test_aberrant_ai_surface_attachment_and_normal_alignment();
    test_player_stance_crouch_interpolation();
    test_stealth_meter_noise_decay_and_bounds();
    test_enemy_awareness_markers_investigating_and_engaged_states();
    test_weapon_reload_cycle_and_audio();
    test_viewmodel_reload_state_defaults();
    test_viewmodel_melee_animations_all_weapons();
    test_acoustic_profile_and_enemy_hearing_detection();

    std::cout << "============================================================" << std::endl;
    std::cout << "ALL 11 COMBAT & OMNI AI REGRESSION TESTS PASSED CLEANLY!" << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}
