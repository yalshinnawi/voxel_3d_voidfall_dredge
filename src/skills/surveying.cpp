#include "surveying.hpp"
#include <algorithm>
#include <cmath>

namespace Voidfall {

SurveyingSystem::SurveyingSystem() = default;

void SurveyingSystem::trigger_scan(const glm::vec3& origin, const World& world, float scan_radius, float linger_bonus, int rank) {
    m_active = true;
    m_rank = rank;
    // Base duration is 1.8s (weaker at base), scaling +0.3s per rank up to ~3.3s
    m_duration = 1.8f + (0.3f * static_cast<float>(rank)) + linger_bonus;
    m_timer = m_duration;
    m_radius = std::min(scan_radius, 45.0f);
    m_origin = origin;
    m_surveyed.clear();
    m_clusters.clear();

    float effective_radius = m_radius;
    int min_x = static_cast<int>(std::floor(origin.x - effective_radius));
    int max_x = static_cast<int>(std::ceil(origin.x + effective_radius));
    int min_y = static_cast<int>(std::floor(origin.y - effective_radius));
    int max_y = static_cast<int>(std::ceil(origin.y + effective_radius));
    int min_z = static_cast<int>(std::floor(origin.z - effective_radius));
    int max_z = static_cast<int>(std::ceil(origin.z + effective_radius));

    float radius_sq = effective_radius * effective_radius;

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
                // Highlight valuable ores and subterranean structures
                if (v.material_id == MAT_VOIDITE ||
                    v.material_id == MAT_VOIDITE_CRYSTAL ||
                    v.material_id == MAT_TITANIUM ||
                    v.material_id == MAT_INDUSTRIAL_BULKHEAD ||
                    v.material_id == MAT_VAULT_DOOR ||
                    v.material_id == MAT_REINFORCED_VAULT_DOOR ||
                    v.material_id == MAT_RADIOACTIVE ||
                    v.material_id == MAT_RADIOACTIVE_ORE) {
                    m_surveyed.push_back(SurveyedVoxel{glm::ivec3(x, y, z), v.material_id});
                }
            }
        }
    }

    // Cluster surveyed voxels into distinct mineral veins for clean HUD labels
    for (const auto& sv : m_surveyed) {
        glm::vec3 vpos = glm::vec3(sv.pos) + glm::vec3(0.5f);
        bool merged = false;
        for (auto& cl : m_clusters) {
            if (cl.material_id == sv.material_id && glm::distance(cl.centroid, vpos) <= 3.2f) {
                // Update running centroid of vein
                cl.centroid = (cl.centroid * static_cast<float>(cl.count) + vpos) / static_cast<float>(cl.count + 1);
                cl.count++;
                merged = true;
                break;
            }
        }
        if (!merged) {
            m_clusters.push_back(SurveyedCluster{vpos, sv.material_id, 1});
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
        m_clusters.clear();
    }
}

float SurveyingSystem::alpha() const {
    if (!m_active || m_duration <= 0.0f) return 0.0f;
    // Immediate linear fade over duration (2.5s)
    return std::clamp(m_timer / m_duration, 0.0f, 1.0f);
}

} // namespace Voidfall
