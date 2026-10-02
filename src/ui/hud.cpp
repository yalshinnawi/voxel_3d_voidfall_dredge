#include "hud.hpp"
#include "font_renderer.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include "../include/font8x8.h"
#include <vector>
#include <algorithm>
#include <cmath>

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

    // 2. Dynamic Text VAO
    glGenVertexArrays(1, &m_text_vao);
    glGenBuffers(1, &m_text_vbo);
    glBindVertexArray(m_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_text_vbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float) * 2048, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

void HUD::init_font_atlas() {
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

void HUD::resize(int width, int height) {
    m_width = width;
    m_height = height;
}

void HUD::update(float dt) {
    m_total_time += dt;
    if (m_warning_timer > 0.0f) {
        m_warning_timer -= dt;
    }
    if (m_damage_flash_timer > 0.0f) {
        m_damage_flash_timer = std::max(0.0f, m_damage_flash_timer - dt * 1.5f);
    }

    for (auto it = m_floating_loot.begin(); it != m_floating_loot.end();) {
        it->world_pos.y += 1.1f * dt;
        it->timer -= dt;
        if (it->timer <= 0.0f) {
            it = m_floating_loot.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_loot_toasts.begin(); it != m_loot_toasts.end();) {
        it->lifetime -= dt;
        if (it->lifetime <= 0.0f) {
            it = m_loot_toasts.erase(it);
        } else {
            ++it;
        }
    }
}

void HUD::show_warning(const std::string& msg, float duration) {
    m_warning_message = msg;
    m_warning_timer = duration;
}

void HUD::clear_target_info() {
    // Explicit instant clearance of crosshair tooltip
}

void HUD::add_loot_toast(const std::string& resource_name, const glm::vec4& color, int count, int unit_points) {
    if (!m_loot_toasts.empty() && m_loot_toasts.front().resource_name == resource_name) {
        auto& toast = m_loot_toasts.front();
        toast.count += count;
        toast.lifetime = 2.5f;
        toast.maxLifetime = 2.5f;
        toast.label = "+" + std::to_string(toast.count) + " " + resource_name + " [x" + std::to_string(toast.count) + "]";
        return;
    }

    LootToast toast;
    toast.resource_name = resource_name;
    toast.count = count;
    toast.unit_points = unit_points;
    toast.color = color;
    toast.lifetime = 2.5f;
    toast.maxLifetime = 2.5f;
    if (count > 1) {
        toast.label = "+" + std::to_string(count) + " " + resource_name + " [x" + std::to_string(count) + "]";
    } else {
        toast.label = "+1 " + resource_name;
    }

    m_loot_toasts.push_front(toast);
    while (m_loot_toasts.size() > 5) {
        m_loot_toasts.pop_back();
    }
}

void HUD::add_floating_loot(const glm::vec3& world_pos, const std::string& text, const glm::vec4& color) {
    (void)world_pos;
    std::string res_key;
    int amt = 1;
    int pts = 0;
    if (text.find("VOIDITE") != std::string::npos) {
        res_key = "VOIDITE CRYSTAL"; amt = 10; pts = 50;
    } else if (text.find("TITANIUM") != std::string::npos) {
        res_key = "TITANIUM CORE"; amt = 2; pts = 12;
    } else if (text.find("SCRAP") != std::string::npos) {
        res_key = "SCRAP METAL"; amt = 1; pts = 0;
    } else if (text.find("RADIOACTIVE") != std::string::npos) {
        res_key = "RADIOACTIVE ORE"; amt = 15; pts = 80;
    } else if (text.find("RELIC") != std::string::npos) {
        res_key = "RELIC HYPER-CORE"; amt = 1; pts = 250;
    } else if (text.find("BULKHEAD") != std::string::npos) {
        res_key = "BULKHEAD PLATE"; amt = 1; pts = 0;
    } else {
        res_key = "SALVAGE SCRAP"; amt = 1; pts = 10;
    }

    // Always push to dedicated multi-hit right-margin stacking toast feed
    add_loot_toast(res_key, color, amt, pts);
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

void HUD::draw_pill(float x, float y, float w, float h, const glm::vec4& border_col) {
    draw_rect(x, y, w, h, Typography::COLOR_PANEL_BG);
    draw_rect(x, y, w, 1.0f, border_col);
    draw_rect(x, y + h - 1.0f, w, 1.0f, border_col);
    draw_rect(x, y, 1.0f, h, border_col);
    draw_rect(x + w - 1.0f, y, 1.0f, h, border_col);
}

void HUD::draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color) {
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
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));
    }

    glBindVertexArray(0);
}

