#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "../voxel/world.hpp"

namespace Voidfall {

struct SurveyedVoxel {
    glm::ivec3 pos;
    uint8_t material_id;
};

class SurveyingSystem {
public:
    SurveyingSystem();

    void trigger_scan(const glm::vec3& origin, const World& world, float scan_radius = 18.0f);
    void update(float dt);

    bool is_active() const { return m_active; }
    float remaining_time() const { return m_timer; }
    float total_duration() const { return m_duration; }
    float alpha() const;

    const std::vector<SurveyedVoxel>& surveyed_voxels() const { return m_surveyed; }
    const glm::vec3& scan_origin() const { return m_origin; }
    float scan_radius() const { return m_radius; }

private:
    bool m_active{false};
    float m_timer{0.0f};
    float m_duration{2.5f};
    float m_radius{14.0f};
    glm::vec3 m_origin{0.0f};
    std::vector<SurveyedVoxel> m_surveyed;
};

} // namespace Voidfall
