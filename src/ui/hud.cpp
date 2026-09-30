#include "hud.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include "../include/font8x8.h"
#include <vector>
#include <algorithm>

namespace Voidfall {

HUD::HUD(int screen_width, int height)
    : m_width(screen_width)
    , m_height(height)
{
    m_ui_shader.load_graphics("assets/shaders/ui.vert", "assets/shaders/ui.frag");
    m_text_shader.load_graphics("assets/shaders/text.vert", "assets/shaders/text.frag");

    init_gl();
    init_font_atlas();
}

HUD::~HUD() {
    if (m_rect_vao != 0) glDeleteVertexArrays(1, &m_rect_vao);
    if (m_rect_vbo != 0) glDeleteBuffers(1, &m_rect_vbo);
    if (m_text_vao != 0) glDeleteVertexArrays(1, &m_text_vao);
    if (m_text_vbo != 0) glDeleteBuffers(1, &m_text_vbo);
    if (m_font_tex != 0) glDeleteTextures(1, &m_font_tex);
}

void HUD::init_gl() {
    // 1. Rect Quad VAO
    float unit_quad[] = {
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,

        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_rect_vao);
    glGenBuffers(1, &m_rect_vbo);
    glBindVertexArray(m_rect_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_rect_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(unit_quad), unit_quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);

    // 2. Dynamic Text VAO
    glGenVertexArrays(1, &m_text_vao);
    glGenBuffers(1, &m_text_vbo);
    glBindVertexArray(m_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_text_vbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float) * 512, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

void HUD::init_font_atlas() {
    // 128 characters arranged horizontally: 128 * 8 = 1024 width, 8 height
    const int atlas_w = 128 * 8;
    const int atlas_h = 8;
    std::vector<uint8_t> atlas(atlas_w * atlas_h, 0);

    for (int c = 0; c < 128; ++c) {
        for (int y = 0; y < 8; ++y) {
            uint8_t row = font8x8_basic[c][y];
            for (int x = 0; x < 8; ++x) {
                bool bit = (row & (1 << x)) != 0;
                int px = c * 8 + x;
                int py = y;
                atlas[py * atlas_w + px] = bit ? 255 : 0;
            }
        }
    }

    glGenTextures(1, &m_font_tex);
    glBindTexture(GL_TEXTURE_2D, m_font_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas_w, atlas_h, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void HUD::resize(int width, int height) {
    m_width = width;
    m_height = height;
}

void HUD::draw_rect(float x, float y, float w, float h, const glm::vec4& color) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);
    m_ui_shader.use();
    m_ui_shader.set_mat4("uProjection", proj * model);
    m_ui_shader.set_vec4("uColor", color);

    glBindVertexArray(m_rect_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void HUD::draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color) {
    if (text.empty()) return;

    std::vector<float> vertices;
    vertices.reserve(text.size() * 24);

    float cur_x = x;
    float cur_y = y;
    float char_w = 8.0f * scale;
    float char_h = 8.0f * scale;

    const float atlas_w = 128.0f * 8.0f;
    const float atlas_h = 8.0f;

    for (char ch : text) {
        if (ch == '\n') {
            cur_y += char_h + 4.0f * scale;
            cur_x = x;
            continue;
        }

        uint8_t c = static_cast<uint8_t>(ch);
        if (c >= 128) c = '?';

        float u0 = (c * 8.0f) / atlas_w;
        float u1 = (c * 8.0f + 8.0f) / atlas_w;
        float v0 = 0.0f;
        float v1 = 1.0f;

        float x0 = cur_x;
        float x1 = cur_x + char_w;
        float y0 = cur_y;
        float y1 = cur_y + char_h;

        // Quad 2 triangles (6 vertices)
        // v0: (x0, y1, u0, v1)
        // v1: (x0, y0, u0, v0)
        // v2: (x1, y0, u1, v0)
        // v3: (x0, y1, u0, v1)
        // v4: (x1, y0, u1, v0)
        // v5: (x1, y1, u1, v1)
        float quad[24] = {
            x0, y1, u0, v1,
            x0, y0, u0, v0,
            x1, y0, u1, v0,

            x0, y1, u0, v1,
            x1, y0, u1, v0,
            x1, y1, u1, v1
        };

        vertices.insert(vertices.end(), quad, quad + 24);
        cur_x += char_w;
    }

    if (vertices.empty()) return;

    m_text_shader.use();
    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);
    m_text_shader.set_mat4("uProjection", proj);
    m_text_shader.set_vec4("uTextColor", color);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_font_tex);
    m_text_shader.set_int("uFontTexture", 0);

    glBindVertexArray(m_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_text_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));
    glBindVertexArray(0);
}

