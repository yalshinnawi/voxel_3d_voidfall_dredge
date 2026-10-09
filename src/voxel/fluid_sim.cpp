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

void FluidSim::Update(float dt, World& world) {
    m_accumulator += dt;
    constexpr float TICK_INTERVAL = 0.0833f; // 12 Hz tick
    constexpr int MAX_STEPS = 256;

    while (m_accumulator >= TICK_INTERVAL) {
        m_accumulator -= TICK_INTERVAL;
        int steps = 0;

        while (!m_activeFluids.empty() && steps < MAX_STEPS) {
            glm::ivec3 pos = m_activeFluids.front();
            m_activeFluids.pop_front();
            m_activeFluidSet.erase(pos);
            steps++;

            uint8_t currentMat = world.GetBlockMaterial(pos);
            Voxel curVox = world.get_voxel(pos.x, pos.y, pos.z);
            bool isPureLiquid = IsLiquid(currentMat);
            bool isWaterlogged = curVox.is_waterlogged();
            if (!isPureLiquid && !isWaterlogged) continue;

            uint8_t fluidMat = isPureLiquid ? currentMat : MAT_WATER;
            int currentLevel = isPureLiquid ? GetFluidLevel(world.GetBlockFlags(pos)) : 5;

            // 1. Gravity Downward (Highest Priority for Pure Liquids)
            if (isPureLiquid && pos.y > 0) {
                glm::ivec3 below = pos + glm::ivec3(0, -1, 0);
                uint8_t matBelow = world.GetBlockMaterial(below);
                Voxel voxBelow = world.get_voxel(below.x, below.y, below.z);
                if (matBelow == MAT_AIR) {
                    world.SetBlockWithFlags(below, fluidMat, 5);
                    WakeFluid(below);
                    continue; // Falls downward without spreading sideways mid-air
                }

                // Sub-block ramp or slab beneath liquid
                if ((voxBelow.is_slab() || voxBelow.is_ramp() || voxBelow.is_corner()) && !voxBelow.is_waterlogged()) {
                    world.set_waterlogged_cell(below, true);
                    WakeFluid(below);
                    continue;
                }

                // Liquid cell below that is not yet full
                if (IsLiquid(matBelow) && GetFluidLevel(world.GetBlockFlags(below)) < 5) {
                    world.SetBlockWithFlags(below, fluidMat, 5);
                    WakeFluid(below);
                    continue;
                }
            }

            // 2. Lateral Permeation (Only if supported by solid, waterlogged sub-block, or liquid below)
            bool floor_supported = (pos.y == 0) || isWaterlogged;
            if (pos.y > 0 && !floor_supported) {
                glm::ivec3 below = pos + glm::ivec3(0, -1, 0);
                Voxel voxBelow = world.get_voxel(below.x, below.y, below.z);
                if (voxBelow.is_solid() || voxBelow.is_waterlogged() || IsLiquid(voxBelow.material_id)) {
                    floor_supported = true;
                }
            }

            if (floor_supported && currentLevel > 1) {
                const glm::ivec3 lateralDirs[4] = {
                    { 1, 0, 0}, {-1, 0, 0},
                    { 0, 0, 1}, { 0, 0,-1}
                };
                for (const auto& dir : lateralDirs) {
                    glm::ivec3 target = pos + dir;
                    uint8_t targetMat = world.GetBlockMaterial(target);
                    Voxel targetVox = world.get_voxel(target.x, target.y, target.z);

                    if (targetMat == MAT_AIR) {
                        // Pool infilling check (>= 2 adjacent sources on solid floor)
                        bool infilled = false;
                        if (target.y > 0 && world.get_voxel(target.x, target.y - 1, target.z).is_solid()) {
                            int adjacent_sources = 0;
                            for (const auto& check_off : lateralDirs) {
                                glm::ivec3 check_pos = target + check_off;
                                uint8_t c_mat = world.GetBlockMaterial(check_pos);
                                if (IsLiquid(c_mat) && GetFluidLevel(world.GetBlockFlags(check_pos)) == 5) {
                                    adjacent_sources++;
                                }
                            }
                            if (adjacent_sources >= 2) {
                                world.SetBlockWithFlags(target, fluidMat, 5);
                                WakeFluid(target);
                                infilled = true;
                            }
                        }
                        if (!infilled) {
                            world.SetBlockWithFlags(target, fluidMat, currentLevel - 1);
                            WakeFluid(target);
                        }
                    } else if (targetMat == fluidMat) {
                        int targetLevel = GetFluidLevel(world.GetBlockFlags(target));
                        bool infilled = false;
                        if (targetLevel < 5 && target.y > 0 && world.get_voxel(target.x, target.y - 1, target.z).is_solid()) {
                            int adjacent_sources = 0;
                            for (const auto& check_off : lateralDirs) {
                                glm::ivec3 check_pos = target + check_off;
                                uint8_t c_mat = world.GetBlockMaterial(check_pos);
                                if (IsLiquid(c_mat) && GetFluidLevel(world.GetBlockFlags(check_pos)) == 5) {
                                    adjacent_sources++;
                                }
                            }
                            if (adjacent_sources >= 2) {
                                world.SetBlockWithFlags(target, fluidMat, 5);
                                WakeFluid(target);
                                infilled = true;
                            }
                        }
                        if (!infilled && targetLevel < currentLevel - 1) {
                            world.SetBlockWithFlags(target, fluidMat, currentLevel - 1);
                            WakeFluid(target);
                        }
                    } else if ((targetVox.is_slab() || targetVox.is_ramp() || targetVox.is_corner()) && !targetVox.is_waterlogged()) {
                        world.set_waterlogged_cell(target, true);
                        WakeFluid(target);
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
                    Voxel dn_vox = world.get_voxel(downhill_pos.x, downhill_pos.y, downhill_pos.z);
                    if (dn_vox.material_id == MAT_AIR) {
                        world.SetBlockWithFlags(downhill_pos, fluidMat, currentLevel > 1 ? currentLevel - 1 : 1);
                        WakeFluid(downhill_pos);
                    } else if ((dn_vox.is_slab() || dn_vox.is_ramp()) && !dn_vox.is_waterlogged()) {
                        world.set_waterlogged_cell(downhill_pos, true);
                        WakeFluid(downhill_pos);
                    }
                }
            }
        }
    }
}

} // namespace Voidfall
