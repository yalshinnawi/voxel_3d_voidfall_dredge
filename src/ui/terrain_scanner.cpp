#include <glad/glad.h>
#include "terrain_scanner.hpp"
#include "../voxel/world.hpp"
#include "../player/controller.hpp"
#include "../systems/extraction.hpp"
#include "../systems/mission_system.hpp"
#include "../core/logger.hpp"
#include "font_atlas.hpp"
#include "font_renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Voidfall {

namespace {

const char* SHAPE_VS = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
uniform mat4 uProjection;
uniform float uAlpha;
out vec4 vColor;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vColor = vec4(aColor.rgb, aColor.a * uAlpha);
}
)";

const char* SHAPE_FS = R"(#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

const char* TEXT_VS = R"(#version 330 core
layout (location = 0) in vec4 aVertex; // pos.xy, uv.xy
uniform mat4 uProjection;
out vec2 vUV;
void main() {
    gl_Position = uProjection * vec4(aVertex.xy, 0.0, 1.0);
    vUV = aVertex.zw;
}
)";

const char* TEXT_FS = R"(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uFontTexture;
uniform vec4 uTextColor;
uniform float uAlpha;
void main() {
    float a = texture(uFontTexture, vUV).r;
    FragColor = vec4(uTextColor.rgb, uTextColor.a * a * uAlpha);
}
)";

inline void add_quad(std::vector<Shape2DVertex>& tris, float x, float y, float w, float h, const glm::vec4& col) {
    tris.push_back({{x, y}, col});
    tris.push_back({{x + w, y}, col});
    tris.push_back({{x + w, y + h}, col});
    tris.push_back({{x, y}, col});
    tris.push_back({{x + w, y + h}, col});
    tris.push_back({{x, y + h}, col});
}

inline void add_line(std::vector<Shape2DVertex>& lines, const glm::vec2& p1, const glm::vec2& p2, const glm::vec4& col) {
    lines.push_back({p1, col});
    lines.push_back({p2, col});
}

inline void add_rect_outline(std::vector<Shape2DVertex>& lines, float x, float y, float w, float h, const glm::vec4& col) {
    add_line(lines, {x, y}, {x + w, y}, col);
    add_line(lines, {x + w, y}, {x + w, y + h}, col);
    add_line(lines, {x + w, y + h}, {x, y + h}, col);
    add_line(lines, {x, y + h}, {x, y}, col);
}

inline void add_circle_outline(std::vector<Shape2DVertex>& lines, const glm::vec2& center, float r, int segs, const glm::vec4& col) {
    for (int i = 0; i < segs; ++i) {
        float a1 = static_cast<float>(i) * (2.0f * glm::pi<float>() / static_cast<float>(segs));
        float a2 = static_cast<float>(i + 1) * (2.0f * glm::pi<float>() / static_cast<float>(segs));
        glm::vec2 p1 = center + glm::vec2(std::cos(a1) * r, std::sin(a1) * r);
        glm::vec2 p2 = center + glm::vec2(std::cos(a2) * r, std::sin(a2) * r);
        add_line(lines, p1, p2, col);
    }
}

inline void add_diamond(std::vector<Shape2DVertex>& tris, const glm::vec2& center, float r, const glm::vec4& col) {
    glm::vec2 top = center + glm::vec2(0.0f, -r);
    glm::vec2 bot = center + glm::vec2(0.0f, r);
    glm::vec2 lft = center + glm::vec2(-r, 0.0f);
    glm::vec2 rgt = center + glm::vec2(r, 0.0f);
    tris.push_back({top, col});
    tris.push_back({rgt, col});
    tris.push_back({bot, col});
    tris.push_back({top, col});
    tris.push_back({bot, col});
    tris.push_back({lft, col});
}

inline void add_player_chevron(std::vector<Shape2DVertex>& tris, std::vector<Shape2DVertex>& lines,
                              const glm::vec2& center, float yaw_deg, float size, const glm::vec4& col) {
    float rad = glm::radians(yaw_deg);
    glm::vec2 fwd(std::cos(rad), std::sin(rad));
    glm::vec2 rgt(-fwd.y, fwd.x);

    glm::vec2 tip = center + fwd * (size * 1.35f);
    glm::vec2 left_fin = center - fwd * (size * 0.70f) - rgt * (size * 0.75f);
    glm::vec2 right_fin = center - fwd * (size * 0.70f) + rgt * (size * 0.75f);
    glm::vec2 notch = center - fwd * (size * 0.25f);

    // Left wing
    tris.push_back({tip, col});
    tris.push_back({notch, col});
    tris.push_back({left_fin, col});

    // Right wing
    tris.push_back({tip, col});
    tris.push_back({right_fin, col});
    tris.push_back({notch, col});

    // Dark high-contrast edge contour
    glm::vec4 edge_col(0.05f, 0.05f, 0.05f, col.a);
    add_line(lines, tip, left_fin, edge_col);
    add_line(lines, left_fin, notch, edge_col);
    add_line(lines, notch, right_fin, edge_col);
    add_line(lines, right_fin, tip, edge_col);
}

inline void add_view_cone(std::vector<Shape2DVertex>& tris, const glm::vec2& center,
                          float yaw_deg, float fov_deg, float length, const glm::vec4& col) {
    float half_fov = fov_deg * 0.5f;
    int segs = 10;
    for (int i = 0; i < segs; ++i) {
        float t1 = static_cast<float>(i) / static_cast<float>(segs);
        float t2 = static_cast<float>(i + 1) / static_cast<float>(segs);
        float a1 = glm::radians(yaw_deg - half_fov + t1 * fov_deg);
        float a2 = glm::radians(yaw_deg - half_fov + t2 * fov_deg);
        glm::vec2 p1 = center + glm::vec2(std::cos(a1) * length, std::sin(a1) * length);
        glm::vec2 p2 = center + glm::vec2(std::cos(a2) * length, std::sin(a2) * length);
        tris.push_back({center, col});
        tris.push_back({p1, col});
        tris.push_back({p2, col});
    }
}

