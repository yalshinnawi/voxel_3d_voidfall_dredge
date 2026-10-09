#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <glm/glm.hpp>
#include "../src/voxel/voxel_types.hpp"
#include "../src/voxel/packed_vertex.hpp"
#include "../src/voxel/chunk.hpp"
#include "../src/voxel/greedy_mesher.hpp"
#include "../src/voxel/world.hpp"
#include "../src/voxel/fluid_sim.hpp"

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
        std::cerr << "[TEST FAILED] " #a " == " #b " failed (" << (a) << " != " << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_NE(a, b) \
    if ((a) == (b)) { \
        std::cerr << "[TEST FAILED] " #a " != " #b " failed (" << (a) << " == " << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_GE(a, b) \
    if (!((a) >= (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") >= " #b " (" << (b) << ") failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define TEST(Suite, Case) void Suite##_##Case()

// ─────────────────────────────────────────────────────────────
// 1. Bullet Hit Does Not Spawn Water
// ─────────────────────────────────────────────────────────────
TEST(FluidPhysics, BulletHitDoesNotSpawnWater) {
    std::cout << "[Test 1] FluidPhysics.BulletHitDoesNotSpawnWater..." << std::endl;

    World world(1001, false);

    // Place a solid granite block at (10, 5, 10)
    world.SetBlock(10, 5, 10, MAT_FRACTURED_GRANITE, 0);
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(10, 5, 10)), MAT_FRACTURED_GRANITE);

    // Part A: Simulate ballistic bullet non-fatal impact (damage = 6)
    Voxel v = world.get_voxel(10, 5, 10);
    uint8_t current_damage = v.flags_and_damage & 0x0F;
    uint8_t new_damage = current_damage + 6;
    if (new_damage >= 15) {
        world.SetBlock(10, 5, 10, MAT_AIR, 0);
    } else {
        v.flags_and_damage = (v.flags_and_damage & 0xF0) | (new_damage & 0x0F);
        world.set_voxel(10, 5, 10, v, true);
    }

    uint8_t matAfterHit1 = world.GetBlockMaterial(glm::ivec3(10, 5, 10));
    ASSERT_EQ(matAfterHit1, MAT_FRACTURED_GRANITE);
    ASSERT_NE(matAfterHit1, MAT_WATER);

    // Part B: Simulate second bullet bringing damage >= 15 (fracturing and shattering the block)
    new_damage += 10;
    if (new_damage >= 15) {
        world.SetBlock(10, 5, 10, MAT_AIR, 0);
    }
    uint8_t matAfterDestroy = world.GetBlockMaterial(glm::ivec3(10, 5, 10));
    ASSERT_EQ(matAfterDestroy, MAT_AIR);
    ASSERT_NE(matAfterDestroy, MAT_WATER);

    std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 2. Liquid Does Not Form Vertical Towers / Pyramids
// ─────────────────────────────────────────────────────────────
TEST(FluidPhysics, LiquidDoesNotFormVerticalTowers) {
    std::cout << "[Test 2] FluidPhysics.LiquidDoesNotFormVerticalTowers..." << std::endl;

    World world(1002, false);
    FluidSim::SetActiveWorld(&world);

    // Solid flat floor at Y = 0 and Y = 1
    for (int x = -5; x <= 5; ++x) {
        for (int z = -5; z <= 5; ++z) {
            world.SetBlock(x, 0, z, MAT_FRACTURED_GRANITE, 0);
            world.SetBlock(x, 1, z, MAT_FRACTURED_GRANITE, 0);
        }
    }

    // Place a single liquid source at (0, 2, 0)
    world.SetBlockWithFlags(glm::ivec3(0, 2, 0), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(0, 2, 0));

    // Step FluidSim::Update() for 10 ticks (each tick 0.1s exceeds TICK_INTERVAL of 0.0833s)
    for (int i = 0; i < 10; ++i) {
        FluidSim::Update(0.1f);
    }

    // Assert that NO cell at Y > 2 contains liquid (anti-stacking / no upward movement)
    for (int y = 3; y <= 8; ++y) {
        for (int x = -5; x <= 5; ++x) {
            for (int z = -5; z <= 5; ++z) {
                uint8_t mat = world.GetBlockMaterial(glm::ivec3(x, y, z));
                ASSERT_FALSE(IsLiquid(mat));
            }
        }
    }

    std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 3. Flowing Liquid Drains When Falling
// ─────────────────────────────────────────────────────────────
TEST(FluidPhysics, FlowingLiquidDrainsWhenFalling) {
    std::cout << "[Test 3] FluidPhysics.FlowingLiquidDrainsWhenFalling..." << std::endl;

    World world(1003, false);
    FluidSim::SetActiveWorld(&world);

    // Solid floor at Y = 0
    for (int x = -2; x <= 2; ++x) {
        for (int z = -2; z <= 2; ++z) {
            world.SetBlock(x, 0, z, MAT_FRACTURED_GRANITE, 0);
        }
    }
    // Air column at Y = 1..4
    for (int y = 1; y <= 4; ++y) {
        world.SetBlock(0, y, 0, MAT_AIR, 0);
    }

    // Place a non-source flowing water block at (0, 5, 0) with level 3
    world.SetBlockWithFlags(glm::ivec3(0, 5, 0), MAT_WATER, 3);
    world.PushActiveFluid(glm::ivec3(0, 5, 0));

    // Step FluidSim::Update() for 1 tick
    FluidSim::Update(0.1f);

    // Assert (0, 5, 0) converts back to MAT_AIR as the volume falls down to (0, 4, 0)
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(0, 5, 0)), MAT_AIR);
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(0, 4, 0)), MAT_WATER);

    std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 4. Recessed Liquid Meshing & Face Culling
// ─────────────────────────────────────────────────────────────
TEST(FluidPhysics, RecessedLiquidMeshingAndFaceCulling) {
    std::cout << "[Test 4] FluidPhysics.RecessedLiquidMeshingAndFaceCulling..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    // Two adjacent water cells at (5, 5, 5) and (6, 5, 5)
    chunk.set_voxel(5, 5, 5, Voxel{MAT_WATER, 5});
    chunk.set_voxel(6, 5, 5, Voxel{MAT_WATER, 5});

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    // 1. Verify top faces (norm == 2, +Y) are recessed to 0.88m (local base Y = 5 -> Y = 5.88m)
    bool found_recessed_top = false;
    for (const auto& v : mesh) {
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        if (norm == 2 && layer == MAT_WATER) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.y - 5.88f) < 0.05f) {
                found_recessed_top = true;
                break;
            }
        }
    }
    ASSERT_TRUE(found_recessed_top);

    // 2. Verify internal boundary face at X = 6.0 (between 5 and 6) is completely culled
    for (const auto& v : mesh) {
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        glm::vec3 pos = v.position();
        if (layer == MAT_WATER) {
            // norm 0 is +X, norm 1 is -X
            if ((norm == 0 || norm == 1) && std::abs(pos.x - 6.0f) < 0.001f && pos.y >= 5.0f && pos.y <= 6.0f && pos.z >= 5.0f && pos.z <= 6.0f) {
                // East/West boundary face between identical liquids should not exist
                ASSERT_TRUE(false);
            }
        }
    }

    std::cout << "  -> PASSED" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- FLUID PHYSICS & RENDERING SUITE     " << std::endl;
    std::cout << "==========================================================" << std::endl;

    FluidPhysics_BulletHitDoesNotSpawnWater();
    FluidPhysics_LiquidDoesNotFormVerticalTowers();
    FluidPhysics_FlowingLiquidDrainsWhenFalling();
    FluidPhysics_RecessedLiquidMeshingAndFaceCulling();

    std::cout << "==========================================================" << std::endl;
    std::cout << "  ALL 4 FLUID PHYSICS REGRESSION TESTS PASSED (0 ERRORS)  " << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
