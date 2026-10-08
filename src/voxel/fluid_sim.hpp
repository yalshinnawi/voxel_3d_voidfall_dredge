#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <deque>
#include <unordered_set>
#include "voxel_types.hpp"
#include "packed_vertex.hpp"

namespace Voidfall {

class Chunk;
class World;

class FluidSim {
public:
    static constexpr float TICK_RATE = 12.0f;
    static constexpr float STEP_DT = 1.0f / TICK_RATE; // ~0.08333f (12 Hz)
    static constexpr int MAX_FLUID_STEPS_PER_TICK = 192;

    static inline World* s_activeWorld{nullptr};
    static void SetActiveWorld(World* w) { s_activeWorld = w; }

    static inline uint64_t PackCoord(const glm::ivec3& p) {
        return (uint64_t(p.x & 0x1FFFFF) << 42) | (uint64_t(p.y & 0x1FFFFF) << 21) | uint64_t(p.z & 0x1FFFFF);
    }
    static inline uint64_t PackPos(const glm::ivec3& pos) { return PackCoord(pos); }

    static inline glm::ivec3 UnpackPos(uint64_t key) {
        int32_t x = static_cast<int32_t>((key >> 42) & 0x1FFFFFu);
        int32_t y = static_cast<int32_t>((key >> 21) & 0x1FFFFFu);
        int32_t z = static_cast<int32_t>(key & 0x1FFFFFu);
        if (x & 0x100000) x |= ~0x1FFFFF;
        if (y & 0x100000) y |= ~0x1FFFFF;
        if (z & 0x100000) z |= ~0x1FFFFF;
        return glm::ivec3(x, y, z);
    }

    // Cellular Automaton flow evaluation for an active cell
    static void SimulateFluidCell(World& world, const glm::ivec3& pos);
    static void SimulateFluidCell(World& world, const glm::ivec3& pos, std::unordered_set<Chunk*>& dirtyChunks);

    static void Update(World& world, float dt);
    static void Update(float dt);
};

} // namespace Voidfall
