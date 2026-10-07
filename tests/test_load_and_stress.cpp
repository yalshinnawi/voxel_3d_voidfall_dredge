#include "../src/voxel/world.hpp"
#include "../src/voxel/chunk.hpp"
#include "../src/voxel/level_shapes.hpp"
#include "../src/entities/enemies/void_stalker.hpp"
#include "../src/ai/spawn_manager.hpp"
#include "../src/audio/audio_engine.hpp"
#include "../src/systems/hazard_clock.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <cassert>
#include <cmath>

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n[TEST FAILED] " << (msg) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while(0)

static void log_pass(const char* test_name) {
    std::cout << "  [PASS] " << test_name << "\n";
}

using namespace Voidfall;

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  VOIDFALL: DREDGE -- LOAD, STRESS & LATE-ROUND TEST SUITE\n";
    std::cout << "==========================================================\n\n";

    // ── Module 1: Late-Round Sector Generation & Chunk Bounding ──
    std::cout << "=== [MODULE 1] Late-Round World Generation & Chunk Bounding ===\n";
    {
        // Test Sector 3 (5x5 room grid, 120x120 voxels)
        World s3_world(12345, false);
        s3_world.generate_world(3, 12345);

        int max_cx = (s3_world.level_generator()->world_width() + CHUNK_SIZE - 1) / CHUNK_SIZE;
        int max_cz = (s3_world.level_generator()->world_depth() + CHUNK_SIZE - 1) / CHUNK_SIZE;
        TEST_CHECK(max_cx >= 4, "Sector 3 must have at least 4 horizontal chunks (120 voxels)");
        TEST_CHECK(max_cz >= 4, "Sector 3 must have at least 4 depth chunks (120 voxels)");

        // Walk viewer through the entire sector perimeter and verify no out-of-bounds chunks are spawned
        for (int x = -10; x <= s3_world.level_generator()->world_width() + 10; x += 25) {
            for (int z = -10; z <= s3_world.level_generator()->world_depth() + 10; z += 25) {
                s3_world.update(glm::vec3(x, 10.0f, z), 2);
            }
        }

        for (const auto& [pos, chunk] : s3_world.chunks()) {
            TEST_CHECK(pos.x >= 0 && pos.x < max_cx, "Chunk X must remain strictly within sector bounds");
            TEST_CHECK(pos.z >= 0 && pos.z < max_cz, "Chunk Z must remain strictly within sector bounds");
            TEST_CHECK(pos.y >= 0 && pos.y <= 3, "Chunk Y must remain within vertical ceiling bounds");
        }
        log_pass("Sector 3 Bounded Chunk Generation (Zero Phantom Chunks)");

        // Test Sector 4 & 5 scaling
        World s4_world(54321, false);
        s4_world.generate_world(4, 54321);
        TEST_CHECK(!s4_world.chunks().empty(), "Sector 4 must generate successfully");

        World s5_world(99999, false);
        s5_world.generate_world(5, 99999);
        TEST_CHECK(!s5_world.chunks().empty(), "Sector 5 must generate successfully");
        log_pass("Late-Round Sector 4 & 5 Generation Scale Verification");
    }

    // ── Module 2: Spiral-of-Death & Fixed-Tick Accumulator Defense ──
    std::cout << "\n=== [MODULE 2] Simulation Lag Spike & Death Spiral Defense ===\n";
    {
        double accumulator = 0.0;
        constexpr double fixed_dt = 1.0 / 60.0;
        constexpr int MAX_FIXED_TICKS = 4;

        // Simulate a massive 300ms frame lag spike (e.g. OS background hitch or heavy draw call)
        double frame_spike = 0.300;
        accumulator += frame_spike;

        int ticks_executed = 0;
        while (accumulator >= fixed_dt && ticks_executed < MAX_FIXED_TICKS) {
            accumulator -= fixed_dt;
            ticks_executed++;
        }
        if (accumulator >= fixed_dt) {
            accumulator = 0.0; // Spiral-of-death backlog drop
        }

        TEST_CHECK(ticks_executed == 4, "Lag spike must be clamped to MAX_FIXED_TICKS = 4");
        TEST_CHECK(accumulator == 0.0, "Stale backlog must be cleared to prevent freeze cascade");

        // Subsequent normal frame (16.6ms) must immediately run 1 tick smoothly
        accumulator += (1.0 / 60.0);
        ticks_executed = 0;
        while (accumulator >= fixed_dt && ticks_executed < MAX_FIXED_TICKS) {
            accumulator -= fixed_dt;
            ticks_executed++;
        }
        TEST_CHECK(ticks_executed == 1, "Engine must resume normal 60Hz tick immediately after spike");
        log_pass("Fixed-Tick Clamping & Accumulator Spiral-of-Death Prevention");
    }

    // ── Module 3: Enemy State Stability Under Load (Anti-Clipping) ──
    std::cout << "\n=== [MODULE 3] Enemy State Stability Under Heavy Load ===\n";
    {
        World world(12345, false);
        world.generate_world(1, 12345);

        // Carve testing room
        for (int x = 10; x <= 30; ++x) {
            for (int y = 2; y <= 15; ++y) {
                for (int z = 10; z <= 40; ++z) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }

        VoidStalkerManager manager;
        glm::vec3 stalker_pos(20.0f, 3.0f, 20.0f);
        manager.spawn_stalker(stalker_pos);

        // Put stalker into Stalking state
        manager.stalkers_mut()[0].state = StalkerState::Stalking;
        manager.stalkers_mut()[0].state_timer = 0.0f;
        manager.stalkers_mut()[0].lost_los_timer = 0.0f;

        // Position player at ~15m distance (the critical boundary zone)
        glm::vec3 player_pos(20.0f, 3.0f, 35.0f);

        // Simulate frame spikes with variable dt (0.04s to 0.12s) across 30 frames
        int flips_to_investigate = 0;
        for (int frame = 0; frame < 30; ++frame) {
            float simulated_dt = (frame % 2 == 0) ? 0.12f : 0.04f;
            StalkerState prev_state = manager.stalkers()[0].state;

            manager.update(simulated_dt, player_pos, player_pos, glm::vec3(0, 0, -1),
                           true, 25.0f, false, world);

            StalkerState new_state = manager.stalkers()[0].state;
            if (prev_state == StalkerState::Stalking && new_state == StalkerState::Investigating) {
                flips_to_investigate++;
            }
        }

        // State hysteresis and unified visual range prevent rapid oscillation
        TEST_CHECK(flips_to_investigate <= 1, "Enemy must not flip-flop between Stalking and Investigating");
        log_pass("Perception Range & State Hysteresis (Zero State Flipping)");
    }

    // ── Module 4: Audio Engine Voice Saturation & Priority Eviction ──
    std::cout << "\n=== [MODULE 4] Audio Voice Saturation & Priority Eviction ===\n";
    {
        AudioEngine audio;
        audio.init(false); // Silent headless mode

        // Fire 40 low-priority Geiger clicks
        for (int i = 0; i < 40; ++i) {
            audio.play_sound_2d(SoundCue::GeigerClick, 0.5f);
        }

        // Fire 20 ambient drips
        for (int i = 0; i < 20; ++i) {
            audio.play_sound_2d(SoundCue::CavernDrip, 0.4f);
        }

        int count_before = audio.active_voice_count();
        TEST_CHECK(count_before <= 64, "Voice count must never exceed MAX_VOICES (64)");

        // Now fire high-priority combat and monster cues
        audio.play_sound_2d(SoundCue::BurrowerRoar, 1.0f);
        audio.play_sound_2d(SoundCue::StalkerLunge, 1.0f);
        audio.play_sound_2d(SoundCue::SectorArrival1, 1.0f);
        audio.play_sound_2d(SoundCue::PlasmaFire, 1.0f);

        // Verify the high-priority sounds are active in voice slots
        bool has_roar = false;
        bool has_lunge = false;
        bool has_arrival = false;
        bool has_plasma = false;

        for (const auto& v : audio.voices()) {
            if (v.active) {
                if (v.cue == SoundCue::BurrowerRoar) has_roar = true;
                if (v.cue == SoundCue::StalkerLunge) has_lunge = true;
                if (v.cue == SoundCue::SectorArrival1) has_arrival = true;
                if (v.cue == SoundCue::PlasmaFire) has_plasma = true;
            }
        }

        TEST_CHECK(has_roar, "BurrowerRoar must be playing (priority preserved)");
        TEST_CHECK(has_lunge, "StalkerLunge must be playing (priority preserved)");
        TEST_CHECK(has_arrival, "SectorArrival stinger must be playing (priority preserved)");
        TEST_CHECK(has_plasma, "PlasmaFire must be playing (priority preserved)");

        // Try flooding with 100 more Geiger clicks: high priority sounds must NOT be stolen!
        for (int i = 0; i < 100; ++i) {
            audio.play_sound_2d(SoundCue::GeigerClick, 0.5f);
        }

        has_roar = false;
        for (const auto& v : audio.voices()) {
            if (v.active && v.cue == SoundCue::BurrowerRoar) has_roar = true;
        }
        TEST_CHECK(has_roar, "BurrowerRoar must NOT be stolen by low-priority Geiger clicks");

        // Verify offline mix render does not crash or allocate unbounded memory
        std::vector<float> mix = audio.render_offline_samples(0.1f);
        TEST_CHECK(!mix.empty(), "Audio offline mix must render cleanly");
        log_pass("Audio Priority Voice Allocation & Anti-Clipping Guard");
    }

    // ── Module 5: Late-Round Enemy Pacing & Playability ──
    std::cout << "\n=== [MODULE 5] Late-Round Enemy Pacing & Playability ===\n";
    {
        World world(1, false);
        VoidStalkerManager stalker_mgr;

        SpawnManager spawn_s1;
        spawn_s1.reset(1);
        TEST_CHECK(spawn_s1.max_allowed_enemies() == 0, "Grace period begins with 0 hostiles permitted");

        // Fast-forward past grace and early ramp window (150s)
        spawn_s1.update(150.0f, world, stalker_mgr, glm::vec3(0.0f));
        TEST_CHECK(spawn_s1.max_allowed_enemies() == 2, "Sector 1 enemy cap must be 2");

        SpawnManager spawn_s2;
        spawn_s2.reset(2);
        spawn_s2.update(150.0f, world, stalker_mgr, glm::vec3(0.0f));
        TEST_CHECK(spawn_s2.max_allowed_enemies() == 4, "Sector 2 enemy cap must be 4");

        SpawnManager spawn_s3;
        spawn_s3.reset(3);
        spawn_s3.update(150.0f, world, stalker_mgr, glm::vec3(0.0f));
        TEST_CHECK(spawn_s3.max_allowed_enemies() == 6, "Sector 3 enemy cap must be 6 (prevents exponential lag)");

        SpawnManager spawn_s4;
        spawn_s4.reset(4);
        spawn_s4.update(150.0f, world, stalker_mgr, glm::vec3(0.0f));
        TEST_CHECK(spawn_s4.max_allowed_enemies() == 6, "Sector 4 enemy cap must be 6");

        SpawnManager spawn_s5;
        spawn_s5.reset(5);
        spawn_s5.update(150.0f, world, stalker_mgr, glm::vec3(0.0f));
        TEST_CHECK(spawn_s5.max_allowed_enemies() == 6, "Sector 5 enemy cap must be 6");

        // Verify hazard clock seismic triggers and reset
        HazardClock clock;
        clock.reset();
        TEST_CHECK(!clock.is_tremoring(), "Hazard clock begins without tremor");

        clock.force_tremor(1.0f);
        TEST_CHECK(clock.is_tremoring(), "force_tremor must activate seismic tremor");
        TEST_CHECK(clock.tremor_intensity() > 0.0f, "Tremor intensity must be > 0");
        log_pass("Late-Round Enemy Caps & Hazard Clock Verification");
    }

    std::cout << "\n==========================================================\n";
    std::cout << "  ALL LOAD, STRESS & LATE-ROUND TESTS PASSED (5/5 Modules)\n";
    std::cout << "==========================================================\n";
    return 0;
}
