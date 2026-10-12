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

void OrbitalHubUI::draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color, FontType font) {
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
    FontRenderer::build_text_vertices(text, x, y, scale * 0.45f, vertices, font);

    if (!vertices.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data());
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));
    }

    glBindVertexArray(0);
}

void OrbitalHubUI::draw_text_centered(const std::string& text, float box_x, float box_y, float box_w, float box_h, float scale, const glm::vec4& color, FontType font) {
    if (text.empty()) return;
    float text_w = FontRenderer::get_rendered_width(text, scale, font);
    float text_h = FontRenderer::get_rendered_height(scale, font);
    float tx = box_x + (box_w - text_w) * 0.5f;
    float ty = box_y + (box_h - text_h) * 0.5f;
    draw_text(text, tx, ty, scale, color, font);
}

void OrbitalHubUI::draw_text_fitted(const std::string& text, float x, float y, float max_w, float base_scale, const glm::vec4& color, float min_scale, FontType font) {
    if (text.empty()) return;
    float scale = FontRenderer::fit_scale(text, max_w, base_scale, min_scale, font);
    draw_text(text, x, y, scale, color, font);
}

void OrbitalHubUI::draw_text_centered_fitted(const std::string& text, float box_x, float box_y, float box_w, float box_h, float base_scale, const glm::vec4& color, float min_scale, FontType font) {
    if (text.empty()) return;
    float pad = 12.0f;
    float scale = FontRenderer::fit_scale(text, std::max(20.0f, box_w - pad * 2.0f), base_scale, min_scale, font);
    draw_text_centered(text, box_x, box_y, box_w, box_h, scale, color, font);
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

bool OrbitalHubUI::render_main_menu(int& selected_level, const PlayerInventory& inventory, float mouse_x, float mouse_y, bool mouse_clicked, bool mouse_down, float scroll_delta) {
    bool has_save = false;
    for (int s = 1; s <= 3; ++s) {
        s_fallback_profile.sector_records[s] = inventory.sector_records[s];
        if (inventory.sector_records[s].highest_completion_rate > 0) has_save = true;
    }
    MainMenuAction action = render_main_menu(selected_level, s_fallback_profile, has_save, mouse_x, mouse_y, mouse_clicked, mouse_down, scroll_delta);
    return (action == MainMenuAction::Continue || action == MainMenuAction::NewExpedition);
}

bool OrbitalHubUI::render_main_menu(int& selected_level, UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked, bool mouse_down, float scroll_delta) {
    bool has_save = (profile.total_exp > 0 || profile.sector_records[1].highest_completion_rate > 0 || profile.sector_records[2].highest_completion_rate > 0 || profile.sector_records[3].highest_completion_rate > 0);
    MainMenuAction action = render_main_menu(selected_level, profile, has_save, mouse_x, mouse_y, mouse_clicked, mouse_down, scroll_delta);
    return (action == MainMenuAction::Continue || action == MainMenuAction::NewExpedition);
}

MainMenuAction OrbitalHubUI::render_main_menu(int& selected_level, UserProfile& profile, bool has_save, float mouse_x, float mouse_y, bool mouse_clicked, bool mouse_down, float scroll_delta) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    MainMenuAction action = MainMenuAction::None;

    if (m_subview == MenuSubView::SectorSelect) {
        render_sector_select_carousel(selected_level, profile, mouse_x, mouse_y, mouse_clicked, mouse_down, scroll_delta, action);
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
        float back_w = std::clamp(170.0f * ui_scale, 130.0f, 200.0f);
        float back_h = std::clamp(42.0f * ui_scale, 36.0f, 48.0f);
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
        draw_text_centered_fitted("< MAIN MENU", back_x, back_y, back_w, back_h, 1.30f * ui_scale, back_hover ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY, 0.90f * ui_scale, FontType::Display);

        // Subtabs for Delvers, Upgrades, and Audio Settings
        float tab_gap = 12.0f * ui_scale;
        float tab1_x = back_x + back_w + 16.0f * ui_scale;
        float avail_tab_w = w - tab1_x - 24.0f * ui_scale;
        float tab_w = std::clamp((avail_tab_w - 2.0f * tab_gap) / 3.0f, 150.0f, 240.0f * ui_scale);

        bool t1_hov = (mouse_x >= tab1_x && mouse_x <= tab1_x + tab_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (t1_hov && mouse_clicked) m_active_tab = HubTab::DelverRoster;
        bool t1_act = (m_active_tab == HubTab::DelverRoster);
        draw_panel_with_border(tab1_x, back_y, tab_w, back_h,
                               t1_act ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) : (t1_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                               t1_act ? Typography::COLOR_CYAN : glm::vec4(0.3f, 0.4f, 0.5f, 0.4f),
                               t1_act ? 2.0f : 1.0f);
        draw_text_centered_fitted("[ DELVER ROSTER ]", tab1_x, back_y, tab_w, back_h, 1.25f * ui_scale, t1_act ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY, 0.85f * ui_scale, FontType::Display);

        float tab2_x = tab1_x + tab_w + tab_gap;
        bool t2_hov = (mouse_x >= tab2_x && mouse_x <= tab2_x + tab_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (t2_hov && mouse_clicked) m_active_tab = HubTab::UpgradeTerminal;
        bool t2_act = (m_active_tab == HubTab::UpgradeTerminal);
        draw_panel_with_border(tab2_x, back_y, tab_w, back_h,
                               t2_act ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) : (t2_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                               t2_act ? Typography::COLOR_CYAN : glm::vec4(0.3f, 0.4f, 0.5f, 0.4f),
                               t2_act ? 2.0f : 1.0f);
        draw_text_centered_fitted("[ UPGRADES ]", tab2_x, back_y, tab_w, back_h, 1.25f * ui_scale, t2_act ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY, 0.85f * ui_scale, FontType::Display);

        float tab3_x = tab2_x + tab_w + tab_gap;
        bool t3_hov = (mouse_x >= tab3_x && mouse_x <= tab3_x + tab_w && mouse_y >= back_y && mouse_y <= back_y + back_h);
        if (t3_hov && mouse_clicked) m_active_tab = HubTab::AudioSettings;
        bool t3_act = (m_active_tab == HubTab::AudioSettings);
        draw_panel_with_border(tab3_x, back_y, tab_w, back_h,
                               t3_act ? glm::vec4(0.08f, 0.16f, 0.24f, 0.95f) : (t3_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                               t3_act ? Typography::COLOR_CYAN : glm::vec4(0.3f, 0.4f, 0.5f, 0.4f),
                               t3_act ? 2.0f : 1.0f);
        draw_text_centered_fitted("[ SETTINGS ]", tab3_x, back_y, tab_w, back_h, 1.25f * ui_scale, t3_act ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY, 0.85f * ui_scale, FontType::Display);

        if (m_active_tab == HubTab::DelverRoster) {
            render_delver_roster(profile, mouse_x, mouse_y, mouse_clicked);
        } else if (m_active_tab == HubTab::UpgradeTerminal) {
            render_upgrade_terminal(profile, mouse_x, mouse_y, mouse_clicked, mouse_down, scroll_delta);
        } else {
            render_audio_settings(profile, mouse_x, mouse_y, mouse_clicked);
        }

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        return action;
    }

    // Default Main Menu View
    draw_rect(0.0f, 0.0f, std::min(w * 0.48f, 660.0f * ui_scale), h, glm::vec4(0.02f, 0.03f, 0.05f, 0.88f));
    draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.01f, 0.02f, 0.03f, 0.25f));

    float start_x = std::max(60.0f * ui_scale, 36.0f);
    float title_y = std::max(h * 0.14f, 32.0f);

    // Bold Game Title banner
    draw_text("VOIDFALL: DREDGE", start_x, title_y, 3.8f * ui_scale, Typography::COLOR_CYAN, FontType::Display);
    draw_text("SUBTERRANEAN EXPEDITION PROTOCOL // DEEP EXTRACTION ENGINE", start_x + 4.0f, title_y + 54.0f * ui_scale, 1.35f * ui_scale, Typography::COLOR_PRIMARY, FontType::Tech);

    // Accent line beneath title
    draw_rect(start_x, title_y + 78.0f * ui_scale, 480.0f * ui_scale, 2.5f, glm::vec4(0.0f, 0.90f, 1.0f, 0.65f));

    // Clean Action List
    struct MenuItem {
        MainMenuAction act;
        std::string label;
        bool enabled;
        std::string tip;
    };

    // Decay confirmation and toast timers
    if (m_save_cleared_toast_timer > 0.0f) {
        m_save_cleared_toast_timer -= 0.016f;
    }
    if (m_confirm_clear_timer > 0.0f) {
        m_confirm_clear_timer -= 0.016f;
        if (m_confirm_clear_timer <= 0.0f) {
            m_confirming_clear_save = false;
        }
    }

    std::string reset_label = "[ RESET SAVE DATA ]";
    std::string reset_tip = "Permanently clear save file & reset delver profile to baseline";
    if (m_save_cleared_toast_timer > 0.0f) {
        reset_label = "[ PROFILE RESET COMPLETE ]";
        reset_tip = "Save data successfully cleared // Fresh Delver initialized";
    } else if (m_confirming_clear_save) {
        reset_label = "[ CONFIRM RESET? CLICK AGAIN ]";
        reset_tip = "WARNING: Click again now to permanently wipe and reset save file";
    }

    bool reset_allowed = (has_save || m_test_mode || m_confirming_clear_save);

    std::vector<MenuItem> items = {
        { MainMenuAction::Continue,        "[ CONTINUE ]",             has_save, "Resume operations in Sector " + std::to_string(selected_level) },
        { MainMenuAction::NewExpedition,   "[ NEW EXPEDITION ]",        true,     "Choose sector & deploy immediately" },
        { MainMenuAction::UpgradeTerminal, "[ UPGRADES / DELVERS ]",    true,     "Delver roster & drill modifications" },
        { MainMenuAction::Settings,        "[ AUDIO & RIG SETTINGS ]",  true,     "Acoustic comfort, sound mixer & controls" },
        { MainMenuAction::ResetSave,       reset_label,                reset_allowed, reset_tip },
        { MainMenuAction::Exit,            "[ EXIT ]",                  true,     "Quit to desktop cleanly" }
    };

    float btn_y = title_y + 94.0f * ui_scale;
    float btn_w = std::clamp(480.0f * ui_scale, 360.0f, 540.0f);
    float btn_h = std::clamp(54.0f * ui_scale, 44.0f, 62.0f);
    float btn_gap = std::clamp(12.0f * ui_scale, 8.0f, 16.0f);

    for (size_t i = 0; i < items.size(); ++i) {
        float y = btn_y + i * (btn_h + btn_gap);
        const auto& item = items[i];

        bool is_hovered = item.enabled && (mouse_x >= start_x && mouse_x <= start_x + btn_w + 10.0f &&
                                           mouse_y >= y && mouse_y <= y + btn_h);

        float current_btn_x = is_hovered ? (start_x + 8.0f * ui_scale) : start_x;

        if (is_hovered && mouse_clicked) {
            if (item.act == MainMenuAction::NewExpedition) {
                m_subview = MenuSubView::SectorSelect;
            } else if (item.act == MainMenuAction::UpgradeTerminal) {
                m_subview = MenuSubView::Upgrades;
            } else if (item.act == MainMenuAction::Settings) {
                m_subview = MenuSubView::Settings;
            } else if (item.act == MainMenuAction::ResetSave) {
                if (!m_confirming_clear_save) {
                    m_confirming_clear_save = true;
                    m_confirm_clear_timer = 4.0f;
                } else {
                    m_confirming_clear_save = false;
                    m_confirm_clear_timer = 0.0f;
                    action = MainMenuAction::ResetSave;
                }
            } else {
                action = item.act;
            }
        }

        // Button background
        glm::vec4 bg_col = !item.enabled ? glm::vec4(0.04f, 0.05f, 0.07f, 0.65f) :
                           (item.act == MainMenuAction::ResetSave && m_confirming_clear_save) ? glm::vec4(0.28f, 0.06f, 0.06f, 0.90f) :
                           (item.act == MainMenuAction::ResetSave && m_save_cleared_toast_timer > 0.0f) ? glm::vec4(0.06f, 0.22f, 0.12f, 0.90f) :
                           is_hovered    ? Typography::COLOR_BUTTON_HOV :
                                           Typography::COLOR_BUTTON_BG;
        glm::vec4 border_col = !item.enabled ? Typography::COLOR_MUTED * 0.4f :
                               (item.act == MainMenuAction::ResetSave && m_confirming_clear_save) ? glm::vec4(1.0f, 0.35f, 0.25f, 0.95f) :
                               (item.act == MainMenuAction::ResetSave && m_save_cleared_toast_timer > 0.0f) ? glm::vec4(0.2f, 0.95f, 0.4f, 0.95f) :
                               is_hovered    ? Typography::COLOR_CYAN_GLOW :
                                               glm::vec4(0.0f, 0.85f, 1.0f, 0.35f);

        draw_panel_with_border(current_btn_x, y, btn_w, btn_h, bg_col, border_col, is_hovered ? 2.5f : 1.0f);

        float text_scale = 1.50f * ui_scale;
        glm::vec4 text_col = !item.enabled ? Typography::COLOR_MUTED :
                             (item.act == MainMenuAction::ResetSave && m_confirming_clear_save) ? glm::vec4(1.0f, 0.45f, 0.35f, 1.0f) :
                             (item.act == MainMenuAction::ResetSave && m_save_cleared_toast_timer > 0.0f) ? glm::vec4(0.35f, 1.0f, 0.55f, 1.0f) :
                             is_hovered    ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) :
                                             Typography::COLOR_PRIMARY;
        draw_text_centered_fitted(item.label, current_btn_x, y, btn_w, btn_h, text_scale, text_col, 0.90f * ui_scale, FontType::Display);

        if (is_hovered) {
            float tip_scale = 1.30f * ui_scale;
            float tip_y = y + (btn_h - FontRenderer::get_rendered_height(tip_scale, FontType::Body)) * 0.5f;
            glm::vec4 tip_col = (item.act == MainMenuAction::ResetSave && m_confirming_clear_save) ? glm::vec4(1.0f, 0.45f, 0.35f, 1.0f) :
                                (item.act == MainMenuAction::ResetSave && m_save_cleared_toast_timer > 0.0f) ? glm::vec4(0.35f, 1.0f, 0.55f, 1.0f) :
                                Typography::COLOR_CYAN;
            draw_text_fitted(item.tip, current_btn_x + btn_w + 20.0f * ui_scale, tip_y, w - (current_btn_x + btn_w + 30.0f * ui_scale), tip_scale, tip_col, 0.75f * ui_scale, FontType::Body);
        }
    }

    // Status watermark at bottom
    std::string ver_info = "VOIDFALL // DEEP EXPEDITION ENGINE v1.0.4";
    draw_text(ver_info, start_x, h - 36.0f * ui_scale, 1.20f * ui_scale, Typography::COLOR_MUTED, FontType::Tech);

    std::string reset_hint = "[F4] QUICK RESET PROFILE";
    draw_text(reset_hint, start_x + 420.0f * ui_scale, h - 36.0f * ui_scale, 1.20f * ui_scale, glm::vec4(0.35f, 0.65f, 0.85f, 0.85f), FontType::Tech);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return action;
}

void OrbitalHubUI::focus_sector(int sector, bool instant) {
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);
    float header_x = std::max(40.0f * ui_scale, 24.0f);
    float w = static_cast<float>(m_width);
    float avail_w = w - 2.0f * header_x;
    float card_gap = std::clamp(20.0f * ui_scale, 12.0f, 26.0f);
    float card_w = (avail_w - 2.0f * card_gap) / 3.0f;
    float card_pitch = card_w + card_gap;

    const int MAX_VISIBLE = UserProfile::MAX_SECTOR_RECORDS - 1;
    int s = std::clamp(sector, 1, MAX_VISIBLE);
    m_focused_sector = s;

    // Center card `s` on screen
    float card_center_x = header_x + (s - 1) * card_pitch + card_w * 0.5f;
    float ideal_offset = card_center_x - (w * 0.5f);

    float last_card_right = header_x + (MAX_VISIBLE - 1) * card_pitch + card_w;
    float max_offset = std::max(0.0f, last_card_right - (w - header_x));
    float target = std::clamp(ideal_offset, 0.0f, max_offset);

    m_carousel_target_x = target;
    if (instant) {
        m_carousel_scroll_x = target;
        m_carousel_velocity_x = 0.0f;
    }
    m_carousel_needs_focus = false;
}

void OrbitalHubUI::focus_upgrade_node(int type_idx) {
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);
    float w = static_cast<float>(m_width);
    float terminal_w = std::clamp(w * 0.94f, 680.0f, 1460.0f);
    float tree_w = std::round(terminal_w * 0.62f);
    float col_pad = 12.0f * ui_scale;
    float col_w = 230.0f * ui_scale;
    float col_gap = 16.0f * ui_scale;
    float total_content_w = 6.0f * col_w + 5.0f * col_gap;
    float max_scroll = std::max(0.0f, total_content_w - (tree_w - 2.0f * col_pad));

    int b = std::clamp(type_idx / 3, 0, 5);
    float col_center = col_pad + b * (col_w + col_gap) + col_w * 0.5f;
    float ideal_target = col_center - (tree_w * 0.5f);
    m_upgrades_target_x = std::clamp(ideal_target, 0.0f, max_scroll);
    m_upgrades_scroll_x = m_upgrades_target_x;
    m_upgrades_velocity_x = 0.0f;
}

