#include "hud.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Voidfall {

HUD::HUD(int screen_width, int height)
    : m_width(screen_width)
    , m_height(height)
{
    m_ui_shader.load_graphics("assets/shaders/ui.vert", "assets/shaders/ui.frag");
    init_gl();
}

HUD::~HUD() {
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
}

void HUD::init_gl() {
    float unit_quad[] = {
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,

        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(unit_quad), unit_quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

void HUD::resize(int width, int height) {
    m_width = width;
    m_height = height;
}

void HUD::draw_rect(float x, float y, float w, float h, const glm::vec4& color) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);
    m_ui_shader.set_mat4("uProjection", proj * model);
    m_ui_shader.set_vec4("uColor", color);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void HUD::render(
    const PlayerController& player,
    const HazardClock& hazard,
    const ExtractionSystem& extraction
) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_ui_shader.use();

    float cx = static_cast<float>(m_width) / 2.0f;
    float cy = static_cast<float>(m_height) / 2.0f;

    // 1. Crosshair
    draw_rect(cx - 1.0f, cy - 8.0f, 2.0f, 16.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.75f));
    draw_rect(cx - 8.0f, cy - 1.0f, 16.0f, 2.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.75f));

    // 2. Exo-Suit Status (Bottom-Left)
    float base_x = 40.0f;
    float base_y = static_cast<float>(m_height) - 120.0f;

    // Background panel
    draw_rect(base_x - 10.0f, base_y - 10.0f, 240.0f, 85.0f, glm::vec4(0.05f, 0.07f, 0.10f, 0.65f));

    const auto& exo = player.exo();

    // Integrity Bar (Green)
    draw_rect(base_x, base_y, 220.0f, 14.0f, glm::vec4(0.15f, 0.20f, 0.15f, 0.8f));
    draw_rect(base_x, base_y, 220.0f * (exo.integrity / 100.0f), 14.0f, glm::vec4(0.2f, 0.85f, 0.3f, 0.95f));

    // Power Bar (Cyan)
    draw_rect(base_x, base_y + 22.0f, 220.0f, 14.0f, glm::vec4(0.10f, 0.20f, 0.25f, 0.8f));
    draw_rect(base_x, base_y + 22.0f, 220.0f * (exo.power / 100.0f), 14.0f, glm::vec4(0.1f, 0.75f, 0.95f, 0.95f));

    // Heat Bar (Orange / Red when overheated)
    draw_rect(base_x, base_y + 44.0f, 220.0f, 14.0f, glm::vec4(0.25f, 0.15f, 0.10f, 0.8f));
    glm::vec4 heat_color = exo.overheated ? glm::vec4(1.0f, 0.15f, 0.15f, 1.0f) : glm::vec4(1.0f, 0.55f, 0.1f, 0.95f);
    draw_rect(base_x, base_y + 44.0f, 220.0f * (exo.heat / 100.0f), 14.0f, heat_color);

    // 3. Radiation Hazard Meter (Top-Right)
    float rad_x = static_cast<float>(m_width) - 260.0f;
    float rad_y = 35.0f;
    draw_rect(rad_x - 10.0f, rad_y - 10.0f, 240.0f, 40.0f, glm::vec4(0.08f, 0.05f, 0.05f, 0.7f));
    draw_rect(rad_x, rad_y, 220.0f, 16.0f, glm::vec4(0.25f, 0.25f, 0.10f, 0.8f));
    draw_rect(rad_x, rad_y, 220.0f * (hazard.radiation_level() / 100.0f), 16.0f, glm::vec4(0.9f, 0.85f, 0.15f, 0.95f));

    // 4. Grapple Indicator (Bottom-Right)
    float gr_x = static_cast<float>(m_width) - 180.0f;
    float gr_y = static_cast<float>(m_height) - 60.0f;
    glm::vec4 gr_color = player.grapple().active ? glm::vec4(0.1f, 0.95f, 0.6f, 0.9f) : glm::vec4(0.4f, 0.4f, 0.4f, 0.5f);
    draw_rect(gr_x, gr_y, 140.0f, 24.0f, gr_color);

    // 5. Extraction Beacon Banner (Top-Center)
    if (extraction.phase() == ExtractionPhase::BeaconDeployed) {
        float ex_w = 320.0f;
        float ex_h = 36.0f;
        float ex_x = cx - ex_w / 2.0f;
        float ex_y = 25.0f;

        float pulse = extraction.siren_pulse();
        glm::vec4 banner_color = glm::mix(glm::vec4(0.85f, 0.15f, 0.1f, 0.9f), glm::vec4(1.0f, 0.4f, 0.1f, 1.0f), pulse);
        draw_rect(ex_x - 5.0f, ex_y - 5.0f, ex_w + 10.0f, ex_h + 10.0f, glm::vec4(0.1f, 0.02f, 0.02f, 0.85f));
        draw_rect(ex_x, ex_y, ex_w * (extraction.countdown() / 90.0f), ex_h, banner_color);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

} // namespace Voidfall
