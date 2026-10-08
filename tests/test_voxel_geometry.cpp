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
#include "../src/player/controller.hpp"

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

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) { \
        std::cerr << "[TEST FAILED] " #a " (" << (a) << ") > " #b " (" << (b) << ") failed at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define ASSERT_NEAR(a, b, eps) \
    if (std::abs((a) - (b)) > (eps)) { \
        std::cerr << "[TEST FAILED] |" #a " - " #b "| <= " #eps " failed (" << (a) << " vs " << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

#define TEST(Suite, Case) void Suite##_##Case()

// Möller–Trumbore ray-triangle intersection
static bool ray_triangle_intersect(
    const glm::vec3& orig, const glm::vec3& dir,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
    float& t
) {
    constexpr float EPSILON = 1e-5f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 pvec = glm::cross(dir, edge2);
    float det = glm::dot(edge1, pvec);
    if (std::abs(det) < EPSILON) return false;
    float invDet = 1.0f / det;
    glm::vec3 tvec = orig - v0;
    float u = glm::dot(tvec, pvec) * invDet;
    if (u < -0.02f || u > 1.02f) return false;
    glm::vec3 qvec = glm::cross(tvec, edge1);
    float v = glm::dot(dir, qvec) * invDet;
    if (v < -0.02f || u + v > 1.02f) return false;
    t = glm::dot(edge2, qvec) * invDet;
    return t > EPSILON;
}

// 1. Watertight boundary culling test
TEST(VoxelGeometry, CubeAgainstRampWatertight) {
    Chunk chunk(ChunkPos(0, 0, 0));
    chunk.set_voxel(0, 0, 0, Voxel{MAT_GRANITE, 0}); // SHAPE_CUBE

    Voxel ramp_v{MAT_GRANITE, 0};
    ramp_v.set_shape(SHAPE_RAMP_EAST);
    chunk.set_voxel(1, 0, 0, ramp_v); // SHAPE_RAMP_EAST

    auto mesh = GreedyMesher::MeshChunk(chunk);
    ASSERT_GT(mesh.size(), 0);

    // Build triangle list from mesh
    struct Triangle {
        glm::vec3 v0, v1, v2;
    };
    std::vector<Triangle> triangles;
    for (size_t i = 0; i + 2 < mesh.size(); i += 3) {
        triangles.push_back({mesh[i].position(), mesh[i + 1].position(), mesh[i + 2].position()});
    }

    // Cast a dense ray grid (0.05m spacing) from (2.0f, y, z) along -X
    glm::vec3 ray_dir(-1.0f, 0.0f, 0.0f);
    int total_rays = 0;
    int hit_rays = 0;
    int missed_rays = 0;

    for (float y = 0.05f; y <= 0.95f; y += 0.05f) {
        for (float z = 0.05f; z <= 0.95f; z += 0.05f) {
            glm::vec3 ray_orig(2.05f, y, z);
            total_rays++;
            bool hit = false;
            float min_t = 1e9f;
            for (const auto& tri : triangles) {
                float t = 0.0f;
                if (ray_triangle_intersect(ray_orig, ray_dir, tri.v0, tri.v1, tri.v2, t)) {
                    if (t < min_t) {
                        min_t = t;
                        hit = true;
                    }
                }
            }
            if (hit && min_t <= 2.2f) {
                hit_rays++;
            } else {
                missed_rays++;
            }
        }
    }

    ASSERT_GT(total_rays, 0);
    ASSERT_EQ(missed_rays, 0);
    ASSERT_EQ(hit_rays, total_rays);
    std::cout << "[PASS] VoxelGeometry.CubeAgainstRampWatertight (Dense ray grid hits: "
              << hit_rays << "/" << total_rays << ", 0 void misses)" << std::endl;
}

// 2. Isolated ramp face count test
TEST(VoxelGeometry, RampSelfEnclosingFaceCount) {
    Chunk chunk(ChunkPos(0, 0, 0));
    Voxel ramp_v{MAT_GRANITE, 0};
    ramp_v.set_shape(SHAPE_RAMP_NORTH);
    chunk.set_voxel(1, 1, 1, ramp_v);

    auto mesh = GreedyMesher::MeshChunk(chunk);
    // 3 quads (18 vertices) + 2 triangles (6 vertices) = 24 vertices
    ASSERT_EQ(mesh.size(), 24);

    // Group triangles into unique coplanar faces
    struct Plane {
        glm::vec3 normal;
        float dist;
    };
    std::vector<Plane> unique_planes;

    for (size_t i = 0; i + 2 < mesh.size(); i += 3) {
        glm::vec3 v0 = mesh[i].position();
        glm::vec3 v1 = mesh[i + 1].position();
        glm::vec3 v2 = mesh[i + 2].position();
        glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        float d = glm::dot(n, v0);

        bool found = false;
        for (const auto& p : unique_planes) {
            if (glm::dot(n, p.normal) > 0.98f && std::abs(d - p.dist) < 0.05f) {
                found = true;
                break;
            }
        }
        if (!found) {
            unique_planes.push_back({n, d});
        }
    }

    ASSERT_EQ(unique_planes.size(), 5);
    std::cout << "[PASS] VoxelGeometry.RampSelfEnclosingFaceCount (Isolated ramp emitted exactly 5 bounding faces)" << std::endl;
}

// 3. World topology invariant test
TEST(VoxelTopology, NoOrphanOrSawtoothSlopes) {
    uint32_t test_seeds[3] = {1001, 2002, 3003};

    for (uint32_t seed : test_seeds) {
        World world(seed);
        world.generate_world(0, seed);

        int checked_ramps = 0;

        for (int cz = 0; cz < 3; ++cz) {
            for (int cy = 0; cy < 1; ++cy) {
                for (int cx = 0; cx < 3; ++cx) {
                    const Chunk* chunk = world.get_chunk(ChunkPos(cx, cy, cz));
                    if (!chunk) continue;

                    int bx = cx * CHUNK_SIZE;
                    int by = cy * CHUNK_SIZE;
                    int bz = cz * CHUNK_SIZE;

                    for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                        for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                            for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                                Voxel v = chunk->get_voxel(lx, ly, lz);
                                int wx = bx + lx;
                                int wy = by + ly;
                                int wz = bz + lz;

                                if (v.is_ramp()) {
                                    checked_ramps++;
                                    // 1. Solid Base Support: voxel directly below must be solid SHAPE_CUBE
                                    Voxel below = world.get_voxel(wx, wy - 1, wz);
                                    ASSERT_TRUE(below.is_solid());
                                    ASSERT_EQ(below.shape(), SHAPE_CUBE);

                                    // 2. No Opposing Sawtooth Peaks on directly adjacent cells
                                    if (v.shape() == SHAPE_RAMP_EAST) {
                                        Voxel n_east = world.get_voxel(wx + 1, wy, wz);
                                        ASSERT_TRUE(n_east.shape() != SHAPE_RAMP_WEST);
                                    } else if (v.shape() == SHAPE_RAMP_WEST) {
                                        Voxel n_west = world.get_voxel(wx - 1, wy, wz);
                                        ASSERT_TRUE(n_west.shape() != SHAPE_RAMP_EAST);
                                    } else if (v.shape() == SHAPE_RAMP_SOUTH) {
                                        Voxel n_south = world.get_voxel(wx, wy, wz + 1);
                                        ASSERT_TRUE(n_south.shape() != SHAPE_RAMP_NORTH);
                                    } else if (v.shape() == SHAPE_RAMP_NORTH) {
                                        Voxel n_north = world.get_voxel(wx, wy, wz - 1);
                                        ASSERT_TRUE(n_north.shape() != SHAPE_RAMP_SOUTH);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::cout << "[PASS] VoxelTopology.NoOrphanOrSawtoothSlopes (Seeds 1001, 2002, 3003 passed base support and sawtooth checks)" << std::endl;
}

// 4. Continuous ramp ascent physics test
TEST(PhysicsTraversal, ContinuousRampAscent) {
    World world(777);

    // Clear corridor for test bounding box
    for (int x = -2; x <= 8; ++x) {
        for (int z = -2; z <= 2; ++z) {
            for (int y = -2; y <= 10; ++y) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
            }
        }
    }

    // Construct 5 consecutive ascending SHAPE_RAMP_EAST blocks from X = 0..4, Y = 0..4
    for (int x = 0; x < 5; ++x) {
        // Foundation below each ramp
        for (int y = -1; y < x; ++y) {
            world.set_voxel(x, y, 0, Voxel{MAT_GRANITE, 0});
        }
        Voxel ramp{MAT_GRANITE, 0};
        ramp.set_shape(SHAPE_RAMP_EAST);
        world.set_voxel(x, x, 0, ramp);
    }
    // Top landing platform at Y = 4 (top surface at Y = 5.0)
    for (int x = 5; x <= 10; ++x) {
        for (int y = -1; y <= 4; ++y) {
            world.set_voxel(x, y, 0, Voxel{MAT_GRANITE, 0});
        }
    }

    PlayerController player(glm::vec3(0.0f, 0.9f, 0.0f));
    player.set_direction(glm::vec3(1.0f, 0.0f, 0.0f));
    player.set_velocity(glm::vec3(2.5f, 0.0f, 0.0f));
    player.add_button(BTN_FORWARD);

    float prev_y = player.position().y;

    for (int tick = 0; tick < 180; ++tick) {
        player.velocity_mut().x = 2.5f;
        player.Update(0.016f, world);

        float cur_y = player.position().y;

        // Monotonic Y increase
        ASSERT_GE(cur_y, prev_y - 0.001f);
        // No downward velocity spikes
        ASSERT_GE(player.velocity().y, -0.001f);
        // Grounded on every tick
        if (!player.is_grounded()) {
            std::cout << "Failed grounded at tick " << tick << " pos=(" << player.position().x << ", "
                      << player.position().y << ", " << player.position().z << ") vel=("
                      << player.velocity().x << ", " << player.velocity().y << ", " << player.velocity().z << ")" << std::endl;
        }
        ASSERT_TRUE(player.is_grounded());

        prev_y = cur_y;
    }

    ASSERT_GT(player.position().y, 3.5f);
    std::cout << "[PASS] PhysicsTraversal.ContinuousRampAscent (180 ticks monotonic ascent, vy >= 0.0 m/s, 100% grounded)" << std::endl;
}

// 5. Slab step transition physics test
TEST(PhysicsTraversal, SlabStepTransition) {
    World world(888);

    // Clear corridor for test bounding box
    for (int x = -2; x <= 6; ++x) {
        for (int z = -2; z <= 2; ++z) {
            for (int y = -2; y <= 6; ++y) {
                world.set_voxel(x, y, z, Voxel{MAT_AIR, 0});
            }
        }
    }

    // Sequence: [Cube Y=0] -> [SlabBottom Y=1] -> [Cube Y=1]
    world.set_voxel(0, 0, 0, Voxel{MAT_GRANITE, 0}); // Top at Y = 1.0

    world.set_voxel(1, 0, 0, Voxel{MAT_GRANITE, 0});
    Voxel slab{MAT_GRANITE, 0};
    slab.set_shape(SHAPE_SLAB_BOTTOM);
    world.set_voxel(1, 1, 0, slab); // Base at Y = 1.0, Top at Y = 1.5

    world.set_voxel(2, 0, 0, Voxel{MAT_GRANITE, 0});
    world.set_voxel(2, 1, 0, Voxel{MAT_GRANITE, 0}); // Base at Y = 1.0, Top at Y = 2.0

    // Start on Cube Y=0 (foot at 1.0, center at 1.9)
    PlayerController player(glm::vec3(0.1f, 1.9f, 0.0f));
    player.set_direction(glm::vec3(1.0f, 0.0f, 0.0f));
    player.add_button(BTN_FORWARD);

    float initial_foot = player.position().y - player.half_extents().y;
    ASSERT_NEAR(initial_foot, 1.0f, 0.05f);

    float max_slab_foot = 0.0f;
    float max_cube_foot = 0.0f;

    for (int tick = 0; tick < 80; ++tick) {
        player.velocity_mut().x = 1.5f;
        player.Update(0.016f, world);

        float foot_y = player.position().y - player.half_extents().y;
        ASSERT_FALSE(player.is_penetrating_solid(world));
        ASSERT_TRUE(player.is_grounded());

        if (player.position().x >= 1.0f && player.position().x <= 1.6f) {
            max_slab_foot = std::max(max_slab_foot, foot_y);
        }
        if (player.position().x >= 1.9f) {
            max_cube_foot = std::max(max_cube_foot, foot_y);
        }
    }

    ASSERT_NEAR(max_slab_foot, 1.5f, 0.08f);
    ASSERT_NEAR(max_cube_foot, 2.0f, 0.08f);
    std::cout << "[PASS] PhysicsTraversal.SlabStepTransition (Smooth 1.0 -> 1.5 -> 2.0 foot transition with zero penetration)" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " RUNNING VOXEL GEOMETRY & TRAVERSAL TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    VoxelGeometry_CubeAgainstRampWatertight();
    VoxelGeometry_RampSelfEnclosingFaceCount();
    VoxelTopology_NoOrphanOrSawtoothSlopes();
    PhysicsTraversal_ContinuousRampAscent();
    PhysicsTraversal_SlabStepTransition();

    std::cout << "========================================" << std::endl;
    std::cout << " ALL VOXEL GEOMETRY & TRAVERSAL TESTS PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
