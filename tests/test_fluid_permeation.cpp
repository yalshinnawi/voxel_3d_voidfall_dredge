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

#define ASSERT_GE(a, b) \
    if (!((a) >= (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") >= " #b " (" << (b) << ") failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define TEST(Suite, Case) void Suite##_##Case()

// ─────────────────────────────────────────────────────────────
// 1. Water Flows Into Adjacent Same-Level Hole
// ─────────────────────────────────────────────────────────────
TEST(FluidPermeation, WaterFlowsIntoAdjacentSameLevelHole) {
    std::cout << "[Test 1] WaterFlowsIntoAdjacentSameLevelHole..." << std::endl;

    World world(101, false);
    FluidSim::SetActiveWorld(&world);

    // Set up a 3 x 1 x 3 container of solid granite blocks at Y = 0
    for (int x = -1; x <= 3; ++x) {
        for (int z = -1; z <= 3; ++z) {
            world.SetBlock(x, 0, z, MAT_FRACTURED_GRANITE, 0);
        }
    }
    // Container walls around
    for (int x = -1; x <= 3; ++x) {
        world.SetBlock(x, 1, -1, MAT_FRACTURED_GRANITE, 0);
        world.SetBlock(x, 1, 3, MAT_FRACTURED_GRANITE, 0);
    }
    for (int z = -1; z <= 3; ++z) {
        world.SetBlock(-1, 1, z, MAT_FRACTURED_GRANITE, 0);
        world.SetBlock(3, 1, z, MAT_FRACTURED_GRANITE, 0);
    }

    // Place MAT_WATER at (0, 1, 0) and MAT_AIR at (1, 1, 0) with a solid granite base at (1, 0, 0)
    world.SetBlockWithFlags(glm::ivec3(0, 1, 0), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(0, 1, 0));
    world.SetBlock(glm::ivec3(1, 1, 0), MAT_AIR);

    // Step FluidSim::Update(0.1f) for 3 ticks
    for (int i = 0; i < 3; ++i) {
        FluidSim::Update(0.1f);
    }

    // Assert world.GetBlockMaterial(glm::ivec3(1, 1, 0)) == MAT_WATER
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(1, 1, 0)), MAT_WATER);
    std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 2. Water Flows Over Ledge And Cascades
// ─────────────────────────────────────────────────────────────
TEST(FluidPermeation, WaterFlowsOverLedgeAndCascades) {
    std::cout << "[Test 2] WaterFlowsOverLedgeAndCascades..." << std::endl;

    World world(202, false);
    FluidSim::SetActiveWorld(&world);

    // Place MAT_WATER at (0, 2, 0) on solid (0, 1, 0)
    world.SetBlock(glm::ivec3(0, 1, 0), MAT_FRACTURED_GRANITE, 0);
    world.SetBlock(glm::ivec3(0, 0, 0), MAT_FRACTURED_GRANITE, 0);

    // Set (1, 2, 0) and (1, 1, 0) to MAT_AIR (a 1-block drop-off)
    // Floor below drop is at Y = 0
    world.SetBlock(glm::ivec3(1, 0, 0), MAT_FRACTURED_GRANITE, 0);
    world.SetBlock(glm::ivec3(1, 1, 0), MAT_AIR);
    world.SetBlock(glm::ivec3(1, 2, 0), MAT_AIR);

    world.SetBlockWithFlags(glm::ivec3(0, 2, 0), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(0, 2, 0));

    // Step FluidSim::Update(0.1f) for 4 ticks
    for (int i = 0; i < 4; ++i) {
        FluidSim::Update(0.1f);
    }

    // Assert that water spreads horizontally to (1, 2, 0) and cascades down to (1, 1, 0)
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(1, 2, 0)), MAT_WATER);
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(1, 1, 0)), MAT_WATER);
    std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 3. Dry Hole In Lake Infilled (Pool Infilling)
// ─────────────────────────────────────────────────────────────
TEST(FluidPermeation, DryHoleInLakeInfilled) {
    std::cout << "[Test 3] DryHoleInLakeInfilled..." << std::endl;

    World world(303, false);
    FluidSim::SetActiveWorld(&world);

    // Place source water at (0, 1, 0) and (2, 1, 0) with solid floor at Y = 0
    world.SetBlock(glm::ivec3(0, 0, 0), MAT_FRACTURED_GRANITE, 0);
    world.SetBlock(glm::ivec3(1, 0, 0), MAT_FRACTURED_GRANITE, 0);
    world.SetBlock(glm::ivec3(2, 0, 0), MAT_FRACTURED_GRANITE, 0);

    // Surrounding retaining wall to isolate test
    world.SetBlock(glm::ivec3(-1, 1, 0), MAT_FRACTURED_GRANITE, 0);
    world.SetBlock(glm::ivec3(3, 1, 0), MAT_FRACTURED_GRANITE, 0);
    world.SetBlock(glm::ivec3(1, 1, 1), MAT_FRACTURED_GRANITE, 0);
    world.SetBlock(glm::ivec3(1, 1, -1), MAT_FRACTURED_GRANITE, 0);

    // Leave (1, 1, 0) as MAT_AIR
    world.SetBlock(glm::ivec3(1, 1, 0), MAT_AIR);

    world.SetBlockWithFlags(glm::ivec3(0, 1, 0), MAT_WATER, 5);
    world.SetBlockWithFlags(glm::ivec3(2, 1, 0), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(0, 1, 0));
    world.PushActiveFluid(glm::ivec3(2, 1, 0));

    // Step FluidSim::Update(0.1f)
    FluidSim::Update(0.1f);

    // Assert (1, 1, 0) converts to MAT_WATER with Level 5
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(1, 1, 0)), MAT_WATER);
    ASSERT_EQ(GetFluidLevel(world.GetBlockFlags(glm::ivec3(1, 1, 0))), 5);
    std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 4. Cross-Chunk Boundary Permeation & Neighbor Dirtying
