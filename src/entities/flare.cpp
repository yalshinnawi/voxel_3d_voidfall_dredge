#include "flare.hpp"
#include "../voxel/world.hpp"
#include <algorithm>
#include <cmath>

namespace Voidfall {

FlareManager& FlareManager::instance() {
    static FlareManager s_mgr;
    return s_mgr;
}

void FlareManager::reset() {
    m_flares.clear();
    m_next_id = 1;
}

glm::vec3 FlareManager::get_flare_color(CharacterClass cls) {
    switch (cls) {
        case CharacterClass::Scout:
            return glm::vec3(0.05f, 0.85f, 1.0f); // Rich electric cyan
        case CharacterClass::Vanguard:
            return glm::vec3(1.0f, 0.78f, 0.12f); // Warm incandescent amber
        case CharacterClass::Demolitionist:
        default:
            return glm::vec3(1.0f, 0.42f, 0.05f); // High-output energetic orange
    }
}

void FlareManager::spawn_flare(const glm::vec3& origin, const glm::vec3& forward_dir, CharacterClass cls) {
    ChemicalFlare f;
    f.id = m_next_id++;
    f.owner_class = cls;
    f.color = get_flare_color(cls);
    f.position = origin + forward_dir * 0.4f;
    f.velocity = forward_dir * 16.0f + glm::vec3(0.0f, 3.5f, 0.0f);
    f.lifetime = 60.0f;
    f.max_lifetime = 60.0f;
    f.light_radius = 18.0f;
    f.is_grounded = false;
    m_flares.push_back(f);
}

void FlareManager::update(float dt, const World& world) {
    const float gravity = 14.0f;
    const float restitution = 0.55f;
    const float friction = 0.82f;

    for (auto it = m_flares.begin(); it != m_flares.end();) {
        it->lifetime -= dt;
        if (it->lifetime <= 0.0f) {
            it = m_flares.erase(it);
            continue;
        }

        if (!it->is_grounded) {
            // Apply downward gravity acceleration
            it->velocity.y -= gravity * dt;

            glm::vec3 next_pos = it->position + it->velocity * dt;

            // Axis-by-axis voxel swept collision
            // Check X axis
            int bx = static_cast<int>(std::floor(next_pos.x));
            int cy = static_cast<int>(std::floor(it->position.y));
            int cz = static_cast<int>(std::floor(it->position.z));
            if (world.is_solid(bx, cy, cz)) {
                it->velocity.x = -it->velocity.x * restitution;
                it->velocity.y *= friction;
                it->velocity.z *= friction;
                next_pos.x = it->position.x;
            }

            // Check Z axis
            int cx = static_cast<int>(std::floor(it->position.x));
            int bz = static_cast<int>(std::floor(next_pos.z));
            if (world.is_solid(cx, cy, bz)) {
                it->velocity.z = -it->velocity.z * restitution;
                it->velocity.x *= friction;
                it->velocity.y *= friction;
                next_pos.z = it->position.z;
            }

            // Check Y axis
            int by = static_cast<int>(std::floor(next_pos.y));
            if (world.is_solid(cx, by, cz)) {
                if (it->velocity.y < 0.0f) {
                    // Struck floor
                    if (std::abs(it->velocity.y) < 0.8f && glm::length(glm::vec2(it->velocity.x, it->velocity.z)) < 0.8f) {
                        it->is_grounded = true;
                        it->velocity = glm::vec3(0.0f);
                        next_pos.y = std::floor(it->position.y) + 0.15f;
                    } else {
                        it->velocity.y = -it->velocity.y * restitution;
                        it->velocity.x *= friction;
                        it->velocity.z *= friction;
                        next_pos.y = it->position.y;
                    }
                } else {
                    // Struck ceiling
                    it->velocity.y = -it->velocity.y * restitution;
                    next_pos.y = it->position.y;
                }
            }

            it->position = next_pos;
        }

        ++it;
    }
}

} // namespace Voidfall
