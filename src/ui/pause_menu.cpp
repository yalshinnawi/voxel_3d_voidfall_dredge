#include "pause_menu.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include "../include/font8x8.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Voidfall {

PauseMenu::PauseMenu(int screen_width, int screen_height)
    : m_width(screen_width)
    , m_height(screen_height)
{
    m_ui_shader.load_graphics("assets/shaders/ui.vert", "assets/shaders/ui.frag");
    m_text_shader.load_graphics("assets/shaders/text.vert", "assets/shaders/text.frag");

    init_gl();
    init_font_atlas();
}

PauseMenu::~PauseMenu() {
    if (m_rect_vao != 0) glDeleteVertexArrays(1, &m_rect_vao);
    if (m_rect_vbo != 0) glDeleteBuffers(1, &m_rect_vbo);
    if (m_text_vao != 0) glDeleteVertexArrays(1, &m_text_vao);
    if (m_text_vbo != 0) glDeleteBuffers(1, &m_text_vbo);
    if (m_font_tex != 0) glDeleteTextures(1, &m_font_tex);
}

void PauseMenu::init_gl() {
    float unit_quad[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,

        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };

    glGenVertexArrays(1, &m_rect_vao);
    glGenBuffers(1, &m_rect_vbo);
    glBindVertexArray(m_rect_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_rect_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(unit_quad), unit_quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);

    glGenVertexArrays(1, &m_text_vao);
    glGenBuffers(1, &m_text_vbo);
    glBindVertexArray(m_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_text_vbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float) * 2048, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

void PauseMenu::init_font_atlas() {
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

void PauseMenu::resize(int width, int height) {
    if (width > 0 && height > 0) {
        m_width = width;
        m_height = height;
    }
}

void PauseMenu::draw_rect(float x, float y, float w, float h, const glm::vec4& color) {
    m_ui_shader.use();
    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);
    m_ui_shader.set_mat4("uProjection", proj);
    m_ui_shader.set_vec4("uRect", glm::vec4(x, y, w, h));
    m_ui_shader.set_vec4("uColor", color);

    glBindVertexArray(m_rect_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void PauseMenu::draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color) {
    if (text.empty()) return;

    m_text_shader.use();
    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);
    m_text_shader.set_mat4("uProjection", proj);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_font_tex);
    m_text_shader.set_int("uFontTexture", 0);

    glBindVertexArray(m_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_text_vbo);

    auto render_pass = [&](float ox, float oy, const glm::vec4& col) {
        m_text_shader.set_vec4("uColor", col);
        std::vector<float> vertices;
        vertices.reserve(text.length() * 24);

        float cur_x = ox;
        float cur_y = oy;
        float char_w = 8.0f * scale;
        float char_h = 8.0f * scale;

        for (char c : text) {
            if (c == '\n') {
                cur_x = ox;
                cur_y += char_h + 4.0f * scale;
                continue;
            }
            uint8_t uc = static_cast<uint8_t>(c);
            if (uc >= 128) uc = '?';

            float u0 = static_cast<float>(uc * 8) / (128.0f * 8.0f);
            float u1 = static_cast<float>((uc + 1) * 8) / (128.0f * 8.0f);
            float v0 = 0.0f;
            float v1 = 1.0f;

            float x0 = cur_x;
            float y0 = cur_y;
            float x1 = cur_x + char_w;
            float y1 = cur_y + char_h;

            float quad_verts[] = {
                x0, y0, u0, v0,
                x0, y1, u0, v1,
                x1, y1, u1, v1,

                x0, y0, u0, v0,
                x1, y1, u1, v1,
                x1, y0, u1, v0
            };
            vertices.insert(vertices.end(), quad_verts, quad_verts + 24);
            cur_x += char_w;
        }

        if (!vertices.empty()) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));
        }
    };

    // 1. Drop shadow
    render_pass(x + 1.0f, y + 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, color.a * 0.9f));
    // 2. Main text glyph
    render_pass(x, y, color);

    glBindVertexArray(0);
}

