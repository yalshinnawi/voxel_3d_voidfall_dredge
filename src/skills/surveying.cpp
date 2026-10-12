#include "surveying.hpp"
#include <algorithm>
#include <cmath>
#include "../entities/enemies/void_stalker.hpp"
#include "../entities/enemies/seismic_burrower.hpp"

namespace Voidfall {

SurveyingSystem::SurveyingSystem() = default;

void SurveyingSystem::trigger_scan(
    const glm::vec3& origin,
    const World& world,
    float scan_radius,
    float linger_bonus,
    int rank,
    const std::vector<VoidStalker>* stalkers,
    const std::vector<SeismicBurrower>* burrowers,
    const glm::vec3& extraction_pos,
    bool extraction_active
) {
    m_active = true;
    m_rank = rank;
    // Base duration is 1.8s (weaker at base), scaling +0.3s per rank up to ~3.3s
    m_duration = 1.8f + (0.3f * static_cast<float>(rank)) + linger_bonus;
    m_timer = m_duration;
    m_radius = std::min(scan_radius, 45.0f);
    m_origin = origin;
    m_surveyed.clear();
    m_clusters.clear();
    m_monsters.clear();

    float effective_radius = m_radius;
    int min_x = static_cast<int>(std::floor(origin.x - effective_radius));
    int max_x = static_cast<int>(std::ceil(origin.x + effective_radius));
    int min_y = static_cast<int>(std::floor(origin.y - effective_radius));
    int max_y = static_cast<int>(std::ceil(origin.y + effective_radius));
    int min_z = static_cast<int>(std::floor(origin.z - effective_radius));
    int max_z = static_cast<int>(std::ceil(origin.z + effective_radius));

    float radius_sq = effective_radius * effective_radius;

    for (int y = min_y; y <= max_y; ++y) {
        float dy = (static_cast<float>(y) + 0.5f) - origin.y;
        float dy2 = dy * dy;
        if (dy2 > radius_sq) continue;

        for (int z = min_z; z <= max_z; ++z) {
            float dz = (static_cast<float>(z) + 0.5f) - origin.z;
            float dyz2 = dy2 + dz * dz;
            if (dyz2 > radius_sq) continue;

            for (int x = min_x; x <= max_x; ++x) {
                float dx = (static_cast<float>(x) + 0.5f) - origin.x;
                if (dyz2 + dx * dx > radius_sq) continue;

                Voxel v = world.get_voxel(x, y, z);
                // Highlight ONLY objectives (Voidite extraction quota and Precursor Vault doors), nothing else
                if (v.material_id == MAT_VOIDITE ||
                    v.material_id == MAT_VOIDITE_CRYSTAL ||
                    v.material_id == MAT_VAULT_DOOR ||
                    v.material_id == MAT_REINFORCED_VAULT_DOOR) {
                    m_surveyed.push_back(SurveyedVoxel{glm::ivec3(x, y, z), v.material_id});
                }
            }
        }
    }

    // Spatial hash for fast O(1) neighbor lookups during flood-fill
    struct VoxelCoordHash {
        size_t operator()(const glm::ivec3& p) const noexcept {
            return (static_cast<size_t>(p.x) * 73856093u) ^
                   (static_cast<size_t>(p.y) * 19349663u) ^
                   (static_cast<size_t>(p.z) * 83492791u);
        }
    };

    std::unordered_map<glm::ivec3, uint8_t, VoxelCoordHash> voxel_map;
    voxel_map.reserve(m_surveyed.size());
    for (const auto& sv : m_surveyed) {
        voxel_map[sv.pos] = sv.material_id;
    }

    std::unordered_set<glm::ivec3, VoxelCoordHash> visited;
    visited.reserve(m_surveyed.size());

    // Vein Clustering: Flood-fill to merge touching target voxels into single connected components
    for (const auto& sv : m_surveyed) {
        if (visited.find(sv.pos) != visited.end()) continue;

        SurveyedCluster cluster;
        cluster.material_id = sv.material_id;

        std::vector<glm::ivec3> queue;
        queue.reserve(m_surveyed.size());
        queue.push_back(sv.pos);
        visited.insert(sv.pos);

        glm::vec3 sum_pos(0.0f);
        glm::vec3 min_b(static_cast<float>(sv.pos.x), static_cast<float>(sv.pos.y), static_cast<float>(sv.pos.z));
        glm::vec3 max_b(min_b + 1.0f);

        size_t head = 0;
        while (head < queue.size()) {
            glm::ivec3 curr = queue[head++];
            cluster.voxels.push_back(curr);
            glm::vec3 vpos = glm::vec3(curr) + glm::vec3(0.5f);
            sum_pos += vpos;

            min_b = glm::min(min_b, glm::vec3(curr));
            max_b = glm::max(max_b, glm::vec3(curr) + 1.0f);

            // 26-connectivity: check touching neighbors (sharing face, edge, or corner)
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0 && dz == 0) continue;
                        glm::ivec3 npos = curr + glm::ivec3(dx, dy, dz);
                        auto it = voxel_map.find(npos);
                        if (it != voxel_map.end() && it->second == cluster.material_id) {
                            if (visited.insert(npos).second) {
                                queue.push_back(npos);
                            }
                        }
                    }
                }
            }
        }

        cluster.count = static_cast<int>(cluster.voxels.size());
        cluster.centroid = sum_pos / static_cast<float>(cluster.count);
        cluster.min_bounds = min_b;
        cluster.max_bounds = max_b;

        // Generate smooth glowing outline & hull around the connected deposit
        generate_cluster_hull(cluster);

        m_clusters.push_back(std::move(cluster));
    }

    // Survey monsters within pulse radius
    update_monsters(stalkers, burrowers);
}

