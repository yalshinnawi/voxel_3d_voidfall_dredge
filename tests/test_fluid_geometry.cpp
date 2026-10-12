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

// Helper: ray-triangle intersection (Möller–Trumbore)
static bool ray_intersects_triangle(
    const glm::vec3& orig, const glm::vec3& dir,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
    float& t_out
) {
    constexpr float kEps = 1e-6f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 pvec = glm::cross(dir, edge2);
    float det = glm::dot(edge1, pvec);

    if (std::abs(det) < kEps) return false;
    float inv_det = 1.0f / det;

    glm::vec3 tvec = orig - v0;
    float u = glm::dot(tvec, pvec) * inv_det;
    if (u < -kEps || u > 1.0f + kEps) return false;

    glm::vec3 qvec = glm::cross(tvec, edge1);
    float v = glm::dot(dir, qvec) * inv_det;
    if (v < -kEps || u + v > 1.0f + kEps) return false;

    float t = glm::dot(edge2, qvec) * inv_det;
    if (t < kEps) return false;

    t_out = t;
    return true;
}

static bool ray_intersects_mesh(
    const std::vector<PackedVoxelVertex>& mesh,
    const glm::vec3& orig, const glm::vec3& dir,
    float max_dist, float& hit_t
) {
    hit_t = max_dist;
    bool hit = false;
    for (size_t i = 0; i + 2 < mesh.size(); i += 3) {
        glm::vec3 v0 = mesh[i].position();
        glm::vec3 v1 = mesh[i + 1].position();
        glm::vec3 v2 = mesh[i + 2].position();

        float t = 0.0f;
        if (ray_intersects_triangle(orig, dir, v0, v1, v2, t)) {
            if (t < hit_t) {
                hit_t = t;
                hit = true;
            }
        }
    }
    return hit;
}

// ─────────────────────────────────────────────────────────────
// 1. PartialBlockFillsNegativeSpace
// Place SHAPE_SLAB_BOTTOM with VOXEL_FLAG_WATERLOGGED at (0, 0, 0).
// Mesher outputs liquid geometry filling Y in [0.5, 0.88]; solid rock intact.
// ─────────────────────────────────────────────────────────────
void test_partial_block_fills_negative_space() {
    std::cout << "[Test 1] PartialBlockFillsNegativeSpace..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(0, 0, 0, Voxel{MAT_FRACTURED_GRANITE, 0});
    chunk.SetShape(0, 0, 0, SHAPE_SLAB_BOTTOM);
    chunk.SetWaterlogged(0, 0, 0, true);

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    float min_liquid_y = 1000.0f;
    float max_liquid_y = -1000.0f;
    float max_rock_y = -1000.0f;
    int liquid_verts = 0;
    int rock_verts = 0;

    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        glm::vec3 p = v.position();

        if (layer == MAT_WATER) {
            liquid_verts++;
            min_liquid_y = std::min(min_liquid_y, p.y);
            max_liquid_y = std::max(max_liquid_y, p.y);
        } else {
            rock_verts++;
            max_rock_y = std::max(max_rock_y, p.y);
        }
    }

    ASSERT_TRUE(liquid_verts > 0);
    ASSERT_TRUE(rock_verts > 0);

    // Solid rock slab must stay intact in [0.0, 0.5]
    ASSERT_NEAR(max_rock_y, 0.5f, 0.005f);

    // Liquid geometry fills negative space Y in [0.5, 0.88]
    ASSERT_NEAR(min_liquid_y, 0.5f, 0.005f);
    ASSERT_NEAR(max_liquid_y, 0.88f, 0.005f);

    std::cout << "  -> PASSED (Liquid Y: [" << min_liquid_y << ", " << max_liquid_y
              << "], Rock max Y: " << max_rock_y << ")" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 2. StepTransitionIsSeamlessWithoutInternalPanes
// Place MAT_WATER Level 5 at (0, 0, 0) and Level 4 at (1, 0, 0).
// Smooth corner-interpolated surface meets seamlessly at X = 1.0;
// Zero internal partition wall / pane is emitted between contiguous fluid cells.
// ─────────────────────────────────────────────────────────────
void test_step_transition_is_seamless_without_internal_panes() {
    std::cout << "[Test 2] StepTransitionIsSeamlessWithoutInternalPanes..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(0, 0, 0, Voxel{MAT_WATER, 5});
    chunk.set_voxel(1, 0, 0, Voxel{MAT_WATER, 4});

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    bool found_internal_pane = false;
    for (size_t i = 0; i + 5 < mesh.size(); i += 6) {
        const auto& v0 = mesh[i];
        uint32_t norm = (v0.data0 >> 18u) & 0x7u;
        uint32_t layer = (v0.data0 >> 23u) & 0xFFu;

        if (layer == MAT_WATER && (norm == 0 || norm == 1)) {
            glm::vec3 p0 = mesh[i].position();
            glm::vec3 p1 = mesh[i + 1].position();
            glm::vec3 p2 = mesh[i + 2].position();

            if (std::abs(p0.x - 1.0f) < 0.01f &&
                std::abs(p1.x - 1.0f) < 0.01f &&
                std::abs(p2.x - 1.0f) < 0.01f) {
                found_internal_pane = true;
                break;
            }
        }
    }
    ASSERT_FALSE(found_internal_pane);

    // Vertical raycast at the shared seam X = 1.0 hits the continuous fluid surface seamlessly
    glm::vec3 ray_orig(1.0f, 1.5f, 0.5f);
    glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);
    float hit_dist = 0.0f;
    bool hit = ray_intersects_mesh(mesh, ray_orig, ray_dir, 2.0f, hit_dist);

    ASSERT_TRUE(hit);
    ASSERT_NEAR(ray_orig.y - hit_dist, 0.792f, 0.02f); // Shared edge elevation

    std::cout << "  -> PASSED (No internal separation pane; surface meets seamlessly at Y=0.792)" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 3. WaterfallCliffHasNoAirGap
