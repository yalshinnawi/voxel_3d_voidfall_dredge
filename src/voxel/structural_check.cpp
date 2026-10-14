#include "structural_check.hpp"
#include <queue>
#include <unordered_set>

namespace Voidfall {

struct Vec3Hash {
    std::size_t operator()(const glm::ivec3& v) const noexcept {
        std::size_t h = 2166136261u;
        std::size_t mult = 16777619u;
        h = (h ^ static_cast<std::size_t>(v.x)) * mult;
        h = (h ^ static_cast<std::size_t>(v.y)) * mult;
        h = (h ^ static_cast<std::size_t>(v.z)) * mult;
        return h;
    }
};

std::vector<UnanchoredIsland> StructuralCheck::solve_cavein(
    World& world,
    int destroyed_x,
    int destroyed_y,
    int destroyed_z,
    size_t max_search_nodes
) {
    std::vector<UnanchoredIsland> unanchored_islands;

    const glm::ivec3 neighbor_dirs[6] = {
        glm::ivec3( 1,  0,  0),
        glm::ivec3(-1,  0,  0),
        glm::ivec3( 0,  1,  0),
        glm::ivec3( 0, -1,  0),
        glm::ivec3( 0,  0,  1),
        glm::ivec3( 0,  0, -1)
    };

    std::unordered_set<glm::ivec3, Vec3Hash> globally_visited;
    size_t expected_cap = std::min(max_search_nodes, size_t(512));
    globally_visited.reserve(expected_cap * 2);

    // Check all 6 immediate neighbors of the destroyed voxel
    for (const auto& dir : neighbor_dirs) {
        glm::ivec3 start = glm::ivec3(destroyed_x, destroyed_y, destroyed_z) + dir;

        if (globally_visited.count(start)) continue;

        Voxel start_vox = world.get_voxel(start.x, start.y, start.z);
        if (!start_vox.is_solid()) continue;

        // Run Breadth-First Search
        std::queue<glm::ivec3> queue;
        std::unordered_set<glm::ivec3, Vec3Hash> island_visited;
        std::vector<glm::ivec3> island_blocks;
        island_visited.reserve(expected_cap);
        island_blocks.reserve(expected_cap);

        queue.push(start);
        island_visited.insert(start);
        island_blocks.push_back(start);

        bool is_anchored = false;

        while (!queue.empty() && island_blocks.size() < max_search_nodes) {
            glm::ivec3 current = queue.front();
            queue.pop();

            Voxel cur_vox = world.get_voxel(current.x, current.y, current.z);

            // If we touch bedrock, ceiling bedrock, or an indestructible anchor, this entire component is stable
            if (cur_vox.is_anchored() || cur_vox.material_id == MAT_DREDGE_BEDROCK || current.y <= 1 || current.y >= world.chunk_ceiling()) {
                is_anchored = true;
                break;
            }

            for (const auto& ndir : neighbor_dirs) {
                glm::ivec3 next = current + ndir;
                if (island_visited.count(next)) continue;

                Voxel next_vox = world.get_voxel(next.x, next.y, next.z);
                if (are_voxels_face_connected(cur_vox, next_vox, ndir.x, ndir.y, ndir.z)) {
                    island_visited.insert(next);
                    island_blocks.push_back(next);
                    queue.push(next);
                }
            }
        }

        // Add all island blocks to global visited set
        for (const auto& b : island_blocks) {
            globally_visited.insert(b);
        }

        // If the island did not hit an anchor and the queue was fully exhausted, it is unanchored!
        if (!is_anchored && queue.empty() && !island_blocks.empty()) {
            UnanchoredIsland island;
            island.world_positions = island_blocks;

            // Calculate center of mass
            glm::vec3 sum(0.0f);
            for (const auto& b : island_blocks) {
                sum += glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f);
            }
            island.center_of_mass = sum / static_cast<float>(island_blocks.size());

            // Build relative block list and detach from world grid with batched chunk dirtying
            std::unordered_set<ChunkPos, ChunkPosHash> affected_chunks;
            for (const auto& b : island_blocks) {
                Voxel v = world.get_voxel(b.x, b.y, b.z);
                island.blocks.push_back(DebrisBlock{
                    b - glm::ivec3(std::floor(island.center_of_mass.x),
                                   std::floor(island.center_of_mass.y),
                                   std::floor(island.center_of_mass.z)),
                    v
                });
                island.primary_material = v.material_id;
                // Remove from static terrain without recursive neighbor queueing
                world.set_voxel(b.x, b.y, b.z, Voxel{MAT_AIR, 0}, false);

                int cx = b.x >> 5;
                int cy = b.y >> 5;
                int cz = b.z >> 5;
                affected_chunks.insert(ChunkPos{cx, cy, cz});
                int lx = b.x & 31;
                int ly = b.y & 31;
                int lz = b.z & 31;
                if (lx == 0) affected_chunks.insert(ChunkPos{cx - 1, cy, cz});
                if (lx == 31) affected_chunks.insert(ChunkPos{cx + 1, cy, cz});
                if (ly == 0) affected_chunks.insert(ChunkPos{cx, cy - 1, cz});
                if (ly == 31) affected_chunks.insert(ChunkPos{cx, cy + 1, cz});
                if (lz == 0) affected_chunks.insert(ChunkPos{cx, cy, cz - 1});
                if (lz == 31) affected_chunks.insert(ChunkPos{cx, cy, cz + 1});
            }

            for (const auto& cpos : affected_chunks) {
                Chunk* c = world.get_chunk(cpos);
                if (c) {
                    c->MarkDirty();
                    world.queue_chunk_for_meshing(cpos);
                }
            }

            unanchored_islands.push_back(std::move(island));
        }
    }

