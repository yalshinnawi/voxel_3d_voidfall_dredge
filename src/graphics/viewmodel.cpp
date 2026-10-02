#include "viewmodel.hpp"
#include "../core/logger.hpp"
#include <glad/glad.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace Voidfall {

ViewModel::ViewModel() {
    bool ok = m_shader.load_graphics(
        "assets/shaders/viewmodel.vert",
        "assets/shaders/viewmodel.frag"
    );
    if (!ok) {
        VF_LOG_ERROR("ViewModel", "Failed to compile viewmodel shaders!");
    } else {
        VF_LOG_INFO("ViewModel", "Viewmodel shader compiled successfully.");
    }

    init_geometry();
}

ViewModel::~ViewModel() {
    if (m_chassis_vao) glDeleteVertexArrays(1, &m_chassis_vao);
    if (m_chassis_vbo) glDeleteBuffers(1, &m_chassis_vbo);
    if (m_bit_vao) glDeleteVertexArrays(1, &m_bit_vao);
    if (m_bit_vbo) glDeleteBuffers(1, &m_bit_vbo);
    if (m_piston_vao) glDeleteVertexArrays(1, &m_piston_vao);
    if (m_piston_vbo) glDeleteBuffers(1, &m_piston_vbo);
}

void ViewModel::on_tool_switched() {
    m_switch_timer = 0.20f;
}

void ViewModel::add_box(std::vector<ViewmodelVertex>& verts, const glm::vec3& min_p, const glm::vec3& max_p, const glm::vec4& color) {
    // 6 faces * 2 triangles * 3 vertices = 36 vertices
    glm::vec3 p0 = min_p;
    glm::vec3 p1 = max_p;

    auto push_quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& n, const glm::vec4& col) {
        verts.push_back({a, n, col});
        verts.push_back({b, n, col});
        verts.push_back({c, n, col});
        verts.push_back({a, n, col});
        verts.push_back({c, n, col});
        verts.push_back({d, n, col});
    };

    // Front (+Z)
    push_quad({p0.x, p0.y, p1.z}, {p1.x, p0.y, p1.z}, {p1.x, p1.y, p1.z}, {p0.x, p1.y, p1.z}, {0, 0, 1}, color);
    // Back (-Z)
    push_quad({p1.x, p0.y, p0.z}, {p0.x, p0.y, p0.z}, {p0.x, p1.y, p0.z}, {p1.x, p1.y, p0.z}, {0, 0, -1}, color);
    // Right (+X)
    push_quad({p1.x, p0.y, p1.z}, {p1.x, p0.y, p0.z}, {p1.x, p1.y, p0.z}, {p1.x, p1.y, p1.z}, {1, 0, 0}, color);
    // Left (-X)
    push_quad({p0.x, p0.y, p0.z}, {p0.x, p0.y, p1.z}, {p0.x, p1.y, p1.z}, {p0.x, p1.y, p0.z}, {-1, 0, 0}, color);
    // Top (+Y)
    push_quad({p0.x, p1.y, p1.z}, {p1.x, p1.y, p1.z}, {p1.x, p1.y, p0.z}, {p0.x, p1.y, p0.z}, {0, 1, 0}, color);
    // Bottom (-Y)
    push_quad({p0.x, p0.y, p0.z}, {p1.x, p0.y, p0.z}, {p1.x, p0.y, p1.z}, {p0.x, p0.y, p1.z}, {0, -1, 0}, color);
}

