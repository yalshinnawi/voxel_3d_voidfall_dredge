#include "orbital_hub.hpp"
#include "font_renderer.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <font8x8.h>
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
    m_text_shader.set_vec2("uShadowOffset", glm::vec2(0.0f, 0.0f));
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

void OrbitalHubUI::draw_text_centered(const std::string& text, float box_x, float box_y, float box_w, float box_h, float scale, const glm::vec4& color) {
    if (text.empty()) return;
    float text_w = FontRenderer::get_rendered_width(text, scale);
    float text_h = FontRenderer::get_rendered_height(scale);
    float tx = box_x + (box_w - text_w) * 0.5f;
    float ty = box_y + (box_h - text_h) * 0.5f;
    draw_text(text, tx, ty, scale, color);
}

void OrbitalHubUI::draw_text_fitted(const std::string& text, float x, float y, float max_w, float base_scale, const glm::vec4& color, float min_scale) {
    if (text.empty()) return;
    float scale = FontRenderer::fit_scale(text, max_w, base_scale, min_scale);
    draw_text(text, x, y, scale, color);
}

void OrbitalHubUI::draw_text_centered_fitted(const std::string& text, float box_x, float box_y, float box_w, float box_h, float base_scale, const glm::vec4& color, float min_scale) {
    if (text.empty()) return;
    float pad = 12.0f;
    float scale = FontRenderer::fit_scale(text, std::max(20.0f, box_w - pad * 2.0f), base_scale, min_scale);
    draw_text_centered(text, box_x, box_y, box_w, box_h, scale, color);
}

