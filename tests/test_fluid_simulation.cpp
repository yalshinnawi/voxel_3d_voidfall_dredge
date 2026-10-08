#include "../src/player/controller.hpp"
#include "../src/voxel/chunk.hpp"
#include "../src/voxel/fluid_sim.hpp"
#include "../src/voxel/greedy_mesher.hpp"
#include "../src/voxel/packed_vertex.hpp"
#include "../src/voxel/voxel_types.hpp"
#include "../src/voxel/world.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vector>

using namespace Voidfall;

#define ASSERT_TRUE(expr)                                                      \
  if (!(expr)) {                                                               \
    std::cerr << "[TEST FAILED] Assertion failed: " #expr " at " << __FILE__   \
              << ":" << __LINE__ << std::endl;                                 \
    std::exit(1);                                                              \
  }

#define ASSERT_FALSE(expr)                                                     \
  if (expr) {                                                                  \
    std::cerr << "[TEST FAILED] Assertion failed (expected false): " #expr     \
                 " at "                                                        \
              << __FILE__ << ":" << __LINE__ << std::endl;                     \
    std::exit(1);                                                              \
  }

#define ASSERT_EQ(a, b)                                                        \
  if (!((a) == (b))) {                                                         \
    std::cerr << "[TEST FAILED] " #a " == " #b " failed (" << (a)              \
              << " != " << (b) << ") at " << __FILE__ << ":" << __LINE__       \
              << std::endl;                                                    \
    std::exit(1);                                                              \
  }

#define ASSERT_GE(a, b)                                                        \
  if (!((a) >= (b))) {                                                         \
    std::cerr << "[TEST FAILED] " #a " (" << (a) << ") >= " #b " (" << (b)     \
              << ") failed at " << __FILE__ << ":" << __LINE__ << std::endl;   \
    std::exit(1);                                                              \
  }

#define ASSERT_NEAR(a, b, eps)                                                 \
  if (std::abs((a) - (b)) > (eps)) {                                           \
    std::cerr << "[TEST FAILED] |" #a " - " #b "| <= " #eps " failed (" << (a) \
              << " vs " << (b) << ") at " << __FILE__ << ":" << __LINE__       \
              << std::endl;                                                    \
    std::exit(1);                                                              \
  }

