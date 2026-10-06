#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>
#include <cstdint>
#include <string>
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

struct Shape2DVertex {
    glm::vec2 pos;
    glm::vec4 color;
};

enum class CavernCellType : uint8_t {
    Unexplored = 0,
    SolidRock = 1,
    CaveWallRim = 2,
    CaveFloor = 3,
    HazardFloor = 4,
    ChasmDrop = 5
};

class TerrainScanner {
public:
    static constexpr int MAP_MIN = -32;
    static constexpr int MAP_MAX = 160;
    static constexpr int MAP_SPAN = MAP_MAX - MAP_MIN; // 192

    TerrainScanner();
    ~TerrainScanner();

    void reset();

    /// Update scanner folding animation, player exploration tracking, and mouse pan
    void update(float dt, bool tab_held, const glm::vec3& player_pos,
                float mouse_dx, float mouse_dy, bool mouse_dragging);

    /// Query active and fold animation states
    bool is_active() const { return m_active; }
    float fold_progress() const { return m_fold_progress; }

    /// Re-samples local cavern boundary voxels into 2D map geometry
    void refresh_geometry(const World& world, const glm::vec3& player_pos,
                          const PlayerController& player,
                          const ExtractionSystem& extraction,
                          const MissionSystem& mission,
                          float total_time);

    /// Renders the 2D tactical cartography map overlay
    void render(int screen_width, int screen_height, const glm::vec3& player_pos, float player_yaw);

    // Orbit / Pan compatibility controls
    float orbit_yaw() const { return m_orbit_yaw; }
    float orbit_pitch() const { return m_orbit_pitch; }
    float orbit_distance() const { return m_orbit_distance; }

    // Map discovery query
    float discovery_percentage() const;

private:
    void init_buffers();
    void init_font();
    void draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color, float alpha);

    bool m_active{false};
    float m_fold_progress{0.0f}; // 0.0f closed, 1.0f fully open

    // Orbit / Pan compatibility
    float m_orbit_yaw{45.0f};
    float m_orbit_pitch{32.0f};
    float m_orbit_distance{28.0f};

    // 2D Map Pan & Zoom
    float m_pan_x{0.0f};
    float m_pan_y{0.0f};
    float m_zoom{7.5f}; // Pixels per meter

    // Persistent Exploration Memory (Fog of War)
    std::vector<uint8_t> m_discovered;
    std::vector<uint8_t> m_cell_types;
    int m_discovered_count{0};

    float m_total_time{0.0f};
    float m_geom_refresh_timer{0.0f};
    glm::vec3 m_last_scan_pos{0.0f};

    int m_screen_w{1600};
    int m_screen_h{900};

    // OpenGL resources
    unsigned int m_shape_prog{0};
    unsigned int m_shape_vao{0};
    unsigned int m_shape_vbo{0};

    unsigned int m_text_prog{0};
    unsigned int m_text_vao{0};
    unsigned int m_text_vbo{0};
    unsigned int m_font_tex{0};

    // Cached batches
    std::vector<Shape2DVertex> m_cached_tris;
    std::vector<Shape2DVertex> m_cached_lines;
    struct TextCommand {
        std::string text;
        float x{0.0f};
        float y{0.0f};
        float scale{1.0f};
        glm::vec4 color{1.0f};
    };
    std::vector<TextCommand> m_cached_texts;
};

} // namespace Voidfall
