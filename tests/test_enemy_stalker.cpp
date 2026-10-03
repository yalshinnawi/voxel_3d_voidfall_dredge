#include <iostream>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include "../src/entities/enemies/void_stalker.hpp"
#include "../src/voxel/world.hpp"

using namespace Voidfall;

#define CHECK(expr, msg) \
    if (!(expr)) { \
        std::cerr << "[TEST FAILURE] " << msg << " (" #expr ") at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "RUNNING VOID STALKER ENEMY UNIT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    World world;
    world.generate_world(1, 1337);

    // Carve open testing chamber to guarantee line of sight
    for (int x = 10; x <= 22; ++x) {
        for (int y = 18; y <= 25; ++y) {
            for (int z = 10; z <= 32; ++z) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
            }
        }
    }

    // Test 1: Spawning, Initial Attributes, and Idle State
    {
        std::cout << "[Test 1] Testing Void Stalker Spawning & Initial State..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 spawn_pos(16.0f, 20.0f, 16.0f);
        manager.spawn_melee(spawn_pos);

        CHECK(manager.stalkers().size() == 1, "Expected 1 stalker spawned");
        CHECK(manager.active_count() == 1, "Expected active count == 1");

        const auto& stalker = manager.stalkers()[0];
        CHECK(stalker.hp == 40.0f, "Expected baseline 40.0 HP");
        CHECK(stalker.max_hp == 40.0f, "Expected baseline max_hp == 40.0");
        CHECK(stalker.state == StalkerState::Idle, "Initial state should be Idle");
        CHECK(stalker.role == StalkerRole::Melee, "Expected Melee role");
        CHECK(stalker.is_melee(), "is_melee() helper should be true");
        CHECK(!stalker.is_shooter(), "is_shooter() helper should be false for Melee");
        CHECK(!stalker.is_dead(), "Stalker should not be dead on spawn");

        // Emissive Eye color in Idle for Melee is Predatory Crimson Red
        glm::vec4 eye_col = stalker.get_eye_color();
        CHECK(eye_col.r > 0.7f && eye_col.g < 0.2f, "Expected predatory red eye color for Melee in Idle state");

        // Test Shooter spawn
        manager.spawn_shooter(spawn_pos + glm::vec3(2.0f, 0.0f, 0.0f));
        const auto& shooter = manager.stalkers()[1];
        CHECK(shooter.role == StalkerRole::Shooter, "Expected Shooter role");
        CHECK(shooter.is_shooter(), "is_shooter() helper should be true");
        CHECK(!shooter.is_melee(), "is_melee() helper should be false for Shooter");
        glm::vec4 shooter_eye = shooter.get_eye_color();
        CHECK(shooter_eye.g > 0.7f && shooter_eye.r < 0.3f, "Expected neon toxic green eye color for Shooter in Idle state");

        std::cout << " -> Stalker initial spawn invariants (Melee & Shooter) verified." << std::endl;
    }

    // Test 2: Perception, Line-of-Sight, and State Progression to Stalking
    {
        std::cout << "[Test 2] Testing Stalker AI Perception & Stalking State..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 player_pos(16.0f, 20.0f, 16.0f);
        glm::vec3 stalker_pos(16.0f, 20.0f, 26.0f); // 10 units away (within 18m detection range)
        manager.spawn_melee(stalker_pos);

        glm::vec3 player_forward(0.0f, 0.0f, -1.0f); // Looking away from stalker
        glm::vec3 headlamp_dir(0.0f, 0.0f, -1.0f);
        bool headlamp_on = false;
        float noise = 55.0f; // Above 30% perception gate
        bool drilling = false;

        auto res = manager.update(0.1f, player_pos, player_forward, headlamp_dir, headlamp_on, noise, drilling, world);

        const auto& stalker = manager.stalkers()[0];
        CHECK(stalker.state == StalkerState::Stalking, "Stalker within range should transition to Stalking");
        CHECK(stalker.target_pos == player_pos, "Stalker should track player position");

        // Melee stalking eye color is crimson red
        glm::vec4 melee_eye = stalker.get_eye_color();
        CHECK(melee_eye.r > 0.8f && melee_eye.g < 0.2f, "Expected crimson red eye color during Melee stalking");

        // Spawn Shooter and verify toxic emerald green stalking eye color
        manager.spawn_shooter(stalker_pos + glm::vec3(3.0f, 0.0f, 0.0f));
        manager.update(0.1f, player_pos, player_forward, headlamp_dir, headlamp_on, noise, drilling, world);
        const auto& shooter = manager.stalkers()[1];
        CHECK(shooter.state == StalkerState::Stalking, "Shooter within range should transition to Stalking");
        glm::vec4 shooter_eye = shooter.get_eye_color();
        CHECK(shooter_eye.g > 0.8f && shooter_eye.r < 0.3f, "Expected emerald green eye color during Shooter stalking");

        std::cout << " -> Perception & stalking state transitions (Melee Red vs Shooter Green) verified." << std::endl;
    }

    // Test 3: Circling Flank, Void Spine Projectiles, and Aggressive Counter-Lunge
    {
        std::cout << "[Test 3] Testing Circling Flank, Void Spines & Aggressive Counter-Lunge..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 player_pos(16.0f, 20.0f, 16.0f);

        // 3a: Circling at mid-range (8.5m) and firing Void Spine projectiles
        glm::vec3 stalker_pos(16.0f, 20.0f, 24.5f); // 8.5 units away (in circling band 6.8m - 12m)
        manager.spawn_shooter(stalker_pos);

        manager.stalkers_mut()[0].state = StalkerState::Stalking;
        manager.stalkers_mut()[0].projectile_cooldown = 0.0f; // Force weapon ready
        auto res_circle = manager.update(0.1f, player_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 40.0f, false, world);
        CHECK(manager.stalkers()[0].state == StalkerState::Circling, "Expected transition to Circling at mid range (8.5m)");
        CHECK(res_circle.any_projectile_fired, "Stalker circling at mid-range should launch void spine projectiles");
        CHECK(!manager.projectiles().empty(), "Projectile manager should contain active void spine needles");
        CHECK(manager.projectiles()[0].active, "Fired projectile must be active");
        CHECK(manager.projectiles()[0].damage == 10.0f, "Void spine damage should be 10.0 HP");

        // 3b: Aggressive Counter-Lunge when Player Approaches (< 6.8m)
        // User Requirement: When player runs toward it, stalker must NOT run away! It must aggressively counter-lunge!
        manager.stalkers_mut()[0].position = player_pos + glm::vec3(0.0f, 0.0f, 5.5f); // 5.5m away (< 6.8m)
        manager.stalkers_mut()[0].state = StalkerState::Stalking;
        manager.stalkers_mut()[0].role = StalkerRole::Melee;
        auto res_approach = manager.update(0.1f, player_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), true, 50.0f, false, world);
        const auto& stalker = manager.stalkers()[0];
        CHECK(stalker.state == StalkerState::Lunging, "Approaching stalker (< 6.8m) even with headlamp MUST trigger aggressive counter-lunge, NOT flee!");
        CHECK(stalker.just_lunged, "just_lunged flag should be raised");

        // Crimson bloom eye color during lunge
        glm::vec4 lunge_eye = stalker.get_eye_color();
        CHECK(lunge_eye.r > 0.9f && lunge_eye.g < 0.2f, "Expected crimson eye color during Lunging attack");

        std::cout << " -> Circling, void spine launch, and aggressive counter-lunge verified." << std::endl;
    }

    // Test 4: Sonar Shockwave Stun & Headlamp Deterrence
    {
        std::cout << "[Test 4] Testing Sonar Shockwave Stun & Light Deterrence..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 player_pos(16.0f, 20.0f, 16.0f);
        glm::vec3 stalker_pos(16.0f, 20.0f, 21.0f); // 5 units away
        manager.spawn_stalker(stalker_pos);

        // Apply Sonar Pulse stun within 15m radius
        manager.apply_sonar_stun(player_pos, 15.0f);
        const auto& stalker = manager.stalkers()[0];
        CHECK(stalker.state == StalkerState::Stunned, "Sonar blast must stun stalker within radius");
        CHECK(stalker.stun_timer > 0.0f, "Stun timer must be positive");

        // Stunned eye color is electric cyan shock
        glm::vec4 stun_eye = stalker.get_eye_color();
        CHECK(stun_eye.b > 0.8f && stun_eye.g > 0.8f && stun_eye.r < 0.3f, "Expected electric cyan eye color during Stunned state");

        // While stunned, update decrements timer
        float initial_timer = stalker.stun_timer;
        manager.update(0.5f, player_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), true, 10.0f, false, world);
        CHECK(manager.stalkers()[0].stun_timer < initial_timer, "Stun timer should decrement across updates");

        std::cout << " -> Sonar shockwave stun mechanics verified." << std::endl;
    }

    // Test 5: Interactive Combat - Enemy Attacks Player & Player Damages Enemy
    {
        std::cout << "[Test 5] Testing Interactive Combat (Enemy Attacks & Player Damage)..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 player_pos(16.0f, 20.0f, 16.0f);
        glm::vec3 stalker_pos(16.0f, 20.0f, 17.2f); // 1.2 units away (within melee attack reach < 1.6m)
        manager.spawn_stalker(stalker_pos);

        // 5a: Stalker Melee Attack against Player
        manager.stalkers_mut()[0].state = StalkerState::Lunging;
        auto attack_res = manager.update(0.05f, player_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 50.0f, false, world);
        CHECK(attack_res.any_melee_hit, "Stalker reaching player in lunge must land melee claw strike");
        CHECK(attack_res.total_damage >= 14.0f, "Melee strike must deal lethal damage (>= 14 HP)");
        CHECK(manager.stalkers()[0].slash_fx_timer > 0.0f, "Melee strike must trigger razor claw slash visual timer");

        // 5b: Player Damages Stalker (Drill grinding / Demolitions)
        // Non-lethal damage when healthy: Stalker does NOT flee! It becomes enraged and counter-attacks!
        manager.stalkers_mut()[0].position = player_pos + glm::vec3(0.0f, 0.0f, 4.0f);
        manager.stalkers_mut()[0].state = StalkerState::Circling;
        bool hit = manager.damage_nearest(player_pos + glm::vec3(0.0f, 0.0f, 3.5f), 2.5f, 15.0f); // 40 HP -> 25 HP
        CHECK(hit, "Damage nearest should hit stalker in drill radius");
        CHECK(manager.stalkers()[0].hp == 25.0f, "Stalker HP should be 25.0");
        CHECK(manager.stalkers()[0].state == StalkerState::Lunging, "Healthy stalker (> 12 HP) damaged by player should immediately counter-attack, NOT flee!");

        // 5c: Critical damage (< 12 HP): Stalker retreats to shadows
        manager.damage_nearest(player_pos + glm::vec3(0.0f, 0.0f, 3.5f), 5.0f, 18.0f); // 25 HP -> 7 HP (< 12 HP)
        CHECK(manager.stalkers()[0].hp == 7.0f, "Stalker HP should now be 7.0");
        manager.update(0.1f, player_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), true, 10.0f, false, world);
        CHECK(manager.stalkers()[0].state == StalkerState::Fleeing, "Stalker below flee threshold (7 HP <= 12 HP) must enter Fleeing state");
        
        // Amber eye color when fleeing
        glm::vec4 flee_eye = manager.stalkers()[0].get_eye_color();
        CHECK(flee_eye.r > 0.9f && flee_eye.g > 0.5f && flee_eye.b < 0.2f, "Expected amber eye color during Fleeing state");

        // 5d: Final lethal blow
        manager.damage_nearest(player_pos, 10.0f, 15.0f);
        CHECK(manager.stalkers()[0].is_dead(), "Stalker at 0 HP should be marked Dead");

        // Remove dead
        manager.remove_dead();
        CHECK(manager.active_count() == 0, "Active count must be 0 after dead cleanup");
        CHECK(manager.stalkers().empty(), "Stalkers list should be empty after removing dead");

        std::cout << " -> Interactive combat (attacks, counter-attacks, fleeing & elimination) verified." << std::endl;
    }

    // Test 6: Multi-Stalker Wave Spawning and Spatial Isolation
    {
        std::cout << "[Test 6] Testing Multi-Stalker Wave Spawning..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 player_pos(16.0f, 20.0f, 16.0f);

        manager.spawn_wave(player_pos, 3, world);
        CHECK(manager.active_count() == 3, "Expected 3 stalkers spawned in wave");

        // Verify each has distinct ID and valid position
        const auto& list = manager.stalkers();
        CHECK(list[0].id != list[1].id && list[1].id != list[2].id, "Stalkers must have unique IDs");

        for (const auto& s : list) {
            float dist = glm::distance(s.position, player_pos);
            CHECK(dist >= 5.0f, "Spawned stalker should not spawn on top of player");
        }

        std::cout << " -> Multi-stalker wave spawning verified." << std::endl;
    }

    // Test 7: Ambient Roaming Patrol & Stealth / Un-noised Proximity Detection
    {
        std::cout << "[Test 7] Testing Ambient Cavern Roaming & Un-noised Proximity Detection..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 chamber_pos(16.0f, 20.0f, 22.0f);
        manager.spawn_ambient_stalker(chamber_pos, world);

        CHECK(manager.active_count() == 1, "Expected 1 ambient stalker spawned in chamber");
        const auto& s_initial = manager.stalkers()[0];
        CHECK(s_initial.state == StalkerState::Idle, "Ambient stalker must start in Idle / Roaming state");

        // 7a: Verify slow roaming prowl movement in Idle without player nearby
        glm::vec3 far_player(60.0f, 20.0f, 60.0f);
        glm::vec3 start_pos = s_initial.position;
        // Update several ticks with 0 noise and player far away
        for (int step = 0; step < 20; ++step) {
            manager.update(0.1f, far_player, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world);
        }
        const auto& s_roaming = manager.stalkers()[0];
        CHECK(s_roaming.state == StalkerState::Idle, "Stalker should remain in Idle roaming while player is distant");
        CHECK(s_roaming.walk_cycle > 0.0f, "Articulated leg crawl cycle must animate during prowling");

        // 7b: Un-noised Proximity Detection (Player approaches to 5.5m with ZERO noise)
        glm::vec3 close_player = s_roaming.position + glm::vec3(0.0f, 0.0f, 5.5f); // 5.5m away (< 6.2m proximity threshold)
        auto prox_res = manager.update(0.1f, close_player, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world);

        const auto& s_alerted = manager.stalkers()[0];
        CHECK(s_alerted.state == StalkerState::Stalking, "Stalker must detect player approaching within 6.2m even with ZERO noise!");
        CHECK(prox_res.any_spotted, "any_spotted flag must be raised upon un-noised proximity detection");

        // 7c: Direct Surprise Ambush Lunge when player stumbles within 3.8m
        manager.stalkers_mut()[0].state = StalkerState::Idle;
        glm::vec3 ambush_player = s_alerted.position + glm::vec3(0.0f, 0.0f, 3.8f); // 3.8m away (< 4.2m ambush threshold)
        auto ambush_res = manager.update(0.1f, ambush_player, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world);
        CHECK(manager.stalkers()[0].state == StalkerState::Lunging, "Stumbling within 4.2m must provoke an immediate ambush counter-lunge!");
        CHECK(ambush_res.any_lunge, "any_lunge flag must be raised on surprise ambush");

        std::cout << " -> Ambient roaming and un-noised proximity detection verified." << std::endl;
    }

    // Test 8: Subtle & Scary Audio Behaviors (Mandible Chitters, Throat Hiss & Screech Throttling)
    {
        std::cout << "[Test 8] Testing Stealth Mandible Chitters, Throat Hiss & Screech Throttling..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 player_pos(16.0f, 20.0f, 16.0f);
        glm::vec3 stalker_pos(16.0f, 20.0f, 26.0f); // 10m away
        manager.spawn_melee(stalker_pos);

        // 8a: Throat Hiss on initial perception transition from Idle to Stalking
        auto hiss_res = manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 45.0f, false, world);
        CHECK(hiss_res.any_spotted, "Initial perception transition into Stalking must trigger spotted flag for throat hiss!");

        // 8b: Stealth Mandible Chittering while stalking in darkness
        // Set chitter_timer close to expiration
        manager.stalkers_mut()[0].chitter_timer = 0.05f;
        auto chitter_res = manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 10.0f, false, world);
        CHECK(chitter_res.any_chitter, "Stalking in darkness without headlamp illumination must emit subtle mandible chittering!");

        // 8c: Global Screech Cooldown: Multiple stalkers must NOT spam screeches
        VoidStalkerManager multi_mgr;
        multi_mgr.spawn_melee(glm::vec3(16.0f, 20.0f, 38.0f)); // 22m away
        multi_mgr.spawn_melee(glm::vec3(16.0f, 20.0f, 42.0f)); // 26m away
        multi_mgr.spawn_melee(glm::vec3(16.0f, 20.0f, 46.0f)); // 30m away

        int total_screeches = 0;
        // Simulate 20 seconds (200 ticks of 0.1s)
        for (int step = 0; step < 200; ++step) {
            auto frame = multi_mgr.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world);
            if (frame.any_screech) {
                total_screeches++;
            }
        }
        CHECK(total_screeches <= 1, "Global screech cooldown (35-55s) must prevent noisy screech spam across multiple stalkers!");

        std::cout << " -> Stealth chittering, throat hiss, and screech rate-limiting verified." << std::endl;
    }

    // Test 9: Acoustic Sound Distraction & Spatial Investigation (Revamped Sound Perception)
    {
        std::cout << "[Test 9] Testing Acoustic Sound Distraction & Spatial Investigation..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 stalker_pos(16.0f, 20.0f, 20.0f);
        manager.spawn_melee(stalker_pos);

        // Player is far away and crouching silently
        glm::vec3 player_pos(16.0f, 20.0f, 40.0f);
        glm::vec3 distraction_pos(20.0f, 20.0f, 12.0f); // Distant wall where bullet struck

        std::vector<SoundEvent> sounds;
        SoundEvent bullet_impact;
        bullet_impact.type = SoundEventType::BulletImpact;
        bullet_impact.position = distraction_pos;
        bullet_impact.intensity = 60.0f;
        bullet_impact.audible_radius = 28.0f;
        bullet_impact.age = 0.0f;
        bullet_impact.lifetime = 1.0f;
        sounds.push_back(bullet_impact);

        // Update with sounds and player crouching
        auto frame = manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world, sounds, true);
        const auto& s = manager.stalkers()[0];

        CHECK(s.state == StalkerState::Investigating, "Stalker hearing acoustic sound must transition to Investigating state");
        CHECK(glm::distance(s.target_pos, distraction_pos) < 0.1f, "Stalker target must be set to sound location, NOT player position!");
        CHECK(s.investigation_target == distraction_pos, "investigation_target must match sound coordinates");
        CHECK(s.investigation_timer > 0.0f, "Investigation timer must be activated");

        // Emissive eye color in Investigating state
        glm::vec4 eye_col = s.get_eye_color();
        CHECK(eye_col.r > 0.8f && eye_col.g > 0.4f, "Expected amber/orange investigation eye color for Melee stalker");

        // Simulate stalker moving towards distraction
        float initial_dist = glm::distance(s.position, distraction_pos);
        for (int step = 0; step < 15; ++step) {
            manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world, {}, true);
        }
        CHECK(glm::distance(manager.stalkers()[0].position, distraction_pos) < initial_dist, "Stalker must physically move toward the acoustic disturbance location!");

        std::cout << " -> Acoustic distraction and spatial sound investigation verified." << std::endl;
    }

    // Test 10: Strict Line-of-Sight Gating for Combat Attacks & Crouching Concealment
    {
        std::cout << "[Test 10] Testing Strict Line-of-Sight Gating & Crouching Concealment..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 stalker_pos(16.0f, 20.0f, 26.0f);
        glm::vec3 player_pos(16.0f, 20.0f, 16.0f); // 10m away
        manager.spawn_melee(stalker_pos);

        // 10a: Place a solid barrier wall between player and stalker at z = 20
        for (int x = 12; x <= 20; ++x) {
            for (int y = 18; y <= 24; ++y) {
                world.set_voxel(x, y, 20, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
            }
        }

        // Player makes loud noise behind the wall
        std::vector<SoundEvent> sounds;
        SoundEvent drill_sound;
        drill_sound.type = SoundEventType::DrillVibration;
        drill_sound.position = player_pos;
        drill_sound.intensity = 70.0f;
        drill_sound.audible_radius = 35.0f;
        drill_sound.age = 0.0f;
        drill_sound.lifetime = 1.0f;
        sounds.push_back(drill_sound);

        // Update: Stalker hears noise behind wall. Since LOS is blocked by granite, it should investigate, NOT direct attack!
        manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 70.0f, true, world, sounds, false);
        const auto& s_blocked = manager.stalkers()[0];
        CHECK(s_blocked.state == StalkerState::Investigating, "Stalker behind solid rock wall must enter Investigating, NOT direct attack (LOS is blocked)");
        CHECK(s_blocked.state != StalkerState::Lunging && s_blocked.state != StalkerState::Circling, "Cannot lunge or circle without Line of Sight!");

        // 10b: Demolish the barrier wall to open direct Line of Sight
        for (int x = 12; x <= 20; ++x) {
            for (int y = 18; y <= 24; ++y) {
                world.set_voxel(x, y, 20, Voxel{MAT_AIR, 0}, false);
            }
        }

        // Update with open LOS: Stalker visually spots player and transitions to Stalking/Attack!
        manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 40.0f, false, world, {}, false);
        const auto& s_spotted = manager.stalkers()[0];
        CHECK(s_spotted.state == StalkerState::Stalking, "Stalker with clear Line of Sight must transition to Stalking attack mode!");

        std::cout << " -> Line-of-sight gating and occlusion-aware stealth mechanics verified." << std::endl;
    }

    // Test 11: Retaliation Pursuit When Attacked & Escape Duration Break
    {
        std::cout << "[Test 11] Testing Retaliation Pursuit When Attacked & Escape Duration Break..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 stalker_pos(16.0f, 20.0f, 25.0f);
        glm::vec3 player_pos(16.0f, 20.0f, 22.0f);
        manager.spawn_melee(stalker_pos);

        auto& stalker = manager.stalkers_mut()[0];
        stalker.state = StalkerState::Idle;
        CHECK(!stalker.is_pursuing_attacker, "Stalker should not be in pursuit initially");

        // 11a: Player damages stalker with drill or weapon
        bool hit = manager.damage_nearest(stalker_pos, 2.0f, 15.0f);
        CHECK(hit, "Expected attack to hit stalker");
        CHECK(stalker.is_pursuing_attacker, "Damaged stalker must enter is_pursuing_attacker state");
        CHECK(stalker.pursuit_lost_timer == 0.0f, "pursuit_lost_timer must reset to 0 upon attack");

        // 11b: Player flees to a distance beyond normal detection (e.g. 24m)
        glm::vec3 fleeing_pos(16.0f, 20.0f, 49.0f); // 24m separation
        // Update for 5 seconds: enemy must CONTINUE pursuing attacker, NOT lose interest!
        for (int i = 0; i < 50; ++i) {
            manager.update(0.1f, fleeing_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world, {}, false);
        }
        CHECK(manager.stalkers()[0].is_pursuing_attacker, "Stalker must still pursue attacker while within pursuit radius");
        CHECK(manager.stalkers()[0].state != StalkerState::Idle, "Stalker must NOT drop to Idle during active retaliation");

        // 11c: Player runs far away (> 35m) for a short time (e.g. 4 seconds)
        for (int i = 0; i < 40; ++i) {
            glm::vec3 distant_pos = manager.stalkers()[0].position + glm::vec3(0.0f, 0.0f, 40.0f);
            manager.update(0.1f, distant_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world, {}, false);
        }
        CHECK(manager.stalkers()[0].is_pursuing_attacker, "Stalker must still be hunting attacker after only 4s of separation (< 12s break time)");

        // 11d: Player stays far away for an extended duration (> 12.0s total)
        for (int i = 0; i < 90; ++i) { // 9 more seconds = 13s total far away
            glm::vec3 distant_pos = manager.stalkers()[0].position + glm::vec3(0.0f, 0.0f, 40.0f);
            manager.update(0.1f, distant_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world, {}, false);
        }
        CHECK(!manager.stalkers()[0].is_pursuing_attacker, "Stalker should finally lose interest after player ran far away for > 12s");
        CHECK(manager.stalkers()[0].state == StalkerState::Idle, "Stalker must return to Idle after losing interest in pursuit");

        std::cout << " -> Retaliation pursuit and prolonged escape break verified." << std::endl;
    }

    std::cout << "\n>>> ALL 11 VOID STALKER TEST MODULES PASSED SUCCESSFULLY! <<<\n" << std::endl;
    return 0;
}


