#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>
#include <cstdint>
#include <memory>

namespace Voidfall {

class World;
class PlayerController;
class ExtractionSystem;
class MissionSystem;

struct ScannerLineVertex {
    glm::vec3 position;
    glm::vec4 color;
};

class TerrainScanner {
public:
    static constexpr int GRID_DIM = 48; // 48x48x48 local occupancy grid
    static constexpr float SCAN_RANGE = 24.0f; // Half-extent in meters

    TerrainScanner();
    ~TerrainScanner();

    void reset();

    /// Update scanner folding animation, player exploration tracking, and mouse orbit
    void update(float dt, bool tab_held, const glm::vec3& player_pos,
                float mouse_dx, float mouse_dy, bool mouse_dragging);

    /// Query active and fold animation states
    bool is_active() const { return m_active; }
    float fold_progress() const { return m_fold_progress; }

    /// Re-samples local cavern boundary voxels into wireframe contour line vertices
    void refresh_geometry(const World& world, const glm::vec3& player_pos,
                          const PlayerController& player,
                          const ExtractionSystem& extraction,
                          const MissionSystem& mission,
                          float total_time);

    /// Renders the 3D holographic projection with additive blending & depth test disabled
    void render(int screen_width, int screen_height, const glm::vec3& player_pos, float player_yaw);

    // Orbit controls
    float orbit_yaw() const { return m_orbit_yaw; }
    float orbit_pitch() const { return m_orbit_pitch; }
    float orbit_distance() const { return m_orbit_distance; }

private:
    void init_buffers();
    void add_wire_box(std::vector<ScannerLineVertex>& lines, const glm::vec3& min_p, const glm::vec3& max_p, const glm::vec4& color);
    void add_directional_cone(std::vector<ScannerLineVertex>& lines, const glm::vec3& apex, const glm::vec3& dir, float height, float radius, const glm::vec4& color);
    void add_lock_icon(std::vector<ScannerLineVertex>& lines, const glm::vec3& center, float size, const glm::vec4& color);
    void add_anchor_marker(std::vector<ScannerLineVertex>& lines, const glm::vec3& center, float size, const glm::vec4& color);

    bool m_active{false};
    float m_fold_progress{0.0f}; // 0.0f folded in gauntlet, 1.0f fully projected

    float m_orbit_yaw{45.0f};
    float m_orbit_pitch{32.0f};
    float m_orbit_distance{28.0f};

    // Visited voxel spatial hash (coarse 2m cells for exploration memory)
    std::unordered_set<uint64_t> m_visited_cells;
    glm::vec3 m_last_scan_pos{0.0f};
    float m_geom_refresh_timer{0.0f};

    unsigned int m_vao{0};
    unsigned int m_vbo{0};
    unsigned int m_shader_prog{0};
    int m_vertex_count{0};
};

} // namespace Voidfall
