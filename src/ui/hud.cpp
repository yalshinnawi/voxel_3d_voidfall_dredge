#include "hud.hpp"
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

void HUD::update(float dt) {
    for (auto it = m_floating_loot.begin(); it != m_floating_loot.end();) {
        it->world_pos.y += 1.1f * dt;
        it->timer -= dt;
        if (it->timer <= 0.0f) {
            it = m_floating_loot.erase(it);
        } else {
            ++it;
        }
    }
}

void HUD::add_floating_loot(const glm::vec3& world_pos, const std::string& text, const glm::vec4& color) {
    FloatingLootText loot;
    loot.world_pos = world_pos;
    loot.text = text;
    loot.color = color;
    loot.timer = 1.5f;
    loot.max_timer = 1.5f;
    m_floating_loot.push_back(loot);
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
        glm::vec4 prompt_col(0.9f, 0.95f, 1.0f, 0.9f);

        if (hit.voxel.material_id == MAT_VOIDITE_CRYSTAL) {
            prompt = "[LMB] Drill Voidite Crystal  |  [F] Grapple";
            prompt_col = glm::vec4(0.8f, 0.4f, 1.0f, 1.0f);
        } else if (hit.voxel.material_id == MAT_INDUSTRIAL_BULKHEAD) {
            prompt = "[LMB] Drill Titanium Bulkhead  |  [F] Grapple";
            prompt_col = glm::vec4(0.4f, 0.85f, 1.0f, 1.0f);
        } else if (hit.voxel.material_id == MAT_REINFORCED_VAULT_DOOR) {
            prompt = "[3/RMB] Demo Charge Breach Required  |  [F] Grapple Anchor";
            prompt_col = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f);
        } else if (hit.voxel.material_id == MAT_RADIOACTIVE_ORE) {
            prompt = "[LMB] Extract Radioactive Ore  |  [F] Grapple";
            prompt_col = glm::vec4(0.3f, 1.0f, 0.4f, 1.0f);
        } else {
            prompt = "[LMB] Drill Subterranean Rock  |  [F] Grapple";
        }

        float text_w = prompt.size() * 8.0f * 1.25f;
        draw_rect(cx - text_w / 2.0f - 8.0f, cy + 24.0f, text_w + 16.0f, 22.0f, glm::vec4(0.04f, 0.06f, 0.08f, 0.85f));
        draw_rect(cx - text_w / 2.0f - 8.0f, cy + 44.0f, text_w + 16.0f, 1.0f, prompt_col * 0.7f);
        draw_text(prompt, cx - text_w / 2.0f, cy + 28.0f, 1.25f, prompt_col);
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

        float text_w = item.text.size() * 8.0f * 1.35f;
        draw_text(item.text, sx - text_w / 2.0f, sy, 1.35f, col);
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
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float cx = static_cast<float>(m_width) / 2.0f;
    float cy = static_cast<float>(m_height) / 2.0f;

    // 1. Center Crosshair & Interactive Prompt
    render_crosshair(player, world);

    // 2. Floating World-Space Loot Popups
    render_floating_loot(view, proj);

    // 3. MISSION OBJECTIVES TRACKER (Top-Left)
    {
        float p_x = 24.0f;
        float p_y = 24.0f;
        float p_w = 480.0f;
        float p_h = 168.0f;

        draw_rect(p_x, p_y, p_w, p_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.88f));
        draw_rect(p_x, p_y, 4.0f, p_h, glm::vec4(0.15f, 0.85f, 1.0f, 0.95f));
        draw_rect(p_x, p_y, p_w, 2.0f, glm::vec4(0.15f, 0.85f, 1.0f, 0.5f));

        std::string level_title;
        if (current_level == 1) level_title = "SECTOR 1: CRYSTALLINE CAVERNS";
        else if (current_level == 2) level_title = "SECTOR 2: SUBTERRANEAN VAULT";
        else level_title = "SECTOR 3: FAULT-LINE COLLAPSE";

        draw_text(level_title, p_x + 14.0f, p_y + 12.0f, 1.55f, glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
        draw_rect(p_x + 14.0f, p_y + 28.0f, p_w - 28.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.6f));

        // Objective 1: Voidite Mineral Extraction
        bool obj1_done = inventory.voidite >= inventory.target_voidite;
        glm::vec4 obj1_col = obj1_done ? glm::vec4(0.25f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.9f, 0.9f, 0.95f, 1.0f);
        std::string obj1_str = std::string(obj1_done ? "[OK] " : "[ ] ") +
            "MINE VOIDITE CRYSTALS (" + std::to_string(inventory.voidite) + "/" +
            std::to_string(inventory.target_voidite) + ")";
        draw_text(obj1_str, p_x + 14.0f, p_y + 36.0f, 1.3f, obj1_col);

        // Objective 2: Sector-Specific Primary Objective
        if (current_level == 2) {
            bool vault_done = inventory.vault_breached;
            bool relic_done = inventory.relic_extracted;
            std::string obj2_str = std::string(vault_done ? "[OK] " : "[ ] ") +
                "LOCATE & BREACH VAULT BULKHEADS [Charges]";
            draw_text(obj2_str, p_x + 14.0f, p_y + 54.0f, 1.3f, vault_done ? glm::vec4(0.25f, 0.95f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.8f, 0.2f, 1.0f));

            std::string obj3_str = std::string(relic_done ? "[OK] " : "[ ] ") +
                "RECOVER HYPER-CORE RELIC FROM VAULT";
            draw_text(obj3_str, p_x + 14.0f, p_y + 72.0f, 1.3f, relic_done ? glm::vec4(0.25f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.9f, 0.9f, 0.95f, 1.0f));
        } else {
            bool ext_active = (extraction.phase() != ExtractionPhase::Dormant);
            glm::vec4 ext_col = ext_active ? glm::vec4(1.0f, 0.65f, 0.15f, 1.0f) : glm::vec4(0.9f, 0.9f, 0.95f, 1.0f);
            std::string ext_str = ext_active ?
                "[!] EXTRACTION BEACON DEFENSE IN PROGRESS" :
                "[ ] DEPLOY EXTRACTION BEACON [Press B]";
            draw_text(ext_str, p_x + 14.0f, p_y + 54.0f, 1.3f, ext_col);
        }

        // Tectonic Stability Meter
        float stability = std::clamp(100.0f - hazard.radiation_level(), 0.0f, 100.0f);
        glm::vec4 stab_col = (stability > 50.0f) ? glm::vec4(0.2f, 0.9f, 0.4f, 1.0f) :
                             (stability > 25.0f) ? glm::vec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                                   glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
        std::string stab_text = "TECTONIC STABILITY: " + std::to_string(static_cast<int>(stability)) + "%";
        draw_text(stab_text, p_x + 14.0f, p_y + 96.0f, 1.25f, stab_col);
        draw_rect(p_x + 14.0f, p_y + 110.0f, 440.0f, 6.0f, glm::vec4(0.12f, 0.15f, 0.18f, 0.8f));
        draw_rect(p_x + 14.0f, p_y + 110.0f, 440.0f * (stability / 100.0f), 6.0f, stab_col);

        // Tactical tips
        draw_text("SONAR: Press Q to scan high-value ores through solid rock", p_x + 14.0f, p_y + 126.0f, 1.15f, glm::vec4(0.4f, 0.8f, 1.0f, 0.85f));
        draw_text("GRAPPLE: F fires anchor, E reels cable tension", p_x + 14.0f, p_y + 142.0f, 1.15f, glm::vec4(0.7f, 0.8f, 0.9f, 0.8f));
    }

    // 4. VOID HAZARD CLOCK (Top-Center)
    {
        float hz_w = 460.0f;
        float hz_h = 44.0f;
        float hz_x = cx - hz_w / 2.0f;
        float hz_y = 20.0f;

        bool tremoring = hazard.is_tremoring();
        glm::vec4 bar_bg = tremoring ? glm::vec4(0.25f, 0.05f, 0.05f, 0.95f) : glm::vec4(0.08f, 0.09f, 0.06f, 0.9f);
        draw_rect(hz_x, hz_y, hz_w, hz_h, bar_bg);
        draw_rect(hz_x, hz_y, hz_w, 2.0f, tremoring ? glm::vec4(1.0f, 0.2f, 0.2f, 1.0f) : glm::vec4(0.6f, 0.9f, 0.2f, 0.8f));

        if (tremoring) {
            draw_text("! SEISMIC TREMOR IN PROGRESS: CAVE-IN RISK !", hz_x + 16.0f, hz_y + 12.0f, 1.35f, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
            draw_rect(hz_x + 16.0f, hz_y + 32.0f, hz_w - 32.0f, 4.0f, glm::vec4(1.0f, 0.15f, 0.15f, 1.0f));
        } else {
            float tremor_ratio = std::clamp(hazard.tremor_timer() / 50.0f, 0.0f, 1.0f);
            glm::vec4 timer_col = (tremor_ratio > 0.4f) ? glm::vec4(0.6f, 0.95f, 0.2f, 0.95f) :
                                  (tremor_ratio > 0.15f) ? glm::vec4(1.0f, 0.75f, 0.2f, 0.95f) :
                                                           glm::vec4(1.0f, 0.25f, 0.15f, 0.95f);

            std::string hz_text = "NEXT TREMOR: " + std::to_string(static_cast<int>(hazard.tremor_timer())) + "s  |  RADIATION: " +
                                  std::to_string(static_cast<int>(hazard.radiation_level())) + "%";
            draw_text(hz_text, hz_x + 16.0f, hz_y + 10.0f, 1.3f, timer_col);
            draw_rect(hz_x + 16.0f, hz_y + 28.0f, hz_w - 32.0f, 6.0f, glm::vec4(0.15f, 0.18f, 0.12f, 0.8f));
            draw_rect(hz_x + 16.0f, hz_y + 28.0f, (hz_w - 32.0f) * tremor_ratio, 6.0f, timer_col);
        }
    }

    // 5. SUIT INTEGRITY & THRUSTER ENERGY (Bottom-Left)
    {
        float s_x = 24.0f;
        float s_y = static_cast<float>(m_height) - 150.0f;
        float s_w = 280.0f;
        float s_h = 110.0f;

        draw_rect(s_x, s_y, s_w, s_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.88f));
        draw_rect(s_x, s_y, 4.0f, s_h, glm::vec4(0.2f, 0.9f, 0.4f, 0.95f));

        const auto& exo = player.exo();

        // Integrity (Health)
        std::string int_str = "SUIT INTEGRITY: " + std::to_string(static_cast<int>(exo.integrity)) + "%";
        draw_text(int_str, s_x + 12.0f, s_y + 8.0f, 1.25f, glm::vec4(0.25f, 0.95f, 0.4f, 1.0f));
        draw_rect(s_x + 12.0f, s_y + 22.0f, 256.0f, 10.0f, glm::vec4(0.12f, 0.22f, 0.14f, 0.8f));
        draw_rect(s_x + 12.0f, s_y + 22.0f, 256.0f * (exo.integrity / 100.0f), 10.0f, glm::vec4(0.25f, 0.9f, 0.35f, 0.95f));

        // Thruster Energy (Power)
        std::string pwr_str = "THRUSTER FUEL: " + std::to_string(static_cast<int>(exo.power)) + "%";
        draw_text(pwr_str, s_x + 12.0f, s_y + 40.0f, 1.25f, glm::vec4(0.2f, 0.85f, 1.0f, 1.0f));
        draw_rect(s_x + 12.0f, s_y + 54.0f, 256.0f, 10.0f, glm::vec4(0.10f, 0.20f, 0.26f, 0.8f));
        draw_rect(s_x + 12.0f, s_y + 54.0f, 256.0f * (exo.power / 100.0f), 10.0f, glm::vec4(0.15f, 0.8f, 1.0f, 0.95f));

        // Heat
        std::string heat_str = exo.overheated ? "THERMAL OVERHEAT!" : ("HEAT SINK: " + std::to_string(static_cast<int>(exo.heat)) + "%");
        glm::vec4 heat_col = exo.overheated ? glm::vec4(1.0f, 0.15f, 0.15f, 1.0f) : glm::vec4(1.0f, 0.65f, 0.15f, 1.0f);
        draw_text(heat_str, s_x + 12.0f, s_y + 72.0f, 1.25f, heat_col);
        draw_rect(s_x + 12.0f, s_y + 86.0f, 256.0f, 8.0f, glm::vec4(0.26f, 0.16f, 0.10f, 0.8f));
        draw_rect(s_x + 12.0f, s_y + 86.0f, 256.0f * (exo.heat / 100.0f), 8.0f, heat_col);
    }

    // 6. HOTBAR & MINERAL TALLIES (Bottom-Right)
    {
        float hb_w = 420.0f;
        float hb_h = 125.0f;
        float hb_x = static_cast<float>(m_width) - hb_w - 24.0f;
        float hb_y = static_cast<float>(m_height) - hb_h - 24.0f;

        draw_rect(hb_x, hb_y, hb_w, hb_h, glm::vec4(0.04f, 0.06f, 0.09f, 0.88f));
        draw_rect(hb_x + hb_w - 4.0f, hb_y, 4.0f, hb_h, glm::vec4(0.15f, 0.85f, 1.0f, 0.95f));

        // Equipment Hotbar Slots
        ToolSlot active = player.active_tool();

        // Slot 1
        bool s1 = (active == ToolSlot::MiningDrill);
        draw_rect(hb_x + 12.0f, hb_y + 10.0f, 120.0f, 32.0f, s1 ? glm::vec4(0.15f, 0.35f, 0.5f, 0.95f) : glm::vec4(0.08f, 0.1f, 0.14f, 0.8f));
        draw_text("[1] DRILL", hb_x + 22.0f, hb_y + 18.0f, 1.25f, s1 ? glm::vec4(0.2f, 0.95f, 1.0f, 1.0f) : glm::vec4(0.6f, 0.65f, 0.7f, 0.8f));

        // Slot 2
        bool s2 = (active == ToolSlot::IndustrialBulkhead);
        draw_rect(hb_x + 140.0f, hb_y + 10.0f, 120.0f, 32.0f, s2 ? glm::vec4(0.15f, 0.35f, 0.5f, 0.95f) : glm::vec4(0.08f, 0.1f, 0.14f, 0.8f));
        draw_text("[2] BULKHEAD", hb_x + 148.0f, hb_y + 18.0f, 1.25f, s2 ? glm::vec4(0.2f, 0.95f, 1.0f, 1.0f) : glm::vec4(0.6f, 0.65f, 0.7f, 0.8f));

        // Slot 3
        bool s3 = (active == ToolSlot::DemolitionCharge);
        draw_rect(hb_x + 268.0f, hb_y + 10.0f, 140.0f, 32.0f, s3 ? glm::vec4(0.45f, 0.25f, 0.1f, 0.95f) : glm::vec4(0.12f, 0.08f, 0.06f, 0.8f));
        std::string s3_text = "[3] DEMO (" + std::to_string(inventory.demolition_charges) + ")";
        draw_text(s3_text, hb_x + 276.0f, hb_y + 18.0f, 1.25f, s3 ? glm::vec4(1.0f, 0.7f, 0.2f, 1.0f) : glm::vec4(0.7f, 0.5f, 0.3f, 0.8f));

        draw_rect(hb_x + 12.0f, hb_y + 50.0f, hb_w - 24.0f, 1.0f, glm::vec4(0.2f, 0.4f, 0.5f, 0.5f));

        // Extracted Resource Tallies
        std::string voidite_str = "VOIDITE:  " + std::to_string(inventory.voidite) + " (" + std::to_string(inventory.voidite * 5) + " pts)";
        std::string titan_str   = "TITANIUM: " + std::to_string(inventory.titanium) + " (" + std::to_string(inventory.titanium * 6) + " pts)";
        std::string salvage_str = "SALVAGE:  " + std::to_string(inventory.salvage_parts);
        std::string score_str   = "EXPEDITION SCORE: " + std::to_string(inventory.total_run_score) + " PTS";

        draw_text(voidite_str, hb_x + 14.0f, hb_y + 58.0f, 1.25f, glm::vec4(0.85f, 0.4f, 1.0f, 1.0f));
        draw_text(titan_str, hb_x + 14.0f, hb_y + 74.0f, 1.25f, glm::vec4(0.4f, 0.85f, 1.0f, 1.0f));
        draw_text(salvage_str, hb_x + 220.0f, hb_y + 74.0f, 1.25f, glm::vec4(0.8f, 0.8f, 0.8f, 0.9f));
        draw_text(score_str, hb_x + 14.0f, hb_y + 94.0f, 1.4f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
    }

    // 7. EXTRACTION BEACON BANNER
    if (extraction.phase() == ExtractionPhase::BeaconDeployed) {
        float ex_w = 460.0f;
        float ex_h = 44.0f;
        float ex_x = cx - ex_w / 2.0f;
        float ex_y = 70.0f;

        float pulse = extraction.siren_pulse();
        glm::vec4 banner_color = glm::mix(glm::vec4(0.85f, 0.15f, 0.1f, 0.9f), glm::vec4(1.0f, 0.4f, 0.1f, 1.0f), pulse);
        draw_rect(ex_x, ex_y, ex_w, ex_h, glm::vec4(0.12f, 0.02f, 0.02f, 0.9f));
        draw_rect(ex_x, ex_y, ex_w * (extraction.countdown() / 90.0f), ex_h, banner_color);

        std::string evac_str = "EXTRACTION DEFENSE IN PROGRESS: " + std::to_string(static_cast<int>(extraction.countdown())) + "s";
        draw_text(evac_str, ex_x + 18.0f, ex_y + 14.0f, 1.4f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else if (extraction.phase() == ExtractionPhase::PodLanded) {
        float ex_w = 460.0f;
        float ex_h = 44.0f;
        float ex_x = cx - ex_w / 2.0f;
        float ex_y = 70.0f;
        draw_rect(ex_x, ex_y, ex_w, ex_h, glm::vec4(0.1f, 0.85f, 0.3f, 0.95f));
        draw_text("EVACUATION POD HAS TOUCHED DOWN! EXTRACT NOW!", ex_x + 14.0f, ex_y + 14.0f, 1.35f, glm::vec4(0.04f, 0.1f, 0.04f, 1.0f));
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

} // namespace Voidfall