void SurveyingSystem::generate_cluster_hull(SurveyedCluster& cluster) {
    if (cluster.voxels.empty()) return;

    struct VoxelCoordHash {
        size_t operator()(const glm::ivec3& p) const noexcept {
            return (static_cast<size_t>(p.x) * 73856093u) ^
                   (static_cast<size_t>(p.y) * 19349663u) ^
                   (static_cast<size_t>(p.z) * 83492791u);
        }
    };

    std::unordered_set<glm::ivec3, VoxelCoordHash> voxel_set(cluster.voxels.begin(), cluster.voxels.end());

    struct FaceQuad {
        glm::ivec3 corners[4];
        glm::ivec3 normal;
    };

    std::vector<FaceQuad> exterior_faces;
    exterior_faces.reserve(cluster.voxels.size() * 6);

    const glm::ivec3 dirs[6] = {
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0},
        { 0,  0,  1}, { 0,  0, -1}
    };

    for (const auto& v : cluster.voxels) {
        int x = v.x, y = v.y, z = v.z;

        // +X exterior face
        if (voxel_set.find(v + dirs[0]) == voxel_set.end()) {
            exterior_faces.push_back({
                { glm::ivec3(x + 1, y,     z),
                  glm::ivec3(x + 1, y + 1, z),
                  glm::ivec3(x + 1, y + 1, z + 1),
                  glm::ivec3(x + 1, y,     z + 1) },
                dirs[0]
            });
        }
        // -X exterior face
        if (voxel_set.find(v + dirs[1]) == voxel_set.end()) {
            exterior_faces.push_back({
                { glm::ivec3(x, y,     z + 1),
                  glm::ivec3(x, y + 1, z + 1),
                  glm::ivec3(x, y + 1, z),
                  glm::ivec3(x, y,     z) },
                dirs[1]
            });
        }
        // +Y exterior face
        if (voxel_set.find(v + dirs[2]) == voxel_set.end()) {
            exterior_faces.push_back({
                { glm::ivec3(x,     y + 1, z),
                  glm::ivec3(x,     y + 1, z + 1),
                  glm::ivec3(x + 1, y + 1, z + 1),
                  glm::ivec3(x + 1, y + 1, z) },
                dirs[2]
            });
        }
        // -Y exterior face
        if (voxel_set.find(v + dirs[3]) == voxel_set.end()) {
            exterior_faces.push_back({
                { glm::ivec3(x,     y, z + 1),
                  glm::ivec3(x,     y, z),
                  glm::ivec3(x + 1, y, z),
                  glm::ivec3(x + 1, y, z + 1) },
                dirs[3]
            });
        }
        // +Z exterior face
        if (voxel_set.find(v + dirs[4]) == voxel_set.end()) {
            exterior_faces.push_back({
                { glm::ivec3(x,     y,     z + 1),
                  glm::ivec3(x + 1, y,     z + 1),
                  glm::ivec3(x + 1, y + 1, z + 1),
                  glm::ivec3(x,     y + 1, z + 1) },
                dirs[4]
            });
        }
        // -Z exterior face
        if (voxel_set.find(v + dirs[5]) == voxel_set.end()) {
            exterior_faces.push_back({
                { glm::ivec3(x + 1, y,     z),
                  glm::ivec3(x,     y,     z),
                  glm::ivec3(x,     y + 1, z),
                  glm::ivec3(x + 1, y + 1, z) },
                dirs[5]
            });
        }
    }

    if (exterior_faces.empty()) return;

    // Collect unique vertices and build topological surface adjacency graph
    std::unordered_map<glm::ivec3, int, VoxelCoordHash> corner_to_idx;
    std::vector<glm::vec3> unique_verts;
    std::vector<glm::vec3> vert_normals;
    std::vector<std::vector<int>> adj;

    auto get_or_add_vertex = [&](const glm::ivec3& c, const glm::ivec3& face_norm) -> int {
        auto it = corner_to_idx.find(c);
        if (it != corner_to_idx.end()) {
            vert_normals[it->second] += glm::vec3(face_norm);
            return it->second;
        }
        int new_idx = static_cast<int>(unique_verts.size());
        corner_to_idx[c] = new_idx;
        unique_verts.push_back(glm::vec3(c));
        vert_normals.push_back(glm::vec3(face_norm));
        adj.emplace_back();
        return new_idx;
    };

    struct IndexedQuad {
        int idx[4];
        glm::ivec3 normal;
    };
    std::vector<IndexedQuad> indexed_quads;
    indexed_quads.reserve(exterior_faces.size());

    struct EdgeKey {
        int u, v;
        bool operator==(const EdgeKey& o) const { return u == o.u && v == o.v; }
    };
    struct EdgeKeyHash {
        size_t operator()(const EdgeKey& k) const noexcept {
            return (static_cast<size_t>(k.u) * 73856093u) ^ (static_cast<size_t>(k.v) * 19349663u);
        }
    };
    std::unordered_map<EdgeKey, std::vector<glm::ivec3>, EdgeKeyHash> edge_faces;

    for (const auto& f : exterior_faces) {
        IndexedQuad iq;
        iq.normal = f.normal;
        for (int i = 0; i < 4; ++i) {
            iq.idx[i] = get_or_add_vertex(f.corners[i], f.normal);
        }
        indexed_quads.push_back(iq);

        for (int i = 0; i < 4; ++i) {
            int a = iq.idx[i];
            int b = iq.idx[(i + 1) % 4];
            adj[a].push_back(b);
            adj[b].push_back(a);

            int u = std::min(a, b);
            int v = std::max(a, b);
            edge_faces[{u, v}].push_back(f.normal);
        }
    }

    // Deduplicate adjacency lists
    for (auto& nbrs : adj) {
        std::sort(nbrs.begin(), nbrs.end());
        nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
    }

    // Laplacian surface smoothing: rounds blocky 90-degree corners into an organic deposit hull
    std::vector<glm::vec3> smoothed = unique_verts;
    for (int iter = 0; iter < 2; ++iter) {
        std::vector<glm::vec3> next_pos = smoothed;
        for (size_t i = 0; i < smoothed.size(); ++i) {
            if (adj[i].empty()) continue;
            glm::vec3 sum(0.0f);
            for (int n_idx : adj[i]) {
                sum += smoothed[n_idx];
            }
            glm::vec3 avg = sum / static_cast<float>(adj[i].size());
            next_pos[i] = glm::mix(smoothed[i], avg, 0.40f);
        }
        smoothed = next_pos;
    }

    // Outward dilation: ensures the smooth organic hull encloses the deposit volume cleanly
    for (size_t i = 0; i < smoothed.size(); ++i) {
        glm::vec3 n = vert_normals[i];
        if (glm::length(n) > 0.01f) {
            n = glm::normalize(n);
        } else {
            glm::vec3 to_cent = smoothed[i] - cluster.centroid;
            n = (glm::length(to_cent) > 0.01f) ? glm::normalize(to_cent) : glm::vec3(0.0f, 1.0f, 0.0f);
        }
        smoothed[i] += n * 0.06f;
    }

    // Extract smooth glowing outline lines (only silhouette / crease edges, skip internal flat coplanar seams)
    cluster.outline_lines.clear();
    for (const auto& [edge, nlist] : edge_faces) {
        bool is_crease = false;
        if (nlist.size() == 1) {
            is_crease = true;
        } else if (nlist.size() == 2) {
            if (nlist[0] != nlist[1]) {
                is_crease = true;
            }
        } else {
            is_crease = true;
        }

        if (is_crease) {
            cluster.outline_lines.push_back(smoothed[edge.u]);
            cluster.outline_lines.push_back(smoothed[edge.v]);
        }
    }

    // Extract smooth glowing translucent hull triangles
    cluster.hull_triangles.clear();
    for (const auto& iq : indexed_quads) {
        // Tri 1: 0, 1, 2
        cluster.hull_triangles.push_back(smoothed[iq.idx[0]]);
        cluster.hull_triangles.push_back(smoothed[iq.idx[1]]);
        cluster.hull_triangles.push_back(smoothed[iq.idx[2]]);

        // Tri 2: 0, 2, 3
        cluster.hull_triangles.push_back(smoothed[iq.idx[0]]);
        cluster.hull_triangles.push_back(smoothed[iq.idx[2]]);
        cluster.hull_triangles.push_back(smoothed[iq.idx[3]]);
    }
}

