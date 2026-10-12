#include "particle_system.hpp"

namespace Voidfall {

static ParticleSystem::BlockBreakCallback s_block_cb = nullptr;
static ParticleSystem::ClutterBreakCallback s_clutter_cb = nullptr;

void ParticleSystem::SetCallbacks(BlockBreakCallback block_cb, ClutterBreakCallback clutter_cb) {
    s_block_cb = std::move(block_cb);
    s_clutter_cb = std::move(clutter_cb);
}

void ParticleSystem::Reset() {
    s_block_cb = nullptr;
    s_clutter_cb = nullptr;
}

void ParticleSystem::SpawnBlockBreakDebris(const glm::vec3& pos, uint8_t mat) {
    if (s_block_cb) {
        s_block_cb(pos, mat);
    }
}

void ParticleSystem::SpawnClutterBreakDebris(const glm::vec3& pos, ClutterType type, const glm::vec4& tint) {
    if (s_clutter_cb) {
        s_clutter_cb(pos, type, tint);
    }
}

} // namespace Voidfall