void OrbitalHubUI::render_sector_select_carousel(int& selected_level, UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked, bool mouse_down, float scroll_delta, MainMenuAction& action) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    // Dark backdrop tint
    draw_rect(0.0f, 0.0f, w, h, glm::vec4(0.02f, 0.03f, 0.05f, 0.94f));

    // Header
    float header_x = std::max(40.0f * ui_scale, 24.0f);
    float header_y = 24.0f * ui_scale;
    draw_text("SELECT EXPEDITION SECTOR", header_x, header_y, 2.80f * ui_scale, Typography::COLOR_CYAN, FontType::Display);
    draw_text("CHOOSE A SUBTERRANEAN SECTOR TO COMMENCE EXTRACTION DESCENT", header_x + 4.0f, header_y + 36.0f * ui_scale, 1.35f * ui_scale, Typography::COLOR_PRIMARY, FontType::Tech);
    if (!m_terminal_msg.empty()) {
        float status_x = header_x + 640.0f * ui_scale;
        if (status_x < w - 100.0f * ui_scale) {
            draw_text(">> " + m_terminal_msg, status_x, header_y + 36.0f * ui_scale, 1.25f * ui_scale, m_terminal_msg_col, FontType::Tech);
        }
    }
    draw_rect(header_x, header_y + 58.0f * ui_scale, w - 2.0f * header_x, 2.5f, glm::vec4(0.0f, 0.90f, 1.0f, 0.50f));

    // --- Endless sector data generation ---
    const int MAX_VISIBLE = UserProfile::MAX_SECTOR_RECORDS - 1;
    int highest_unlocked = 1;
    for (int s = 1; s <= MAX_VISIBLE; ++s) {
        if (profile.is_sector_unlocked(s)) {
            highest_unlocked = s;
        }
    }
    int max_available = std::max({3, profile.highest_cleared_sector + 1, highest_unlocked});
    if (highest_unlocked < MAX_VISIBLE && max_available < highest_unlocked + 1) {
        max_available = highest_unlocked + 1;
    }
    max_available = std::clamp(max_available, 3, MAX_VISIBLE);

    // --- Layout ---
    float avail_w       = w - 2.0f * header_x;
    float card_gap      = std::clamp(22.0f * ui_scale, 14.0f, 28.0f);
    float card_w        = (avail_w - 2.0f * card_gap) / 3.0f;
    float card_pitch    = card_w + card_gap;
    float start_x       = header_x;
    float start_y       = header_y + 72.0f * ui_scale;
    float bar_h         = std::clamp(50.0f * ui_scale, 40.0f, 56.0f);
    float card_h        = h - start_y - bar_h - 22.0f * ui_scale;
    float pad           = 18.0f * ui_scale;

    // Total content bounds and scroll range
    float total_content_w = max_available * card_pitch - card_gap;
    float max_offset = std::max(0.0f, total_content_w - avail_w);

    // When the user visits level selection, ALWAYS place on the current level the user is at!
    if (m_carousel_needs_focus) {
        focus_sector(selected_level, true);
    }

    // --- Mouse Dragging & Scroll Interaction ---
    bool in_cards_area = (mouse_x >= start_x && mouse_x <= w - start_x &&
                          mouse_y >= start_y && mouse_y <= start_y + card_h);

    if (mouse_down) {
        if (mouse_clicked && in_cards_area) {
            m_is_dragging_carousel = true;
            m_drag_start_mouse_x = mouse_x;
            m_last_mouse_x = mouse_x;
            m_drag_total_dist = 0.0f;
            m_carousel_velocity_x = 0.0f;
        } else if (m_is_dragging_carousel) {
            float dx = mouse_x - m_last_mouse_x;
            m_drag_total_dist += std::abs(dx);
            m_carousel_scroll_x -= dx; // Dragging mouse left moves view to right
            m_carousel_target_x = m_carousel_scroll_x;
            m_carousel_velocity_x = -dx * 55.0f;
            m_last_mouse_x = mouse_x;
        }
    } else {
        if (m_is_dragging_carousel) {
            m_is_dragging_carousel = false;
            // Momentum fling on drag release
            m_carousel_target_x = m_carousel_scroll_x + m_carousel_velocity_x * 0.12f;
        }
    }

    // Mouse wheel support
    if (std::abs(scroll_delta) > 0.01f && in_cards_area) {
        m_carousel_target_x = std::clamp(m_carousel_target_x - scroll_delta * card_pitch * 0.8f, 0.0f, max_offset);
    }

    // Target clamping
    m_carousel_target_x = std::clamp(m_carousel_target_x, 0.0f, max_offset);

    // Smooth spring dampening towards target when not dragging
    if (!m_is_dragging_carousel) {
        float damp = 14.0f * (1.0f / 60.0f);
        m_carousel_scroll_x += (m_carousel_target_x - m_carousel_scroll_x) * std::min(1.0f, damp);
        if (std::abs(m_carousel_target_x - m_carousel_scroll_x) < 0.2f) {
            m_carousel_scroll_x = m_carousel_target_x;
        }
    }

    // Pure click detection (not dragged)
    bool is_pure_click = mouse_clicked && (!m_is_dragging_carousel || m_drag_total_dist < 8.0f);

    // --- Procedural sector descriptor helpers ---
    auto get_tier_str = [](int s) -> std::string {
        if (s <= 1)  return "[ TIER I // PERIMETER ]";
        if (s == 2)  return "[ TIER II // FAULTLINE ]";
        if (s == 3)  return "[ TIER III // ABYSSAL ]";
        if (s <= 6)  return "[ TIER IV // MAGMA CORE ]";
        if (s <= 10) return "[ TIER V // VOID DEEP ]";
        return               "[ TIER VI // UNCHARTED ]";
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
    auto get_description = [](int s) -> std::string {
        static const char* descs[] = {
            "Shallow seismic bedrock with stable quartz veins. Minimal void stalker presence. Ideal for fresh contractors.",
            "Deep pressurized fissure riddled with acoustic faults. Void Burrowers actively displace voxel geometry.",
            "Sub-mantle abyssal trenches with lethal void storms and apex Void Stalkers. Maximum extraction hazard.",
            "Molten volcanic cavern system. Magma heat surges and burning rock formations demand heat shields.",
            "High-density crystalline cavern. Resonant crystals shatter into volatile kinetic fragments upon impact.",
            "Abyssal sound-chamber with swarming predators. Heavy acoustic reflection amplifies drill noise."
        };
        int idx = (s - 1) % 6;
        return std::string(descs[idx]);
    };
    auto get_depth = [](int s) -> std::string {
        int depth_m = 800 + (s - 1) * 600;
        char buf[64];
        if (depth_m < 10000)
            std::snprintf(buf, sizeof(buf), "%dm", depth_m);
        else
            std::snprintf(buf, sizeof(buf), "%.1fkm", depth_m / 1000.0f);
        return std::string(buf);
    };
    auto get_objective = [](int s) -> std::string {
        int voidite = (s <= 1) ? 25 : (s == 2) ? 35 : (s == 3) ? 50 : std::min(50 + (s - 3) * 15, 200);
        if (s == 2) return "RECOVER PRECURSOR RELIC";
        if (s % 5 == 0) {
            char buf[72];
            std::snprintf(buf, sizeof(buf), "NEUTRALIZE ALPHA (x%d)", std::min(1 + (s / 5), 4));
            return std::string(buf);
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "<voidite> TARGET: %d VOIDITE", voidite);
        return std::string(buf);
    };
    auto get_hazard = [](int s) -> std::string {
        static const char* hazards[] = {
            "ESCALATION CURVE // SEISMIC TREMORS",
            "ACID GAS & VOID BURROWERS",
            "3-MIN COLLAPSE // APEX STALKERS",
            "MAGMA HEAT SURGE & MOLTEN LAVA",
            "VOID RADIATION & CRYSTAL BURSTS",
            "ECHO SWARM & STUNNING SCREECHES",
            "GRAVITY SHIFTS & CRUSHING DEBRIS",
            "SPORE BLOOM & CORROSIVE SPORES",
            "WARP RIFTS & ANOMALOUS PULLS",
            "NECROTIC GAS & ROTTING ROCK",
            "TIME DISPLACEMENT & PHASE VOID",
            "ABYSS DESTABILIZATION COLLAPSE"
        };
        return "<hazard> " + std::string(hazards[(s - 1) % 12]);
    };
    auto get_bonus = [](int s) -> std::string {
        float mult = 1.0f + (s - 1) * 0.25f;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1fx EXP", mult);
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
    for (int sec_num = 1; sec_num <= max_available; ++sec_num) {
        float cx = start_x + (sec_num - 1) * card_pitch - m_carousel_scroll_x;

        // Cull cards outside the visible horizontal window (with padding)
        if (cx + card_w < start_x - 120.0f || cx > w - start_x + 120.0f) {
            continue;
        }

        bool is_unlocked  = profile.is_sector_unlocked(sec_num);
        bool is_completed = profile.is_sector_completed(sec_num);
        bool is_charted   = profile.is_sector_charted(sec_num);
        int  req_lvl      = UserProfile::get_required_level_for_sector(sec_num);
        bool is_selected  = (selected_level == sec_num);
        bool is_hovered   = in_cards_area && (mouse_x >= cx && mouse_x <= cx + card_w);

        // Ghost slot when beyond max_available and not unlocked
        bool is_ghost = (sec_num > max_available && !is_unlocked) || (sec_num > MAX_VISIBLE);
        if (is_ghost) {
            if (is_hovered && is_pure_click) {
                m_terminal_msg = profile.get_sector_lock_detail(sec_num);
                m_terminal_msg_col = glm::vec4(1.0f, 0.5f, 0.35f, 1.0f);
            }
            glm::vec4 g_bg = is_hovered ? glm::vec4(0.04f, 0.04f, 0.06f, 0.70f) : glm::vec4(0.02f, 0.02f, 0.03f, 0.55f);
            glm::vec4 g_bd = is_hovered ? glm::vec4(0.3f, 0.35f, 0.4f, 0.5f) : glm::vec4(0.12f, 0.15f, 0.18f, 0.25f);
            draw_panel_with_border(cx, start_y, card_w, card_h, g_bg, g_bd, is_hovered ? 2.0f : 1.0f);
            draw_text_centered_fitted("-- UNCHARTED TERRITORY --", cx, start_y, card_w, card_h,
                                      1.35f * ui_scale, is_hovered ? Typography::COLOR_PRIMARY : glm::vec4(0.3f, 0.35f, 0.4f, 0.5f), 0.90f * ui_scale, FontType::Display);
            continue;
        }

        if (is_hovered && is_pure_click) {
            if (is_unlocked) {
                selected_level = sec_num;
                m_focused_sector = sec_num;
                if (is_completed) {
                    m_terminal_msg = "SECTOR " + std::to_string(sec_num) + " SELECTED: RECORD " +
                                     profile.sector_records[sec_num].best_badge + " (" +
                                     std::to_string(profile.sector_records[sec_num].highest_completion_rate) + "%) // READY FOR DESCENT";
                    m_terminal_msg_col = Typography::COLOR_GREEN;
                } else if (is_charted) {
                    m_terminal_msg = "SECTOR " + std::to_string(sec_num) + " SELECTED: RECORD " +
                                     profile.sector_records[sec_num].best_badge + " // EXPEDITION READY";
                    m_terminal_msg_col = Typography::COLOR_CYAN;
                } else {
                    m_terminal_msg = "UNCHARTED SECTOR " + std::to_string(sec_num) + " SELECTED: COMMENCE EXPEDITION TO GENERATE TOPOLOGY";
                    m_terminal_msg_col = Typography::COLOR_AMBER;
                }
            } else {
                m_terminal_msg = profile.get_sector_lock_detail(sec_num);
                m_terminal_msg_col = glm::vec4(1.0f, 0.4f, 0.35f, 1.0f);
            }
        }

        glm::vec4 accent     = get_accent(sec_num);
        glm::vec4 bg_col     = !is_unlocked ? glm::vec4(0.04f, 0.04f, 0.05f, 0.90f) :
                               is_selected  ? (is_completed ? glm::vec4(0.05f, 0.18f, 0.12f, 0.96f) : glm::vec4(0.08f, 0.16f, 0.24f, 0.96f)) :
                               is_hovered   ? glm::vec4(0.06f, 0.12f, 0.18f, 0.92f) :
                                              glm::vec4(0.03f, 0.05f, 0.08f, 0.88f);
        glm::vec4 border_col = !is_unlocked ? glm::vec4(0.35f, 0.2f, 0.25f, 0.45f) :
                               is_selected  ? (is_completed ? Typography::COLOR_GREEN : accent) :
                               is_hovered   ? glm::vec4(0.4f, 0.85f, 1.0f, 0.85f) :
                                              glm::vec4(0.2f, 0.35f, 0.45f, 0.45f);

        draw_panel_with_border(cx, start_y, card_w, card_h, bg_col, border_col,
                               is_selected ? 3.0f : (is_hovered ? 2.0f : 1.0f));

        float inner_w = card_w - 2.0f * pad;

        // 1. Header: Tier Badge & Sector Title (Large & High Contrast)
        draw_text_fitted(get_tier_str(sec_num), cx + pad, start_y + 16.0f * ui_scale, inner_w, 1.35f * ui_scale,
                         is_unlocked ? accent : glm::vec4(0.5f, 0.55f, 0.55f, 0.8f), 0.85f * ui_scale, FontType::Tech);
        draw_text_fitted(get_title(sec_num), cx + pad, start_y + 40.0f * ui_scale, inner_w, 1.90f * ui_scale,
                         is_unlocked ? glm::vec4(1.0f) : glm::vec4(0.6f, 0.6f, 0.65f, 0.8f), 0.90f * ui_scale, FontType::Display);
        draw_rect(cx + pad, start_y + 74.0f * ui_scale, inner_w, 2.0f,
                  is_unlocked ? accent * 0.75f : glm::vec4(0.25f, 0.35f, 0.45f, 0.4f));

        // 2. Tactical Dossier Briefing Box
        float box1_y = start_y + 86.0f * ui_scale;
        float box1_h = 96.0f * ui_scale;
        draw_panel_with_border(cx + pad, box1_y, inner_w, box1_h,
                               glm::vec4(0.02f, 0.04f, 0.06f, 0.75f), glm::vec4(0.20f, 0.30f, 0.40f, 0.5f), 1.0f);
        draw_text("// SECTOR TACTICAL DOSSIER", cx + pad + 10.0f * ui_scale, box1_y + 8.0f * ui_scale, 1.15f * ui_scale,
                  Typography::COLOR_MUTED, FontType::Tech);
        auto desc_lines = FontRenderer::wrap_text(get_description(sec_num), inner_w - 20.0f * ui_scale, 1.22f * ui_scale, FontType::Body);
        float desc_cur_y = box1_y + 32.0f * ui_scale;
        for (const auto& dline : desc_lines) {
            draw_text_fitted(dline, cx + pad + 10.0f * ui_scale, desc_cur_y, inner_w - 20.0f * ui_scale, 1.22f * ui_scale,
                             glm::vec4(0.88f, 0.92f, 0.96f, 0.95f), 0.85f * ui_scale, FontType::Body);
            desc_cur_y += 20.0f * ui_scale;
        }

        // 3. Mission Objectives Box
        float box2_y = box1_y + box1_h + 12.0f * ui_scale;
        float box2_h = 78.0f * ui_scale;
        draw_panel_with_border(cx + pad, box2_y, inner_w, box2_h,
                               glm::vec4(0.02f, 0.04f, 0.06f, 0.75f), glm::vec4(0.15f, 0.45f, 0.55f, 0.6f), 1.0f);
        draw_text("// PRIMARY EXTRACTION OBJECTIVE", cx + pad + 10.0f * ui_scale, box2_y + 8.0f * ui_scale, 1.15f * ui_scale,
                  Typography::COLOR_CYAN, FontType::Tech);
        draw_text_fitted(get_objective(sec_num), cx + pad + 10.0f * ui_scale, box2_y + 34.0f * ui_scale, inner_w - 20.0f * ui_scale, 1.45f * ui_scale,
                         Typography::COLOR_PRIMARY, 0.90f * ui_scale, FontType::Display);

        // 4. Critical Hazard Threat Box
        float box3_y = box2_y + box2_h + 12.0f * ui_scale;
        float box3_h = 78.0f * ui_scale;
        draw_panel_with_border(cx + pad, box3_y, inner_w, box3_h,
                               glm::vec4(0.03f, 0.03f, 0.04f, 0.75f), accent * 0.6f, 1.0f);
        draw_text("// CRITICAL HAZARDS & THREATS", cx + pad + 10.0f * ui_scale, box3_y + 8.0f * ui_scale, 1.15f * ui_scale,
                  accent, FontType::Tech);
        draw_text_fitted(get_hazard(sec_num), cx + pad + 10.0f * ui_scale, box3_y + 34.0f * ui_scale, inner_w - 20.0f * ui_scale, 1.35f * ui_scale,
                         accent, 0.85f * ui_scale, FontType::Tech);

        // 5. Depth & EXP Telemetry strip
        float box4_y = box3_y + box3_h + 12.0f * ui_scale;
        float box4_h = 44.0f * ui_scale;
        draw_panel_with_border(cx + pad, box4_y, inner_w, box4_h,
                               glm::vec4(0.02f, 0.04f, 0.06f, 0.85f), glm::vec4(0.20f, 0.35f, 0.45f, 0.5f), 1.0f);
        std::string depth_exp_str = "<drill> DEPTH: " + get_depth(sec_num) + "   |   <exp> " + get_bonus(sec_num);
        draw_text_centered_fitted(depth_exp_str, cx + pad, box4_y, inner_w, box4_h, 1.30f * ui_scale,
                                  Typography::COLOR_PRIMARY, 0.85f * ui_scale, FontType::Tech);

        // 6. Status Line
        float status_y = box4_y + box4_h + 14.0f * ui_scale;
        if (is_completed) {
            std::string badge_str = "STATUS: CLEARED (" + std::to_string(profile.sector_records[sec_num].highest_completion_rate) + "%) // SECTOR SECURED";
            draw_text_fitted(badge_str, cx + pad, status_y, inner_w, 1.30f * ui_scale,
                             glm::vec4(0.35f, 1.0f, 0.65f, 0.95f), 0.85f * ui_scale, FontType::Tech);
        } else if (is_charted) {
            std::string badge_str = "STATUS: CHARTED (" + std::to_string(profile.sector_records[sec_num].highest_completion_rate) + "%) // READY FOR DEPLOY";
            draw_text_fitted(badge_str, cx + pad, status_y, inner_w, 1.30f * ui_scale,
                             glm::vec4(0.45f, 0.95f, 1.0f, 0.95f), 0.85f * ui_scale, FontType::Tech);
        } else if (is_unlocked) {
            draw_text_fitted("STATUS: UNCHARTED // EXTRACTION READY", cx + pad, status_y, inner_w, 1.30f * ui_scale,
                             Typography::COLOR_AMBER, 0.85f * ui_scale, FontType::Tech);
        } else {
            std::string lock_line = profile.is_prior_sector_cleared(sec_num)
                ? "STATUS: LOCKED // REQUIRES PLAYER LEVEL " + std::to_string(req_lvl)
                : "STATUS: LOCKED // COMPLETE SECTOR " + std::to_string(sec_num - 1) + " FIRST";
            draw_text_fitted(lock_line, cx + pad, status_y, inner_w, 1.30f * ui_scale,
                             Typography::COLOR_MUTED, 0.85f * ui_scale, FontType::Tech);
        }

        // 7. Selection Action Button at card bottom
        float badge_w = inner_w;
        float badge_h = 48.0f * ui_scale;
        float badge_x = cx + pad;
        float badge_y = start_y + card_h - badge_h - 14.0f * ui_scale;

        if (!is_unlocked) {
            draw_panel_with_border(badge_x, badge_y, badge_w, badge_h,
                                   glm::vec4(0.3f, 0.1f, 0.12f, 0.85f), glm::vec4(1.0f, 0.35f, 0.35f, 0.8f));
            draw_text_centered_fitted(profile.get_sector_lock_badge(sec_num),
                                      badge_x, badge_y, badge_w, badge_h, 1.30f * ui_scale,
                                      glm::vec4(1.0f, 0.5f, 0.5f, 1.0f), 0.85f * ui_scale, FontType::Display);
        } else if (is_selected) {
            glm::vec4 sel_bg = is_completed ? glm::vec4(0.04f, 0.28f, 0.16f, 0.50f) :
                               is_charted   ? glm::vec4(0.0f, 0.25f, 0.35f, 0.50f) :
                                              glm::vec4(0.0f, 0.85f, 1.0f, 0.25f);
            glm::vec4 sel_col = is_completed ? Typography::COLOR_GREEN : Typography::COLOR_CYAN;
            draw_panel_with_border(badge_x, badge_y, badge_w, badge_h, sel_bg, sel_col, 2.5f);
            std::string sel_label = is_completed ? "[ ACTIVE // CLEARED ]" :
                                    is_charted   ? "[ ACTIVE // READY ]" :
                                                   "[ ACTIVE // UNCHARTED ]";
            draw_text_centered_fitted(sel_label, badge_x, badge_y, badge_w, badge_h,
                                      1.40f * ui_scale, sel_col, 0.90f * ui_scale, FontType::Display);
        } else {
            if (is_completed) {
                draw_panel_with_border(badge_x, badge_y, badge_w, badge_h,
                                       glm::vec4(0.04f, 0.18f, 0.10f, 0.75f), glm::vec4(0.2f, 0.85f, 0.45f, 0.8f), 1.0f);
                draw_text_centered_fitted("[ CLEARED // SELECT ]", badge_x, badge_y, badge_w, badge_h,
                                          1.35f * ui_scale, glm::vec4(0.4f, 1.0f, 0.6f, 1.0f), 0.85f * ui_scale, FontType::Display);
            } else if (is_charted) {
                draw_panel_with_border(badge_x, badge_y, badge_w, badge_h,
                                       glm::vec4(0.05f, 0.14f, 0.22f, 0.75f), glm::vec4(0.2f, 0.65f, 0.9f, 0.8f), 1.0f);
                draw_text_centered_fitted("[ CHARTED // SELECT ]", badge_x, badge_y, badge_w, badge_h,
                                          1.35f * ui_scale, Typography::COLOR_CYAN, 0.85f * ui_scale, FontType::Display);
            } else {
                draw_panel_with_border(badge_x, badge_y, badge_w, badge_h,
                                       glm::vec4(0.12f, 0.10f, 0.02f, 0.75f), Typography::COLOR_AMBER, 1.0f);
                draw_text_centered_fitted("[ UNCHARTED // SELECT ]", badge_x, badge_y, badge_w, badge_h,
                                          1.35f * ui_scale, Typography::COLOR_AMBER, 0.85f * ui_scale, FontType::Display);
            }
        }
    }

    // Edge overflow visual hints
    if (m_carousel_scroll_x > 8.0f) {
        draw_rect(start_x, start_y, 4.0f, card_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.7f));
    }
    if (m_carousel_scroll_x < max_offset - 8.0f) {
        draw_rect(start_x + avail_w - 4.0f, start_y, 4.0f, card_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.7f));
    }

    // --- Bottom Navigation Bar ---
    float bar_y = start_y + card_h + 12.0f * ui_scale;

    // < BACK button
    float back_w = 160.0f * ui_scale;
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
    draw_text_centered_fitted("< BACK", back_x, bar_y, back_w, back_h, 1.35f * ui_scale,
                              back_hov ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY, 0.90f * ui_scale, FontType::Display);

    // < PREV arrow
    float nav_btn_w = std::clamp(110.0f * ui_scale, 90.0f, 130.0f);
    float prev_x = back_x + back_w + 12.0f * ui_scale;
    bool prev_en  = (m_carousel_target_x > 1.0f);
    bool prev_hov = prev_en &&
                    mouse_x >= prev_x && mouse_x <= prev_x + nav_btn_w &&
                    mouse_y >= bar_y  && mouse_y <= bar_y + bar_h;
    if (prev_hov && mouse_clicked) {
        m_carousel_target_x = std::clamp(m_carousel_target_x - card_pitch, 0.0f, max_offset);
    }
    {
        glm::vec4 pb = prev_en && prev_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG;
        glm::vec4 bd = prev_en && prev_hov ? Typography::COLOR_CYAN_GLOW  : glm::vec4(0.0f, 0.85f, 1.0f, 0.3f);
        if (!prev_en) { pb = glm::vec4(0.05f, 0.06f, 0.08f, 0.4f); bd = glm::vec4(0.2f, 0.2f, 0.2f, 0.2f); }
        draw_panel_with_border(prev_x, bar_y, nav_btn_w, bar_h, pb, bd, prev_hov ? 2.0f : 1.0f);
        glm::vec4 tc = !prev_en ? glm::vec4(0.4f, 0.4f, 0.4f, 0.4f) :
                        prev_hov ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY;
        draw_text_centered_fitted("< PREV", prev_x, bar_y, nav_btn_w, bar_h, 1.25f * ui_scale, tc, 0.85f * ui_scale, FontType::Display);
    }

    // NEXT > arrow
    float launch_w = std::clamp(400.0f * ui_scale, 280.0f, 460.0f);
    float next_x = start_x + avail_w - launch_w - nav_btn_w - 12.0f * ui_scale;
    bool next_en  = (m_carousel_target_x < max_offset - 1.0f);
    bool next_hov = next_en &&
                    mouse_x >= next_x && mouse_x <= next_x + nav_btn_w &&
                    mouse_y >= bar_y  && mouse_y <= bar_y + bar_h;
    if (next_hov && mouse_clicked) {
        m_carousel_target_x = std::clamp(m_carousel_target_x + card_pitch, 0.0f, max_offset);
    }
    {
        glm::vec4 nb = next_en && next_hov ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG;
        glm::vec4 bd = next_en && next_hov ? Typography::COLOR_CYAN_GLOW  : glm::vec4(0.0f, 0.85f, 1.0f, 0.3f);
        if (!next_en) { nb = glm::vec4(0.05f, 0.06f, 0.08f, 0.4f); bd = glm::vec4(0.2f, 0.2f, 0.2f, 0.2f); }
        draw_panel_with_border(next_x, bar_y, nav_btn_w, bar_h, nb, bd, next_hov ? 2.0f : 1.0f);
        glm::vec4 tc = !next_en ? glm::vec4(0.4f, 0.4f, 0.4f, 0.4f) :
                        next_hov ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY;
        draw_text_centered_fitted("NEXT >", next_x, bar_y, nav_btn_w, bar_h, 1.25f * ui_scale, tc, 0.85f * ui_scale, FontType::Display);
    }

    // Drag / Progress indicator centered between PREV and NEXT
    float center_x1 = prev_x + nav_btn_w + 12.0f * ui_scale;
    float center_x2 = next_x - 12.0f * ui_scale;
    float center_w  = center_x2 - center_x1;
    if (center_w > 80.0f) {
        int cur_sec_approx = std::clamp(static_cast<int>(std::round(m_carousel_scroll_x / card_pitch)) + 1, 1, max_available);
        char page_buf[64];
        std::snprintf(page_buf, sizeof(page_buf), "SECTOR %d / %d  [ < DRAG TO BROWSE > ]", cur_sec_approx, max_available);
        draw_text_centered_fitted(page_buf, center_x1, bar_y + 4.0f * ui_scale, center_w, bar_h * 0.45f, 1.10f * ui_scale, Typography::COLOR_MUTED, 0.75f * ui_scale, FontType::Tech);

        // Mini track line & thumb
        float track_y = bar_y + bar_h * 0.70f;
        float track_w = std::min(center_w * 0.75f, 240.0f * ui_scale);
        float track_x = center_x1 + (center_w - track_w) * 0.5f;
        draw_rect(track_x, track_y, track_w, 3.0f, glm::vec4(0.12f, 0.18f, 0.25f, 0.6f));
        float pct = max_offset > 0.0f ? std::clamp(m_carousel_scroll_x / max_offset, 0.0f, 1.0f) : 0.0f;
        float thumb_w = std::max(28.0f * ui_scale, track_w / static_cast<float>(max_available));
        float thumb_x = track_x + pct * (track_w - thumb_w);
        draw_rect(thumb_x, track_y - 1.0f, thumb_w, 5.0f, glm::vec4(0.0f, 0.85f, 1.0f, 0.85f));
    }

    // LAUNCH button
    bool cur_unlocked  = profile.is_sector_unlocked(selected_level);
    bool cur_completed = profile.is_sector_completed(selected_level);
    bool cur_charted   = profile.is_sector_charted(selected_level);

    float launch_h = bar_h;
    float launch_x = start_x + avail_w - launch_w;
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

    std::string launch_text;
    if (cur_unlocked) {
        if (cur_completed) {
            launch_text = "[ LAUNCH SECTOR " + std::to_string(selected_level) + " (COMPLETED) ]";
        } else if (cur_charted) {
            launch_text = "[ LAUNCH SECTOR " + std::to_string(selected_level) + " ]";
        } else {
            launch_text = "[ GENERATE SECTOR " + std::to_string(selected_level) + " ]";
        }
    } else {
        bool needs_prior = !profile.is_prior_sector_cleared(selected_level);
        if (needs_prior) {
            launch_text = "[ LOCKED // COMPLETE SECTOR " + std::to_string(selected_level - 1) + " FIRST ]";
        } else {
            int cur_req_lvl = UserProfile::get_required_level_for_sector(selected_level);
            launch_text = "[ LOCKED // LEVEL " + std::to_string(cur_req_lvl) + " REQUIRED ]";
        }
    }
    float text_scale = cur_unlocked ? 1.45f * ui_scale : 1.25f * ui_scale;
    draw_text_centered_fitted(launch_text, launch_x, bar_y, launch_w, launch_h, text_scale,
                              !cur_unlocked ? glm::vec4(1.0f, 0.45f, 0.45f, 0.95f) :
                              launch_hov    ? glm::vec4(1.0f) :
                              cur_completed ? Typography::COLOR_GREEN :
                              cur_charted   ? Typography::COLOR_CYAN : Typography::COLOR_AMBER, 0.85f * ui_scale, FontType::Display);
}


