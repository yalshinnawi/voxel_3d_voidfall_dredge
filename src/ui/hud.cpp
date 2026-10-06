#include "hud.hpp"
#include "font_renderer.hpp"
#include "../skills/surveying.hpp"
#include "../ai/swarm_manager.hpp"
#include "../entities/enemies/void_stalker.hpp"
#include "../entities/enemies/seismic_burrower.hpp"
#include "../systems/mission_system.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <font8x8.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace Voidfall {

HUD::HUD(int screen_width, int height, bool headless)
    : m_width(screen_width)
    , m_height(height)
    , m_headless(headless)
{
    if (!m_headless) {
        m_ui_shader.load_graphics("assets/shaders/ui.vert", "assets/shaders/ui.frag");
        m_text_shader.load_graphics("assets/shaders/text.vert", "assets/shaders/text.frag");

        init_gl();
        init_font_atlas();
    }
}

HUD::~HUD() {
    if (m_rect_vao != 0) glDeleteVertexArrays(1, &m_rect_vao);
    if (m_rect_vbo != 0) glDeleteBuffers(1, &m_rect_vbo);
    if (m_tri_vao != 0) glDeleteVertexArrays(1, &m_tri_vao);
    if (m_tri_vbo != 0) glDeleteBuffers(1, &m_tri_vbo);
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

    // 1b. Dynamic Triangle VAO (for alert triangles, combat chevrons and pointer carets)
    glGenVertexArrays(1, &m_tri_vao);
    glGenBuffers(1, &m_tri_vbo);
    glBindVertexArray(m_tri_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_tri_vbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
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
    for (auto it = m_notifications.begin(); it != m_notifications.end();) {
        it->timer -= dt;
        if (it->timer <= 0.0f) {
            it = m_notifications.erase(it);
        } else {
            ++it;
        }
    }
    if (m_damage_flash_timer > 0.0f) {
        m_damage_flash_timer = std::max(0.0f, m_damage_flash_timer - dt * 1.5f);
    }
    if (m_radiation_flash_timer > 0.0f) {
        m_radiation_flash_timer = std::max(0.0f, m_radiation_flash_timer - dt * 1.6f);
    }
    if (m_toxic_gas_flash_timer > 0.0f) {
        m_toxic_gas_flash_timer = std::max(0.0f, m_toxic_gas_flash_timer - dt * 1.4f);
    }
    m_toxic_gas_exposure = std::max(0.0f, m_toxic_gas_exposure - dt * 2.0f);

    if (m_hit_marker_timer > 0.0f) {
        m_hit_marker_timer = std::max(0.0f, m_hit_marker_timer - dt);
    }
    if (m_tool_switch_toast_timer > 0.0f) {
        m_tool_switch_toast_timer = std::max(0.0f, m_tool_switch_toast_timer - dt);
    }

    // Update screen-space visor toxic vapor particles
    for (auto it = m_screen_toxic_particles.begin(); it != m_screen_toxic_particles.end();) {
        it->x += it->vx * dt;
        it->y += it->vy * dt;
        it->life -= dt;
        if (it->life <= 0.0f) {
            it = m_screen_toxic_particles.erase(it);
        } else {
            ++it;
        }
    }

    // Update on-screen visor blood splatters (enemy attacks only)
    for (auto it = m_blood_splatters.begin(); it != m_blood_splatters.end();) {
        it->y += it->drip_speed * dt;
        it->life -= dt;
        if (it->life <= 0.0f) {
            it = m_blood_splatters.erase(it);
        } else {
            ++it;
        }
    }
    if (m_briefingTimer > 0.0f) {
        m_briefingTimer = std::max(0.0f, m_briefingTimer - dt);
        m_briefing_auto_timer = m_briefingTimer;
    } else if (m_briefing_auto_timer > 0.0f) {
        m_briefing_auto_timer = std::max(0.0f, m_briefing_auto_timer - dt);
    }
    if (m_warning_debounce_timer > 0.0f) {
        m_warning_debounce_timer = std::max(0.0f, m_warning_debounce_timer - dt);
    }

    if (m_noise_peak_timer > 0.0f) {
        m_noise_peak_timer = std::max(0.0f, m_noise_peak_timer - dt);
    } else if (m_noise_peak > 0.0f) {
        m_noise_peak = std::max(0.0f, m_noise_peak - 40.0f * dt);
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

void HUD::set_toxic_gas_exposure(float exposure, float dt) {
    m_toxic_gas_exposure = std::clamp(exposure, 0.0f, 1.0f);
    if (m_toxic_gas_exposure > 0.05f) {
        // Continuously generate drifting on-screen smoke/vapor particles on the visor
        if (m_screen_toxic_particles.size() < 24) {
            float sw = static_cast<float>(m_width);
            float sh = static_cast<float>(m_height);
            OnScreenToxicDroplet d;
            d.x = static_cast<float>(rand() % std::max(1, m_width));
            d.y = sh + 12.0f;
            d.vx = (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 60.0f;
            d.vy = -(80.0f + static_cast<float>(rand() % 120));
            d.size = 12.0f + static_cast<float>(rand() % 100) / 6.0f;
            d.max_life = 0.9f + static_cast<float>(rand() % 100) / 100.0f;
            d.life = d.max_life;
            d.alpha = 0.60f;
            m_screen_toxic_particles.push_back(d);
        }
    }
}

void HUD::trigger_enemy_blood_splatter(float intensity) {
    if (m_headless) {
        // Still register splatter so automated headless tests can verify count and behavior
        OnScreenBloodSplatter s{};
        s.x = static_cast<float>(m_width) * 0.5f;
        s.y = static_cast<float>(m_height) * 0.5f;
        s.size = 28.0f * intensity;
        s.life = s.max_life = 2.0f;
        s.droplet_count = 4;
        m_blood_splatters.push_back(s);
        return;
    }

    int num_splatters = std::clamp(static_cast<int>(2.0f + intensity * 2.0f), 2, 5);
    float sw = static_cast<float>(m_width);
    float sh = static_cast<float>(m_height);

    for (int s_idx = 0; s_idx < num_splatters; ++s_idx) {
        if (m_blood_splatters.size() >= 20) {
            m_blood_splatters.erase(m_blood_splatters.begin());
        }

        OnScreenBloodSplatter s{};
        float rand_u = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        float rand_v = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        s.x = sw * (0.12f + 0.76f * rand_u);
        s.y = sh * (0.12f + 0.76f * rand_v);
        s.size = (20.0f + 16.0f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX))) * intensity;
        s.life = s.max_life = 1.8f + 0.8f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
        s.alpha = std::clamp(0.85f + 0.15f * intensity, 0.70f, 1.0f);
        s.drip_speed = 8.0f + 14.0f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
        s.droplet_count = 3 + std::rand() % 4; // 3 to 6

        for (int d = 0; d < s.droplet_count && d < 6; ++d) {
            float angle = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 6.2831853f;
            float dist = s.size * (0.6f + 0.9f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)));
            s.dx[d] = std::cos(angle) * dist;
            s.dy[d] = std::sin(angle) * dist + (s.size * 0.25f);
            s.radii[d] = s.size * (0.14f + 0.14f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)));
        }
        m_blood_splatters.push_back(s);
    }
}

void HUD::show_warning(const std::string& msg, float duration) {
    if (msg.empty()) return;

    bool is_status_toggle = (msg.find("HEADLAMP") != std::string::npos ||
                             msg.find("FLASHLIGHT") != std::string::npos ||
                             msg.find("OBSERVATION") != std::string::npos ||
                             msg.find("BRIGHTNESS") != std::string::npos ||
                             msg.find("DISPLAY") != std::string::npos ||
                             msg.find("FULLSCREEN") != std::string::npos ||
                             msg.find("WINDOWED") != std::string::npos);

    // Enforce 0.5s debounce for repeat status toggle messages
    if (is_status_toggle && msg == m_last_warning_text && m_warning_debounce_timer > 0.0f) {
        if (!m_notifications.empty() && m_notifications.front().text == msg) {
            m_notifications.front().timer = duration;
            m_notifications.front().max_timer = duration;
            m_notifications.front().count = 1;
        }
        return;
    }
    m_last_warning_text = msg;
    m_warning_debounce_timer = 0.5f;

    // Check if matching notification is already active in queue:
    // Status toggles overwrite single notification (count=1); gameplay events increment count
    for (auto it = m_notifications.begin(); it != m_notifications.end(); ++it) {
        if (it->text == msg) {
            if (is_status_toggle) {
                it->count = 1; // Overwrite rather than stacking [0x2], [0x3], [0x4]
            } else {
                it->count++;
            }
            it->timer = duration;
            it->max_timer = duration;
            if (it != m_notifications.begin()) {
                HudNotification updated = *it;
                m_notifications.erase(it);
                m_notifications.push_front(updated);
            }
            return;
        }
    }

    // Determine semantic color based on message intent
    glm::vec4 color(1.0f, 0.55f, 0.15f, 1.0f); // Default amber/gold

    // 1. Eliminations / Rewards / Upgrades / Success
    if (msg.find("ELIMINATED") != std::string::npos ||
        msg.find("DESTROYED") != std::string::npos ||
        msg.find("FABRICATED") != std::string::npos ||
        msg.find("REFUNDED") != std::string::npos ||
        msg.find("GRANTED") != std::string::npos ||
        msg.find("REPELLED") != std::string::npos ||
        msg.find("TOUCHED DOWN") != std::string::npos ||
        msg.find("FULLSCREEN") != std::string::npos ||
        msg.find("WINDOWED") != std::string::npos ||
        msg.find("BRIGHTNESS") != std::string::npos ||
        msg.find("HEADLAMP") != std::string::npos ||
        msg.find("FLASHLIGHT") != std::string::npos) {
        color = glm::vec4(0.20f, 0.95f, 0.45f, 1.0f); // Emerald / Green
    }
    // 2. Severe Damage / Hostile Swarms / Toxic Gas / Radiation
    else if (msg.find("CRITICAL") != std::string::npos ||
             msg.find("SWARM") != std::string::npos ||
             msg.find("BREACH") != std::string::npos ||
             msg.find("LETHAL") != std::string::npos ||
             msg.find("RADIATION") != std::string::npos ||
             msg.find("TOXIC") != std::string::npos ||
             msg.find("DAMAGE") != std::string::npos ||
             msg.find("MAUL") != std::string::npos ||
             msg.find("PIERCE") != std::string::npos ||
             msg.find("IMPACT") != std::string::npos ||
             msg.find("COLLAPSE PROTOCOL") != std::string::npos ||
             msg.find("SURGE") != std::string::npos ||
             msg.find("M.I.A.") != std::string::npos ||
             msg.find("SPINES") != std::string::npos ||
             msg.find("LUNGE") != std::string::npos) {
        color = glm::vec4(1.0f, 0.22f, 0.22f, 1.0f); // Vivid Crimson
    }
    // 3. Seismic / Tectonic / Spall Hazards
    else if (msg.find("SEISMIC") != std::string::npos ||
             msg.find("TREMOR") != std::string::npos ||
             msg.find("TECTONIC") != std::string::npos ||
             msg.find("SPALL") != std::string::npos ||
             msg.find("FAULT") != std::string::npos ||
             msg.find("BURROWER") != std::string::npos ||
             msg.find("CAVE-IN") != std::string::npos) {
        color = glm::vec4(1.0f, 0.72f, 0.10f, 1.0f); // Vibrant Amber
    }
    // 4. Tactical / Sonar / Acoustic / Fuel / Quota
    else if (msg.find("TACTICAL") != std::string::npos ||
             msg.find("ACOUSTIC") != std::string::npos ||
             msg.find("AUDIO") != std::string::npos ||
             msg.find("SONAR") != std::string::npos ||
             msg.find("SENSOR") != std::string::npos ||
             msg.find("THRUSTER") != std::string::npos ||
             msg.find("QUOTA") != std::string::npos) {
        color = glm::vec4(0.05f, 0.92f, 1.0f, 1.0f); // Electric Cyan
    }

    HudNotification notif;
    notif.text = msg;
    notif.timer = duration;
    notif.max_timer = duration;
    notif.color = color;
    notif.count = 1;

    m_notifications.push_front(notif);
    while (m_notifications.size() > 4) {
        m_notifications.pop_back();
    }
}