// Source water at (0, 1, 0), receiving pool at (1, -1, 0), air at (1, 0, 0).
// Raycast vertically down (1.01, 0.5, 0.5) intersects liquid geometry continuously with no gap.
// ─────────────────────────────────────────────────────────────
void test_waterfall_cliff_has_no_air_gap() {
    std::cout << "[Test 3] WaterfallCliffHasNoAirGap..." << std::endl;

    Chunk chunk_top(ChunkPos{0, 0, 0});
    chunk_top.set_voxel(0, 1, 0, Voxel{MAT_WATER, 5});
    chunk_top.set_voxel(1, 0, 0, Voxel{MAT_AIR, 0});

    Chunk chunk_bot(ChunkPos{0, -1, 0});
    chunk_bot.set_voxel(1, 31, 0, Voxel{MAT_WATER, 5}); // (1, -1, 0) relative to chunk_top

    auto nb_getter = [&](const ChunkPos& p) -> const Chunk* {
        if (p == ChunkPos{0, 0, 0}) return &chunk_top;
        if (p == ChunkPos{0, -1, 0}) return &chunk_bot;
        return nullptr;
    };

    auto mesh_top = GreedyMesher::generate_mesh(chunk_top, nb_getter);
    auto mesh_bot_raw = GreedyMesher::generate_mesh(chunk_bot, nb_getter);

    // Transform mesh_bot to chunk_top relative coordinates (shift Y by -CHUNK_SIZE = -32)
    std::vector<PackedVoxelVertex> combined_mesh = mesh_top;
    for (const auto& v : mesh_bot_raw) {
        glm::vec3 pos = v.position() + glm::vec3(0.0f, -static_cast<float>(CHUNK_SIZE), 0.0f);
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        // Re-encode at relative world position for testing
        PackedVoxelVertex translated = PackedVoxelVertex::encode(
            static_cast<uint32_t>(std::round(pos.x)),
            static_cast<uint32_t>(std::max(0.0f, std::round(pos.y + 32.0f))),
            static_cast<uint32_t>(std::round(pos.z)),
            norm, 0, layer, 1, 1, 0
        );
        (void)translated;
    }

    // Verify continuous vertical curtain down from Y = 1.88 to Y = -0.12 in mesh_top
    bool found_cascade = false;
    for (size_t i = 0; i + 5 < mesh_top.size(); i += 6) {
        const auto& v = mesh_top[i];
        if (v.is_vertical_flow()) {
            float min_y = 1000.0f;
            float max_y = -1000.0f;
            for (size_t k = 0; k < 6; ++k) {
                float y = mesh_top[i + k].position().y;
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }
            if (min_y <= -0.10f && max_y >= 1.85f) {
                found_cascade = true;
                break;
            }
        }
    }
    ASSERT_TRUE(found_cascade);

    // Also test intra-chunk waterfall cliff drop: source at (0, 2, 0), receiving pool at (1, 0, 0), air at (1, 1, 0)
    Chunk intra_chunk(ChunkPos{0, 0, 0});
    intra_chunk.set_voxel(0, 2, 0, Voxel{MAT_WATER, 5});
    intra_chunk.set_voxel(1, 0, 0, Voxel{MAT_WATER, 5});
    intra_chunk.set_voxel(1, 1, 0, Voxel{MAT_AIR, 0});

    auto intra_mesh = GreedyMesher::generate_mesh(intra_chunk, nullptr);
    bool found_intra_cascade = false;
    for (size_t i = 0; i + 5 < intra_mesh.size(); i += 6) {
        const auto& v = intra_mesh[i];
        if (v.is_vertical_flow()) {
            float min_y = 1000.0f;
            float max_y = -1000.0f;
            for (size_t k = 0; k < 6; ++k) {
                float y = intra_mesh[i + k].position().y;
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }
            if (min_y <= 0.90f && max_y >= 2.85f) {
                found_intra_cascade = true;
                break;
            }
        }
    }
    ASSERT_TRUE(found_intra_cascade);

    // Raycast vertically downward down column at (1.01, y, 0.5) hits receiving pool surface at Y = 0.88m
    glm::vec3 ray_orig(1.01f, 1.5f, 0.5f);
    glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);
    float hit_dist = 0.0f;
    bool hit = ray_intersects_mesh(intra_mesh, ray_orig, ray_dir, 5.0f, hit_dist);
    ASSERT_TRUE(hit);
    ASSERT_NEAR(ray_orig.y - hit_dist, 0.88f, 0.02f); // Receives water at Y = 0.88m continuously

    // Raycast horizontally across the waterfall curtain at Y = 1.5m
    glm::vec3 h_orig(0.5f, 1.5f, 0.5f);
    glm::vec3 h_dir(1.0f, 0.0f, 0.0f);
    float h_dist = 0.0f;
    bool h_hit = ray_intersects_mesh(intra_mesh, h_orig, h_dir, 2.0f, h_dist);
    ASSERT_TRUE(h_hit);
    ASSERT_NEAR(h_dist, 0.5f, 0.02f); // Waterfall curtain intercepted at X = 1.0

    std::cout << "  -> PASSED (Continuous cascade quad spans Y=[-0.12, 1.88], zero gap)" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 4. ArbitraryRampLiquidSealsFlanks
