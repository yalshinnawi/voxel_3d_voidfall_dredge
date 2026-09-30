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

void OrbitalHubUI::render_main_menu(int selected_level) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

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

    // Sector Selection Cards
    float card_w = 760.0f;
    float card_h = 95.0f;
    float card_x = cx - card_w / 2.0f;
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
        bool is_sel = (selected_level == levels[i].id);

        glm::vec4 bg_col = is_sel ? glm::vec4(0.08f, 0.16f, 0.25f, 0.95f) : glm::vec4(0.04f, 0.06f, 0.09f, 0.85f);
        draw_rect(card_x, y, card_w, card_h, bg_col);

        glm::vec4 border_col = is_sel ? glm::vec4(0.2f, 0.95f, 1.0f, 1.0f) : glm::vec4(0.2f, 0.35f, 0.45f, 0.5f);
        draw_rect(card_x, y, 4.0f, card_h, border_col);
        draw_rect(card_x, y, card_w, 1.0f, border_col * 0.7f);

        std::string selector = is_sel ? ">> " : "   ";
        draw_text(selector + levels[i].title, card_x + 16.0f, y + 14.0f, 1.6f, is_sel ? glm::vec4(0.2f, 0.95f, 1.0f, 1.0f) : glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
        draw_text(levels[i].desc, card_x + 36.0f, y + 40.0f, 1.25f, glm::vec4(0.7f, 0.75f, 0.8f, 0.85f));
        draw_text(levels[i].obj, card_x + 36.0f, y + 62.0f, 1.25f, is_sel ? glm::vec4(1.0f, 0.85f, 0.2f, 0.95f) : glm::vec4(0.5f, 0.75f, 0.55f, 0.8f));
    }

    // Controls overview panel (Bottom)
    float ctrl_y = start_y + 3 * (card_h + 18.0f) + 8.0f;
    float ctrl_w = 760.0f;
    float ctrl_h = 100.0f;
    draw_rect(card_x, ctrl_y, ctrl_w, ctrl_h, glm::vec4(0.03f, 0.05f, 0.07f, 0.9f));
    draw_rect(card_x, ctrl_y, ctrl_w, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.6f));

    draw_text("DELVER DRILL & EXOSUIT CONTROLS", card_x + 16.0f, ctrl_y + 12.0f, 1.35f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    draw_text("[W,A,S,D] Move  |  [SPACE] Thrusters / Hover  |  [LMB] Drill  |  [RMB] Micro-Charge / Bulkhead", card_x + 16.0f, ctrl_y + 36.0f, 1.25f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    draw_text("[F] Anchor Grapple  |  [E] Reel Cable  |  [Q] Sonar Pulse  |  [1,2,3] Equipment Hotbar", card_x + 16.0f, ctrl_y + 56.0f, 1.25f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    draw_text("[B] Deploy Extraction Beacon  |  [H] Toggle Headlamp  |  [TAB / ESC] Toggle Cursor", card_x + 16.0f, ctrl_y + 76.0f, 1.25f, glm::vec4(0.7f, 0.75f, 0.8f, 0.85f));

    // Action button prompt
    float prompt_y = ctrl_y + ctrl_h + 18.0f;
    draw_rect(card_x, prompt_y, card_w, 42.0f, glm::vec4(0.12f, 0.4f, 0.65f, 0.95f));
    draw_text("[1, 2, 3] SELECT SECTOR   --   [SPACE / ENTER] LAUNCH EXPEDITION", card_x + 80.0f, prompt_y + 13.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void OrbitalHubUI::render_orbital_hub(int selected_level, const SkillMatrix& skills, const PlayerInventory& inventory) {
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

    if (selected_level == 1) {
        draw_text("Target Depth: 120m | Crust Stability: 85%", col1_x + 20.0f, col_y + 80.0f, 1.25f, glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
        draw_text("Directives:", col1_x + 20.0f, col_y + 110.0f, 1.35f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
        draw_text("  1. Mine 25 Voidite Crystals using Subterranean Drill [LMB]", col1_x + 20.0f, col_y + 135.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  2. Use Sonar Pulse [Q] to locate rich mineral veins", col1_x + 20.0f, col_y + 160.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  3. Deploy Extraction Beacon [B] when quota met", col1_x + 20.0f, col_y + 185.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  4. Defend zone for 90s until evacuation landing pod arrives", col1_x + 20.0f, col_y + 210.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
    } else if (selected_level == 2) {
        draw_text("Target Depth: 340m | Crust Stability: 65%", col1_x + 20.0f, col_y + 80.0f, 1.25f, glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
        draw_text("Directives:", col1_x + 20.0f, col_y + 110.0f, 1.35f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
        draw_text("  1. Track subterranean vault signatures using Sonar [Q]", col1_x + 20.0f, col_y + 135.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  2. Equip Demolition Charges [3] to blast Reinforced Doors", col1_x + 20.0f, col_y + 160.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  3. Breach the vault chamber and extract the Hyper-Core Relic", col1_x + 20.0f, col_y + 185.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  4. Call beacon [B] and extract all salvaged minerals", col1_x + 20.0f, col_y + 210.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
    } else {
        draw_text("Target Depth: 600m | Crust Stability: CRITICAL (Collapse Imminent)", col1_x + 20.0f, col_y + 80.0f, 1.25f, glm::vec4(1.0f, 0.3f, 0.3f, 0.9f));
        draw_text("Directives:", col1_x + 20.0f, col_y + 110.0f, 1.35f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
        draw_text("  1. 180-SECOND HARD COUNTDOWN: Subterranean collapse timer active", col1_x + 20.0f, col_y + 135.0f, 1.25f, glm::vec4(1.0f, 0.35f, 0.2f, 0.95f));
        draw_text("  2. Mine 50 Voidite Crystals while surviving recurring cave-ins", col1_x + 20.0f, col_y + 160.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        draw_text("  3. Reach the emergency drop pod before seismic fault collapse", col1_x + 20.0f, col_y + 185.0f, 1.25f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
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
    draw_rect(40.0f, btn_y, w - 80.0f, 44.0f, glm::vec4(0.15f, 0.45f, 0.75f, 0.95f));
    draw_text("[PRESS SPACE / ENTER TO LAUNCH EXPEDITION]    |    [TAB / ESC TO RETURN TO MENU]",
              cx - 360.0f, btn_y + 14.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void OrbitalHubUI::render_debrief(bool success, int level, const PlayerInventory& inventory, const SkillMatrix& skills) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

    draw_rect(0, 0, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.96f));

    float cx = w / 2.0f;

    // Header banner
    float banner_y = 70.0f;
    glm::vec4 status_col = success ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.25f, 0.2f, 1.0f);
    std::string status_text = success ? "EXPEDITION EXTRACTION SUCCESSFUL" : "EXPEDITION FAILED: DELVER M.I.A.";
    draw_text(status_text, cx - 290.0f, banner_y, 2.4f, status_col);

    draw_rect(cx - 380.0f, banner_y + 40.0f, 760.0f, 2.0f, status_col * 0.6f);

    // Summary Card
    float card_w = 760.0f;
    float card_h = 320.0f;
    float card_x = cx - card_w / 2.0f;
    float card_y = banner_y + 60.0f;

    draw_rect(card_x, card_y, card_w, card_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.9f));
    draw_rect(card_x, card_y, 4.0f, card_h, status_col);

    draw_text("SUBTERRANEAN CARGO MANIFEST", card_x + 24.0f, card_y + 20.0f, 1.6f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    draw_rect(card_x + 24.0f, card_y + 44.0f, card_w - 48.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));

    std::string v_str = "Voidite Crystals Extracted: " + std::to_string(inventory.voidite) + " units (" + std::to_string(inventory.voidite * 5) + " pts)";
    draw_text(v_str, card_x + 24.0f, card_y + 60.0f, 1.35f, glm::vec4(0.85f, 0.4f, 1.0f, 1.0f));

    std::string t_str = "Titanium Bulkheads Salvaged: " + std::to_string(inventory.titanium) + " units (" + std::to_string(inventory.titanium * 6) + " pts)";
    draw_text(t_str, card_x + 24.0f, card_y + 88.0f, 1.35f, glm::vec4(0.4f, 0.85f, 1.0f, 1.0f));

    std::string s_str = "General Mineral Salvage:    " + std::to_string(inventory.salvage_parts) + " units (" + std::to_string(inventory.salvage_parts * 2) + " pts)";
    draw_text(s_str, card_x + 24.0f, card_y + 116.0f, 1.35f, glm::vec4(0.75f, 0.8f, 0.85f, 1.0f));

    if (level == 2) {
        std::string r_str = inventory.relic_extracted ? "Hyper-Core Relic:           RECOVERED (+250 pts)" : "Hyper-Core Relic:           NOT RECOVERED";
        draw_text(r_str, card_x + 24.0f, card_y + 144.0f, 1.35f, inventory.relic_extracted ? glm::vec4(1.0f, 0.82f, 0.2f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.6f, 0.8f));
    }

    draw_rect(card_x + 24.0f, card_y + 180.0f, card_w - 48.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));

    std::string total_str = "TOTAL EXPEDITION SCORE: " + std::to_string(inventory.total_run_score) + " POINTS";
    draw_text(total_str, card_x + 24.0f, card_y + 195.0f, 1.7f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    // Skill unlocks notification
    draw_text("DELVER SPECIALIZATIONS EARNED:", card_x + 24.0f, card_y + 240.0f, 1.35f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    std::string sk1 = "Demolitions: " + std::to_string(skills.demolitions.xp) + " XP" + (skills.demolitions.unlocked ? " [Micro-Charges UNLOCKED]" : "");
    std::string sk2 = "Surveying:   " + std::to_string(skills.surveying.xp) + " XP" + (skills.surveying.unlocked ? " [30-Voxel Sonar UNLOCKED]" : "");
    draw_text(sk1, card_x + 24.0f, card_y + 264.0f, 1.25f, skills.demolitions.unlocked ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
    draw_text(sk2, card_x + 360.0f, card_y + 264.0f, 1.25f, skills.surveying.unlocked ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));

    // Next Actions
    float btn_y = card_y + card_h + 30.0f;
    draw_rect(card_x, btn_y, card_w, 44.0f, glm::vec4(0.12f, 0.4f, 0.65f, 0.95f));
    draw_text("[SPACE / ENTER] RETURN TO ORBITAL HUB      |      [R] RETRY EXPEDITION",
              card_x + 70.0f, btn_y + 14.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

} // namespace Voidfall