void HUD::render_crosshair(const PlayerController& player, const World& world) {
    float cx = static_cast<float>(m_width) / 2.0f;
    float cy = static_cast<float>(m_height) / 2.0f;

    float progress = player.mine_progress();
    // Dynamic crosshair tightens as drilling progresses
    float gap = 8.0f - progress * 5.0f;
    float len = 10.0f;

    glm::vec4 ch_col = (progress > 0.0f) ?
        glm::mix(glm::vec4(1.0f, 1.0f, 1.0f, 0.85f), glm::vec4(0.2f, 0.95f, 1.0f, 1.0f), progress) :
        glm::vec4(1.0f, 1.0f, 1.0f, 0.75f);

    // Crosshair bars
    draw_rect(cx - 1.0f, cy - gap - len, 2.0f, len, ch_col); // Top
    draw_rect(cx - 1.0f, cy + gap, 2.0f, len, ch_col);       // Bottom
    draw_rect(cx - gap - len, cy - 1.0f, len, 2.0f, ch_col); // Left
    draw_rect(cx + gap, cy - 1.0f, len, 2.0f, ch_col);       // Right

    // Center point
    draw_rect(cx - 1.0f, cy - 1.0f, 2.0f, 2.0f, ch_col);

    // Interactive target query within 5 units
    RaycastHit hit = player.get_look_target(world, 5.0f);
    if (hit.hit && hit.voxel.is_solid()) {
        std::string prompt;
        glm::vec4 prompt_col = Typography::COLOR_PRIMARY;

        if (hit.voxel.material_id == MAT_VOIDITE_CRYSTAL) {
            prompt = "[LMB] DRILL VOIDITE";
            prompt_col = Typography::COLOR_CYAN;
        } else if (hit.voxel.material_id == MAT_INDUSTRIAL_BULKHEAD) {
            prompt = "[LMB] DRILL BULKHEAD";
            prompt_col = Typography::COLOR_AMBER;
        } else if (hit.voxel.material_id == MAT_REINFORCED_VAULT_DOOR) {
            prompt = "[3] BREACH VAULT";
            prompt_col = Typography::COLOR_AMBER;
        } else if (hit.voxel.material_id == MAT_RADIOACTIVE_ORE) {
            prompt = "[LMB] MINE ORE";
            prompt_col = Typography::COLOR_GREEN;
        } else {
            prompt = "[LMB] DRILL";
            prompt_col = Typography::COLOR_PRIMARY;
        }

        float text_w = static_cast<float>(prompt.size()) * 9.0f * 1.15f;
        float pill_w = text_w + 16.0f;
        float pill_h = 22.0f;
        draw_pill(cx - pill_w / 2.0f, cy + 20.0f, pill_w, pill_h, prompt_col * 0.45f);
        draw_text(prompt, cx - text_w / 2.0f, cy + 24.0f, 1.15f, prompt_col);
    }
}

void HUD::render_floating_loot(const glm::mat4& view, const glm::mat4& proj) {
    for (const auto& item : m_floating_loot) {
        glm::vec4 clip = proj * view * glm::vec4(item.world_pos, 1.0f);
        if (clip.w <= 0.1f) continue;

        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) continue;

        float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(m_width);
        float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(m_height);

        float alpha = std::clamp(item.timer / item.max_timer, 0.0f, 1.0f);
        glm::vec4 col = item.color;
        col.a *= alpha;

        float text_w = static_cast<float>(item.text.size()) * 9.0f * 1.25f;
        draw_pill(sx - text_w / 2.0f - 8.0f, sy - 2.0f, text_w + 16.0f, 22.0f, glm::vec4(col.r, col.g, col.b, 0.5f * alpha));
        draw_text(item.text, sx - text_w / 2.0f, sy + 3.0f, 1.25f, col);
    }
}