    return unanchored_islands;
}

std::vector<glm::ivec3> StructuralCheck::query_seismic_detachment_blocks(
    const World& world,
    const glm::vec3& player_pos,
    int min_blocks,
    int max_blocks,
    float radius,
    int min_y_offset,
    int max_y_offset
) {
    int px = static_cast<int>(std::floor(player_pos.x));
    int py = static_cast<int>(std::floor(player_pos.y));
    int pz = static_cast<int>(std::floor(player_pos.z));

    std::vector<glm::ivec3> candidates;
    int r_ceil = static_cast<int>(std::ceil(radius));

    const glm::ivec3 neighbors[6] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}
    };

    // Fast column-based surface scan: for each (dx, dz) within circle radius,
    // scan upward from min_y_offset to max_y_offset to find the exposed ceiling boundary.
    // The first solid block encountered above air is the ceiling surface block for this column;
    // any blocks above it have solid rock below them and cannot detach.
    for (int dx = -r_ceil; dx <= r_ceil; ++dx) {
        for (int dz = -r_ceil; dz <= r_ceil; ++dz) {
            if (dx * dx + dz * dz > radius * radius) continue;

            int vx = px + dx;
            int vz = pz + dz;

            for (int dy = min_y_offset; dy <= max_y_offset; ++dy) {
                int vy = py + dy;
                if (vy < 1 || vy >= 127) continue;

                Voxel v = world.get_voxel(vx, vy, vz);
                if (!v.is_solid()) continue; // Still in open cavern air

                // First solid block encountered in this column (ceiling face)
                if (v.material_id == MAT_DREDGE_BEDROCK ||
                    v.material_id == MAT_INDUSTRIAL_BULKHEAD ||
                    v.material_id == MAT_REINFORCED_VAULT_DOOR) {
                    break; // Bedrock/bulkhead ceiling is indestructible, stop column
                }

                // Must have air directly below it so it can fall
                Voxel v_below = world.get_voxel(vx, vy - 1, vz);
                if (v_below.is_solid()) {
                    break;
                }

                // Check if supported by any adjacent player-placed MAT_BULKHEAD blocks
                bool supported_by_bulkhead = false;
                for (const auto& offset : neighbors) {
                    Voxel adj = world.get_voxel(vx + offset.x, vy + offset.y, vz + offset.z);
                    if (adj.material_id == MAT_INDUSTRIAL_BULKHEAD) {
                        supported_by_bulkhead = true;
                        break;
                    }
                }

                if (!supported_by_bulkhead) {
                    candidates.push_back(glm::ivec3(vx, vy, vz));
                }

                // Ceiling block identified for this column, proceed to next column
                break;
            }
        }
    }

    if (candidates.empty()) return {};

    int target_count = min_blocks + (rand() % (std::max(1, max_blocks - min_blocks + 1)));
    if (static_cast<int>(candidates.size()) <= target_count) {
        return candidates;
    }

    std::vector<glm::ivec3> selected;
    selected.reserve(target_count);
    for (int i = 0; i < target_count && !candidates.empty(); ++i) {
        size_t idx = rand() % candidates.size();
        selected.push_back(candidates[idx]);
        candidates[idx] = candidates.back();
        candidates.pop_back();
    }

    return selected;
}

