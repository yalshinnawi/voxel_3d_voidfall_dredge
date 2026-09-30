#include "surveying.hpp"
#include <algorithm>
#include <cmath>

namespace Voidfall {

SurveyingSystem::SurveyingSystem() = default;

void SurveyingSystem::trigger_scan(const glm::vec3& origin, const World& world, float scan_radius) {
    m_active = true;
    m_timer = m_duration;
    m_radius = scan_radius;
    m_origin = origin;
    m_surveyed.clear();

    int min_x = static_cast<int>(std::floor(origin.x - scan_radius));
    int max_x = static_cast<int>(std::ceil(origin.x + scan_radius));
    int min_y = static_cast<int>(std::floor(origin.y - scan_radius));
    int max_y = static_cast<int>(std::ceil(origin.y + scan_radius));
    int min_z = static_cast<int>(std::floor(origin.z - scan_radius));
    int max_z = static_cast<int>(std::ceil(origin.z + scan_radius));

    float radius_sq = scan_radius * scan_radius;

    for (int y = min_y; y <= max_y; ++y) {
        float dy = (static_cast<float>(y) + 0.5f) - origin.y;
        float dy2 = dy * dy;
        if (dy2 > radius_sq) continue;

        for (int z = min_z; z <= max_z; ++z) {
            float dz = (static_cast<float>(z) + 0.5f) - origin.z;
            float dyz2 = dy2 + dz * dz;
            if (dyz2 > radius_sq) continue;

            for (int x = min_x; x <= max_x; ++x) {
                float dx = (static_cast<float>(x) + 0.5f) - origin.x;
                if (dyz2 + dx * dx > radius_sq) continue;

                Voxel v = world.get_voxel(x, y, z);
                if (v.material_id == MAT_VOIDITE_CRYSTAL ||
                    v.material_id == MAT_INDUSTRIAL_BULKHEAD ||
                    v.material_id == MAT_RADIOACTIVE_ORE ||
                    v.material_id == MAT_REINFORCED_VAULT_DOOR) {
                    m_surveyed.push_back(SurveyedVoxel{glm::ivec3(x, y, z), v.material_id});
                }
            }
        }
    }
}

void SurveyingSystem::update(float dt) {
    if (!m_active) return;

    m_timer -= dt;
    if (m_timer <= 0.0f) {
        m_timer = 0.0f;
        m_active = false;
        m_surveyed.clear();
    }
}

float SurveyingSystem::alpha() const {
    if (!m_active || m_duration <= 0.0f) return 0.0f;
    return std::clamp(m_timer / m_duration, 0.0f, 1.0f);
}

} // namespace Voidfall
