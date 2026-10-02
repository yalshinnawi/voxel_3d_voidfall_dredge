#include "orbital_hub.hpp"
#include "font_renderer.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include "../include/font8x8.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace Voidfall {

static UserProfile s_fallback_profile;

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

    m_text_shader.use();
    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);
    m_text_shader.set_mat4("uProjection", proj);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_font_tex);
    m_text_shader.set_int("uFontTexture", 0);
    m_text_shader.set_vec2("uShadowOffset", glm::vec2(-1.5f / 4096.0f, -1.5f / 32.0f));
    m_text_shader.set_vec4("uTextColor", color);

    glBindVertexArray(m_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_text_vbo);

    std::vector<float> vertices;
    vertices.reserve(text.length() * 24);
    FontRenderer::build_text_vertices(text, x, y, scale * 0.45f, vertices);

    if (!vertices.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data());
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));
    }

    glBindVertexArray(0);
}

bool OrbitalHubUI::render_main_menu(int& selected_level, const PlayerInventory& inventory, float mouse_x, float mouse_y, bool mouse_clicked) {
    bool has_save = false;
    for (int s = 1; s <= 3; ++s) {
        s_fallback_profile.sector_records[s] = inventory.sector_records[s];
        if (inventory.sector_records[s].highest_completion_rate > 0) has_save = true;
    }
    MainMenuAction action = render_main_menu(selected_level, s_fallback_profile, has_save, mouse_x, mouse_y, mouse_clicked);
    return (action == MainMenuAction::Continue || action == MainMenuAction::NewExpedition);
}

bool OrbitalHubUI::render_main_menu(int& selected_level, UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked) {
    bool has_save = (profile.total_exp > 0 || profile.sector_records[1].highest_completion_rate > 0 || profile.sector_records[2].highest_completion_rate > 0 || profile.sector_records[3].highest_completion_rate > 0);
    MainMenuAction action = render_main_menu(selected_level, profile, has_save, mouse_x, mouse_y, mouse_clicked);
    return (action == MainMenuAction::Continue || action == MainMenuAction::NewExpedition);
}

