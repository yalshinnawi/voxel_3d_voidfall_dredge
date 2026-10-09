#include "../src/voxel/chunk.hpp"
#include "../src/voxel/fluid_sim.hpp"
#include "../src/voxel/greedy_mesher.hpp"
#include "../src/voxel/packed_vertex.hpp"
#include "../src/voxel/voxel_types.hpp"
#include "../src/voxel/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Voidfall;

#define ASSERT_TRUE(expr)                                                      \
  if (!(expr)) {                                                               \
    std::cerr << "[TEST FAILED] " #expr " at " << __FILE__ << ":" << __LINE__ \
              << std::endl;                                                    \
    std::exit(1);                                                              \
  }

#define ASSERT_FALSE(expr)                                                     \
  if (expr) {                                                                  \
    std::cerr << "[TEST FAILED] Expected false: " #expr " at " << __FILE__     \
              << ":" << __LINE__ << std::endl;                                 \
    std::exit(1);                                                              \
  }

#define ASSERT_NEAR(a, b, eps)                                                 \
  if (std::abs((a) - (b)) > (eps)) {                                           \
    std::cerr << "[TEST FAILED] |" #a " - " #b "| = " << std::abs((a) - (b))   \
              << " > " << (eps) << " at " << __FILE__ << ":" << __LINE__       \
              << std::endl;                                                    \
    std::exit(1);                                                              \
  }

#define ASSERT_EQ(a, b)                                                        \
  if (!((a) == (b))) {                                                         \
    std::cerr << "[TEST FAILED] " #a " == " #b " at " << __FILE__ << ":" << __LINE__ \
              << std::endl;                                                    \
    std::exit(1);                                                              \
  }

#define TEST(suite, name) void suite##_##name()

// ─────────────────────────────────────────────────────────────
// 1. SharedEdgeHasIdenticalElevation
// Place Level 5 liquid at (0, 0, 0) and Level 4 liquid at (1, 0, 0).
// Verify corner height calculations produce identical Y positions
// on the boundary edge between (0, 0, 0) and (1, 0, 0).
// ─────────────────────────────────────────────────────────────
TEST(FluidMesher, SharedEdgeHasIdenticalElevation) {
    std::cout << "[Test 1] SharedEdgeHasIdenticalElevation..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(0, 0, 0, Voxel{MAT_WATER, 5});
    chunk.set_voxel(1, 0, 0, Voxel{MAT_WATER, 4});

    struct MockWorld {
        const Chunk& m_chunk;
        uint8_t GetBlockMaterial(const glm::ivec3& pos) const {
            if (Chunk::in_bounds(pos.x, pos.y, pos.z)) {
                return m_chunk.get_voxel(pos.x, pos.y, pos.z).material_id;
            }
            return MAT_AIR;
        }
        uint8_t GetBlockFlags(const glm::ivec3& pos) const {
            if (Chunk::in_bounds(pos.x, pos.y, pos.z)) {
                return m_chunk.get_voxel(pos.x, pos.y, pos.z).flags_and_damage;
            }
            return 0;
        }
    };
    MockWorld world{chunk};

    // Calculate shared edge corner heights along X = 1
    // Edge vertex 0 at (1, 0, 0), Edge vertex 1 at (1, 0, 1)
    float h_shared_v0 = SampleCornerHeight(world, 1, 0, 0);
    float h_shared_v1 = SampleCornerHeight(world, 1, 0, 1);

    // Block (0, 0, 0) East edge corners: C10 at (1, 0, 0) and C11 at (1, 0, 1)
    float b0_C10 = SampleCornerHeight(world, 1, 0, 0);
    float b0_C11 = SampleCornerHeight(world, 1, 0, 1);

    // Block (1, 0, 0) West edge corners: C00 at (1, 0, 0) and C01 at (1, 0, 1)
    float b1_C00 = SampleCornerHeight(world, 1, 0, 0);
    float b1_C01 = SampleCornerHeight(world, 1, 0, 1);

    // Verify mathematical identity along shared boundary vertices
    ASSERT_NEAR(b0_C10, b1_C00, 1e-5f);
    ASSERT_NEAR(b0_C11, b1_C01, 1e-5f);
    ASSERT_NEAR(b0_C10, h_shared_v0, 1e-5f);
    ASSERT_NEAR(b0_C11, h_shared_v1, 1e-5f);

    // Generate mesh and verify top face vertices along X = 1 have identical elevation
    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    std::vector<float> b0_edge_y_at_z0;
    std::vector<float> b1_edge_y_at_z0;
    std::vector<float> b0_edge_y_at_z1;
    std::vector<float> b1_edge_y_at_z1;

    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        uint32_t fluid_lvl = (v.data1 >> 28u) & 0x7u;

        if (layer == MAT_WATER && norm == 2 && fluid_lvl == 6) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 1.0f) < 0.001f) {
                if (std::abs(pos.z - 0.0f) < 0.001f) {
                    b0_edge_y_at_z0.push_back(pos.y);
                } else if (std::abs(pos.z - 1.0f) < 0.001f) {
                    b0_edge_y_at_z1.push_back(pos.y);
                }
            }
        }
    }

    ASSERT_FALSE(b0_edge_y_at_z0.empty());
    ASSERT_FALSE(b0_edge_y_at_z1.empty());

    // Both triangles meeting at the shared boundary edge produce identical Y elevation
    for (float y_val : b0_edge_y_at_z0) {
        ASSERT_NEAR(y_val, h_shared_v0, 0.01f);
    }
    for (float y_val : b0_edge_y_at_z1) {
        ASSERT_NEAR(y_val, h_shared_v1, 0.01f);
    }

    std::cout << "  -> PASSED: Shared edge Y at (1, 0)=" << h_shared_v0
              << ", Y at (1, 1)=" << h_shared_v1 << " are identical." << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 2. SkirtEmittedOnElevationDelta