std::string get_bearing_str(const glm::vec3& from, const glm::vec3& to) {
    float dx = to.x - from.x;
    float dz = to.z - from.z; // -Z is North, +Z is South
    float angle_rad = std::atan2(dx, -dz);
    float deg = glm::degrees(angle_rad);
    if (deg < 0.0f) deg += 360.0f;

    if (deg >= 337.5f || deg < 22.5f) return "N";
    if (deg >= 22.5f && deg < 67.5f) return "NE";
    if (deg >= 67.5f && deg < 112.5f) return "E";
    if (deg >= 112.5f && deg < 157.5f) return "SE";
    if (deg >= 157.5f && deg < 202.5f) return "S";
    if (deg >= 202.5f && deg < 247.5f) return "SW";
    if (deg >= 247.5f && deg < 292.5f) return "W";
    return "NW";
}

} // namespace

TerrainScanner::TerrainScanner() {
    m_discovered.assign(MAP_SPAN * MAP_SPAN, 0);
    m_cell_types.assign(MAP_SPAN * MAP_SPAN, 0);
}

TerrainScanner::~TerrainScanner() {
    if (m_shape_vao != 0 && glDeleteVertexArrays != nullptr) glDeleteVertexArrays(1, &m_shape_vao);
    if (m_shape_vbo != 0 && glDeleteBuffers != nullptr) glDeleteBuffers(1, &m_shape_vbo);
    if (m_shape_prog != 0 && glDeleteProgram != nullptr) glDeleteProgram(m_shape_prog);

    if (m_text_vao != 0 && glDeleteVertexArrays != nullptr) glDeleteVertexArrays(1, &m_text_vao);
    if (m_text_vbo != 0 && glDeleteBuffers != nullptr) glDeleteBuffers(1, &m_text_vbo);
    if (m_text_prog != 0 && glDeleteProgram != nullptr) glDeleteProgram(m_text_prog);

    if (m_font_tex != 0 && glDeleteTextures != nullptr) glDeleteTextures(1, &m_font_tex);
}

void TerrainScanner::init_buffers() {
    if (m_shape_vao != 0 || glCreateShader == nullptr) return;

    // 1. Compile 2D Shape Shader
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &SHAPE_VS, nullptr);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &SHAPE_FS, nullptr);
    glCompileShader(fs);

    m_shape_prog = glCreateProgram();
    glAttachShader(m_shape_prog, vs);
    glAttachShader(m_shape_prog, fs);
    glLinkProgram(m_shape_prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &m_shape_vao);
    glGenBuffers(1, &m_shape_vbo);

    glBindVertexArray(m_shape_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_shape_vbo);

    // Position (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Shape2DVertex), reinterpret_cast<void*>(0));
    // Color (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Shape2DVertex), reinterpret_cast<void*>(sizeof(glm::vec2)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 2. Compile Text Shader
    unsigned int tvs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(tvs, 1, &TEXT_VS, nullptr);
    glCompileShader(tvs);

    unsigned int tfs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(tfs, 1, &TEXT_FS, nullptr);
    glCompileShader(tfs);

    m_text_prog = glCreateProgram();
    glAttachShader(m_text_prog, tvs);
    glAttachShader(m_text_prog, tfs);
    glLinkProgram(m_text_prog);

    glDeleteShader(tvs);
    glDeleteShader(tfs);

    glGenVertexArrays(1, &m_text_vao);
    glGenBuffers(1, &m_text_vbo);

    glBindVertexArray(m_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_text_vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    init_font();
}

void TerrainScanner::init_font() {
    if (m_font_tex != 0 || glGenTextures == nullptr) return;
    int atlas_w = 0, atlas_h = 0;
    std::vector<uint8_t> atlas;
    std::array<GlyphMetric, 128> metrics;
    generate_proportional_sans_font_atlas(atlas_w, atlas_h, atlas, metrics);

    glGenTextures(1, &m_font_tex);
    glBindTexture(GL_TEXTURE_2D, m_font_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas_w, atlas_h, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void TerrainScanner::reset() {
    m_active = false;
    m_fold_progress = 0.0f;
    m_discovered.assign(MAP_SPAN * MAP_SPAN, 0);
    m_cell_types.assign(MAP_SPAN * MAP_SPAN, 0);
    m_discovered_count = 0;
    m_pan_x = 0.0f;
    m_pan_y = 0.0f;
    m_zoom = 7.5f;
    m_orbit_yaw = 45.0f;
    m_orbit_pitch = 32.0f;
    m_orbit_distance = 28.0f;
    m_last_scan_pos = glm::vec3(0.0f);
    m_geom_refresh_timer = 0.0f;
}

void TerrainScanner::update(float dt, bool tab_held, const glm::vec3& player_pos,
                            float mouse_dx, float mouse_dy, bool mouse_dragging) {
    bool was_active = m_active;
    m_active = tab_held;

    if (m_active && !was_active) {
        // Reset pan to delver center whenever the map is newly opened
        m_pan_x = 0.0f;
        m_pan_y = 0.0f;
    }

    // Smooth fold/unfold animation
    float target_fold = m_active ? 1.0f : 0.0f;
    float fold_speed = 8.5f;
    if (m_fold_progress < target_fold) {
        m_fold_progress = std::min(target_fold, m_fold_progress + fold_speed * dt);
    } else if (m_fold_progress > target_fold) {
        m_fold_progress = std::max(target_fold, m_fold_progress - fold_speed * dt);
    }

    // Permanently uncover explored territory around the player (22m horizon)
    int px = static_cast<int>(std::floor(player_pos.x));
    int pz = static_cast<int>(std::floor(player_pos.z));
    const int radius = 22;
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dz * dz <= radius * radius) {
                int wx = px + dx;
                int wz = pz + dz;
                if (wx >= MAP_MIN && wx < MAP_MAX && wz >= MAP_MIN && wz < MAP_MAX) {
                    int idx = (wz - MAP_MIN) * MAP_SPAN + (wx - MAP_MIN);
                    if (m_discovered[idx] == 0) {
                        m_discovered[idx] = 1;
                        m_discovered_count++;
                    }
                }
            }
        }
    }

    // Pan map with mouse dragging while TAB held
    if (m_active && mouse_dragging) {
        m_pan_x -= mouse_dx * (1.0f / m_zoom);
        m_pan_y -= mouse_dy * (1.0f / m_zoom);
        m_orbit_yaw += mouse_dx * 0.35f;
        m_orbit_pitch = glm::clamp(m_orbit_pitch - mouse_dy * 0.35f, -80.0f, 80.0f);
    }
}

