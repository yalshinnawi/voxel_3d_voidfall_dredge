#include "hud.hpp"
#include "font_renderer.hpp"
#include "../skills/surveying.hpp"
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
    if (m_briefing_auto_timer > 0.0f) {
        m_briefing_auto_timer = std::max(0.0f, m_briefing_auto_timer - dt);
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
        res_key = "VOIDITE CRYSTAL";
        amt = (text.find("+2") != std::string::npos) ? 2 : 1;
        pts = (amt == 2) ? 25 : 15;
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

void HUD::draw_text_centered(const std::string& text, float box_x, float box_y, float box_w, float box_h, float scale, const glm::vec4& color) {
    if (text.empty()) return;
    float text_w = FontRenderer::get_rendered_width(text, scale);
    float text_h = FontRenderer::get_rendered_height(scale);
    float x = box_x + (box_w - text_w) * 0.5f;
    float y = box_y + (box_h - text_h) * 0.5f;
    draw_text(text, x, y, scale, color);
}

void HUD::render_crosshair(const PlayerController& player, const World& world) {
    float cx = static_cast<float>(m_width) / 2.0f;
    float cy = static_cast<float>(m_height) / 2.0f;
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    if (player.active_tool() == ToolSlot::CombatWeapon) {
        // Combat Tactical Reticle tailored to weapon archetype
        WeaponArchetype arch = player.weapon_archetype();
        float reticle_rad = (arch == WeaponArchetype::MagmaScattergun) ? (18.0f * ui_scale) :
                            (arch == WeaponArchetype::NeedlerRailgun)  ? (16.0f * ui_scale) : (12.0f * ui_scale);

        glm::vec4 theme_col = (arch == WeaponArchetype::MagmaScattergun) ? Typography::COLOR_AMBER :
                              (arch == WeaponArchetype::NeedlerRailgun)  ? Typography::COLOR_GREEN : Typography::COLOR_CYAN;
        glm::vec4 ret_col = player.is_firing_weapon() ? glm::vec4(1.0f, 0.4f, 0.2f, 1.0f) : theme_col;

        if (arch == WeaponArchetype::MagmaScattergun) {
            // Wide circular spread brackets
            draw_rect(cx - reticle_rad, cy - 4.0f * ui_scale, 2.0f, 8.0f * ui_scale, ret_col);
            draw_rect(cx + reticle_rad - 2.0f, cy - 4.0f * ui_scale, 2.0f, 8.0f * ui_scale, ret_col);
            draw_rect(cx - 4.0f * ui_scale, cy - reticle_rad, 8.0f * ui_scale, 2.0f, ret_col);
            draw_rect(cx - 4.0f * ui_scale, cy + reticle_rad - 2.0f, 8.0f * ui_scale, 2.0f, ret_col);
        } else if (arch == WeaponArchetype::NeedlerRailgun) {
            // Precision sniper crosshairs with mil-dots
            draw_rect(cx - reticle_rad, cy - 0.5f, reticle_rad * 2.0f, 1.0f, ret_col);
            draw_rect(cx - 0.5f, cy - reticle_rad, 1.0f, reticle_rad * 2.0f, ret_col);
            draw_rect(cx - 8.0f * ui_scale, cy - 3.0f * ui_scale, 1.0f, 6.0f * ui_scale, ret_col);
            draw_rect(cx + 8.0f * ui_scale, cy - 3.0f * ui_scale, 1.0f, 6.0f * ui_scale, ret_col);
        } else {
            // Tactical box brackets for Plasma Carbine
            draw_rect(cx - reticle_rad, cy - 1.0f, 6.0f * ui_scale, 2.0f, ret_col);
            draw_rect(cx + reticle_rad - 6.0f * ui_scale, cy - 1.0f, 6.0f * ui_scale, 2.0f, ret_col);
            draw_rect(cx - 1.0f, cy - reticle_rad, 2.0f, 6.0f * ui_scale, ret_col);
            draw_rect(cx - 1.0f, cy + reticle_rad - 6.0f * ui_scale, 2.0f, 6.0f * ui_scale, ret_col);
        }

        // Center dot
        draw_rect(cx - 1.0f, cy - 1.0f, 2.0f, 2.0f, Typography::COLOR_PRIMARY);

        // Ammo counter pill below crosshair
        std::string ammo_str;
        glm::vec4 ammo_col = theme_col;
        if (player.is_reloading()) {
            ammo_str = (arch == WeaponArchetype::MagmaScattergun) ? "RELOADING DRUM..." :
                       (arch == WeaponArchetype::NeedlerRailgun)  ? "CHAMBERING NEEDLE..." : "RECHARGING CAPACITOR...";
            ammo_col = Typography::COLOR_AMBER;
        } else {
            ammo_str = player.weapon_name() + ": " + std::to_string(player.weapon_ammo()) + " / " + std::to_string(player.weapon_max_ammo());
            if (player.weapon_ammo() <= 2) {
                ammo_col = Typography::COLOR_CRIMSON;
            }
        }
        float t_w = FontRenderer::get_rendered_width(ammo_str, 0.95f * ui_scale);
        draw_rect(cx - t_w * 0.5f - 6.0f * ui_scale, cy + reticle_rad + 6.0f * ui_scale, t_w + 12.0f * ui_scale, 16.0f * ui_scale, glm::vec4(0.02f, 0.04f, 0.08f, 0.75f));
        draw_text(ammo_str, cx - t_w * 0.5f, cy + reticle_rad + 8.0f * ui_scale, 0.95f * ui_scale, ammo_col);
    } else {
        float progress = player.mine_progress();
        // Dynamic crosshair tightens as drilling progresses
        float gap = (8.0f - progress * 5.0f) * ui_scale;
        float len = 10.0f * ui_scale;

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
    }

    // Interactive target query within 5 units (Mining tools only)
    if (player.active_tool() != ToolSlot::CombatWeapon) {
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

            float text_w = FontRenderer::get_rendered_width(prompt, 1.15f * ui_scale);
            float pill_w = text_w + 24.0f * ui_scale;
            float pill_h = 24.0f * ui_scale;
            draw_pill(cx - pill_w / 2.0f, cy + 36.0f * ui_scale, pill_w, pill_h, prompt_col * 0.45f);
            draw_text_centered(prompt, cx - pill_w / 2.0f, cy + 36.0f * ui_scale, pill_w, pill_h, 1.15f * ui_scale, prompt_col);
        }
    }
}

void HUD::render_floating_loot(const glm::mat4& view, const glm::mat4& proj) {
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);
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

        float text_w = FontRenderer::get_rendered_width(item.text, 1.20f * ui_scale);
        float pill_w = text_w + 20.0f * ui_scale;
        float pill_h = 24.0f * ui_scale;
        draw_pill(sx - pill_w * 0.5f, sy - 2.0f * ui_scale, pill_w, pill_h, glm::vec4(col.r, col.g, col.b, 0.5f * alpha));
        draw_text_centered(item.text, sx - pill_w * 0.5f, sy - 2.0f * ui_scale, pill_w, pill_h, 1.20f * ui_scale, col);
    }
}

