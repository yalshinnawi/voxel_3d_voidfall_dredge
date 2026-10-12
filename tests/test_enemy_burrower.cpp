#include <iostream>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include "../src/entities/enemies/seismic_burrower.hpp"
#include "../src/voxel/world.hpp"

using namespace Voidfall;

#define CHECK(expr, msg) \
    if (!(expr)) { \
        std::cerr << "[TEST FAILURE] " << msg << " (" #expr ") at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "RUNNING SEISMIC BURROWER ENEMY UNIT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    World world;
    world.generate_world(1, 4242);

    // Test 1: Spawning, Attributes, and Initial State
    {
        std::cout << "[Test 1] Testing Seismic Burrower Spawning & Initial State..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(20.0f, 15.0f, 20.0f);
        manager.spawn_burrower(spawn_pos);

        CHECK(manager.burrowers().size() == 1, "Expected 1 burrower spawned");
        CHECK(manager.active_count() == 1, "Expected active count == 1");

        const auto& b = manager.burrowers()[0];
        CHECK(b.hp == 120.0f, "Expected baseline 120.0 HP for Seismic Burrower");
        CHECK(b.max_hp == 120.0f, "Expected baseline max_hp == 120.0");
        CHECK(b.scale > 1.5f, "Burrower should have large scale >= 1.5");
        CHECK(!b.is_dead(), "Burrower should not be dead on spawn");

        glm::vec4 core_col = b.get_core_color();
        CHECK(core_col.r > 0.8f, "Core color should be bright warm volcanic/magma");

        std::cout << " -> Initial attributes and invariants verified." << std::endl;
    }

    // Test 2: Subterranean Excavation & Voxel Destruction
    {
        std::cout << "[Test 2] Testing Soft Voxel Excavation & Tunneling..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(25.0f, 20.0f, 25.0f);
        manager.spawn_burrower(spawn_pos);

        // Place soft granite blocks ahead of burrower
        world.set_voxel(26, 20, 25, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
        world.set_voxel(27, 20, 25, Voxel{MAT_FRACTURED_GRANITE, 0}, false);

        // Target player in the same direction
        glm::vec3 player_pos(35.0f, 20.0f, 25.0f);
        auto res = manager.update(0.5f, player_pos, world);

        // Check if soft voxels were excavated
        Voxel v = world.get_voxel(26, 20, 25);
        CHECK(v.material_id == MAT_AIR, "Expected soft granite to be excavated to air by burrower");
        CHECK(!res.excavated_voxels.empty(), "Expected excavated voxel deltas in frame result");

        std::cout << " -> Voxel tunneling and excavation verified." << std::endl;
    }

    // Test 3: Bedrock Indestructibility & Collision Stun
    {
        std::cout << "[Test 3] Testing Bedrock Collision & Self-Stun..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(10.0f, 20.0f, 10.0f);
        manager.spawn_burrower(spawn_pos);
        auto& b = manager.burrowers_mut()[0];
        b.state = BurrowerState::Charging;
        b.yaw = 1.5707963f; // Facing +X towards target

        // Place unbreakable bedrock directly in front of the burrower
        world.set_voxel(11, 20, 10, Voxel{MAT_DREDGE_BEDROCK, 0}, false);

        glm::vec3 player_pos(20.0f, 20.0f, 10.0f);
        manager.update(0.2f, player_pos, world);

        // Bedrock must remain intact
        Voxel bed = world.get_voxel(11, 20, 10);
        CHECK(bed.material_id == MAT_DREDGE_BEDROCK, "Bedrock should NEVER be destroyed by burrower");

        // Burrower must become stunned after hitting bedrock
        CHECK(b.state == BurrowerState::Stunned, "Burrower should be STUNNED after hitting bedrock");
        CHECK(b.stun_timer > 0.0f, "Stun timer should be active");

        std::cout << " -> Bedrock collision and stun verified." << std::endl;
    }

    // Test 4: Breaching, Kinetic Charge, and Player Damage
    {
        std::cout << "[Test 4] Testing Open Cavern Breaching & Kinetic Charge..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(15.0f, 20.0f, 15.0f);
        manager.spawn_burrower(spawn_pos);

        // Carve open air around player and burrower
        for (int x = 12; x <= 22; ++x) {
            for (int y = 18; y <= 24; ++y) {
                for (int z = 12; z <= 22; ++z) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }

        glm::vec3 player_pos(17.0f, 20.0f, 15.0f);
        manager.update(0.1f, player_pos, world);

        auto& b = manager.burrowers_mut()[0];
        CHECK(b.state == BurrowerState::Breaching || b.state == BurrowerState::Charging,
              "Burrower in open air near player should transition to Breaching or Charging");

        // Force charging state close to player
        b.state = BurrowerState::Charging;
        b.position = glm::vec3(16.5f, 20.0f, 15.0f);
        auto res = manager.update(0.1f, player_pos, world);

        CHECK(res.any_ram_hit, "Expected kinetic ram hit when colliding with player during charge");
        CHECK(res.total_damage >= 20.0f, "Expected heavy ram damage >= 20 HP");

        std::cout << " -> Breaching and kinetic ram damage verified." << std::endl;
    }

    // Test 5: Vulnerability to Demolitionist Explosives (2.5x Multiplier)
    {
        std::cout << "[Test 5] Testing Explosive Charge Vulnerability (2.5x Multiplier)..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(30.0f, 20.0f, 30.0f);
        manager.spawn_burrower(spawn_pos);

        // Regular drill damage: 20 damage
        bool hit_normal = manager.damage_nearest(spawn_pos, 5.0f, 20.0f, false);
        CHECK(hit_normal, "Expected to damage burrower");
        CHECK(manager.burrowers()[0].hp == 100.0f, "Expected 120 - 20 = 100 HP");

        // Explosive damage: 20 base damage * 2.5x = 50 damage!
        bool hit_explosive = manager.damage_nearest(spawn_pos, 5.0f, 20.0f, true);
        CHECK(hit_explosive, "Expected explosive damage to connect");
        CHECK(manager.burrowers()[0].hp == 50.0f, "Expected 100 - (20 * 2.5) = 50 HP with explosive vulnerability");
        CHECK(manager.burrowers()[0].state == BurrowerState::Stunned, "Explosives should stun the burrower");

        // Lethal explosive blast
        manager.damage_nearest(spawn_pos, 5.0f, 30.0f, true);
        CHECK(manager.burrowers()[0].is_dead(), "Expected burrower to die when HP reaches 0");

        manager.remove_dead();
        CHECK(manager.active_count() == 0, "Expected 0 active burrowers after cleanup");

        std::cout << " -> Explosive vulnerability (2.5x) and death verified." << std::endl;
    }

    // Test 6: Enraged State Tectonic Cave-In Emission
    {
        std::cout << "[Test 6] Testing Enraged Tectonic Shockwave..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(40.0f, 20.0f, 40.0f);
        manager.spawn_burrower(spawn_pos);
        auto& b = manager.burrowers_mut()[0];
        b.state = BurrowerState::Enraged;
        b.state_timer = 1.0f; // Ready to trigger shockwave

        glm::vec3 player_pos(42.0f, 20.0f, 40.0f);
        auto res = manager.update(0.1f, player_pos, world);

        CHECK(res.any_cavein_triggered, "Expected cave-in trigger in Enraged state");
        CHECK(!res.cavein_origins.empty(), "Expected cave-in origins emitted");

        std::cout << " -> Enraged tectonic shockwave verified." << std::endl;
    }

    // Test 7: Attacking Burrower Triggers Relentless Pursuit & Prolonged Escape
    {
        std::cout << "[Test 7] Testing Attacking Burrower Triggers Retaliation Pursuit..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(30.0f, 20.0f, 30.0f);
        manager.spawn_burrower(spawn_pos);

        auto& b = manager.burrowers_mut()[0];
        b.state = BurrowerState::Dormant;
        CHECK(!b.is_pursuing_attacker, "Burrower should not initially be pursuing");

        // 7a: Damage burrower while dormant -> Awakens and triggers retaliation pursuit
        bool hit = manager.damage_nearest(spawn_pos, 2.0f, 20.0f, false);
        CHECK(hit, "Expected to hit dormant burrower");
        CHECK(b.is_pursuing_attacker, "Damaged burrower must enter is_pursuing_attacker state");
        CHECK(b.state == BurrowerState::Burrowing, "Damaged dormant burrower must immediately awaken into Burrowing state");
        CHECK(b.pursuit_lost_timer == 0.0f, "pursuit_lost_timer must start at 0");

        // 7b: Player moves 25m away (< 35m pursuit_break_dist) -> Burrower continues pursuit
        glm::vec3 player_pos(30.0f, 20.0f, 55.0f); // 25m away
        for (int i = 0; i < 50; ++i) {
            manager.update(0.1f, player_pos, world);
        }
        CHECK(b.is_pursuing_attacker, "Burrower must continue pursuing player within 35m");

        // 7c: Player flees far away (> 35m, e.g. 58m separation from clamp boundary) for 5 seconds (< 15s break time)
        glm::vec3 far_player_pos(30.0f, 20.0f, 120.0f); // Even clamped at z=62, distance is >= 58m > 35m
        for (int i = 0; i < 50; ++i) {
            manager.update(0.1f, far_player_pos, world);
        }
        CHECK(b.is_pursuing_attacker, "Burrower should still be hunting player after only 5 seconds (< 15s)");

        // 7d: Player maintains distant separation for total > 15s (11 more seconds)
        for (int i = 0; i < 110; ++i) {
            manager.update(0.1f, far_player_pos, world);
        }
        CHECK(!b.is_pursuing_attacker, "Burrower must finally lose interest after player has fled far away for > 15 seconds");

        std::cout << " -> Attacking burrower retaliation pursuit & prolonged escape verified." << std::endl;
    }

    // Test 8: Subterranean Wall Drilling & Chattering Noises
    {
        std::cout << "[Test 8] Testing Burrower Wall Drilling & Chattering Noises..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(30.0f, 20.0f, 30.0f);
        manager.spawn_burrower(spawn_pos);
        auto& b = manager.burrowers_mut()[0];
        b.state = BurrowerState::Burrowing;
        b.grind_timer = 0.05f; // Ready to emit drilling/grinding noise
        b.chatter_timer = 0.05f; // Ready to emit chattering noise

        glm::vec3 player_pos(30.0f, 20.0f, 35.0f);
        auto res = manager.update(0.1f, player_pos, world);

        CHECK(res.any_grind, "Burrower tunneling through walls must emit drilling/digging noises (any_grind)");
        CHECK(b.just_ground, "Burrower must set just_ground flag when drilling into walls");
        CHECK(res.any_chatter, "Burrower must emit chattering noises while traveling through stone");
        CHECK(b.just_chattered, "Burrower must set just_chattered flag");

        std::cout << " -> Subterranean wall drilling & chattering noises verified." << std::endl;
    }

    // Test 9: Localized Burrowing Occlusion & Wall Line-Of-Sight Isolation
    {
        std::cout << "[Test 9] Testing Localized Burrowing Occlusion & Wall Line-Of-Sight..." << std::endl;

        // Construct two chambers separated by a solid rock barrier wall at x = 20
        World chamber_world;
        chamber_world.generate_world(1, 9999);
        // Clear Room A (x: 10..18, y: 18..22, z: 10..20)
        for (int x = 10; x <= 18; ++x) {
            for (int y = 18; y <= 22; ++y) {
                for (int z = 10; z <= 20; ++z) {
                    chamber_world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }
        // Build solid dividing wall at x = 19..21
        for (int x = 19; x <= 21; ++x) {
            for (int y = 18; y <= 22; ++y) {
                for (int z = 10; z <= 20; ++z) {
                    chamber_world.set_voxel(x, y, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
                }
            }
        }
        // Clear Room B (x: 22..30, y: 18..22, z: 10..20)
        for (int x = 22; x <= 30; ++x) {
            for (int y = 18; y <= 22; ++y) {
                for (int z = 10; z <= 20; ++z) {
                    chamber_world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }

        glm::vec3 player_eye(14.0f, 20.0f, 15.0f);
        glm::vec3 behind_wall_burrow(26.0f, 20.0f, 15.0f);
        glm::vec3 same_room_burrow(16.0f, 20.0f, 15.0f);

        // Raycast across dividing wall
        glm::vec3 diff_wall = behind_wall_burrow - player_eye;
        float d_wall = glm::length(diff_wall);
        RaycastHit hit_wall = chamber_world.raycast(player_eye, diff_wall / d_wall, d_wall - 0.25f);
        CHECK(hit_wall.hit, "Intervening wall must block line-of-sight to behind-wall burrowing");
        CHECK(hit_wall.block_pos.x >= 19 && hit_wall.block_pos.x <= 21, "Raycast must hit the dividing basalt wall");

        // Raycast within same room (direct line-of-sight)
        glm::vec3 diff_room = same_room_burrow - player_eye;
        float d_room = glm::length(diff_room);
        RaycastHit hit_room = chamber_world.raycast(player_eye, diff_room / d_room, d_room - 0.25f);
        CHECK(!hit_room.hit, "Direct line-of-sight within same room must NOT be blocked by wall");

        std::cout << " -> Localized burrowing wall occlusion & line-of-sight verified." << std::endl;
    }

    // Test 10: Acoustic Awakening of Dormant Burrowers & Retaliation When Shot
    {
        std::cout << "[Test 10] Testing Acoustic Awakening of Dormant Burrowers & Retaliation When Shot..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 burrower_pos(20.0f, 20.0f, 20.0f);
        manager.spawn_burrower(burrower_pos);

        auto& b = manager.burrowers_mut()[0];
        b.state = BurrowerState::Dormant;
        CHECK(b.is_dormant(), "Burrower should be in Dormant state");

        // 10.1: Acoustic awakening via distant gunshot sound (30m away)
        glm::vec3 gunshot_pos(20.0f, 20.0f, 50.0f);
        std::vector<SoundEvent> sounds;
        SoundEvent gunshot;
        gunshot.type = SoundEventType::Gunshot;
        gunshot.position = gunshot_pos;
        gunshot.intensity = 28.0f;
        gunshot.audible_radius = 50.0f;
        gunshot.lifetime = 0.5f;
        sounds.push_back(gunshot);

        glm::vec3 player_idle_pos(20.0f, 20.0f, 50.0f);
        manager.update(0.1f, player_idle_pos, world, sounds);

        CHECK(!b.is_dormant(), "Dormant burrower must awaken immediately upon hearing gunfire disturbance");
        CHECK(b.state == BurrowerState::Burrowing || b.state == BurrowerState::Charging,
              "Awakened burrower must enter active tunneling pursuit");

        // 10.2: Reaction when shot: immediate lock-on and shooter reorientation
        b.state = BurrowerState::Dormant; // Reset to dormant
        glm::vec3 shooter_pos(20.0f, 20.0f, 10.0f); // Shooter is at -Z
        glm::vec3 shot_dir(0.0f, 0.0f, 1.0f);       // Shot travels in +Z
        bool hit = manager.damage_nearest(burrower_pos, 3.0f, 35.0f, false, false,
                                          nullptr, nullptr, &shooter_pos, &shot_dir);
        CHECK(hit, "damage_nearest must hit burrower");
        CHECK(!b.is_dormant(), "Burrower must awaken immediately when shot");
        CHECK(b.target_pos == shooter_pos, "Burrower must target the shooter's coordinates");

        std::cout << " -> Acoustic awakening and shooter retaliation verified." << std::endl;
    }

    // Test 11: Spawn Dormancy, Audio Rate-Limiting, and Stealth Safeguards
    {
        std::cout << "[Test 11] Testing Spawn Dormancy, Audio Rate-Limiting, and Proximity Safeguards..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 burrower_pos(20.0f, 20.0f, 20.0f);
        manager.spawn_dormant_burrower(burrower_pos);

        CHECK(manager.burrowers().size() == 1, "Expected 1 burrower spawned");
        const auto& b = manager.burrowers()[0];
        CHECK(b.is_dormant(), "Spawned burrower should initialize in Dormant state");
        CHECK(b.is_unalerted(), "Dormant burrower must be unalerted");

        // Player is 18m away, walking quietly without loud sounds
        glm::vec3 player_pos(20.0f, 20.0f, 38.0f);
        auto res = manager.update(0.2f, player_pos, world, {}, false);
        CHECK(b.is_dormant(), "Burrower must remain dormant when player is 18m away");
        CHECK(!res.any_roar, "Dormant burrower must NOT emit ambient subterranean roars");
        CHECK(!res.any_grind, "Dormant burrower must NOT emit grind sounds");

        // Player approaches within 6m (un-crouched) -> wakes up
        glm::vec3 close_player(20.0f, 20.0f, 25.0f);
        res = manager.update(0.2f, close_player, world, {}, false);
        CHECK(!b.is_dormant(), "Burrower must awaken when player approaches within 6m without stealth");
        CHECK(b.state == BurrowerState::Burrowing, "Awakened burrower should begin burrowing");

        // Subsequent update immediately afterwards should not trigger another roar due to rate-limiting
        res = manager.update(0.1f, close_player, world, {}, false);
        CHECK(!res.any_roar, "Roar should be rate-limited and not spam back-to-back");

        std::cout << " -> Spawn dormancy, proximity safeguards, and audio rate-limiting verified." << std::endl;
    }

    // Test 12: Breaching Emergence State, Rotary Cutter Teeth Spin & Kinetic Transition
    {
        std::cout << "[Test 12] Testing Breaching Emergence, High-RPM Rotary Cutter Spin & Kinetic Charge Transition..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(20.0f, 20.0f, 20.0f);
        glm::vec3 breach_facing(0.0f, 0.0f, 1.0f);
        manager.spawn_emerging_burrower(spawn_pos, breach_facing);

        CHECK(manager.burrowers().size() == 1, "Expected 1 burrower spawned");
        const auto& b = manager.burrowers()[0];
        CHECK(b.state == BurrowerState::Breaching, "Emerging burrower must initialize in Breaching state");
        CHECK(b.is_emerging(), "is_emerging() helper must return true during Breaching");
        CHECK(b.just_roared, "Emerging breach should trigger subterranean roar event");
        CHECK(b.just_ground, "Emerging breach should trigger initial grinding teeth event");

        float initial_cutter = b.cutter_angle;
        glm::vec3 player_pos(20.0f, 20.0f, 30.0f);
        auto res = manager.update(0.4f, player_pos, world, {}, false);

        CHECK(b.state == BurrowerState::Breaching, "Burrower should remain breaching at 0.4s (duration 1.2s)");
        CHECK(b.cutter_angle > initial_cutter + 10.0f, "Rotary cutter head must spin at high RPM (dt * 32.0f) during emergence");
        CHECK(res.any_grind, "Emergence grinding teeth events must be emitted");

        // Advance past 1.2s breach threshold -> transitions into Charging
        manager.update(0.9f, player_pos, world, {}, false); // total 1.3s
        CHECK(b.state == BurrowerState::Charging, "Burrower must transition from Breaching to Charging after 1.2s");
        CHECK(!b.is_emerging(), "is_emerging() should return false once in Charging state");

        std::cout << " -> Breaching emergence, high-RPM cutter teeth spin, and kinetic charge transition verified." << std::endl;
    }

    // Test 13: Charging State Destroys Voxels in Path
    {
        std::cout << "[Test 13] Testing Charging State Destroys Voxels In Path..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(20.0f, 20.0f, 20.0f);
        manager.spawn_burrower(spawn_pos);

        auto& b = manager.burrowers_mut()[0];
        b.state = BurrowerState::Charging;
        b.yaw = 1.5707963f; // Facing +X
        b.pitch = 0.0f;

        // Place destructible basalt block in front of charge trajectory
        world.set_voxel(22, 20, 20, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
        world.set_voxel(23, 20, 20, Voxel{MAT_FRACTURED_GRANITE, 0}, false);

        glm::vec3 player_pos(30.0f, 20.0f, 20.0f);
        auto res = manager.update(0.3f, player_pos, world);

        // Blocks in path should be destroyed / excavated during kinetic charge
        Voxel v1 = world.get_voxel(22, 20, 20);
        CHECK(v1.material_id == MAT_AIR, "Expected destructible basalt to be obliterated in charge path");
        CHECK(!res.excavated_voxels.empty(), "Expected excavated voxel deltas emitted during charge destruction");
        CHECK(res.any_grind, "Grinding cutter teeth cues should be emitted when smashing voxels during charge");

        std::cout << " -> Charging state terrain destruction verified." << std::endl;
    }

    // Test 14: Relentless Non-Retreating Combat Tenacity (No Scare / No Flee)
    {
        std::cout << "[Test 14] Testing Relentless Non-Retreating Combat Tenacity..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(25.0f, 20.0f, 25.0f);
        manager.spawn_burrower(spawn_pos);

        auto& b = manager.burrowers_mut()[0];
        b.state = BurrowerState::Breaching;
        b.hp = 100.0f;

        // Deal heavy damage bringing HP down to 25 (< 35 critical threshold)
        glm::vec3 attacker(25.0f, 20.0f, 35.0f);
        bool hit = manager.damage_nearest(spawn_pos, 3.0f, 75.0f, false, false, nullptr, nullptr, &attacker);
        CHECK(hit, "Expected hit on burrower");
        CHECK(b.hp == 25.0f, "Expected burrower HP to be 25.0");

        // The burrower must NOT flee into retreat; it enrages instead!
        CHECK(b.state == BurrowerState::Enraged, "Wounded burrower must NOT retreat into hiding; it should enter Enraged state");
        CHECK(b.is_pursuing_attacker, "Burrower must remain locked onto attacker");

        std::cout << " -> Relentless non-retreating combat tenacity verified." << std::endl;
    }

    // Test 15: 3D Vertical Elevation Targeting Accuracy
    {
        std::cout << "[Test 15] Testing 3D Vertical Elevation Targeting Accuracy..." << std::endl;
        // 15a: Target elevated above burrower
        {
            SeismicBurrowerManager manager;
            glm::vec3 spawn_pos(20.0f, 20.0f, 20.0f);
            manager.spawn_burrower(spawn_pos);

            world.set_voxel(20, 20, 20, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
            world.set_voxel(20, 21, 20, Voxel{MAT_VOLCANIC_BASALT, 0}, false);

            glm::vec3 elevated_player(20.0f, 35.0f, 20.0f);
            for (int i = 0; i < 5; ++i) {
                manager.update(0.2f, elevated_player, world);
            }

            const auto& b_up = manager.burrowers()[0];
            CHECK(b_up.forward().y > 0.8f, "Burrower forward direction Y must be positive when target is elevated above");
            CHECK(b_up.velocity.y > 1.0f, "Burrower velocity Y must be positive when target is elevated above");
        }

        // 15b: Target located below burrower
        {
            SeismicBurrowerManager manager;
            glm::vec3 spawn_pos(20.0f, 20.0f, 20.0f);
            manager.spawn_burrower(spawn_pos);

            world.set_voxel(20, 20, 20, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
            world.set_voxel(20, 19, 20, Voxel{MAT_VOLCANIC_BASALT, 0}, false);

            glm::vec3 depressed_player(20.0f, 5.0f, 20.0f);
            for (int i = 0; i < 5; ++i) {
                manager.update(0.2f, depressed_player, world);
            }

            const auto& b_down = manager.burrowers()[0];
            CHECK(b_down.forward().y < -0.8f, "Burrower forward direction Y must be negative when target is below");
            CHECK(b_down.velocity.y < -1.0f, "Burrower velocity Y must be negative when target is below");
        }

        std::cout << " -> 3D vertical elevation targeting accuracy verified." << std::endl;
    }

    // Test 16: Crouched Delver Sneak Approach & Unalerted Point-Blank Sneak Critical
    {
        std::cout << "[Test 16] Testing Crouched Delver Sneak Approach & Point-Blank Sneak Critical..." << std::endl;
        SeismicBurrowerManager manager;
        glm::vec3 spawn_pos(20.0f, 20.0f, 20.0f);
        manager.spawn_burrower(spawn_pos);

        auto& b = manager.burrowers_mut()[0];
        b.state = BurrowerState::Dormant;
        b.hp = 100.0f;

        CHECK(b.is_unalerted(), "Initial dormant burrower must be unalerted");

        // Player approaches closely (2.0m away) while crouched (player_is_crouching = true)
        glm::vec3 crouch_delver_pos(20.0f, 20.0f, 22.0f); // 2m distance
        manager.update(0.1f, crouch_delver_pos, world, {}, true);

        // Must remain Dormant and unalerted because crouching delver generates 0 sound and is > 0.9m collision
        CHECK(b.state == BurrowerState::Dormant, "Crouching delver at 2.0m must NOT alert or wake dormant burrower");
        CHECK(b.is_unalerted(), "Burrower must remain unalerted when crept upon by crouching delver");

        // Land a sneak attack melee strike with allow_crit = true
        bool is_crit = false;
        float damage_dealt = 0.0f;
        bool hit = manager.damage_nearest(crouch_delver_pos, 3.0f, 20.0f, false, true, &is_crit, &damage_dealt);

        CHECK(hit, "Expected melee strike to hit dormant burrower");
        CHECK(is_crit, "Expected sneak attack critical hit on unalerted dormant burrower");
        CHECK(damage_dealt == 20.0f * StealthSystem::SNEAK_ATTACK_CRIT_MULTIPLIER,
              "Sneak attack must deal multiplier (2.5x) critical damage");

        // Now test standing approach wakes dormant burrower:
        SeismicBurrowerManager manager2;
        manager2.spawn_burrower(spawn_pos);
        auto& b2 = manager2.burrowers_mut()[0];
        b2.state = BurrowerState::Dormant;

        // Player standing at 4.0m (within 8.0m standing wake radius)
        glm::vec3 standing_delver_pos(20.0f, 20.0f, 24.0f);
        manager2.update(0.1f, standing_delver_pos, world, {}, false);
        CHECK(b2.state != BurrowerState::Dormant, "Standing delver at 4.0m must awaken dormant burrower");

        std::cout << " -> Crouched delver sneak approach & unalerted point-blank critical verified." << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "ALL SEISMIC BURROWER TESTS PASSED (16/16)" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}

