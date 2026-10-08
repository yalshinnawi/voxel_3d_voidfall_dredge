#include "greedy_mesher.hpp"
#include <array>
#include <algorithm>

namespace Voidfall {

uint8_t GreedyMesher::compute_vertex_ao(bool s1, bool s2, bool c) {
    if (s1 && s2) {
        return 3; // Fully occluded corner
    }
    return static_cast<uint8_t>((s1 ? 1 : 0) + (s2 ? 1 : 0) + (c ? 1 : 0));
}

Voxel GreedyMesher::sample_voxel(
    const Chunk& chunk,
    const NeighborChunkGetter& get_neighbor,
    int x, int y, int z
) {
    if (Chunk::in_bounds(x, y, z)) {
        return chunk.get_voxel(x, y, z);
    }

    if (!get_neighbor) {
        return Voxel{MAT_FRACTURED_GRANITE, 0};
    }

    ChunkPos pos = chunk.get_pos();
    int nx = x;
    int ny = y;
    int nz = z;

    if (nx < 0) {
        pos.x -= 1;
        nx += CHUNK_SIZE;
    } else if (nx >= CHUNK_SIZE) {
        pos.x += 1;
        nx -= CHUNK_SIZE;
    }

    if (ny < 0) {
        pos.y -= 1;
        ny += CHUNK_SIZE;
    } else if (ny >= CHUNK_SIZE) {
        pos.y += 1;
        ny -= CHUNK_SIZE;
    }

    if (nz < 0) {
        pos.z -= 1;
        nz += CHUNK_SIZE;
    } else if (nz >= CHUNK_SIZE) {
        pos.z += 1;
        nz -= CHUNK_SIZE;
    }

    const Chunk* neighbor = get_neighbor(pos);
    if (neighbor) {
        return neighbor->get_voxel(nx, ny, nz);
    }

    // Treat unloaded neighbor boundaries as solid granite (SHAPE_CUBE) to prevent void leaks and seam culling
    return Voxel{MAT_FRACTURED_GRANITE, 0};
}

glm::vec3 GreedyMesher::calculate_diagonal_normal(VoxelShape shape) {
    glm::vec3 n_base(0.0f, 1.0f, 0.0f);
    glm::vec3 n_side(0.0f);
    switch (shape) {
        case SHAPE_RAMP_EAST: // POS_X
            n_side = glm::vec3(-1.0f, 0.0f, 0.0f);
            break;
        case SHAPE_RAMP_WEST: // NEG_X
            n_side = glm::vec3( 1.0f, 0.0f, 0.0f);
            break;
        case SHAPE_RAMP_SOUTH: // POS_Z
            n_side = glm::vec3( 0.0f, 0.0f, -1.0f);
            break;
        case SHAPE_RAMP_NORTH: // NEG_Z
            n_side = glm::vec3( 0.0f, 0.0f,  1.0f);
            break;
        default:
            return n_base;
    }
    return glm::normalize(n_base + n_side);
}

static inline uint32_t get_emissive_intensity(uint32_t mat_id) {
    switch (mat_id) {
        case MAT_VOIDITE_CRYSTAL: return 220;
        case MAT_THERMITE_SLAG: return 255;
        case MAT_RADIOACTIVE_ORE: return 180;
        case MAT_PRISMATIC_CRYSTAL: return 210;
        case MAT_BIOLUMINESCENT_FLORA: return 175;
        case MAT_OBSIDIAN_SPIKES: return 190;
        case MAT_CRYSTAL_AQUIFER: return 80;
        default: return 0;
    }
}

static inline bool is_face_occluded(Voxel current, Voxel neighbor) {
    if (!neighbor.is_renderable()) {
        return false;
    }
    if (current.is_liquid()) {
        return neighbor.material_id == current.material_id || neighbor.is_solid();
    }
    // Solid voxels: a full cube is ONLY occluded if neighbor provides a 100% flush face (solid cube)
    if (current.shape() == SHAPE_CUBE) {
        return neighbor.is_solid() && (neighbor.shape() == SHAPE_CUBE);
    }
    return neighbor.is_solid();
}

bool GreedyMesher::is_face_visible(
    const Chunk& chunk,
    const NeighborChunkGetter& get_neighbor,
    int x, int y, int z,
    int nx, int ny, int nz
) {
    Voxel current = sample_voxel(chunk, get_neighbor, x, y, z);
    if (!current.is_renderable()) {
        return false;
    }
    Voxel neighbor = sample_voxel(chunk, get_neighbor, nx, ny, nz);
    return !is_face_occluded(current, neighbor);
}

std::vector<PackedVoxelVertex> GreedyMesher::generate_mesh(
    const Chunk& chunk,
    const NeighborChunkGetter& get_neighbor
) {
    std::vector<PackedVoxelVertex> vertices;
    if (chunk.is_empty()) {
        return vertices;
    }

    vertices.reserve(2048);

    struct FaceQuad {
        Voxel voxel;
        uint8_t ao[4]; // 00, 10, 11, 01
        bool visible{false};
    };

    // ─────────────────────────────────────────────────────────────
    // PART 1: 3-Axis Greedy Quad Meshing for SHAPE_CUBE Blocks
    // ─────────────────────────────────────────────────────────────
    for (int d = 0; d < 3; ++d) {
        int u = (d + 1) % 3;
        int v = (d + 2) % 3;

        int x[3] = {0, 0, 0};
        int q[3] = {0, 0, 0};

        std::array<FaceQuad, CHUNK_SIZE * CHUNK_SIZE> mask;

        for (x[d] = 0; x[d] < CHUNK_SIZE; ++x[d]) {
            for (int face_dir = 0; face_dir < 2; ++face_dir) {
                int norm_idx = (d * 2) + face_dir;
                int dir_step = (face_dir == 0) ? 1 : -1;

                q[0] = 0; q[1] = 0; q[2] = 0;
                q[d] = dir_step;

                // 1. Generate face visibility mask for current slice
                for (x[v] = 0; x[v] < CHUNK_SIZE; ++x[v]) {
                    for (x[u] = 0; x[u] < CHUNK_SIZE; ++x[u]) {
                        int mask_idx = x[u] + x[v] * CHUNK_SIZE;
                        mask[mask_idx].visible = false;

                        Voxel current = chunk.get_voxel(x[0], x[1], x[2]);
                        if (!current.is_renderable() || current.shape() != SHAPE_CUBE || current.is_liquid()) {
                            continue;
                        }

                        int nx = x[0] + q[0];
                        int ny = x[1] + q[1];
                        int nz = x[2] + q[2];
                        Voxel neighbor = Chunk::in_bounds(nx, ny, nz)
                            ? chunk.get_voxel(nx, ny, nz)
                            : sample_voxel(chunk, get_neighbor, nx, ny, nz);

                        // Boundary Face Culling Rule:
                        // A face may ONLY be culled if the neighbor cell is opaque AND provides
                        // a 100% coplanar, flush surface across that entire boundary quad.
                        // If a full cube touches a SHAPE_RAMP_*, SHAPE_SLAB_*, or SHAPE_CORNER_*,
                        // the full cube must render its full boundary quad facing that cell!
                        if (current.is_liquid()) {
                            if (neighbor.material_id == current.material_id || neighbor.is_solid()) {
                                continue;
                            }
                        } else {
                            if (neighbor.is_solid() && neighbor.shape() == SHAPE_CUBE) {
                                continue;
                            }
                        }

                        mask[mask_idx].visible = true;
                        mask[mask_idx].voxel = current;

                        // Calculate Baked Vertex AO for the 4 corners
                        int tu[3] = {0, 0, 0}; tu[u] = 1;
                        int tv[3] = {0, 0, 0}; tv[v] = 1;

                        int bx = x[0] + q[0];
                        int by = x[1] + q[1];
                        int bz = x[2] + q[2];

                        bool s_left   = sample_voxel(chunk, get_neighbor, bx - tu[0], by - tu[1], bz - tu[2]).is_solid();
                        bool s_right  = sample_voxel(chunk, get_neighbor, bx + tu[0], by + tu[1], bz + tu[2]).is_solid();
                        bool s_down   = sample_voxel(chunk, get_neighbor, bx - tv[0], by - tv[1], bz - tv[2]).is_solid();
                        bool s_up     = sample_voxel(chunk, get_neighbor, bx + tv[0], by + tv[1], bz + tv[2]).is_solid();

                        bool c_ld = sample_voxel(chunk, get_neighbor, bx - tu[0] - tv[0], by - tu[1] - tv[1], bz - tu[2] - tv[2]).is_solid();
                        bool c_rd = sample_voxel(chunk, get_neighbor, bx + tu[0] - tv[0], by + tu[1] - tv[1], bz + tu[2] - tv[2]).is_solid();
                        bool c_ru = sample_voxel(chunk, get_neighbor, bx + tu[0] + tv[0], by + tu[1] + tv[1], bz + tu[2] + tv[2]).is_solid();
                        bool c_lu = sample_voxel(chunk, get_neighbor, bx - tu[0] + tv[0], by - tu[1] + tv[1], bz - tu[2] + tv[2]).is_solid();

                        mask[mask_idx].ao[0] = compute_vertex_ao(s_left, s_down, c_ld);
                        mask[mask_idx].ao[1] = compute_vertex_ao(s_right, s_down, c_rd);
                        mask[mask_idx].ao[2] = compute_vertex_ao(s_right, s_up, c_ru);
                        mask[mask_idx].ao[3] = compute_vertex_ao(s_left, s_up, c_lu);
                    }
                }

                // 2. Greedy merge quads in mask
                for (int j = 0; j < CHUNK_SIZE; ++j) {
                    for (int i = 0; i < CHUNK_SIZE; ) {
                        int idx = i + j * CHUNK_SIZE;
                        if (!mask[idx].visible) {
                            ++i;
                            continue;
                        }

                        FaceQuad root = mask[idx];
                        int width = 1;
                        while (i + width < CHUNK_SIZE) {
                            int next_idx = (i + width) + j * CHUNK_SIZE;
                            const FaceQuad& next = mask[next_idx];
                            if (!next.visible ||
                                next.voxel.material_id != root.voxel.material_id ||
                                next.voxel.shape() != root.voxel.shape() ||
                                next.voxel.damage() != root.voxel.damage() ||
                                next.ao[0] != root.ao[0] || next.ao[1] != root.ao[1] ||
                                next.ao[2] != root.ao[2] || next.ao[3] != root.ao[3]) {
                                break;
                            }
                            ++width;
                        }

                        int height = 1;
                        bool can_expand_v = true;
                        while (j + height < CHUNK_SIZE && can_expand_v) {
                            for (int k = 0; k < width; ++k) {
                                int next_idx = (i + k) + (j + height) * CHUNK_SIZE;
                                const FaceQuad& next = mask[next_idx];
                                if (!next.visible ||
                                    next.voxel.material_id != root.voxel.material_id ||
                                    next.voxel.shape() != root.voxel.shape() ||
                                    next.voxel.damage() != root.voxel.damage() ||
                                    next.ao[0] != root.ao[0] || next.ao[1] != root.ao[1] ||
                                    next.ao[2] != root.ao[2] || next.ao[3] != root.ao[3]) {
                                    can_expand_v = false;
                                    break;
                                }
                            }
                            if (can_expand_v) {
                                ++height;
                            }
                        }

                        for (int dy = 0; dy < height; ++dy) {
                            for (int dx = 0; dx < width; ++dx) {
                                mask[(i + dx) + (j + dy) * CHUNK_SIZE].visible = false;
                            }
                        }

                        int p[3];
                        p[d] = x[d] + (face_dir == 0 ? 1 : 0);
                        p[u] = i;
                        p[v] = j;

                        int du[3] = {0, 0, 0}; du[u] = width;
                        int dv[3] = {0, 0, 0}; dv[v] = height;

                        uint32_t mat_id = root.voxel.material_id;
                        uint32_t damage = root.voxel.damage();
                        uint32_t emissive = get_emissive_intensity(mat_id);
                        uint32_t aux = root.voxel.is_highlighted() ? 1 : 0;

                        uint8_t ao0 = mask[i + j * CHUNK_SIZE].ao[0];
                        uint8_t ao1 = mask[(i + width - 1) + j * CHUNK_SIZE].ao[1];
                        uint8_t ao2 = mask[(i + width - 1) + (j + height - 1) * CHUNK_SIZE].ao[2];
                        uint8_t ao3 = mask[i + (j + height - 1) * CHUNK_SIZE].ao[3];

                        PackedVoxelVertex vert0 = PackedVoxelVertex::encode(
                            p[0], p[1], p[2], norm_idx, ao0, mat_id, width, height, 0, damage, emissive, aux
                        );
                        PackedVoxelVertex vert1 = PackedVoxelVertex::encode(
                            p[0] + du[0], p[1] + du[1], p[2] + du[2], norm_idx, ao1, mat_id, width, height, 1, damage, emissive, aux
                        );
                        PackedVoxelVertex vert2 = PackedVoxelVertex::encode(
                            p[0] + du[0] + dv[0], p[1] + du[1] + dv[1], p[2] + du[2] + dv[2], norm_idx, ao2, mat_id, width, height, 2, damage, emissive, aux
                        );
                        PackedVoxelVertex vert3 = PackedVoxelVertex::encode(
                            p[0] + dv[0], p[1] + dv[1], p[2] + dv[2], norm_idx, ao3, mat_id, width, height, 3, damage, emissive, aux
                        );

                        bool flip_diag = (ao0 + ao2) < (ao1 + ao3);
                        if (face_dir == 0) {
                            if (flip_diag) {
                                vertices.push_back(vert0); vertices.push_back(vert1); vertices.push_back(vert2);
                                vertices.push_back(vert0); vertices.push_back(vert2); vertices.push_back(vert3);
                            } else {
                                vertices.push_back(vert1); vertices.push_back(vert2); vertices.push_back(vert3);
                                vertices.push_back(vert1); vertices.push_back(vert3); vertices.push_back(vert0);
                            }
                        } else {
                            if (flip_diag) {
                                vertices.push_back(vert0); vertices.push_back(vert2); vertices.push_back(vert1);
                                vertices.push_back(vert0); vertices.push_back(vert3); vertices.push_back(vert2);
                            } else {
                                vertices.push_back(vert1); vertices.push_back(vert3); vertices.push_back(vert2);
                                vertices.push_back(vert1); vertices.push_back(vert0); vertices.push_back(vert3);
                            }
                        }

                        i += width;
                    }
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // PART 2: Watertight Self-Enclosing Meshing for SHAPE_RAMP_*
    // ─────────────────────────────────────────────────────────────
    // Helper lambda to emit a quad (2 triangles / 6 vertices)
    auto emit_quad = [&](
        const glm::ivec3& p0, const glm::ivec3& p1, const glm::ivec3& p2, const glm::ivec3& p3,
        uint32_t norm_idx, uint32_t mat_id, uint32_t u_dim, uint32_t v_dim,
        uint32_t damage, uint32_t emissive, uint32_t aux, uint32_t sub_y_half = 0
    ) {
        PackedVoxelVertex v0 = PackedVoxelVertex::encode(p0.x, p0.y, p0.z, norm_idx, 0, mat_id, u_dim, v_dim, 0, damage, emissive, aux, sub_y_half);
        PackedVoxelVertex v1 = PackedVoxelVertex::encode(p1.x, p1.y, p1.z, norm_idx, 0, mat_id, u_dim, v_dim, 1, damage, emissive, aux, sub_y_half);
        PackedVoxelVertex v2 = PackedVoxelVertex::encode(p2.x, p2.y, p2.z, norm_idx, 0, mat_id, u_dim, v_dim, 2, damage, emissive, aux, sub_y_half);
        PackedVoxelVertex v3 = PackedVoxelVertex::encode(p3.x, p3.y, p3.z, norm_idx, 0, mat_id, u_dim, v_dim, 3, damage, emissive, aux, sub_y_half);

        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
    };

    // Helper lambda to emit a triangular side cap (1 triangle / 3 vertices)
    auto emit_triangle = [&](
        const glm::ivec3& p0, const glm::ivec3& p1, const glm::ivec3& p2,
        uint32_t norm_idx, uint32_t mat_id,
        uint32_t damage, uint32_t emissive, uint32_t aux
    ) {
        PackedVoxelVertex v0 = PackedVoxelVertex::encode(p0.x, p0.y, p0.z, norm_idx, 0, mat_id, 1, 1, 0, damage, emissive, aux);
        PackedVoxelVertex v1 = PackedVoxelVertex::encode(p1.x, p1.y, p1.z, norm_idx, 0, mat_id, 1, 1, 1, damage, emissive, aux);
        PackedVoxelVertex v2 = PackedVoxelVertex::encode(p2.x, p2.y, p2.z, norm_idx, 0, mat_id, 1, 1, 2, damage, emissive, aux);

        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
    };

    // Pass 2A: Ramps along X (SHAPE_RAMP_EAST, SHAPE_RAMP_WEST) - Perpendicular axis Z
    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ) {
                Voxel cur = chunk.get_voxel(x, y, z);
                VoxelShape s = cur.shape();
                if (s != SHAPE_RAMP_EAST && s != SHAPE_RAMP_WEST) {
                    ++z;
                    continue;
                }

                // Headroom check: solid cube directly above occludes hypotenuse
                Voxel above = sample_voxel(chunk, get_neighbor, x, y + 1, z);
                bool above_solid = above.is_solid() && (above.shape() == SHAPE_CUBE);

                // Run-length greedy merging along perpendicular axis Z
                int width = 1;
                while (z + width < CHUNK_SIZE) {
                    Voxel next_v = chunk.get_voxel(x, y, z + width);
                    if (next_v.shape() != s ||
                        next_v.material_id != cur.material_id ||
                        next_v.damage() != cur.damage()) {
                        break;
                    }
                    Voxel next_above = sample_voxel(chunk, get_neighbor, x, y + 1, z + width);
                    if (next_above.is_solid() && next_above.shape() == SHAPE_CUBE) {
                        break;
                    }
                    ++width;
                }

                uint32_t mat_id = cur.material_id;
                uint32_t damage = cur.damage();
                uint32_t emissive = get_emissive_intensity(mat_id);
                uint32_t aux = cur.is_highlighted() ? 1 : 0;

                // 1. Hypotenuse Quad (45° diagonal plane)
                if (!above_solid) {
                    if (s == SHAPE_RAMP_EAST) {
                        emit_quad(
                            glm::ivec3(x,     y,     z),
                            glm::ivec3(x,     y,     z + width),
                            glm::ivec3(x + 1, y + 1, z + width),
                            glm::ivec3(x + 1, y + 1, z),
                            2, mat_id, width, 1, damage, emissive, aux
                        );
                    } else { // SHAPE_RAMP_WEST
                        emit_quad(
                            glm::ivec3(x + 1, y,     z + width),
                            glm::ivec3(x + 1, y,     z),
                            glm::ivec3(x,     y + 1, z),
                            glm::ivec3(x,     y + 1, z + width),
                            2, mat_id, width, 1, damage, emissive, aux
                        );
                    }
                }

                // 2. Floor Base Quad (facing -Y)
                for (int k = 0; k < width; ++k) {
                    Voxel below = sample_voxel(chunk, get_neighbor, x, y - 1, z + k);
                    bool below_flush = below.is_solid() && (below.shape() == SHAPE_CUBE || below.shape() == SHAPE_SLAB_TOP);
                    if (!below_flush) {
                        emit_quad(
                            glm::ivec3(x,     y, z + k),
                            glm::ivec3(x + 1, y, z + k),
                            glm::ivec3(x + 1, y, z + k + 1),
                            glm::ivec3(x,     y, z + k + 1),
                            3, mat_id, 1, 1, damage, emissive, aux
                        );
                    }
                }

                // 3. Vertical Back Quad (high end)
                for (int k = 0; k < width; ++k) {
                    if (s == SHAPE_RAMP_EAST) {
                        Voxel back_n = sample_voxel(chunk, get_neighbor, x + 1, y, z + k);
                        bool back_flush = back_n.is_solid() && (back_n.shape() == SHAPE_CUBE);
                        if (!back_flush) {
                            emit_quad(
                                glm::ivec3(x + 1, y,     z + k + 1),
                                glm::ivec3(x + 1, y,     z + k),
                                glm::ivec3(x + 1, y + 1, z + k),
                                glm::ivec3(x + 1, y + 1, z + k + 1),
                                0, mat_id, 1, 1, damage, emissive, aux
                            );
                        }
                    } else { // SHAPE_RAMP_WEST
                        Voxel back_n = sample_voxel(chunk, get_neighbor, x - 1, y, z + k);
                        bool back_flush = back_n.is_solid() && (back_n.shape() == SHAPE_CUBE);
                        if (!back_flush) {
                            emit_quad(
                                glm::ivec3(x, y,     z + k),
                                glm::ivec3(x, y,     z + k + 1),
                                glm::ivec3(x, y + 1, z + k + 1),
                                glm::ivec3(x, y + 1, z + k),
                                1, mat_id, 1, 1, damage, emissive, aux
                            );
                        }
                    }
                }

                // 4. Two Triangular Side Walls
                // Flank at start (-Z)
                Voxel side_neg = sample_voxel(chunk, get_neighbor, x, y, z - 1);
                bool side_neg_flush = side_neg.is_solid() && (side_neg.shape() == SHAPE_CUBE || side_neg.shape() == s);
                if (!side_neg_flush) {
                    if (s == SHAPE_RAMP_EAST) {
                        emit_triangle(
                            glm::ivec3(x,     y,     z),
                            glm::ivec3(x + 1, y + 1, z),
                            glm::ivec3(x + 1, y,     z),
                            5, mat_id, damage, emissive, aux
                        );
                    } else { // SHAPE_RAMP_WEST
                        emit_triangle(
                            glm::ivec3(x + 1, y,     z),
                            glm::ivec3(x,     y,     z),
                            glm::ivec3(x,     y + 1, z),
                            5, mat_id, damage, emissive, aux
                        );
                    }
                }

                // Flank at end (+Z)
                Voxel side_pos = sample_voxel(chunk, get_neighbor, x, y, z + width);
                bool side_pos_flush = side_pos.is_solid() && (side_pos.shape() == SHAPE_CUBE || side_pos.shape() == s);
                if (!side_pos_flush) {
                    if (s == SHAPE_RAMP_EAST) {
                        emit_triangle(
                            glm::ivec3(x,     y,     z + width),
                            glm::ivec3(x + 1, y,     z + width),
                            glm::ivec3(x + 1, y + 1, z + width),
                            4, mat_id, damage, emissive, aux
                        );
                    } else { // SHAPE_RAMP_WEST
                        emit_triangle(
                            glm::ivec3(x + 1, y,     z + width),
                            glm::ivec3(x,     y + 1, z + width),
                            glm::ivec3(x,     y,     z + width),
                            4, mat_id, damage, emissive, aux
                        );
                    }
                }

                z += width;
            }
        }
    }

    // Pass 2B: Ramps along Z (SHAPE_RAMP_SOUTH, SHAPE_RAMP_NORTH) - Perpendicular axis X
    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ) {
                Voxel cur = chunk.get_voxel(x, y, z);
                VoxelShape s = cur.shape();
                if (s != SHAPE_RAMP_SOUTH && s != SHAPE_RAMP_NORTH) {
                    ++x;
                    continue;
                }

                // Headroom check
                Voxel above = sample_voxel(chunk, get_neighbor, x, y + 1, z);
                bool above_solid = above.is_solid() && (above.shape() == SHAPE_CUBE);

                // Run-length greedy merging along perpendicular axis X
                int width = 1;
                while (x + width < CHUNK_SIZE) {
                    Voxel next_v = chunk.get_voxel(x + width, y, z);
                    if (next_v.shape() != s ||
                        next_v.material_id != cur.material_id ||
                        next_v.damage() != cur.damage()) {
                        break;
                    }
                    Voxel next_above = sample_voxel(chunk, get_neighbor, x + width, y + 1, z);
                    if (next_above.is_solid() && next_above.shape() == SHAPE_CUBE) {
                        break;
                    }
                    ++width;
                }

                uint32_t mat_id = cur.material_id;
                uint32_t damage = cur.damage();
                uint32_t emissive = get_emissive_intensity(mat_id);
                uint32_t aux = cur.is_highlighted() ? 1 : 0;

                // 1. Hypotenuse Quad
                if (!above_solid) {
                    if (s == SHAPE_RAMP_SOUTH) {
                        emit_quad(
                            glm::ivec3(x + width, y,     z),
                            glm::ivec3(x,         y,     z),
                            glm::ivec3(x,         y + 1, z + 1),
                            glm::ivec3(x + width, y + 1, z + 1),
                            2, mat_id, width, 1, damage, emissive, aux
                        );
                    } else { // SHAPE_RAMP_NORTH
                        emit_quad(
                            glm::ivec3(x,         y,     z + 1),
                            glm::ivec3(x + width, y,     z + 1),
                            glm::ivec3(x + width, y + 1, z),
                            glm::ivec3(x,         y + 1, z),
                            2, mat_id, width, 1, damage, emissive, aux
                        );
                    }
                }

                // 2. Floor Base Quad (facing -Y)
                for (int k = 0; k < width; ++k) {
                    Voxel below = sample_voxel(chunk, get_neighbor, x + k, y - 1, z);
                    bool below_flush = below.is_solid() && (below.shape() == SHAPE_CUBE || below.shape() == SHAPE_SLAB_TOP);
                    if (!below_flush) {
                        emit_quad(
                            glm::ivec3(x + k,     y, z),
                            glm::ivec3(x + k + 1, y, z),
                            glm::ivec3(x + k + 1, y, z + 1),
                            glm::ivec3(x + k,     y, z + 1),
                            3, mat_id, 1, 1, damage, emissive, aux
                        );
                    }
                }

                // 3. Vertical Back Quad (high end)
                for (int k = 0; k < width; ++k) {
                    if (s == SHAPE_RAMP_SOUTH) {
                        Voxel back_n = sample_voxel(chunk, get_neighbor, x + k, y, z + 1);
                        bool back_flush = back_n.is_solid() && (back_n.shape() == SHAPE_CUBE);
                        if (!back_flush) {
                            emit_quad(
                                glm::ivec3(x + k,     y,     z + 1),
                                glm::ivec3(x + k + 1, y,     z + 1),
                                glm::ivec3(x + k + 1, y + 1, z + 1),
                                glm::ivec3(x + k,     y + 1, z + 1),
                                4, mat_id, 1, 1, damage, emissive, aux
                            );
                        }
                    } else { // SHAPE_RAMP_NORTH
                        Voxel back_n = sample_voxel(chunk, get_neighbor, x + k, y, z - 1);
                        bool back_flush = back_n.is_solid() && (back_n.shape() == SHAPE_CUBE);
                        if (!back_flush) {
                            emit_quad(
                                glm::ivec3(x + k + 1, y,     z),
                                glm::ivec3(x + k,     y,     z),
                                glm::ivec3(x + k,     y + 1, z),
                                glm::ivec3(x + k + 1, y + 1, z),
                                5, mat_id, 1, 1, damage, emissive, aux
                            );
                        }
                    }
                }

                // 4. Two Triangular Side Walls
                // Flank at start (-X)
                Voxel side_neg = sample_voxel(chunk, get_neighbor, x - 1, y, z);
                bool side_neg_flush = side_neg.is_solid() && (side_neg.shape() == SHAPE_CUBE || side_neg.shape() == s);
                if (!side_neg_flush) {
                    if (s == SHAPE_RAMP_SOUTH) {
                        emit_triangle(
                            glm::ivec3(x, y,     z),
                            glm::ivec3(x, y,     z + 1),
                            glm::ivec3(x, y + 1, z + 1),
                            1, mat_id, damage, emissive, aux
                        );
                    } else { // SHAPE_RAMP_NORTH
                        emit_triangle(
                            glm::ivec3(x, y,     z + 1),
                            glm::ivec3(x, y + 1, z),
                            glm::ivec3(x, y,     z),
                            1, mat_id, damage, emissive, aux
                        );
                    }
                }

                // Flank at end (+X)
                Voxel side_pos = sample_voxel(chunk, get_neighbor, x + width, y, z);
                bool side_pos_flush = side_pos.is_solid() && (side_pos.shape() == SHAPE_CUBE || side_pos.shape() == s);
                if (!side_pos_flush) {
                    if (s == SHAPE_RAMP_SOUTH) {
                        emit_triangle(
                            glm::ivec3(x + width, y,     z),
                            glm::ivec3(x + width, y + 1, z + 1),
                            glm::ivec3(x + width, y,     z + width > 1 ? z + 1 : z + 1),
                            0, mat_id, damage, emissive, aux
                        );
                    } else { // SHAPE_RAMP_NORTH
                        emit_triangle(
                            glm::ivec3(x + width, y,     z + 1),
                            glm::ivec3(x + width, y,     z),
                            glm::ivec3(x + width, y + 1, z),
                            0, mat_id, damage, emissive, aux
                        );
                    }
                }

                x += width;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // PART 3: Meshing SHAPE_SLAB_BOTTOM and SHAPE_SLAB_TOP
    // ─────────────────────────────────────────────────────────────
    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                Voxel cur = chunk.get_voxel(x, y, z);
                VoxelShape s = cur.shape();
                if (s != SHAPE_SLAB_BOTTOM && s != SHAPE_SLAB_TOP) {
                    continue;
                }

                uint32_t mat_id = cur.material_id;
                uint32_t damage = cur.damage();
                uint32_t emissive = get_emissive_intensity(mat_id);
                uint32_t aux = cur.is_highlighted() ? 1 : 0;

                if (s == SHAPE_SLAB_BOTTOM) {
                    // Top quad at Y = 0.5 (encoded with Y = y + 1, sub_y_half = 1)
                    Voxel above = sample_voxel(chunk, get_neighbor, x, y + 1, z);
                    bool above_flush = above.is_solid() && (above.shape() == SHAPE_CUBE || above.shape() == SHAPE_SLAB_TOP);
                    if (!above_flush) {
                        emit_quad(
                            glm::ivec3(x,     y + 1, z),
                            glm::ivec3(x,     y + 1, z + 1),
                            glm::ivec3(x + 1, y + 1, z + 1),
                            glm::ivec3(x + 1, y + 1, z),
                            2, mat_id, 1, 1, damage, emissive, aux, 1 /* sub_y_half = 1 */
                        );
                    }

                    // Bottom quad at Y = 0.0 (sub_y_half = 0)
                    Voxel below = sample_voxel(chunk, get_neighbor, x, y - 1, z);
                    bool below_flush = below.is_solid() && (below.shape() == SHAPE_CUBE || below.shape() == SHAPE_SLAB_TOP);
                    if (!below_flush) {
                        emit_quad(
                            glm::ivec3(x,     y, z),
                            glm::ivec3(x + 1, y, z),
                            glm::ivec3(x + 1, y, z + 1),
                            glm::ivec3(x,     y, z + 1),
                            3, mat_id, 1, 1, damage, emissive, aux, 0
                        );
                    }

                    // 4 Side quads of height 0.5m:
                    // +X side:
                    Voxel n_px = sample_voxel(chunk, get_neighbor, x + 1, y, z);
                    if (!n_px.is_solid() || (n_px.shape() != SHAPE_CUBE && n_px.shape() != SHAPE_SLAB_BOTTOM)) {
                        PackedVoxelVertex v0 = PackedVoxelVertex::encode(x + 1, y,     z + 1, 0, 0, mat_id, 1, 1, 0, damage, emissive, aux, 0);
                        PackedVoxelVertex v1 = PackedVoxelVertex::encode(x + 1, y,     z,     0, 0, mat_id, 1, 1, 1, damage, emissive, aux, 0);
                        PackedVoxelVertex v2 = PackedVoxelVertex::encode(x + 1, y + 1, z,     0, 0, mat_id, 1, 1, 2, damage, emissive, aux, 1);
                        PackedVoxelVertex v3 = PackedVoxelVertex::encode(x + 1, y + 1, z + 1, 0, 0, mat_id, 1, 1, 3, damage, emissive, aux, 1);
                        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
                        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
                    }

                    // -X side:
                    Voxel n_nx = sample_voxel(chunk, get_neighbor, x - 1, y, z);
                    if (!n_nx.is_solid() || (n_nx.shape() != SHAPE_CUBE && n_nx.shape() != SHAPE_SLAB_BOTTOM)) {
                        PackedVoxelVertex v0 = PackedVoxelVertex::encode(x, y,     z,     1, 0, mat_id, 1, 1, 0, damage, emissive, aux, 0);
                        PackedVoxelVertex v1 = PackedVoxelVertex::encode(x, y,     z + 1, 1, 0, mat_id, 1, 1, 1, damage, emissive, aux, 0);
                        PackedVoxelVertex v2 = PackedVoxelVertex::encode(x, y + 1, z + 1, 1, 0, mat_id, 1, 1, 2, damage, emissive, aux, 1);
                        PackedVoxelVertex v3 = PackedVoxelVertex::encode(x, y + 1, z,     1, 0, mat_id, 1, 1, 3, damage, emissive, aux, 1);
                        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
                        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
                    }

                    // +Z side:
                    Voxel n_pz = sample_voxel(chunk, get_neighbor, x, y, z + 1);
                    if (!n_pz.is_solid() || (n_pz.shape() != SHAPE_CUBE && n_pz.shape() != SHAPE_SLAB_BOTTOM)) {
                        PackedVoxelVertex v0 = PackedVoxelVertex::encode(x,     y,     z + 1, 4, 0, mat_id, 1, 1, 0, damage, emissive, aux, 0);
                        PackedVoxelVertex v1 = PackedVoxelVertex::encode(x + 1, y,     z + 1, 4, 0, mat_id, 1, 1, 1, damage, emissive, aux, 0);
                        PackedVoxelVertex v2 = PackedVoxelVertex::encode(x + 1, y + 1, z + 1, 4, 0, mat_id, 1, 1, 2, damage, emissive, aux, 1);
                        PackedVoxelVertex v3 = PackedVoxelVertex::encode(x,     y + 1, z + 1, 4, 0, mat_id, 1, 1, 3, damage, emissive, aux, 1);
                        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
                        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
                    }

                    // -Z side:
                    Voxel n_nz = sample_voxel(chunk, get_neighbor, x, y, z - 1);
                    if (!n_nz.is_solid() || (n_nz.shape() != SHAPE_CUBE && n_nz.shape() != SHAPE_SLAB_BOTTOM)) {
                        PackedVoxelVertex v0 = PackedVoxelVertex::encode(x + 1, y,     z, 5, 0, mat_id, 1, 1, 0, damage, emissive, aux, 0);
                        PackedVoxelVertex v1 = PackedVoxelVertex::encode(x,     y,     z, 5, 0, mat_id, 1, 1, 1, damage, emissive, aux, 0);
                        PackedVoxelVertex v2 = PackedVoxelVertex::encode(x,     y + 1, z, 5, 0, mat_id, 1, 1, 2, damage, emissive, aux, 1);
                        PackedVoxelVertex v3 = PackedVoxelVertex::encode(x + 1, y + 1, z, 5, 0, mat_id, 1, 1, 3, damage, emissive, aux, 1);
                        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
                        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
                    }
                } else { // SHAPE_SLAB_TOP
                    // Top quad at Y = 1.0 (sub_y_half = 0)
                    Voxel above = sample_voxel(chunk, get_neighbor, x, y + 1, z);
                    bool above_flush = above.is_solid() && (above.shape() == SHAPE_CUBE || above.shape() == SHAPE_SLAB_BOTTOM);
                    if (!above_flush) {
                        emit_quad(
                            glm::ivec3(x,     y + 1, z),
                            glm::ivec3(x,     y + 1, z + 1),
                            glm::ivec3(x + 1, y + 1, z + 1),
                            glm::ivec3(x + 1, y + 1, z),
                            2, mat_id, 1, 1, damage, emissive, aux, 0
                        );
                    }

                    // Bottom quad at Y = 0.5 (encoded as Y = y + 1, sub_y_half = 1)
                    Voxel below = sample_voxel(chunk, get_neighbor, x, y - 1, z);
                    bool below_flush = below.is_solid() && (below.shape() == SHAPE_CUBE || below.shape() == SHAPE_SLAB_BOTTOM);
                    if (!below_flush) {
                        emit_quad(
                            glm::ivec3(x,     y + 1, z),
                            glm::ivec3(x + 1, y + 1, z),
                            glm::ivec3(x + 1, y + 1, z + 1),
                            glm::ivec3(x,     y + 1, z + 1),
                            3, mat_id, 1, 1, damage, emissive, aux, 1
                        );
                    }

                    // 4 Side quads along upper [0.5, 1.0]:
                    // +X side:
                    Voxel n_px = sample_voxel(chunk, get_neighbor, x + 1, y, z);
                    if (!n_px.is_solid() || (n_px.shape() != SHAPE_CUBE && n_px.shape() != SHAPE_SLAB_TOP)) {
                        PackedVoxelVertex v0 = PackedVoxelVertex::encode(x + 1, y + 1, z + 1, 0, 0, mat_id, 1, 1, 0, damage, emissive, aux, 1);
                        PackedVoxelVertex v1 = PackedVoxelVertex::encode(x + 1, y + 1, z,     0, 0, mat_id, 1, 1, 1, damage, emissive, aux, 1);
                        PackedVoxelVertex v2 = PackedVoxelVertex::encode(x + 1, y + 1, z,     0, 0, mat_id, 1, 1, 2, damage, emissive, aux, 0);
                        PackedVoxelVertex v3 = PackedVoxelVertex::encode(x + 1, y + 1, z + 1, 0, 0, mat_id, 1, 1, 3, damage, emissive, aux, 0);
                        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
                        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
                    }

                    // -X side:
                    Voxel n_nx = sample_voxel(chunk, get_neighbor, x - 1, y, z);
                    if (!n_nx.is_solid() || (n_nx.shape() != SHAPE_CUBE && n_nx.shape() != SHAPE_SLAB_TOP)) {
                        PackedVoxelVertex v0 = PackedVoxelVertex::encode(x, y + 1, z,     1, 0, mat_id, 1, 1, 0, damage, emissive, aux, 1);
                        PackedVoxelVertex v1 = PackedVoxelVertex::encode(x, y + 1, z + 1, 1, 0, mat_id, 1, 1, 1, damage, emissive, aux, 1);
                        PackedVoxelVertex v2 = PackedVoxelVertex::encode(x, y + 1, z + 1, 1, 0, mat_id, 1, 1, 2, damage, emissive, aux, 0);
                        PackedVoxelVertex v3 = PackedVoxelVertex::encode(x, y + 1, z,     1, 0, mat_id, 1, 1, 3, damage, emissive, aux, 0);
                        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
                        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
                    }

                    // +Z side:
                    Voxel n_pz = sample_voxel(chunk, get_neighbor, x, y, z + 1);
                    if (!n_pz.is_solid() || (n_pz.shape() != SHAPE_CUBE && n_pz.shape() != SHAPE_SLAB_TOP)) {
                        PackedVoxelVertex v0 = PackedVoxelVertex::encode(x,     y + 1, z + 1, 4, 0, mat_id, 1, 1, 0, damage, emissive, aux, 1);
                        PackedVoxelVertex v1 = PackedVoxelVertex::encode(x + 1, y + 1, z + 1, 4, 0, mat_id, 1, 1, 1, damage, emissive, aux, 1);
                        PackedVoxelVertex v2 = PackedVoxelVertex::encode(x + 1, y + 1, z + 1, 4, 0, mat_id, 1, 1, 2, damage, emissive, aux, 0);
                        PackedVoxelVertex v3 = PackedVoxelVertex::encode(x,     y + 1, z + 1, 4, 0, mat_id, 1, 1, 3, damage, emissive, aux, 0);
                        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
                        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
                    }

                    // -Z side:
                    Voxel n_nz = sample_voxel(chunk, get_neighbor, x, y, z - 1);
                    if (!n_nz.is_solid() || (n_nz.shape() != SHAPE_CUBE && n_nz.shape() != SHAPE_SLAB_TOP)) {
                        PackedVoxelVertex v0 = PackedVoxelVertex::encode(x + 1, y + 1, z, 5, 0, mat_id, 1, 1, 0, damage, emissive, aux, 1);
                        PackedVoxelVertex v1 = PackedVoxelVertex::encode(x,     y + 1, z, 5, 0, mat_id, 1, 1, 1, damage, emissive, aux, 1);
                        PackedVoxelVertex v2 = PackedVoxelVertex::encode(x,     y + 1, z, 5, 0, mat_id, 1, 1, 2, damage, emissive, aux, 0);
                        PackedVoxelVertex v3 = PackedVoxelVertex::encode(x + 1, y + 1, z, 5, 0, mat_id, 1, 1, 3, damage, emissive, aux, 0);
                        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
                        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
                    }
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // PART 4: Meshing SHAPE_CORNER_* Sub-Blocks
    // ─────────────────────────────────────────────────────────────
    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                Voxel cur = chunk.get_voxel(x, y, z);
                VoxelShape s = cur.shape();
                if (!is_corner_shape(s)) {
                    continue;
                }

                uint32_t mat_id = cur.material_id;
                uint32_t damage = cur.damage();
                uint32_t emissive = get_emissive_intensity(mat_id);
                uint32_t aux = cur.is_highlighted() ? 1 : 0;

                // Base quad at Y = y (omitted only if solid cube below)
                Voxel below = sample_voxel(chunk, get_neighbor, x, y - 1, z);
                if (!below.is_solid() || below.shape() != SHAPE_CUBE) {
                    emit_quad(
                        glm::ivec3(x,     y, z),
                        glm::ivec3(x + 1, y, z),
                        glm::ivec3(x + 1, y, z + 1),
                        glm::ivec3(x,     y, z + 1),
                        3, mat_id, 1, 1, damage, emissive, aux, 0
                    );
                }

                // Outer Corners (Convex pyramid slope)
                if (s == SHAPE_CORNER_OUTER_NE) {
                    // Apex at (x + 1, y + 1, z)
                    emit_triangle(glm::ivec3(x, y, z), glm::ivec3(x + 1, y + 1, z), glm::ivec3(x + 1, y, z), 2, mat_id, damage, emissive, aux);
                    emit_triangle(glm::ivec3(x, y, z + 1), glm::ivec3(x + 1, y + 1, z), glm::ivec3(x, y, z), 2, mat_id, damage, emissive, aux);
                    emit_triangle(glm::ivec3(x + 1, y, z + 1), glm::ivec3(x + 1, y + 1, z), glm::ivec3(x, y, z + 1), 2, mat_id, damage, emissive, aux);
                } else if (s == SHAPE_CORNER_OUTER_NW) {
                    // Apex at (x, y + 1, z)
                    emit_triangle(glm::ivec3(x + 1, y, z), glm::ivec3(x, y, z), glm::ivec3(x, y + 1, z), 2, mat_id, damage, emissive, aux);
                    emit_triangle(glm::ivec3(x + 1, y, z + 1), glm::ivec3(x + 1, y, z), glm::ivec3(x, y + 1, z), 2, mat_id, damage, emissive, aux);
                    emit_triangle(glm::ivec3(x, y, z + 1), glm::ivec3(x + 1, y, z + 1), glm::ivec3(x, y + 1, z), 2, mat_id, damage, emissive, aux);
                } else if (s == SHAPE_CORNER_OUTER_SE) {
                    // Apex at (x + 1, y + 1, z + 1)
                    emit_triangle(glm::ivec3(x, y, z), glm::ivec3(x + 1, y, z), glm::ivec3(x + 1, y + 1, z + 1), 2, mat_id, damage, emissive, aux);
                    emit_triangle(glm::ivec3(x + 1, y, z), glm::ivec3(x + 1, y, z + 1), glm::ivec3(x + 1, y + 1, z + 1), 2, mat_id, damage, emissive, aux);
                    emit_triangle(glm::ivec3(x, y, z + 1), glm::ivec3(x, y, z), glm::ivec3(x + 1, y + 1, z + 1), 2, mat_id, damage, emissive, aux);
                } else if (s == SHAPE_CORNER_OUTER_SW) {
                    // Apex at (x, y + 1, z + 1)
                    emit_triangle(glm::ivec3(x, y, z), glm::ivec3(x, y + 1, z + 1), glm::ivec3(x + 1, y, z), 2, mat_id, damage, emissive, aux);
                    emit_triangle(glm::ivec3(x + 1, y, z), glm::ivec3(x, y + 1, z + 1), glm::ivec3(x + 1, y, z + 1), 2, mat_id, damage, emissive, aux);
                    emit_triangle(glm::ivec3(x + 1, y, z + 1), glm::ivec3(x, y + 1, z + 1), glm::ivec3(x, y, z + 1), 2, mat_id, damage, emissive, aux);
                } else {
                    // Inner valley corners: diagonal slope face
                    emit_quad(
                        glm::ivec3(x,     y,     z),
                        glm::ivec3(x,     y,     z + 1),
                        glm::ivec3(x + 1, y + 1, z + 1),
                        glm::ivec3(x + 1, y + 1, z),
                        2, mat_id, 1, 1, damage, emissive, aux, 0
                    );
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // PART 5: Dedicated Liquid & Waterlogged Sub-Block Meshing Pass
    // ─────────────────────────────────────────────────────────────
    mesh_liquid_pass(chunk, get_neighbor, vertices);

    return vertices;
}

void GreedyMesher::mesh_liquid_pass(
    const Chunk& chunk,
    const NeighborChunkGetter& get_neighbor,
    std::vector<PackedVoxelVertex>& vertices
) {
    if (chunk.is_empty()) return;

    auto emit_quad = [&](
        const glm::ivec3& p0, const glm::ivec3& p1, const glm::ivec3& p2, const glm::ivec3& p3,
        uint32_t norm_idx, uint32_t mat_id, uint32_t u_dim, uint32_t v_dim,
        uint32_t damage, uint32_t emissive, uint32_t aux, uint32_t sub_y_half = 0, uint32_t water_offset = 0
    ) {
        PackedVoxelVertex v0 = PackedVoxelVertex::encode(p0.x, p0.y, p0.z, norm_idx, 0, mat_id, u_dim, v_dim, 0, damage, emissive, aux, sub_y_half, water_offset);
        PackedVoxelVertex v1 = PackedVoxelVertex::encode(p1.x, p1.y, p1.z, norm_idx, 0, mat_id, u_dim, v_dim, 1, damage, emissive, aux, sub_y_half, water_offset);
        PackedVoxelVertex v2 = PackedVoxelVertex::encode(p2.x, p2.y, p2.z, norm_idx, 0, mat_id, u_dim, v_dim, 2, damage, emissive, aux, sub_y_half, water_offset);
        PackedVoxelVertex v3 = PackedVoxelVertex::encode(p3.x, p3.y, p3.z, norm_idx, 0, mat_id, u_dim, v_dim, 3, damage, emissive, aux, sub_y_half, water_offset);

        vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
        vertices.push_back(v0); vertices.push_back(v2); vertices.push_back(v3);
    };

    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                Voxel cur = chunk.get_voxel(x, y, z);
                bool is_pure_liquid = cur.is_liquid();
                bool is_waterlogged = cur.is_waterlogged();

                if (!is_pure_liquid && !is_waterlogged) {
                    continue;
                }

                // ─────────────────────────────────────────────────────────────
                // PASS 1: PURE LIQUID BLOCKS (MAT_WATER, MAT_ACID, MAT_COOLANT)
                // ─────────────────────────────────────────────────────────────
                if (is_pure_liquid) {
                    uint32_t mat_id = cur.material_id;
                    uint32_t emissive = get_emissive_intensity(mat_id);
                    uint32_t aux = cur.is_highlighted() ? 1 : 0;
                    uint8_t fluid_lvl = cur.fluid_level();
                    uint32_t top_sub_half = (fluid_lvl > 0 && fluid_lvl <= 2) ? 1 : 0;

                    // 1. Top face (+Y, normal_idx = 2)
                    Voxel above = sample_voxel(chunk, get_neighbor, x, y + 1, z);
                    bool top_culled = (above.material_id == cur.material_id) ||
                                      (above.is_solid() && (above.shape() == SHAPE_CUBE || above.shape() == SHAPE_SLAB_BOTTOM));
                    if (!top_culled) {
                        emit_quad(
                            glm::ivec3(x,     y + 1, z),
                            glm::ivec3(x,     y + 1, z + 1),
                            glm::ivec3(x + 1, y + 1, z + 1),
                            glm::ivec3(x + 1, y + 1, z),
                            2, mat_id, 1, 1, 0, emissive, aux, top_sub_half
                        );
                    }

                    // 2. Bottom face (-Y, normal_idx = 3)
                    Voxel below = sample_voxel(chunk, get_neighbor, x, y - 1, z);
                    bool bottom_culled = (below.material_id == cur.material_id) ||
                                         (below.is_solid() && (below.shape() == SHAPE_CUBE || below.shape() == SHAPE_SLAB_TOP));
                    if (!bottom_culled) {
                        emit_quad(
                            glm::ivec3(x,     y, z),
                            glm::ivec3(x + 1, y, z),
                            glm::ivec3(x + 1, y, z + 1),
                            glm::ivec3(x,     y, z + 1),
                            3, mat_id, 1, 1, 0, emissive, aux, 0
                        );
                    }

                    // 3. Side face: +X (normal_idx = 0)
                    Voxel n_px = sample_voxel(chunk, get_neighbor, x + 1, y, z);
                    bool px_culled = (n_px.material_id == cur.material_id) ||
                                     (n_px.is_solid() && n_px.shape() == SHAPE_CUBE);
                    if (!px_culled) {
                        emit_quad(
                            glm::ivec3(x + 1, y,     z + 1),
                            glm::ivec3(x + 1, y,     z),
                            glm::ivec3(x + 1, y + 1, z),
                            glm::ivec3(x + 1, y + 1, z + 1),
                            0, mat_id, 1, 1, 0, emissive, aux, top_sub_half
                        );
                    }

                    // 4. Side face: -X (normal_idx = 1)
                    Voxel n_nx = sample_voxel(chunk, get_neighbor, x - 1, y, z);
                    bool nx_culled = (n_nx.material_id == cur.material_id) ||
                                     (n_nx.is_solid() && n_nx.shape() == SHAPE_CUBE);
                    if (!nx_culled) {
                        emit_quad(
                            glm::ivec3(x, y,     z),
                            glm::ivec3(x, y,     z + 1),
                            glm::ivec3(x, y + 1, z + 1),
                            glm::ivec3(x, y + 1, z),
                            1, mat_id, 1, 1, 0, emissive, aux, top_sub_half
                        );
                    }

                    // 5. Side face: +Z (normal_idx = 4)
                    Voxel n_pz = sample_voxel(chunk, get_neighbor, x, y, z + 1);
                    bool pz_culled = (n_pz.material_id == cur.material_id) ||
                                     (n_pz.is_solid() && n_pz.shape() == SHAPE_CUBE);
                    if (!pz_culled) {
                        emit_quad(
                            glm::ivec3(x,     y,     z + 1),
                            glm::ivec3(x + 1, y,     z + 1),
                            glm::ivec3(x + 1, y + 1, z + 1),
                            glm::ivec3(x,     y + 1, z + 1),
                            4, mat_id, 1, 1, 0, emissive, aux, top_sub_half
                        );
                    }

                    // 6. Side face: -Z (normal_idx = 5)
                    Voxel n_nz = sample_voxel(chunk, get_neighbor, x, y, z - 1);
                    bool nz_culled = (n_nz.material_id == cur.material_id) ||
                                     (n_nz.is_solid() && n_nz.shape() == SHAPE_CUBE);
                    if (!nz_culled) {
                        emit_quad(
                            glm::ivec3(x + 1, y,     z),
                            glm::ivec3(x,     y,     z),
                            glm::ivec3(x,     y + 1, z),
                            glm::ivec3(x + 1, y + 1, z),
                            5, mat_id, 1, 1, 0, emissive, aux, top_sub_half
                        );
                    }
                }

                // ─────────────────────────────────────────────────────────────
                // PASS 2: WATERLOGGED SLABS (SHAPE_SLAB_BOTTOM + WATERLOGGED)
                // ─────────────────────────────────────────────────────────────
                if (cur.shape() == SHAPE_SLAB_BOTTOM && is_waterlogged) {
                    uint32_t mat_id = MAT_WATER;
                    uint32_t emissive = get_emissive_intensity(mat_id);
                    uint32_t aux = cur.is_highlighted() ? 1 : 0;

                    // Liquid surface sitting at Y = 0.5m (sub_y_half = 1)
                    Voxel above = sample_voxel(chunk, get_neighbor, x, y + 1, z);
                    bool above_culled = IsLiquid(above.material_id) ||
                                        (above.is_solid() && (above.shape() == SHAPE_CUBE || above.shape() == SHAPE_SLAB_BOTTOM));
                    if (!above_culled) {
                        emit_quad(
                            glm::ivec3(x,     y + 1, z),
                            glm::ivec3(x,     y + 1, z + 1),
                            glm::ivec3(x + 1, y + 1, z + 1),
                            glm::ivec3(x + 1, y + 1, z),
                            2, mat_id, 1, 1, 0, emissive, aux, 1 /* Y = 0.5m */
                        );
                    }
                }

                // ─────────────────────────────────────────────────────────────
                // PASS 3: WATERLOGGED RAMPS (SHAPE_RAMP_* + WATERLOGGED)
                // ─────────────────────────────────────────────────────────────
                if (cur.is_ramp() && is_waterlogged) {
                    uint32_t mat_id = MAT_WATER;
                    uint32_t emissive = get_emissive_intensity(mat_id);
                    uint32_t aux = cur.is_highlighted() ? 1 : 0;

                    Voxel above = sample_voxel(chunk, get_neighbor, x, y + 1, z);
                    bool above_solid = above.is_solid() && (above.shape() == SHAPE_CUBE);

                    if (!above_solid) {
                        // Diagonal 45° liquid top plane offset by +0.04m (water_offset = 1)
                        if (cur.shape() == SHAPE_RAMP_EAST) {
                            emit_quad(
                                glm::ivec3(x,     y,     z),
                                glm::ivec3(x,     y,     z + 1),
                                glm::ivec3(x + 1, y + 1, z + 1),
                                glm::ivec3(x + 1, y + 1, z),
                                2, mat_id, 1, 1, 0, emissive, aux, 0, 1 /* water_offset = 1 */
                            );
                        } else if (cur.shape() == SHAPE_RAMP_WEST) {
                            emit_quad(
                                glm::ivec3(x + 1, y,     z + 1),
                                glm::ivec3(x + 1, y,     z),
                                glm::ivec3(x,     y + 1, z),
                                glm::ivec3(x,     y + 1, z + 1),
                                2, mat_id, 1, 1, 0, emissive, aux, 0, 1 /* water_offset = 1 */
                            );
                        } else if (cur.shape() == SHAPE_RAMP_SOUTH) {
                            emit_quad(
                                glm::ivec3(x + 1, y,     z),
                                glm::ivec3(x,     y,     z),
                                glm::ivec3(x,     y + 1, z + 1),
                                glm::ivec3(x + 1, y + 1, z + 1),
                                2, mat_id, 1, 1, 0, emissive, aux, 0, 1 /* water_offset = 1 */
                            );
                        } else if (cur.shape() == SHAPE_RAMP_NORTH) {
                            emit_quad(
                                glm::ivec3(x,     y,     z + 1),
                                glm::ivec3(x + 1, y,     z + 1),
                                glm::ivec3(x + 1, y + 1, z),
                                glm::ivec3(x,     y + 1, z),
                                2, mat_id, 1, 1, 0, emissive, aux, 0, 1 /* water_offset = 1 */
                            );
                        }
                    }
                }
            }
        }
    }
}

} // namespace Voidfall
