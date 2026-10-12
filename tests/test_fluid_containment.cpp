#include "../src/voxel/chunk.hpp"
#include "../src/voxel/fluid_sim.hpp"
#include "../src/voxel/greedy_mesher.hpp"
#include "../src/voxel/packed_vertex.hpp"
#include "../src/voxel/voxel_types.hpp"
#include "../src/voxel/world.hpp"
#include "../src/voxel/level_shapes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unordered_set>

using namespace Voidfall;

#define ASSERT_TRUE(expr) \
  if (!(expr)) { \
    std::cerr << "[TEST FAILED] " #expr " at " << __FILE__ << ":" << __LINE__ \
              << std::endl; \
    std::exit(1); \
  }

#define ASSERT_FALSE(expr) \
  if (expr) { \
    std::cerr << "[TEST FAILED] Expected false: " #expr " at " << __FILE__ \
              << ":" << __LINE__ << std::endl; \
    std::exit(1); \
  }

#define ASSERT_EQ(a, b) \
  if (!((a) == (b))) { \
    std::cerr << "[TEST FAILED] " #a " == " #b " at " << __FILE__ << ":" << __LINE__ \
              << std::endl; \
    std::exit(1); \
  }

void test_watertight_containment_across_seeds() {
    std::cout << "[Test 1] WatertightContainmentAcrossSeeds (Zero Leaks & Sleep on Spawn)..." << std::endl;

    const glm::ivec3 lateralDirs[4] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    int total_liquid_blocks_checked = 0;
    int total_leaks = 0;
    int num_seeds = 15; // 5 seeds per sector across Sectors 1, 2, and 3

    for (int seed_idx = 0; seed_idx < num_seeds; ++seed_idx) {
        int sector = (seed_idx % 3) + 1;
        uint32_t seed = 77000 + seed_idx * 1337;

        World world(seed, false);
        world.generate_world(sector, seed);

        // Sleep on Spawn requirement: Spawn queues must be empty (0 ms CPU cost until disturbed)
        ASSERT_EQ(world.active_fluid_count(), 0);
        ASSERT_TRUE(world.m_fluidSim.m_activeFluids.empty());

        int seed_liquid_count = 0;

        for (const auto& [cpos, chunk] : world.chunks()) {
            if (!chunk || chunk->is_empty()) continue;
            int bx = cpos.x * CHUNK_SIZE;
            int by = cpos.y * CHUNK_SIZE;
            int bz = cpos.z * CHUNK_SIZE;

            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                        Voxel vox = chunk->get_voxel(lx, ly, lz);
                        if (!vox.is_liquid() && !vox.is_waterlogged()) continue;

                        seed_liquid_count++;
                        int wx = bx + lx;
                        int wy = by + ly;
                        int wz = bz + lz;

                        // 1. Vertical support check: Voxel below must be solid, liquid, or waterlogged
                        Voxel below = world.get_voxel(wx, wy - 1, wz);
                        if (below.material_id == MAT_AIR) {
                            total_leaks++;
                            std::cout << "  -> LEAK (Uncontained vertical fall) at seed " << seed 
                                      << " world (" << wx << "," << wy << "," << wz << ")\n";
                        }

                        // 2. Lateral containment check: No horizontal neighbor at same height can be MAT_AIR
                        for (const auto& d : lateralDirs) {
                            Voxel lat = world.get_voxel(wx + d.x, wy, wz + d.z);
                            if (lat.material_id == MAT_AIR) {
                                total_leaks++;
                                std::cout << "  -> LEAK (Uncontained lateral spill) at seed " << seed 
                                          << " world (" << wx << "," << wy << "," << wz << ") to dir ("
                                          << d.x << "," << d.y << "," << d.z << ")\n";
                            }
                        }

                        // 3. Watertight bounding wall check: Dry bounding walls must exceed liquid surface height
                        for (const auto& d : lateralDirs) {
                            Voxel lat = world.get_voxel(wx + d.x, wy, wz + d.z);
                            if (lat.is_solid() && !lat.is_waterlogged()) {
                                if (lat.shape() != SHAPE_CUBE) {
                                    bool slopes_away = false;
                                    if (d.x == 1 && lat.shape() == SHAPE_RAMP_EAST) slopes_away = true;
                                    if (d.x == -1 && lat.shape() == SHAPE_RAMP_WEST) slopes_away = true;
                                    if (d.z == 1 && lat.shape() == SHAPE_RAMP_SOUTH) slopes_away = true;
                                    if (d.z == -1 && lat.shape() == SHAPE_RAMP_NORTH) slopes_away = true;

                                    if (!slopes_away) {
                                        Voxel wall_above = world.get_voxel(wx + d.x, wy + 1, wz + d.z);
                                        if (wall_above.material_id == MAT_AIR) {
                                            total_leaks++;
                                            std::cout << "  -> LEAK (Bounding wall does not exceed liquid surface) at seed " << seed 
                                                      << " world (" << wx + d.x << "," << wy << "," << wz + d.z << ")\n";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        total_liquid_blocks_checked += seed_liquid_count;

        // Verify fluids remain completely dormant after offline generation
        world.update_fluids(0.0833f);
        ASSERT_EQ(world.active_fluid_count(), 0);
    }

    ASSERT_EQ(total_leaks, 0);
    std::cout << "  -> PASSED (" << num_seeds << " seeds checked, " << total_liquid_blocks_checked
              << " liquid blocks verified: 0 leaks, 0 uncontained fluids, 0 ms idle CPU)\n";
}

void test_random_level_fluids_generation_and_containment() {
    std::cout << "[Test 2] RandomLevelFluidsGenerationAndContainment..." << std::endl;

    int total_levels_tested = 12; // 4 levels per sector across Sectors 1, 2, 3
    int levels_with_fluids = 0;
    std::unordered_set<uint8_t> observed_liquid_types;

    for (int idx = 0; idx < total_levels_tested; ++idx) {
        int sector = (idx % 3) + 1;
        uint32_t seed = 88000 + idx * 777;

        World world(seed, false);
        world.generate_world(sector, seed);

        // Sleep on Spawn requirement: 0 ms CPU cost until disturbed, spawn queues empty
        ASSERT_EQ(world.active_fluid_count(), 0);
        ASSERT_TRUE(world.m_fluidSim.m_activeFluids.empty());

        int liquid_blocks = 0;
        for (const auto& [cpos, chunk] : world.chunks()) {
            if (!chunk || chunk->is_empty()) continue;
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                        Voxel vox = chunk->get_voxel(lx, ly, lz);
                        if (vox.is_liquid()) {
                            liquid_blocks++;
                            observed_liquid_types.insert(vox.material_id);
                        }
                    }
                }
            }
        }

        if (liquid_blocks > 0) {
            levels_with_fluids++;
        }

        // Verify that rooms with random fluids keep their center and doorway corridors completely free of liquid
        if (world.level_generator()) {
            for (const auto& room : world.level_generator()->rooms()) {
                if (room.has_random_fluid) {
                    // Center of room must not be liquid (thoroughfares clear!)
                    Voxel center_vox = world.get_voxel(room.center.x, room.floor_y, room.center.z);
                    ASSERT_FALSE(center_vox.is_liquid());

                    // Cardinal doorway corridors (|dx| <= 2 or |dz| <= 2) must remain walkable and dry
                    for (int d = -2; d <= 2; ++d) {
                        Voxel v_north = world.get_voxel(room.center.x + d, room.floor_y, room.center.z + room.half_depth - 1);
                        Voxel v_south = world.get_voxel(room.center.x + d, room.floor_y, room.center.z - room.half_depth + 1);
                        Voxel v_east  = world.get_voxel(room.center.x + room.half_width - 1, room.floor_y, room.center.z + d);
                        Voxel v_west  = world.get_voxel(room.center.x - room.half_width + 1, room.floor_y, room.center.z + d);
                        ASSERT_FALSE(v_north.is_liquid());
                        ASSERT_FALSE(v_south.is_liquid());
                        ASSERT_FALSE(v_east.is_liquid());
                        ASSERT_FALSE(v_west.is_liquid());
                    }
                }
            }
        }
    }

    // Every single generated level across all seeds must have procedurally generated fluids!
    ASSERT_EQ(levels_with_fluids, total_levels_tested);

    // Verify rich fluid diversity across levels (water, coolant, magma, acid)
    ASSERT_TRUE(observed_liquid_types.count(MAT_CRYSTAL_AQUIFER) > 0);
    ASSERT_TRUE(observed_liquid_types.count(MAT_COOLANT) > 0);
    ASSERT_TRUE(observed_liquid_types.count(MAT_THERMITE_SLAG) > 0 || observed_liquid_types.count(MAT_MOLTEN_MAGMA) > 0);
    ASSERT_TRUE(observed_liquid_types.count(MAT_ACID) > 0);

    std::cout << "  -> PASSED (" << total_levels_tested << " levels checked: 100% have procedural fluids, "
              << observed_liquid_types.size() << " distinct fluid types generated, 0 flooded doorways)\n";
}

int main() {
    std::cout << "=== Running Fluid Containment & Procedural Placement Tests ===" << std::endl;

    test_watertight_containment_across_seeds();
    test_random_level_fluids_generation_and_containment();

    std::cout << "=== All Fluid Containment Tests PASSED successfully ===" << std::endl;
    return 0;
}
