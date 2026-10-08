#include "fluid_sim.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include <array>

namespace Voidfall {

static inline int floor_mod(int a, int b) {
    int rem = a % b;
    return rem < 0 ? rem + b : rem;
}

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

bool FluidSim::SimulateAirCell(World& world, const glm::ivec3& pos) {
    std::unordered_set<Chunk*> localDirty;
    bool res = SimulateAirCell(world, pos, localDirty);
    for (Chunk* c : localDirty) {
        if (c) c->MarkDirty();
    }
    return res;
}

bool FluidSim::SimulateAirCell(World& world, const glm::ivec3& pos, std::unordered_set<Chunk*>& dirtyChunks) {
    Voxel cur = world.get_voxel(pos.x, pos.y, pos.z);
    if (cur.material_id != MAT_AIR) {
        return false;
    }
    if (pos.y <= 0) return false;

    glm::ivec3 below_pos(pos.x, pos.y - 1, pos.z);
    Voxel below = world.get_voxel(below_pos.x, below_pos.y, below_pos.z);
    if (!below.is_solid()) {
        return false;
    }

    static const glm::ivec3 HORIZ_4[4] = {
        glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
        glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)
    };

    int sourceCount = 0;
    uint8_t liquidMat = MAT_WATER;
    for (const auto& offset : HORIZ_4) {
        glm::ivec3 n_pos = pos + offset;
        uint8_t n_mat = world.GetBlockMaterial(n_pos);
        if (IsLiquid(n_mat)) {
            uint8_t n_flags = world.GetBlockFlags(n_pos);
            if (GetFluidLevel(n_flags) == 5) {
                sourceCount++;
                liquidMat = n_mat;
            }
        }
    }

    if (sourceCount >= 2) {
        world.SetBlockWithFlags(pos, liquidMat, 5);
        world.PushActiveFluid(pos);

        auto mark_chunk_dirty = [&](const glm::ivec3& p) {
            Chunk* c = world.GetChunkFromBlockPos(p);
            if (c) dirtyChunks.insert(c);
            int lx = floor_mod(p.x, CHUNK_SIZE);
            int ly = floor_mod(p.y, CHUNK_SIZE);
            int lz = floor_mod(p.z, CHUNK_SIZE);
            if (lx == 0)              { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(-1, 0, 0)); if (nc) dirtyChunks.insert(nc); }
            if (lx == CHUNK_SIZE - 1) { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(1, 0, 0));  if (nc) dirtyChunks.insert(nc); }
            if (ly == 0)              { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(0, -1, 0)); if (nc) dirtyChunks.insert(nc); }
            if (ly == CHUNK_SIZE - 1) { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(0, 1, 0));  if (nc) dirtyChunks.insert(nc); }
            if (lz == 0)              { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(0, 0, -1)); if (nc) dirtyChunks.insert(nc); }
            if (lz == CHUNK_SIZE - 1) { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(0, 0, 1));  if (nc) dirtyChunks.insert(nc); }
        };
        mark_chunk_dirty(pos);
        return true;
    }
    return false;
}