void OrbitalHubUI::draw_panel_with_border(float x, float y, float w, float h, const glm::vec4& bg_col, const glm::vec4& border_col, float border_width) {
    draw_rect(x, y, w, h, bg_col);
    if (border_width > 0.0f) {
        draw_rect(x, y, w, border_width, border_col);
        draw_rect(x, y + h - border_width, w, border_width, border_col);
        draw_rect(x, y, border_width, h, border_col);
        draw_rect(x + w - border_width, y, border_width, h, border_col);
    }
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
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    MainMenuAction action = MainMenuAction::None;

    if (m_subview == MenuSubView::SectorSelect) {
        render_sector_select_carousel(selected_level, profile, mouse_x, mouse_y, mouse_clicked, action);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        return action;
    } else if (m_subview == MenuSubView::Settings) {
        render_audio_settings(profile, mouse_x, mouse_y, mouse_clicked);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        return action;
    } else if (m_subview == MenuSubView::Upgrades) {
        // Dark backdrop for upgrades
        draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.95f));

        // Back button
        float back_w = std::clamp(150.0f * ui_scale, 110.0f, 170.0f);
        float back_h = 38.0f * ui_scale;
        float back_x = 24.0f * ui_scale;
        float back_y = 16.0f * ui_scale;
        bool back_hover = (mouse_x >= back_x && mouse_x <= back_x + back_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (back_hover && mouse_clicked) {
            m_subview = MenuSubView::Main;
        }
        draw_panel_with_border(back_x, back_y, back_w, back_h,
                               back_hover ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG,
                               back_hover ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f),
                               back_hover ? 2.0f : 1.0f);
        draw_text_centered_fitted("< MAIN MENU", back_x, back_y, back_w, back_h, 1.20f * ui_scale, back_hover ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

        // Subtabs for Delvers, Upgrades, and Audio Settings
        float tab_gap = 10.0f * ui_scale;
        float tab1_x = back_x + back_w + 16.0f * ui_scale;
        float avail_tab_w = w - tab1_x - 24.0f * ui_scale;
        float tab_w = std::clamp((avail_tab_w - 2.0f * tab_gap) / 3.0f, 120.0f, 220.0f * ui_scale);

        bool t1_hov = (mouse_x >= tab1_x && mouse_x <= tab1_x + tab_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (t1_hov && mouse_clicked) m_active_tab = HubTab::DelverRoster;
        bool t1_act = (m_active_tab == HubTab::DelverRoster);
        draw_panel_with_border(tab1_x, back_y, tab_w, back_h,
                               t1_act ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) : (t1_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                               t1_act ? Typography::COLOR_CYAN : glm::vec4(0.3f, 0.4f, 0.5f, 0.4f),
                               t1_act ? 2.0f : 1.0f);
        draw_text_centered_fitted("[ DELVER ROSTER ]", tab1_x, back_y, tab_w, back_h, 1.10f * ui_scale, t1_act ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

        float tab2_x = tab1_x + tab_w + tab_gap;
        bool t2_hov = (mouse_x >= tab2_x && mouse_x <= tab2_x + tab_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (t2_hov && mouse_clicked) m_active_tab = HubTab::UpgradeTerminal;
        bool t2_act = (m_active_tab == HubTab::UpgradeTerminal);
        draw_panel_with_border(tab2_x, back_y, tab_w, back_h,
                               t2_act ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) : (t2_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                               t2_act ? Typography::COLOR_CYAN : glm::vec4(0.3f, 0.4f, 0.5f, 0.4f),
                               t2_act ? 2.0f : 1.0f);
        draw_text_centered_fitted("[ UPGRADES ]", tab2_x, back_y, tab_w, back_h, 1.10f * ui_scale, t2_act ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

        float tab3_x = tab2_x + tab_w + tab_gap;
        bool t3_hov = (mouse_x >= tab3_x && mouse_x <= tab3_x + tab_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (t3_hov && mouse_clicked) m_active_tab = HubTab::AudioSettings;
        bool t3_act = (m_active_tab == HubTab::AudioSettings);
        draw_panel_with_border(tab3_x, back_y, tab_w, back_h,
                               t3_act ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) : (t3_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                               t3_act ? Typography::COLOR_CYAN : glm::vec4(0.3f, 0.4f, 0.5f, 0.4f),
                               t3_act ? 2.0f : 1.0f);
        draw_text_centered_fitted("[ SETTINGS ]", tab3_x, back_y, tab_w, back_h, 1.10f * ui_scale, t3_act ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

        if (m_active_tab == HubTab::DelverRoster) {
            render_delver_roster(profile, mouse_x, mouse_y, mouse_clicked);
        } else if (m_active_tab == HubTab::UpgradeTerminal) {
            render_upgrade_terminal(profile, mouse_x, mouse_y, mouse_clicked);
        } else {
            render_audio_settings(profile, mouse_x, mouse_y, mouse_clicked);
        }

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        return action;
    }

    // Default Main Menu View
    draw_rect(0.0f, 0.0f, std::min(w * 0.45f, 600.0f * ui_scale), h, glm::vec4(0.02f, 0.03f, 0.05f, 0.85f));
    draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.01f, 0.02f, 0.03f, 0.25f));

    float start_x = std::max(60.0f * ui_scale, 36.0f);
    float title_y = std::max(h * 0.16f, 36.0f);

    // Bold Game Title banner
    draw_text("VOIDFALL: DREDGE", start_x, title_y, 3.2f * ui_scale, Typography::COLOR_CYAN);
    draw_text("SUBTERRANEAN EXPEDITION PROTOCOL", start_x + 4.0f, title_y + 44.0f * ui_scale, 1.25f * ui_scale, Typography::COLOR_PRIMARY);

    // Accent line beneath title
    draw_rect(start_x, title_y + 68.0f * ui_scale, 420.0f * ui_scale, 2.0f, glm::vec4(0.0f, 0.90f, 1.0f, 0.60f));

    // Clean Action List
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
        { MainMenuAction::Settings,        "[ AUDIO & RIG SETTINGS ]",  true,     "Acoustic comfort, sound mixer & controls" },
        { MainMenuAction::Exit,            "[ EXIT ]",                  true,     "Quit to desktop cleanly" }
    };

    float btn_y = title_y + 82.0f * ui_scale;
    float btn_w = std::clamp(420.0f * ui_scale, 320.0f, 480.0f);
    float btn_h = std::clamp(46.0f * ui_scale, 36.0f, 52.0f);
    float btn_gap = std::clamp(10.0f * ui_scale, 6.0f, 14.0f);

    for (size_t i = 0; i < items.size(); ++i) {
        float y = btn_y + i * (btn_h + btn_gap);
        const auto& item = items[i];

        bool is_hovered = item.enabled && (mouse_x >= start_x && mouse_x <= start_x + btn_w + 10.0f &&
                                           mouse_y >= y && mouse_y <= y + btn_h);

        float current_btn_x = is_hovered ? (start_x + 6.0f * ui_scale) : start_x;

        if (is_hovered && mouse_clicked) {
            if (item.act == MainMenuAction::NewExpedition) {
                m_subview = MenuSubView::SectorSelect;
            } else if (item.act == MainMenuAction::UpgradeTerminal) {
                m_subview = MenuSubView::Upgrades;
            } else if (item.act == MainMenuAction::Settings) {
                m_subview = MenuSubView::Settings;
            } else {
                action = item.act;
            }
        }

        // Button background
        glm::vec4 bg_col = !item.enabled ? glm::vec4(0.04f, 0.05f, 0.07f, 0.65f) :
                           is_hovered    ? Typography::COLOR_BUTTON_HOV :
                                           Typography::COLOR_BUTTON_BG;
        glm::vec4 border_col = !item.enabled ? Typography::COLOR_MUTED * 0.4f :
                               is_hovered    ? Typography::COLOR_CYAN_GLOW :
                                               glm::vec4(0.0f, 0.85f, 1.0f, 0.35f);

        draw_panel_with_border(current_btn_x, y, btn_w, btn_h, bg_col, border_col, is_hovered ? 2.0f : 1.0f);

        float text_scale = 1.35f * ui_scale;
        glm::vec4 text_col = !item.enabled ? Typography::COLOR_MUTED :
                             is_hovered    ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) :
                                             Typography::COLOR_PRIMARY;
        draw_text_centered_fitted(item.label, current_btn_x, y, btn_w, btn_h, text_scale, text_col);

        if (is_hovered) {
            float tip_scale = 1.15f * ui_scale;
            float tip_y = y + (btn_h - FontRenderer::get_rendered_height(tip_scale)) * 0.5f;
            draw_text_fitted(item.tip, current_btn_x + btn_w + 20.0f * ui_scale, tip_y, w - (current_btn_x + btn_w + 30.0f * ui_scale), tip_scale, Typography::COLOR_CYAN);
        }
    }

    // Status watermark at bottom
    std::string ver_info = "VOIDFALL // DEEP EXPEDITION ENGINE v1.0.4";
    draw_text(ver_info, start_x, h - 34.0f * ui_scale, 1.05f * ui_scale, Typography::COLOR_MUTED);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return action;
}

void OrbitalHubUI::render_sector_select_carousel(int& selected_level, UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked, MainMenuAction& action) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    // Dark backdrop tint
    draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.94f));

    // Header
    float header_x = std::max(40.0f * ui_scale, 24.0f);
    float header_y = 28.0f * ui_scale;
    draw_text("SELECT EXPEDITION SECTOR", header_x, header_y, 2.4f * ui_scale, Typography::COLOR_CYAN);
    draw_text("CHOOSE A SECTOR TO COMMENCE EXTRACTION DESCENT", header_x + 4.0f, header_y + 32.0f * ui_scale, 1.15f * ui_scale, Typography::COLOR_PRIMARY);
    draw_rect(header_x, header_y + 52.0f * ui_scale, w - 2.0f * header_x, 2.0f, glm::vec4(0.0f, 0.90f, 1.0f, 0.40f));

    // --- Endless sector data generation ---
    // Visible sectors: 1 through (highest_cleared + 1), capped at MAX_SECTOR_RECORDS-1
    const int MAX_VISIBLE = UserProfile::MAX_SECTOR_RECORDS - 1;
    int max_available = std::min(profile.highest_cleared_sector + 1, MAX_VISIBLE);
    max_available = std::max(max_available, 1); // Always show at least Sector 1

    // 3 cards per page
    static constexpr int CARDS_PER_PAGE = 3;
    int total_pages = (max_available + CARDS_PER_PAGE - 1) / CARDS_PER_PAGE;

    // Clamp page index
    m_carousel_page = std::clamp(m_carousel_page, 0, total_pages - 1);

    int page_start = m_carousel_page * CARDS_PER_PAGE + 1; // 1-indexed sector
    int page_end   = std::min(page_start + CARDS_PER_PAGE - 1, max_available);
    (void)page_end;

    // --- Layout ---
    float avail_w       = w - 2.0f * header_x;
    float card_gap      = std::clamp(20.0f * ui_scale, 12.0f, 26.0f);
    float card_w        = (avail_w - 2.0f * card_gap) / static_cast<float>(CARDS_PER_PAGE);
    float total_cards_w = card_w * CARDS_PER_PAGE + card_gap * (CARDS_PER_PAGE - 1);
    float start_x       = header_x;
    float start_y       = header_y + 68.0f * ui_scale;
    float bar_h         = std::clamp(48.0f * ui_scale, 38.0f, 54.0f);
    float card_h        = h - start_y - bar_h - 24.0f * ui_scale;
    float pad           = 16.0f * ui_scale;

    // --- Procedural sector descriptor helpers ---
    auto get_tier_str = [](int s) -> std::string {
        if (s <= 1)  return "[ TIER I  // LOW HAZARD ]";
        if (s == 2)  return "[ TIER II // MEDIUM HAZARD ]";
        if (s == 3)  return "[ TIER III // CRITICAL HAZARD ]";
        if (s <= 6)  return "[ TIER IV // EXTREME HAZARD ]";
        if (s <= 10) return "[ TIER V  // VOID-CLASS HAZARD ]";
        return               "[ TIER VI // ABYSS-CLASS HAZARD ]";
    };
    auto get_title = [](int s) -> std::string {
        static const char* names[] = {
            "PERIMETER DRIFT", "VOLATILE FAULT", "VOID CRADLE",
            "MAGMA UNDERCROFT", "CRYSTALLINE RIFT", "ECHO ABYSS",
            "TECTONIC MAW", "ASHEN SANCTUM", "OBSIDIAN DEEP",
            "NECROTIC VEIN", "WARP FISSURE", "RESONANCE VAULT"
        };
        int idx = (s - 1) % 12;
        int cycle = (s - 1) / 12;
        char buf[80];
        if (cycle == 0) std::snprintf(buf, sizeof(buf), "SECTOR %d: %s", s, names[idx]);
        else            std::snprintf(buf, sizeof(buf), "SECTOR %d: %s [CYCLE %d]", s, names[idx], cycle + 1);
        return std::string(buf);
    };
    auto get_depth = [](int s) -> std::string {
        int depth_m = 800 + (s - 1) * 600;
        char buf[64];
        if (depth_m < 10000)
            std::snprintf(buf, sizeof(buf), "ESTIMATED DEPTH: %d METERS", depth_m);
        else
            std::snprintf(buf, sizeof(buf), "ESTIMATED DEPTH: %.1f KM", depth_m / 1000.0f);
        return std::string(buf);
    };
    auto get_objective = [](int s) -> std::string {
        int voidite = (s <= 1) ? 25 : (s == 2) ? 35 : (s == 3) ? 50 : std::min(50 + (s - 3) * 15, 200);
        if (s == 2) return "OBJECTIVE: BREACH VAULT & RECOVER RELIC";
        if (s % 5 == 0) {
            char buf[72];
            std::snprintf(buf, sizeof(buf), "OBJECTIVE: NEUTRALIZE ALPHA UNIT (x%d)", std::min(1 + (s / 5), 4));
            return std::string(buf);
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "OBJECTIVE: EXTRACT %d VOIDITE", voidite);
        return std::string(buf);
    };
    auto get_hazard = [](int s) -> std::string {
        static const char* hazards[] = {
            "HAZARD: 4-PHASE ESCALATION CURVE",
            "HAZARD: ACID GAS & SEISMIC TREMORS",
            "HAZARD: 3-MIN TECTONIC COLLAPSE",
            "HAZARD: MAGMA VENTS & HEAT SURGE",
            "HAZARD: VOID RADIATION & PSYCHIC PULSE",
            "HAZARD: ECHO SWARM & RESONANCE SHOCKWAVE",
            "HAZARD: GRAVITATIONAL SHIFT ZONES",
            "HAZARD: SPORE BLOOM & SUFFOCATION TIDE",
            "HAZARD: WARP RIFT CASCADES",
            "HAZARD: NECROTIC GAS FLOODS",
            "HAZARD: TEMPORAL DISPLACEMENT BURSTS",
            "HAZARD: FULL ABYSS DESTABILIZATION"
        };
        return hazards[(s - 1) % 12];
    };
    auto get_desc = [](int s) -> std::string {
        static const char* descs[] = {
            "Porous crystalline caverns with stable geological anchor points. Rich in unrefined voidite deposits.",
            "Reinforced basalt subterranean chambers holding pre-fall research vaults. High titanium concentration.",
            "Extreme abyss fissures bordering bedrock mantle. Unstable tectonic gravity causes imminent total cave-in.",
            "Molten rock channels beneath volcanic substrata. Lava flows block critical extraction corridors.",
            "Refracted prismatic tunnels warping spatial perception. Crystalline growths block sonar pulses.",
            "Resonating hollow chambers amplifying Void Stalker aggression. Echo-silence corridors suppress sonar.",
            "Shifting tectonic plates open and close pathways mid-expedition. Platform stability is never guaranteed.",
            "Ashen biome fossilized in ancient combustion event. Spore clouds reduce suit visibility to near zero.",
            "Obsidian-lined deep fissure columns with zero ambient light. Structural integrity is critically compromised.",
            "Corrupted biological substrate fused into voxel matrix. Necrotic gas dissolves bulkhead plating rapidly.",
            "Reality weave is unstable; warp rifts teleport delvers to random cavern nodes.",
            "Pure void substrate bordering the planetary core. No geological rules apply beyond this depth."
        };
        return descs[(s - 1) % 12];
    };
    auto get_bonus = [](int s) -> std::string {
        float mult = 1.0f + (s - 1) * 0.25f;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "EXP MULTIPLIER: %.1fx", mult);
        return std::string(buf);
    };
    auto get_accent = [](int s) -> glm::vec4 {
        if (s <= 1)  return Typography::COLOR_CYAN;
        if (s == 2)  return Typography::COLOR_AMBER;
        if (s <= 4)  return Typography::COLOR_CRIMSON;
        if (s <= 7)  return glm::vec4(0.9f, 0.35f, 1.0f, 1.0f);  // Violet
        if (s <= 10) return glm::vec4(1.0f, 0.6f,  0.1f, 1.0f);  // Orange
        return               glm::vec4(0.4f, 1.0f,  0.5f, 1.0f);  // Neon Green (Abyss)
    };

    // --- Render Cards ---
    for (int slot = 0; slot < CARDS_PER_PAGE; ++slot) {
        int sec_num = page_start + slot;
        float cx = start_x + slot * (card_w + card_gap);

        bool is_unlocked = profile.is_sector_unlocked(sec_num);
        int  req_lvl     = UserProfile::get_required_level_for_sector(sec_num);
        bool is_selected = (selected_level == sec_num);
        bool is_hovered  = (mouse_x >= cx && mouse_x <= cx + card_w &&
                            mouse_y >= start_y && mouse_y <= start_y + card_h);

        // Ghost slot when this page has fewer than 3 valid sectors
        if (sec_num > max_available) {
            draw_panel_with_border(cx, start_y, card_w, card_h,
                                   glm::vec4(0.02f, 0.02f, 0.03f, 0.55f),
                                   glm::vec4(0.12f, 0.15f, 0.18f, 0.25f));
            draw_text_centered("-- UNCHARTED TERRITORY --", cx, start_y, card_w, card_h,
                                1.0f * ui_scale, glm::vec4(0.3f, 0.35f, 0.4f, 0.5f));
            continue;
        }

        if (is_hovered && mouse_clicked) {
            if (is_unlocked) {
                selected_level = sec_num;
            } else {
                m_terminal_msg = "SECTOR LOCKED: REQUIRES DELVER LEVEL " + std::to_string(req_lvl);
                m_terminal_msg_col = glm::vec4(1.0f, 0.35f, 0.35f, 1.0f);
            }
        }

        glm::vec4 accent     = get_accent(sec_num);
        glm::vec4 bg_col     = !is_unlocked ? glm::vec4(0.04f, 0.04f, 0.05f, 0.88f) :
                               is_selected  ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) :
                               is_hovered   ? glm::vec4(0.06f, 0.11f, 0.17f, 0.90f) :
                                              glm::vec4(0.03f, 0.05f, 0.08f, 0.85f);
        glm::vec4 border_col = !is_unlocked ? glm::vec4(0.35f, 0.2f, 0.25f, 0.45f) :
                               is_selected  ? accent :
                               is_hovered   ? glm::vec4(0.4f, 0.85f, 1.0f, 0.8f) :
                                              glm::vec4(0.2f, 0.35f, 0.45f, 0.4f);

        draw_panel_with_border(cx, start_y, card_w, card_h, bg_col, border_col,
                               is_selected ? 3.0f : (is_hovered ? 2.0f : 1.0f));

        // Header
        draw_text(get_tier_str(sec_num), cx + pad, start_y + 14.0f * ui_scale, 1.05f * ui_scale,
                  is_unlocked ? accent : glm::vec4(0.5f, 0.5f, 0.55f, 0.8f));
        draw_text(get_title(sec_num), cx + pad, start_y + 32.0f * ui_scale, 1.30f * ui_scale,
                  is_unlocked ? glm::vec4(1.0f) : glm::vec4(0.6f, 0.6f, 0.65f, 0.8f));
        draw_text(get_depth(sec_num), cx + pad, start_y + 54.0f * ui_scale, 1.00f * ui_scale, Typography::COLOR_MUTED);
        draw_rect(cx + pad, start_y + 70.0f * ui_scale, card_w - 2.0f * pad, 1.0f,
                  glm::vec4(0.25f, 0.4f, 0.55f, 0.4f));

        // Mission parameters
        float param_y = start_y + 82.0f * ui_scale;
        draw_text(get_objective(sec_num), cx + pad, param_y, 0.95f * ui_scale, Typography::COLOR_PRIMARY);
        draw_text(get_hazard(sec_num), cx + pad, param_y + 22.0f * ui_scale, 1.00f * ui_scale, accent);
        draw_text("TOPOLOGICAL SCAN:", cx + pad, param_y + 50.0f * ui_scale, 1.05f * ui_scale, Typography::COLOR_MUTED);

        float desc_w = card_w - 2.0f * pad;
        auto  desc_lines = FontRenderer::wrap_text(get_desc(sec_num), desc_w, 0.95f * ui_scale);
        float line_y = param_y + 72.0f * ui_scale;
        float line_h = FontRenderer::get_line_height(0.95f * ui_scale, 3.0f);
        for (const auto& line : desc_lines) {
            draw_text(line, cx + pad, line_y, 0.95f * ui_scale, glm::vec4(0.85f, 0.90f, 0.95f, 0.90f));
            line_y += line_h;
        }

        // EXP multiplier & best badge
        float bonus_y = start_y + card_h - 58.0f * ui_scale;
        draw_rect(cx + pad, bonus_y - 6.0f * ui_scale, card_w - 2.0f * pad, 1.0f,
                  glm::vec4(0.25f, 0.4f, 0.55f, 0.4f));
        draw_text(get_bonus(sec_num), cx + pad, bonus_y, 1.10f * ui_scale,
                  is_unlocked ? Typography::COLOR_GREEN : glm::vec4(0.5f, 0.55f, 0.5f, 0.7f));
        if (sec_num < UserProfile::MAX_SECTOR_RECORDS &&
            profile.sector_records[sec_num].highest_completion_rate > 0) {
            std::string badge_str = "BEST: " + profile.sector_records[sec_num].best_badge +
                                    "  (" + std::to_string(profile.sector_records[sec_num].highest_completion_rate) + "%)";
            draw_text(badge_str, cx + pad, bonus_y + 18.0f * ui_scale, 0.90f * ui_scale,
                      glm::vec4(0.6f, 1.0f, 0.75f, 0.9f));
        }

        float badge_w = card_w - 2.0f * pad;
        float badge_h = 26.0f * ui_scale;
        float badge_x = cx + pad;
        float badge_y = start_y + card_h - badge_h - 10.0f * ui_scale;

        if (!is_unlocked) {
            draw_panel_with_border(badge_x, badge_y, badge_w, badge_h,
                                   glm::vec4(0.3f, 0.1f, 0.12f, 0.85f), glm::vec4(1.0f, 0.35f, 0.35f, 0.8f));
            draw_text_centered_fitted("[ LOCKED // REQUIRES LEVEL " + std::to_string(req_lvl) + " ]",
                                      badge_x, badge_y, badge_w, badge_h, 0.98f * ui_scale,
                                      glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));
        } else if (is_selected) {
            draw_panel_with_border(badge_x, badge_y, badge_w, badge_h,
                                   glm::vec4(0.0f, 0.85f, 1.0f, 0.20f), Typography::COLOR_CYAN, 1.0f);
            draw_text_centered_fitted("[ ACTIVE SECTOR ]", badge_x, badge_y, badge_w, badge_h,
                                      1.05f * ui_scale, Typography::COLOR_CYAN);
        }
    }

    // --- Bottom Navigation Bar ---
    float bar_y = start_y + card_h + 12.0f * ui_scale;

    // < BACK button
    float back_w = 140.0f * ui_scale;
    float back_h = bar_h;
    float back_x = start_x;
    bool back_hov = (mouse_x >= back_x && mouse_x <= back_x + back_w &&
                     mouse_y >= bar_y  && mouse_y <= bar_y + back_h);
    if (back_hov && mouse_clicked) {
        m_subview = MenuSubView::Main;
    }
    draw_panel_with_border(back_x, bar_y, back_w, back_h,
                           back_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG,
                           back_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f),
                           back_hov ? 2.0f : 1.0f);
    draw_text_centered_fitted("< BACK", back_x, bar_y, back_w, back_h, 1.25f * ui_scale,
                              back_hov ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

    // Page indicator (centred on bar)
    {
        char page_buf[32];
        std::snprintf(page_buf, sizeof(page_buf), "PAGE %d / %d", m_carousel_page + 1, total_pages);
        float pi_scale = 1.10f * ui_scale;
        float pi_w = FontRenderer::get_rendered_width(page_buf, pi_scale);
        float pi_x = start_x + (total_cards_w - pi_w) * 0.5f;
        float pi_y = bar_y + (bar_h - FontRenderer::get_rendered_height(pi_scale)) * 0.5f;
        draw_text(page_buf, pi_x, pi_y, pi_scale, Typography::COLOR_MUTED);
    }

    // < PREV page arrow
    float nav_btn_w = std::clamp(110.0f * ui_scale, 85.0f, 130.0f);
    float prev_x = back_x + back_w + 10.0f * ui_scale;
    bool prev_en  = (total_pages > 1 && m_carousel_page > 0);
    bool prev_hov = prev_en &&
                    mouse_x >= prev_x && mouse_x <= prev_x + nav_btn_w &&
                    mouse_y >= bar_y  && mouse_y <= bar_y + bar_h;
    if (prev_hov && mouse_clicked) {
        m_carousel_page = std::max(0, m_carousel_page - 1);
    }
    {
        glm::vec4 pb = prev_en && prev_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG;
        glm::vec4 bd = prev_en && prev_hov ? Typography::COLOR_CYAN_GLOW  : glm::vec4(0.0f, 0.85f, 1.0f, 0.3f);
        if (!prev_en) { pb = glm::vec4(0.05f, 0.06f, 0.08f, 0.4f); bd = glm::vec4(0.2f, 0.2f, 0.2f, 0.2f); }
        draw_panel_with_border(prev_x, bar_y, nav_btn_w, bar_h, pb, bd, prev_hov ? 2.0f : 1.0f);
        glm::vec4 tc = !prev_en ? glm::vec4(0.4f, 0.4f, 0.4f, 0.4f) :
                        prev_hov ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY;
        draw_text_centered_fitted("< PREV", prev_x, bar_y, nav_btn_w, bar_h, 1.15f * ui_scale, tc);
    }

    // NEXT > page arrow
    float launch_w = std::clamp(380.0f * ui_scale, 260.0f, 440.0f);
    float next_x = start_x + total_cards_w - launch_w - nav_btn_w - 10.0f * ui_scale;
    bool next_en  = (total_pages > 1 && m_carousel_page < total_pages - 1);
    bool next_hov = next_en &&
                    mouse_x >= next_x && mouse_x <= next_x + nav_btn_w &&
                    mouse_y >= bar_y  && mouse_y <= bar_y + bar_h;
    if (next_hov && mouse_clicked) {
        m_carousel_page = std::min(total_pages - 1, m_carousel_page + 1);
    }
    {
        glm::vec4 nb = next_en && next_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG;
        glm::vec4 bd = next_en && next_hov ? Typography::COLOR_CYAN_GLOW  : glm::vec4(0.0f, 0.85f, 1.0f, 0.3f);
        if (!next_en) { nb = glm::vec4(0.05f, 0.06f, 0.08f, 0.4f); bd = glm::vec4(0.2f, 0.2f, 0.2f, 0.2f); }
        draw_panel_with_border(next_x, bar_y, nav_btn_w, bar_h, nb, bd, next_hov ? 2.0f : 1.0f);
        glm::vec4 tc = !next_en ? glm::vec4(0.4f, 0.4f, 0.4f, 0.4f) :
                        next_hov ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY;
        draw_text_centered_fitted("NEXT >", next_x, bar_y, nav_btn_w, bar_h, 1.15f * ui_scale, tc);
    }

    // LAUNCH button
    bool cur_unlocked = profile.is_sector_unlocked(selected_level);
    int  cur_req_lvl  = UserProfile::get_required_level_for_sector(selected_level);

    float launch_h = bar_h;
    float launch_x = start_x + total_cards_w - launch_w;
    bool launch_hov = (mouse_x >= launch_x && mouse_x <= launch_x + launch_w &&
                       mouse_y >= bar_y     && mouse_y <= bar_y + launch_h);

    if (launch_hov && mouse_clicked && cur_unlocked) {
        action = MainMenuAction::NewExpedition;
    }

    glm::vec4 launch_bg = !cur_unlocked ? glm::vec4(0.25f, 0.10f, 0.12f, 0.85f) :
                          launch_hov    ? glm::vec4(0.08f, 0.32f, 0.18f, 0.95f) : glm::vec4(0.05f, 0.22f, 0.12f, 0.90f);
    glm::vec4 launch_border = !cur_unlocked ? glm::vec4(0.7f, 0.25f, 0.25f, 0.7f) :
                              launch_hov    ? Typography::COLOR_GREEN : glm::vec4(0.18f, 0.80f, 0.44f, 0.50f);
    draw_panel_with_border(launch_x, bar_y, launch_w, launch_h, launch_bg, launch_border,
                           launch_hov ? 2.5f : 1.5f);

    std::string launch_text = cur_unlocked
        ? "[ LAUNCH SECTOR " + std::to_string(selected_level) + " ]"
        : "[ LOCKED // LEVEL " + std::to_string(cur_req_lvl) + " REQUIRED ]";
    float text_scale = cur_unlocked ? 1.45f * ui_scale : 1.20f * ui_scale;
    draw_text_centered_fitted(launch_text, launch_x, bar_y, launch_w, launch_h, text_scale,
                              !cur_unlocked ? glm::vec4(1.0f, 0.45f, 0.45f, 0.95f) :
                              launch_hov    ? glm::vec4(1.0f) : Typography::COLOR_GREEN);
}


