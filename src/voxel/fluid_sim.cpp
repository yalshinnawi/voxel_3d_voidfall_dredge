#include "fluid_sim.hpp"
#include "world.hpp"
#include "chunk.hpp"

namespace Voidfall {

void FluidSim::Update(World& world, float dt) {
    world.m_fluidSim.Update(dt, world);
}

void FluidSim::Update(float dt) {
    if (s_activeWorld) {
        s_activeWorld->m_fluidSim.Update(dt, *s_activeWorld);
    }
}

void FluidSim::SimulateFluidCell(const glm::ivec3& pos) {
    if (s_activeWorld) {
        SimulateFluidCell(pos, *s_activeWorld);
    }
}

void FluidSim::SimulateFluidCell(const glm::ivec3& pos, World& world) {
    std::unordered_set<Chunk*> dirtyChunks;
    SimulateFluidCell(pos, world, dirtyChunks);
    for (Chunk* c : dirtyChunks) {
        if (c) {
            c->MarkDirty();
            world.queue_chunk_for_meshing(c->get_pos());
        }
    }
}

void FluidSim::SimulateFluidCell(const glm::ivec3& pos, World& world, std::unordered_set<Chunk*>& dirtyChunks) {
    uint8_t currentMat = world.GetBlockMaterial(pos);
    Voxel curVox = world.get_voxel(pos.x, pos.y, pos.z);
    bool isPureLiquid = IsLiquid(currentMat);
    bool isWaterlogged = curVox.is_waterlogged();
    if (!isPureLiquid && !isWaterlogged) return;

    uint8_t fluidMat = isPureLiquid ? currentMat : MAT_WATER;
    int currentLevel = isPureLiquid ? GetFluidLevel(world.GetBlockFlags(pos)) : 5;
    bool isSource = (currentLevel >= 5);

    auto collectDirty = [&](const glm::ivec3& p) {
        int cx = p.x >> 5;
        int cy = p.y >> 5;
        int cz = p.z >> 5;
        Chunk* c = world.get_chunk(ChunkPos{cx, cy, cz});
        if (c) {
            dirtyChunks.insert(c);
            int lx = p.x & 31;
            int ly = p.y & 31;
            int lz = p.z & 31;
            if (lx == 0)              { Chunk* nb = world.get_chunk(ChunkPos{cx - 1, cy, cz}); if (nb) dirtyChunks.insert(nb); }
            if (lx == CHUNK_SIZE - 1) { Chunk* nb = world.get_chunk(ChunkPos{cx + 1, cy, cz}); if (nb) dirtyChunks.insert(nb); }
            if (ly == 0)              { Chunk* nb = world.get_chunk(ChunkPos{cx, cy - 1, cz}); if (nb) dirtyChunks.insert(nb); }
            if (ly == CHUNK_SIZE - 1) { Chunk* nb = world.get_chunk(ChunkPos{cx, cy + 1, cz}); if (nb) dirtyChunks.insert(nb); }
            if (lz == 0)              { Chunk* nb = world.get_chunk(ChunkPos{cx, cy, cz - 1}); if (nb) dirtyChunks.insert(nb); }
            if (lz == CHUNK_SIZE - 1) { Chunk* nb = world.get_chunk(ChunkPos{cx, cy, cz + 1}); if (nb) dirtyChunks.insert(nb); }
        }
    };

    const glm::ivec3 lateralDirs[4] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    // Horizontally adjacent porous sub-blocks become waterlogged upon liquid contact
    if (isPureLiquid) {
        for (const auto& dir : lateralDirs) {
            glm::ivec3 target = pos + dir;
            Voxel targetVox = world.get_voxel(target.x, target.y, target.z);
            if ((targetVox.is_slab() || targetVox.is_ramp() || targetVox.is_corner()) && !targetVox.is_waterlogged()) {
                world.set_waterlogged_cell(target, true);
                WakeFluid(target);
                collectDirty(target);
            }
        }
    }

    // 1. Gravity Downward (Highest Priority for Pure Liquids)
    if (isPureLiquid && pos.y > 0) {
        glm::ivec3 below = pos + glm::ivec3(0, -1, 0);
        uint8_t matBelow = world.GetBlockMaterial(below);
        Voxel voxBelow = world.get_voxel(below.x, below.y, below.z);

        glm::ivec3 above = pos + glm::ivec3(0, 1, 0);
        uint8_t matAbove = world.GetBlockMaterial(above);
        Voxel voxAbove = world.get_voxel(above.x, above.y, above.z);
        bool hasLiquidAbove = IsLiquid(matAbove) || voxAbove.is_waterlogged();

        if (matBelow == MAT_AIR) {
            // Flowing fluid created below is level 4 (never level 5 source)
            world.SetBlockWithFlags(below, fluidMat, 4);
            WakeFluid(below);
            collectDirty(below);

            // If current cell is Flowing and has no liquid stream above feeding it, drain volume down
            if (!isSource && !hasLiquidAbove) {
                world.SetBlock(pos, MAT_AIR);
                collectDirty(pos);
            }
            return; // Falls downward without spreading sideways mid-air
        }

        // Sub-block ramp or slab beneath liquid
        if ((voxBelow.is_slab() || voxBelow.is_ramp() || voxBelow.is_corner()) && !voxBelow.is_waterlogged()) {
            world.set_waterlogged_cell(below, true);
            WakeFluid(below);
            collectDirty(below);
            if (!isSource && !hasLiquidAbove) {
                world.SetBlock(pos, MAT_AIR);
                collectDirty(pos);
            }
            return;
        }

        // Liquid cell below that is not yet full (level < 4)
        if (IsLiquid(matBelow) && GetFluidLevel(world.GetBlockFlags(below)) < 4) {
            world.SetBlockWithFlags(below, fluidMat, 4);
            WakeFluid(below);
            collectDirty(below);
            if (!isSource && !hasLiquidAbove) {
                world.SetBlock(pos, MAT_AIR);
                collectDirty(pos);
            }
            return;
        }
    }

    // 2. Lateral Permeation (Only if supported by solid floor, waterlogged sub-block, or full liquid below)
    bool floor_supported = (pos.y == 0) || (isWaterlogged && !curVox.is_ramp());
    if (pos.y > 0 && !floor_supported) {
        glm::ivec3 below = pos + glm::ivec3(0, -1, 0);
        Voxel voxBelow = world.get_voxel(below.x, below.y, below.z);
        if (voxBelow.is_solid() || voxBelow.is_waterlogged() ||
            (IsLiquid(voxBelow.material_id) && GetFluidLevel(world.GetBlockFlags(below)) >= 4)) {
            floor_supported = true;
        }
    }


    if (floor_supported && currentLevel > 1 && !curVox.is_ramp()) {
        int baseTargetLevel = currentLevel - 1;
        for (const auto& dir : lateralDirs) {
            glm::ivec3 target = pos + dir;
            if (target.y > pos.y) continue; // Anti-stacking: fluid never moves upward

            uint8_t targetMat = world.GetBlockMaterial(target);
            Voxel targetVox = world.get_voxel(target.x, target.y, target.z);

            int targetLevel = baseTargetLevel;
            // Lake Infilling: Two adjacent source blocks (level 5) infill an empty pocket on a solid floor to a source block (level 5)
            if (target.y == 0 || world.get_voxel(target.x, target.y - 1, target.z).is_solid()) {
                int adjacentSources = 0;
                for (const auto& d : lateralDirs) {
                    glm::ivec3 nb = target + d;
                    uint8_t m = world.GetBlockMaterial(nb);
                    if (IsLiquid(m) && GetFluidLevel(world.GetBlockFlags(nb)) >= 5) {
                        adjacentSources++;
                    }
                }
                if (adjacentSources >= 2) {
                    targetLevel = 5;
                }
            }

            if (targetMat == MAT_AIR) {
                world.SetBlockWithFlags(target, fluidMat, targetLevel);
                WakeFluid(target);
                collectDirty(target);
            } else if (targetMat == fluidMat) {
                int existingLevel = GetFluidLevel(world.GetBlockFlags(target));
                if (existingLevel < targetLevel) {
                    world.SetBlockWithFlags(target, fluidMat, targetLevel);
                    WakeFluid(target);
                    collectDirty(target);
                }
            } else if ((targetVox.is_slab() || targetVox.is_ramp() || targetVox.is_corner()) && !targetVox.is_waterlogged()) {
                world.set_waterlogged_cell(target, true);
                WakeFluid(target);
                collectDirty(target);
            }
        }
    }

    // 3. Slope Downhill Conformance
    if (curVox.is_ramp() && isWaterlogged) {
        glm::ivec3 downhill_offset(0);
        switch (curVox.shape()) {
            case SHAPE_RAMP_EAST:  downhill_offset = glm::ivec3(-1, 0, 0); break;
            case SHAPE_RAMP_WEST:  downhill_offset = glm::ivec3(1, 0, 0);  break;
            case SHAPE_RAMP_SOUTH: downhill_offset = glm::ivec3(0, 0, -1); break;
            case SHAPE_RAMP_NORTH: downhill_offset = glm::ivec3(0, 0, 1);  break;
            default: break;
        }
        if (downhill_offset != glm::ivec3(0)) {
            glm::ivec3 downhill_pos = pos + downhill_offset;
            if (downhill_pos.y <= pos.y) {
                Voxel dn_vox = world.get_voxel(downhill_pos.x, downhill_pos.y, downhill_pos.z);
                int dnLevel = currentLevel > 1 ? currentLevel - 1 : 1;
                if (dn_vox.material_id == MAT_AIR) {
                    world.SetBlockWithFlags(downhill_pos, fluidMat, dnLevel);
                    WakeFluid(downhill_pos);
                    collectDirty(downhill_pos);
                } else if ((dn_vox.is_slab() || dn_vox.is_ramp() || dn_vox.is_corner()) && !dn_vox.is_waterlogged()) {
                    world.set_waterlogged_cell(downhill_pos, true);
                    WakeFluid(downhill_pos);
                    collectDirty(downhill_pos);
                }
            }
        }
    }
}

void FluidSim::Update(float dt, World& world) {
    m_accumulator += dt;
    constexpr float TICK_INTERVAL = 0.0833f; // 12 Hz tick
    constexpr int MAX_FLUID_STEPS_PER_TICK = 128; // Hard evaluation ceiling

    // Active Queue Ceiling: If > 1024, drop oldest non-source fluid updates
    if (m_activeFluids.size() > 1024) {
        std::deque<glm::ivec3> pruned;
        size_t excess = m_activeFluids.size() - 1024;
        for (const auto& p : m_activeFluids) {
            uint8_t mat = world.GetBlockMaterial(p);
            bool isSource = IsLiquid(mat) && (GetFluidLevel(world.GetBlockFlags(p)) >= 5);
            if (!isSource && excess > 0) {
                m_activeFluidSet.erase(p);
                excess--;
            } else {
                pruned.push_back(p);
            }
        }
        m_activeFluids = std::move(pruned);
        while (m_activeFluids.size() > 1024) {
            glm::ivec3 front = m_activeFluids.front();
            m_activeFluids.pop_front();
            m_activeFluidSet.erase(front);
        }
    }

    m_last_steps_processed = 0;
    while (m_accumulator >= TICK_INTERVAL) {
        m_accumulator -= TICK_INTERVAL;
        int stepsProcessed = 0;
        std::unordered_set<Chunk*> dirtyChunks;

        size_t batchSize = std::min(m_activeFluids.size(), static_cast<size_t>(MAX_FLUID_STEPS_PER_TICK));
        while (stepsProcessed < static_cast<int>(batchSize) && !m_activeFluids.empty()) {
            glm::ivec3 pos = m_activeFluids.front();
            m_activeFluids.pop_front();
            m_activeFluidSet.erase(pos);

            SimulateFluidCell(pos, world, dirtyChunks);
            stepsProcessed++;
        }
        m_last_steps_processed += stepsProcessed;

        // Mark affected chunks dirty in a single batch at the end of the tick
        for (Chunk* chunk : dirtyChunks) {
            if (chunk) {
                chunk->MarkDirty();
                world.queue_chunk_for_meshing(chunk->get_pos());
            }
        }
    }
}

} // namespace Voidfall
