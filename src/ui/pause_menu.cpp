#include "pause_menu.hpp"
#include "font_renderer.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <font8x8.h>
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

void PauseMenu::resize(int width, int height) {
    if (width > 0 && height > 0) {
        m_width = width;
        m_height = height;
    }
}

void PauseMenu::draw_rect(float x, float y, float w, float h, const glm::vec4& color) {
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

void PauseMenu::draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color) {
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
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));
    }

    glBindVertexArray(0);
}

void PauseMenu::draw_text_fitted(const std::string& text, float x, float y, float max_w, float base_scale, const glm::vec4& color) {
    if (text.empty() || max_w <= 0.0f) return;
    float scale = FontRenderer::fit_scale(text, max_w, base_scale, base_scale * 0.65f);
    draw_text(text, x, y, scale, color);
}

void PauseMenu::draw_text_centered(const std::string& text, float box_x, float box_y, float box_w, float box_h, float scale, const glm::vec4& color) {
    if (text.empty()) return;
    float text_w = FontRenderer::get_rendered_width(text, scale);
    float text_h = FontRenderer::get_rendered_height(scale);
    float x = box_x + (box_w - text_w) * 0.5f;
    float y = box_y + (box_h - text_h) * 0.5f;
    draw_text(text, x, y, scale, color);
}

void PauseMenu::draw_text_centered_fitted(const std::string& text, float box_x, float box_y, float box_w, float box_h, float base_scale, const glm::vec4& color) {
    if (text.empty() || box_w <= 0.0f) return;
    float max_w = box_w - 12.0f;
    float scale = FontRenderer::fit_scale(text, max_w, base_scale, base_scale * 0.65f);
    float text_w = FontRenderer::get_rendered_width(text, scale);
    float text_h = FontRenderer::get_rendered_height(scale);
    float x = box_x + (box_w - text_w) * 0.5f;
    float y = box_y + (box_h - text_h) * 0.5f;
    draw_text(text, x, y, scale, color);
}

void PauseMenu::draw_panel_with_border(float x, float y, float w, float h, const glm::vec4& bg_col, const glm::vec4& border_col, float border_thick) {
    draw_rect(x, y, w, h, bg_col);
    draw_rect(x, y, w, border_thick, border_col);
    draw_rect(x, y + h - border_thick, w, border_thick, border_col);
    draw_rect(x, y, border_thick, h, border_col);
    draw_rect(x + w - border_thick, y, border_thick, h, border_col);
}