void OrbitalHubUI::render_delver_roster(UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    float avail_w = w - 80.0f * ui_scale;
    float card_gap = std::clamp(20.0f * ui_scale, 12.0f, 26.0f);
    float card_w = (avail_w - 2.0f * card_gap) / 3.0f;
    float total_roster_w = card_w * 3.0f + card_gap * 2.0f;
    float start_x = (w - total_roster_w) * 0.5f;
    float start_y = 76.0f * ui_scale;
    float card_h = h - start_y - 24.0f * ui_scale;
    float pad = 16.0f * ui_scale;

    CharacterClass classes[3] = {
        CharacterClass::Demolitionist,
        CharacterClass::Vanguard,
        CharacterClass::Scout
    };

    for (int i = 0; i < 3; ++i) {
        float cx = start_x + i * (card_w + card_gap);
        auto attr = get_character_attributes(classes[i]);
        bool is_unlocked = profile.is_class_unlocked(classes[i]);
        int req_lvl = UserProfile::get_required_level_for_class(classes[i]);
        bool is_selected = (profile.selected_class_id == i);
        bool is_hovered = (mouse_x >= cx && mouse_x <= cx + card_w &&
                           mouse_y >= start_y && mouse_y <= start_y + card_h);

        if (is_hovered && mouse_clicked) {
            if (is_unlocked) {
                profile.selected_class_id = i;
                m_profile_dirty = true;
            } else {
                m_terminal_msg = "DELVER LOCKED: REQUIRES PLAYER LEVEL " + std::to_string(req_lvl);
                m_terminal_msg_col = glm::vec4(1.0f, 0.35f, 0.35f, 1.0f);
            }
        }

        glm::vec4 bg_col = !is_unlocked ? glm::vec4(0.04f, 0.04f, 0.06f, 0.88f) :
                           is_selected  ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) :
                           is_hovered   ? glm::vec4(0.06f, 0.12f, 0.18f, 0.90f) :
                                          glm::vec4(0.03f, 0.05f, 0.08f, 0.85f);
        glm::vec4 border_col = !is_unlocked ? glm::vec4(0.4f, 0.2f, 0.25f, 0.45f) :
                               is_selected  ? attr.primaryAccentColor :
                               is_hovered   ? glm::vec4(0.4f, 0.85f, 1.0f, 0.9f) :
                                              glm::vec4(0.2f, 0.35f, 0.45f, 0.5f);

        draw_panel_with_border(cx, start_y, card_w, card_h, bg_col, border_col, is_selected ? 3.0f : (is_hovered ? 2.0f : 1.0f));

        // Header: Emblem & Delver Name
        std::string emblem = (i == 0) ? "[ DEMOLITIONIST ]" :
                             (i == 1) ? "[ VANGUARD ]" : "[ SCOUT ]";
        draw_text(emblem, cx + pad, start_y + 14.0f * ui_scale, 1.15f * ui_scale, is_unlocked ? attr.primaryAccentColor : glm::vec4(0.5f, 0.5f, 0.55f, 0.8f));
        draw_text(attr.name, cx + pad, start_y + 32.0f * ui_scale, 1.60f * ui_scale, is_unlocked ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.65f, 0.8f));
        draw_text(attr.role, cx + pad, start_y + 54.0f * ui_scale, 1.05f * ui_scale, glm::vec4(0.7f, 0.8f, 0.9f, 0.9f));
        draw_rect(cx + pad, start_y + 70.0f * ui_scale, card_w - 2.0f * pad, 1.0f, glm::vec4(0.25f, 0.4f, 0.55f, 0.5f));

        // Stat Bars Section
        float stat_y = start_y + 80.0f * ui_scale;
        draw_text("DELVER BASELINE SPECIFICATIONS:", cx + pad, stat_y, 1.05f * ui_scale, glm::vec4(0.2f, 0.95f, 1.0f, 1.0f));
        stat_y += 18.0f * ui_scale;

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

        float font_h = FontRenderer::get_rendered_height(1.00f * ui_scale);
        for (int s = 0; s < 4; ++s) {
            draw_text(stats[s].label, cx + pad, stat_y, 1.00f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
            float val_w = FontRenderer::get_rendered_width(stats[s].val_str, 1.00f * ui_scale);
            draw_text(stats[s].val_str, cx + card_w - pad - val_w, stat_y, 1.00f * ui_scale, is_unlocked ? attr.primaryAccentColor : glm::vec4(0.6f));
            stat_y += font_h + 3.0f * ui_scale;
            float bar_h = 6.0f * ui_scale;
            draw_rect(cx + pad, stat_y, card_w - 2.0f * pad, bar_h, glm::vec4(0.1f, 0.15f, 0.2f, 0.8f));
            draw_rect(cx + pad, stat_y, (card_w - 2.0f * pad) * std::clamp(stats[s].ratio, 0.0f, 1.0f), bar_h, is_unlocked ? attr.primaryAccentColor : glm::vec4(0.4f, 0.45f, 0.5f, 0.6f));
            stat_y += bar_h + 7.0f * ui_scale;
        }

        // Traits & Ability Description
        draw_rect(cx + pad, stat_y + 2.0f * ui_scale, card_w - 2.0f * pad, 1.0f, glm::vec4(0.25f, 0.4f, 0.55f, 0.5f));
        stat_y += 10.0f * ui_scale;
        draw_text("TRAIT: " + attr.traitName, cx + pad, stat_y, 1.10f * ui_scale, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
        stat_y += font_h + 4.0f * ui_scale;

        auto trait_lines = FontRenderer::wrap_text(attr.traitDescription, card_w - 2.0f * pad, 0.95f * ui_scale);
        float line_h = FontRenderer::get_line_height(0.95f * ui_scale, 3.0f);
        for (const auto& l : trait_lines) {
            draw_text(l, cx + pad, stat_y, 0.95f * ui_scale, glm::vec4(0.75f, 0.8f, 0.85f, 0.9f));
            stat_y += line_h;
        }

        // Selection Action Button at card bottom
        float btn_h = std::clamp(38.0f * ui_scale, 32.0f, 44.0f);
        float btn_y = start_y + card_h - btn_h - 12.0f * ui_scale;
        float btn_w = card_w - 2.0f * pad;
        float btn_x = cx + pad;

        glm::vec4 sel_col = !is_unlocked ? glm::vec4(0.25f, 0.10f, 0.12f, 0.85f) :
                            is_selected  ? glm::vec4(0.15f, 0.65f, 0.4f, 1.0f) :
                            is_hovered   ? glm::vec4(0.2f, 0.45f, 0.75f, 1.0f) :
                                           glm::vec4(0.1f, 0.2f, 0.3f, 0.85f);
        glm::vec4 b_border = !is_unlocked ? glm::vec4(0.7f, 0.25f, 0.25f, 0.7f) :
                             is_selected  ? glm::vec4(0.4f, 1.0f, 0.6f, 1.0f) : border_col;
        draw_panel_with_border(btn_x, btn_y, btn_w, btn_h, sel_col, b_border);

        std::string btn_txt = !is_unlocked ? "[ LOCKED // REQUIRES LEVEL " + std::to_string(req_lvl) + " ]" :
                              is_selected  ? "[ ACTIVE DELVER SELECTED ]" :
                              is_hovered   ? "[ CLICK TO SELECT DELVER ]" : "[ SELECT DELVER ]";
        draw_text_centered_fitted(btn_txt, btn_x, btn_y, btn_w, btn_h, 1.05f * ui_scale,
                                  !is_unlocked ? glm::vec4(1.0f, 0.45f, 0.45f, 0.95f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
}

void OrbitalHubUI::render_upgrade_terminal(UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    float terminal_w = std::clamp(w * 0.94f, 680.0f, 1460.0f);
    float start_x = (w - terminal_w) * 0.5f;
    float start_y = 66.0f * ui_scale;

    // Pulse feedback overlay
    if (m_purchase_pulse_timer > 0.0f) {
        float pulse_alpha = m_purchase_pulse_timer * 0.35f;
        draw_rect(0, 0, w, 4.0f, glm::vec4(1.0f, 0.75f, 0.1f, pulse_alpha * 2.0f));
        draw_rect(0, h - 4.0f, w, 4.0f, glm::vec4(1.0f, 0.75f, 0.1f, pulse_alpha * 2.0f));
        m_purchase_pulse_timer = std::max(0.0f, m_purchase_pulse_timer - 0.016f);
    }

    // Top Balance & Respec Strip
    float top_h = 44.0f * ui_scale;
    draw_panel_with_border(start_x, start_y, terminal_w, top_h, glm::vec4(0.04f, 0.07f, 0.11f, 0.95f), glm::vec4(0.2f, 0.45f, 0.65f, 0.6f));

    // Respec Button: Sleek, high-contrast, well-padded tactical button (100% full recovery)
    float respec_w = std::clamp(210.0f * ui_scale, 160.0f, 250.0f);
    float respec_h = 30.0f * ui_scale;
    float respec_x = start_x + terminal_w - respec_w - 10.0f * ui_scale;
    float respec_y = start_y + (top_h - respec_h) * 0.5f;
    bool respec_hov = (mouse_x >= respec_x && mouse_x <= respec_x + respec_w &&
                       mouse_y >= respec_y && mouse_y <= respec_y + respec_h);

    if (respec_hov && mouse_clicked) {
        int ref_coins = 0, ref_voidite = 0, ref_titanium = 0;
        profile.upgrades.respec(ref_coins, ref_voidite, ref_titanium);
        profile.total_coins += ref_coins;
        profile.total_voidite += ref_voidite;
        profile.total_titanium += ref_titanium;
        m_profile_dirty = true;
        m_purchase_pulse_timer = 1.0f;
        m_terminal_msg = "RESPEC COMPLETE: REFUNDED 100% (" + std::to_string(ref_coins) + " COINS, " +
                         std::to_string(ref_voidite) + " VOIDITE, " + std::to_string(ref_titanium) + " TITANIUM). READY TO RE-ALLOCATE!";
        m_terminal_msg_col = glm::vec4(0.2f, 0.95f, 0.4f, 1.0f);
    }

    glm::vec4 respec_bg = respec_hov ? glm::vec4(0.55f, 0.18f, 0.20f, 0.98f) : glm::vec4(0.20f, 0.08f, 0.10f, 0.92f);
    glm::vec4 respec_border = respec_hov ? Typography::COLOR_CRIMSON : glm::vec4(0.85f, 0.35f, 0.35f, 0.75f);
    draw_panel_with_border(respec_x, respec_y, respec_w, respec_h, respec_bg, respec_border);
    draw_text_centered_fitted("[RESPEC (100% REFUND)]", respec_x, respec_y, respec_w, respec_h, 0.88f * ui_scale,
                              respec_hov ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(1.0f, 0.85f, 0.85f, 0.95f));

    // Full-Width Delver Balance Line
    float avail_bal_w = respec_x - start_x - 18.0f * ui_scale;
    int player_lvl = profile.get_player_level();
    float bal_scale = 1.15f * ui_scale;

    std::string bal_str = "DELVER RANK: LVL " + std::to_string(player_lvl) +
                          "  |  EXP: " + std::to_string(profile.total_exp) + "/" + std::to_string(profile.get_next_level_exp_req()) +
                          "  |  COINS: " + std::to_string(profile.total_coins) +
                          "  |  VOIDITE: " + std::to_string(profile.total_voidite) +
                          "  |  TITANIUM: " + std::to_string(profile.total_titanium);

    if (FontRenderer::get_rendered_width(bal_str, bal_scale) > avail_bal_w) {
        bal_scale = std::max(0.85f * ui_scale, 0.72f);
    }
    draw_text(bal_str, start_x + 14.0f * ui_scale, start_y + (top_h - FontRenderer::get_rendered_height(bal_scale)) * 0.5f, bal_scale, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    // Sub-row: Terminal Status Feedback Line & Test Mode Action Badges
    float subrow_y = start_y + top_h + 4.0f * ui_scale;
    if (!m_terminal_msg.empty()) {
        draw_text(">> " + m_terminal_msg, start_x + 14.0f * ui_scale, subrow_y, 0.98f * ui_scale, m_terminal_msg_col);
    }

    if (m_test_mode) {
        float grant_w = std::clamp(125.0f * ui_scale, 95.0f, 145.0f);
        float grant_h = 18.0f * ui_scale;
        float grant_x = start_x + terminal_w - grant_w - 10.0f * ui_scale;
        float grant_y = subrow_y;
        bool grant_hov = (mouse_x >= grant_x && mouse_x <= grant_x + grant_w &&
                          mouse_y >= grant_y && mouse_y <= grant_y + grant_h);

        if (grant_hov && mouse_clicked) {
            profile.grant_resources(1000, 500, 20, 10);
            m_profile_dirty = true;
            m_purchase_pulse_timer = 1.0f;
            m_terminal_msg = "TEST GRANT: +1000 EXP, +500 COINS, +20 VOIDITE, +10 TITANIUM ADDED";
            m_terminal_msg_col = glm::vec4(0.2f, 0.95f, 0.4f, 1.0f);
        }

        glm::vec4 grant_bg = grant_hov ? glm::vec4(0.15f, 0.45f, 0.25f, 1.0f) : glm::vec4(0.08f, 0.22f, 0.14f, 0.85f);
        draw_panel_with_border(grant_x, grant_y, grant_w, grant_h, grant_bg, glm::vec4(0.3f, 0.85f, 0.45f, 0.7f));
        draw_text_centered("[+1000 EXP (F5)]", grant_x, grant_y, grant_w, grant_h, 0.76f * ui_scale, glm::vec4(0.6f, 1.0f, 0.7f, 1.0f));

        float cycle_w = std::clamp(130.0f * ui_scale, 100.0f, 150.0f);
        float cycle_h = grant_h;
        float cycle_x = grant_x - cycle_w - 8.0f * ui_scale;
        float cycle_y = subrow_y;
        bool cycle_hov = (mouse_x >= cycle_x && mouse_x <= cycle_x + cycle_w &&
                          mouse_y >= cycle_y && mouse_y <= cycle_y + cycle_h);

        if (cycle_hov && mouse_clicked) {
            if (profile.total_coins < 500) profile.grant_resources(1000, 500, 30, 15);
            bool p1 = profile.upgrades.purchase(UpgradeType::DrillSpeed, profile.get_player_level(), profile.total_coins, profile.total_voidite, profile.total_titanium);
            bool p2 = profile.upgrades.purchase(UpgradeType::ThrusterTank, profile.get_player_level(), profile.total_coins, profile.total_voidite, profile.total_titanium);
            (void)p1; (void)p2;
            int ref_c = 0, ref_v = 0, ref_t = 0;
            profile.upgrades.respec(ref_c, ref_v, ref_t);
            profile.total_coins += ref_c;
            profile.total_voidite += ref_v;
            profile.total_titanium += ref_t;
            m_profile_dirty = true;
            m_purchase_pulse_timer = 1.0f;
            m_terminal_msg = "TEST CYCLE: PURCHASED & RESPECCED (REFUNDED 100%: " + std::to_string(ref_c) + "C, " +
                             std::to_string(ref_v) + "V, " + std::to_string(ref_t) + "T)";
            m_terminal_msg_col = glm::vec4(0.3f, 0.85f, 1.0f, 1.0f);
        }

        glm::vec4 cycle_bg = cycle_hov ? glm::vec4(0.2f, 0.32f, 0.48f, 1.0f) : glm::vec4(0.10f, 0.18f, 0.28f, 0.85f);
        draw_panel_with_border(cycle_x, cycle_y, cycle_w, cycle_h, cycle_bg, glm::vec4(0.35f, 0.65f, 0.9f, 0.7f));
        draw_text_centered("[TEST RESPEC (F6)]", cycle_x, cycle_y, cycle_w, cycle_h, 0.76f * ui_scale, glm::vec4(0.8f, 0.9f, 1.0f, 1.0f));
    }

    // ── Main Content Area: Skill Tree (Left) & Horizon-Style Inspector (Right) ──
    float foot_h = 32.0f * ui_scale;
    float foot_y = h - foot_h - 10.0f * ui_scale;
    float main_y = start_y + top_h + 24.0f * ui_scale;
    float main_h = foot_y - main_y - 8.0f * ui_scale;

    float tree_w = std::round(terminal_w * 0.58f);
    float insp_x = start_x + tree_w + 14.0f * ui_scale;
    float insp_w = terminal_w - tree_w - 14.0f * ui_scale;

    // 6 Upgrade Types mapped to 3 Disciplines
    UpgradeType branch_nodes[3][2] = {
        { UpgradeType::DrillSpeed, UpgradeType::DrillDurability },       // EXCAVATION (Drill Matrix)
        { UpgradeType::ThrusterTank, UpgradeType::KineticDynamo },       // TRAVERSAL (Exo-Suit)
        { UpgradeType::ReinforcedPlating, UpgradeType::SonarFrequency }  // DEFENSE & SURVEYING
    };

    std::string branch_titles[3] = {
        "EXCAVATION",
        "TRAVERSAL",
        "SURVEYING"
    };

    std::string branch_subtitles[3] = {
        "DRILL MATRIX",
        "EXO-SUIT",
        "DEFENSE & SCAN"
    };

    glm::vec4 branch_colors[3] = {
        glm::vec4(1.0f, 0.65f, 0.15f, 1.0f),  // Amber
        glm::vec4(0.2f, 0.85f, 1.0f, 1.0f),   // Cyan
        glm::vec4(0.3f, 0.95f, 0.55f, 1.0f)   // Emerald
    };

    float col_gap = 10.0f * ui_scale;
    float col_w = (tree_w - 2.0f * col_gap) / 3.0f;
    float b_header_h = 28.0f * ui_scale;
    float node_h = std::clamp((main_h - b_header_h - 40.0f * ui_scale) / 2.0f, 110.0f, 210.0f);
    float arrow_gap = 26.0f * ui_scale;

    m_selected_upgrade_idx = std::clamp(m_selected_upgrade_idx, 0, 5);

    // Render 3 Discipline Columns
    for (int b = 0; b < 3; ++b) {
        float cx = start_x + b * (col_w + col_gap);
        float cy = main_y;

        // Discipline Header
        draw_panel_with_border(cx, cy, col_w, b_header_h, glm::vec4(0.05f, 0.08f, 0.12f, 0.9f), branch_colors[b] * glm::vec4(1.0f, 1.0f, 1.0f, 0.6f));
        draw_rect(cx, cy, 3.0f, b_header_h, branch_colors[b]);
        draw_text(branch_titles[b] + " // " + branch_subtitles[b], cx + 8.0f * ui_scale, cy + 6.0f * ui_scale, 0.88f * ui_scale, branch_colors[b]);

        // Render 2 Nodes in this branch (Root Node then Advanced Node)
        for (int r = 0; r < 2; ++r) {
            UpgradeType type = branch_nodes[b][r];
            int type_idx = static_cast<int>(type);
            float ny = cy + b_header_h + 8.0f * ui_scale + r * (node_h + arrow_gap);

            // Prerequisite Connector Flow (between Node 0 and Node 1)
            if (r == 1) {
                float conn_y = ny - arrow_gap;
                float conn_cx = cx + col_w * 0.5f;
                draw_rect(conn_cx - 1.0f, conn_y + 2.0f, 2.0f, arrow_gap - 6.0f, branch_colors[b] * glm::vec4(1.0f, 1.0f, 1.0f, 0.5f));
                // Tactical flow arrow ▼
                draw_text_centered("v", conn_cx - 10.0f, conn_y + 6.0f * ui_scale, 20.0f, 14.0f, 0.72f * ui_scale, branch_colors[b]);
                draw_text_centered("REQUIRES T1", cx, conn_y + 4.0f * ui_scale, col_w, 14.0f, 0.65f * ui_scale, glm::vec4(0.7f, 0.75f, 0.8f, 0.7f));
            }

            int cur_tier = profile.upgrades.get_tier(type);
            int req_lvl = UpgradeTree::get_required_level_for_tier(cur_tier);
            bool is_maxed = (cur_tier >= UpgradeTree::MAX_TIER);
            bool can_buy = profile.upgrades.can_purchase(type, player_lvl, profile.total_coins, profile.total_voidite, profile.total_titanium);
            bool is_selected = (m_selected_upgrade_idx == type_idx);

            bool node_hov = (mouse_x >= cx && mouse_x <= cx + col_w && mouse_y >= ny && mouse_y <= ny + node_h);
            if (node_hov && mouse_clicked) {
                m_selected_upgrade_idx = type_idx;
            }

            // Node Panel Background & Selection Border
            glm::vec4 n_bg = is_selected ? glm::vec4(0.08f, 0.12f, 0.18f, 0.98f) :
                             node_hov ? glm::vec4(0.06f, 0.09f, 0.14f, 0.95f) :
                                        glm::vec4(0.04f, 0.06f, 0.09f, 0.92f);
            glm::vec4 n_border = is_selected ? branch_colors[b] :
                                 node_hov ? glm::vec4(0.35f, 0.6f, 0.8f, 0.8f) :
                                            glm::vec4(0.16f, 0.24f, 0.32f, 0.6f);

            draw_panel_with_border(cx, ny, col_w, node_h, n_bg, n_border, is_selected ? 2.0f : 1.0f);
            if (is_selected) {
                // Corner selection accents
                draw_rect(cx, ny, 6.0f, 2.0f, branch_colors[b]);
                draw_rect(cx, ny, 2.0f, 6.0f, branch_colors[b]);
                draw_rect(cx + col_w - 6.0f, ny, 6.0f, 2.0f, branch_colors[b]);
                draw_rect(cx + col_w - 2.0f, ny, 2.0f, 6.0f, branch_colors[b]);
            }

            auto info = UpgradeTree::get_info(type);

            // Node Title
            draw_text_fitted(info.name, cx + 10.0f * ui_scale, ny + 8.0f * ui_scale, col_w - 20.0f * ui_scale, 0.92f * ui_scale,
                             is_selected ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.85f, 0.90f, 0.95f, 0.9f));

            // Rank Subtitle
            std::string sub_title = (cur_tier > 0 && cur_tier <= static_cast<int>(info.tier_subtitles.size())) ?
                                     info.tier_subtitles[cur_tier - 1] : (r == 0 ? "Foundation Node" : "Specialization Node");
            draw_text_fitted(sub_title, cx + 10.0f * ui_scale, ny + 24.0f * ui_scale, col_w - 20.0f * ui_scale, 0.78f * ui_scale, branch_colors[b] * 0.9f);

            // Progress Pips [ ■ ■ ■ □ □ ]
            float pip_start_x = cx + 10.0f * ui_scale;
            float pip_y = ny + 42.0f * ui_scale;
            float pip_size = 11.0f * ui_scale;
            float pip_gap = 4.0f * ui_scale;

            for (int p = 0; p < UpgradeTree::MAX_TIER; ++p) {
                float px = pip_start_x + p * (pip_size + pip_gap);
                bool filled = (p < cur_tier);
                glm::vec4 pip_col = filled ? (is_maxed ? glm::vec4(1.0f, 0.85f, 0.2f, 1.0f) : glm::vec4(0.2f, 0.95f, 0.4f, 1.0f)) :
                                    (p == cur_tier && can_buy) ? glm::vec4(0.2f, 0.75f, 0.95f, 0.9f) :
                                                                 glm::vec4(0.12f, 0.16f, 0.22f, 0.8f);
                draw_rect(px, pip_y, pip_size, pip_size, pip_col);
                draw_rect(px, pip_y, pip_size, 1.0f, glm::vec4(0.3f, 0.5f, 0.6f, 0.7f));
            }

            std::string tier_str = "TIER " + std::to_string(cur_tier) + "/" + std::to_string(UpgradeTree::MAX_TIER);
            draw_text(tier_str, pip_start_x + UpgradeTree::MAX_TIER * (pip_size + pip_gap) + 6.0f * ui_scale, pip_y + 1.0f, 0.85f * ui_scale,
                      is_maxed ? glm::vec4(1.0f, 0.85f, 0.2f, 1.0f) : glm::vec4(0.8f, 0.85f, 0.9f, 0.85f));

            // Current Stat Line
            std::string cur_stat = profile.upgrades.get_current_stat_string(type);
            draw_text_fitted("Bonus: " + cur_stat, cx + 10.0f * ui_scale, ny + 62.0f * ui_scale, col_w - 20.0f * ui_scale, 0.82f * ui_scale, glm::vec4(0.9f, 0.9f, 0.9f, 0.9f));

            // Quick Next Cost or Status Tag
            std::string cost_tag;
            glm::vec4 tag_col;
            if (is_maxed) {
                cost_tag = "[MASTERED]";
                tag_col = glm::vec4(0.2f, 0.95f, 0.4f, 1.0f);
            } else if (player_lvl < req_lvl) {
                cost_tag = "REQ LVL " + std::to_string(req_lvl);
                tag_col = glm::vec4(1.0f, 0.55f, 0.4f, 0.95f);
            } else {
                auto prereq = UpgradeTree::get_prerequisite(type);
                if (prereq.has_prerequisite && profile.upgrades.get_tier(prereq.required_type) < prereq.required_tier) {
                    cost_tag = "LOCKED (REQ T1)";
                    tag_col = glm::vec4(1.0f, 0.5f, 0.4f, 0.9f);
                } else {
                    int c_cost = UpgradeTree::get_coin_cost(cur_tier);
                    int v_cost = UpgradeTree::get_voidite_cost(cur_tier);
                    int t_cost = UpgradeTree::get_titanium_cost(cur_tier);
                    cost_tag = std::to_string(c_cost) + "C | " + std::to_string(v_cost) + "V | " + std::to_string(t_cost) + "T";
                    tag_col = can_buy ? Typography::COLOR_CYAN : glm::vec4(0.85f, 0.7f, 0.4f, 0.9f);
                }
            }

            float tag_y = ny + node_h - 22.0f * ui_scale;
            draw_rect(cx + 8.0f * ui_scale, tag_y - 2.0f, col_w - 16.0f * ui_scale, 18.0f * ui_scale, glm::vec4(0.02f, 0.04f, 0.07f, 0.8f));
            draw_text_centered_fitted(cost_tag, cx + 8.0f * ui_scale, tag_y - 2.0f, col_w - 16.0f * ui_scale, 18.0f * ui_scale, 0.80f * ui_scale, tag_col);
        }
    }

    // ── Horizon Zero Dawn Style Skill Detail & Cost Inspector (Right Panel) ──
    UpgradeType sel_type = static_cast<UpgradeType>(m_selected_upgrade_idx);
    auto sel_info = UpgradeTree::get_info(sel_type);
    int sel_tier = profile.upgrades.get_tier(sel_type);
    int sel_req_lvl = UpgradeTree::get_required_level_for_tier(sel_tier);
    bool sel_is_maxed = (sel_tier >= UpgradeTree::MAX_TIER);
    bool sel_can_buy = profile.upgrades.can_purchase(sel_type, player_lvl, profile.total_coins, profile.total_voidite, profile.total_titanium);
    UpgradePrerequisite sel_prereq = UpgradeTree::get_prerequisite(sel_type);
    bool prereq_met = !sel_prereq.has_prerequisite || (profile.upgrades.get_tier(sel_prereq.required_type) >= sel_prereq.required_tier);

    // Inspector Panel
    draw_panel_with_border(insp_x, main_y, insp_w, main_h, glm::vec4(0.04f, 0.07f, 0.11f, 0.96f), glm::vec4(0.22f, 0.45f, 0.65f, 0.8f), 1.5f);
    draw_rect(insp_x, main_y, insp_w, 3.0f, Typography::COLOR_CYAN_GLOW);

    float ix = insp_x + 16.0f * ui_scale;
    float iy = main_y + 12.0f * ui_scale;
    float iw = insp_w - 32.0f * ui_scale;

    // Header Tag & Title
    draw_text("// " + sel_info.discipline + " DISCIPLINE", ix, iy, 0.80f * ui_scale, Typography::COLOR_CYAN);
    iy += 16.0f * ui_scale;
    draw_text_fitted(sel_info.name, ix, iy, iw, 1.25f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    iy += 26.0f * ui_scale;

    // Current & Next Rank Subtitles
    std::string cur_title = (sel_tier > 0 && sel_tier <= static_cast<int>(sel_info.tier_subtitles.size())) ?
                            sel_info.tier_subtitles[sel_tier - 1] : "Unranked";
    std::string next_title = (!sel_is_maxed && sel_tier < static_cast<int>(sel_info.tier_subtitles.size())) ?
                             sel_info.tier_subtitles[sel_tier] : "Maximum Rank Achieved";
    draw_text_fitted("Current: Tier " + std::to_string(sel_tier) + " [" + cur_title + "]", ix, iy, iw, 0.88f * ui_scale, glm::vec4(0.85f, 0.88f, 0.92f, 0.9f));
    iy += 18.0f * ui_scale;
    if (!sel_is_maxed) {
        draw_text_fitted("Next: Tier " + std::to_string(sel_tier + 1) + " [" + next_title + "]", ix, iy, iw, 0.88f * ui_scale, glm::vec4(0.3f, 0.95f, 0.55f, 0.95f));
        iy += 18.0f * ui_scale;
    }

    // Description text wrapped
    auto desc_lines = FontRenderer::wrap_text(sel_info.description, iw, 0.88f * ui_scale);
    for (const auto& line : desc_lines) {
        draw_text(line, ix, iy, 0.88f * ui_scale, glm::vec4(0.7f, 0.75f, 0.82f, 0.85f));
        iy += 16.0f * ui_scale;
    }
    iy += 6.0f * ui_scale;

    // Milestone Perk Banner (if any)
    if (!sel_info.milestone_perk.empty()) {
        float mb_h = 24.0f * ui_scale;
        draw_panel_with_border(ix, iy, iw, mb_h, glm::vec4(0.12f, 0.20f, 0.16f, 0.9f), glm::vec4(0.2f, 0.85f, 0.45f, 0.7f));
        draw_text_fitted("★ " + sel_info.milestone_perk, ix + 8.0f * ui_scale, iy + 5.0f * ui_scale, iw - 16.0f * ui_scale, 0.78f * ui_scale, glm::vec4(0.6f, 1.0f, 0.7f, 1.0f));
        iy += mb_h + 8.0f * ui_scale;
    }

    // ── Dedicated Clear Upgrade Cost Box for Next Level ──
    float cost_box_h = 92.0f * ui_scale;
    draw_panel_with_border(ix, iy, iw, cost_box_h, glm::vec4(0.02f, 0.04f, 0.07f, 0.92f), glm::vec4(0.25f, 0.4f, 0.55f, 0.7f));
    draw_text("NEXT LEVEL UPGRADE REQUIREMENTS", ix + 10.0f * ui_scale, iy + 6.0f * ui_scale, 0.82f * ui_scale, Typography::COLOR_CYAN_GLOW);

    if (sel_is_maxed) {
        draw_text_centered("ALL 5 TIERS MASTERED - MAXIMUM SPECIALIZATION", ix, iy + 30.0f * ui_scale, iw, 30.0f * ui_scale, 0.95f * ui_scale, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
    } else {
        int coin_c = UpgradeTree::get_coin_cost(sel_tier);
        int void_c = UpgradeTree::get_voidite_cost(sel_tier);
        int tit_c = UpgradeTree::get_titanium_cost(sel_tier);

        bool lvl_ok = (player_lvl >= sel_req_lvl);
        bool coins_ok = (profile.total_coins >= coin_c);
        bool void_ok = (profile.total_voidite >= void_c);
        bool tit_ok = (profile.total_titanium >= tit_c);

        // Row 1: Level & Prerequisite requirements
        std::string lvl_txt = "PLAYER LEVEL " + std::to_string(sel_req_lvl) + (lvl_ok ? " [OK]" : " [LOCKED]");
        glm::vec4 lvl_col = lvl_ok ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.45f, 0.45f, 1.0f);
        draw_text(lvl_txt, ix + 10.0f * ui_scale, iy + 26.0f * ui_scale, 0.82f * ui_scale, lvl_col);

        if (sel_prereq.has_prerequisite) {
            std::string pre_txt = "PREREQ: " + sel_prereq.required_name + (prereq_met ? " [MET]" : " [LOCKED]");
            glm::vec4 pre_col = prereq_met ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.5f, 0.4f, 1.0f);
            draw_text_fitted(pre_txt, ix + iw * 0.50f, iy + 26.0f * ui_scale, iw * 0.50f - 8.0f * ui_scale, 0.82f * ui_scale, pre_col);
        }

        // Row 2: Exact Currency Costs (Coins, Voidite, Titanium)
        float c_y = iy + 48.0f * ui_scale;
        std::string c_str = std::to_string(coin_c) + " COINS (" + std::to_string(profile.total_coins) + ")";
        glm::vec4 c_col = coins_ok ? glm::vec4(1.0f, 0.85f, 0.2f, 1.0f) : glm::vec4(1.0f, 0.4f, 0.4f, 1.0f);
        draw_text(c_str, ix + 10.0f * ui_scale, c_y, 0.85f * ui_scale, c_col);

        std::string v_str = std::to_string(void_c) + " VOIDITE (" + std::to_string(profile.total_voidite) + ")";
        glm::vec4 v_col = void_ok ? glm::vec4(0.7f, 0.35f, 1.0f, 1.0f) : glm::vec4(1.0f, 0.4f, 0.4f, 1.0f);
        draw_text(v_str, ix + 10.0f * ui_scale, c_y + 18.0f * ui_scale, 0.85f * ui_scale, v_col);

        std::string t_str = std::to_string(tit_c) + " TITANIUM (" + std::to_string(profile.total_titanium) + ")";
        glm::vec4 t_col = tit_ok ? glm::vec4(0.2f, 0.85f, 1.0f, 1.0f) : glm::vec4(1.0f, 0.4f, 0.4f, 1.0f);
        draw_text(t_str, ix + iw * 0.50f, c_y + 18.0f * ui_scale, 0.85f * ui_scale, t_col);
    }
    iy += cost_box_h + 8.0f * ui_scale;

    // ── Full 5-Level Progression Roadmap Table ──
    float road_h = 100.0f * ui_scale;
    draw_panel_with_border(ix, iy, iw, road_h, glm::vec4(0.03f, 0.05f, 0.08f, 0.88f), glm::vec4(0.18f, 0.28f, 0.38f, 0.5f));
    draw_text("5-TIER PROGRESSION ROADMAP", ix + 8.0f * ui_scale, iy + 5.0f * ui_scale, 0.78f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.8f));

    for (int t = 1; t <= 5; ++t) {
        auto t_info = UpgradeTree::get_tier_cost_info(t);
        float ry = iy + 18.0f * ui_scale + (t - 1) * 15.0f * ui_scale;
        bool is_current = (t == sel_tier);
        bool is_next = (t == sel_tier + 1);

        glm::vec4 r_col = is_current ? glm::vec4(1.0f, 0.85f, 0.2f, 1.0f) :
                          is_next ? glm::vec4(0.3f, 0.95f, 0.55f, 1.0f) :
                          (t < sel_tier) ? glm::vec4(0.5f, 0.6f, 0.7f, 0.7f) :
                                           glm::vec4(0.7f, 0.75f, 0.8f, 0.85f);

        std::string r_prefix = is_current ? "> TIER " : "  TIER ";
        std::string row_str = r_prefix + std::to_string(t) + " (Req Lv " + std::to_string(t_info.required_level) + "): " +
                              std::to_string(t_info.coin_cost) + "C | " +
                              std::to_string(t_info.voidite_cost) + "V | " +
                              std::to_string(t_info.titanium_cost) + "T";
        draw_text_fitted(row_str, ix + 8.0f * ui_scale, ry, iw - 16.0f * ui_scale, 0.75f * ui_scale, r_col);
    }
    iy += road_h + 10.0f * ui_scale;

    // ── Big Tactile Upgrade Button ──
    float btn_w = iw;
    float btn_h = std::clamp(48.0f * ui_scale, 38.0f, 54.0f);
    float bx = ix;
    float by = iy;

    bool btn_hov = (mouse_x >= bx && mouse_x <= bx + btn_w && mouse_y >= by && mouse_y <= by + btn_h);

    if (btn_hov && mouse_clicked && sel_can_buy && !sel_is_maxed) {
        profile.upgrades.purchase(sel_type, player_lvl, profile.total_coins, profile.total_voidite, profile.total_titanium);
        m_profile_dirty = true;
        m_purchase_pulse_timer = 1.0f;
        m_terminal_msg = "UPGRADE ACQUIRED: " + sel_info.name + " [TIER " + std::to_string(sel_tier + 1) + "]";
        m_terminal_msg_col = glm::vec4(0.2f, 0.95f, 0.4f, 1.0f);
    }

    glm::vec4 b_bg = sel_is_maxed ? glm::vec4(0.10f, 0.14f, 0.18f, 0.6f) :
                     !prereq_met ? glm::vec4(0.25f, 0.12f, 0.10f, 0.8f) :
                     (player_lvl < sel_req_lvl) ? glm::vec4(0.25f, 0.10f, 0.12f, 0.8f) :
                     sel_can_buy ? (btn_hov ? glm::vec4(0.2f, 0.75f, 0.95f, 1.0f) : glm::vec4(0.12f, 0.45f, 0.75f, 0.95f)) :
                                   glm::vec4(0.15f, 0.18f, 0.22f, 0.65f);

    glm::vec4 b_border = (sel_can_buy && !sel_is_maxed) ? (btn_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.4f, 0.85f, 1.0f, 0.8f)) :
                         glm::vec4(0.22f, 0.35f, 0.45f, 0.6f);

    draw_panel_with_border(bx, by, btn_w, btn_h, b_bg, b_border, (sel_can_buy && !sel_is_maxed) ? 2.0f : 1.0f);

    if (sel_is_maxed) {
        draw_text_centered_fitted("[MAX RANK MASTERED]", bx, by, btn_w, btn_h, 1.00f * ui_scale, glm::vec4(0.5f, 0.6f, 0.7f, 0.85f));
    } else if (!prereq_met) {
        draw_text_centered_fitted("[LOCKED: REQUIRES " + sel_prereq.required_name + "]", bx, by, btn_w, btn_h, 0.88f * ui_scale, glm::vec4(1.0f, 0.55f, 0.45f, 0.95f));
    } else if (player_lvl < sel_req_lvl) {
        draw_text_centered_fitted("[REQUIRES DELVER LEVEL " + std::to_string(sel_req_lvl) + "]", bx, by, btn_w, btn_h, 0.92f * ui_scale, glm::vec4(1.0f, 0.45f, 0.45f, 0.95f));
    } else if (sel_can_buy) {
        int coin_c = UpgradeTree::get_coin_cost(sel_tier);
        draw_text_centered_fitted("ACQUIRE UPGRADE [" + std::to_string(coin_c) + " COINS]", bx, by, btn_w, btn_h, 0.95f * ui_scale,
                                  btn_hov ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.95f, 0.98f, 1.0f, 1.0f));
    } else {
        std::string reason_str = profile.upgrades.get_lock_reason_string(sel_type, player_lvl, profile.total_coins, profile.total_voidite, profile.total_titanium);
        draw_text_centered_fitted("[" + reason_str + "]", bx, by, btn_w, btn_h, 0.85f * ui_scale, glm::vec4(0.9f, 0.7f, 0.4f, 0.9f));
    }

    // ── Bottom Delver Progression Footer (Horizon Zero Dawn Style) ──
    draw_panel_with_border(start_x, foot_y, terminal_w, foot_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.95f), glm::vec4(0.2f, 0.35f, 0.5f, 0.6f));

    // Current Level Badge
    draw_text("DELVER LEVEL: " + std::to_string(player_lvl), start_x + 12.0f * ui_scale, foot_y + 8.0f * ui_scale, 0.92f * ui_scale, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    // Wide Experience Progress Bar
    float exp_bar_x = start_x + 180.0f * ui_scale;
    float exp_bar_w = terminal_w - 200.0f * ui_scale;
    float exp_bar_h = 16.0f * ui_scale;
    float exp_bar_y = foot_y + (foot_h - exp_bar_h) * 0.5f;

    int cur_exp = profile.total_exp;
    int next_exp = profile.get_next_level_exp_req();
    float exp_progress = (next_exp > 0) ? std::clamp(static_cast<float>(cur_exp) / static_cast<float>(next_exp), 0.0f, 1.0f) : 1.0f;

    draw_rect(exp_bar_x, exp_bar_y, exp_bar_w, exp_bar_h, glm::vec4(0.08f, 0.12f, 0.16f, 0.9f));
    draw_rect(exp_bar_x, exp_bar_y, exp_bar_w * exp_progress, exp_bar_h, glm::vec4(0.2f, 0.75f, 0.95f, 0.85f));
    draw_panel_with_border(exp_bar_x, exp_bar_y, exp_bar_w, exp_bar_h, glm::vec4(0.0f), glm::vec4(0.3f, 0.5f, 0.7f, 0.7f));

    std::string exp_label = "EXP: " + std::to_string(cur_exp) + " / " + std::to_string(next_exp) + " (" + std::to_string(static_cast<int>(exp_progress * 100.0f)) + "%)";
    draw_text_centered_fitted(exp_label, exp_bar_x, exp_bar_y, exp_bar_w, exp_bar_h, 0.75f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 0.95f));
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
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    draw_rect(0, 0, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.96f));

    // Header
    float back_btn_w = std::clamp(160.0f * ui_scale, 130.0f, 180.0f);
    float back_btn_h = std::clamp(28.0f * ui_scale, 24.0f, 34.0f);
    float back_btn_x = 30.0f * ui_scale;
    float back_btn_y = 18.0f * ui_scale;
    bool back_hovered = (mouse_x >= back_btn_x && mouse_x <= back_btn_x + back_btn_w &&
                         mouse_y >= back_btn_y && mouse_y <= back_btn_y + back_btn_h);
    if (back_hovered && mouse_clicked) {
        m_return_to_title = true;
    }

    glm::vec4 back_bg = back_hovered ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG;
    draw_panel_with_border(back_btn_x, back_btn_y, back_btn_w, back_btn_h, back_bg,
                           back_hovered ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f),
                           back_hovered ? 2.0f : 1.0f);
    draw_text_centered("< TITLE SCREEN", back_btn_x, back_btn_y, back_btn_w, back_btn_h, 1.10f * ui_scale,
                       back_hovered ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

    float title_x = back_btn_x + back_btn_w + 16.0f * ui_scale;
    float title_h = FontRenderer::get_rendered_height(1.55f * ui_scale);
    draw_text("ORBITAL HUB // EXPEDITION STAGING & UPGRADE TERMINAL", title_x, back_btn_y + (back_btn_h - title_h) * 0.5f, 1.55f * ui_scale, Typography::COLOR_CYAN);
    draw_rect(30.0f * ui_scale, back_btn_y + back_btn_h + 8.0f * ui_scale, w - 60.0f * ui_scale, 2.0f, glm::vec4(0.15f, 0.85f, 1.0f, 0.6f));

    // Navigation Tabs Header
    float tab_y = back_btn_y + back_btn_h + 16.0f * ui_scale;
    float tab_h = std::clamp(32.0f * ui_scale, 26.0f, 38.0f);
    float tab_w = std::clamp(230.0f * ui_scale, 175.0f, 260.0f);
    float tab_gap = 12.0f * ui_scale;
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
        glm::vec4 border_col = is_cur ? glm::vec4(0.2f, 0.95f, 1.0f, 1.0f) :
                               is_hov ? glm::vec4(0.5f, 0.85f, 1.0f, 0.8f) :
                                        glm::vec4(0.2f, 0.35f, 0.45f, 0.5f);
        draw_panel_with_border(tx, tab_y, tab_w, tab_h, bg_col, border_col, is_cur ? 2.0f : 1.0f);

        draw_text_centered(tab_names[t], tx, tab_y, tab_w, tab_h, 1.10f * ui_scale,
                           is_cur ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : is_hov ? glm::vec4(0.85f, 0.95f, 1.0f, 0.9f) : glm::vec4(0.6f, 0.7f, 0.8f, 0.8f));
    }

    bool launch_triggered = false;

    if (m_active_tab == HubTab::DelverRoster) {
        render_delver_roster(profile, mouse_x, mouse_y, mouse_clicked);
    } else if (m_active_tab == HubTab::UpgradeTerminal) {
        render_upgrade_terminal(profile, mouse_x, mouse_y, mouse_clicked);
    } else {
        // Tab 0: Sector Intel & Skill Matrix
        float total_margin = 60.0f * ui_scale;
        float col_gap = 24.0f * ui_scale;
        float avail_w = w - total_margin * 2.0f - col_gap;
        float col1_x = total_margin;
        float col1_w = avail_w * 0.46f;
        float col2_x = col1_x + col1_w + col_gap;
        float col2_w = avail_w * 0.54f;

        float col_y = tab_y + tab_h + 12.0f * ui_scale;
        float btn_h = std::clamp(42.0f * ui_scale, 36.0f, 48.0f);
        float btn_margin = 16.0f * ui_scale;
        float col_h = h - col_y - btn_h - btn_margin - 12.0f * ui_scale;

        // LEFT COLUMN: SECTOR BRIEFING
        draw_panel_with_border(col1_x, col_y, col1_w, col_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.88f), glm::vec4(0.2f, 0.35f, 0.45f, 0.5f));
        draw_rect(col1_x, col_y, 4.0f, col_h, glm::vec4(0.2f, 0.85f, 1.0f, 0.95f));

        float pad = 16.0f * ui_scale;
        float text_y = col_y + 14.0f * ui_scale;
        draw_text("EXPEDITION SECTOR BRIEFING", col1_x + pad, text_y, 1.40f * ui_scale, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
        text_y += 24.0f * ui_scale;
        draw_rect(col1_x + pad, text_y, col1_w - 2.0f * pad, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));
        text_y += 10.0f * ui_scale;

        std::string sec_name = (selected_level == 1) ? "SECTOR 1: CRYSTALLINE CAVERNS" :
                               (selected_level == 2) ? "SECTOR 2: SUBTERRANEAN VAULT" :
                                                       "SECTOR 3: FAULT-LINE COLLAPSE";
        draw_text(sec_name, col1_x + pad, text_y, 1.25f * ui_scale, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
        text_y += 20.0f * ui_scale;

        std::string sec_badge = profile.sector_records[selected_level].best_badge;
        int sec_rate = profile.sector_records[selected_level].highest_completion_rate;
        std::string rec_str = "RECORD: [" + sec_badge + "] - BEST RATING: " + std::to_string(sec_rate) + "%";
        glm::vec4 rec_col = (sec_badge == "CLEARED (100%)" || sec_badge == "SECTOR CLEARED (100%)") ? glm::vec4(0.2f, 1.0f, 0.4f, 1.0f) :
                            (sec_badge.find("PARTIAL") != std::string::npos) ? glm::vec4(1.0f, 0.75f, 0.0f, 1.0f) :
                            (sec_badge.find("ABANDONED") != std::string::npos || sec_badge == "EXPEDITION ABANDONED") ? glm::vec4(1.0f, 0.35f, 0.35f, 1.0f) :
                                                                   glm::vec4(0.5f, 0.7f, 0.8f, 0.85f);
        draw_text(rec_str, col1_x + pad, text_y, 1.10f * ui_scale, rec_col);
        text_y += 22.0f * ui_scale;

        float line_step = 20.0f * ui_scale;
        if (selected_level == 1) {
            draw_text("Target Depth: 120m | Crust Stability: 85%", col1_x + pad, text_y, 1.10f * ui_scale, glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
            text_y += line_step;
            draw_text("Tactical: ANY DELVER (Standard Cavern Survey)", col1_x + pad, text_y, 1.10f * ui_scale, glm::vec4(0.2f, 0.95f, 0.4f, 1.0f));
            text_y += line_step + 4.0f * ui_scale;
            draw_text("Directives:", col1_x + pad, text_y, 1.20f * ui_scale, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
            text_y += line_step;
            draw_text("  1. Mine 25 Voidite Crystals using Subterranean Drill [LMB]", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            text_y += line_step;
            draw_text("  2. Use Sonar Pulse [Q] to locate rich mineral veins", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            text_y += line_step;
            draw_text("  3. Deploy Extraction Beacon [B] when quota met", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            text_y += line_step;
            draw_text("  4. Defend zone for 40s until evacuation landing pod arrives", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        } else if (selected_level == 2) {
            draw_text("Target Depth: 340m | Crust Stability: 65%", col1_x + pad, text_y, 1.10f * ui_scale, glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
            text_y += line_step;
            draw_text("Tactical: DEMOLITIONIST (Vault Breach Specialist)", col1_x + pad, text_y, 1.10f * ui_scale, glm::vec4(1.0f, 0.65f, 0.2f, 1.0f));
            text_y += line_step + 4.0f * ui_scale;
            draw_text("Directives:", col1_x + pad, text_y, 1.20f * ui_scale, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
            text_y += line_step;
            draw_text("  1. Track subterranean vault signatures using Sonar [Q]", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            text_y += line_step;
            draw_text("  2. Equip Demolition Charges [3] to blast Reinforced Doors", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            text_y += line_step;
            draw_text("  3. Mine 30 Voidite Crystals OR breach vault for the Hyper-Core Relic", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            text_y += line_step;
            draw_text("  4. Call beacon [B] once quota met or relic secured; defend evac pod", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        } else {
            draw_text("Target Depth: 600m | Crust Stability: CRITICAL (Collapse Imminent)", col1_x + pad, text_y, 1.10f * ui_scale, glm::vec4(1.0f, 0.3f, 0.3f, 0.9f));
            text_y += line_step;
            draw_text("Tactical: VANGUARD (Fault-Line Seismic Defense)", col1_x + pad, text_y, 1.10f * ui_scale, glm::vec4(0.4f, 0.85f, 1.0f, 1.0f));
            text_y += line_step + 4.0f * ui_scale;
            draw_text("Directives:", col1_x + pad, text_y, 1.20f * ui_scale, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
            text_y += line_step;
            draw_text("  1. 180-SECOND HARD COUNTDOWN: Subterranean collapse timer active", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(1.0f, 0.35f, 0.2f, 0.95f));
            text_y += line_step;
            draw_text("  2. Mine 50 Voidite Crystals while surviving recurring cave-ins", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
            text_y += line_step;
            draw_text("  3. Reach the emergency drop pod before seismic fault collapse", col1_x + pad, text_y, 1.05f * ui_scale, glm::vec4(0.8f, 0.85f, 0.9f, 0.9f));
        }

        // RIGHT COLUMN: PROFICIENCY MATRICES
        draw_panel_with_border(col2_x, col_y, col2_w, col_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.88f), glm::vec4(0.2f, 0.35f, 0.45f, 0.5f));
        draw_rect(col2_x, col_y, 4.0f, col_h, glm::vec4(1.0f, 0.75f, 0.2f, 0.95f));

        float r_pad = 16.0f * ui_scale;
        float r_text_y = col_y + 14.0f * ui_scale;
        draw_text("DELVER PROFICIENCY MATRICES", col2_x + r_pad, r_text_y, 1.40f * ui_scale, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
        r_text_y += 24.0f * ui_scale;
        draw_rect(col2_x + r_pad, r_text_y, col2_w - 2.0f * r_pad, 1.0f, glm::vec4(0.4f, 0.35f, 0.2f, 0.5f));
        r_text_y += 12.0f * ui_scale;

        const SkillBranch* branches[4] = {
            &skills.demolitions,
            &skills.surveying,
            &skills.suit,
            &skills.acrobatics
        };

        float branch_step = (col_h - 70.0f * ui_scale) / 4.0f;
        for (int i = 0; i < 4; ++i) {
            const auto& b = *branches[i];
            float by = r_text_y + i * branch_step;
            std::string branch_title = b.name + " (" + std::to_string(b.xp) + "/" + std::to_string(b.xp_for_unlock) + " XP)";
            if (i == 1) {
                int rk = skills.get_surveying_rank();
                branch_title = b.name + " [RANK " + std::to_string(rk) + "] (" + std::to_string(b.xp) + " XP)";
            }
            draw_text(branch_title,
                      col2_x + r_pad, by, 1.20f * ui_scale, b.unlocked ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.85f, 0.9f, 0.95f, 1.0f));

            float bbar_y = by + 16.0f * ui_scale;
            float bbar_h = 7.0f * ui_scale;
            draw_rect(col2_x + r_pad, bbar_y, col2_w - 2.0f * r_pad, bbar_h, glm::vec4(0.12f, 0.15f, 0.18f, 0.8f));
            glm::vec4 bar_col = b.unlocked ? glm::vec4(0.2f, 0.9f, 0.35f, 0.95f) : glm::vec4(0.2f, 0.75f, 1.0f, 0.95f);
            draw_rect(col2_x + r_pad, bbar_y, (col2_w - 2.0f * r_pad) * b.progress(), bbar_h, bar_col);

            std::string perk_status = b.unlocked ? "[UNLOCKED] " : "[LOCKED] ";
            std::string perk_line = perk_status + b.unlock_perk_name + ": " + b.unlock_description;
            draw_text(perk_line, col2_x + r_pad, bbar_y + bbar_h + 5.0f * ui_scale, 1.02f * ui_scale,
                      b.unlocked ? glm::vec4(0.3f, 0.9f, 0.5f, 0.95f) : glm::vec4(0.6f, 0.65f, 0.7f, 0.8f));
        }

        // Launch expedition banner
        float btn_y = h - btn_h - btn_margin;
        float btn_w = w - total_margin * 2.0f;
        float btn_x = total_margin;

        bool btn_hovered = (mouse_x >= btn_x && mouse_x <= btn_x + btn_w &&
                            mouse_y >= btn_y && mouse_y <= btn_y + btn_h);
        if (btn_hovered && mouse_clicked) {
            launch_triggered = true;
        }

        glm::vec4 btn_col = btn_hovered ? glm::vec4(0.25f, 0.65f, 0.95f, 1.0f) : glm::vec4(0.15f, 0.45f, 0.75f, 0.95f);
        draw_panel_with_border(btn_x, btn_y, btn_w, btn_h, btn_col, btn_hovered ? glm::vec4(0.8f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.3f, 0.6f, 0.9f, 0.8f), btn_hovered ? 2.0f : 1.0f);

        draw_text_centered("[CLICK OR PRESS SPACE / ENTER TO LAUNCH EXPEDITION]    |    [TAB / ESC TO RETURN TO MENU]",
                           btn_x, btn_y, btn_w, btn_h, 1.25f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
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
        int run_coins = inventory.run_coins_earned;
        if (run_coins <= 0) {
            run_coins = (success ? 60 : 20) + inventory.run_completion_rate + (inventory.voidite * 2 + inventory.titanium * 3);
            if (inventory.is_abandoned) run_coins /= 2;
        }

        int lvls_gained = 0;
        int bonus_coins = 0;
        profile.add_exp(run_exp, lvls_gained, bonus_coins);
        profile.grant_coins(run_coins);
        profile.total_voidite += inventory.voidite;
        profile.total_titanium += inventory.titanium;

        if (lvls_gained > 0) {
            m_terminal_msg = "LEVEL UP! REACHED DELVER RANK " + std::to_string(profile.get_player_level()) + " (+" + std::to_string(bonus_coins) + " BONUS COINS)";
            m_terminal_msg_col = glm::vec4(1.0f, 0.85f, 0.2f, 1.0f);
        }

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
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    glm::vec4 bg_col = inventory.suit_failed ? glm::vec4(0.08f, 0.015f, 0.02f, 0.98f) : glm::vec4(0.02f, 0.03f, 0.05f, 0.96f);
    draw_rect(0, 0, w, h, bg_col);

    // Full-screen perimeter hazard border for Delver death / critical failure
    if (inventory.suit_failed) {
        float f_border = 6.0f * ui_scale;
        glm::vec4 death_crimson(0.95f, 0.15f, 0.12f, 0.85f);
        draw_rect(0, 0, w, f_border, death_crimson);
        draw_rect(0, h - f_border, w, f_border, death_crimson);
        draw_rect(0, 0, f_border, h, death_crimson);
        draw_rect(w - f_border, 0, f_border, h, death_crimson);
    }

    // Header banner
    float banner_y = 16.0f * ui_scale;
    float banner_h = 36.0f * ui_scale;
    std::string status_text = inventory.is_abandoned ? "EXPEDITION ABANDONED // SALVAGE PENALTY (50%)" :
                              inventory.suit_failed ? "[ ! ] EXPEDITION FAILED: DELVER M.I.A. [ ! ]" :
                              success ? "EXPEDITION EXTRACTION SUCCESSFUL" : "EXPEDITION FAILED";
    glm::vec4 status_col = (success && !inventory.is_abandoned && !inventory.suit_failed) ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) :
                           inventory.is_abandoned ? glm::vec4(1.0f, 0.70f, 0.0f, 1.0f) :
                           glm::vec4(1.0f, 0.22f, 0.18f, 1.0f);

    draw_text_centered_fitted(status_text, 16.0f, banner_y, w - 32.0f, banner_h, 1.70f * ui_scale, status_col, 0.90f * ui_scale);
    float div_w = std::clamp(w * 0.75f, 360.0f, 1000.0f);
    draw_rect((w - div_w) * 0.5f, banner_y + banner_h + 2.0f * ui_scale, div_w, 2.0f, status_col * 0.7f);

    float total_content_w = std::clamp(w * 0.92f, 620.0f, std::min(w - 24.0f, 1380.0f * ui_scale));
    float col_gap = std::clamp(16.0f * ui_scale, 10.0f, 24.0f);
    float col_w = (total_content_w - col_gap) * 0.5f;
    float start_x = (w - total_content_w) * 0.5f;
    float col1_x = start_x;
    float col2_x = start_x + col_w + col_gap;

    float bot_btn_h = std::clamp(42.0f * ui_scale, 36.0f, 48.0f);
    float bot_margin = 12.0f * ui_scale;
    float panel_y = banner_y + banner_h + 10.0f * ui_scale;
    float panel_h = h - panel_y - bot_btn_h - bot_margin - 10.0f * ui_scale;

    // LEFT COLUMN: SUBTERRANEAN EXPEDITION SUMMARY
    draw_panel_with_border(col1_x, panel_y, col_w, panel_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.92f), glm::vec4(0.2f, 0.35f, 0.45f, 0.6f));
    draw_rect(col1_x, panel_y, 4.0f, panel_h, status_col);

    float l_pad = std::clamp(14.0f * ui_scale, 10.0f, 18.0f);
    float content_inner_w = col_w - 2.0f * l_pad;
    float cur_y = panel_y + 10.0f * ui_scale;

    draw_text_fitted("EXPEDITION HARVEST & MANIFEST", col1_x + l_pad, cur_y, content_inner_w, 1.25f * ui_scale, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    cur_y += 22.0f * ui_scale;

    std::string badge_disp = "[" + inventory.run_outcome_badge + "]  --  " + std::to_string(inventory.run_completion_rate) + "% COMPLETION";
    draw_text_fitted(badge_disp, col1_x + l_pad, cur_y, content_inner_w, 1.15f * ui_scale, status_col);
    cur_y += 18.0f * ui_scale;
    draw_rect(col1_x + l_pad, cur_y, content_inner_w, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));
    cur_y += 8.0f * ui_scale;

    auto draw_stat_row = [&](const std::string& label, const std::string& val, const glm::vec4& lcol, const glm::vec4& vcol, float scale) {
        float row_scale = FontRenderer::fit_scale(label + " " + val, content_inner_w, scale, 0.65f * ui_scale);
        float val_w = FontRenderer::get_rendered_width(val, row_scale);
        draw_text(label, col1_x + l_pad, cur_y, row_scale, lcol);
        draw_text(val, col1_x + col_w - l_pad - val_w, cur_y, row_scale, vcol);
    };

    float stat_step = std::clamp(22.0f * ui_scale, 18.0f, 26.0f);

    draw_stat_row("Voidite Extracted:", std::to_string(inventory.voidite) + " units (" + std::to_string(inventory.voidite * 5) + " pts)",
                  glm::vec4(0.7f, 0.85f, 0.95f, 1.0f), glm::vec4(0.0f, 0.94f, 1.0f, 1.0f), 1.10f * ui_scale);
    cur_y += stat_step;

    draw_stat_row("Titanium Cores:", std::to_string(inventory.titanium) + " cores (" + std::to_string(inventory.titanium * 6) + " pts)",
                  glm::vec4(0.7f, 0.85f, 0.95f, 1.0f), glm::vec4(1.0f, 0.70f, 0.0f, 1.0f), 1.10f * ui_scale);
    cur_y += stat_step;

    draw_stat_row("Scrap Metal Salvaged:", std::to_string(inventory.scrap_metal) + " scrap",
                  glm::vec4(0.7f, 0.85f, 0.95f, 1.0f), glm::vec4(0.85f, 0.85f, 0.9f, 1.0f), 1.10f * ui_scale);
    cur_y += stat_step;

    draw_stat_row("Mineral Salvage:", std::to_string(inventory.salvage_parts) + " units (" + std::to_string(inventory.salvage_parts * 2) + " pts)",
                  glm::vec4(0.7f, 0.85f, 0.95f, 1.0f), glm::vec4(0.7f, 0.75f, 0.8f, 1.0f), 1.10f * ui_scale);
    cur_y += stat_step;

    if (level == 2) {
        std::string r_val = inventory.relic_extracted ? "RECOVERED (+250 pts)" : "NOT RECOVERED";
        glm::vec4 r_col = inventory.relic_extracted ? glm::vec4(1.0f, 0.0f, 0.85f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.6f, 0.8f);
        draw_stat_row("Hyper-Core Relic:", r_val, glm::vec4(0.7f, 0.85f, 0.95f, 1.0f), r_col, 1.10f * ui_scale);
        cur_y += stat_step;
    }

    draw_rect(col1_x + l_pad, cur_y, content_inner_w, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));
    cur_y += 6.0f * ui_scale;

    // Currency Rewards: Coins & Player EXP
    int final_run_coins = inventory.run_coins_earned;
    if (final_run_coins <= 0) {
        final_run_coins = (success ? 60 : 20) + inventory.run_completion_rate + (inventory.voidite * 2 + inventory.titanium * 3);
        if (inventory.is_abandoned) final_run_coins /= 2;
    }
    int final_run_exp = inventory.total_run_score > 0 ? inventory.total_run_score :
                        (inventory.voidite * 5 + inventory.titanium * 6 + inventory.salvage_parts * 2);

    draw_stat_row("COINS EARNED:", "+" + std::to_string(final_run_coins) + " COINS",
                  glm::vec4(1.0f, 0.85f, 0.2f, 1.0f), glm::vec4(1.0f, 0.95f, 0.4f, 1.0f), 1.15f * ui_scale);
    cur_y += stat_step;

    draw_stat_row("EXP BANKED:", "+" + std::to_string(final_run_exp) + " EXP (RANK " + std::to_string(profile.get_player_level()) + ")",
                  glm::vec4(0.2f, 0.95f, 0.4f, 1.0f), glm::vec4(0.35f, 1.0f, 0.5f, 1.0f), 1.15f * ui_scale);
    cur_y += stat_step;

    draw_rect(col1_x + l_pad, cur_y, content_inner_w, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));
    cur_y += 6.0f * ui_scale;

    std::string score_str = "TOTAL SCORE: " + std::to_string(inventory.total_run_score) + " POINTS";
    draw_text_fitted(score_str, col1_x + l_pad, cur_y, content_inner_w, 1.25f * ui_scale, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
    cur_y += stat_step;

    draw_text_fitted("DELVER SPECIALIZATIONS XP:", col1_x + l_pad, cur_y, content_inner_w, 1.10f * ui_scale, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    cur_y += 16.0f * ui_scale;
    draw_text_fitted("Demolitions: " + std::to_string(skills.demolitions.xp) + " XP" + (skills.demolitions.unlocked ? " [ACTIVE]" : ""),
                     col1_x + l_pad, cur_y, content_inner_w, 0.95f * ui_scale, skills.demolitions.unlocked ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));
    cur_y += 14.0f * ui_scale;
    draw_text_fitted("Surveying:   " + std::to_string(skills.surveying.xp) + " XP (Rank " + std::to_string(skills.get_surveying_rank()) + ")" + (skills.surveying.unlocked ? " [ACTIVE]" : ""),
                     col1_x + l_pad, cur_y, content_inner_w, 0.95f * ui_scale, skills.surveying.unlocked ? glm::vec4(0.3f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.7f, 0.75f, 0.8f, 0.9f));

    // RIGHT COLUMN: PERSISTENT ACCOUNT POOL & QUICK UPGRADES
    draw_panel_with_border(col2_x, panel_y, col_w, panel_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.92f), glm::vec4(0.2f, 0.35f, 0.45f, 0.6f));
    draw_rect(col2_x, panel_y, 4.0f, panel_h, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));

    float r_pad = std::clamp(14.0f * ui_scale, 10.0f, 18.0f);
    float r_inner_w = col_w - 2.0f * r_pad;
    float r_y = panel_y + 10.0f * ui_scale;
    draw_text_fitted("PERSISTENT ACCOUNT POOL", col2_x + r_pad, r_y, r_inner_w, 1.25f * ui_scale, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    r_y += 22.0f * ui_scale;
    draw_rect(col2_x + r_pad, r_y, r_inner_w, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));
    r_y += 8.0f * ui_scale;

    int debrief_player_lvl = profile.get_player_level();
    std::string pool_str = "RANK: LVL " + std::to_string(debrief_player_lvl) +
                           "  |  " + std::to_string(profile.total_coins) + " COINS  |  " +
                           std::to_string(profile.total_exp) + " EXP";
    draw_text_fitted(pool_str, col2_x + r_pad, r_y, r_inner_w, 1.10f * ui_scale, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
    r_y += 22.0f * ui_scale;

    // Show 4 Quick Upgrades in Debrief
    UpgradeType quick_types[4] = {
        UpgradeType::DrillSpeed,
        UpgradeType::ThrusterTank,
        UpgradeType::SonarFrequency,
        UpgradeType::ReinforcedPlating
    };

    float up_gap = std::clamp(8.0f * ui_scale, 6.0f, 10.0f);
    float up_w = r_inner_w;
    float avail_up_h = panel_h - (r_y - panel_y) - 10.0f * ui_scale;
    float up_h = std::clamp((avail_up_h - 3.0f * up_gap) / 4.0f, 48.0f * ui_scale, 74.0f * ui_scale);

    for (int i = 0; i < 4; ++i) {
        UpgradeType type = quick_types[i];
        auto info = UpgradeTree::get_info(type);
        int cur_tier = profile.upgrades.get_tier(type);
        int req_lvl = UpgradeTree::get_required_level_for_tier(cur_tier);
        bool is_maxed = (cur_tier >= UpgradeTree::MAX_TIER);
        bool level_locked = (!is_maxed && debrief_player_lvl < req_lvl);

        int coin_cost = UpgradeTree::get_coin_cost(cur_tier);
        bool can_buy = profile.upgrades.can_purchase(type, debrief_player_lvl, profile.total_coins, profile.total_voidite, profile.total_titanium);

        float ux = col2_x + r_pad;
        float uy = r_y + i * (up_h + up_gap);

        draw_panel_with_border(ux, uy, up_w, up_h, glm::vec4(0.06f, 0.08f, 0.12f, 0.85f), glm::vec4(0.15f, 0.25f, 0.35f, 0.5f));

        // Upgrade button
        float btn_w = std::clamp(120.0f * ui_scale, 95.0f, 150.0f);
        float btn_h = std::clamp(up_h - 12.0f * ui_scale, 30.0f, 46.0f);
        float bx = ux + up_w - btn_w - 6.0f * ui_scale;
        float by = uy + (up_h - btn_h) * 0.5f;

        float text_max_w = bx - ux - 12.0f * ui_scale;

        // Upgrade name auto-scaled to available column width
        std::string title = info.name + " [LVL " + std::to_string(cur_tier) + "/5]";
        draw_text_fitted(title, ux + 8.0f * ui_scale, uy + 6.0f * ui_scale, text_max_w, 0.95f * ui_scale, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f), 0.65f * ui_scale);

        std::string stat_prev = profile.upgrades.get_stat_preview(type);
        draw_text_fitted(stat_prev, ux + 8.0f * ui_scale, uy + 24.0f * ui_scale, text_max_w, 0.88f * ui_scale, glm::vec4(0.65f, 0.75f, 0.85f, 0.85f), 0.60f * ui_scale);

        bool btn_hov = (mouse_x >= bx && mouse_x <= bx + btn_w && mouse_y >= by && mouse_y <= by + btn_h);

        if (btn_hov && mouse_clicked && can_buy && !is_maxed) {
            profile.upgrades.purchase(type, debrief_player_lvl, profile.total_coins, profile.total_voidite, profile.total_titanium);
            m_profile_dirty = true;
        }

        glm::vec4 b_bg = is_maxed ? glm::vec4(0.1f, 0.12f, 0.15f, 0.5f) :
                         level_locked ? glm::vec4(0.22f, 0.10f, 0.12f, 0.7f) :
                         can_buy  ? (btn_hov ? glm::vec4(0.2f, 0.6f, 0.85f, 1.0f) : glm::vec4(0.1f, 0.35f, 0.6f, 0.9f)) :
                                    glm::vec4(0.1f, 0.12f, 0.15f, 0.6f);
        glm::vec4 b_border = (can_buy && !is_maxed) ? (btn_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.4f, 0.85f, 1.0f, 0.8f)) :
                             glm::vec4(0.2f, 0.35f, 0.45f, 0.5f);
        draw_panel_with_border(bx, by, btn_w, btn_h, b_bg, b_border);

        if (is_maxed) {
            draw_text_centered_fitted("MAXED", bx, by, btn_w, btn_h, 1.05f * ui_scale, glm::vec4(0.5f, 0.5f, 0.5f, 0.7f));
        } else if (level_locked) {
            std::string req_str = "[REQ LVL " + std::to_string(req_lvl) + "]";
            draw_text_centered_fitted(req_str, bx, by, btn_w, btn_h, 0.95f * ui_scale, glm::vec4(1.0f, 0.45f, 0.45f, 0.9f));
        } else {
            std::string cost_str = std::to_string(coin_cost) + " COINS";
            draw_text_centered_fitted("UPGRADE", bx, by + 2.0f * ui_scale, btn_w, btn_h * 0.5f, 0.95f * ui_scale,
                               can_buy ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.5f, 0.5f, 0.5f, 0.7f));
            draw_text_centered_fitted(cost_str, bx, by + btn_h * 0.5f - 2.0f * ui_scale, btn_w, btn_h * 0.5f, 0.88f * ui_scale,
                               can_buy ? glm::vec4(1.0f, 0.85f, 0.2f, 1.0f) : glm::vec4(0.45f, 0.45f, 0.45f, 0.7f));
        }
    }

    // BOTTOM BUTTONS: LAUNCH NEXT SECTOR & RETURN TO HUB
    DebriefAction result = DebriefAction::None;
    float bot_y = panel_y + panel_h + 10.0f * ui_scale;
    float bot_btn_w = (total_content_w - col_gap) * 0.5f;

    // Button 1: [LAUNCH NEXT SECTOR] or [RE-DEPLOY EXPEDITION]
    float b1_x = start_x;
    bool b1_hov = (mouse_x >= b1_x && mouse_x <= b1_x + bot_btn_w && mouse_y >= bot_y && mouse_y <= bot_y + bot_btn_h);
    if (b1_hov && mouse_clicked) {
        if (inventory.suit_failed || !success) {
            result = DebriefAction::RedeployExpedition;
        } else {
            result = DebriefAction::LaunchNextSector;
        }
    }
    std::string b1_label = (inventory.suit_failed || !success) ? "[RE-DEPLOY EXPEDITION]" : "[LAUNCH NEXT SECTOR]";
    glm::vec4 b1_col = (inventory.suit_failed || !success)
        ? (b1_hov ? glm::vec4(0.85f, 0.25f, 0.15f, 1.0f) : glm::vec4(0.60f, 0.15f, 0.10f, 0.95f))
        : (b1_hov ? glm::vec4(0.15f, 0.65f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.45f, 0.38f, 0.95f));
    glm::vec4 b1_border = (inventory.suit_failed || !success)
        ? (b1_hov ? glm::vec4(1.0f, 0.5f, 0.3f, 1.0f) : glm::vec4(0.8f, 0.2f, 0.15f, 0.8f))
        : (b1_hov ? glm::vec4(0.4f, 1.0f, 0.8f, 1.0f) : glm::vec4(0.2f, 0.6f, 0.5f, 0.7f));
    draw_panel_with_border(b1_x, bot_y, bot_btn_w, bot_btn_h, b1_col, b1_border, b1_hov ? 2.0f : 1.0f);
    draw_text_centered_fitted(b1_label, b1_x, bot_y, bot_btn_w, bot_btn_h, 1.25f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // Button 2: [RETURN TO ORBITAL HUB]
    float b2_x = start_x + bot_btn_w + col_gap;
    bool b2_hov = (mouse_x >= b2_x && mouse_x <= b2_x + bot_btn_w && mouse_y >= bot_y && mouse_y <= bot_y + bot_btn_h);
    if (b2_hov && mouse_clicked) {
        result = DebriefAction::ReturnToHub;
    }
    glm::vec4 b2_col = b2_hov ? glm::vec4(0.2f, 0.45f, 0.75f, 1.0f) : glm::vec4(0.12f, 0.3f, 0.55f, 0.95f);
    draw_panel_with_border(b2_x, bot_y, bot_btn_w, bot_btn_h, b2_col, b2_hov ? glm::vec4(0.6f, 0.85f, 1.0f, 1.0f) : glm::vec4(0.3f, 0.5f, 0.7f, 0.7f), b2_hov ? 2.0f : 1.0f);
    draw_text_centered_fitted("[RETURN TO ORBITAL HUB]", b2_x, bot_y, bot_btn_w, bot_btn_h, 1.30f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return result;
}

void OrbitalHubUI::render_audio_settings(UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    // Dark backdrop if in standalone subview
    if (m_subview == MenuSubView::Settings) {
        draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.95f));

        // Back button
        float back_w = 160.0f * ui_scale;
        float back_h = 38.0f * ui_scale;
        float back_x = 36.0f * ui_scale;
        float back_y = 20.0f * ui_scale;
        bool back_hover = (mouse_x >= back_x && mouse_x <= back_x + back_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (back_hover && mouse_clicked) {
            m_subview = MenuSubView::Main;
        }
        draw_panel_with_border(back_x, back_y, back_w, back_h,
                               back_hover ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG,
                               back_hover ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f),
                               back_hover ? 2.0f : 1.0f);
        draw_text_centered("< MAIN MENU", back_x, back_y, back_w, back_h, 1.25f * ui_scale, back_hover ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);
    }

    GameSettings& s = profile.settings;

    // Centered audio mixer panel
    float panel_w = std::clamp(w * 0.70f, 600.0f, 850.0f);
    float panel_h = std::clamp(h * 0.85f, 520.0f, 720.0f);
    float panel_x = (w - panel_w) * 0.5f;
    float panel_y = (m_subview == MenuSubView::Settings) ? ((h - panel_h) * 0.5f + 16.0f * ui_scale) : (80.0f * ui_scale);

    draw_panel_with_border(panel_x, panel_y, panel_w, panel_h, glm::vec4(0.04f, 0.07f, 0.11f, 0.95f), glm::vec4(0.2f, 0.45f, 0.65f, 0.6f), 2.0f);
    draw_rect(panel_x, panel_y, panel_w, 3.0f, Typography::COLOR_CYAN);

    float pad = 24.0f * ui_scale;
    float content_w = panel_w - 2.0f * pad;
    float cur_y = panel_y + 16.0f * ui_scale;

    draw_text("// TACTICAL RIG & SOUND MIXER", panel_x + pad, cur_y, 1.80f * ui_scale, Typography::COLOR_CYAN);
    cur_y += 28.0f * ui_scale;
    draw_text("ACOUSTIC COMFORT PROTOCOL // 5-STAGE MASTERING & SOUND CHANNEL OPTIONS", panel_x + pad, cur_y, 1.12f * ui_scale, glm::vec4(0.6f, 0.75f, 0.85f, 0.85f));
    cur_y += 18.0f * ui_scale;
    draw_rect(panel_x + pad, cur_y, content_w, 1.0f, glm::vec4(0.2f, 0.4f, 0.55f, 0.6f));
    cur_y += 14.0f * ui_scale;

    float adj_btn_w = std::clamp(42.0f * ui_scale, 34.0f, 48.0f);
    float adj_btn_h = std::clamp(24.0f * ui_scale, 20.0f, 28.0f);
    float btn2_x = panel_x + pad + content_w - 14.0f * ui_scale - adj_btn_w;
    float btn1_x = btn2_x - adj_btn_w - 8.0f * ui_scale;

    auto draw_volume_row = [&](const std::string& name, float& vol, float row_y, bool is_master = false) {
        char val_buf[64];
        if (is_master && s.mute_all) {
            std::snprintf(val_buf, sizeof(val_buf), "%s: [MUTED]", name.c_str());
        } else {
            std::snprintf(val_buf, sizeof(val_buf), "%s: %3d%%", name.c_str(), static_cast<int>(std::round(vol * 100.0f)));
        }

        glm::vec4 text_col = (is_master && s.mute_all) ? glm::vec4(1.0f, 0.4f, 0.4f, 1.0f) : glm::vec4(0.85f, 0.92f, 0.98f, 0.95f);
        draw_text(val_buf, panel_x + pad + 14.0f * ui_scale, row_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f, 1.15f * ui_scale, text_col);

        // Graphical Volume Level Bar
        float meter_w = 120.0f * ui_scale;
        float meter_h = 12.0f * ui_scale;
        float meter_x = btn1_x - meter_w - (is_master ? (adj_btn_w * 1.6f + 16.0f * ui_scale) : 14.0f * ui_scale);
        float meter_y = row_y + (adj_btn_h - meter_h) * 0.5f;

        draw_rect(meter_x, meter_y, meter_w, meter_h, glm::vec4(0.06f, 0.10f, 0.14f, 1.0f));
        float fill_w = meter_w * std::clamp(vol, 0.0f, 1.0f);
        if (!is_master || !s.mute_all) {
            glm::vec4 meter_col = (vol > 0.75f) ? glm::vec4(0.0f, 0.94f, 1.0f, 0.95f) :
                                  (vol > 0.35f) ? glm::vec4(0.2f, 0.85f, 0.5f, 0.95f) :
                                                  glm::vec4(1.0f, 0.75f, 0.1f, 0.95f);
            if (fill_w > 1.0f) {
                draw_rect(meter_x, meter_y, fill_w, meter_h, meter_col);
            }
        }

        // Mute button for master
        if (is_master) {
            float mute_w = adj_btn_w * 1.55f;
            float mute_x = btn1_x - mute_w - 8.0f * ui_scale;
            bool m_hov = (mouse_x >= mute_x && mouse_x <= mute_x + mute_w && mouse_y >= row_y && mouse_y <= row_y + adj_btn_h);
            glm::vec4 m_bg = s.mute_all ? glm::vec4(0.45f, 0.08f, 0.08f, 1.0f) :
                             m_hov      ? glm::vec4(0.25f, 0.35f, 0.45f, 1.0f) :
                                          glm::vec4(0.08f, 0.14f, 0.20f, 1.0f);
            glm::vec4 m_border = s.mute_all ? glm::vec4(1.0f, 0.3f, 0.3f, 0.9f) :
                                 m_hov      ? Typography::COLOR_CYAN_GLOW :
                                              glm::vec4(0.2f, 0.5f, 0.7f, 0.6f);
            draw_panel_with_border(mute_x, row_y, mute_w, adj_btn_h, m_bg, m_border);
            draw_text_centered(s.mute_all ? "UNMUTE" : "MUTE", mute_x, row_y, mute_w, adj_btn_h, 1.10f * ui_scale,
                               s.mute_all ? glm::vec4(1.0f, 0.5f, 0.5f, 1.0f) : glm::vec4(1.0f));
            if (mouse_clicked && m_hov) {
                s.mute_all = !s.mute_all;
                m_profile_dirty = true;
            }
        }

        // [-] and [+] steppers
        bool b1_hov = (mouse_x >= btn1_x && mouse_x <= btn1_x + adj_btn_w && mouse_y >= row_y && mouse_y <= row_y + adj_btn_h);
        bool b2_hov = (mouse_x >= btn2_x && mouse_x <= btn2_x + adj_btn_w && mouse_y >= row_y && mouse_y <= row_y + adj_btn_h);
        draw_panel_with_border(btn1_x, row_y, adj_btn_w, adj_btn_h, b1_hov ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
        draw_panel_with_border(btn2_x, row_y, adj_btn_w, adj_btn_h, b2_hov ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
        draw_text_centered("-", btn1_x, row_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
        draw_text_centered("+", btn2_x, row_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));

        if (mouse_clicked && b1_hov) {
            vol = std::max(0.0f, std::round((vol - 0.10f) * 10.0f) / 10.0f);
            if (is_master) s.mute_all = false;
            m_profile_dirty = true;
        }
        if (mouse_clicked && b2_hov) {
            vol = std::min(1.0f, std::round((vol + 0.10f) * 10.0f) / 10.0f);
            if (is_master) s.mute_all = false;
            m_profile_dirty = true;
        }
    };

    float row_h = adj_btn_h + 8.0f * ui_scale;

    // 1. Master Volume
    draw_volume_row("MASTER SYNTHESIZER", s.master_volume, cur_y, true);
    cur_y += row_h;

    // 2. SFX / Mining Volume
    draw_volume_row("SFX & MINING TOOLS", s.sfx_volume, cur_y, false);
    cur_y += row_h;

    // 3. Enemy Volume
    draw_volume_row("HOSTILE / VOID STALKERS", s.enemy_volume, cur_y, false);
    cur_y += row_h;

    // 4. Ambience Volume
    draw_volume_row("CAVERN & HAZARDS", s.ambient_volume, cur_y, false);
    cur_y += row_h;

    // 5. UI Volume
    draw_volume_row("UI & SYSTEM ALARMS", s.ui_volume, cur_y, false);
    cur_y += row_h + 6.0f * ui_scale;

    draw_rect(panel_x + pad + 14.0f * ui_scale, cur_y, content_w - 28.0f * ui_scale, 1.0f, glm::vec4(0.15f, 0.3f, 0.45f, 0.4f));
    cur_y += 10.0f * ui_scale;

    // 6. Sensitivity
    char sens_buf[32];
    std::snprintf(sens_buf, sizeof(sens_buf), "MOUSE SENSITIVITY: %.2f", s.mouse_sensitivity);
    draw_text(sens_buf, panel_x + pad + 14.0f * ui_scale, cur_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f, 1.15f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    bool s_hover1 = (mouse_x >= btn1_x && mouse_x <= btn1_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    bool s_hover2 = (mouse_x >= btn2_x && mouse_x <= btn2_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    draw_panel_with_border(btn1_x, cur_y, adj_btn_w, adj_btn_h, s_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_panel_with_border(btn2_x, cur_y, adj_btn_w, adj_btn_h, s_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_text_centered("-", btn1_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    draw_text_centered("+", btn2_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    if (mouse_clicked && s_hover1) { s.mouse_sensitivity = std::max(0.04f, s.mouse_sensitivity - 0.02f); m_profile_dirty = true; }
    if (mouse_clicked && s_hover2) { s.mouse_sensitivity = std::min(0.40f, s.mouse_sensitivity + 0.02f); m_profile_dirty = true; }
    cur_y += row_h;

    // 7. FOV
    char fov_buf[32];
    std::snprintf(fov_buf, sizeof(fov_buf), "FIELD OF VIEW:     %d DEG", static_cast<int>(s.fov));
    draw_text(fov_buf, panel_x + pad + 14.0f * ui_scale, cur_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f, 1.15f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    bool f_hover1 = (mouse_x >= btn1_x && mouse_x <= btn1_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    bool f_hover2 = (mouse_x >= btn2_x && mouse_x <= btn2_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    draw_panel_with_border(btn1_x, cur_y, adj_btn_w, adj_btn_h, f_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_panel_with_border(btn2_x, cur_y, adj_btn_w, adj_btn_h, f_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_text_centered("-", btn1_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    draw_text_centered("+", btn2_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    if (mouse_clicked && f_hover1) { s.fov = std::max(60.0f, s.fov - 5.0f); m_profile_dirty = true; }
    if (mouse_clicked && f_hover2) { s.fov = std::min(105.0f, s.fov + 5.0f); m_profile_dirty = true; }
    cur_y += row_h;

    // 8. Cavern Brightness
    char bright_buf[32];
    std::snprintf(bright_buf, sizeof(bright_buf), "CAVERN BRIGHTNESS: %d%%", static_cast<int>(std::round(s.brightness * 100.0f)));
    draw_text(bright_buf, panel_x + pad + 14.0f * ui_scale, cur_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f, 1.15f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    bool b_hover1 = (mouse_x >= btn1_x && mouse_x <= btn1_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    bool b_hover2 = (mouse_x >= btn2_x && mouse_x <= btn2_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    draw_panel_with_border(btn1_x, cur_y, adj_btn_w, adj_btn_h, b_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_panel_with_border(btn2_x, cur_y, adj_btn_w, adj_btn_h, b_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_text_centered("-", btn1_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    draw_text_centered("+", btn2_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    if (mouse_clicked && b_hover1) { s.brightness = std::max(0.40f, s.brightness - 0.10f); m_profile_dirty = true; }
    if (mouse_clicked && b_hover2) { s.brightness = std::min(2.00f, s.brightness + 0.10f); m_profile_dirty = true; }
    cur_y += row_h;

    // 9. Screen Shake (Comfort Setting)
    char shake_buf[32];
    if (s.screen_shake <= 0.001f) {
        std::snprintf(shake_buf, sizeof(shake_buf), "SCREEN SHAKE:      OFF (0%%)");
    } else {
        std::snprintf(shake_buf, sizeof(shake_buf), "SCREEN SHAKE:      %d%%", static_cast<int>(std::round(s.screen_shake * 100.0f)));
    }
    draw_text(shake_buf, panel_x + pad + 14.0f * ui_scale, cur_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f, 1.15f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    bool shk_hover1 = (mouse_x >= btn1_x && mouse_x <= btn1_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    bool shk_hover2 = (mouse_x >= btn2_x && mouse_x <= btn2_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    draw_panel_with_border(btn1_x, cur_y, adj_btn_w, adj_btn_h, shk_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_panel_with_border(btn2_x, cur_y, adj_btn_w, adj_btn_h, shk_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_text_centered("-", btn1_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    draw_text_centered("+", btn2_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    if (mouse_clicked && shk_hover1) { s.screen_shake = std::max(0.0f, s.screen_shake - 0.10f); m_profile_dirty = true; }
    if (mouse_clicked && shk_hover2) { s.screen_shake = std::min(1.0f, s.screen_shake + 0.10f); m_profile_dirty = true; }
    cur_y += row_h + 8.0f * ui_scale;

    // Test Audio Button
    float tst_w = content_w - 28.0f * ui_scale;
    float tst_h = std::clamp(32.0f * ui_scale, 26.0f, 38.0f);
    float tst_x = panel_x + pad + 14.0f * ui_scale;
    bool tst_hov = (mouse_x >= tst_x && mouse_x <= tst_x + tst_w && mouse_y >= cur_y && mouse_y <= cur_y + tst_h);
    draw_panel_with_border(tst_x, cur_y, tst_w, tst_h,
                           tst_hov ? glm::vec4(0.08f, 0.35f, 0.30f, 0.95f) : glm::vec4(0.04f, 0.18f, 0.16f, 0.85f),
                           tst_hov ? glm::vec4(0.2f, 1.0f, 0.6f, 1.0f) : glm::vec4(0.1f, 0.6f, 0.4f, 0.6f));
    draw_text_centered("[* PREVIEW AUDIO (PLAY TEST SOUND) *]", tst_x, cur_y, tst_w, tst_h, 1.20f * ui_scale,
                       tst_hov ? glm::vec4(0.4f, 1.0f, 0.7f, 1.0f) : glm::vec4(0.3f, 0.85f, 0.55f, 0.9f));
    if (mouse_clicked && tst_hov) {
        m_test_sound_requested = true;
    }
}

} // namespace Voidfall

