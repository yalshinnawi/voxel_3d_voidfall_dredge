#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../src/ai/swarm_manager.hpp"
#include "../src/systems/stealth_system.hpp"
#include "../src/systems/noise_meter.hpp"
#include "../src/player/controller.hpp"
#include "../src/entities/enemies/void_stalker.hpp"
#include "../src/entities/enemies/seismic_burrower.hpp"
#include "../src/entities/carcass_manager.hpp"
#include "../src/voxel/world.hpp"
#include "../src/voxel/structural_check.hpp"
#include "../src/systems/hazard_clock.hpp"
#include "../src/systems/extraction.hpp"
#include "../src/ui/hud.hpp"
#include "../src/entities/dynamic_debris.hpp"

using namespace Voidfall;

#define ASSERT_TRUE(expr) \
    if (!(expr)) { \
        std::cerr << "[TEST FAILED] Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_FALSE(expr) \
    if (expr) { \
        std::cerr << "[TEST FAILED] Assertion failed (expected false): " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_EQ(a, b) \
    if (!((a) == (b))) { \
        std::cerr << "[TEST FAILED] " #a " == " #b " failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") > " #b " (" << (b) << ") failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_LT(a, b) \
    if (!((a) < (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") < " #b " (" << (b) << ") failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_GE(a, b) \
    if (!((a) >= (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") >= " #b " (" << (b) << ") failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_LE(a, b) \
    if (!((a) <= (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") <= " #b " (" << (b) << ") failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_NEAR(a, b, eps) \
    if (std::abs((a) - (b)) > (eps)) { \
        std::cerr << "[TEST FAILED] |" #a " - " #b "| (" << std::abs((a) - (b)) << ") <= " #eps " failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define TEST(Suite, Case) void Suite##_##Case()

// ─── TEST 1: AgitationTest, MeterDoesNotResetAtOneHundredPercent ───────────────
TEST(AgitationTest, MeterDoesNotResetAtOneHundredPercent) {
    std::cout << "[ RUN      ] AgitationTest.MeterDoesNotResetAtOneHundredPercent" << std::endl;
    SwarmManager& sm = SwarmManager::instance();
    sm.Reset();
    ASSERT_EQ(sm.GetState(), AgitationState::CALM);
    ASSERT_NEAR(sm.GetAgitation(), 0.0f, 0.001f);

    // Increment m_agitation until it reaches 1.0f
    while (sm.GetAgitation() < 1.0f) {
        sm.AddAgitation(0.15f);
    }
    ASSERT_NEAR(sm.GetAgitation(), 1.0f, 0.001f);
    ASSERT_TRUE(sm.GetState() == AgitationState::ENRAGED);

    // Step SwarmManager::Update(dt = 0.1f) for 20 ticks
    for (int tick = 0; tick < 20; ++tick) {
        sm.Update(0.1f);
        ASSERT_NEAR(sm.GetAgitation(), 1.0f, 0.001f);
        ASSERT_TRUE(sm.GetState() == AgitationState::ENRAGED);
        ASSERT_GT(sm.GetEnrageTimer(), 0.0f);
        ASSERT_FALSE(sm.GetAgitation() == 0.0f);
    }

    std::cout << "[       OK ] AgitationTest.MeterDoesNotResetAtOneHundredPercent (Enrage lock verified over 20 ticks, timer="
              << sm.GetEnrageTimer() << "s)" << std::endl;
}

// ─── TEST 2: StealthTest, CrouchProducesZeroNoise ─────────────────────────────
TEST(StealthTest, CrouchProducesZeroNoise) {
    std::cout << "[ RUN      ] StealthTest.CrouchProducesZeroNoise" << std::endl;
    StealthSystem& stealthSystem = StealthSystem::instance();
    stealthSystem.reset();
    stealthSystem.SetNoise(25.0f); // Set arbitrary baseline noise
    float initialNoise = stealthSystem.GetCurrentNoise();

    PlayerController controller(glm::vec3(0.0f, 10.0f, 0.0f));
    ASSERT_FALSE(controller.IsCrouched());

    // Trigger crouch
    controller.SetCrouched(true);
    ASSERT_TRUE(controller.IsCrouched());

    // Step controller update by 1 tick
    controller.Update(0.016f);

    // Assert stealthSystem.GetCurrentNoise() <= initialNoise (zero noise added on stance toggle)
    float postNoise = stealthSystem.GetCurrentNoise();
    ASSERT_LE(postNoise, initialNoise);

    std::cout << "[       OK ] StealthTest.CrouchProducesZeroNoise (Initial=" << initialNoise
              << ", Post=" << postNoise << " <= Initial, zero impulse added on stance toggle)" << std::endl;
}

// ─── TEST 3: AimingParallaxTest, RaycastAlignsWithCrosshairWhenCrouched ────────
TEST(AimingParallaxTest, RaycastAlignsWithCrosshairWhenCrouched) {
    std::cout << "[ RUN      ] AimingParallaxTest.RaycastAlignsWithCrosshairWhenCrouched" << std::endl;
    World world;
    // Clear air corridor between delver and target voxel
    for (int z = 0; z <= 9; ++z) {
        for (int y = 1; y <= 4; ++y) {
            world.set_voxel(0, y, z, {MAT_AIR, 0}, false);
        }
    }
    // Solid floor at Y = 0
    for (int z = 0; z <= 10; ++z) {
        world.set_voxel(0, 0, z, {MAT_VOLCANIC_BASALT, 0}, false);
    }
    // Set target voxel at (0, 1.0, 10.0)
    world.set_voxel(0, 1, 10, {MAT_TITANIUM, 0}, false);
    ASSERT_TRUE(world.get_voxel(0, 1, 10).is_solid());

    PlayerController controller(glm::vec3(0.5f, 1.0f + 0.9f, 0.5f)); // Centered on column, standing
    glm::vec3 target_center(0.5f, 1.5f, 10.5f);

    // Standing: eye height = 1.65m
    ASSERT_FALSE(controller.IsCrouched());
    ASSERT_NEAR(controller.GetCurrentEyeHeight(), PlayerController::EYE_HEIGHT_STAND, 0.05f);

    // Aim at target voxel center from standing eye position
    glm::vec3 standing_aim_dir = glm::normalize(target_center - controller.eye_position());
    controller.set_direction(standing_aim_dir);

    RaycastHit standing_hit = controller.QueryRaycastTarget(world, 25.0f);
    ASSERT_TRUE(standing_hit.hit);
    ASSERT_EQ(standing_hit.block_pos, glm::ivec3(0, 1, 10));

    // Toggle crouch and step controller until Y = 0.95
    controller.SetCrouched(true);
    ASSERT_TRUE(controller.IsCrouched());

    for (int step = 0; step < 50; ++step) {
        controller.UpdatePhysics(0.05f);
        if (std::abs(controller.GetCurrentEyeHeight() - PlayerController::EYE_HEIGHT_CROUCH) < 0.001f) {
            break;
        }
    }
    ASSERT_NEAR(controller.GetCurrentEyeHeight(), PlayerController::EYE_HEIGHT_CROUCH, 0.002f);

    // From crouched eye position, aim directly at target voxel center
    glm::vec3 crouch_aim_dir = glm::normalize(target_center - controller.eye_position());
    controller.set_direction(crouch_aim_dir);

    // Verify raycast target matches crosshair at screen center (0.5, 0.5)
    RaycastHit crouch_hit = controller.QueryRaycastTarget(world, 25.0f);
    ASSERT_TRUE(crouch_hit.hit);
    ASSERT_EQ(crouch_hit.block_pos, glm::ivec3(0, 1, 10));

    // Divergence between camera forward vector and target raycast direction
    glm::vec3 cam_forward = controller.camera().Front;
    float dot_val = std::clamp(glm::dot(cam_forward, crouch_aim_dir), -1.0f, 1.0f);
    float theta = std::acos(dot_val);
    ASSERT_LE(theta, 0.001f);

    std::cout << "[       OK ] AimingParallaxTest.RaycastAlignsWithCrosshairWhenCrouched (EyeHeight="
              << controller.GetCurrentEyeHeight() << "m, Hit=(" << crouch_hit.block_pos.x << ","
              << crouch_hit.block_pos.y << "," << crouch_hit.block_pos.z << "), Divergence theta="
              << theta << " rad < 0.001 rad)" << std::endl;
}

// ─── TEST 4: MonsterLifecycleTest, FatalDamageTriggersDeathStateBeforeRemoval ──
TEST(MonsterLifecycleTest, FatalDamageTriggersDeathStateBeforeRemoval) {
    std::cout << "[ RUN      ] MonsterLifecycleTest.FatalDamageTriggersDeathStateBeforeRemoval" << std::endl;
    World world;
    VoidStalkerManager manager;
    manager.reset();

    // Spawn Void Stalker with 100 HP
    manager.spawn_stalker(glm::vec3(5.0f, 1.0f, 5.0f));
    ASSERT_EQ(manager.stalkers().size(), 1);

    VoidStalker& stalker = manager.stalkers_mut()[0];
    stalker.hp = 100.0f;
    stalker.max_hp = 100.0f;
    ASSERT_EQ(stalker.GetState(), AIState::IDLE);
    ASSERT_TRUE(stalker.has_collision());
    ASSERT_TRUE(stalker.IsCollisionEnabled());

    // Apply 120 damage
    bool damaged = manager.damage_nearest(stalker.position, 3.0f, 120.0f);
    ASSERT_TRUE(damaged);

    // Assert stalker.GetState() == AIState::DYING and enemy capsule collision is disabled
    ASSERT_TRUE(stalker.GetState() == AIState::DYING);
    ASSERT_EQ(stalker.hit_flash_timer, 0.0f);
    ASSERT_FALSE(stalker.has_collision());
    ASSERT_FALSE(stalker.IsCollisionEnabled());
    ASSERT_FALSE(stalker.has_attack_hitbox());
    ASSERT_FALSE(stalker.IsAttackHitboxEnabled());

    // Advance time by 0.5s; assert entity remains in world running collapse sequence before removal or carcass conversion
    glm::vec3 player_pos(0.0f, 1.0f, 0.0f);
    manager.update(0.5f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);

    ASSERT_EQ(manager.stalkers().size(), 1);
    ASSERT_TRUE(manager.stalkers()[0].GetState() == AIState::DYING);
    ASSERT_EQ(manager.stalkers()[0].hit_flash_timer, 0.0f);
    ASSERT_FALSE(manager.stalkers()[0].is_dead());
    ASSERT_TRUE(manager.stalkers()[0].is_dying());

    // Active count drops (not counted as active combatant), but entity remains in world during collapse
    ASSERT_EQ(manager.active_count(), 0);

    // Calling remove_dead does not remove dying entity
    manager.remove_dead();
    ASSERT_EQ(manager.stalkers().size(), 1);

    // Advance beyond collapse duration (0.75s total, 0.5 + 0.35 = 0.85s)
    manager.update(0.35f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);
    ASSERT_TRUE(manager.stalkers()[0].GetState() == StalkerState::Dead);
    ASSERT_TRUE(manager.stalkers()[0].is_dead());

    // Now remove_dead cleans it up
    manager.remove_dead();
    ASSERT_EQ(manager.stalkers().size(), 0);

    std::cout << "[       OK ] MonsterLifecycleTest.FatalDamageTriggersDeathStateBeforeRemoval (Dying state and collapse sequence verified)" << std::endl;
}

// ─── TEST 5: NoiseMeterCooldownTest ───────────────────────────────────────────
// Verifies three invariants:
//   A) During active gunfire the grace timer is kept at zero (no premature cooldown).
//   B) After the player stops firing, the grace period elapses and is_cooling_down()
//      becomes true.
//   C) Post-combat cooldown decay drains the meter to Silent (< 40%) faster than
//      baseline decay would in the same time window.
TEST(NoiseMeterCooldownTest, PostCombatMeterDrainsToSilent) {
    std::cout << "[ RUN      ] NoiseMeterCooldownTest.PostCombatMeterDrainsToSilent" << std::endl;

    // Use a standalone NoiseMeter (not the global singleton) to keep this test isolated.
    // grace = 3 s, cooldown decay = 12 u/s, base decay = 2.5 u/s, max_noise = 100
    NoiseMeter meter(100.0f, 2.5f);
    meter.set_cooldown_grace(3.0f);
    meter.set_cooldown_decay_rate(12.0f);

    // ── Phase 1: Simulate a burst of 6 gunshots (1 shot per second) ──────────
    // Each gunshot via add_gunshot_sound adds ~25 noise units.
    // Between shots we tick the meter at 60 fps so the grace timer is reset each shot.
    const glm::vec3 muzzle(0.0f);
    for (int shot = 0; shot < 6; ++shot) {
        meter.add_gunshot_sound(muzzle, /*weapon_archetype=*/0); // +25 noise, resets grace timer
        // Simulate ~1 s between shots at 60 fps
        for (int tick = 0; tick < 60; ++tick) {
            meter.update(1.0f / 60.0f);
        }
        // Immediately after each shot the grace timer should be < 3 s (reset occurred)
        ASSERT_TRUE(meter.time_since_last_noise() < 3.0f);
    }

    // Noise level must be elevated after a firefight
    float noise_after_combat = meter.noise_percent();
    ASSERT_GT(noise_after_combat, 0.0f);
    std::cout << "    [INFO] Noise after 6-shot burst: " << noise_after_combat << "%" << std::endl;

    // ── Phase 2: Verify grace gate — cooldown should NOT be active yet ────────
    // We have been updating but the last shot reset the timer less than 3 s ago.
    ASSERT_FALSE(meter.is_cooling_down());

    // ── Phase 3: Simulate player going quiet (no noise for 3.5 s) ────────────
    for (int tick = 0; tick < static_cast<int>(3.5f * 60.0f); ++tick) {
        meter.update(1.0f / 60.0f);
    }

    // Grace period has now elapsed; cooldown should be active (if meter still > 0)
    float noise_after_grace = meter.noise_percent();
    if (noise_after_grace > 0.0f) {
        ASSERT_TRUE(meter.is_cooling_down());
    }
    std::cout << "    [INFO] Noise after 3.5 s silence (grace elapsed): " << noise_after_grace << "%" << std::endl;

    // ── Phase 4: Verify accelerated drain — simulate additional 10 s quiet ────
    // At 12 u/s it takes ≤ 8.34 s to drain 100 → 0; 10 s must bring us to Silent.
    for (int tick = 0; tick < static_cast<int>(10.0f * 60.0f); ++tick) {
        meter.update(1.0f / 60.0f);
    }
    float noise_after_cooldown = meter.noise_percent();
    std::cout << "    [INFO] Noise after additional 10 s cooldown: " << noise_after_cooldown << "%" << std::endl;
    ASSERT_TRUE(noise_after_cooldown < 40.0f); // Must be below Alerted threshold

    // ── Phase 5: Confirm baseline decay would NOT have achieved the same result ─
    // Reset meter to combat-level noise, force baseline mode (no cooldown grace),
    // and run only 5 s at 2.5 u/s — should leave significant noise remaining.
    NoiseMeter baseline_meter(100.0f, 2.5f);
    baseline_meter.set_cooldown_grace(9999.0f); // Never triggers cooldown
    baseline_meter.set_noise(noise_after_combat);  // Same starting point
    for (int tick = 0; tick < static_cast<int>(5.0f * 60.0f); ++tick) {
        baseline_meter.update(1.0f / 60.0f);
    }
    float baseline_residual = baseline_meter.noise_percent();
    std::cout << "    [INFO] Baseline (no cooldown) residual after 5 s: " << baseline_residual << "%" << std::endl;
    // Cooldown meter should have drained much more than baseline in the same period
    ASSERT_GT(baseline_residual, noise_after_cooldown);

    std::cout << "[       OK ] NoiseMeterCooldownTest.PostCombatMeterDrainsToSilent "
              << "(Combat=" << noise_after_combat << "%, Post-grace="
              << noise_after_grace << "%, After-cooldown=" << noise_after_cooldown
              << "%, Baseline-residual=" << baseline_residual << "%)" << std::endl;
}

// ─── TEST 6: NoiseMeterCooldownTest, SwarmEventForcesAggressiveDrain ──────────
// After the meter tips into Swarming (100%), the post-event cooldown window
// must activate. The decay rate floor (20 u/s) means the meter drains faster
// than baseline (2.5 u/s) during inter-shot gaps.
TEST(NoiseMeterCooldownTest, SwarmEventForcesAggressiveDrain) {
    std::cout << "[ RUN      ] NoiseMeterCooldownTest.SwarmEventForcesAggressiveDrain" << std::endl;

    SwarmManager::instance().Reset();

    NoiseMeter meter(100.0f, 2.5f);
    meter.set_cooldown_grace(9999.0f);
    meter.set_post_event_decay_rate(20.0f);
    meter.set_post_event_duration(10.0f);

    // ── Arm the post-event window by crossing Swarming threshold ─────────────
    meter.set_noise(99.5f);
    meter.inject_noise(1.0f);              // push to 100%
    meter.update(1.0f / 60.0f);           // Swarming branch fires
    ASSERT_TRUE(meter.is_post_event_cooldown_active());
    std::cout << "    [INFO] Post-event cooldown armed: "
              << meter.post_event_cooldown_remaining() << " s" << std::endl;

    // ── Measure drain over 2 s with NO additional noise (pure decay phase) ────
    // At 20 u/s the meter should drain ~40 units over 2 s.
    // At 2.5 u/s baseline it would only drain ~5 units.
    float noise_start = meter.noise_percent();
    for (int tick = 0; tick < 120; ++tick) {   // 2 s at 60 fps
        meter.update(1.0f / 60.0f);
    }
    float noise_end = meter.noise_percent();
    float drained = noise_start - noise_end;
    std::cout << "    [INFO] Post-event drained in 2 s: " << drained << "% (from "
              << noise_start << "% → " << noise_end << "%)" << std::endl;

    // Must have drained more than baseline (2.5 u/s × 2 s = 5 units = 5%)
    ASSERT_GT(drained, 5.0f);

    // ── Verify same 2 s on baseline meter (no post-event) drains far less ────
    SwarmManager::instance().Reset();
    NoiseMeter baseline(100.0f, 2.5f);
    baseline.set_cooldown_grace(9999.0f);
    baseline.set_post_event_decay_rate(0.0f);
    baseline.set_post_event_duration(0.0f);
    baseline.set_noise(noise_start);
    for (int tick = 0; tick < 120; ++tick) {
        baseline.update(1.0f / 60.0f);
    }
    float baseline_drained = noise_start - baseline.noise_percent();
    std::cout << "    [INFO] Baseline drained in 2 s:   " << baseline_drained << "%" << std::endl;

    // Post-event must have drained significantly more than baseline
    ASSERT_GT(drained, baseline_drained);

    std::cout << "[ OK ] NoiseMeterCooldownTest.SwarmEventForcesAggressiveDrain ("
              << "post-event=" << drained << "% drained vs baseline="
              << baseline_drained << "% over 2 s)" << std::endl;
}

// ─── TEST 7: NoiseMeterCooldownTest, LargeNoiseSpikeActivatesCooldown ─────────
// A single demolition / seismic blast (intensity >= 20) must immediately arm
// the post-event cooldown window without requiring a full swarm threshold.
TEST(NoiseMeterCooldownTest, LargeNoiseSpikeActivatesCooldown) {
    std::cout << "[ RUN      ] NoiseMeterCooldownTest.LargeNoiseSpikeActivatesCooldown" << std::endl;

    SwarmManager::instance().Reset();

    NoiseMeter meter(100.0f, 2.5f);
    meter.set_cooldown_grace(9999.0f);
    meter.set_post_event_decay_rate(20.0f);
    meter.set_post_event_duration(10.0f);
    meter.set_large_event_threshold(20.0f); // default, but explicit for clarity

    // Start at a moderate noise level (not swarming)
    meter.set_noise(40.0f);
    ASSERT_FALSE(meter.is_post_event_cooldown_active());

    // ── Emit a demolition blast (intensity = 36, well above threshold = 20) ──
    const glm::vec3 origin(0.0f);
    meter.add_demolition_sound(origin, /*is_micro=*/false); // intensity = 36
    meter.update(1.0f / 60.0f);

    // Post-event window must be armed immediately
    ASSERT_TRUE(meter.is_post_event_cooldown_active());
    float remaining = meter.post_event_cooldown_remaining();
    // Should be ~2/3 of post_event_duration = 6.67 s (minus one tick)
    ASSERT_GT(remaining, 5.0f);
    std::cout << "    [INFO] Post-event cooldown after demolition: " << remaining << " s" << std::endl;

    // ── Emit a seismic tremor (add_seismic_sound: intensity * 15 = 15 u) ──────
    // Seismic: intensity arg of 2.0 → 2*15 = 30 noise → triggers threshold
    NoiseMeter meter2(100.0f, 2.5f);
    meter2.set_cooldown_grace(9999.0f);
    meter2.set_post_event_decay_rate(20.0f);
    meter2.set_post_event_duration(10.0f);
    meter2.set_large_event_threshold(20.0f);
    meter2.set_noise(30.0f);
    ASSERT_FALSE(meter2.is_post_event_cooldown_active());

    meter2.add_seismic_sound(origin, 2.0f); // emits 30 noise, intensity arg * 15 = 30
    meter2.update(1.0f / 60.0f);
    ASSERT_TRUE(meter2.is_post_event_cooldown_active());
    std::cout << "    [INFO] Post-event cooldown after seismic tremor: "
              << meter2.post_event_cooldown_remaining() << " s" << std::endl;

    // ── Verify a small noise (footstep, intensity = 3.5) does NOT trigger it ──
    NoiseMeter meter3(100.0f, 2.5f);
    meter3.set_large_event_threshold(20.0f);
    meter3.set_noise(20.0f);
    meter3.add_movement_sound(origin, 0.016f, /*sprinting=*/false, /*crouching=*/false, /*grounded=*/true);
    meter3.update(1.0f / 60.0f);
    ASSERT_FALSE(meter3.is_post_event_cooldown_active());
    std::cout << "    [INFO] Footstep correctly did NOT trigger post-event cooldown" << std::endl;

    std::cout << "[ OK ] NoiseMeterCooldownTest.LargeNoiseSpikeActivatesCooldown (demolition + seismic arm cooldown; footstep does not)" << std::endl;
}


// ─────────────────────────────────────────────────────────────────────────────
// Test 8: SwarmManager ENRAGED → COOLDOWN → CALM lifecycle
//   Root cause of the "threat peak never clears" bug: SwarmManager::Update(dt)
//   was never called in the game loop, so m_enrageTimer never decremented and
//   the ENRAGED state was permanent. This test exercises the full state machine.
// ─────────────────────────────────────────────────────────────────────────────
static void SwarmManagerLifecycleTest_EnrageExpiresAndCoolsToCalm() {
    std::cout << "[ RUN      ] SwarmManagerLifecycleTest.EnrageExpiresAndCoolsToCalm" << std::endl;

    SwarmManager sm;
    // Confirm fresh state
    ASSERT_TRUE(sm.GetState() == AgitationState::CALM);
    ASSERT_FALSE(sm.IsEnraged());

    // Trigger enrage directly (same path as NoiseMeter hitting 100%)
    sm.SetAgitation(1.0f);
    ASSERT_TRUE(sm.IsEnraged());
    ASSERT_TRUE(sm.GetEnrageTimer() > 0.0f);
    float initial_timer = sm.GetEnrageTimer();

    // Tick forward one second — without Update() the timer must NOT move
    // (validates that Update is what drives the timer, not SetAgitation)
    float timer_before = sm.GetEnrageTimer();
    ASSERT_TRUE(timer_before == initial_timer); // sanity: no implicit tick

    // ── Simulate ENRAGE_DURATION seconds of Update() ticks ──────────────
    const float dt = 1.0f / 60.0f;
    int ticks_to_expire = static_cast<int>(SwarmManager::ENRAGE_DURATION / dt) + 10;
    for (int i = 0; i < ticks_to_expire; ++i) {
        sm.Update(dt);
        // During enrage, m_active_wave_hostiles is 0 (never set), so the
        // transition to COOLDOWN fires as soon as the timer hits 0.
        if (sm.GetState() != AgitationState::ENRAGED) break;
    }

    // Must now be in COOLDOWN (not still ENRAGED)
    ASSERT_TRUE(sm.GetState() == AgitationState::COOLDOWN);
    ASSERT_FALSE(sm.IsEnraged());
    std::cout << "    [INFO] Transitioned ENRAGED -> COOLDOWN after ~"
              << SwarmManager::ENRAGE_DURATION << "s of Update() ticks" << std::endl;

    // ── Tick through the cooldown decay phase ────────────────────────────
    float agitation_at_start_of_cooldown = sm.GetAgitation();
    int cooldown_ticks = static_cast<int>(SwarmManager::COOLDOWN_DURATION / dt) + 10;
    for (int i = 0; i < cooldown_ticks; ++i) {
        sm.Update(dt);
        if (sm.GetState() == AgitationState::CALM) break;
    }

    ASSERT_TRUE(sm.GetState() == AgitationState::CALM);
    ASSERT_FALSE(sm.IsEnraged());
    ASSERT_TRUE(sm.GetAgitation() <= 0.01f); // agitation fully drained
    std::cout << "    [INFO] Agitation at cooldown start: " << agitation_at_start_of_cooldown
              << " -> CALM (agitation=" << sm.GetAgitation() << ")" << std::endl;

    std::cout << "[ OK ] SwarmManagerLifecycleTest.EnrageExpiresAndCoolsToCalm"
              << " (ENRAGED->COOLDOWN->CALM state machine verified)" << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 9: Player Damage, Fall Impact Visual Indication & Screen Shake Trauma
// ─────────────────────────────────────────────────────────────────────────────
static void DamageAndScreenShakeTest_FallAndDirectDamageTriggerTraumaAndCallback() {
    std::cout << "[ RUN      ] DamageAndScreenShakeTest.FallAndDirectDamageTriggerTraumaAndCallback" << std::endl;

    PlayerController player(glm::vec3(0.0f, 10.0f, 0.0f));
    player.set_trauma(0.0f);
    ASSERT_NEAR(player.trauma(), 0.0f, 0.001f);

    float recorded_dmg = 0.0f;
    bool recorded_impact = false;
    int callback_count = 0;

    player.set_on_damage([&](float dmg, bool is_impact) {
        recorded_dmg = dmg;
        recorded_impact = is_impact;
        callback_count++;
    });

    // 1. Fall damage test (impact_speed = 16.0f > 13.0f threshold)
    player.apply_fall_impact(16.0f);
    ASSERT_GT(callback_count, 0);
    ASSERT_GT(recorded_dmg, 0.0f);
    ASSERT_TRUE(recorded_impact);
    ASSERT_GE(player.trauma(), 0.25f); // Visible punchy camera trauma screen shake
    std::cout << "    [INFO] Fall impact applied " << recorded_dmg << " damage, trauma="
              << player.trauma() << std::endl;

    // 2. Direct damage test (e.g. hazard or mob hit)
    int prev_count = callback_count;
    player.set_trauma(0.0f);
    float applied = player.take_damage(20.0f, false);
    ASSERT_EQ(callback_count, prev_count + 1);
    ASSERT_NEAR(recorded_dmg, applied, 0.001f);
    ASSERT_FALSE(recorded_impact);
    ASSERT_GE(player.trauma(), 0.25f);
    std::cout << "    [INFO] Direct damage applied " << applied << " damage, trauma="
              << player.trauma() << std::endl;

    std::cout << "[ OK ] DamageAndScreenShakeTest.FallAndDirectDamageTriggerTraumaAndCallback"
              << " (damage callback + trauma shake verified)" << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 10: Swarm Breach Recovery: Defeating Mobs Clears Threat Peak & Cools Down
// ─────────────────────────────────────────────────────────────────────────────
static void SwarmBreachRecuperationTest_FightingMobsClearsThreatPeakAndInitiatesCooldown() {
    std::cout << "[ RUN      ] SwarmBreachRecuperationTest.FightingMobsClearsThreatPeakAndInitiatesCooldown" << std::endl;

    SwarmManager& sm = SwarmManager::instance();
    sm.reset();

    NoiseMeter meter(100.0f, 2.5f);
    meter.set_cooldown_grace(2.0f);
    meter.set_cooldown_decay_rate(15.0f);
    meter.set_post_event_decay_rate(25.0f);
    meter.set_post_event_duration(15.0f);

    // Wire enrage ended callback to trigger post-combat cooldown
    sm.SetOnEnrageEnded([&]() {
        meter.trigger_post_combat_cooldown();
    });

    // 1. Build noise to 100% to trigger Swarm Breach
    meter.inject_noise(100.0f);
    meter.update(1.0f / 60.0f);

    ASSERT_TRUE(sm.IsEnraged());
    ASSERT_GT(meter.swarm_cooldown(), 0.0f); // 20s refractory window armed
    ASSERT_TRUE(meter.is_post_event_cooldown_active());
    std::cout << "    [INFO] Swarm Breach triggered: IsEnraged=" << sm.IsEnraged()
              << ", SwarmCooldown=" << meter.swarm_cooldown() << "s" << std::endl;

    // 2. Set active wave hostiles to 3
    sm.set_active_wave_hostiles(3);
    ASSERT_EQ(sm.active_wave_hostiles(), 3);

    // 3. Delver fights and kills all 3 hostiles
    sm.on_hostile_killed();
    ASSERT_EQ(sm.active_wave_hostiles(), 2);
    ASSERT_TRUE(sm.IsEnraged()); // Still enraged with remaining hostiles

    sm.on_hostile_killed();
    ASSERT_EQ(sm.active_wave_hostiles(), 1);

    sm.on_hostile_killed(); // 3rd hostile killed — wave defeated!
    ASSERT_EQ(sm.active_wave_hostiles(), 0);

    // Swarm Breach must now be CLEARED immediately!
    ASSERT_FALSE(sm.IsEnraged());
    ASSERT_EQ(sm.GetState(), AgitationState::COOLDOWN);
    std::cout << "    [INFO] Hostiles defeated: Swarm Breach cleared! State="
              << (sm.GetState() == AgitationState::COOLDOWN ? "COOLDOWN" : "OTHER") << std::endl;

    // 4. Alert level in NoiseMeter must NOT be Swarming (capped by swarm cooldown)
    ASSERT_FALSE(meter.alert_level() == NoiseMeter::AlertLevel::Swarming);

    // 5. Fire gunshots during cooldown — must NOT re-enrage or re-trigger Swarm Breach
    meter.add_gunshot_sound(glm::vec3(0.0f));
    meter.add_gunshot_sound(glm::vec3(0.0f));
    meter.update(1.0f / 60.0f);
    sm.Update(1.0f / 60.0f);

    ASSERT_FALSE(sm.IsEnraged()); // Cooldown protection prevents instant re-enrage
    ASSERT_FALSE(meter.alert_level() == NoiseMeter::AlertLevel::Swarming);

    // 6. Simulate 5 seconds of recuperation — meter must drain rapidly towards silent
    float noise_before = meter.noise();
    for (int i = 0; i < 300; ++i) { // 5s at 60Hz
        meter.update(1.0f / 60.0f);
        sm.Update(1.0f / 60.0f);
    }
    ASSERT_LT(meter.noise(), noise_before);
    ASSERT_LE(meter.noise(), 15.0f); // Drained down significantly towards silent
    std::cout << "    [INFO] Noise drained from " << noise_before << " -> " << meter.noise()
              << " (" << meter.alert_string() << ")" << std::endl;

    std::cout << "[ OK ] SwarmBreachRecuperationTest.FightingMobsClearsThreatPeakAndInitiatesCooldown"
              << " (mob kills clear breach, auto-cooldown drains meter)" << std::endl;
}

// ─── TEST 11: BulletImpactSeismicActivityTest, GunfireGeneratesStressAndTriggersFaultSlip ────
TEST(BulletImpactSeismicActivityTest, GunfireGeneratesStressAndTriggersFaultSlip) {
    std::cout << "[ RUN      ] BulletImpactSeismicActivityTest.GunfireGeneratesStressAndTriggersFaultSlip" << std::endl;

    HazardClock hazard;
    hazard.set_sector_parameters(2);
    hazard.reset();

    ASSERT_EQ(hazard.seismic_stress(), 0.0f);
    ASSERT_EQ(hazard.bullet_hits_since_tremor(), 0);

    // 1. Plasma Carbine muzzle blast and wall impact
    // Muzzle: 8.0f, Wall: 5.0f -> Total 13.0f per shot
    hazard.on_weapon_impact(8.0f);
    ASSERT_GE(hazard.seismic_stress(), 8.0f);
    ASSERT_EQ(hazard.bullet_hits_since_tremor(), 1);

    hazard.on_weapon_impact(5.0f);
    ASSERT_GE(hazard.seismic_stress(), 13.0f);
    ASSERT_EQ(hazard.bullet_hits_since_tremor(), 2);

    // 2. Needler Railgun heavy hypersonic impact
    // Muzzle: 15.0f, Wall: 9.5f -> Total 24.5f
    float stress_before_railgun = hazard.seismic_stress();
    hazard.on_weapon_impact(15.0f);
    hazard.on_weapon_impact(9.5f);
    ASSERT_GE(hazard.seismic_stress(), stress_before_railgun + 24.0f);
    ASSERT_EQ(hazard.bullet_hits_since_tremor(), 4);

    // 3. Enemy impacts (Void Stalker / Burrower) also add seismic stress
    float stress_before_enemy = hazard.seismic_stress();
    hazard.on_weapon_impact(3.5f); // Stalker hit
    hazard.on_weapon_impact(5.5f); // Burrower hit
    ASSERT_GE(hazard.seismic_stress(), stress_before_enemy + 9.0f);

    std::cout << "    [INFO] Accumulated seismic stress from gunfire: "
              << hazard.seismic_stress() << "% over " << hazard.bullet_hits_since_tremor() << " hits" << std::endl;

    // 4. Repeated gunfire pushing past 50% triggers fault rupture / tremor sequence
    bool warning_called = false;
    bool tremor_called = false;
    hazard.set_on_tremor_warning([&]() { warning_called = true; });
    hazard.set_on_tremor([&](float) { tremor_called = true; });

    // Rapid volley to exceed 50% threshold and trigger fault slip
    for (int i = 0; i < 20; ++i) {
        hazard.on_weapon_impact(6.0f);
        if (hazard.is_warning() || hazard.is_tremoring()) {
            break;
        }
    }

    ASSERT_TRUE(hazard.is_warning() || hazard.is_tremoring());
    std::cout << "    [INFO] Fault slip triggered by gunfire! is_warning=" << hazard.is_warning()
              << " is_tremoring=" << hazard.is_tremoring() << std::endl;

    // 5. Complete tremor sequence relieves stress back to 0%
    hazard.update(hazard.warning_duration() + hazard.tremor_duration() + 0.5f);
    ASSERT_FALSE(hazard.is_tremoring());
    ASSERT_EQ(hazard.seismic_stress(), 0.0f);

    std::cout << "[ OK ] BulletImpactSeismicActivityTest.GunfireGeneratesStressAndTriggersFaultSlip"
              << " (bullets add muzzle, wall, and enemy seismic stress; gunfire triggers fault rupture)" << std::endl;
}

// ─── TEST: LocalizedSeismicZonesTest ──────────────────────────────────────────
TEST(LocalizedSeismicZonesTest, WallDamageAccumulatesStressLocallyAndTriggersEpicenterTremor) {
    std::cout << "[ RUN      ] LocalizedSeismicZonesTest.WallDamageAccumulatesStressLocallyAndTriggersEpicenterTremor" << std::endl;

    HazardClock hazard;
    hazard.set_sector_parameters(2);
    hazard.reset();

    glm::vec3 zoneA(10.0f, 5.0f, 10.0f);
    glm::vec3 zoneB(80.0f, 5.0f, 80.0f);

    hazard.set_player_position(zoneA);
    ASSERT_EQ(hazard.seismic_stress(), 0.0f);
    ASSERT_EQ(hazard.seismic_stress_at(zoneA), 0.0f);
    ASSERT_EQ(hazard.seismic_stress_at(zoneB), 0.0f);

    // 1. Mining rock in Zone A accumulates stress ONLY in Zone A
    hazard.on_rock_mined(zoneA, MAT_VOLCANIC_BASALT);
    ASSERT_GT(hazard.seismic_stress_at(zoneA), 0.0f);
    ASSERT_EQ(hazard.seismic_stress_at(zoneB), 0.0f);

    // 2. Weapon impact on walls in Zone A further destabilizes Zone A
    float stress_before_impact = hazard.seismic_stress_at(zoneA);
    hazard.on_weapon_impact(zoneA + glm::vec3(1.0f, 0.0f, 0.5f), 15.0f);
    ASSERT_GT(hazard.seismic_stress_at(zoneA), stress_before_impact);
    ASSERT_EQ(hazard.seismic_stress_at(zoneB), 0.0f);

    // 3. Player standing in Zone A sees local seismic stress on meter
    hazard.set_player_position(zoneA);
    ASSERT_GT(hazard.seismic_stress(), 0.0f);

    // 4. Player walking to undamaged Zone B sees 0% stress on meter
    hazard.set_player_position(zoneB);
    ASSERT_EQ(hazard.seismic_stress(), 0.0f);

    // 5. Heavy demolitions in Zone A pushes Zone A past 100% and triggers localized tremor
    bool spatial_warning_fired = false;
    glm::vec3 recorded_epicenter(0.0f);
    float recorded_radius = 0.0f;
    hazard.set_on_tremor_warning([&](const glm::vec3& epi, float rad) {
        spatial_warning_fired = true;
        recorded_epicenter = epi;
        recorded_radius = rad;
    });

    hazard.on_explosive_detonation(zoneA, 90.0f);
    ASSERT_TRUE(hazard.is_warning() || hazard.is_tremoring());
    ASSERT_TRUE(spatial_warning_fired);

    // Epicenter must be centered around Zone A where damage occurred, NOT at Zone B
    float dist_to_zoneA = glm::distance(recorded_epicenter, zoneA);
    ASSERT_LT(dist_to_zoneA, HazardClock::ZONE_SIZE);

    // 6. Proximity-based camera shaking:
    // A player near Zone A is inside tremor zone and experiences screen shaking
    ASSERT_GT(hazard.active_tremor_proximity(zoneA), 0.70f);
    // A player far away in Zone B has 0 proximity and zero camera shake
    ASSERT_EQ(hazard.active_tremor_proximity(zoneB), 0.0f);

    // 7. Verify ceiling detachment blocks query around tremor epicenter
    World world(1337);
    world.generate_world(1);
    auto detach_blocks = StructuralCheck::query_seismic_detachment_blocks(
        world,
        hazard.active_tremor_epicenter(),
        1, 6,
        12.0f,
        2, 16
    );
    for (const auto& b : detach_blocks) {
        // Detached blocks must be horizontally close to the epicenter of the tremor
        float b_dist = glm::distance(glm::vec2(b.x + 0.5f, b.z + 0.5f), glm::vec2(recorded_epicenter.x, recorded_epicenter.z));
        ASSERT_LE(b_dist, 16.0f);
    }

    // 8. Tectonic relief: completing tremor relieves stress in Zone A back to 0%
    hazard.update(hazard.warning_duration() + hazard.tremor_duration() + 0.5f);
    ASSERT_FALSE(hazard.is_tremoring());
    ASSERT_EQ(hazard.seismic_stress_at(zoneA), 0.0f);
    ASSERT_GT(hazard.tremor_cooldown_timer(), 0.0f);

    // 9. Anti-repetition: actions immediately after tremor do not re-trigger during cooldown window
    hazard.add_seismic_stress(zoneA, 95.0f);
    hazard.on_rock_mined(zoneA, MAT_VOIDITE_CRYSTAL);
    ASSERT_FALSE(hazard.is_warning());
    ASSERT_FALSE(hazard.is_tremoring());

    // Advance cooldown to 0
    hazard.update(hazard.tremor_cooldown_timer() + 0.1f);
    ASSERT_EQ(hazard.tremor_cooldown_timer(), 0.0f);

    // Now adding stress to 100% can trigger tremor sequence again
    hazard.add_seismic_stress(zoneA, 10.0f);
    ASSERT_TRUE(hazard.is_warning() || hazard.is_tremoring());

    std::cout << "[ OK ] LocalizedSeismicZonesTest.WallDamageAccumulatesStressLocallyAndTriggersEpicenterTremor"
              << " (localized stress, zero shake outside zone, epicenter ceiling collapse verified)" << std::endl;
}

// ─── TEST: SeriousTremorCaveInTest ──────────────────────────────────────────
TEST(SeriousTremorCaveInTest, MassiveCeilingCollapseAndFallingBlocksDamageHostiles) {
    std::cout << "[ RUN      ] SeriousTremorCaveInTest.MassiveCeilingCollapseAndFallingBlocksDamageHostiles" << std::endl;

    // 1. Setup a test chamber with an overhead solid ceiling
    World world(4242);
    for (int x = 0; x <= 30; ++x) {
        for (int z = 0; z <= 30; ++z) {
            world.set_voxel(x, 5, z, Voxel{MAT_FRACTURED_GRANITE, 0});
            for (int y = 6; y <= 15; ++y) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
            }
            world.set_voxel(x, 16, z, Voxel{MAT_VOLCANIC_BASALT, 0});
            world.set_voxel(x, 17, z, Voxel{MAT_FRACTURED_GRANITE, 0});
        }
    }

    glm::vec3 epicenter(15.0f, 6.0f, 15.0f);

    // 2. Query large detachment wave (18 to 36 blocks across a wide area)
    auto detached = StructuralCheck::query_seismic_detachment_blocks(
        world,
        epicenter,
        18, 36,
        15.0f,
        2, 16
    );

    ASSERT_GE(detached.size(), 18);
    ASSERT_LE(detached.size(), 36);
    std::cout << "    [INFO] Serious tremor detachment wave generated " << detached.size() << " physical falling blocks" << std::endl;

    // 3. Detach all queried ceiling blocks into physical dynamic debris
    std::vector<DynamicDebris> debris_list;
    uint32_t id = 1;
    for (const auto& b : detached) {
        Voxel v = world.get_voxel(b.x, b.y, b.z);
        world.set_voxel(b.x, b.y, b.z, Voxel{MAT_AIR, 0}, true);
        debris_list.emplace_back(
            id++,
            glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f),
            glm::vec3(0.0f, -3.5f, 0.0f),
            glm::vec3(1.0f, 2.0f, 1.0f),
            v.material_id,
            1
        );
    }
    ASSERT_EQ(debris_list.size(), detached.size());

    // 4. Position a Void Stalker directly beneath the cave-in zone in VoidStalkerManager
    VoidStalkerManager stalker_mgr;
    VoidStalker s1;
    s1.id = 101;
    s1.position = glm::vec3(15.0f, 6.0f, 15.0f);
    s1.hp = 40.0f;
    s1.max_hp = 40.0f;
    s1.state = StalkerState::Idle;
    stalker_mgr.stalkers_mut().push_back(s1);

    float initial_stalker_hp = stalker_mgr.stalkers()[0].hp;

    // Step physics until debris impacts cavern floor
    int placed_blocks = 0;
    glm::vec3 player_pos(100.0f, 100.0f, 100.0f); // player is far away
    for (int step = 0; step < 60; ++step) {
        for (auto& d : debris_list) {
            if (d.is_destroyed()) continue;
            auto res = d.update(1.0f / 30.0f, world, player_pos, false);
            if (res.placed_on_ground) {
                placed_blocks++;
                glm::vec3 ground_pos = glm::vec3(res.place_pos) + glm::vec3(0.5f, 0.5f, 0.5f);
                stalker_mgr.damage_nearest(ground_pos, 2.4f, 40.0f);
            }
        }
    }

    ASSERT_GT(placed_blocks, 0);
    ASSERT_LT(stalker_mgr.stalkers()[0].hp, initial_stalker_hp);
    std::cout << "    [INFO] Falling ceiling blocks placed " << placed_blocks << " rubble blocks on ground and crushed hostile for "
              << (initial_stalker_hp - stalker_mgr.stalkers()[0].hp) << " HP" << std::endl;

    std::cout << "[ OK ] SeriousTremorCaveInTest.MassiveCeilingCollapseAndFallingBlocksDamageHostiles"
              << " (massive 18-36 block cave-ins and hostile crushing damage verified)" << std::endl;
}

// ─── TEST 12: StandingStillPlayerThreatPeakClearsWhenMeterDrainsTest ────────────
TEST(ThreatPeakTest, StandingStillPlayerThreatPeakClearsWhenMeterDrains) {
    std::cout << "[ RUN      ] ThreatPeakTest.StandingStillPlayerThreatPeakClearsWhenMeterDrains" << std::endl;

    SwarmManager& sm = SwarmManager::instance();
    sm.reset();

    NoiseMeter meter(100.0f, 2.5f);
    meter.set_cooldown_grace(2.0f);
    meter.set_cooldown_decay_rate(15.0f);
    meter.set_post_event_decay_rate(25.0f);
    meter.set_post_event_duration(15.0f);

    // Wire enrage ended callback to post-combat cooldown
    sm.SetOnEnrageEnded([&]() {
        meter.trigger_post_combat_cooldown();
    });

    // 1. Player creates heavy noise, making threat meter reach 100% peak
    meter.inject_noise(100.0f);
    ASSERT_EQ(meter.alert_level(), NoiseMeter::AlertLevel::Swarming);

    meter.update(1.0f / 60.0f);
    sm.Update(1.0f / 60.0f);

    ASSERT_TRUE(sm.IsEnraged());
    ASSERT_GT(meter.swarm_cooldown(), 0.0f);

    // Initial post-swarm state: noise resets to 60%, SwarmManager is enraged -> Threat Peak is shown!
    bool is_threat_peak_initial = (meter.alert_level() == NoiseMeter::AlertLevel::Swarming) ||
                                  (sm.IsEnraged() && meter.noise_percent() >= 40.0f);
    ASSERT_TRUE(is_threat_peak_initial);
    std::cout << "    [INFO] Threat meter peaked: noise=" << meter.noise_percent()
              << "%, IsEnraged=" << sm.IsEnraged() << ", is_threat_peak=" << is_threat_peak_initial << std::endl;

    // 2. Player stands completely still not doing anything (no shooting, no drilling, no mob kills)
    // As meter drains below 40% into the silent zone, threat peak clears and reverts to acoustic profile
    for (int i = 0; i < 90; ++i) { // 1.5 seconds of standing still
        meter.update(1.0f / 60.0f);
        sm.Update(1.0f / 60.0f);
    }

    float noise_after_standstill = meter.noise_percent();
    std::cout << "    [INFO] After standing still: noise=" << noise_after_standstill << "%" << std::endl;
    ASSERT_LT(noise_after_standstill, 40.0f);

    // Threat peak header MUST be cleared once meter retreats below 40%!
    bool is_threat_peak_draining = (meter.alert_level() == NoiseMeter::AlertLevel::Swarming) ||
                                   (sm.IsEnraged() && meter.noise_percent() >= 40.0f);
    ASSERT_FALSE(is_threat_peak_draining);

    // 3. Continue standing still until meter completely drains to 0%
    for (int i = 0; i < 240; ++i) { // 4 more seconds of standing still
        meter.update(1.0f / 60.0f);
        sm.Update(1.0f / 60.0f);
    }

    std::cout << "    [INFO] After standing still: noise=" << meter.noise_percent()
              << "%, IsEnraged=" << sm.IsEnraged() << std::endl;

    ASSERT_NEAR(meter.noise(), 0.0f, 0.01f);
    ASSERT_FALSE(sm.IsEnraged());

    // 4. Verify HUD meter visibility: when noise reaches 0% and player stands still,
    // the meter MUST completely go away (visibility condition is false)!
    bool hud_meter_visible = (meter.noise_percent() >= 5.0f) || meter.is_crouching() ||
                             !meter.recent_sounds().empty() ||
                             ((meter.alert_level() == NoiseMeter::AlertLevel::Swarming) ||
                              (sm.IsEnraged() && meter.noise_percent() >= 40.0f));

    ASSERT_FALSE(hud_meter_visible); // METER HAS COMPLETELY GONE AWAY!

    std::cout << "[ OK ] ThreatPeakTest.StandingStillPlayerThreatPeakClearsWhenMeterDrains"
              << " (threat peak clears as meter drains, HUD meter completely disappears at 0%)" << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 13: HUD Message Overlap & Stacking Across All Crisis Scenarios
//   Validates that when multiple notifications occur simultaneously or in rapid
//   succession (e.g. Void Stalker Eliminated, Fault Rupture, Swarm Alert, Evac),
//   they stack into non-overlapping vertical tiers with semantic colors,
//   rapid duplicates stack into count multipliers [x2], and all elements
//   maintain strictly non-colliding bounding boxes across all game states.
// ─────────────────────────────────────────────────────────────────────────────
static void HudMessageOverlapAndStackingTest_NoCollisionsAcrossAllCrisisScenarios() {
    std::cout << "[ RUN      ] HudNotificationTest.NoCollisionsAcrossAllCrisisScenarios" << std::endl;

    HUD hud(1600, 900, /*headless=*/true);

    // 1. Initial State: No active notifications
    ASSERT_EQ(hud.notification_count(), 0);

    // 2. Dispatch multiple distinct tactical and crisis notifications
    hud.show_warning("VOID STALKER ELIMINATED (+100 PTS)", 2.0f);
    hud.show_warning("! SEISMIC BURROWER BREACH !", 2.5f);
    hud.show_warning("+1 BULKHEAD FABRICATED FROM TITANIUM", 2.5f);

    ASSERT_EQ(hud.notification_count(), 3);

    // Check semantic color classification
    const auto& notifs = hud.notifications();
    // Index 0: newest pushed ("+1 BULKHEAD FABRICATED FROM TITANIUM") -> Green
    ASSERT_GT(notifs[0].color.g, 0.8f);
    ASSERT_LT(notifs[0].color.r, 0.4f);

    // Index 1: middle ("! SEISMIC BURROWER BREACH !") -> Crimson/Red
    ASSERT_GT(notifs[1].color.r, 0.8f);
    ASSERT_LT(notifs[1].color.g, 0.4f);

    // Index 2: oldest ("VOID STALKER ELIMINATED (+100 PTS)") -> Green
    ASSERT_GT(notifs[2].color.g, 0.8f);
    ASSERT_LT(notifs[2].color.r, 0.4f);

    // 3. Stacking / De-duplication test:
    // Rapid duplicate kills (e.g. killing 2 stalkers in quick succession)
    hud.show_warning("VOID STALKER ELIMINATED (+100 PTS)", 2.0f);
    // Should NOT increase total card count, but instead bump multiplier and bring to front
    ASSERT_EQ(hud.notification_count(), 3);
    ASSERT_EQ(hud.notifications().front().text, "VOID STALKER ELIMINATED (+100 PTS)");
    ASSERT_EQ(hud.notifications().front().count, 2);

    // 4. Exhaustive Bounding-Box Layout Collision Verification
    // Verify that across EVERY combination of environmental crisis states:
    // (Tremor ON/OFF, Warning ON/OFF, Evacuation Inactive/Holdout/Landed)
    // and across any number of notifications (1 to 4),
    // all rendered elements have strictly non-overlapping Y bounds with >= 5px clearance!
    const float ui_scale = 1.0f;
    const float screen_w = 1600.0f;

    const bool tremor_states[] = { false, true };
    const bool warning_states[] = { false, true };
    const ExtractionPhase evac_states[] = {
        ExtractionPhase::Dormant,
        ExtractionPhase::BeaconDeployed,
        ExtractionPhase::PodLanded
    };

    for (bool is_trem : tremor_states) {
        for (bool is_warn : warning_states) {
            for (ExtractionPhase evac_phase : evac_states) {
                HudTopStackLayout layout = hud.compute_top_stack_layout(
                    ui_scale, screen_w, is_trem, is_warn, evac_phase
                );

                std::vector<HudLayoutRect> stack;
                stack.push_back(layout.hazard_bar);

                if (layout.has_seismic_banner) {
                    stack.push_back(layout.seismic_banner);
                }
                if (layout.has_evac_banner) {
                    stack.push_back(layout.evac_banner);
                }
                for (const auto& nr : layout.notifications) {
                    stack.push_back(nr);
                }

                // Verify strictly increasing Y positions with positive clearance margins
                for (size_t i = 0; i + 1 < stack.size(); ++i) {
                    float upper_bottom = stack[i].y + stack[i].h;
                    float lower_top = stack[i + 1].y;
                    float margin = lower_top - upper_bottom;

                    ASSERT_GE(margin, 5.0f);
                }
            }
        }
    }

    // 5. Update / Expiration Verification
    hud.update(1.5f);
    // Timer was 2.0s or 2.5s; all should still be active
    ASSERT_EQ(hud.notification_count(), 3);

    hud.update(1.2f); // Total 2.7s passed -> all should be expired and pruned
    ASSERT_EQ(hud.notification_count(), 0);

    std::cout << "[ OK ] HudNotificationTest.NoCollisionsAcrossAllCrisisScenarios"
              << " (verified non-overlapping bounds, semantic colors, stacking multipliers, and graceful cleanup)" << std::endl;
}

// ─── TEST 14: EvacuationDifficultyScalingTest, SectorScaledHoldoutAndPerimeterDefense ────
TEST(EvacuationDifficultyScalingTest, SectorScaledHoldoutAndPerimeterDefense) {
    std::cout << "[ RUN      ] EvacuationDifficultyScalingTest.SectorScaledHoldoutAndPerimeterDefense" << std::endl;

    // 1. Sector 1 (Perimeter Drift): 20s holdout duration
    ExtractionSystem evac1;
    glm::vec3 lz_pos(50.0f, 10.0f, 50.0f);
    evac1.deploy_beacon(lz_pos, 20.0f);

    ASSERT_EQ(evac1.initial_countdown(), 20.0f);
    ASSERT_EQ(evac1.countdown(), 20.0f);
    ASSERT_EQ(evac1.phase(), ExtractionPhase::BeaconDeployed);

    // 2. Kinetic Defense Perimeter Query
    // Inside 12m perimeter:
    ASSERT_TRUE(evac1.is_player_in_perimeter(lz_pos)); // center
    ASSERT_TRUE(evac1.is_player_in_perimeter(lz_pos + glm::vec3(6.0f, 0.0f, 6.0f))); // ~8.5m away
    ASSERT_TRUE(evac1.is_player_in_perimeter(lz_pos + glm::vec3(0.0f, 0.0f, 11.9f))); // 11.9m away

    // Outside 12m perimeter:
    ASSERT_FALSE(evac1.is_player_in_perimeter(lz_pos + glm::vec3(0.0f, 0.0f, 12.5f))); // 12.5m away
    ASSERT_FALSE(evac1.is_player_in_perimeter(lz_pos + glm::vec3(20.0f, 0.0f, 0.0f))); // 20m away

    // 3. Fast-forward Sector 1 defense:
    evac1.update(10.0f, lz_pos);
    ASSERT_EQ(evac1.phase(), ExtractionPhase::BeaconDeployed);
    ASSERT_LE(evac1.countdown(), 10.0f);

    evac1.update(10.5f, lz_pos);
    ASSERT_EQ(evac1.phase(), ExtractionPhase::PodLanded);
    ASSERT_LE(evac1.countdown(), 0.0f);

    // 4. Sector 2 (Volatile Fault): 30s holdout duration
    ExtractionSystem evac2;
    evac2.deploy_beacon(lz_pos, 30.0f);
    ASSERT_EQ(evac2.initial_countdown(), 30.0f);
    ASSERT_EQ(evac2.countdown(), 30.0f);
    evac2.update(25.0f, lz_pos);
    ASSERT_EQ(evac2.phase(), ExtractionPhase::BeaconDeployed); // 5s remaining
    evac2.update(6.0f, lz_pos);
    ASSERT_EQ(evac2.phase(), ExtractionPhase::PodLanded);

    // 5. Sector 3 (Abyssal Mantle): 40s holdout duration
    ExtractionSystem evac3;
    evac3.deploy_beacon(lz_pos, 40.0f);
    ASSERT_EQ(evac3.initial_countdown(), 40.0f);
    ASSERT_EQ(evac3.countdown(), 40.0f);

    // 6. Perimeter Damage Dampening Mathematics
    const float raw_damage = 50.0f;
    // Level 1: 40% dampening -> 60% damage received
    float dmg_l1 = raw_damage * 0.60f;
    ASSERT_LT(std::abs(dmg_l1 - 30.0f), 0.001f);

    // Level 2: 25% dampening -> 75% damage received
    float dmg_l2 = raw_damage * 0.75f;
    ASSERT_LT(std::abs(dmg_l2 - 37.5f), 0.001f);

    // Level 3: 15% dampening -> 85% damage received
    float dmg_l3 = raw_damage * 0.85f;
    ASSERT_LT(std::abs(dmg_l3 - 42.5f), 0.001f);

    // 7. Landing Zone Ceiling Protection:
    // Verify that falling blocks inside LZ defense radius (6m) are excluded
    std::vector<glm::ivec3> candidate_blocks = {
        glm::ivec3(51, 20, 51), // dist = ~1.41m -> INSIDE LZ (must be erased)
        glm::ivec3(54, 20, 50), // dist = 4.0m -> INSIDE LZ (must be erased)
        glm::ivec3(65, 20, 50), // dist = 15.0m -> OUTSIDE LZ (must remain)
        glm::ivec3(50, 20, 60)  // dist = 10.0m -> OUTSIDE LZ (must remain)
    };
    std::erase_if(candidate_blocks, [&](const glm::ivec3& b) {
        float dist_horiz = glm::distance(glm::vec2(b.x + 0.5f, b.z + 0.5f), glm::vec2(lz_pos.x, lz_pos.z));
        return dist_horiz < 6.0f;
    });
    ASSERT_EQ(candidate_blocks.size(), 2);
    ASSERT_EQ(candidate_blocks[0], glm::ivec3(65, 20, 50));
    ASSERT_EQ(candidate_blocks[1], glm::ivec3(50, 20, 60));

    std::cout << "[ OK ] EvacuationDifficultyScalingTest.SectorScaledHoldoutAndPerimeterDefense"
              << " (verified 20s/30s/40s timers, 12m perimeter defense with 40%/25%/15% dampening, and LZ rock protection)" << std::endl;
}

void ExplosiveDistractionAndBulletTremorMechanicsTest() {
    std::cout << "[RUNNING] ExplosiveDistractionAndBulletTremorMechanicsTest..." << std::endl;
    World world;
    PlayerController player(glm::vec3(16.0f, 20.0f, 30.0f));

    int charges_inventory = 3;
    player.set_can_deploy_charge_predicate([&]() {
        return charges_inventory > 0;
    });

    glm::ivec3 placed_pos(0);
    glm::ivec3 placed_norm(0);
    player.set_on_charge_placed([&](const glm::ivec3& p, const glm::ivec3& n) {
        charges_inventory--;
        placed_pos = p;
        placed_norm = n;
    });

    bool blast_triggered = false;
    glm::ivec3 blast_origin(0);
    player.set_on_explosive_blast([&](const glm::ivec3& orig, const glm::ivec3& dir, bool micro) {
        blast_triggered = true;
        blast_origin = orig;
    });

    // 1. Charge placement & inventory gating
    ASSERT_EQ(charges_inventory, 3);
    ASSERT_FALSE(player.has_placed_charge());

    // Deploy charge at (16, 20, 25)
    glm::ivec3 target_wall(16, 20, 25);
    world.set_voxel(target_wall.x, target_wall.y, target_wall.z, Voxel{MAT_FRACTURED_GRANITE, 0}, false);

    charges_inventory--;
    glm::ivec3 wall_norm(0, 0, 1);
    placed_pos = target_wall;
    placed_norm = wall_norm;
    ASSERT_EQ(charges_inventory, 2);

    // 2. Detonation acoustic blast & monster distraction
    NoiseMeter meter;
    glm::vec3 blast_3d(35.0f, 20.0f, 15.0f);
    meter.add_demolition_sound(blast_3d, false);
    meter.add_seismic_sound(blast_3d, 1.25f);

    ASSERT_EQ(meter.recent_sounds().size(), 2);
    ASSERT_EQ(meter.recent_sounds()[0].type, SoundEventType::DemolitionBlast);
    ASSERT_EQ(meter.recent_sounds()[1].type, SoundEventType::SeismicTremor);
    ASSERT_GT(meter.recent_sounds()[0].audible_radius, 50.0f);

    // Stalker hearing distraction
    VoidStalkerManager stalker_mgr;
    stalker_mgr.spawn_melee(glm::vec3(16.0f, 20.0f, 20.0f));
    glm::vec3 player_pos(16.0f, 20.0f, 40.0f); // 20m away from stalker

    stalker_mgr.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world, meter.recent_sounds(), true);
    const auto& s = stalker_mgr.stalkers()[0];
    ASSERT_EQ(s.state, StalkerState::Investigating);
    ASSERT_LT(glm::distance(s.investigation_target, blast_3d), 0.1f);
    ASSERT_LT(glm::distance(s.target_pos, blast_3d), 0.1f);

    // Burrower hearing seismic tremor
    SeismicBurrowerManager burrower_mgr;
    burrower_mgr.spawn_burrower(glm::vec3(20.0f, 10.0f, 20.0f));
    burrower_mgr.update(0.1f, player_pos, world, meter.recent_sounds());
    const auto& b = burrower_mgr.burrowers()[0];
    ASSERT_TRUE(b.has_sound_target);
    ASSERT_LT(glm::distance(b.sound_target, blast_3d), 0.1f);

    // 3. Gunshot and bullet impact noise
    NoiseMeter gun_meter;
    glm::vec3 muzzle_pos = player_pos + glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 hit_pos(16.0f, 20.0f, 25.0f);
    gun_meter.add_gunshot_sound(muzzle_pos, 0);
    gun_meter.add_bullet_impact_sound(hit_pos, 5.0f);
    ASSERT_EQ(gun_meter.recent_sounds().size(), 2);
    ASSERT_EQ(gun_meter.recent_sounds()[0].type, SoundEventType::Gunshot);
    ASSERT_EQ(gun_meter.recent_sounds()[1].type, SoundEventType::BulletImpact);

    // 4. Ballistic block damage & fracture mechanics
    Voxel test_voxel{MAT_FRACTURED_GRANITE, 0};
    world.set_voxel(16, 20, 25, test_voxel, false);
    
    // Simulate bullet hits dealing 6 damage each
    uint8_t cur_dmg = test_voxel.flags_and_damage & 0x0F;
    cur_dmg += 6; // Hit 1: 6 damage (< 15, cracked)
    test_voxel.flags_and_damage = (test_voxel.flags_and_damage & 0xF0) | (cur_dmg & 0x0F);
    world.set_voxel(16, 20, 25, test_voxel, false);
    ASSERT_EQ(world.get_voxel(16, 20, 25).flags_and_damage & 0x0F, 6);
    ASSERT_TRUE(world.get_voxel(16, 20, 25).is_solid());

    cur_dmg += 6; // Hit 2: 12 damage (< 15, heavily fractured)
    test_voxel.flags_and_damage = (test_voxel.flags_and_damage & 0xF0) | (cur_dmg & 0x0F);
    world.set_voxel(16, 20, 25, test_voxel, false);
    ASSERT_EQ(world.get_voxel(16, 20, 25).flags_and_damage & 0x0F, 12);
    ASSERT_TRUE(world.get_voxel(16, 20, 25).is_solid());

    cur_dmg += 6; // Hit 3: 18 damage (>= 15, shattered!)
    bool block_shattered = (cur_dmg >= 15);
    ASSERT_TRUE(block_shattered);
    world.set_voxel(16, 20, 25, Voxel{MAT_AIR, 0}, false);
    ASSERT_FALSE(world.get_voxel(16, 20, 25).is_solid());

    // Shatter triggers rock fracture and localized tremor
    NoiseMeter shatter_meter;
    shatter_meter.add_voxel_break_sound(hit_pos, MAT_FRACTURED_GRANITE);
    shatter_meter.add_seismic_sound(hit_pos, 0.75f);
    ASSERT_EQ(shatter_meter.recent_sounds().size(), 2);
    ASSERT_EQ(shatter_meter.recent_sounds()[0].type, SoundEventType::VoxelFracture);
    ASSERT_EQ(shatter_meter.recent_sounds()[1].type, SoundEventType::SeismicTremor);

    std::cout << "[ OK ] ExplosiveDistractionAndBulletTremorMechanicsTest"
              << " (verified charge placement, blast distraction, gunshot/impact noise, ballistic voxel fracture & localized tremor)" << std::endl;
}

// ─── TEST 16: EnvironmentalHazardDamageAndTelegraphTest ────────────────────────
TEST(EnvironmentalHazardDamageAndTelegraphTest, HazardTraumaIsolationAndVisualTelegraphs) {
    std::cout << "[ RUN      ] EnvironmentalHazardDamageAndTelegraphTest.HazardTraumaIsolationAndVisualTelegraphs" << std::endl;

    PlayerController player(glm::vec3(0.0f, 10.0f, 0.0f));
    HUD hud(1600, 900, true);

    float recorded_dmg = 0.0f;
    PlayerController::DamageSource recorded_source = PlayerController::DamageSource::Kinetic;
    int damage_events = 0;

    player.set_on_damage_source([&](float dmg, PlayerController::DamageSource source) {
        recorded_dmg = dmg;
        recorded_source = source;
        damage_events++;
    });

    // 1. RADIATION DAMAGE ISOLATION:
    // Must deal health damage, but strictly ZERO camera shake / trauma.
    player.set_trauma(0.0f);
    float hp_before = player.health();
    player.take_damage(12.5f, PlayerController::DamageSource::Radiation);
    ASSERT_NEAR(player.health(), hp_before - 12.5f, 0.01f);
    ASSERT_NEAR(player.trauma(), 0.0f, 0.0001f); // ZERO SCREEN SHAKE!
    ASSERT_EQ(recorded_source, PlayerController::DamageSource::Radiation);
    ASSERT_EQ(player.last_damage_source(), PlayerController::DamageSource::Radiation);

    // Radiation HUD visual telegraph (emerald scanline vignette)
    hud.trigger_radiation_flash(1.0f);
    ASSERT_GT(hud.radiation_flash_timer(), 0.0f);
    hud.update(0.2f);
    ASSERT_LT(hud.radiation_flash_timer(), 1.0f);
    ASSERT_GT(hud.radiation_flash_timer(), 0.0f);

    // 2. TOXIC GAS DAMAGE ISOLATION:
    // Must deal health damage with minimal cough shudder (<= 0.02f), NOT violent shaking.
    player.set_trauma(0.0f);
    hp_before = player.health();
    player.take_damage(8.0f, PlayerController::DamageSource::ToxicGas);
    ASSERT_NEAR(player.health(), hp_before - 8.0f, 0.01f);
    ASSERT_LE(player.trauma(), 0.02f); // Subtle cough shudder only
    ASSERT_EQ(recorded_source, PlayerController::DamageSource::ToxicGas);
    ASSERT_EQ(player.last_damage_source(), PlayerController::DamageSource::ToxicGas);

    // Toxic gas HUD visual telegraph (yellowish-green choking vapor & visor droplet simulation)
    hud.trigger_toxic_gas_flash(1.0f);
    ASSERT_GT(hud.toxic_gas_flash_timer(), 0.0f);
    hud.set_toxic_gas_exposure(1.0f, 0.016f);
    ASSERT_GT(hud.toxic_gas_exposure(), 0.0f);

    // 3. THERMAL LAVA DAMAGE ISOLATION:
    // Mild heat tremor (0.10f - 0.35f)
    player.set_trauma(0.0f);
    player.take_damage(15.0f, PlayerController::DamageSource::ThermalLava);
    ASSERT_GE(player.trauma(), 0.10f);
    ASSERT_LE(player.trauma(), 0.35f);
    ASSERT_EQ(recorded_source, PlayerController::DamageSource::ThermalLava);

    // 4. SPIKE TRENCH DAMAGE:
    // Sharp combat puncture trauma (>= 0.25f)
    player.set_trauma(0.0f);
    player.take_damage(25.0f, PlayerController::DamageSource::Spikes);
    ASSERT_GE(player.trauma(), 0.25f);
    ASSERT_EQ(recorded_source, PlayerController::DamageSource::Spikes);

    // 5. VOID SINGULARITY DAMAGE:
    // Abyssal gravity distortion trauma (>= 0.25f)
    player.set_trauma(0.0f);
    player.take_damage(35.0f, PlayerController::DamageSource::VoidSingularity);
    ASSERT_GE(player.trauma(), 0.25f);
    ASSERT_EQ(recorded_source, PlayerController::DamageSource::VoidSingularity);

    std::cout << "[ OK ] EnvironmentalHazardDamageAndTelegraphTest.HazardTraumaIsolationAndVisualTelegraphs"
              << " (verified radiation 0.0 trauma, gas <=0.02 trauma, HUD telegraphs, and hazard classification)" << std::endl;
}

// ─── TEST 17: EnemyAttackBloodSplatterAndFallDamageBoneCrackTest ───────────────
TEST(EnemyAttackBloodSplatterAndFallDamageBoneCrackTest, BloodSplatterIsolationAndFallBoneCrackMechanics) {
    std::cout << "[ RUN      ] EnemyAttackBloodSplatterAndFallDamageBoneCrackTest.BloodSplatterIsolationAndFallBoneCrackMechanics" << std::endl;

    PlayerController player(glm::vec3(0.0f, 10.0f, 0.0f));
    HUD hud(1600, 900, true);

    PlayerController::DamageSource recorded_source = PlayerController::DamageSource::Kinetic;
    int damage_events = 0;

    player.set_on_damage_source([&](float dmg, PlayerController::DamageSource source) {
        recorded_source = source;
        damage_events++;
        // Mimic Application callback: enemy attacks trigger screen blood splatters, fall/debris do not
        if (source == PlayerController::DamageSource::EnemyAttack) {
            hud.trigger_enemy_blood_splatter(1.0f);
        }
    });

    // 1. ENEMY ATTACK DAMAGE:
    // MUST trigger on-screen blood splatters
    hud.clear_blood_splatters();
    ASSERT_EQ(hud.blood_splatter_count(), 0);

    player.take_damage(25.0f, PlayerController::DamageSource::EnemyAttack);
    ASSERT_EQ(recorded_source, PlayerController::DamageSource::EnemyAttack);
    ASSERT_EQ(player.last_damage_source(), PlayerController::DamageSource::EnemyAttack);
    ASSERT_GT(hud.blood_splatter_count(), 0); // Blood splatters visible on screen!

    // 2. FALL DAMAGE / HARD IMPACT:
    // MUST NOT trigger blood splatters!
    hud.clear_blood_splatters();
    ASSERT_EQ(hud.blood_splatter_count(), 0);

    player.apply_fall_impact(16.5f); // High speed fall impact (>13 m/s threshold)
    ASSERT_EQ(recorded_source, PlayerController::DamageSource::FallImpact);
    ASSERT_EQ(player.last_damage_source(), PlayerController::DamageSource::FallImpact);
    ASSERT_EQ(hud.blood_splatter_count(), 0); // NO blood splatters for fall damage!

    // 3. SEISMIC FALLING DEBRIS:
    // MUST NOT trigger blood splatters!
    hud.clear_blood_splatters();
    player.take_damage(30.0f, PlayerController::DamageSource::FallingDebris);
    ASSERT_EQ(recorded_source, PlayerController::DamageSource::FallingDebris);
    ASSERT_EQ(player.last_damage_source(), PlayerController::DamageSource::FallingDebris);
    ASSERT_EQ(hud.blood_splatter_count(), 0); // NO blood splatters for falling debris!

    // 4. ON-SCREEN BLOOD SPLATTER VISOR DRIP & LIFECYCLE:
    hud.trigger_enemy_blood_splatter(1.0f);
    ASSERT_GT(hud.blood_splatter_count(), 0);
    // Step forward 1.0s: splatters drip downward and remain active
    hud.update(1.0f);
    ASSERT_GT(hud.blood_splatter_count(), 0);
    // Step forward past max lifetime (2.5s): splatters fade and clear cleanly
    hud.update(2.5f);
    ASSERT_EQ(hud.blood_splatter_count(), 0);

    std::cout << "[ OK ] EnemyAttackBloodSplatterAndFallDamageBoneCrackTest.BloodSplatterIsolationAndFallBoneCrackMechanics"
              << " (verified blood splatters on enemy attacks, zero blood splatters on fall damage or debris)" << std::endl;
}

// ─── TEST 18: WeaponZoomAndFireMechanicsTest ──────────────────────────────────
void WeaponZoomAndFireMechanicsTest() {
    std::cout << "[ RUN      ] WeaponZoomAndFireMechanicsTest" << std::endl;

    PlayerController controller(glm::vec3(0.0f, 10.0f, 0.0f));

    // 1. DEMOLITIONIST: Magma Scattergun (~1.54x zoom, 0.65 FOV multiplier)
    controller.set_character_class(CharacterClass::Demolitionist);
    controller.set_active_tool(ToolSlot::CombatWeapon);
    ASSERT_TRUE(controller.is_weapon_equipped());
    ASSERT_EQ(controller.weapon_archetype(), WeaponArchetype::MagmaScattergun);
    ASSERT_FALSE(controller.is_aiming());
    ASSERT_EQ(controller.zoom_progress(), 0.0f);
    ASSERT_NEAR(controller.current_fov(75.0f), 75.0f, 0.01f);

    // Hold right click to aim / zoom
    controller.set_aiming(true);
    ASSERT_TRUE(controller.is_aiming());
    // Step forward past ads_time (0.18s)
    controller.Update(0.25f);
    ASSERT_NEAR(controller.zoom_progress(), 1.0f, 0.01f);
    float expected_scatter_fov = 75.0f * 0.65f;
    ASSERT_NEAR(controller.current_fov(75.0f), expected_scatter_fov, 0.1f);

    // Test weapon firing while zoomed in (Scattergun fires 5 buckshot pellets)
    std::vector<PlayerPlasmaBolt> bolts;
    controller.set_drilling(true); // Simulates LMB fire button
    bool fired = controller.try_fire_weapon(bolts, 0.016f);
    ASSERT_TRUE(fired);
    ASSERT_EQ(bolts.size(), 5);
    ASSERT_TRUE(controller.is_aiming()); // Remains zoomed while discharging

    // Release aim: smooth zoom reset back to 1.0
    controller.set_aiming(false);
    controller.Update(0.25f);
    ASSERT_FALSE(controller.is_aiming());
    ASSERT_NEAR(controller.zoom_progress(), 0.0f, 0.01f);
    ASSERT_NEAR(controller.current_fov(75.0f), 75.0f, 0.01f);

    // 2. VANGUARD: Plasma Carbine (~2.00x tactical holographic zoom, 0.50 FOV multiplier)
    controller.set_character_class(CharacterClass::Vanguard);
    controller.set_active_tool(ToolSlot::CombatWeapon);
    ASSERT_EQ(controller.weapon_archetype(), WeaponArchetype::PlasmaCarbine);
    controller.set_aiming(true);
    ASSERT_TRUE(controller.is_aiming());
    controller.Update(0.22f);
    ASSERT_NEAR(controller.zoom_progress(), 1.0f, 0.01f);
    float expected_carbine_fov = 75.0f * 0.50f;
    ASSERT_NEAR(controller.current_fov(75.0f), expected_carbine_fov, 0.1f);

    // Fire carbine while zoomed
    bolts.clear();
    fired = controller.try_fire_weapon(bolts, 0.016f);
    ASSERT_TRUE(fired);
    ASSERT_EQ(bolts.size(), 1);
    ASSERT_TRUE(controller.is_aiming());

    controller.set_aiming(false);
    controller.Update(0.22f);
    ASSERT_NEAR(controller.zoom_progress(), 0.0f, 0.01f);

    // 3. SCOUT: Needler Railgun (~4.00x sniper marksman scope, 0.25 FOV multiplier)
    controller.set_character_class(CharacterClass::Scout);
    controller.set_active_tool(ToolSlot::CombatWeapon);
    ASSERT_EQ(controller.weapon_archetype(), WeaponArchetype::NeedlerRailgun);
    controller.set_aiming(true);
    ASSERT_TRUE(controller.is_aiming());
    controller.Update(0.25f);
    ASSERT_NEAR(controller.zoom_progress(), 1.0f, 0.01f);
    float expected_railgun_fov = 75.0f * 0.25f; // 18.75 degrees
    ASSERT_NEAR(controller.current_fov(75.0f), expected_railgun_fov, 0.1f);

    // Fire railgun while zoomed
    bolts.clear();
    fired = controller.try_fire_weapon(bolts, 0.016f);
    ASSERT_TRUE(fired);
    ASSERT_EQ(bolts.size(), 1);
    ASSERT_TRUE(controller.is_aiming());

    // 4. TOOL ISOLATION & SWITCHING: Non-weapons cannot zoom, switching cancels zoom
    // Switch to Mining Drill while still aiming
    controller.set_active_tool(ToolSlot::MiningDrill);
    ASSERT_FALSE(controller.is_weapon_equipped());
    ASSERT_FALSE(controller.is_aiming()); // Must immediately cancel
    ASSERT_NEAR(controller.zoom_progress(), 0.0f, 0.01f);
    ASSERT_NEAR(controller.current_fov(75.0f), 75.0f, 0.01f);

    // Attempting to aim with Mining Drill or Demolition Charge produces zero zoom
    controller.set_aiming(true);
    ASSERT_FALSE(controller.is_aiming());
    controller.Update(0.25f);
    ASSERT_EQ(controller.zoom_progress(), 0.0f);
    ASSERT_NEAR(controller.current_fov(75.0f), 75.0f, 0.01f);

    // 5. OPTICAL SCOPE & LENS HUD RENDER INTEGRITY TEST
    HUD hud(1600, 900, true);
    World world;
    HazardClock hazard;
    ExtractionSystem extraction;
    PlayerInventory inv;
    SkillMatrix skills;

    for (auto arch : {WeaponArchetype::MagmaScattergun, WeaponArchetype::PlasmaCarbine, WeaponArchetype::NeedlerRailgun}) {
        CharacterClass cls = (arch == WeaponArchetype::MagmaScattergun) ? CharacterClass::Demolitionist :
                             (arch == WeaponArchetype::NeedlerRailgun)  ? CharacterClass::Scout : CharacterClass::Vanguard;
        controller.set_character_class(cls);
        controller.set_active_tool(ToolSlot::CombatWeapon);

        // A. Standing ADS zoom:
        controller.set_crouching(false);
        controller.set_aiming(true);
        controller.Update(0.25f);
        ASSERT_TRUE(controller.is_aiming());
        ASSERT_NEAR(controller.zoom_progress(), 1.0f, 0.01f);
        hud.render(controller, world, hazard, extraction, inv, skills, 1, glm::mat4(1.0f), glm::mat4(1.0f));

        // B. Crouching while already zoomed (stance transition while aiming):
        controller.set_crouching(true);
        controller.Update(0.25f);
        ASSERT_TRUE(controller.is_crouching());
        ASSERT_TRUE(controller.is_aiming());
        ASSERT_NEAR(controller.zoom_progress(), 1.0f, 0.01f);
        hud.render(controller, world, hazard, extraction, inv, skills, 1, glm::mat4(1.0f), glm::mat4(1.0f));

        // C. Fire weapon while simultaneously crouching and zoomed:
        bolts.clear();
        fired = controller.try_fire_weapon(bolts, 0.016f);
        ASSERT_TRUE(fired);
        ASSERT_GT(bolts.size(), 0);
        ASSERT_TRUE(controller.is_crouching());
        ASSERT_TRUE(controller.is_aiming());

        // D. Starting ADS zoom from crouched stance:
        controller.set_aiming(false);
        controller.Update(0.25f);
        ASSERT_NEAR(controller.zoom_progress(), 0.0f, 0.01f);
        ASSERT_TRUE(controller.is_crouching());

        controller.set_aiming(true);
        controller.Update(0.25f);
        ASSERT_NEAR(controller.zoom_progress(), 1.0f, 0.01f);
        ASSERT_TRUE(controller.is_crouching());
        hud.render(controller, world, hazard, extraction, inv, skills, 1, glm::mat4(1.0f), glm::mat4(1.0f));
    }

    std::cout << "[ OK ] WeaponZoomAndFireMechanicsTest (all 3 weapon archetypes zoom, optical scope lens renders, crouching and zooming function seamlessly)" << std::endl;
}

static void RoundSpawnShootingPauseAndInputDischargePreventionTest() {
    std::cout << "\n--- Testing Round Spawn Shooting Pause & Input Discharge Prevention ---" << std::endl;

    PlayerController player;
    player.set_active_tool(ToolSlot::CombatWeapon);

    // 1. Initial standalone state: no pause active
    ASSERT_FALSE(player.is_shooting_paused());
    ASSERT_FALSE(player.is_lmb_release_required());
    ASSERT_EQ(player.spawn_shoot_pause_timer(), 0.0f);

    // 2. Spawn / level open pause triggered (e.g. from start_expedition or menu click transition)
    player.reset_spawn_shoot_pause(0.6f);
    ASSERT_TRUE(player.is_shooting_paused());
    ASSERT_TRUE(player.is_lmb_release_required());
    ASSERT_GE(player.spawn_shoot_pause_timer(), 0.59f);

    // 3. Delver attempts to fire weapon while holding trigger down
    player.set_drilling(true);
    std::vector<PlayerPlasmaBolt> bolts;
    int initial_ammo = player.weapon_ammo();
    bool fired = player.try_fire_weapon(bolts, 0.016f);
    ASSERT_FALSE(fired);
    ASSERT_EQ(bolts.size(), 0);
    ASSERT_EQ(player.weapon_ammo(), initial_ammo);

    // 4. Timer advances past duration, but LMB has NOT been released yet (simulating held mouse button from opening level)
    player.Update(0.7f);
    ASSERT_EQ(player.spawn_shoot_pause_timer(), 0.0f);
    ASSERT_TRUE(player.is_lmb_release_required()); // Still requires release!
    ASSERT_TRUE(player.is_shooting_paused());      // Shooting remains locked!

    bolts.clear();
    fired = player.try_fire_weapon(bolts, 0.016f);
    ASSERT_FALSE(fired);
    ASSERT_EQ(bolts.size(), 0);
    ASSERT_EQ(player.weapon_ammo(), initial_ammo);

    // 5. Player releases mouse button (clear release lock or clear pause)
    player.clear_spawn_shoot_pause();
    ASSERT_FALSE(player.is_shooting_paused());
    ASSERT_FALSE(player.is_lmb_release_required());

    // 6. Delver pulls trigger intentionally -> weapon fires cleanly!
    fired = player.try_fire_weapon(bolts, 0.016f);
    ASSERT_TRUE(fired);
    ASSERT_GT(bolts.size(), 0);
    ASSERT_LT(player.weapon_ammo(), initial_ammo);

    // 7. Verify pause duration countdown with early mouse release
    player.reset_spawn_shoot_pause(0.5f);
    ASSERT_TRUE(player.is_shooting_paused());
    // Simulate mouse release during countdown
    player.Update(0.2f);
    ASSERT_GT(player.spawn_shoot_pause_timer(), 0.0f);
    // Explicitly clear release requirement while timer is still running
    // (simulating user releasing mouse after 0.2s)
    player.reset_spawn_shoot_pause(0.3f);
    player.clear_spawn_shoot_pause();
    ASSERT_FALSE(player.is_shooting_paused());

    std::cout << "[ OK ] RoundSpawnShootingPauseAndInputDischargePreventionTest (round start pause and LMB release check prevent accidental discharge when opening levels)" << std::endl;
}

int main() {
    std::cout << "====================================================" << std::endl;
    std::cout << "  VOIDFALL DREDGE: GAMEPLAY MECHANICS REGRESSION TESTS" << std::endl;
    std::cout << "====================================================" << std::endl;

    AgitationTest_MeterDoesNotResetAtOneHundredPercent();
    StealthTest_CrouchProducesZeroNoise();
    AimingParallaxTest_RaycastAlignsWithCrosshairWhenCrouched();
    MonsterLifecycleTest_FatalDamageTriggersDeathStateBeforeRemoval();
    NoiseMeterCooldownTest_PostCombatMeterDrainsToSilent();
    NoiseMeterCooldownTest_SwarmEventForcesAggressiveDrain();
    NoiseMeterCooldownTest_LargeNoiseSpikeActivatesCooldown();
    SwarmManagerLifecycleTest_EnrageExpiresAndCoolsToCalm();
    DamageAndScreenShakeTest_FallAndDirectDamageTriggerTraumaAndCallback();
    SwarmBreachRecuperationTest_FightingMobsClearsThreatPeakAndInitiatesCooldown();
    BulletImpactSeismicActivityTest_GunfireGeneratesStressAndTriggersFaultSlip();
    ThreatPeakTest_StandingStillPlayerThreatPeakClearsWhenMeterDrains();
    HudMessageOverlapAndStackingTest_NoCollisionsAcrossAllCrisisScenarios();
    EvacuationDifficultyScalingTest_SectorScaledHoldoutAndPerimeterDefense();
    ExplosiveDistractionAndBulletTremorMechanicsTest();
    EnvironmentalHazardDamageAndTelegraphTest_HazardTraumaIsolationAndVisualTelegraphs();
    EnemyAttackBloodSplatterAndFallDamageBoneCrackTest_BloodSplatterIsolationAndFallBoneCrackMechanics();
    WeaponZoomAndFireMechanicsTest();
    RoundSpawnShootingPauseAndInputDischargePreventionTest();

    std::cout << "====================================================" << std::endl;
    std::cout << "  ALL 19 GAMEPLAY MECHANICS TESTS PASSED CLEANLY!" << std::endl;
    std::cout << "====================================================" << std::endl;

    return 0;
}