HudTopStackLayout HUD::compute_top_stack_layout(
    float ui_scale,
    float screen_w,
    bool is_tremoring,
    bool is_warning,
    ExtractionPhase extraction_phase
) const {
    HudTopStackLayout layout;
    float cx = screen_w * 0.5f;

    // 1. Top Hazard Clock Bar
    float top_bar_y = 16.0f * ui_scale;
    float top_bar_h = 34.0f * ui_scale;
    layout.hazard_bar = { 0.0f, top_bar_y, screen_w, top_bar_h };

    float stack_y = top_bar_y + top_bar_h + 8.0f * ui_scale;

    // 2. Active Seismic Tremor or Seismic Rupture Warning
    if (is_tremoring) {
        float alert_w = std::clamp(540.0f * ui_scale, 400.0f, 720.0f);
        float alert_h = 40.0f * ui_scale;
        float alert_x = cx - alert_w * 0.5f;
        layout.seismic_banner = { alert_x, stack_y, alert_w, alert_h };
        layout.has_seismic_banner = true;
        stack_y += alert_h + 8.0f * ui_scale;
    } else if (is_warning) {
        float alert_w = std::clamp(500.0f * ui_scale, 360.0f, 660.0f);
        float alert_h = 36.0f * ui_scale;
        float alert_x = cx - alert_w * 0.5f;
        layout.seismic_banner = { alert_x, stack_y, alert_w, alert_h };
        layout.has_seismic_banner = true;
        stack_y += alert_h + 8.0f * ui_scale;
    }

    // 3. Evacuation Beacon / Pod Arrival Banner
    if (extraction_phase == ExtractionPhase::BeaconDeployed) {
        float ex_w = std::clamp(420.0f * ui_scale, 320.0f, 500.0f);
        float ex_h = 36.0f * ui_scale;
        float ex_x = cx - ex_w * 0.5f;
        layout.evac_banner = { ex_x, stack_y, ex_w, ex_h };
        layout.has_evac_banner = true;
        stack_y += ex_h + 8.0f * ui_scale;
    } else if (extraction_phase == ExtractionPhase::PodLanded) {
        float ex_w = std::clamp(440.0f * ui_scale, 340.0f, 520.0f);
        float ex_h = 36.0f * ui_scale;
        float ex_x = cx - ex_w * 0.5f;
        layout.evac_banner = { ex_x, stack_y, ex_w, ex_h };
        layout.has_evac_banner = true;
        stack_y += ex_h + 8.0f * ui_scale;
    }

    // 4. Notifications Stack (Sequential non-overlapping cards)
    size_t count = 0;
    for (const auto& notif : m_notifications) {
        if (count >= 4) break;
        std::string display_text = (notif.count > 1) ?
            (notif.text + " [x" + std::to_string(notif.count) + "]") : notif.text;

        float warn_scale = 1.10f * ui_scale;
        float text_w = FontRenderer::get_rendered_width(display_text, warn_scale);
        float warn_w = std::clamp(text_w + 32.0f * ui_scale, 280.0f * ui_scale, screen_w * 0.75f);
        float warn_h = std::clamp(28.0f * ui_scale, 24.0f, 34.0f);
        float warn_x = cx - warn_w * 0.5f;

        layout.notifications.push_back({ warn_x, stack_y, warn_w, warn_h });
        stack_y += warn_h + 6.0f * ui_scale;
        count++;
    }

    return layout;
}

void HUD::clear_target_info() {
    // Explicit instant clearance of crosshair tooltip
}

void HUD::PushLootToast(const std::string& itemId, const std::string& displayName, int count, const glm::vec4& color) {
    auto it = std::find_if(m_loot_toasts.begin(), m_loot_toasts.end(),
        [&](const LootToast& toast) {
            return (!toast.itemId.empty() && toast.itemId == itemId) ||
                   toast.resource_name == displayName ||
                   toast.displayName == displayName;
        });

    if (it != m_loot_toasts.end()) {
        it->count += count;
        it->lifetime = 2.5f; // Refresh timer
        it->maxLifetime = 2.5f;
        it->text = "+" + std::to_string(it->count) + " " + displayName + " [x" + std::to_string(it->count) + "]";
        it->label = it->text;
        return;
    }

    // If queue is full (max 4 distinct rows), pop oldest before adding
    while (m_loot_toasts.size() >= 4) {
        m_loot_toasts.pop_back();
    }

    LootToast toast;
    toast.itemId = itemId;
    toast.displayName = displayName;
    toast.resource_name = displayName;
    toast.count = count;
    toast.color = color;
    toast.lifetime = 2.5f;
    toast.maxLifetime = 2.5f;
    toast.text = (count > 1) ? ("+" + std::to_string(count) + " " + displayName + " [x" + std::to_string(count) + "]")
                             : ("+1 " + displayName);
    toast.label = toast.text;
    m_loot_toasts.push_front(toast);
}

void HUD::add_loot_toast(const std::string& resource_name, const glm::vec4& color, int count, int unit_points) {
    (void)unit_points;
    PushLootToast(resource_name, resource_name, count, color);
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

void HUD::draw_triangle(float x0, float y0, float x1, float y1, float x2, float y2, const glm::vec4& color) {
    if (m_headless) return;
    float tri_verts[6] = { x0, y0, x1, y1, x2, y2 };

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);
    m_ui_shader.use();
    m_ui_shader.set_mat4("uProjection", proj);
    m_ui_shader.set_vec4("uColor", color);

    glBindVertexArray(m_tri_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_tri_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(tri_verts), tri_verts);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void HUD::draw_line_segment(float x0, float y0, float x1, float y1, float thickness, const glm::vec4& color) {
    if (m_headless) return;
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = std::hypot(dx, dy);
    if (len < 0.0001f) return;
    float nx = -dy / len * (thickness * 0.5f);
    float ny = dx / len * (thickness * 0.5f);

    draw_triangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, color);
    draw_triangle(x0 - nx, y0 - ny, x1 + nx, y1 + ny, x0 + nx, y0 + ny, color);
}

