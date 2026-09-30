#include "structural_check.hpp"
#include <queue>
#include <unordered_set>

namespace Voidfall {

struct Vec3Hash {
    std::size_t operator()(const glm::ivec3& v) const noexcept {
        std::size_t h = 2166136261u;
        h = (h ^ static_cast<std::size_t>(v.x)) * 16777619u;
        h = (h ^ static_cast<std::size_t>(v.y)) * 16777619u;
        h = (h ^ static_cast<std::size_t>(v.z)) * 16777619u;
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

        queue.push(start);
        island_visited.insert(start);
        island_blocks.push_back(start);

        bool is_anchored = false;

        while (!queue.empty() && island_blocks.size() < max_search_nodes) {
            glm::ivec3 current = queue.front();
            queue.pop();

            Voxel cur_vox = world.get_voxel(current.x, current.y, current.z);

            // If we touch bedrock or an indestructible anchor, this entire component is stable
            if (cur_vox.is_anchored() || current.y <= 1) {
                is_anchored = true;
                break;
            }

            for (const auto& ndir : neighbor_dirs) {
                glm::ivec3 next = current + ndir;
                if (island_visited.count(next)) continue;

                Voxel next_vox = world.get_voxel(next.x, next.y, next.z);
                if (next_vox.is_solid()) {
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

            // Build relative block list and detach from world grid
            for (const auto& b : island_blocks) {
                Voxel v = world.get_voxel(b.x, b.y, b.z);
                island.blocks.push_back(DebrisBlock{
                    b - glm::ivec3(std::floor(island.center_of_mass.x),
                                   std::floor(island.center_of_mass.y),
                                   std::floor(island.center_of_mass.z)),
                    v
                });
                island.primary_material = v.material_id;
                // Remove from static terrain
                world.set_voxel(b.x, b.y, b.z, Voxel{MAT_AIR, 0}, true);
            }

            unanchored_islands.push_back(std::move(island));
        }
    }

    return unanchored_islands;
}

} // namespace Voidfall
