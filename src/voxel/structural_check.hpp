#pragma once
#include "world.hpp"
#include <vector>
#include <glm/glm.hpp>

namespace Voidfall {

struct UnanchoredIsland {
    glm::vec3 center_of_mass{0.0f};
    std::vector<glm::ivec3> world_positions;
    std::vector<DebrisBlock> blocks;
    uint8_t primary_material{MAT_FRACTURED_GRANITE};
};

class StructuralCheck {
public:
    // Checks if voxel a and voxel b share physical contact across their boundary in direction (dx, dy, dz) = (b - a).
    // Slabs and sub-block shapes only establish load-bearing and face contact when substance meets flush across the boundary.
    static inline bool are_voxels_face_connected(const Voxel& a, const Voxel& b, int dx, int dy, int dz) {
        if (!a.is_solid() || !b.is_solid()) return false;
        if (dy == 1) { // b is above a
            if (a.shape() == SHAPE_SLAB_BOTTOM) return false;
            if (b.shape() == SHAPE_SLAB_TOP) return false;
            return true;
        }
        if (dy == -1) { // b is below a
            if (a.shape() == SHAPE_SLAB_TOP) return false;
            if (b.shape() == SHAPE_SLAB_BOTTOM) return false;
            return true;
        }
        // Horizontal adjacency (dy == 0)
        if (a.shape() == SHAPE_SLAB_BOTTOM && b.shape() == SHAPE_SLAB_TOP) return false;
        if (a.shape() == SHAPE_SLAB_TOP && b.shape() == SHAPE_SLAB_BOTTOM) return false;
        return true;
    }

    // Runs anchored BFS around the destroyed block position
    // Returns unanchored islands detected that should become DynamicDebris
    static std::vector<UnanchoredIsland> solve_cavein(
        World& world,
        int destroyed_x,
        int destroyed_y,
        int destroyed_z,
        size_t max_search_nodes = 64
    );

    // Queries 3 to 6 stone ceiling blocks (MAT_GRANITE, MAT_BASALT) 3-10 units above player within 6-block radius
    // Ignores blocks supported by adjacent player-placed MAT_BULKHEAD blocks
    static std::vector<glm::ivec3> query_seismic_detachment_blocks(
        const World& world,
        const glm::vec3& player_pos,
        int min_blocks = 3,
        int max_blocks = 6,
        float radius = 6.0f,
        int min_y_offset = 3,
        int max_y_offset = 10
    );

    // Counts solid unanchored/floating blocks that cannot trace a continuous solid
    // face-connected path to foundational anchors (bedrock floor, ceiling, or boundary walls)
    static int count_unanchored_blocks(const World& world);

    // Scans the newly created/generated world and guarantees all blocks and objects
    // are structurally connected to anchor foundations (floor, walls, ceiling).
    // Connects unanchored/floating blocks via support pillars or grounding,
    // and cleans up any floating/disconnected props.
    // Returns the number of unanchored components/islands connected.
    static int ensure_world_connectivity(World& world);
};

} // namespace Voidfall