void SurveyingSystem::update(float dt) {
    if (!m_active) return;

    m_timer -= dt;
    if (m_timer <= 0.0f) {
        m_timer = 0.0f;
        m_active = false;
        m_surveyed.clear();
        m_clusters.clear();
        m_monsters.clear();
    }
}

void SurveyingSystem::update_monsters(
    const std::vector<VoidStalker>* stalkers,
    const std::vector<SeismicBurrower>* burrowers
) {
    m_monsters.clear();
    if (!m_active) return;

    float r_sq = m_radius * m_radius;

    if (stalkers) {
        for (const auto& s : *stalkers) {
            if (s.is_dead()) continue;
            glm::vec3 diff = s.position - m_origin;
            float dist_sq = glm::dot(diff, diff);
            if (dist_sq <= r_sq) {
                SurveyedMonster sm;
                sm.position = s.position;
                sm.distance = std::sqrt(dist_sq);
                sm.is_burrower = false;
                sm.name = "VOID STALKER";
                sm.min_bounds = s.position - glm::vec3(0.55f, 0.25f, 0.65f);
                sm.max_bounds = s.position + glm::vec3(0.55f, 0.65f, 0.65f);
                generate_monster_hull(sm);
                m_monsters.push_back(std::move(sm));
            }
        }
    }

    if (burrowers) {
        for (const auto& b : *burrowers) {
            if (b.is_dead()) continue;
            glm::vec3 diff = b.position - m_origin;
            float dist_sq = glm::dot(diff, diff);
            if (dist_sq <= r_sq) {
                SurveyedMonster bm;
                bm.position = b.position;
                bm.distance = std::sqrt(dist_sq);
                bm.is_burrower = true;
                bm.name = "SEISMIC BURROWER";
                float sc = b.scale > 0.1f ? b.scale : 1.0f;
                bm.min_bounds = b.position - glm::vec3(0.95f * sc, 0.95f * sc, 1.65f * sc);
                bm.max_bounds = b.position + glm::vec3(0.95f * sc, 0.95f * sc, 1.65f * sc);
                generate_monster_hull(bm);
                m_monsters.push_back(std::move(bm));
            }
        }
    }
}

