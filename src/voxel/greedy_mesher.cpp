#include "greedy_mesher.hpp"
#include <array>

namespace Voidfall {

uint8_t GreedyMesher::compute_vertex_ao(bool s1, bool s2, bool c) {
    if (s1 && s2) {
        return 0; // Fully occluded corner
    }
    return static_cast<uint8_t>(3 - ((s1 ? 1 : 0) + (s2 ? 1 : 0) + (c ? 1 : 0)));
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

    // Treat unloaded neighbor boundaries as solid granite to prevent void leaks and seam culling
    return Voxel{MAT_FRACTURED_GRANITE, 0};
}

bool GreedyMesher::is_face_visible(
    const Chunk& chunk,
    const NeighborChunkGetter& get_neighbor,
    int x, int y, int z,
    int nx, int ny, int nz
) {
    Voxel current = sample_voxel(chunk, get_neighbor, x, y, z);
    if (!current.is_solid()) {
        return false;
    }
    Voxel neighbor = sample_voxel(chunk, get_neighbor, nx, ny, nz);
    return !neighbor.is_solid();
}

std::vector<PackedVoxelVertex> GreedyMesher::generate_mesh(
    const Chunk& chunk,
    const NeighborChunkGetter& get_neighbor
) {
    std::vector<PackedVoxelVertex> vertices;
    if (chunk.is_empty()) {
        return vertices;
    }

    // Reserve reasonable initial capacity
    vertices.reserve(2048);

    // 6 Directions / Faces
    // 0: +X, 1: -X, 2: +Y, 3: -Y, 4: +Z, 5: -Z
    const int normal_dirs[6][3] = {
        { 1,  0,  0}, // +X (0)
        {-1,  0,  0}, // -X (1)
        { 0,  1,  0}, // +Y (2)
        { 0, -1,  0}, // -Y (3)
        { 0,  0,  1}, // +Z (4)
        { 0,  0, -1}  // -Z (5)
    };

    struct FaceQuad {
        Voxel voxel;
        uint8_t ao[4]; // 00, 10, 11, 01
        bool visible{false};
    };

    // Meshing across 3 axes
    for (int d = 0; d < 3; ++d) {
        int u = (d + 1) % 3;
        int v = (d + 2) % 3;

        int x[3] = {0, 0, 0};
        int q[3] = {0, 0, 0};

        std::array<FaceQuad, CHUNK_SIZE * CHUNK_SIZE> mask;

        // Iterate through all slices along axis d
        for (x[d] = 0; x[d] < CHUNK_SIZE; ++x[d]) {
            // For both forward (+1) and backward (-1) faces
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

                        if (!is_face_visible(chunk, get_neighbor, x[0], x[1], x[2], x[0] + q[0], x[1] + q[1], x[2] + q[2])) {
                            continue;
                        }

                        Voxel current = sample_voxel(chunk, get_neighbor, x[0], x[1], x[2]);
                        mask[mask_idx].visible = true;
                        mask[mask_idx].voxel = current;

                        // Calculate Baked Vertex AO for the 4 corners
                        // Tangent offsets:
                        int tu[3] = {0, 0, 0}; tu[u] = 1;
                        int tv[3] = {0, 0, 0}; tv[v] = 1;

                        int bx = x[0] + q[0];
                        int by = x[1] + q[1];
                        int bz = x[2] + q[2];

                        // Check 8 neighbors in the adjacent face plane
                        bool s_left   = sample_voxel(chunk, get_neighbor, bx - tu[0], by - tu[1], bz - tu[2]).is_solid();
                        bool s_right  = sample_voxel(chunk, get_neighbor, bx + tu[0], by + tu[1], bz + tu[2]).is_solid();
                        bool s_down   = sample_voxel(chunk, get_neighbor, bx - tv[0], by - tv[1], bz - tv[2]).is_solid();
                        bool s_up     = sample_voxel(chunk, get_neighbor, bx + tv[0], by + tv[1], bz + tv[2]).is_solid();

                        bool c_ld = sample_voxel(chunk, get_neighbor, bx - tu[0] - tv[0], by - tu[1] - tv[1], bz - tu[2] - tv[2]).is_solid();
                        bool c_rd = sample_voxel(chunk, get_neighbor, bx + tu[0] - tv[0], by + tu[1] - tv[1], bz + tu[2] - tv[2]).is_solid();
                        bool c_ru = sample_voxel(chunk, get_neighbor, bx + tu[0] + tv[0], by + tu[1] + tv[1], bz + tu[2] + tv[2]).is_solid();
                        bool c_lu = sample_voxel(chunk, get_neighbor, bx - tu[0] + tv[0], by - tu[1] + tv[1], bz - tu[2] + tv[2]).is_solid();

                        mask[mask_idx].ao[0] = compute_vertex_ao(s_left, s_down, c_ld);  // Corner (0, 0)
                        mask[mask_idx].ao[1] = compute_vertex_ao(s_right, s_down, c_rd); // Corner (1, 0)
                        mask[mask_idx].ao[2] = compute_vertex_ao(s_right, s_up, c_ru);   // Corner (1, 1)
                        mask[mask_idx].ao[3] = compute_vertex_ao(s_left, s_up, c_lu);    // Corner (0, 1)
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
                        // Extend along U axis (blocks must match material, damage, and AO values)
                        while (i + width < CHUNK_SIZE) {
                            int next_idx = (i + width) + j * CHUNK_SIZE;
                            const FaceQuad& next = mask[next_idx];
                            if (!next.visible ||
                                next.voxel.material_id != root.voxel.material_id ||
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

                        // Mark merged quads as processed
                        for (int dy = 0; dy < height; ++dy) {
                            for (int dx = 0; dx < width; ++dx) {
                                mask[(i + dx) + (j + dy) * CHUNK_SIZE].visible = false;
                            }
                        }

                        // Corner vertex coordinates in 3D
                        int p[3];
                        p[d] = x[d] + (face_dir == 0 ? 1 : 0);
                        p[u] = i;
                        p[v] = j;

                        int du[3] = {0, 0, 0}; du[u] = width;
                        int dv[3] = {0, 0, 0}; dv[v] = height;

                        uint32_t mat_id = root.voxel.material_id;
                        uint32_t damage = root.voxel.damage();
                        uint32_t emissive = (mat_id == MAT_VOIDITE_CRYSTAL) ? 220 :
                                            (mat_id == MAT_THERMITE_SLAG) ? 255 :
                                            (mat_id == MAT_RADIOACTIVE_ORE) ? 180 : 0;
                        uint32_t aux = root.voxel.is_highlighted() ? 1 : 0;

                        // 4 Quad vertices:
                        // v0: (p)
                        // v1: (p + du)
                        // v2: (p + du + dv)
                        // v3: (p + dv)
                        PackedVoxelVertex vert0 = PackedVoxelVertex::encode(
                            p[0], p[1], p[2],
                            norm_idx, root.ao[0], mat_id,
                            width, height, 0, damage, emissive, aux
                        );
                        PackedVoxelVertex vert1 = PackedVoxelVertex::encode(
                            p[0] + du[0], p[1] + du[1], p[2] + du[2],
                            norm_idx, root.ao[1], mat_id,
                            width, height, 1, damage, emissive, aux
                        );
                        PackedVoxelVertex vert2 = PackedVoxelVertex::encode(
                            p[0] + du[0] + dv[0], p[1] + du[1] + dv[1], p[2] + du[2] + dv[2],
                            norm_idx, root.ao[2], mat_id,
                            width, height, 2, damage, emissive, aux
                        );
                        PackedVoxelVertex vert3 = PackedVoxelVertex::encode(
                            p[0] + dv[0], p[1] + dv[1], p[2] + dv[2],
                            norm_idx, root.ao[3], mat_id,
                            width, height, 3, damage, emissive, aux
                        );

                        // Winding order and AO anisotropy diagonal flip
                        bool flip_diag = (root.ao[0] + root.ao[2]) > (root.ao[1] + root.ao[3]);

                        if (face_dir == 0) { // Forward face (+X, +Y, +Z)
                            if (flip_diag) {
                                vertices.push_back(vert0);
                                vertices.push_back(vert1);
                                vertices.push_back(vert2);

                                vertices.push_back(vert0);
                                vertices.push_back(vert2);
                                vertices.push_back(vert3);
                            } else {
                                vertices.push_back(vert1);
                                vertices.push_back(vert2);
                                vertices.push_back(vert3);

                                vertices.push_back(vert1);
                                vertices.push_back(vert3);
                                vertices.push_back(vert0);
                            }
                        } else { // Backward face (-X, -Y, -Z)
                            if (flip_diag) {
                                vertices.push_back(vert0);
                                vertices.push_back(vert2);
                                vertices.push_back(vert1);

                                vertices.push_back(vert0);
                                vertices.push_back(vert3);
                                vertices.push_back(vert2);
                            } else {
                                vertices.push_back(vert1);
                                vertices.push_back(vert3);
                                vertices.push_back(vert2);

                                vertices.push_back(vert1);
                                vertices.push_back(vert0);
                                vertices.push_back(vert3);
                            }
                        }

                        i += width;
                    }
                }
            }
        }
    }

    return vertices;
}

} // namespace Voidfall