void FluidSim::SimulateFluidCell(World& world, const glm::ivec3& pos, std::unordered_set<Chunk*>& dirtyChunks) {
    Voxel cur = world.get_voxel(pos.x, pos.y, pos.z);
    if (cur.material_id == MAT_AIR) {
        SimulateAirCell(world, pos, dirtyChunks);
        return;
    }

    bool pure_liquid = IsLiquid(cur.material_id);
    bool waterlogged = cur.is_waterlogged();

    if (!pure_liquid && !waterlogged) {
        return; // Inactive or drained
    }

    uint8_t liquid_mat = pure_liquid ? cur.material_id : MAT_WATER;
    uint8_t cur_level = pure_liquid ? static_cast<uint8_t>(GetFluidLevel(cur.flags_and_damage)) : 5;

    auto mark_chunk_dirty = [&](const glm::ivec3& p) {
        Chunk* c = world.GetChunkFromBlockPos(p);
        if (c) dirtyChunks.insert(c);
        int lx = floor_mod(p.x, CHUNK_SIZE);
        int ly = floor_mod(p.y, CHUNK_SIZE);
        int lz = floor_mod(p.z, CHUNK_SIZE);
        if (lx == 0)              { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(-1, 0, 0)); if (nc) dirtyChunks.insert(nc); }
        if (lx == CHUNK_SIZE - 1) { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(1, 0, 0));  if (nc) dirtyChunks.insert(nc); }
        if (ly == 0)              { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(0, -1, 0)); if (nc) dirtyChunks.insert(nc); }
        if (ly == CHUNK_SIZE - 1) { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(0, 1, 0));  if (nc) dirtyChunks.insert(nc); }
        if (lz == 0)              { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(0, 0, -1)); if (nc) dirtyChunks.insert(nc); }
        if (lz == CHUNK_SIZE - 1) { Chunk* nc = world.GetChunkFromBlockPos(p + glm::ivec3(0, 0, 1));  if (nc) dirtyChunks.insert(nc); }
    };

    static const glm::ivec3 HORIZ_4[4] = {
        glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
        glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)
    };

    // ─────────────────────────────────────────────────────────────
    // 1. PRIORITY 1: GRAVITY DOWNWARD FLOW (Falls & Cascades)
    // ─────────────────────────────────────────────────────────────
    if (pos.y > 0) {
        glm::ivec3 below_pos(pos.x, pos.y - 1, pos.z);
        Voxel below = world.get_voxel(below_pos.x, below_pos.y, below_pos.z);

        if (below.material_id == MAT_AIR) {
            // Set cell below to current liquid material with Level 5 (falling column)
            world.SetBlockWithFlags(below_pos, liquid_mat, 5);
            world.PushActiveFluid(below_pos);
            mark_chunk_dirty(pos);
            mark_chunk_dirty(below_pos);
            // Terminate step (do not spread horizontally from mid-air falling columns).
            return;
        }

        // Sub-block ramp or slab beneath liquid
        if ((below.is_slab() || below.is_ramp() || below.is_corner()) && !below.is_waterlogged()) {
            world.set_waterlogged_cell(below_pos, true);
            world.PushActiveFluid(below_pos);
            mark_chunk_dirty(pos);
            mark_chunk_dirty(below_pos);
            world.check_wake_fluid_around(below_pos);
            return;
        }

        // Liquid cell below that is not yet full
        if (IsLiquid(below.material_id) && below.fluid_level() < 5) {
            world.SetBlockWithFlags(below_pos, liquid_mat, 5);
            world.PushActiveFluid(below_pos);
            mark_chunk_dirty(pos);
            mark_chunk_dirty(below_pos);
            return;
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 2. PRIORITY 2: LATERAL PERMEATION (Same Level & Over Ledges)
    // ─────────────────────────────────────────────────────────────
    // If cell below is solid rock, a waterlogged sub-block, or another liquid block:
    bool floor_supported = (pos.y == 0);
    if (pos.y > 0) {
        glm::ivec3 below_pos(pos.x, pos.y - 1, pos.z);
        Voxel below = world.get_voxel(below_pos.x, below_pos.y, below_pos.z);
        if (below.is_solid() || below.is_waterlogged() || IsLiquid(below.material_id)) {
            floor_supported = true;
        }
    }

    if (floor_supported) {
        int currentLevel = GetFluidLevel(world.GetBlockFlags(pos));
        if (currentLevel > 1) {
            for (const auto& offset : HORIZ_4) {
                glm::ivec3 targetPos = pos + offset;
                uint8_t targetMat = world.GetBlockMaterial(targetPos);
                Voxel targetVox = world.get_voxel(targetPos.x, targetPos.y, targetPos.z);

                if (targetMat == MAT_AIR) {
                    // Check Step 3: Pool Infilling (Eliminating 1x1 Dry Holes in Lakes)
                    bool infilled = false;
                    if (targetPos.y > 0 && world.get_voxel(targetPos.x, targetPos.y - 1, targetPos.z).is_solid()) {
                        int adjacent_sources = 0;
                        for (const auto& check_off : HORIZ_4) {
                            glm::ivec3 check_pos = targetPos + check_off;
                            uint8_t c_mat = world.GetBlockMaterial(check_pos);
                            if (IsLiquid(c_mat) && GetFluidLevel(world.GetBlockFlags(check_pos)) == 5) {
                                adjacent_sources++;
                            }
                        }
                        if (adjacent_sources >= 2) {
                            world.SetBlockWithFlags(targetPos, liquid_mat, 5);
                            world.PushActiveFluid(targetPos);
                            mark_chunk_dirty(targetPos);
                            infilled = true;
                        }
                    }

                    if (!infilled) {
                        // DO NOT REQUIRE A SOLID FLOOR BENEATH targetPos!
                        // If over cliff/drop, liquid still flows into targetPos with currentLevel - 1.
                        world.SetBlockWithFlags(targetPos, liquid_mat, currentLevel - 1);
                        world.PushActiveFluid(targetPos);
                        mark_chunk_dirty(targetPos);
                    }
                } else if (targetMat == liquid_mat) {
                    int targetLevel = GetFluidLevel(world.GetBlockFlags(targetPos));
                    // Check if target can be upgraded to full source block via infill
                    bool infilled = false;
                    if (targetLevel < 5 && targetPos.y > 0 && world.get_voxel(targetPos.x, targetPos.y - 1, targetPos.z).is_solid()) {
                        int adjacent_sources = 0;
                        for (const auto& check_off : HORIZ_4) {
                            glm::ivec3 check_pos = targetPos + check_off;
                            uint8_t c_mat = world.GetBlockMaterial(check_pos);
                            if (IsLiquid(c_mat) && GetFluidLevel(world.GetBlockFlags(check_pos)) == 5) {
                                adjacent_sources++;
                            }
                        }
                        if (adjacent_sources >= 2) {
                            world.SetBlockWithFlags(targetPos, liquid_mat, 5);
                            world.PushActiveFluid(targetPos);
                            mark_chunk_dirty(targetPos);
                            infilled = true;
                        }
                    }
                    if (!infilled && targetLevel < currentLevel - 1) {
                        world.SetBlockWithFlags(targetPos, liquid_mat, currentLevel - 1);
                        world.PushActiveFluid(targetPos);
                        mark_chunk_dirty(targetPos);
                    }
                } else if ((targetVox.is_slab() || targetVox.is_ramp() || targetVox.is_corner()) && !targetVox.is_waterlogged()) {
                    world.set_waterlogged_cell(targetPos, true);
                    world.PushActiveFluid(targetPos);
                    mark_chunk_dirty(targetPos);
                }
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
                world.SetBlockWithFlags(downhill_pos, liquid_mat, cur_level > 1 ? cur_level - 1 : 1);
                world.PushActiveFluid(downhill_pos);
                mark_chunk_dirty(downhill_pos);
            } else if ((dn_vox.is_slab() || dn_vox.is_ramp()) && !dn_vox.is_waterlogged()) {
                world.set_waterlogged_cell(downhill_pos, true);
                world.PushActiveFluid(downhill_pos);
                mark_chunk_dirty(downhill_pos);
            }
        }
    }
}

} // namespace Voidfall