void SurveyingSystem::generate_monster_hull(SurveyedMonster& monster) {
    monster.outline_lines.clear();
    monster.hull_triangles.clear();

    const glm::vec3& bmin = monster.min_bounds;
    const glm::vec3& bmax = monster.max_bounds;

    glm::vec3 p0(bmin.x, bmin.y, bmin.z);
    glm::vec3 p1(bmax.x, bmin.y, bmin.z);
    glm::vec3 p2(bmax.x, bmax.y, bmin.z);
    glm::vec3 p3(bmin.x, bmax.y, bmin.z);

    glm::vec3 p4(bmin.x, bmin.y, bmax.z);
    glm::vec3 p5(bmax.x, bmin.y, bmax.z);
    glm::vec3 p6(bmax.x, bmax.y, bmax.z);
    glm::vec3 p7(bmin.x, bmax.y, bmax.z);

    auto add_line = [&](const glm::vec3& a, const glm::vec3& b) {
        monster.outline_lines.push_back(a);
        monster.outline_lines.push_back(b);
    };

    // 12 edges for bounding box
    add_line(p0, p1); add_line(p1, p5); add_line(p5, p4); add_line(p4, p0);
    add_line(p3, p2); add_line(p2, p6); add_line(p6, p7); add_line(p7, p3);
    add_line(p0, p3); add_line(p1, p2); add_line(p5, p6); add_line(p4, p7);

    // Acoustic waist scan ring
    glm::vec3 center = (bmin + bmax) * 0.5f;
    float rx = (bmax.x - bmin.x) * 0.52f;
    float rz = (bmax.z - bmin.z) * 0.52f;
    const int RING_SEGS = 16;
    for (int i = 0; i < RING_SEGS; ++i) {
        float a0 = (static_cast<float>(i) / static_cast<float>(RING_SEGS)) * 6.2831853f;
        float a1 = (static_cast<float>(i + 1) / static_cast<float>(RING_SEGS)) * 6.2831853f;
        glm::vec3 pt0 = center + glm::vec3(std::cos(a0) * rx, 0.0f, std::sin(a0) * rz);
        glm::vec3 pt1 = center + glm::vec3(std::cos(a1) * rx, 0.0f, std::sin(a1) * rz);
        add_line(pt0, pt1);
    }

    auto add_quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d) {
        monster.hull_triangles.push_back(a);
        monster.hull_triangles.push_back(b);
        monster.hull_triangles.push_back(c);
        monster.hull_triangles.push_back(a);
        monster.hull_triangles.push_back(c);
        monster.hull_triangles.push_back(d);
    };

    // 6 quads (12 triangles) for translucent volumetric hull
    add_quad(p4, p5, p6, p7);
    add_quad(p1, p0, p3, p2);
    add_quad(p0, p4, p7, p3);
    add_quad(p5, p1, p2, p6);
    add_quad(p3, p7, p6, p2);
    add_quad(p0, p1, p5, p4);
}

float SurveyingSystem::alpha() const {
    if (!m_active || m_duration <= 0.0f) return 0.0f;
    // Immediate linear fade over duration (2.5s)
    return std::clamp(m_timer / m_duration, 0.0f, 1.0f);
}

} // namespace Voidfall
