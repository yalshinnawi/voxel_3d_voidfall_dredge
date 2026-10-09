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

struct Ivec3Hash {
    size_t operator()(const glm::ivec3& p) const noexcept {
        size_t h1 = std::hash<int>()(p.x);
        size_t h2 = std::hash<int>()(p.y);
        size_t h3 = std::hash<int>()(p.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class FluidSim {
public:
    void WakeFluid(const glm::ivec3& pos) {
        if (m_activeFluidSet.insert(pos).second) {
            m_activeFluids.push_back(pos);
        }
    }
    void Update(float dt, World& world);

    // Static test harness hooks
    static inline World* s_activeWorld{nullptr};
    static void SetActiveWorld(World* w) { s_activeWorld = w; }
    static void Update(World& world, float dt);
    static void Update(float dt);

    std::deque<glm::ivec3>& active_fluids() { return m_activeFluids; }
    const std::deque<glm::ivec3>& active_fluids() const { return m_activeFluids; }
    std::unordered_set<glm::ivec3, Ivec3Hash>& active_fluid_set() { return m_activeFluidSet; }
    const std::unordered_set<glm::ivec3, Ivec3Hash>& active_fluid_set() const { return m_activeFluidSet; }

    std::deque<glm::ivec3> m_activeFluids;
    std::unordered_set<glm::ivec3, Ivec3Hash> m_activeFluidSet;
    float m_accumulator = 0.0f;
};

} // namespace Voidfall
