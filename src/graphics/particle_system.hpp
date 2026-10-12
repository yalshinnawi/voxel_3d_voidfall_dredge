#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <functional>
#include "../voxel/clutter_types.hpp"

namespace Voidfall {

class Renderer;

class ParticleSystem {
public:
    using BlockBreakCallback = std::function<void(const glm::vec3&, uint8_t)>;
    using ClutterBreakCallback = std::function<void(const glm::vec3&, ClutterType, const glm::vec4&)>;

    static void SetRenderer(Renderer* renderer);
    static void SetCallbacks(BlockBreakCallback block_cb, ClutterBreakCallback clutter_cb);
    static void SpawnBlockBreakDebris(const glm::vec3& pos, uint8_t mat);
    static void SpawnClutterBreakDebris(const glm::vec3& pos, ClutterType type, const glm::vec4& tint = glm::vec4(1.0f));
    static void Reset();
};

} // namespace Voidfall
