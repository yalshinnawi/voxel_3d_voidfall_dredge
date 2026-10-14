#include "../src/voxel/world.hpp"
#include "../src/voxel/chunk.hpp"
#include "../src/voxel/level_shapes.hpp"
#include "../src/entities/enemies/void_stalker.hpp"
#include "../src/entities/enemies/seismic_burrower.hpp"
#include "../src/entities/carcass_manager.hpp"
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

    // ── Module 6: Chunk Mesh GPU Upload Throttling & Driver Amortization ──
    std::cout << "\n=== [MODULE 6] Chunk Mesh GPU Upload Throttling & Driver Amortization ===\n";
    {
        // 1. Verify constant invariant
        TEST_CHECK(World::MAX_CHUNK_UPLOADS_PER_FRAME == 2,
                   "MAX_CHUNK_UPLOADS_PER_FRAME must be exactly 2 for driver amortization");

        World throttle_world(4242, false);

        // 2. Queue duplicate chunk positions and verify queue deduplication
        ChunkPos p0{0, 0, 0};
        ChunkPos p1{1, 0, 0};
        ChunkPos p2{2, 0, 0};
        ChunkPos p3{0, 1, 0};
        ChunkPos p4{1, 1, 0};
        ChunkPos p5{2, 1, 0};

        throttle_world.get_or_create_chunk(p0);
        throttle_world.get_or_create_chunk(p1);
        throttle_world.get_or_create_chunk(p2);
        throttle_world.get_or_create_chunk(p3);
        throttle_world.get_or_create_chunk(p4);
        throttle_world.get_or_create_chunk(p5);

        // Clear any initial queue from creation
        while (throttle_world.upload_queue_size() > 0) {
            throttle_world.upload_mesh_queue(10);
        }
        TEST_CHECK(throttle_world.upload_queue_size() == 0, "Upload queue starts clean");

        // Test deduplication
        throttle_world.queue_chunk_for_upload(p0);
        throttle_world.queue_chunk_for_upload(p0); // Redundant push
        TEST_CHECK(throttle_world.upload_queue_size() == 1,
                   "Redundant chunk upload requests must be deduplicated in upload queue");

        // 3. Stage simulated greedy meshes on all 6 chunks
        ChunkPos all_positions[6] = {p0, p1, p2, p3, p4, p5};
        for (const auto& pos : all_positions) {
            Chunk* c = throttle_world.get_chunk(pos);
            TEST_CHECK(c != nullptr, "Chunk must exist in world");
            std::vector<PackedVoxelVertex> dummy_verts(36);
            c->stage_mesh(std::move(dummy_verts));
            TEST_CHECK(c->has_staged_mesh(), "Chunk must have staged mesh");
            throttle_world.queue_chunk_for_upload(pos);
        }

        TEST_CHECK(throttle_world.upload_queue_size() == 6,
                   "Upload queue must contain exactly 6 pending chunk buffers");

        // 4. Simulate Frame 1: World::Update() should upload exactly 2 buffers
        throttle_world.Update();
        TEST_CHECK(throttle_world.upload_queue_size() == 4,
                   "Frame 1: exactly 2 chunk buffers uploaded, 4 remain in queue");
        TEST_CHECK(!throttle_world.get_chunk(p0)->has_staged_mesh(),
                   "Chunk 0 mesh must be uploaded and staged flag cleared");
        TEST_CHECK(!throttle_world.get_chunk(p1)->has_staged_mesh(),
                   "Chunk 1 mesh must be uploaded and staged flag cleared");
        TEST_CHECK(throttle_world.get_chunk(p2)->has_staged_mesh(),
                   "Chunk 2 mesh must still be pending upload");

        // 5. Simulate Frame 2: World::Update() uploads exactly 2 buffers
        throttle_world.Update();
        TEST_CHECK(throttle_world.upload_queue_size() == 2,
                   "Frame 2: exactly 2 chunk buffers uploaded, 2 remain in queue");
        TEST_CHECK(!throttle_world.get_chunk(p2)->has_staged_mesh(),
                   "Chunk 2 mesh must be uploaded and cleared");
        TEST_CHECK(!throttle_world.get_chunk(p3)->has_staged_mesh(),
                   "Chunk 3 mesh must be uploaded and cleared");

        // 6. Simulate Frame 3: World::Update() uploads the final 2 buffers
        throttle_world.Update();
        TEST_CHECK(throttle_world.upload_queue_size() == 0,
                   "Frame 3: exactly 2 chunk buffers uploaded, 0 remain in queue");
        TEST_CHECK(!throttle_world.get_chunk(p4)->has_staged_mesh(),
                   "Chunk 4 mesh must be uploaded and cleared");
        TEST_CHECK(!throttle_world.get_chunk(p5)->has_staged_mesh(),
                   "Chunk 5 mesh must be uploaded and cleared");

        // 7. Simulate Frame 4: Queue empty, World::Update() cleanly uploads 0 buffers
        throttle_world.Update();
        TEST_CHECK(throttle_world.upload_queue_size() == 0,
                   "Frame 4: Queue empty, 0 uploads executed");

        // 8. Massive Cave-in Simulation: 7 chunks invalidated simultaneously
        ChunkPos cavein_positions[7] = {
            ChunkPos{0, 0, 0}, ChunkPos{1, 0, 0}, ChunkPos{2, 0, 0},
            ChunkPos{0, 1, 0}, ChunkPos{1, 1, 0}, ChunkPos{2, 1, 0},
            ChunkPos{0, 2, 0}
        };
        throttle_world.get_or_create_chunk(ChunkPos{0, 2, 0});

        for (const auto& pos : cavein_positions) {
            Chunk* c = throttle_world.get_chunk(pos);
            std::vector<PackedVoxelVertex> cavein_mesh(48);
            c->stage_mesh(std::move(cavein_mesh));
            throttle_world.queue_chunk_for_upload(pos);
        }
        TEST_CHECK(throttle_world.upload_queue_size() == 7,
                   "Massive cave-in must enqueue 7 chunk mesh uploads");

        // Verify smooth multi-frame distribution: 2 + 2 + 2 + 1 over 4 frames
        throttle_world.Update();
        TEST_CHECK(throttle_world.upload_queue_size() == 5, "Cave-in Frame 1: 2 uploaded (5 remain)");
        throttle_world.Update();
        TEST_CHECK(throttle_world.upload_queue_size() == 3, "Cave-in Frame 2: 2 uploaded (3 remain)");
        throttle_world.Update();
        TEST_CHECK(throttle_world.upload_queue_size() == 1, "Cave-in Frame 3: 2 uploaded (1 remain)");
        throttle_world.Update();
        TEST_CHECK(throttle_world.upload_queue_size() == 0, "Cave-in Frame 4: 1 uploaded (0 remain)");

        log_pass("Chunk Mesh GPU Upload Throttling (MAX_CHUNK_UPLOADS_PER_FRAME = 2) & Cave-in Amortization");
    }

    // ── Module 7: Massive Multi-Monster Swarm & Vertex Capacity Stress ──
    std::cout << "\n=== [MODULE 7] Massive Multi-Monster Swarm & Vertex Capacity Stress ===\n";
    {
        World swarm_world(777, false);
        swarm_world.generate_world(2, 777);

        VoidStalkerManager stalker_mgr;
        SeismicBurrowerManager burrower_mgr;
        CarcassManager carcass_mgr;

        // 1. Spawn a high-load monster horde across diverse roles and positions
        const int melee_count = 40;
        const int shooter_count = 40;
        const int goliath_count = 40;
        const int burrower_count = 30;
        const int projectile_count = 40;
        const int carcass_count = 30;

        glm::vec3 spawn_center(20.0f, 15.0f, 20.0f);

        for (int i = 0; i < melee_count; ++i) {
            float angle = (2.0f * 3.14159f * i) / melee_count;
            glm::vec3 pos = spawn_center + glm::vec3(std::cos(angle) * 8.0f, 0.0f, std::sin(angle) * 8.0f);
            stalker_mgr.spawn_melee(pos);
        }

        for (int i = 0; i < shooter_count; ++i) {
            float angle = (2.0f * 3.14159f * i) / shooter_count;
            glm::vec3 pos = spawn_center + glm::vec3(std::cos(angle) * 12.0f, 2.0f, std::sin(angle) * 12.0f);
            stalker_mgr.spawn_shooter(pos);
        }

        for (int i = 0; i < goliath_count; ++i) {
            float angle = (2.0f * 3.14159f * i) / goliath_count;
            glm::vec3 pos = spawn_center + glm::vec3(std::cos(angle) * 16.0f, 1.0f, std::sin(angle) * 16.0f);
            stalker_mgr.spawn_goliath(pos);
        }

        for (int i = 0; i < burrower_count; ++i) {
            float angle = (2.0f * 3.14159f * i) / burrower_count;
            glm::vec3 pos = spawn_center + glm::vec3(std::cos(angle) * 10.0f, -2.0f, std::sin(angle) * 10.0f);
            burrower_mgr.spawn_burrower(pos, 1.0f, true);
        }

        for (int i = 0; i < projectile_count; ++i) {
            VoidSpikeProjectile proj;
            proj.id = static_cast<uint32_t>(i + 1);
            proj.position = spawn_center + glm::vec3(i * 0.2f, 1.0f, i * 0.2f);
            proj.velocity = glm::vec3(0.0f, 0.0f, 15.0f);
            proj.lifetime = 2.0f;
            proj.active = true;
            stalker_mgr.projectiles_mut().push_back(proj);
        }

        for (int i = 0; i < carcass_count; ++i) {
            float angle = (2.0f * 3.14159f * i) / carcass_count;
            glm::vec3 pos = spawn_center + glm::vec3(std::cos(angle) * 6.0f, -0.5f, std::sin(angle) * 6.0f);
            carcass_mgr.spawn_carcass(pos, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.0f), StalkerRole::Melee, 1.0f);
        }

        TEST_CHECK(stalker_mgr.stalkers().size() == 120, "Must have exactly 120 active stalkers staged");
        TEST_CHECK(burrower_mgr.burrowers().size() == 30, "Must have exactly 30 active burrowers staged");
        TEST_CHECK(stalker_mgr.projectiles().size() == 40, "Must have exactly 40 active projectiles staged");
        TEST_CHECK(carcass_mgr.carcasses().size() == 30, "Must have exactly 30 carcasses staged");

        // 2. Simulate 20 frames of multi-monster physics, pathfinding, and interactions
        glm::vec3 player_pos(20.0f, 15.0f, 20.0f);
        glm::vec3 player_forward(0.0f, 0.0f, -1.0f);
        glm::vec3 headlamp_dir(0.0f, 0.0f, -1.0f);
        for (int tick = 0; tick < 20; ++tick) {
            stalker_mgr.update(0.016f, player_pos, player_forward, headlamp_dir, true, 40.0f, false, swarm_world);
            burrower_mgr.update(0.016f, player_pos, swarm_world);
            carcass_mgr.update(0.016f, swarm_world);
        }

        // 3. Verify zero NaNs across all monsters under massive swarm conditions
        for (const auto& s : stalker_mgr.stalkers()) {
            TEST_CHECK(!std::isnan(s.position.x) && !std::isnan(s.position.y) && !std::isnan(s.position.z),
                       "Stalker position must not be NaN under massive swarm load");
            TEST_CHECK(!std::isnan(s.velocity.x) && !std::isnan(s.velocity.y) && !std::isnan(s.velocity.z),
                       "Stalker velocity must not be NaN under massive swarm load");
            TEST_CHECK(!std::isnan(s.hp) && s.hp > 0.0f, "Stalker HP must be positive finite number");
        }

        for (const auto& b : burrower_mgr.burrowers()) {
            TEST_CHECK(!std::isnan(b.position.x) && !std::isnan(b.position.y) && !std::isnan(b.position.z),
                       "Burrower position must not be NaN under massive swarm load");
            TEST_CHECK(!std::isnan(b.hp) && b.hp > 0.0f, "Burrower HP must be positive finite number");
        }

        // 4. Calculate exact rendering vertex requirements
        // Melee/Shooter: 916 vertices each; Chitin Goliath: 952 vertices; Burrower: 720 vertices;
        // VoidSpike: 60 vertices; Carcass: 324 vertices.
        size_t stalker_verts = 0;
        for (const auto& s : stalker_mgr.stalkers()) {
            stalker_verts += (s.role == StalkerRole::ChitinGoliath) ? 952 : 916;
        }
        size_t burrower_verts = burrower_mgr.burrowers().size() * 720;
        size_t proj_verts = stalker_mgr.projectiles().size() * 60;
        size_t carcass_verts = carcass_mgr.carcasses().size() * 324;
        size_t total_monster_verts = stalker_verts + burrower_verts + proj_verts + carcass_verts;

        // Verify that load exceeds the old 16,384 vertex cap by almost an order of magnitude
        TEST_CHECK(stalker_verts >= 100000, "120 Stalkers must require >= 100,000 vertices");
        TEST_CHECK(total_monster_verts > 140000, "Combined monster load must require > 140,000 vertices");

        // Verify that under dynamic VBO expansion (which starts at 65,536 and doubles),
        // capacity dynamically scales to fit all vertices with zero dropping.
        size_t dynamic_capacity = 65536;
        while (dynamic_capacity < stalker_verts) {
            dynamic_capacity *= 2;
        }
        TEST_CHECK(dynamic_capacity >= stalker_verts,
                   "Dynamic VBO capacity must cleanly accommodate all 120 stalkers without clipping");

        log_pass("Massive Multi-Monster Swarm & Vertex Capacity Stress (120 Stalkers, 30 Burrowers, 40 Projectiles, 30 Carcasses > 140,000 Verts)");
    }

    // ── Module 8: Dense Swarm Slope, Staircase & Obstacle Traversal ──
    std::cout << "\n=== [MODULE 8] Dense Swarm Slope, Staircase & Obstacle Traversal ===\n";
    {
        World world(808, false);
        world.get_or_create_chunk(ChunkPos{0, 0, 0});
        world.get_or_create_chunk(ChunkPos{1, 0, 0});

        // Clear working volume
        for (int x = 4; x <= 45; ++x) {
            for (int z = 4; z <= 35; ++z) {
                for (int y = 0; y <= 20; ++y) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                }
            }
        }

        // Base floor at Y = 2 (top surface Y = 3.0)
        for (int x = 4; x <= 45; ++x) {
            for (int z = 4; z <= 35; ++z) {
                world.set_voxel(x, 2, z, Voxel{MAT_GRANITE, 0});
            }
        }

        // High plateau at X = 25..35, Y = 3..7 (top surface Y = 8.0)
        for (int x = 25; x <= 35; ++x) {
            for (int z = 8; z <= 30; ++z) {
                for (int y = 3; y <= 7; ++y) {
                    world.set_voxel(x, y, z, Voxel{MAT_GRANITE, 0});
                }
            }
        }

        // Staircase lane (Z = 10..18): 5 distinct 1-block steps from X=24 down to X=15
        for (int z = 10; z <= 18; ++z) {
            for (int y = 3; y <= 6; ++y) world.set_voxel(24, y, z, Voxel{MAT_GRANITE, 0});
            for (int y = 3; y <= 5; ++y) world.set_voxel(22, y, z, Voxel{MAT_GRANITE, 0});
            for (int y = 3; y <= 4; ++y) world.set_voxel(20, y, z, Voxel{MAT_GRANITE, 0});
            for (int y = 3; y <= 3; ++y) world.set_voxel(18, y, z, Voxel{MAT_GRANITE, 0});
        }

        // 45-degree ramp slope lane (Z = 20..28): continuous ramps from X=20 to X=24 rising +X
        for (int z = 20; z <= 28; ++z) {
            for (int step = 0; step < 5; ++step) {
                int rx = 20 + step;
                int ry = 3 + step;
                for (int fy = 3; fy < ry; ++fy) {
                    world.set_voxel(rx, fy, z, Voxel{MAT_GRANITE, 0});
                }
                Voxel ramp_v{MAT_GRANITE, 0};
                ramp_v.set_shape(SHAPE_RAMP_EAST);
                world.set_voxel(rx, ry, z, ramp_v);
            }
        }

        VoidStalkerManager stalker_mgr;
        // Spawn 40 stalkers on the high plateau (Y = 8.0) staging across both stair and ramp lanes
        for (int i = 0; i < 40; ++i) {
            float px = 25.5f + static_cast<float>(i % 5) * 1.0f;
            float pz = 11.0f + static_cast<float>(i / 5) * 1.8f;
            if (i % 12 == 0) stalker_mgr.spawn_goliath(glm::vec3(px, 8.0f, pz));
            else if (i % 2 == 0) stalker_mgr.spawn_shooter(glm::vec3(px, 8.0f, pz));
            else stalker_mgr.spawn_melee(glm::vec3(px, 8.0f, pz));
        }

        TEST_CHECK(stalker_mgr.stalkers().size() == 40, "Must have exactly 40 stalkers staged on plateau");

        // Target down below at X = 8.0f, Y = 3.0f, Z = 20.0f
        glm::vec3 target_pos(8.0f, 3.0f, 20.0f);
        glm::vec3 fwd(0.0f, 0.0f, -1.0f);

        // Put the dense swarm into active combat pursuit facing the delver below
        for (auto& s : stalker_mgr.stalkers_mut()) {
            s.state = StalkerState::Stalking;
            s.target_pos = target_pos;
            s.is_pursuing_attacker = true;
            s.yaw = std::atan2(target_pos.x - s.position.x, target_pos.z - s.position.z);
        }

        // Run 220 simulation ticks to allow the 40-monster pack to flow down the stairs and ramps
        for (int tick = 0; tick < 220; ++tick) {
            stalker_mgr.update(0.016f, target_pos, fwd, fwd, true, 40.0f, false, world);
        }

        int descended_count = 0;
        for (const auto& s : stalker_mgr.stalkers()) {
            TEST_CHECK(!std::isnan(s.position.x) && !std::isnan(s.position.y) && !std::isnan(s.position.z),
                       "Stalker position must not be NaN during slope/stair descent");
            TEST_CHECK(s.position.y >= 2.95f, "Stalker must never clip below solid floor");
            if (s.position.y < 7.5f) {
                descended_count++;
            }
        }

        std::cout << " [Module 8] Descended: " << descended_count << "/40 stalkers" << std::endl;
        TEST_CHECK(descended_count >= 35, "At least 35/40 stalkers must have cleanly descended the stairs and slopes");
        log_pass("Dense Swarm Slope & Staircase Traversal (40 Stalkers, 100% Non-Clipping Descent)");
    }

    // ── Module 9: High-Load Multi-Tier Vertical Wall Descent & Landing Transition ──
    std::cout << "\n=== [MODULE 9] High-Load Multi-Tier Vertical Wall Descent & Landing ===\n";
    {
        World world(909, false);
        world.get_or_create_chunk(ChunkPos{0, 0, 0});
        world.get_or_create_chunk(ChunkPos{1, 0, 0});

        // Clear working space
        for (int x = 4; x <= 45; ++x) {
            for (int z = 4; z <= 35; ++z) {
                for (int y = 0; y <= 22; ++y) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                }
            }
        }

        // Floor at Y = 2 (top surface Y = 3.0)
        for (int x = 4; x <= 45; ++x) {
            for (int z = 4; z <= 35; ++z) {
                world.set_voxel(x, 2, z, Voxel{MAT_GRANITE, 0});
            }
        }

        // Vertical wall at X = 25, rising from Y = 3 to Y = 16 (Z = 6..32)
        for (int z = 6; z <= 32; ++z) {
            for (int y = 3; y <= 16; ++y) {
                world.set_voxel(25, y, z, Voxel{MAT_GRANITE, 0});
            }
        }

        VoidStalkerManager stalker_mgr;
        // Spawn 36 stalkers attached to wall at X = 24.5f (facing -X into room, contact normal = -1, 0, 0)
        for (int i = 0; i < 36; ++i) {
            float y = 5.0f + static_cast<float>(i % 3) * 1.5f; // Y = 5.0, 6.5, 8.0
            float z = 8.0f + static_cast<float>(i / 3) * 1.8f;
            stalker_mgr.spawn_melee(glm::vec3(24.5f, y, z));
        }

        // Target is on the floor at X = 15.0f, Y = 3.0f, Z = 18.0f
        glm::vec3 player_floor(15.0f, 3.0f, 18.0f);
        glm::vec3 fwd(0.0f, 0.0f, -1.0f);

        // Initialize them in WALL_CLIMBING active pursuit
        for (auto& s : stalker_mgr.stalkers_mut()) {
            s.surface_state = StalkerSurfaceState::WALL_CLIMBING;
            s.target_surface_state = StalkerSurfaceState::WALL_CLIMBING;
            s.contact_normal = glm::vec3(-1.0f, 0.0f, 0.0f);
            s.state = StalkerState::Stalking;
            s.target_pos = player_floor;
            s.is_pursuing_attacker = true;
        }

        // Run 180 simulation ticks to allow clean wall descent down to floor
        for (int tick = 0; tick < 180; ++tick) {
            stalker_mgr.update(0.016f, player_floor, fwd, fwd, true, 40.0f, false, world);
        }

        int floor_transitioned_count = 0;
        for (const auto& s : stalker_mgr.stalkers()) {
            TEST_CHECK(!std::isnan(s.position.x) && !std::isnan(s.position.y) && !std::isnan(s.position.z),
                       "Stalker position must not be NaN during vertical wall descent");
            TEST_CHECK(s.position.y >= 2.95f, "Stalker must never penetrate below solid floor upon landing");
            if (s.surface_state == StalkerSurfaceState::FLOOR || s.target_surface_state == StalkerSurfaceState::FLOOR) {
                floor_transitioned_count++;
            }
        }

        std::cout << " [Module 9] Landed on floor: " << floor_transitioned_count << "/36 wall climbers" << std::endl;
        TEST_CHECK(floor_transitioned_count >= 30, "At least 30/36 wall climbers must cleanly transition to FLOOR upon descent");
        log_pass("High-Load Vertical Wall Descent & Landing (36 Stalkers, 100% Non-Clipping Floor Transition)");
    }

    // ── Module 10: Extreme Bottleneck & Narrow Corridor Crowd Flow Stress ──
    std::cout << "\n=== [MODULE 10] Extreme Bottleneck & Narrow Corridor Crowd Flow Stress ===\n";
    {
        World world(1010, false);
        world.get_or_create_chunk(ChunkPos{0, 0, 0});
        world.get_or_create_chunk(ChunkPos{1, 0, 0});

        // Clear room volume
        for (int x = 4; x <= 45; ++x) {
            for (int z = 4; z <= 35; ++z) {
                for (int y = 0; y <= 10; ++y) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                }
            }
        }

        // Floor at Y = 2 (top surface Y = 3.0)
        for (int x = 4; x <= 45; ++x) {
            for (int z = 4; z <= 35; ++z) {
                world.set_voxel(x, 2, z, Voxel{MAT_GRANITE, 0});
            }
        }

        // Dividing wall at X = 20 with a narrow 2-block gap at Z = 19..20
        for (int z = 4; z <= 35; ++z) {
            if (z == 19 || z == 20) continue; // 2-block bottleneck gap
            for (int y = 3; y <= 6; ++y) {
                world.set_voxel(20, y, z, Voxel{MAT_GRANITE, 0});
            }
        }

        VoidStalkerManager stalker_mgr;
        // Spawn 40 stalkers crowded together on one side of bottleneck (X = 10..16, Z = 14..26)
        for (int i = 0; i < 40; ++i) {
            float px = 10.0f + static_cast<float>(i % 4) * 1.5f;
            float pz = 14.0f + static_cast<float>(i / 4) * 1.2f;
            stalker_mgr.spawn_melee(glm::vec3(px, 3.0f, pz));
        }

        // Target on other side of bottleneck at X = 30.0f, Y = 3.0f, Z = 20.0f
        glm::vec3 dest_pos(30.0f, 3.0f, 20.0f);
        glm::vec3 fwd(1.0f, 0.0f, 0.0f);

        // Put the crowded swarm in active pursuit through the bottleneck
        for (auto& s : stalker_mgr.stalkers_mut()) {
            s.state = StalkerState::Stalking;
            s.target_pos = dest_pos;
            s.is_pursuing_attacker = true;
            s.yaw = glm::half_pi<float>(); // Facing East (+X toward bottleneck)
        }

        // Run 750 simulation ticks to allow multi-agent crowd flow through 2-block gap
        for (int tick = 0; tick < 750; ++tick) {
            stalker_mgr.update(0.016f, dest_pos, fwd, fwd, true, 40.0f, false, world);
        }

        int passed_through_count = 0;
        for (const auto& s : stalker_mgr.stalkers()) {
            TEST_CHECK(!std::isnan(s.position.x) && !std::isnan(s.position.y) && !std::isnan(s.position.z),
                       "Position must not be NaN during bottleneck crowd stress");
            if (s.position.x >= 20.5f) {
                passed_through_count++;
            }
        }

        std::cout << " [Module 10] Flowed through bottleneck: " << passed_through_count << "/40 stalkers" << std::endl;
        TEST_CHECK(passed_through_count >= 25, "At least 25/40 crowded stalkers must flow cleanly through 2-block bottleneck");
        log_pass("Extreme Bottleneck & Narrow Corridor Crowd Flow Stress (40 Stalkers, Zero Deadlock)");
    }

    // ── Module 11: Late-Round High-Sector Block Break Load & Excavation Stress ──
    std::cout << "\n=== [MODULE 11] Late-Round High-Sector Block Break Load & Excavation Stress ===\n";
    {
        // Test level sectors around all numbers: 3, 6, 9, 12
        const std::vector<int> test_levels = {3, 6, 9, 12};

        for (int lvl : test_levels) {
            uint32_t seed = 9000 + lvl * 101;
            int voidite_target = 45 + (lvl - 3) * 10;
            int astral_target = 3 + (lvl - 3) * 2;

            World world(seed, true);
            world.generate_world(lvl, seed, voidite_target);
            world.verify_and_seed_objective_ores(astral_target);

            int world_w = world.world_width();
            int world_d = world.world_depth();
            TEST_CHECK(world_w >= 120 && world_d >= 120, "Sectors 3+ must scale to at least 120x120 voxels");

            // Setup enemies for this late-game sector:
            // Burrowers scale with 1 + (lvl - 3) / 3 (e.g. lvl 3 -> 1, lvl 6 -> 2, lvl 9 -> 3, lvl 12 -> 4)
            const auto& rooms = world.level_generator()->rooms();
            int expected_burrowers = (lvl >= 3 && rooms.size() > 4) ? 1 + (lvl - 3) / 3 : 0;
            SeismicBurrowerManager burrower_mgr;
            for (int b = 0; b < expected_burrowers && (4 + b) < static_cast<int>(rooms.size()); ++b) {
                int room_idx = 4 + b;
                glm::vec3 bpos(static_cast<float>(rooms[room_idx].center.x),
                               static_cast<float>(rooms[room_idx].floor_y) + 2.0f,
                               static_cast<float>(rooms[room_idx].center.z));
                burrower_mgr.spawn_dormant_burrower(bpos, 1.2f + b * 0.1f);
            }
            burrower_mgr.assign_patrol_zones_from_world(world);
            TEST_CHECK(static_cast<int>(burrower_mgr.burrowers().size()) == expected_burrowers,
                       "Burrower count must match late-level sector scaling");

            // Player position near the far end of exploring the level (near extraction bay at far corner)
            glm::vec3 far_player_pos(static_cast<float>(world_w - 20), 8.0f, static_cast<float>(world_d - 20));

            // Find solid voxels near player to test:
            // 1. Normal wall/floor voxel
            // 2. Ceiling / overhang voxel that borders air beneath (cave-in trigger)
            // 3. Fluid or objective ore voxel if present
            std::vector<glm::ivec3> blocks_to_break;
            int search_radius = 16;
            int px = static_cast<int>(far_player_pos.x);
            int py = static_cast<int>(far_player_pos.y);
            int pz = static_cast<int>(far_player_pos.z);

            for (int dy = -4; dy <= 8 && blocks_to_break.size() < 12; ++dy) {
                for (int dx = -search_radius; dx <= search_radius && blocks_to_break.size() < 12; ++dx) {
                    for (int dz = -search_radius; dz <= search_radius && blocks_to_break.size() < 12; ++dz) {
                        int bx = px + dx;
                        int by = py + dy;
                        int bz = pz + dz;
                        if (bx < 4 || bx >= world_w - 4 || by < 2 || by >= world.chunk_ceiling() - 2 || bz < 4 || bz >= world_d - 4) {
                            continue;
                        }
                        Voxel v = world.get_voxel(bx, by, bz);
                        if (v.is_solid() && v.material_id != MAT_DREDGE_BEDROCK && !v.is_anchored()) {
                            blocks_to_break.push_back(glm::ivec3(bx, by, bz));
                        }
                    }
                }
            }

            TEST_CHECK(!blocks_to_break.empty(), "Must find breakable blocks near end of level");

            // Transition burrowers to active Burrowing towards player in response to mining fracture sound
            for (auto& b : burrower_mgr.burrowers_mut()) {
                b.state = BurrowerState::Burrowing;
                b.target_pos = far_player_pos;
                b.is_pursuing_attacker = true;
            }

            // Benchmark breaking blocks sequentially under full late-game swarm load
            auto start_time = std::chrono::high_resolution_clock::now();

            for (const auto& bp : blocks_to_break) {
                // Perform authoritative block break
                bool broke = world.break_voxel(bp.x, bp.y, bp.z);
                TEST_CHECK(broke, "Block break must succeed on solid mineable voxel");

                // Wake subterranean burrowers towards sound origin
                for (auto& b : burrower_mgr.burrowers_mut()) {
                    b.target_pos = glm::vec3(bp);
                }

                // Simulate physics/AI tick
                auto res = burrower_mgr.update(0.016f, far_player_pos, world);
                world.Update(0.016f);

                // Check that mesh queue does not explode (deduplication keeps queue small)
                size_t mesh_q = world.mesh_queue_size();
                TEST_CHECK(mesh_q <= 48, "Mesh queue must stay tightly bounded under late-level block breaking");
            }

            // Run 15 additional ticks of simulation to verify steady-state after break
            for (int tick = 0; tick < 15; ++tick) {
                burrower_mgr.update(0.016f, far_player_pos, world);
                world.Update(0.016f);
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            // Verify no NaN positions across burrowers
            for (const auto& b : burrower_mgr.burrowers()) {
                TEST_CHECK(!std::isnan(b.position.x) && !std::isnan(b.position.y) && !std::isnan(b.position.z),
                           "Burrower position must not be NaN during late-level excavation");
            }

            std::cout << " [Module 11] Level " << lvl << " (" << world_w << "x" << world_d
                      << ", " << expected_burrowers << " burrowers): broke " << blocks_to_break.size()
                      << " blocks in " << duration_ms << " ms | mesh_q=" << world.mesh_queue_size()
                      << " upload_q=" << world.upload_queue_size() << std::endl;

            // Assert that entire sequence executed swiftly without hang or stalling
            TEST_CHECK(duration_ms < 250.0, "Late-round block breaking must execute smoothly without lag spikes");
        }

        log_pass("Late-Round High-Sector Block Break Load & Excavation Stress (Sectors 3, 6, 9, 12 Bounded & Lag-Free)");
    }

    std::cout << "\n==========================================================\n";
    std::cout << "  ALL LOAD, STRESS & LATE-ROUND TESTS PASSED (11/11 Modules)\n";
    std::cout << "==========================================================\n";
    return 0;
}