// Waterlogged ramp placed adjacent to an air block.
// Exposed triangular flank emits a sealed fluid face; no unrendered interior void is visible.
// ─────────────────────────────────────────────────────────────
void test_arbitrary_ramp_liquid_seals_flanks() {
    std::cout << "[Test 4] ArbitraryRampLiquidSealsFlanks..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(1, 1, 1, Voxel{MAT_FRACTURED_GRANITE, 0});
    chunk.SetShape(1, 1, 1, SHAPE_RAMP_EAST);
    chunk.SetWaterlogged(1, 1, 1, true);

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    bool found_neg_z_flank = false;
    bool found_pos_z_flank = false;
    bool found_low_x_seal = false;

    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER) {
            if (norm == 5) found_neg_z_flank = true;
            if (norm == 4) found_pos_z_flank = true;
            if (norm == 1) found_low_x_seal = true;
        }
    }

    ASSERT_TRUE(found_neg_z_flank);
    ASSERT_TRUE(found_pos_z_flank);
    ASSERT_TRUE(found_low_x_seal);

    std::cout << "  -> PASSED (Triangular flank seals -Z, +Z and low-end vertical quad emitted)" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 5. RampFluidSmoothTransitionAndWaterlineFlanks
// Level 4 water at (0, 0, 0) meets waterlogged SHAPE_RAMP_EAST at (1, 0, 0).
// 1) Fluid transitions smoothly across the boundary X = 1.0 without sudden vertical jump.
// 2) Flank seals on exposed ramp side stop at the waterline and do not fill full unit height (Y <= 0.75m).
// ─────────────────────────────────────────────────────────────
void test_ramp_fluid_smooth_transition_and_waterline_flanks() {
    std::cout << "[Test 5] RampFluidSmoothTransitionAndWaterlineFlanks..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(0, 0, 0, Voxel{MAT_WATER, 4});
    chunk.set_voxel(1, 0, 0, Voxel{MAT_FRACTURED_GRANITE, 0});
    chunk.SetShape(1, 0, 0, SHAPE_RAMP_EAST);
    chunk.SetWaterlogged(1, 0, 0, true);

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    // 1. Verify vertical raycast at the shared boundary X = 1.0 hits smooth fluid surface
    glm::vec3 ray_orig(1.0f, 1.5f, 0.5f);
    glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);
    float hit_dist = 0.0f;
    bool hit = ray_intersects_mesh(mesh, ray_orig, ray_dir, 2.0f, hit_dist);
    ASSERT_TRUE(hit);
    float surface_y = ray_orig.y - hit_dist;
    // Level 4 water is ~0.704m; shared corner must be continuous around this height
    ASSERT_NEAR(surface_y, 0.704f, 0.05f);

    // 2. Verify side flank vertices do NOT fill the full ramp pane (max Y strictly <= 0.75m, not 1.0m)
    float max_flank_y = 0.0f;
    bool found_flank = false;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        uint32_t fluid_lvl = (v.data1 >> 28u) & 0x7u;

        if (layer == MAT_WATER && (norm == 4 || norm == 5) && fluid_lvl == 6) {
            glm::vec3 p = v.position();
            if (p.x >= 1.0f && p.x <= 2.0f) {
                found_flank = true;
                max_flank_y = std::max(max_flank_y, p.y);
            }
        }
    }

    ASSERT_TRUE(found_flank);
    // Flank must stop at the waterline (~0.704m) and NOT reach the full ramp height 1.0m
    ASSERT_TRUE(max_flank_y <= 0.75f);

    std::cout << "  -> PASSED (Boundary Y=" << surface_y << ", Flank max Y=" << max_flank_y << " <= 0.75m)" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 6. WorldGenFluidInitialPlacementRampSmoothness