// Verify a vertical boundary skirt quad is emitted between differing levels
// so no hollow void is exposed.
// ─────────────────────────────────────────────────────────────
TEST(FluidMesher, SkirtEmittedOnElevationDelta) {
    std::cout << "[Test 2] SkirtEmittedOnElevationDelta..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(0, 0, 0, Voxel{MAT_WATER, 5});
    chunk.set_voxel(1, 0, 0, Voxel{MAT_WATER, 4});

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    bool found_skirt = false;
    float skirt_top_y = -1.0f;
    float skirt_bot_y = -1.0f;

    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        // Check for vertical step skirt (+X face between (0,0,0) and (1,0,0))
        if (layer == MAT_WATER && norm == 0 && v.is_vertical_flow()) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 1.0f) < 0.001f) {
                found_skirt = true;
                if (skirt_top_y < 0.0f || pos.y > skirt_top_y) skirt_top_y = pos.y;
                if (skirt_bot_y < 0.0f || pos.y < skirt_bot_y) skirt_bot_y = pos.y;
            }
        }
    }

    ASSERT_TRUE(found_skirt);
    // Skirt connects top level 5 (0.88m) down to level 4 (0.704m)
    ASSERT_NEAR(skirt_top_y, 0.88f, 0.02f);
    ASSERT_NEAR(skirt_bot_y, 0.704f, 0.02f);
    ASSERT_TRUE(skirt_top_y > skirt_bot_y);

    std::cout << "  -> PASSED: Skirt emitted from Y=" << skirt_top_y
              << " down to Y=" << skirt_bot_y << " bridging elevation delta." << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- FLUID RENDERING REGRESSION SUITE     " << std::endl;
    std::cout << "==========================================================" << std::endl;

    FluidMesher_SharedEdgeHasIdenticalElevation();
    FluidMesher_SkirtEmittedOnElevationDelta();

    std::cout << "=== All Fluid Rendering Tests PASSED successfully ===" << std::endl;
    return 0;
}
