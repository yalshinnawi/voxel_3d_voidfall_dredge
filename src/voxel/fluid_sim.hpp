#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include "voxel_types.hpp"
#include "packed_vertex.hpp"

namespace Voidfall {

class Chunk;
class World;

struct Ivec3Hash {
    size_t operator()(const glm::ivec3& p) const noexcept {
        size_t h1 = std::hash<int>()(p.x);
        size_t h2 = std::hash<int>()(p.y);
        size_t h3 = std::hash<int>()(p.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct ScheduledFluidUpdate {
    glm::ivec3 pos;
    uint8_t material;
    int level;
    int delayTicks; // Decremented each tick; spreads when <= 0
};

static inline int GetSpreadDelayTicks(uint8_t material) {
    if (material == MAT_LAVA || material == MAT_THERMITE_SLAG || material == MAT_MOLTEN_MAGMA) {
        return 12; // approx 1.0s at 12 Hz
    }
    if (material == MAT_ACID || material == MAT_COOLANT) {
        return 4;
    }
    return 3; // Water: approx 0.25s at 12 Hz
}

class FluidSim {
public:
    void WakeFluid(const glm::ivec3& pos) {
        if (m_activeFluidSet.insert(pos).second) {
            m_activeFluids.push_back(pos);
        }
    }
    void SimulateFluidCell(const glm::ivec3& pos, World& world, std::unordered_set<Chunk*>& dirtyChunks);
    void SimulateFluidCell(const glm::ivec3& pos, World& world);
    void SimulateFluidCell(const glm::ivec3& pos);
    void TrySpreadToNeighbor(World& world, const glm::ivec3& targetPos, 
                             uint8_t liquidMat, int newLevel, 
                             std::unordered_set<Chunk*>& dirtyChunks,
                             bool immediate = false);
    void Update(float dt, World& world);

    // Static test harness hooks
    static inline World* s_activeWorld{nullptr};
    static void SetActiveWorld(World* w) { s_activeWorld = w; }
    static void Update(World& world, float dt);
    static void Update(float dt = 0.0833f);

    std::deque<glm::ivec3>& active_fluids() { return m_activeFluids; }
    const std::deque<glm::ivec3>& active_fluids() const { return m_activeFluids; }
    std::unordered_set<glm::ivec3, Ivec3Hash>& active_fluid_set() { return m_activeFluidSet; }
    const std::unordered_set<glm::ivec3, Ivec3Hash>& active_fluid_set() const { return m_activeFluidSet; }

    std::deque<ScheduledFluidUpdate>& scheduled_spreads() { return m_scheduledSpreads; }
    const std::deque<ScheduledFluidUpdate>& scheduled_spreads() const { return m_scheduledSpreads; }

    int last_steps_processed() const { return m_last_steps_processed; }

    std::deque<glm::ivec3> m_activeFluids;
    std::unordered_set<glm::ivec3, Ivec3Hash> m_activeFluidSet;
    std::deque<ScheduledFluidUpdate> m_scheduledSpreads;
    std::unordered_map<glm::ivec3, size_t, Ivec3Hash> m_scheduledSpreadMap;
    float m_accumulator = 0.0f;
    int m_last_steps_processed = 0;
};

} // namespace Voidfall