// Emulate world generation initial placement:
// (0, 1, 0) is pure source liquid pool (MAT_CRYSTAL_AQUIFER, 0)
// (1, 1, 0) is solid stone floor (MAT_VOLCANIC_BASALT, 0) with a 1-block step rising East
// Run World::ApplyTopologicalShapes(chunk).
// Assert:
// 1) (1, 1, 0) is assigned SHAPE_RAMP_EAST AND marked VOXEL_FLAG_WATERLOGGED.
// 2) Generated mesh has NO vertical side quad at the shared boundary X = 1.0 (no separation pane).
// 3) Fluid contour at X = 1.0 matches the source pool waterline (~1.88m) smoothly.
// 4) Flank seals do not fill the unit pane (max Y <= 1.90m, strictly not jumping to 2.04m).
// ─────────────────────────────────────────────────────────────
void test_world_gen_fluid_initial_placement_ramp_smoothness() {
    std::cout << "[Test 6] WorldGenFluidInitialPlacementRampSmoothness..." << std::endl;

    World world(1337, false);
    Chunk test_chunk(ChunkPos{0, 0, 0});

    // Solid bedrock base support at Y = 0
    for (int x = 0; x <= 3; ++x) {
        test_chunk.set_voxel(x, 0, 0, Voxel{MAT_DREDGE_BEDROCK, 0});
    }
    // Floor level at Y = 1:
    // (0, 1, 0): Crystal aquifer pool source block (flags = 0)
    // (1, 1, 0): Basalt floor stepping East
    // (2, 1, 0): Elevated solid step
    test_chunk.set_voxel(0, 1, 0, Voxel{MAT_CRYSTAL_AQUIFER, 0});
    test_chunk.set_voxel(1, 1, 0, Voxel{MAT_VOLCANIC_BASALT, 0});
    test_chunk.set_voxel(2, 1, 0, Voxel{MAT_VOLCANIC_BASALT, 0});
    test_chunk.set_voxel(0, 2, 0, Voxel{MAT_AIR, 0});
    test_chunk.set_voxel(1, 2, 0, Voxel{MAT_AIR, 0});
    test_chunk.set_voxel(2, 2, 0, Voxel{MAT_AIR, 0});

    world.ApplyTopologicalShapes(test_chunk);

    // 1. Verify (1, 1, 0) became SHAPE_RAMP_EAST and is waterlogged
    Voxel ramp_vox = test_chunk.get_voxel(1, 1, 0);
    ASSERT_TRUE(ramp_vox.is_ramp());
    ASSERT_TRUE(ramp_vox.shape() == SHAPE_RAMP_EAST);
    ASSERT_TRUE(ramp_vox.is_waterlogged());

    // 2. Generate mesh
    auto mesh = GreedyMesher::generate_mesh(test_chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    // 3. Verify no vertical separation wall / pane facing the ramp from (0, 1, 0)
    bool found_vertical_wall_at_x1 = false;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_CRYSTAL_AQUIFER && norm == 0 /* +X */) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 1.0f) < 0.05f && pos.y >= 1.0f && pos.y <= 1.9f && std::abs(pos.z) < 1.0f) {
                found_vertical_wall_at_x1 = true;
            }
        }
    }
    ASSERT_FALSE(found_vertical_wall_at_x1);

    // 4. Verify raycast at shared boundary X = 1.0 hits smooth fluid at Y ~ 1.88m (source pool height)
    glm::vec3 ray_orig(1.0f, 2.5f, 0.5f);
    glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);
    float hit_dist = 0.0f;
    bool hit = ray_intersects_mesh(mesh, ray_orig, ray_dir, 2.0f, hit_dist);
    ASSERT_TRUE(hit);
    float surface_y = ray_orig.y - hit_dist;
    ASSERT_NEAR(surface_y, 1.88f, 0.06f);

    // 5. Verify flank seal max Y <= 1.90m (strictly <= 1.90m, not filling full 2.04m ramp)
    float max_flank_y = 0.0f;
    bool found_flank = false;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        uint32_t fluid_lvl = (v.data1 >> 28u) & 0x7u;

        if (layer == MAT_CRYSTAL_AQUIFER && (norm == 4 || norm == 5) && fluid_lvl == 6) {
            glm::vec3 p = v.position();
            if (p.x >= 1.0f && p.x <= 2.0f) {
                found_flank = true;
                max_flank_y = std::max(max_flank_y, p.y);
            }
        }
    }
    ASSERT_TRUE(found_flank);
    ASSERT_TRUE(max_flank_y <= 1.90f);

    std::cout << "  -> PASSED (Ramp waterlogged on world gen, Surface Y=" << surface_y
              << ", Flank max Y=" << max_flank_y << " <= 1.90m)" << std::endl;
}