void HUD::render_loot_toasts() {
    if (m_loot_toasts.empty()) return;

    float toast_w = 260.0f;
    float toast_x = static_cast<float>(m_width) - 280.0f;
    float row_h = 28.0f;
    float base_y = 120.0f;

    for (size_t i = 0; i < m_loot_toasts.size(); ++i) {
        const auto& toast = m_loot_toasts[i];
        float alpha = (toast.lifetime < 0.4f) ? (toast.lifetime / 0.4f) : 1.0f;
        alpha = glm::clamp(alpha, 0.0f, 1.0f);

        // Smooth slide-in animation over first 0.25s
        float age = toast.maxLifetime - toast.lifetime;
        float slide_offset = 0.0f;
        if (age < 0.25f) {
            float t = age / 0.25f;
            slide_offset = (1.0f - t) * 40.0f;
        }

        float draw_x = toast_x + slide_offset;
        float y = base_y + static_cast<float>(i * 34.0f);

        glm::vec4 border_col = (toast.resource_name.find("VOIDITE") != std::string::npos) ?
            glm::vec4(Typography::COLOR_CYAN.r, Typography::COLOR_CYAN.g, Typography::COLOR_CYAN.b, 0.90f * alpha) :
            glm::vec4(Typography::COLOR_AMBER.r, Typography::COLOR_AMBER.g, Typography::COLOR_AMBER.b, 0.90f * alpha);

        // Dark semi-transparent pill container (rgba(10, 15, 22, 0.85)) with 1px border
        glm::vec4 pill_bg(10.0f / 255.0f, 15.0f / 255.0f, 22.0f / 255.0f, 0.85f * alpha);
        draw_rect(draw_x, y, toast_w, row_h, pill_bg);
        draw_rect(draw_x, y, toast_w, 1.0f, border_col);
        draw_rect(draw_x, y + row_h - 1.0f, toast_w, 1.0f, border_col);
        draw_rect(draw_x, y, 1.0f, row_h, border_col);
        draw_rect(draw_x + toast_w - 1.0f, y, 1.0f, row_h, border_col);

        glm::vec4 text_col = glm::vec4(1.0f, 1.0f, 1.0f, alpha);
        draw_text(toast.label, draw_x + 12.0f, y + 6.0f, 0.95f, text_col);
    }
}