void ViewModel::add_cylinder(std::vector<ViewmodelVertex>& verts, const glm::vec3& base, float radius, float length, int segments, const glm::vec4& color, int axis) {
    // axis: 0=X, 1=Y, 2=Z (extends down -Z by default for axis=2)
    float step = 2.0f * 3.14159265f / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float c2 = std::cos(a2), s2 = std::sin(a2);

        glm::vec3 p1, p2, p3, p4, n1, n2;
        if (axis == 2) { // Z axis, extending towards -Z
            p1 = base + glm::vec3(radius * c1, radius * s1, 0.0f);
            p2 = base + glm::vec3(radius * c2, radius * s2, 0.0f);
            p3 = base + glm::vec3(radius * c2, radius * s2, -length);
            p4 = base + glm::vec3(radius * c1, radius * s1, -length);
            n1 = glm::vec3(c1, s1, 0.0f);
            n2 = glm::vec3(c2, s2, 0.0f);
        } else if (axis == 1) { // Y axis
            p1 = base + glm::vec3(radius * c1, 0.0f, radius * s1);
            p2 = base + glm::vec3(radius * c2, 0.0f, radius * s2);
            p3 = base + glm::vec3(radius * c2, length, radius * s2);
            p4 = base + glm::vec3(radius * c1, length, radius * s1);
            n1 = glm::vec3(c1, 0.0f, s1);
            n2 = glm::vec3(c2, 0.0f, s2);
        } else { // X axis
            p1 = base + glm::vec3(0.0f, radius * c1, radius * s1);
            p2 = base + glm::vec3(0.0f, radius * c2, radius * s2);
            p3 = base + glm::vec3(length, radius * c2, radius * s2);
            p4 = base + glm::vec3(length, radius * c1, radius * s1);
            n1 = glm::vec3(0.0f, c1, s1);
            n2 = glm::vec3(0.0f, c2, s2);
        }

        verts.push_back({p1, n1, color});
        verts.push_back({p2, n2, color});
        verts.push_back({p3, n2, color});
        verts.push_back({p1, n1, color});
        verts.push_back({p3, n2, color});
        verts.push_back({p4, n1, color});
    }
}

void ViewModel::add_cone(std::vector<ViewmodelVertex>& verts, const glm::vec3& base, float radius, float length, int segments, const glm::vec4& color) {
    // Cone pointing along -Z (drill bit tip)
    float step = 2.0f * 3.14159265f / static_cast<float>(segments);
    glm::vec3 tip = base + glm::vec3(0.0f, 0.0f, -length);

    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float c2 = std::cos(a2), s2 = std::sin(a2);

        glm::vec3 p1 = base + glm::vec3(radius * c1, radius * s1, 0.0f);
        glm::vec3 p2 = base + glm::vec3(radius * c2, radius * s2, 0.0f);

        glm::vec3 normal = glm::normalize(glm::cross(p2 - tip, p1 - tip));
        // Fluted drill bit teeth color alternation
        glm::vec4 flute_col = (i % 2 == 0) ? color : glm::vec4(color.r * 0.75f, color.g * 0.75f, color.b * 0.75f, color.a);

        verts.push_back({p1, normal, flute_col});
        verts.push_back({p2, normal, flute_col});
        verts.push_back({tip, normal, flute_col});
    }
}

void ViewModel::set_character_class(CharacterClass cls) {
    if (m_character_class != cls) {
        m_character_class = cls;
        init_geometry();
    }
}

