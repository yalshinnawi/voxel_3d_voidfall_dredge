#include "orbital_hub.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include "../include/font8x8.h"
#include <vector>
#include <algorithm>

namespace Voidfall {

OrbitalHubUI::OrbitalHubUI(int screen_width, int screen_height)
    : m_width(screen_width)
    , m_height(screen_height)
{
    m_ui_shader.load_graphics("assets/shaders/ui.vert", "assets/shaders/ui.frag");
    m_text_shader.load_graphics("assets/shaders/text.vert", "assets/shaders/text.frag");

    init_gl();
    init_font_atlas();
}

OrbitalHubUI::~OrbitalHubUI() {
    if (m_rect_vao != 0) glDeleteVertexArrays(1, &m_rect_vao);
    if (m_rect_vbo != 0) glDeleteBuffers(1, &m_rect_vbo);
    if (m_text_vao != 0) glDeleteVertexArrays(1, &m_text_vao);
    if (m_text_vbo != 0) glDeleteBuffers(1, &m_text_vbo);
    if (m_font_tex != 0) glDeleteTextures(1, &m_font_tex);
}

void OrbitalHubUI::init_gl() {
    // 2 triangles forming a unit quad [0, 1]^2 with CCW winding in screen ortho
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

void OrbitalHubUI::init_font_atlas() {
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

void OrbitalHubUI::resize(int width, int height) {
    m_width = width;
    m_height = height;
}

void OrbitalHubUI::draw_rect(float x, float y, float w, float h, const glm::vec4& color) {
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

void OrbitalHubUI::draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color) {
    if (text.empty()) return;

    std::vector<float> vertices;
    vertices.reserve(text.size() * 24);

    float cur_x = x;
    float cur_y = y;
    float char_w = 8.0f * scale;
    float char_h = 8.0f * scale;

    const float atlas_w = 128.0f * 8.0f;

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

        // Quad with CCW winding in top-left screen ortho:
        // (x0, y0), (x0, y1), (x1, y1)
        // (x0, y0), (x1, y1), (x1, y0)
        float quad[24] = {
            x0, y0, u0, v0,
            x0, y1, u0, v1,
            x1, y1, u1, v1,

            x0, y0, u0, v0,
            x1, y1, u1, v1,
            x1, y0, u1, v0
        };

        vertices.insert(vertices.end(), quad, quad + 24);
        cur_x += char_w;
    }

    if (vertices.empty()) return;

    m_text_shader.use();
    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_font_tex);
    m_text_shader.set_int("uFontTexture", 0);

    glBindVertexArray(m_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_text_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data());

    // 1. Draw 1-pixel dark drop shadow / outline quad behind text for high contrast
    glm::mat4 shadow_proj = glm::translate(proj, glm::vec3(1.2f, 1.2f, 0.0f));
    m_text_shader.set_mat4("uProjection", shadow_proj);
    m_text_shader.set_vec4("uTextColor", glm::vec4(0.0f, 0.0f, 0.0f, color.a * 0.95f));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));

    // 2. Draw high-visibility foreground text
    m_text_shader.set_mat4("uProjection", proj);
    m_text_shader.set_vec4("uTextColor", color);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));

    glBindVertexArray(0);
}