MainMenuAction OrbitalHubUI::render_main_menu(int& selected_level, UserProfile& profile, bool has_save, float mouse_x, float mouse_y, bool mouse_clicked) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

    MainMenuAction action = MainMenuAction::None;

    if (m_subview == MenuSubView::SectorSelect) {
        render_sector_select_carousel(selected_level, profile, mouse_x, mouse_y, mouse_clicked, action);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        return action;
    } else if (m_subview == MenuSubView::Upgrades) {
        // Dark backdrop for upgrades
        draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.95f));

        // Back button
        float back_w = 180.0f;
        float back_h = 42.0f;
        float back_x = 40.0f;
        float back_y = 24.0f;
        bool back_hover = (mouse_x >= back_x && mouse_x <= back_x + back_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (back_hover && mouse_clicked) {
            m_subview = MenuSubView::Main;
        }
        draw_rect(back_x, back_y, back_w, back_h, back_hover ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG);
        draw_rect(back_x, back_y, back_w, 1.0f, back_hover ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        draw_rect(back_x, back_y + back_h - 1.0f, back_w, 1.0f, back_hover ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        draw_rect(back_x, back_y, 1.0f, back_h, back_hover ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        draw_rect(back_x + back_w - 1.0f, back_y, 1.0f, back_h, back_hover ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        draw_text("< MAIN MENU", back_x + 20.0f, back_y + 12.0f, 1.35f, back_hover ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

        // Subtabs for Delvers and Upgrades
        float tab1_x = back_x + back_w + 32.0f;
        float tab_w = 200.0f;
        bool t1_hov = (mouse_x >= tab1_x && mouse_x <= tab1_x + tab_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (t1_hov && mouse_clicked) m_active_tab = HubTab::DelverRoster;
        bool t1_act = (m_active_tab == HubTab::DelverRoster);
        draw_rect(tab1_x, back_y, tab_w, back_h, t1_act ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) : (t1_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG));
        draw_rect(tab1_x, back_y + back_h - 2.0f, tab_w, 2.0f, t1_act ? Typography::COLOR_CYAN : glm::vec4(0.3f, 0.4f, 0.5f, 0.4f));
        draw_text("[ DELVER ROSTER ]", tab1_x + 16.0f, back_y + 12.0f, 1.25f, t1_act ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

        float tab2_x = tab1_x + tab_w + 16.0f;
        bool t2_hov = (mouse_x >= tab2_x && mouse_x <= tab2_x + tab_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (t2_hov && mouse_clicked) m_active_tab = HubTab::UpgradeTerminal;
        bool t2_act = (m_active_tab == HubTab::UpgradeTerminal);
        draw_rect(tab2_x, back_y, tab_w, back_h, t2_act ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) : (t2_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG));
        draw_rect(tab2_x, back_y + back_h - 2.0f, tab_w, 2.0f, t2_act ? Typography::COLOR_CYAN : glm::vec4(0.3f, 0.4f, 0.5f, 0.4f));
        draw_text("[ UPGRADE TERMINAL ]", tab2_x + 12.0f, back_y + 12.0f, 1.25f, t2_act ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

        if (m_active_tab == HubTab::DelverRoster) {
            render_delver_roster(profile, mouse_x, mouse_y, mouse_clicked);
        } else {
            render_upgrade_terminal(profile, mouse_x, mouse_y, mouse_clicked);
        }

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        return action;
    }

    // Default Main Menu View
    draw_rect(0.0f, 0.0f, std::min(w * 0.45f, 600.0f), h, glm::vec4(0.02f, 0.03f, 0.05f, 0.85f));
    draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.01f, 0.02f, 0.03f, 0.25f));

    float start_x = 100.0f;
    float title_y = h * 0.20f;

    // Bold Game Title banner
    draw_text("VOIDFALL: DREDGE", start_x, title_y, 3.4f, Typography::COLOR_CYAN);
    draw_text("SUBTERRANEAN EXPEDITION PROTOCOL", start_x + 4.0f, title_y + 44.0f, 1.35f, Typography::COLOR_PRIMARY);

    // Accent line beneath title
    draw_rect(start_x, title_y + 70.0f, 440.0f, 2.0f, glm::vec4(0.0f, 0.90f, 1.0f, 0.60f));

    // Clean Action List (Only 4 Primary Buttons)
    struct MenuItem {
        MainMenuAction act;
        std::string label;
        bool enabled;
        std::string tip;
    };

    std::vector<MenuItem> items = {
        { MainMenuAction::Continue,        "[ CONTINUE ]",             has_save, "Resume operations in Sector " + std::to_string(selected_level) },
        { MainMenuAction::NewExpedition,   "[ NEW EXPEDITION ]",        true,     "Choose sector & deploy immediately" },
        { MainMenuAction::UpgradeTerminal, "[ UPGRADES / DELVERS ]",    true,     "Delver roster & drill modifications" },
        { MainMenuAction::Exit,            "[ EXIT ]",                  true,     "Quit to desktop cleanly" }
    };

    float btn_y = title_y + 96.0f;
    float btn_w = 420.0f;
    float btn_h = 56.0f; // Scaled button height for >= 28px text
    float btn_gap = 14.0f;

    for (size_t i = 0; i < items.size(); ++i) {
        float y = btn_y + i * (btn_h + btn_gap);
        const auto& item = items[i];

        bool is_hovered = item.enabled && (mouse_x >= start_x && mouse_x <= start_x + btn_w + 10.0f &&
                                           mouse_y >= y && mouse_y <= y + btn_h);

        float current_btn_x = is_hovered ? (start_x + 6.0f) : start_x;

        if (is_hovered && mouse_clicked) {
            if (item.act == MainMenuAction::NewExpedition) {
                m_subview = MenuSubView::SectorSelect;
            } else if (item.act == MainMenuAction::UpgradeTerminal) {
                m_subview = MenuSubView::Upgrades;
            } else {
                action = item.act;
            }
        }

        // Button background
        glm::vec4 bg_col = !item.enabled ? glm::vec4(0.04f, 0.05f, 0.07f, 0.65f) :
                           is_hovered    ? Typography::COLOR_BUTTON_HOV :
                                           Typography::COLOR_BUTTON_BG;
        draw_rect(current_btn_x, y, btn_w, btn_h, bg_col);

        // Button border
        glm::vec4 border_col = !item.enabled ? Typography::COLOR_MUTED * 0.4f :
                               is_hovered    ? Typography::COLOR_CYAN_GLOW :
                                               glm::vec4(0.0f, 0.85f, 1.0f, 0.35f);

        draw_rect(current_btn_x, y, btn_w, is_hovered ? 2.0f : 1.0f, border_col);
        draw_rect(current_btn_x, y + btn_h - (is_hovered ? 2.0f : 1.0f), btn_w, is_hovered ? 2.0f : 1.0f, border_col);
        draw_rect(current_btn_x, y, is_hovered ? 4.0f : 2.0f, btn_h, border_col);
        draw_rect(current_btn_x + btn_w - (is_hovered ? 2.0f : 1.0f), y, is_hovered ? 2.0f : 1.0f, btn_h, border_col);

        // Centered button text at >= 28px height
        float text_scale = 1.75f;
        float text_w = FontRenderer::get_text_width(item.label, text_scale * 0.45f);
        float text_x = current_btn_x + (btn_w - text_w) * 0.5f;
        float text_y = y + (btn_h - 32.0f * text_scale * 0.45f) * 0.5f;

        glm::vec4 text_col = !item.enabled ? Typography::COLOR_MUTED :
                             is_hovered    ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) :
                                             Typography::COLOR_PRIMARY;
        draw_text(item.label, text_x, text_y, text_scale, text_col);

        if (is_hovered) {
            draw_text(item.tip, current_btn_x + btn_w + 24.0f, y + (btn_h - 16.0f) * 0.5f, 1.25f, Typography::COLOR_CYAN);
        }
    }

    // Status watermark at bottom
    std::string ver_info = "VOIDFALL // DEEP EXPEDITION ENGINE v1.0.4";
    draw_text(ver_info, start_x, h - 40.0f, 1.1f, Typography::COLOR_MUTED);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return action;
}

void OrbitalHubUI::render_sector_select_carousel(int& selected_level, UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked, MainMenuAction& action) {
    (void)profile;
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

    // Dark backdrop tint
    draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.94f));

    // Header
    float header_x = 80.0f;
    float header_y = 40.0f;
    draw_text("SELECT EXPEDITION SECTOR", header_x, header_y, 2.6f, Typography::COLOR_CYAN);
    draw_text("CHOOSE A SECTOR TO COMMENCE EXTRACTION DESCENT", header_x + 4.0f, header_y + 36.0f, 1.25f, Typography::COLOR_PRIMARY);
    draw_rect(header_x, header_y + 58.0f, w - 160.0f, 2.0f, glm::vec4(0.0f, 0.90f, 1.0f, 0.40f));

    // 3 Sector Cards - generous width and spacing
    float card_w = 440.0f;
    float card_h = h - 240.0f;
    float card_gap = 24.0f;
    float total_cards_w = card_w * 3.0f + card_gap * 2.0f;
    float start_x = (w - total_cards_w) * 0.5f;
    float start_y = 125.0f;

    struct SectorCardData {
        int level;
        std::string title;
        std::string tier;
        std::string depth;
        std::string objective;
        std::string hazard;
        std::string desc;
        std::string bonus;
        glm::vec4 accent;
    };

    SectorCardData sectors[3] = {
        {
            1,
            "SECTOR 1: PERIMETER DRIFT",
            "[ TIER I // LOW HAZARD ]",
            "ESTIMATED DEPTH: 800 METERS",
            "OBJECTIVE: EXTRACT 25 VOIDITE",
            "HAZARD: 4-PHASE ESCALATION CURVE",
            "Porous crystalline caverns with stable geological anchor points. Rich in unrefined voidite deposits.",
            "EXP MULTIPLIER: 1.0x",
            Typography::COLOR_CYAN
        },
        {
            2,
            "SECTOR 2: VOLATILE FAULT",
            "[ TIER II // MEDIUM HAZARD ]",
            "ESTIMATED DEPTH: 1,800 METERS",
            "OBJECTIVE: BREACH VAULT & RECOVER RELIC",
            "HAZARD: ACID GAS & SEISMIC TREMORS",
            "Reinforced basalt subterranean chambers holding pre-fall research vaults. High titanium concentration.",
            "EXP MULTIPLIER: 1.5x",
            Typography::COLOR_AMBER
        },
        {
            3,
            "SECTOR 3: VOID CRADLE",
            "[ TIER III // CRITICAL HAZARD ]",
            "ESTIMATED DEPTH: 3,200 METERS",
            "OBJECTIVE: 50 VOIDITE / EXTRACTION POD",
            "HAZARD: 3-MINUTE TECTONIC COLLAPSE",
            "Extreme abyss fissures bordering bedrock mantle. Unstable tectonic gravity causes imminent total cave-in.",
            "EXP MULTIPLIER: 2.5x",
            Typography::COLOR_CRIMSON
        }
    };

    for (int i = 0; i < 3; ++i) {
        float cx = start_x + i * (card_w + card_gap);
        const auto& sec = sectors[i];
        bool is_selected = (selected_level == sec.level);
        bool is_hovered = (mouse_x >= cx && mouse_x <= cx + card_w &&
                           mouse_y >= start_y && mouse_y <= start_y + card_h);

        if (is_hovered && mouse_clicked) {
            selected_level = sec.level;
        }

        glm::vec4 bg_col = is_selected ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) :
                           is_hovered  ? glm::vec4(0.06f, 0.11f, 0.17f, 0.90f) :
                                         glm::vec4(0.03f, 0.05f, 0.08f, 0.85f);
        draw_rect(cx, start_y, card_w, card_h, bg_col);

        glm::vec4 border_col = is_selected ? sec.accent :
                               is_hovered  ? glm::vec4(0.4f, 0.85f, 1.0f, 0.8f) :
                                             glm::vec4(0.2f, 0.35f, 0.45f, 0.4f);

        draw_rect(cx, start_y, card_w, is_selected ? 3.0f : 1.0f, border_col);
        draw_rect(cx, start_y + card_h - (is_selected ? 3.0f : 1.0f), card_w, is_selected ? 3.0f : 1.0f, border_col);
        draw_rect(cx, start_y, is_selected ? 4.0f : 2.0f, card_h, border_col);
        draw_rect(cx + card_w - (is_selected ? 4.0f : 2.0f), start_y, is_selected ? 4.0f : 2.0f, card_h, border_col);

        // Header
        draw_text(sec.tier, cx + 18.0f, start_y + 18.0f, 1.25f, sec.accent);
        draw_text(sec.title, cx + 18.0f, start_y + 40.0f, 1.55f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        draw_text(sec.depth, cx + 18.0f, start_y + 68.0f, 1.15f, Typography::COLOR_MUTED);

        draw_rect(cx + 18.0f, start_y + 88.0f, card_w - 36.0f, 1.0f, glm::vec4(0.25f, 0.4f, 0.55f, 0.4f));

        // Mission parameters
        draw_text(sec.objective, cx + 18.0f, start_y + 104.0f, 1.30f, Typography::COLOR_PRIMARY);
        draw_text(sec.hazard, cx + 18.0f, start_y + 132.0f, 1.20f, sec.accent);

        draw_text("TOPOLOGICAL SCAN:", cx + 18.0f, start_y + 168.0f, 1.15f, Typography::COLOR_MUTED);
        
        // Multi-line wrapped description
        float line_y = start_y + 194.0f;
        std::string remaining = sec.desc;
        while (!remaining.empty()) {
            size_t max_chars = 42;
            size_t len = std::min(max_chars, remaining.size());
            if (len < remaining.size()) {
                size_t space = remaining.rfind(' ', len);
                if (space != std::string::npos && space > 0) len = space;
            }
            std::string line = remaining.substr(0, len);
            draw_text(line, cx + 18.0f, line_y, 1.10f, glm::vec4(0.85f, 0.90f, 0.95f, 0.90f));
            line_y += 22.0f;
            if (len >= remaining.size()) break;
            remaining = remaining.substr(len + (remaining[len] == ' ' ? 1 : 0));
        }

        draw_rect(cx + 18.0f, start_y + card_h - 70.0f, card_w - 36.0f, 1.0f, glm::vec4(0.25f, 0.4f, 0.55f, 0.4f));
        draw_text(sec.bonus, cx + 18.0f, start_y + card_h - 52.0f, 1.3f, Typography::COLOR_GREEN);

        if (is_selected) {
            float badge_w = 210.0f;
            draw_rect(cx + card_w - badge_w - 12.0f, start_y + 12.0f, badge_w, 24.0f, glm::vec4(0.0f, 0.85f, 1.0f, 0.20f));
            draw_rect(cx + card_w - badge_w - 12.0f, start_y + 12.0f, badge_w, 1.0f, Typography::COLOR_CYAN);
            draw_rect(cx + card_w - badge_w - 12.0f, start_y + 35.0f, badge_w, 1.0f, Typography::COLOR_CYAN);
            draw_text("[ ACTIVE SECTOR ]", cx + card_w - badge_w - 4.0f, start_y + 16.0f, 1.15f, Typography::COLOR_CYAN);
        }
    }

    // Bottom Navigation Bar
    float bar_y = start_y + card_h + 16.0f;

    // Back Button
    float back_w = 160.0f;
    float back_h = 52.0f;
    float back_x = start_x;
    bool back_hov = (mouse_x >= back_x && mouse_x <= back_x + back_w &&
                     mouse_y >= bar_y && mouse_y <= bar_y + back_h);
    if (back_hov && mouse_clicked) {
        m_subview = MenuSubView::Main;
    }
    draw_rect(back_x, bar_y, back_w, back_h, back_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG);
    draw_rect(back_x, bar_y, back_w, 1.0f, back_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
    draw_rect(back_x, bar_y + back_h - 1.0f, back_w, 1.0f, back_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
    draw_rect(back_x, bar_y, 1.0f, back_h, back_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
    draw_rect(back_x + back_w - 1.0f, bar_y, 1.0f, back_h, back_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
    draw_text("< BACK", back_x + 40.0f, bar_y + 16.0f, 1.35f, back_hov ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

    // Prominent LAUNCH Button (Immediately starts game!)
    float launch_w = 420.0f;
    float launch_h = 52.0f;
    float launch_x = start_x + total_cards_w - launch_w;
    bool launch_hov = (mouse_x >= launch_x && mouse_x <= launch_x + launch_w &&
                      mouse_y >= bar_y && mouse_y <= bar_y + launch_h);

    if (launch_hov && mouse_clicked) {
        action = MainMenuAction::NewExpedition;
    }

    glm::vec4 launch_bg = launch_hov ? glm::vec4(0.08f, 0.32f, 0.18f, 0.95f) : glm::vec4(0.05f, 0.22f, 0.12f, 0.90f);
    draw_rect(launch_x, bar_y, launch_w, launch_h, launch_bg);
    glm::vec4 launch_border = launch_hov ? Typography::COLOR_GREEN : glm::vec4(0.18f, 0.80f, 0.44f, 0.50f);
    draw_rect(launch_x, bar_y, launch_w, launch_hov ? 3.0f : 2.0f, launch_border);
    draw_rect(launch_x, bar_y + launch_h - (launch_hov ? 3.0f : 2.0f), launch_w, launch_hov ? 3.0f : 2.0f, launch_border);
    draw_rect(launch_x, bar_y, launch_hov ? 4.0f : 2.0f, launch_h, launch_border);
    draw_rect(launch_x + launch_w - (launch_hov ? 4.0f : 2.0f), bar_y, launch_hov ? 4.0f : 2.0f, launch_h, launch_border);

    // Centered button text
    float text_scale = 1.75f;
    float text_w = FontRenderer::get_text_width("[ LAUNCH EXPEDITION ]", text_scale * 0.45f);
    float text_x = launch_x + (launch_w - text_w) * 0.5f;
    float text_y = bar_y + (launch_h - 32.0f * text_scale * 0.45f) * 0.5f;
    draw_text("[ LAUNCH EXPEDITION ]", text_x, text_y, text_scale, launch_hov ? glm::vec4(1.0f) : Typography::COLOR_GREEN);
}

void OrbitalHubUI::render_delver_roster(UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

    float card_w = std::min((w - 100.0f) / 3.0f, 380.0f);
    float card_h = h - 215.0f;
    float total_roster_w = card_w * 3.0f + 32.0f;
    float start_x = (w - total_roster_w) * 0.5f;
    float start_y = 148.0f;

    CharacterClass classes[3] = {
        CharacterClass::Demolitionist,
        CharacterClass::Vanguard,
        CharacterClass::Scout
    };

    for (int i = 0; i < 3; ++i) {
        float cx = start_x + i * (card_w + 16.0f);
        auto attr = get_character_attributes(classes[i]);
        bool is_selected = (profile.selected_class_id == i);
        bool is_hovered = (mouse_x >= cx && mouse_x <= cx + card_w &&
                           mouse_y >= start_y && mouse_y <= start_y + card_h);

        if (is_hovered && mouse_clicked) {
            profile.selected_class_id = i;
            m_profile_dirty = true;
        }

        glm::vec4 bg_col = is_selected ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) :
                           is_hovered  ? glm::vec4(0.06f, 0.12f, 0.18f, 0.90f) :
                                         glm::vec4(0.03f, 0.05f, 0.08f, 0.85f);
        draw_rect(cx, start_y, card_w, card_h, bg_col);

        glm::vec4 border_col = is_selected ? attr.primaryAccentColor :
                               is_hovered  ? glm::vec4(0.4f, 0.85f, 1.0f, 0.9f) :
                                             glm::vec4(0.2f, 0.35f, 0.45f, 0.5f);
        draw_rect(cx, start_y, card_w, is_selected ? 3.0f : 1.0f, border_col);
        draw_rect(cx, start_y + card_h - 3.0f, card_w, is_selected ? 3.0f : 1.0f, border_col);
        draw_rect(cx, start_y, is_selected ? 4.0f : 2.0f, card_h, border_col);
        draw_rect(cx + card_w - (is_selected ? 4.0f : 2.0f), start_y, is_selected ? 4.0f : 2.0f, card_h, border_col);

        // Header: Emblem & Delver Name
        std::string emblem = (i == 0) ? "[ DEMOLITIONIST ]" :
                             (i == 1) ? "[ VANGUARD ]" : "[ SCOUT ]";
        draw_text(emblem, cx + 16.0f, start_y + 16.0f, 1.3f, attr.primaryAccentColor);
        draw_text(attr.name, cx + 16.0f, start_y + 36.0f, 1.8f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        draw_text(attr.role, cx + 16.0f, start_y + 58.0f, 1.15f, glm::vec4(0.7f, 0.8f, 0.9f, 0.9f));
        draw_rect(cx + 16.0f, start_y + 74.0f, card_w - 32.0f, 1.0f, glm::vec4(0.25f, 0.4f, 0.55f, 0.5f));

        // Stat Bars Section
        float stat_y = start_y + 86.0f;
        draw_text("DELVER BASELINE SPECIFICATIONS:", cx + 16.0f, stat_y, 1.15f, glm::vec4(0.2f, 0.95f, 1.0f, 1.0f));

        struct StatBar {
            std::string label;
            std::string val_str;
            float ratio;
        };

        StatBar stats[4] = {
            {"Mining Speed", std::to_string(static_cast<int>(attr.baseMineSpeed * 100)) + "%", attr.baseMineSpeed / 1.5f},
            {"Armor Plating", std::to_string(static_cast<int>(attr.suitIntegrity)) + " HP", attr.suitIntegrity / 160.0f},
            {"Mobility Speed", std::to_string(static_cast<int>(attr.moveSpeed * 100)) + "%", attr.moveSpeed / 1.3f},
            {"Utility Rating", (i == 0) ? "Demolitions" : (i == 1) ? "Fortification" : "Surveying", 0.95f}
        };

        stat_y += 20.0f;
        for (int s = 0; s < 4; ++s) {
            draw_text(stats[s].label + ": " + stats[s].val_str, cx + 16.0f, stat_y, 1.1f, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
            stat_y += 14.0f;
            draw_rect(cx + 16.0f, stat_y, card_w - 32.0f, 6.0f, glm::vec4(0.1f, 0.15f, 0.2f, 0.8f));
            draw_rect(cx + 16.0f, stat_y, (card_w - 32.0f) * std::clamp(stats[s].ratio, 0.0f, 1.0f), 6.0f, attr.primaryAccentColor);
            stat_y += 14.0f;
        }

        // Traits & Ability Description
        draw_rect(cx + 16.0f, stat_y + 4.0f, card_w - 32.0f, 1.0f, glm::vec4(0.25f, 0.4f, 0.55f, 0.5f));
        stat_y += 14.0f;
        draw_text("TRAIT: " + attr.traitName, cx + 16.0f, stat_y, 1.2f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
        stat_y += 18.0f;

        // Wrapped trait description
        std::string desc = attr.traitDescription;
        std::vector<std::string> lines;
        std::string cur_line;
        for (char c : desc) {
            cur_line += c;
            if (cur_line.length() >= 34 && c == ' ') {
                lines.push_back(cur_line);
                cur_line.clear();
            }
        }
        if (!cur_line.empty()) lines.push_back(cur_line);

        for (const auto& l : lines) {
            draw_text(l, cx + 16.0f, stat_y, 1.05f, glm::vec4(0.75f, 0.8f, 0.85f, 0.9f));
            stat_y += 15.0f;
        }

        // Selection Action Button at card bottom
        float btn_h = 38.0f;
        float btn_y = start_y + card_h - btn_h - 16.0f;
        float btn_w = card_w - 32.0f;
        float btn_x = cx + 16.0f;

        glm::vec4 sel_col = is_selected ? glm::vec4(0.15f, 0.65f, 0.4f, 1.0f) :
                            is_hovered  ? glm::vec4(0.2f, 0.45f, 0.75f, 1.0f) :
                                          glm::vec4(0.1f, 0.2f, 0.3f, 0.85f);
        draw_rect(btn_x, btn_y, btn_w, btn_h, sel_col);
        draw_rect(btn_x, btn_y, btn_w, 1.0f, is_selected ? glm::vec4(0.4f, 1.0f, 0.6f, 1.0f) : border_col);

        std::string btn_txt = is_selected ? "[ ACTIVE DELVER SELECTED ]" :
                              is_hovered  ? "[ CLICK TO SELECT DELVER ]" : "[ SELECT DELVER ]";
        float txt_len = static_cast<float>(btn_txt.length()) * 8.0f * 1.15f;
        draw_text(btn_txt, btn_x + (btn_w - txt_len) * 0.5f, btn_y + 12.0f, 1.15f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
}

void OrbitalHubUI::render_upgrade_terminal(UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float cx = w / 2.0f;

    float terminal_w = std::min(w * 0.88f, 1080.0f);
    float start_x = (w - terminal_w) * 0.5f;
    float start_y = 142.0f;

    // Pulse feedback overlay
    if (m_purchase_pulse_timer > 0.0f) {
        float pulse_alpha = m_purchase_pulse_timer * 0.35f;
        draw_rect(0, 0, w, 4.0f, glm::vec4(1.0f, 0.75f, 0.1f, pulse_alpha * 2.0f));
        draw_rect(0, h - 4.0f, w, 4.0f, glm::vec4(1.0f, 0.75f, 0.1f, pulse_alpha * 2.0f));
        m_purchase_pulse_timer = std::max(0.0f, m_purchase_pulse_timer - 0.016f);
    }

    // Top Balance & Respec Strip
    float top_h = 44.0f;
    draw_rect(start_x, start_y, terminal_w, top_h, glm::vec4(0.04f, 0.07f, 0.11f, 0.95f));
    draw_rect(start_x, start_y, terminal_w, 1.0f, glm::vec4(0.2f, 0.45f, 0.65f, 0.6f));

    std::string bal_str = "STOCKPILED: " + std::to_string(profile.total_exp) + " EXP  |  " +
                          std::to_string(profile.total_voidite) + " VOIDITE  |  " +
                          std::to_string(profile.total_titanium) + " TITANIUM";
    draw_text(bal_str, start_x + 18.0f, start_y + 14.0f, 1.35f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    // Respec Button
    float respec_w = 230.0f;
    float respec_h = 30.0f;
    float respec_x = start_x + terminal_w - respec_w - 12.0f;
    float respec_y = start_y + 7.0f;
    bool respec_hov = (mouse_x >= respec_x && mouse_x <= respec_x + respec_w &&
                       mouse_y >= respec_y && mouse_y <= respec_y + respec_h);

    if (respec_hov && mouse_clicked) {
        int refunded = 0;
        profile.upgrades.respec(refunded);
        profile.total_exp += refunded;
        m_profile_dirty = true;
        m_purchase_pulse_timer = 1.0f;
        m_terminal_msg = "RESPEC COMPLETE: REFUNDED " + std::to_string(refunded) + " EXP (85% RECOVERY)";
        m_terminal_msg_col = glm::vec4(1.0f, 0.85f, 0.2f, 1.0f);
    }

    glm::vec4 respec_bg = respec_hov ? glm::vec4(0.6f, 0.2f, 0.2f, 1.0f) : glm::vec4(0.35f, 0.12f, 0.15f, 0.9f);
    draw_rect(respec_x, respec_y, respec_w, respec_h, respec_bg);
    draw_rect(respec_x, respec_y, respec_w, 1.0f, glm::vec4(1.0f, 0.4f, 0.4f, 0.8f));
    draw_text("[RESPEC UPGRADES (-15% EXP)]", respec_x + 8.0f, respec_y + 8.0f, 1.15f, glm::vec4(1.0f, 0.9f, 0.9f, 1.0f));

    // Terminal Status Feedback Line
    if (!m_terminal_msg.empty()) {
        draw_text(">> " + m_terminal_msg, start_x + 18.0f, start_y + top_h + 8.0f, 1.2f, m_terminal_msg_col);
    }

    // 6 Upgrade Nodes (2 Columns of 3 Nodes)
    float grid_y = start_y + top_h + 30.0f;
    float col_w = (terminal_w - 20.0f) * 0.5f;
    float row_h = 102.0f;
    float row_gap = 14.0f;

    UpgradeType types[6] = {
        UpgradeType::DrillSpeed,
        UpgradeType::DrillDurability,
        UpgradeType::ThrusterTank,
        UpgradeType::KineticDynamo,
        UpgradeType::SonarFrequency,
        UpgradeType::ReinforcedPlating
    };

    for (int idx = 0; idx < 6; ++idx) {
        UpgradeType type = types[idx];
        int col = idx / 3;
        int row = idx % 3;

        float ux = start_x + col * (col_w + 20.0f);
        float uy = grid_y + row * (row_h + row_gap);

        auto info = UpgradeTree::get_info(type);
        int cur_tier = profile.upgrades.get_tier(type);
        bool is_maxed = (cur_tier >= UpgradeTree::MAX_TIER);

        int exp_cost = UpgradeTree::get_exp_cost(cur_tier);
        int void_cost = UpgradeTree::get_voidite_cost(cur_tier);
        int tit_cost = UpgradeTree::get_titanium_cost(cur_tier);
        bool can_buy = profile.upgrades.can_purchase(type, profile.total_exp, profile.total_voidite, profile.total_titanium);

        draw_rect(ux, uy, col_w, row_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.92f));
        draw_rect(ux, uy, 4.0f, row_h, is_maxed ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.2f, 0.85f, 1.0f, 0.9f));
        draw_rect(ux, uy, col_w, 1.0f, glm::vec4(0.18f, 0.3f, 0.4f, 0.5f));

        // Title and Category
        draw_text(info.category + " // " + info.name, ux + 14.0f, uy + 10.0f, 1.25f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
        draw_text(info.description, ux + 14.0f, uy + 28.0f, 1.05f, glm::vec4(0.7f, 0.75f, 0.8f, 0.85f));

        // Progress Pips [ ■ ■ ■ □ □ ]
        float pip_start_x = ux + 14.0f;
        float pip_y = uy + 48.0f;
        float pip_size = 14.0f;
        float pip_gap = 5.0f;

        for (int p = 0; p < UpgradeTree::MAX_TIER; ++p) {
            float px = pip_start_x + p * (pip_size + pip_gap);
            bool filled = (p < cur_tier);
            glm::vec4 pip_col = filled ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.15f, 0.20f, 0.25f, 0.7f);
            draw_rect(px, pip_y, pip_size, pip_size, pip_col);
            draw_rect(px, pip_y, pip_size, 1.0f, glm::vec4(0.3f, 0.5f, 0.6f, 0.8f));
        }

        std::string tier_text = "TIER " + std::to_string(cur_tier) + "/" + std::to_string(UpgradeTree::MAX_TIER);
        draw_text(tier_text, pip_start_x + UpgradeTree::MAX_TIER * (pip_size + pip_gap) + 8.0f, pip_y + 2.0f, 1.15f,
                  is_maxed ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));

        // Stat Delta Preview
        std::string delta_preview = profile.upgrades.get_stat_preview(type);
        draw_text("Effect: " + delta_preview, ux + 14.0f, uy + 72.0f, 1.15f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

        // Purchase Button
        float btn_w = 170.0f;
        float btn_h = 36.0f;
        float bx = ux + col_w - btn_w - 12.0f;
        float by = uy + 52.0f;

        bool btn_hov = (mouse_x >= bx && mouse_x <= bx + btn_w && mouse_y >= by && mouse_y <= by + btn_h);

        if (btn_hov && mouse_clicked && can_buy && !is_maxed) {
            profile.upgrades.purchase(type, profile.total_exp, profile.total_voidite, profile.total_titanium);
            m_profile_dirty = true;
            m_purchase_pulse_timer = 1.0f;
            m_terminal_msg = "UPGRADE ACQUIRED: " + info.name + " [TIER " + std::to_string(cur_tier + 1) + "]";
            m_terminal_msg_col = glm::vec4(0.2f, 0.95f, 0.4f, 1.0f);
        }

        glm::vec4 b_bg = is_maxed ? glm::vec4(0.1f, 0.14f, 0.18f, 0.5f) :
                         can_buy  ? (btn_hov ? glm::vec4(0.2f, 0.65f, 0.95f, 1.0f) : glm::vec4(0.12f, 0.4f, 0.7f, 0.9f)) :
                                    glm::vec4(0.15f, 0.18f, 0.22f, 0.6f);
        draw_rect(bx, by, btn_w, btn_h, b_bg);
        if (can_buy && !is_maxed) {
            draw_rect(bx, by, btn_w, 1.0f, glm::vec4(0.4f, 0.85f, 1.0f, 0.8f));
        }

        if (is_maxed) {
            draw_text("MAX TIER", bx + 45.0f, by + 12.0f, 1.2f, glm::vec4(0.5f, 0.55f, 0.6f, 0.8f));
        } else {
            std::string cost_str = std::to_string(exp_cost) + " EXP";
            std::string mat_str = std::to_string(void_cost) + "V " + std::to_string(tit_cost) + "T";
            draw_text("UPGRADE: " + cost_str, bx + 12.0f, by + 6.0f, 1.1f,
                      can_buy ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.6f, 0.8f));
            draw_text(mat_str, bx + 12.0f, by + 20.0f, 1.0f,
                      can_buy ? glm::vec4(0.0f, 0.94f, 1.0f, 1.0f) : glm::vec4(0.45f, 0.45f, 0.45f, 0.7f));
        }
    }
}

bool OrbitalHubUI::render_orbital_hub(int selected_level, const SkillMatrix& skills, const PlayerInventory& inventory, float mouse_x, float mouse_y, bool mouse_clicked) {
    for (int s = 1; s <= 3; ++s) {
        s_fallback_profile.sector_records[s] = inventory.sector_records[s];
    }
    return render_orbital_hub(selected_level, skills, inventory, s_fallback_profile, mouse_x, mouse_y, mouse_clicked);
}

bool OrbitalHubUI::render_orbital_hub(int selected_level, const SkillMatrix& skills, const PlayerInventory& inventory, UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float cx = w / 2.0f;

    draw_rect(0, 0, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.96f));

    // Header
    float back_btn_w = 160.0f;
    float back_btn_h = 28.0f;
    float back_btn_x = 40.0f;
    float back_btn_y = 22.0f;
    bool back_hovered = (mouse_x >= back_btn_x && mouse_x <= back_btn_x + back_btn_w &&
                         mouse_y >= back_btn_y && mouse_y <= back_btn_y + back_btn_h);
    if (back_hovered && mouse_clicked) {
        m_return_to_title = true;
    }

    glm::vec4 back_bg = back_hovered ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG;
    draw_rect(back_btn_x, back_btn_y, back_btn_w, back_btn_h, back_bg);
    glm::vec4 back_border = back_hovered ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f);
    draw_rect(back_btn_x, back_btn_y, back_btn_w, 1.0f, back_border);
    draw_rect(back_btn_x, back_btn_y + back_btn_h - 1.0f, back_btn_w, 1.0f, back_border);
    draw_rect(back_btn_x, back_btn_y, back_hovered ? 3.0f : 1.0f, back_btn_h, back_border);
    draw_rect(back_btn_x + back_btn_w - 1.0f, back_btn_y, 1.0f, back_btn_h, back_border);
    draw_text("< TITLE SCREEN", back_btn_x + 16.0f, back_btn_y + 8.0f, 1.15f, back_hovered ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

    draw_text("ORBITAL HUB // EXPEDITION STAGING & UPGRADE TERMINAL", back_btn_x + back_btn_w + 20.0f, 26.0f, 1.75f, Typography::COLOR_CYAN);
    draw_rect(40.0f, 56.0f, w - 80.0f, 2.0f, glm::vec4(0.15f, 0.85f, 1.0f, 0.6f));

    // Navigation Tabs Header
    float tab_y = 66.0f;
    float tab_h = 32.0f;
    float tab_w = 230.0f;
    float tab_gap = 12.0f;
    float tabs_total_w = 3.0f * tab_w + 2.0f * tab_gap;
    float tab_start_x = cx - tabs_total_w * 0.5f;

    const char* tab_names[3] = {
        "[ 1. SECTOR BRIEFING ]",
        "[ 2. SELECT DELVER ]",
        "[ 3. UPGRADE TERMINAL ]"
    };

    for (int t = 0; t < 3; ++t) {
        float tx = tab_start_x + t * (tab_w + tab_gap);
        bool is_cur = (static_cast<int>(m_active_tab) == t);
        bool is_hov = (mouse_x >= tx && mouse_x <= tx + tab_w && mouse_y >= tab_y && mouse_y <= tab_y + tab_h);

        if (is_hov && mouse_clicked) {
            m_active_tab = static_cast<HubTab>(t);
        }

        glm::vec4 bg_col = is_cur ? glm::vec4(0.14f, 0.40f, 0.60f, 0.95f) :
                           is_hov ? glm::vec4(0.08f, 0.22f, 0.35f, 0.85f) :
                                    glm::vec4(0.04f, 0.08f, 0.12f, 0.75f);
        draw_rect(tx, tab_y, tab_w, tab_h, bg_col);

        glm::vec4 border_col = is_cur ? glm::vec4(0.2f, 0.95f, 1.0f, 1.0f) :
                               is_hov ? glm::vec4(0.5f, 0.85f, 1.0f, 0.8f) :
                                        glm::vec4(0.2f, 0.35f, 0.45f, 0.5f);
        draw_rect(tx, tab_y, tab_w, is_cur ? 2.0f : 1.0f, border_col);
        draw_rect(tx, tab_y + tab_h - 2.0f, tab_w, 2.0f, border_col);
        draw_rect(tx, tab_y, 2.0f, tab_h, border_col);
        draw_rect(tx + tab_w - 2.0f, tab_y, 2.0f, tab_h, border_col);

        float text_len = static_cast<float>(std::string(tab_names[t]).length()) * 8.0f * 1.15f;
        draw_text(tab_names[t], tx + (tab_w - text_len) * 0.5f, tab_y + 9.0f, 1.15f,
                  is_cur ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : is_hov ? glm::vec4(0.85f, 0.95f, 1.0f, 0.9f) : glm::vec4(0.6f, 0.7f, 0.8f, 0.8f));
    }

    bool launch_triggered = false;

    if (m_active_tab == HubTab::DelverRoster) {
        render_delver_roster(profile, mouse_x, mouse_y, mouse_clicked);
    } else if (m_active_tab == HubTab::UpgradeTerminal) {
        render_upgrade_terminal(profile, mouse_x, mouse_y, mouse_clicked);
    } else {
        // Tab 0: Sector Intel & Skill Matrix
        float col1_x = 40.0f;
        float col1_w = (w - 110.0f) * 0.45f;
        float col_y = 112.0f;
        float col_h = h - 190.0f;

        draw_rect(col1_x, col_y, col1_w, col_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.88f));
        draw_rect(col1_x, col_y, 4.0f, col_h, glm::vec4(0.2f, 0.85f, 1.0f, 0.95f));

        draw_text("EXPEDITION SECTOR BRIEFING", col1_x + 20.0f, col_y + 16.0f, 1.5f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
        draw_rect(col1_x + 20.0f, col_y + 36.0f, col1_w - 40.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));

        std::string sec_name = (selected_level == 1) ? "SECTOR 1: CRYSTALLINE CAVERNS" :
                               (selected_level == 2) ? "SECTOR 2: SUBTERRANEAN VAULT" :
                                                       "SECTOR 3: FAULT-LINE COLLAPSE";
        draw_text(sec_name, col1_x + 20.0f, col_y + 48.0f, 1.35f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

        std::string sec_badge = profile.sector_records[selected_level].best_badge;
        int sec_rate = profile.sector_records[selected_level].highest_completion_rate;
        std::string rec_str = "RECORD: [" + sec_badge + "] - BEST RATING: " + std::to_string(sec_rate) + "%";
        glm::vec4 rec_col = (sec_badge == "CLEARED (100%)" || sec_badge == "SECTOR CLEARED (100%)") ? glm::vec4(0.2f, 1.0f, 0.4f, 1.0f) :
                            (sec_badge.find("PARTIAL") != std::string::npos) ? glm::vec4(1.0f, 0.75f, 0.0f, 1.0f) :
                            (sec_badge.find("ABANDONED") != std::string::npos || sec_badge == "EXPEDITION ABANDONED") ? glm::vec4(1.0f, 0.35f, 0.35f, 1.0f) :
                                                                   glm::vec4(0.5f, 0.7f, 0.8f, 0.85f);
        draw_text(rec_str, col1_x + 20.0f, col_y + 68.0f, 1.15f, rec_col);

        if (selected_level == 1) {
            draw_text("Target Depth: 120m | Crust Stability: 85%", col1_x + 20.0f, col_y + 90.0f, 1.2f, glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
            draw_text("Tactical: ANY DELVER (Standard Cavern Survey)", col1_x + 20.0f, col_y + 110.0f, 1.15f, glm::vec4(0.2f, 0.95f, 0.4f, 1.0f));
            draw_text("Directives:", col1_x + 20.0f, col_y + 134.0f, 1.3f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
            draw_text("  1. Mine 25 Voidite Crystals using Subterranean Drill [LMB]", col1_x + 20.0f, col_y + 156.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            draw_text("  2. Use Sonar Pulse [Q] to locate rich mineral veins", col1_x + 20.0f, col_y + 178.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            draw_text("  3. Deploy Extraction Beacon [B] when quota met", col1_x + 20.0f, col_y + 200.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            draw_text("  4. Defend zone for 40s until evacuation landing pod arrives", col1_x + 20.0f, col_y + 222.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        } else if (selected_level == 2) {
            draw_text("Target Depth: 340m | Crust Stability: 65%", col1_x + 20.0f, col_y + 90.0f, 1.2f, glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
            draw_text("Tactical: DEMOLITIONIST (Vault Breach Specialist)", col1_x + 20.0f, col_y + 110.0f, 1.15f, glm::vec4(1.0f, 0.65f, 0.2f, 1.0f));
            draw_text("Directives:", col1_x + 20.0f, col_y + 134.0f, 1.3f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
            draw_text("  1. Track subterranean vault signatures using Sonar [Q]", col1_x + 20.0f, col_y + 156.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            draw_text("  2. Equip Demolition Charges [3] to blast Reinforced Doors", col1_x + 20.0f, col_y + 178.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            draw_text("  3. Breach the vault chamber and extract the Hyper-Core Relic", col1_x + 20.0f, col_y + 200.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            draw_text("  4. Call beacon [B] and extract all salvaged minerals", col1_x + 20.0f, col_y + 222.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        } else {
            draw_text("Target Depth: 600m | Crust Stability: CRITICAL (Collapse Imminent)", col1_x + 20.0f, col_y + 90.0f, 1.2f, glm::vec4(1.0f, 0.3f, 0.3f, 0.9f));
            draw_text("Tactical: VANGUARD (Fault-Line Seismic Defense)", col1_x + 20.0f, col_y + 110.0f, 1.15f, glm::vec4(0.4f, 0.85f, 1.0f, 1.0f));
            draw_text("Directives:", col1_x + 20.0f, col_y + 134.0f, 1.3f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
            draw_text("  1. 180-SECOND HARD COUNTDOWN: Subterranean collapse timer active", col1_x + 20.0f, col_y + 156.0f, 1.15f, glm::vec4(1.0f, 0.35f, 0.2f, 0.95f));
            draw_text("  2. Mine 50 Voidite Crystals while surviving recurring cave-ins", col1_x + 20.0f, col_y + 178.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            draw_text("  3. Reach the emergency drop pod before seismic fault collapse", col1_x + 20.0f, col_y + 200.0f, 1.15f, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        }

        // Right Column: Skill Matrix & Proficiency Branches
        float col2_x = col1_x + col1_w + 30.0f;
        float col2_w = (w - 110.0f) * 0.55f;

        draw_rect(col2_x, col_y, col2_w, col_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.88f));
        draw_rect(col2_x, col_y, 4.0f, col_h, glm::vec4(1.0f, 0.75f, 0.2f, 0.95f));

        draw_text("DELVER PROFICIENCY MATRICES", col2_x + 20.0f, col_y + 16.0f, 1.5f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
        draw_rect(col2_x + 20.0f, col_y + 36.0f, col2_w - 40.0f, 1.0f, glm::vec4(0.4f, 0.35f, 0.2f, 0.5f));

        const SkillBranch* branches[4] = {
            &skills.demolitions,
            &skills.surveying,
            &skills.suit,
            &skills.acrobatics
        };

        float branch_y = col_y + 48.0f;
        for (int i = 0; i < 4; ++i) {
            const auto& b = *branches[i];
            draw_text(b.name + " (" + std::to_string(b.xp) + "/" + std::to_string(b.xp_for_unlock) + " XP)",
                      col2_x + 20.0f, branch_y, 1.3f, b.unlocked ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.85f, 0.9f, 0.95f, 1.0f));

            draw_rect(col2_x + 20.0f, branch_y + 18.0f, col2_w - 40.0f, 8.0f, glm::vec4(0.12f, 0.15f, 0.18f, 0.8f));
            glm::vec4 bar_col = b.unlocked ? glm::vec4(0.2f, 0.9f, 0.35f, 0.95f) : glm::vec4(0.2f, 0.75f, 1.0f, 0.95f);
            draw_rect(col2_x + 20.0f, branch_y + 18.0f, (col2_w - 40.0f) * b.progress(), 8.0f, bar_col);

            std::string perk_status = b.unlocked ? "[UNLOCKED] " : "[LOCKED] ";
            std::string perk_line = perk_status + b.unlock_perk_name + ": " + b.unlock_description;
            draw_text(perk_line, col2_x + 20.0f, branch_y + 32.0f, 1.1f, b.unlocked ? glm::vec4(0.3f, 0.9f, 0.5f, 0.95f) : glm::vec4(0.6f, 0.65f, 0.7f, 0.8f));

            branch_y += 76.0f;
        }

        // Launch expedition banner
        float btn_y = h - 60.0f;
        float btn_w = w - 80.0f;
        float btn_x = 40.0f;
        float btn_h = 42.0f;

        bool btn_hovered = (mouse_x >= btn_x && mouse_x <= btn_x + btn_w &&
                            mouse_y >= btn_y && mouse_y <= btn_y + btn_h);
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
                  cx - 360.0f, btn_y + 13.0f, 1.35f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return launch_triggered;
}

DebriefAction OrbitalHubUI::render_debrief(bool success, int level, PlayerInventory& inventory, const SkillMatrix& skills, float mouse_x, float mouse_y, bool mouse_clicked) {
    for (int s = 1; s <= 3; ++s) {
        s_fallback_profile.sector_records[s] = inventory.sector_records[s];
    }
    return render_debrief(success, level, inventory, skills, s_fallback_profile, mouse_x, mouse_y, mouse_clicked);
}

DebriefAction OrbitalHubUI::render_debrief(bool success, int level, PlayerInventory& inventory, const SkillMatrix& skills, UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked) {
    // Bank run mission rewards into user profile once
    if (!inventory.has_banked_run) {
        inventory.has_banked_run = true;
        int run_exp = inventory.total_run_score;
        if (run_exp <= 0) {
            run_exp = inventory.voidite * 5 + inventory.titanium * 6 + inventory.salvage_parts * 2;
        }
        profile.total_exp += run_exp;
        profile.total_voidite += inventory.voidite;
        profile.total_titanium += inventory.titanium;

        for (int s = 1; s <= 3; ++s) {
            if (inventory.sector_records[s].highest_completion_rate > profile.sector_records[s].highest_completion_rate) {
                profile.sector_records[s].highest_completion_rate = inventory.sector_records[s].highest_completion_rate;
            }
            if (inventory.sector_records[s].best_badge != "UNEXPLORED") {
                profile.sector_records[s].best_badge = inventory.sector_records[s].best_badge;
            }
        }
        m_profile_dirty = true;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float cx = w / 2.0f;

    draw_rect(0, 0, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.96f));

    // Header banner
    float banner_y = 30.0f;
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
    float panel_y = banner_y + 48.0f;
    float panel_h = 450.0f;

    // LEFT COLUMN: SUBTERRANEAN EXPEDITION SUMMARY
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

    stat_y += 28.0f;
    std::string t_str = "Titanium Cores:        " + std::to_string(inventory.titanium) + " cores (" + std::to_string(inventory.titanium * 6) + " pts)";
    draw_text(t_str, col1_x + 20.0f, stat_y, 1.3f, glm::vec4(1.0f, 0.70f, 0.0f, 1.0f));

    stat_y += 28.0f;
    std::string sc_str = "Scrap Metal Salvaged:  " + std::to_string(inventory.scrap_metal) + " scrap";
    draw_text(sc_str, col1_x + 20.0f, stat_y, 1.3f, glm::vec4(0.85f, 0.85f, 0.9f, 1.0f));

    stat_y += 28.0f;
    std::string s_str = "Mineral Salvage:       " + std::to_string(inventory.salvage_parts) + " units (" + std::to_string(inventory.salvage_parts * 2) + " pts)";
    draw_text(s_str, col1_x + 20.0f, stat_y, 1.3f, glm::vec4(0.7f, 0.75f, 0.8f, 1.0f));

    if (level == 2) {
        stat_y += 28.0f;
        std::string r_str = inventory.relic_extracted ? "Hyper-Core Relic:      RECOVERED (+250 pts)" : "Hyper-Core Relic:      NOT RECOVERED";
        draw_text(r_str, col1_x + 20.0f, stat_y, 1.3f, inventory.relic_extracted ? glm::vec4(1.0f, 0.0f, 0.85f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.6f, 0.8f));
    }

    stat_y += 34.0f;
    draw_rect(col1_x + 20.0f, stat_y - 6.0f, col_w - 40.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));
    std::string score_str = "TOTAL SCORE: " + std::to_string(inventory.total_run_score) + " POINTS";
    draw_text(score_str, col1_x + 20.0f, stat_y, 1.5f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    stat_y += 38.0f;
    draw_text("DELVER SPECIALIZATIONS XP:", col1_x + 20.0f, stat_y, 1.25f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    stat_y += 22.0f;
    draw_text("Demolitions: " + std::to_string(skills.demolitions.xp) + " XP" + (skills.demolitions.unlocked ? " [ACTIVE]" : ""),
              col1_x + 20.0f, stat_y, 1.15f, skills.demolitions.unlocked ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
    stat_y += 18.0f;
    draw_text("Surveying:   " + std::to_string(skills.surveying.xp) + " XP" + (skills.surveying.unlocked ? " [ACTIVE]" : ""),
              col1_x + 20.0f, stat_y, 1.15f, skills.surveying.unlocked ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));

    // RIGHT COLUMN: PERSISTENT HUB UPGRADES TERMINAL
    draw_rect(col2_x, panel_y, col_w, panel_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.92f));
    draw_rect(col2_x, panel_y, 4.0f, panel_h, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    draw_rect(col2_x, panel_y, col_w, 1.0f, glm::vec4(0.2f, 0.35f, 0.45f, 0.6f));

    draw_text("PERSISTENT ACCOUNT POOL", col2_x + 20.0f, panel_y + 14.0f, 1.45f, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    draw_rect(col2_x + 20.0f, panel_y + 36.0f, col_w - 40.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));

    std::string pool_str = "TOTAL: " + std::to_string(profile.total_exp) + " EXP | " +
                           std::to_string(profile.total_voidite) + " VOID | " +
                           std::to_string(profile.total_titanium) + " TITAN";
    draw_text(pool_str, col2_x + 20.0f, panel_y + 44.0f, 1.2f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    // Show 4 Quick Upgrades in Debrief
    UpgradeType quick_types[4] = {
        UpgradeType::DrillSpeed,
        UpgradeType::ThrusterTank,
        UpgradeType::SonarFrequency,
        UpgradeType::ReinforcedPlating
    };

    float up_y = panel_y + 68.0f;
    float up_h = 60.0f;
    float up_w = col_w - 40.0f;

    for (int i = 0; i < 4; ++i) {
        UpgradeType type = quick_types[i];
        auto info = UpgradeTree::get_info(type);
        int cur_tier = profile.upgrades.get_tier(type);
        bool is_maxed = (cur_tier >= UpgradeTree::MAX_TIER);

        int exp_cost = UpgradeTree::get_exp_cost(cur_tier);
        int void_cost = UpgradeTree::get_voidite_cost(cur_tier);
        int tit_cost = UpgradeTree::get_titanium_cost(cur_tier);
        bool can_buy = profile.upgrades.can_purchase(type, profile.total_exp, profile.total_voidite, profile.total_titanium);

        float ux = col2_x + 20.0f;
        float uy = up_y + i * (up_h + 8.0f);

        draw_rect(ux, uy, up_w, up_h, glm::vec4(0.06f, 0.08f, 0.12f, 0.85f));
        draw_rect(ux, uy, up_w, 1.0f, glm::vec4(0.15f, 0.25f, 0.35f, 0.5f));

        std::string title = info.name + " [LVL " + std::to_string(cur_tier) + "/5]";
        draw_text(title, ux + 10.0f, uy + 8.0f, 1.15f, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
        draw_text(profile.upgrades.get_stat_preview(type), ux + 10.0f, uy + 26.0f, 1.05f, glm::vec4(0.65f, 0.75f, 0.85f, 0.85f));

        // Upgrade button
        float btn_w = 135.0f;
        float btn_h = 34.0f;
        float bx = ux + up_w - btn_w - 10.0f;
        float by = uy + 12.0f;

        bool btn_hov = (mouse_x >= bx && mouse_x <= bx + btn_w && mouse_y >= by && mouse_y <= by + btn_h);

        if (btn_hov && mouse_clicked && can_buy && !is_maxed) {
            profile.upgrades.purchase(type, profile.total_exp, profile.total_voidite, profile.total_titanium);
            m_profile_dirty = true;
        }

        glm::vec4 b_bg = is_maxed ? glm::vec4(0.1f, 0.12f, 0.15f, 0.5f) :
                         can_buy  ? (btn_hov ? glm::vec4(0.2f, 0.6f, 0.85f, 1.0f) : glm::vec4(0.1f, 0.35f, 0.6f, 0.9f)) :
                                    glm::vec4(0.1f, 0.12f, 0.15f, 0.6f);
        draw_rect(bx, by, btn_w, btn_h, b_bg);
        if (can_buy && !is_maxed) {
            draw_rect(bx, by, btn_w, 1.0f, glm::vec4(0.4f, 0.85f, 1.0f, 0.8f));
        }

        if (is_maxed) {
            draw_text("MAXED", bx + 36.0f, by + 10.0f, 1.1f, glm::vec4(0.5f, 0.5f, 0.5f, 0.7f));
        } else {
            std::string cost_str = std::to_string(exp_cost) + " EXP";
            draw_text("UPGRADE", bx + 14.0f, by + 5.0f, 1.1f, can_buy ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.5f, 0.5f, 0.5f, 0.7f));
            draw_text(cost_str, bx + 14.0f, by + 18.0f, 1.0f, can_buy ? glm::vec4(0.0f, 0.94f, 1.0f, 1.0f) : glm::vec4(0.45f, 0.45f, 0.45f, 0.7f));
        }
    }

    // BOTTOM BUTTONS: LAUNCH NEXT SECTOR & RETURN TO HUB
    DebriefAction result = DebriefAction::None;
    float bot_y = panel_y + panel_h + 14.0f;
    float bot_btn_w = (total_content_w - 20.0f) * 0.5f;
    float bot_btn_h = 44.0f;

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
    draw_text("[LAUNCH NEXT SECTOR]", b1_x + bot_btn_w * 0.5f - 100.0f, bot_y + 14.0f, 1.35f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

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
    draw_text("[RETURN TO ORBITAL HUB]", b2_x + bot_btn_w * 0.5f - 110.0f, bot_y + 14.0f, 1.35f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return result;
}

} // namespace Voidfall
