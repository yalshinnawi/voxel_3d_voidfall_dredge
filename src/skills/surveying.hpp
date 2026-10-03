#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "../voxel/world.hpp"

namespace Voidfall {

struct SurveyedVoxel {
    glm::ivec3 pos;
    uint8_t material_id;
};

struct SurveyedCluster {
    glm::vec3 centroid{0.0f};
    uint8_t material_id{0};
    int count{0};
};

class SurveyingSystem {
public:
    SurveyingSystem();

    void trigger_scan(const glm::vec3& origin, const World& world, float scan_radius = 18.0f, float linger_bonus = 0.0f, int rank = 0);
    void update(float dt);

    bool is_active() const { return m_active; }
    float remaining_time() const { return m_timer; }
    float total_duration() const { return m_duration; }
    float alpha() const;

    // Immediately clears any active sonar scan (e.g. when deploying extraction beacon)
    void reset() { m_active = false; m_timer = 0.0f; m_surveyed.clear(); m_clusters.clear(); }

    int scan_rank() const { return m_rank; }
    bool is_identifying_materials() const { return m_rank >= 2; }

    const std::vector<SurveyedVoxel>& surveyed_voxels() const { return m_surveyed; }
    const std::vector<SurveyedCluster>& clusters() const { return m_clusters; }
    const glm::vec3& scan_origin() const { return m_origin; }
    float scan_radius() const { return m_radius; }

private:
    bool m_active{false};
    int m_rank{0};
    float m_timer{0.0f};
    float m_duration{2.5f};
    float m_radius{14.0f};
    glm::vec3 m_origin{0.0f};
    std::vector<SurveyedVoxel> m_surveyed;
    std::vector<SurveyedCluster> m_clusters;
};

} // namespace Voidfall