int StructuralCheck::count_unanchored_blocks(const World& world) {
    int w = world.world_width();
    int h = world.world_height();
    int d = world.world_depth();
    if (w <= 0 || h <= 0 || d <= 0) return 0;

    int total_voxels = w * h * d;
    std::vector<uint8_t> visited(total_voxels, 0);

    auto to_idx = [w, h](int x, int y, int z) -> int {
        return x + y * w + z * (w * h);
    };

    std::vector<int> queue;
    queue.reserve(total_voxels / 4);

    int ceil_y = std::min(h - 4, world.chunk_ceiling());

    // Multi-source BFS from foundational anchors
    for (int z = 0; z < d; ++z) {
        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y) {
                Voxel v = world.get_voxel(x, y, z);
                if (!v.is_solid()) continue;

                bool is_anchor = (y <= 3) || (y >= ceil_y) ||
                                 (x <= 3) || (x >= w - 4) ||
                                 (z <= 3) || (z >= d - 4) ||
                                 v.is_anchored() ||
                                 (v.material_id == MAT_DREDGE_BEDROCK);

                if (is_anchor) {
                    int idx = to_idx(x, y, z);
                    visited[idx] = 1;
                    queue.push_back(idx);
                }
            }
        }
    }

    const int dxs[6] = {1, -1, 0, 0, 0, 0};
    const int dys[6] = {0, 0, 1, -1, 0, 0};
    const int dzs[6] = {0, 0, 0, 0, 1, -1};

    size_t q_head = 0;
    while (q_head < queue.size()) {
        int cur_idx = queue[q_head++];
        int cz = cur_idx / (w * h);
        int rem = cur_idx % (w * h);
        int cy = rem / w;
        int cx = rem % w;

        Voxel cur_vox = world.get_voxel(cx, cy, cz);

        for (int i = 0; i < 6; ++i) {
            int nx = cx + dxs[i];
            int ny = cy + dys[i];
            int nz = cz + dzs[i];

            if (nx >= 0 && nx < w && ny >= 0 && ny < h && nz >= 0 && nz < d) {
                int n_idx = to_idx(nx, ny, nz);
                if (!visited[n_idx]) {
                    Voxel nv = world.get_voxel(nx, ny, nz);
                    if (are_voxels_face_connected(cur_vox, nv, dxs[i], dys[i], dzs[i])) {
                        visited[n_idx] = 1;
                        queue.push_back(n_idx);
                    }
                }
            }
        }
    }

    int unanchored_count = 0;
    for (int z = 0; z < d; ++z) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int idx = to_idx(x, y, z);
                if (!visited[idx]) {
                    Voxel v = world.get_voxel(x, y, z);
                    if (v.is_solid()) {
                        unanchored_count++;
                    }
                }
            }
        }
    }

    return unanchored_count;
}

