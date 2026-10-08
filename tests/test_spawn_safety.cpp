#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../src/voxel/world.hpp"
#include "../src/entities/enemies/void_stalker.hpp"
#include "../src/entities/vault_door.hpp"
#include "../src/systems/stealth_system.hpp"
#include "../src/systems/mission_system.hpp"
#include "../src/ai/spawn_manager.hpp"
#include "../src/player/controller.hpp"
#include "../src/ui/hud.hpp"

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

#define ASSERT_GE(a, b) \
    if (!((a) >= (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") >= " #b " (" << (b) << ") failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define TEST(Suite, Case) void Suite##_##Case()

// ── Test 1: No Fauna Within Safe Radius On World Load ──
TEST(SpawnSafetyTest, NoFaunaWithinSafeRadiusOnWorldLoad) {
    std::cout << "[RUN] SpawnSafetyTest.NoFaunaWithinSafeRadiusOnWorldLoad" << std::endl;

    const uint32_t seeds[10] = { 101, 202, 303, 404, 505, 606, 707, 808, 909, 1337 };

    for (int sector = 1; sector <= 2; ++sector) {
        for (int i = 0; i < 10; ++i) {
            uint32_t seed = seeds[i];
            World world(seed);
            world.generate_world(sector, seed);

            const glm::vec3& playerSpawnPos = world.GetPlayerSpawnPos();
            const auto& entities = world.GetActiveEntities();

            // Verify active entities exist in the sector
            ASSERT_FALSE(entities.empty());

            // Strict insertion quarantine sphere enforcement: >= 28.0m
            for (const auto& entity : entities) {
                float dist = glm::distance(entity.pos, playerSpawnPos);
                ASSERT_GE(dist, 28.0f);
            }
        }
    }

    std::cout << "[PASS] SpawnSafetyTest.NoFaunaWithinSafeRadiusOnWorldLoad (verified 20 worlds across 10 seeds)" << std::endl;
}

// ── Test 2: Enemies Initialize Dormant ──
TEST(SpawnSafetyTest, EnemiesInitializeDormant) {
    std::cout << "[RUN] SpawnSafetyTest.EnemiesInitializeDormant" << std::endl;

    uint32_t seed = 4242;
    World world(seed);
    world.generate_world(1, seed);

    const auto& entities = world.GetActiveEntities();
    ASSERT_FALSE(entities.empty());

    // 1. Verify all pre-placed entities initialize with state == AIState::ROOSTING
    for (const auto& entity : entities) {
        ASSERT_TRUE(entity.state == AIState::ROOSTING);
    }

    // 2. Instantiate VoidStalkerManager and populate with roosting entities
    VoidStalkerManager stalker_mgr;
    stalker_mgr.reset();
    for (const auto& entity : entities) {
        stalker_mgr.spawn_roosting(entity.pos, entity.role);
    }

    ASSERT_EQ(stalker_mgr.active_count(), static_cast<int>(entities.size()));
    for (const auto& s : stalker_mgr.stalkers()) {
        ASSERT_TRUE(s.state == AIState::ROOSTING);
    }

    // 3. Advance simulation ticks by 5.0s with player idle
    glm::vec3 player_pos = world.GetPlayerSpawnPos();
    glm::vec3 player_fwd(0.0f, 0.0f, 1.0f);
    glm::vec3 headlamp_dir(0.0f, 0.0f, 1.0f);
    bool headlamp_on = false;
    float noise_level = 0.0f; // Idle player produces 0 noise
    bool player_is_drilling = false;

    float dt = 0.1f;
    for (float t = 0.0f; t < 5.0f; t += dt) {
        stalker_mgr.update(
            dt, player_pos, player_fwd, headlamp_dir,
            headlamp_on, noise_level, player_is_drilling,
            world, {}, false
        );
    }

    // Assert all entities remain in AIState::ROOSTING
    for (const auto& s : stalker_mgr.stalkers()) {
        ASSERT_TRUE(s.state == AIState::ROOSTING);
        // Roosting entities remain stationary
        ASSERT_EQ(s.velocity.x, 0.0f);
        ASSERT_EQ(s.velocity.y, 0.0f);
        ASSERT_EQ(s.velocity.z, 0.0f);
    }

    std::cout << "[PASS] SpawnSafetyTest.EnemiesInitializeDormant (entities remained in AIState::ROOSTING after 5.0s idle)" << std::endl;
}

// ── Test 3: Vault Door Resists Drill Mining ──
TEST(VaultTest, VaultDoorResistsDrillMining) {
    std::cout << "[RUN] VaultTest.VaultDoorResistsDrillMining" << std::endl;

    glm::ivec3 door_pos(40, 13, 44);
    VaultDoor vault_door(door_pos, 3);
    vault_door.set_relic_position(glm::ivec3(43, 13, 44));

    ASSERT_FALSE(vault_door.is_breached());
    ASSERT_FALSE(vault_door.is_relic_interactable());
    ASSERT_EQ(vault_door.voxel_health(), 1.0f); // 100%

    // 1. Apply mining drill damage to MAT_VAULT_DOOR; assert voxel health remains 100%
    bool drill_result = vault_door.apply_drill_damage(50.0f);
    ASSERT_FALSE(drill_result);
    ASSERT_EQ(vault_door.voxel_health(), 1.0f); // Resisted: remains 100%
    ASSERT_FALSE(vault_door.is_breached());

    // Drill damage cannot breach or make relic interactable
    ASSERT_FALSE(vault_door.is_relic_interactable());

    // 2. Simulate adjacent satchel detonation
    World world(777);
    world.generate_world(2, 777);

    // Set up door voxels in world
    for (int dy = 0; dy < 3; ++dy) {
        world.set_voxel(door_pos.x, door_pos.y + dy, door_pos.z, Voxel{MAT_VAULT_DOOR, VOXEL_FLAG_ANCHORED}, false);
    }

    glm::vec3 satchel_pos = glm::vec3(door_pos) + glm::vec3(1.0f, 1.0f, 0.0f);
    float blast_radius = 4.5f;

    bool breach_result = vault_door.apply_satchel_blast(satchel_pos, blast_radius, world);
    ASSERT_TRUE(breach_result);
    ASSERT_TRUE(vault_door.is_breached());

    // Assert door converts to air
    for (int dy = 0; dy < 3; ++dy) {
        Voxel v = world.get_voxel(door_pos.x, door_pos.y + dy, door_pos.z);
        ASSERT_EQ(v.material_id, MAT_AIR);
    }

    // Assert relic becomes interactable
    ASSERT_TRUE(vault_door.is_relic_interactable());

    // Interacting awards +350 EXP and +5 Titanium Cores
    int exp_reward = 0;
    int ti_reward = 0;
    bool interact_ok = vault_door.interact_relic(exp_reward, ti_reward);
    ASSERT_TRUE(interact_ok);
    ASSERT_EQ(exp_reward, 350);
    ASSERT_EQ(ti_reward, 5);
    ASSERT_TRUE(vault_door.is_relic_secured());

    std::cout << "[PASS] VaultTest.VaultDoorResistsDrillMining (drill resisted, satchel breached, relic secured)" << std::endl;

    // Test MissionSystem interactive relic pickup mechanics
    MissionSystem mission;
    mission.embed_precursor_vault(world, 2);
    glm::vec3 door_world = glm::vec3(mission.vault().door_pos) + glm::vec3(0.5f, 1.5f, 0.0f);
    mission.check_satchel_vault_breach(world, door_world, 4.0f);
    ASSERT_TRUE(mission.is_vault_breached());

    // 1. Standing far away (10m away): cannot interact
    glm::vec3 far_player = glm::vec3(mission.vault().relic_pos) + glm::vec3(10.0f, 0.0f, 0.0f);
    ASSERT_FALSE(mission.can_interact_relic(far_player));
    ASSERT_FALSE(mission.interact_relic(world, far_player));

    // 2. Approaching near relic does NOT auto-retrieve on walkover
    glm::vec3 near_player = glm::vec3(mission.vault().relic_pos) + glm::vec3(1.0f, 0.0f, 0.0f);
    mission.update(0.1f, world, near_player);
    ASSERT_FALSE(mission.is_relic_retrieved());

    // 3. Player presses interact [E] in range -> successfully retrieves relic
    ASSERT_TRUE(mission.can_interact_relic(near_player));
    bool m_retrieved = mission.interact_relic(world, near_player);
    ASSERT_TRUE(m_retrieved);
    ASSERT_TRUE(mission.is_relic_retrieved());
    uint8_t relic_mat = world.get_voxel(mission.vault().relic_pos.x, mission.vault().relic_pos.y, mission.vault().relic_pos.z).material_id;
    ASSERT_EQ(relic_mat, MAT_AIR);
    std::cout << "[PASS] VaultTest.MissionSystemInteractiveRelicPickup (out-of-range rejected, no auto-pickup, [E] interaction secured)" << std::endl;
}

// ── Test 4: Spawn Camera Yaw Alignment Towards Open Corridor / Grotto ──
TEST(SpawnSafetyTest, SpawnCameraYawAlignsToOpenCorridor) {
    std::cout << "[RUN] SpawnSafetyTest.SpawnCameraYawAlignsToOpenCorridor" << std::endl;

    const uint32_t test_seeds[5] = { 101, 505, 1337, 4242, 9999 };
    for (uint32_t seed : test_seeds) {
        World world(seed);
        world.generate_world(1, seed);

        const glm::vec3& spawn_pos = world.GetPlayerSpawnPos();
        float spawn_yaw = world.GetPlayerSpawnYaw();

        // Calculate clearance along the spawn yaw direction
        float rad = glm::radians(spawn_yaw);
        glm::vec3 front(std::cos(rad), 0.0f, std::sin(rad));
        glm::vec3 eye = spawn_pos + glm::vec3(0.0f, 1.6f, 0.0f);
        RaycastHit hit = world.raycast(eye, front, 48.0f);
        float clearance = hit.hit ? hit.distance : 48.0f;

        // Player must NOT spawn 1 meter away facing a solid rock wall
        ASSERT_GE(clearance, 4.0f);

        // PlayerController align_spawn_yaw verification
        PlayerController player(spawn_pos);
        player.align_spawn_yaw(world);
        ASSERT_EQ(player.yaw(), spawn_yaw);

        // If player is placed right against a wall facing it, clamp_to_surface must realign to open space
        player.set_look_angles(-90.0f, 0.0f); // Arbitrary initial angle
        player.clamp_to_surface(world);
        RaycastHit facing_hit = world.raycast(player.eye_position(), player.forward(), 1.5f);
        ASSERT_FALSE(facing_hit.hit); // Must not be facing a wall < 1.5m
    }

    std::cout << "[PASS] SpawnSafetyTest.SpawnCameraYawAlignsToOpenCorridor (open grotto clearance verified across 5 seeds)" << std::endl;
}

// ── Test 5: HUD 0.6s Spawn Fade-From-Black Overlay ──
TEST(SpawnSafetyTest, SpawnFadeFromBlackCountdown) {
    std::cout << "[RUN] SpawnSafetyTest.SpawnFadeFromBlackCountdown" << std::endl;

    HUD hud(1600, 900, true);
    ASSERT_FALSE(hud.is_spawn_fade_active());
    ASSERT_EQ(hud.spawn_fade_timer(), 0.0f);

    hud.trigger_spawn_fade(0.6f);
    ASSERT_TRUE(hud.is_spawn_fade_active());
    ASSERT_EQ(hud.spawn_fade_timer(), 0.6f);

    // Halfway through (0.3s)
    hud.update(0.3f);
    ASSERT_TRUE(hud.is_spawn_fade_active());
    ASSERT_TRUE(hud.spawn_fade_timer() > 0.25f && hud.spawn_fade_timer() < 0.35f);

    // Expiration after remaining 0.35s
    hud.update(0.35f);
    ASSERT_FALSE(hud.is_spawn_fade_active());
    ASSERT_EQ(hud.spawn_fade_timer(), 0.0f);

    std::cout << "[PASS] SpawnSafetyTest.SpawnFadeFromBlackCountdown (0.6s smooth countdown and active flag verified)" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " RUNNING SPAWN SAFETY & VAULT REGRESSION TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    SpawnSafetyTest_NoFaunaWithinSafeRadiusOnWorldLoad();
    SpawnSafetyTest_EnemiesInitializeDormant();
    VaultTest_VaultDoorResistsDrillMining();
    SpawnSafetyTest_SpawnCameraYawAlignsToOpenCorridor();
    SpawnSafetyTest_SpawnFadeFromBlackCountdown();

    std::cout << "========================================" << std::endl;
    std::cout << " ALL SPAWN SAFETY & VAULT TESTS PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