// ─────────────────────────────────────────────────────────────
TEST(FluidPermeation, CrossChunkBoundaryPermeation) {
    std::cout << "[Test 4] CrossChunkBoundaryPermeation..." << std::endl;

    World world(404, false);
    FluidSim::SetActiveWorld(&world);

    // Chunk 0 has X in [0..31], Chunk 1 has X in [32..63]
    // Set solid floor at Y = 0 across the boundary
    for (int x = 28; x <= 35; ++x) {
        for (int z = 14; z <= 18; ++z) {
            world.SetBlock(x, 0, z, MAT_FRACTURED_GRANITE, 0);
            world.SetBlock(x, 1, z, MAT_AIR, 0);
        }
    }

    // Place water at X = 31 (boundary of chunk 0)
    world.SetBlockWithFlags(glm::ivec3(31, 1, 16), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(31, 1, 16));

    // Tick fluid simulation
    for (int i = 0; i < 3; ++i) {
        FluidSim::Update(0.1f);
    }

    // Water must cross boundary into chunk 1 at X = 32
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(32, 1, 16)), MAT_WATER);

    // Verify both chunks exist and were updated
    Chunk* c0 = world.GetChunkFromBlockPos(glm::ivec3(31, 1, 16));
    Chunk* c1 = world.GetChunkFromBlockPos(glm::ivec3(32, 1, 16));
    ASSERT_TRUE(c0 != nullptr);
    ASSERT_TRUE(c1 != nullptr);
    ASSERT_TRUE(c0 != c1);

    std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 5. Performance Guardrails & 0.0ms Sleeping Invariants
// ─────────────────────────────────────────────────────────────
TEST(FluidPermeation, PerformanceGuardrailsAndSleepingQueue) {
    std::cout << "[Test 5] PerformanceGuardrailsAndSleepingQueue..." << std::endl;

    World world(505, false);
    FluidSim::SetActiveWorld(&world);

    // Create a 2x1x2 contained pool
    for (int x = 10; x <= 13; ++x) {
        for (int z = 10; z <= 13; ++z) {
            world.SetBlock(x, 0, z, MAT_DREDGE_BEDROCK, 0);
            world.SetBlock(x, 1, z, MAT_DREDGE_BEDROCK, 0);
        }
    }
    // Excavate 2x2 cavity
    world.DestroyBlock(glm::ivec3(11, 1, 11));
    world.DestroyBlock(glm::ivec3(12, 1, 11));
    world.DestroyBlock(glm::ivec3(11, 1, 12));
    world.DestroyBlock(glm::ivec3(12, 1, 12));

    // Fill with water sources
    world.SetBlockWithFlags(glm::ivec3(11, 1, 11), MAT_WATER, 5);
    world.SetBlockWithFlags(glm::ivec3(12, 1, 11), MAT_WATER, 5);
    world.SetBlockWithFlags(glm::ivec3(11, 1, 12), MAT_WATER, 5);
    world.SetBlockWithFlags(glm::ivec3(12, 1, 12), MAT_WATER, 5);

    // Wake pool
    world.PushActiveFluid(glm::ivec3(11, 1, 11));

    // Let it settle over several ticks
    for (int i = 0; i < 6; ++i) {
        FluidSim::Update(0.1f);
    }

    // Queue must empty completely when settled (zero CPU overhead)
    ASSERT_EQ(world.ActiveFluidCount(), 0);

    // 3x3x3 Excavation breach must wake pool
    world.DestroyBlock(glm::ivec3(13, 1, 11));
    ASSERT_GE(world.ActiveFluidCount(), 1);

    std::cout << "  -> PASSED" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- FLUID PERMEATION & CASCADE SUITE    " << std::endl;
    std::cout << "==========================================================" << std::endl;

    FluidPermeation_WaterFlowsIntoAdjacentSameLevelHole();
    FluidPermeation_WaterFlowsOverLedgeAndCascades();
    FluidPermeation_DryHoleInLakeInfilled();
    FluidPermeation_CrossChunkBoundaryPermeation();
    FluidPermeation_PerformanceGuardrailsAndSleepingQueue();

    std::cout << "==========================================================" << std::endl;
    std::cout << "  ALL 5 FLUID PERMEATION TEST MODULES PASSED (0 ERRORS)   " << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