int StructuralCheck::ensure_world_connectivity(World& world) {
    int w = world.world_width();
    int h = world.world_height();
    int d = world.world_depth();
    if (w <= 0 || h <= 0 || d <= 0) return 0;

    int total_voxels = w * h * d;
    std::vector<uint8_t> visited(total_voxels, 0);

    auto to_idx = [w, h](int x, int y, int z) -> int {
        return x + y * w + z * (w * h);
    };

    std::vector<int> queue;
    queue.reserve(total_voxels / 4);

    int ceil_y = std::min(h - 4, world.chunk_ceiling());

    // 1. Multi-source BFS from all foundational anchors
    for (int z = 0; z < d; ++z) {
        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y) {
                Voxel v = world.get_voxel(x, y, z);
                if (!v.is_solid()) continue;

                bool is_anchor = (y <= 3) || (y >= ceil_y) ||
                                 (x <= 3) || (x >= w - 4) ||
                                 (z <= 3) || (z >= d - 4) ||
                                 v.is_anchored() ||
                                 (v.material_id == MAT_DREDGE_BEDROCK);

                if (is_anchor) {
                    int idx = to_idx(x, y, z);
                    visited[idx] = 1;
                    queue.push_back(idx);
                }
            }
        }
    }

    const int dxs[6] = {1, -1, 0, 0, 0, 0};
    const int dys[6] = {0, 0, 1, -1, 0, 0};
    const int dzs[6] = {0, 0, 0, 0, 1, -1};

    size_t q_head = 0;
    while (q_head < queue.size()) {
        int cur_idx = queue[q_head++];
        int cz = cur_idx / (w * h);
        int rem = cur_idx % (w * h);
        int cy = rem / w;
        int cx = rem % w;

        Voxel cur_vox = world.get_voxel(cx, cy, cz);

        for (int i = 0; i < 6; ++i) {
            int nx = cx + dxs[i];
            int ny = cy + dys[i];
            int nz = cz + dzs[i];

            if (nx >= 0 && nx < w && ny >= 0 && ny < h && nz >= 0 && nz < d) {
                int n_idx = to_idx(nx, ny, nz);
                if (!visited[n_idx]) {
                    Voxel nv = world.get_voxel(nx, ny, nz);
                    if (are_voxels_face_connected(cur_vox, nv, dxs[i], dys[i], dzs[i])) {
                        visited[n_idx] = 1;
                        queue.push_back(n_idx);
                    }
                }
            }
        }
    }

    // 2. Group all unanchored solid voxels into connected components (islands)
    std::vector<std::vector<glm::ivec3>> islands;
    std::vector<uint8_t> island_visited(total_voxels, 0);

    for (int z = 0; z < d; ++z) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int idx = to_idx(x, y, z);
                if (!visited[idx] && !island_visited[idx]) {
                    Voxel v = world.get_voxel(x, y, z);
                    if (!v.is_solid()) continue;

                    std::vector<glm::ivec3> component;
                    std::vector<int> comp_queue;
                    comp_queue.push_back(idx);
                    island_visited[idx] = 1;

                    size_t c_head = 0;
                    while (c_head < comp_queue.size()) {
                        int c_idx = comp_queue[c_head++];
                        int iz = c_idx / (w * h);
                        int irem = c_idx % (w * h);
                        int iy = irem / w;
                        int ix = irem % w;
                        component.push_back(glm::ivec3(ix, iy, iz));

                        Voxel cur_vox = world.get_voxel(ix, iy, iz);

                        for (int i = 0; i < 6; ++i) {
                            int nx = ix + dxs[i];
                            int ny = iy + dys[i];
                            int nz = iz + dzs[i];
                            if (nx >= 0 && nx < w && ny >= 0 && ny < h && nz >= 0 && nz < d) {
                                int n_idx = to_idx(nx, ny, nz);
                                if (!visited[n_idx] && !island_visited[n_idx]) {
                                    Voxel nv = world.get_voxel(nx, ny, nz);
                                    if (are_voxels_face_connected(cur_vox, nv, dxs[i], dys[i], dzs[i])) {
                                        island_visited[n_idx] = 1;
                                        comp_queue.push_back(n_idx);
                                    }
                                }
                            }
                        }
                    }

                    islands.push_back(std::move(component));
                }
            }
        }
    }

    if (islands.empty()) {
        // Still ensure any disconnected clutter props are purged
        for (auto& [cpos, chunk] : world.chunks_mut()) {
            if (chunk) {
                world.cleanup_all_disconnected_clutter(*chunk);
            }
        }
        return 0;
    }

    // 3. Connect unanchored islands to solid anchor foundations
    int connected_count = 0;
    for (const auto& island : islands) {
        if (island.empty()) continue;

        std::unordered_map<int, std::pair<int, int>> col_bounds; // key = x + z * w -> min_y, max_y
        uint8_t primary_mat = MAT_VOLCANIC_BASALT;
        for (const auto& pos : island) {
            int key = pos.x + pos.z * w;
            auto it = col_bounds.find(key);
            if (it == col_bounds.end()) {
                col_bounds[key] = {pos.y, pos.y};
            } else {
                it->second.first = std::min(it->second.first, pos.y);
                it->second.second = std::max(it->second.second, pos.y);
            }
            Voxel iv = world.get_voxel(pos.x, pos.y, pos.z);
            if (iv.material_id != MAT_AIR) {
                primary_mat = iv.material_id;
            }
        }

        uint8_t pillar_mat = MAT_VOLCANIC_BASALT;
        if (primary_mat == MAT_TITANIUM || primary_mat == MAT_INDUSTRIAL_BULKHEAD || primary_mat == MAT_REINFORCED_VAULT_DOOR) {
            pillar_mat = MAT_INDUSTRIAL_BULKHEAD;
        } else if (primary_mat == MAT_FRACTURED_GRANITE) {
            pillar_mat = MAT_FRACTURED_GRANITE;
        } else if (primary_mat == MAT_PRECURSOR_STONE) {
            pillar_mat = MAT_PRECURSOR_STONE;
        }

        bool is_tiny_stray = (island.size() <= 2);
        int lowest_y = 999;
        int highest_y = -999;
        for (const auto& pos : island) {
            lowest_y = std::min(lowest_y, pos.y);
            highest_y = std::max(highest_y, pos.y);
        }

        int best_floor_y = -1;
        int best_col_x = island[0].x;
        int best_col_z = island[0].z;
        int min_drop = 999;

        for (const auto& [key, y_range] : col_bounds) {
            int cx = key % w;
            int cz = key / w;
            int y_low = y_range.first;

            for (int scan_y = y_low - 1; scan_y >= 0; --scan_y) {
                Voxel sv = world.get_voxel(cx, scan_y, cz);
                if (sv.is_solid() && visited[to_idx(cx, scan_y, cz)]) {
                    int drop = y_low - scan_y;
                    if (drop < min_drop) {
                        min_drop = drop;
                        best_floor_y = scan_y;
                        best_col_x = cx;
                        best_col_z = cz;
                    }
                    break;
                }
            }
        }

        if (is_tiny_stray) {
            // Check if near ceiling (e.g. stalactite fragment)
            int min_rise = 999;
            int best_ceil_y = -1;
            for (const auto& [key, y_range] : col_bounds) {
                int cx = key % w;
                int cz = key / w;
                int y_high = y_range.second;
                for (int scan_y = y_high + 1; scan_y < h; ++scan_y) {
                    Voxel sv = world.get_voxel(cx, scan_y, cz);
                    if (sv.is_solid() && visited[to_idx(cx, scan_y, cz)]) {
                        int rise = scan_y - y_high;
                        if (rise < min_rise) {
                            min_rise = rise;
                            best_ceil_y = scan_y;
                        }
                        break;
                    }
                }
            }

            if (min_rise <= 2 && best_ceil_y > 0) {
                // Connect up to ceiling as hanging stalactite
                for (const auto& [key, y_range] : col_bounds) {
                    int cx = key % w;
                    int cz = key / w;
                    for (int sy = y_range.second + 1; sy < best_ceil_y; ++sy) {
                        world.set_voxel(cx, sy, cz, Voxel{pillar_mat, VOXEL_FLAG_ANCHORED}, false);
                        visited[to_idx(cx, sy, cz)] = 1;
                    }
                }
                for (const auto& pos : island) {
                    visited[to_idx(pos.x, pos.y, pos.z)] = 1;
                }
                connected_count++;
                continue;
            } else {
                // Remove stray floating speck so the space looks clean and natural
                for (const auto& pos : island) {
                    world.set_voxel(pos.x, pos.y, pos.z, Voxel{MAT_AIR, 0}, false);
                }
                continue;
            }
        }

        // Structural connection: Place support pillars down to the solid floor below
        bool island_connected = false;
        for (const auto& [key, y_range] : col_bounds) {
            int cx = key % w;
            int cz = key / w;
            int y_low = y_range.first;

            bool is_pylon_loc = (cx == best_col_x && cz == best_col_z);

            if (!is_pylon_loc) continue;

            for (int scan_y = y_low - 1; scan_y >= 0; --scan_y) {
                Voxel sv = world.get_voxel(cx, scan_y, cz);
                if (sv.is_solid() && visited[to_idx(cx, scan_y, cz)]) {
                    if (sv.shape() != SHAPE_CUBE) {
                        sv.set_shape(SHAPE_CUBE);
                        world.set_voxel(cx, scan_y, cz, sv, false);
                    }
                    for (int sy = scan_y + 1; sy < y_low; ++sy) {
                        world.set_voxel(cx, sy, cz, Voxel{pillar_mat, 0}, false);
                        visited[to_idx(cx, sy, cz)] = 1;
                    }
                    island_connected = true;
                    break;
                }
            }
        }

        // If not connected downward (e.g. over deep chasm/void), connect upward to ceiling
        if (!island_connected) {
            for (const auto& [key, y_range] : col_bounds) {
                int cx = key % w;
                int cz = key / w;
                int y_high = y_range.second;
                for (int scan_y = y_high + 1; scan_y < h; ++scan_y) {
                    Voxel sv = world.get_voxel(cx, scan_y, cz);
                    if (sv.is_solid() && visited[to_idx(cx, scan_y, cz)]) {
                        for (int sy = y_high + 1; sy < scan_y; ++sy) {
                            world.set_voxel(cx, sy, cz, Voxel{pillar_mat, VOXEL_FLAG_ANCHORED}, false);
                            visited[to_idx(cx, sy, cz)] = 1;
                        }
                        island_connected = true;
                        break;
                    }
                }
                if (island_connected) break;
            }
        }

        // If still not connected (e.g. over deep void without ceiling), anchor to bedrock at y = 1
        if (!island_connected) {
            int cx = best_col_x;
            int cz = best_col_z;
            for (int sy = lowest_y - 1; sy >= 1; --sy) {
                world.set_voxel(cx, sy, cz, Voxel{pillar_mat, static_cast<uint8_t>((sy <= 3) ? VOXEL_FLAG_ANCHORED : 0)}, false);
                visited[to_idx(cx, sy, cz)] = 1;
            }
            island_connected = true;
        }

        for (const auto& pos : island) {
            visited[to_idx(pos.x, pos.y, pos.z)] = 1;
        }
        connected_count++;
    }

    // 4. Ensure that any slab or ramp supporting solid blocks above is restored to full cube
    for (int cz = 0; cz < d; ++cz) {
        for (int cx = 0; cx < w; ++cx) {
            for (int cy = 0; cy < h - 1; ++cy) {
                Voxel v = world.get_voxel(cx, cy, cz);
                if (v.is_solid() && (v.is_slab() || v.is_ramp())) {
                    Voxel above = world.get_voxel(cx, cy + 1, cz);
                    if (above.is_solid()) {
                        v.set_shape(SHAPE_CUBE);
                        world.set_voxel(cx, cy, cz, v, false);
                    }
                }
            }
        }
    }

    // 5. Ensure all clutter props across all chunks are connected to solid host blocks
    for (auto& [cpos, chunk] : world.chunks_mut()) {
        if (chunk) {
            world.cleanup_all_disconnected_clutter(*chunk);
        }
    }

    return connected_count;
}

} // namespace Voidfall