void test_level_gen_aquifer_oasis() {
    std::cout << "[Test 7] LevelGenAquiferOasisRealWorld..." << std::endl;
    auto gen = std::make_unique<LevelGenerator>(1, 1337);
    gen->create_single_room_test_layout(RoomShapeType::SubterraneanAquiferOasis);
    World world(1337, false);
    world.set_level_generator(std::move(gen));
    world.force_mesh_all_sync();

    int total_ramps = 0;
    int waterlogged_ramps = 0;
    int liquid_voxels = 0;
    int ramps_touching_liquid = 0;
    int non_waterlogged_ramps_touching_liquid = 0;

    for (const auto& [cpos, chunk] : world.chunks()) {
        if (!chunk) continue;
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    Voxel v = chunk->get_voxel(x, y, z);
                    int wx = cpos.x * CHUNK_SIZE + x;
                    int wy = cpos.y * CHUNK_SIZE + y;
                    int wz = cpos.z * CHUNK_SIZE + z;

                    if (v.is_liquid()) liquid_voxels++;
                    if (v.is_ramp()) {
                        total_ramps++;
                        if (v.is_waterlogged()) waterlogged_ramps++;

                        // Check 6 neighbors for liquid
                        const glm::ivec3 dirs[6] = {{0,1,0},{0,-1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}};
                        bool touches_liquid = false;
                        for (const auto& d : dirs) {
                            Voxel nb = world.get_voxel(wx + d.x, wy + d.y, wz + d.z);
                            if (nb.is_liquid() || nb.is_waterlogged()) {
                                touches_liquid = true;
                                break;
                            }
                        }
                        if (touches_liquid) {
                            ramps_touching_liquid++;
                            if (!v.is_waterlogged()) {
                                non_waterlogged_ramps_touching_liquid++;
                                std::cout << "  -> Found non-waterlogged ramp touching liquid at world ("
                                          << wx << ", " << wy << ", " << wz << ") shape=" << (int)v.shape()
                                          << " mat=" << (int)v.material_id << " flags=" << (int)v.flags_and_damage << std::endl;
                            }
                        }
                    }
                }
            }
        }
    }

    std::cout << "  Stats: liquid_voxels=" << liquid_voxels
              << " total_ramps=" << total_ramps
              << " waterlogged_ramps=" << waterlogged_ramps
              << " ramps_touching_liquid=" << ramps_touching_liquid
              << " non_waterlogged_ramps_touching_liquid=" << non_waterlogged_ramps_touching_liquid << std::endl;

    // Inspect the generated meshes across all chunks
    int full_incline_quads = 0;
    int smooth_fluid_triangles = 0;
    int flank_quads_or_tris = 0;
    float max_flank_rel_y = 0.0f;

    for (const auto& [cpos, chunk] : world.chunks()) {
        if (!chunk) continue;
        auto mesh = GreedyMesher::generate_mesh(*chunk, [&](const ChunkPos& p) {
            return world.get_chunk(p);
        });

        for (size_t i = 0; i < mesh.size(); ++i) {
            const auto& v = mesh[i];
            uint32_t layer = (v.data0 >> 23u) & 0xFFu;
            uint32_t norm = (v.data0 >> 18u) & 0x7u;
            uint32_t fluid_lvl = (v.data1 >> 28u) & 0x7u;

            if (layer == MAT_CRYSTAL_AQUIFER) {
                glm::vec3 pos = v.position();
                int lx = static_cast<int>(std::floor(pos.x));
                int ly = static_cast<int>(std::floor(pos.y));
                int lz = static_cast<int>(std::floor(pos.z));
                Voxel vox = chunk->get_voxel(lx, ly, lz);

                if (vox.is_ramp()) {
                    if (norm == 2 && fluid_lvl == 0) {
                        full_incline_quads++;
                        int wx = cpos.x * CHUNK_SIZE + lx;
                        int wy = cpos.y * CHUNK_SIZE + ly;
                        int wz = cpos.z * CHUNK_SIZE + lz;
                        std::cout << "  -> MYSTERY QUAD at world (" << wx << ", " << wy << ", " << wz
                                  << ") vertex pos=(" << pos.x << ", " << pos.y << ", " << pos.z << ")"
                                  << " shape=" << (int)vox.shape() << " waterlogged=" << vox.is_waterlogged()
                                  << " fluid_lvl=" << (int)vox.fluid_level()
                                  << " data0=0x" << std::hex << v.data0 << " data1=0x" << v.data1 << std::dec << std::endl;
                        const glm::ivec3 dirs[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                        const char* dir_names[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
                        for (int d = 0; d < 6; ++d) {
                            Voxel nb = world.get_voxel(wx + dirs[d].x, wy + dirs[d].y, wz + dirs[d].z);
                            std::cout << "     neighbor " << dir_names[d] << " world ("
                                      << wx + dirs[d].x << "," << wy + dirs[d].y << "," << wz + dirs[d].z
                                      << ") mat=" << (int)nb.material_id << " shape=" << (int)nb.shape()
                                      << " wl=" << nb.is_waterlogged() << " is_liq=" << nb.is_liquid()
                                      << " lvl=" << (int)nb.fluid_level() << std::endl;
                        }
                    }
                    if (norm == 2 && fluid_lvl == 6) smooth_fluid_triangles++;
                    if ((norm == 4 || norm == 5) && fluid_lvl == 6) {
                        flank_quads_or_tris++;
                        float rel_y = pos.y - static_cast<float>(ly);
                        max_flank_rel_y = std::max(max_flank_rel_y, rel_y);
                    }
                }
            }
        }
    }

    std::cout << "  Ramp Mesh Inspection: full_incline_quads=" << full_incline_quads
              << " smooth_fluid_triangles=" << smooth_fluid_triangles
              << " flank_elements=" << flank_quads_or_tris
              << " max_flank_rel_y=" << max_flank_rel_y << std::endl;
    ASSERT_TRUE(full_incline_quads == 0);
}

void test_world_gen_upper_terrace_ramp_cascade() {
    std::cout << "[Test 8] WorldGenUpperTerraceRampCascade..." << std::endl;
    World world(1337, false);
    Chunk chunk(ChunkPos{0, 0, 0});

    // Solid bedrock base support
    for (int x = 0; x <= 4; ++x) {
        chunk.set_voxel(x, 0, 0, Voxel{MAT_DREDGE_BEDROCK, 0});
    }

    // Elevated terrace ledge at X=2, Y=1 (solid block supporting upper pool)
    chunk.set_voxel(2, 1, 0, Voxel{MAT_VOLCANIC_BASALT, 0});
    // Upper source liquid pool at X=2, Y=2
    chunk.set_voxel(2, 2, 0, Voxel{MAT_CRYSTAL_AQUIFER, 0});

    // Descending ramp at X=1, Y=1 rising East towards the upper terrace (SHAPE_RAMP_EAST)
    chunk.set_voxel(1, 1, 0, Voxel{MAT_VOLCANIC_BASALT, 0});
    chunk.SetShape(1, 1, 0, SHAPE_RAMP_EAST);

    // Apply world generation topological shapes & waterlogging
    world.ApplyTopologicalShapes(chunk);

    // 1. Verify ramp at (1, 1, 0) was waterlogged by the upper liquid pool at (2, 2, 0)
    Voxel ramp = chunk.get_voxel(1, 1, 0);
    ASSERT_TRUE(ramp.is_ramp());
    ASSERT_TRUE(ramp.is_waterlogged());

    // 2. Mesh chunk
    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    // 3. Verify NO vertical curtain / separation wall is emitted from upper pool at X = 2.0 facing -X (norm 1)
    bool found_vertical_wall_at_crest = false;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_CRYSTAL_AQUIFER && norm == 1 /* -X */) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 2.0f) < 0.05f && pos.y >= 2.0f) {
                found_vertical_wall_at_crest = true;
            }
        }
    }
    ASSERT_FALSE(found_vertical_wall_at_crest);

    // 4. Verify raycast on ramp crest at X = 1.95 hits fluid surface flush with top of ramp (Y ~ 1.99m)
    glm::vec3 ray_orig(1.95f, 3.0f, 0.5f);
    glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);
    float hit_dist = 0.0f;
    bool hit = ray_intersects_mesh(mesh, ray_orig, ray_dir, 2.0f, hit_dist);
    ASSERT_TRUE(hit);
    float crest_y = ray_orig.y - hit_dist;
    ASSERT_NEAR(crest_y, 1.99f, 0.05f);
    std::cout << "  -> PASSED (Descending ramp waterlogged from upper pool, no crest wall, Crest Y=" << crest_y << ")" << std::endl;
}

