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

static inline void CollectDirtyChunks(World& world, const glm::ivec3& p, std::unordered_set<Chunk*>& dirtyChunks) {
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
}

void FluidSim::TrySpreadToNeighbor(World& world, const glm::ivec3& targetPos, 
                                   uint8_t liquidMat, int newLevel, 
                                   std::unordered_set<Chunk*>& dirtyChunks,
                                   bool immediate) {
    uint8_t targetMat = world.GetBlockMaterial(targetPos);
    uint8_t targetFlags = world.GetBlockFlags(targetPos);
    VoxelShape targetShape = world.GetShape(targetPos);

    if (targetMat != MAT_AIR && ShapeGeometry::IsFullCube(targetShape) && targetMat != liquidMat) {
        return; // Impassable solid block
    }

    if (targetMat == liquidMat) {
        int existingLevel = GetFluidLevel(targetFlags);
        if (existingLevel >= newLevel) {
            return;
        }
    }

    if (!ShapeGeometry::IsFullCube(targetShape) && (targetFlags & VOXEL_FLAG_WATERLOGGED)) {
        int existingLevel = GetFluidLevel(targetFlags);
        if (existingLevel >= newLevel) {
            return;
        }
    }

    if (immediate) {
        if (targetMat == MAT_AIR) {
            // Standard full-block spread
            world.SetBlockWithFlags(targetPos, liquidMat, newLevel);
            WakeFluid(targetPos);
            CollectDirtyChunks(world, targetPos, dirtyChunks);
        } 
        else if (!ShapeGeometry::IsFullCube(targetShape)) {
            // Target is a partial block (ramp, slab, wedge): waterlog it without destroying solid rock
            uint8_t updatedFlags = (targetFlags & ~VOXEL_DAMAGE_MASK) | (newLevel & VOXEL_DAMAGE_MASK) | VOXEL_FLAG_WATERLOGGED;
            world.SetBlockFlags(targetPos, updatedFlags);
            WakeFluid(targetPos);
            CollectDirtyChunks(world, targetPos, dirtyChunks);

            // Downward ramp shedding vector
            glm::ivec3 shedDir(0);
            switch (targetShape) {
                case SHAPE_RAMP_EAST:  shedDir = glm::ivec3(-1, 0, 0); break;
                case SHAPE_RAMP_WEST:  shedDir = glm::ivec3( 1, 0, 0); break;
                case SHAPE_RAMP_SOUTH: shedDir = glm::ivec3( 0, 0,-1); break;
                case SHAPE_RAMP_NORTH: shedDir = glm::ivec3( 0, 0, 1); break;
                default: break;
            }
            if (shedDir != glm::ivec3(0)) {
                uint8_t shedMat = world.GetBlockMaterial(targetPos + shedDir);
                uint8_t shedFlags = world.GetBlockFlags(targetPos + shedDir);
                if (IsLiquid(shedMat) || (shedFlags & VOXEL_FLAG_WATERLOGGED)) {
                    WakeFluid(targetPos + shedDir);
                }
            }

            // Wake neighbors downstream/downward from this partial block
            glm::ivec3 belowPos = targetPos + glm::ivec3(0, -1, 0);
            uint8_t belowMat = world.GetBlockMaterial(belowPos);
            uint8_t belowFlags = world.GetBlockFlags(belowPos);
            if (IsLiquid(belowMat) || (belowFlags & VOXEL_FLAG_WATERLOGGED)) {
                WakeFluid(belowPos);
            }
        }
        else if (targetMat == liquidMat) {
            // Merge and update level if higher
            int existingLevel = GetFluidLevel(targetFlags);
            if (existingLevel < newLevel) {
                world.SetBlockFlags(targetPos, (targetFlags & ~VOXEL_DAMAGE_MASK) | (newLevel & VOXEL_DAMAGE_MASK));
                WakeFluid(targetPos);
                CollectDirtyChunks(world, targetPos, dirtyChunks);
            }
        }
    } else {
        // Lateral / timed propagation: queue into scheduled spreads with material delay
        int delay = GetSpreadDelayTicks(liquidMat);
        auto it = m_scheduledSpreadMap.find(targetPos);
        if (it != m_scheduledSpreadMap.end()) {
            size_t idx = it->second;
            if (idx < m_scheduledSpreads.size() && m_scheduledSpreads[idx].pos == targetPos) {
                if (newLevel > m_scheduledSpreads[idx].level) {
                    m_scheduledSpreads[idx].level = newLevel;
                    m_scheduledSpreads[idx].material = liquidMat;
                }
                return;
            }
        }
        if (m_scheduledSpreads.size() < 1024) {
            m_scheduledSpreadMap[targetPos] = m_scheduledSpreads.size();
            m_scheduledSpreads.push_back(ScheduledFluidUpdate{targetPos, liquidMat, newLevel, delay});
        }
    }
}