PauseMenuAction PauseMenu::render(
    int sector,
    float time_elapsed,
    const PlayerInventory& inventory,
    GameSettings& settings,
    float mouse_x,
    float mouse_y,
    bool mouse_clicked
) {
    PauseMenuAction action = PauseMenuAction::None;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

    // 1. Dark tinted modal backdrop over frozen game frame
    draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.015f, 0.022f, 0.035f, 0.88f));

    // 2. Central Modal Dialog Window
    float panel_w = std::min(w * 0.72f, 760.0f);
    float panel_h = 600.0f;
    float panel_x = (w - panel_w) * 0.5f;
    float panel_y = (h - panel_h) * 0.5f;

    // Window background & border
    draw_rect(panel_x, panel_y, panel_w, panel_h, glm::vec4(0.04f, 0.07f, 0.11f, 0.96f));
    draw_rect(panel_x, panel_y, panel_w, 3.0f, glm::vec4(0.0f, 0.94f, 1.0f, 0.9f));
    draw_rect(panel_x, panel_y, 3.0f, panel_h, glm::vec4(0.2f, 0.4f, 0.55f, 0.5f));
    draw_rect(panel_x + panel_w - 3.0f, panel_y, 3.0f, panel_h, glm::vec4(0.2f, 0.4f, 0.55f, 0.5f));
    draw_rect(panel_x, panel_y + panel_h - 3.0f, panel_w, 3.0f, glm::vec4(0.2f, 0.4f, 0.55f, 0.5f));

    // Header Title
    std::string header = "EXPEDITION PAUSED -- SECTOR " + std::to_string(sector);
    draw_text(header, panel_x + 28.0f, panel_y + 22.0f, 2.2f, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    draw_text("TACTICAL DELVER TELEMETRY // OPERATIONS SUSPENDED", panel_x + 28.0f, panel_y + 48.0f, 1.25f, glm::vec4(0.6f, 0.75f, 0.85f, 0.85f));
    draw_rect(panel_x + 28.0f, panel_y + 68.0f, panel_w - 56.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.55f, 0.6f));

    // 3. Current Run Statistics Box
    float stats_y = panel_y + 80.0f;
    float stats_w = panel_w - 56.0f;
    float stats_h = 135.0f;
    draw_rect(panel_x + 28.0f, stats_y, stats_w, stats_h, glm::vec4(0.02f, 0.035f, 0.06f, 0.9f));
    draw_rect(panel_x + 28.0f, stats_y, stats_w, 1.0f, glm::vec4(0.15f, 0.35f, 0.5f, 0.5f));

    int mins = static_cast<int>(time_elapsed) / 60;
    int secs = static_cast<int>(time_elapsed) % 60;
    char time_buf[32];
    std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d", mins, secs);

    draw_text("MISSION TELEMETRY & RESOURCES", panel_x + 42.0f, stats_y + 12.0f, 1.35f, glm::vec4(1.0f, 0.70f, 0.0f, 1.0f));
    draw_text("Time In Cavern:    " + std::string(time_buf), panel_x + 42.0f, stats_y + 36.0f, 1.3f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    draw_text("Voidite Extracted: " + std::to_string(inventory.voidite) + " / " + std::to_string(inventory.target_voidite) + " CRYSTALS", panel_x + 42.0f, stats_y + 58.0f, 1.3f, glm::vec4(0.0f, 0.94f, 1.0f, 0.95f));
    draw_text("Scrap Metal: " + std::to_string(inventory.scrap_metal) + "  |  Titanium Cores: " + std::to_string(inventory.titanium) + "  |  Salvage: " + std::to_string(inventory.salvage_parts), panel_x + 42.0f, stats_y + 80.0f, 1.3f, glm::vec4(1.0f, 0.70f, 0.0f, 0.9f));

    // Completion Rate Progress Bar
    int est_rate = inventory.calculate_completion_rate(false);
    draw_text("ESTIMATED COMPLETION RATE: " + std::to_string(est_rate) + "%", panel_x + 42.0f, stats_y + 104.0f, 1.35f, glm::vec4(0.2f, 1.0f, 0.4f, 1.0f));

    float bar_x = panel_x + 360.0f;
    float bar_y = stats_y + 104.0f;
    float bar_w = stats_w - 350.0f;
    float bar_h = 12.0f;
    draw_rect(bar_x, bar_y, bar_w, bar_h, glm::vec4(0.08f, 0.12f, 0.16f, 1.0f));
    draw_rect(bar_x, bar_y, bar_w * (static_cast<float>(est_rate) / 100.0f), bar_h, glm::vec4(0.2f, 0.9f, 0.4f, 1.0f));

    // 4. Interactive Settings Controls
    float set_y = stats_y + stats_h + 16.0f;
    float set_w = stats_w;
    float set_h = 115.0f;
    draw_rect(panel_x + 28.0f, set_y, set_w, set_h, glm::vec4(0.02f, 0.035f, 0.06f, 0.9f));
    draw_rect(panel_x + 28.0f, set_y, set_w, 1.0f, glm::vec4(0.15f, 0.35f, 0.5f, 0.5f));

    draw_text("TACTICAL RIG SETTINGS", panel_x + 42.0f, set_y + 10.0f, 1.35f, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));

    // Sensitivity Row
    char sens_buf[32];
    std::snprintf(sens_buf, sizeof(sens_buf), "MOUSE SENSITIVITY: %.2f", settings.mouse_sensitivity);
    draw_text(sens_buf, panel_x + 42.0f, set_y + 36.0f, 1.3f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));

    float s_btn1_x = panel_x + 380.0f;
    float s_btn2_x = panel_x + 440.0f;
    bool s_hover1 = (mouse_x >= s_btn1_x && mouse_x <= s_btn1_x + 45.0f && mouse_y >= set_y + 32.0f && mouse_y <= set_y + 52.0f);
    bool s_hover2 = (mouse_x >= s_btn2_x && mouse_x <= s_btn2_x + 45.0f && mouse_y >= set_y + 32.0f && mouse_y <= set_y + 52.0f);
    draw_rect(s_btn1_x, set_y + 32.0f, 45.0f, 20.0f, s_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f));
    draw_rect(s_btn2_x, set_y + 32.0f, 45.0f, 20.0f, s_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f));
    draw_text(" [ - ]", s_btn1_x, set_y + 34.0f, 1.3f, glm::vec4(1.0f));
    draw_text(" [ + ]", s_btn2_x, set_y + 34.0f, 1.3f, glm::vec4(1.0f));
    if (mouse_clicked && s_hover1) settings.mouse_sensitivity = std::max(0.04f, settings.mouse_sensitivity - 0.02f);
    if (mouse_clicked && s_hover2) settings.mouse_sensitivity = std::min(0.40f, settings.mouse_sensitivity + 0.02f);

    // FOV Row
    char fov_buf[32];
    std::snprintf(fov_buf, sizeof(fov_buf), "FIELD OF VIEW:     %d DEG", static_cast<int>(settings.fov));
    draw_text(fov_buf, panel_x + 42.0f, set_y + 62.0f, 1.3f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    bool f_hover1 = (mouse_x >= s_btn1_x && mouse_x <= s_btn1_x + 45.0f && mouse_y >= set_y + 58.0f && mouse_y <= set_y + 78.0f);
    bool f_hover2 = (mouse_x >= s_btn2_x && mouse_x <= s_btn2_x + 45.0f && mouse_y >= set_y + 58.0f && mouse_y <= set_y + 78.0f);
    draw_rect(s_btn1_x, set_y + 58.0f, 45.0f, 20.0f, f_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f));
    draw_rect(s_btn2_x, set_y + 58.0f, 45.0f, 20.0f, f_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f));
    draw_text(" [ - ]", s_btn1_x, set_y + 60.0f, 1.3f, glm::vec4(1.0f));
    draw_text(" [ + ]", s_btn2_x, set_y + 60.0f, 1.3f, glm::vec4(1.0f));
    if (mouse_clicked && f_hover1) settings.fov = std::max(60.0f, settings.fov - 5.0f);
    if (mouse_clicked && f_hover2) settings.fov = std::min(105.0f, settings.fov + 5.0f);

    // Audio Row
    char vol_buf[32];
    std::snprintf(vol_buf, sizeof(vol_buf), "AUDIO SYNTHESIZER: %d%%", static_cast<int>(settings.master_volume * 100.0f));
    draw_text(vol_buf, panel_x + 42.0f, set_y + 88.0f, 1.3f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    bool v_hover1 = (mouse_x >= s_btn1_x && mouse_x <= s_btn1_x + 45.0f && mouse_y >= set_y + 84.0f && mouse_y <= set_y + 104.0f);
    bool v_hover2 = (mouse_x >= s_btn2_x && mouse_x <= s_btn2_x + 45.0f && mouse_y >= set_y + 84.0f && mouse_y <= set_y + 104.0f);
    draw_rect(s_btn1_x, set_y + 84.0f, 45.0f, 20.0f, v_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f));
    draw_rect(s_btn2_x, set_y + 84.0f, 45.0f, 20.0f, v_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f));
    draw_text(" [ - ]", s_btn1_x, set_y + 86.0f, 1.3f, glm::vec4(1.0f));
    draw_text(" [ + ]", s_btn2_x, set_y + 86.0f, 1.3f, glm::vec4(1.0f));
    if (mouse_clicked && v_hover1) settings.master_volume = std::max(0.0f, settings.master_volume - 0.25f);
    if (mouse_clicked && v_hover2) settings.master_volume = std::min(1.0f, settings.master_volume + 0.25f);

    // 5. Interactive Navigation Buttons
    float btn_w = panel_w - 56.0f;
    float btn_h = 42.0f;

    // [RESUME EXPEDITION]
    float res_y = set_y + set_h + 18.0f;
    bool res_hover = (mouse_x >= panel_x + 28.0f && mouse_x <= panel_x + 28.0f + btn_w &&
                      mouse_y >= res_y && mouse_y <= res_y + btn_h);
    draw_rect(panel_x + 28.0f, res_y, btn_w, btn_h, res_hover ? glm::vec4(0.05f, 0.35f, 0.45f, 0.95f) : glm::vec4(0.04f, 0.18f, 0.26f, 0.9f));
    draw_rect(panel_x + 28.0f, res_y, 4.0f, btn_h, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    draw_text(">> [RESUME EXPEDITION] (ESC)", panel_x + 48.0f, res_y + 13.0f, 1.55f, res_hover ? glm::vec4(1.0f) : glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    if (res_hover && mouse_clicked) {
        action = PauseMenuAction::Resume;
    }

    // [ABANDON RUN] (Red warning)
    float abn_y = res_y + btn_h + 10.0f;
    bool abn_hover = (mouse_x >= panel_x + 28.0f && mouse_x <= panel_x + 28.0f + btn_w &&
                      mouse_y >= abn_y && mouse_y <= abn_y + btn_h);
    draw_rect(panel_x + 28.0f, abn_y, btn_w, btn_h, abn_hover ? glm::vec4(0.45f, 0.08f, 0.08f, 0.95f) : glm::vec4(0.25f, 0.04f, 0.04f, 0.85f));
    draw_rect(panel_x + 28.0f, abn_y, 4.0f, btn_h, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
    draw_text(">> [ABANDON RUN] -- 50% SALVAGE PENALTY", panel_x + 48.0f, abn_y + 13.0f, 1.5f, abn_hover ? glm::vec4(1.0f) : glm::vec4(1.0f, 0.35f, 0.35f, 1.0f));
    if (abn_hover && mouse_clicked) {
        action = PauseMenuAction::Abandon;
    }

    // [RETURN TO ORBITAL HUB]
    float hub_y = abn_y + btn_h + 10.0f;
    bool hub_hover = (mouse_x >= panel_x + 28.0f && mouse_x <= panel_x + 28.0f + btn_w &&
                      mouse_y >= hub_y && mouse_y <= hub_y + btn_h);
    draw_rect(panel_x + 28.0f, hub_y, btn_w, btn_h, hub_hover ? glm::vec4(0.12f, 0.18f, 0.24f, 0.95f) : glm::vec4(0.06f, 0.09f, 0.13f, 0.85f));
    draw_rect(panel_x + 28.0f, hub_y, 4.0f, btn_h, glm::vec4(0.4f, 0.6f, 0.75f, 0.8f));
    draw_text(">> [RETURN TO ORBITAL HUB]", panel_x + 48.0f, hub_y + 13.0f, 1.5f, hub_hover ? glm::vec4(1.0f) : glm::vec4(0.7f, 0.8f, 0.9f, 0.9f));
    if (hub_hover && mouse_clicked) {
        action = PauseMenuAction::ReturnToHub;
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    return action;
}

} // namespace Voidfall