void HUD::render(
    const PlayerController& player,
    const HazardClock& hazard,
    const ExtractionSystem& extraction
) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float cx = static_cast<float>(m_width) / 2.0f;
    float cy = static_cast<float>(m_height) / 2.0f;

    // 1. Center Crosshair
    draw_rect(cx - 1.0f, cy - 8.0f, 2.0f, 16.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.75f));
    draw_rect(cx - 8.0f, cy - 1.0f, 16.0f, 2.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.75f));

    // 2. MISSION DIRECTIVES & OBJECTIVES PANEL (Top-Left)
    {
        float p_x = 25.0f;
        float p_y = 25.0f;
        float p_w = 460.0f;
        float p_h = 160.0f;

        // Tactical glassmorphism background
        draw_rect(p_x, p_y, p_w, p_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.85f));
        // Accent border
        draw_rect(p_x, p_y, 4.0f, p_h, glm::vec4(0.15f, 0.85f, 1.0f, 0.95f));
        draw_rect(p_x, p_y, p_w, 2.0f, glm::vec4(0.15f, 0.85f, 1.0f, 0.5f));

        draw_text("VOIDFALL: DREDGE // MISSION DIRECTIVES", p_x + 14.0f, p_y + 12.0f, 1.6f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
        draw_rect(p_x + 14.0f, p_y + 30.0f, p_w - 28.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.6f));

        // Objective 1: Mining
        bool obj1_done = m_stats.crystals_mined >= m_stats.target_crystals;
        glm::vec4 obj1_col = obj1_done ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        std::string obj1_str = std::string(obj1_done ? "[OK] " : "[ ] ") +
            "MINE VOIDITE CRYSTALS (" + std::to_string(m_stats.crystals_mined) + "/" +
            std::to_string(m_stats.target_crystals) + ") [L-CLICK to Drill]";
        draw_text(obj1_str, p_x + 14.0f, p_y + 38.0f, 1.35f, obj1_col);

        // Objective 2: Surveying
        bool obj2_done = m_stats.sonar_scans_performed >= 3;
        glm::vec4 obj2_col = obj2_done ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        std::string obj2_str = std::string(obj2_done ? "[OK] " : "[ ] ") +
            "SURVEY CAVERN VEINS (" + std::to_string(m_stats.sonar_scans_performed) + "/3) [Press Q]";
        draw_text(obj2_str, p_x + 14.0f, p_y + 58.0f, 1.35f, obj2_col);

        // Objective 3: Extraction Beacon
        bool obj3_active = (extraction.phase() != ExtractionPhase::Dormant);
        glm::vec4 obj3_col = obj3_active ? glm::vec4(1.0f, 0.65f, 0.15f, 1.0f) : glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        std::string obj3_str = obj3_active ?
            "[!] BEACON DEFENSE ACTIVE (Hold Area)" :
            "[ ] DEPLOY EXTRACTION BEACON [Press B when ready]";
        draw_text(obj3_str, p_x + 14.0f, p_y + 78.0f, 1.35f, obj3_col);

        // Warning text
        draw_text("HAZARD: Escalate radiation triggers structural cave-ins!", p_x + 14.0f, p_y + 104.0f, 1.25f, glm::vec4(1.0f, 0.45f, 0.2f, 0.9f));
        draw_text("TREMOR: Disconnected ceilings collapse into falling debris", p_x + 14.0f, p_y + 124.0f, 1.2f, glm::vec4(0.7f, 0.75f, 0.8f, 0.8f));
        draw_text("JETPACK: Hold SPACE in air | GRAPPLE: F to Anchor, E Reel", p_x + 14.0f, p_y + 140.0f, 1.2f, glm::vec4(0.3f, 0.85f, 0.95f, 0.9f));
    }

    // 3. TACTICAL QUICK CONTROLS BAR (Bottom-Center)
    {
        float bar_w = 780.0f;
        float bar_h = 32.0f;
        float bar_x = cx - bar_w / 2.0f;
        float bar_y = static_cast<float>(m_height) - 44.0f;

        draw_rect(bar_x, bar_y, bar_w, bar_h, glm::vec4(0.04f, 0.05f, 0.08f, 0.85f));
        draw_rect(bar_x, bar_y, bar_w, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.6f));

        std::string ctrl_str = "[W,A,S,D] Move  [SPACE] Jetpack  [L-CLK] Drill  [R-CLK] Build  [F] Grapple  [E] Reel  [Q] Sonar  [B] Beacon  [H] Light";
        draw_text(ctrl_str, bar_x + 10.0f, bar_y + 10.0f, 1.25f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    }

    // 4. Exo-Suit Status & Telemetry Gauges (Bottom-Left)
    {
        float base_x = 25.0f;
        float base_y = static_cast<float>(m_height) - 130.0f;

        // Background panel
        draw_rect(base_x, base_y, 250.0f, 75.0f, glm::vec4(0.04f, 0.06f, 0.09f, 0.85f));
        draw_rect(base_x, base_y, 3.0f, 75.0f, glm::vec4(0.1f, 0.8f, 0.4f, 0.9f));

        const auto& exo = player.exo();

        // Integrity Bar
        draw_text("SUIT INTEGRITY", base_x + 10.0f, base_y + 8.0f, 1.15f, glm::vec4(0.2f, 0.9f, 0.4f, 1.0f));
        draw_rect(base_x + 10.0f, base_y + 20.0f, 230.0f, 8.0f, glm::vec4(0.15f, 0.20f, 0.15f, 0.8f));
        draw_rect(base_x + 10.0f, base_y + 20.0f, 230.0f * (exo.integrity / 100.0f), 8.0f, glm::vec4(0.2f, 0.85f, 0.3f, 0.95f));

        // Power Bar
        draw_text("EXO POWER CELL", base_x + 10.0f, base_y + 30.0f, 1.15f, glm::vec4(0.1f, 0.8f, 1.0f, 1.0f));
        draw_rect(base_x + 10.0f, base_y + 42.0f, 230.0f, 8.0f, glm::vec4(0.10f, 0.20f, 0.25f, 0.8f));
        draw_rect(base_x + 10.0f, base_y + 42.0f, 230.0f * (exo.power / 100.0f), 8.0f, glm::vec4(0.1f, 0.75f, 0.95f, 0.95f));

        // Heat Bar
        draw_text(exo.overheated ? "THERMAL OVERHEAT!" : "THERMAL DISSIPATION", base_x + 10.0f, base_y + 52.0f, 1.15f,
                  exo.overheated ? glm::vec4(1.0f, 0.2f, 0.2f, 1.0f) : glm::vec4(1.0f, 0.6f, 0.2f, 1.0f));
        draw_rect(base_x + 10.0f, base_y + 64.0f, 230.0f, 8.0f, glm::vec4(0.25f, 0.15f, 0.10f, 0.8f));
        glm::vec4 heat_color = exo.overheated ? glm::vec4(1.0f, 0.15f, 0.15f, 1.0f) : glm::vec4(1.0f, 0.55f, 0.1f, 0.95f);
        draw_rect(base_x + 10.0f, base_y + 64.0f, 230.0f * (exo.heat / 100.0f), 8.0f, heat_color);
    }

    // 5. Radiation Hazard Meter (Top-Right)
    {
        float rad_x = static_cast<float>(m_width) - 275.0f;
        float rad_y = 25.0f;
        draw_rect(rad_x, rad_y, 250.0f, 60.0f, glm::vec4(0.08f, 0.04f, 0.04f, 0.85f));
        draw_rect(rad_x + 247.0f, rad_y, 3.0f, 60.0f, glm::vec4(1.0f, 0.8f, 0.1f, 0.9f));

        std::string rad_str = "VOID RADIATION: " + std::to_string(static_cast<int>(hazard.radiation_level())) + "%";
        draw_text(rad_str, rad_x + 10.0f, rad_y + 8.0f, 1.25f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
        draw_rect(rad_x + 10.0f, rad_y + 22.0f, 230.0f, 10.0f, glm::vec4(0.25f, 0.20f, 0.10f, 0.8f));
        draw_rect(rad_x + 10.0f, rad_y + 22.0f, 230.0f * (hazard.radiation_level() / 100.0f), 10.0f, glm::vec4(0.95f, 0.85f, 0.15f, 0.95f));

        std::string trem_str = "NEXT TREMOR: " + std::to_string(static_cast<int>(hazard.tremor_timer())) + "s";
        draw_text(trem_str, rad_x + 10.0f, rad_y + 38.0f, 1.2f, glm::vec4(0.85f, 0.4f, 0.2f, 0.9f));
    }

    // 6. Extraction Beacon Banner (Top-Center)
    if (extraction.phase() == ExtractionPhase::BeaconDeployed) {
        float ex_w = 420.0f;
        float ex_h = 44.0f;
        float ex_x = cx - ex_w / 2.0f;
        float ex_y = 20.0f;

        float pulse = extraction.siren_pulse();
        glm::vec4 banner_color = glm::mix(glm::vec4(0.85f, 0.15f, 0.1f, 0.9f), glm::vec4(1.0f, 0.4f, 0.1f, 1.0f), pulse);
        draw_rect(ex_x, ex_y, ex_w, ex_h, glm::vec4(0.12f, 0.02f, 0.02f, 0.9f));
        draw_rect(ex_x, ex_y, ex_w * (extraction.countdown() / 90.0f), ex_h, banner_color);

        std::string evac_str = "EXTRACTION DEFENSE IN PROGRESS: " + std::to_string(static_cast<int>(extraction.countdown())) + "s";
        draw_text(evac_str, ex_x + 18.0f, ex_y + 14.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else if (extraction.phase() == ExtractionPhase::PodLanded) {
        float ex_w = 420.0f;
        float ex_h = 44.0f;
        float ex_x = cx - ex_w / 2.0f;
        float ex_y = 20.0f;
        draw_rect(ex_x, ex_y, ex_w, ex_h, glm::vec4(0.1f, 0.85f, 0.3f, 0.9f));
        draw_text("DROP POD HAS LANDED! BOARD POD NOW!", ex_x + 18.0f, ex_y + 14.0f, 1.4f, glm::vec4(0.05f, 0.1f, 0.05f, 1.0f));
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

} // namespace Voidfall