void HUD::render_loot_toasts() {
    if (m_loot_toasts.empty()) return;

    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);
    float toast_w = std::clamp(300.0f * ui_scale, 230.0f, 360.0f);
    float toast_x = static_cast<float>(m_width) - toast_w - 20.0f * ui_scale;
    float row_h = std::clamp(28.0f * ui_scale, 24.0f, 34.0f);
    float base_y = 110.0f * ui_scale;

    for (size_t i = 0; i < m_loot_toasts.size(); ++i) {
        const auto& toast = m_loot_toasts[i];
        float alpha = (toast.lifetime < 0.4f) ? (toast.lifetime / 0.4f) : 1.0f;
        alpha = glm::clamp(alpha, 0.0f, 1.0f);

        // Smooth slide-in animation over first 0.25s
        float age = toast.maxLifetime - toast.lifetime;
        float slide_offset = 0.0f;
        if (age < 0.25f) {
            float t = age / 0.25f;
            slide_offset = (1.0f - t) * 40.0f * ui_scale;
        }

        float draw_x = toast_x + slide_offset;
        float y = base_y + static_cast<float>(i * (row_h + 6.0f * ui_scale));

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
        float font_h = FontRenderer::get_rendered_height(0.95f * ui_scale);
        draw_text(toast.label, draw_x + 12.0f * ui_scale, y + (row_h - font_h) * 0.5f, 0.95f * ui_scale, text_col);
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
    const glm::mat4& proj,
    const SurveyingSystem* surveying,
    const NoiseMeter* noise_meter,
    int enemy_count
) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float sw = static_cast<float>(m_width);
    float sh = static_cast<float>(m_height);
    float cx = sw / 2.0f;
    float cy = sh / 2.0f;
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    // 1. Center Crosshair & Simplified Interactive Prompt
    render_crosshair(player, world);

    // 2. Discrete Toast Queue in Right Margin Above Resources
    render_loot_toasts();

    // 2b. TOP COMPASS & WAYPOINT RIBBON (Subterranean Orientation)
    {
        float comp_w = std::clamp(320.0f * ui_scale, 260.0f, 380.0f);
        float comp_h = 10.0f * ui_scale;
        float comp_x = cx - comp_w * 0.5f;
        float comp_y = 2.0f * ui_scale;

        // Subtle dark translucent backing with cyan border
        draw_rect(comp_x, comp_y, comp_w, comp_h, glm::vec4(0.02f, 0.04f, 0.06f, 0.55f));
        draw_rect(comp_x, comp_y + comp_h - 1.0f, comp_w, 1.0f, glm::vec4(0.0f, 0.85f, 1.0f, 0.25f));

        // Player yaw angle converted to 0..360 clockwise from North (-Z)
        float current_heading = std::fmod(player.yaw() + 90.0f + 360.0f, 360.0f);

        // Visible field of view across the compass ribbon is 90 degrees (+/- 45 deg)
        const float fov_range = 45.0f;

        // Render cardinal and ordinal tick marks every 15 degrees
        for (int deg = 0; deg < 360; deg += 15) {
            float diff = static_cast<float>(deg) - current_heading;
            while (diff < -180.0f) diff += 360.0f;
            while (diff > 180.0f) diff -= 360.0f;

            if (std::abs(diff) <= fov_range) {
                float norm_x = cx + (diff / fov_range) * (comp_w * 0.46f);
                bool is_major = (deg % 90 == 0);
                bool is_minor = (deg % 45 == 0);

                if (is_major) {
                    draw_rect(norm_x - 0.5f, comp_y + 1.0f, 2.0f, comp_h - 2.0f, Typography::COLOR_CYAN);
                    std::string card_name;
                    if (deg == 0) card_name = "N";
                    else if (deg == 90) card_name = "E";
                    else if (deg == 180) card_name = "S";
                    else card_name = "W";
                    draw_text(card_name, norm_x - 3.0f * ui_scale, comp_y + 1.0f, 0.80f * ui_scale, Typography::COLOR_CYAN);
                } else if (is_minor) {
                    draw_rect(norm_x, comp_y + 2.0f, 1.0f, comp_h - 4.0f, Typography::COLOR_PRIMARY * 0.7f);
                } else {
                    draw_rect(norm_x, comp_y + 4.0f, 1.0f, comp_h - 6.0f, glm::vec4(0.2f, 0.35f, 0.45f, 0.5f));
                }
            }
        }

        // Center reticle notch (active heading indicator)
        draw_rect(cx - 1.0f, comp_y, 2.0f, comp_h, Typography::COLOR_AMBER);

        // Extraction Beacon Waypoint Blip on Compass Ribbon
        if (extraction.phase() == ExtractionPhase::BeaconDeployed || extraction.phase() == ExtractionPhase::PodLanded) {
            glm::vec3 to_beacon = extraction.beacon_position() - player.position();
            float b_angle = glm::degrees(std::atan2(to_beacon.x, -to_beacon.z));
            float b_heading = std::fmod(b_angle + 360.0f, 360.0f);
            float b_diff = b_heading - current_heading;
            while (b_diff < -180.0f) b_diff += 360.0f;
            while (b_diff > 180.0f) b_diff -= 360.0f;

            float clamped_diff = std::clamp(b_diff, -fov_range, fov_range);
            float b_x = cx + (clamped_diff / fov_range) * (comp_w * 0.46f);
            bool is_landed = (extraction.phase() == ExtractionPhase::PodLanded);
            glm::vec4 b_col = is_landed ? Typography::COLOR_GREEN : Typography::COLOR_AMBER;
            draw_rect(b_x - 2.0f, comp_y + 1.0f, 4.0f, comp_h - 2.0f, b_col);
        }
    }

    // 3. TOP-LEFT (Extraction Manifest & Resources) - Dedicated, prominent, easy to read
    {
        float mf_x = 24.0f * ui_scale;
        float mf_y = 16.0f * ui_scale;
        float mf_w = std::clamp(280.0f * ui_scale, 220.0f, 320.0f);
        float mf_h = std::clamp(74.0f * ui_scale, 62.0f, 82.0f);

        draw_pill(mf_x, mf_y, mf_w, mf_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        draw_rect(mf_x, mf_y, 3.0f, mf_h, Typography::COLOR_CYAN);

        float pad_x = mf_x + 12.0f * ui_scale;
        float r0_y = mf_y + 8.0f * ui_scale;
        draw_text("// EXTRACTION MANIFEST", pad_x, r0_y, 0.85f * ui_scale, Typography::COLOR_MUTED);

        // Row 1: Voidite Haul & Quota Progress
        float r1_y = mf_y + 22.0f * ui_scale;
        int target = (current_level == 1) ? inventory.target_voidite : (current_level == 2 ? 35 : 50);
        bool quota_met = (inventory.voidite >= target);

        std::string v_str = "VOIDITE: " + std::to_string(inventory.voidite) + " / " + std::to_string(target);
        if (quota_met) {
            v_str += " [MET!]";
        }
        glm::vec4 v_col = quota_met ? Typography::COLOR_GREEN : Typography::COLOR_CYAN;
        draw_text(v_str, pad_x, r1_y, 1.15f * ui_scale, v_col);

        // Progress bar under Voidite
        float bar_w = mf_w - 24.0f * ui_scale;
        float bar_h = 5.0f * ui_scale;
        float bar_y = r1_y + 16.0f * ui_scale;
        float fill_ratio = (target > 0) ? std::clamp(static_cast<float>(inventory.voidite) / static_cast<float>(target), 0.0f, 1.0f) : 0.0f;
        draw_rect(pad_x, bar_y, bar_w, bar_h, glm::vec4(0.08f, 0.14f, 0.18f, 0.85f));
        draw_rect(pad_x, bar_y, bar_w * fill_ratio, bar_h, v_col);

        // Row 2: Secondary Minerals & Cargo Weight
        float r2_y = bar_y + bar_h + 6.0f * ui_scale;
        std::string ti_str = "TITAN: " + std::to_string(inventory.titanium);
        draw_text(ti_str, pad_x, r2_y, 0.98f * ui_scale, Typography::COLOR_AMBER);

        float cur_w = inventory.carry_weight();
        float max_w = inventory.max_carry_weight(static_cast<int>(player.character_class()));
        bool over = inventory.is_overburdened(static_cast<int>(player.character_class()));
        char load_buf[32];
        std::snprintf(load_buf, sizeof(load_buf), "LOAD: %.0f/%.0fKG", cur_w, max_w);
        glm::vec4 load_col = over ? Typography::COLOR_CRIMSON : Typography::COLOR_MUTED;
        if (over && std::fmod(m_total_time * 4.0f, 1.0f) > 0.5f) {
            load_col = Typography::COLOR_AMBER;
        }
        draw_text(std::string(load_buf), pad_x + bar_w * 0.52f, r2_y, 0.98f * ui_scale, load_col);
    }

    // 4. TOP-CENTER (Hazard Clock & Cavern Tension)
    float hz_w = std::clamp(480.0f * ui_scale, 380.0f, 540.0f);
    float hz_h = std::clamp(34.0f * ui_scale, 28.0f, 38.0f);
    float hz_x = cx - hz_w / 2.0f;
    float hz_y = 16.0f * ui_scale;

    bool tremoring = hazard.is_tremoring();
    glm::vec4 border_col = tremoring ? Typography::COLOR_CRIMSON : glm::vec4(0.0f, 0.85f, 1.0f, 0.35f);
    draw_pill(hz_x, hz_y, hz_w, hz_h, border_col);

    // Left: Objective / Extraction State
    std::string obj_str;
    glm::vec4 obj_col = Typography::COLOR_PRIMARY;
    int target_val = (current_level == 1) ? inventory.target_voidite : (current_level == 2 ? 35 : 50);
    bool quota_achieved = (inventory.voidite >= target_val);

    if (extraction.phase() == ExtractionPhase::BeaconDeployed) {
        obj_str = "BEACON DEFENSE ACTIVE";
        obj_col = Typography::COLOR_AMBER;
    } else if (extraction.phase() == ExtractionPhase::PodLanded) {
        obj_str = "BOARD EVACUATION POD!";
        obj_col = Typography::COLOR_GREEN;
    } else if (quota_achieved) {
        obj_str = "[B] READY: CALL BEACON";
        obj_col = Typography::COLOR_GREEN;
    } else {
        obj_str = "SECTOR: STABLE";
        obj_col = Typography::COLOR_CYAN;
    }
    float obj_font_h = FontRenderer::get_rendered_height(1.02f * ui_scale);
    draw_text(obj_str, hz_x + 12.0f * ui_scale, hz_y + (hz_h - obj_font_h) * 0.5f, 1.02f * ui_scale, obj_col);

    // Subtle vertical divider line
    float div_x = hz_x + hz_w * 0.48f;
    draw_rect(div_x, hz_y + 5.0f * ui_scale, 1.0f, hz_h - 10.0f * ui_scale, glm::vec4(0.2f, 0.3f, 0.4f, 0.5f));

    // Right: Hazard Clock & Stress
    float right_x = div_x + 12.0f * ui_scale;
    if (tremoring) {
        draw_text("! SEISMIC TREMOR !", right_x, hz_y + (hz_h - obj_font_h) * 0.5f, 1.02f * ui_scale, Typography::COLOR_CRIMSON);
    } else if (hazard.is_warning()) {
        draw_text("! FAULT RUPTURE !", right_x, hz_y + (hz_h - obj_font_h) * 0.5f, 1.02f * ui_scale, Typography::COLOR_CRIMSON);
    } else {
        float stress = hazard.seismic_stress();
        std::string trem_str = "SEISMIC: " + std::to_string(static_cast<int>(stress)) + "%";
        glm::vec4 trem_col = (stress >= 75.0f) ? Typography::COLOR_CRIMSON :
                             (stress >= 40.0f) ? Typography::COLOR_AMBER :
                                                 Typography::COLOR_PRIMARY;
        draw_text(trem_str, right_x, hz_y + (hz_h - obj_font_h) * 0.5f, 1.02f * ui_scale, trem_col);

        float t_ratio = std::clamp(stress / 100.0f, 0.0f, 1.0f);
        float s_bar_w = std::clamp(90.0f * ui_scale, 60.0f, 110.0f);
        float s_bar_x = hz_x + hz_w - s_bar_w - 12.0f * ui_scale;
        float s_bar_h = 5.0f * ui_scale;
        draw_rect(s_bar_x, hz_y + (hz_h - s_bar_h) * 0.5f, s_bar_w, s_bar_h, glm::vec4(0.12f, 0.15f, 0.18f, 0.8f));
        draw_rect(s_bar_x, hz_y + (hz_h - s_bar_h) * 0.5f, s_bar_w * t_ratio, s_bar_h, trem_col);
    }

    // 4b. SILENCE & ACOUSTIC DISTURBANCE METER (Directly below Hazard Pill)
    float nm_h = 0.0f;
    if (noise_meter) {
        float pct = noise_meter->noise_percent();
        bool is_crouching = noise_meter->is_crouching();
        bool has_recent_sounds = !noise_meter->recent_sounds().empty();

        // Display when noise >= 8%, or crouching in stealth mode, or active acoustic disturbances present
        if (pct >= 8.0f || is_crouching || has_recent_sounds) {
            float nm_w = std::clamp(440.0f * ui_scale, 340.0f, 480.0f);
            nm_h = std::clamp(20.0f * ui_scale, 18.0f, 24.0f);
            float nm_x = cx - nm_w / 2.0f;
            float nm_y = hz_y + hz_h + 4.0f * ui_scale;

            float nr, ng, nb;
            noise_meter->alert_color(nr, ng, nb);
            glm::vec4 alert_col(nr, ng, nb, 1.0f);

            if (is_crouching) {
                alert_col = glm::vec4(0.2f, 0.95f, 0.65f, 1.0f); // Stealth emerald
            }

            draw_pill(nm_x, nm_y, nm_w, nm_h, alert_col * 0.35f);

            std::string noise_str;
            if (is_crouching && pct < 15.0f) {
                noise_str = "[STEALTH] CROUCH CONCEALED (" + std::to_string(static_cast<int>(pct)) + "%)";
            } else {
                noise_str = "ACOUSTIC: " + std::string(noise_meter->alert_string()) + " (" + std::to_string(static_cast<int>(pct)) + "%)";
            }

            if (enemy_count > 0) {
                noise_str += " | FOES: " + std::to_string(enemy_count);
            }
            if (has_recent_sounds && !is_crouching) {
                noise_str += " | ECHOING";
            }

            float font_h = FontRenderer::get_rendered_height(0.92f * ui_scale);
            draw_text(noise_str, nm_x + 10.0f * ui_scale, nm_y + (nm_h - font_h) * 0.5f, 0.92f * ui_scale, alert_col);

            float g_w = std::clamp(85.0f * ui_scale, 65.0f, 105.0f);
            float g_h = 5.0f * ui_scale;
            float g_x = nm_x + nm_w - g_w - 10.0f * ui_scale;
            float g_y = nm_y + (nm_h - g_h) * 0.5f;

            draw_rect(g_x, g_y, g_w, g_h, glm::vec4(0.12f, 0.15f, 0.18f, 0.8f));
            float fill_ratio = std::clamp(pct / 100.0f, 0.0f, 1.0f);
            draw_rect(g_x, g_y, g_w * fill_ratio, g_h, alert_col);
        }
    }

    // 5. BOTTOM-LEFT (Delver Vitals: HP & Thruster Fuel - Clean Stacked Layout)
    {
        float s_w = std::clamp(280.0f * ui_scale, 240.0f, 320.0f);
        float s_h = std::clamp(78.0f * ui_scale, 72.0f, 88.0f);
        float s_x = 24.0f * ui_scale;
        float s_y = sh - s_h - 24.0f * ui_scale;

        const auto& exo = player.exo();

        draw_pill(s_x, s_y, s_w, s_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        glm::vec4 hp_bar_col = (exo.integrity <= 25.0f) ? Typography::COLOR_CRIMSON : Typography::COLOR_GREEN;
        draw_rect(s_x, s_y, 3.0f, s_h, hp_bar_col);

        float pad_x = s_x + 12.0f * ui_scale;
        draw_text("// EXOSUIT VITALS", pad_x, s_y + 8.0f * ui_scale, 0.82f * ui_scale, Typography::COLOR_MUTED);

        float bar_w = s_w - 24.0f * ui_scale;
        float bar_h = 6.0f * ui_scale;

        // Row 1: Health (Integrity) - Text line with percentage, bar on next line
        float r1_text_y = s_y + 20.0f * ui_scale;
        draw_text("SUIT INTEGRITY", pad_x, r1_text_y, 0.95f * ui_scale, Typography::COLOR_PRIMARY);
        std::string hp_val = std::to_string(static_cast<int>(exo.integrity)) + "%";
        draw_text(hp_val, s_x + s_w - 48.0f * ui_scale, r1_text_y, 0.95f * ui_scale, hp_bar_col);

        float hp_bar_y = s_y + 34.0f * ui_scale;
        draw_rect(pad_x, hp_bar_y, bar_w, bar_h, glm::vec4(0.10f, 0.16f, 0.12f, 0.8f));
        draw_rect(pad_x, hp_bar_y, bar_w * std::clamp(exo.integrity / 100.0f, 0.0f, 1.0f), bar_h, hp_bar_col);

        // Row 2: Fuel (Power) - Text line with percentage, bar on next line
        float r2_text_y = s_y + 46.0f * ui_scale;
        draw_text("THRUST POWER", pad_x, r2_text_y, 0.95f * ui_scale, Typography::COLOR_PRIMARY);
        std::string fuel_val = std::to_string(static_cast<int>(exo.power)) + "%";
        draw_text(fuel_val, s_x + s_w - 48.0f * ui_scale, r2_text_y, 0.95f * ui_scale, Typography::COLOR_CYAN);

        float fuel_bar_y = r2_text_y + 14.0f * ui_scale;
        draw_rect(pad_x, fuel_bar_y, bar_w, bar_h, glm::vec4(0.08f, 0.14f, 0.20f, 0.8f));
        draw_rect(pad_x, fuel_bar_y, bar_w * std::clamp(exo.power / 100.0f, 0.0f, 1.0f), bar_h, Typography::COLOR_CYAN);

        // 5b. Geiger / Radiation monitor
        float cur_rad = exo.radiation;
        if (cur_rad > 2.0f) {
            float rad_w = s_w;
            float rad_h = std::clamp(22.0f * ui_scale, 18.0f, 26.0f);
            float rad_x = s_x;
            float rad_y = s_y - rad_h - 6.0f * ui_scale;

            glm::vec4 rad_col = (cur_rad >= 80.0f) ? Typography::COLOR_CRIMSON :
                                (cur_rad >= 45.0f) ? Typography::COLOR_AMBER :
                                                     glm::vec4(0.20f, 0.95f, 0.35f, 1.0f);

            draw_pill(rad_x, rad_y, rad_w, rad_h, rad_col * 0.45f);

            float dose = (cur_rad / 100.0f) * 8.5f;
            char rad_buf[64];
            if (dose < 1.0f) {
                std::snprintf(rad_buf, sizeof(rad_buf), "[!] RAD: %.0f mSv/h (%d%%)", dose * 1000.0f, static_cast<int>(cur_rad));
            } else {
                std::snprintf(rad_buf, sizeof(rad_buf), "[!] RAD: %.1f Sv/h (%d%%)", dose, static_cast<int>(cur_rad));
            }

            float font_h = FontRenderer::get_rendered_height(0.92f * ui_scale);
            draw_text(std::string(rad_buf), rad_x + 10.0f * ui_scale, rad_y + (rad_h - font_h) * 0.5f, 0.92f * ui_scale, rad_col);

            float g_bar_w = std::clamp(80.0f * ui_scale, 60.0f, 100.0f);
            float g_bar_h = 5.0f * ui_scale;
            float g_bar_x = rad_x + rad_w - g_bar_w - 10.0f * ui_scale;
            float g_bar_y = rad_y + (rad_h - g_bar_h) * 0.5f;
            draw_rect(g_bar_x, g_bar_y, g_bar_w, g_bar_h, glm::vec4(0.10f, 0.14f, 0.12f, 0.8f));
            draw_rect(g_bar_x, g_bar_y, g_bar_w * std::clamp(cur_rad / 100.0f, 0.0f, 1.0f), g_bar_h, rad_col);
        }
    }

    // 6. BOTTOM-RIGHT (Equipment Hotbar & Tactical Abilities)
    {
        float hb_w = std::clamp(380.0f * ui_scale, 300.0f, 440.0f);
        float hb_h = std::clamp(48.0f * ui_scale, 40.0f, 54.0f);
        float hb_x = sw - hb_w - 24.0f * ui_scale;
        float hb_y = sh - hb_h - 24.0f * ui_scale;

        draw_pill(hb_x, hb_y, hb_w, hb_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        draw_rect(hb_x + hb_w - 3.0f, hb_y, 3.0f, hb_h, Typography::COLOR_CYAN);

        ToolSlot active = player.active_tool();
        bool s1 = (active == ToolSlot::MiningDrill);
        bool s2 = (active == ToolSlot::CombatWeapon);
        bool s3 = (active == ToolSlot::IndustrialBulkhead);
        bool s4 = (active == ToolSlot::DemolitionCharge);

        float slot_w = (hb_w - 16.0f * ui_scale) / 4.0f;
        float slot_scale = 0.95f * ui_scale;
        float slot_y = hb_y + (hb_h - FontRenderer::get_rendered_height(slot_scale)) * 0.5f;

        // Slot 1: Drill
        float x1 = hb_x + 8.0f * ui_scale;
        if (s1) draw_rect(x1, hb_y + hb_h - 2.0f, slot_w - 4.0f * ui_scale, 2.0f, Typography::COLOR_CYAN);
        draw_text("[1] DRILL", x1, slot_y, slot_scale, s1 ? Typography::COLOR_CYAN : Typography::COLOR_MUTED);

        // Slot 2: Weapon
        float x2 = x1 + slot_w;
        std::string w_str = player.is_reloading() ? "[2] LOAD" : ("[2] " + player.weapon_short_name() + ":" + std::to_string(player.weapon_ammo()));
        if (s2) draw_rect(x2, hb_y + hb_h - 2.0f, slot_w - 4.0f * ui_scale, 2.0f, Typography::COLOR_CYAN);
        draw_text(w_str, x2, slot_y, slot_scale, s2 ? Typography::COLOR_CYAN : Typography::COLOR_MUTED);

        // Slot 3: Bulkhead
        float x3 = x2 + slot_w;
        std::string b_str = "[3] BLK:" + std::to_string(inventory.bulkheads);
        if (s3) draw_rect(x3, hb_y + hb_h - 2.0f, slot_w - 4.0f * ui_scale, 2.0f, Typography::COLOR_AMBER);
        draw_text(b_str, x3, slot_y, slot_scale, s3 ? Typography::COLOR_AMBER : Typography::COLOR_MUTED);

        // Slot 4: Demo
        float x4 = x3 + slot_w;
        std::string d_str = "[4] DEM:" + std::to_string(inventory.demolition_charges);
        if (s4) draw_rect(x4, hb_y + hb_h - 2.0f, slot_w - 4.0f * ui_scale, 2.0f, Typography::COLOR_AMBER);
        draw_text(d_str, x4, slot_y, slot_scale, s4 ? Typography::COLOR_AMBER : Typography::COLOR_MUTED);

        // 6b. DELVER TACTICAL ABILITIES (Combined single tactical pill above hotbar)
        float ab_w = hb_w;
        float ab_h = 24.0f * ui_scale;
        float ab_x = hb_x;
        float ab_y = hb_y - ab_h - 4.0f * ui_scale;

        draw_pill(ab_x, ab_y, ab_w, ab_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));

        // Center vertical divider
        float mid_x = ab_x + ab_w * 0.5f;
        draw_rect(mid_x, ab_y + 4.0f * ui_scale, 1.0f, ab_h - 8.0f * ui_scale, glm::vec4(0.2f, 0.35f, 0.45f, 0.5f));

        float half_w = ab_w * 0.5f;

        // Left Half: [Q] Seismic Sonar
        bool is_ready = player.is_sonar_ready();
        bool is_active = (surveying && surveying->is_active());
        std::string q_str;
        glm::vec4 q_col;
        if (is_active) {
            q_str = "[Q] SONAR: ACTIVE";
            q_col = Typography::COLOR_CYAN;
        } else if (is_ready) {
            q_str = "[Q] SONAR: READY";
            q_col = Typography::COLOR_CYAN;
        } else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "[Q] SONAR: %.1fs", player.sonar_cooldown());
            q_str = std::string(buf);
            q_col = Typography::COLOR_AMBER;
            float prog = player.sonar_recharge_progress();
            draw_rect(ab_x + 4.0f, ab_y + ab_h - 2.0f, (half_w - 8.0f) * prog, 2.0f, Typography::COLOR_CYAN * 0.7f);
        }
        draw_text_centered(q_str, ab_x, ab_y, half_w, ab_h, 0.92f * ui_scale, q_col);

        // Right Half: [C] Tactical Class Skill
        bool t_ready = player.is_tactical_ready();
        CharacterClass cls = player.character_class();
        std::string skill_short = (cls == CharacterClass::Demolitionist) ? "BLAST" :
                                  (cls == CharacterClass::Vanguard) ? "BARRICADE" : "DASH";
        std::string c_str;
        glm::vec4 c_col;
        if (t_ready) {
            c_str = "[C] " + skill_short + ": READY";
            c_col = Typography::COLOR_CYAN;
        } else {
            char t_buf[32];
            std::snprintf(t_buf, sizeof(t_buf), "[C] %s: %.1fs", skill_short.c_str(), player.tactical_cooldown());
            c_str = std::string(t_buf);
            c_col = Typography::COLOR_AMBER;
            float t_prog = player.tactical_recharge_progress();
            draw_rect(mid_x + 4.0f, ab_y + ab_h - 2.0f, (half_w - 8.0f) * t_prog, 2.0f, Typography::COLOR_AMBER * 0.7f);
        }
        draw_text_centered(c_str, mid_x, ab_y, half_w, ab_h, 0.92f * ui_scale, c_col);
    }

    // 7. EXTRACTION BEACON COUNTDOWN BANNER (Non-colliding positioning)
    float top_stack_bottom = 16.0f * ui_scale + 34.0f * ui_scale + ((noise_meter != nullptr) ? (4.0f * ui_scale + nm_h) : 0.0f);
    float ex_y = top_stack_bottom + 8.0f * ui_scale;
    float ex_h = 36.0f * ui_scale;
    bool has_evac_banner = (extraction.phase() == ExtractionPhase::BeaconDeployed || extraction.phase() == ExtractionPhase::PodLanded);

    if (extraction.phase() == ExtractionPhase::BeaconDeployed) {
        float ex_w = std::clamp(420.0f * ui_scale, 320.0f, 500.0f);
        float ex_x = cx - ex_w * 0.5f;

        float pulse = extraction.siren_pulse();
        glm::vec4 banner_color = glm::mix(glm::vec4(0.85f, 0.15f, 0.1f, 0.9f), glm::vec4(1.0f, 0.4f, 0.1f, 1.0f), pulse);
        draw_pill(ex_x, ex_y, ex_w, ex_h, banner_color);
        draw_rect(ex_x, ex_y, ex_w * std::clamp(extraction.countdown() / 40.0f, 0.0f, 1.0f), ex_h, banner_color * 0.35f);

        std::string evac_str = "EVAC POD ARRIVING IN: " + std::to_string(static_cast<int>(extraction.countdown())) + "s";
        draw_text_centered(evac_str, ex_x, ex_y, ex_w, ex_h, 1.22f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else if (extraction.phase() == ExtractionPhase::PodLanded) {
        float ex_w = std::clamp(440.0f * ui_scale, 340.0f, 520.0f);
        float ex_x = cx - ex_w * 0.5f;
        draw_pill(ex_x, ex_y, ex_w, ex_h, glm::vec4(0.1f, 0.85f, 0.3f, 0.95f));
        draw_text_centered("EVACUATION POD HAS TOUCHED DOWN! EXTRACT NOW!", ex_x, ex_y, ex_w, ex_h, 1.18f * ui_scale, glm::vec4(0.2f, 1.0f, 0.4f, 1.0f));
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

                float d_sz = (16.0f + 2.0f * std::sin(m_total_time * 6.0f)) * ui_scale;
                glm::mat4 ui_proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);

                glm::mat4 d_model = glm::translate(glm::mat4(1.0f), glm::vec3(sx, sy, 0.0f));
                d_model = glm::rotate(d_model, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                d_model = glm::scale(d_model, glm::vec3(d_sz, d_sz, 1.0f));
                d_model = glm::translate(d_model, glm::vec3(-0.5f, -0.5f, 0.0f));

                m_ui_shader.use();
                m_ui_shader.set_mat4("uProjection", ui_proj * d_model);
                m_ui_shader.set_vec4("uColor", marker_col);

                glBindVertexArray(m_rect_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                float in_sz = d_sz - 4.0f * ui_scale;
                glm::mat4 in_model = glm::translate(glm::mat4(1.0f), glm::vec3(sx, sy, 0.0f));
                in_model = glm::rotate(in_model, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                in_model = glm::scale(in_model, glm::vec3(in_sz, in_sz, 1.0f));
                in_model = glm::translate(in_model, glm::vec3(-0.5f, -0.5f, 0.0f));
                m_ui_shader.set_mat4("uProjection", ui_proj * in_model);
                m_ui_shader.set_vec4("uColor", glm::vec4(0.04f, 0.06f, 0.08f, 0.85f));
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                float dist = glm::distance(player.position(), extraction.beacon_position());
                std::string dist_str = (is_landed ? "[EVAC POD: " : "[EVAC BEACON: ") + std::to_string(static_cast<int>(dist)) + "m]";
                float text_w = FontRenderer::get_rendered_width(dist_str, 1.10f * ui_scale);
                float pill_w = text_w + 16.0f * ui_scale;
                float pill_h = 22.0f * ui_scale;
                draw_pill(sx - pill_w * 0.5f, sy + d_sz * 0.8f + 6.0f * ui_scale, pill_w, pill_h, marker_col * 0.6f);
                draw_text_centered(dist_str, sx - pill_w * 0.5f, sy + d_sz * 0.8f + 6.0f * ui_scale, pill_w, pill_h, 1.10f * ui_scale, marker_col);
            }
        }
    }

    // 9. ON-SCREEN WARNING BANNER (Guaranteed clear vertical spacing, zero overlaps)
    if (m_warning_timer > 0.0f && !m_warning_message.empty()) {
        float warn_scale = 1.10f * ui_scale;
        float text_w = FontRenderer::get_rendered_width(m_warning_message, warn_scale);
        float warn_w = text_w + 28.0f * ui_scale;
        float warn_h = std::clamp(28.0f * ui_scale, 24.0f, 34.0f);
        float warn_x = cx - warn_w * 0.5f;
        float warn_y = has_evac_banner ? (ex_y + ex_h + 8.0f * ui_scale) : (top_stack_bottom + 8.0f * ui_scale);

        bool is_spall_amber = (m_warning_message.find("TECTONIC SPALL") != std::string::npos ||
                               m_warning_message.find("SEISMIC") != std::string::npos);
        bool is_acoustic_cyan = (m_warning_message.find("ACOUSTIC") != std::string::npos ||
                                 m_warning_message.find("AUDIO ALERT") != std::string::npos);
        glm::vec4 warn_col = is_acoustic_cyan ? glm::vec4(0.05f, 0.92f, 1.0f, 1.0f) :
                             is_spall_amber ? glm::vec4(1.0f, 0.72f, 0.05f, 1.0f) :
                                              glm::vec4(1.0f, 0.22f, 0.22f, 1.0f);
        draw_pill(warn_x, warn_y, warn_w, warn_h, warn_col);
        draw_text_centered(m_warning_message, warn_x, warn_y, warn_w, warn_h, warn_scale, warn_col);
    }

    // 9. SURVEYING SONAR MINERAL LABELS (Unlocked at Rank 2+)
    if (surveying && surveying->is_active() &&
        (surveying->is_identifying_materials() || skills.can_identify_materials() || player.upgrades().can_identify_materials())) {
        glm::mat4 vp = proj * view;
        float s_alpha = surveying->alpha();
        float screen_w = static_cast<float>(m_width);
        float screen_h = static_cast<float>(m_height);

        for (const auto& cl : surveying->clusters()) {
            glm::vec4 clip = vp * glm::vec4(cl.centroid, 1.0f);
            if (clip.w > 0.1f) {
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                if (ndc.z >= -1.0f && ndc.z <= 1.0f &&
                    ndc.x >= -1.05f && ndc.x <= 1.05f &&
                    ndc.y >= -1.05f && ndc.y <= 1.05f) {
                    float sx = (ndc.x * 0.5f + 0.5f) * screen_w;
                    float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * screen_h;
                    float dist = glm::distance(player.position(), cl.centroid);

                    std::string label;
                    glm::vec4 mat_col = Typography::COLOR_PRIMARY;
                    if (cl.material_id == MAT_VOIDITE || cl.material_id == MAT_VOIDITE_CRYSTAL) {
                        label = "[VOIDITE: " + std::to_string(static_cast<int>(dist)) + "m]";
                        mat_col = Typography::COLOR_CYAN;
                    } else if (cl.material_id == MAT_TITANIUM || cl.material_id == MAT_INDUSTRIAL_BULKHEAD) {
                        label = "[TITANIUM: " + std::to_string(static_cast<int>(dist)) + "m]";
                        mat_col = Typography::COLOR_AMBER;
                    } else if (cl.material_id == MAT_VAULT_DOOR || cl.material_id == MAT_REINFORCED_VAULT_DOOR) {
                        label = "[VAULT DOOR: " + std::to_string(static_cast<int>(dist)) + "m]";
                        mat_col = glm::vec4(1.0f, 0.0f, 0.85f, 1.0f);
                    } else if (cl.material_id == MAT_RADIOACTIVE || cl.material_id == MAT_RADIOACTIVE_ORE) {
                        label = "[RADIOACTIVE: " + std::to_string(static_cast<int>(dist)) + "m]";
                        mat_col = Typography::COLOR_GREEN;
                    } else {
                        label = "[MINERAL: " + std::to_string(static_cast<int>(dist)) + "m]";
                        mat_col = Typography::COLOR_PRIMARY;
                    }

                    glm::vec4 text_col = mat_col;
                    text_col.a = s_alpha;
                    glm::vec4 border_col = mat_col;
                    border_col.a = s_alpha * 0.8f;

                    float text_w = FontRenderer::get_rendered_width(label, 1.05f * ui_scale);
                    float pill_w = text_w + 14.0f * ui_scale;
                    float pill_h = 20.0f * ui_scale;

                    draw_pill(sx - pill_w * 0.5f, sy - pill_h * 0.5f, pill_w, pill_h, border_col);
                    draw_text_centered(label, sx - pill_w * 0.5f, sy - pill_h * 0.5f, pill_w, pill_h, 1.05f * ui_scale, text_col);
                }
            }
        }
    }

    // 9b. FULL-SCREEN SEISMIC TREMOR EFFECT & OBVIOUS VISUAL WARNING
    if (hazard.is_tremoring()) {
        float pulse = 0.55f + 0.45f * std::sin(m_total_time * 14.0f);
        float b_thick = 10.0f * ui_scale;

        // Pulsing tectonic perimeter border around entire display
        glm::vec4 tremor_col(1.0f, 0.15f, 0.10f, 0.85f * pulse);
        draw_rect(0.0f, 0.0f, sw, b_thick, tremor_col);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, tremor_col);
        draw_rect(0.0f, 0.0f, b_thick, sh, tremor_col);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, tremor_col);

        // Huge, unmistakable central alert banner across top
        float alert_w = std::clamp(540.0f * ui_scale, 400.0f, 720.0f);
        float alert_h = 40.0f * ui_scale;
        float alert_x = (sw - alert_w) * 0.5f;
        float alert_y = 66.0f * ui_scale;

        draw_pill(alert_x, alert_y, alert_w, alert_h, glm::vec4(1.0f, 0.15f, 0.15f, 0.95f));
        draw_rect(alert_x, alert_y, alert_w, alert_h, glm::vec4(0.20f, 0.02f, 0.02f, 0.80f * pulse));
        draw_text_centered("! ! ! ACTIVE SEISMIC TREMOR IN PROGRESS ! ! !", alert_x, alert_y + 4.0f * ui_scale, alert_w, alert_h * 0.45f, 1.25f * ui_scale, glm::vec4(1.0f, 0.9f, 0.9f, 1.0f));
        draw_text_centered("CAVERN CEILING DESTABILIZED // WATCH FOR FALLING ROCKS", alert_x, alert_y + 20.0f * ui_scale, alert_w, alert_h * 0.45f, 0.95f * ui_scale, glm::vec4(1.0f, 0.70f, 0.20f, 1.0f));
    } else if (hazard.is_warning()) {
        float pulse = 0.5f + 0.5f * std::sin(m_total_time * 10.0f);
        float b_thick = 7.0f * ui_scale;

        glm::vec4 warn_col(1.0f, 0.75f, 0.10f, 0.75f * pulse);
        draw_rect(0.0f, 0.0f, sw, b_thick, warn_col);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, warn_col);
        draw_rect(0.0f, 0.0f, b_thick, sh, warn_col);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, warn_col);

        float alert_w = std::clamp(500.0f * ui_scale, 360.0f, 660.0f);
        float alert_h = 36.0f * ui_scale;
        float alert_x = (sw - alert_w) * 0.5f;
        float alert_y = 66.0f * ui_scale;

        draw_pill(alert_x, alert_y, alert_w, alert_h, warn_col);
        draw_rect(alert_x, alert_y, alert_w, alert_h, glm::vec4(0.18f, 0.12f, 0.02f, 0.75f * pulse));
        draw_text_centered("[ ! ] WARNING: SEISMIC FAULT RUPTURE DETECTED [ ! ]", alert_x, alert_y + 4.0f * ui_scale, alert_w, alert_h * 0.45f, 1.15f * ui_scale, glm::vec4(1.0f, 0.95f, 0.6f, 1.0f));
        draw_text_centered("EARTHQUAKE IMMINENT -- SEEK REINFORCED SHELTER", alert_x, alert_y + 19.0f * ui_scale, alert_w, alert_h * 0.45f, 0.92f * ui_scale, Typography::COLOR_AMBER);
    }

    // 9c. DYNAMIC TACTICAL HUD WARNING BANNER (When active and not suppressed by tremor banner)
    if (m_warning_timer > 0.0f && !m_warning_message.empty() && !hazard.is_tremoring() && !hazard.is_warning()) {
        float scale = 1.15f * ui_scale;
        float text_w = FontRenderer::get_rendered_width(m_warning_message, scale);
        float banner_w = std::clamp(text_w + 32.0f * ui_scale, 320.0f, sw * 0.75f);
        float banner_h = 30.0f * ui_scale;
        float banner_x = (sw - banner_w) * 0.5f;
        float banner_y = 68.0f * ui_scale;

        float alpha = std::min(1.0f, m_warning_timer / 0.3f);
        glm::vec4 border_col(1.0f, 0.35f, 0.15f, 0.9f * alpha);
        glm::vec4 bg_col(0.12f, 0.04f, 0.04f, 0.85f * alpha);

        draw_pill(banner_x, banner_y, banner_w, banner_h, border_col);
        draw_rect(banner_x, banner_y, banner_w, banner_h, bg_col);
        draw_text_centered(m_warning_message, banner_x, banner_y, banner_w, banner_h, scale, glm::vec4(1.0f, 0.95f, 0.8f, alpha));
    }

    // 10. DIRECTIONAL DAMAGE FLASH / VIGNETTE
    if (m_damage_flash_timer > 0.0f) {
        float f_alpha = glm::clamp(m_damage_flash_timer, 0.0f, 0.75f);
        float b_thick = 24.0f;
        glm::vec4 flash_col(1.0f, 0.15f, 0.1f, f_alpha);
        draw_rect(0.0f, 0.0f, sw, b_thick, flash_col);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, flash_col);
        draw_rect(0.0f, 0.0f, b_thick, sh, flash_col);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, flash_col);
    }

    // 11. MINIMAL CONTROLS HINT (Bottom Center, Clean & Unobtrusive - auto-fades after 6s)
    if (m_total_time < 6.0f) {
        float alpha = std::clamp((6.0f - m_total_time) / 2.0f, 0.0f, 1.0f);
        std::string control_guide = "[H] FIELD MANUAL";
        float guide_scale = 0.85f * ui_scale;
        float gw = FontRenderer::get_rendered_width(control_guide, guide_scale);
        float gx = (sw - gw) * 0.5f;
        float gy = sh - 16.0f * ui_scale;
        draw_text(control_guide, gx, gy, guide_scale, glm::vec4(0.35f, 0.50f, 0.65f, 0.70f * alpha));
    }

    // 12. CONTRACTOR FIELD BRIEFING OVERLAY (Toggleable via H / F1)
    if (m_show_help_briefing) {
        float card_w = std::clamp(sw * 0.38f, 380.0f, 520.0f);
        float card_h = 182.0f * ui_scale;
        float card_x = sw - card_w - 24.0f * ui_scale;
        float card_y = 70.0f * ui_scale;

        draw_pill(card_x, card_y, card_w, card_h, glm::vec4(0.0f, 0.94f, 1.0f, 0.7f));
        draw_rect(card_x, card_y, card_w, 20.0f * ui_scale, glm::vec4(0.04f, 0.12f, 0.18f, 0.95f));
        draw_text("// CONTRACTOR FIELD BRIEFING // DIRECTIVE", card_x + 10.0f * ui_scale, card_y + 4.0f * ui_scale, 1.05f * ui_scale, Typography::COLOR_CYAN);

        float line_x = card_x + 10.0f * ui_scale;
        float by = card_y + 26.0f * ui_scale;
        float b_step = 15.5f * ui_scale;
        float line_scale = 0.95f * ui_scale;
        draw_text("1. QUOTA: Mine Voidite crystals using [LMB] Drill.", line_x, by, line_scale, glm::vec4(0.85f, 0.95f, 1.0f, 0.95f));
        by += b_step;
        draw_text("2. STEALTH: [L-CTRL] to crouch; decays acoustic noise.", line_x, by, line_scale, glm::vec4(0.2f, 0.95f, 0.5f, 0.95f));
        by += b_step;
        draw_text("3. HAZARD: >50% radiation drains suit HP. Move fast!", line_x, by, line_scale, glm::vec4(1.0f, 0.4f, 0.3f, 0.95f));
        by += b_step;
        draw_text("4. BURROWERS: Wyrms tunnel in Phase 3/4 & holdouts.", line_x, by, line_scale, glm::vec4(1.0f, 0.7f, 0.1f, 0.95f));
        by += b_step;
        draw_text("5. EXTRACTION: [B] at quota deploys beacon. Survive 40s.", line_x, by, line_scale, glm::vec4(0.0f, 0.94f, 1.0f, 0.95f));
        by += b_step + 5.0f * ui_scale;
        draw_text(">> [H] / [F1] TO DISMISS FIELD MANUAL <<", line_x, by, 0.88f * ui_scale, glm::vec4(0.6f, 0.65f, 0.75f, 0.75f));
    }

    // 13. FULL-SCREEN CINEMATIC DEATH FAILURE SEQUENCE
    if (m_death_active) {
        float t = std::clamp(m_death_timer / (m_death_duration > 0.0f ? m_death_duration : 3.5f), 0.0f, 1.0f);

        // Heavy dark crimson vignette overlay that steadily darkens
        float alpha = 0.40f + 0.55f * t;
        draw_rect(0.0f, 0.0f, sw, sh, glm::vec4(0.14f, 0.015f, 0.02f, alpha));

        // Pulsing hazard borders
        float pulse = 0.55f + 0.45f * std::sin(m_death_timer * 10.0f);
        float b_thick = 14.0f * ui_scale;
        glm::vec4 border_col(1.0f, 0.12f, 0.10f, 0.90f * pulse);
        draw_rect(0.0f, 0.0f, sw, b_thick, border_col);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, border_col);
        draw_rect(0.0f, 0.0f, b_thick, sh, border_col);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, border_col);

        // Giant prominent Death Notification Card
        float card_w = std::clamp(720.0f * ui_scale, 420.0f, sw * 0.90f);
        float card_h = 150.0f * ui_scale;
        float card_x = (sw - card_w) * 0.5f;
        float card_y = (sh - card_h) * 0.42f;

        draw_pill(card_x, card_y, card_w, card_h, glm::vec4(1.0f, 0.15f, 0.12f, 1.0f));
        draw_rect(card_x, card_y, card_w, card_h, glm::vec4(0.06f, 0.015f, 0.02f, 0.95f));

        draw_text_centered("! ! ! CRITICAL SUIT FAILURE ! ! !", card_x, card_y + 16.0f * ui_scale, card_w, 32.0f * ui_scale, 1.85f * ui_scale, glm::vec4(1.0f, 0.20f, 0.15f, 1.0f));
        draw_text_centered("DELVER VITAL SIGNS TERMINATED // M.I.A.", card_x, card_y + 56.0f * ui_scale, card_w, 24.0f * ui_scale, 1.35f * ui_scale, glm::vec4(1.0f, 0.90f, 0.90f, 1.0f));

        float remaining_time = std::max(0.0f, m_death_duration - m_death_timer);
        char sec_buf[32];
        std::snprintf(sec_buf, sizeof(sec_buf), "%.1fs", remaining_time);
        std::string prog_text = "TRANSMITTING BLACK BOX TELEMETRY... [" + std::string(sec_buf) + "]";
        draw_text_centered(prog_text, card_x, card_y + 92.0f * ui_scale, card_w, 20.0f * ui_scale, 1.10f * ui_scale, glm::vec4(1.0f, 0.70f, 0.30f, 0.95f));

        // Subterranean progress bar
        float bar_w = card_w - 40.0f * ui_scale;
        float bar_h = 6.0f * ui_scale;
        float bar_x = card_x + 20.0f * ui_scale;
        float bar_y = card_y + card_h - 18.0f * ui_scale;
        draw_rect(bar_x, bar_y, bar_w, bar_h, glm::vec4(0.2f, 0.05f, 0.05f, 0.8f));
        draw_rect(bar_x, bar_y, bar_w * t, bar_h, glm::vec4(1.0f, 0.25f, 0.15f, 1.0f));
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

} // namespace Voidfall