void ViewModel::init_geometry() {
    if (m_chassis_vao) { glDeleteVertexArrays(1, &m_chassis_vao); m_chassis_vao = 0; }
    if (m_chassis_vbo) { glDeleteBuffers(1, &m_chassis_vbo); m_chassis_vbo = 0; }
    if (m_bit_vao) { glDeleteVertexArrays(1, &m_bit_vao); m_bit_vao = 0; }
    if (m_bit_vbo) { glDeleteBuffers(1, &m_bit_vbo); m_bit_vbo = 0; }
    if (m_piston_vao) { glDeleteVertexArrays(1, &m_piston_vao); m_piston_vao = 0; }
    if (m_piston_vbo) { glDeleteBuffers(1, &m_piston_vbo); m_piston_vbo = 0; }

    // -------------------------------------------------------------
    // 1. CHASSIS + HANDS / FOREARMS + RIG
    // -------------------------------------------------------------
    std::vector<ViewmodelVertex> chassis_verts;

    // Archetype dynamic palette:
    CharacterAttributes char_attr = get_character_attributes(m_character_class);
    glm::vec4 col_suit_arm = char_attr.suitSleeveColor;
    glm::vec4 col_suit_glove = char_attr.gloveColor;
    glm::vec4 col_suit_accent = char_attr.primaryAccentColor;
    glm::vec4 col_chassis_dark(0.13f, 0.14f, 0.16f, 1.0f); // Heavy pneumatic casing
    glm::vec4 col_chassis_body(0.22f, 0.24f, 0.28f, 1.0f); // Industrial chassis
    glm::vec4 col_chassis_metal(0.48f, 0.52f, 0.56f, 1.0f);// Machined alloy collar
    glm::vec4 col_hazard_black(0.08f, 0.08f, 0.09f, 1.0f); // Striping black
    glm::vec4 col_status_led = (m_character_class == CharacterClass::Scout) ? glm::vec4(0.0f, 0.95f, 1.0f, 1.0f) :
                               (m_character_class == CharacterClass::Demolitionist) ? glm::vec4(1.0f, 0.45f, 0.05f, 1.0f) :
                               glm::vec4(0.95f, 0.85f, 0.15f, 1.0f);

    // Right Forearm & Hand (extending from lower-right screen space at (0.28, -0.24, -0.45))
    add_box(chassis_verts, {0.22f, -0.38f, -0.20f}, {0.34f, -0.10f, -0.45f}, col_suit_arm);
    // Right Glove (holding rear grip)
    add_box(chassis_verts, {0.18f, -0.22f, -0.25f}, {0.30f, -0.06f, -0.35f}, col_suit_glove);
    add_box(chassis_verts, {0.17f, -0.20f, -0.35f}, {0.21f, -0.08f, -0.28f}, col_suit_accent); // Cuff ring

    // Left Forearm (reaching from lower-left to side stabilizer bar)
    add_box(chassis_verts, {-0.30f, -0.42f, 0.05f}, {-0.14f, -0.18f, -0.30f}, col_suit_arm);
    // Left Glove (gripping side stabilization handle)
    add_box(chassis_verts, {-0.16f, -0.14f, -0.38f}, {-0.04f, -0.02f, -0.48f}, col_suit_glove);

    // Drill Rear Grip & Trigger Housing
    add_box(chassis_verts, {0.19f, -0.20f, -0.20f}, {0.25f, -0.05f, -0.34f}, col_chassis_dark);
    add_box(chassis_verts, {0.19f, -0.12f, -0.34f}, {0.25f, -0.06f, -0.38f}, col_suit_accent); // Trigger

    // Main Industrial Drill Body (Center-Right in view)
    add_box(chassis_verts, {0.02f, -0.16f, -0.35f}, {0.22f, 0.06f, -0.65f}, col_chassis_body);

    // Side Stabilizer Handle (left side of drill)
    add_cylinder(chassis_verts, {0.02f, -0.06f, -0.43f}, 0.022f, 0.16f, 10, col_chassis_dark, 0); // X-axis handle

    // Top Air Intake / Exhaust Heatsink
    add_box(chassis_verts, {0.05f, 0.06f, -0.42f}, {0.19f, 0.11f, -0.62f}, col_chassis_dark);

    // Hazard Stripes on Drill Top Cover (alternating yellow and black slashes)
    float stripe_z = -0.44f;
    for (int s = 0; s < 4; ++s) {
        glm::vec4 scol = (s % 2 == 0) ? col_suit_accent : col_hazard_black;
        add_box(chassis_verts, {0.048f, 0.062f, stripe_z - 0.038f}, {0.192f, 0.112f, stripe_z}, scol);
        stripe_z -= 0.042f;
    }

    // Status LED Panel (Tachometer / Pressure Indicator)
    add_box(chassis_verts, {0.10f, 0.07f, -0.38f}, {0.18f, 0.095f, -0.36f}, col_status_led);

    // Front Heavy Chuck Collar (holding the drill spindle)
    add_cylinder(chassis_verts, {0.12f, -0.05f, -0.65f}, 0.075f, 0.08f, 16, col_chassis_metal, 2);
    // Spindle Hub ring
    add_cylinder(chassis_verts, {0.12f, -0.05f, -0.73f}, 0.052f, 0.04f, 16, col_chassis_dark, 2);

    m_chassis_count = chassis_verts.size();

    glGenVertexArrays(1, &m_chassis_vao);
    glGenBuffers(1, &m_chassis_vbo);
    glBindVertexArray(m_chassis_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_chassis_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_chassis_count * sizeof(ViewmodelVertex), chassis_verts.data(), GL_STATIC_DRAW);

    // aPos (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, pos));
    // aNormal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, normal));
    // aColor (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, color));
    glBindVertexArray(0);

    // -------------------------------------------------------------
    // 2. RECIPROCATING DUAL PNEUMATIC PISTONS
    // -------------------------------------------------------------
    std::vector<ViewmodelVertex> piston_verts;
    glm::vec4 col_piston_chrome(0.78f, 0.82f, 0.88f, 1.0f);
    glm::vec4 col_piston_sleeve(0.18f, 0.19f, 0.21f, 1.0f);

    // Left hydraulic cylinder
    add_cylinder(piston_verts, {0.04f, 0.02f, -0.40f}, 0.018f, 0.22f, 12, col_piston_sleeve, 2);
    add_cylinder(piston_verts, {0.04f, 0.02f, -0.58f}, 0.012f, 0.15f, 12, col_piston_chrome, 2);

    // Right hydraulic cylinder
    add_cylinder(piston_verts, {0.20f, 0.02f, -0.40f}, 0.018f, 0.22f, 12, col_piston_sleeve, 2);
    add_cylinder(piston_verts, {0.20f, 0.02f, -0.58f}, 0.012f, 0.15f, 12, col_piston_chrome, 2);

    m_piston_count = piston_verts.size();

    glGenVertexArrays(1, &m_piston_vao);
    glGenBuffers(1, &m_piston_vbo);
    glBindVertexArray(m_piston_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_piston_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_piston_count * sizeof(ViewmodelVertex), piston_verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, color));
    glBindVertexArray(0);

    // -------------------------------------------------------------
    // 3. ROTATING FLUTED DRILL BIT
    // (Centered at local origin (0,0,0) so it spins cleanly around Z)
    // -------------------------------------------------------------
    std::vector<ViewmodelVertex> bit_verts;
    glm::vec4 col_bit_tungsten(0.68f, 0.72f, 0.76f, 1.0f);
    glm::vec4 col_bit_flute(0.40f, 0.44f, 0.48f, 1.0f);
    glm::vec4 col_bit_tip(0.90f, 0.94f, 1.0f, 1.0f);

    // Drill Bit Base Shaft
    add_cylinder(bit_verts, {0.0f, 0.0f, 0.0f}, 0.048f, 0.08f, 16, col_bit_tungsten, 2);

    // Stepped Mid-Section with spiral grooves
    add_cylinder(bit_verts, {0.0f, 0.0f, -0.08f}, 0.042f, 0.12f, 16, col_bit_flute, 2);

    // Tapered Fluted Cutting Head
    add_cone(bit_verts, {0.0f, 0.0f, -0.20f}, 0.042f, 0.18f, 12, col_bit_tungsten);

    // Diamond Reinforced Spindle Tip
    add_cone(bit_verts, {0.0f, 0.0f, -0.35f}, 0.015f, 0.07f, 8, col_bit_tip);

    m_bit_count = bit_verts.size();

    glGenVertexArrays(1, &m_bit_vao);
    glGenBuffers(1, &m_bit_vbo);
    glBindVertexArray(m_bit_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_bit_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_bit_count * sizeof(ViewmodelVertex), bit_verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, color));
    glBindVertexArray(0);
}