PauseMenuAction PauseMenu::render(
    int sector,
    float time_elapsed,
    PlayerInventory& inventory,
    GameSettings& settings,
    int class_id,
    float mouse_x,
    float mouse_y,
    bool mouse_clicked
) {
    PauseMenuAction action = PauseMenuAction::None;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    // 1. High-contrast dark tinted modal backdrop over frozen game frame: rgba(6, 10, 16, 0.94)
    draw_rect(0.0f, 0.0f, w, h, glm::vec4(6.0f / 255.0f, 10.0f / 255.0f, 16.0f / 255.0f, 0.94f));

    // 2. Central Modal Dialog Window - responsive sizing across resolutions
    float panel_w = std::clamp(w * 0.72f, 620.0f, std::min(w - 32.0f, 980.0f * ui_scale));
    float panel_h = std::clamp(h * 0.88f, 480.0f, std::min(h - 24.0f, 760.0f * ui_scale));
    float panel_x = (w - panel_w) * 0.5f;
    float panel_y = (h - panel_h) * 0.5f;

    draw_panel_with_border(panel_x, panel_y, panel_w, panel_h, glm::vec4(0.03f, 0.05f, 0.08f, 0.98f), glm::vec4(0.0f, 0.85f, 1.0f, 0.45f), 2.0f);
    draw_rect(panel_x, panel_y, panel_w, 3.0f, glm::vec4(0.0f, 0.94f, 1.0f, 0.9f));

    float pad = 20.0f * ui_scale;
    float content_w = panel_w - 2.0f * pad;

    // Header Title
    std::string sec_name = (sector == 1) ? "CRYSTALLINE CAVERNS" :
                           (sector == 2) ? "SUBTERRANEAN VAULT" :
                                           "FAULT-LINE COLLAPSE";
    std::string header = "// EXPEDITION PAUSED: " + sec_name;
    float cur_y = panel_y + 16.0f * ui_scale;
    draw_text_fitted(header, panel_x + pad, cur_y, content_w, 1.40f * ui_scale, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    cur_y += 24.0f * ui_scale;
    draw_text_fitted("TACTICAL DELVER TELEMETRY // OPERATIONS SUSPENDED", panel_x + pad, cur_y, content_w, 0.95f * ui_scale, glm::vec4(0.6f, 0.75f, 0.85f, 0.85f));
    cur_y += 18.0f * ui_scale;
    draw_rect(panel_x + pad, cur_y, content_w, 1.0f, glm::vec4(0.2f, 0.4f, 0.55f, 0.6f));
    cur_y += 10.0f * ui_scale;

    // Sub-Navigation Tabs: [ 1. TELEMETRY ] | [ 2. INVENTORY ] | [ 3. AUDIO & RIG ] | [ 4. CONTROLS ]
    float tab_h = std::clamp(30.0f * ui_scale, 26.0f, 36.0f);
    float tab_gap = 6.0f * ui_scale;
    float tab_w = (content_w - 3.0f * tab_gap) / 4.0f;
    float tab1_x = panel_x + pad;
    float tab2_x = tab1_x + tab_w + tab_gap;
    float tab3_x = tab2_x + tab_w + tab_gap;
    float tab4_x = tab3_x + tab_w + tab_gap;

    bool t1_hover = (mouse_x >= tab1_x && mouse_x <= tab1_x + tab_w && mouse_y >= cur_y && mouse_y <= cur_y + tab_h);
    bool t2_hover = (mouse_x >= tab2_x && mouse_x <= tab2_x + tab_w && mouse_y >= cur_y && mouse_y <= cur_y + tab_h);
    bool t3_hover = (mouse_x >= tab3_x && mouse_x <= tab3_x + tab_w && mouse_y >= cur_y && mouse_y <= cur_y + tab_h);
    bool t4_hover = (mouse_x >= tab4_x && mouse_x <= tab4_x + tab_w && mouse_y >= cur_y && mouse_y <= cur_y + tab_h);

    if (mouse_clicked && t1_hover) m_active_tab = PauseTab::Mission;
    if (mouse_clicked && t2_hover) m_active_tab = PauseTab::Inventory;
    if (mouse_clicked && t3_hover) m_active_tab = PauseTab::AudioSettings;
    if (mouse_clicked && t4_hover) m_active_tab = PauseTab::ControlsBriefing;

    bool t1_active = (m_active_tab == PauseTab::Mission);
    bool t2_active = (m_active_tab == PauseTab::Inventory);
    bool t3_active = (m_active_tab == PauseTab::AudioSettings);
    bool t4_active = (m_active_tab == PauseTab::ControlsBriefing);

    draw_panel_with_border(tab1_x, cur_y, tab_w, tab_h,
                           t1_active ? glm::vec4(0.08f, 0.18f, 0.28f, 0.95f) : (t1_hover ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                           t1_active ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.2f, 0.4f, 0.5f, 0.5f),
                           t1_active ? 2.0f : 1.0f);
    draw_text_centered_fitted("[ 1. TELEMETRY ]", tab1_x, cur_y, tab_w, tab_h, 1.00f * ui_scale,
                              t1_active ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

    draw_panel_with_border(tab2_x, cur_y, tab_w, tab_h,
                           t2_active ? glm::vec4(0.08f, 0.18f, 0.28f, 0.95f) : (t2_hover ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                           t2_active ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.2f, 0.4f, 0.5f, 0.5f),
                           t2_active ? 2.0f : 1.0f);
    draw_text_centered_fitted("[ 2. INVENTORY ]", tab2_x, cur_y, tab_w, tab_h, 1.00f * ui_scale,
                              t2_active ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

    draw_panel_with_border(tab3_x, cur_y, tab_w, tab_h,
                           t3_active ? glm::vec4(0.08f, 0.18f, 0.28f, 0.95f) : (t3_hover ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                           t3_active ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.2f, 0.4f, 0.5f, 0.5f),
                           t3_active ? 2.0f : 1.0f);
    draw_text_centered_fitted("[ 3. AUDIO/RIG ]", tab3_x, cur_y, tab_w, tab_h, 1.00f * ui_scale,
                              t3_active ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

    draw_panel_with_border(tab4_x, cur_y, tab_w, tab_h,
                           t4_active ? glm::vec4(0.08f, 0.18f, 0.28f, 0.95f) : (t4_hover ? Typography::COLOR_BUTTON_HOV : Typography::COLOR_BUTTON_BG),
                           t4_active ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.2f, 0.4f, 0.5f, 0.5f),
                           t4_active ? 2.0f : 1.0f);
    draw_text_centered_fitted("[ 4. CONTROLS ]", tab4_x, cur_y, tab_w, tab_h, 1.00f * ui_scale,
                              t4_active ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);

    cur_y += tab_h + 10.0f * ui_scale;

    // Bottom action buttons anchored dynamically to modal bottom
    float btn_w = content_w;
    float btn_h = std::clamp(34.0f * ui_scale, 28.0f, 40.0f);
    float btn_gap = 6.0f * ui_scale;

    float hub_y = panel_y + panel_h - pad - btn_h;
    float abn_y = hub_y - btn_gap - btn_h;
    float res_y = abn_y - btn_gap - btn_h;

    // Content box height exactly fills the space between tabs and buttons!
    float content_box_h = res_y - 10.0f * ui_scale - cur_y;

    if (m_active_tab == PauseTab::Mission) {
        // ── 3A. Mission Telemetry View ──
        draw_panel_with_border(panel_x + pad, cur_y, content_w, content_box_h, glm::vec4(0.02f, 0.035f, 0.06f, 0.92f), glm::vec4(0.15f, 0.35f, 0.5f, 0.6f));

        float inner_pad = 16.0f * ui_scale;
        float rx = panel_x + pad + inner_pad;
        float row_w = content_w - 2.0f * inner_pad;
        float in_y = cur_y + 12.0f * ui_scale;

        draw_text("MISSION TELEMETRY & RESOURCES", rx, in_y, 1.15f * ui_scale, Typography::COLOR_AMBER);
        in_y += 20.0f * ui_scale;

        int mins = static_cast<int>(time_elapsed) / 60;
        int secs = static_cast<int>(time_elapsed) % 60;
        char time_buf[32];
        std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d", mins, secs);

        float row_step = std::clamp(22.0f * ui_scale, 18.0f, 26.0f);
        auto draw_telemetry_row = [&](const std::string& label, const std::string& val, const glm::vec4& l_col, const glm::vec4& v_col) {
            draw_text(label, rx, in_y, 1.00f * ui_scale, l_col);
            float v_w = FontRenderer::get_rendered_width(val, 1.00f * ui_scale);
            draw_text(val, rx + row_w - v_w, in_y, 1.00f * ui_scale, v_col);
            in_y += row_step;
        };

        draw_telemetry_row("Time In Cavern:", std::string(time_buf), Typography::COLOR_PRIMARY, Typography::COLOR_CYAN);
        draw_telemetry_row("Voidite Extracted:", std::to_string(inventory.voidite) + " / " + std::to_string(inventory.target_voidite) + " Crystals", Typography::COLOR_PRIMARY, Typography::COLOR_CYAN);
        draw_telemetry_row("Titanium Cores:", std::to_string(inventory.titanium) + " Cores", Typography::COLOR_PRIMARY, Typography::COLOR_AMBER);
        draw_telemetry_row("Scrap Metal Salvaged:", std::to_string(inventory.scrap_metal) + " Scrap", Typography::COLOR_PRIMARY, Typography::COLOR_PRIMARY);
        draw_telemetry_row("Mineral Salvage:", std::to_string(inventory.salvage_parts) + " Units", Typography::COLOR_PRIMARY, Typography::COLOR_PRIMARY);

        // Completion progress bar
        int completion = inventory.run_completion_rate;
        std::string comp_label = "Expedition Progress: " + std::to_string(completion) + "%";
        draw_text(comp_label, rx, in_y, 1.00f * ui_scale, Typography::COLOR_GREEN);
        in_y += 16.0f * ui_scale;

        float pbar_w = row_w;
        float pbar_h = 6.0f * ui_scale;
        draw_rect(rx, in_y, pbar_w, pbar_h, glm::vec4(0.08f, 0.14f, 0.20f, 0.9f));
        float p_fill = pbar_w * std::clamp(completion / 100.0f, 0.0f, 1.0f);
        draw_rect(rx, in_y, p_fill, pbar_h, Typography::COLOR_GREEN);
        in_y += pbar_h + 16.0f * ui_scale;

        // Separator
        draw_rect(rx, in_y, row_w, 1.0f, glm::vec4(0.15f, 0.35f, 0.5f, 0.4f));
        in_y += 12.0f * ui_scale;

        // Audio shortcut prompt
        draw_text("QUICK SETTINGS OVERVIEW", rx, in_y, 1.10f * ui_scale, Typography::COLOR_CYAN);
        in_y += 18.0f * ui_scale;

        std::string vol_summary = "Master Audio: " + std::to_string(static_cast<int>(std::round(settings.master_volume * 100.0f))) + "%" + (settings.mute_all ? " [MUTED]" : "");
        std::string fov_summary = "FOV: " + std::to_string(static_cast<int>(settings.fov)) + " Deg  |  Sensitivity: " + std::to_string(static_cast<int>(settings.mouse_sensitivity * 100.0f)) + "%";
        draw_text(vol_summary, rx, in_y, 0.95f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
        in_y += 16.0f * ui_scale;
        draw_text(fov_summary, rx, in_y, 0.95f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
        in_y += 20.0f * ui_scale;

        float mix_btn_w = row_w;
        float mix_btn_h = std::clamp(26.0f * ui_scale, 22.0f, 30.0f);
        bool mix_hov = (mouse_x >= rx && mouse_x <= rx + mix_btn_w && mouse_y >= in_y && mouse_y <= in_y + mix_btn_h);
        draw_panel_with_border(rx, in_y, mix_btn_w, mix_btn_h,
                               mix_hov ? glm::vec4(0.12f, 0.26f, 0.38f, 0.95f) : glm::vec4(0.06f, 0.12f, 0.18f, 0.85f),
                               mix_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.2f, 0.5f, 0.7f, 0.5f));
        draw_text_centered_fitted(">> [OPEN ADVANCED AUDIO MIXER & RIG OPTIONS]", rx, in_y, mix_btn_w, mix_btn_h, 1.05f * ui_scale,
                                  mix_hov ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY);
        if (mouse_clicked && mix_hov) {
            m_active_tab = PauseTab::AudioSettings;
        }

    } else if (m_active_tab == PauseTab::Inventory) {
        // ── 3B. Cargo Inventory & Jettison View ──
        draw_panel_with_border(panel_x + pad, cur_y, content_w, content_box_h, glm::vec4(0.02f, 0.035f, 0.06f, 0.92f), glm::vec4(0.15f, 0.35f, 0.5f, 0.6f));

        float inner_pad = 16.0f * ui_scale;
        float rx = panel_x + pad + inner_pad;
        float row_w = content_w - 2.0f * inner_pad;
        float in_y = cur_y + 12.0f * ui_scale;

        // Carry weight telemetry
        float cur_w = inventory.carry_weight();
        float max_w = inventory.max_carry_weight(class_id);
        bool over = inventory.is_overburdened(class_id);
        float penalty = inventory.overburden_penalty(class_id);

        char weight_header[64];
        std::snprintf(weight_header, sizeof(weight_header), "DELVER CARGO RIG // LOAD: %.1f / %.1f KG", cur_w, max_w);
        glm::vec4 w_col = over ? Typography::COLOR_CRIMSON : Typography::COLOR_CYAN;
        draw_text(weight_header, rx, in_y, 1.15f * ui_scale, w_col);
        in_y += 18.0f * ui_scale;

        // Weight bar
        float wbar_w = row_w;
        float wbar_h = 7.0f * ui_scale;
        draw_rect(rx, in_y, wbar_w, wbar_h, glm::vec4(0.08f, 0.14f, 0.20f, 0.9f));
        float w_ratio = std::clamp(cur_w / max_w, 0.0f, 1.0f);
        draw_rect(rx, in_y, wbar_w * w_ratio, wbar_h, over ? Typography::COLOR_CRIMSON : Typography::COLOR_CYAN);
        in_y += wbar_h + 10.0f * ui_scale;

        if (over) {
            int slow_pct = static_cast<int>(std::round((1.0f - penalty) * 100.0f));
            std::string status_msg = "! OVERBURDEN WARNING: -" + std::to_string(slow_pct) + "% SPEED | +67% THRUSTER BURN ! JETTISON CARGO TO RESTORE MOBILITY";
            draw_text_fitted(status_msg, rx, in_y, row_w, 0.92f * ui_scale, Typography::COLOR_AMBER);
        } else {
            draw_text("MOBILITY STATUS: OPTIMAL (100% SPRINT & JETPACK POWER)", rx, in_y, 0.92f * ui_scale, Typography::COLOR_GREEN);
        }
        in_y += 18.0f * ui_scale;
        draw_rect(rx, in_y, row_w, 1.0f, glm::vec4(0.15f, 0.35f, 0.5f, 0.4f));
        in_y += 10.0f * ui_scale;

        // Item List with Drop Buttons
        struct InventoryRow {
            std::string name;
            int count;
            float unit_weight;
            DropItemType item_type;
            glm::vec4 color;
        };
        InventoryRow items[] = {
            {"Voidite Ore",         inventory.voidite,            1.0f, DropItemType::Voidite,          Typography::COLOR_CYAN},
            {"Titanium Cores",      inventory.titanium,           2.5f, DropItemType::Titanium,         Typography::COLOR_AMBER},
            {"Mineral Salvage",     inventory.salvage_parts,      0.4f, DropItemType::Salvage,          glm::vec4(0.85f, 0.85f, 0.95f, 1.0f)},
            {"Bulkhead Barricades", inventory.bulkheads,          2.0f, DropItemType::Bulkhead,         Typography::COLOR_PRIMARY},
            {"Demolition Charges",  inventory.demolition_charges, 1.5f, DropItemType::DemolitionCharge, Typography::COLOR_CRIMSON}
        };

        float item_row_h = std::clamp((content_box_h - (in_y - cur_y) - 12.0f * ui_scale) / 5.0f, 26.0f * ui_scale, 38.0f * ui_scale);
        float drop_btn_w = std::clamp(78.0f * ui_scale, 65.0f, 95.0f);
        float drop_btn_h = std::clamp(item_row_h - 4.0f * ui_scale, 20.0f, 28.0f);

        for (const auto& item : items) {
            float row_bg_y = in_y;
            draw_panel_with_border(rx, row_bg_y, row_w, item_row_h - 2.0f * ui_scale,
                                   glm::vec4(0.04f, 0.08f, 0.12f, 0.80f),
                                   glm::vec4(0.12f, 0.25f, 0.35f, 0.50f), 1.0f);

            float total_item_w = item.count * item.unit_weight;
            char info_buf[64];
            std::snprintf(info_buf, sizeof(info_buf), "%s: %d  (%.1f kg ea | %.1f kg tot)",
                          item.name.c_str(), item.count, item.unit_weight, total_item_w);

            float text_y = row_bg_y + (item_row_h - 2.0f * ui_scale - FontRenderer::get_rendered_height(0.98f * ui_scale)) * 0.5f;
            draw_text_fitted(info_buf, rx + 10.0f * ui_scale, text_y, row_w - (drop_btn_w * 2.0f + 30.0f * ui_scale), 0.98f * ui_scale, item.color);

            // [DROP 1] and [DROP 5] buttons
            float b1_x = rx + row_w - drop_btn_w * 2.0f - 8.0f * ui_scale;
            float b5_x = rx + row_w - drop_btn_w;
            float btn_y = row_bg_y + (item_row_h - 2.0f * ui_scale - drop_btn_h) * 0.5f;

            bool can_drop_1 = (item.count >= 1);
            bool can_drop_5 = (item.count >= 5);

            bool hov1 = (mouse_x >= b1_x && mouse_x <= b1_x + drop_btn_w && mouse_y >= btn_y && mouse_y <= btn_y + drop_btn_h);
            bool hov5 = (mouse_x >= b5_x && mouse_x <= b5_x + drop_btn_w && mouse_y >= btn_y && mouse_y <= btn_y + drop_btn_h);

            // Render DROP 1 button
            glm::vec4 b1_bg = !can_drop_1 ? glm::vec4(0.06f, 0.08f, 0.10f, 0.5f) :
                              (hov1 ? glm::vec4(0.40f, 0.15f, 0.15f, 0.95f) : glm::vec4(0.20f, 0.08f, 0.08f, 0.85f));
            glm::vec4 b1_border = !can_drop_1 ? glm::vec4(0.2f, 0.2f, 0.2f, 0.3f) :
                                  (hov1 ? glm::vec4(1.0f, 0.3f, 0.3f, 1.0f) : glm::vec4(0.7f, 0.2f, 0.2f, 0.6f));
            draw_panel_with_border(b1_x, btn_y, drop_btn_w, drop_btn_h, b1_bg, b1_border, 1.0f);
            draw_text_centered_fitted("[DROP 1]", b1_x, btn_y, drop_btn_w, drop_btn_h, 0.92f * ui_scale,
                                      can_drop_1 ? glm::vec4(1.0f, 0.85f, 0.85f, 1.0f) : glm::vec4(0.4f, 0.45f, 0.5f, 0.5f));

            if (can_drop_1 && hov1 && mouse_clicked) {
                m_requested_drop_item = item.item_type;
                m_requested_drop_amount = 1;
            }

            // Render DROP 5 button
            glm::vec4 b5_bg = !can_drop_5 ? glm::vec4(0.06f, 0.08f, 0.10f, 0.5f) :
                              (hov5 ? glm::vec4(0.45f, 0.12f, 0.12f, 0.95f) : glm::vec4(0.22f, 0.06f, 0.06f, 0.85f));
            glm::vec4 b5_border = !can_drop_5 ? glm::vec4(0.2f, 0.2f, 0.2f, 0.3f) :
                                  (hov5 ? glm::vec4(1.0f, 0.2f, 0.2f, 1.0f) : glm::vec4(0.8f, 0.2f, 0.2f, 0.6f));
            draw_panel_with_border(b5_x, btn_y, drop_btn_w, drop_btn_h, b5_bg, b5_border, 1.0f);
            draw_text_centered_fitted("[DROP 5]", b5_x, btn_y, drop_btn_w, drop_btn_h, 0.92f * ui_scale,
                                      can_drop_5 ? glm::vec4(1.0f, 0.85f, 0.85f, 1.0f) : glm::vec4(0.4f, 0.45f, 0.5f, 0.5f));

            if (can_drop_5 && hov5 && mouse_clicked) {
                m_requested_drop_item = item.item_type;
                m_requested_drop_amount = 5;
            }

            in_y += item_row_h;
        }

    } else if (m_active_tab == PauseTab::AudioSettings) {
        // ── 3B. Detailed Audio & Rig Mixer View ──
        draw_panel_with_border(panel_x + pad, cur_y, content_w, content_box_h, glm::vec4(0.02f, 0.035f, 0.06f, 0.92f), glm::vec4(0.15f, 0.35f, 0.5f, 0.6f));

        float r_y = cur_y + 10.0f * ui_scale;
        draw_text("ACOUSTIC COMFORT & SOUND CHANNELS", panel_x + pad + 14.0f * ui_scale, r_y, 1.15f * ui_scale, Typography::COLOR_CYAN);
        r_y += 18.0f * ui_scale;

        auto draw_volume_row = [&](const std::string& name, float& vol, float row_y, bool is_master = false) {
            float adj_btn_w = std::clamp(32.0f * ui_scale, 26.0f, 38.0f);
            float adj_btn_h = std::clamp(22.0f * ui_scale, 18.0f, 26.0f);
            float btn_plus_x = panel_x + pad + content_w - 16.0f * ui_scale - adj_btn_w;
            float btn_minus_x = btn_plus_x - adj_btn_w - 4.0f * ui_scale;

            float val_w = 46.0f * ui_scale;
            float val_x = btn_minus_x - val_w - 6.0f * ui_scale;

            float meter_w = std::clamp(80.0f * ui_scale, 60.0f, 110.0f);
            float meter_x = val_x - meter_w - 8.0f * ui_scale;

            float mute_w = 0.0f;
            float mute_x = 0.0f;
            if (is_master) {
                mute_w = 54.0f * ui_scale;
                mute_x = meter_x - mute_w - 6.0f * ui_scale;
            }

            float label_x = panel_x + pad + 14.0f * ui_scale;
            float label_max_w = (is_master ? mute_x : meter_x) - label_x - 6.0f * ui_scale;

            // 1. Channel label
            draw_text_fitted(name, label_x, row_y + (adj_btn_h - FontRenderer::get_rendered_height(1.00f * ui_scale)) * 0.5f,
                             label_max_w, 1.00f * ui_scale, glm::vec4(0.85f, 0.92f, 0.98f, 0.95f));

            // 2. Mute button if master
            if (is_master) {
                bool m_hov = (mouse_x >= mute_x && mouse_x <= mute_x + mute_w && mouse_y >= row_y && mouse_y <= row_y + adj_btn_h);
                glm::vec4 m_bg = settings.mute_all ? glm::vec4(0.45f, 0.08f, 0.08f, 1.0f) :
                                 m_hov             ? glm::vec4(0.25f, 0.35f, 0.45f, 1.0f) :
                                                     glm::vec4(0.08f, 0.14f, 0.20f, 1.0f);
                glm::vec4 m_border = settings.mute_all ? glm::vec4(1.0f, 0.3f, 0.3f, 0.9f) :
                                     m_hov             ? Typography::COLOR_CYAN_GLOW :
                                                         glm::vec4(0.2f, 0.5f, 0.7f, 0.6f);
                draw_panel_with_border(mute_x, row_y, mute_w, adj_btn_h, m_bg, m_border);
                draw_text_centered_fitted(settings.mute_all ? "UNMUTE" : "MUTE", mute_x, row_y, mute_w, adj_btn_h, 0.92f * ui_scale,
                                          settings.mute_all ? glm::vec4(1.0f, 0.4f, 0.4f, 1.0f) : (m_hov ? Typography::COLOR_CYAN : Typography::COLOR_PRIMARY));
                if (mouse_clicked && m_hov) {
                    settings.mute_all = !settings.mute_all;
                }
            }

            // 3. Slider meter bar
            float meter_h = 8.0f * ui_scale;
            float meter_y = row_y + (adj_btn_h - meter_h) * 0.5f;
            draw_rect(meter_x, meter_y, meter_w, meter_h, glm::vec4(0.06f, 0.10f, 0.14f, 1.0f));
            float fill_w = meter_w * std::clamp(vol, 0.0f, 1.0f);
            if (!is_master || !settings.mute_all) {
                glm::vec4 meter_col = (vol > 0.75f) ? Typography::COLOR_CYAN :
                                      (vol > 0.35f) ? Typography::COLOR_GREEN :
                                                      Typography::COLOR_AMBER;
                if (fill_w > 1.0f) {
                    draw_rect(meter_x, meter_y, fill_w, meter_h, meter_col);
                }
            }

            // 4. Value text
            char val_buf[16];
            if (is_master && settings.mute_all) {
                std::snprintf(val_buf, sizeof(val_buf), "OFF");
            } else {
                std::snprintf(val_buf, sizeof(val_buf), "%d%%", static_cast<int>(std::round(vol * 100.0f)));
            }
            draw_text_centered_fitted(val_buf, val_x, row_y, val_w, adj_btn_h, 0.95f * ui_scale,
                                      (is_master && settings.mute_all) ? glm::vec4(1.0f, 0.4f, 0.4f, 1.0f) : glm::vec4(0.75f, 0.85f, 0.95f, 0.95f));

            // 5. Minus button
            bool minus_hov = (mouse_x >= btn_minus_x && mouse_x <= btn_minus_x + adj_btn_w && mouse_y >= row_y && mouse_y <= row_y + adj_btn_h);
            draw_panel_with_border(btn_minus_x, row_y, adj_btn_w, adj_btn_h,
                                   minus_hov ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f),
                                   minus_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
            draw_text_centered("-", btn_minus_x, row_y, adj_btn_w, adj_btn_h, 1.15f * ui_scale, glm::vec4(1.0f));
            if (mouse_clicked && minus_hov) {
                vol = std::clamp(vol - 0.05f, 0.0f, 1.0f);
            }

            // 6. Plus button
            bool plus_hov = (mouse_x >= btn_plus_x && mouse_x <= btn_plus_x + adj_btn_w && mouse_y >= row_y && mouse_y <= row_y + adj_btn_h);
            draw_panel_with_border(btn_plus_x, row_y, adj_btn_w, adj_btn_h,
                                   plus_hov ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f),
                                   plus_hov ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
            draw_text_centered("+", btn_plus_x, row_y, adj_btn_w, adj_btn_h, 1.15f * ui_scale, glm::vec4(1.0f));
            if (mouse_clicked && plus_hov) {
                vol = std::clamp(vol + 0.05f, 0.0f, 1.0f);
            }
        };

        float row_spacing = std::clamp(28.0f * ui_scale, 24.0f, 32.0f);
        draw_volume_row("MASTER VOLUME", settings.master_volume, r_y, true);
        r_y += row_spacing;
        draw_volume_row("SFX & MINING TOOLS", settings.sfx_volume, r_y, false);
        r_y += row_spacing;
        draw_volume_row("HOSTILE / VOID STALKERS", settings.enemy_volume, r_y, false);
        r_y += row_spacing;
        draw_volume_row("CAVERN & HAZARDS", settings.ambient_volume, r_y, false);
        r_y += row_spacing;
        draw_volume_row("UI & SYSTEM ALARMS", settings.ui_volume, r_y, false);
        r_y += row_spacing;

        draw_rect(panel_x + pad + 14.0f * ui_scale, r_y, content_w - 28.0f * ui_scale, 1.0f, glm::vec4(0.15f, 0.3f, 0.45f, 0.4f));
        r_y += 6.0f * ui_scale;

        // Mouse Sensitivity & FOV Rows
        float adj_btn_w = std::clamp(32.0f * ui_scale, 26.0f, 38.0f);
        float adj_btn_h = std::clamp(22.0f * ui_scale, 18.0f, 26.0f);
        float btn2_x = panel_x + pad + content_w - 16.0f * ui_scale - adj_btn_w;
        float btn1_x = btn2_x - adj_btn_w - 4.0f * ui_scale;

        char sens_buf[32];
        std::snprintf(sens_buf, sizeof(sens_buf), "MOUSE SENSITIVITY: %.2f", settings.mouse_sensitivity);
        draw_text_fitted(sens_buf, panel_x + pad + 14.0f * ui_scale, r_y + (adj_btn_h - FontRenderer::get_rendered_height(1.00f * ui_scale)) * 0.5f,
                         btn1_x - (panel_x + pad + 14.0f * ui_scale) - 8.0f, 1.00f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
        bool s_hover1 = (mouse_x >= btn1_x && mouse_x <= btn1_x + adj_btn_w && mouse_y >= r_y && mouse_y <= r_y + adj_btn_h);
        bool s_hover2 = (mouse_x >= btn2_x && mouse_x <= btn2_x + adj_btn_w && mouse_y >= r_y && mouse_y <= r_y + adj_btn_h);
        draw_panel_with_border(btn1_x, r_y, adj_btn_w, adj_btn_h, s_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
        draw_panel_with_border(btn2_x, r_y, adj_btn_w, adj_btn_h, s_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
        draw_text_centered("-", btn1_x, r_y, adj_btn_w, adj_btn_h, 1.15f * ui_scale, glm::vec4(1.0f));
        draw_text_centered("+", btn2_x, r_y, adj_btn_w, adj_btn_h, 1.15f * ui_scale, glm::vec4(1.0f));
        if (mouse_clicked && s_hover1) settings.mouse_sensitivity = std::max(0.04f, settings.mouse_sensitivity - 0.02f);
        if (mouse_clicked && s_hover2) settings.mouse_sensitivity = std::min(0.40f, settings.mouse_sensitivity + 0.02f);
        r_y += adj_btn_h + 6.0f * ui_scale;

        char fov_buf[32];
        std::snprintf(fov_buf, sizeof(fov_buf), "FIELD OF VIEW: %d DEG", static_cast<int>(settings.fov));
        draw_text_fitted(fov_buf, panel_x + pad + 14.0f * ui_scale, r_y + (adj_btn_h - FontRenderer::get_rendered_height(1.00f * ui_scale)) * 0.5f,
                         btn1_x - (panel_x + pad + 14.0f * ui_scale) - 8.0f, 1.00f * ui_scale, glm::vec4(0.85f, 0.9f, 0.95f, 0.9f));
        bool f_hover1 = (mouse_x >= btn1_x && mouse_x <= btn1_x + adj_btn_w && mouse_y >= r_y && mouse_y <= r_y + adj_btn_h);
        bool f_hover2 = (mouse_x >= btn2_x && mouse_x <= btn2_x + adj_btn_w && mouse_y >= r_y && mouse_y <= r_y + adj_btn_h);
        draw_panel_with_border(btn1_x, r_y, adj_btn_w, adj_btn_h, f_hover1 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
        draw_panel_with_border(btn2_x, r_y, adj_btn_w, adj_btn_h, f_hover2 ? glm::vec4(0.2f, 0.4f, 0.55f, 1.0f) : glm::vec4(0.08f, 0.14f, 0.2f, 1.0f), glm::vec4(0.2f, 0.5f, 0.7f, 0.6f));
        draw_text_centered("-", btn1_x, r_y, adj_btn_w, adj_btn_h, 1.15f * ui_scale, glm::vec4(1.0f));
        draw_text_centered("+", btn2_x, r_y, adj_btn_w, adj_btn_h, 1.15f * ui_scale, glm::vec4(1.0f));
        if (mouse_clicked && f_hover1) settings.fov = std::max(60.0f, settings.fov - 5.0f);
        if (mouse_clicked && f_hover2) settings.fov = std::min(105.0f, settings.fov + 5.0f);
        r_y += adj_btn_h + 6.0f * ui_scale;

        // Test Audio Button
        float tst_w = content_w - 28.0f * ui_scale;
        float tst_h = std::clamp(22.0f * ui_scale, 18.0f, 26.0f);
        float tst_x = panel_x + pad + 14.0f * ui_scale;
        bool tst_hov = (mouse_x >= tst_x && mouse_x <= tst_x + tst_w && mouse_y >= r_y && mouse_y <= r_y + tst_h);
        draw_panel_with_border(tst_x, r_y, tst_w, tst_h,
                               tst_hov ? glm::vec4(0.08f, 0.30f, 0.25f, 0.95f) : glm::vec4(0.04f, 0.16f, 0.14f, 0.85f),
                               tst_hov ? glm::vec4(0.2f, 1.0f, 0.6f, 1.0f) : glm::vec4(0.1f, 0.6f, 0.4f, 0.6f));
        draw_text_centered_fitted("[* PREVIEW AUDIO LEVEL (TEST BLIP) *]", tst_x, r_y, tst_w, tst_h, 1.05f * ui_scale,
                                  tst_hov ? glm::vec4(0.4f, 1.0f, 0.7f, 1.0f) : glm::vec4(0.3f, 0.85f, 0.55f, 0.9f));
        if (mouse_clicked && tst_hov) {
            m_test_sound_requested = true;
        }

    } else {
        // ── 3C. Operations Manual & Controls Briefing (Two Non-Colliding Responsive Columns) ──
        draw_panel_with_border(panel_x + pad, cur_y, content_w, content_box_h, glm::vec4(0.02f, 0.035f, 0.06f, 0.92f), glm::vec4(0.15f, 0.35f, 0.5f, 0.6f));

        float col_gap = 14.0f * ui_scale;
        float col_w = (content_w - 28.0f * ui_scale - col_gap) * 0.5f;
        float col1_x = panel_x + pad + 14.0f * ui_scale;
        float col2_x = col1_x + col_w + col_gap;

        // Column 1: Keybindings Table
        float c1_y = cur_y + 10.0f * ui_scale;
        draw_text("KEYBOARD CONTROLS & AVIONICS", col1_x, c1_y, 1.08f * ui_scale, Typography::COLOR_CYAN);
        c1_y += 18.0f * ui_scale;
        draw_rect(col1_x, c1_y, col_w, 1.0f, glm::vec4(0.15f, 0.35f, 0.5f, 0.4f));
        c1_y += 8.0f * ui_scale;

        struct Binding {
            std::string key;
            std::string action;
            glm::vec4 color;
        };
        Binding bindings[] = {
            {"[WASD]",         "Locomotion & Strafing",       Typography::COLOR_PRIMARY},
            {"[SPACE]",        "Jump / Jetpack Thruster",     Typography::COLOR_PRIMARY},
            {"[L-CTRL]",       "Crouch (Dampen Acoustics)",   Typography::COLOR_GREEN},
            {"[1-3 / MWHEEL]", "Equip Drill / Weapon / Demo", Typography::COLOR_CYAN},
            {"[LMB]",          "Mining Drill / Attack",       Typography::COLOR_AMBER},
            {"[RMB]",          "Deploy Bulkhead Shelter",     Typography::COLOR_PRIMARY},
            {"[F]",            "Toggle Flashlight / Headlamp",Typography::COLOR_CYAN},
            {"[G]",            "Grappling Hook Tether",       Typography::COLOR_GREEN},
            {"[E]",            "Reel Grapple Cable",          Typography::COLOR_GREEN},
            {"[T]",            "Deploy Chemical Flare",       Typography::COLOR_CYAN},
            {"[Q]",            "Seismic Sonar Pulse",         Typography::COLOR_CYAN},
            {"[C]",            "Class Tactical Ability",      Typography::COLOR_AMBER},
            {"[I]",            "Cargo Inventory & Jettison",  Typography::COLOR_CYAN},
            {"[B]",            "Deploy Evacuation Beacon",    Typography::COLOR_GREEN},
            {"[H / F1]",       "Toggle Contractor Guide",     Typography::COLOR_PRIMARY}
        };

        float b_step = std::clamp((content_box_h - 45.0f * ui_scale) / 15.0f, 14.0f * ui_scale, 19.0f * ui_scale);
        float key_col_w = std::clamp(92.0f * ui_scale, 75.0f, 105.0f);
        for (const auto& b : bindings) {
            draw_text_fitted(b.key, col1_x, c1_y, key_col_w, 0.92f * ui_scale, Typography::COLOR_CYAN);
            float act_max_w = col_w - key_col_w - 6.0f * ui_scale;
            draw_text_fitted(b.action, col1_x + key_col_w + 6.0f * ui_scale, c1_y, act_max_w, 0.92f * ui_scale, b.color);
            c1_y += b_step;
        }

        // Column 2: Tactical Field Directives (Properly Wrapped within col_w!)
        float c2_y = cur_y + 10.0f * ui_scale;
        draw_text("FIELD DIRECTIVES & HAZARDS", col2_x, c2_y, 1.08f * ui_scale, Typography::COLOR_AMBER);
        c2_y += 18.0f * ui_scale;
        draw_rect(col2_x, c2_y, col_w, 1.0f, glm::vec4(0.15f, 0.35f, 0.5f, 0.4f));
        c2_y += 8.0f * ui_scale;

        struct Directive {
            std::string title;
            std::string desc;
            glm::vec4 color;
        };
        Directive directives[] = {
            {"NOISE DYNAMICS:", "Acoustic vibrations awaken Void Stalkers. Crouch (L-Ctrl) to dampen decibels.", Typography::COLOR_CYAN},
            {"HAZARD CLOCK:", "Escalates every 4 mins. Radiation above 50% corrodes delver suit integrity.", Typography::COLOR_CRIMSON},
            {"SEISMIC BURROWERS:", "Armored wyrms tunnel through rock. Highly weak to explosives (2.5x dmg).", Typography::COLOR_AMBER},
            {"EVACUATION BEACON:", "Hold out for 40s beacon defense, then board within 3m of pod to extract.", Typography::COLOR_GREEN}
        };

        float dir_step = std::clamp((content_box_h - 45.0f * ui_scale) / 4.0f, 44.0f * ui_scale, 62.0f * ui_scale);
        for (const auto& d : directives) {
            draw_text(d.title, col2_x, c2_y, 0.98f * ui_scale, d.color);
            float d_line_y = c2_y + 14.0f * ui_scale;
            auto wrapped = FontRenderer::wrap_text(d.desc, col_w - 8.0f * ui_scale, 0.90f * ui_scale);
            for (const auto& line : wrapped) {
                draw_text(line, col2_x + 6.0f * ui_scale, d_line_y, 0.90f * ui_scale, glm::vec4(0.85f, 0.90f, 0.95f, 0.90f));
                d_line_y += 13.0f * ui_scale;
            }
            c2_y += dir_step;
        }
    }

    // ── 4. Interactive Navigation Buttons (Anchored to Modal Bottom) ──
    // [RESUME EXPEDITION]
    bool res_hover = (mouse_x >= panel_x + pad && mouse_x <= panel_x + pad + btn_w &&
                      mouse_y >= res_y && mouse_y <= res_y + btn_h);
    draw_panel_with_border(panel_x + pad, res_y, btn_w, btn_h,
                           res_hover ? glm::vec4(0.05f, 0.35f, 0.45f, 0.95f) : glm::vec4(0.04f, 0.18f, 0.26f, 0.9f),
                           res_hover ? Typography::COLOR_CYAN_GLOW : glm::vec4(0.0f, 0.94f, 1.0f, 0.6f),
                           res_hover ? 2.0f : 1.0f);
    draw_rect(panel_x + pad, res_y, 4.0f, btn_h, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    draw_text_centered_fitted(">> [RESUME EXPEDITION]", panel_x + pad, res_y, btn_w, btn_h, 1.20f * ui_scale,
                              res_hover ? glm::vec4(1.0f) : glm::vec4(0.0f, 0.94f, 1.0f, 1.0f));
    if (res_hover && mouse_clicked) {
        action = PauseMenuAction::Resume;
    }

    // [ABANDON EXPEDITION] (Red warning)
    bool abn_hover = (mouse_x >= panel_x + pad && mouse_x <= panel_x + pad + btn_w &&
                      mouse_y >= abn_y && mouse_y <= abn_y + btn_h);
    draw_panel_with_border(panel_x + pad, abn_y, btn_w, btn_h,
                           abn_hover ? glm::vec4(0.45f, 0.08f, 0.08f, 0.95f) : glm::vec4(0.25f, 0.04f, 0.04f, 0.85f),
                           abn_hover ? glm::vec4(1.0f, 0.4f, 0.4f, 1.0f) : glm::vec4(0.8f, 0.2f, 0.2f, 0.6f),
                           abn_hover ? 2.0f : 1.0f);
    draw_rect(panel_x + pad, abn_y, 4.0f, btn_h, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
    draw_text_centered_fitted(">> [ABANDON EXPEDITION] (50% SALVAGE LOSS)", panel_x + pad, abn_y, btn_w, btn_h, 1.15f * ui_scale,
                              abn_hover ? glm::vec4(1.0f) : glm::vec4(1.0f, 0.35f, 0.35f, 1.0f));
    if (abn_hover && mouse_clicked) {
        action = PauseMenuAction::Abandon;
    }

    // [RETURN TO ORBITAL HUB]
    bool hub_hover = (mouse_x >= panel_x + pad && mouse_x <= panel_x + pad + btn_w &&
                      mouse_y >= hub_y && mouse_y <= hub_y + btn_h);
    draw_panel_with_border(panel_x + pad, hub_y, btn_w, btn_h,
                           hub_hover ? glm::vec4(0.12f, 0.18f, 0.24f, 0.95f) : glm::vec4(0.06f, 0.09f, 0.13f, 0.85f),
                           hub_hover ? glm::vec4(0.6f, 0.8f, 1.0f, 1.0f) : glm::vec4(0.3f, 0.45f, 0.6f, 0.6f),
                           hub_hover ? 2.0f : 1.0f);
    draw_rect(panel_x + pad, hub_y, 4.0f, btn_h, glm::vec4(0.4f, 0.6f, 0.75f, 0.8f));
    draw_text_centered_fitted(">> [RETURN TO ORBITAL HUB]", panel_x + pad, hub_y, btn_w, btn_h, 1.15f * ui_scale,
                              hub_hover ? glm::vec4(1.0f) : glm::vec4(0.7f, 0.8f, 0.9f, 0.9f));
    if (hub_hover && mouse_clicked) {
        action = PauseMenuAction::ReturnToStartup;
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    return action;
}

} // namespace Voidfall