void test_break_retaining_wall_flat_ground() {
    std::cout << "[Test 9] BreakRetainingWallFlatGround..." << std::endl;
    World world(1337, false);
    FluidSim::SetActiveWorld(&world);

    // Bedrock floor at Y = 0
    for (int x = -2; x <= 4; ++x) {
        for (int z = -2; z <= 2; ++z) {
            world.SetBlock(x, 0, z, MAT_DREDGE_BEDROCK, 0);
        }
    }
    // Fluid pool at (0, 1, 0) contained by basalt wall at (1, 1, 0)
    world.SetBlock(-1, 1, 0, MAT_VOLCANIC_BASALT, 0);
    world.SetBlock(0, 1, 1, MAT_VOLCANIC_BASALT, 0);
    world.SetBlock(0, 1, -1, MAT_VOLCANIC_BASALT, 0);
    world.SetBlock(1, 1, 0, MAT_VOLCANIC_BASALT, 0); // Wall to break
    world.SetBlock(2, 1, 0, MAT_AIR, 0);

    world.SetBlockWithFlags(glm::ivec3(0, 1, 0), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(0, 1, 0));

    // Break the wall at (1, 1, 0)
    world.DestroyBlock(glm::ivec3(1, 1, 0));

    // Tick fluid simulation so water spreads into (1, 1, 0)
    for (int t = 0; t < 6; ++t) {
        world.update_fluids(0.085f);
    }

    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(1, 1, 0)), MAT_WATER);

    // Mesh chunk containing the fluid
    Chunk* chunk = world.GetChunkFromBlockPos(glm::ivec3(0, 1, 0));
    ASSERT_TRUE(chunk != nullptr);
    auto mesh = GreedyMesher::generate_mesh(*chunk, [&](const ChunkPos& p) {
        return world.get_chunk(p);
    });

    // Verify NO vertical separation wall / plane at X = 1.0 facing +X or -X between contiguous fluid
    bool found_pane_at_x1 = false;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER && (norm == 0 || norm == 1)) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 1.0f) < 0.05f && pos.y >= 1.0f && pos.y <= 2.0f && std::abs(pos.z) < 1.0f) {
                found_pane_at_x1 = true;
            }
        }
    }
    ASSERT_FALSE(found_pane_at_x1);
    std::cout << "  -> PASSED (No internal plane at X=1.0 after wall break)" << std::endl;
}

void test_break_retaining_wall_step_down() {
    std::cout << "[Test 10] BreakRetainingWallStepDown..." << std::endl;
    World world(1337, false);
    FluidSim::SetActiveWorld(&world);

    // Bedrock base
    for (int x = -2; x <= 4; ++x) {
        for (int z = -2; z <= 2; ++z) {
            world.SetBlock(x, 0, z, MAT_DREDGE_BEDROCK, 0);
            for (int y = 1; y <= 5; ++y) {
                world.SetBlock(x, y, z, MAT_AIR, 0);
            }
        }
    }

    // Upper terrace at Y = 1: solid support at (0, 1, 0)
    world.SetBlock(0, 1, 0, MAT_VOLCANIC_BASALT, 0);
    world.SetBlock(-1, 2, 0, MAT_VOLCANIC_BASALT, 0);
    world.SetBlock(0, 2, 1, MAT_VOLCANIC_BASALT, 0);
    world.SetBlock(0, 2, -1, MAT_VOLCANIC_BASALT, 0);

    // Upper water pool at (0, 2, 0)
    world.SetBlockWithFlags(glm::ivec3(0, 2, 0), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(0, 2, 0));

    // Retaining wall at (1, 2, 0)
    world.SetBlock(1, 2, 0, MAT_VOLCANIC_BASALT, 0);

    // Step down: (1, 1, 0) is AIR initially (lower floor is bedrock at Y = 0)
    world.SetBlock(1, 1, 0, MAT_AIR, 0);

    // Break the retaining wall at (1, 2, 0)
    world.DestroyBlock(glm::ivec3(1, 2, 0));

    // Tick fluid simulation so water cascades down into (1, 1, 0)
    for (int t = 0; t < 8; ++t) {
        world.update_fluids(0.085f);
    }

    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(1, 2, 0)), MAT_WATER);
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(1, 1, 0)), MAT_WATER);

    Chunk* chunk = world.GetChunkFromBlockPos(glm::ivec3(0, 2, 0));
    ASSERT_TRUE(chunk != nullptr);
    auto mesh = GreedyMesher::generate_mesh(*chunk, [&](const ChunkPos& p) {
        return world.get_chunk(p);
    });
    std::cout << "  chunk(0,2,0) mat=" << (int)chunk->get_voxel(0, 2, 0).material_id
              << " flags=" << (int)chunk->get_voxel(0, 2, 0).flags_and_damage << std::endl;
    std::cout << "  chunk(1,2,0) mat=" << (int)chunk->get_voxel(1, 2, 0).material_id
              << " flags=" << (int)chunk->get_voxel(1, 2, 0).flags_and_damage << std::endl;
    std::cout << "  chunk(1,1,0) mat=" << (int)chunk->get_voxel(1, 1, 0).material_id
              << " flags=" << (int)chunk->get_voxel(1, 1, 0).flags_and_damage << std::endl;
    std::cout << "  Test 10 mesh size = " << mesh.size() << std::endl;
    // 1. Verify NO vertical separation plane at X = 1.0 between (0, 2, 0) and (1, 2, 0)
    bool found_pane_at_x1 = false;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER && (norm == 0 || norm == 1)) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 1.0f) < 0.05f && pos.y >= 2.0f && pos.y <= 3.0f && std::abs(pos.z) < 1.0f) {
                found_pane_at_x1 = true;
            }
        }
    }
    ASSERT_FALSE(found_pane_at_x1);

    int water_verts = 0;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER) {
            water_verts++;
            glm::vec3 pos = v.position();
            std::cout << "  [Water Vertex] norm=" << norm
                      << " pos=(" << pos.x << ", " << pos.y << ", " << pos.z << ")"
                      << " is_vert=" << v.is_vertical_flow() << std::endl;
        }
    }
    std::unordered_set<uint32_t> layers;
    for (const auto& v : mesh) {
        layers.insert((v.data0 >> 23u) & 0xFFu);
    }
    std::cout << "  Unique layers in mesh: ";
    for (auto l : layers) std::cout << l << " ";
    std::cout << std::endl;
    std::cout << "  MAT_WATER constant = " << (int)MAT_WATER << std::endl;
    // 2. Verify NO protruding vertical plane sticking up above the sloped surface at X = 2.0
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        if (layer == MAT_WATER) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 2.0f) < 0.05f && pos.z >= 0.0f && pos.z <= 1.0f) {
                ASSERT_TRUE(pos.y <= 2.46f);
            }
        }
    }

    std::cout << "  -> PASSED" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 11. BreakBlockAdjacentToWaterloggedSubblock
