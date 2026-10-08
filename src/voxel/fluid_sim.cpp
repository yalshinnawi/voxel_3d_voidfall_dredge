#include "fluid_sim.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include <array>

namespace Voidfall {

void FluidSim::Update(World& world, float dt) {
    world.update_fluids(dt);
}

void FluidSim::Update(float dt) {
    if (s_activeWorld) {
        s_activeWorld->update_fluids(dt);
    }
}

void FluidSim::SimulateFluidCell(World& world, const glm::ivec3& pos) {
    std::unordered_set<Chunk*> localDirty;
    SimulateFluidCell(world, pos, localDirty);
    for (Chunk* c : localDirty) {
        if (c) c->MarkDirty();
    }
}

void FluidSim::SimulateFluidCell(World& world, const glm::ivec3& pos, std::unordered_set<Chunk*>& dirtyChunks) {
    Voxel cur = world.get_voxel(pos.x, pos.y, pos.z);
    bool pure_liquid = IsLiquid(cur.material_id);
    bool waterlogged = cur.is_waterlogged();

    if (!pure_liquid && !waterlogged) {
        return; // Inactive or drained
    }

    uint8_t liquid_mat = pure_liquid ? cur.material_id : MAT_WATER;
    uint8_t cur_level = pure_liquid ? (cur.fluid_level() == 0 ? 5 : cur.fluid_level()) : 5;

    auto mark_chunk_dirty = [&](const glm::ivec3& p) {
        int cx = p.x >> 5;
        int cy = p.y >> 5;
        int cz = p.z >> 5;
        Chunk* c = world.get_chunk(ChunkPos{cx, cy, cz});
        if (c) dirtyChunks.insert(c);
    };

    // ─────────────────────────────────────────────────────────────
    // 1. PRIORITY 1: GRAVITY DOWNWARD FLOW
    // ─────────────────────────────────────────────────────────────
    if (pos.y > 0) {
        glm::ivec3 below_pos(pos.x, pos.y - 1, pos.z);
        Voxel below = world.get_voxel(below_pos.x, below_pos.y, below_pos.z);

        if (below.material_id == MAT_AIR) {
            // Set cell below to current liquid material with source level 5
            world.set_fluid_cell(below_pos, liquid_mat, 5, false);
            world.WakeFluid(below_pos);
            mark_chunk_dirty(below_pos);
            // Terminate step (liquid falls straight down without spreading sideways in mid-air).
            return;
        }

        // Sub-block ramp or slab beneath liquid
        if ((below.is_slab() || below.is_ramp() || below.is_corner()) && !below.is_waterlogged()) {
            world.set_waterlogged_cell(below_pos, true);
            world.WakeFluid(below_pos);
            mark_chunk_dirty(below_pos);
            // Wake adjacent neighbors of newly waterlogged sub-block
            world.check_wake_fluid_around(below_pos);
            return;
        }

        // Liquid cell below that is not yet full
        if (IsLiquid(below.material_id) && below.fluid_level() < 5) {
            world.set_fluid_cell(below_pos, liquid_mat, 5, false);
            world.WakeFluid(below_pos);
            mark_chunk_dirty(below_pos);
            return;
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 2. PRIORITY 2: LATERAL SPREADING (when cell below is solid or full liquid)
    // ─────────────────────────────────────────────────────────────
    if (cur_level > 1) {
        static const glm::ivec3 HORIZ_4[4] = {
            glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
            glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)
        };

        for (const auto& offset : HORIZ_4) {
            glm::ivec3 n_pos = pos + offset;
            Voxel n_vox = world.get_voxel(n_pos.x, n_pos.y, n_pos.z);

            if (n_vox.material_id == MAT_AIR) {
                world.set_fluid_cell(n_pos, liquid_mat, cur_level - 1, false);
                world.WakeFluid(n_pos);
                mark_chunk_dirty(n_pos);
            } else if (n_vox.material_id == liquid_mat && n_vox.fluid_level() < cur_level - 1) {
                world.set_fluid_cell(n_pos, liquid_mat, cur_level - 1, false);
                world.WakeFluid(n_pos);
                mark_chunk_dirty(n_pos);
            } else if ((n_vox.is_slab() || n_vox.is_ramp() || n_vox.is_corner()) && !n_vox.is_waterlogged()) {
                world.set_waterlogged_cell(n_pos, true);
                world.WakeFluid(n_pos);
                mark_chunk_dirty(n_pos);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 3. SLOPE DOWNHILL CONFORMANCE
    // ─────────────────────────────────────────────────────────────
    if (cur.is_ramp() && waterlogged) {
        glm::ivec3 downhill_offset(0);
        switch (cur.shape()) {
            case SHAPE_RAMP_EAST:  downhill_offset = glm::ivec3(-1, 0, 0); break;
            case SHAPE_RAMP_WEST:  downhill_offset = glm::ivec3(1, 0, 0);  break;
            case SHAPE_RAMP_SOUTH: downhill_offset = glm::ivec3(0, 0, -1); break;
            case SHAPE_RAMP_NORTH: downhill_offset = glm::ivec3(0, 0, 1);  break;
            default: break;
        }

        if (downhill_offset != glm::ivec3(0)) {
            glm::ivec3 downhill_pos = pos + downhill_offset;
            Voxel dn_vox = world.get_voxel(downhill_pos.x, downhill_pos.y, downhill_pos.z);
            if (dn_vox.material_id == MAT_AIR) {
                world.set_fluid_cell(downhill_pos, liquid_mat, cur_level > 1 ? cur_level - 1 : 1, false);
                world.WakeFluid(downhill_pos);
                mark_chunk_dirty(downhill_pos);
            } else if ((dn_vox.is_slab() || dn_vox.is_ramp()) && !dn_vox.is_waterlogged()) {
                world.set_waterlogged_cell(downhill_pos, true);
                world.WakeFluid(downhill_pos);
                mark_chunk_dirty(downhill_pos);
            }
        }
    }
}

} // namespace Voidfall