void OrbitalHubUI::render_delver_roster(UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    float avail_w = w - 80.0f * ui_scale;
    float card_gap = std::clamp(24.0f * ui_scale, 16.0f, 32.0f);
    float card_w = (avail_w - 2.0f * card_gap) / 3.0f;
    float total_roster_w = card_w * 3.0f + card_gap * 2.0f;
    float start_x = (w - total_roster_w) * 0.5f;
    float start_y = 76.0f * ui_scale;
    float card_h = h - start_y - 24.0f * ui_scale;
    float pad = 18.0f * ui_scale;

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

        glm::vec4 bg_col = !is_unlocked ? glm::vec4(0.04f, 0.04f, 0.06f, 0.92f) :
                           is_selected  ? glm::vec4(0.08f, 0.16f, 0.25f, 0.96f) :
                           is_hovered   ? glm::vec4(0.06f, 0.12f, 0.19f, 0.92f) :
                                          glm::vec4(0.03f, 0.05f, 0.08f, 0.88f);
        glm::vec4 border_col = !is_unlocked ? glm::vec4(0.45f, 0.22f, 0.28f, 0.50f) :
                               is_selected  ? attr.primaryAccentColor :
                               is_hovered   ? glm::vec4(0.45f, 0.88f, 1.0f, 0.95f) :
                                              glm::vec4(0.22f, 0.38f, 0.48f, 0.55f);

        draw_panel_with_border(cx, start_y, card_w, card_h, bg_col, border_col, is_selected ? 3.0f : (is_hovered ? 2.0f : 1.5f));

        float inner_w = card_w - 2.0f * pad;
        float cur_y = start_y + 14.0f * ui_scale;

        // 1. Header: Contractor Badge, Name & Role (Large & High Contrast)
        std::string emblem = (i == 0) ? "[ CONTRACTOR // CLASS 01: DEMOLITIONIST ]" :
                             (i == 1) ? "[ CONTRACTOR // CLASS 02: VANGUARD ]" :
                                        "[ CONTRACTOR // CLASS 03: SCOUT ]";
        draw_text_fitted(emblem, cx + pad, cur_y, inner_w, 1.25f * ui_scale,
                         is_unlocked ? attr.primaryAccentColor : glm::vec4(0.6f, 0.6f, 0.65f, 0.85f), 0.80f * ui_scale, FontType::Tech);
        cur_y += 24.0f * ui_scale;

        draw_text_fitted(attr.name, cx + pad, cur_y, inner_w, 2.20f * ui_scale,
                         is_unlocked ? glm::vec4(1.0f) : glm::vec4(0.65f, 0.65f, 0.70f, 0.85f), 0.95f * ui_scale, FontType::Display);
        cur_y += 34.0f * ui_scale;

        draw_text_fitted(attr.role, cx + pad, cur_y, inner_w, 1.25f * ui_scale,
                         glm::vec4(0.72f, 0.88f, 0.98f, 0.95f), 0.75f * ui_scale, FontType::Tech);
        cur_y += 22.0f * ui_scale;

        draw_rect(cx + pad, cur_y, inner_w, 1.5f, glm::vec4(0.25f, 0.45f, 0.60f, 0.55f));
        cur_y += 10.0f * ui_scale;

        // 2. Field Dossier Box
        float box1_h = 92.0f * ui_scale;
        draw_panel_with_border(cx + pad, cur_y, inner_w, box1_h, glm::vec4(0.02f, 0.04f, 0.07f, 0.70f), glm::vec4(0.18f, 0.32f, 0.45f, 0.45f));
        draw_text("// FIELD OPERATIVE DOSSIER", cx + pad + 8.0f * ui_scale, cur_y + 6.0f * ui_scale, 1.20f * ui_scale, Typography::COLOR_CYAN, FontType::Tech);

        std::string bio = (i == 0) ? "Veteran subterranean sapper equipped with thermal mining rigs and kinetic shockwave emitters. Highly optimized for disintegrating high-density ore clusters." :
                          (i == 1) ? "Frontline expedition point-man fitted with reinforced titanium bulkheads and kinetic aegis barrier tech. Unrivaled resilience under heavy assault." :
                                     "Deep-tier agile surveyor utilizing directional thruster vectors and wideband seismic sonar arrays. Masters speed navigation and cavern escape.";
        auto bio_lines = FontRenderer::wrap_text(bio, inner_w - 16.0f * ui_scale, 1.15f * ui_scale, FontType::Body);
        float bio_y = cur_y + 26.0f * ui_scale;
        for (const auto& bline : bio_lines) {
            draw_text_fitted(bline, cx + pad + 8.0f * ui_scale, bio_y, inner_w - 16.0f * ui_scale, 1.15f * ui_scale, glm::vec4(0.85f, 0.88f, 0.92f, 0.90f), 0.75f * ui_scale, FontType::Body);
            bio_y += 18.0f * ui_scale;
        }
        cur_y += box1_h + 12.0f * ui_scale;

        // 3. Tactical Specifications & Telemetry (Enlarged 12px bars with clear values)
        float box2_h = 205.0f * ui_scale;
        draw_panel_with_border(cx + pad, cur_y, inner_w, box2_h, glm::vec4(0.02f, 0.04f, 0.07f, 0.70f), glm::vec4(0.18f, 0.32f, 0.45f, 0.45f));
        draw_text("// TACTICAL SPECIFICATIONS", cx + pad + 8.0f * ui_scale, cur_y + 6.0f * ui_scale, 1.20f * ui_scale, Typography::COLOR_CYAN, FontType::Tech);

        struct StatBar {
            std::string label;
            std::string val_str;
            float ratio;
        };
        StatBar stats[4] = {
            {"Drill Extraction Speed", std::to_string(static_cast<int>(attr.baseMineSpeed * 100)) + "%", attr.baseMineSpeed / 1.5f},
            {"Exosuit Armor Plating",  std::to_string(static_cast<int>(attr.suitIntegrity)) + " HP", attr.suitIntegrity / 160.0f},
            {"Mobility Vector Speed", std::to_string(static_cast<int>(attr.moveSpeed * 100)) + "%", attr.moveSpeed / 1.3f},
            {"Delver Specialization", (i == 0) ? "Demolitions" : (i == 1) ? "Fortification" : "Surveying", 0.95f}
        };

        float stat_y = cur_y + 26.0f * ui_scale;
        float s_font_h = FontRenderer::get_rendered_height(1.25f * ui_scale, FontType::Tech);
        for (int s = 0; s < 4; ++s) {
            draw_text(stats[s].label, cx + pad + 8.0f * ui_scale, stat_y, 1.25f * ui_scale, glm::vec4(0.88f, 0.92f, 0.96f, 0.95f), FontType::Tech);
            float val_w = FontRenderer::get_rendered_width(stats[s].val_str, 1.25f * ui_scale, FontType::Tech);
            draw_text(stats[s].val_str, cx + card_w - pad - 8.0f * ui_scale - val_w, stat_y, 1.25f * ui_scale, is_unlocked ? attr.primaryAccentColor : glm::vec4(0.65f), FontType::Tech);
            stat_y += s_font_h + 3.0f * ui_scale;

            float bar_h = 10.0f * ui_scale;
            float bar_w = inner_w - 16.0f * ui_scale;
            draw_rect(cx + pad + 8.0f * ui_scale, stat_y, bar_w, bar_h, glm::vec4(0.08f, 0.12f, 0.18f, 0.9f));
            draw_rect(cx + pad + 8.0f * ui_scale, stat_y, bar_w * std::clamp(stats[s].ratio, 0.0f, 1.0f), bar_h, is_unlocked ? attr.primaryAccentColor : glm::vec4(0.4f, 0.45f, 0.5f, 0.6f));
            draw_panel_with_border(cx + pad + 8.0f * ui_scale, stat_y, bar_w, bar_h, glm::vec4(0.0f), glm::vec4(0.2f, 0.35f, 0.5f, 0.4f));
            stat_y += bar_h + 8.0f * ui_scale;
        }
        cur_y += box2_h + 12.0f * ui_scale;

        // 4. Combat Traits & Signature Ability Panel
        float btn_h = std::clamp(48.0f * ui_scale, 42.0f, 54.0f);
        float box3_h = card_h - (cur_y - start_y) - btn_h - 22.0f * ui_scale;
        if (box3_h > 80.0f * ui_scale) {
            draw_panel_with_border(cx + pad, cur_y, inner_w, box3_h, glm::vec4(0.02f, 0.04f, 0.07f, 0.70f), glm::vec4(0.18f, 0.32f, 0.45f, 0.45f));
            draw_text("// COMBAT TRAITS & TACTICAL PROTOCOL", cx + pad + 8.0f * ui_scale, cur_y + 6.0f * ui_scale, 1.20f * ui_scale, Typography::COLOR_CYAN, FontType::Tech);

            float tr_y = cur_y + 24.0f * ui_scale;
            draw_text_fitted("★ PERK: " + attr.traitName, cx + pad + 8.0f * ui_scale, tr_y, inner_w - 16.0f * ui_scale, 1.30f * ui_scale,
                             Typography::COLOR_AMBER, 0.80f * ui_scale, FontType::Display);
            tr_y += 22.0f * ui_scale;

            std::string perk_summary = (i == 0) ? "+35% Mining drill power & substantially reduced overheat rates." :
                                       (i == 1) ? "+50% Kinetic barrier integrity & direct incoming damage dampening." :
                                                  "+30% Sprint mobility velocity & expanded acoustic sonar detection.";
            draw_text_fitted(perk_summary, cx + pad + 8.0f * ui_scale, tr_y, inner_w - 16.0f * ui_scale, 1.15f * ui_scale,
                             glm::vec4(0.85f, 0.90f, 0.95f, 0.90f), 0.75f * ui_scale, FontType::Body);
            tr_y += 26.0f * ui_scale;

            std::string ability_title = (i == 0) ? "⚡ ABILITY [C]: SEISMIC SHOCKWAVE" :
                                        (i == 1) ? "⚡ ABILITY [C]: KINETIC REPULSOR" :
                                                   "⚡ ABILITY [C]: PHASE THRUST DASH";
            draw_text_fitted(ability_title, cx + pad + 8.0f * ui_scale, tr_y, inner_w - 16.0f * ui_scale, 1.30f * ui_scale,
                             Typography::COLOR_CYAN, 0.80f * ui_scale, FontType::Display);
            tr_y += 22.0f * ui_scale;

            std::string ability_summary = (i == 0) ? "Discharges a high-yield acoustic blast that stuns surrounding aberrants." :
                                          (i == 1) ? "Projects an omni-directional barrier deflecting incoming kinetic attacks." :
                                                     "Executes an omni-directional velocity dash slipping past hostile grasping claws.";
            draw_text_fitted(ability_summary, cx + pad + 8.0f * ui_scale, tr_y, inner_w - 16.0f * ui_scale, 1.15f * ui_scale,
                             glm::vec4(0.82f, 0.88f, 0.95f, 0.90f), 0.75f * ui_scale, FontType::Body);
        }

        // 5. Selection Action Button at card bottom
        float btn_y = start_y + card_h - btn_h - 12.0f * ui_scale;
        float btn_w = inner_w;
        float btn_x = cx + pad;

        glm::vec4 sel_col = !is_unlocked ? glm::vec4(0.25f, 0.10f, 0.12f, 0.85f) :
                            is_selected  ? glm::vec4(0.15f, 0.65f, 0.4f, 1.0f) :
                            is_hovered   ? glm::vec4(0.2f, 0.45f, 0.75f, 1.0f) :
                                           glm::vec4(0.1f, 0.2f, 0.3f, 0.85f);
        glm::vec4 b_border = !is_unlocked ? glm::vec4(0.7f, 0.25f, 0.25f, 0.7f) :
                             is_selected  ? glm::vec4(0.4f, 1.0f, 0.6f, 1.0f) : border_col;
        draw_panel_with_border(btn_x, btn_y, btn_w, btn_h, sel_col, b_border, is_selected ? 2.5f : 1.5f);

        std::string btn_txt = !is_unlocked ? "[ LOCKED // REQUIRES LEVEL " + std::to_string(req_lvl) + " ]" :
                              is_selected  ? "[ ACTIVE DELVER ASSIGNED ]" :
                              is_hovered   ? "[ CLICK TO SELECT DELVER ]" : "[ SELECT DELVER ]";
        draw_text_centered_fitted(btn_txt, btn_x, btn_y, btn_w, btn_h, 1.40f * ui_scale,
                                  !is_unlocked ? glm::vec4(1.0f, 0.45f, 0.45f, 0.95f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.85f * ui_scale, FontType::Display);
    }
}

