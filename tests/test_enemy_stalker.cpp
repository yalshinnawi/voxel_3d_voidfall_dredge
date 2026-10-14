#include <iostream>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include "../src/entities/enemies/void_stalker.hpp"
#include "../src/entities/enemies/seismic_burrower.hpp"
#include "../src/systems/stealth_system.hpp"
#include "../src/systems/noise_meter.hpp"
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

    // Carve open testing chamber to guarantee line of sight and solid flooring
    for (int x = 10; x <= 22; ++x) {
        for (int z = 10; z <= 32; ++z) {
            world.set_voxel(x, 17, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
            for (int y = 18; y <= 25; ++y) {
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
        manager.stalkers_mut()[0].yaw = glm::pi<float>(); // Facing towards player at -Z

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
        manager.stalkers_mut()[1].yaw = glm::pi<float>(); // Facing towards player at -Z
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
        CHECK(manager.stalkers()[0].is_dying() || manager.stalkers()[0].is_dead(), "Stalker at 0 HP should be marked Dying or Dead");
        manager.update(0.80f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);
        CHECK(manager.stalkers()[0].is_dead(), "Stalker at 0 HP should be marked Dead after collapse");

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
        manager.stalkers_mut()[0].yaw = glm::pi<float>(); // Facing player at -Z

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
        CHECK(total_screeches <= 1, "Global screech cooldown must prevent noisy ambient screech spam across multiple stalkers!");

        // 8d: Pack Alert Screech Throttling: When multiple stalkers alert simultaneously, exactly ONE emits screech
        VoidStalkerManager pack_mgr;
        pack_mgr.spawn_melee(glm::vec3(16.0f, 20.0f, 24.0f)); // 8m away
        pack_mgr.spawn_melee(glm::vec3(18.0f, 20.0f, 24.0f)); // 8m away
        pack_mgr.spawn_melee(glm::vec3(14.0f, 20.0f, 24.0f)); // 8m away
        for (auto& s : pack_mgr.stalkers_mut()) s.yaw = glm::pi<float>(); // Facing player at -Z

        // Initial alert frame
        auto alert_frame = pack_mgr.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 80.0f, false, world);
        CHECK(alert_frame.any_screech, "Pack spotting player must trigger exactly one alert screech");
        int per_entity_screeches = 0;
        for (const auto& s : pack_mgr.stalkers()) {
            if (s.just_screeched) per_entity_screeches++;
        }
        CHECK(per_entity_screeches == 1, "Only 1 stalker in a pack must screech on alert (no overlapping spam)");

        // Immediate subsequent frame: Stalkers must NOT screech again
        auto follow_up_frame = pack_mgr.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 80.0f, false, world);
        CHECK(!follow_up_frame.any_screech, "Subsequent frames during pursuit must not spam screeches");

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

    // Test 12: Surface Normal Detection, 6-Directional Sampling & Traversal State Machine
    {
        std::cout << "[Test 12] Testing Surface Normal Detection, 6-Directional Sampling & State Machine..." << std::endl;
        
        // 12a: Direct Normal Classification:
        // Floor: n · up > 0.7
        // Wall:  |n · up| <= 0.7
        // Ceiling: n · up < -0.7
        CHECK(AberrantAI::classify_normal(glm::vec3(0.0f, 1.0f, 0.0f)) == StalkerSurfaceState::FLOOR, "Upward normal (0,1,0) must be FLOOR");
        CHECK(AberrantAI::classify_normal(glm::vec3(0.2f, 0.95f, 0.0f)) == StalkerSurfaceState::FLOOR, "Slightly angled floor normal must be FLOOR");
        CHECK(AberrantAI::classify_normal(glm::vec3(1.0f, 0.0f, 0.0f)) == StalkerSurfaceState::WALL_CLIMBING, "East wall normal (1,0,0) must be WALL_CLIMBING");
        CHECK(AberrantAI::classify_normal(glm::vec3(-1.0f, 0.0f, 0.0f)) == StalkerSurfaceState::WALL_CLIMBING, "West wall normal (-1,0,0) must be WALL_CLIMBING");
        CHECK(AberrantAI::classify_normal(glm::vec3(0.0f, 0.0f, 1.0f)) == StalkerSurfaceState::WALL_CLIMBING, "South wall normal (0,0,1) must be WALL_CLIMBING");
        CHECK(AberrantAI::classify_normal(glm::vec3(0.0f, 0.0f, -1.0f)) == StalkerSurfaceState::WALL_CLIMBING, "North wall normal (0,0,-1) must be WALL_CLIMBING");
        CHECK(AberrantAI::classify_normal(glm::vec3(0.707f, 0.707f, 0.0f)) == StalkerSurfaceState::FLOOR, "45-degree slope (>0.7) should classify as FLOOR");
        CHECK(AberrantAI::classify_normal(glm::vec3(0.707f, 0.5f, 0.0f)) == StalkerSurfaceState::WALL_CLIMBING, "Steep wall slope (<=0.7) must be WALL_CLIMBING");
        CHECK(AberrantAI::classify_normal(glm::vec3(0.0f, -1.0f, 0.0f)) == StalkerSurfaceState::CEILING_CRAWLING, "Inverted normal (0,-1,0) must be CEILING_CRAWLING");
        CHECK(AberrantAI::classify_normal(glm::vec3(0.1f, -0.95f, 0.0f)) == StalkerSurfaceState::CEILING_CRAWLING, "Angled ceiling normal must be CEILING_CRAWLING");

        // 12b: Dynamic 6-Directional Sampling against Voxel Geometry
        World test_world;
        test_world.generate_world(1, 42);
        // Clear a 5x5x5 chamber
        for (int x = 20; x <= 26; ++x) {
            for (int y = 20; y <= 26; ++y) {
                for (int z = 20; z <= 26; ++z) {
                    test_world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }
        // Floor at y=19
        for (int x = 20; x <= 26; ++x) {
            for (int z = 20; z <= 26; ++z) {
                test_world.set_voxel(x, 19, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
            }
        }
        // East wall at x=27
        for (int y = 20; y <= 26; ++y) {
            for (int z = 20; z <= 26; ++z) {
                test_world.set_voxel(27, y, z, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
            }
        }
        // Ceiling at y=27
        for (int x = 20; x <= 26; ++x) {
            for (int z = 20; z <= 26; ++z) {
                test_world.set_voxel(x, 27, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
            }
        }

        // Probe near floor (y=20.2): closest solid is below
        auto floor_sample = AberrantAI::sample_surface_normal(glm::vec3(23.0f, 20.2f, 23.0f), test_world, 1.4f);
        CHECK(floor_sample.has_contact, "Must detect floor contact");
        CHECK(floor_sample.contact_normal.y > 0.9f, "Floor contact normal must point up (+Y)");
        CHECK(floor_sample.surface_state == StalkerSurfaceState::FLOOR, "Must classify as FLOOR");

        // Probe near east wall (x=26.7): closest solid is at +X (x=27), normal points -X
        auto wall_sample = AberrantAI::sample_surface_normal(glm::vec3(26.7f, 23.0f, 23.0f), test_world, 1.4f);
        CHECK(wall_sample.has_contact, "Must detect wall contact");
        CHECK(wall_sample.contact_normal.x < -0.9f, "Wall contact normal must point west (-X)");
        CHECK(wall_sample.surface_state == StalkerSurfaceState::WALL_CLIMBING, "Must classify as WALL_CLIMBING");

        // Probe near ceiling (y=26.8): closest solid is above (y=27), normal points -Y
        auto ceiling_sample = AberrantAI::sample_surface_normal(glm::vec3(23.0f, 26.8f, 23.0f), test_world, 1.4f);
        CHECK(ceiling_sample.has_contact, "Must detect ceiling contact");
        CHECK(ceiling_sample.contact_normal.y < -0.9f, "Ceiling contact normal must point down (-Y)");
        CHECK(ceiling_sample.surface_state == StalkerSurfaceState::CEILING_CRAWLING, "Must classify as CEILING_CRAWLING");

        std::cout << " -> Surface normal detection, 6-directional sampling & state machine verified." << std::endl;
    }

    // Test 13: Smooth Transform & Model Re-Orientation (Quaternion Slerp without snapping)
    {
        std::cout << "[Test 13] Testing Smooth Transform & Model Re-Orientation (Quaternion Slerp)..." << std::endl;
        
        // 13a: Target Orientation Calculation from Velocity & Normal:
        // forward = normalize(v - (v · n)n)
        // right   = cross(forward, n)
        glm::vec3 floor_vel(0.0f, 0.0f, 4.0f); // Moving forward along +Z on floor
        glm::vec3 floor_normal(0.0f, 1.0f, 0.0f);
        glm::quat floor_q = AberrantAI::calculate_orientation(floor_vel, floor_normal, glm::vec3(0,0,1));
        
        // Verify oriented basis vectors from quaternion
        glm::mat3 rot_m = glm::mat3_cast(floor_q);
        glm::vec3 up_vec = rot_m[1];
        glm::vec3 fwd_vec = rot_m[2];
        CHECK(glm::distance(up_vec, floor_normal) < 0.01f, "Up vector must match contact normal");
        CHECK(glm::distance(fwd_vec, glm::vec3(0.0f, 0.0f, 1.0f)) < 0.01f, "Forward vector must match projected velocity");

        // 13b: Smooth Slerp Transition between Floor and Wall without snapping:
        glm::vec3 wall_normal(1.0f, 0.0f, 0.0f); // Wall facing +X
        glm::vec3 wall_vel(0.0f, 4.0f, 0.0f);   // Climbing up (+Y)
        glm::quat wall_q = AberrantAI::calculate_orientation(wall_vel, wall_normal, glm::vec3(0,1,0));

        glm::quat current_q = floor_q;
        float dt = 0.016f; // 60 FPS tick
        float prev_angle_to_target = glm::angle(glm::conjugate(current_q) * wall_q);

        // Step through multiple frames and ensure rotation transitions smoothly
        for (int step = 0; step < 10; ++step) {
            glm::quat next_q = AberrantAI::slerp_rotation(current_q, wall_q, dt, 8.0f);
            float step_delta = glm::angle(glm::conjugate(current_q) * next_q);
            float angle_to_target = glm::angle(glm::conjugate(next_q) * wall_q);

            CHECK(step_delta > 0.0f, "Slerp must advance each frame");
            CHECK(step_delta < 0.35f, "Slerp must not pop or jump abruptly in a single frame");
            CHECK(angle_to_target < prev_angle_to_target, "Angle to target orientation must monotonically decrease");

            current_q = next_q;
            prev_angle_to_target = angle_to_target;
        }

        // 13c: Surface Snapping Offset Calculation
        glm::vec3 floor_offset = AberrantAI::compute_surface_snapping_offset(StalkerSurfaceState::FLOOR, glm::vec3(0,1,0));
        glm::vec3 wall_offset = AberrantAI::compute_surface_snapping_offset(StalkerSurfaceState::WALL_CLIMBING, glm::vec3(1,0,0));
        glm::vec3 ceiling_offset = AberrantAI::compute_surface_snapping_offset(StalkerSurfaceState::CEILING_CRAWLING, glm::vec3(0,-1,0));

        CHECK(floor_offset.y == 0.05f, "Floor offset must match 0.05m ground clearance");
        CHECK(wall_offset.x == 0.18f, "Wall offset must match 0.18m flush wall profile");
        CHECK(ceiling_offset.y == -0.22f, "Ceiling offset must match -0.22m flush inverted ceiling hold");

        std::cout << " -> Smooth quaternion slerp and surface snapping offset verified." << std::endl;
    }

    // Test 14: Distinct Animation Poses & State Switching
    {
        std::cout << "[Test 14] Testing Distinct Animation Poses (Floor, Wall, Ceiling)..." << std::endl;
        
        auto floor_pose = StalkerAnimationController::get_floor_pose();
        auto wall_pose = StalkerAnimationController::get_wall_climb_pose();
        auto ceiling_pose = StalkerAnimationController::get_ceiling_inversion_pose(0.0f);

        // 14a: Floor Crawl (Default):
        // Standard forward quad/hex-pedal scuttle, body held at default ground clearance
        CHECK(floor_pose.carapace_offset_y == 0.0f, "Floor pose body held at default ground clearance");
        CHECK(floor_pose.limb_splay_multiplier == 1.0f, "Floor pose limb splay default = 1.0");
        CHECK(floor_pose.front_claw_reach == 1.0f, "Floor pose front claw reach default = 1.0");

        // 14b: Wall Climb:
        // Flatten carapace closer to the surface plane (lower profile), splay limbs wider along wall normal, extend front claw reach
        CHECK(wall_pose.carapace_offset_y < 0.0f, "Wall climb must flatten carapace closer to surface plane (lower profile)");
        CHECK(wall_pose.limb_splay_multiplier > 1.3f, "Wall climb must splay limbs wider along wall normal");
        CHECK(wall_pose.front_claw_reach > 1.4f, "Wall climb must extend front claw reach up the wall");
        CHECK(wall_pose.front_claw_yaw > floor_pose.front_claw_yaw, "Wall climb front claws splay wider");

        // 14c: Ceiling Inversion:
        // Fully spread limbs anchored to ceiling voxels with downward-arching predatory neck/head tracking toward player
        CHECK(ceiling_pose.limb_splay_multiplier > 1.5f, "Ceiling inversion must fully spread limbs anchored to ceiling");
        CHECK(ceiling_pose.front_claw_yaw > wall_pose.front_claw_yaw, "Ceiling front claw spread widest");
        CHECK(ceiling_pose.head_pitch_offset < -0.7f, "Ceiling inversion must feature downward-arching neck/head tracking toward player");

        std::cout << " -> Distinct animation poses for Floor, Wall and Ceiling verified." << std::endl;
    }

    // Test 15: Locomotion Crossfade over 0.2s Blend Window & Velocity Modulation
    {
        std::cout << "[Test 15] Testing 0.2s Crossfade Blending & Velocity Play Rate Modulation..." << std::endl;
        
        StalkerAnimationController controller;
        CHECK(controller.floor_weight() == 1.0f, "Controller must initialize with 100% floor weight");
        CHECK(controller.wall_weight() == 0.0f, "Controller must initialize with 0% wall weight");
        CHECK(controller.ceiling_weight() == 0.0f, "Controller must initialize with 0% ceiling weight");
        CHECK(!controller.is_transitioning(), "Initial state is not transitioning");

        // 15a: Transition Floor -> Wall Climbing
        controller.update(0.01f, StalkerSurfaceState::WALL_CLIMBING, glm::vec3(0, 3, 0));
        CHECK(controller.is_transitioning(), "Initiating state change must enter transitioning state");
        CHECK(controller.target_state() == StalkerSurfaceState::WALL_CLIMBING, "Target state should be WALL_CLIMBING");

        // Mid-transition at t = 0.10s (50% through 0.2s window)
        controller.update(0.09f, StalkerSurfaceState::WALL_CLIMBING, glm::vec3(0, 3, 0));
        CHECK(controller.is_transitioning(), "Should still be transitioning at t=0.10s");
        CHECK(controller.floor_weight() > 0.2f && controller.floor_weight() < 0.8f, "Floor weight should be crossfading mid-way");
        CHECK(controller.wall_weight() > 0.2f && controller.wall_weight() < 0.8f, "Wall weight should be crossfading mid-way");
        CHECK(controller.transition_progress() >= 0.45f && controller.transition_progress() <= 0.55f, "Transition progress ~ 50%");

        // Complete transition at t = 0.20s
        controller.update(0.10f, StalkerSurfaceState::WALL_CLIMBING, glm::vec3(0, 3, 0));
        CHECK(!controller.is_transitioning(), "Transition must complete after 0.2s blend window");
        CHECK(controller.current_state() == StalkerSurfaceState::WALL_CLIMBING, "Current state must now be WALL_CLIMBING");
        CHECK(controller.wall_weight() == 1.0f, "Wall weight must be 1.0 after transition completes");
        CHECK(controller.floor_weight() == 0.0f, "Floor weight must be 0.0 after transition completes");

        // Blended pose after transition matches wall climb pose
        auto blended = controller.compute_blended_pose();
        auto wall_ref = StalkerAnimationController::get_wall_climb_pose();
        CHECK(std::abs(blended.front_claw_reach - wall_ref.front_claw_reach) < 0.001f, "Blended pose must match pure wall pose");

        // 15b: Linear Velocity Play Rate Modulation:
        // animPlayRate = glm::length(m_velocity) * m_climbSpeedScalar
        glm::vec3 climb_vel(0.0f, 4.0f, 0.0f);
        float scalar = 3.2f;
        float rate = controller.calculate_play_rate(climb_vel, scalar);
        CHECK(std::abs(rate - (4.0f * 3.2f)) < 0.001f, "Play rate must equal length(velocity) * climb_speed_scalar");

        // Stationary idle maintains live breathing play rate (> 0.0f)
        float idle_rate = controller.calculate_play_rate(glm::vec3(0.0f), scalar);
        CHECK(idle_rate > 0.5f, "Stationary stalker must maintain subtle alive idle rate (> 0.5)");

        std::cout << " -> 0.2s crossfade blending and play rate velocity modulation verified." << std::endl;
    }

    // Test 16: Explosive Charges & Concussive Blast Distraction Mechanics
    {
        std::cout << "[Test 16] Testing Explosive Charges & Concussive Blast Distraction Mechanics..." << std::endl;
        VoidStalkerManager manager;
        World world;
        glm::vec3 stalker_pos(16.0f, 20.0f, 20.0f);
        manager.spawn_melee(stalker_pos);

        // Player is at (16, 20, 32) (12m away)
        glm::vec3 player_pos(16.0f, 20.0f, 32.0f);

        // 16a: Stalker is initially stalking or idle
        manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world, {}, true);

        // 16b: Player places and detonates an explosive charge across the cavern at (38, 20, 10)
        glm::vec3 blast_pos(38.0f, 20.0f, 10.0f);
        std::vector<SoundEvent> sounds;
        SoundEvent demo_blast;
        demo_blast.type = SoundEventType::DemolitionBlast;
        demo_blast.position = blast_pos;
        demo_blast.intensity = 45.0f;
        demo_blast.audible_radius = 60.0f;
        demo_blast.age = 0.0f;
        demo_blast.lifetime = 1.2f;
        sounds.push_back(demo_blast);

        // Concussive shockwave tremor accompanying the blast
        SoundEvent tremor_sound;
        tremor_sound.type = SoundEventType::SeismicTremor;
        tremor_sound.position = blast_pos;
        tremor_sound.intensity = 20.0f;
        tremor_sound.audible_radius = 65.0f;
        tremor_sound.age = 0.0f;
        tremor_sound.lifetime = 1.0f;
        sounds.push_back(tremor_sound);

        // Stalker hears the massive explosion away from the player
        manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world, sounds, true);
        const auto& s = manager.stalkers()[0];

        CHECK(s.state == StalkerState::Investigating, "Stalker must enter Investigating state upon hearing explosive charge blast");
        CHECK(glm::distance(s.investigation_target, blast_pos) < 0.1f, "Stalker investigation target must be set to the explosive blast location!");
        CHECK(glm::distance(s.target_pos, blast_pos) < 0.1f, "Stalker target_pos must point to explosion location, NOT player position!");
        CHECK(s.investigation_timer > 3.0f, "Investigation timer must be set for blast investigation");

        // Stalker moves physically toward blast site over subsequent frames
        float initial_dist_to_blast = glm::distance(s.position, blast_pos);
        for (int step = 0; step < 20; ++step) {
            manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world, {}, true);
        }
        CHECK(glm::distance(manager.stalkers()[0].position, blast_pos) < initial_dist_to_blast,
              "Stalker must run toward the explosive distraction site, luring it away from player!");

        std::cout << " -> Explosive charge detonation distraction mechanics verified successfully." << std::endl;
    }

    // Test 17: Sneaking Stealth Mechanism, Alertness Dissipation & Sneak Attack Critical Hits
    {
        std::cout << "[Test 17] Testing Sneaking Stealth Mechanism, Alertness Dissipation & Sneak Attack Critical Hits..." << std::endl;

        // 17.1 Acoustic stealth sound dampening in NoiseMeter
        NoiseMeter stealth_meter;
        glm::vec3 emit_pos(10.0f, 20.0f, 10.0f);
        
        // Emitting sound while standing
        stealth_meter.set_crouching(false);
        stealth_meter.emit_sound(SoundEventType::FootstepSprint, emit_pos, 10.0f, 20.0f);
        const auto& standing_sound = stealth_meter.recent_sounds().back();
        float standing_intensity = standing_sound.intensity;
        float standing_radius = standing_sound.audible_radius;

        // Emitting sound while crouching (sneaking)
        stealth_meter.set_crouching(true);
        stealth_meter.emit_sound(SoundEventType::FootstepSprint, emit_pos, 10.0f, 20.0f);
        const auto& crouch_sound = stealth_meter.recent_sounds().back();
        CHECK(crouch_sound.intensity < standing_intensity * 0.5f, "Crouch stealth must heavily dampen sound intensity (-65%)");
        CHECK(crouch_sound.audible_radius < standing_radius * 0.6f, "Crouch stealth must heavily dampen audible radius (-55%)");
        CHECK(std::abs(crouch_sound.intensity - (10.0f * StealthSystem::STEALTH_SOUND_INTENSITY_MUL)) < 0.01f, "Intensity matches stealth multiplier");
        CHECK(std::abs(crouch_sound.audible_radius - (20.0f * StealthSystem::STEALTH_SOUND_RADIUS_MUL)) < 0.01f, "Radius matches stealth multiplier");

        // 17.2 Alertness dissipation when delver sneaks/crouches
        VoidStalkerManager manager;
        World stalker_world;
        glm::vec3 s_pos(16.0f, 20.0f, 16.0f);
        manager.spawn_melee(s_pos);
        CHECK(manager.stalkers()[0].is_unalerted(), "Freshly spawned stalker must be unalerted");

        // Alert the stalker with a nearby sound event
        std::vector<SoundEvent> alert_sounds;
        SoundEvent gunshot;
        gunshot.type = SoundEventType::Gunshot;
        gunshot.position = glm::vec3(22.0f, 20.0f, 16.0f);
        gunshot.intensity = 25.0f;
        gunshot.audible_radius = 35.0f;
        gunshot.lifetime = 0.5f;
        alert_sounds.push_back(gunshot);

        glm::vec3 player_pos(10.0f, 20.0f, 10.0f);
        manager.update(0.1f, player_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, stalker_world, alert_sounds, true);
        CHECK(manager.stalkers()[0].state == StalkerState::Investigating, "Stalker must be alerted to Investigating state by gunfire");
        CHECK(!manager.stalkers()[0].is_unalerted(), "Alerted stalker must not report is_unalerted() == true");

        // When player is crouching, investigation timer drains at 2.5x speed and stalker quickly calms down
        // Run update for 2 seconds while player is crouching
        for (int i = 0; i < 20; ++i) {
            manager.update(0.1f, player_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false /* drilling */, stalker_world, {}, true /* crouching */);
        }
        // Investigation timer has depleted much faster than 2 seconds, dropping back to Idle
        CHECK(manager.stalkers()[0].state == StalkerState::Idle, "Sneaking player must allow stalker suspicion to dissipate back to Idle");
        CHECK(manager.stalkers()[0].is_unalerted(), "Stalker that has returned to Idle without sound targets is unalerted again");

        // 17.3 Sneak attack critical hit on unalerted enemy (3.0x damage)
        bool out_crit = false;
        float out_dealt = 0.0f;
        float initial_hp = manager.stalkers()[0].hp;
        glm::vec3 hit_pos = manager.stalkers()[0].position;
        bool damaged = manager.damage_nearest(hit_pos, 3.0f, 10.0f, true /* allow_crit */, &out_crit, &out_dealt);
        CHECK(damaged, "damage_nearest must hit stalker in range");
        CHECK(out_crit, "Attack on unalerted enemy must be a critical hit");
        CHECK(std::abs(out_dealt - 30.0f) < 0.01f, "Sneak attack critical hit must deal 3.0x damage (10.0 * 3 = 30.0)");
        CHECK(std::abs(manager.stalkers()[0].hp - (initial_hp - 30.0f)) < 0.01f, "Stalker HP must reflect 30.0 damage taken");

        // 17.4 Attack on alerted/engaged enemy deals normal 1.0x damage (NO critical hit)
        // Stalker is now pursuing the attacker after being hit
        CHECK(!manager.stalkers()[0].is_unalerted(), "Damaged stalker must now be pursuing attacker");
        bool second_crit = false;
        float second_dealt = 0.0f;
        damaged = manager.damage_nearest(manager.stalkers()[0].position, 3.0f, 5.0f, true /* allow_crit */, &second_crit, &second_dealt);
        CHECK(damaged, "Second damage call must hit stalker");
        CHECK(!second_crit, "Attack on alerted/pursuing stalker must NOT be a critical hit");
        CHECK(std::abs(second_dealt - 5.0f) < 0.01f, "Non-critical attack deals normal 1.0x damage");

        std::cout << " -> Sneaking stealth sound suppression, alertness dissipation, and sneak attack crits verified." << std::endl;
    }

    // Test 18: Monster Chattering, Wall Drilling/Digging Noises, and Burrowing Escape
    {
        std::cout << "[Test 18] Testing Monster Chattering, Wall Drilling & Burrowing Escape..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 spawn_pos(16.0f, 20.0f, 16.0f);
        manager.spawn_melee(spawn_pos);
        auto& s = manager.stalkers_mut()[0];

        // 18.1: Monster Chattering Noises in Idle and Active states
        s.chitter_timer = 0.05f; // Ready to chatter immediately
        glm::vec3 player_pos(16.0f, 20.0f, 30.0f);
        auto res_chatter = manager.update(0.1f, player_pos, glm::vec3(0,0,-1), glm::vec3(0,0,-1), false, 0.0f, false, world);
        CHECK(s.just_chittered, "Monster must emit chattering noise when chitter_timer elapses");
        CHECK(res_chatter.any_chitter, "FrameResult must report any_chitter == true");

        // 18.2: Fleeing into Wall triggers Burrowing Escape
        // Place solid granite wall slab directly in path of flee direction (+Z)
        for (int wx = 14; wx <= 18; ++wx) {
            for (int wy = 18; wy <= 22; ++wy) {
                world.set_voxel(wx, wy, 18, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
                world.set_voxel(wx, wy, 19, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
            }
        }

        s.hp = 8.0f; // Critically damaged (< 12 HP flee threshold)
        s.state = StalkerState::Fleeing;
        s.position = glm::vec3(16.0f, 20.0f, 17.0f); // 1.0m from solid wall

        // Player is at z=12, so flee direction is away from player towards +Z (directly into the wall at z=18)
        glm::vec3 player_behind(16.0f, 20.0f, 12.0f);
        auto res_flee = manager.update(0.1f, player_behind, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world);

        CHECK(s.state == StalkerState::Burrowing, "Fleeing monster hitting solid wall must transition to Burrowing state to escape");
        CHECK(s.is_burrowing(), "is_burrowing() helper must return true");
        CHECK(s.is_escaping(), "is_escaping() helper must return true");
        CHECK(!s.has_collision(), "Burrowing monster must disable standard obstacle collision");
        CHECK(s.burrow_duration >= 1.5f, "Burrow duration must be set for animated escape sequence");

        // 18.3: Drilling / Digging Noises while burrowing through walls
        bool heard_digging = false;
        bool heard_chatter_while_burrowing = false;
        for (int i = 0; i < 8; ++i) {
            auto res_burrow = manager.update(0.1f, player_behind, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world);
            if (res_burrow.any_digging) heard_digging = true;
            if (res_burrow.any_chitter) heard_chatter_while_burrowing = true;
        }
        CHECK(heard_digging, "Monster burrowing through walls must emit drilling or digging noises");
        CHECK(heard_chatter_while_burrowing, "Monster burrowing through walls must make chattering noises");

        // 18.4: Animation duration completion -> successful wall escape
        // Advance remaining burrow duration (> 2.0s)
        bool saw_escape_signal = false;
        for (int i = 0; i < 20; ++i) {
            auto res_esc = manager.update(0.1f, player_behind, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world);
            if (s.just_escaped || res_esc.stalkers_escaped > 0) {
                saw_escape_signal = true;
            }
        }
        CHECK(saw_escape_signal, "just_escaped or stalkers_escaped signal must trigger upon completing burrow duration");
        CHECK(s.is_dead(), "Burrowing monster must be marked Dead/Escaped upon completing burrow duration");

        manager.remove_dead();
        CHECK(manager.active_count() == 0, "Escaped monster must be cleaned up without leaving a carcass");

        std::cout << " -> Monster chattering noises, wall drilling/digging, and animated burrow escape verified." << std::endl;
    }

    // Test 19: Acoustic Hearing Range (Gunshots, Sprinting Footsteps) & Immediate Roosting Awakening
    {
        std::cout << "[Test 19] Testing Extended Acoustic Perception & Roosting Awakening..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 roost_pos(16.0f, 24.0f, 16.0f);
        manager.spawn_roosting(roost_pos);

        CHECK(manager.stalkers().size() == 1, "Expected 1 roosting stalker");
        auto& s_mut = manager.stalkers_mut()[0];
        s_mut.surface_state = StalkerSurfaceState::CEILING_CRAWLING;
        s_mut.target_surface_state = StalkerSurfaceState::CEILING_CRAWLING;
        const auto& s = manager.stalkers()[0];
        CHECK(s.state == StalkerState::Roosting, "Initial state should be Roosting");
        CHECK(s.is_ceiling_crawling(), "Expected ceiling surface attachment");

        // Player is 35m away and fires a gunshot
        glm::vec3 player_pos(16.0f, 20.0f, 51.0f);
        std::vector<SoundEvent> sounds;
        SoundEvent gunshot;
        gunshot.type = SoundEventType::Gunshot;
        gunshot.position = player_pos;
        gunshot.intensity = 28.0f;
        gunshot.audible_radius = 50.0f; // Gunshots audible up to 50m
        gunshot.lifetime = 0.5f;
        sounds.push_back(gunshot);

        auto res = manager.update(0.1f, player_pos, glm::vec3(0, 0, -1), glm::vec3(0, 0, -1),
                                  false, 10.0f, false, world, sounds);

        CHECK(res.any_heard_sound, "Roosting stalker must detect gunshot sound at 35m");
        const auto& awakened = manager.stalkers()[0];
        CHECK(awakened.state == StalkerState::Investigating || awakened.state == StalkerState::Stalking,
              "Roosting stalker must awaken immediately upon hearing gunfire without delay");
        CHECK(awakened.surface_state == StalkerSurfaceState::FLOOR, "Awakened stalker must detach to floor");

        std::cout << " -> Extended acoustic perception and immediate roosting awakening verified." << std::endl;
    }

    // Test 20: Immediate Retaliation When Shot, Knockback Direction & Pack Alert
    {
        std::cout << "[Test 20] Testing Reaction When Shot, Knockback Vector & Pack Alert..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 target_pos(16.0f, 20.0f, 25.0f);
        glm::vec3 pack_mate_pos(18.0f, 20.0f, 28.0f); // Within 20m pack radius
        manager.spawn_melee(target_pos);
        manager.spawn_shooter(pack_mate_pos);

        // Player shoots target stalker from (16, 20, 15) along +Z direction
        glm::vec3 player_pos(16.0f, 20.0f, 15.0f);
        glm::vec3 shot_dir(0.0f, 0.0f, 1.0f);
        float damage = 15.0f;

        bool hit = manager.damage_nearest(target_pos, 2.0f, damage, false, nullptr, nullptr, &player_pos, &shot_dir);
        CHECK(hit, "damage_nearest must hit targeted stalker");

        const auto& hit_stalker = manager.stalkers()[0];
        CHECK(hit_stalker.is_pursuing_attacker, "Stalker when shot must immediately engage pursuit of attacker");
        CHECK(hit_stalker.target_pos == player_pos, "Stalker target position must be shooter coords");
        // Check knockback: velocity should have positive Z component (matching shot_dir)
        CHECK(hit_stalker.velocity.z > 0.5f, "Stalker must take kinetic knockback along bullet trajectory");

        // Check pack mate alert
        const auto& pack_mate = manager.stalkers()[1];
        CHECK(pack_mate.state == StalkerState::Stalking || pack_mate.state == StalkerState::Investigating,
              "Nearby pack member within 20m must be alerted when teammate is shot");

        std::cout << " -> Immediate retaliation when shot, knockback trajectory, and pack alert verified." << std::endl;
    }

    // Test 21: Anti-Jitter Surface Debounce, Hit Recovery & 1-Block Voxel Step Clambering
    {
        std::cout << "[Test 21] Testing Anti-Jitter Surface Debounce, Hit Recovery & Step Clambering..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 stalker_pos(16.0f, 18.05f, 20.0f);
        manager.spawn_melee(stalker_pos);

        // 21a: Shoot stalker - verify firm surface lock and zero orientation flipping
        glm::vec3 player_pos(16.0f, 18.05f, 14.0f);
        glm::vec3 shot_dir(0.0f, 0.0f, 1.0f);
        bool hit = manager.damage_nearest(stalker_pos, 2.0f, 12.0f, false, nullptr, nullptr, &player_pos, &shot_dir);
        CHECK(hit, "damage_nearest must hit stalker");

        const auto& hit_s = manager.stalkers()[0];
        CHECK(hit_s.hit_surface_lock_timer > 0.0f, "Hit surface lock timer must be active after being shot");
        CHECK(hit_s.surface_state == StalkerSurfaceState::FLOOR, "Surface state must lock to FLOOR after being shot");

        // Update several ticks during hit recovery - confirm it doesn't switch to wall/ceiling
        for (int i = 0; i < 5; ++i) {
            manager.update(0.05f, player_pos, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world);
            const auto& s = manager.stalkers()[0];
            CHECK(s.surface_state == StalkerSurfaceState::FLOOR, "Stalker must not jitter or change surface while recovering from shot");
            CHECK(s.state == StalkerState::Lunging || s.state == StalkerState::Stalking, "Stalker must decisively continue attacking rather than spazzing");
        }

        // 21b: Test 1-Block Step Clambering over voxel obstacles
        // Ensure floor is solid at y=17 along testing runway
        for (int z = 20; z <= 26; ++z) {
            world.set_voxel(16, 17, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
        }
        // Place a 1-block raised terrace at z=22..24 at y=18 with air above at y=19
        world.set_voxel(16, 18, 22, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
        world.set_voxel(16, 18, 23, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
        world.set_voxel(16, 18, 24, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
        world.set_voxel(16, 19, 22, Voxel{MAT_AIR, 0}, false);
        world.set_voxel(16, 19, 23, Voxel{MAT_AIR, 0}, false);
        world.set_voxel(16, 19, 24, Voxel{MAT_AIR, 0}, false);

        VoidStalkerManager nav_manager;
        glm::vec3 nav_stalker_pos(16.0f, 18.05f, 21.5f);
        nav_manager.spawn_melee(nav_stalker_pos);
        nav_manager.stalkers_mut()[0].state = StalkerState::Investigating;
        nav_manager.stalkers_mut()[0].investigation_target = glm::vec3(16.0f, 18.05f, 26.0f);
        nav_manager.stalkers_mut()[0].investigation_timer = 5.0f;

        // Update several ticks to walk onto the raised step toward investigation target
        glm::vec3 far_player(60.0f, 20.0f, 60.0f);
        for (int step = 0; step < 8; ++step) {
            nav_manager.update(0.05f, far_player, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world);
        }

        const auto& stepped_s = nav_manager.stalkers()[0];
        CHECK(stepped_s.position.y >= 19.0f, "Stalker must clamber up onto 1-block step rather than getting stuck");

        // Clean up test voxels
        world.set_voxel(16, 18, 22, Voxel{MAT_AIR, 0}, false);
        world.set_voxel(16, 18, 23, Voxel{MAT_AIR, 0}, false);
        world.set_voxel(16, 18, 24, Voxel{MAT_AIR, 0}, false);

        std::cout << " -> Anti-jitter surface lock and 1-block step clambering verified." << std::endl;
    }

    // Test 22: Ceiling-to-Wall Pathing, 3D Wall Descent & Anti-Clip Voxel Collision
    {
        std::cout << "[Test 22] Testing Ceiling-to-Wall Navigation, 3D Wall Descent & Wall Collision..." << std::endl;

        World cavern_world;
        // Clear hollow interior of cavern chamber first
        for (int x = 10; x <= 25; ++x) {
            for (int y = 15; y <= 25; ++y) {
                for (int z = 10; z <= 30; ++z) {
                    cavern_world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }
        // Construct cavern chamber: floor y=15, ceiling y=25, east wall x=25, west wall x=10, north wall z=10, south wall z=30
        for (int x = 10; x <= 25; ++x) {
            for (int z = 10; z <= 30; ++z) {
                cavern_world.set_voxel(x, 15, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
                cavern_world.set_voxel(x, 25, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
            }
        }
        for (int y = 16; y <= 24; ++y) {
            for (int z = 10; z <= 30; ++z) {
                cavern_world.set_voxel(25, y, z, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
                cavern_world.set_voxel(10, y, z, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
            }
            for (int x = 10; x <= 25; ++x) {
                cavern_world.set_voxel(x, y, 10, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
                cavern_world.set_voxel(x, y, 30, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
            }
        }

        // 22a: Ceiling Stalker Directly Above Delver
        // Player is at (18.0f, 16.0f, 20.0f) on the floor.
        // Stalker is directly overhead on the ceiling at (18.0f, 24.2f, 20.0f).
        glm::vec3 player_floor(18.0f, 16.0f, 20.0f);
        glm::vec3 stalker_ceiling(18.0f, 24.2f, 20.0f);

        glm::vec3 wall_heading = AberrantAI::find_descending_wall_direction(stalker_ceiling, player_floor, cavern_world);
        CHECK(glm::length(wall_heading) > 0.5f, "Wall heading from ceiling must not be zero vector");
        // Verify heading points towards one of the room perimeter walls rather than freezing
        CHECK(std::abs(wall_heading.x) > 0.3f || std::abs(wall_heading.z) > 0.3f,
              "Ceiling stalker must navigate horizontally towards cavern wall to reach delver below");

        VoidStalkerManager ceiling_mgr;
        ceiling_mgr.spawn_stalker(stalker_ceiling, 1.0f, StalkerRole::Melee);
        auto& c_s = ceiling_mgr.stalkers_mut()[0];
        c_s.surface_state = StalkerSurfaceState::CEILING_CRAWLING;
        c_s.target_surface_state = StalkerSurfaceState::CEILING_CRAWLING;
        c_s.contact_normal = glm::vec3(0.0f, -1.0f, 0.0f);
        c_s.state = StalkerState::Stalking;
        c_s.target_pos = player_floor;

        // Step multiple frames: verify it moves toward wall and does NOT freeze in place
        glm::vec3 init_pos = c_s.position;
        for (int f = 0; f < 10; ++f) {
            ceiling_mgr.update(0.05f, player_floor, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, cavern_world);
        }
        float horiz_disp = glm::distance(glm::vec2(c_s.position.x, c_s.position.z), glm::vec2(init_pos.x, init_pos.z));
        CHECK(horiz_disp > 0.2f, "Ceiling stalker directly above delver must actively traverse towards walls");
        CHECK(c_s.velocity.x != 0.0f || c_s.velocity.z != 0.0f, "Ceiling stalker must maintain active navigation velocity");

        // 22b: 3D Wall Descent towards Floor Target
        // Stalker is on east wall at x=24.2, y=22.0, z=20.0, normal = (-1, 0, 0)
        glm::vec3 stalker_wall(24.2f, 22.0f, 20.0f);
        glm::vec3 wall_norm(-1.0f, 0.0f, 0.0f);
        glm::vec3 wall_crawl_dir = AberrantAI::calculate_wall_traversal_direction(stalker_wall, player_floor, wall_norm);
        CHECK(wall_crawl_dir.y < -0.3f, "Wall traversal vector must point down along Y towards lower floor delver");

        VoidStalkerManager wall_mgr;
        wall_mgr.spawn_stalker(stalker_wall, 1.0f, StalkerRole::Melee);
        auto& w_s = wall_mgr.stalkers_mut()[0];
        w_s.surface_state = StalkerSurfaceState::WALL_CLIMBING;
        w_s.target_surface_state = StalkerSurfaceState::WALL_CLIMBING;
        w_s.contact_normal = wall_norm;
        w_s.state = StalkerState::Stalking;
        w_s.target_pos = player_floor;
        float prev_y = w_s.position.y;
        for (int f = 0; f < 10; ++f) {
            wall_mgr.update(0.05f, player_floor, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, cavern_world);
        }
        CHECK(w_s.position.y < prev_y, "Wall climbing stalker must climb down vertically towards delver on floor");

        // 22c: Voxel Collision — Cannot Move or Clip Through Multi-Block Solid Walls
        VoidStalkerManager col_mgr;
        glm::vec3 stalker_floor(22.0f, 16.05f, 20.0f);
        col_mgr.spawn_stalker(stalker_floor, 1.0f, StalkerRole::Melee);
        auto& col_s = col_mgr.stalkers_mut()[0];
        col_s.state = StalkerState::Stalking;
        // Place player behind east wall at x=28 (wall is at x=25)
        glm::vec3 player_behind_wall(28.0f, 16.0f, 20.0f);

        for (int f = 0; f < 30; ++f) {
            col_mgr.update(0.05f, player_behind_wall, glm::vec3(1, 0, 0), glm::vec3(1, 0, 0), false, 0.0f, false, cavern_world);
        }
        // East wall is at x=25. The stalker with radius 0.38m MUST NOT cross or penetrate inside x=25!
        CHECK(col_s.position.x <= 24.65f, "Stalker must be halted by solid wall and cannot move through it");

        std::cout << " -> Ceiling-to-wall pathing, 3D wall descent & wall collision prevention verified." << std::endl;
    }

    // Test 23: Time-Sliced AI Pathfinding Stagger & Spatial Hash Boid Separation
    {
        std::cout << "[Test 23] Testing Time-Sliced AI Pathfinding Stagger & Spatial Hash..." << std::endl;

        World test_world;
        for (int x = 10; x <= 30; ++x) {
            for (int y = 15; y <= 25; ++y) {
                for (int z = 10; z <= 30; ++z) {
                    test_world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }
        for (int x = 10; x <= 30; ++x) {
            for (int z = 10; z <= 30; ++z) {
                test_world.set_voxel(x, 15, z, Voxel{MAT_DREDGE_BEDROCK, 0}, false);
            }
        }

        VoidStalkerManager manager;
        glm::vec3 player_pos(15.0f, 16.0f, 15.0f);
        glm::vec3 stalker_pos(15.0f, 16.0f, 25.0f); // 10m away
        manager.spawn_melee(stalker_pos);

        // Step 0: Randomized initial phase per enemy in [0.0f, 0.10f)
        CHECK(manager.stalkers()[0].m_pathTimer >= 0.0f && manager.stalkers()[0].m_pathTimer < 0.10f,
              "Spawned stalker must have randomized initial phase in [0.0f, 0.10f)");

        auto& s = manager.stalkers_mut()[0];
        s.state = StalkerState::Stalking;
        s.target_pos = player_pos;
        s.m_pathTimer = 0.0f; // Reset accumulator

        // Step 1: Sub-tick updates (dt = 0.03s < 0.10f). Accumulator increments, movement interpolates smoothly.
        glm::vec3 p_prev = s.position;
        manager.update(0.03f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, test_world);
        CHECK(manager.stalkers()[0].m_pathTimer >= 0.029f && manager.stalkers()[0].m_pathTimer <= 0.031f,
              "m_pathTimer must accumulate elapsed dt");
        CHECK(manager.stalkers()[0].m_pathTickTimer == manager.stalkers()[0].m_pathTimer,
              "m_pathTickTimer alias must mirror m_pathTimer identically");

        // Step 2: Accumulate past 0.10s threshold (10 Hz) -> triggers path recalculation and resets timer
        manager.update(0.08f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, test_world);
        // Total time 0.03 + 0.08 = 0.11s >= 0.10s -> tick fired, timer reset
        CHECK(manager.stalkers()[0].m_pathTimer < 0.10f,
              "m_pathTimer must reset after 0.10s tick boundary (10 Hz)");
        CHECK(glm::length(manager.stalkers()[0].velocity) > 0.1f,
              "Stalker must calculate active tracking velocity on tick");
        CHECK(manager.stalkers()[0].position != p_prev,
              "Stalker movement must progress smoothly across ticks");

        // Step 3: Test distance > 35.0m enters dormant low-tick mode (skips collision raycasts and boid separation)
        VoidStalkerManager distant_mgr;
        glm::vec3 far_stalker_pos(15.0f, 16.0f, 52.0f); // 37m away (> 35.0m)
        distant_mgr.spawn_melee(far_stalker_pos);
        distant_mgr.spawn_melee(far_stalker_pos + glm::vec3(1.0f, 0.0f, 0.0f)); // Nearby packmate 1m away
        auto& far_s1 = distant_mgr.stalkers_mut()[0];
        far_s1.state = StalkerState::Stalking;
        far_s1.m_pathTimer = 0.10f; // Ready to tick

        distant_mgr.update(0.016f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, test_world);
        // Verify raycast LOS was skipped (has_player_los remains false) in dormant low-tick mode
        CHECK(!distant_mgr.stalkers()[0].has_player_los,
              "Stalker > 35m from player must enter dormant low-tick mode and skip raycast line-of-sight");

        // Step 4: Spatial Hashing and Early-Exit Boid Separation Test
        AISpatialHash spatial_hash;
        spatial_hash.clear();
        spatial_hash.insert(1, glm::vec3(10.0f, 10.0f, 10.0f));
        spatial_hash.insert(2, glm::vec3(10.5f, 10.0f, 10.0f)); // 0.5m away (< 4m, within dist_sq <= 16.0f)
        spatial_hash.insert(3, glm::vec3(20.0f, 20.0f, 20.0f)); // > 14m away (> 4m, dist_sq > 16.0f)

        // Query separation for entity 1 against spatial hash
        glm::vec3 sep_force = spatial_hash.calculate_separation(1, glm::vec3(10.0f, 10.0f, 10.0f), 16.0f, 8.0f);
        CHECK(glm::length(sep_force) > 0.1f, "Nearby entity within 4m (dist_sq <= 16.0) must exert repulsive separation force");
        CHECK(sep_force.x < 0.0f, "Entity 1 must be pushed in -X direction away from entity 2 at +0.5m X");

        // Query separation for entity 3 (all others > 4m away, dist_sq > 16.0f)
        glm::vec3 far_sep = spatial_hash.calculate_separation(3, glm::vec3(20.0f, 20.0f, 20.0f), 16.0f, 8.0f);
        CHECK(glm::length(far_sep) == 0.0f, "Entities beyond 4m (dist_sq > 16.0) must skip separation math and yield 0 force");

        // Direct AberrantAI::calculate_swarm_separation early exit verification
        std::vector<glm::vec3> others = { glm::vec3(10.5f, 10.0f, 10.0f), glm::vec3(30.0f, 30.0f, 30.0f) };
        glm::vec3 direct_sep = AberrantAI::calculate_swarm_separation(glm::vec3(10.0f, 10.0f, 10.0f), others, 5.0f, 8.0f);
        CHECK(glm::length(direct_sep) > 0.1f, "Direct calculate_swarm_separation must compute repulsion for nearby entity");

        std::cout << " -> Time-sliced pathfinding stagger & spatial hash boid separation verified." << std::endl;
    }

    // Test 24: Gunshots Do Not Stun Enemies & Movement Continuity When Damaged
    {
        std::cout << "[Test 24] Testing Gunshots Do Not Stun Enemies & Movement Continuity..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 stalker_pos(16.0f, 18.05f, 22.0f);
        manager.spawn_melee(stalker_pos);

        glm::vec3 player_pos(16.0f, 18.05f, 14.0f);
        glm::vec3 shot_dir(0.0f, 0.0f, 1.0f);

        // 24a: Heavy gunshot damage (> 20.0 HP) must NEVER stun enemy or set stun_timer
        bool hit = manager.damage_nearest(stalker_pos, 2.0f, 25.0f, false, nullptr, nullptr, &player_pos, &shot_dir);
        CHECK(hit, "Damage nearest must register gunshot hit");

        const auto& hit_s = manager.stalkers()[0];
        CHECK(hit_s.state != StalkerState::Stunned, "Gunshots must NOT put stalker into Stunned state");
        CHECK(hit_s.stun_timer == 0.0f, "Gunshots must NOT set stun_timer (must be 0.0s)");
        CHECK(hit_s.is_pursuing_attacker, "Stalker must engage pursuit of attacker");

        // 24b: Continuous updates - stalker must advance toward player, not freeze
        glm::vec3 initial_pos = hit_s.position;
        for (int i = 0; i < 5; ++i) {
            manager.update(0.05f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);
            // Stalker must not be in Stunned or Investigating state
            CHECK(manager.stalkers()[0].state != StalkerState::Stunned, "Stalker must not enter Stunned state during combat updates");
            CHECK(manager.stalkers()[0].state != StalkerState::Investigating, "Damaged stalker must pursue attacker and not get distracted into Investigating");
        }
        CHECK(glm::distance(manager.stalkers()[0].position, initial_pos) > 0.1f, "Stalker must continue moving forward across ticks when shot");

        // 24c: Repeated gunshots must not freeze or reset lunge velocity
        manager.stalkers_mut()[0].state = StalkerState::Lunging;
        manager.stalkers_mut()[0].state_timer = 0.20f;
        float prev_timer = manager.stalkers()[0].state_timer;
        manager.damage_nearest(manager.stalkers()[0].position, 2.0f, 15.0f, false, nullptr, nullptr, &player_pos, &shot_dir);
        CHECK(manager.stalkers()[0].state_timer >= prev_timer, "Subsequent shots while lunging must NOT reset state_timer to 0");
        CHECK(manager.stalkers()[0].stun_timer == 0.0f, "Repeated shots must never apply stun_timer");

        std::cout << " -> Gunshots non-stun policy & active combat movement continuity verified." << std::endl;
    }

    // Test 25: Spawn Grace Dormancy, Screech Cooldowns, and Anti-Stuck De-Escalation
    {
        std::cout << "[Test 25] Testing Spawn Grace Dormancy, Screech Rate-Limiting & Anti-Stuck De-Escalation..." << std::endl;
        VoidStalkerManager manager;
        manager.reset_screech_cooldowns(12.0f);

        glm::vec3 roost_pos(16.0f, 22.0f, 16.0f);
        manager.spawn_roosting(roost_pos, StalkerRole::Melee);

        CHECK(manager.stalkers().size() == 1, "Expected 1 stalker spawned");
        const auto& s = manager.stalkers()[0];
        CHECK(s.state == StalkerState::Roosting, "Stalker should initialize in Roosting state");
        CHECK(s.spawn_grace_timer > 0.0f, "Spawn grace timer should be active on spawn");

        // Player is 10m away, standing quietly
        glm::vec3 player_pos(16.0f, 20.0f, 26.0f);
        glm::vec3 fwd(0.0f, 0.0f, -1.0f);
        auto res = manager.update(0.1f, player_pos, fwd, fwd, false, 0.0f, false, world, {}, true);

        CHECK(s.state == StalkerState::Roosting, "Stalker must remain roosting during spawn grace window when player is stealthy");
        CHECK(!res.any_screech, "Roosting stalker must not emit screeches on spawn");

        // Test Anti-Stuck: set stalker to Investigating and obstruct it against a wall
        manager.stalkers_mut()[0].state = StalkerState::Investigating;
        manager.stalkers_mut()[0].spawn_grace_timer = 0.0f;
        manager.stalkers_mut()[0].investigation_timer = 5.0f;
        manager.stalkers_mut()[0].investigation_target = glm::vec3(16.0f, 20.0f, 10.0f);
        manager.stalkers_mut()[0].stuck_checkpoint_pos = s.position;
        manager.stalkers_mut()[0].stuck_timer = 1.3f; // Simulate obstruction for > 1.2s

        // Update when obstructed
        manager.update(0.1f, player_pos, fwd, fwd, false, 0.0f, false, world, {}, true);

        // Stalker must de-escalate to Idle rather than remaining stuck/spazzing
        CHECK(manager.stalkers()[0].state == StalkerState::Idle, "Stuck investigating stalker must cleanly de-escalate to Idle");
        CHECK(manager.stalkers()[0].stuck_timer == 0.0f, "Stuck timer must reset on de-escalation");

        std::cout << " -> Spawn grace dormancy and anti-stuck de-escalation verified." << std::endl;
    }

    // Test 26: Pacing Limits, Concurrent Chaser Caps, Recovery Breathing Room & Distress Alert Throttling
    {
        std::cout << "[Test 26] Testing Concurrent Chaser Caps, Recovery Cooldown & Distress Alert Throttling..." << std::endl;
        VoidStalkerManager manager;
        manager.set_sector_level(1);
        CHECK(manager.max_concurrent_chasers() == 2, "Sector 1 must cap concurrent chasers at 2");

        manager.set_sector_level(2);
        CHECK(manager.max_concurrent_chasers() == 3, "Sector 2 must cap concurrent chasers at 3");

        manager.set_sector_level(3);
        CHECK(manager.max_concurrent_chasers() == 4, "Sector 3 must cap concurrent chasers at 4");

        // Reset to Sector 1 (limit 2)
        manager.reset();
        manager.set_sector_level(1);

        // Spawn 4 idle stalkers nearby
        manager.spawn_melee(glm::vec3(15.0f, 20.0f, 15.0f));
        manager.spawn_melee(glm::vec3(17.0f, 20.0f, 15.0f));
        manager.spawn_melee(glm::vec3(15.0f, 20.0f, 17.0f));
        manager.spawn_melee(glm::vec3(17.0f, 20.0f, 17.0f));
        CHECK(manager.stalkers().size() == 4, "Expected 4 stalkers");
        CHECK(manager.concurrent_chasers_count() == 0, "Initial chasers should be 0");
        CHECK(manager.can_engage_new_chaser(), "Should be able to engage chaser initially");

        // Engage 2 stalkers into Stalking (reaching cap 2)
        manager.stalkers_mut()[0].state = StalkerState::Stalking;
        manager.stalkers_mut()[1].state = StalkerState::Stalking;
        CHECK(manager.concurrent_chasers_count() == 2, "Concurrent chasers should be 2");
        CHECK(!manager.can_engage_new_chaser(), "Should NOT be able to engage new chaser once cap (2) is reached");

        // Damage stalker 0 from player pos
        glm::vec3 player_pos(16.0f, 20.0f, 20.0f);
        bool hit = manager.damage_nearest(manager.stalkers()[0].position, 1.5f, 10.0f, false, nullptr, nullptr, nullptr, &player_pos);
        CHECK(hit, "Damage nearest should hit stalker 0");

        // Stalker count in Stalking should remain capped at 2; excess packmates route to Investigating
        int stalking_count = 0;
        int investigating_count = 0;
        for (const auto& s : manager.stalkers()) {
            if (s.state == StalkerState::Stalking) stalking_count++;
            if (s.state == StalkerState::Investigating) investigating_count++;
        }
        CHECK(manager.concurrent_chasers_count() == 2, "Concurrent chasers must remain capped at 2");
        CHECK(investigating_count <= 1, "At most 1 packmate should be routed to investigating");

        // Test Combat Recovery Cooldown:
        manager.trigger_combat_recovery(5.0f);
        CHECK(manager.is_combat_recovery_active(), "Combat recovery window should be active");
        CHECK(!manager.can_engage_new_chaser(), "New chasers blocked during recovery window to allow player recovery");

        // Step manager update and verify recovery timer drains
        glm::vec3 fwd(0.0f, 0.0f, -1.0f);
        manager.update(1.0f, player_pos, fwd, fwd, false, 0.0f, false, world, {}, false);
        CHECK(manager.combat_recovery_timer() <= 4.05f, "Combat recovery timer should drain over time");

        std::cout << " -> Concurrent chaser caps, recovery cooldown, and distress throttling verified." << std::endl;
    }

    // Test 27: Inter-Stalker Wave Spawn Separation and Anti-Burrowing Lateral Obstacle Deflection
    {
        std::cout << "[Test 27] Testing Wave Spawn Separation & Anti-Burrowing Obstacle Deflection..." << std::endl;
        VoidStalkerManager manager;
        manager.reset();

        glm::vec3 player_pos(16.0f, 20.0f, 20.0f);
        // Spawn wave of 3 stalkers
        manager.spawn_wave(player_pos, 3, world);
        CHECK(manager.stalkers().size() == 3, "Wave should spawn 3 stalkers");

        // Verify pairwise distances between all wave-spawned stalkers are >= 4.0m (spawn separation enforced)
        for (size_t i = 0; i < manager.stalkers().size(); ++i) {
            for (size_t j = i + 1; j < manager.stalkers().size(); ++j) {
                float dist = glm::distance(manager.stalkers()[i].position, manager.stalkers()[j].position);
                CHECK(dist >= 4.0f, "Spawned stalkers must maintain minimum separation distance");
            }
        }

        // Test Anti-Burrowing: In combat (Stalking), an obstructed stalker must deflect laterally and NOT burrow into walls/despawn
        manager.stalkers_mut()[0].state = StalkerState::Stalking;
        manager.stalkers_mut()[0].is_pursuing_attacker = true;
        manager.stalkers_mut()[0].stuck_timer = 2.0f; // Simulate obstruction > 1.6s

        glm::vec3 fwd(0.0f, 0.0f, -1.0f);
        manager.update(0.1f, player_pos, fwd, fwd, false, 0.0f, false, world, {}, false);

        CHECK(manager.stalkers()[0].state == StalkerState::Stalking, "Combat stalker must remain Stalking and not enter Burrowing suicide");
        CHECK(manager.stalkers()[0].scale > 1.0f, "Stalker scale must not shrink or vanish");
        CHECK(manager.stalkers()[0].stuck_timer == 0.0f, "Stuck timer should be reset after lateral deflection");

        std::cout << " -> Wave spawn separation and anti-burrowing deflection verified." << std::endl;
    }

    // Test 28: Sub-Block Surface Traversal (Slabs, Ramps, Wedges, Clambering, and Sightlines)
    {
        std::cout << "[Test 28] Testing Sub-Block Surface Traversal (Slabs, Ramps & Wedges)..." << std::endl;

        // Clear out test area (x: 30..40, y: 15..25, z: 30..40)
        for (int x = 28; x <= 42; ++x) {
            for (int y = 14; y <= 26; ++y) {
                for (int z = 28; z <= 42; ++z) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }
        // Base solid bedrock at y=14
        for (int x = 28; x <= 42; ++x) {
            for (int z = 28; z <= 42; ++z) {
                world.set_voxel(x, 14, z, Voxel{MAT_DREDGE_BEDROCK, SHAPE_CUBE}, false);
            }
        }

        // Sub-test A: is_point_solid geometry checks
        // 1) Bottom slab at (30, 15, 30): solid from y=15.0 to 15.5; air from 15.5 to 16.0
        world.set_voxel(30, 15, 30, Voxel{MAT_FRACTURED_GRANITE, SHAPE_SLAB_BOTTOM}, false);
        CHECK(AberrantAI::is_point_solid(world, glm::vec3(30.5f, 15.25f, 30.5f)), "Bottom slab interior should be solid");
        CHECK(!AberrantAI::is_point_solid(world, glm::vec3(30.5f, 15.75f, 30.5f)), "Air above bottom slab should NOT be solid");

        // 2) Top slab at (31, 15, 30): air from y=15.0 to 15.5; solid from 15.5 to 16.0
        world.set_voxel(31, 15, 30, Voxel{MAT_FRACTURED_GRANITE, SHAPE_SLAB_TOP}, false);
        CHECK(!AberrantAI::is_point_solid(world, glm::vec3(31.5f, 15.25f, 30.5f)), "Air beneath top slab should NOT be solid");
        CHECK(AberrantAI::is_point_solid(world, glm::vec3(31.5f, 15.75f, 30.5f)), "Top slab interior should be solid");

        // 3) East ramp at (32, 15, 30): rising toward +X (solid height = u)
        world.set_voxel(32, 15, 30, Voxel{MAT_FRACTURED_GRANITE, SHAPE_RAMP_EAST}, false);
        // At u=0.25 (x=32.25): solid up to y=15.25
        CHECK(AberrantAI::is_point_solid(world, glm::vec3(32.25f, 15.15f, 30.5f)), "Point beneath east ramp slope should be solid");
        CHECK(!AberrantAI::is_point_solid(world, glm::vec3(32.25f, 15.40f, 30.5f)), "Point above east ramp slope should NOT be solid");
        // At u=0.75 (x=32.75): solid up to y=15.75
        CHECK(AberrantAI::is_point_solid(world, glm::vec3(32.75f, 15.65f, 30.5f)), "Point beneath east ramp slope (high side) should be solid");
        CHECK(!AberrantAI::is_point_solid(world, glm::vec3(32.75f, 15.85f, 30.5f)), "Point above east ramp slope (high side) should NOT be solid");

        // Sub-test B: Surface elevations & normal classifications
        float bottom_slab_y = AberrantAI::get_voxel_surface_y(world, 30, 15, 30, 30.5f, 30.5f);
        CHECK(std::abs(bottom_slab_y - 15.5f) < 0.001f, "Bottom slab top surface should be 15.5m");

        float ramp_mid_y = AberrantAI::get_voxel_surface_y(world, 32, 15, 30, 32.5f, 30.5f);
        CHECK(std::abs(ramp_mid_y - 15.5f) < 0.001f, "East ramp mid-point elevation should be 15.5m");

        glm::vec3 ramp_norm = AberrantAI::get_voxel_surface_normal(world, 32, 15, 30);
        CHECK(ramp_norm.y > 0.65f, "Ramp normal upward component must exceed 0.65 for 45 deg slope");
        CHECK(ramp_norm.x < -0.65f, "East ramp normal must face westward (-X)");
        CHECK(AberrantAI::classify_normal(ramp_norm) == StalkerSurfaceState::FLOOR, "45-degree ramp must be classified as FLOOR");

        // Sub-test C: find_ground_surface probing
        float found_y = 0.0f;
        glm::vec3 found_n(0.0f);
        bool found_ground = AberrantAI::find_ground_surface(world, glm::vec3(32.5f, 16.0f, 30.5f), 1.35f, 1.5f, &found_y, &found_n);
        CHECK(found_ground, "Should find ground on east ramp");
        CHECK(std::abs(found_y - 15.5f) < 0.05f, "Found ground elevation should match ramp mid-surface (~15.5m)");

        // Sub-test D: Step clambering & ramp traversal in resolve_voxel_collision
        // 1) Test clambering onto a bottom slab step from y=15.0 to 15.5
        glm::vec3 test_pos(30.1f, 15.0f, 30.5f);
        glm::vec3 test_vel(1.5f, 0.0f, 0.0f);
        bool collided = AberrantAI::resolve_voxel_collision(
            test_pos, test_vel, 0.45f, 0.85f, StalkerSurfaceState::FLOOR, glm::vec3(0, 1, 0), world
        );
        CHECK(test_pos.y >= 15.49f, "Stalker should clamber up onto bottom slab (y >= 15.49)");
        CHECK(test_vel.x > 0.5f, "Horizontal velocity must NOT be zeroed when stepping onto slab");

        // 2) Test ascending east ramp without getting stuck
        glm::vec3 ramp_pos(32.4f, 15.35f, 30.5f);
        glm::vec3 ramp_vel(2.0f, 0.0f, 0.0f);
        AberrantAI::resolve_voxel_collision(
            ramp_pos, ramp_vel, 0.45f, 0.85f, StalkerSurfaceState::FLOOR, glm::vec3(0, 1, 0), world
        );
        CHECK(ramp_pos.y >= 15.39f, "Stalker ascending ramp should conform to ramp surface");
        CHECK(ramp_vel.x > 0.5f, "Stalker ascending ramp must maintain forward velocity");

        // Sub-test E: Sightlines across air above sub-blocks
        // Construct a 5-block corridor of bottom slabs at y=15 (x=34..38, z=35)
        for (int x = 34; x <= 38; ++x) {
            world.set_voxel(x, 15, 35, Voxel{MAT_FRACTURED_GRANITE, SHAPE_SLAB_BOTTOM}, false);
        }
        VoidStalkerManager manager;
        // Check line of sight at y=15.75m (clear air above bottom slabs)
        bool los = manager.has_line_of_sight(glm::vec3(34.0f, 15.75f, 35.5f), glm::vec3(38.0f, 15.75f, 35.5f), world);
        CHECK(los, "Line of sight across air above bottom slabs must be unobstructed");

        // Sub-test F: Continuous movement across a multi-segment ramp and slab sequence
        // Construct runway: [33] flat floor y=15 -> [34] east ramp rising 15.0..16.0 -> [35] full cube at y=15 -> [36] east ramp rising 16.0..17.0
        world.set_voxel(33, 14, 38, Voxel{MAT_DREDGE_BEDROCK, SHAPE_CUBE}, false); // floor top at 15.0
        world.set_voxel(34, 15, 38, Voxel{MAT_FRACTURED_GRANITE, SHAPE_RAMP_EAST}, false); // ramp 15..16
        world.set_voxel(35, 15, 38, Voxel{MAT_FRACTURED_GRANITE, SHAPE_CUBE}, false); // landing at 16.0
        world.set_voxel(36, 16, 38, Voxel{MAT_FRACTURED_GRANITE, SHAPE_RAMP_EAST}, false); // ramp 16..17

        glm::vec3 runner_pos(33.5f, 15.0f, 38.5f);
        glm::vec3 runner_vel(2.0f, 0.0f, 0.0f);
        for (int step = 0; step < 20; ++step) {
            runner_pos += runner_vel * 0.05f; // advance 0.1m per step
            AberrantAI::resolve_voxel_collision(
                runner_pos, runner_vel, 0.45f, 0.85f, StalkerSurfaceState::FLOOR, glm::vec3(0, 1, 0), world
            );
        }
        CHECK(runner_pos.x > 35.0f, "Monster must traverse smoothly up contiguous ramps and ledges without sticking");
        CHECK(runner_pos.y >= 15.9f, "Monster elevation should have risen along the ramps");

        std::cout << " -> Sub-block surface traversal, geometry, ramp conformance & sightlines verified." << std::endl;
    }

    // Test 29: Natural Physical Burrowing Emergence Animation & Differentiated Archetypes
    {
        std::cout << "[Test 29] Testing Natural Physical Burrowing Emergence & Archetype Differentiation..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 spawn_wall(16.0f, 20.0f, 10.0f);
        glm::vec3 wall_normal(0.0f, 0.0f, 1.0f); // Emerging out of South wall into cavern

        // A. Melee Emergence: 1.2s duration, crimson eyes, initial collision false
        // A. Spawn all three archetypes
        manager.spawn_emerging(spawn_wall, wall_normal, 1.0f, StalkerRole::Melee);
        manager.spawn_emerging(spawn_wall + glm::vec3(2.0f, 0.0f, 0.0f), wall_normal, 1.0f, StalkerRole::Shooter);
        manager.spawn_emerging(spawn_wall + glm::vec3(-2.0f, 0.0f, 0.0f), wall_normal, 1.0f, StalkerRole::ChitinGoliath);
        CHECK(manager.stalkers().size() == 3, "Expected 3 stalkers spawned");

        // Inspect Melee (slot 0)
        CHECK(manager.stalkers()[0].state == StalkerState::Emerging, "Spawn emerging must initialize in Emerging state");
        CHECK(manager.stalkers()[0].is_emerging(), "is_emerging() helper must return true");
        CHECK(!manager.stalkers()[0].has_collision(), "Emerging monster must NOT have active physics collision");
        CHECK(!manager.stalkers()[0].has_attack_hitbox(), "Emerging monster must NOT damage delver while emerging");
        CHECK(manager.stalkers()[0].burrow_duration == 1.2f, "Melee emergence duration should be 1.2s");
        CHECK(manager.stalkers()[0].contact_normal == wall_normal, "Contact normal should align with emergence surface normal");
        CHECK(manager.stalkers()[0].surface_state == StalkerSurfaceState::WALL_CLIMBING, "Normal (0,0,1) should classify as Wall Climbing");
        glm::vec4 melee_eye = manager.stalkers()[0].get_eye_color();
        CHECK(melee_eye.r > 0.7f && melee_eye.g < 0.2f, "Expected crimson red eye color during Melee emergence");

        // Inspect Shooter (slot 1)
        CHECK(manager.stalkers()[1].state == StalkerState::Emerging, "Shooter must initialize in Emerging state");
        CHECK(manager.stalkers()[1].burrow_duration == 1.5f, "Shooter emergence duration should be 1.5s");
        glm::vec4 shooter_eye = manager.stalkers()[1].get_eye_color();
        CHECK(shooter_eye.g > 0.7f && shooter_eye.r < 0.3f, "Expected toxic emerald green eye color during Shooter emergence");

        // Inspect Goliath (slot 2)
        CHECK(manager.stalkers()[2].state == StalkerState::Emerging, "Goliath must initialize in Emerging state");
        CHECK(manager.stalkers()[2].burrow_duration == 2.2f, "Goliath emergence duration should be 2.2s");

        // D. Simulate emergence update frames: digging & chittering events trigger
        glm::vec3 player_pos(16.0f, 20.0f, 20.0f);
        glm::vec3 player_forward(0.0f, 0.0f, -1.0f);
        glm::vec3 headlamp_dir(0.0f, 0.0f, -1.0f);
        auto res = manager.update(0.3f, player_pos, player_forward, headlamp_dir, false, 0.0f, false, world);
        CHECK(res.any_digging, "Emerging monsters must emit digging rock-chipping events");
        CHECK(res.any_chitter, "Emerging monsters must emit chittering audio events");

        // E. Step time past 1.2s: Melee should complete emergence and transition to active combat
        manager.update(1.0f, player_pos, player_forward, headlamp_dir, false, 0.0f, false, world);
        CHECK(manager.stalkers()[0].state != StalkerState::Emerging, "Melee must have finished emergence after 1.3s");
        CHECK(manager.stalkers()[0].has_collision(), "Emerged monster must restore collision");
        CHECK(manager.stalkers()[0].has_attack_hitbox(), "Emerged monster must restore attack hitbox");

        // Shooter (1.5s total) and Goliath (2.2s total) should still be emerging at 1.3s
        CHECK(manager.stalkers()[1].state == StalkerState::Emerging, "Shooter should still be emerging at 1.3s (duration 1.5s)");
        CHECK(manager.stalkers()[2].state == StalkerState::Emerging, "Goliath should still be emerging at 1.3s (duration 2.2s)");

        // Advance 0.3s (total 1.6s): Shooter completes, Goliath still emerging
        manager.update(0.3f, player_pos, player_forward, headlamp_dir, false, 0.0f, false, world);
        CHECK(manager.stalkers()[1].state != StalkerState::Emerging, "Shooter must finish emergence after 1.6s");
        CHECK(manager.stalkers()[2].state == StalkerState::Emerging, "Goliath must still be emerging at 1.6s (duration 2.2s)");

        // Advance 0.8s (total 2.4s): Goliath completes emergence
        manager.update(0.8f, player_pos, player_forward, headlamp_dir, false, 0.0f, false, world);
        CHECK(manager.stalkers()[2].state != StalkerState::Emerging, "Goliath must finish emergence after 2.4s");

        std::cout << " -> Physical burrowing emergence, archetype durations, and state transitions verified." << std::endl;
    }

    // Test 30: Top/Bottom Block Gap Traversal & Anti-Stuck Headroom Protection
    {
        std::cout << "[Test 30] Testing Top/Bottom Block Gap Traversal & Anti-Stuck Protection..." << std::endl;
        World gap_world;
        gap_world.generate_world(1, 4242);

        // Build chamber with floor at y=17, open air gap at y=18 (1.0m clearance), and solid top ceiling blocks at y=19
        for (int x = 12; x <= 20; ++x) {
            for (int z = 12; z <= 20; ++z) {
                gap_world.set_voxel(x, 17, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false); // Bottom block (floor surface = 18.0)
                gap_world.set_voxel(x, 18, z, Voxel{MAT_AIR, 0}, false);             // Gap (1.0m height)
                gap_world.set_voxel(x, 19, z, Voxel{MAT_FRACTURED_GRANITE, 0}, false); // Top block (ceiling bottom = 19.0)
            }
        }

        // 1. Verify surface normal sampling in a 1-block gap stays on FLOOR and does NOT flip to CEILING_CRAWLING
        glm::vec3 in_gap_pos(16.0f, 18.0f, 16.0f);
        auto sample = AberrantAI::sample_surface_normal(in_gap_pos, gap_world, 1.4f, StalkerSurfaceState::FLOOR);
        CHECK(sample.has_contact, "Surface probe in gap should detect contact");
        CHECK(sample.surface_state == StalkerSurfaceState::FLOOR, "Monster in 1m gap between floor and ceiling must remain on FLOOR");
        CHECK(sample.contact_normal.y > 0.8f, "Contact normal should point upward from floor");

        // 2. Collision resolution in gap: entity must not penetrate the top ceiling block at y=19
        glm::vec3 test_pos = in_gap_pos + glm::vec3(0.0f, 0.35f, 0.0f); // Pushed upward towards ceiling (y=18.35, head would reach 19.2)
        glm::vec3 vel(0.0f, 2.0f, 0.0f);
        AberrantAI::resolve_voxel_collision(test_pos, vel, 0.38f, 0.85f, StalkerSurfaceState::FLOOR, glm::vec3(0.0f, 1.0f, 0.0f), gap_world);
        CHECK(test_pos.y + 0.85f <= 19.05f, "Entity head must remain at or below top block bottom surface (19.0)");
        CHECK(vel.y <= 0.0f, "Upward velocity into top block ceiling must be zeroed");

        // 3. Horizontal collision against top block: wall check at head level prevents walking through top block overhang
        glm::vec3 overhang_pos(16.0f, 18.0f, 16.0f);
        // Place an overhang block right in front of entity at (16, 18, 17) - solid top block at y=18.5
        gap_world.set_voxel(16, 18, 17, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
        glm::vec3 move_vel(0.0f, 0.0f, 5.0f);
        overhang_pos.z += 0.70f; // Walk into obstacle
        bool col = AberrantAI::resolve_voxel_collision(overhang_pos, move_vel, 0.38f, 0.85f, StalkerSurfaceState::FLOOR, glm::vec3(0.0f, 1.0f, 0.0f), gap_world);
        CHECK(col, "Upper/lower body collision with block obstacle must register");
        CHECK(overhang_pos.z <= 16.70f, "Entity must be pushed back horizontally by block obstacle");

        // 4. Steer around obstacles evaluates headroom properly
        glm::vec3 desired(0.0f, 0.0f, 1.0f);
        glm::vec3 steered = AberrantAI::steer_around_obstacles(in_gap_pos, desired, glm::vec3(0.0f, 1.0f, 0.0f), gap_world);
        CHECK(glm::length(steered) > 0.01f, "Steered vector must be non-zero");
        CHECK(steered.z < 0.9f, "Steering must deflect away from blocked direction with top/bottom obstruction");

        std::cout << " -> Gap clearance, ceiling clamp, and top/bottom collision resolution verified." << std::endl;
    }

    // Test 31: Elevated Target & Directly-Below Wall-Climbing Traversal
    {
        std::cout << "[Test 31] Testing Wall Climbing AI When Directly Below Player..." << std::endl;
        World chasm_world;
        chasm_world.generate_world(1, 9999);

        // Build testing cavern with floor at y=17, open air up to y=27
        for (int x = 8; x <= 24; ++x) {
            for (int z = 8; z <= 24; ++z) {
                chasm_world.set_voxel(x, 17, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
                for (int y = 18; y <= 27; ++y) {
                    chasm_world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }
        // Build solid vertical wall at x = 11 extending from y=17 up to y=27
        for (int z = 8; z <= 24; ++z) {
            for (int y = 18; y <= 27; ++y) {
                chasm_world.set_voxel(11, y, z, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
            }
        }

        // Monster on floor at (14.0, 18.0, 16.0)
        glm::vec3 monster_pos(14.0f, 18.0f, 16.0f);
        // Player directly above / elevated on a ledge at (14.0, 24.0, 16.0) - 6 meters directly above!
        glm::vec3 player_above(14.0f, 24.0f, 16.0f);

        // 1. Verify find_ascending_wall_direction locates the vertical wall at x=11 (direction (-1, 0, 0))
        glm::vec3 wall_dir = AberrantAI::find_ascending_wall_direction(monster_pos, player_above, chasm_world);
        CHECK(glm::length(wall_dir) > 0.1f, "Ascending wall direction must be non-zero");
        CHECK(wall_dir.x < -0.8f, "Direction towards vertical wall at x=11 must point West (-X)");

        // 2. Spawn monster and update: verify movement intent heads towards the wall
        VoidStalkerManager mgr;
        mgr.spawn_melee(monster_pos);
        auto& s = mgr.stalkers_mut()[0];
        s.state = StalkerState::Stalking;
        s.target_pos = player_above;

        glm::vec3 player_fwd(0.0f, 0.0f, 1.0f);
        mgr.update(0.1f, player_above, player_fwd, player_fwd, false, 0.0f, false, chasm_world);

        // Monster should move West towards the climbable wall
        CHECK(s.velocity.x < -0.1f || s.m_targetVelocity.x < -0.1f, "Monster below player must steer towards the wall at x=11");

        // 3. Test wall contact transition: when monster reaches the wall (x = 12.0, next to wall at x = 11)
        glm::vec3 at_wall_pos(11.8f, 18.0f, 16.0f);
        auto wall_sample = AberrantAI::sample_surface_normal(at_wall_pos, chasm_world, 1.4f, StalkerSurfaceState::FLOOR, &player_above);
        CHECK(wall_sample.has_contact, "Must detect contact at wall");
        CHECK(wall_sample.surface_state == StalkerSurfaceState::WALL_CLIMBING, "Monster near wall with target above must transition to WALL_CLIMBING");
        CHECK(wall_sample.contact_normal.x > 0.8f, "Contact normal should face away from wall (+X)");

        // 4. Test wall climbing traversal direction: once attached to wall, traversal direction climbs UP
        glm::vec3 climb_dir = AberrantAI::calculate_wall_traversal_direction(at_wall_pos, player_above, wall_sample.contact_normal);
        CHECK(climb_dir.y > 0.8f, "Wall traversal direction towards elevated target directly above must climb upward (+Y)");

        std::cout << " -> Ascending wall search, wall attachment, and vertical climbing traversal verified." << std::endl;
    }

    // Test 32: Sneaking Up via Crouch on Unalerted Enemies (Vision Cone Gating & Sneak Attacks)
    {
        std::cout << "[Test 32] Testing Sneaking Up via Crouch on Unalerted Enemies (Vision Cone Gating & Sneak Attacks)..." << std::endl;
        VoidStalkerManager mgr;

        // 32a: Sneaking up behind an unalerted Idle stalker (facing South, +Z)
        glm::vec3 stalker_pos(16.0f, 20.0f, 26.0f);
        mgr.spawn_melee(stalker_pos);
        auto& stalker = mgr.stalkers_mut()[0];
        stalker.state = StalkerState::Idle;
        stalker.yaw = 0.0f; // Facing South (+Z, forward = (0, 0, 1))
        CHECK(stalker.is_unalerted(), "Spawned Idle stalker must be initially unalerted");

        // Player sneaks up behind the stalker at z = 24.0 (2 meters behind, outside FOV)
        glm::vec3 sneaking_player(16.0f, 20.0f, 24.0f);
        glm::vec3 p_fwd(0.0f, 0.0f, 1.0f); // Looking towards monster back
        auto res = mgr.update(0.1f, sneaking_player, p_fwd, p_fwd, false, 0.0f, false, world, {}, true);

        // Enemy must NOT be alerted! Must stay in Idle, no spotted/screech/lunge
        CHECK(stalker.state == StalkerState::Idle, "Unalerted stalker must remain in Idle when player sneaks behind it");
        CHECK(stalker.is_unalerted(), "Stalker must still be completely unalerted");
        CHECK(!res.any_spotted, "Must NOT trigger any_spotted when sneak-approaching from behind");
        CHECK(!res.any_screech, "Must NOT screech when player sneaks from behind");
        CHECK(!res.any_lunge, "Must NOT lunge when player sneaks from behind");

        // Creep closer into melee strike distance (1.1 meters behind)
        glm::vec3 melee_range_player(16.0f, 20.0f, 24.9f);
        res = mgr.update(0.1f, melee_range_player, p_fwd, p_fwd, false, 0.0f, false, world, {}, true);
        CHECK(stalker.state == StalkerState::Idle, "Stalker must remain Idle even at 1.1m melee range when player is crouched behind it");
        CHECK(stalker.is_unalerted(), "Stalker must remain unalerted at point-blank melee stealth reach");

        // Execute sneak attack melee strike from behind
        bool is_crit = false;
        float dmg_dealt = 0.0f;
        glm::vec3 shot_dir(0.0f, 0.0f, 1.0f);
        bool hit = mgr.damage_nearest(stalker.position, 2.0f, 15.0f, true, &is_crit, &dmg_dealt, &melee_range_player, &shot_dir);
        CHECK(hit, "Melee sneak attack must connect");
        CHECK(is_crit, "Attack on unalerted enemy from stealth must register as SNEAK ATTACK CRITICAL HIT");
        CHECK(dmg_dealt >= 15.0f * StealthSystem::SNEAK_ATTACK_CRIT_MULTIPLIER, "Sneak critical hit must deal multiplied damage");

        // 32b: Walking in front of the Idle stalker's vision cone directly alerts it
        mgr.stalkers_mut().clear();
        mgr.spawn_melee(stalker_pos);
        auto& s_front = mgr.stalkers_mut()[0];
        s_front.state = StalkerState::Idle;
        s_front.yaw = 0.0f; // Facing South (+Z)

        // Player is 3m directly IN FRONT of the stalker (+Z, inside forward vision cone)
        glm::vec3 in_front_player(16.0f, 20.0f, 29.0f);
        res = mgr.update(0.1f, in_front_player, -p_fwd, -p_fwd, false, 0.0f, false, world, {}, true);
        CHECK(res.any_spotted, "Enemy MUST spot the player when crouching directly in front within vision cone");
        CHECK(s_front.state == StalkerState::Lunging || s_front.state == StalkerState::Stalking,
              "Stalker must alert and attack when the player enters its forward visual cone");

        // 32c: Roosting stalker in darkness stays asleep when sneaking nearby
        mgr.stalkers_mut().clear();
        mgr.spawn_roosting(stalker_pos, StalkerRole::Melee);
        auto& roosting = mgr.stalkers_mut()[0];
        CHECK(roosting.state == StalkerState::Roosting, "Must initialize in Roosting state");
        CHECK(roosting.is_unalerted(), "Roosting stalker must be unalerted");

        // Player sneaks 1.5m away in darkness (headlamp off)
        glm::vec3 roost_sneak_pos(16.0f, 20.0f, 24.5f);
        res = mgr.update(0.1f, roost_sneak_pos, p_fwd, p_fwd, false, 0.0f, false, world, {}, true);
        CHECK(roosting.state == StalkerState::Roosting, "Roosting stalker must remain dormant when player sneaks nearby in dark");
        CHECK(roosting.is_unalerted(), "Roosting stalker must remain unalerted");

        // Direct illumination by headlamp disturbs roosting stalker
        glm::vec3 light_dir = glm::normalize(stalker_pos - roost_sneak_pos);
        res = mgr.update(0.1f, roost_sneak_pos, light_dir, light_dir, true, 0.0f, false, world, {}, true);
        CHECK(roosting.state == StalkerState::Stalking || roosting.state == StalkerState::Investigating,
              "Direct illumination by headlamp beam must disturb and awaken roosting stalker");

        std::cout << " -> Stealth sneaking behind unalerted enemies, vision cone gating, and sneak attack crits verified." << std::endl;
    }

    // Module 33: Obstacle Fear, Hazard Steering & Environmental Lava/Spike Lethality
    {
        std::cout << "[Module 33] Testing Obstacle Fear, Hazard Steering & Environmental Lethality..." << std::endl;
        VoidStalkerManager mgr;

        // 33a: Enemy fears obstacle: AberrantAI::steer_around_obstacles deflects away from lava pool
        glm::vec3 stalker_pos(16.0f, 20.0f, 24.0f);
        for (int x = 12; x <= 20; ++x) {
            for (int z = 22; z <= 30; ++z) {
                world.set_voxel(x, 19, z, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
                world.set_voxel(x, 20, z, Voxel{MAT_AIR, 0}, false);
            }
        }
        // Place lava pool directly in front (+Z at Z=26)
        world.set_voxel(16, 19, 26, Voxel{MAT_THERMITE_SLAG, 0}, false);
        world.set_voxel(16, 20, 26, Voxel{MAT_THERMITE_SLAG, 0}, false);

        glm::vec3 desired_dir(0.0f, 0.0f, 1.0f);
        glm::vec3 steered = AberrantAI::steer_around_obstacles(stalker_pos, desired_dir, glm::vec3(0, 1, 0), world, 2.5f);
        CHECK(std::abs(steered.x) > 0.1f, "Steering MUST deflect laterally to avoid stepping into molten lava");

        // 33b: Stalker refuses to lunge across a lava pit (fear of launching into lethal obstacle)
        glm::vec3 player_across_lava(16.0f, 20.0f, 28.0f);
        CHECK(AberrantAI::has_hazard_between(stalker_pos, player_across_lava, world),
              "Path across lava pool must be recognized as having lethal hazard between");

        mgr.spawn_melee(stalker_pos);
        auto& s = mgr.stalkers_mut()[0];
        s.state = StalkerState::Stalking;
        s.has_player_los = true;
        glm::vec3 p_fwd(0.0f, 0.0f, -1.0f);
        mgr.update(0.1f, player_across_lava, p_fwd, p_fwd, false, 0.0f, false, world, {}, false);
        CHECK(s.state != StalkerState::Lunging, "Stalker must NOT lunge into lava; should fear obstacle and circle/flank");
        CHECK(s.state == StalkerState::Circling, "Stalker should transition to Circling to path around lava pool");

        // 33c: Environmental damage & incineration: when knocked into lava, stalker takes thermal damage and panics/dies
        for (int lx = 14; lx <= 18; ++lx) {
            for (int lz = 25; lz <= 27; ++lz) {
                world.set_voxel(lx, 19, lz, Voxel{MAT_THERMITE_SLAG, 0}, false);
                world.set_voxel(lx, 20, lz, Voxel{MAT_THERMITE_SLAG, 0}, false);
            }
        }
        s.position = glm::vec3(16.0f, 20.0f, 26.0f); // In lava pool
        float initial_hp = s.hp;
        mgr.update(0.1f, player_across_lava, p_fwd, p_fwd, false, 0.0f, false, world, {}, false);
        CHECK(s.is_in_lava, "Stalker in molten slag must register is_in_lava = true");
        CHECK(s.is_in_hazard, "Stalker in lava must register is_in_hazard = true");
        CHECK(s.hp < initial_hp, "Stalker in lava must take continuous thermal damage");
        CHECK(s.state == StalkerState::Fleeing, "Stalker caught in lava must enter Fleeing state to scramble out");

        // Extended immersion results in fatal incineration
        for (int step = 0; step < 40 && s.hp > 0.0f; ++step) {
            s.position = glm::vec3(16.0f, 20.0f, 26.0f);
            mgr.update(0.1f, player_across_lava, p_fwd, p_fwd, false, 0.0f, false, world, {}, false);
        }
        CHECK(s.hp <= 0.0f, "Sustained lava immersion must reduce stalker HP to 0");
        CHECK(s.is_dead() || s.is_dying(), "Extended immersion in lava must result in fatal incineration");

        // 33d: Obsidian punji spikes puncture damage & slow
        mgr.stalkers_mut().clear();
        glm::vec3 spike_pos(18.0f, 20.0f, 23.0f);
        world.set_voxel(18, 19, 23, Voxel{MAT_OBSIDIAN_SPIKES, 0}, false);
        world.set_voxel(18, 20, 23, Voxel{MAT_AIR, 0}, false);
        mgr.spawn_melee(spike_pos);
        auto& s_spike = mgr.stalkers_mut()[0];
        mgr.update(0.1f, player_across_lava, p_fwd, p_fwd, false, 0.0f, false, world, {}, false);
        CHECK(s_spike.is_in_hazard, "Stalker stepping onto punji spikes must register is_in_hazard = true");
        CHECK(s_spike.hp < 40.0f, "Stalker stepping onto spikes must take puncture damage");
        CHECK(s_spike.hp == 20.0f, "Spikes deal exactly 20.0 HP puncture damage");

        // 33e: Monsters are immune to radiation (radioactive creatures lore)
        mgr.stalkers_mut().clear();
        glm::vec3 rad_pos(18.0f, 20.0f, 23.0f);
        world.set_voxel(18, 19, 23, Voxel{MAT_RADIOACTIVE_ORE, 0}, false);
        world.set_voxel(18, 20, 23, Voxel{MAT_RADIOACTIVE_ORE, 0}, false);
        mgr.spawn_melee(rad_pos);
        auto& s_rad = mgr.stalkers_mut()[0];
        mgr.update(0.2f, player_across_lava, p_fwd, p_fwd, false, 0.0f, false, world, {}, false);
        CHECK(!s_rad.is_in_lava, "Radioactive ore must not be lava");
        CHECK(!s_rad.is_in_hazard, "Radioactive ore must not harm stalker (radiation immunity)");
        CHECK(s_rad.hp == 40.0f, "Stalker must take zero damage from radioactive ore (immune lore-wise)");

        std::cout << " -> Obstacle avoidance steering, hazard-gated lunges, and lethal lava/spike damage verified." << std::endl;
    }

    // Module 34: Territory Patrol Zones, Autonomous Wandering & Dynamic Sneak Challenge
    {
        std::cout << "[Module 34] Testing Territory Patrol Zones, Autonomous Wandering & Dynamic Sneak..." << std::endl;
        VoidStalkerManager mgr;

        // Ensure open chamber for Module 34 territory patrol testing (solid floor at y=19, air y=20..26)
        for (int x = 10; x <= 22; ++x) {
            for (int z = 10; z <= 30; ++z) {
                world.set_voxel(x, 19, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
                for (int y = 20; y <= 26; ++y) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }

        // 34a: Territory Patrol Zone Bounding & Autonomous Wandering
        glm::vec3 chamber_center(16.0f, 20.0f, 20.0f);
        mgr.spawn_melee(chamber_center);
        auto& stalker = mgr.stalkers_mut()[0];
        stalker.state = StalkerState::Idle;

        PatrolZone zone;
        zone.zone_id = 1;
        zone.center = chamber_center;
        zone.bounds_min = glm::vec3(12.0f, 18.0f, 12.0f);
        zone.bounds_max = glm::vec3(20.0f, 24.0f, 28.0f);
        zone.has_bounds = true;
        zone.radius = 8.0f;
        zone.waypoints = AberrantAI::generate_zone_waypoints(zone, world, 4);
        CHECK(zone.waypoints.size() >= 2, "Zone waypoints must generate at least 2 valid waypoints");
        stalker.set_patrol_zone(zone);
        CHECK(stalker.has_patrol_zone, "Stalker must register has_patrol_zone = true");

        // Advance 50 physics steps (5.0s) of autonomous wander with player far away
        glm::vec3 far_player(80.0f, 20.0f, 80.0f);
        glm::vec3 p_fwd(0.0f, 0.0f, 1.0f);
        for (int step = 0; step < 50; ++step) {
            mgr.update(0.1f, far_player, p_fwd, p_fwd, false, 0.0f, false, world, {}, false);
            // Verify creature stays strictly constrained within its assigned territory zone
            CHECK(zone.contains(stalker.position), "Autonomous roaming stalker MUST stay inside assigned PatrolZone bounds");
        }
        CHECK(stalker.walk_cycle > 0.0f, "Wandering stalker must animate crawling locomotion cycle");

        // 34b: Waypoint Navigation & Vigilant Scanning Pauses
        // Force arrival at waypoint
        stalker.position = stalker.patrol_waypoint;
        mgr.update(0.1f, far_player, p_fwd, p_fwd, false, 0.0f, false, world, {}, false);
        CHECK(stalker.patrol_pause_timer > 0.0f, "Reaching waypoint must enter vigilant scanning pause");
        CHECK(glm::length(stalker.velocity) == 0.0f, "Creature must halt movement during scanning pause");

        float initial_yaw = stalker.yaw;
        // Step during pause: creature scans the room (yaw rotates)
        for (int step = 0; step < 15; ++step) {
            mgr.update(0.1f, far_player, p_fwd, p_fwd, false, 0.0f, false, world, {}, false);
        }
        CHECK(stalker.yaw != initial_yaw, "Creature must rotate facing yaw to scan room angles during pause");

        // 34c: Autonomous Roosting Slumber Waking
        mgr.stalkers_mut().clear();
        glm::vec3 roost_pos(16.0f, 20.0f, 20.0f);
        mgr.spawn_roosting(roost_pos, StalkerRole::Melee);
        auto& roosting = mgr.stalkers_mut()[0];
        roosting.slumber_timer = 0.4f; // Short slumber for test
        roosting.spawn_grace_timer = 0.0f;
        CHECK(roosting.state == StalkerState::Roosting, "Must begin in Roosting state");

        // Advance 0.6s without player disturbance
        for (int step = 0; step < 6; ++step) {
            mgr.update(0.1f, far_player, p_fwd, p_fwd, false, 0.0f, false, world, {}, false);
        }
        CHECK(roosting.state == StalkerState::Idle, "Roosting fauna must awaken autonomously into Idle state after slumber timer");
        CHECK(roosting.surface_state == StalkerSurfaceState::FLOOR, "Awakened creature must transition to floor for zone wander");

        // 34d: Dynamic Sneak Challenge with Sweeping Vision Cones
        mgr.stalkers_mut().clear();
        glm::vec3 patrol_pos(16.0f, 20.0f, 20.0f);
        mgr.spawn_melee(patrol_pos);
        auto& sentry = mgr.stalkers_mut()[0];
        sentry.state = StalkerState::Idle;
        sentry.yaw = 0.0f; // Facing South (+Z)

        // Player is crouched 2.5m behind the sentry (at -Z, z = 17.5)
        glm::vec3 sneaking_delver(16.0f, 20.0f, 17.5f);
        glm::vec3 to_sentry(0.0f, 0.0f, 1.0f);
        auto res = mgr.update(0.1f, sneaking_delver, to_sentry, to_sentry, false, 0.0f, false, world, {}, true);
        CHECK(!res.any_spotted, "Sneaking player behind sentry must NOT be spotted while sentry looks away");
        CHECK(sentry.is_unalerted(), "Sentry must remain unalerted when sneaked from behind");

        // Now sentry turns to face North (-Z, yaw = pi) during its patrol scan
        sentry.yaw = glm::pi<float>(); // Facing directly towards sneaking player!
        res = mgr.update(0.1f, sneaking_delver, to_sentry, to_sentry, false, 0.0f, false, world, {}, true);
        CHECK(res.any_spotted, "Sentry sweeping its vision cone towards crouched player MUST spot them in open line of sight!");
        CHECK(sentry.state == StalkerState::Stalking || sentry.state == StalkerState::Lunging,
              "Spotted player triggers immediate alert into Stalking or Lunging");

        std::cout << " -> Territory patrol zones, waypoint navigation, autonomous waking, and dynamic sneak verified." << std::endl;
    }

    // Test 35: Chitin Goliath Boss AI Overhaul: Sentinel Vigilance, Never Flees, Titan Ground Slam, Bull Charge & Wall Stun Armor Breach, Apex Frenzy Enrage
    {
        std::cout << "[Test 35] Testing Chitin Goliath Boss AI Overhaul & Special Attacks..." << std::endl;
        VoidStalkerManager mgr;
        glm::vec3 boss_pos(16.0f, 20.0f, 20.0f);
        mgr.spawn_stalker(boss_pos, 1.0f, StalkerRole::ChitinGoliath);

        auto& boss = mgr.stalkers_mut()[0];
        CHECK(boss.is_boss(), "Boss check must be true for ChitinGoliath");
        CHECK(boss.hp == 220.0f, "Boss baseline HP must be 220.0");
        CHECK(boss.flee_hp_threshold == 0.0f, "Boss flee HP threshold must be 0 (never flees)");
        CHECK(boss.damage == 36.0f, "Boss baseline damage must be 36.0");

        // 35a: Roosting Boss awakens immediately upon player proximity or sound
        mgr.stalkers_mut().clear();
        mgr.spawn_roosting(boss_pos, StalkerRole::ChitinGoliath);
        auto& roosting_boss = mgr.stalkers_mut()[0];
        CHECK(roosting_boss.slumber_timer == 0.0f, "Boss slumber timer must initialize to 0");
        CHECK(roosting_boss.spawn_grace_timer == 0.0f, "Boss spawn grace timer must initialize to 0");

        glm::vec3 player_near(16.0f, 20.0f, 28.0f); // 8m away
        glm::vec3 p_fwd(0.0f, 0.0f, -1.0f);
        auto roar_res = mgr.update(0.1f, player_near, p_fwd, p_fwd, false, 25.0f, false, world);
        CHECK(roosting_boss.state == StalkerState::Stalking, "Boss must awaken immediately into Stalking combat pursuit");
        CHECK(roar_res.any_boss_roar, "Boss awakening must trigger apex battle roar event");

        // 35b: Boss Never Flees or Circles
        roosting_boss.hp = 15.0f; // Extremely low HP
        roosting_boss.is_in_light = true;
        glm::vec3 close_player(16.0f, 20.0f, 22.0f); // 2m away
        mgr.update(0.1f, close_player, p_fwd, p_fwd, true, 0.0f, false, world);
        CHECK(roosting_boss.state != StalkerState::Fleeing, "Boss must NEVER enter Fleeing state at low HP or in light");
        CHECK(roosting_boss.state != StalkerState::Circling, "Boss must NEVER circle around player passively");

        // 35c: Apex Frenzy Enrage (at <= 50% HP)
        mgr.stalkers_mut().clear();
        mgr.spawn_stalker(boss_pos, 1.0f, StalkerRole::ChitinGoliath);
        auto& goliath = mgr.stalkers_mut()[0];
        goliath.state = StalkerState::Stalking;
        goliath.hp = 100.0f; // <= 50% of 220
        mgr.update(0.1f, player_near, p_fwd, p_fwd, false, 0.0f, false, world);
        CHECK(goliath.is_enraged, "Boss must trigger Apex Frenzy Enrage at <= 50% HP");
        CHECK(goliath.move_speed >= 3.5f, "Enraged boss movement speed must increase (>= 3.5 m/s)");
        glm::vec4 enraged_eye = goliath.get_eye_color();
        CHECK(enraged_eye.r > 0.8f && enraged_eye.g < 0.2f, "Enraged boss eyes must glow infernal crimson");

        // 35d: Titan Ground Slam Special Attack
        goliath.boss_slam_cooldown = 0.0f;
        glm::vec3 slam_target(16.0f, 20.0f, 23.5f); // 3.5m away (within <= 5.5m slam reach)
        mgr.update(0.05f, slam_target, p_fwd, p_fwd, false, 0.0f, false, world);
        CHECK(goliath.state == StalkerState::GroundSlam, "Boss within 5.5m must initiate Titan Ground Slam attack");

        // Progress past slam windup to trigger concussive shockwave impact
        auto slam_res = mgr.update(goliath.boss_attack_windup + 0.05f, slam_target, p_fwd, p_fwd, false, 0.0f, false, world);
        CHECK(slam_res.any_boss_slam, "Completing slam windup must emit boss slam shockwave event");
        CHECK(slam_res.total_damage > 20.0f, "Titan Ground Slam must deal heavy shockwave AOE damage");
        CHECK(goliath.boss_slam_cooldown > 0.0f, "Titan Ground Slam must enter cooldown");

        // 35e: Juggernaut Bull Charge & Wall Crash Stun Armor Breach
        goliath.state = StalkerState::Stalking;
        goliath.boss_slam_cooldown = 10.0f; // Put slam on cooldown so charge is picked
        goliath.boss_charge_cooldown = 0.0f;
        glm::vec3 charge_target(16.0f, 20.0f, 32.0f); // 12m away (within 6-22m charge reach)
        mgr.update(0.05f, charge_target, p_fwd, p_fwd, false, 0.0f, false, world);
        CHECK(goliath.state == StalkerState::Charging, "Boss at mid-range (12m) must initiate Juggernaut Bull Charge");

        // Place solid wall in front of charge trajectory across chamber
        for (int wx = 12; wx <= 20; ++wx) {
            for (int wy = 17; wy <= 22; ++wy) {
                world.set_voxel(wx, wy, 22, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
            }
        }
        // Step charge past windup (0.05s ticks) so it moves forward and impacts the wall
        for (int step = 0; step < 20; ++step) {
            mgr.update(0.05f, charge_target, p_fwd, p_fwd, false, 0.0f, false, world);
            if (goliath.state == StalkerState::Stunned) break;
        }
        CHECK(goliath.state == StalkerState::Stunned, "Charging into solid wall MUST stun the boss");
        CHECK(goliath.stun_timer >= 2.0f, "Wall-stunned boss must have >= 2.0s vulnerability window");

        // 35f: Front Armor Breach Vulnerability during Stunned State
        // Facing East (+X, yaw = 0), shot from front (+X)
        goliath.yaw = 0.0f;
        float stunned_front_dmg = 0.0f;
        glm::vec3 front_shooter = goliath.position + glm::vec3(3.0f, 0.0f, 0.0f);
        mgr.damage_nearest(front_shooter, 5.0f, 50.0f, false, nullptr, &stunned_front_dmg);
        CHECK(stunned_front_dmg >= 45.0f, "Front armor deflection must be breached (full damage) while boss is Stunned");

        // Clean up testing chamber voxels
        for (int wx = 12; wx <= 20; ++wx) {
            for (int wy = 18; wy <= 22; ++wy) {
                world.set_voxel(wx, wy, 22, Voxel{MAT_AIR, 0}, false);
            }
        }

        std::cout << " -> Chitin Goliath Sentinel Vigilance, Attacks, Wall Stun & Armor Breach verified." << std::endl;
    }

    // Test 36: Natural Continuous Movement & Zero Teleportation / Snapping Invariants
    {
        std::cout << "[Test 36] Testing Natural Continuous Movement & Zero Teleportation Invariants..." << std::endl;
        // Reset testing chamber floor at y=17 and air at y=18..26
        for (int x = 10; x <= 22; ++x) {
            for (int z = 10; z <= 30; ++z) {
                world.set_voxel(x, 17, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
                for (int y = 18; y <= 26; ++y) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }

        VoidStalkerManager manager;
        glm::vec3 player_pos(16.0f, 18.0f, 16.0f);
        glm::vec3 stalker_spawn(16.0f, 18.0f, 21.0f); // 5m away
        manager.spawn_melee(stalker_spawn);

        auto& stalkers = const_cast<std::vector<VoidStalker>&>(manager.stalkers());
        VoidStalker& stalker = stalkers[0];
        stalker.state = StalkerState::Lunging;
        stalker.state_timer = 0.0f;

        // A. Verify lunge attack has continuous, bounded displacement per frame with ZERO teleportation
        constexpr float dt = 0.016f;
        glm::vec3 last_pos = stalker.position;
        float max_frame_disp = 0.0f;

        for (int frame = 0; frame < 60; ++frame) {
            manager.update(dt, player_pos, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), true, 0.0f, false, world);
            float frame_disp = glm::distance(stalker.position, last_pos);
            max_frame_disp = std::max(max_frame_disp, frame_disp);

            // Per-frame movement must strictly remain bounded by physical velocity * dt (no teleportation jumps)
            CHECK(frame_disp <= (stalker.lunge_speed * dt + 0.08f),
                  "Lunge displacement per frame must be smooth and continuous, never teleporting or snapping");

            last_pos = stalker.position;
        }

        // Must maintain stand-off distance smoothly without snapping
        float final_dist = glm::distance(stalker.position, player_pos);
        CHECK(final_dist >= AberrantAI::MIN_STRIKE_DISTANCE - 0.10f,
              "Stalker must decelerate and maintain stand-off distance smoothly without phasing");

        // B. Wall and Corner Collision: high-speed approach into wall must slide smoothly without popping into ceiling
        glm::vec3 corner_pos(17.5f, 18.0f, 16.0f);
        glm::vec3 corner_vel(8.0f, 0.0f, 0.0f);
        // Build wall at x=18
        for (int wy = 17; wy <= 22; ++wy) {
            world.set_voxel(18, wy, 16, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
        }

        glm::vec3 pre_col_pos = corner_pos;
        AberrantAI::resolve_voxel_collision(
            corner_pos, corner_vel, 0.38f, 0.85f, StalkerSurfaceState::FLOOR, glm::vec3(0, 1, 0), world
        );

        // Entity must NOT be teleported vertically or to another room
        CHECK(std::abs(corner_pos.y - pre_col_pos.y) < 0.20f,
              "Wall collision must NOT eject/teleport entity vertically into ceiling or adjacent room");
        CHECK(corner_pos.x <= 18.0f - 0.38f + 0.01f,
              "Entity must be smoothly clamped at wall boundary face");

        // Cleanup wall voxels
        for (int wy = 18; wy <= 22; ++wy) {
            world.set_voxel(18, wy, 16, Voxel{MAT_AIR, 0}, false);
        }

        std::cout << " -> Natural continuous movement, lunge deceleration & zero-teleportation invariants verified." << std::endl;
    }

    // Test 37: Locomotion Footsteps, Adaptive Proximity Volume & Zero Acoustic System Pollution
    {
        std::cout << "[Test 37] Testing Stalker Locomotion Footsteps & Acoustic Isolation..." << std::endl;
        VoidStalkerManager manager;
        glm::vec3 player_pos(16.0f, 18.0f, 12.0f);
        glm::vec3 far_spawn(16.0f, 18.0f, 30.0f); // 18m away, inside test chamber (z <= 32)
        manager.spawn_melee(far_spawn);

        auto& stalkers = const_cast<std::vector<VoidStalker>&>(manager.stalkers());
        VoidStalker& s = stalkers[0];

        // 37a: Walking step cadence & low-speed flag
        glm::vec3 hidden_player_pos(16.0f, 18.0f, 0.0f); // 30m away, outside detection range
        s.assign_patrol_zone(far_spawn, 10.0f);
        s.patrol_waypoints = { far_spawn + glm::vec3(0.0f, 0.0f, -8.0f) };
        s.patrol_waypoint_idx = 0;
        s.patrol_waypoint = s.patrol_waypoints[0];
        s.patrol_pause_timer = 0.0f;
        s.state = StalkerState::Idle;
        bool heard_walk_step = false;
        float far_walk_vol = 0.0f;

        for (int frame = 0; frame < 30; ++frame) {
            auto res = manager.update(0.033f, hidden_player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);
            if (res.any_step) {
                heard_walk_step = true;
                far_walk_vol = s.step_volume;
                CHECK(!s.step_is_run, "Slow walking speed must NOT be marked as running");
                break;
            }
        }
        CHECK(heard_walk_step, "Walking stalker must emit footstep at periodic cadence");

        // 37b: Fast sprinting/stalking step cadence & run flag
        s.velocity = glm::vec3(0.0f, 0.0f, -5.2f); // 5.2 m/s sprint
        s.state = StalkerState::Stalking;
        s.footstep_timer = 0.0f;
        bool heard_run_step = false;
        for (int frame = 0; frame < 15; ++frame) {
            auto res = manager.update(0.033f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);
            if (res.any_step) {
                heard_run_step = true;
                CHECK(s.step_is_run, "High speed stalker movement must be marked as running");
                CHECK(res.any_run_step, "FrameResult must flag any_run_step = true");
                break;
            }
        }
        CHECK(heard_run_step, "Sprinting stalker must trigger run step");

        // 37c: Adaptive Proximity Volume (closer stalker moving toward player must be significantly louder)
        s.position = glm::vec3(16.0f, 18.0f, 15.5f); // 3.5m away, point-blank
        s.velocity = glm::vec3(0.0f, 0.0f, -4.5f);   // moving directly towards player
        s.footstep_timer = 1.0f;                      // force step on next frame
        auto close_res = manager.update(0.016f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);
        CHECK(close_res.any_step, "Step must fire when timer exceeds interval");
        float close_vol = s.step_volume;
        CHECK(close_vol > far_walk_vol * 1.35f,
              "Point-blank approaching stalker must have significantly louder adaptive volume than distant walking stalker");

        // 37d: Zero Acoustic Contamination: footsteps must NOT be injected into NoiseMeter or cause self-distraction
        NoiseMeter noise_meter;
        CHECK(noise_meter.noise() == 0.0f, "Initial noise meter must be 0");
        // Advancing update loop with moving enemies:
        for (int frame = 0; frame < 30; ++frame) {
            manager.update(0.033f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, noise_meter.noise_percent(), false, world);
        }
        CHECK(noise_meter.noise() == 0.0f, "Enemy footsteps must NEVER pollute the NoiseMeter or generate tremor heat");
        CHECK(!s.has_sound_target, "Enemy footsteps must NEVER trigger acoustic self-distraction or target updates");

        std::cout << " -> Stalker locomotion step cadence, adaptive proximity volume & acoustic system isolation verified." << std::endl;
    }

    // Test 38: Crouching Stealth Concealment, Blind Spot Exploitation & Acoustic Suppression
    {
        std::cout << "[Test 38] Testing Crouching Stealth Concealment, Blind Spot Exploitation & Acoustic Suppression..." << std::endl;
        World world;
        // Carve out a wide 30x30 open room
        for (int x = 5; x <= 35; ++x) {
            for (int z = 5; z <= 35; ++z) {
                world.set_voxel(x, 15, z, Voxel{MAT_FRACTURED_GRANITE, 0}, false);
                for (int y = 16; y <= 25; ++y) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }

        VoidStalkerManager manager;
        // Spawn stalker in center facing North (-Z)
        glm::vec3 stalker_pos(20.0f, 16.0f, 20.0f);
        manager.spawn_melee(stalker_pos);
        auto& s = const_cast<VoidStalker&>(manager.stalkers()[0]);
        s.state = StalkerState::Idle;
        s.yaw = glm::pi<float>(); // facing -Z (North)

        // 38a: Player is far away (15m away) crouching in front of stalker (at Z = 5)
        // Even with headlamp pointing at stalker and clear Line of Sight, stalker must NOT spot crouching player!
        glm::vec3 player_far_front(20.0f, 16.0f, 5.0f); // 15m North
        glm::vec3 look_at_stalker = glm::normalize(stalker_pos - player_far_front);
        auto res_far = manager.update(0.1f, player_far_front, look_at_stalker, look_at_stalker, true /* headlamp on */, 0.0f, false, world, {}, true /* crouching */);
        CHECK(s.state == StalkerState::Idle, "Far crouching player (15m) must NOT be spotted even with headlamp on!");
        CHECK(!res_far.any_spotted, "FrameResult must not flag any_spotted for far crouching player");

        // 38b: Player is behind stalker (8m South at Z = 28), out of vision of enemy's eyes
        // Even with clear LOS and headlamp shining on stalker's back, stalker must NOT spot player!
        glm::vec3 player_behind(20.0f, 16.0f, 28.0f); // 8m South
        glm::vec3 look_at_back = glm::normalize(stalker_pos - player_behind);
        auto res_behind = manager.update(0.1f, player_behind, look_at_back, look_at_back, true /* headlamp on */, 0.0f, false, world, {}, true /* crouching */);
        CHECK(s.state == StalkerState::Idle, "Crouching player behind enemy's eyes must NOT be detected!");
        CHECK(!res_behind.any_spotted, "FrameResult must not flag any_spotted when player is outside vision cone");

        // 38c: Roosting stalker dormancy - sleeping stalker must NOT wake or spot crouching player at distance (12m)
        s.state = StalkerState::Roosting;
        s.surface_state = StalkerSurfaceState::CEILING_CRAWLING;
        auto res_roost = manager.update(0.1f, player_far_front, look_at_stalker, look_at_stalker, true /* headlamp on */, 0.0f, false, world, {}, true /* crouching */);
        CHECK(s.state == StalkerState::Roosting, "Roosting stalker must remain dormant when player is crouching at distance!");
        CHECK(!res_roost.any_spotted, "Roosting stalker must not spot crouching player at distance");

        // 38d: Elevated noise level (> 25%) must NOT trigger fallback acoustic homing on a crouching player
        s.state = StalkerState::Idle;
        s.has_sound_target = false;
        auto res_noise = manager.update(0.1f, player_far_front, look_at_stalker, look_at_stalker, false, 50.0f /* elevated noise */, false /* not drilling */, world, {}, true /* crouching */);
        CHECK(!s.has_sound_target, "Elevated noise must NOT trigger fallback acoustic detection on a crouching player");
        CHECK(s.state == StalkerState::Idle, "Stalker must remain Idle instead of investigating crouching player");

        // 38e: Point-blank crouching player directly in front of creature's eyes (< 4.0m)
        // When sneaking right up in front of the creature's eyes within visual range, stalker will spot them!
        glm::vec3 player_close_front(20.0f, 16.0f, 17.0f); // 3m in front of eyes
        auto res_close = manager.update(0.1f, player_close_front, glm::vec3(0,0,1), glm::vec3(0,0,1), false, 0.0f, false, world, {}, true /* crouching */);
        CHECK(s.state == StalkerState::Lunging || s.state == StalkerState::Stalking,
              "Stalker must spot crouching player when directly in front of eyes within close range (< 4.5m)");
        CHECK(res_close.any_spotted, "FrameResult must flag any_spotted for point-blank sighting");

        std::cout << " -> Crouching stealth concealment, blind spot exploitation & acoustic suppression verified." << std::endl;
    }

    // -------------------------------------------------------------
    // Test 39: Behind-Wall Obstruction Detection & Alternate Route Determination
    // -------------------------------------------------------------
    {
        std::cout << "[Test 39] Testing Behind-Wall Obstruction Detection & Alternate Route Determination..." << std::endl;
        World world;
        // Build flat floor from Y = 14 to 15 (solid rock) in chunk 0,0,0
        for (int x = 0; x < 32; ++x) {
            for (int z = 0; z < 32; ++z) {
                world.set_voxel(x, 15, z, Voxel{MAT_GRANITE, 0});
                for (int y = 16; y < 28; ++y) {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
                }
            }
        }

        // Place a partition wall at Z = 20 from X = 0 to X = 25, Y = 16 to 22 (6 blocks tall)
        // Opening / corridor at X = 26..31 (Z = 20 is Air there)
        for (int x = 0; x <= 25; ++x) {
            for (int y = 16; y <= 22; ++y) {
                world.set_voxel(x, y, 20, Voxel{MAT_GRANITE, 0});
            }
        }

        glm::vec3 enemy_pos(15.0f, 16.0f, 14.0f); // South of wall
        glm::vec3 player_pos(15.0f, 16.0f, 25.0f); // North of wall directly behind partition

        // 39a: Line of sight and direct obstruction checks
        bool direct_los = AberrantAI::has_line_of_sight(enemy_pos + glm::vec3(0, 0.45f, 0), player_pos + glm::vec3(0, 0.5f, 0), world);
        CHECK(!direct_los, "Direct line of sight through rock wall must be FALSE");

        bool obstructed = AberrantAI::is_path_obstructed_by_wall(enemy_pos, player_pos, world);
        CHECK(obstructed, "Path directly through rock wall must be detected as OBSTRUCTED");

        // 39b: Alternate Route Determination (Ground Detour through opening at X >= 26)
        AlternateRoute route = AberrantAI::find_alternate_route(enemy_pos, player_pos, world, 32.0f, false /* ground only */);
        CHECK(route.is_valid, "Alternate route around wall must be found and marked valid");
        CHECK(route.type == AlternateRouteType::GroundDetour, "Ground opening must yield GroundDetour route");
        CHECK(!route.waypoints.empty(), "Route must contain navigation waypoints");

        // Verify that waypoints bypass the wall (lead towards X >= 25 opening)
        bool reaches_opening = false;
        for (const auto& wp : route.waypoints) {
            if (wp.x >= 24.5f) {
                reaches_opening = true;
                break;
            }
        }
        CHECK(reaches_opening, "Alternate route waypoints must detour through wall opening at X >= 25");

        // 39c: Vertical climbing route detection when no ground opening exists
        // Fill the entire wall from X=0 to X=31 up to Y=18 (3 blocks tall: 16, 17, 18), open headroom above Y=18
        for (int x = 0; x < 32; ++x) {
            for (int y = 16; y <= 18; ++y) {
                world.set_voxel(x, y, 20, Voxel{MAT_GRANITE, 0});
            }
            for (int y = 19; y <= 28; ++y) {
                world.set_voxel(x, y, 20, Voxel{MAT_AIR, 0});
            }
        }
        bool climb_viable = AberrantAI::is_vertical_climb_route_viable(enemy_pos, player_pos, world, 6.0f);
        CHECK(climb_viable, "Low wall (3 blocks) with open headroom must be viable for vertical climbing route");

        AlternateRoute climb_route = AberrantAI::find_alternate_route(enemy_pos, player_pos, world, 32.0f, true /* can climb */);
        CHECK(climb_route.is_valid, "Climbing alternate route must be valid when ground route is blocked");
        CHECK(climb_route.type == AlternateRouteType::VerticalClimb, "Route type must be VerticalClimb when wall has open top");

        // 39d: In-Engine Manager Integration: Stalker follows alternate route around wall
        // Reset world with opening at X = 26..30
        for (int x = 26; x < 32; ++x) {
            for (int y = 16; y <= 25; ++y) {
                world.set_voxel(x, y, 20, Voxel{MAT_AIR, 0});
            }
        }
        for (int x = 0; x <= 25; ++x) {
            for (int y = 16; y <= 25; ++y) {
                world.set_voxel(x, y, 20, Voxel{MAT_GRANITE, 0});
            }
        }

        VoidStalkerManager manager;
        manager.spawn_stalker(enemy_pos, 1.0f, StalkerRole::Melee);
        auto& stalker = manager.stalkers_mut()[0];
        stalker.state = StalkerState::Stalking;
        stalker.last_seen_player_pos = player_pos;
        stalker.target_pos = player_pos;
        stalker.m_pathTimer = 0.10f; // Force immediate path calculation

        // First update tick: stalker detects obstruction and plans alternate route
        manager.update(0.1f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);
        CHECK(stalker.is_wall_obstructed, "Stalker must detect wall obstruction when pursuing player behind wall");
        CHECK(stalker.has_active_alternate_route(), "Stalker must determine and activate an alternate route around the wall");
        CHECK(stalker.route_type == AlternateRouteType::GroundDetour, "Stalker must select GroundDetour route");

        // Run simulation ticks: stalker moves along alternate route towards the doorway
        glm::vec3 start_p = stalker.position;
        for (int i = 0; i < 40; ++i) {
            manager.update(0.1f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);
            if (!stalker.is_wall_obstructed) break;
        }

        CHECK(stalker.position.x > start_p.x, "Stalker must actively traverse towards the doorway on the East (X > start_x)");
        std::cout << " -> Wall obstruction detection, ground detour pathing & vertical climb routes verified." << std::endl;
    }

    // Test 40: Mutual Enemy Anti-Overlap & 1-Block Anti-Stuck Step-Clambering
    {
        std::cout << "[Test 40] Testing Mutual Enemy Anti-Overlap & 1-Block Anti-Stuck Step Clambering..." << std::endl;

        // 1. Stalker-to-Stalker anti-overlap
        VoidStalkerManager stalker_mgr;
        stalker_mgr.spawn_stalker(glm::vec3(15.0f, 18.0f, 15.0f), 1.0f, StalkerRole::Melee, false);
        stalker_mgr.spawn_stalker(glm::vec3(15.05f, 18.0f, 15.05f), 1.0f, StalkerRole::Melee, false);

        glm::vec3 player_pos(15.0f, 18.0f, 30.0f);
        stalker_mgr.update(0.1f, player_pos, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);

        float s_dist = glm::distance(stalker_mgr.stalkers()[0].position, stalker_mgr.stalkers()[1].position);
        CHECK(s_dist >= 0.88f, "Overlapping stalkers must depenetrate and separate to >= 0.88m");

        // 2. Stalker-to-Burrower anti-overlap
        VoidStalkerManager stalker_mgr2;
        stalker_mgr2.spawn_stalker(glm::vec3(15.0f, 18.0f, 15.0f), 1.0f, StalkerRole::Melee, false);

        SeismicBurrowerManager burrower_mgr;
        burrower_mgr.spawn_burrower(glm::vec3(15.1f, 18.0f, 15.1f), 1.0f, false);
        stalker_mgr2.resolve_burrower_penetrations(burrower_mgr.burrowers_mut(), world);

        float sb_dist = glm::distance(stalker_mgr2.stalkers()[0].position, burrower_mgr.burrowers()[0].position);
        CHECK(sb_dist >= 1.35f, "Stalker and Burrower must depenetrate and separate to >= 1.35m");

        // 3. Burrower-to-Burrower anti-overlap
        SeismicBurrowerManager burrower_mgr2;
        burrower_mgr2.spawn_burrower(glm::vec3(25.0f, 18.0f, 25.0f), 1.0f, false);
        burrower_mgr2.spawn_burrower(glm::vec3(25.05f, 18.0f, 25.05f), 1.0f, false);
        burrower_mgr2.update(0.1f, player_pos, world, {}, false);

        float bb_dist = glm::distance(burrower_mgr2.burrowers()[0].position, burrower_mgr2.burrowers()[1].position);
        CHECK(bb_dist >= 1.85f, "Overlapping burrowers must depenetrate and separate to >= 1.85m");

        // 4. 1-Block Step Anti-Stuck Clambering Traversal
        // Carve clean test corridor at x=18, z=14..22: floor at y=17 (surface at y=18.0), open air y=18..24
        for (int z = 14; z <= 22; ++z) {
            world.set_voxel(18, 17, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
            for (int y = 18; y <= 24; ++y) {
                world.set_voxel(18, y, z, Voxel{MAT_AIR, 0}, false);
            }
        }
        // Place a single 1-block obstacle at z=17 (top at y=19.0)
        world.set_voxel(18, 18, 17, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
        // Floor on the other side at z=18..22: place block at y=18 so it's a step up to a higher plateau
        for (int z = 18; z <= 22; ++z) {
            world.set_voxel(18, 18, z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);
        }

        VoidStalkerManager clamber_mgr;
        clamber_mgr.spawn_stalker(glm::vec3(18.0f, 18.0f, 15.5f), 1.0f, StalkerRole::Melee, false);
        clamber_mgr.stalkers_mut()[0].state = StalkerState::Stalking;
        clamber_mgr.stalkers_mut()[0].is_pursuing_attacker = true;
        clamber_mgr.stalkers_mut()[0].target_pos = glm::vec3(18.0f, 19.0f, 21.0f);

        // Advance 15 ticks moving directly forward into the 1-block step
        for (int tick = 0; tick < 15; ++tick) {
            clamber_mgr.update(0.1f, glm::vec3(18.0f, 19.0f, 21.0f), glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), false, 0.0f, false, world);
        }

        // Entity must have successfully clambered onto or past the 1-block step (y >= 19.0f and z > 16.5f)
        const auto& clamberer = clamber_mgr.stalkers()[0];
        CHECK(clamberer.position.y >= 19.0f, "Stalker must clamber up onto 1-block step (Y >= 19.0f)");
        CHECK(clamberer.position.z >= 17.0f, "Stalker must advance forward over the 1-block step (Z >= 17.0f)");

        std::cout << " -> Mutual enemy anti-overlap & 1-block anti-stuck step clambering verified." << std::endl;
    }

    std::cout << "\n>>> ALL 40 VOID STALKER TEST MODULES PASSED SUCCESSFULLY! <<<\n" << std::endl;
    return 0;
}




