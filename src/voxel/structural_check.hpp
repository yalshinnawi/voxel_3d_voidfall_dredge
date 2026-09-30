#pragma once
#include "world.hpp"
#include <vector>
#include <glm/glm.hpp>

namespace Voidfall {

struct DebrisBlock {
    glm::ivec3 local_offset; // Relative to cluster center of mass
    Voxel voxel;
};

struct UnanchoredIsland {
    glm::vec3 center_of_mass{0.0f};
    std::vector<glm::ivec3> world_positions;
    std::vector<DebrisBlock> blocks;
    uint8_t primary_material{MAT_FRACTURED_GRANITE};
};

class StructuralCheck {
public:
    // Runs anchored BFS around the destroyed block position
    // Returns unanchored islands detected that should become DynamicDebris
    static std::vector<UnanchoredIsland> solve_cavein(
        World& world,
        int destroyed_x,
        int destroyed_y,
        int destroyed_z,
        size_t max_search_nodes = 1024
    );
};

} // namespace Voidfall