void OrbitalHubUI::render_upgrade_terminal(UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked, bool mouse_down, float scroll_delta) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    float terminal_w = std::clamp(w * 0.94f, 680.0f, 1500.0f);
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
    float top_h = 48.0f * ui_scale;
    draw_panel_with_border(start_x, start_y, terminal_w, top_h, glm::vec4(0.04f, 0.07f, 0.11f, 0.95f), glm::vec4(0.2f, 0.45f, 0.65f, 0.6f));

    // Respec Button: Sleek, high-contrast, well-padded tactical button (100% full recovery of EXP)
    float respec_w = std::clamp(260.0f * ui_scale, 200.0f, 300.0f);
    float respec_h = 36.0f * ui_scale;
    float respec_x = start_x + terminal_w - respec_w - 10.0f * ui_scale;
    float respec_y = start_y + (top_h - respec_h) * 0.5f;
    bool respec_hov = (mouse_x >= respec_x && mouse_x <= respec_x + respec_w &&
                       mouse_y >= respec_y && mouse_y <= respec_y + respec_h);

    if (respec_hov && mouse_clicked) {
        int ref_exp = 0;
        profile.upgrades.respec(ref_exp);
        profile.total_exp += ref_exp;
        m_profile_dirty = true;
        m_purchase_pulse_timer = 1.0f;
        m_terminal_msg = "RESPEC COMPLETE: REFUNDED 100% (" + std::to_string(ref_exp) + " EXP). READY TO RE-ALLOCATE!";
        m_terminal_msg_col = glm::vec4(0.2f, 0.95f, 0.4f, 1.0f);
    }

    glm::vec4 respec_bg = respec_hov ? glm::vec4(0.55f, 0.18f, 0.20f, 0.98f) : glm::vec4(0.20f, 0.08f, 0.10f, 0.92f);
    glm::vec4 respec_border = respec_hov ? Typography::COLOR_CRIMSON : glm::vec4(0.85f, 0.35f, 0.35f, 0.75f);
    draw_panel_with_border(respec_x, respec_y, respec_w, respec_h, respec_bg, respec_border);
    draw_text_centered_fitted("[RESPEC (100% EXP REFUND)]", respec_x, respec_y, respec_w, respec_h, 1.20f * ui_scale,
                              respec_hov ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(1.0f, 0.85f, 0.85f, 0.95f), 0.80f * ui_scale, FontType::Display);

    // Top Balance Line (EXP-only progression economy - Clean Minimalistic Badges)
    int player_lvl = profile.get_player_level();
    int assigned_pts = profile.upgrades.get_total_assigned_points();
    int max_assignable = profile.get_max_assignable_points();
    bool game_completed = profile.is_game_completed();

    float top_text_y = start_y + (top_h - FontRenderer::get_rendered_height(1.35f * ui_scale, FontType::Tech)) * 0.5f;

    std::string exp_str = "<exp> " + std::to_string(profile.total_exp) + " EXP";
    draw_text(exp_str, start_x + 14.0f * ui_scale, top_text_y, 1.38f * ui_scale, Typography::COLOR_AMBER, FontType::Tech);
    float exp_w = FontRenderer::get_rendered_width(exp_str, 1.38f * ui_scale, FontType::Tech);

    std::string pts_str = "POINTS: " + std::to_string(assigned_pts) + " / " + std::to_string(max_assignable);
    float pts_x = start_x + 14.0f * ui_scale + exp_w + 24.0f * ui_scale;
    draw_text(pts_str, pts_x, top_text_y, 1.32f * ui_scale, Typography::COLOR_CYAN, FontType::Tech);
    float pts_w = FontRenderer::get_rendered_width(pts_str, 1.32f * ui_scale, FontType::Tech);

    std::string cap_status = game_completed ? "[GRANDMASTER UNLOCKED]" :
                             (assigned_pts >= UpgradeTree::CAMPAIGN_POINT_CAP ? "[CAP: 24/24 - RESPEC TO SWITCH]" : "[CAMPAIGN CAP: 24 PTS]");
    float cap_x = pts_x + pts_w + 20.0f * ui_scale;
    float max_cap_w = std::max(60.0f, respec_x - cap_x - 12.0f * ui_scale);
    draw_text_fitted(cap_status, cap_x, top_text_y, max_cap_w, 1.22f * ui_scale,
                     assigned_pts >= UpgradeTree::CAMPAIGN_POINT_CAP && !game_completed ? glm::vec4(1.0f, 0.65f, 0.2f, 0.95f) : Typography::COLOR_MUTED, 0.80f * ui_scale, FontType::Tech);

    // Sub-row: Terminal Status Feedback Line & Test Mode Action Badges
    float subrow_y = start_y + top_h + 4.0f * ui_scale;
    if (!m_terminal_msg.empty()) {
        draw_text(">> " + m_terminal_msg, start_x + 14.0f * ui_scale, subrow_y, 1.15f * ui_scale, m_terminal_msg_col, FontType::Tech);
    }

    if (m_test_mode) {
        float grant_w = std::clamp(145.0f * ui_scale, 115.0f, 175.0f);
        float grant_h = 22.0f * ui_scale;
        float grant_x = start_x + terminal_w - grant_w - 10.0f * ui_scale;
        float grant_y = subrow_y;
        bool grant_hov = (mouse_x >= grant_x && mouse_x <= grant_x + grant_w &&
                          mouse_y >= grant_y && mouse_y <= grant_y + grant_h);

        if (grant_hov && mouse_clicked) {
            profile.grant_exp(1000);
            m_profile_dirty = true;
            m_purchase_pulse_timer = 1.0f;
            m_terminal_msg = "TEST GRANT: +1000 EXP ADDED (F5)";
            m_terminal_msg_col = glm::vec4(0.2f, 0.95f, 0.4f, 1.0f);
        }

        glm::vec4 grant_bg = grant_hov ? glm::vec4(0.15f, 0.45f, 0.25f, 1.0f) : glm::vec4(0.08f, 0.22f, 0.14f, 0.85f);
        draw_panel_with_border(grant_x, grant_y, grant_w, grant_h, grant_bg, glm::vec4(0.3f, 0.85f, 0.45f, 0.7f));
        draw_text_centered_fitted("[+1000 EXP (F5)]", grant_x, grant_y, grant_w, grant_h, 1.10f * ui_scale, glm::vec4(0.6f, 1.0f, 0.7f, 1.0f), 0.75f * ui_scale, FontType::Tech);

        float cycle_w = std::clamp(155.0f * ui_scale, 125.0f, 185.0f);
        float cycle_h = grant_h;
        float cycle_x = grant_x - cycle_w - 8.0f * ui_scale;
        float cycle_y = subrow_y;
        bool cycle_hov = (mouse_x >= cycle_x && mouse_x <= cycle_x + cycle_w &&
                          mouse_y >= cycle_y && mouse_y <= cycle_y + cycle_h);

        if (cycle_hov && mouse_clicked) {
            if (profile.total_exp < 500) profile.grant_exp(1000);
            profile.upgrades.purchase(UpgradeType::DrillSpeed, profile.get_player_level(), profile.total_exp);
            profile.upgrades.purchase(UpgradeType::ThrusterTank, profile.get_player_level(), profile.total_exp);
            int ref_exp = 0;
            profile.upgrades.respec(ref_exp);
            profile.total_exp += ref_exp;
            m_profile_dirty = true;
            m_purchase_pulse_timer = 1.0f;
            m_terminal_msg = "TEST CYCLE: PURCHASED & RESPECCED (REFUNDED 100%: " + std::to_string(ref_exp) + " EXP)";
            m_terminal_msg_col = glm::vec4(0.3f, 0.85f, 1.0f, 1.0f);
        }

        glm::vec4 cycle_bg = cycle_hov ? glm::vec4(0.2f, 0.32f, 0.48f, 1.0f) : glm::vec4(0.10f, 0.18f, 0.28f, 0.85f);
        draw_panel_with_border(cycle_x, cycle_y, cycle_w, cycle_h, cycle_bg, glm::vec4(0.35f, 0.65f, 0.9f, 0.7f));
        draw_text_centered_fitted("[TEST RESPEC (F6)]", cycle_x, cycle_y, cycle_w, cycle_h, 1.10f * ui_scale, glm::vec4(0.8f, 0.9f, 1.0f, 1.0f), 0.75f * ui_scale, FontType::Tech);
    }

    // ── Main Content Area: Skill Tree Web (Left) & Inspector (Right) ──
    float foot_h = 38.0f * ui_scale;
    float foot_y = h - foot_h - 10.0f * ui_scale;
    float main_y = start_y + top_h + 24.0f * ui_scale;
    float main_h = foot_y - main_y - 8.0f * ui_scale;

    float tree_w = std::round(terminal_w * 0.62f);
    float insp_x = start_x + tree_w + 14.0f * ui_scale;
    float insp_w = terminal_w - tree_w - 14.0f * ui_scale;

    // ── 1. Skill Tree Web Grid Background & Frame ──
    draw_panel_with_border(start_x, main_y, tree_w, main_h, glm::vec4(0.02f, 0.035f, 0.055f, 0.96f), glm::vec4(0.18f, 0.32f, 0.48f, 0.70f), 1.5f);
    draw_rect(start_x, main_y, tree_w, 2.5f, Typography::COLOR_CYAN_GLOW);

    // Web Canvas Top Banner
    float web_hdr_h = 30.0f * ui_scale;
    draw_panel_with_border(start_x, main_y, tree_w, web_hdr_h, glm::vec4(0.04f, 0.07f, 0.11f, 0.95f), glm::vec4(0.15f, 0.28f, 0.42f, 0.5f));
    draw_text("[ DELVER NEURAL MATRIX // TALENT TREE ]", start_x + 12.0f * ui_scale, main_y + 7.0f * ui_scale, 1.30f * ui_scale, Typography::COLOR_CYAN, FontType::Display);
    std::string proto_str = "[ ◄ DRAG CANVAS / SCROLL WHEEL TO EXPLORE 6 BRANCHES ► ]";
    draw_text_fitted(proto_str, start_x + tree_w - 380.0f * ui_scale, main_y + 8.0f * ui_scale, 370.0f * ui_scale, 1.15f * ui_scale,
                     Typography::COLOR_AMBER, 0.80f * ui_scale, FontType::Tech);

    // 6 Disciplines across 3 Tiers (18 Nodes Total)
    UpgradeType branch_nodes[6][3] = {
        { UpgradeType::DrillSpeed,         UpgradeType::DrillDurability,  UpgradeType::MicroCharges },       // EXCAVATION
        { UpgradeType::ThrusterTank,       UpgradeType::KineticDynamo,    UpgradeType::SonarFrequency },     // TRAVERSAL
        { UpgradeType::ReinforcedPlating,  UpgradeType::KineticShielding, UpgradeType::Defibrillator },      // DEFENSE
        { UpgradeType::AcousticDampening,  UpgradeType::ShadowVeil,       UpgradeType::AmbushProtocol },     // STEALTH
        { UpgradeType::PneumaticImpact,    UpgradeType::CleaveShockwave,  UpgradeType::BerserkerOverdrive }, // MELEE
        { UpgradeType::HyperVelocitySlugs, UpgradeType::RapidCycling,     UpgradeType::CriticalWeakpoint }   // BALLISTICS
    };

    std::string branch_titles[6] = { "EXCAVATION", "TRAVERSAL", "DEFENSE", "STEALTH", "MELEE", "BALLISTICS" };
    glm::vec4 branch_colors[6] = {
        glm::vec4(1.0f, 0.65f, 0.15f, 1.0f),  // Amber / Topaz
        glm::vec4(0.2f, 0.85f, 1.0f, 1.0f),   // Cyan / Diamond
        glm::vec4(0.35f, 0.65f, 1.0f, 1.0f),  // Cobalt / Sapphire
        glm::vec4(0.78f, 0.42f, 1.0f, 1.0f),  // Amethyst / Purple
        glm::vec4(1.0f, 0.35f, 0.35f, 1.0f),  // Vermilion / Crimson
        glm::vec4(1.0f, 0.85f, 0.25f, 1.0f)   // Gold / Amber
    };

    float col_w = 230.0f * ui_scale;
    float col_gap = 16.0f * ui_scale;
    float col_pad = 16.0f * ui_scale;
    float total_tree_w = 6.0f * col_w + 5.0f * col_gap + 2.0f * col_pad;
    float max_scroll_x = std::max(0.0f, total_tree_w - tree_w);

    // Mouse Dragging & Momentum / Scroll Wheel Handling
    bool mouse_in_tree = (mouse_x >= start_x && mouse_x <= start_x + tree_w &&
                          mouse_y >= main_y && mouse_y <= main_y + main_h);

    if (mouse_in_tree) {
        if (std::abs(scroll_delta) > 0.01f) {
            m_upgrades_target_x -= scroll_delta * 90.0f * ui_scale;
        }

        if (mouse_down) {
            if (!m_is_dragging_upgrades) {
                m_is_dragging_upgrades = true;
                m_drag_start_upgrades_x = mouse_x;
                m_drag_total_dist_upgrades = 0.0f;
                m_last_upgrades_mouse_x = mouse_x;
            } else {
                float dx = mouse_x - m_last_upgrades_mouse_x;
                m_upgrades_target_x -= dx;
                m_drag_total_dist_upgrades += std::abs(dx);
                m_last_upgrades_mouse_x = mouse_x;
            }
        }
    }
    if (!mouse_down) {
        m_is_dragging_upgrades = false;
    }

    m_upgrades_target_x = std::clamp(m_upgrades_target_x, 0.0f, max_scroll_x);
    m_upgrades_scroll_x += (m_upgrades_target_x - m_upgrades_scroll_x) * 0.25f;
    if (std::abs(m_upgrades_target_x - m_upgrades_scroll_x) < 0.2f) {
        m_upgrades_scroll_x = m_upgrades_target_x;
    }

    // Horizontal Scrollbar indicator track right beneath the header banner
    float track_y = main_y + web_hdr_h;
    float track_h = 5.0f * ui_scale;
    draw_rect(start_x, track_y, tree_w, track_h, glm::vec4(0.05f, 0.08f, 0.12f, 0.95f));
    if (max_scroll_x > 0.0f) {
        float thumb_ratio = tree_w / total_tree_w;
        float thumb_w = std::max(60.0f * ui_scale, tree_w * thumb_ratio);
        float thumb_x = start_x + (m_upgrades_scroll_x / max_scroll_x) * (tree_w - thumb_w);
        draw_rect(thumb_x, track_y, thumb_w, track_h, Typography::COLOR_CYAN);
    }

    float web_y0 = main_y + web_hdr_h + track_h + 4.0f * ui_scale;
    float sec_hdr_h = 24.0f * ui_scale;
    float content_h = main_h - web_hdr_h - track_h - sec_hdr_h - 18.0f * ui_scale;
    float node_h = std::clamp((content_h - 32.0f * ui_scale) / 3.0f, 95.0f, 130.0f);
    float conduit_v_gap = (content_h - 3.0f * node_h) / 2.0f;

    float row_y[3];
    row_y[2] = web_y0 + sec_hdr_h + 4.0f * ui_scale;             // Capstone (Top)
    row_y[1] = row_y[2] + node_h + conduit_v_gap;               // Specialization (Middle)
    row_y[0] = row_y[1] + node_h + conduit_v_gap;               // Foundation (Bottom)

    m_selected_upgrade_idx = std::clamp(m_selected_upgrade_idx, 0, static_cast<int>(UpgradeType::COUNT) - 1);

    // OpenGL Scissor Clipping for the tree viewport so scrolling nodes never bleed
    GLint scissor_x = static_cast<GLint>(start_x);
    GLint scissor_y = static_cast<GLint>(m_height - (main_y + main_h));
    GLsizei scissor_w = static_cast<GLsizei>(tree_w);
    GLsizei scissor_h = static_cast<GLsizei>(main_h - web_hdr_h - track_h);

    glEnable(GL_SCISSOR_TEST);
    glScissor(scissor_x, std::max(0, scissor_y), scissor_w, std::max(0, scissor_h));

    // Tactical Coordinate Web Grid Mesh
    float grid_step = 36.0f * ui_scale;
    float grid_start_x = start_x - std::fmod(m_upgrades_scroll_x, grid_step);
    for (float gx = grid_start_x; gx < start_x + tree_w + grid_step; gx += grid_step) {
        draw_rect(gx, web_y0, 1.0f, main_h - (web_y0 - main_y), glm::vec4(0.10f, 0.18f, 0.26f, 0.18f));
    }
    for (float gy = web_y0 + 10.0f * ui_scale; gy < main_y + main_h - 4.0f * ui_scale; gy += grid_step) {
        draw_rect(start_x, gy, tree_w, 1.0f, glm::vec4(0.10f, 0.18f, 0.26f, 0.18f));
    }

    // Bottom Central Neural Core Bus linking all 6 foundation root nodes
    float bus_y = row_y[0] + node_h + 4.0f * ui_scale;
    float bus_start_x = start_x + col_pad + col_w * 0.5f - m_upgrades_scroll_x;
    float bus_end_x = start_x + col_pad + 5.0f * (col_w + col_gap) + col_w * 0.5f - m_upgrades_scroll_x;
    draw_rect(bus_start_x, bus_y, bus_end_x - bus_start_x, 3.0f, glm::vec4(0.2f, 0.75f, 0.95f, 0.55f));
    for (int b = 0; b < 6; ++b) {
        float jx = start_x + col_pad + b * (col_w + col_gap) + col_w * 0.5f - m_upgrades_scroll_x;
        draw_rect(jx - 4.0f, bus_y - 2.0f, 8.0f, 7.0f, Typography::COLOR_CYAN);
        draw_rect(jx - 1.5f, row_y[0] + node_h, 3.0f, 4.0f * ui_scale, Typography::COLOR_CYAN * glm::vec4(1.0f, 1.0f, 1.0f, 0.7f));
    }

    // Render Web Sectors, Vertical Conduits & Nodes
    for (int b = 0; b < 6; ++b) {
        float cx = start_x + col_pad + b * (col_w + col_gap) - m_upgrades_scroll_x;

        // Discipline Sector Header
        draw_panel_with_border(cx, web_y0, col_w, sec_hdr_h, glm::vec4(0.04f, 0.07f, 0.11f, 0.92f), branch_colors[b] * glm::vec4(1.0f, 1.0f, 1.0f, 0.6f), 1.5f);
        draw_rect(cx, web_y0, 3.0f, sec_hdr_h, branch_colors[b]);
        draw_text_fitted("// " + branch_titles[b], cx + 8.0f * ui_scale, web_y0 + 4.0f * ui_scale, col_w - 12.0f * ui_scale, 1.25f * ui_scale, branch_colors[b], 0.85f * ui_scale, FontType::Display);

        // Vertical Conduits between nodes
        float mid_x = cx + col_w * 0.5f;
        // Conduit 0 -> 1
        float v01_top = row_y[1] + node_h;
        float v01_bot = row_y[0];
        int t0 = profile.upgrades.get_tier(branch_nodes[b][0]);
        bool cond01_active = (t0 >= 1);
        draw_rect(mid_x - 1.5f, v01_top, 3.0f, v01_bot - v01_top,
                  cond01_active ? branch_colors[b] : glm::vec4(0.20f, 0.24f, 0.28f, 0.45f));

        // Conduit 1 -> 2
        float v12_top = row_y[2] + node_h;
        float v12_bot = row_y[1];
        int t1 = profile.upgrades.get_tier(branch_nodes[b][1]);
        bool cond12_active = (t1 >= 1);
        draw_rect(mid_x - 1.5f, v12_top, 3.0f, v12_bot - v12_top,
                  cond12_active ? branch_colors[b] : glm::vec4(0.20f, 0.24f, 0.28f, 0.45f));

        // Render 3 Nodes: r=0 (Foundation), r=1 (Specialization), r=2 (Capstone)
        for (int r = 0; r < 3; ++r) {
            UpgradeType type = branch_nodes[b][r];
            int type_idx = static_cast<int>(type);
            float ny = row_y[r];

            int cur_tier = profile.upgrades.get_tier(type);
            int max_tier = UpgradeTree::get_max_tier(type);
            int exp_cost = UpgradeTree::get_exp_cost(cur_tier);
            int req_lvl = UpgradeTree::get_required_level_for_tier(cur_tier);
            bool is_maxed = (cur_tier >= max_tier);
            bool can_buy = profile.upgrades.can_purchase(type, player_lvl, profile.total_exp, game_completed);
            bool is_selected = (m_selected_upgrade_idx == type_idx);

            NodeWebState web_state = profile.upgrades.get_node_web_state(type, player_lvl, profile.total_exp, game_completed);
            bool is_node_greyed = (web_state == NodeWebState::GreyedOutLocked);
            bool is_node_capped = (web_state == NodeWebState::CampaignCapped);

            bool node_hov = (mouse_in_tree && mouse_x >= cx && mouse_x <= cx + col_w && mouse_y >= ny && mouse_y <= ny + node_h);
            if (node_hov && mouse_clicked && m_drag_total_dist_upgrades < 6.0f) {
                m_selected_upgrade_idx = type_idx;
            }

            // Node Panel Background & Selection Border
            glm::vec4 n_bg = is_node_greyed ? glm::vec4(0.040f, 0.050f, 0.065f, 0.94f) :
                             is_selected    ? glm::vec4(0.08f, 0.14f, 0.22f, 0.98f) :
                             node_hov       ? glm::vec4(0.06f, 0.11f, 0.17f, 0.95f) :
                                              glm::vec4(0.038f, 0.058f, 0.085f, 0.92f);

            glm::vec4 n_border = is_selected ? (is_node_greyed ? glm::vec4(0.7f, 0.8f, 0.9f, 0.95f) : branch_colors[b]) :
                                 node_hov    ? (is_node_greyed ? glm::vec4(0.40f, 0.45f, 0.50f, 0.85f) : glm::vec4(0.45f, 0.85f, 1.0f, 0.95f)) :
                                 is_node_greyed ? glm::vec4(0.20f, 0.23f, 0.26f, 0.45f) :
                                 (cur_tier > 0) ? branch_colors[b] * 0.85f : glm::vec4(0.22f, 0.35f, 0.48f, 0.65f);

            draw_panel_with_border(cx, ny, col_w, node_h, n_bg, n_border, is_selected ? 2.5f : 1.5f);

            if (is_selected) {
                glm::vec4 bracket_col = is_node_greyed ? glm::vec4(0.7f, 0.8f, 0.9f, 1.0f) : branch_colors[b];
                draw_rect(cx, ny, 6.0f, 2.5f, bracket_col);
                draw_rect(cx, ny, 2.5f, 6.0f, bracket_col);
                draw_rect(cx + col_w - 6.0f, ny, 6.0f, 2.5f, bracket_col);
                draw_rect(cx + col_w - 2.5f, ny, 2.5f, 6.0f, bracket_col);
            }

            auto info = UpgradeTree::get_info(type);

            // Node Title (Large & High Contrast)
            glm::vec4 title_col = is_node_greyed ? glm::vec4(0.50f, 0.55f, 0.60f, 0.85f) :
                                  is_selected    ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) :
                                                   glm::vec4(0.90f, 0.94f, 0.98f, 0.98f);
            draw_text_fitted(info.name, cx + 8.0f * ui_scale, ny + 6.0f * ui_scale, col_w - 16.0f * ui_scale, 1.25f * ui_scale, title_col, 0.85f * ui_scale, FontType::Display);

            // Subtitle / Tier classification
            std::string sub_title = (r == 0) ? "FOUNDATION ROOT" : (r == 1) ? "SPECIALIZATION" : "APEX CAPSTONE";
            if (is_node_greyed) sub_title = "[LOCKED // REQ T1]";
            draw_text_fitted(sub_title, cx + 8.0f * ui_scale, ny + 24.0f * ui_scale, col_w - 16.0f * ui_scale, 1.10f * ui_scale,
                             is_node_greyed ? glm::vec4(0.45f, 0.50f, 0.55f, 0.75f) : branch_colors[b] * 0.90f, 0.75f * ui_scale, FontType::Tech);

            // Progress Pips [ ■ ■ □ ]
            float pip_start_x = cx + 8.0f * ui_scale;
            float pip_y = ny + 40.0f * ui_scale;
            float pip_size = std::clamp(11.0f * ui_scale, 9.0f, 13.0f);
            float pip_gap = 4.0f * ui_scale;

            for (int p = 0; p < max_tier; ++p) {
                float px = pip_start_x + p * (pip_size + pip_gap);
                bool filled = (p < cur_tier);
                glm::vec4 pip_col = is_node_greyed ? glm::vec4(0.14f, 0.16f, 0.18f, 0.5f) :
                                    filled ? (is_maxed ? Typography::COLOR_AMBER : branch_colors[b]) :
                                    (p == cur_tier && can_buy) ? Typography::COLOR_CYAN :
                                                                 glm::vec4(0.12f, 0.18f, 0.24f, 0.75f);
                draw_rect(px, pip_y, pip_size, pip_size, pip_col);
                draw_panel_with_border(px, pip_y, pip_size, pip_size, glm::vec4(0.0f), glm::vec4(0.25f, 0.4f, 0.55f, 0.5f));
            }

            std::string tier_str = "TIER " + std::to_string(cur_tier) + "/" + std::to_string(max_tier);
            draw_text(tier_str, pip_start_x + max_tier * (pip_size + pip_gap) + 8.0f * ui_scale, pip_y, 1.15f * ui_scale,
                      is_node_greyed ? glm::vec4(0.45f, 0.50f, 0.55f, 0.75f) :
                      is_maxed ? Typography::COLOR_AMBER : glm::vec4(0.88f, 0.92f, 0.98f, 0.95f), FontType::Tech);

            // Stat Preview
            std::string cur_stat = profile.upgrades.get_current_stat_string(type);
            draw_text_fitted(cur_stat, cx + 8.0f * ui_scale, ny + 58.0f * ui_scale, col_w - 16.0f * ui_scale, 1.15f * ui_scale,
                             is_node_greyed ? glm::vec4(0.42f, 0.46f, 0.50f, 0.70f) : Typography::COLOR_CYAN, 0.75f * ui_scale, FontType::Tech);

            // Bottom Tag
            std::string cost_tag;
            glm::vec4 tag_col;
            if (is_node_greyed) {
                cost_tag = "[PREREQ LOCKED]";
                tag_col = glm::vec4(0.50f, 0.55f, 0.60f, 0.75f);
            } else if (is_maxed) {
                cost_tag = "[TIER MASTERED]";
                tag_col = Typography::COLOR_AMBER;
            } else if (is_node_capped) {
                cost_tag = "[CAP REACHED (24)]";
                tag_col = glm::vec4(1.0f, 0.65f, 0.25f, 0.95f);
            } else if (player_lvl < req_lvl) {
                cost_tag = "REQ LEVEL " + std::to_string(req_lvl);
                tag_col = glm::vec4(1.0f, 0.55f, 0.45f, 0.95f);
            } else {
                cost_tag = "<exp> " + std::to_string(exp_cost) + " EXP";
                tag_col = can_buy ? Typography::COLOR_GREEN : glm::vec4(0.95f, 0.75f, 0.35f, 0.95f);
            }

            float tag_h = 22.0f * ui_scale;
            float tag_y = ny + node_h - tag_h - 4.0f * ui_scale;
            draw_panel_with_border(cx + 6.0f * ui_scale, tag_y, col_w - 12.0f * ui_scale, tag_h, glm::vec4(0.02f, 0.04f, 0.07f, 0.90f), glm::vec4(0.2f, 0.35f, 0.5f, 0.4f));
            draw_text_centered_fitted(cost_tag, cx + 6.0f * ui_scale, tag_y, col_w - 12.0f * ui_scale, tag_h, 1.15f * ui_scale, tag_col, 0.75f * ui_scale, FontType::Tech);
        }
    }

    glDisable(GL_SCISSOR_TEST);

    // ── 2. Talent Inspector Panel (Right Panel - Clean, Minimalistic & Large Text) ──
    UpgradeType sel_type = static_cast<UpgradeType>(m_selected_upgrade_idx);
    auto sel_info = UpgradeTree::get_info(sel_type);
    int sel_tier = profile.upgrades.get_tier(sel_type);
    int sel_max_tier = UpgradeTree::get_max_tier(sel_type);
    int sel_exp_cost = UpgradeTree::get_exp_cost(sel_tier);
    int sel_req_lvl = UpgradeTree::get_required_level_for_tier(sel_tier);
    bool sel_is_maxed = (sel_tier >= sel_max_tier);
    bool sel_can_buy = profile.upgrades.can_purchase(sel_type, player_lvl, profile.total_exp, game_completed);
    UpgradePrerequisite sel_prereq = UpgradeTree::get_prerequisite(sel_type);
    bool prereq_met = !sel_prereq.has_prerequisite || (profile.upgrades.get_tier(sel_prereq.required_type) >= sel_prereq.required_tier);
    NodeWebState sel_web_state = profile.upgrades.get_node_web_state(sel_type, player_lvl, profile.total_exp, game_completed);
    bool sel_is_greyed = (sel_web_state == NodeWebState::GreyedOutLocked);
    bool sel_is_capped = (sel_web_state == NodeWebState::CampaignCapped);

    // Inspector Panel Body
    draw_panel_with_border(insp_x, main_y, insp_w, main_h, glm::vec4(0.04f, 0.07f, 0.11f, 0.96f), glm::vec4(0.22f, 0.45f, 0.65f, 0.8f), 1.5f);
    draw_rect(insp_x, main_y, insp_w, 3.0f, Typography::COLOR_CYAN_GLOW);

    float ix = insp_x + 16.0f * ui_scale;
    float iy = main_y + 16.0f * ui_scale;
    float iw = insp_w - 32.0f * ui_scale;

    // Discipline Tag & Skill Title
    draw_text_fitted("// " + sel_info.discipline + " DISCIPLINE", ix, iy, iw, 1.30f * ui_scale, Typography::COLOR_CYAN, 0.80f * ui_scale, FontType::Tech);
    iy += 24.0f * ui_scale;
    draw_text_fitted(sel_info.name, ix, iy, iw, 2.10f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.95f * ui_scale, FontType::Display);
    iy += 34.0f * ui_scale;
    draw_rect(ix, iy, iw, 1.5f, glm::vec4(0.25f, 0.45f, 0.65f, 0.60f));
    iy += 12.0f * ui_scale;

    // Prerequisite Alert Box (only if locked)
    if (sel_is_greyed) {
        float wb_h = 28.0f * ui_scale;
        draw_panel_with_border(ix, iy, iw, wb_h, glm::vec4(0.18f, 0.08f, 0.10f, 0.9f), glm::vec4(0.85f, 0.35f, 0.35f, 0.7f));
        draw_text_fitted("! GREYED OUT: PREREQUISITE UNMET", ix + 8.0f * ui_scale, iy + 6.0f * ui_scale, iw - 16.0f * ui_scale, 1.15f * ui_scale, glm::vec4(1.0f, 0.6f, 0.6f, 1.0f), 0.75f * ui_scale, FontType::Tech);
        iy += wb_h + 10.0f * ui_scale;
    } else if (sel_is_capped) {
        float cb_h = 28.0f * ui_scale;
        draw_panel_with_border(ix, iy, iw, cb_h, glm::vec4(0.22f, 0.14f, 0.05f, 0.9f), glm::vec4(1.0f, 0.65f, 0.2f, 0.8f));
        draw_text_fitted("! CAMPAIGN CAP (24 PTS): RESPEC TO SWITCH", ix + 8.0f * ui_scale, iy + 6.0f * ui_scale, iw - 16.0f * ui_scale, 1.15f * ui_scale, glm::vec4(1.0f, 0.85f, 0.3f, 1.0f), 0.75f * ui_scale, FontType::Tech);
        iy += cb_h + 10.0f * ui_scale;
    }

    // Tier Progress & Stat Progression (Large & High Contrast)
    std::string rank_label = "TIER " + std::to_string(sel_tier) + " / " + std::to_string(sel_max_tier);
    draw_text(rank_label, ix, iy, 1.35f * ui_scale, Typography::COLOR_AMBER, FontType::Tech);
    iy += 24.0f * ui_scale;

    std::string cur_stat_line = "CURRENT: " + profile.upgrades.get_current_stat_string(sel_type);
    draw_text_fitted(cur_stat_line, ix, iy, iw, 1.30f * ui_scale, Typography::COLOR_CYAN, 0.85f * ui_scale, FontType::Tech);
    iy += 24.0f * ui_scale;

    if (!sel_is_maxed) {
        std::string next_stat_line = "NEXT:    " + profile.upgrades.get_next_stat_string(sel_type);
        draw_text_fitted(next_stat_line, ix, iy, iw, 1.30f * ui_scale, Typography::COLOR_GREEN, 0.85f * ui_scale, FontType::Tech);
        iy += 26.0f * ui_scale;
    }

    // Description (Clean & Concise)
    auto desc_lines = FontRenderer::wrap_text(sel_info.description, iw, 1.22f * ui_scale, FontType::Body);
    for (const auto& line : desc_lines) {
        draw_text_fitted(line, ix, iy, iw, 1.22f * ui_scale, glm::vec4(0.88f, 0.90f, 0.95f, 0.92f), 0.80f * ui_scale, FontType::Body);
        iy += 20.0f * ui_scale;
    }
    iy += 10.0f * ui_scale;

    if (!sel_info.milestone_perk.empty()) {
        float mb_h = 28.0f * ui_scale;
        draw_panel_with_border(ix, iy, iw, mb_h, glm::vec4(0.12f, 0.20f, 0.16f, 0.9f), glm::vec4(0.2f, 0.85f, 0.45f, 0.7f));
        draw_text_fitted("★ " + sel_info.milestone_perk, ix + 8.0f * ui_scale, iy + 6.0f * ui_scale, iw - 16.0f * ui_scale, 1.20f * ui_scale, glm::vec4(0.6f, 1.0f, 0.7f, 1.0f), 0.75f * ui_scale, FontType::Tech);
        iy += mb_h + 10.0f * ui_scale;
    }

    // Requirements Box (Crisp & Minimalist)
    float cost_box_h = 48.0f * ui_scale;
    draw_panel_with_border(ix, iy, iw, cost_box_h, glm::vec4(0.02f, 0.04f, 0.07f, 0.92f), glm::vec4(0.25f, 0.4f, 0.55f, 0.7f));

    if (sel_is_maxed) {
        draw_text_centered_fitted("[ALL TIERS MASTERED]", ix, iy, iw, cost_box_h, 1.30f * ui_scale, Typography::COLOR_AMBER, 0.80f * ui_scale, FontType::Display);
    } else {
        bool lvl_ok = (player_lvl >= sel_req_lvl);
        bool exp_ok = (profile.total_exp >= sel_exp_cost);

        std::string req_str = "<exp> " + std::to_string(sel_exp_cost) + " EXP   |   DELVER LVL " + std::to_string(sel_req_lvl);
        glm::vec4 req_col = (lvl_ok && exp_ok) ? Typography::COLOR_CYAN :
                            (!exp_ok) ? glm::vec4(1.0f, 0.75f, 0.35f, 1.0f) : glm::vec4(1.0f, 0.45f, 0.45f, 1.0f);
        draw_text_centered_fitted(req_str, ix, iy, iw, cost_box_h, 1.30f * ui_scale, req_col, 0.80f * ui_scale, FontType::Tech);
    }
    iy += cost_box_h + 12.0f * ui_scale;

    // Big Tactile Upgrade Action Button
    float btn_w = iw;
    float btn_h = std::clamp(50.0f * ui_scale, 42.0f, 56.0f);
    float bx = ix;
    float by = iy;

    bool btn_hov = (mouse_x >= bx && mouse_x <= bx + btn_w && mouse_y >= by && mouse_y <= by + btn_h);

    if (btn_hov && mouse_clicked && sel_can_buy && !sel_is_maxed) {
        profile.upgrades.purchase(sel_type, player_lvl, profile.total_exp, game_completed);
        m_profile_dirty = true;
        m_purchase_pulse_timer = 1.0f;
        m_terminal_msg = "UPGRADE ACQUIRED: " + sel_info.name + " [TIER " + std::to_string(sel_tier + 1) + "]";
        m_terminal_msg_col = glm::vec4(0.2f, 0.95f, 0.4f, 1.0f);
    }

    glm::vec4 b_bg = sel_is_maxed ? glm::vec4(0.10f, 0.14f, 0.18f, 0.6f) :
                     !prereq_met  ? glm::vec4(0.25f, 0.12f, 0.10f, 0.8f) :
                     sel_is_capped? glm::vec4(0.28f, 0.16f, 0.08f, 0.85f) :
                     (player_lvl < sel_req_lvl) ? glm::vec4(0.25f, 0.10f, 0.12f, 0.8f) :
                     sel_can_buy  ? (btn_hov ? glm::vec4(0.2f, 0.75f, 0.95f, 1.0f) : glm::vec4(0.12f, 0.45f, 0.75f, 0.95f)) :
                                    glm::vec4(0.15f, 0.18f, 0.22f, 0.65f);

    glm::vec4 b_border = (sel_can_buy && !sel_is_maxed) ? (btn_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.4f, 0.85f, 1.0f, 0.8f)) :
                         glm::vec4(0.22f, 0.35f, 0.45f, 0.6f);

    draw_panel_with_border(bx, by, btn_w, btn_h, b_bg, b_border, (sel_can_buy && !sel_is_maxed) ? 2.5f : 1.5f);

    if (sel_is_maxed) {
        draw_text_centered_fitted("[ MAX RANK MASTERED ]", bx, by, btn_w, btn_h, 1.35f * ui_scale, glm::vec4(0.5f, 0.6f, 0.7f, 0.85f), 0.85f * ui_scale, FontType::Display);
    } else if (!prereq_met) {
        draw_text_centered_fitted("[ LOCKED: REQ " + sel_prereq.required_name + " ]", bx, by, btn_w, btn_h, 1.20f * ui_scale, glm::vec4(1.0f, 0.55f, 0.45f, 0.95f), 0.75f * ui_scale, FontType::Display);
    } else if (sel_is_capped) {
        draw_text_centered_fitted("[ CAMPAIGN CAP: 24 PTS ]", bx, by, btn_w, btn_h, 1.25f * ui_scale, glm::vec4(1.0f, 0.80f, 0.35f, 0.95f), 0.80f * ui_scale, FontType::Display);
    } else if (player_lvl < sel_req_lvl) {
        draw_text_centered_fitted("[ LOCKED: LEVEL " + std::to_string(sel_req_lvl) + " ]", bx, by, btn_w, btn_h, 1.30f * ui_scale, glm::vec4(1.0f, 0.45f, 0.45f, 0.95f), 0.80f * ui_scale, FontType::Display);
    } else if (sel_can_buy) {
        draw_text_centered_fitted("[ UNLOCK (" + std::to_string(sel_exp_cost) + " EXP) ]", bx, by, btn_w, btn_h, 1.45f * ui_scale,
                                  btn_hov ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.95f, 0.98f, 1.0f, 1.0f), 0.90f * ui_scale, FontType::Display);
    } else {
        std::string reason_str = profile.upgrades.get_lock_reason_string(sel_type, player_lvl, profile.total_exp, game_completed);
        draw_text_centered_fitted("[" + reason_str + "]", bx, by, btn_w, btn_h, 1.20f * ui_scale, glm::vec4(0.9f, 0.7f, 0.4f, 0.9f), 0.75f * ui_scale, FontType::Display);
    }

    // ── Bottom Delver Progression Footer ──
    draw_panel_with_border(start_x, foot_y, terminal_w, foot_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.95f), glm::vec4(0.2f, 0.35f, 0.5f, 0.6f));

    // Current Level Badge
    draw_text("DELVER LEVEL: " + std::to_string(player_lvl), start_x + 14.0f * ui_scale, foot_y + 10.0f * ui_scale, 1.30f * ui_scale, Typography::COLOR_AMBER, FontType::Tech);

    // Wide Experience Progress Bar
    float exp_bar_x = start_x + 210.0f * ui_scale;
    float exp_bar_w = terminal_w - 230.0f * ui_scale;
    float exp_bar_h = 20.0f * ui_scale;
    float exp_bar_y = foot_y + (foot_h - exp_bar_h) * 0.5f;

    int cur_exp = profile.total_exp;
    int next_exp = profile.get_next_level_exp_req();
    float exp_progress = (next_exp > 0) ? std::clamp(static_cast<float>(cur_exp) / static_cast<float>(next_exp), 0.0f, 1.0f) : 1.0f;

    draw_rect(exp_bar_x, exp_bar_y, exp_bar_w, exp_bar_h, glm::vec4(0.08f, 0.12f, 0.16f, 0.9f));
    draw_rect(exp_bar_x, exp_bar_y, exp_bar_w * exp_progress, exp_bar_h, glm::vec4(0.2f, 0.75f, 0.95f, 0.85f));
    draw_panel_with_border(exp_bar_x, exp_bar_y, exp_bar_w, exp_bar_h, glm::vec4(0.0f), glm::vec4(0.3f, 0.5f, 0.7f, 0.7f));

    std::string exp_label = "EXP: " + std::to_string(cur_exp) + " / " + std::to_string(next_exp) + " (" + std::to_string(static_cast<int>(exp_progress * 100.0f)) + "%)";
    draw_text_centered_fitted(exp_label, exp_bar_x, exp_bar_y, exp_bar_w, exp_bar_h, 1.15f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 0.95f), 0.75f * ui_scale, FontType::Tech);
}