bool OrbitalHubUI::render_main_menu(int& selected_level, const PlayerInventory& inventory, float mouse_x, float mouse_y, bool mouse_clicked) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

    bool action_triggered = false;

    // Semi-transparent deep subterranean background
    draw_rect(0, 0, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.94f));

    // Decorative geometric grid lines
    for (float y = 50.0f; y < h; y += 120.0f) {
        draw_rect(0, y, w, 1.0f, glm::vec4(0.1f, 0.2f, 0.3f, 0.2f));
    }

    float cx = w / 2.0f;

    // Header banner
    float banner_y = 50.0f;
    draw_text("V O I D F A L L :   D R E D G E", cx - 340.0f, banner_y, 3.2f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    draw_text("SUBTERRANEAN VOXEL CAVERN EXTRACTION // INDUSTRIAL EXPEDITION PROTOCOL", cx - 330.0f, banner_y + 40.0f, 1.35f, glm::vec4(0.6f, 0.75f, 0.85f, 0.85f));

    draw_rect(cx - 380.0f, banner_y + 65.0f, 760.0f, 2.0f, glm::vec4(0.15f, 0.85f, 1.0f, 0.6f));

    // Responsive Sector Selection Cards: cardWidth = min(w * 0.75, 900)
    float card_w = std::min(w * 0.75f, 900.0f);
    float card_h = 95.0f;
    float card_x = (w - card_w) * 0.5f;
    float start_y = banner_y + 85.0f;

    struct LevelInfo {
        int id;
        std::string title;
        std::string desc;
        std::string obj;
    };

    LevelInfo levels[3] = {
        {
            1,
            "LEVEL 1 -- CRYSTALLINE CAVERNS",
            "Survey deep granite fissures. Mine precious Voidite veins and call extraction beacon.",
            "Objective: Extract 25 Voidite Crystals, deploy Beacon [B], and board evac pod."
        },
        {
            2,
            "LEVEL 2 -- SUBTERRANEAN VAULT",
            "Subterranean titanium bulkheads and blast doors buried in volcanic basalt.",
            "Objective: Locate vault via Sonar [Q], breach Reinforced Doors [3], retrieve Relic Hyper-Core."
        },
        {
            3,
            "LEVEL 3 -- FAULT-LINE COLLAPSE",
            "High-radiation unstable fault line with seismic tremors and rapid structural collapse.",
            "Objective: 3-Minute strict hazard clock! Mine 50 Voidite and evacuate before total collapse."
        }
    };

    for (int i = 0; i < 3; ++i) {
        float y = start_y + i * (card_h + 18.0f);

        // AABB Mouse hit detection
        bool is_hovered = (mouse_x >= card_x && mouse_x <= card_x + card_w &&
                           mouse_y >= y && mouse_y <= y + card_h);
        if (is_hovered) {
            selected_level = levels[i].id;
            if (mouse_clicked) {
                action_triggered = true; // Launch or select
            }
        }

        bool is_sel = (selected_level == levels[i].id);

        glm::vec4 bg_col = is_hovered ? glm::vec4(0.10f, 0.22f, 0.32f, 0.98f) :
                           is_sel     ? glm::vec4(0.08f, 0.16f, 0.25f, 0.95f) :
                                        glm::vec4(0.04f, 0.06f, 0.09f, 0.85f);
        draw_rect(card_x, y, card_w, card_h, bg_col);

        glm::vec4 border_col = is_hovered ? glm::vec4(0.4f, 1.0f, 1.0f, 1.0f) :
                               is_sel     ? glm::vec4(0.2f, 0.95f, 1.0f, 1.0f) :
                                            glm::vec4(0.2f, 0.35f, 0.45f, 0.5f);
        draw_rect(card_x, y, is_hovered ? 6.0f : 4.0f, card_h, border_col);
        draw_rect(card_x, y, card_w, is_hovered ? 2.0f : 1.0f, border_col);
        if (is_hovered) {
            draw_rect(card_x, y + card_h - 2.0f, card_w, 2.0f, border_col);
            draw_rect(card_x + card_w - 2.0f, y, 2.0f, card_h, border_col);
        }

        std::string selector = (is_hovered || is_sel) ? ">> " : "   ";
        draw_text(selector + levels[i].title, card_x + 16.0f, y + 14.0f, 1.6f, is_hovered ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : is_sel ? glm::vec4(0.2f, 0.95f, 1.0f, 1.0f) : glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
        draw_text(levels[i].desc, card_x + 36.0f, y + 40.0f, 1.25f, glm::vec4(0.7f, 0.75f, 0.8f, 0.85f));
        draw_text(levels[i].obj, card_x + 36.0f, y + 62.0f, 1.25f, is_sel ? glm::vec4(1.0f, 0.85f, 0.2f, 0.95f) : glm::vec4(0.5f, 0.75f, 0.55f, 0.8f));

        // Render Sector Progress Badge and Completion Rate
        int s_idx = levels[i].id;
        std::string badge = inventory.sector_records[s_idx].best_badge;
        int comp_rate = inventory.sector_records[s_idx].highest_completion_rate;

        glm::vec4 badge_col = (badge == "CLEARED (100%)" || badge == "SECTOR CLEARED (100%)") ? glm::vec4(0.2f, 1.0f, 0.4f, 1.0f) :
                              (badge.find("PARTIAL") != std::string::npos) ? glm::vec4(1.0f, 0.70f, 0.0f, 1.0f) :
                              (badge.find("ABANDONED") != std::string::npos || badge == "EXPEDITION ABANDONED") ? glm::vec4(1.0f, 0.35f, 0.35f, 1.0f) :
                                                                 glm::vec4(0.45f, 0.60f, 0.70f, 0.8f);

        std::string badge_str = (badge == "UNEXPLORED") ? "[UNEXPLORED]" : "[" + badge + "]";
        float badge_x = card_x + card_w - static_cast<float>(badge_str.length() * 8.0f * 1.3f) - 18.0f;
        draw_text(badge_str, badge_x, y + 14.0f, 1.3f, badge_col);
    }

    // Controls overview panel (Bottom)
    float ctrl_y = start_y + 3 * (card_h + 18.0f) + 8.0f;
    float ctrl_w = card_w;
    float ctrl_h = 100.0f;
    draw_rect(card_x, ctrl_y, ctrl_w, ctrl_h, glm::vec4(0.03f, 0.05f, 0.07f, 0.9f));
    draw_rect(card_x, ctrl_y, ctrl_w, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.6f));

    draw_text("DELVER DRILL & EXOSUIT CONTROLS", card_x + 16.0f, ctrl_y + 12.0f, 1.35f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    draw_text("[W,A,S,D] Move  |  [SPACE] Thrusters / Hover  |  [LMB] Drill  |  [RMB] Micro-Charge / Bulkhead", card_x + 16.0f, ctrl_y + 36.0f, 1.25f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    draw_text("[F] Anchor Grapple  |  [E] Reel Cable  |  [Q] Sonar Pulse  |  [1,2,3] Equipment Hotbar", card_x + 16.0f, ctrl_y + 56.0f, 1.25f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    draw_text("[B] Deploy Extraction Beacon  |  [H] Toggle Headlamp  |  [TAB / ESC] Toggle Cursor", card_x + 16.0f, ctrl_y + 76.0f, 1.25f, glm::vec4(0.7f, 0.75f, 0.8f, 0.85f));

    // Action button prompt: [LAUNCH EXPEDITION]
    float prompt_y = ctrl_y + ctrl_h + 18.0f;
    float prompt_h = 44.0f;
    bool btn_hovered = (mouse_x >= card_x && mouse_x <= card_x + card_w &&
                        mouse_y >= prompt_y && mouse_y <= prompt_y + prompt_h);
    if (btn_hovered && mouse_clicked) {
        action_triggered = true;
    }

    glm::vec4 btn_col = btn_hovered ? glm::vec4(0.2f, 0.65f, 0.95f, 1.0f) : glm::vec4(0.12f, 0.4f, 0.65f, 0.95f);
    draw_rect(card_x, prompt_y, card_w, prompt_h, btn_col);
    if (btn_hovered) {
        draw_rect(card_x, prompt_y, card_w, 2.0f, glm::vec4(0.8f, 1.0f, 1.0f, 1.0f));
        draw_rect(card_x, prompt_y + prompt_h - 2.0f, card_w, 2.0f, glm::vec4(0.8f, 1.0f, 1.0f, 1.0f));
    }

    draw_text("[CLICK OR ENTER / SPACE] LAUNCH EXPEDITION   --   [1, 2, 3] SELECT SECTOR", card_x + 30.0f, prompt_y + 14.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return action_triggered;
}

bool OrbitalHubUI::render_orbital_hub(int selected_level, const SkillMatrix& skills, const PlayerInventory& inventory, float mouse_x, float mouse_y, bool mouse_clicked) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

    draw_rect(0, 0, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.96f));

    float cx = w / 2.0f;

    // Header
    draw_text("ORBITAL HUB // EXPEDITION STAGING & LOADOUT CALIBRATION", 40.0f, 40.0f, 2.2f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    draw_rect(40.0f, 75.0f, w - 80.0f, 2.0f, glm::vec4(0.15f, 0.85f, 1.0f, 0.6f));

    // Left Column: Sector Intel & Objectives
    float col1_x = 40.0f;
    float col1_w = (w - 110.0f) * 0.45f;
    float col_y = 95.0f;
    float col_h = h - 180.0f;

    draw_rect(col1_x, col_y, col1_w, col_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.88f));
    draw_rect(col1_x, col_y, 4.0f, col_h, glm::vec4(0.2f, 0.85f, 1.0f, 0.95f));

    draw_text("EXPEDITION SECTOR BRIEFING", col1_x + 20.0f, col_y + 18.0f, 1.6f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    draw_rect(col1_x + 20.0f, col_y + 40.0f, col1_w - 40.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));

    std::string sec_name = (selected_level == 1) ? "SECTOR 1: CRYSTALLINE CAVERNS" :
                           (selected_level == 2) ? "SECTOR 2: SUBTERRANEAN VAULT" :
                                                   "SECTOR 3: FAULT-LINE COLLAPSE";
    draw_text(sec_name, col1_x + 20.0f, col_y + 54.0f, 1.4f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    std::string sec_badge = inventory.sector_records[selected_level].best_badge;
    int sec_rate = inventory.sector_records[selected_level].highest_completion_rate;
    std::string rec_str = "RECORD: [" + sec_badge + "] - BEST RATING: " + std::to_string(sec_rate) + "%";
    glm::vec4 rec_col = (sec_badge == "CLEARED (100%)" || sec_badge == "SECTOR CLEARED (100%)") ? glm::vec4(0.2f, 1.0f, 0.4f, 1.0f) :
                        (sec_badge.find("PARTIAL") != std::string::npos) ? glm::vec4(1.0f, 0.75f, 0.0f, 1.0f) :
                        (sec_badge.find("ABANDONED") != std::string::npos || sec_badge == "EXPEDITION ABANDONED") ? glm::vec4(1.0f, 0.35f, 0.35f, 1.0f) :
                                                               glm::vec4(0.5f, 0.7f, 0.8f, 0.85f);
    draw_text(rec_str, col1_x + 20.0f, col_y + 74.0f, 1.15f, rec_col);

    if (selected_level == 1) {
        draw_text("Target Depth: 120m | Crust Stability: 85%", col1_x + 20.0f, col_y + 96.0f, 1.25f, glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
        draw_text("Directives:", col1_x + 20.0f, col_y + 120.0f, 1.35f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
        draw_text("  1. Mine 25 Voidite Crystals using Subterranean Drill [LMB]", col1_x + 20.0f, col_y + 145.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  2. Use Sonar Pulse [Q] to locate rich mineral veins", col1_x + 20.0f, col_y + 170.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  3. Deploy Extraction Beacon [B] when quota met", col1_x + 20.0f, col_y + 195.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  4. Defend zone for 40s until evacuation landing pod arrives", col1_x + 20.0f, col_y + 220.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
    } else if (selected_level == 2) {
        draw_text("Target Depth: 340m | Crust Stability: 65%", col1_x + 20.0f, col_y + 96.0f, 1.25f, glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
        draw_text("Directives:", col1_x + 20.0f, col_y + 120.0f, 1.35f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
        draw_text("  1. Track subterranean vault signatures using Sonar [Q]", col1_x + 20.0f, col_y + 145.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  2. Equip Demolition Charges [3] to blast Reinforced Doors", col1_x + 20.0f, col_y + 170.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  3. Breach the vault chamber and extract the Hyper-Core Relic", col1_x + 20.0f, col_y + 195.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  4. Call beacon [B] and extract all salvaged minerals", col1_x + 20.0f, col_y + 220.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
    } else {
        draw_text("Target Depth: 600m | Crust Stability: CRITICAL (Collapse Imminent)", col1_x + 20.0f, col_y + 96.0f, 1.25f, glm::vec4(1.0f, 0.3f, 0.3f, 0.9f));
        draw_text("Directives:", col1_x + 20.0f, col_y + 120.0f, 1.35f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
        draw_text("  1. 180-SECOND HARD COUNTDOWN: Subterranean collapse timer active", col1_x + 20.0f, col_y + 145.0f, 1.25f, glm::vec4(1.0f, 0.35f, 0.2f, 0.95f));
        draw_text("  2. Mine 50 Voidite Crystals while surviving recurring cave-ins", col1_x + 20.0f, col_y + 170.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  3. Reach the emergency drop pod before seismic fault collapse", col1_x + 20.0f, col_y + 195.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
    }

    // Right Column: Skill Matrix & Proficiency Branches
    float col2_x = col1_x + col1_w + 30.0f;
    float col2_w = (w - 110.0f) * 0.55f;

    draw_rect(col2_x, col_y, col2_w, col_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.88f));
    draw_rect(col2_x, col_y, 4.0f, col_h, glm::vec4(1.0f, 0.75f, 0.2f, 0.95f));

    draw_text("DELVER PROFICIENCY MATRICES", col2_x + 20.0f, col_y + 18.0f, 1.6f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
    draw_rect(col2_x + 20.0f, col_y + 40.0f, col2_w - 40.0f, 1.0f, glm::vec4(0.4f, 0.35f, 0.2f, 0.5f));

    const SkillBranch* branches[4] = {
        &skills.demolitions,
        &skills.surveying,
        &skills.suit,
        &skills.acrobatics
    };

    float branch_y = col_y + 55.0f;
    for (int i = 0; i < 4; ++i) {
        const auto& b = *branches[i];
        draw_text(b.name + " (" + std::to_string(b.xp) + "/" + std::to_string(b.xp_for_unlock) + " XP)",
                  col2_x + 20.0f, branch_y, 1.35f, b.unlocked ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.85f, 0.9f, 0.95f, 1.0f));

        // Progress bar
        draw_rect(col2_x + 20.0f, branch_y + 20.0f, col2_w - 40.0f, 8.0f, glm::vec4(0.12f, 0.15f, 0.18f, 0.8f));
        glm::vec4 bar_col = b.unlocked ? glm::vec4(0.2f, 0.9f, 0.35f, 0.95f) : glm::vec4(0.2f, 0.75f, 1.0f, 0.95f);
        draw_rect(col2_x + 20.0f, branch_y + 20.0f, (col2_w - 40.0f) * b.progress(), 8.0f, bar_col);

        std::string perk_status = b.unlocked ? "[UNLOCKED] " : "[LOCKED] ";
        std::string perk_line = perk_status + b.unlock_perk_name + ": " + b.unlock_description;
        draw_text(perk_line, col2_x + 20.0f, branch_y + 36.0f, 1.15f, b.unlocked ? glm::vec4(0.3f, 0.9f, 0.5f, 0.95f) : glm::vec4(0.6f, 0.65f, 0.7f, 0.8f));

        branch_y += 82.0f;
    }

    // Launch expedition banner
    float btn_y = h - 65.0f;
    float btn_w = w - 80.0f;
    float btn_x = 40.0f;
    float btn_h = 44.0f;

    bool btn_hovered = (mouse_x >= btn_x && mouse_x <= btn_x + btn_w &&
                        mouse_y >= btn_y && mouse_y <= btn_y + btn_h);
    bool launch_triggered = false;
    if (btn_hovered && mouse_clicked) {
        launch_triggered = true;
    }

    glm::vec4 btn_col = btn_hovered ? glm::vec4(0.25f, 0.65f, 0.95f, 1.0f) : glm::vec4(0.15f, 0.45f, 0.75f, 0.95f);
    draw_rect(btn_x, btn_y, btn_w, btn_h, btn_col);
    if (btn_hovered) {
        draw_rect(btn_x, btn_y, btn_w, 2.0f, glm::vec4(0.8f, 1.0f, 1.0f, 1.0f));
        draw_rect(btn_x, btn_y + btn_h - 2.0f, btn_w, 2.0f, glm::vec4(0.8f, 1.0f, 1.0f, 1.0f));
    }

    draw_text("[CLICK OR PRESS SPACE / ENTER TO LAUNCH EXPEDITION]    |    [TAB / ESC TO RETURN TO MENU]",
              cx - 380.0f, btn_y + 14.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return launch_triggered;
}

DebriefAction OrbitalHubUI::render_debrief(bool success, int level, PlayerInventory& inventory, const SkillMatrix& skills, float mouse_x, float mouse_y, bool mouse_clicked) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

    draw_rect(0, 0, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.96f));

    float cx = w / 2.0f;

    // Header banner
    float banner_y = 35.0f;
    std::string status_text = inventory.is_abandoned ? "EXPEDITION ABANDONED // SALVAGE PENALTY (50%)" :
                              inventory.suit_failed ? "EXPEDITION FAILED: DELVER M.I.A." :
                              success ? "EXPEDITION EXTRACTION SUCCESSFUL" : "EXPEDITION FAILED";
    glm::vec4 status_col = (success && !inventory.is_abandoned && !inventory.suit_failed) ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) :
                           inventory.is_abandoned ? glm::vec4(1.0f, 0.70f, 0.0f, 1.0f) :
                           glm::vec4(1.0f, 0.25f, 0.2f, 1.0f);

    draw_text(status_text, cx - 330.0f, banner_y, 2.0f, status_col);
    draw_rect(cx - 400.0f, banner_y + 35.0f, 800.0f, 2.0f, status_col * 0.7f);

    float total_content_w = std::min(w * 0.88f, 1100.0f);
    float col_w = (total_content_w - 24.0f) * 0.5f;
    float start_x = (w - total_content_w) * 0.5f;
    float col1_x = start_x;
    float col2_x = start_x + col_w + 24.0f;
    float panel_y = banner_y + 50.0f;
    float panel_h = 440.0f;

    // ==========================================
    // LEFT COLUMN: SUBTERRANEAN EXPEDITION SUMMARY
    // ==========================================
    draw_rect(col1_x, panel_y, col_w, panel_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.92f));
    draw_rect(col1_x, panel_y, 4.0f, panel_h, status_col);
    draw_rect(col1_x, panel_y, col_w, 1.0f, glm::vec4(0.2f, 0.35f, 0.45f, 0.6f));

    draw_text("EXPEDITION HARVEST & MANIFEST", col1_x + 20.0f, panel_y + 14.0f, 1.45f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    std::string badge_disp = "[" + inventory.run_outcome_badge + "]  --  " + std::to_string(inventory.run_completion_rate) + "% COMPLETION";
    draw_text(badge_disp, col1_x + 20.0f, panel_y + 36.0f, 1.25f, status_col);
    draw_rect(col1_x + 20.0f, panel_y + 54.0f, col_w - 40.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));

    float stat_y = panel_y + 64.0f;
    std::string v_str = "Voidite Extracted:     " + std::to_string(inventory.voidite) + " units (" + std::to_string(inventory.voidite * 5) + " pts)";
    draw_text(v_str, col1_x + 20.0f, stat_y, 1.3f, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));

    stat_y += 30.0f;
    std::string t_str = "Titanium Cores:        " + std::to_string(inventory.titanium) + " cores (" + std::to_string(inventory.titanium * 6) + " pts)";
    draw_text(t_str, col1_x + 20.0f, stat_y, 1.3f, glm::vec4(1.0f, 0.70f, 0.0f, 1.0f));

    stat_y += 30.0f;
    std::string sc_str = "Scrap Metal Salvaged:  " + std::to_string(inventory.scrap_metal) + " scrap";
    draw_text(sc_str, col1_x + 20.0f, stat_y, 1.3f, glm::vec4(0.85f, 0.85f, 0.9f, 1.0f));

    stat_y += 30.0f;
    std::string s_str = "Mineral Salvage:       " + std::to_string(inventory.salvage_parts) + " units (" + std::to_string(inventory.salvage_parts * 2) + " pts)";
    draw_text(s_str, col1_x + 20.0f, stat_y, 1.3f, glm::vec4(0.7f, 0.75f, 0.8f, 1.0f));

    if (level == 2) {
        stat_y += 30.0f;
        std::string r_str = inventory.relic_extracted ? "Hyper-Core Relic:      RECOVERED (+250 pts)" : "Hyper-Core Relic:      NOT RECOVERED";
        draw_text(r_str, col1_x + 20.0f, stat_y, 1.3f, inventory.relic_extracted ? glm::vec4(1.0f, 0.0f, 0.85f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.6f, 0.8f));
    }

    stat_y += 38.0f;
    draw_rect(col1_x + 20.0f, stat_y - 8.0f, col_w - 40.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));
    std::string score_str = "TOTAL SCORE: " + std::to_string(inventory.total_run_score) + " POINTS";
    draw_text(score_str, col1_x + 20.0f, stat_y, 1.6f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    stat_y += 45.0f;
    draw_text("DELVER SPECIALIZATIONS XP:", col1_x + 20.0f, stat_y, 1.3f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    stat_y += 24.0f;
    draw_text("Demolitions: " + std::to_string(skills.demolitions.xp) + " XP" + (skills.demolitions.unlocked ? " [ACTIVE]" : ""),
              col1_x + 20.0f, stat_y, 1.15f, skills.demolitions.unlocked ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
    stat_y += 20.0f;
    draw_text("Surveying:   " + std::to_string(skills.surveying.xp) + " XP" + (skills.surveying.unlocked ? " [ACTIVE]" : ""),
              col1_x + 20.0f, stat_y, 1.15f, skills.surveying.unlocked ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));

    // ==========================================
    // RIGHT COLUMN: AUGMENTATIONS & UPGRADES TERMINAL
    // ==========================================
    draw_rect(col2_x, panel_y, col_w, panel_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.92f));
    draw_rect(col2_x, panel_y, 4.0f, panel_h, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    draw_rect(col2_x, panel_y, col_w, 1.0f, glm::vec4(0.2f, 0.35f, 0.45f, 0.6f));

    draw_text("AUGMENTATIONS & UPGRADES TERMINAL", col2_x + 20.0f, panel_y + 16.0f, 1.5f, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    draw_rect(col2_x + 20.0f, panel_y + 38.0f, col_w - 40.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));

    std::string bal_str = "FUNDS: " + std::to_string(inventory.voidite) + " VOIDITE  |  " + std::to_string(inventory.titanium) + " TITANIUM";
    draw_text(bal_str, col2_x + 20.0f, panel_y + 46.0f, 1.25f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    // 5 Upgrades
    struct UpgradeDef {
        std::string name;
        std::string benefit;
        int* level_ptr;
        bool is_voidite;
        int cost;
    };

    UpgradeDef upgrades[5] = {
        {"1. Drill Speed", "+20% Mining Rate", &inventory.upgrades.drill_speed_level, true, 15},
        {"2. Thruster Energy", "+25% Thruster & Recharge", &inventory.upgrades.thruster_energy_level, true, 20},
        {"3. Sonar Pulse Range", "+5m Scan Radius", &inventory.upgrades.sonar_range_level, true, 15},
        {"4. Max Bulkhead Cap", "+5 Max Bulkheads", &inventory.upgrades.max_bulkheads_level, false, 4},
        {"5. Shield Plating", "+25% Impact Armor", &inventory.upgrades.shield_plating_level, false, 6}
    };

    float up_y = panel_y + 72.0f;
    float up_h = 64.0f;
    float up_w = col_w - 40.0f;

    for (int i = 0; i < 5; ++i) {
        auto& u = upgrades[i];
        float ux = col2_x + 20.0f;
        float uy = up_y + i * (up_h + 8.0f);

        draw_rect(ux, uy, up_w, up_h, glm::vec4(0.06f, 0.08f, 0.12f, 0.85f));
        draw_rect(ux, uy, up_w, 1.0f, glm::vec4(0.15f, 0.25f, 0.35f, 0.5f));

        std::string title = u.name + " [LVL " + std::to_string(*u.level_ptr) + "]";
        draw_text(title, ux + 10.0f, uy + 10.0f, 1.25f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
        draw_text(u.benefit, ux + 10.0f, uy + 32.0f, 1.1f, glm::vec4(0.65f, 0.75f, 0.85f, 0.85f));

        // Upgrade button
        float btn_w = 120.0f;
        float btn_h = 36.0f;
        float bx = ux + up_w - btn_w - 10.0f;
        float by = uy + 14.0f;

        bool can_afford = u.is_voidite ? (inventory.voidite >= u.cost) : (inventory.titanium >= u.cost);
        bool btn_hov = (mouse_x >= bx && mouse_x <= bx + btn_w && mouse_y >= by && mouse_y <= by + btn_h);

        if (btn_hov && mouse_clicked && can_afford) {
            if (u.is_voidite) {
                inventory.voidite -= u.cost;
            } else {
                inventory.titanium -= u.cost;
            }
            (*u.level_ptr)++;
        }

        glm::vec4 b_bg = can_afford ?
            (btn_hov ? glm::vec4(0.2f, 0.6f, 0.85f, 1.0f) : glm::vec4(0.1f, 0.35f, 0.6f, 0.9f)) :
            glm::vec4(0.1f, 0.12f, 0.15f, 0.6f);
        draw_rect(bx, by, btn_w, btn_h, b_bg);
        if (can_afford) {
            draw_rect(bx, by, btn_w, 1.0f, glm::vec4(0.4f, 0.85f, 1.0f, 0.8f));
        }

        std::string cost_str = std::to_string(u.cost) + (u.is_voidite ? " VOID" : " TITAN");
        draw_text("UPGRADE", bx + 18.0f, by + 6.0f, 1.15f, can_afford ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.5f, 0.5f, 0.5f, 0.7f));
        draw_text(cost_str, bx + 16.0f, by + 20.0f, 1.0f, can_afford ? (u.is_voidite ? glm::vec4(0.0f, 0.94f, 1.0f, 1.0f) : glm::vec4(1.0f, 0.70f, 0.0f, 1.0f)) : glm::vec4(0.45f, 0.45f, 0.45f, 0.7f));
    }

    // ==========================================
    // BOTTOM BUTTONS: LAUNCH NEXT SECTOR & RETURN TO HUB
    // ==========================================
    DebriefAction result = DebriefAction::None;
    float bot_y = panel_y + panel_h + 16.0f;
    float bot_btn_w = (total_content_w - 20.0f) * 0.5f;
    float bot_btn_h = 48.0f;

    // Button 1: [LAUNCH NEXT SECTOR]
    float b1_x = start_x;
    bool b1_hov = (mouse_x >= b1_x && mouse_x <= b1_x + bot_btn_w && mouse_y >= bot_y && mouse_y <= bot_y + bot_btn_h);
    if (b1_hov && mouse_clicked) {
        result = DebriefAction::LaunchNextSector;
    }
    glm::vec4 b1_col = b1_hov ? glm::vec4(0.15f, 0.65f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.45f, 0.38f, 0.95f);
    draw_rect(b1_x, bot_y, bot_btn_w, bot_btn_h, b1_col);
    if (b1_hov) {
        draw_rect(b1_x, bot_y, bot_btn_w, 2.0f, glm::vec4(0.4f, 1.0f, 0.8f, 1.0f));
        draw_rect(b1_x, bot_y + bot_btn_h - 2.0f, bot_btn_w, 2.0f, glm::vec4(0.4f, 1.0f, 0.8f, 1.0f));
    }
    draw_text("[LAUNCH NEXT SECTOR]", b1_x + bot_btn_w * 0.5f - 110.0f, bot_y + 16.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // Button 2: [RETURN TO ORBITAL HUB]
    float b2_x = start_x + bot_btn_w + 20.0f;
    bool b2_hov = (mouse_x >= b2_x && mouse_x <= b2_x + bot_btn_w && mouse_y >= bot_y && mouse_y <= bot_y + bot_btn_h);
    if (b2_hov && mouse_clicked) {
        result = DebriefAction::ReturnToHub;
    }
    glm::vec4 b2_col = b2_hov ? glm::vec4(0.2f, 0.45f, 0.75f, 1.0f) : glm::vec4(0.12f, 0.3f, 0.55f, 0.95f);
    draw_rect(b2_x, bot_y, bot_btn_w, bot_btn_h, b2_col);
    if (b2_hov) {
        draw_rect(b2_x, bot_y, bot_btn_w, 2.0f, glm::vec4(0.6f, 0.85f, 1.0f, 1.0f));
        draw_rect(b2_x, bot_y + bot_btn_h - 2.0f, bot_btn_w, 2.0f, glm::vec4(0.6f, 0.85f, 1.0f, 1.0f));
    }
    draw_text("[RETURN TO ORBITAL HUB]", b2_x + bot_btn_w * 0.5f - 120.0f, bot_y + 16.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return result;
}

} // namespace Voidfall