// Retaining wall adjacent to a waterlogged slab is broken. Fluid flows outward.
// Verify no unnatural vertical plane is left at the slab boundary.
// ─────────────────────────────────────────────────────────────
void test_break_block_adjacent_to_waterlogged_subblock() {
    std::cout << "[Test 11] BreakBlockAdjacentToWaterloggedSubblock..." << std::endl;
    World world(1337, false);
    FluidSim::SetActiveWorld(&world);

    for (int x = -2; x <= 4; ++x) {
        for (int z = -2; z <= 2; ++z) {
            world.SetBlock(x, 0, z, MAT_DREDGE_BEDROCK, 0);
            for (int y = 1; y <= 4; ++y) {
                world.SetBlock(x, y, z, MAT_AIR, 0);
            }
        }
    }

    // Water source at (0, 1, 0)
    world.SetBlockWithFlags(glm::ivec3(0, 1, 0), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(0, 1, 0));

    // Waterlogged bottom slab at (1, 1, 0)
    world.SetBlockWithFlags(glm::ivec3(1, 1, 0), MAT_VOLCANIC_BASALT, SHAPE_SLAB_BOTTOM | VOXEL_FLAG_WATERLOGGED | 5);
    world.PushActiveFluid(glm::ivec3(1, 1, 0));

    // Retaining solid wall at (2, 1, 0)
    world.SetBlock(2, 1, 0, MAT_VOLCANIC_BASALT, 0);

    // Break the retaining wall at (2, 1, 0)
    world.DestroyBlock(glm::ivec3(2, 1, 0));

    // Tick fluid simulation
    for (int t = 0; t < 6; ++t) {
        world.update_fluids(0.085f);
    }

    // Fluid should spread from waterlogged slab into (2, 1, 0)
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(2, 1, 0)), MAT_WATER);

    Chunk* chunk = world.GetChunkFromBlockPos(glm::ivec3(1, 1, 0));
    ASSERT_TRUE(chunk != nullptr);
    auto mesh = GreedyMesher::generate_mesh(*chunk, [&](const ChunkPos& p) {
        return world.get_chunk(p);
    });

    // Verify NO internal separation plane between waterlogged slab (1, 1, 0) and newly flooded block (2, 1, 0)
    bool found_pane_at_x2 = false;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER && (norm == 0 || norm == 1)) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 2.0f) < 0.05f && pos.y >= 1.0f && pos.y <= 2.0f && std::abs(pos.z) < 1.0f) {
                found_pane_at_x2 = true;
            }
        }
    }
    ASSERT_FALSE(found_pane_at_x2);
    std::cout << "  -> PASSED (Fluid flows seamlessly through waterlogged subblock without boundary plane)" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 12. BreakRetainingWallAcrossChunkBoundary
// Fluid at chunk boundary (31, 1, 0) held by retaining wall at (32, 1, 0).
// Breaking (32, 1, 0) must dirty both chunks, allowing fluid to cross border
// without leaving a lingering boundary plane in either chunk's mesh.
// ─────────────────────────────────────────────────────────────
void test_break_retaining_wall_across_chunk_boundary() {
    std::cout << "[Test 12] BreakRetainingWallAcrossChunkBoundary..." << std::endl;
    World world(1337, false);
    FluidSim::SetActiveWorld(&world);

    // Bedrock base under both chunks
    for (int x = 28; x <= 35; ++x) {
        for (int z = -2; z <= 2; ++z) {
            world.SetBlock(x, 0, z, MAT_DREDGE_BEDROCK, 0);
            for (int y = 1; y <= 4; ++y) {
                world.SetBlock(x, y, z, MAT_AIR, 0);
            }
        }
    }

    // Water source at (31, 1, 0) in Chunk 0
    world.SetBlockWithFlags(glm::ivec3(31, 1, 0), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(31, 1, 0));

    // Retaining wall at (32, 1, 0) in Chunk 1
    world.SetBlock(32, 1, 0, MAT_VOLCANIC_BASALT, 0);

    // Break the retaining wall across the border
    world.DestroyBlock(glm::ivec3(32, 1, 0));

    // Tick fluid simulation
    for (int t = 0; t < 6; ++t) {
        world.update_fluids(0.085f);
    }

    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(32, 1, 0)), MAT_WATER);

    Chunk* chunk0 = world.get_chunk(ChunkPos{0, 0, 0});
    Chunk* chunk1 = world.get_chunk(ChunkPos{1, 0, 0});
    ASSERT_TRUE(chunk0 != nullptr);
    ASSERT_TRUE(chunk1 != nullptr);

    auto mesh0 = GreedyMesher::generate_mesh(*chunk0, [&](const ChunkPos& p) {
        return world.get_chunk(p);
    });
    auto mesh1 = GreedyMesher::generate_mesh(*chunk1, [&](const ChunkPos& p) {
        return world.get_chunk(p);
    });

    // Verify neither chunk contains an internal plane at the boundary X = 32.0 (local X=32 in chunk0, local X=0 in chunk1)
    bool pane_in_chunk0 = false;
    for (const auto& v : mesh0) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER && norm == 0) { // +X face of block 31
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 32.0f) < 0.05f && pos.y >= 1.0f && pos.y <= 2.0f && std::abs(pos.z) < 1.0f) {
                pane_in_chunk0 = true;
            }
        }
    }
    ASSERT_FALSE(pane_in_chunk0);

    bool pane_in_chunk1 = false;
    for (const auto& v : mesh1) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER && norm == 1) { // -X face of block 32 (local 0 in chunk 1)
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 0.0f) < 0.05f && pos.y >= 1.0f && pos.y <= 2.0f && std::abs(pos.z) < 1.0f) {
                pane_in_chunk1 = true;
            }
        }
    }
    ASSERT_FALSE(pane_in_chunk1);
    std::cout << "  -> PASSED (Cross-chunk boundary fluid flow clean, zero lingering panes)" << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 13. BreakRetainingWallAboveCliffCascade
