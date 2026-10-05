#include "terrain_scanner.hpp"
#include "../voxel/world.hpp"
#include "../player/controller.hpp"
#include "../systems/extraction.hpp"
#include "../systems/mission_system.hpp"
#include "../core/logger.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>

namespace Voidfall {

namespace {

const char* SCANNER_VS = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
uniform mat4 uMVP;
uniform float uAlpha;
uniform vec3 uGauntletAnchor;
uniform float uFoldProgress;
out vec4 vColor;
void main() {
    // Project expanding outwards from suit left gauntlet rig
    vec3 animatedPos = mix(uGauntletAnchor, aPos, uFoldProgress);
    gl_Position = uMVP * vec4(animatedPos, 1.0);
    vColor = vec4(aColor.rgb, aColor.a * uAlpha * uFoldProgress);
}
)";

const char* SCANNER_FS = R"(#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

inline uint64_t hash_cell(int x, int y, int z) {
    uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(x));
    uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(y));
    uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(z));
    return (ux * 73856093u) ^ (uy * 19349663u) ^ (uz * 83492791u);
}

} // namespace

TerrainScanner::TerrainScanner() {
    // Lazily initialized when OpenGL context is active
}

TerrainScanner::~TerrainScanner() {
    if (m_vao != 0 && glDeleteVertexArrays != nullptr) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo != 0 && glDeleteBuffers != nullptr) glDeleteBuffers(1, &m_vbo);
    if (m_shader_prog != 0 && glDeleteProgram != nullptr) glDeleteProgram(m_shader_prog);
}

void TerrainScanner::init_buffers() {
    if (m_vao != 0 || m_shader_prog != 0) return;
    if (glCreateShader == nullptr) return; // GLAD not loaded yet
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &SCANNER_VS, nullptr);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &SCANNER_FS, nullptr);
    glCompileShader(fs);

    m_shader_prog = glCreateProgram();
    glAttachShader(m_shader_prog, vs);
    glAttachShader(m_shader_prog, fs);
    glLinkProgram(m_shader_prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ScannerLineVertex), reinterpret_cast<void*>(0));
    // Color (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ScannerLineVertex), reinterpret_cast<void*>(sizeof(glm::vec3)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TerrainScanner::reset() {
    m_active = false;
    m_fold_progress = 0.0f;
    m_visited_cells.clear();
    m_last_scan_pos = glm::vec3(0.0f);
    m_geom_refresh_timer = 0.0f;
    m_orbit_yaw = 45.0f;
    m_orbit_pitch = 32.0f;
    m_orbit_distance = 28.0f;
}

void TerrainScanner::update(float dt, bool tab_held, const glm::vec3& player_pos,
                            float mouse_dx, float mouse_dy, bool mouse_dragging) {
    m_active = tab_held;

    // Smoothly fold / unfold projection (folds into / out of left wrist rig)
    float target_fold = m_active ? 1.0f : 0.0f;
    float fold_speed = 7.5f;
    if (m_fold_progress < target_fold) {
        m_fold_progress = std::min(target_fold, m_fold_progress + fold_speed * dt);
    } else if (m_fold_progress > target_fold) {
        m_fold_progress = std::max(target_fold, m_fold_progress - fold_speed * dt);
    }

    // Mark visited cells around player (2m voxel bins)
    int px = static_cast<int>(std::floor(player_pos.x / 2.0f));
    int py = static_cast<int>(std::floor(player_pos.y / 2.0f));
    int pz = static_cast<int>(std::floor(player_pos.z / 2.0f));
    for (int dx = -5; dx <= 5; ++dx) {
        for (int dy = -4; dy <= 4; ++dy) {
            for (int dz = -5; dz <= 5; ++dz) {
                m_visited_cells.insert(hash_cell(px + dx, py + dy, pz + dz));
            }
        }
    }

    // Orbit camera mouse dragging
    if (m_active && mouse_dragging) {
        m_orbit_yaw += mouse_dx * 0.35f;
        m_orbit_pitch = glm::clamp(m_orbit_pitch - mouse_dy * 0.35f, -80.0f, 80.0f);
    }
}

