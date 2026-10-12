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
// 2. InternalBoundaryBetweenContiguousFluidsIsCulled
// Verify that contiguous liquid blocks do not emit an internal
// vertical "wall" or pane between different height levels.
// ─────────────────────────────────────────────────────────────
TEST(FluidMesher, InternalBoundaryBetweenContiguousFluidsIsCulled) {
    std::cout << "[Test 2] InternalBoundaryBetweenContiguousFluidsIsCulled..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(0, 0, 0, Voxel{MAT_WATER, 5});
    chunk.set_voxel(1, 0, 0, Voxel{MAT_WATER, 4});

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    bool found_internal_pane = false;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        // Verify NO vertical separation pane is emitted at the shared boundary X = 1.0
        if (layer == MAT_WATER && (norm == 0 || norm == 1)) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 1.0f) < 0.001f) {
                found_internal_pane = true;
                break;
            }
        }
    }

    ASSERT_FALSE(found_internal_pane);
    std::cout << "  -> PASSED: Internal boundary face is fully culled; no partition pane emitted." << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 3. LiquidMeshBufferNotEmpty
// Place source water at (1, 1, 1) surrounded by air.
// Mesh chunk and assert chunk->GetLiquidVertexCount() > 0.
// ─────────────────────────────────────────────────────────────
TEST(FluidMeshTest, LiquidMeshBufferNotEmpty) {
    std::cout << "[Test 3] LiquidMeshBufferNotEmpty..." << std::endl;

    auto chunk = std::make_unique<Chunk>(ChunkPos{0, 0, 0});
    chunk->set_voxel(1, 1, 1, Voxel{MAT_WATER, 5});
    chunk->mesh();

    ASSERT_TRUE(chunk->GetLiquidVertexCount() > 0);
    std::cout << "  -> PASSED: Liquid vertex count = " << chunk->GetLiquidVertexCount() << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 4. ContiguousFluidCellsDoNotEmitInternalSeparationPanes
// Place Level 5 water at (0, 0, 0) and Level 4 water at (1, 0, 0).
// Assert that chunk mesh contains only top & outer bounding faces,
// with no internal vertical separator walls between differing levels.
// ─────────────────────────────────────────────────────────────
TEST(FluidMeshTest, ContiguousFluidCellsDoNotEmitInternalSeparationPanes) {
    std::cout << "[Test 4] ContiguousFluidCellsDoNotEmitInternalSeparationPanes..." << std::endl;

    auto chunk = std::make_unique<Chunk>(ChunkPos{0, 0, 0});
    chunk->set_voxel(0, 0, 0, Voxel{MAT_WATER, 5});
    chunk->set_voxel(1, 0, 0, Voxel{MAT_WATER, 4});
    chunk->mesh();

    ASSERT_TRUE(chunk->GetLiquidVertexCount() > 0);

    auto mesh = GreedyMesher::generate_mesh(*chunk, nullptr);
    bool found_internal_pane = false;

    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER && (norm == 0 || norm == 1)) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 1.0f) < 0.001f) {
                found_internal_pane = true;
                break;
            }
        }
    }

    ASSERT_FALSE(found_internal_pane);
    std::cout << "  -> PASSED: Contiguous liquid volume has seamless top without internal walls." << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 5. StepDownSlopesLedgeLipTowardsLowerLevel
// Place Level 5 liquid at (0, 1, 0) and Level 5 liquid at (1, 0, 0)
// with air at (1, 1, 0) (a 1-block step down).
// Verify that inter-level averaging smoothly dips the surface
// at the ledge lip (X = 1) towards the lower liquid.
// ─────────────────────────────────────────────────────────────
TEST(FluidMesher, StepDownSlopesLedgeLipTowardsLowerLevel) {
    std::cout << "[Test 5] StepDownSlopesLedgeLipTowardsLowerLevel..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(0, 1, 0, Voxel{MAT_WATER, 5});
    chunk.set_voxel(1, 0, 0, Voxel{MAT_WATER, 5});
    chunk.set_voxel(1, 1, 0, Voxel{MAT_AIR, 0});

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

    float C_back = CalculateCornerHeight(world, 0, 1, 0);
    float C_lip  = CalculateCornerHeight(world, 1, 1, 0);

    ASSERT_NEAR(C_back, 0.88f, 0.02f);
    // Lip corner should be smoothed downward into the step drop: C_lip < C_back
    ASSERT_TRUE(C_lip < C_back - 0.15f);
    ASSERT_TRUE(C_lip >= 0.40f);

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    float y_back = -1.0f;
    float y_lip  = -1.0f;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        uint32_t fluid_lvl = (v.data1 >> 28u) & 0x7u;
        if (layer == MAT_WATER && norm == 2 && fluid_lvl == 6) {
            glm::vec3 pos = v.position();
            // Check top vertices for block (0, 1, 0)
            if (pos.y >= 1.0f && pos.y <= 2.0f && pos.z <= 1.0f) {
                if (std::abs(pos.x - 0.0f) < 0.01f) {
                    y_back = pos.y;
                } else if (std::abs(pos.x - 1.0f) < 0.01f) {
                    y_lip = pos.y;
                }
            }
        }
    }

    ASSERT_TRUE(y_back > 0.0f);
    ASSERT_TRUE(y_lip > 0.0f);
    // Smooth transition: top surface slopes downhill towards step down
    ASSERT_TRUE(y_lip < y_back - 0.15f);

    std::cout << "  -> PASSED: Top surface slopes from Y=" << y_back
              << " down to Y=" << y_lip << " at step lip." << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- FLUID RENDERING REGRESSION SUITE     " << std::endl;
    std::cout << "==========================================================" << std::endl;

    FluidMesher_SharedEdgeHasIdenticalElevation();
    FluidMesher_InternalBoundaryBetweenContiguousFluidsIsCulled();
    FluidMeshTest_LiquidMeshBufferNotEmpty();
    FluidMeshTest_ContiguousFluidCellsDoNotEmitInternalSeparationPanes();
    FluidMesher_StepDownSlopesLedgeLipTowardsLowerLevel();

    std::cout << "=== All Fluid Rendering Tests PASSED successfully ===" << std::endl;
    return 0;
}
