#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <deque>
#include <unordered_set>
#include "voxel_types.hpp"
#include "packed_vertex.hpp"

namespace Voidfall {

class World;

class FluidSim {
public:
    static constexpr float TICK_RATE = 12.0f;
    static constexpr float STEP_DT = 1.0f / TICK_RATE; // ~0.08333f (12 Hz)
    static constexpr int MAX_FLUID_STEPS_PER_TICK = 192;

    static inline uint64_t PackPos(const glm::ivec3& pos) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(pos.x) & 0x1FFFFFu) << 42) |
               (static_cast<uint64_t>(static_cast<uint32_t>(pos.y) & 0x1FFFFFu) << 21) |
               (static_cast<uint64_t>(static_cast<uint32_t>(pos.z) & 0x1FFFFFu));
    }

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
};

} // namespace Voidfall