// ─────────────────────────────────────────────────────────────
// 1. LIQUID METADATA & WATERLOGGING FLAGS
// ─────────────────────────────────────────────────────────────
void test_liquid_metadata_and_flags() {
  std::cout << "[Test 1] Liquid Metadata & Waterlogging Flags..." << std::endl;

  // Verify flag allocation: bit 2
  ASSERT_EQ(VOXEL_FLAG_WATERLOGGED, 0x04);
  ASSERT_EQ(VOXEL_FLUID_LEVEL_MASK, 0x07);

  // Verify pure liquid materials
  ASSERT_TRUE(IsLiquid(MAT_WATER));
  ASSERT_TRUE(IsLiquid(MAT_ACID));
  ASSERT_TRUE(IsLiquid(MAT_COOLANT));
  ASSERT_FALSE(IsLiquid(MAT_FRACTURED_GRANITE));
  ASSERT_FALSE(IsLiquid(MAT_AIR));

  // Verify Chunk helper methods
  Chunk chunk(ChunkPos{0, 0, 0});
  ASSERT_TRUE(chunk.IsLiquid(MAT_WATER));
  ASSERT_TRUE(chunk.IsLiquid(MAT_ACID));
  ASSERT_TRUE(chunk.IsLiquid(MAT_COOLANT));
  ASSERT_FALSE(chunk.IsLiquid(MAT_VOLCANIC_BASALT));

  // Sub-block ramp with waterlogged state
  chunk.set_voxel(5, 5, 5, Voxel{MAT_FRACTURED_GRANITE, 0});
  chunk.SetShape(5, 5, 5, SHAPE_RAMP_EAST);
  ASSERT_FALSE(chunk.IsWaterlogged(5, 5, 5));

  chunk.SetWaterlogged(5, 5, 5, true);
  ASSERT_TRUE(chunk.IsWaterlogged(5, 5, 5));
  ASSERT_EQ(chunk.GetShape(5, 5, 5), SHAPE_RAMP_EAST);

  chunk.SetWaterlogged(5, 5, 5, false);
  ASSERT_FALSE(chunk.IsWaterlogged(5, 5, 5));
  ASSERT_EQ(chunk.GetShape(5, 5, 5), SHAPE_RAMP_EAST);

  // Voxel struct fluid level helpers
  Voxel liquid_vox{MAT_WATER, 0};
  liquid_vox.set_fluid_level(5);
  ASSERT_EQ(liquid_vox.fluid_level(), 5);
  liquid_vox.set_fluid_level(2);
  ASSERT_EQ(liquid_vox.fluid_level(), 2);

  std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 2. EVENT-DRIVEN SIMULATION & GRAVITY DOWNWARD FLOW
// ─────────────────────────────────────────────────────────────
void test_gravity_downward_flow() {
  std::cout << "[Test 2] Cellular Automaton Gravity Downward Flow..."
            << std::endl;

  World world(1337, false); // Headless non-threaded world

  // Carve a vertical column of air from y = 10 to y = 15 at (16, y, 16)
  for (int y = 10; y <= 15; ++y) {
    world.SetBlock(16, y, 16, MAT_AIR, 0);
  }
  // Solid floor at y = 9
  world.SetBlock(16, 9, 16, MAT_DREDGE_BEDROCK, 0);

  // Undisturbed state before adding liquid: queue must be empty
  ASSERT_EQ(world.ActiveFluidCount(), 0);

  // Place a water source at (16, 15, 16) with level 5
  world.set_fluid_cell(glm::ivec3(16, 15, 16), MAT_WATER, 5, false);
  world.WakeFluid(glm::ivec3(16, 15, 16));
  ASSERT_GE(world.ActiveFluidCount(), 1);

  // Tick the fluid solver for 5 steps (fixed 12 Hz ticks)
  for (int step = 0; step < 6; ++step) {
    world.update_fluids(0.085f);
  }

  // Water must have flowed straight down to the floor at y = 10
  Voxel floor_liquid = world.get_voxel(16, 10, 16);
  ASSERT_TRUE(IsLiquid(floor_liquid.material_id));
  ASSERT_EQ(floor_liquid.fluid_level(), 5);

  // Cell at y = 11, 12, 13, 14, 15 should all be water
  for (int y = 10; y <= 15; ++y) {
    ASSERT_TRUE(IsLiquid(world.get_voxel(16, y, 16).material_id));
  }

  std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 3. LATERAL SPREADING ACROSS FLAT CAVERN FLOOR
// ─────────────────────────────────────────────────────────────
void test_lateral_spreading() {
  std::cout << "[Test 3] Lateral Fluid Spreading (Level Cascade L - 1)..."
            << std::endl;

  World world(1337, false);

  // Flat floor at y = 10 surrounded by air at y = 11
  for (int x = 14; x <= 18; ++x) {
    for (int z = 14; z <= 18; ++z) {
      world.SetBlock(x, 10, z, MAT_DREDGE_BEDROCK, 0);
      world.SetBlock(x, 11, z, MAT_AIR, 0);
    }
  }

  // Place a source block with level 3 at (16, 11, 16)
  world.set_fluid_cell(glm::ivec3(16, 11, 16), MAT_WATER, 3, false);
  world.WakeFluid(glm::ivec3(16, 11, 16));

  // Tick fluid simulation
  world.update_fluids(0.085f);

  // Horizontal neighbors (17, 11, 16), (15, 11, 16), etc. must have received
  // water with level 2 (3 - 1)
  Voxel east = world.get_voxel(17, 11, 16);
  Voxel west = world.get_voxel(15, 11, 16);
  Voxel south = world.get_voxel(16, 11, 17);
  Voxel north = world.get_voxel(16, 11, 15);

  ASSERT_TRUE(IsLiquid(east.material_id));
  ASSERT_EQ(east.fluid_level(), 2);
  ASSERT_TRUE(IsLiquid(west.material_id));
  ASSERT_EQ(west.fluid_level(), 2);
  ASSERT_TRUE(IsLiquid(south.material_id));
  ASSERT_EQ(south.fluid_level(), 2);
  ASSERT_TRUE(IsLiquid(north.material_id));
  ASSERT_EQ(north.fluid_level(), 2);

  // Next tick: spreads to level 1
  world.update_fluids(0.085f);
  Voxel east2 = world.get_voxel(18, 11, 16);
  ASSERT_TRUE(IsLiquid(east2.material_id));
  ASSERT_EQ(east2.fluid_level(), 1);

  // Level 1 does NOT spread laterally further (terminal trickle)
  world.update_fluids(0.085f);
  Voxel east3 = world.get_voxel(19, 11, 16);
  ASSERT_FALSE(IsLiquid(east3.material_id));

  std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 4. SUB-BLOCK WATERLOGGING & SLOPE DOWNHILL CONFORMANCE
// ─────────────────────────────────────────────────────────────
void test_sub_block_waterlogging_and_slope_conformance() {
  std::cout << "[Test 4] Sub-Block Waterlogging & Slope Conformance..."
            << std::endl;

  World world(1337, false);

  // Setup an east ramp at (16, 10, 16) (slopes down toward -X, low at X=15,
  // high at X=16)
  world.SetBlock(16, 10, 16, MAT_FRACTURED_GRANITE, 0);
  world.set_block_with_flags(glm::ivec3(16, 10, 16), MAT_FRACTURED_GRANITE,
                             static_cast<uint8_t>(SHAPE_RAMP_EAST));
  world.SetBlock(16, 11, 16, MAT_AIR, 0);
  world.SetBlock(15, 10, 16, MAT_AIR, 0); // Open space downhill

  // Setup a bottom slab at (17, 10, 16)
  world.SetBlock(17, 10, 16, MAT_FRACTURED_GRANITE, 0);
  world.set_block_with_flags(glm::ivec3(17, 10, 16), MAT_FRACTURED_GRANITE,
                             static_cast<uint8_t>(SHAPE_SLAB_BOTTOM));

  // Place water on top of the ramp at (16, 11, 16)
  world.set_fluid_cell(glm::ivec3(16, 11, 16), MAT_WATER, 5, false);
  world.WakeFluid(glm::ivec3(16, 11, 16));

  // Tick simulation
  world.update_fluids(0.085f);

  // Ramp block below (16, 10, 16) must become WATERLOGGED!
  Voxel ramp_vox = world.get_voxel(16, 10, 16);
  ASSERT_TRUE(ramp_vox.is_waterlogged());
  ASSERT_EQ(ramp_vox.shape(), SHAPE_RAMP_EAST);
  ASSERT_EQ(ramp_vox.material_id, MAT_FRACTURED_GRANITE); // Retains rock base

  // Next tick: water conforms downhill to the west neighbor at (15, 10, 16)
  world.update_fluids(0.085f);
  Voxel downhill_vox = world.get_voxel(15, 10, 16);
  ASSERT_TRUE(IsLiquid(downhill_vox.material_id));

  // Place water adjacent to the bottom slab at (17, 10, 16)
  world.set_fluid_cell(glm::ivec3(18, 10, 16), MAT_WATER, 3, false);
  world.WakeFluid(glm::ivec3(18, 10, 16));
  world.update_fluids(0.085f);

  // Slab at (17, 10, 16) must become WATERLOGGED!
  Voxel slab_vox = world.get_voxel(17, 10, 16);
  ASSERT_TRUE(slab_vox.is_waterlogged());
  ASSERT_EQ(slab_vox.shape(), SHAPE_SLAB_BOTTOM);

  std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 5. EVENT-DRIVEN RESERVOIR BREACH & WAKE-UP HOOKS
// ─────────────────────────────────────────────────────────────
void test_reservoir_breach_and_zero_cpu_sleep() {
  std::cout << "[Test 5] Event-Driven Reservoir Breach & 0% CPU Sleep..."
            << std::endl;

  World world(1337, false);

  // Create a closed water pool: water at (16, 10, 16), surrounded by rock walls
  world.SetBlock(16, 9, 16, MAT_FRACTURED_GRANITE, 0);  // Floor
  world.SetBlock(17, 10, 16, MAT_FRACTURED_GRANITE, 0); // East wall
  world.SetBlock(15, 10, 16, MAT_FRACTURED_GRANITE, 0); // West wall
  world.SetBlock(16, 10, 17, MAT_FRACTURED_GRANITE, 0); // South wall
  world.SetBlock(16, 10, 15, MAT_FRACTURED_GRANITE, 0); // North wall
  world.SetBlock(16, 11, 16, MAT_FRACTURED_GRANITE, 0); // Ceiling

  // Pure liquid block inside
  world.set_fluid_cell(glm::ivec3(16, 10, 16), MAT_WATER, 5, false);

  // Drain the active queue completely to simulate undisturbed sleep
  world.m_activeFluids.clear();
  world.m_activeFluidSet.clear();
  ASSERT_EQ(world.ActiveFluidCount(), 0);

  // Calling update_fluids on undisturbed world should do 0 steps
  world.update_fluids(0.085f);
  ASSERT_EQ(world.ActiveFluidCount(), 0);

  // Excavate/drill the east rock wall at (17, 10, 16) to MAT_AIR
  world.DestroyBlock(glm::ivec3(17, 10, 16));

  // Breaking the adjacent wall MUST wake the fluid!
  ASSERT_GE(world.ActiveFluidCount(), 1);

  // Run ticks: water must naturally flow out into the breached cavity at (17, 10, 16)
  world.update_fluids(0.085f);
  Voxel breach_vox = world.get_voxel(17, 10, 16);
  ASSERT_TRUE(IsLiquid(breach_vox.material_id));

  // Drain and test SetBlock(pos, MAT_AIR) also wakes adjacent fluid
  world.m_activeFluids.clear();
  world.m_activeFluidSet.clear();
  ASSERT_EQ(world.ActiveFluidCount(), 0);

  world.SetBlock(glm::ivec3(15, 10, 16), MAT_AIR);
  ASSERT_GE(world.ActiveFluidCount(), 1);

  world.update_fluids(0.085f);
  Voxel west_breach = world.get_voxel(15, 10, 16);
  ASSERT_TRUE(IsLiquid(west_breach.material_id));

  std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 6. GREEDY MESHER SLOPE CONFORMANCE & VERTEX OFFSETS
// ─────────────────────────────────────────────────────────────
void test_greedy_mesher_liquid_conformance() {
  std::cout << "[Test 6] Greedy Mesher Waterlogged Slope Conformance..."
            << std::endl;

  Chunk chunk(ChunkPos{0, 0, 0});

  // Waterlogged ramp at (2, 2, 2)
  chunk.set_voxel(2, 2, 2, Voxel{MAT_FRACTURED_GRANITE, 0});
  chunk.SetShape(2, 2, 2, SHAPE_RAMP_EAST);
  chunk.SetWaterlogged(2, 2, 2, true);

  // Waterlogged bottom slab at (4, 2, 4)
  chunk.set_voxel(4, 2, 4, Voxel{MAT_FRACTURED_GRANITE, 0});
  chunk.SetShape(4, 2, 4, SHAPE_SLAB_BOTTOM);
  chunk.SetWaterlogged(4, 2, 4, true);

  // Pure water block at (6, 2, 6)
  chunk.set_voxel(6, 2, 6, Voxel{MAT_WATER, 5});

  auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
  ASSERT_FALSE(mesh.empty());

  // Search for the waterlogged ramp liquid diagonal quad:
  // It must have tex_layer = MAT_WATER (11), normal_idx = 2, and bit 27 in
  // data1 set (+0.04m offset)
  bool found_ramp_liquid_plane = false;
  bool found_slab_liquid_plane = false;

  for (const auto &v : mesh) {
    uint32_t layer = (v.data0 >> 23u) & 0xFFu;
    uint32_t norm = (v.data0 >> 18u) & 0x7u;
    uint32_t water_offset = (v.data1 >> 27u) & 0x1u;
    uint32_t sub_y_half = (v.data1 >> 26u) & 0x1u;

    if (layer == MAT_WATER && norm == 2 && water_offset == 1) {
      found_ramp_liquid_plane = true;
      // Verify position() factors in +0.04m offset
      glm::vec3 pos = v.position();
      ASSERT_NEAR(pos.y - std::floor(pos.y), 0.04f, 0.005f);
    }

    if (layer == MAT_WATER && norm == 2 && sub_y_half == 1 &&
        water_offset == 0) {
      found_slab_liquid_plane = true;
      // Slab surface sits at Y = 2.5m
      glm::vec3 pos = v.position();
      ASSERT_NEAR(pos.y, 2.5f, 0.005f);
    }
  }

  ASSERT_TRUE(found_ramp_liquid_plane);
  ASSERT_TRUE(found_slab_liquid_plane);

  // Verify greedy rectangular strip merging for liquid pools:
  Chunk pool_chunk(ChunkPos{0, 0, 0});
  for (int x = 2; x <= 4; ++x) {
    for (int z = 2; z <= 4; ++z) {
      pool_chunk.set_voxel(x, 5, z, Voxel{MAT_WATER, 5});
    }
  }
  auto pool_mesh = GreedyMesher::generate_mesh(pool_chunk, nullptr);
  int top_quad_count = 0;
  int merged_3x3_quads = 0;
  for (const auto &v : pool_mesh) {
    uint32_t layer = (v.data0 >> 23u) & 0xFFu;
    uint32_t norm = (v.data0 >> 18u) & 0x7u;
    uint32_t u_dim = v.data1 & 0x3Fu;
    uint32_t v_dim = (v.data1 >> 6u) & 0x3Fu;
    uint32_t corner_idx = (v.data1 >> 12u) & 0x3u;
    if (layer == MAT_WATER && norm == 2) {
      if (corner_idx == 1) {
        top_quad_count++;
        if (u_dim == 3 && v_dim == 3) {
          merged_3x3_quads++;
        }
      }
    }
  }
  ASSERT_EQ(top_quad_count, 1);
  ASSERT_EQ(merged_3x3_quads, 1);

  // Verify pure liquid block adjacent to SHAPE_RAMP_* emits vertical side quad sealing against ramp:
  Chunk ramp_seal_chunk(ChunkPos{0, 0, 0});
  ramp_seal_chunk.set_voxel(5, 5, 5, Voxel{MAT_WATER, 5});
  ramp_seal_chunk.set_voxel(6, 5, 5, Voxel{MAT_FRACTURED_GRANITE, 0});
  ramp_seal_chunk.SetShape(6, 5, 5, SHAPE_RAMP_EAST);
  auto seal_mesh = GreedyMesher::generate_mesh(ramp_seal_chunk, nullptr);
  bool found_sealing_quad = false;
  for (const auto &v : seal_mesh) {
    uint32_t layer = (v.data0 >> 23u) & 0xFFu;
    uint32_t norm = (v.data0 >> 18u) & 0x7u;
    // Side face +X (norm = 0) facing ramp at (6, 5, 5)
    if (layer == MAT_WATER && norm == 0) {
      found_sealing_quad = true;
      break;
    }
  }
  ASSERT_TRUE(found_sealing_quad);

  std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 7. PLAYER WATERLOGGED WADING & FLUID IMMERSION
// ─────────────────────────────────────────────────────────────
void test_player_waterlogged_immersion() {
  std::cout << "[Test 7] Player Submersion on Waterlogged Slabs & Ramps..."
            << std::endl;

  World world(1337, false);

  // Build floor of waterlogged bottom slabs at y = 10
  for (int x = 14; x <= 18; ++x) {
    for (int z = 14; z <= 18; ++z) {
      world.SetBlock(x, 10, z, MAT_FRACTURED_GRANITE, 0);
      world.set_block_with_flags(glm::ivec3(x, 10, z), MAT_FRACTURED_GRANITE,
                                 static_cast<uint8_t>(SHAPE_SLAB_BOTTOM));
      world.set_waterlogged_cell(glm::ivec3(x, 10, z), true);
    }
  }

  // Place player standing on the waterlogged slab floor
  PlayerController player(glm::vec3(16.0f, 10.6f, 16.0f));

  // Update player physics in the world
  player.update_physics(0.016f, world);

  // Player must detect that they are in liquid (wading in waterlogged slab)
  ASSERT_TRUE(player.is_in_liquid());
  ASSERT_EQ(player.current_liquid_material(), MAT_CRYSTAL_AQUIFER);
  ASSERT_GE(player.liquid_submersion(), 0.0f);

  std::cout << "  -> PASSED" << std::endl;
}

int main() {
  std::cout << "=========================================================="
            << std::endl;
  std::cout << "  VOIDFALL: DREDGE -- CELLULAR AUTOMATON LIQUID SUITE     "
            << std::endl;
  std::cout << "=========================================================="
            << std::endl;

  test_liquid_metadata_and_flags();
  test_gravity_downward_flow();
  test_lateral_spreading();
  test_sub_block_waterlogging_and_slope_conformance();
  test_reservoir_breach_and_zero_cpu_sleep();
  test_greedy_mesher_liquid_conformance();
  test_player_waterlogged_immersion();

  std::cout << "=========================================================="
            << std::endl;
  std::cout << "  ALL 7 FLUID SIMULATION TEST MODULES PASSED (0 ERRORS)   "
            << std::endl;
  std::cout << "=========================================================="
            << std::endl;
  return 0;
}