void HUD::render(
    const PlayerController& player,
    const World& world,
    const HazardClock& hazard,
    const ExtractionSystem& extraction,
    const PlayerInventory& inventory,
    const SkillMatrix& skills,
    int current_level,
    const glm::mat4& view,
    const glm::mat4& proj
) {
    (void)skills;
    (void)view;
    (void)proj;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float cx = static_cast<float>(m_width) / 2.0f;
    float cy = static_cast<float>(m_height) / 2.0f;

    // 1. Center Crosshair & Simplified Interactive Prompt
    render_crosshair(player, world);

    // 2. Discrete Toast Queue in Right Margin Above Resources (Anchored x = width - 280, y = 120 + i * 34)
    render_loot_toasts();

    // 3. Center-screen floating text popups completely eliminated per Task 5

    // 4. TOP-CENTER (Objective / Timer Only) - Single-line compact pill
    {
        float hz_w = 540.0f;
        float hz_h = 36.0f;
        float hz_x = cx - hz_w / 2.0f;
        float hz_y = 16.0f;

        bool tremoring = hazard.is_tremoring();
        glm::vec4 border_col = tremoring ? Typography::COLOR_CRIMSON : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f);
        draw_pill(hz_x, hz_y, hz_w, hz_h, border_col);

        // Left: Active Objective Tracker
        std::string obj_str;
        glm::vec4 obj_col = Typography::COLOR_PRIMARY;
        if (extraction.phase() == ExtractionPhase::BeaconDeployed) {
            obj_str = "EVAC IN: " + std::to_string(static_cast<int>(extraction.countdown())) + "s";
            obj_col = Typography::COLOR_AMBER;
        } else if (extraction.phase() == ExtractionPhase::PodLanded) {
            obj_str = "BOARD EVAC POD!";
            obj_col = Typography::COLOR_GREEN;
        } else if (current_level == 1) {
            bool done = inventory.voidite >= inventory.target_voidite;
            obj_str = "VOIDITE: " + std::to_string(inventory.voidite) + " / " + std::to_string(inventory.target_voidite);
            obj_col = done ? Typography::COLOR_GREEN : Typography::COLOR_CYAN;
        } else if (current_level == 2) {
            if (!inventory.vault_breached) {
                obj_str = "OBJ: BREACH VAULT [3]";
                obj_col = Typography::COLOR_AMBER;
            } else if (!inventory.relic_extracted) {
                obj_str = "OBJ: EXTRACT RELIC";
                obj_col = Typography::COLOR_CYAN;
            } else {
                obj_str = "OBJ: CALL BEACON [B]";
                obj_col = Typography::COLOR_GREEN;
            }
        } else {
            obj_str = "VOIDITE: " + std::to_string(inventory.voidite) + " / 50";
            obj_col = Typography::COLOR_AMBER;
        }
        draw_text(obj_str, hz_x + 16.0f, hz_y + 11.0f, 1.25f, obj_col);

        // Subtle vertical divider line
        draw_rect(hz_x + 285.0f, hz_y + 6.0f, 1.0f, hz_h - 12.0f, glm::vec4(0.2f, 0.3f, 0.4f, 0.5f));

        // Right: Hazard Clock (Seismic Tremor Countdown)
        if (tremoring) {
            draw_text("! SEISMIC TREMOR !", hz_x + 300.0f, hz_y + 11.0f, 1.25f, Typography::COLOR_CRIMSON);
        } else {
            int t_sec = static_cast<int>(hazard.tremor_timer());
            std::string trem_str = "TREMOR: " + std::to_string(t_sec) + "s";
            glm::vec4 trem_col = (t_sec <= 10) ? Typography::COLOR_CRIMSON :
                                 (t_sec <= 25) ? Typography::COLOR_AMBER :
                                                 Typography::COLOR_PRIMARY;
            draw_text(trem_str, hz_x + 300.0f, hz_y + 11.0f, 1.25f, trem_col);

            float t_ratio = std::clamp(hazard.tremor_timer() / 50.0f, 0.0f, 1.0f);
            draw_rect(hz_x + 420.0f, hz_y + 15.0f, 100.0f, 6.0f, glm::vec4(0.12f, 0.15f, 0.18f, 0.8f));
            draw_rect(hz_x + 420.0f, hz_y + 15.0f, 100.0f * t_ratio, 6.0f, trem_col);
        }
    }

    // 5. BOTTOM-LEFT (Vitals Only) - Compact Health & Thruster Fuel bars
    {
        float s_w = 230.0f;
        float s_h = 58.0f;
        float s_x = 24.0f;
        float s_y = static_cast<float>(m_height) - s_h - 20.0f;

        const auto& exo = player.exo();

        draw_pill(s_x, s_y, s_w, s_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        draw_rect(s_x, s_y, 3.0f, s_h, Typography::COLOR_GREEN);

        // Row 1: Health (Integrity)
        std::string hp_str = "HP   " + std::to_string(static_cast<int>(exo.integrity)) + "%";
        draw_text(hp_str, s_x + 12.0f, s_y + 8.0f, 1.15f, Typography::COLOR_GREEN);
        draw_rect(s_x + 85.0f, s_y + 12.0f, 130.0f, 8.0f, glm::vec4(0.10f, 0.16f, 0.12f, 0.8f));
        draw_rect(s_x + 85.0f, s_y + 12.0f, 130.0f * (exo.integrity / 100.0f), 8.0f, Typography::COLOR_GREEN);

        // Row 2: Fuel (Power)
        std::string fuel_str = "FUEL " + std::to_string(static_cast<int>(exo.power)) + "%";
        draw_text(fuel_str, s_x + 12.0f, s_y + 32.0f, 1.15f, Typography::COLOR_CYAN);
        draw_rect(s_x + 85.0f, s_y + 36.0f, 130.0f, 8.0f, glm::vec4(0.08f, 0.14f, 0.20f, 0.8f));
        draw_rect(s_x + 85.0f, s_y + 36.0f, 130.0f * (exo.power / 100.0f), 8.0f, Typography::COLOR_CYAN);
    }

    // 6. BOTTOM-RIGHT (Resources Only) - Compact Hotbar & Counters
    {
        float hb_w = 296.0f;
        float hb_h = 58.0f;
        float hb_x = static_cast<float>(m_width) - hb_w - 24.0f;
        float hb_y = static_cast<float>(m_height) - hb_h - 20.0f;

        draw_pill(hb_x, hb_y, hb_w, hb_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        draw_rect(hb_x + hb_w - 3.0f, hb_y, 3.0f, hb_h, Typography::COLOR_CYAN);

        ToolSlot active = player.active_tool();

        // Row 1: Hotbar Slots [1] DRILL  [2] BULK: X  [3] DEMO: Y
        bool s1 = (active == ToolSlot::MiningDrill);
        bool s2 = (active == ToolSlot::IndustrialBulkhead);
        bool s3 = (active == ToolSlot::DemolitionCharge);

        draw_text("[1] DRILL", hb_x + 12.0f, hb_y + 8.0f, 1.15f, s1 ? Typography::COLOR_CYAN : Typography::COLOR_MUTED);
        std::string b_str = "[2] BULK:" + std::to_string(inventory.bulkheads);
        draw_text(b_str, hb_x + 104.0f, hb_y + 8.0f, 1.15f, s2 ? Typography::COLOR_AMBER : Typography::COLOR_MUTED);
        std::string d_str = "[3] DEMO:" + std::to_string(inventory.demolition_charges);
        draw_text(d_str, hb_x + 204.0f, hb_y + 8.0f, 1.15f, s3 ? Typography::COLOR_AMBER : Typography::COLOR_MUTED);

        // Thin separator
        draw_rect(hb_x + 12.0f, hb_y + 28.0f, hb_w - 24.0f, 1.0f, glm::vec4(0.2f, 0.3f, 0.4f, 0.4f));

        // Row 2: Resources
        std::string voidite_str = "VOIDITE: " + std::to_string(inventory.voidite);
        std::string titan_str = "TITANIUM: " + std::to_string(inventory.titanium);
        draw_text(voidite_str, hb_x + 12.0f, hb_y + 34.0f, 1.2f, Typography::COLOR_CYAN);
        draw_text(titan_str, hb_x + 154.0f, hb_y + 34.0f, 1.2f, Typography::COLOR_AMBER);
    }

    // 7. EXTRACTION BEACON BANNER
    if (extraction.phase() == ExtractionPhase::BeaconDeployed) {
        float ex_w = 460.0f;
        float ex_h = 44.0f;
        float ex_x = cx - ex_w / 2.0f;
        float ex_y = 70.0f;

        float pulse = extraction.siren_pulse();
        glm::vec4 banner_color = glm::mix(glm::vec4(0.85f, 0.15f, 0.1f, 0.9f), glm::vec4(1.0f, 0.4f, 0.1f, 1.0f), pulse);
        draw_pill(ex_x, ex_y, ex_w, ex_h, banner_color);
        draw_rect(ex_x, ex_y, ex_w * std::clamp(extraction.countdown() / 40.0f, 0.0f, 1.0f), ex_h, banner_color * 0.4f);

        std::string evac_str = "EVAC POD ARRIVING IN: " + std::to_string(static_cast<int>(extraction.countdown())) + "s";
        draw_text(evac_str, ex_x + 18.0f, ex_y + 14.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else if (extraction.phase() == ExtractionPhase::PodLanded) {
        float ex_w = 460.0f;
        float ex_h = 44.0f;
        float ex_x = cx - ex_w / 2.0f;
        float ex_y = 70.0f;
        draw_pill(ex_x, ex_y, ex_w, ex_h, glm::vec4(0.1f, 0.85f, 0.3f, 0.95f));
        draw_text("EVACUATION POD HAS TOUCHED DOWN! EXTRACT NOW!", ex_x + 14.0f, ex_y + 14.0f, 1.35f, glm::vec4(0.2f, 1.0f, 0.4f, 1.0f));
    }

    // 8. 3D-to-2D SCREEN WAYPOINT DIAMOND FOR EXTRACTION BEACON / POD
    if (extraction.phase() == ExtractionPhase::BeaconDeployed || extraction.phase() == ExtractionPhase::PodLanded) {
        glm::vec3 b_world = extraction.beacon_position() + glm::vec3(0.0f, 1.2f, 0.0f);
        glm::vec4 clip = proj * view * glm::vec4(b_world, 1.0f);
        if (clip.w > 0.1f) {
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (ndc.z >= -1.0f && ndc.z <= 1.0f) {
                float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(m_width);
                float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(m_height);

                bool is_landed = (extraction.phase() == ExtractionPhase::PodLanded);
                glm::vec4 marker_col = is_landed ? glm::vec4(0.2f, 1.0f, 0.4f, 1.0f) :
                    glm::mix(glm::vec4(1.0f, 0.3f, 0.1f, 1.0f), glm::vec4(1.0f, 0.85f, 0.2f, 1.0f), extraction.siren_pulse());

                float d_sz = 16.0f + 2.0f * std::sin(m_total_time * 6.0f);

                glm::mat4 ui_proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);

                // Outer rotated diamond
                glm::mat4 d_model = glm::translate(glm::mat4(1.0f), glm::vec3(sx, sy, 0.0f));
                d_model = glm::rotate(d_model, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                d_model = glm::scale(d_model, glm::vec3(d_sz, d_sz, 1.0f));
                d_model = glm::translate(d_model, glm::vec3(-0.5f, -0.5f, 0.0f));

                m_ui_shader.use();
                m_ui_shader.set_mat4("uProjection", ui_proj * d_model);
                m_ui_shader.set_vec4("uColor", marker_col);

                glBindVertexArray(m_rect_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // Inner cutout for diamond wireframe look
                float in_sz = d_sz - 4.0f;
                glm::mat4 in_model = glm::translate(glm::mat4(1.0f), glm::vec3(sx, sy, 0.0f));
                in_model = glm::rotate(in_model, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                in_model = glm::scale(in_model, glm::vec3(in_sz, in_sz, 1.0f));
                in_model = glm::translate(in_model, glm::vec3(-0.5f, -0.5f, 0.0f));
                m_ui_shader.set_mat4("uProjection", ui_proj * in_model);
                m_ui_shader.set_vec4("uColor", glm::vec4(0.04f, 0.06f, 0.08f, 0.85f));
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                // Waypoint distance
                float dist = glm::distance(player.position(), extraction.beacon_position());
                std::string dist_str = (is_landed ? "[EVAC POD: " : "[EVAC BEACON: ") + std::to_string(static_cast<int>(dist)) + "m]";
                float text_w = dist_str.size() * (8.0f + 1.0f) * 1.25f;
                draw_pill(sx - text_w / 2.0f - 8.0f, sy + d_sz * 0.8f + 6.0f, text_w + 16.0f, 22.0f, marker_col * 0.6f);
                draw_text(dist_str, sx - text_w / 2.0f, sy + d_sz * 0.8f + 10.0f, 1.25f, marker_col);
            }
        }
    }

    // 9. ON-SCREEN WARNING BANNER (High-Contrast Red #FF3838 or Amber #FFAE00 on Dark Semi-Transparent Pill Backing)
    if (m_warning_timer > 0.0f && !m_warning_message.empty()) {
        float warn_w = m_warning_message.size() * (8.0f + 1.0f) * 1.35f + 40.0f;
        float warn_h = 36.0f;
        float warn_x = cx - warn_w / 2.0f;
        float warn_y = cy + 55.0f;
        bool is_spall_amber = (m_warning_message.find("TECTONIC SPALL") != std::string::npos);
        glm::vec4 warn_col = is_spall_amber ? glm::vec4(1.0f, 0.68f, 0.0f, 1.0f) : glm::vec4(1.0f, 0.22f, 0.22f, 1.0f); // Amber or Crimson
        draw_pill(warn_x, warn_y, warn_w, warn_h, warn_col);
        draw_text(m_warning_message, warn_x + 20.0f, warn_y + 11.0f, 1.35f, warn_col);
    }

    // 10. DIRECTIONAL DAMAGE FLASH / VIGNETTE
    if (m_damage_flash_timer > 0.0f) {
        float f_alpha = glm::clamp(m_damage_flash_timer, 0.0f, 0.75f);
        float b_thick = 24.0f;
        float sw = static_cast<float>(m_width);
        float sh = static_cast<float>(m_height);
        glm::vec4 flash_col(1.0f, 0.15f, 0.1f, f_alpha);
        draw_rect(0.0f, 0.0f, sw, b_thick, flash_col);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, flash_col);
        draw_rect(0.0f, 0.0f, b_thick, sh, flash_col);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, flash_col);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

} // namespace Voidfall