float TerrainScanner::discovery_percentage() const {
    // Estimated baseline cavern footprint (~4200 voxels across standard sector)
    float pct = (static_cast<float>(m_discovered_count) / 4200.0f) * 100.0f;
    return std::min(100.0f, std::max(1.0f, pct));
}

void TerrainScanner::refresh_geometry(const World& world, const glm::vec3& player_pos,
                                      const PlayerController& player,
                                      const ExtractionSystem& extraction,
                                      const MissionSystem& mission,
                                      float total_time) {
    m_total_time = total_time;
    if (m_shape_vao == 0) {
        init_buffers();
    }
    if (m_shape_vbo == 0) return;

    m_cached_tris.clear();
    m_cached_lines.clear();
    m_cached_texts.clear();

    m_cached_tris.reserve(16384);
    m_cached_lines.reserve(8192);
    m_cached_texts.reserve(64);

    // ── Update Cavern Voxel Cell Types for Discovered Columns ──
    int py = static_cast<int>(std::floor(player_pos.y));
    int min_y = std::max(1, py - 6);
    int max_y = std::min(48, py + 8);

    // Dynamic layout coordinates based on screen
    float panel_w = std::min(1180.0f, static_cast<float>(m_screen_w) * 0.90f);
    float panel_h = std::min(760.0f, static_cast<float>(m_screen_h) * 0.88f);
    float panel_x = (static_cast<float>(m_screen_w) - panel_w) * 0.5f;
    float panel_y = (static_cast<float>(m_screen_h) - panel_h) * 0.5f;

    float vp_pad = 16.0f;
    float vp_x = panel_x + vp_pad;
    float vp_y = panel_y + 44.0f;
    float vp_w = panel_w * 0.67f;
    float vp_h = panel_h - 60.0f;

    glm::vec2 map_center(vp_x + vp_w * 0.5f, vp_y + vp_h * 0.5f);
    float center_wx = player_pos.x + m_pan_x;
    float center_wz = player_pos.z + m_pan_y;

    float half_span_x = (vp_w * 0.5f) / m_zoom;
    float half_span_z = (vp_h * 0.5f) / m_zoom;

    int min_wx = std::max(MAP_MIN, static_cast<int>(std::floor(center_wx - half_span_x - 1.0f)));
    int max_wx = std::min(MAP_MAX - 1, static_cast<int>(std::ceil(center_wx + half_span_x + 1.0f)));
    int min_wz = std::max(MAP_MIN, static_cast<int>(std::floor(center_wz - half_span_z - 1.0f)));
    int max_wz = std::min(MAP_MAX - 1, static_cast<int>(std::ceil(center_wz + half_span_z + 1.0f)));

    // Reclassify discovered cells in visible region
    for (int wz = min_wz; wz <= max_wz; ++wz) {
        for (int wx = min_wx; wx <= max_wx; ++wx) {
            int idx = (wz - MAP_MIN) * MAP_SPAN + (wx - MAP_MIN);
            if (m_discovered[idx] == 0) continue;

            bool is_floor = false;
            bool is_hazard = false;
            bool has_air = false;
            int floor_y = -1;

            for (int y = min_y; y <= max_y; ++y) {
                Voxel v = world.get_voxel(wx, y, wz);
                if (v.material_id == MAT_AIR) {
                    has_air = true;
                    Voxel below = world.get_voxel(wx, y - 1, wz);
                    if (below.is_solid()) {
                        is_floor = true;
                        floor_y = y;
                        if (below.material_id == MAT_THERMITE_SLAG || below.material_id == MAT_RADIOACTIVE_ORE || below.material_id == MAT_OBSIDIAN_SPIKES) {
                            is_hazard = true;
                        }
                    }
                }
            }

            if (is_floor) {
                bool near_wall = world.get_voxel(wx + 1, floor_y, wz).is_solid() ||
                                 world.get_voxel(wx - 1, floor_y, wz).is_solid() ||
                                 world.get_voxel(wx, floor_y, wz + 1).is_solid() ||
                                 world.get_voxel(wx, floor_y, wz - 1).is_solid();
                if (near_wall) {
                    m_cell_types[idx] = static_cast<uint8_t>(CavernCellType::CaveWallRim);
                } else if (is_hazard) {
                    m_cell_types[idx] = static_cast<uint8_t>(CavernCellType::HazardFloor);
                } else {
                    m_cell_types[idx] = static_cast<uint8_t>(CavernCellType::CaveFloor);
                }
            } else if (has_air) {
                m_cell_types[idx] = static_cast<uint8_t>(CavernCellType::ChasmDrop);
            } else {
                m_cell_types[idx] = static_cast<uint8_t>(CavernCellType::SolidRock);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 1. TACTICAL OVERLAY BACKGROUND & BEZEL
    // ─────────────────────────────────────────────────────────────
    glm::vec4 frame_bg(0.035f, 0.045f, 0.060f, 0.94f);
    glm::vec4 cyan_accent(0.00f, 0.90f, 1.00f, 0.90f);
    glm::vec4 amber_accent(1.00f, 0.70f, 0.00f, 0.95f);
    glm::vec4 green_accent(0.18f, 0.85f, 0.44f, 0.95f);
    glm::vec4 muted_slate(0.45f, 0.52f, 0.60f, 0.85f);

    // Main frame backdrop
    add_quad(m_cached_tris, panel_x, panel_y, panel_w, panel_h, frame_bg);
    add_rect_outline(m_cached_lines, panel_x, panel_y, panel_w, panel_h, glm::vec4(0.0f, 0.75f, 0.95f, 0.45f));

    // Corner decorative brackets
    float corner_len = 18.0f;
    add_line(m_cached_lines, {panel_x, panel_y + corner_len}, {panel_x, panel_y}, cyan_accent);
    add_line(m_cached_lines, {panel_x, panel_y}, {panel_x + corner_len, panel_y}, cyan_accent);
    add_line(m_cached_lines, {panel_x + panel_w - corner_len, panel_y}, {panel_x + panel_w, panel_y}, cyan_accent);
    add_line(m_cached_lines, {panel_x + panel_w, panel_y}, {panel_x + panel_w, panel_y + corner_len}, cyan_accent);
    add_line(m_cached_lines, {panel_x, panel_y + panel_h - corner_len}, {panel_x, panel_y + panel_h}, cyan_accent);
    add_line(m_cached_lines, {panel_x, panel_y + panel_h}, {panel_x + corner_len, panel_y + panel_h}, cyan_accent);
    add_line(m_cached_lines, {panel_x + panel_w - corner_len, panel_y + panel_h}, {panel_x + panel_w, panel_y + panel_h}, cyan_accent);
    add_line(m_cached_lines, {panel_x + panel_w, panel_y + panel_h}, {panel_x + panel_w, panel_y + panel_h - corner_len}, cyan_accent);

    // Header bar
    add_quad(m_cached_tris, panel_x + 1.0f, panel_y + 1.0f, panel_w - 2.0f, 34.0f, glm::vec4(0.06f, 0.08f, 0.11f, 0.95f));
    add_line(m_cached_lines, {panel_x, panel_y + 35.0f}, {panel_x + panel_w, panel_y + 35.0f}, glm::vec4(0.0f, 0.85f, 1.0f, 0.65f));

    m_cached_texts.push_back({
        "// DELVER TACTICAL CARTOGRAPHY // 2D CAVERN TOPOGRAPHY",
        panel_x + 18.0f, panel_y + 9.0f, 1.10f, cyan_accent
    });

    std::stringstream sub_ss;
    sub_ss << "SECTOR MAPPED: " << static_cast<int>(std::round(discovery_percentage())) << "%  //  SCAN: ACTIVE";
    m_cached_texts.push_back({
        sub_ss.str(),
        panel_x + panel_w - 320.0f, panel_y + 11.0f, 0.85f, muted_slate
    });

    // ─────────────────────────────────────────────────────────────
    // 2. 2D MAP VIEWPORT CANVAS & TOPOGRAPHY
    // ─────────────────────────────────────────────────────────────
    // Canvas background (dark excavated cavern stone)
    add_quad(m_cached_tris, vp_x, vp_y, vp_w, vp_h, glm::vec4(0.055f, 0.048f, 0.040f, 0.98f));

    // Subtle tactical radar coordinate grid
    glm::vec4 grid_col(0.12f, 0.16f, 0.22f, 0.35f);
    for (float gx = vp_x + std::fmod(-center_wx * m_zoom, 32.0f); gx < vp_x + vp_w; gx += 32.0f) {
        if (gx > vp_x) add_line(m_cached_lines, {gx, vp_y}, {gx, vp_y + vp_h}, grid_col);
    }
    for (float gy = vp_y + std::fmod(-center_wz * m_zoom, 32.0f); gy < vp_y + vp_h; gy += 32.0f) {
        if (gy > vp_y) add_line(m_cached_lines, {vp_x, gy}, {vp_x + vp_w, gy}, grid_col);
    }

    // Render discovered cavern tiles
    // CONCEPT COLOR PALETTE MATCHING USER IMAGE:
    // - Cave Floor: slate stone brown-gray #34302B
    // - Cave Wall Rim: warm golden-amber carved cliff contour #BA8A42 / #D4A359
    // - Solid Rock: dark bedrock perimeter #1B1815
    for (int wz = min_wz; wz <= max_wz; ++wz) {
        for (int wx = min_wx; wx <= max_wx; ++wx) {
            int idx = (wz - MAP_MIN) * MAP_SPAN + (wx - MAP_MIN);
            if (m_discovered[idx] == 0) {
                // Undiscovered / Fog of War cell (pitch black)
                float sx = map_center.x + (static_cast<float>(wx) - center_wx) * m_zoom;
                float sy = map_center.y + (static_cast<float>(wz) - center_wz) * m_zoom;
                add_quad(m_cached_tris, sx, sy, m_zoom, m_zoom, glm::vec4(0.025f, 0.025f, 0.025f, 0.98f));
                continue;
            }

            CavernCellType ctype = static_cast<CavernCellType>(m_cell_types[idx]);
            float sx = map_center.x + (static_cast<float>(wx) - center_wx) * m_zoom;
            float sy = map_center.y + (static_cast<float>(wz) - center_wz) * m_zoom;

            // Subtle organic stone variation
            float var = static_cast<float>((wx * 7 + wz * 13) % 7) * 0.012f;

            if (ctype == CavernCellType::CaveFloor) {
                // Carved walkable cave floor
                glm::vec4 floor_col(0.24f + var, 0.22f + var, 0.20f + var, 0.96f);
                add_quad(m_cached_tris, sx, sy, m_zoom, m_zoom, floor_col);
            } else if (ctype == CavernCellType::CaveWallRim) {
                // Warm golden-amber rock cliff contour (directly matching concept image)
                glm::vec4 rim_col(0.70f + var * 0.5f, 0.50f + var * 0.5f, 0.22f, 0.98f);
                add_quad(m_cached_tris, sx, sy, m_zoom, m_zoom, rim_col);
                // Golden edge highlight line
                add_rect_outline(m_cached_lines, sx, sy, m_zoom, m_zoom, glm::vec4(0.85f, 0.65f, 0.28f, 0.85f));
            } else if (ctype == CavernCellType::HazardFloor) {
                // Molten or toxic hot zone
                glm::vec4 hazard_col(0.85f, 0.32f, 0.12f, 0.95f);
                add_quad(m_cached_tris, sx, sy, m_zoom, m_zoom, hazard_col);
            } else if (ctype == CavernCellType::ChasmDrop) {
                // Abyssal vertical drop pit
                glm::vec4 pit_col(0.09f, 0.08f, 0.07f, 0.98f);
                add_quad(m_cached_tris, sx, sy, m_zoom, m_zoom, pit_col);
            } else if (ctype == CavernCellType::SolidRock) {
                // Dark bedrock adjacent to cave boundary
                glm::vec4 rock_col(0.11f, 0.09f, 0.08f, 0.92f);
                add_quad(m_cached_tris, sx, sy, m_zoom, m_zoom, rock_col);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 3. BREADCRUMBS TRAIL (Visited Delver Footsteps)
    // ─────────────────────────────────────────────────────────────
    const auto& crumbs = player.breadcrumbs();
    glm::vec4 crumb_col(0.0f, 0.85f, 1.0f, 0.65f);
    for (size_t i = 0; i < crumbs.size(); ++i) {
        float csx = map_center.x + (crumbs[i].x - center_wx) * m_zoom;
        float csy = map_center.y + (crumbs[i].z - center_wz) * m_zoom;
        add_diamond(m_cached_tris, {csx, csy}, 2.0f, crumb_col);
        if (i + 1 < crumbs.size()) {
            float next_sx = map_center.x + (crumbs[i + 1].x - center_wx) * m_zoom;
            float next_sy = map_center.y + (crumbs[i + 1].z - center_wz) * m_zoom;
            add_line(m_cached_lines, {csx, csy}, {next_sx, next_sy}, glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 4. OBJECTIVES & MARKERS WHERE TO GO
    // ─────────────────────────────────────────────────────────────
    // Primary vs Secondary objective determination
    bool need_vault = mission.vault().exists && !mission.is_relic_retrieved();
    glm::vec3 primary_target = need_vault ? glm::vec3(mission.vault().door_pos) : extraction.beacon_position();
    std::string primary_name = need_vault ? "PRECURSOR VAULT" : "EXTRACTION BEACON";
    glm::vec4 primary_col = need_vault ? cyan_accent : green_accent;

    // Player screen position on map canvas
    float psx = map_center.x + (player_pos.x - center_wx) * m_zoom;
    float psy = map_center.y + (player_pos.z - center_wz) * m_zoom;

    // Dynamic Waypoint Navigation Route Line (from player to active objective)
    float tsx = map_center.x + (primary_target.x - center_wx) * m_zoom;
    float tsy = map_center.y + (primary_target.z - center_wz) * m_zoom;
    glm::vec2 route_vec(tsx - psx, tsy - psy);
    float route_len = glm::length(route_vec);
    if (route_len > 12.0f) {
        glm::vec2 route_dir = route_vec / route_len;
        float dash_len = 10.0f;
        float gap_len = 7.0f;
        float step = dash_len + gap_len;
        float phase = std::fmod(total_time * 28.0f, step);

        for (float d = phase; d < route_len - 14.0f; d += step) {
            float s1 = std::max(0.0f, d);
            float s2 = std::min(route_len, d + dash_len);
            if (s2 > s1) {
                add_line(m_cached_lines, {psx + route_dir.x * s1, psy + route_dir.y * s1},
                                         {psx + route_dir.x * s2, psy + route_dir.y * s2},
                                         primary_col);
            }
        }
    }

    // ── Extraction Beacon Marker ──
    glm::vec3 bpos = extraction.beacon_position();
    float bsx = map_center.x + (bpos.x - center_wx) * m_zoom;
    float bsy = map_center.y + (bpos.z - center_wz) * m_zoom;
    float b_dist = glm::distance(glm::vec2(player_pos.x, player_pos.z), glm::vec2(bpos.x, bpos.z));
    std::string b_bearing = get_bearing_str(player_pos, bpos);

    bool b_in_view = (bsx >= vp_x + 18.0f && bsx <= vp_x + vp_w - 18.0f &&
                      bsy >= vp_y + 18.0f && bsy <= vp_y + vp_h - 18.0f);

    float b_pulse = std::fmod(total_time * 2.5f, 1.0f);
    float radar_r = 6.0f + b_pulse * 24.0f;

    if (b_in_view) {
        // Expanding radar beacon wave
        add_circle_outline(m_cached_lines, {bsx, bsy}, radar_r, 16, glm::vec4(0.18f, 0.95f, 0.44f, 1.0f - b_pulse));
        // Solid beacon diamond + inner core
        add_diamond(m_cached_tris, {bsx, bsy}, 8.0f, green_accent);
        add_diamond(m_cached_tris, {bsx, bsy}, 4.0f, glm::vec4(1.0f));

        // In-map text badge
        std::string badge = "EXTRACTION [" + std::to_string(static_cast<int>(std::round(b_dist))) + "m]";
        float badge_w = FontRenderer::get_rendered_width(badge, 0.75f);
        add_quad(m_cached_tris, bsx - badge_w * 0.5f - 4.0f, bsy - 22.0f, badge_w + 8.0f, 14.0f, glm::vec4(0.06f, 0.12f, 0.08f, 0.90f));
        add_rect_outline(m_cached_lines, bsx - badge_w * 0.5f - 4.0f, bsy - 22.0f, badge_w + 8.0f, 14.0f, green_accent);
        m_cached_texts.push_back({badge, bsx - badge_w * 0.5f, bsy - 21.0f, 0.75f, green_accent});
    } else {
        // Off-screen edge guidance arrow
        float edge_pad = 22.0f;
        float ex = glm::clamp(bsx, vp_x + edge_pad, vp_x + vp_w - edge_pad);
        float ey = glm::clamp(bsy, vp_y + edge_pad, vp_y + vp_h - edge_pad);
        add_diamond(m_cached_tris, {ex, ey}, 6.0f, green_accent);
        std::string edge_txt = ">> EXTRACT " + std::to_string(static_cast<int>(std::round(b_dist))) + "m";
        m_cached_texts.push_back({edge_txt, ex - 35.0f, ey - 18.0f, 0.72f, green_accent});
    }

    // ── Precursor Vault Marker ──
    if (mission.vault().exists) {
        glm::vec3 vpos = glm::vec3(mission.vault().door_pos);
        float vsx = map_center.x + (vpos.x - center_wx) * m_zoom;
        float vsy = map_center.y + (vpos.z - center_wz) * m_zoom;
        float v_dist = glm::distance(glm::vec2(player_pos.x, player_pos.z), glm::vec2(vpos.x, vpos.z));
        std::string v_bearing = get_bearing_str(player_pos, vpos);

        bool v_in_view = (vsx >= vp_x + 18.0f && vsx <= vp_x + vp_w - 18.0f &&
                          vsy >= vp_y + 18.0f && vsy <= vp_y + vp_h - 18.0f);

        glm::vec4 vault_col = mission.is_relic_retrieved() ? glm::vec4(0.5f, 0.5f, 0.6f, 0.7f) : cyan_accent;

        if (v_in_view) {
            float v_pulse = 0.75f + 0.25f * std::sin(total_time * 5.0f);
            add_diamond(m_cached_tris, {vsx, vsy}, 7.5f, vault_col * v_pulse);
            add_circle_outline(m_cached_lines, {vsx, vsy}, 12.0f, 12, vault_col);

            std::string v_label = mission.is_relic_retrieved() ? "VAULT [SECURED]" : ("VAULT [" + std::to_string(static_cast<int>(std::round(v_dist))) + "m]");
            float v_w = FontRenderer::get_rendered_width(v_label, 0.75f);
            add_quad(m_cached_tris, vsx - v_w * 0.5f - 4.0f, vsy + 10.0f, v_w + 8.0f, 14.0f, glm::vec4(0.05f, 0.08f, 0.12f, 0.90f));
            add_rect_outline(m_cached_lines, vsx - v_w * 0.5f - 4.0f, vsy + 10.0f, v_w + 8.0f, 14.0f, vault_col);
            m_cached_texts.push_back({v_label, vsx - v_w * 0.5f, vsy + 11.0f, 0.75f, vault_col});
        } else {
            float edge_pad = 22.0f;
            float vx_e = glm::clamp(vsx, vp_x + edge_pad, vp_x + vp_w - edge_pad);
            float vy_e = glm::clamp(vsy, vp_y + edge_pad, vp_y + vp_h - edge_pad);
            add_diamond(m_cached_tris, {vx_e, vy_e}, 6.0f, vault_col);
            std::string v_edge = ">> VAULT " + std::to_string(static_cast<int>(std::round(v_dist))) + "m";
            m_cached_texts.push_back({v_edge, vx_e - 30.0f, vy_e - 18.0f, 0.72f, vault_col});
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 5. PLAYER DELVER CHEVRON & VIEW CONE
    // ─────────────────────────────────────────────────────────────
    // Flashlight / Visor Field of View Cone
    add_view_cone(m_cached_tris, {psx, psy}, player.yaw(), 55.0f, 16.0f * m_zoom, glm::vec4(1.00f, 0.75f, 0.10f, 0.16f));

    // Outer tactical aura ring
    add_circle_outline(m_cached_lines, {psx, psy}, 14.0f, 18, glm::vec4(1.00f, 0.70f, 0.00f, 0.40f));

    // Sharp Military Delver Chevron pointing in player look yaw direction
    add_player_chevron(m_cached_tris, m_cached_lines, {psx, psy}, player.yaw(), 9.0f, amber_accent);

    // ─────────────────────────────────────────────────────────────
    // 6. VIEWPORT FRAME, COMPASS & SCALE BAR
    // ─────────────────────────────────────────────────────────────
    add_rect_outline(m_cached_lines, vp_x, vp_y, vp_w, vp_h, cyan_accent);

    // Cardinal compass points
    m_cached_texts.push_back({"[ NORTH (-Z) ]", vp_x + vp_w * 0.5f - 42.0f, vp_y + 4.0f, 0.78f, cyan_accent});
    m_cached_texts.push_back({"[ SOUTH (+Z) ]", vp_x + vp_w * 0.5f - 42.0f, vp_y + vp_h - 16.0f, 0.78f, cyan_accent});
    m_cached_texts.push_back({"W (-X)", vp_x + 6.0f, vp_y + vp_h * 0.5f - 6.0f, 0.78f, cyan_accent});
    m_cached_texts.push_back({"E (+X)", vp_x + vp_w - 42.0f, vp_y + vp_h * 0.5f - 6.0f, 0.78f, cyan_accent});

    // Distance Scale Bar (10 meters)
    float scale_bar_w = 10.0f * m_zoom;
    float sb_x = vp_x + 16.0f;
    float sb_y = vp_y + vp_h - 24.0f;
    add_line(m_cached_lines, {sb_x, sb_y}, {sb_x + scale_bar_w, sb_y}, amber_accent);
    add_line(m_cached_lines, {sb_x, sb_y - 4.0f}, {sb_x, sb_y + 4.0f}, amber_accent);
    add_line(m_cached_lines, {sb_x + scale_bar_w, sb_y - 4.0f}, {sb_x + scale_bar_w, sb_y + 4.0f}, amber_accent);
    m_cached_texts.push_back({"10 METERS", sb_x + 8.0f, sb_y - 14.0f, 0.72f, amber_accent});

    // ─────────────────────────────────────────────────────────────
    // 7. RIGHT TACTICAL SIDEBAR: DIRECTIVES & TELEMETRY
    // ─────────────────────────────────────────────────────────────
    float sb_col_x = panel_x + vp_w + 28.0f;
    float sb_col_w = panel_w - vp_w - 44.0f;
    float cur_y = vp_y;

    // Sidebar Section Header
    add_quad(m_cached_tris, sb_col_x, cur_y, sb_col_w, 24.0f, glm::vec4(0.08f, 0.11f, 0.16f, 0.95f));
    add_line(m_cached_lines, {sb_col_x, cur_y + 24.0f}, {sb_col_x + sb_col_w, cur_y + 24.0f}, cyan_accent);
    m_cached_texts.push_back({"EXPEDITION DIRECTIVES", sb_col_x + 10.0f, cur_y + 5.0f, 0.95f, cyan_accent});
    cur_y += 32.0f;

    // Card 1: Active Waypoint / Primary Directive
    float card1_h = 76.0f;
    add_quad(m_cached_tris, sb_col_x, cur_y, sb_col_w, card1_h, glm::vec4(0.06f, 0.08f, 0.12f, 0.90f));
    add_rect_outline(m_cached_lines, sb_col_x, cur_y, sb_col_w, card1_h, primary_col);

    m_cached_texts.push_back({">> PRIMARY WAYPOINT <<", sb_col_x + 10.0f, cur_y + 8.0f, 0.78f, primary_col});
    m_cached_texts.push_back({primary_name, sb_col_x + 10.0f, cur_y + 24.0f, 1.05f, Typography::COLOR_PRIMARY});

    std::stringstream p_dist_ss;
    float p_dist = glm::distance(glm::vec2(player_pos.x, player_pos.z), glm::vec2(primary_target.x, primary_target.z));
    p_dist_ss << "Distance: " << std::fixed << std::setprecision(1) << p_dist << "m  [" << get_bearing_str(player_pos, primary_target) << "]";
    m_cached_texts.push_back({p_dist_ss.str(), sb_col_x + 10.0f, cur_y + 44.0f, 0.85f, amber_accent});
    m_cached_texts.push_back({"Route Guidance: ACTIVE (Dotted Vector)", sb_col_x + 10.0f, cur_y + 58.0f, 0.72f, muted_slate});
    cur_y += card1_h + 12.0f;

    // Card 2: Evacuation Status
    float card2_h = 80.0f;
    add_quad(m_cached_tris, sb_col_x, cur_y, sb_col_w, card2_h, glm::vec4(0.06f, 0.08f, 0.12f, 0.90f));
    add_rect_outline(m_cached_lines, sb_col_x, cur_y, sb_col_w, card2_h, green_accent);

    m_cached_texts.push_back({"EXTRACTION BEACON", sb_col_x + 10.0f, cur_y + 8.0f, 0.92f, green_accent});

    std::string evac_status;
    if (extraction.phase() == ExtractionPhase::PodLanded) {
        evac_status = "POD GROUNDED // BOARD TO ESCAPE";
    } else if (extraction.phase() == ExtractionPhase::BeaconDeployed) {
        evac_status = "TOUCHDOWN COUNTDOWN: " + std::to_string(static_cast<int>(std::round(extraction.countdown()))) + "s";
    } else {
        evac_status = "BEACON READY AT LANDING BAY";
    }
    m_cached_texts.push_back({evac_status, sb_col_x + 10.0f, cur_y + 26.0f, 0.82f, Typography::COLOR_PRIMARY});

    std::stringstream b_dist_ss;
    b_dist_ss << "Beacon Dist: " << std::fixed << std::setprecision(1) << b_dist << "m  [" << b_bearing << "]";
    m_cached_texts.push_back({b_dist_ss.str(), sb_col_x + 10.0f, cur_y + 44.0f, 0.82f, muted_slate});

    std::string zone_txt = extraction.is_player_in_perimeter(player_pos) ? "Delver Status: INSIDE LZ PERIMETER" : "Delver Status: OUTSIDE LZ PERIMETER";
    m_cached_texts.push_back({zone_txt, sb_col_x + 10.0f, cur_y + 60.0f, 0.75f, extraction.is_player_in_perimeter(player_pos) ? green_accent : amber_accent});
    cur_y += card2_h + 12.0f;

    // Card 3: Precursor Vault Objective
    if (mission.vault().exists) {
        float card3_h = 68.0f;
        add_quad(m_cached_tris, sb_col_x, cur_y, sb_col_w, card3_h, glm::vec4(0.06f, 0.08f, 0.12f, 0.90f));
        add_rect_outline(m_cached_lines, sb_col_x, cur_y, sb_col_w, card3_h, cyan_accent);

        m_cached_texts.push_back({"PRECURSOR RELIC VAULT", sb_col_x + 10.0f, cur_y + 8.0f, 0.92f, cyan_accent});

        std::string v_state_str;
        if (mission.is_relic_retrieved()) {
            v_state_str = "STATUS: RELIC RETRIEVED (SECURED)";
        } else if (mission.vault().state == VaultObjectiveState::Breached) {
            v_state_str = "STATUS: DOOR BREACHED // TAKE RELIC";
        } else {
            v_state_str = "STATUS: SEALED BLAST DOOR (CHARGE)";
        }
        m_cached_texts.push_back({v_state_str, sb_col_x + 10.0f, cur_y + 26.0f, 0.82f, Typography::COLOR_PRIMARY});

        float v_dist = glm::distance(glm::vec2(player_pos.x, player_pos.z), glm::vec2(mission.vault().door_pos.x, mission.vault().door_pos.z));
        std::stringstream v_ss;
        v_ss << "Vault Dist: " << std::fixed << std::setprecision(1) << v_dist << "m  [" << get_bearing_str(player_pos, glm::vec3(mission.vault().door_pos)) << "]";
        m_cached_texts.push_back({v_ss.str(), sb_col_x + 10.0f, cur_y + 44.0f, 0.80f, muted_slate});
        cur_y += card3_h + 12.0f;
    }

    // Card 4: Delver Positional Telemetry
    float card4_h = 74.0f;
    add_quad(m_cached_tris, sb_col_x, cur_y, sb_col_w, card4_h, glm::vec4(0.05f, 0.07f, 0.10f, 0.85f));
    add_rect_outline(m_cached_lines, sb_col_x, cur_y, sb_col_w, card4_h, glm::vec4(0.35f, 0.45f, 0.55f, 0.60f));

    m_cached_texts.push_back({"DELVER AVIONICS TELEMETRY", sb_col_x + 10.0f, cur_y + 8.0f, 0.80f, amber_accent});

    std::stringstream loc_ss;
    loc_ss << "POS: X: " << std::fixed << std::setprecision(1) << player_pos.x
           << " | Z: " << player_pos.z << " | DEPTH: -" << std::max(0.0f, 32.0f - player_pos.y) << "m";
    m_cached_texts.push_back({loc_ss.str(), sb_col_x + 10.0f, cur_y + 26.0f, 0.78f, Typography::COLOR_PRIMARY});

    std::stringstream head_ss;
    float yaw_norm = std::fmod(player.yaw(), 360.0f);
    if (yaw_norm < 0.0f) yaw_norm += 360.0f;
    head_ss << "HEADING: " << static_cast<int>(std::round(yaw_norm)) << " deg  //  TRAIL: "
            << player.breadcrumbs().size() << " BREADCRUMBS";
    m_cached_texts.push_back({head_ss.str(), sb_col_x + 10.0f, cur_y + 42.0f, 0.78f, muted_slate});
    cur_y += card4_h + 12.0f;

    // Card 5: Map Legend & Controls
    float card5_h = panel_y + panel_h - cur_y - 12.0f;
    if (card5_h > 40.0f) {
        add_quad(m_cached_tris, sb_col_x, cur_y, sb_col_w, card5_h, glm::vec4(0.04f, 0.06f, 0.08f, 0.85f));
        add_rect_outline(m_cached_lines, sb_col_x, cur_y, sb_col_w, card5_h, glm::vec4(0.25f, 0.35f, 0.45f, 0.50f));

        m_cached_texts.push_back({"MAP LEGEND & CONTROLS", sb_col_x + 10.0f, cur_y + 8.0f, 0.78f, cyan_accent});

        // Legend boxes
        add_quad(m_cached_tris, sb_col_x + 10.0f, cur_y + 26.0f, 10.0f, 10.0f, glm::vec4(0.24f, 0.22f, 0.20f, 1.0f));
        m_cached_texts.push_back({"Discovered Cave Floor", sb_col_x + 26.0f, cur_y + 26.0f, 0.72f, muted_slate});

        add_quad(m_cached_tris, sb_col_x + 10.0f, cur_y + 40.0f, 10.0f, 10.0f, glm::vec4(0.70f, 0.50f, 0.22f, 1.0f));
        m_cached_texts.push_back({"Cavern Wall Rim (Rock Contour)", sb_col_x + 26.0f, cur_y + 40.0f, 0.72f, muted_slate});

        add_diamond(m_cached_tris, {sb_col_x + 15.0f, cur_y + 58.0f}, 5.0f, amber_accent);
        m_cached_texts.push_back({"Delver Chevron & Look Heading", sb_col_x + 26.0f, cur_y + 54.0f, 0.72f, amber_accent});

        add_diamond(m_cached_tris, {sb_col_x + 15.0f, cur_y + 72.0f}, 5.0f, green_accent);
        m_cached_texts.push_back({"Extraction Beacon / Pod", sb_col_x + 26.0f, cur_y + 68.0f, 0.72f, green_accent});

        m_cached_texts.push_back({"[TAB] Close Map  |  [DRAG] Pan Cavern", sb_col_x + 10.0f, cur_y + card5_h - 16.0f, 0.75f, amber_accent});
    }
}

void TerrainScanner::draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color, float alpha) {
    if (text.empty() || m_text_prog == 0 || m_font_tex == 0) return;
    std::vector<float> verts;
    verts.reserve(text.size() * 24);
    FontRenderer::build_text_vertices(text, x, y, scale * FontRenderer::DRAW_SCALE_FACTOR, verts);
    if (verts.empty()) return;

    glUseProgram(m_text_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_font_tex);
    glUniform1i(glGetUniformLocation(m_text_prog, "uFontTexture"), 0);
    glUniform4f(glGetUniformLocation(m_text_prog, "uTextColor"), color.r, color.g, color.b, color.a);
    glUniform1f(glGetUniformLocation(m_text_prog, "uAlpha"), alpha);

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(m_screen_w), static_cast<float>(m_screen_h), 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(m_text_prog, "uProjection"), 1, GL_FALSE, &proj[0][0]);

    glBindVertexArray(m_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_text_vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 4));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TerrainScanner::render(int screen_width, int screen_height, const glm::vec3& /*player_pos*/, float /*player_yaw*/) {
    m_screen_w = screen_width;
    m_screen_h = screen_height;

    if (m_shape_vao == 0) {
        init_buffers();
    }
    if (m_fold_progress <= 0.001f || m_shape_prog == 0) {
        return;
    }

    // Modern Alpha Blending & Disabled Depth Test for crisp 2D Overlay
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(screen_width), static_cast<float>(screen_height), 0.0f);

    glUseProgram(m_shape_prog);
    glUniformMatrix4fv(glGetUniformLocation(m_shape_prog, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniform1f(glGetUniformLocation(m_shape_prog, "uAlpha"), m_fold_progress);

    // 1. Draw 2D Filled Triangles
    if (!m_cached_tris.empty()) {
        glBindVertexArray(m_shape_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_shape_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_cached_tris.size() * sizeof(Shape2DVertex), m_cached_tris.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_cached_tris.size()));
    }

    // 2. Draw 2D Wireframe Lines
    if (!m_cached_lines.empty()) {
        glBindVertexArray(m_shape_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_shape_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_cached_lines.size() * sizeof(Shape2DVertex), m_cached_lines.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_cached_lines.size()));
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 3. Draw All Cached Labels & Directives Text
    for (const auto& cmd : m_cached_texts) {
        draw_text(cmd.text, cmd.x, cmd.y, cmd.scale, cmd.color, m_fold_progress);
    }
}

} // namespace Voidfall