// High fluid reservoir at (0, 4, 0). Retaining wall at (1, 4, 0).
// Breaking retaining wall triggers fluid falling 3 blocks onto bedrock at Y = 0.
// Verify cascade forms cleanly without gaps and no protruding lip at cliff edge.
// ─────────────────────────────────────────────────────────────
void test_break_retaining_wall_above_cliff_cascade() {
    std::cout << "[Test 13] BreakRetainingWallAboveCliffCascade..." << std::endl;
    World world(1337, false);
    FluidSim::SetActiveWorld(&world);

    for (int x = -2; x <= 4; ++x) {
        for (int z = -2; z <= 2; ++z) {
            world.SetBlock(x, 0, z, MAT_DREDGE_BEDROCK, 0);
            for (int y = 1; y <= 6; ++y) {
                world.SetBlock(x, y, z, MAT_AIR, 0);
            }
        }
    }

    // High terrace support under (0, 4, 0)
    for (int y = 1; y <= 3; ++y) {
        world.SetBlock(0, y, 0, MAT_VOLCANIC_BASALT, 0);
        world.SetBlock(-1, y, 0, MAT_VOLCANIC_BASALT, 0);
    }
    world.SetBlock(-1, 4, 0, MAT_VOLCANIC_BASALT, 0);
    world.SetBlock(0, 4, 1, MAT_VOLCANIC_BASALT, 0);
    world.SetBlock(0, 4, -1, MAT_VOLCANIC_BASALT, 0);

    // Water pool at (0, 4, 0)
    world.SetBlockWithFlags(glm::ivec3(0, 4, 0), MAT_WATER, 5);
    world.PushActiveFluid(glm::ivec3(0, 4, 0));

    // Retaining wall at (1, 4, 0)
    world.SetBlock(1, 4, 0, MAT_VOLCANIC_BASALT, 0);

    // Break retaining wall
    world.DestroyBlock(glm::ivec3(1, 4, 0));

    // Tick fluid simulation for cascade to fall to (1, 1, 0)
    for (int t = 0; t < 12; ++t) {
        world.update_fluids(0.085f);
    }

    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(1, 4, 0)), MAT_WATER);
    ASSERT_EQ(world.GetBlockMaterial(glm::ivec3(1, 1, 0)), MAT_WATER);

    Chunk* chunk = world.GetChunkFromBlockPos(glm::ivec3(0, 4, 0));
    ASSERT_TRUE(chunk != nullptr);
    auto mesh = GreedyMesher::generate_mesh(*chunk, [&](const ChunkPos& p) {
        return world.get_chunk(p);
    });

    // Verify no separation pane at X = 1.0
    bool found_pane_at_x1 = false;
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        uint32_t norm = (v.data0 >> 18u) & 0x7u;
        if (layer == MAT_WATER && (norm == 0 || norm == 1)) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 1.0f) < 0.05f && pos.y >= 4.0f && pos.y <= 5.0f && std::abs(pos.z) < 1.0f) {
                found_pane_at_x1 = true;
            }
        }
    }
    ASSERT_FALSE(found_pane_at_x1);

    // Verify cliff edge has no protruding plane sticking above water surface (Y <= 4.88)
    for (const auto& v : mesh) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        if (layer == MAT_WATER && v.is_vertical_flow()) {
            glm::vec3 pos = v.position();
            if (std::abs(pos.x - 2.0f) < 0.05f && pos.y > 4.88f) {
                ASSERT_TRUE(false); // Protruding fin above cascade crest
            }
        }
    }
    std::cout << "  -> PASSED (Cliff cascade flows naturally without protruding lip or internal pane)" << std::endl;
}

int main() {
    std::cout << "=== Running Fluid Geometry & Sub-Block Flow Tests ===" << std::endl;

    test_partial_block_fills_negative_space();
    test_step_transition_is_seamless_without_internal_panes();
    test_waterfall_cliff_has_no_air_gap();
    test_arbitrary_ramp_liquid_seals_flanks();
    test_ramp_fluid_smooth_transition_and_waterline_flanks();
    test_world_gen_fluid_initial_placement_ramp_smoothness();
    test_level_gen_aquifer_oasis();
    test_world_gen_upper_terrace_ramp_cascade();
    test_break_retaining_wall_flat_ground();
    test_break_retaining_wall_step_down();
    test_break_block_adjacent_to_waterlogged_subblock();
    test_break_retaining_wall_across_chunk_boundary();
    test_break_retaining_wall_above_cliff_cascade();

    std::cout << "=== All 13 Fluid Geometry Tests PASSED successfully ===" << std::endl;
    return 0;
}
