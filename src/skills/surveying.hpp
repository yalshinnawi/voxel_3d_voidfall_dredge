#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "../voxel/world.hpp"

namespace Voidfall {

struct VoidStalker;
struct SeismicBurrower;

struct SurveyedVoxel {
    glm::ivec3 pos;
    uint8_t material_id;
};

struct SurveyedCluster {
    glm::vec3 centroid{0.0f};
    uint8_t material_id{0};
    int count{0};
    glm::vec3 min_bounds{0.0f};
    glm::vec3 max_bounds{0.0f};
    std::vector<glm::ivec3> voxels;
    std::vector<glm::vec3> outline_lines;   // Pairs of endpoints for smooth glowing outline (GL_LINES)
    std::vector<glm::vec3> hull_triangles;  // Triangles for smooth translucent glowing hull (GL_TRIANGLES)
};

struct SurveyedMonster {
    glm::vec3 position{0.0f};
    glm::vec3 min_bounds{0.0f};
    glm::vec3 max_bounds{0.0f};
    std::string name;
    bool is_burrower{false};
    float distance{0.0f};
    std::vector<glm::vec3> outline_lines;   // Pairs of endpoints for glowing wireframe outline (GL_LINES)
    std::vector<glm::vec3> hull_triangles;  // Triangles for translucent glowing hull (GL_TRIANGLES)
};

class SurveyingSystem {
public:
    SurveyingSystem();

    void trigger_scan(
        const glm::vec3& origin,
        const World& world,
        float scan_radius = 18.0f,
        float linger_bonus = 0.0f,
        int rank = 0,
        const std::vector<VoidStalker>* stalkers = nullptr,
        const std::vector<SeismicBurrower>* burrowers = nullptr,
        const glm::vec3& extraction_pos = glm::vec3(0.0f),
        bool extraction_active = false
    );

    void update(float dt);
    void update_monsters(
        const std::vector<VoidStalker>* stalkers,
        const std::vector<SeismicBurrower>* burrowers
    );

    // Generates a smooth organic glowing outline & hull for a merged connected component of voxels
    static void generate_cluster_hull(SurveyedCluster& cluster);

    // Generates a glowing outline and hull for a surveyed monster bounding box
    static void generate_monster_hull(SurveyedMonster& monster);

    bool is_active() const { return m_active; }
    float remaining_time() const { return m_timer; }
    float total_duration() const { return m_duration; }
    float alpha() const;

    // Immediately clears any active sonar scan (e.g. when deploying extraction beacon)
    void reset() {
        m_active = false;
        m_timer = 0.0f;
        m_surveyed.clear();
        m_clusters.clear();
        m_monsters.clear();
    }

    int scan_rank() const { return m_rank; }
    bool is_identifying_materials() const { return m_rank >= 2; }

    const std::vector<SurveyedVoxel>& surveyed_voxels() const { return m_surveyed; }
    const std::vector<SurveyedCluster>& clusters() const { return m_clusters; }
    const std::vector<SurveyedMonster>& monsters() const { return m_monsters; }
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
    std::vector<SurveyedMonster> m_monsters;
};

} // namespace Voidfall
