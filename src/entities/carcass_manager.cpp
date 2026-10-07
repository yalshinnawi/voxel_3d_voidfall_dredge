#include "carcass_manager.hpp"
#include "../voxel/world.hpp"
#include <algorithm>
#include <cmath>

namespace Voidfall {

CarcassManager& CarcassManager::instance() {
    static CarcassManager s_instance;
    return s_instance;
}

uint32_t CarcassManager::spawn_carcass(
    const glm::vec3& position,
    const glm::quat& rotation,
    const glm::vec3& lethal_hit_dir,
    StalkerRole role,
    float scale)
{
    // Capped circular pool: If max capacity reached, begin dissolving the oldest carcass
    if (m_carcasses.size() >= MAX_CARCASSES) {
        for (auto& c : m_carcasses) {
            if (!c.is_dissolving) {
                c.is_dissolving = true;
                break;
            }
        }
        // If all are already dissolving, prune the most dissolved one
        if (m_carcasses.size() >= MAX_CARCASSES + 4) {
            m_carcasses.erase(m_carcasses.begin());
        }
    }

    EnemyCarcass carcass;
    carcass.id = m_next_id++;
    carcass.position = position;
    carcass.rotation = rotation;
    carcass.scale = scale;
    carcass.role = role;
    carcass.hp = 20.0f;
    carcass.max_hp = 20.0f;
    carcass.alpha = 1.0f;
    carcass.is_dissolving = false;
    carcass.dissolve_timer = 0.0f;
    carcass.is_sleeping = false;
    carcass.harvested = false;

    // Detach wall/ceiling gravity adhesion; apply downward cavern gravity and hit impulse
    glm::vec3 impulse = (glm::length(lethal_hit_dir) > 0.001f)
        ? (glm::normalize(lethal_hit_dir) * 3.8f + glm::vec3(0.0f, 1.6f, 0.0f))
        : glm::vec3(0.0f, 0.5f, 0.0f);
    carcass.velocity = impulse;
    carcass.lethal_hit_dir = lethal_hit_dir;

    m_carcasses.push_back(carcass);
    return carcass.id;
}

void CarcassManager::update(float dt, World& world) {
    for (auto& c : m_carcasses) {
        c.lifetime += dt;
        // Persist on cavern floor for 45s before decaying into ash
        if (c.lifetime >= c.max_lifetime && !c.is_dissolving) {
            c.is_dissolving = true;
            c.dissolve_timer = 0.0f;
        }

        // Alpha burn-away dissolver shader processing
        if (c.is_dissolving) {
            c.dissolve_timer += dt;
            c.alpha = std::max(0.0f, 1.0f - (c.dissolve_timer / 2.0f));
            continue;
        }

        // Settling ragdoll physics
        if (!c.is_sleeping) {
            // Standard subterranean down-vector gravity
            c.velocity.y -= 18.0f * dt;
            c.velocity.x *= std::max(0.0f, 1.0f - 1.8f * dt);
            c.velocity.z *= std::max(0.0f, 1.0f - 1.8f * dt);

            c.position += c.velocity * dt;

            // Check voxel ground collision
            int ix = static_cast<int>(std::floor(c.position.x));
            int iy = static_cast<int>(std::floor(c.position.y - 0.25f));
            int iz = static_cast<int>(std::floor(c.position.z));

            Voxel floor_vox = world.get_voxel(ix, iy, iz);
            if (floor_vox.material_id != MAT_AIR && floor_vox.material_id != MAT_GAS &&
                floor_vox.material_id != MAT_VOLATILE_SMOKE)
            {
                // Settled on solid cavern voxel: freeze physics simulation to zero CPU overhead
                c.position.y = static_cast<float>(iy + 1) + 0.05f;
                c.velocity = glm::vec3(0.0f);
                c.is_sleeping = true;

                // Flatten/collapse skeletal rig into limp death pose
                glm::vec3 flat_forward = glm::normalize(glm::vec3(c.lethal_hit_dir.x, 0.0f, c.lethal_hit_dir.z));
                if (glm::length(flat_forward) < 0.001f) flat_forward = glm::vec3(0.0f, 0.0f, 1.0f);
                glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), flat_forward));

                glm::mat3 flat_mat;
                flat_mat[0] = right;
                flat_mat[1] = glm::vec3(0.0f, 1.0f, 0.0f);
                flat_mat[2] = flat_forward;
                c.rotation = glm::quat_cast(flat_mat);
            }
        }
    }

    // Prune fully dissolved carcasses
    std::erase_if(m_carcasses, [](const EnemyCarcass& c) {
        return c.is_dissolving && c.alpha <= 0.001f;
    });
}

bool CarcassManager::harvest_nearest(
    const glm::vec3& /*origin*/,
    float /*radius*/,
    float /*drill_damage*/,
    int& out_carapace,
    int& out_biomass,
    glm::vec3* /*out_shatter_pos*/)
{
    // Delvers cannot mine monsters or carcasses with mining drills to obtain chitin
    out_carapace = 0;
    out_biomass = 0;
    return false;
}

void CarcassManager::clear() {
    m_carcasses.clear();
}

} // namespace Voidfall