void TerrainScanner::add_wire_box(std::vector<ScannerLineVertex>& lines, const glm::vec3& min_p, const glm::vec3& max_p, const glm::vec4& color) {
    // 12 edges of a box
    glm::vec3 c[8] = {
        {min_p.x, min_p.y, min_p.z},
        {max_p.x, min_p.y, min_p.z},
        {max_p.x, max_p.y, min_p.z},
        {min_p.x, max_p.y, min_p.z},
        {min_p.x, min_p.y, max_p.z},
        {max_p.x, min_p.y, max_p.z},
        {max_p.x, max_p.y, max_p.z},
        {min_p.x, max_p.y, max_p.z}
    };
    int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    for (int i = 0; i < 12; ++i) {
        lines.push_back({c[edges[i][0]], color});
        lines.push_back({c[edges[i][1]], color});
    }
}

void TerrainScanner::add_directional_cone(std::vector<ScannerLineVertex>& lines, const glm::vec3& apex, const glm::vec3& dir, float height, float radius, const glm::vec4& color) {
    glm::vec3 fwd = (glm::length(dir) > 0.001f) ? glm::normalize(dir) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(fwd, up);
    if (glm::length(right) < 0.001f) right = glm::vec3(1.0f, 0.0f, 0.0f);
    else right = glm::normalize(right);
    up = glm::normalize(glm::cross(right, fwd));

    glm::vec3 base_center = apex - fwd * height;
    const int segments = 8;
    glm::vec3 pts[segments];
    for (int i = 0; i < segments; ++i) {
        float angle = static_cast<float>(i) * (2.0f * glm::pi<float>() / static_cast<float>(segments));
        pts[i] = base_center + right * (std::cos(angle) * radius) + up * (std::sin(angle) * radius);
        lines.push_back({apex, color});
        lines.push_back({pts[i], color});
    }
    for (int i = 0; i < segments; ++i) {
        lines.push_back({pts[i], color});
        lines.push_back({pts[(i + 1) % segments], color});
    }
}

void TerrainScanner::add_lock_icon(std::vector<ScannerLineVertex>& lines, const glm::vec3& center, float size, const glm::vec4& color) {
    // Box body + arch shackle
    glm::vec3 b_min = center - glm::vec3(size * 0.5f, size * 0.6f, size * 0.25f);
    glm::vec3 b_max = center + glm::vec3(size * 0.5f, 0.0f, size * 0.25f);
    add_wire_box(lines, b_min, b_max, color);

    // Arch shackle above body
    float arch_r = size * 0.35f;
    glm::vec3 arch_center = center + glm::vec3(0.0f, 0.0f, 0.0f);
    const int segs = 6;
    for (int i = 0; i < segs; ++i) {
        float a1 = static_cast<float>(i) * (glm::pi<float>() / static_cast<float>(segs));
        float a2 = static_cast<float>(i + 1) * (glm::pi<float>() / static_cast<float>(segs));
        glm::vec3 p1 = arch_center + glm::vec3(std::cos(a1) * arch_r, std::sin(a1) * arch_r, 0.0f);
        glm::vec3 p2 = arch_center + glm::vec3(std::cos(a2) * arch_r, std::sin(a2) * arch_r, 0.0f);
        lines.push_back({p1, color});
        lines.push_back({p2, color});
    }
}

