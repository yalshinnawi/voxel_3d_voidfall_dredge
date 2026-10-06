#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Voidfall {

class Renderer;

class ParticleSystem {
public:
    static void SetRenderer(Renderer* renderer);
    static void SpawnBlockBreakDebris(const glm::vec3& pos, uint8_t mat);
};

} // namespace Voidfall
