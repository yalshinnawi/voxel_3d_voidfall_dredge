#include "particle_system.hpp"
#include "renderer.hpp"

namespace Voidfall {

static Renderer* s_particle_renderer = nullptr;

void ParticleSystem::SetRenderer(Renderer* renderer) {
    s_particle_renderer = renderer;
}

void ParticleSystem::SpawnBlockBreakDebris(const glm::vec3& pos, uint8_t mat) {
    if (s_particle_renderer) {
        s_particle_renderer->spawn_break_particles(pos, glm::ivec3(0, 1, 0), mat);
    }
}

} // namespace Voidfall