void TerrainScanner::add_anchor_marker(std::vector<ScannerLineVertex>& lines, const glm::vec3& center, float size, const glm::vec4& color) {
    // Vertical mast
    lines.push_back({center + glm::vec3(0.0f, size * 0.8f, 0.0f), color});
    lines.push_back({center - glm::vec3(0.0f, size * 0.6f, 0.0f), color});
    // Crossbar
    lines.push_back({center + glm::vec3(-size * 0.4f, size * 0.3f, 0.0f), color});
    lines.push_back({center + glm::vec3(size * 0.4f, size * 0.3f, 0.0f), color});
    // Anchor flukes (curved arc)
    lines.push_back({center + glm::vec3(-size * 0.5f, -size * 0.2f, 0.0f), color});
    lines.push_back({center - glm::vec3(0.0f, size * 0.6f, 0.0f), color});
    lines.push_back({center - glm::vec3(0.0f, size * 0.6f, 0.0f), color});
    lines.push_back({center + glm::vec3(size * 0.5f, -size * 0.2f, 0.0f), color});
}

void TerrainScanner::refresh_geometry(const World& world, const glm::vec3& player_pos,
                                      const PlayerController& player,
                                      const ExtractionSystem& extraction,
                                      const MissionSystem& mission,
                                      float total_time) {
    if (m_vao == 0) {
        init_buffers();
    }
    if (m_vbo == 0) return;

    std::vector<ScannerLineVertex> lines;
    lines.reserve(4096);

    glm::vec4 wire_color(0.0f, 0.95f, 1.0f, 0.65f); // #00F2FF wireframe contour lines

    int min_x = static_cast<int>(std::floor(player_pos.x - SCAN_RANGE));
    int max_x = static_cast<int>(std::ceil(player_pos.x + SCAN_RANGE));
    int min_y = std::max(1, static_cast<int>(std::floor(player_pos.y - 14.0f)));
    int max_y = std::min(126, static_cast<int>(std::ceil(player_pos.y + 14.0f)));
    int min_z = static_cast<int>(std::floor(player_pos.z - SCAN_RANGE));
    int max_z = static_cast<int>(std::ceil(player_pos.z + SCAN_RANGE));

    // Occupancy grid boundary search (stride = 2 for performance)
    for (int y = min_y; y <= max_y; y += 2) {
        for (int z = min_z; z <= max_z; z += 2) {
            for (int x = min_x; x <= max_x; x += 2) {
                // Check if in visited occupancy cells
                int cx = x / 2;
                int cy = y / 2;
                int cz = z / 2;
                if (m_visited_cells.find(hash_cell(cx, cy, cz)) == m_visited_cells.end()) {
                    continue;
                }

                Voxel v = world.get_voxel(x, y, z);
                if (v.material_id == MAT_AIR) {
                    // Check if adjacent to solid rock (boundary contour voxel)
                    bool near_solid = (world.get_voxel(x + 1, y, z).is_solid() ||
                                       world.get_voxel(x - 1, y, z).is_solid() ||
                                       world.get_voxel(x, y + 1, z).is_solid() ||
                                       world.get_voxel(x, y - 1, z).is_solid() ||
                                       world.get_voxel(x, y, z + 1).is_solid() ||
                                       world.get_voxel(x, y, z - 1).is_solid());
                    if (near_solid) {
                        glm::vec3 p(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                        add_wire_box(lines, p, p + glm::vec3(1.8f), wire_color);
                    }
                }
            }
        }
    }

    // ── 3D HUD Markers inside Hologram ──

    // 1. Player Position: Amber directional cone oriented to look yaw
    glm::vec4 amber_color(1.0f, 0.67f, 0.09f, 0.95f);
    glm::vec3 player_look(std::cos(player.yaw()), 0.0f, std::sin(player.yaw()));
    add_directional_cone(lines, player_pos + glm::vec3(0.0f, 1.2f, 0.0f), player_look, 1.8f, 0.75f, amber_color);

    // 2. Extraction Beacon / Pod: Pulsing green anchor marker
    float green_pulse = 0.75f + 0.25f * std::sin(total_time * 6.0f);
    glm::vec4 green_color(0.15f, 1.0f, 0.35f, green_pulse);
    add_anchor_marker(lines, extraction.beacon_position() + glm::vec3(0.0f, 2.5f, 0.0f), 2.2f, green_color);

    // 3. Precursor Vault: Pulsing cyan lock icon
    if (mission.vault().exists && !mission.is_relic_retrieved()) {
        float cyan_pulse = 0.80f + 0.20f * std::sin(total_time * 4.5f);
        glm::vec4 cyan_lock(0.0f, 0.95f, 1.0f, cyan_pulse);
        add_lock_icon(lines, glm::vec3(mission.vault().door_pos) + glm::vec3(0.5f, 2.0f, 0.5f), 2.0f, cyan_lock);
    }

    // 4. Visited Shafts: Dotted luminescent breadcrumb trail
    glm::vec4 crumb_color(0.1f, 0.90f, 1.0f, 0.85f);
    const auto& crumbs = player.breadcrumbs();
    for (size_t i = 0; i < crumbs.size(); ++i) {
        glm::vec3 cp = crumbs[i] + glm::vec3(0.0f, 0.5f, 0.0f);
        // Small 3D cross
        lines.push_back({cp - glm::vec3(0.35f, 0.0f, 0.0f), crumb_color});
        lines.push_back({cp + glm::vec3(0.35f, 0.0f, 0.0f), crumb_color});
        lines.push_back({cp - glm::vec3(0.0f, 0.0f, 0.35f), crumb_color});
        lines.push_back({cp + glm::vec3(0.0f, 0.0f, 0.35f), crumb_color});

        if (i + 1 < crumbs.size()) {
            glm::vec3 next_cp = crumbs[i + 1] + glm::vec3(0.0f, 0.5f, 0.0f);
            lines.push_back({cp, crumb_color * 0.6f});
            lines.push_back({next_cp, crumb_color * 0.6f});
        }
    }

    m_vertex_count = static_cast<int>(lines.size());
    if (m_vertex_count > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(ScannerLineVertex), lines.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void TerrainScanner::render(int screen_width, int screen_height, const glm::vec3& player_pos, float /*player_yaw*/) {
    if (m_vao == 0) {
        init_buffers();
    }
    if (m_fold_progress <= 0.001f || m_vertex_count == 0 || m_shader_prog == 0) {
        return;
    }

    // Holographic Projection from suit left gauntlet rig
    glm::vec3 gauntlet_pos = player_pos + glm::vec3(-0.4f, 0.7f, 0.3f);

    float rad_yaw = glm::radians(m_orbit_yaw);
    float rad_pitch = glm::radians(m_orbit_pitch);
    float r = m_orbit_distance;

    glm::vec3 cam_offset(
        r * std::cos(rad_pitch) * std::sin(rad_yaw),
        r * std::sin(rad_pitch),
        r * std::cos(rad_pitch) * std::cos(rad_yaw)
    );
    glm::vec3 eye = player_pos + cam_offset;

    glm::mat4 view = glm::lookAt(eye, player_pos, glm::vec3(0.0f, 1.0f, 0.0f));
    float aspect = (screen_height > 0) ? (static_cast<float>(screen_width) / static_cast<float>(screen_height)) : 1.777f;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.5f, 250.0f);
    glm::mat4 mvp = proj * view;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive alpha blending for glowing hologram look
    glDisable(GL_DEPTH_TEST);          // Depth testing disabled so cave interior is visible

    glUseProgram(m_shader_prog);
    glUniformMatrix4fv(glGetUniformLocation(m_shader_prog, "uMVP"), 1, GL_FALSE, &mvp[0][0]);
    glUniform1f(glGetUniformLocation(m_shader_prog, "uAlpha"), 0.95f);
    glUniform3fv(glGetUniformLocation(m_shader_prog, "uGauntletAnchor"), 1, &gauntlet_pos[0]);
    glUniform1f(glGetUniformLocation(m_shader_prog, "uFoldProgress"), m_fold_progress);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, m_vertex_count);
    glBindVertexArray(0);

    // Restore standard UI blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

} // namespace Voidfall
