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

#define TEST(suite, name) void suite##_##name()

// ─────────────────────────────────────────────────────────────
// 1. CornerAveragingCreatesSlopedQuad
// Place Level 5 water at (0, 0, 0) and Level 4 water at (1, 0, 0).
// Mesh the chunk and inspect the top surface quad for (0, 0, 0).
// Assert C10 < C00 (shared edge vertices are lower than source edge, verifying incline).
// ─────────────────────────────────────────────────────────────
TEST(MinecraftFluid, CornerAveragingCreatesSlopedQuad) {
    std::cout << "[Test 1] CornerAveragingCreatesSlopedQuad..." << std::endl;

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

    float C00 = CalculateCornerHeight(world, 0, 0, 0);
    float C10 = CalculateCornerHeight(world, 1, 0, 0);
    ASSERT_NEAR(C00, 0.88f, 0.01f);
    ASSERT_TRUE(C10 < C00);

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    // Inspect top surface vertices emitted for block (0, 0, 0)
    float min_x0_y = 100.0f;
    float min_x1_y = 100.0f;
    bool found_top = false;

    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER && norm == 2) {
            glm::vec3 pos = v.position();
            if (pos.x <= 1.0f && pos.z <= 1.0f && pos.y <= 1.0f) {
                found_top = true;
                if (pos.x < 0.01f) {
                    min_x0_y = std::min(min_x0_y, pos.y);
                } else if (std::abs(pos.x - 1.0f) < 0.01f) {
                    min_x1_y = std::min(min_x1_y, pos.y);
                }
            }
        }
    }

    ASSERT_TRUE(found_top);
    ASSERT_TRUE(min_x1_y < min_x0_y); // Incline slope verified

    std::cout << "  -> PASSED (C00=" << C00 << ", C10=" << C10 << ", C10 < C00 incline confirmed)" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 2. FluidFillsNegativeSpaceOfRamp
// Place SHAPE_RAMP_EAST with VOXEL_FLAG_WATERLOGGED and Level 4 at (0, 0, 0).
// Assert that the mesher produces fluid vertices occupying the volume above the incline and seals open vertical flanks.
// ─────────────────────────────────────────────────────────────
TEST(MinecraftFluid, FluidFillsNegativeSpaceOfRamp) {
    std::cout << "[Test 2] FluidFillsNegativeSpaceOfRamp..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(0, 0, 0, Voxel{MAT_FRACTURED_GRANITE, 0});
    chunk.set_shape(0, 0, 0, SHAPE_RAMP_EAST);
    chunk.SetFlags(0, 0, 0, static_cast<uint8_t>(SHAPE_RAMP_EAST) | VOXEL_FLAG_WATERLOGGED | 4);

    Chunk empty_chunk(ChunkPos{0, 0, -1});
    auto get_nb = [&](const ChunkPos&) -> const Chunk* {
        return &empty_chunk;
    };
    auto mesh = GreedyMesher::generate_mesh(chunk, get_nb);
    ASSERT_FALSE(mesh.empty());

    bool found_sloped_fluid_top = false;
    bool found_neg_z_flank = false;
    bool found_pos_z_flank = false;
    bool found_low_x_seal = false;

    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        uint32_t fluid_lvl = (v.data1 >> 28u) & 0x7u;

        if (layer == MAT_WATER) {
            if (norm == 2 && fluid_lvl == 6) {
                found_sloped_fluid_top = true;
                glm::vec3 pos = v.position();
                ASSERT_TRUE(pos.y >= 0.0f && pos.y <= 1.0f);
            }
            if (norm == 5) found_neg_z_flank = true;
            if (norm == 4) found_pos_z_flank = true;
            if (norm == 1) found_low_x_seal = true;
        }
    }

    ASSERT_TRUE(found_sloped_fluid_top);
    ASSERT_TRUE(found_neg_z_flank);
    ASSERT_TRUE(found_pos_z_flank);
    ASSERT_TRUE(found_low_x_seal);

    std::cout << "  -> PASSED (Fluid top triangles + vertical flank seals verified)" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 3. WaterStreamsDownhillOnRamp
// Waterlog a ramp with fluid level 4.
// Step FluidSim::Update().
// Assert neighbor voxel at foot of ramp receives flowing water on subsequent tick.
// ─────────────────────────────────────────────────────────────
TEST(MinecraftFluid, WaterStreamsDownhillOnRamp) {
    std::cout << "[Test 3] WaterStreamsDownhillOnRamp..." << std::endl;

    World world(1337, false);
    glm::ivec3 rampPos(16, 10, 16);
    glm::ivec3 footPos(15, 10, 16); // Downhill foot for SHAPE_RAMP_EAST (-X)

    world.SetBlock(rampPos.x, rampPos.y, rampPos.z, MAT_FRACTURED_GRANITE, 0);
    world.SetBlockWithFlags(rampPos, MAT_FRACTURED_GRANITE,
                            static_cast<uint8_t>(SHAPE_RAMP_EAST) | VOXEL_FLAG_WATERLOGGED | 4);

    world.SetBlock(footPos.x, footPos.y, footPos.z, MAT_AIR, 0);
    world.SetBlock(footPos.x, footPos.y - 1, footPos.z, MAT_FRACTURED_GRANITE, 0); // Solid floor under foot

    world.WakeFluid(rampPos);

    // Step simulation
    world.update_fluids(0.085f);

    Voxel footVox = world.get_voxel(footPos.x, footPos.y, footPos.z);
    ASSERT_TRUE(IsLiquid(footVox.material_id));

    std::cout << "  -> PASSED (Foot of ramp received liquid: mat=" << static_cast<int>(footVox.material_id) << ")" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- MINECRAFT FLUID REGRESSION SUITE     " << std::endl;
    std::cout << "==========================================================" << std::endl;

    MinecraftFluid_CornerAveragingCreatesSlopedQuad();
    MinecraftFluid_FluidFillsNegativeSpaceOfRamp();
    MinecraftFluid_WaterStreamsDownhillOnRamp();

    std::cout << "=== All Minecraft Fluid Tests PASSED successfully ===" << std::endl;
    return 0;
}