void ViewModel::render(
    float dt,
    float aspect,
    bool is_drilling,
    bool is_in_range,
    ToolSlot active_tool,
    const glm::vec3& drill_target_pos
) {
    if (m_chassis_vao == 0) return;

    m_total_time += dt;

    // Detect tool switch
    if (active_tool != m_last_tool) {
        on_tool_switched();
        m_last_tool = active_tool;
    }

    // Update switch transition timer
    float switch_dip_y = 0.0f;
    if (m_switch_timer > 0.0f) {
        m_switch_timer -= dt;
        float progress = 1.0f - (std::max(0.0f, m_switch_timer) / 0.20f);
        switch_dip_y = -0.16f * std::sin(progress * 3.14159265f);
    }

    // 1. Dedicated Projection Matrix (FOV = 68.0f, near = 0.05f, far = 10.0f) to prevent cavern wall clipping
    glm::mat4 proj = glm::perspective(glm::radians(68.0f), aspect, 0.05f, 10.0f);
    glm::mat4 view = glm::mat4(1.0f); // Screen / Camera Space

    // 2. Idle Lissajous curve breathing sway: dx = sin(t * 1.8) * 0.008, dy = cos(t * 3.6) * 0.006
    float lissajous_x = std::sin(m_total_time * 1.8f) * 0.008f;
    float lissajous_y = std::cos(m_total_time * 3.6f) * 0.006f;

    // 3. Active Drilling state: rotation, stochastic recoil jitter, spark particle triggers
    float recoil_z = 0.0f;
    float piston_z = 0.0f;
    float jitter_x = 0.0f;
    float jitter_y = 0.0f;

    if (is_drilling) {
        // Continuously spin drill bit (1500 deg/s scaled by drillSpeedTier)
        float rot_speed = 1500.0f * (1.0f + 0.15f * m_drill_speed_tier);
        m_drill_rotation += rot_speed * dt;
        if (m_drill_rotation > 360000.0f) m_drill_rotation -= 360000.0f;

        // Rapid stochastic backward recoil jitter along view vector: ((rand() % 100) / 1000.0f - 0.05f) * 0.4f
        recoil_z = std::sin(m_total_time * 60.0f) * 0.02f + ((static_cast<float>(rand() % 100) / 1000.0f) - 0.05f) * 0.4f;
        jitter_x = ((static_cast<float>(rand() % 100) / 100.0f) - 0.5f) * 0.006f;
        jitter_y = ((static_cast<float>(rand() % 100) / 100.0f) - 0.5f) * 0.006f;

        // Reciprocating pistons (counter-phase oscillation along Z)
        piston_z = std::sin(m_total_time * 50.0f) * 0.035f;

        // Drill tip spark emission when in range (<= 4.5m)
        if (is_in_range && m_on_spark) {
            m_spark_timer += dt;
            if (m_spark_timer >= 0.045f) {
                m_spark_timer = 0.0f;
                // Emit spark towards player / impact site
                glm::vec3 spark_origin = drill_target_pos;
                glm::vec3 spark_dir(
                    ((rand() % 100) / 50.0f - 1.0f) * 1.5f,
                    ((rand() % 100) / 50.0f) * 2.0f + 0.5f,
                    ((rand() % 100) / 50.0f - 1.0f) * 1.5f
                );
                m_on_spark(spark_origin, spark_dir);
            }
        }
    } else {
        m_spark_timer = 0.0f;
    }

    // Dedicated depth pass with glDepthRange(0.0f, 0.15f) so viewmodel never clips cavern walls
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0.0f, 0.15f);

    m_shader.use();
    m_shader.set_mat4("uProjection", proj);
    m_shader.set_mat4("uView", view);

    // Base root transform: positioned slightly right and down in first-person camera space
    glm::vec3 base_pos(
        0.04f + lissajous_x + jitter_x,
        -0.12f + lissajous_y + switch_dip_y + jitter_y,
        -0.45f + recoil_z
    );

    glm::mat4 root_model = glm::translate(glm::mat4(1.0f), base_pos);

    // A. Render Chassis and Arms
    m_shader.set_mat4("uModel", root_model);
    m_shader.set_float("uEmissive", is_drilling ? (0.35f + 0.08f * m_drill_speed_tier) : 0.15f);

    glBindVertexArray(m_chassis_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_chassis_count));

    // B. Render Reciprocating Pistons
    glm::mat4 piston_model = glm::translate(root_model, glm::vec3(0.0f, 0.0f, piston_z));
    m_shader.set_mat4("uModel", piston_model);
    glBindVertexArray(m_piston_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_piston_count));

    // C. Render Rotating Fluted Drill Bit
    // Bit spindle origin relative to chassis is at (0.12f, -0.05f, -0.77f)
    glm::vec3 bit_origin(0.12f, -0.05f, -0.77f);
    glm::mat4 bit_model = root_model;
    bit_model = glm::translate(bit_model, bit_origin);
    bit_model = glm::rotate(bit_model, glm::radians(m_drill_rotation), glm::vec3(0.0f, 0.0f, 1.0f));

    m_shader.set_mat4("uModel", bit_model);
    float bit_emissive = is_drilling ? (0.65f + 0.12f * m_drill_speed_tier) : (0.10f + 0.06f * m_drill_speed_tier);
    m_shader.set_float("uEmissive", bit_emissive); // Incandescent cutting shine boosted by tier
    glBindVertexArray(m_bit_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_bit_count));

    glBindVertexArray(0);
    glDepthRange(0.0f, 1.0f);
}

} // namespace Voidfall