void HUD::trigger_hit_marker(bool is_critical, float damage) {
    m_hit_marker_duration = is_critical ? 0.35f : 0.18f;
    m_hit_marker_timer = m_hit_marker_duration;
    m_hit_marker_is_crit = is_critical;
    m_hit_marker_damage = damage;
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

void HUD::draw_text_fitted(const std::string& text, float x, float y, float max_w, float base_scale, const glm::vec4& color, float min_scale) {
    if (text.empty()) return;
    float scale = FontRenderer::fit_scale(text, max_w, base_scale, min_scale);
    draw_text(text, x, y, scale, color);
}

void HUD::draw_text_centered_fitted(const std::string& text, float box_x, float box_y, float box_w, float box_h, float base_scale, const glm::vec4& color, float min_scale) {
    if (text.empty()) return;
    float pad = 4.0f;
    float scale = FontRenderer::fit_scale(text, std::max(20.0f, box_w - pad * 2.0f), base_scale, min_scale);
    draw_text_centered(text, box_x, box_y, box_w, box_h, scale, color);
}

void HUD::render_crosshair(const PlayerController& player, const World& world) {
    float cx = static_cast<float>(m_width) / 2.0f;
    float cy = static_cast<float>(m_height) / 2.0f;
    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);

    if (player.active_tool() == ToolSlot::CombatWeapon) {
        // Combat Tactical Reticle tailored to weapon archetype
        WeaponArchetype arch = player.weapon_archetype();
        float zoom_prog = player.zoom_progress();
        float reticle_rad = (arch == WeaponArchetype::MagmaScattergun) ? (18.0f * ui_scale) :
                            (arch == WeaponArchetype::NeedlerRailgun)  ? (16.0f * ui_scale) : (12.0f * ui_scale);

        // Dynamically tighten reticle brackets during ADS zoom to visually reflect tighter spread
        reticle_rad = glm::mix(reticle_rad, reticle_rad * 0.60f, zoom_prog);

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
            float line_len = glm::mix(reticle_rad, reticle_rad * 3.0f, zoom_prog);
            draw_rect(cx - line_len, cy - 0.5f, line_len * 2.0f, 1.0f, ret_col);
            draw_rect(cx - 0.5f, cy - line_len, 1.0f, line_len * 2.0f, ret_col);
            draw_rect(cx - 8.0f * ui_scale, cy - 3.0f * ui_scale, 1.0f, 6.0f * ui_scale, ret_col);
            draw_rect(cx + 8.0f * ui_scale, cy - 3.0f * ui_scale, 1.0f, 6.0f * ui_scale, ret_col);
            if (zoom_prog > 0.3f) {
                // Secondary optical mil-dots when scoped
                draw_rect(cx - 16.0f * ui_scale, cy - 2.0f * ui_scale, 1.0f, 4.0f * ui_scale, ret_col);
                draw_rect(cx + 16.0f * ui_scale, cy - 2.0f * ui_scale, 1.0f, 4.0f * ui_scale, ret_col);
                draw_rect(cx - 2.0f * ui_scale, cy - 16.0f * ui_scale, 4.0f * ui_scale, 1.0f, ret_col);
                draw_rect(cx - 2.0f * ui_scale, cy + 16.0f * ui_scale, 4.0f * ui_scale, 1.0f, ret_col);
            }
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
        bool is_reloading = (m_show_reload || player.is_reloading()) && (player.weapon_ammo() < player.weapon_max_ammo());
        if (is_reloading) {
            float rem_time = std::max(0.0f, player.reload_timer() > 0.0f ? player.reload_timer() : m_reload_status_timer);
            int sec = static_cast<int>(std::ceil(rem_time));
            ammo_str = "RELOADING: " + std::to_string(sec) + "s";
            ammo_col = Typography::COLOR_AMBER;
        } else {
            ClearReloadStatus();
            ammo_str = player.weapon_name() + ": " + std::to_string(player.weapon_ammo()) + " / " + std::to_string(player.weapon_max_ammo());
            if (player.zoom_progress() > 0.3f) {
                float mag = 1.0f / std::max(0.01f, player.weapon_stats().zoom_fov_multiplier);
                char mag_buf[16];
                std::snprintf(mag_buf, sizeof(mag_buf), " [%.1fX]", mag);
                ammo_str += mag_buf;
            }
            if (player.weapon_ammo() <= 2) {
                ammo_col = Typography::COLOR_CRIMSON;
            }
        }
        float t_w = FontRenderer::get_rendered_width(ammo_str, 0.95f * ui_scale);
        draw_rect(cx - t_w * 0.5f - 6.0f * ui_scale, cy + reticle_rad + 6.0f * ui_scale, t_w + 12.0f * ui_scale, 16.0f * ui_scale, glm::vec4(0.02f, 0.04f, 0.08f, 0.75f));
        draw_text(ammo_str, cx - t_w * 0.5f, cy + reticle_rad + 8.0f * ui_scale, 0.95f * ui_scale, ammo_col);
    } else {
        // Clear reload status immediately upon switching to Mining Drill (Slot 1)
        ClearReloadStatus();

        float progress = glm::clamp(player.mine_progress(), 0.0f, 1.0f);
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

        // Dynamic radial progress ring around HUD crosshair (filling 0° to 360° for CrackRatio)
        float ring_rad = 18.0f * ui_scale;
        float track_alpha = (progress > 0.01f) ? 0.35f : 0.12f;
        const int ring_segs = 36;
        for (int i = 0; i < ring_segs; ++i) {
            float a1 = glm::radians(i * (360.0f / ring_segs));
            float a2 = glm::radians((i + 1) * (360.0f / ring_segs));
            draw_line_segment(cx + std::cos(a1) * ring_rad, cy + std::sin(a1) * ring_rad,
                              cx + std::cos(a2) * ring_rad, cy + std::sin(a2) * ring_rad,
                              1.2f * ui_scale, glm::vec4(0.12f, 0.22f, 0.32f, track_alpha));
        }

        if (progress > 0.01f) {
            int active_segs = std::max(1, static_cast<int>(std::ceil(progress * ring_segs)));
            glm::vec4 arc_col = (progress < 0.5f) ?
                glm::mix(Typography::COLOR_CYAN, Typography::COLOR_AMBER, progress * 2.0f) :
                glm::mix(Typography::COLOR_AMBER, glm::vec4(1.0f, 0.95f, 0.85f, 1.0f), (progress - 0.5f) * 2.0f);
            for (int i = 0; i < active_segs; ++i) {
                float f1 = static_cast<float>(i) / static_cast<float>(ring_segs);
                float f2 = std::min(progress, static_cast<float>(i + 1) / static_cast<float>(ring_segs));
                float a1 = glm::radians(-90.0f + f1 * 360.0f);
                float a2 = glm::radians(-90.0f + f2 * 360.0f);
                draw_line_segment(cx + std::cos(a1) * ring_rad, cy + std::sin(a1) * ring_rad,
                                  cx + std::cos(a2) * ring_rad, cy + std::sin(a2) * ring_rad,
                                  2.5f * ui_scale, arc_col);
            }
        }
    }

    // ── Hit Marker Rendering (Normal vs Sneak Attack Critical Hit) ──
    if (m_hit_marker_timer > 0.0f) {
        float alpha = std::clamp((m_hit_marker_timer / m_hit_marker_duration) * 1.35f, 0.0f, 1.0f);
        if (m_hit_marker_is_crit) {
            // Radiant Golden Amber-Crimson Sneak Attack Critical Hit Marker
            glm::vec4 crit_col(1.0f, 0.82f, 0.15f, alpha);
            glm::vec4 crit_glow(1.0f, 0.28f, 0.12f, alpha * 0.90f);

            float r1 = 7.0f * ui_scale;
            float r2 = 18.0f * ui_scale;
            float thick = 2.4f * ui_scale;
            float barb = 4.0f * ui_scale;

            // 4 Bold Diagonal Prongs (\ / and / \)
            draw_line_segment(cx - r1, cy - r1, cx - r2, cy - r2, thick, crit_col); // Top-left
            draw_line_segment(cx + r1, cy - r1, cx + r2, cy - r2, thick, crit_col); // Top-right
            draw_line_segment(cx - r1, cy + r1, cx - r2, cy + r2, thick, crit_col); // Bottom-left
            draw_line_segment(cx + r1, cy + r1, cx + r2, cy + r2, thick, crit_col); // Bottom-right

            // Perpendicular outer barb tick marks for lethal bite
            draw_line_segment(cx - r2 - barb, cy - r2 + barb * 0.5f, cx - r2 + barb * 0.5f, cy - r2 - barb, thick * 0.85f, crit_glow);
            draw_line_segment(cx + r2 + barb, cy - r2 + barb * 0.5f, cx + r2 - barb * 0.5f, cy - r2 - barb, thick * 0.85f, crit_glow);
            draw_line_segment(cx - r2 - barb, cy + r2 - barb * 0.5f, cx - r2 + barb * 0.5f, cy + r2 + barb, thick * 0.85f, crit_glow);
            draw_line_segment(cx + r2 + barb, cy + r2 - barb * 0.5f, cx + r2 - barb * 0.5f, cy + r2 + barb, thick * 0.85f, crit_glow);

            // Center critical flash diamond
            float d_sz = 3.2f * ui_scale;
            draw_triangle(cx, cy - d_sz, cx + d_sz, cy, cx, cy + d_sz, crit_col);
            draw_triangle(cx, cy - d_sz, cx, cy + d_sz, cx - d_sz, cy, crit_col);

            // Floating "CRIT!" indicator text above crosshair
            std::string crit_str = (m_hit_marker_damage > 0.0f) ?
                ("CRIT! " + std::to_string(static_cast<int>(m_hit_marker_damage))) : "CRIT!";
            float cw = FontRenderer::get_rendered_width(crit_str, 1.10f * ui_scale);
            draw_text(crit_str, cx - cw * 0.5f, cy - r2 - 16.0f * ui_scale, 1.10f * ui_scale, crit_col);
        } else {
            // Crisp White Standard Hit Marker
            glm::vec4 hit_col(1.0f, 1.0f, 1.0f, alpha * 0.95f);
            float r1 = 5.0f * ui_scale;
            float r2 = 12.0f * ui_scale;
            float thick = 1.6f * ui_scale;

            draw_line_segment(cx - r1, cy - r1, cx - r2, cy - r2, thick, hit_col);
            draw_line_segment(cx + r1, cy - r1, cx + r2, cy - r2, thick, hit_col);
            draw_line_segment(cx - r1, cy + r1, cx - r2, cy + r2, thick, hit_col);
            draw_line_segment(cx + r1, cy + r1, cx + r2, cy + r2, thick, hit_col);
        }
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
    float base_y = (m_briefingTimer > 0.0f && !m_show_help_briefing) ? (208.0f * ui_scale) : (110.0f * ui_scale);

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
    int enemy_count,
    const std::vector<VoidStalker>* stalkers,
    const std::vector<SeismicBurrower>* burrowers,
    const class MissionSystem* mission
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

        // Extraction Beacon / Drop Pod Waypoint Blip with Euclidean Distance on Compass Ribbon
        glm::vec3 to_beacon = extraction.beacon_position() - player.position();
        float b_dist = glm::length(to_beacon);
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

        if (std::abs(b_diff) <= fov_range) {
            std::string dist_str = std::to_string(static_cast<int>(b_dist)) + "m";
            draw_text(dist_str, b_x - 6.0f * ui_scale, comp_y + comp_h + 1.0f, 0.70f * ui_scale, b_col);
        }

        // Discovered Voidite clusters and high-value mineral anomalies on Compass Ribbon
        if (surveying) {
            int shown_anomalies = 0;
            for (const auto& sv : surveying->surveyed_voxels()) {
                if (shown_anomalies >= 8) break;
                if (sv.material_id == MAT_VOIDITE_CRYSTAL || sv.material_id == MAT_PRISMATIC_CRYSTAL) {
                    glm::vec3 to_anom = glm::vec3(sv.pos.x + 0.5f, sv.pos.y + 0.5f, sv.pos.z + 0.5f) - player.position();
                    float anom_angle = glm::degrees(std::atan2(to_anom.x, -to_anom.z));
                    float anom_heading = std::fmod(anom_angle + 360.0f, 360.0f);
                    float a_diff = anom_heading - current_heading;
                    while (a_diff < -180.0f) a_diff += 360.0f;
                    while (a_diff > 180.0f) a_diff -= 360.0f;

                    if (std::abs(a_diff) <= fov_range) {
                        float a_x = cx + (a_diff / fov_range) * (comp_w * 0.46f);
                        glm::vec4 a_col = (sv.material_id == MAT_VOIDITE_CRYSTAL)
                            ? glm::vec4(0.85f, 0.25f, 1.0f, 0.95f) // Voidite purple
                            : glm::vec4(0.2f, 1.0f, 0.85f, 0.95f); // Prismatic crystal
                        draw_rect(a_x - 1.5f, comp_y + 2.0f, 3.0f, comp_h - 4.0f, a_col);
                        shown_anomalies++;
                    }
                }
            }
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

    // Multi-Stage Secondary Objective readout
    if (mission && !mission->get_secondary_objective_text().empty()) {
        float sec_y = hz_y + hz_h + 3.0f * ui_scale;
        draw_text(mission->get_secondary_objective_text(), hz_x + 12.0f * ui_scale, sec_y, 0.80f * ui_scale,
                  mission->is_relic_retrieved() ? Typography::COLOR_GREEN : Typography::COLOR_AMBER);
    }

    // Subtle vertical divider line
    float div_x = hz_x + hz_w * 0.45f;   // 45% left / 55% right — prevents long objective text overflow
    draw_rect(div_x, hz_y + 5.0f * ui_scale, 1.0f, hz_h - 10.0f * ui_scale, glm::vec4(0.2f, 0.3f, 0.4f, 0.5f));

    // Right section: seismic readout — two-row layout avoids text / bar collision
    float right_x     = div_x + 10.0f * ui_scale;
    float right_end   = hz_x + hz_w - 8.0f * ui_scale;
    float right_avail = right_end - right_x;
    if (tremoring) {
        if (hazard.is_player_in_tremor_zone()) {
            draw_text("! SEISMIC TREMOR !", right_x, hz_y + (hz_h - obj_font_h) * 0.5f, 0.90f * ui_scale, Typography::COLOR_CRIMSON);
        } else {
            draw_text("! REMOTE TREMOR  !", right_x, hz_y + (hz_h - obj_font_h) * 0.5f, 0.90f * ui_scale, Typography::COLOR_AMBER);
        }
    } else if (hazard.is_warning()) {
        if (hazard.is_player_in_tremor_zone()) {
            draw_text("! FAULT RUPTURE  !", right_x, hz_y + (hz_h - obj_font_h) * 0.5f, 0.90f * ui_scale, Typography::COLOR_CRIMSON);
        } else {
            draw_text("! REMOTE RUPTURE !", right_x, hz_y + (hz_h - obj_font_h) * 0.5f, 0.90f * ui_scale, Typography::COLOR_AMBER);
        }
    } else {
        float stress = hazard.seismic_stress();
        glm::vec4 trem_col = (stress >= 75.0f) ? Typography::COLOR_CRIMSON :
                             (stress >= 40.0f) ? Typography::COLOR_AMBER :
                                                 Typography::COLOR_PRIMARY;

        // Row 1 (top): "SEISMIC" label left, percentage value right — no bar on this row
        float label_y = hz_y + 5.0f * ui_scale;
        draw_text("SEISMIC", right_x, label_y, 0.80f * ui_scale, Typography::COLOR_MUTED);
        std::string pct_str = std::to_string(static_cast<int>(stress)) + "%";
        float pct_w = FontRenderer::get_rendered_width(pct_str, 0.92f * ui_scale);
        draw_text(pct_str, right_end - pct_w, label_y, 0.92f * ui_scale, trem_col);

        // Row 2 (bottom strip): full-width progress bar — completely separate from text above
        float bar_h = std::clamp(4.0f * ui_scale, 3.0f, 6.0f);
        float bar_y = hz_y + hz_h - bar_h - 5.0f * ui_scale;
        float t_ratio = std::clamp(stress / 100.0f, 0.0f, 1.0f);
        draw_rect(right_x, bar_y, right_avail, bar_h, glm::vec4(0.10f, 0.14f, 0.18f, 0.80f));
        draw_rect(right_x, bar_y, right_avail * t_ratio, bar_h, trem_col);
    }

    // 4b. BOTTOM-CENTER ACOUSTIC PROFILE STEALTH / NOISE METER
    if (noise_meter) {
        float pct = noise_meter->noise_percent();
        bool is_crouching = noise_meter->is_crouching();
        bool has_recent_sounds = !noise_meter->recent_sounds().empty();

        if (pct > m_noise_peak) {
            m_noise_peak = pct;
            m_noise_peak_timer = 0.8f;
        }

        // Threat peak is active during 100% swarming or during enrage while noise is elevated (>= 40%).
        // Once the player stands still and the meter drains below 40% into the silent zone,
        // threat peak clears and reverts to normal acoustic profile.
        bool is_at_threat_peak = (noise_meter->alert_level() == NoiseMeter::AlertLevel::Swarming) ||
                                 (SwarmManager::instance().IsEnraged() && pct >= 40.0f);

        // Display when noise >= 5%, or crouching in stealth mode, or active acoustic disturbances present, or at threat peak.
        // Once the meter drains to 0% and player is standing still, the entire meter cleanly goes away.
        if (pct >= 5.0f || is_crouching || has_recent_sounds || is_at_threat_peak) {
            float nm_w = std::clamp(340.0f * ui_scale, 280.0f, 380.0f);
            float nm_h = std::clamp(34.0f * ui_scale, 30.0f, 38.0f);
            float nm_x = cx - nm_w * 0.5f;
            float nm_y = sh - nm_h - 18.0f * ui_scale;

            float pulse_crimson = 0.75f + 0.25f * std::sin(m_total_time * 10.0f);
            glm::vec4 enraged_crimson(1.0f, 30.0f / 255.0f, 39.0f / 255.0f, 1.0f); // #FF1E27

            // Dark semi-transparent pill container (rgba(8, 12, 18, 0.85)) with border
            draw_pill(nm_x, nm_y, nm_w, nm_h, glm::vec4(8.0f / 255.0f, 12.0f / 255.0f, 18.0f / 255.0f, 0.85f));
            glm::vec4 border_col = is_at_threat_peak ? (enraged_crimson * pulse_crimson) : glm::vec4(0.12f, 0.22f, 0.30f, 0.85f);
            draw_rect(nm_x, nm_y, nm_w, 1.0f, border_col);
            draw_rect(nm_x, nm_y + nm_h - 1.0f, nm_w, 1.0f, border_col);
            draw_rect(nm_x, nm_y, 1.0f, nm_h, border_col);
            draw_rect(nm_x + nm_w - 1.0f, nm_y, 1.0f, nm_h, border_col);

            // Header: ACOUSTIC PROFILE or THREAT PEAK
            float header_y = nm_y + 5.0f * ui_scale;
            if (is_at_threat_peak) {
                draw_text_fitted("// THREAT PEAK - SWARM BREACH //", nm_x + 12.0f * ui_scale, header_y, nm_w - 24.0f * ui_scale, 0.78f * ui_scale, enraged_crimson * pulse_crimson);
            } else {
                std::string status_str = std::to_string(static_cast<int>(pct)) + "% " + std::string(noise_meter->alert_string());
                glm::vec4 status_col = (pct <= 40.0f) ? glm::vec4(0.0f, 0.90f, 1.0f, 1.0f) :
                                       (pct <= 75.0f) ? glm::vec4(1.0f, 0.70f, 0.0f, 1.0f) :
                                                        glm::vec4(1.0f, 0.20f, 0.20f, 1.0f);
                float stat_w = FontRenderer::get_rendered_width(status_str, 0.78f * ui_scale);
                float stat_x = nm_x + nm_w - 12.0f * ui_scale - stat_w;

                float max_header_w = std::max(60.0f, (stat_x - (nm_x + 12.0f * ui_scale)) - 8.0f * ui_scale);
                if (is_crouching) {
                    draw_text_fitted("ACOUSTIC // STEALTH (DAMPENED -65%)", nm_x + 12.0f * ui_scale, header_y, max_header_w, 0.78f * ui_scale, glm::vec4(0.20f, 0.95f, 0.55f, 1.0f));
                } else {
                    draw_text_fitted("ACOUSTIC PROFILE", nm_x + 12.0f * ui_scale, header_y, max_header_w, 0.78f * ui_scale, Typography::COLOR_MUTED);
                }

                draw_text(status_str, stat_x, header_y, 0.78f * ui_scale, status_col);
            }

            // Dynamic Segmented Bar with clear color thresholds:
            // 0% - 40% (Silent / Crouched): Muted Cyan (#00E5FF)
            // 41% - 75% (Walking / Drilling): Warning Amber (#FFB300)
            // 76% - 100% (Sprinting / Blasting): Pulsing Crimson (#FF3333)
            // Enraged (Peak Threat): High-contrast pulsing crimson (#FF1E27)
            float bar_x = nm_x + 12.0f * ui_scale;
            float bar_y = nm_y + 19.0f * ui_scale;
            float bar_w = nm_w - 24.0f * ui_scale;
            float bar_h = 7.0f * ui_scale;

            constexpr int NUM_SEGMENTS = 20;
            float seg_spacing = 2.0f * ui_scale;
            float total_spacing = seg_spacing * (NUM_SEGMENTS - 1);
            float seg_w = (bar_w - total_spacing) / NUM_SEGMENTS;

            for (int i = 0; i < NUM_SEGMENTS; ++i) {
                float seg_x = bar_x + i * (seg_w + seg_spacing);
                float seg_pct = (static_cast<float>(i + 1) / NUM_SEGMENTS) * 100.0f;
                bool is_active = (pct >= seg_pct - 2.5f);

                glm::vec4 seg_col;
                if (is_at_threat_peak) {
                    seg_col = enraged_crimson * pulse_crimson; // High-contrast crimson #FF1E27
                } else if (seg_pct <= 40.0f) {
                    seg_col = glm::vec4(0.0f, 0.90f, 1.0f, 1.0f); // Muted Cyan #00E5FF
                } else if (seg_pct <= 75.0f) {
                    seg_col = glm::vec4(1.0f, 0.70f, 0.0f, 1.0f); // Warning Amber #FFB300
                } else {
                    seg_col = glm::vec4(1.0f, 0.20f, 0.20f, 1.0f) * pulse_crimson; // Pulsing Crimson #FF3333
                }

                if (is_active) {
                    draw_rect(seg_x, bar_y, seg_w, bar_h, seg_col);
                } else {
                    draw_rect(seg_x, bar_y, seg_w, bar_h, glm::vec4(0.08f, 0.12f, 0.16f, 0.65f));
                }
            }

            // Trailing peak indicator needle that lingers for 0.8s on sound spikes before decaying smoothly
            float peak_ratio = std::clamp(m_noise_peak / 100.0f, 0.0f, 1.0f);
            float needle_x = bar_x + peak_ratio * bar_w;
            draw_rect(needle_x - 1.0f, bar_y - 2.0f, 2.0f, bar_h + 4.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.95f));
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
        float crouch_badge_w = player.is_crouching() ? (FontRenderer::get_rendered_width("[CROUCH]", 0.78f * ui_scale) + 6.0f * ui_scale) : 0.0f;
        std::string hl_str = player.is_headlamp_on() ? "[* F: LIGHT ON]" : "[ F: LIGHT OFF ]";
        glm::vec4 hl_col = player.is_headlamp_on() ? Typography::COLOR_CYAN : Typography::COLOR_MUTED;
        float hl_badge_w = FontRenderer::get_rendered_width(hl_str, 0.78f * ui_scale) + 6.0f * ui_scale;

        float max_title_w = std::max(50.0f, s_w - 24.0f * ui_scale - crouch_badge_w - hl_badge_w);
        draw_text_fitted("// EXOSUIT VITALS", pad_x, s_y + 8.0f * ui_scale, max_title_w, 0.82f * ui_scale, Typography::COLOR_MUTED);

        float right_badges_x = s_x + s_w - 12.0f * ui_scale;
        if (player.is_crouching()) {
            right_badges_x -= crouch_badge_w;
            draw_text("[CROUCH]", right_badges_x + 6.0f * ui_scale, s_y + 8.0f * ui_scale, 0.78f * ui_scale, Typography::COLOR_CYAN);
        }
        right_badges_x -= hl_badge_w;
        draw_text(hl_str, right_badges_x + 6.0f * ui_scale, s_y + 8.0f * ui_scale, 0.78f * ui_scale, hl_col);

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

        // 5b. Geiger / Radiation monitor (Color-coded: Green = Safe, Yellow = Caution, Red = Danger & Damage)
        float cur_rad = exo.radiation;
        if (cur_rad > 2.0f) {
            float rad_w = s_w;
            float rad_h = std::clamp(26.0f * ui_scale, 22.0f, 30.0f);
            float rad_x = s_x;
            float rad_y = s_y - rad_h - 8.0f * ui_scale;

            bool is_danger = (cur_rad >= 50.0f);
            bool is_caution = (cur_rad >= 25.0f && !is_danger);

            float pulse = is_danger ? (0.75f + 0.25f * std::sin(m_total_time * 12.0f)) : 1.0f;
            glm::vec4 rad_col;
            std::string rad_status;

            if (is_danger) {
                rad_col = Typography::COLOR_CRIMSON * pulse;
                rad_status = "GEIGER: DANGER (" + std::to_string(static_cast<int>(cur_rad)) + "%) - TAKING DAMAGE!";
            } else if (is_caution) {
                rad_col = Typography::COLOR_AMBER;
                rad_status = "GEIGER: CAUTION (" + std::to_string(static_cast<int>(cur_rad)) + "%) - ELEVATED";
            } else {
                rad_col = Typography::COLOR_GREEN;
                rad_status = "GEIGER: SAFE (" + std::to_string(static_cast<int>(cur_rad)) + "%)";
            }

            draw_pill(rad_x, rad_y, rad_w, rad_h, rad_col * 0.40f);
            draw_rect(rad_x, rad_y, rad_w, 1.0f, rad_col * 0.8f);
            draw_rect(rad_x, rad_y + rad_h - 1.0f, rad_w, 1.0f, rad_col * 0.8f);
            draw_rect(rad_x, rad_y, 2.0f, rad_h, rad_col * 0.8f);
            draw_rect(rad_x + rad_w - 2.0f, rad_y, 2.0f, rad_h, rad_col * 0.8f);

            // Progress bar on right
            float g_bar_w = std::clamp(70.0f * ui_scale, 50.0f, 90.0f);
            float g_bar_h = 6.0f * ui_scale;
            float g_bar_x = rad_x + rad_w - g_bar_w - 8.0f * ui_scale;
            float g_bar_y = rad_y + (rad_h - g_bar_h) * 0.5f;

            draw_rect(g_bar_x, g_bar_y, g_bar_w, g_bar_h, glm::vec4(0.08f, 0.12f, 0.15f, 0.85f));
            draw_rect(g_bar_x, g_bar_y, g_bar_w * std::clamp(cur_rad / 100.0f, 0.0f, 1.0f), g_bar_h, rad_col);

            // Left text fitted
            float max_text_w = (g_bar_x - rad_x) - 14.0f * ui_scale;
            float font_h = FontRenderer::get_rendered_height(0.82f * ui_scale);
            draw_text_fitted(rad_status, rad_x + 8.0f * ui_scale, rad_y + (rad_h - font_h) * 0.5f, max_text_w, 0.82f * ui_scale, rad_col);
        }
    }

    // 6. BOTTOM-RIGHT (Delver Tactical Abilities & Brief Weapon Equip Toast)
    {
        ToolSlot current_tool = player.active_tool();
        if (!m_tool_initialized) {
            m_last_active_tool = current_tool;
            m_tool_initialized = true;
        } else if (current_tool != m_last_active_tool) {
            m_last_active_tool = current_tool;
            m_tool_switch_toast_timer = 2.4f;
        }

        if (current_tool == ToolSlot::MiningDrill) {
            m_tool_switch_name = "MINING DRILL";
            m_tool_switch_details = "[LMB] MINE VOXEL  |  [RMB] QUICK BULKHEAD";
            m_tool_switch_color = Typography::COLOR_CYAN;
        } else if (current_tool == ToolSlot::CombatWeapon) {
            m_tool_switch_name = player.weapon_name();
            m_tool_switch_details = "AMMO: " + std::to_string(player.weapon_ammo()) + " / " + std::to_string(player.weapon_max_ammo()) + "  |  [R] RELOAD";
            m_tool_switch_color = (player.weapon_archetype() == WeaponArchetype::MagmaScattergun) ? Typography::COLOR_AMBER :
                                  (player.weapon_archetype() == WeaponArchetype::NeedlerRailgun)  ? Typography::COLOR_GREEN : Typography::COLOR_CYAN;
        } else {
            m_tool_switch_name = "SATCHEL DEMOLITION CHARGE";
            m_tool_switch_details = player.has_placed_charge() ? "[RMB] DETONATE PLACED CHARGE" : ("[LMB] PLANT CHARGE (" + std::to_string(inventory.demolition_charges) + " AVAIL)  |  [RMB] DETONATE");
            m_tool_switch_color = Typography::COLOR_AMBER;
        }

        float ab_w = std::clamp(380.0f * ui_scale, 300.0f, 440.0f);
        float ab_h = std::clamp(32.0f * ui_scale, 28.0f, 36.0f);
        float ab_x = sw - ab_w - 24.0f * ui_scale;
        float ab_y = sh - ab_h - 24.0f * ui_scale;

        draw_pill(ab_x, ab_y, ab_w, ab_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.35f));
        draw_rect(ab_x + ab_w - 3.0f, ab_y, 3.0f, ab_h, Typography::COLOR_CYAN);

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
        draw_text_centered_fitted(q_str, ab_x, ab_y, half_w, ab_h, 0.92f * ui_scale, q_col);

        // Right Half: [C] Tactical Class Skill
        bool t_ready = player.is_tactical_ready();
        CharacterClass cls = player.character_class();
        std::string skill_short = (cls == CharacterClass::Demolitionist) ? "SHOCKWAVE" :
                                  (cls == CharacterClass::Vanguard) ? "REPULSOR" : "DASH";
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
        draw_text_centered_fitted(c_str, mid_x, ab_y, half_w, ab_h, 0.92f * ui_scale, c_col);

        // 6a. Throwable Chemical Flare Status: [T] FLARE: X/3
        float flare_y = ab_y - 20.0f * ui_scale;
        float flare_w = ab_w;
        float flare_h = 16.0f * ui_scale;
        std::string fl_str = "[T] CHEMICAL FLARE: " + std::to_string(player.flare_count()) + "/" + std::to_string(player.max_flares());
        if (player.flare_count() < player.max_flares()) {
            char fl_buf[32];
            std::snprintf(fl_buf, sizeof(fl_buf), " (%.0fs)", player.flare_recharge_timer());
            fl_str += fl_buf;
        }
        glm::vec4 fl_col = (player.flare_count() > 0) ? Typography::COLOR_CYAN : Typography::COLOR_AMBER;
        draw_pill(ab_x, flare_y, flare_w, flare_h, glm::vec4(0.02f, 0.04f, 0.07f, 0.85f));
        draw_text_centered_fitted(fl_str, ab_x, flare_y, flare_w, flare_h, 0.78f * ui_scale, fl_col);
        if (player.flare_count() < player.max_flares()) {
            draw_rect(ab_x + 4.0f, flare_y + flare_h - 2.0f, (flare_w - 8.0f) * player.flare_recharge_progress(), 2.0f, Typography::COLOR_CYAN * 0.7f);
        }

        // 6b. Brief Equipment Switch Toast (Appears briefly when equipping/switching weapon)
        if (m_tool_switch_toast_timer > 0.0f) {
            float toast_w = ab_w;
            float toast_h = 36.0f * ui_scale;
            float toast_x = ab_x;
            float toast_y = ab_y - toast_h - 6.0f * ui_scale;
            float alpha = std::clamp(m_tool_switch_toast_timer / 0.4f, 0.0f, 1.0f);

            glm::vec4 bg_toast = glm::vec4(0.04f, 0.07f, 0.11f, 0.92f * alpha);
            glm::vec4 border_toast = m_tool_switch_color * glm::vec4(1.0f, 1.0f, 1.0f, alpha);
            draw_pill(toast_x, toast_y, toast_w, toast_h, bg_toast);
            draw_rect(toast_x, toast_y, toast_w, 1.0f, border_toast);
            draw_rect(toast_x, toast_y + toast_h - 1.0f, toast_w, 1.0f, border_toast);
            draw_rect(toast_x, toast_y, 2.0f, toast_h, border_toast);
            draw_rect(toast_x + toast_w - 2.0f, toast_y, 2.0f, toast_h, border_toast);

            std::string title_str = "EQUIPPED // " + m_tool_switch_name;
            draw_text_fitted(title_str, toast_x + 10.0f * ui_scale, toast_y + 4.0f * ui_scale, toast_w - 20.0f * ui_scale, 0.88f * ui_scale, border_toast);
            draw_text_fitted(m_tool_switch_details, toast_x + 10.0f * ui_scale, toast_y + 19.0f * ui_scale, toast_w - 20.0f * ui_scale, 0.76f * ui_scale, glm::vec4(0.85f, 0.90f, 0.95f, 0.85f * alpha));
        }
    }

    // 7. COMPUTE UNIFIED TOP-STACK LAYOUT (Seismic Alert -> Evac Banner -> Notifications Stack)
    HudTopStackLayout top_stack = compute_top_stack_layout(ui_scale, sw, hazard.is_tremoring(), hazard.is_warning(), extraction.phase());

    // 7a. PRIMARY ENVIRONMENTAL CRISIS BANNER (Seismic Tremor / Fault Rupture)
    if (top_stack.has_seismic_banner) {
        float alert_x = top_stack.seismic_banner.x;
        float alert_y = top_stack.seismic_banner.y;
        float alert_w = top_stack.seismic_banner.w;
        float alert_h = top_stack.seismic_banner.h;

        if (hazard.is_tremoring()) {
            float pulse = 0.55f + 0.45f * std::sin(m_total_time * 14.0f);
            if (hazard.is_player_in_tremor_zone()) {
                draw_pill(alert_x, alert_y, alert_w, alert_h, glm::vec4(1.0f, 0.15f, 0.15f, 0.95f));
                draw_rect(alert_x, alert_y, alert_w, alert_h, glm::vec4(0.20f, 0.02f, 0.02f, 0.80f * pulse));
                draw_text_centered("! ! ! ACTIVE SEISMIC TREMOR IN PROGRESS ! ! !", alert_x, alert_y + 4.0f * ui_scale, alert_w, alert_h * 0.45f, 1.25f * ui_scale, glm::vec4(1.0f, 0.9f, 0.9f, 1.0f));
                draw_text_centered("LOCAL CEILING DESTABILIZED // WATCH FOR FALLING ROCKS", alert_x, alert_y + 20.0f * ui_scale, alert_w, alert_h * 0.45f, 0.95f * ui_scale, glm::vec4(1.0f, 0.70f, 0.20f, 1.0f));
            } else {
                draw_pill(alert_x, alert_y, alert_w, alert_h, glm::vec4(0.95f, 0.45f, 0.15f, 0.90f));
                draw_rect(alert_x, alert_y, alert_w, alert_h, glm::vec4(0.18f, 0.08f, 0.02f, 0.75f * pulse));
                draw_text_centered("! ! ! REMOTE SEISMIC FAULT COLLAPSE ! ! !", alert_x, alert_y + 4.0f * ui_scale, alert_w, alert_h * 0.45f, 1.25f * ui_scale, glm::vec4(1.0f, 0.9f, 0.9f, 1.0f));
                draw_text_centered("TECTONIC STRATUM SHIFT IN EXCAVATION ZONE", alert_x, alert_y + 20.0f * ui_scale, alert_w, alert_h * 0.45f, 0.95f * ui_scale, glm::vec4(1.0f, 0.80f, 0.40f, 1.0f));
            }
        } else if (hazard.is_warning()) {
            float pulse = 0.5f + 0.5f * std::sin(m_total_time * 10.0f);
            if (hazard.is_player_in_tremor_zone()) {
                glm::vec4 warn_col(1.0f, 0.75f, 0.10f, 0.75f * pulse);
                draw_pill(alert_x, alert_y, alert_w, alert_h, warn_col);
                draw_rect(alert_x, alert_y, alert_w, alert_h, glm::vec4(0.18f, 0.12f, 0.02f, 0.75f * pulse));
                draw_text_centered("[ ! ] WARNING: SEISMIC FAULT RUPTURE DETECTED [ ! ]", alert_x, alert_y + 4.0f * ui_scale, alert_w, alert_h * 0.45f, 1.15f * ui_scale, glm::vec4(1.0f, 0.95f, 0.6f, 1.0f));
                draw_text_centered("EARTHQUAKE IMMINENT -- SEEK REINFORCED SHELTER", alert_x, alert_y + 19.0f * ui_scale, alert_w, alert_h * 0.45f, 0.92f * ui_scale, Typography::COLOR_AMBER);
            } else {
                glm::vec4 warn_col(0.85f, 0.65f, 0.20f, 0.65f * pulse);
                draw_pill(alert_x, alert_y, alert_w, alert_h, warn_col);
                draw_rect(alert_x, alert_y, alert_w, alert_h, glm::vec4(0.12f, 0.09f, 0.02f, 0.65f * pulse));
                draw_text_centered("[ ! ] ADVISORY: REMOTE FAULT RUPTURE DETECTED [ ! ]", alert_x, alert_y + 4.0f * ui_scale, alert_w, alert_h * 0.45f, 1.15f * ui_scale, glm::vec4(1.0f, 0.95f, 0.7f, 1.0f));
                draw_text_centered("SUBSURFACE STRAIN IN DISTANT EXCAVATION ZONE", alert_x, alert_y + 19.0f * ui_scale, alert_w, alert_h * 0.45f, 0.92f * ui_scale, Typography::COLOR_AMBER);
            }
        }
    }

    // 7b. MISSION EXTRACTION BEACON / POD ARRIVAL BANNER
    if (top_stack.has_evac_banner) {
        float ex_x = top_stack.evac_banner.x;
        float ex_y = top_stack.evac_banner.y;
        float ex_w = top_stack.evac_banner.w;
        float ex_h = top_stack.evac_banner.h;

        if (extraction.phase() == ExtractionPhase::BeaconDeployed) {
            float pulse = extraction.siren_pulse();
            glm::vec4 banner_color = glm::mix(glm::vec4(0.85f, 0.15f, 0.1f, 0.9f), glm::vec4(1.0f, 0.4f, 0.1f, 1.0f), pulse);
            draw_pill(ex_x, ex_y, ex_w, ex_h, banner_color);
            float total_cd = std::max(1.0f, extraction.initial_countdown());
            draw_rect(ex_x, ex_y, ex_w * std::clamp(extraction.countdown() / total_cd, 0.0f, 1.0f), ex_h, banner_color * 0.35f);

            bool in_perimeter = extraction.is_player_in_perimeter(player.position());
            int remaining_sec = static_cast<int>(std::ceil(std::max(0.0f, extraction.countdown())));
            std::string evac_str = "EVAC POD ARRIVING IN: " + std::to_string(remaining_sec) + "s";
            if (in_perimeter) {
                int def_bonus = (current_level <= 1) ? 40 : (current_level == 2 ? 25 : 15);
                evac_str += "  [LZ DEFENSE +" + std::to_string(def_bonus) + "% RESIST]";
            }
            draw_text_centered(evac_str, ex_x, ex_y, ex_w, ex_h, 1.15f * ui_scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        } else if (extraction.phase() == ExtractionPhase::PodLanded) {
            draw_pill(ex_x, ex_y, ex_w, ex_h, glm::vec4(0.1f, 0.85f, 0.3f, 0.95f));
            draw_text_centered("EVACUATION POD HAS TOUCHED DOWN! EXTRACT NOW!", ex_x, ex_y, ex_w, ex_h, 1.18f * ui_scale, glm::vec4(0.2f, 1.0f, 0.4f, 1.0f));
        }
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

    // 8b. OVERHEAD ENEMY AWARENESS INDICATORS (Yellow '!' for Investigating, Red Triangle for Engaged/Attack)
    render_enemy_awareness_markers(view, proj, player.position(), world, stalkers, burrowers);

    // 8c. 3D-to-2D PRECURSOR VAULT BULKHEAD WAYPOINT DIAMOND (When within 35m)
    if (mission && mission->vault().exists && !mission->is_relic_retrieved()) {
        glm::vec3 v_world = (mission->vault().state == VaultObjectiveState::Breached)
            ? (glm::vec3(mission->vault().relic_pos) + glm::vec3(0.5f, 1.0f, 0.5f))
            : (glm::vec3(mission->vault().door_pos) + glm::vec3(0.5f, 1.5f, 0.5f));

        float dist = glm::distance(player.position(), v_world);
        if (dist <= 35.0f) {
            glm::vec4 clip = proj * view * glm::vec4(v_world, 1.0f);
            if (clip.w > 0.1f) {
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                if (ndc.z >= -1.0f && ndc.z <= 1.0f) {
                    float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(m_width);
                    float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(m_height);

                    float pulse = 0.5f + 0.5f * std::sin(m_total_time * 5.0f);
                    glm::vec4 marker_col = (mission->vault().state == VaultObjectiveState::Breached)
                        ? glm::vec4(0.0f, 0.95f, 1.0f, 1.0f) // Cyan for unlocked relic
                        : glm::mix(glm::vec4(0.0f, 0.85f, 1.0f, 1.0f), glm::vec4(1.0f, 0.75f, 0.1f, 1.0f), pulse);

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

                    std::string label = (mission->vault().state == VaultObjectiveState::Breached)
                        ? ("[!] PRECURSOR RELIC [" + std::to_string(static_cast<int>(dist)) + "m] [INTERACT TO SECURE]")
                        : ("[!] VAULT BULKHEAD [" + std::to_string(static_cast<int>(dist)) + "m] [BREACH WITH SATCHEL CHARGE]");

                    float text_w = FontRenderer::get_rendered_width(label, 1.0f * ui_scale);
                    float pill_w = text_w + 16.0f * ui_scale;
                    float pill_h = 22.0f * ui_scale;
                    draw_pill(sx - pill_w * 0.5f, sy + d_sz * 0.8f + 6.0f * ui_scale, pill_w, pill_h, marker_col * 0.6f);
                    draw_text_centered(label, sx - pill_w * 0.5f, sy + d_sz * 0.8f + 6.0f * ui_scale, pill_w, pill_h, 1.0f * ui_scale, marker_col);
                }
            }
        }
    }

    // 9. ON-SCREEN TACTICAL NOTIFICATIONS STACK (Guaranteed non-overlapping, semantic colors)
    for (size_t i = 0; i < top_stack.notifications.size() && i < m_notifications.size(); ++i) {
        const auto& notif = m_notifications[i];
        const auto& rect = top_stack.notifications[i];

        std::string display_text = (notif.count > 1) ?
            (notif.text + " [x" + std::to_string(notif.count) + "]") : notif.text;

        float alpha = std::clamp(notif.timer / 0.30f, 0.0f, 1.0f);

        glm::vec4 border_col = notif.color;
        border_col.a *= (0.95f * alpha);

        glm::vec4 bg_col(0.06f, 0.08f, 0.11f, 0.90f * alpha);
        if (notif.color.r > 0.8f && notif.color.g < 0.4f) {
            bg_col = glm::vec4(0.18f, 0.03f, 0.03f, 0.92f * alpha);
        } else if (notif.color.g > 0.8f && notif.color.r < 0.4f) {
            bg_col = glm::vec4(0.02f, 0.16f, 0.06f, 0.92f * alpha);
        }

        // Crisp semi-transparent tinted backdrop
        draw_rect(rect.x, rect.y, rect.w, rect.h, bg_col);

        // Vibrant 1px border with semantic accent
        draw_rect(rect.x, rect.y, rect.w, 1.0f, border_col);
        draw_rect(rect.x, rect.y + rect.h - 1.0f, rect.w, 1.0f, border_col);
        draw_rect(rect.x, rect.y, 1.0f, rect.h, border_col);
        draw_rect(rect.x + rect.w - 1.0f, rect.y, 1.0f, rect.h, border_col);

        // Immediate high-contrast legible text
        draw_text_centered(display_text, rect.x, rect.y, rect.w, rect.h, 1.10f * ui_scale,
                           glm::vec4(notif.color.r, notif.color.g, notif.color.b, alpha));
    }

    // 9b. SURVEYING SONAR MINERAL LABELS (Unlocked at Rank 2+)
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

    // 9c. FULL-SCREEN SEISMIC TREMOR EFFECT PERIMETER BORDER
    // Only pulses display perimeter border if the player is in or near the affected zone
    if (hazard.is_tremoring() && hazard.is_player_in_tremor_zone()) {
        float pulse = 0.55f + 0.45f * std::sin(m_total_time * 14.0f);
        float b_thick = 10.0f * ui_scale;

        // Pulsing tectonic perimeter border around entire display
        glm::vec4 tremor_col(1.0f, 0.15f, 0.10f, 0.85f * pulse);
        draw_rect(0.0f, 0.0f, sw, b_thick, tremor_col);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, tremor_col);
        draw_rect(0.0f, 0.0f, b_thick, sh, tremor_col);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, tremor_col);
    } else if (hazard.is_warning() && hazard.is_player_in_tremor_zone()) {
        float pulse = 0.5f + 0.5f * std::sin(m_total_time * 10.0f);
        float b_thick = 7.0f * ui_scale;

        glm::vec4 warn_col(1.0f, 0.75f, 0.10f, 0.75f * pulse);
        draw_rect(0.0f, 0.0f, sw, b_thick, warn_col);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, warn_col);
        draw_rect(0.0f, 0.0f, b_thick, sh, warn_col);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, warn_col);
    }

    // 10. DIRECTIONAL DAMAGE FLASH / VIGNETTE
    if (m_damage_flash_timer > 0.0f) {
        float f_alpha = glm::clamp(m_damage_flash_timer, 0.0f, 0.85f);
        float b_thick = 32.0f * ui_scale;
        glm::vec4 flash_col(1.0f, 0.12f, 0.10f, f_alpha);
        // Subtle red wash across full screen for immediate visual impact
        draw_rect(0.0f, 0.0f, sw, sh, glm::vec4(0.95f, 0.08f, 0.08f, f_alpha * 0.22f));
        // Outer intense crimson vignette border
        draw_rect(0.0f, 0.0f, sw, b_thick, flash_col);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, flash_col);
        draw_rect(0.0f, 0.0f, b_thick, sh, flash_col);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, flash_col);
        // Soft secondary inner border
        float inner_thick = b_thick * 0.6f;
        glm::vec4 inner_col(0.85f, 0.15f, 0.12f, f_alpha * 0.45f);
        draw_rect(b_thick, b_thick, sw - 2.0f * b_thick, inner_thick, inner_col);
        draw_rect(b_thick, sh - b_thick - inner_thick, sw - 2.0f * b_thick, inner_thick, inner_col);
        draw_rect(b_thick, b_thick, inner_thick, sh - 2.0f * b_thick, inner_col);
        draw_rect(sw - b_thick - inner_thick, b_thick, inner_thick, sh - 2.0f * b_thick, inner_col);
    }

    // 10b. RADIATION IONIZING VIGNETTE & STATIC (No screen shake)
    if (m_radiation_flash_timer > 0.0f) {
        float r_alpha = glm::clamp(m_radiation_flash_timer, 0.0f, 0.85f);
        float b_thick = 28.0f * ui_scale;
        glm::vec4 rad_edge(0.18f, 0.98f, 0.35f, r_alpha);
        // Soft green wash
        draw_rect(0.0f, 0.0f, sw, sh, glm::vec4(0.10f, 0.85f, 0.25f, r_alpha * 0.14f));
        // Emerald perimeter border
        draw_rect(0.0f, 0.0f, sw, b_thick, rad_edge);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, rad_edge);
        draw_rect(0.0f, 0.0f, b_thick, sh, rad_edge);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, rad_edge);
        // Ionizing static micro scanlines
        float scanline_y = std::fmod(m_total_time * 240.0f, sh);
        draw_rect(0.0f, scanline_y, sw, 3.0f * ui_scale, glm::vec4(0.30f, 1.0f, 0.50f, r_alpha * 0.40f));
    }

    // 10c. TOXIC GAS VISOR ASPHYXIATION VIGNETTE & ON-SCREEN PARTICLES
    float gas_vis = std::max(m_toxic_gas_flash_timer, m_toxic_gas_exposure);
    if (gas_vis > 0.0f) {
        float g_alpha = glm::clamp(gas_vis, 0.0f, 0.85f);
        float b_thick = 34.0f * ui_scale;
        glm::vec4 gas_edge(0.45f, 0.88f, 0.12f, g_alpha);
        // Caustic sickly yellow-green wash
        draw_rect(0.0f, 0.0f, sw, sh, glm::vec4(0.35f, 0.75f, 0.10f, g_alpha * 0.18f));
        // Toxic perimeter border
        draw_rect(0.0f, 0.0f, sw, b_thick, gas_edge);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, gas_edge);
        draw_rect(0.0f, 0.0f, b_thick, sh, gas_edge);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, gas_edge);

        // Render on-screen drifting toxic gas droplets & vapor wisps on the visor
        for (const auto& p : m_screen_toxic_particles) {
            float p_life_frac = std::clamp(p.life / p.max_life, 0.0f, 1.0f);
            float p_alpha = p.alpha * p_life_frac * g_alpha;
            draw_pill(p.x, p.y, p.size * ui_scale, p.size * 0.7f * ui_scale, glm::vec4(0.48f, 0.86f, 0.15f, p_alpha));
        }
    }

    // 10d. ON-SCREEN VISOR BLOOD SPLATTERS (Enemy Attacks Only)
    for (const auto& s : m_blood_splatters) {
        float life_frac = std::clamp(s.life / s.max_life, 0.0f, 1.0f);
        float fade = (life_frac < 0.35f) ? (life_frac / 0.35f) : 1.0f;
        float cur_alpha = s.alpha * fade;
        if (cur_alpha <= 0.01f) continue;

        // Dark coagulated visceral red border/shadow
        glm::vec4 dark_blood(0.38f, 0.01f, 0.02f, cur_alpha * 0.95f);
        // Rich arterial crimson core
        glm::vec4 core_blood(0.72f, 0.04f, 0.06f, cur_alpha);

        // Main splatter body (elongated drip pill)
        float main_w = s.size * ui_scale;
        float main_h = s.size * 0.75f * ui_scale;
        draw_pill(s.x - main_w * 0.5f, s.y - main_h * 0.5f, main_w, main_h, dark_blood);
        draw_rect(s.x - main_w * 0.3f, s.y - main_h * 0.3f, main_w * 0.6f, main_h * 0.6f, core_blood);

        // Satellite spray droplets
        for (int i = 0; i < s.droplet_count && i < 6; ++i) {
            float d_x = s.x + s.dx[i] * ui_scale;
            float d_y = s.y + s.dy[i] * ui_scale;
            float d_rad = s.radii[i] * ui_scale;
            draw_pill(d_x - d_rad, d_y - d_rad, d_rad * 2.0f, d_rad * 2.0f, dark_blood);
            draw_rect(d_x - d_rad * 0.6f, d_y - d_rad * 0.6f, d_rad * 1.2f, d_rad * 1.2f, core_blood);
        }
    }

    // 10e. INSERTION POD RECALL MATRIX SHIELD VIGNETTE
    if (player.has_insertion_shield()) {
        float s_alpha = std::clamp(player.insertion_shield_timer() / 4.0f, 0.0f, 1.0f);
        float b_thick = 24.0f * ui_scale;
        glm::vec4 shield_cyan(0.0f, 0.95f, 1.0f, s_alpha * 0.75f);
        // Low-opacity cyan wash across full screen
        draw_rect(0.0f, 0.0f, sw, sh, glm::vec4(0.0f, 0.80f, 1.0f, s_alpha * 0.12f));
        // Outer cybernetic cyan border
        draw_rect(0.0f, 0.0f, sw, b_thick, shield_cyan);
        draw_rect(0.0f, sh - b_thick, sw, b_thick, shield_cyan);
        draw_rect(0.0f, 0.0f, b_thick, sh, shield_cyan);
        draw_rect(sw - b_thick, 0.0f, b_thick, sh, shield_cyan);

        // Active Recall Matrix Banner
        std::string matrix_text = "// DROP POD RECALL MATRIX ACTIVE //";
        float mat_scale = 1.15f * ui_scale;
        float mat_w = FontRenderer::get_rendered_width(matrix_text, mat_scale);
        float mat_box_w = mat_w + 32.0f * ui_scale;
        float mat_box_h = 24.0f * ui_scale;
        float mat_box_x = cx - mat_box_w * 0.5f;
        float mat_box_y = 65.0f * ui_scale;
        draw_rect(mat_box_x, mat_box_y, mat_box_w, mat_box_h, glm::vec4(0.02f, 0.08f, 0.14f, 0.85f * s_alpha));
        draw_pill(mat_box_x, mat_box_y, mat_box_w, mat_box_h, shield_cyan);
        draw_text_centered(matrix_text, mat_box_x, mat_box_y, mat_box_w, mat_box_h, mat_scale, glm::vec4(0.0f, 0.95f, 1.0f, s_alpha));
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

    // 12. CONTRACTOR FIELD BRIEFING OVERLAY (Auto-fades linearly to 0.0 over 8s, toggleable via H / F1)
    if (m_show_help_briefing) {
        // ── 12A. FULL OPERATIONS MANUAL & CONTRACTOR FIELD BRIEFING (Centered Responsive Tactical Modal) ──
        float menu_w = std::clamp(sw * 0.78f, 620.0f * ui_scale, std::min(sw - 32.0f, 920.0f * ui_scale));
        float menu_h = std::clamp(sh * 0.76f, 440.0f * ui_scale, std::min(sh - 32.0f, 600.0f * ui_scale));
        float menu_x = (sw - menu_w) * 0.5f;
        float menu_y = (sh - menu_h) * 0.5f;

        // Dimmed cinematic backdrop wash to isolate manual text over 3D cavern
        draw_rect(0.0f, 0.0f, sw, sh, glm::vec4(0.015f, 0.025f, 0.045f, 0.82f));

        // Outer cybernetic modal shell
        draw_pill(menu_x, menu_y, menu_w, menu_h, Typography::COLOR_CYAN);
        draw_rect(menu_x + 1.0f, menu_y + 1.0f, menu_w - 2.0f, menu_h - 2.0f, glm::vec4(0.025f, 0.040f, 0.065f, 0.96f));
        // Subtle inner high-tech border lines
        draw_rect(menu_x + 3.0f, menu_y + 3.0f, menu_w - 6.0f, 1.0f, glm::vec4(0.0f, 0.85f, 1.0f, 0.25f));
        draw_rect(menu_x + 3.0f, menu_y + menu_h - 4.0f, menu_w - 6.0f, 1.0f, glm::vec4(0.0f, 0.85f, 1.0f, 0.25f));
        draw_rect(menu_x + 3.0f, menu_y + 3.0f, 1.0f, menu_h - 6.0f, glm::vec4(0.0f, 0.85f, 1.0f, 0.25f));
        draw_rect(menu_x + menu_w - 4.0f, menu_y + 3.0f, 1.0f, menu_h - 6.0f, glm::vec4(0.0f, 0.85f, 1.0f, 0.25f));

        // Header bar
        float hdr_h = 32.0f * ui_scale;
        draw_rect(menu_x, menu_y, menu_w, hdr_h, glm::vec4(0.04f, 0.10f, 0.17f, 0.98f));
        draw_rect(menu_x, menu_y + hdr_h, menu_w, 1.5f * ui_scale, Typography::COLOR_CYAN);
        float title_w = menu_w - 180.0f * ui_scale;
        draw_text_fitted("// CONTRACTOR FIELD BRIEFING & OPERATIONS MANUAL", menu_x + 14.0f * ui_scale, menu_y + 7.0f * ui_scale, title_w, 1.08f * ui_scale, Typography::COLOR_CYAN);
        draw_text_fitted("[ H / ESC: CLOSE ]", menu_x + menu_w - 150.0f * ui_scale, menu_y + 7.0f * ui_scale, 140.0f * ui_scale, 0.92f * ui_scale, Typography::COLOR_AMBER);

        // Footer prompt bar
        float ftr_h = 28.0f * ui_scale;
        float ftr_y = menu_y + menu_h - ftr_h - 6.0f * ui_scale;
        float ftr_w = menu_w - 24.0f * ui_scale;
        float ftr_x = menu_x + 12.0f * ui_scale;
        draw_pill(ftr_x, ftr_y, ftr_w, ftr_h, glm::vec4(0.0f, 0.85f, 1.0f, 0.40f));
        draw_text_centered_fitted(">> PRESS [H] OR [ESC] TO CLOSE BRIEFING & RESUME EXPEDITION <<", ftr_x, ftr_y, ftr_w, ftr_h, 0.95f * ui_scale, Typography::COLOR_CYAN);

        // Two-Column Responsive Content
        float pad_x = 16.0f * ui_scale;
        float col_gap = 18.0f * ui_scale;
        float col_w = (menu_w - pad_x * 2.0f - col_gap) * 0.5f;
        float col1_x = menu_x + pad_x;
        float col2_x = col1_x + col_w + col_gap;
        float cur_y = menu_y + hdr_h + 10.0f * ui_scale;
        float body_h = ftr_y - cur_y - 8.0f * ui_scale;

        // Column 1: Field Directives & Expedition Protocols
        draw_text("FIELD DIRECTIVES & OBJECTIVES", col1_x, cur_y, 1.04f * ui_scale, Typography::COLOR_AMBER);
        draw_rect(col1_x, cur_y + 17.0f * ui_scale, col_w, 1.0f, glm::vec4(0.2f, 0.35f, 0.5f, 0.4f));

        struct DirectiveEntry {
            std::string tag;
            std::string desc;
            glm::vec4 color;
        };
        DirectiveEntry directives[] = {
            {"1. QUOTA & EXTRACTION:", "Mine Voidite crystals with [LMB] Mining Drill until quota is achieved.", Typography::COLOR_GREEN},
            {"2. ACOUSTIC STEALTH:", "Hold [L-CTRL] to crouch; dampens decibels to evade stalking swarms.", Typography::COLOR_CYAN},
            {"3. SEISMIC HAZARDS:", "Radiation >50% corrodes suit HP. Watch ceiling during tremor cave-ins.", Typography::COLOR_CRIMSON},
            {"4. BURROWER WYRMS:", "Armored wyrms tunnel during Phase 3/4. Weak to satchel charges (2.5x).", Typography::COLOR_AMBER},
            {"5. EVAC POD DEFENSE:", "Press [B] at quota to deploy beacon. Defend LZ for 40s to extract.", Typography::COLOR_GREEN}
        };

        float d_step = (body_h - 22.0f * ui_scale) / 5.0f;
        float dy = cur_y + 24.0f * ui_scale;
        for (const auto& d : directives) {
            draw_text_fitted(d.tag, col1_x + 4.0f * ui_scale, dy, col_w - 8.0f * ui_scale, 0.95f * ui_scale, d.color);
            float desc_y = dy + 14.0f * ui_scale;
            auto wrapped = FontRenderer::wrap_text(d.desc, col_w - 12.0f * ui_scale, 0.84f * ui_scale);
            for (size_t wi = 0; wi < wrapped.size() && wi < 2; ++wi) {
                draw_text_fitted(wrapped[wi], col1_x + 8.0f * ui_scale, desc_y + wi * 12.0f * ui_scale, col_w - 16.0f * ui_scale, 0.84f * ui_scale, glm::vec4(0.85f, 0.90f, 0.95f, 0.92f));
            }
            dy += d_step;
        }

        // Column 2: Delver Controls & Avionics
        draw_text("DELVER CONTROLS & AVIONICS", col2_x, cur_y, 1.04f * ui_scale, Typography::COLOR_CYAN);
        draw_rect(col2_x, cur_y + 17.0f * ui_scale, col_w, 1.0f, glm::vec4(0.2f, 0.35f, 0.5f, 0.4f));

        struct BindingEntry {
            std::string key;
            std::string action;
            glm::vec4 color;
        };
        BindingEntry bindings[] = {
            {"[WASD]",         "Locomotion & Strafing",       Typography::COLOR_PRIMARY},
            {"[SPACE]",        "Jump / Jetpack Thruster",     Typography::COLOR_PRIMARY},
            {"[L-CTRL]",       "Crouch (Acoustic Stealth)",   Typography::COLOR_GREEN},
            {"[1-3 / MWHEEL]", "Drill / Weapon / Satchel",    Typography::COLOR_CYAN},
            {"[LMB]",          "Mine Voxel / Attack Fire",    Typography::COLOR_AMBER},
            {"[RMB]",          "Deploy Bulkhead Shelter",     Typography::COLOR_PRIMARY},
            {"[F]",            "Grappling Hook Tether",       Typography::COLOR_CYAN},
            {"[Q]",            "Seismic Sonar Pulse Scan",    Typography::COLOR_CYAN},
            {"[C]",            "Class Tactical Ability",      Typography::COLOR_AMBER},
            {"[G]",            "Chemical Flare Illumination", Typography::COLOR_CYAN},
            {"[B]",            "Deploy Evac Beacon (At Quota)", Typography::COLOR_GREEN},
            {"[H / F1]",       "Toggle This Field Manual",    Typography::COLOR_PRIMARY}
        };

        float b_step = (body_h - 22.0f * ui_scale) / 12.0f;
        float key_w = std::clamp(112.0f * ui_scale, 96.0f, 132.0f);
        float by_bind = cur_y + 24.0f * ui_scale;
        for (const auto& b : bindings) {
            draw_text_fitted(b.key, col2_x + 4.0f * ui_scale, by_bind, key_w - 4.0f * ui_scale, 0.88f * ui_scale, Typography::COLOR_CYAN);
            draw_text_fitted(b.action, col2_x + key_w + 8.0f * ui_scale, by_bind, col_w - key_w - 12.0f * ui_scale, 0.88f * ui_scale, b.color);
            by_bind += b_step;
        }
    } else if (m_briefingTimer > 0.0f) {
        // ── 12B. CORNER EXPEDITION DIRECTIVE BANNER (Auto-fades over 8s at expedition start) ──
        float briefing_alpha = glm::clamp(m_briefingTimer / 2.0f, 0.0f, 1.0f);
        if (briefing_alpha > 0.001f) {
            float card_w = std::clamp(sw * 0.36f, 380.0f * ui_scale, 500.0f * ui_scale);
            card_w = std::min(card_w, sw - 32.0f);
            float card_h = 136.0f * ui_scale;
            float card_x = sw - card_w - 20.0f * ui_scale;
            float card_y = 65.0f * ui_scale;

            draw_pill(card_x, card_y, card_w, card_h, glm::vec4(0.0f, 0.94f, 1.0f, 0.7f * briefing_alpha));
            draw_rect(card_x, card_y, card_w, 20.0f * ui_scale, glm::vec4(0.04f, 0.12f, 0.18f, 0.95f * briefing_alpha));
            glm::vec4 cyan_hdr = Typography::COLOR_CYAN;
            cyan_hdr.a *= briefing_alpha;
            float max_text_w = card_w - 20.0f * ui_scale;
            float line_x = card_x + 10.0f * ui_scale;
            draw_text_fitted("// CONTRACTOR FIELD BRIEFING // DIRECTIVE", line_x, card_y + 4.0f * ui_scale, max_text_w, 1.00f * ui_scale, cyan_hdr);

            float by = card_y + 24.0f * ui_scale;
            float b_step = 16.0f * ui_scale;
            float line_scale = 0.88f * ui_scale;

            draw_text_fitted("1. QUOTA: Mine Voidite crystals using [LMB] Drill.", line_x, by, max_text_w, line_scale, glm::vec4(0.85f, 0.95f, 1.0f, 0.95f * briefing_alpha));
            by += b_step;
            draw_text_fitted("2. STEALTH: [L-CTRL] to crouch; decays acoustic noise.", line_x, by, max_text_w, line_scale, glm::vec4(0.2f, 0.95f, 0.5f, 0.95f * briefing_alpha));
            by += b_step;
            draw_text_fitted("3. HAZARD: >50% radiation drains suit HP. Move fast!", line_x, by, max_text_w, line_scale, glm::vec4(1.0f, 0.4f, 0.3f, 0.95f * briefing_alpha));
            by += b_step;
            draw_text_fitted("4. BURROWERS: Wyrms tunnel in Phase 3/4 & holdouts.", line_x, by, max_text_w, line_scale, glm::vec4(1.0f, 0.7f, 0.1f, 0.95f * briefing_alpha));
            by += b_step;
            draw_text_fitted("5. EXTRACTION: [B] at quota deploys beacon. Survive 40s.", line_x, by, max_text_w, line_scale, glm::vec4(0.0f, 0.94f, 1.0f, 0.95f * briefing_alpha));
            by += b_step + 4.0f * ui_scale;
            draw_text_fitted(">> PRESS [H] FOR FULL FIELD MANUAL & CONTROLS <<", line_x, by, max_text_w, 0.82f * ui_scale, glm::vec4(0.6f, 0.75f, 0.9f, 0.85f * briefing_alpha));
        }
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

std::vector<EnemyAwarenessMarker> HUD::compute_awareness_markers(
    const glm::vec3& cam_pos,
    const World& world,
    const std::vector<VoidStalker>* stalkers,
    const std::vector<SeismicBurrower>* burrowers
) const {
    std::vector<EnemyAwarenessMarker> markers;

    if (stalkers) {
        for (const auto& s : *stalkers) {
            EnemyAwarenessMarkerType m_type = get_awareness_marker_type(s);
            if (m_type == EnemyAwarenessMarkerType::None) continue;

            float v_offset = s.scale * 0.85f + 0.35f;
            glm::vec3 head_pos = s.position + glm::vec3(0.0f, v_offset, 0.0f);
            float dist = glm::distance(cam_pos, head_pos);
            if (dist > 45.0f || dist < 0.3f) continue;

            glm::vec3 to_head = head_pos - cam_pos;
            glm::vec3 dir = (dist > 0.001f) ? (to_head / dist) : glm::vec3(0.0f, 1.0f, 0.0f);
            RaycastHit hit = world.raycast(cam_pos, dir, dist);
            bool has_los = (!hit.hit || hit.distance >= (dist - 0.25f));

            EnemyAwarenessMarker marker;
            marker.world_pos = head_pos;
            marker.marker_type = static_cast<int>(m_type);
            marker.state_timer = s.state_timer;
            marker.has_los = has_los;
            marker.dist = dist;
            markers.push_back(marker);
        }
    }

    if (burrowers) {
        for (const auto& b : *burrowers) {
            EnemyAwarenessMarkerType m_type = get_awareness_marker_type(b);
            if (m_type == EnemyAwarenessMarkerType::None) continue;

            float v_offset = b.scale * 1.0f + 0.5f;
            glm::vec3 head_pos = b.position + glm::vec3(0.0f, v_offset, 0.0f);
            float dist = glm::distance(cam_pos, head_pos);
            if (dist > 45.0f || dist < 0.3f) continue;

            glm::vec3 to_head = head_pos - cam_pos;
            glm::vec3 dir = (dist > 0.001f) ? (to_head / dist) : glm::vec3(0.0f, 1.0f, 0.0f);
            RaycastHit hit = world.raycast(cam_pos, dir, dist);
            bool has_los = (!hit.hit || hit.distance >= (dist - 0.25f));

            EnemyAwarenessMarker marker;
            marker.world_pos = head_pos;
            marker.marker_type = static_cast<int>(m_type);
            marker.state_timer = b.state_timer;
            marker.has_los = has_los;
            marker.dist = dist;
            markers.push_back(marker);
        }
    }

    return markers;
}

void HUD::render_enemy_awareness_markers(
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::vec3& cam_pos,
    const World& world,
    const std::vector<VoidStalker>* stalkers,
    const std::vector<SeismicBurrower>* burrowers
) {
    auto markers = compute_awareness_markers(cam_pos, world, stalkers, burrowers);
    if (markers.empty()) return;

    float ui_scale = UIUtils::compute_ui_scale(m_width, m_height);
    float screen_w = static_cast<float>(m_width);
    float screen_h = static_cast<float>(m_height);

    for (const auto& marker : markers) {
        glm::vec4 clip = proj * view * glm::vec4(marker.world_pos, 1.0f);
        if (clip.w <= 0.1f) continue;

        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) continue;
        if (ndc.x < -1.15f || ndc.x > 1.15f || ndc.y < -1.15f || ndc.y > 1.15f) continue;

        float sx = (ndc.x * 0.5f + 0.5f) * screen_w;
        float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * screen_h;

        float alpha = marker.has_los ? 1.0f : 0.50f;
        float dist_scale = std::clamp(16.0f / std::max(marker.dist, 5.0f), 0.70f, 1.25f);

        float pop_scale = 1.0f;
        if (marker.state_timer < 0.25f) {
            float pop_t = 1.0f - (marker.state_timer / 0.25f);
            pop_scale += pop_t * 0.40f;
        }

        if (marker.marker_type == 2) { // RedTriangle (Engaged / Attack Mode)
            float combat_pulse = 1.0f + 0.12f * std::sin(m_total_time * 12.0f);
            float tri_size = 28.0f * ui_scale * dist_scale * pop_scale * combat_pulse;
            float half_w = tri_size * 0.55f;
            float half_h = tri_size * 0.50f;

            float top_y = sy - half_h;
            float bot_y = sy + half_h;

            // Outer crimson triangle pointing down to enemy head
            glm::vec4 red_col(1.0f, 0.14f, 0.14f, alpha);
            draw_triangle(
                sx - half_w, top_y,
                sx + half_w, top_y,
                sx, bot_y,
                red_col
            );

            // Inset dark crimson triangle
            float inset = 2.5f * ui_scale * dist_scale;
            glm::vec4 dark_red(0.18f, 0.02f, 0.02f, 0.92f * alpha);
            draw_triangle(
                sx - half_w + inset * 1.2f, top_y + inset,
                sx + half_w - inset * 1.2f, top_y + inset,
                sx, bot_y - inset * 1.4f,
                dark_red
            );

            // Centered sharp exclamation mark inside triangle
            float text_scale = 1.25f * ui_scale * dist_scale * pop_scale;
            draw_text_centered("!", sx - half_w, top_y + 1.0f * ui_scale, half_w * 2.0f, half_h * 1.3f, text_scale, glm::vec4(1.0f, 0.45f, 0.45f, alpha));
        } else if (marker.marker_type == 1) { // YellowExclamation (Investigating / Alerted)
            float bob = std::sin(m_total_time * 6.0f) * (2.5f * ui_scale);
            float cur_sy = sy + bob;

            float badge_w = 26.0f * ui_scale * dist_scale * pop_scale;
            float badge_h = 26.0f * ui_scale * dist_scale * pop_scale;
            float bx = sx - badge_w * 0.5f;
            float by = cur_sy - badge_h * 0.5f;

            glm::vec4 yellow_col(1.0f, 0.82f, 0.08f, alpha);
            glm::vec4 bg_col(0.06f, 0.08f, 0.11f, 0.92f * alpha);

            // 1. Dark pill backdrop
            draw_rect(bx, by, badge_w, badge_h, bg_col);

            // 2. Glowing yellow border
            float b_thick = 1.5f * ui_scale;
            draw_rect(bx, by, badge_w, b_thick, yellow_col);
            draw_rect(bx, by + badge_h - b_thick, badge_w, b_thick, yellow_col);
            draw_rect(bx, by, b_thick, badge_h, yellow_col);
            draw_rect(bx + badge_w - b_thick, by, b_thick, badge_h, yellow_col);

            // 3. Downward indicator caret pointing to enemy head
            float caret_w = 4.0f * ui_scale * dist_scale;
            float caret_h = 4.0f * ui_scale * dist_scale;
            draw_triangle(
                sx - caret_w, by + badge_h,
                sx + caret_w, by + badge_h,
                sx, by + badge_h + caret_h,
                yellow_col
            );

            // 4. Vibrant Yellow "!"
            float text_scale = 1.30f * ui_scale * dist_scale * pop_scale;
            draw_text_centered("!", bx, by - 1.0f * ui_scale, badge_w, badge_h, text_scale, glm::vec4(1.0f, 0.90f, 0.15f, alpha));
        }
    }
}

void HUD::SetContractorBriefing(bool active, float duration) {
    if (active) {
        m_briefingTimer = duration;
        m_briefing_auto_timer = duration;
    } else {
        m_briefingTimer = 0.0f;
        m_briefing_auto_timer = 0.0f;
        m_show_help_briefing = false;
    }
}

void HUD::toggle_help_briefing() {
    if (m_show_help_briefing) {
        m_show_help_briefing = false;
        m_briefingTimer = 0.0f;
        m_briefing_auto_timer = 0.0f;
    } else {
        m_show_help_briefing = true;
        m_briefingTimer = 0.0f;
        m_briefing_auto_timer = 0.0f;
    }
}

void HUD::dismiss_help_briefing() {
    m_show_help_briefing = false;
    m_briefingTimer = 0.0f;
    m_briefing_auto_timer = 0.0f;
}

} // namespace Voidfall