void FluidSim::SimulateFluidCell(const glm::ivec3& pos, World& world, std::unordered_set<Chunk*>& dirtyChunks) {
    uint8_t currentMat = world.GetBlockMaterial(pos);
    Voxel curVox = world.get_voxel(pos.x, pos.y, pos.z);
    bool isPureLiquid = IsLiquid(currentMat);
    bool isWaterlogged = curVox.is_waterlogged();
    if (!isPureLiquid && !isWaterlogged) return;

    uint8_t fluidMat = currentMat;
    if (!isPureLiquid) {
        // Waterlogged sub-block: search adjacent neighbors to detect liquid type
        const glm::ivec3 searchDirs[6] = {{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
        for (const auto& d : searchDirs) {
            uint8_t m = world.GetBlockMaterial(pos + d);
            if (IsLiquid(m)) {
                fluidMat = m;
                break;
            }
        }
        if (!IsLiquid(fluidMat)) {
            fluidMat = MAT_WATER;
        }
    }

    int currentLevel = GetFluidLevel(world.GetBlockFlags(pos));
    bool isSource = (currentLevel >= 5);

    auto collectDirty = [&](const glm::ivec3& p) {
        CollectDirtyChunks(world, p, dirtyChunks);
    };

    const glm::ivec3 lateralDirs[4] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    // Horizontally adjacent porous sub-blocks become waterlogged upon liquid contact
    if (isPureLiquid) {
        for (const auto& dir : lateralDirs) {
            glm::ivec3 target = pos + dir;
            uint8_t targetFlags = world.GetBlockFlags(target);
            VoxelShape targetShape = static_cast<VoxelShape>(targetFlags & VOXEL_SHAPE_MASK);
            if (!ShapeGeometry::IsFullCube(targetShape) && !(targetFlags & VOXEL_FLAG_WATERLOGGED)) {
                TrySpreadToNeighbor(world, target, fluidMat, currentLevel > 1 ? currentLevel - 1 : 1, dirtyChunks, false);
                collectDirty(target);
            }
        }
    }

    // 1. Gravity Downward (Highest Priority for Pure Liquids - Immediate Flow)
    if (isPureLiquid && pos.y > 0) {
        glm::ivec3 below = pos + glm::ivec3(0, -1, 0);
        uint8_t matBelow = world.GetBlockMaterial(below);
        Voxel voxBelow = world.get_voxel(below.x, below.y, below.z);

        glm::ivec3 above = pos + glm::ivec3(0, 1, 0);
        uint8_t matAbove = world.GetBlockMaterial(above);
        Voxel voxAbove = world.get_voxel(above.x, above.y, above.z);
        bool hasLiquidAbove = IsLiquid(matAbove) || voxAbove.is_waterlogged();

        // Check if there is an incoming feeder (liquid from above, higher-level liquid horizontally, or a ramp shedding into pos)
        bool hasLiquidFeeder = hasLiquidAbove;
        if (!hasLiquidFeeder) {
            for (const auto& d : lateralDirs) {
                glm::ivec3 nb = pos + d;
                uint8_t m = world.GetBlockMaterial(nb);
                uint8_t flags = world.GetBlockFlags(nb);
                Voxel nbVox = world.get_voxel(nb.x, nb.y, nb.z);
                if (m == fluidMat) {
                    int nbLevel = GetFluidLevel(flags);
                    if (nbLevel > currentLevel || nbLevel >= 5) {
                        hasLiquidFeeder = true;
                        break;
                    }
                }
                if (nbVox.is_waterlogged() && nbVox.is_ramp()) {
                    glm::ivec3 shedDir(0);
                    switch (nbVox.shape()) {
                        case SHAPE_RAMP_EAST:  shedDir = glm::ivec3(-1, 0, 0); break;
                        case SHAPE_RAMP_WEST:  shedDir = glm::ivec3( 1, 0, 0); break;
                        case SHAPE_RAMP_SOUTH: shedDir = glm::ivec3( 0, 0,-1); break;
                        case SHAPE_RAMP_NORTH: shedDir = glm::ivec3( 0, 0, 1); break;
                        default: break;
                    }
                    if (nb + shedDir == pos) {
                        hasLiquidFeeder = true;
                        break;
                    }
                }
            }
        }

        if (matBelow == MAT_AIR) {
            // Downward Fall Priority: set block below to current liquid material with Level 5 immediately
            TrySpreadToNeighbor(world, below, fluidMat, 5, dirtyChunks, true);
            collectDirty(below);

            // If current cell is Flowing and has no liquid stream feeding it, drain volume down
            if (!isSource && !hasLiquidFeeder) {
                world.SetBlock(pos, MAT_AIR);
                collectDirty(pos);
            }
            return; // Falls downward without spreading sideways mid-air
        }

        // Sub-block ramp or slab beneath liquid
        if (!ShapeGeometry::IsFullCube(voxBelow.shape()) && !voxBelow.is_waterlogged()) {
            TrySpreadToNeighbor(world, below, fluidMat, 5, dirtyChunks, true);
            collectDirty(below);
            if (!isSource && !hasLiquidFeeder) {
                world.SetBlock(pos, MAT_AIR);
                collectDirty(pos);
            }
            return;
        }

        // Liquid cell below that is not yet full (level < 5)
        if (IsLiquid(matBelow) && GetFluidLevel(world.GetBlockFlags(below)) < 5) {
            TrySpreadToNeighbor(world, below, fluidMat, 5, dirtyChunks, true);
            collectDirty(below);
            if (!isSource && !hasLiquidFeeder) {
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

            TrySpreadToNeighbor(world, target, fluidMat, targetLevel, dirtyChunks, false);
            collectDirty(target);
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
                int dnLevel = currentLevel > 1 ? currentLevel - 1 : 1;
                TrySpreadToNeighbor(world, downhill_pos, fluidMat, dnLevel, dirtyChunks, false);
                collectDirty(downhill_pos);
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

        const glm::ivec3 lateralDirs[4] = {
            { 1, 0, 0}, {-1, 0, 0},
            { 0, 0, 1}, { 0, 0,-1}
        };

        // 1. Process active fluids queue (snapshot of current active fluids)
        size_t batchSize = std::min(m_activeFluids.size(), static_cast<size_t>(MAX_FLUID_STEPS_PER_TICK));
        while (stepsProcessed < static_cast<int>(batchSize) && !m_activeFluids.empty()) {
            glm::ivec3 pos = m_activeFluids.front();
            m_activeFluids.pop_front();
            m_activeFluidSet.erase(pos);

            SimulateFluidCell(pos, world, dirtyChunks);
            stepsProcessed++;
        }
        m_last_steps_processed += stepsProcessed;

        // 2. Process scheduled delayed fluid spreads (place blocks and wake for subsequent tick)
        std::deque<ScheduledFluidUpdate> remaining;
        int spreadsProcessed = 0;
        constexpr int MAX_SPREADS_PER_TICK = 96;
        for (auto& upd : m_scheduledSpreads) {
            upd.delayTicks--;
            if (upd.delayTicks <= 0 && spreadsProcessed < MAX_SPREADS_PER_TICK) {
                // Execute block placement
                TrySpreadToNeighbor(world, upd.pos, upd.material, upd.level, dirtyChunks, true);
                WakeFluid(upd.pos);
                for (const auto& d : lateralDirs) {
                    WakeFluid(upd.pos + d);
                }
                WakeFluid(upd.pos + glm::ivec3(0, -1, 0));
                WakeFluid(upd.pos + glm::ivec3(0, 1, 0));
                spreadsProcessed++;
            } else {
                remaining.push_back(upd);
            }
        }
        m_scheduledSpreads = std::move(remaining);
        m_scheduledSpreadMap.clear();
        for (size_t i = 0; i < m_scheduledSpreads.size(); ++i) {
            m_scheduledSpreadMap[m_scheduledSpreads[i].pos] = i;
        }

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