bool OrbitalHubUI::render_orbital_hub(int selected_level, const SkillMatrix& skills, const PlayerInventory& inventory, float mouse_x, float mouse_y, bool mouse_clicked, bool mouse_down, float scroll_delta) {
    for (int s = 1; s <= 3; ++s) {
        s_fallback_profile.sector_records[s] = inventory.sector_records[s];
    }
    return render_orbital_hub(selected_level, skills, inventory, s_fallback_profile, mouse_x, mouse_y, mouse_clicked, mouse_down, scroll_delta);
}

bool OrbitalHubUI::render_orbital_hub(int selected_level, const SkillMatrix& skills, const PlayerInventory& inventory, UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked, bool mouse_down, float scroll_delta) {
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
    float back_btn_w = std::clamp(180.0f * ui_scale, 140.0f, 210.0f);
    float back_btn_h = std::clamp(34.0f * ui_scale, 28.0f, 40.0f);
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
    draw_text_centered("< TITLE SCREEN", back_btn_x, back_btn_y, back_btn_w, back_btn_h, 1.30f * ui_scale,
                       back_hovered ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY, FontType::Display);

    float title_x = back_btn_x + back_btn_w + 16.0f * ui_scale;
    float title_h = FontRenderer::get_rendered_height(1.85f * ui_scale, FontType::Display);
    draw_text("ORBITAL HUB // EXPEDITION STAGING & UPGRADE TERMINAL", title_x, back_btn_y + (back_btn_h - title_h) * 0.5f, 1.85f * ui_scale, Typography::COLOR_CYAN, FontType::Display);
    draw_rect(30.0f * ui_scale, back_btn_y + back_btn_h + 8.0f * ui_scale, w - 60.0f * ui_scale, 2.0f, glm::vec4(0.15f, 0.85f, 1.0f, 0.6f));

    // Navigation Tabs Header
    float tab_y = back_btn_y + back_btn_h + 16.0f * ui_scale;
    float tab_h = std::clamp(38.0f * ui_scale, 30.0f, 44.0f);
    float tab_w = std::clamp(260.0f * ui_scale, 200.0f, 300.0f);
    float tab_gap = 14.0f * ui_scale;
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
        draw_panel_with_border(tx, tab_y, tab_w, tab_h, bg_col, border_col, is_cur ? 2.5f : 1.5f);

        draw_text_centered(tab_names[t], tx, tab_y, tab_w, tab_h, 1.30f * ui_scale,
                           is_cur ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : is_hov ? glm::vec4(0.85f, 0.95f, 1.0f, 0.9f) : glm::vec4(0.6f, 0.7f, 0.8f, 0.8f), FontType::Display);
    }

    bool launch_triggered = false;

    if (m_active_tab == HubTab::DelverRoster) {
        render_delver_roster(profile, mouse_x, mouse_y, mouse_clicked);
    } else if (m_active_tab == HubTab::UpgradeTerminal) {
        render_upgrade_terminal(profile, mouse_x, mouse_y, mouse_clicked, mouse_down, scroll_delta);
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

        static const char* s_names[] = {
            "PERIMETER DRIFT", "VOLATILE FAULT", "VOID CRADLE",
            "MAGMA UNDERCROFT", "CRYSTALLINE RIFT", "ECHO ABYSS",
            "TECTONIC MAW", "ASHEN SANCTUM", "OBSIDIAN DEEP",
            "NECROTIC VEIN", "WARP FISSURE", "RESONANCE VAULT"
        };
        int s_idx = (selected_level - 1) % 12;
        int s_cycle = (selected_level - 1) / 12;
        char s_buf[80];
        if (s_cycle == 0) std::snprintf(s_buf, sizeof(s_buf), "SECTOR %d: %s", selected_level, s_names[s_idx]);
        else              std::snprintf(s_buf, sizeof(s_buf), "SECTOR %d: %s [CYCLE %d]", selected_level, s_names[s_idx], s_cycle + 1);
        std::string sec_name(s_buf);
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

        for (int s = 1; s < UserProfile::MAX_SECTOR_RECORDS; ++s) {
            if (inventory.sector_records[s].highest_completion_rate > profile.sector_records[s].highest_completion_rate) {
                profile.sector_records[s].highest_completion_rate = inventory.sector_records[s].highest_completion_rate;
            }
            if (inventory.sector_records[s].best_badge != "UNEXPLORED") {
                profile.sector_records[s].best_badge = inventory.sector_records[s].best_badge;
            }
            if (inventory.sector_records[s].completed) {
                profile.sector_records[s].completed = true;
            }
            if (inventory.sector_records[s].saved_seed != 0 && profile.sector_records[s].saved_seed == 0) {
                profile.sector_records[s].saved_seed = inventory.sector_records[s].saved_seed;
            }
        }
        if ((success && !inventory.is_abandoned && !inventory.suit_failed) || inventory.run_completion_rate >= 50) {
            profile.highest_cleared_sector = std::max(profile.highest_cleared_sector, level);
            if (level >= 1 && level < UserProfile::MAX_SECTOR_RECORDS) {
                profile.sector_records[level].completed = true;
                if (profile.sector_records[level].saved_seed == 0 && inventory.active_seed != 0) {
                    profile.sector_records[level].saved_seed = inventory.active_seed;
                }
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

    float total_content_w = std::clamp(w * 0.95f, 700.0f, w - 32.0f);
    float col_gap = std::clamp(24.0f * ui_scale, 16.0f, 32.0f);
    float col_w = (total_content_w - col_gap) * 0.5f;
    float start_x = (w - total_content_w) * 0.5f;
    float col1_x = start_x;
    float col2_x = start_x + col_w + col_gap;

    float bot_btn_h = std::clamp(60.0f * ui_scale, 52.0f, 68.0f);
    float bot_margin = 16.0f * ui_scale;
    float bot_y = h - bot_btn_h - bot_margin;

    // Header banner
    float banner_y = 14.0f * ui_scale;
    float banner_h = 44.0f * ui_scale;
    std::string status_text = inventory.is_abandoned ? "EXPEDITION ABANDONED // SALVAGE PENALTY (50%)" :
                              inventory.suit_failed ? "[ ! ] EXPEDITION FAILED: DELVER M.I.A. [ ! ]" :
                              success ? "EXPEDITION EXTRACTION SUCCESSFUL" : "EXPEDITION FAILED";
    glm::vec4 status_col = (success && !inventory.is_abandoned && !inventory.suit_failed) ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) :
                           inventory.is_abandoned ? glm::vec4(1.0f, 0.70f, 0.0f, 1.0f) :
                           glm::vec4(1.0f, 0.22f, 0.18f, 1.0f);

    draw_text_centered_fitted(status_text, 16.0f, banner_y, w - 32.0f, banner_h, 2.50f * ui_scale, status_col, 1.40f * ui_scale, FontType::Display);
    float div_w = std::clamp(w * 0.95f, 600.0f, total_content_w);
    draw_rect((w - div_w) * 0.5f, banner_y + banner_h + 3.0f * ui_scale, div_w, 2.5f, status_col * 0.8f);

    // Sub-header: Sector telemetry & Completion rating
    float sub_y = banner_y + banner_h + 8.0f * ui_scale;
    float sub_h = 24.0f * ui_scale;
    std::string sub_text = "SECTOR " + std::to_string(level) + " SUBTERRANEAN EXTRACTION LOG   //   [" +
                           inventory.run_outcome_badge + "] -- " + std::to_string(inventory.run_completion_rate) + "% COMPLETION RATING";
    draw_text_centered_fitted(sub_text, (w - div_w) * 0.5f, sub_y, div_w, sub_h, 1.45f * ui_scale, glm::vec4(0.75f, 0.88f, 0.98f, 0.95f), 1.00f * ui_scale, FontType::Tech);

    float panel_y = sub_y + sub_h + 10.0f * ui_scale;
    float panel_h = bot_y - panel_y - 12.0f * ui_scale;

    // LEFT COLUMN: SUBTERRANEAN EXPEDITION HARVEST & MANIFEST
    draw_panel_with_border(col1_x, panel_y, col_w, panel_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.94f), glm::vec4(0.20f, 0.40f, 0.55f, 0.70f), 1.5f);
    draw_rect(col1_x, panel_y, 5.0f, panel_h, status_col);

    float l_pad = std::clamp(22.0f * ui_scale, 16.0f, 28.0f);
    float content_inner_w = col_w - 2.0f * l_pad;
    float cur_y = panel_y + 16.0f * ui_scale;

    draw_text_fitted("EXPEDITION HARVEST & MANIFEST", col1_x + l_pad, cur_y, content_inner_w, 1.85f * ui_scale, glm::vec4(0.0f, 0.92f, 1.0f, 1.0f), 1.20f * ui_scale, FontType::Display);
    cur_y += 32.0f * ui_scale;
    draw_rect(col1_x + l_pad, cur_y, content_inner_w, 1.5f, glm::vec4(0.2f, 0.45f, 0.6f, 0.6f));
    cur_y += 18.0f * ui_scale;

    auto draw_stat_row = [&](float px, float pw, float pad, float y, const std::string& label, const std::string& val, const glm::vec4& lcol, const glm::vec4& vcol, float scale) {
        float inner_w = pw - 2.0f * pad;
        float row_scale = FontRenderer::fit_scale(label + "   " + val, inner_w, scale, 1.05f * ui_scale, FontType::Tech);
        float val_w = FontRenderer::get_rendered_width(val, row_scale, FontType::Tech);
        draw_text(label, px + pad, y, row_scale, lcol, FontType::Body);
        draw_text(val, px + pw - pad - val_w, y, row_scale, vcol, FontType::Tech);
    };

    float stat_step = std::clamp(54.0f * ui_scale, 44.0f, 62.0f);

    draw_stat_row(col1_x, col_w, l_pad, cur_y, "<voidite> Voidite Crystals Extracted:",
                  std::to_string(inventory.voidite) + " units (" + std::to_string(inventory.voidite * 5) + " pts)",
                  glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), Typography::COLOR_CYAN_GLOW, 1.60f * ui_scale);
    cur_y += stat_step;
    draw_rect(col1_x + l_pad, cur_y - 10.0f * ui_scale, content_inner_w, 1.0f, glm::vec4(0.12f, 0.20f, 0.28f, 0.5f));

    draw_stat_row(col1_x, col_w, l_pad, cur_y, "<titanium> Titanium Cores Extracted:",
                  std::to_string(inventory.titanium) + " cores (" + std::to_string(inventory.titanium * 6) + " pts)",
                  glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), Typography::COLOR_AMBER, 1.60f * ui_scale);
    cur_y += stat_step;
    draw_rect(col1_x + l_pad, cur_y - 10.0f * ui_scale, content_inner_w, 1.0f, glm::vec4(0.12f, 0.20f, 0.28f, 0.5f));

    draw_stat_row(col1_x, col_w, l_pad, cur_y, "<scrap> Scrap Metal Salvaged:",
                  std::to_string(inventory.scrap_metal) + " scrap",
                  glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), Typography::COLOR_SCRAP, 1.60f * ui_scale);
    cur_y += stat_step;
    draw_rect(col1_x + l_pad, cur_y - 10.0f * ui_scale, content_inner_w, 1.0f, glm::vec4(0.12f, 0.20f, 0.28f, 0.5f));

    draw_stat_row(col1_x, col_w, l_pad, cur_y, "<salvage> Mineral Salvage Parts:",
                  std::to_string(inventory.salvage_parts) + " units (" + std::to_string(inventory.salvage_parts * 2) + " pts)",
                  glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), Typography::COLOR_SALVAGE, 1.60f * ui_scale);
    cur_y += stat_step;
    draw_rect(col1_x + l_pad, cur_y - 10.0f * ui_scale, content_inner_w, 1.0f, glm::vec4(0.12f, 0.20f, 0.28f, 0.5f));

    std::string astral_val = (inventory.astral_ore > 0) ? (std::to_string(inventory.astral_ore) + " ore (" + std::to_string(inventory.astral_ore * 10) + " pts)") : "0 ore (0 pts)";
    glm::vec4 astral_col = (inventory.astral_ore > 0) ? Typography::COLOR_VOIDITE : glm::vec4(0.55f, 0.60f, 0.65f, 0.7f);
    draw_stat_row(col1_x, col_w, l_pad, cur_y, "<exp> Astral Ore Extracted:",
                  astral_val, glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), astral_col, 1.60f * ui_scale);
    cur_y += stat_step;
    draw_rect(col1_x + l_pad, cur_y - 10.0f * ui_scale, content_inner_w, 1.0f, glm::vec4(0.12f, 0.20f, 0.28f, 0.5f));

    std::string r_val = inventory.relic_extracted ? "RECOVERED (+250 pts)" : "NOT RECOVERED";
    glm::vec4 r_col = inventory.relic_extracted ? glm::vec4(1.0f, 0.0f, 0.85f, 1.0f) : glm::vec4(0.55f, 0.60f, 0.65f, 0.7f);
    draw_stat_row(col1_x, col_w, l_pad, cur_y, "Hyper-Core Vault Relic:",
                  r_val, glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), r_col, 1.60f * ui_scale);
    cur_y += stat_step;
    draw_rect(col1_x + l_pad, cur_y - 10.0f * ui_scale, content_inner_w, 1.0f, glm::vec4(0.12f, 0.20f, 0.28f, 0.5f));

    std::string mine_val = std::to_string(inventory.granite_mined_count) + " blocks excavated";
    draw_stat_row(col1_x, col_w, l_pad, cur_y, "<drill> Subterranean Rock Cleared:",
                  mine_val, glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), glm::vec4(0.80f, 0.85f, 0.90f, 1.0f), 1.60f * ui_scale);
    cur_y += stat_step;
    draw_rect(col1_x + l_pad, cur_y - 10.0f * ui_scale, content_inner_w, 1.0f, glm::vec4(0.12f, 0.20f, 0.28f, 0.5f));

    std::string apex_val = std::to_string(inventory.alphas_eliminated) + " Hostiles slain";
    glm::vec4 apex_col = (inventory.alphas_eliminated > 0) ? Typography::COLOR_CRIMSON : glm::vec4(0.55f, 0.60f, 0.65f, 0.7f);
    draw_stat_row(col1_x, col_w, l_pad, cur_y, "<skull> Apex Entities Neutralized:",
                  apex_val, glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), apex_col, 1.60f * ui_scale);

    // Left bottom summary callout box
    float bot_card_h = 86.0f * ui_scale;
    float bot_card_y = panel_y + panel_h - bot_card_h - 16.0f * ui_scale;
    draw_panel_with_border(col1_x + l_pad, bot_card_y, content_inner_w, bot_card_h, glm::vec4(0.06f, 0.09f, 0.14f, 0.92f), glm::vec4(0.18f, 0.50f, 0.70f, 0.80f), 1.5f);
    int total_materials = inventory.voidite + inventory.titanium + inventory.scrap_metal + inventory.salvage_parts + inventory.astral_ore;
    draw_text_centered_fitted("TOTAL EXTRACTION YIELD", col1_x + l_pad, bot_card_y + 10.0f * ui_scale, content_inner_w, 22.0f * ui_scale, 1.35f * ui_scale, Typography::COLOR_CYAN, 0.95f * ui_scale, FontType::Tech);
    std::string mat_val = std::to_string(total_materials) + " RAW UNITS EXTRACTED";
    draw_text_centered_fitted(mat_val, col1_x + l_pad, bot_card_y + 36.0f * ui_scale, content_inner_w, 38.0f * ui_scale, 1.85f * ui_scale, Typography::COLOR_PRIMARY, 1.20f * ui_scale, FontType::Display);

    // RIGHT COLUMN: EXPEDITION REWARDS & CAREER PROGRESSION
    draw_panel_with_border(col2_x, panel_y, col_w, panel_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.94f), glm::vec4(0.50f, 0.40f, 0.20f, 0.70f), 1.5f);
    draw_rect(col2_x, panel_y, 5.0f, panel_h, glm::vec4(1.0f, 0.75f, 0.15f, 1.0f));

    float r_pad = std::clamp(22.0f * ui_scale, 16.0f, 28.0f);
    float r_inner_w = col_w - 2.0f * r_pad;
    float r_y = panel_y + 16.0f * ui_scale;

    draw_text_fitted("EXPEDITION REWARDS & PROGRESSION", col2_x + r_pad, r_y, r_inner_w, 1.85f * ui_scale, glm::vec4(1.0f, 0.85f, 0.25f, 1.0f), 1.20f * ui_scale, FontType::Display);
    r_y += 32.0f * ui_scale;
    draw_rect(col2_x + r_pad, r_y, r_inner_w, 1.5f, glm::vec4(0.6f, 0.45f, 0.2f, 0.6f));
    r_y += 18.0f * ui_scale;

    // Prominent Score Highlight Box
    float score_card_h = 112.0f * ui_scale;
    draw_panel_with_border(col2_x + r_pad, r_y, r_inner_w, score_card_h, glm::vec4(0.08f, 0.09f, 0.14f, 0.95f), glm::vec4(1.0f, 0.80f, 0.20f, 0.90f), 1.5f);
    draw_text_centered_fitted("FINAL EXPEDITION SCORE", col2_x + r_pad, r_y + 10.0f * ui_scale, r_inner_w, 24.0f * ui_scale, 1.40f * ui_scale, glm::vec4(0.88f, 0.92f, 0.98f, 0.95f), 1.00f * ui_scale, FontType::Body);
    std::string score_str = std::to_string(inventory.total_run_score) + " POINTS";
    draw_text_centered_fitted(score_str, col2_x + r_pad, r_y + 36.0f * ui_scale, r_inner_w, 46.0f * ui_scale, 2.50f * ui_scale, Typography::COLOR_EXP, 1.50f * ui_scale, FontType::Display);
    std::string rating_str = "RATING: [" + inventory.run_outcome_badge + "]  --  " + std::to_string(inventory.run_completion_rate) + "% COMPLETION";
    draw_text_centered_fitted(rating_str, col2_x + r_pad, r_y + 82.0f * ui_scale, r_inner_w, 22.0f * ui_scale, 1.35f * ui_scale, status_col, 0.95f * ui_scale, FontType::Tech);
    r_y += score_card_h + 20.0f * ui_scale;

    // Currency Rewards: Coins & Player EXP
    int final_run_coins = inventory.run_coins_earned;
    if (final_run_coins <= 0) {
        final_run_coins = (success ? 60 : 20) + inventory.run_completion_rate + (inventory.voidite * 2 + inventory.titanium * 3);
        if (inventory.is_abandoned) final_run_coins /= 2;
    }
    int final_run_exp = inventory.total_run_score > 0 ? inventory.total_run_score :
                        (inventory.voidite * 5 + inventory.titanium * 6 + inventory.salvage_parts * 2);

    float rew_step = std::clamp(54.0f * ui_scale, 44.0f, 62.0f);

    draw_stat_row(col2_x, col_w, r_pad, r_y, "ROUND COINS EARNED:",
                  "+" + std::to_string(final_run_coins) + " COINS",
                  glm::vec4(1.0f, 0.90f, 0.40f, 1.0f), Typography::COLOR_EXP, 1.65f * ui_scale);
    r_y += rew_step;
    draw_rect(col2_x + r_pad, r_y - 10.0f * ui_scale, r_inner_w, 1.0f, glm::vec4(0.28f, 0.22f, 0.12f, 0.5f));

    draw_stat_row(col2_x, col_w, r_pad, r_y, "<exp> EXP BANKED THIS RUN:",
                  "+" + std::to_string(final_run_exp) + " EXP",
                  glm::vec4(0.35f, 0.95f, 0.55f, 1.0f), Typography::COLOR_GREEN, 1.65f * ui_scale);
    r_y += rew_step;
    draw_rect(col2_x + r_pad, r_y - 10.0f * ui_scale, r_inner_w, 1.0f, glm::vec4(0.12f, 0.28f, 0.18f, 0.5f));

    int debrief_player_lvl = profile.get_player_level();
    draw_stat_row(col2_x, col_w, r_pad, r_y, "CURRENT DELVER RANK:",
                  "DELVER RANK " + std::to_string(debrief_player_lvl),
                  glm::vec4(0.70f, 0.88f, 1.0f, 1.0f), Typography::COLOR_CYAN, 1.60f * ui_scale);
    r_y += rew_step;
    draw_rect(col2_x + r_pad, r_y - 10.0f * ui_scale, r_inner_w, 1.0f, glm::vec4(0.12f, 0.20f, 0.28f, 0.5f));

    if (!m_terminal_msg.empty() && m_terminal_msg.find("LEVEL UP") != std::string::npos) {
        draw_text_centered_fitted(m_terminal_msg, col2_x + r_pad, r_y, r_inner_w, 26.0f * ui_scale, 1.40f * ui_scale, m_terminal_msg_col, 1.00f * ui_scale, FontType::Display);
        r_y += 32.0f * ui_scale;
    }

    r_y += 8.0f * ui_scale;
    draw_text_fitted("DELVER FIELD SPECIALIZATIONS:", col2_x + r_pad, r_y, r_inner_w, 1.55f * ui_scale, glm::vec4(0.20f, 0.90f, 1.0f, 1.0f), 1.10f * ui_scale, FontType::Display);
    r_y += 30.0f * ui_scale;

    std::string demo_val = std::to_string(skills.demolitions.xp) + " XP" + (skills.demolitions.unlocked ? "  [ACTIVE SPEC]" : "");
    glm::vec4 demo_col = skills.demolitions.unlocked ? Typography::COLOR_GREEN : glm::vec4(0.70f, 0.75f, 0.80f, 0.85f);
    draw_stat_row(col2_x, col_w, r_pad, r_y, "Demolitions Specialization:",
                  demo_val, glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), demo_col, 1.45f * ui_scale);
    r_y += rew_step - 8.0f * ui_scale;
    draw_rect(col2_x + r_pad, r_y - 10.0f * ui_scale, r_inner_w, 1.0f, glm::vec4(0.12f, 0.20f, 0.28f, 0.5f));

    std::string surv_val = std::to_string(skills.surveying.xp) + " XP (Rank " + std::to_string(skills.get_surveying_rank()) + ")" + (skills.surveying.unlocked ? "  [ACTIVE SPEC]" : "");
    glm::vec4 surv_col = skills.surveying.unlocked ? Typography::COLOR_GREEN : glm::vec4(0.70f, 0.75f, 0.80f, 0.85f);
    draw_stat_row(col2_x, col_w, r_pad, r_y, "Surveying & Sonar Spec:",
                  surv_val, glm::vec4(0.88f, 0.93f, 0.98f, 1.0f), surv_col, 1.45f * ui_scale);

    // Right bottom persistent vault reserves callout box
    draw_panel_with_border(col2_x + r_pad, bot_card_y, r_inner_w, bot_card_h, glm::vec4(0.08f, 0.08f, 0.12f, 0.92f), glm::vec4(0.50f, 0.45f, 0.20f, 0.80f), 1.5f);
    draw_text_centered_fitted("ORBITAL VAULT PERSISTENT RESERVES", col2_x + r_pad, bot_card_y + 10.0f * ui_scale, r_inner_w, 22.0f * ui_scale, 1.35f * ui_scale, Typography::COLOR_AMBER, 0.95f * ui_scale, FontType::Tech);
    std::string vault_reserves = std::to_string(profile.total_coins) + " TOTAL COINS   |   " + std::to_string(profile.total_exp) + " TOTAL EXP   |   " + std::to_string(profile.total_voidite) + " VOIDITE";
    draw_text_centered_fitted(vault_reserves, col2_x + r_pad, bot_card_y + 36.0f * ui_scale, r_inner_w, 38.0f * ui_scale, 1.55f * ui_scale, Typography::COLOR_PRIMARY, 1.10f * ui_scale, FontType::Tech);

    // BOTTOM BUTTONS: [CONTINUE TO NEXT ROUND] / [RE-DEPLOY] & [RETURN TO ORBITAL HUB]
    DebriefAction result = DebriefAction::None;
    float bot_btn_w = (total_content_w - col_gap) * 0.5f;

    // Button 1: [CONTINUE TO NEXT ROUND] or [RE-DEPLOY EXPEDITION]
    float b1_x = start_x;
    bool b1_hov = (mouse_x >= b1_x && mouse_x <= b1_x + bot_btn_w && mouse_y >= bot_y && mouse_y <= bot_y + bot_btn_h);
    if (b1_hov && mouse_clicked) {
        if (inventory.suit_failed || !success) {
            result = DebriefAction::RedeployExpedition;
        } else {
            result = DebriefAction::LaunchNextSector;
        }
    }
    std::string b1_label = (inventory.suit_failed || !success) ? "[RE-DEPLOY EXPEDITION]" : "[CONTINUE TO NEXT ROUND]";
    glm::vec4 b1_col = (inventory.suit_failed || !success)
        ? (b1_hov ? glm::vec4(0.85f, 0.25f, 0.15f, 1.0f) : glm::vec4(0.60f, 0.15f, 0.10f, 0.95f))
        : (b1_hov ? glm::vec4(0.18f, 0.65f, 0.40f, 1.0f) : glm::vec4(0.10f, 0.48f, 0.30f, 0.95f));
    glm::vec4 b1_border = (inventory.suit_failed || !success)
        ? (b1_hov ? glm::vec4(1.0f, 0.5f, 0.3f, 1.0f) : glm::vec4(0.8f, 0.2f, 0.15f, 0.8f))
        : (b1_hov ? glm::vec4(0.4f, 1.0f, 0.7f, 1.0f) : glm::vec4(0.2f, 0.75f, 0.45f, 0.8f));
    draw_panel_with_border(b1_x, bot_y, bot_btn_w, bot_btn_h, b1_col, b1_border, b1_hov ? 2.5f : 1.5f);
    draw_text_centered_fitted(b1_label, b1_x, bot_y, bot_btn_w, bot_btn_h, 1.65f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 1.10f * ui_scale, FontType::Display);

    // Button 2: [RETURN TO ORBITAL HUB]
    float b2_x = start_x + bot_btn_w + col_gap;
    bool b2_hov = (mouse_x >= b2_x && mouse_x <= b2_x + bot_btn_w && mouse_y >= bot_y && mouse_y <= bot_y + bot_btn_h);
    if (b2_hov && mouse_clicked) {
        result = DebriefAction::ReturnToHub;
    }
    glm::vec4 b2_col = b2_hov ? glm::vec4(0.22f, 0.48f, 0.78f, 1.0f) : glm::vec4(0.12f, 0.32f, 0.58f, 0.95f);
    glm::vec4 b2_border = b2_hov ? glm::vec4(0.60f, 0.88f, 1.0f, 1.0f) : glm::vec4(0.30f, 0.55f, 0.75f, 0.8f);
    draw_panel_with_border(b2_x, bot_y, bot_btn_w, bot_btn_h, b2_col, b2_border, b2_hov ? 2.5f : 1.5f);
    draw_text_centered_fitted("[RETURN TO ORBITAL HUB]", b2_x, bot_y, bot_btn_w, bot_btn_h, 1.65f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 1.10f * ui_scale, FontType::Display);

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

        // Graphical Volume Level Bar
        float meter_w = 120.0f * ui_scale;
        float meter_h = 12.0f * ui_scale;
        float meter_x = btn1_x - meter_w - (is_master ? (adj_btn_w * 1.6f + 16.0f * ui_scale) : 14.0f * ui_scale);
        float meter_y = row_y + (adj_btn_h - meter_h) * 0.5f;

        float label_x = panel_x + pad + 14.0f * ui_scale;
        float label_max_w = meter_x - label_x - 10.0f * ui_scale;
        glm::vec4 text_col = (is_master && s.mute_all) ? glm::vec4(1.0f, 0.4f, 0.4f, 1.0f) : glm::vec4(0.85f, 0.92f, 0.98f, 0.95f);
        draw_text_fitted(val_buf, label_x, row_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f,
                         label_max_w, 1.15f * ui_scale, text_col);

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
            draw_text_centered_fitted(s.mute_all ? "UNMUTE" : "MUTE", mute_x, row_y, mute_w, adj_btn_h, 1.10f * ui_scale,
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

    float ctrl_label_x = panel_x + pad + 14.0f * ui_scale;
    float ctrl_label_max_w = btn1_x - ctrl_label_x - 10.0f * ui_scale;

    // 6. Sensitivity
    char sens_buf[32];
    std::snprintf(sens_buf, sizeof(sens_buf), "MOUSE SENSITIVITY: %.2f", s.mouse_sensitivity);
    draw_text_fitted(sens_buf, ctrl_label_x, cur_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f,
                     ctrl_label_max_w, 1.15f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
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
    draw_text_fitted(fov_buf, ctrl_label_x, cur_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f,
                     ctrl_label_max_w, 1.15f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
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
    draw_text_fitted(bright_buf, ctrl_label_x, cur_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f,
                     ctrl_label_max_w, 1.15f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
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
    draw_text_fitted(shake_buf, ctrl_label_x, cur_y + (adj_btn_h - FontRenderer::get_rendered_height(1.15f * ui_scale)) * 0.5f,
                     ctrl_label_max_w, 1.15f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
    bool shk_hover1 = (mouse_x >= btn1_x && mouse_x <= btn1_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    bool shk_hover2 = (mouse_x >= btn2_x && mouse_x <= btn2_x + adj_btn_w && mouse_y >= cur_y && mouse_y <= cur_y + adj_btn_h);
    draw_panel_with_border(btn1_x, cur_y, adj_btn_w, adj_btn_h, shk_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_panel_with_border(btn2_x, cur_y, adj_btn_w, adj_btn_h, shk_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
    draw_text_centered("-", btn1_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    draw_text_centered("+", btn2_x, cur_y, adj_btn_w, adj_btn_h, 1.25f * ui_scale, glm::vec4(1.0f));
    if (mouse_clicked && shk_hover1) { s.screen_shake = std::max(0.0f, s.screen_shake - 0.10f); m_profile_dirty = true; }
    if (mouse_clicked && shk_hover2) { s.screen_shake = std::min(1.0f, s.screen_shake + 0.10f); m_profile_dirty = true; }
    cur_y += row_h + 8.0f * ui_scale;

    // Bottom actions: Audio Preview and Reset Save Data
    float gap_bot = 12.0f * ui_scale;
    float tst_w = (content_w - 28.0f * ui_scale - gap_bot) * 0.5f;
    float tst_h = std::clamp(32.0f * ui_scale, 26.0f, 38.0f);
    float tst_x = panel_x + pad + 14.0f * ui_scale;
    bool tst_hov = (mouse_x >= tst_x && mouse_x <= tst_x + tst_w && mouse_y >= cur_y && mouse_y <= cur_y + tst_h);
    draw_panel_with_border(tst_x, cur_y, tst_w, tst_h,
                           tst_hov ? glm::vec4(0.08f, 0.35f, 0.30f, 0.95f) : glm::vec4(0.04f, 0.18f, 0.16f, 0.85f),
                           tst_hov ? glm::vec4(0.2f, 1.0f, 0.6f, 1.0f) : glm::vec4(0.1f, 0.6f, 0.4f, 0.6f));
    draw_text_centered("[* PREVIEW AUDIO *]", tst_x, cur_y, tst_w, tst_h, 1.15f * ui_scale,
                       tst_hov ? glm::vec4(0.4f, 1.0f, 0.7f, 1.0f) : glm::vec4(0.3f, 0.85f, 0.55f, 0.9f));
    if (mouse_clicked && tst_hov) {
        m_test_sound_requested = true;
    }

    float rst_x = tst_x + tst_w + gap_bot;
    bool rst_hov = (mouse_x >= rst_x && mouse_x <= rst_x + tst_w && mouse_y >= cur_y && mouse_y <= cur_y + tst_h);

    std::string rst_text = "[ RESET SAVE FILE ]";
    if (m_save_cleared_toast_timer > 0.0f) {
        rst_text = "[ SAVE RESET DONE ]";
    } else if (m_confirming_clear_save) {
        rst_text = "[ CONFIRM RESET? ]";
    }

    glm::vec4 rst_bg = (m_confirming_clear_save) ? glm::vec4(0.35f, 0.08f, 0.08f, 0.95f) :
                       (m_save_cleared_toast_timer > 0.0f) ? glm::vec4(0.08f, 0.28f, 0.14f, 0.95f) :
                       rst_hov ? glm::vec4(0.25f, 0.10f, 0.10f, 0.90f) : glm::vec4(0.12f, 0.06f, 0.06f, 0.85f);
    glm::vec4 rst_border = (m_confirming_clear_save) ? glm::vec4(1.0f, 0.4f, 0.3f, 1.0f) :
                           (m_save_cleared_toast_timer > 0.0f) ? glm::vec4(0.2f, 0.95f, 0.4f, 1.0f) :
                           rst_hov ? glm::vec4(0.9f, 0.4f, 0.4f, 0.9f) : glm::vec4(0.5f, 0.25f, 0.25f, 0.6f);
    glm::vec4 rst_tc = (m_confirming_clear_save) ? glm::vec4(1.0f, 0.5f, 0.4f, 1.0f) :
                       (m_save_cleared_toast_timer > 0.0f) ? glm::vec4(0.3f, 1.0f, 0.5f, 1.0f) :
                       rst_hov ? glm::vec4(1.0f, 0.7f, 0.7f, 1.0f) : glm::vec4(0.85f, 0.65f, 0.65f, 0.9f);

    draw_panel_with_border(rst_x, cur_y, tst_w, tst_h, rst_bg, rst_border);
    draw_text_centered(rst_text, rst_x, cur_y, tst_w, tst_h, 1.15f * ui_scale, rst_tc);

    if (mouse_clicked && rst_hov) {
        if (!m_confirming_clear_save) {
            m_confirming_clear_save = true;
            m_confirm_clear_timer = 4.0f;
        } else {
            SaveSystem::clear_save();
            SaveSystem::reset_profile(profile);
            trigger_save_cleared_feedback();
            m_profile_dirty = true;
        }
    }
}

} // namespace Voidfall

