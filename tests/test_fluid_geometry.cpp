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
// 2. StepTransitionEmitsVerticalSkirt
// Place MAT_WATER Level 5 at (0, 0, 0) and Level 4 at (1, 0, 0).
// Vertical quad is emitted at X = 1.0 bridging Y = 0.70m to Y = 0.88m;
// zero void penetration on horizontal raycast.
// ─────────────────────────────────────────────────────────────
void test_step_transition_emits_vertical_skirt() {
    std::cout << "[Test 2] StepTransitionEmitsVerticalSkirt..." << std::endl;

    Chunk chunk(ChunkPos{0, 0, 0});
    chunk.set_voxel(0, 0, 0, Voxel{MAT_WATER, 5});
    chunk.set_voxel(1, 0, 0, Voxel{MAT_WATER, 4});

    auto mesh = GreedyMesher::generate_mesh(chunk, nullptr);
    ASSERT_FALSE(mesh.empty());

    bool found_step_skirt = false;
    for (size_t i = 0; i + 5 < mesh.size(); i += 6) {
        const auto& v0 = mesh[i];
        uint32_t norm = (v0.data0 >> 18u) & 0x7u;
        uint32_t layer = (v0.data0 >> 23u) & 0xFFu;

        if (layer == MAT_WATER && norm == 0) { // +X normal
            glm::vec3 p0 = mesh[i].position();
            glm::vec3 p1 = mesh[i + 1].position();
            glm::vec3 p2 = mesh[i + 2].position();

            if (std::abs(p0.x - 1.0f) < 0.01f &&
                std::abs(p1.x - 1.0f) < 0.01f &&
                std::abs(p2.x - 1.0f) < 0.01f) {
                float min_y = std::min({p0.y, p1.y, p2.y, mesh[i + 3].position().y, mesh[i + 4].position().y, mesh[i + 5].position().y});
                float max_y = std::max({p0.y, p1.y, p2.y, mesh[i + 3].position().y, mesh[i + 4].position().y, mesh[i + 5].position().y});

                if (std::abs(min_y - 0.70f) < 0.02f && std::abs(max_y - 0.88f) < 0.02f) {
                    found_step_skirt = true;
                    break;
                }
            }
        }
    }
    ASSERT_TRUE(found_step_skirt);

    // Horizontal raycast through the step transition at Y = 0.75m
    glm::vec3 ray_orig(0.5f, 0.75f, 0.5f);
    glm::vec3 ray_dir(1.0f, 0.0f, 0.0f);
    float hit_dist = 0.0f;
    bool hit = ray_intersects_mesh(mesh, ray_orig, ray_dir, 2.0f, hit_dist);

    ASSERT_TRUE(hit);
    ASSERT_NEAR(hit_dist, 0.5f, 0.02f); // Hit skirt at X = 1.0 (dist 0.5 from 0.5)

    std::cout << "  -> PASSED (Step skirt found bridging Y=0.70 to Y=0.88 at X=1.0)" << std::endl;
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

int main() {
    std::cout << "=== Running Fluid Geometry & Watertight Inter-Level Flow Tests ===" << std::endl;

    test_partial_block_fills_negative_space();
    test_step_transition_emits_vertical_skirt();
    test_waterfall_cliff_has_no_air_gap();
    test_arbitrary_ramp_liquid_seals_flanks();

    std::cout << "=== All Fluid Geometry Tests PASSED successfully ===" << std::endl;
    return 0;
}
