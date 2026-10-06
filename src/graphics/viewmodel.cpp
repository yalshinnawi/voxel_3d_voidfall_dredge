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
        VF_LOG_INFO("ViewModel", "Viewmodel PBR shaders compiled successfully.");
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
    if (m_carbine_vao) glDeleteVertexArrays(1, &m_carbine_vao);
    if (m_carbine_vbo) glDeleteBuffers(1, &m_carbine_vbo);
    if (m_scattergun_vao) glDeleteVertexArrays(1, &m_scattergun_vao);
    if (m_scattergun_vbo) glDeleteBuffers(1, &m_scattergun_vbo);
    if (m_railgun_vao) glDeleteVertexArrays(1, &m_railgun_vao);
    if (m_railgun_vbo) glDeleteBuffers(1, &m_railgun_vbo);
    if (m_detonator_vao) glDeleteVertexArrays(1, &m_detonator_vao);
    if (m_detonator_vbo) glDeleteBuffers(1, &m_detonator_vbo);
    if (m_plunger_vao) glDeleteVertexArrays(1, &m_plunger_vao);
    if (m_plunger_vbo) glDeleteBuffers(1, &m_plunger_vbo);
}

void ViewModel::on_tool_switched() {
    m_switch_timer = 0.20f;  // 0.2s downward dip and raise transition
}

void ViewModel::add_box(
    std::vector<ViewmodelVertex>& verts,
    const glm::vec3& min_p,
    const glm::vec3& max_p,
    const glm::vec4& color,
    const glm::vec4& material
) {
    glm::vec3 p0 = min_p;
    glm::vec3 p1 = max_p;

    auto push_quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& n) {
        verts.push_back({a, n, color, material});
        verts.push_back({b, n, color, material});
        verts.push_back({c, n, color, material});
        verts.push_back({a, n, color, material});
        verts.push_back({c, n, color, material});
        verts.push_back({d, n, color, material});
    };

    // Front (+Z)
    push_quad({p0.x, p0.y, p1.z}, {p1.x, p0.y, p1.z}, {p1.x, p1.y, p1.z}, {p0.x, p1.y, p1.z}, {0.0f, 0.0f, 1.0f});
    // Back (-Z)
    push_quad({p1.x, p0.y, p0.z}, {p0.x, p0.y, p0.z}, {p0.x, p1.y, p0.z}, {p1.x, p1.y, p0.z}, {0.0f, 0.0f, -1.0f});
    // Right (+X)
    push_quad({p1.x, p0.y, p1.z}, {p1.x, p0.y, p0.z}, {p1.x, p1.y, p0.z}, {p1.x, p1.y, p1.z}, {1.0f, 0.0f, 0.0f});
    // Left (-X)
    push_quad({p0.x, p0.y, p0.z}, {p0.x, p0.y, p1.z}, {p0.x, p1.y, p1.z}, {p0.x, p1.y, p0.z}, {-1.0f, 0.0f, 0.0f});
    // Top (+Y)
    push_quad({p0.x, p1.y, p1.z}, {p1.x, p1.y, p1.z}, {p1.x, p1.y, p0.z}, {p0.x, p1.y, p0.z}, {0.0f, 1.0f, 0.0f});
    // Bottom (-Y)
    push_quad({p0.x, p0.y, p0.z}, {p1.x, p0.y, p0.z}, {p1.x, p0.y, p1.z}, {p0.x, p0.y, p1.z}, {0.0f, -1.0f, 0.0f});
}

void ViewModel::add_transformed_box(
    std::vector<ViewmodelVertex>& verts,
    const glm::mat4& transform,
    const glm::vec3& half_extents,
    const glm::vec4& color,
    const glm::vec4& material
) {
    glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(transform)));
    auto xform_pt = [&](float x, float y, float z) -> glm::vec3 {
        return glm::vec3(transform * glm::vec4(x * half_extents.x, y * half_extents.y, z * half_extents.z, 1.0f));
    };
    auto xform_n = [&](const glm::vec3& n) -> glm::vec3 {
        return glm::normalize(normal_mat * n);
    };

    auto push_quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& n) {
        verts.push_back({a, n, color, material});
        verts.push_back({b, n, color, material});
        verts.push_back({c, n, color, material});
        verts.push_back({a, n, color, material});
        verts.push_back({c, n, color, material});
        verts.push_back({d, n, color, material});
    };

    push_quad(xform_pt(-1, -1,  1), xform_pt( 1, -1,  1), xform_pt( 1,  1,  1), xform_pt(-1,  1,  1), xform_n({0, 0, 1}));
    push_quad(xform_pt( 1, -1, -1), xform_pt(-1, -1, -1), xform_pt(-1,  1, -1), xform_pt( 1,  1, -1), xform_n({0, 0, -1}));
    push_quad(xform_pt( 1, -1,  1), xform_pt( 1, -1, -1), xform_pt( 1,  1, -1), xform_pt( 1,  1,  1), xform_n({1, 0, 0}));
    push_quad(xform_pt(-1, -1, -1), xform_pt(-1, -1,  1), xform_pt(-1,  1,  1), xform_pt(-1,  1, -1), xform_n({-1, 0, 0}));
    push_quad(xform_pt(-1,  1,  1), xform_pt( 1,  1,  1), xform_pt( 1,  1, -1), xform_pt(-1,  1, -1), xform_n({0, 1, 0}));
    push_quad(xform_pt(-1, -1, -1), xform_pt( 1, -1, -1), xform_pt( 1, -1,  1), xform_pt(-1, -1,  1), xform_n({0, -1, 0}));
}

void ViewModel::add_capsule(
    std::vector<ViewmodelVertex>& verts,
    const glm::vec3& p1,
    const glm::vec3& p2,
    float r1,
    float r2,
    int segments,
    const glm::vec4& color,
    const glm::vec4& material
) {
    glm::vec3 diff = p2 - p1;
    float len = glm::length(diff);
    if (len < 0.001f) return;
    glm::vec3 dir = diff / len;

    glm::vec3 up = (std::abs(dir.y) < 0.95f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(up, dir));
    up = glm::cross(dir, right);

    float step = 2.0f * 3.14159265f / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float c2 = std::cos(a2), s2 = std::sin(a2);

        glm::vec3 rad1 = right * c1 + up * s1;
        glm::vec3 rad2 = right * c2 + up * s2;

        glm::vec3 v1 = p1 + rad1 * r1;
        glm::vec3 v2 = p1 + rad2 * r1;
        glm::vec3 v3 = p2 + rad2 * r2;
        glm::vec3 v4 = p2 + rad1 * r2;

        glm::vec3 n1 = rad1;
        glm::vec3 n2 = rad2;

        verts.push_back({v1, n1, color, material});
        verts.push_back({v2, n2, color, material});
        verts.push_back({v3, n2, color, material});
        verts.push_back({v1, n1, color, material});
        verts.push_back({v3, n2, color, material});
        verts.push_back({v4, n1, color, material});

        // Caps
        verts.push_back({p1, -dir, color, material});
        verts.push_back({v2, -dir, color, material});
        verts.push_back({v1, -dir, color, material});

        verts.push_back({p2, dir, color, material});
        verts.push_back({v4, dir, color, material});
        verts.push_back({v3, dir, color, material});
    }
}

void ViewModel::add_cylinder(
    std::vector<ViewmodelVertex>& verts,
    const glm::vec3& base,
    float radius,
    float length,
    int segments,
    const glm::vec4& color,
    const glm::vec4& material,
    int axis,
    bool cap_ends
) {
    float step = 2.0f * 3.14159265f / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float c2 = std::cos(a2), s2 = std::sin(a2);

        glm::vec3 p1, p2, p3, p4, n1, n2;
        if (axis == 2) {
            p1 = base + glm::vec3(radius * c1, radius * s1, 0.0f);
            p2 = base + glm::vec3(radius * c2, radius * s2, 0.0f);
            p3 = base + glm::vec3(radius * c2, radius * s2, -length);
            p4 = base + glm::vec3(radius * c1, radius * s1, -length);
            n1 = glm::vec3(c1, s1, 0.0f);
            n2 = glm::vec3(c2, s2, 0.0f);

            verts.push_back({p1, n1, color, material});
            verts.push_back({p2, n2, color, material});
            verts.push_back({p3, n2, color, material});
            verts.push_back({p1, n1, color, material});
            verts.push_back({p3, n2, color, material});
            verts.push_back({p4, n1, color, material});

            if (cap_ends) {
                verts.push_back({base, {0, 0, 1}, color, material});
                verts.push_back({p2, {0, 0, 1}, color, material});
                verts.push_back({p1, {0, 0, 1}, color, material});

                glm::vec3 end_c = base + glm::vec3(0.0f, 0.0f, -length);
                verts.push_back({end_c, {0, 0, -1}, color, material});
                verts.push_back({p4, {0, 0, -1}, color, material});
                verts.push_back({p3, {0, 0, -1}, color, material});
            }
        } else if (axis == 1) {
            p1 = base + glm::vec3(radius * c1, 0.0f, radius * s1);
            p2 = base + glm::vec3(radius * c2, 0.0f, radius * s2);
            p3 = base + glm::vec3(radius * c2, length, radius * s2);
            p4 = base + glm::vec3(radius * c1, length, radius * s1);
            n1 = glm::vec3(c1, 0.0f, s1);
            n2 = glm::vec3(c2, 0.0f, s2);

            verts.push_back({p1, n1, color, material});
            verts.push_back({p2, n2, color, material});
            verts.push_back({p3, n2, color, material});
            verts.push_back({p1, n1, color, material});
            verts.push_back({p3, n2, color, material});
            verts.push_back({p4, n1, color, material});

            if (cap_ends) {
                verts.push_back({base, {0, -1, 0}, color, material});
                verts.push_back({p1, {0, -1, 0}, color, material});
                verts.push_back({p2, {0, -1, 0}, color, material});

                glm::vec3 end_c = base + glm::vec3(0.0f, length, 0.0f);
                verts.push_back({end_c, {0, 1, 0}, color, material});
                verts.push_back({p4, {0, 1, 0}, color, material});
                verts.push_back({p3, {0, 1, 0}, color, material});
            }
        } else {
            p1 = base + glm::vec3(0.0f, radius * c1, radius * s1);
            p2 = base + glm::vec3(0.0f, radius * c2, radius * s2);
            p3 = base + glm::vec3(length, radius * c2, radius * s2);
            p4 = base + glm::vec3(length, radius * c1, radius * s1);
            n1 = glm::vec3(0.0f, c1, s1);
            n2 = glm::vec3(0.0f, c2, s2);

            verts.push_back({p1, n1, color, material});
            verts.push_back({p2, n2, color, material});
            verts.push_back({p3, n2, color, material});
            verts.push_back({p1, n1, color, material});
            verts.push_back({p3, n2, color, material});
            verts.push_back({p4, n1, color, material});

            if (cap_ends) {
                verts.push_back({base, {-1, 0, 0}, color, material});
                verts.push_back({p1, {-1, 0, 0}, color, material});
                verts.push_back({p2, {-1, 0, 0}, color, material});

                glm::vec3 end_c = base + glm::vec3(length, 0.0f, 0.0f);
                verts.push_back({end_c, {1, 0, 0}, color, material});
                verts.push_back({p4, {1, 0, 0}, color, material});
                verts.push_back({p3, {1, 0, 0}, color, material});
            }
        }
    }
}

void ViewModel::add_cone(
    std::vector<ViewmodelVertex>& verts,
    const glm::vec3& base,
    float radius_base,
    float radius_tip,
    float length,
    int segments,
    const glm::vec4& color,
    const glm::vec4& material
) {
    float step = 2.0f * 3.14159265f / static_cast<float>(segments);
    glm::vec3 tip_c = base + glm::vec3(0.0f, 0.0f, -length);

    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float c2 = std::cos(a2), s2 = std::sin(a2);

        glm::vec3 p1 = base + glm::vec3(radius_base * c1, radius_base * s1, 0.0f);
        glm::vec3 p2 = base + glm::vec3(radius_base * c2, radius_base * s2, 0.0f);
        glm::vec3 p3 = tip_c + glm::vec3(radius_tip * c2, radius_tip * s2, 0.0f);
        glm::vec3 p4 = tip_c + glm::vec3(radius_tip * c1, radius_tip * s1, 0.0f);

        glm::vec3 side_n = glm::normalize(glm::vec3((c1 + c2) * 0.5f, (s1 + s2) * 0.5f, (radius_base - radius_tip) / std::max(0.001f, length)));

        if (radius_tip <= 0.001f) {
            verts.push_back({p1, side_n, color, material});
            verts.push_back({p2, side_n, color, material});
            verts.push_back({tip_c, side_n, color, material});
        } else {
            verts.push_back({p1, side_n, color, material});
            verts.push_back({p2, side_n, color, material});
            verts.push_back({p3, side_n, color, material});
            verts.push_back({p1, side_n, color, material});
            verts.push_back({p3, side_n, color, material});
            verts.push_back({p4, side_n, color, material});
        }
    }
}

void ViewModel::add_forearm_and_gauntlet(
    std::vector<ViewmodelVertex>& verts,
    const glm::vec3& elbow_origin,
    const glm::vec3& wrist_pos,
    const glm::vec3& palm_pos,
    const glm::vec4& sleeve_col,
    const glm::vec4& accent_col,
    const glm::vec4& glove_col,
    bool is_right_arm
) {
    glm::vec4 mat_cloth(0.02f, 0.88f, 0.0f, 0.85f);
    glm::vec4 mat_armor(0.45f, 0.40f, 0.0f, 0.90f);
    glm::vec4 mat_metal(0.72f, 0.35f, 0.0f, 0.90f);
    glm::vec4 mat_rubber(0.05f, 0.75f, 0.0f, 0.80f);
    glm::vec4 col_armor_plate(0.25f, 0.28f, 0.32f, 1.0f);
    glm::vec4 col_chassis_dark(0.13f, 0.14f, 0.16f, 1.0f);

    // 1. Dual-segmented Forearm Sleeve (Upper segment entering from viewport corner)
    add_capsule(verts, elbow_origin, wrist_pos, 0.048f, 0.036f, 10, sleeve_col, mat_cloth);

    // 2. Forearm Reinforced Armor Plating with Class Accent Trim
    glm::vec3 mid_arm = (elbow_origin + wrist_pos) * 0.5f + (is_right_arm ? glm::vec3(0.015f, 0.015f, 0.0f) : glm::vec3(-0.015f, 0.015f, 0.0f));
    add_capsule(verts, mid_arm + glm::vec3(0.0f, 0.015f, 0.05f), mid_arm - glm::vec3(0.0f, -0.015f, 0.05f), 0.018f, 0.015f, 8, col_armor_plate, mat_armor);
    add_capsule(verts, mid_arm + glm::vec3(0.0f, 0.018f, 0.04f), mid_arm - glm::vec3(0.0f, -0.012f, 0.04f), 0.007f, 0.005f, 6, accent_col, mat_metal);

    // 3. Wrist Cuff Seal Ring
    add_capsule(verts, wrist_pos + glm::vec3(0.005f, -0.005f, 0.010f), wrist_pos - glm::vec3(0.005f, -0.005f, 0.010f), 0.038f, 0.038f, 12, col_chassis_dark, mat_metal);
    add_capsule(verts, wrist_pos + glm::vec3(0.003f, -0.003f, 0.006f), wrist_pos - glm::vec3(0.003f, -0.003f, 0.006f), 0.040f, 0.040f, 12, accent_col, mat_metal);

    // 4. Glove Palm & Knuckle Guard
    add_capsule(verts, wrist_pos, palm_pos, 0.030f, 0.026f, 8, glove_col, mat_rubber);
    add_capsule(verts, palm_pos + glm::vec3(0.0f, 0.012f, 0.010f), palm_pos + glm::vec3(0.0f, -0.015f, 0.010f), 0.012f, 0.010f, 6, col_armor_plate, mat_armor);
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
    if (m_carbine_vao) { glDeleteVertexArrays(1, &m_carbine_vao); m_carbine_vao = 0; }
    if (m_carbine_vbo) { glDeleteBuffers(1, &m_carbine_vbo); m_carbine_vbo = 0; }
    if (m_scattergun_vao) { glDeleteVertexArrays(1, &m_scattergun_vao); m_scattergun_vao = 0; }
    if (m_scattergun_vbo) { glDeleteBuffers(1, &m_scattergun_vbo); m_scattergun_vbo = 0; }
    if (m_railgun_vao) { glDeleteVertexArrays(1, &m_railgun_vao); m_railgun_vao = 0; }
    if (m_railgun_vbo) { glDeleteBuffers(1, &m_railgun_vbo); m_railgun_vbo = 0; }
    if (m_detonator_vao) { glDeleteVertexArrays(1, &m_detonator_vao); m_detonator_vao = 0; }
    if (m_detonator_vbo) { glDeleteBuffers(1, &m_detonator_vbo); m_detonator_vbo = 0; }
    if (m_plunger_vao) { glDeleteVertexArrays(1, &m_plunger_vao); m_plunger_vao = 0; }
    if (m_plunger_vbo) { glDeleteBuffers(1, &m_plunger_vbo); m_plunger_vbo = 0; }

    glm::vec4 col_suit_arm;
    glm::vec4 col_suit_accent;
    glm::vec4 col_suit_glove;

    if (m_character_class == CharacterClass::Demolitionist) {
        // Demolitionist (Kaelen): Industrial Hazard Orange / Dark Charcoal
        col_suit_accent = glm::vec4(1.0f, 0.55f, 0.0f, 1.0f);   // Industrial Hazard Orange
        col_suit_arm    = glm::vec4(0.12f, 0.12f, 0.14f, 1.0f); // Dark Charcoal
        col_suit_glove  = glm::vec4(0.20f, 0.18f, 0.16f, 1.0f);
    } else if (m_character_class == CharacterClass::Vanguard) {
        // Vanguard (Rhodes): Heavy Slate Gray / Reinforced Bronze
        col_suit_arm    = glm::vec4(0.32f, 0.35f, 0.40f, 1.0f); // Heavy Slate Gray
        col_suit_accent = glm::vec4(0.72f, 0.50f, 0.22f, 1.0f); // Reinforced Bronze
        col_suit_glove  = glm::vec4(0.24f, 0.26f, 0.30f, 1.0f);
    } else {
        // Scout (Vesper): Electric Cyan / Matte White
        col_suit_accent = glm::vec4(0.0f, 0.90f, 1.0f, 1.0f);   // Electric Cyan
        col_suit_arm    = glm::vec4(0.88f, 0.90f, 0.92f, 1.0f); // Matte White
        col_suit_glove  = glm::vec4(0.20f, 0.24f, 0.28f, 1.0f);
    }
    glm::vec4 col_glove_rubber(0.12f, 0.13f, 0.15f, 1.0f);
    glm::vec4 col_armor_plate(0.25f, 0.28f, 0.32f, 1.0f);

    glm::vec4 col_chassis_dark(0.13f, 0.14f, 0.16f, 1.0f);
    glm::vec4 col_chassis_body(0.24f, 0.26f, 0.29f, 1.0f);
    glm::vec4 col_chassis_metal(0.50f, 0.54f, 0.58f, 1.0f);
    glm::vec4 col_steel_bright(0.80f, 0.84f, 0.88f, 1.0f);
    glm::vec4 col_brass(0.78f, 0.64f, 0.28f, 1.0f);
    glm::vec4 col_hazard_yellow(0.96f, 0.80f, 0.12f, 1.0f);
    glm::vec4 col_hazard_black(0.08f, 0.08f, 0.09f, 1.0f);
    glm::vec4 col_hud_display(0.04f, 0.15f, 0.22f, 1.0f);
    glm::vec4 col_hud_cyan(0.15f, 0.95f, 1.0f, 1.0f);
    glm::vec4 col_hud_amber(1.0f, 0.65f, 0.10f, 1.0f);
    glm::vec4 col_heat_coil(1.0f, 0.45f, 0.10f, 1.0f);

    glm::vec4 mat_cloth(0.02f, 0.88f, 0.0f, 0.85f);
    glm::vec4 mat_rubber(0.05f, 0.75f, 0.0f, 0.80f);
    glm::vec4 mat_armor(0.45f, 0.40f, 0.0f, 0.90f);
    glm::vec4 mat_metal(0.72f, 0.35f, 0.0f, 0.90f);
    glm::vec4 mat_dark_polymer(0.15f, 0.55f, 0.0f, 0.80f);
    glm::vec4 mat_chrome(0.96f, 0.08f, 0.0f, 1.0f);
    glm::vec4 mat_led_emissive(0.0f, 0.20f, 0.95f, 1.0f);
    glm::vec4 mat_screen_bg(0.05f, 0.30f, 0.60f, 1.0f);

    // =============================================================
    // 1. CHASSIS + HANDS & DELVER GAUNTLETS + MINING RIG
    // (Local space: +X = Right, +Y = Up, -Z = Forward along spindle)
    // =============================================================
    std::vector<ViewmodelVertex> chassis_verts;

    // -------------------------------------------------------------
    // A. SLEEK INDUSTRIAL MOTOR CASING & ENGINE BLOCK
    // (Compact 9cm x 9cm profile so the drill bit out front is fully visible!)
    // -------------------------------------------------------------
    add_box(chassis_verts, {-0.046f, -0.052f, -0.16f}, {0.046f, 0.042f, 0.02f}, col_chassis_body, mat_dark_polymer);
    add_box(chassis_verts, {-0.038f, 0.042f, -0.15f}, {0.038f, 0.054f, 0.01f}, col_chassis_dark, mat_armor);
    add_box(chassis_verts, {-0.040f, -0.065f, -0.15f}, {0.040f, -0.052f, 0.01f}, col_chassis_dark, mat_armor);

    // Left and Right Radiator Cooling Vents & Internal Glowing Heat Coils
    for (int v = 0; v < 3; ++v) {
        float vz = -0.12f + v * 0.045f;
        // Left
        add_box(chassis_verts, {-0.048f, -0.035f, vz - 0.012f}, {-0.045f, 0.025f, vz + 0.012f}, col_heat_coil, mat_led_emissive);
        add_box(chassis_verts, {-0.050f, -0.040f, vz - 0.003f}, {-0.046f, 0.030f, vz + 0.003f}, col_chassis_dark, mat_metal);
        // Right
        add_box(chassis_verts, {0.045f, -0.035f, vz - 0.012f}, {0.048f, 0.025f, vz + 0.012f}, col_heat_coil, mat_led_emissive);
        add_box(chassis_verts, {0.046f, -0.040f, vz - 0.003f}, {0.050f, 0.030f, vz + 0.003f}, col_chassis_dark, mat_metal);
    }

    // Top Hazard Caution Stripes
    float hz_z = 0.01f;
    for (int h = 0; h < 4; ++h) {
        glm::vec4 h_col = (h % 2 == 0) ? col_hazard_yellow : col_hazard_black;
        add_box(chassis_verts, {-0.036f, 0.055f, hz_z - 0.025f}, {0.036f, 0.056f, hz_z}, h_col, mat_armor);
        hz_z -= 0.034f;
    }

    // Top Protective Tubular Roll-Cage
    add_capsule(chassis_verts, {-0.038f, 0.048f, 0.01f}, {-0.038f, 0.058f, -0.15f}, 0.006f, 0.006f, 8, col_chassis_dark, mat_armor);
    add_capsule(chassis_verts, { 0.038f, 0.048f, 0.01f}, { 0.038f, 0.058f, -0.15f}, 0.006f, 0.006f, 8, col_chassis_dark, mat_armor);

    // Left Pneumatic Pressure Canister
    add_capsule(chassis_verts, {-0.062f, -0.015f, 0.01f}, {-0.062f, -0.015f, -0.13f}, 0.019f, 0.019f, 12, col_chassis_dark, mat_armor);
    add_capsule(chassis_verts, {-0.062f, -0.015f, 0.01f}, {-0.062f, -0.015f, 0.025f}, 0.011f, 0.011f, 10, col_brass, mat_metal);
    add_capsule(chassis_verts, {-0.062f, -0.015f, -0.045f}, {-0.062f, -0.015f, -0.065f}, 0.0205f, 0.0205f, 12, col_suit_accent, mat_metal);

    // Left Forward Stabilizer Support Arm
    add_capsule(chassis_verts, {-0.046f, -0.015f, -0.08f}, {-0.13f, -0.015f, -0.08f}, 0.011f, 0.011f, 10, col_chassis_dark, mat_armor);
    add_capsule(chassis_verts, {-0.075f, -0.015f, -0.08f}, {-0.12f, -0.015f, -0.08f}, 0.014f, 0.014f, 12, col_glove_rubber, mat_rubber);
    add_capsule(chassis_verts, {-0.125f, -0.015f, -0.08f}, {-0.135f, -0.015f, -0.08f}, 0.016f, 0.016f, 12, col_steel_bright, mat_metal);

    // -------------------------------------------------------------
    // B. COMPACT REAR DELVER OLED TELEMETRY DISPLAY
    // -------------------------------------------------------------
    glm::mat4 hud_m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.052f, 0.015f));
    hud_m = glm::rotate(hud_m, glm::radians(32.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    add_transformed_box(chassis_verts, hud_m, glm::vec3(0.040f, 0.022f, 0.008f), col_chassis_dark, mat_dark_polymer);
    add_transformed_box(chassis_verts, hud_m, glm::vec3(0.035f, 0.018f, 0.009f), col_hud_display, mat_screen_bg);

    // 5-Segment Dynamic Tachometer across top
    float seg_start_x = -0.025f;
    for (int s = 0; s < 5; ++s) {
        glm::vec4 seg_col = (s < 3) ? col_hud_cyan : col_hud_amber;
        glm::mat4 seg_m = glm::translate(hud_m, glm::vec3(seg_start_x + s * 0.0125f, 0.010f, 0.010f));
        add_transformed_box(chassis_verts, seg_m, glm::vec3(0.0045f, 0.003f, 0.001f), seg_col, mat_led_emissive);
    }
    // Telemetry crosshair
    glm::mat4 reticle_m = glm::translate(hud_m, glm::vec3(0.0f, -0.004f, 0.010f));
    add_transformed_box(chassis_verts, reticle_m, glm::vec3(0.022f, 0.002f, 0.001f), col_hud_cyan, mat_led_emissive);
    add_transformed_box(chassis_verts, reticle_m, glm::vec3(0.002f, 0.007f, 0.001f), col_hud_cyan, mat_led_emissive);

    // -------------------------------------------------------------
    // C. REAR ERGONOMIC GRIP & TRIGGER
    // -------------------------------------------------------------
    glm::vec3 grip_top(0.0f, -0.045f, 0.005f);
    glm::vec3 grip_bot(0.0f, -0.165f, 0.045f);
    add_capsule(chassis_verts, grip_top, grip_bot, 0.016f, 0.014f, 10, col_chassis_dark, mat_dark_polymer);
    add_capsule(chassis_verts, grip_top + glm::vec3(0, -0.02f, 0), grip_bot + glm::vec3(0, 0.015f, 0), 0.0175f, 0.0155f, 10, col_glove_rubber, mat_rubber);

    // Trigger guard
    add_capsule(chassis_verts, {0.0f, -0.06f, 0.00f}, {0.0f, -0.11f, -0.025f}, 0.005f, 0.005f, 6, col_chassis_dark, mat_metal);
    add_capsule(chassis_verts, {0.0f, -0.11f, -0.025f}, {0.0f, -0.12f, 0.025f}, 0.005f, 0.005f, 6, col_chassis_dark, mat_metal);

    // Pneumatic Trigger Blade
    add_capsule(chassis_verts, {0.0f, -0.075f, 0.00f}, {0.0f, -0.098f, -0.015f}, 0.005f, 0.004f, 6, col_suit_accent, mat_metal);

    // -------------------------------------------------------------
    // D. TRANSMISSION BELL HOUSING & HEAVY ROTARY CHUCK COLLAR
    // -------------------------------------------------------------
    // Tapered transition from motor block to chuck collar
    add_cone(chassis_verts, {0.0f, 0.0f, -0.16f}, 0.052f, 0.048f, 0.04f, 16, col_chassis_dark, mat_metal);

    // Front Rotary Chuck Collar
    add_cylinder(chassis_verts, {0.0f, 0.0f, -0.20f}, 0.048f, 0.06f, 16, col_chassis_metal, mat_metal, 2);

    // 4 Locking Jaws bolted around the chuck
    for (int j = 0; j < 4; ++j) {
        float j_angle = j * 1.5707963f;
        glm::mat4 jaw_m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.23f));
        jaw_m = glm::rotate(jaw_m, j_angle, glm::vec3(0.0f, 0.0f, 1.0f));
        jaw_m = glm::translate(jaw_m, glm::vec3(0.045f, 0.0f, 0.0f));
        add_transformed_box(chassis_verts, jaw_m, glm::vec3(0.009f, 0.012f, 0.018f), col_steel_bright, mat_metal);
    }

    // Spindle seal hub
    add_cylinder(chassis_verts, {0.0f, 0.0f, -0.26f}, 0.034f, 0.015f, 16, col_chassis_dark, mat_metal, 2);

    // -------------------------------------------------------------
    // E. RIGHT ARM, GAUNTLET & HAND (Operating rear handle & trigger)
    // -------------------------------------------------------------
    glm::vec3 r_palm(0.015f, -0.10f, 0.03f);
    glm::vec3 r_wrist(0.06f, -0.15f, 0.12f);
    glm::vec3 r_elbow(0.20f, -0.32f, 0.36f);

    // Forearm sleeve
    add_capsule(chassis_verts, r_elbow, r_wrist, 0.048f, 0.036f, 10, col_suit_arm, mat_cloth);

    // Composite forearm armor plate
    glm::vec3 r_mid_arm = (r_elbow + r_wrist) * 0.5f + glm::vec3(0.015f, 0.015f, 0.0f);
    add_capsule(chassis_verts, r_mid_arm + glm::vec3(0.015f, 0.02f, 0.06f), r_mid_arm - glm::vec3(0.015f, -0.015f, 0.06f), 0.018f, 0.015f, 8, col_armor_plate, mat_armor);
    add_capsule(chassis_verts, r_mid_arm + glm::vec3(0.017f, 0.022f, 0.05f), r_mid_arm - glm::vec3(0.013f, -0.013f, 0.05f), 0.006f, 0.005f, 6, col_suit_accent, mat_metal);

    // Wrist cuff seal ring
    add_capsule(chassis_verts, r_wrist + glm::vec3(0.008f, -0.008f, 0.015f), r_wrist - glm::vec3(0.008f, -0.008f, 0.015f), 0.038f, 0.038f, 12, col_chassis_dark, mat_metal);
    add_capsule(chassis_verts, r_wrist + glm::vec3(0.004f, -0.004f, 0.008f), r_wrist - glm::vec3(0.004f, -0.004f, 0.008f), 0.040f, 0.040f, 12, col_suit_accent, mat_metal);

    // Hand palm
    add_capsule(chassis_verts, r_wrist, r_palm, 0.032f, 0.028f, 8, col_suit_glove, mat_rubber);
    // Knuckle guard
    add_capsule(chassis_verts, r_palm + glm::vec3(0.018f, 0.012f, 0.015f), r_palm + glm::vec3(0.018f, -0.020f, 0.015f), 0.012f, 0.010f, 6, col_armor_plate, mat_armor);

    // Thumb wrapped over upper handle
    add_capsule(chassis_verts, r_palm + glm::vec3(0.008f, 0.015f, -0.008f), glm::vec3(-0.015f, -0.06f, 0.015f), 0.010f, 0.009f, 6, col_suit_glove, mat_rubber);

    // Index finger curled on trigger
    add_capsule(chassis_verts, r_palm + glm::vec3(-0.008f, 0.01f, -0.015f), glm::vec3(0.0f, -0.085f, -0.015f), 0.009f, 0.007f, 6, col_suit_glove, mat_rubber);
    add_capsule(chassis_verts, glm::vec3(0.0f, -0.085f, -0.015f), glm::vec3(-0.015f, -0.095f, 0.005f), 0.007f, 0.006f, 6, col_suit_accent, mat_rubber);

    // Middle, Ring, Pinky curled tightly around grip
    add_capsule(chassis_verts, r_palm + glm::vec3(-0.008f, -0.008f, -0.008f), glm::vec3(-0.016f, -0.11f, 0.015f), 0.009f, 0.007f, 6, col_suit_glove, mat_rubber);
    add_capsule(chassis_verts, r_palm + glm::vec3(-0.008f, -0.024f, 0.000f), glm::vec3(-0.015f, -0.13f, 0.022f), 0.008f, 0.007f, 6, col_suit_glove, mat_rubber);
    add_capsule(chassis_verts, r_palm + glm::vec3(-0.008f, -0.040f, 0.008f), glm::vec3(-0.014f, -0.15f, 0.030f), 0.007f, 0.006f, 6, col_suit_glove, mat_rubber);

    // -------------------------------------------------------------
    // F. LEFT ARM, GAUNTLET & HAND (Gripping forward stabilizer bar)
    // -------------------------------------------------------------
    glm::vec3 l_palm(-0.095f, -0.015f, -0.08f);
    glm::vec3 l_wrist(-0.16f, -0.08f, -0.02f);
    glm::vec3 l_elbow(-0.30f, -0.26f, 0.22f);

    // Forearm sleeve
    add_capsule(chassis_verts, l_elbow, l_wrist, 0.048f, 0.036f, 10, col_suit_arm, mat_cloth);

    // Composite forearm armor plate
    glm::vec3 l_mid_arm = (l_elbow + l_wrist) * 0.5f + glm::vec3(-0.015f, 0.015f, 0.0f);
    add_capsule(chassis_verts, l_mid_arm + glm::vec3(-0.015f, 0.02f, 0.06f), l_mid_arm - glm::vec3(-0.015f, -0.015f, 0.06f), 0.018f, 0.015f, 8, col_armor_plate, mat_armor);
    add_capsule(chassis_verts, l_mid_arm + glm::vec3(-0.017f, 0.022f, 0.05f), l_mid_arm - glm::vec3(-0.013f, -0.013f, 0.05f), 0.006f, 0.005f, 6, col_suit_accent, mat_metal);

    // Wrist cuff ring
    add_capsule(chassis_verts, l_wrist + glm::vec3(-0.008f, -0.008f, 0.015f), l_wrist - glm::vec3(-0.008f, -0.008f, 0.015f), 0.038f, 0.038f, 12, col_chassis_dark, mat_metal);
    add_capsule(chassis_verts, l_wrist + glm::vec3(-0.004f, -0.004f, 0.008f), l_wrist - glm::vec3(-0.004f, -0.004f, 0.008f), 0.040f, 0.040f, 12, col_suit_accent, mat_metal);

    // Hand palm
    add_capsule(chassis_verts, l_wrist, l_palm, 0.030f, 0.026f, 8, col_suit_glove, mat_rubber);
    // Knuckle guard
    add_capsule(chassis_verts, l_palm + glm::vec3(-0.008f, 0.018f, -0.015f), l_palm + glm::vec3(-0.008f, 0.018f, 0.015f), 0.011f, 0.011f, 6, col_armor_plate, mat_armor);

    // Thumb clamped over top of stabilizer
    add_capsule(chassis_verts, l_palm + glm::vec3(0.008f, 0.015f, 0.008f), glm::vec3(-0.090f, 0.005f, -0.075f), 0.010f, 0.008f, 6, col_suit_accent, mat_rubber);

    // 4 Fingers curled around and under stabilizer handle
    for (int f = 0; f < 4; ++f) {
        float f_z = -0.065f - f * 0.012f;
        glm::vec3 f_base = l_palm + glm::vec3(-0.008f, 0.008f, f_z - l_palm.z);
        glm::vec3 f_curl1 = glm::vec3(-0.098f - f * 0.004f, -0.032f, f_z);
        glm::vec3 f_curl2 = glm::vec3(-0.082f - f * 0.004f, -0.022f, f_z);
        add_capsule(chassis_verts, f_base, f_curl1, 0.008f, 0.007f, 6, col_suit_glove, mat_rubber);
        add_capsule(chassis_verts, f_curl1, f_curl2, 0.007f, 0.006f, 6, col_glove_rubber, mat_rubber);
    }

    m_chassis_count = chassis_verts.size();

    glGenVertexArrays(1, &m_chassis_vao);
    glGenBuffers(1, &m_chassis_vbo);
    glBindVertexArray(m_chassis_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_chassis_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_chassis_count * sizeof(ViewmodelVertex), chassis_verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, material));
    glBindVertexArray(0);

    // =============================================================
    // 2. RECIPROCATING DUAL PNEUMATIC PISTONS
    // =============================================================
    std::vector<ViewmodelVertex> piston_verts;

    // Left hydraulic assembly
    add_cylinder(piston_verts, {-0.036f, 0.032f, -0.06f}, 0.012f, 0.14f, 12, col_chassis_dark, mat_armor, 2);
    add_cylinder(piston_verts, {-0.036f, 0.032f, -0.06f}, 0.014f, 0.015f, 12, col_brass, mat_metal, 2);
    add_cylinder(piston_verts, {-0.036f, 0.032f, -0.16f}, 0.008f, 0.12f, 12, col_steel_bright, mat_chrome, 2);

    // Right hydraulic assembly
    add_cylinder(piston_verts, { 0.036f, 0.032f, -0.06f}, 0.012f, 0.14f, 12, col_chassis_dark, mat_armor, 2);
    add_cylinder(piston_verts, { 0.036f, 0.032f, -0.06f}, 0.014f, 0.015f, 12, col_brass, mat_metal, 2);
    add_cylinder(piston_verts, { 0.036f, 0.032f, -0.16f}, 0.008f, 0.12f, 12, col_steel_bright, mat_chrome, 2);

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
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, material));
    glBindVertexArray(0);

    // =============================================================
    // 3. ROTATING FLUTED SPIRAL AUGER DRILL BIT
    // (Local origin: attaches to spindle at (0.0f, 0.0f, -0.275f))
    // =============================================================
    std::vector<ViewmodelVertex> bit_verts;
    glm::vec4 col_tungsten(0.62f, 0.66f, 0.72f, 1.0f);
    glm::vec4 col_carbide_flute(0.38f, 0.42f, 0.46f, 1.0f);
    glm::vec4 col_diamond_tip(0.92f, 0.97f, 1.0f, 1.0f);
    glm::vec4 col_cutting_edge(0.85f, 0.88f, 0.94f, 1.0f);

    glm::vec4 mat_bit_tungsten(0.75f, 0.30f, 0.05f, 0.95f);
    glm::vec4 mat_bit_carbide(0.85f, 0.22f, 0.10f, 0.90f);
    glm::vec4 mat_bit_tip(0.92f, 0.12f, 0.50f, 1.0f);

    // Heavy Tungsten Spindle Coupling
    add_cylinder(bit_verts, {0.0f, 0.0f, 0.0f}, 0.042f, 0.045f, 16, col_tungsten, mat_bit_tungsten, 2);

    // Stepped Transition Shaft with locking notches
    add_cylinder(bit_verts, {0.0f, 0.0f, -0.045f}, 0.038f, 0.055f, 16, col_carbide_flute, mat_bit_carbide, 2);

    // Core Heavy Tapered Shaft (Extended length: 32cm!)
    add_cone(bit_verts, {0.0f, 0.0f, -0.10f}, 0.038f, 0.014f, 0.30f, 16, col_carbide_flute, mat_bit_carbide);

    // 4 Massive Sculpted Helical Auger Flutes / Spiral Blades
    const int FLUTE_COUNT = 4;
    const int STEPS_PER_FLUTE = 18;
    float flute_len = 0.30f;
    float start_z = -0.08f;

    for (int f = 0; f < FLUTE_COUNT; ++f) {
        float base_phase = f * (6.2831853f / FLUTE_COUNT);
        for (int s = 0; s < STEPS_PER_FLUTE; ++s) {
            float t1 = static_cast<float>(s) / STEPS_PER_FLUTE;
            float t2 = static_cast<float>(s + 1) / STEPS_PER_FLUTE;

            float z1 = start_z - t1 * flute_len;
            float z2 = start_z - t2 * flute_len;

            float angle1 = base_phase + t1 * 3.14159265f;
            float angle2 = base_phase + t2 * 3.14159265f;

            float r_inner1 = 0.036f * (1.0f - t1 * 0.52f);
            float r_inner2 = 0.036f * (1.0f - t2 * 0.52f);
            float r_outer1 = r_inner1 + 0.024f * (1.0f - t1 * 0.35f);
            float r_outer2 = r_inner2 + 0.024f * (1.0f - t2 * 0.35f);

            glm::vec3 in_p1(r_inner1 * std::cos(angle1), r_inner1 * std::sin(angle1), z1);
            glm::vec3 in_p2(r_inner2 * std::cos(angle2), r_inner2 * std::sin(angle2), z2);
            glm::vec3 out_p1(r_outer1 * std::cos(angle1), r_outer1 * std::sin(angle1), z1);
            glm::vec3 out_p2(r_outer2 * std::cos(angle2), r_outer2 * std::sin(angle2), z2);

            glm::vec3 n_edge = glm::normalize(glm::cross(out_p2 - out_p1, in_p1 - out_p1));

            bit_verts.push_back({in_p1, n_edge, col_tungsten, mat_bit_tungsten});
            bit_verts.push_back({out_p1, n_edge, col_cutting_edge, mat_bit_tungsten});
            bit_verts.push_back({out_p2, n_edge, col_cutting_edge, mat_bit_tungsten});
            bit_verts.push_back({in_p1, n_edge, col_tungsten, mat_bit_tungsten});
            bit_verts.push_back({out_p2, n_edge, col_cutting_edge, mat_bit_tungsten});
            bit_verts.push_back({in_p2, n_edge, col_tungsten, mat_bit_tungsten});

            // Reinforced tungsten-carbide teeth blocks along the spiral flutes
            if (s % 2 == 0) {
                glm::mat4 tooth_m = glm::translate(glm::mat4(1.0f), (out_p1 + out_p2) * 0.5f);
                tooth_m = glm::rotate(tooth_m, angle1, glm::vec3(0, 0, 1));
                add_transformed_box(bit_verts, tooth_m, glm::vec3(0.006f, 0.008f, 0.008f), col_steel_bright, mat_bit_carbide);
            }
        }
    }

    // Faceted Diamond Pilot Boring Spike at Apex (Z = -0.38 to -0.50)
    add_cone(bit_verts, {0.0f, 0.0f, -0.38f}, 0.018f, 0.001f, 0.12f, 8, col_diamond_tip, mat_bit_tip);
    add_cone(bit_verts, {0.0f, 0.0f, -0.36f}, 0.025f, 0.016f, 0.04f, 8, col_tungsten, mat_bit_carbide);

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
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, material));
    glBindVertexArray(0);

    // =============================================================
    // 4. FIRST-PERSON PLASMA CARBINE (TACTICAL COMBAT WEAPON)
    // =============================================================
    std::vector<ViewmodelVertex> carbine_verts;

    glm::vec4 col_carbine_body(0.16f, 0.18f, 0.22f, 1.0f);     // Dark matte tactical receiver
    glm::vec4 col_carbine_shroud(0.24f, 0.27f, 0.31f, 1.0f);   // Anodized composite barrel shroud
    glm::vec4 col_carbine_rails(0.68f, 0.72f, 0.78f, 1.0f);    // Chrome accelerator magnetic rails
    glm::vec4 col_plasma_cyan(0.05f, 0.92f, 1.0f, 1.0f);      // Bright electric-cyan coils & vents
    glm::vec4 col_plasma_white(0.85f, 0.98f, 1.0f, 1.0f);     // White-hot plasma core emitter
    glm::vec4 col_optic_glass(0.10f, 0.85f, 0.95f, 0.70f);    // Holographic reflex sight lens
    glm::vec4 col_carbine_grip(0.09f, 0.09f, 0.11f, 1.0f);    // Non-slip rubber grip
    glm::vec4 col_power_cell(0.12f, 0.40f, 0.50f, 1.0f);      // High-capacity plasma battery pack

    glm::vec4 mat_carbine_armor(0.55f, 0.35f, 0.05f, 0.90f);
    glm::vec4 mat_carbine_chrome(0.95f, 0.12f, 0.10f, 0.95f);
    glm::vec4 mat_carbine_emissive(0.10f, 0.05f, 1.00f, 1.00f);
    glm::vec4 mat_carbine_rubber(0.05f, 0.85f, 0.00f, 0.80f);
    glm::vec4 mat_carbine_glass(0.90f, 0.05f, 0.80f, 1.00f);

    // 4.1 Receiver & Upper Housing
    add_box(carbine_verts, {-0.026f, -0.040f, -0.20f}, {0.026f, 0.045f, 0.06f}, col_carbine_body, mat_carbine_armor);
    add_box(carbine_verts, {-0.016f, 0.045f, -0.22f}, {0.016f, 0.056f, 0.04f}, col_chassis_dark, mat_carbine_armor); // Picatinny top rail

    // Stock extension toward delver shoulder
    add_box(carbine_verts, {-0.020f, -0.030f, 0.06f}, {0.020f, 0.035f, 0.20f}, col_carbine_body, mat_carbine_armor);
    add_box(carbine_verts, {-0.022f, -0.055f, 0.18f}, {0.022f, 0.045f, 0.23f}, col_carbine_grip, mat_carbine_rubber); // Recoil buttpad

    // 4.2 Twin Magnetic Accelerator Rails & Plasma Chamber
    // Upper & Lower chrome acceleration guide rods
    add_cylinder(carbine_verts, {0.0f, 0.018f, -0.19f}, 0.010f, 0.28f, 12, col_carbine_rails, mat_carbine_chrome, 2);
    add_cylinder(carbine_verts, {0.0f, -0.014f, -0.19f}, 0.010f, 0.28f, 12, col_carbine_rails, mat_carbine_chrome, 2);

    // Vented Barrel Shroud surrounding rails
    add_box(carbine_verts, {-0.024f, -0.028f, -0.42f}, {0.024f, 0.032f, -0.19f}, col_carbine_shroud, mat_carbine_armor);

    // Emissive Heat Vents / Cooling Gills along Left and Right
    for (int vent = 0; vent < 4; ++vent) {
        float vz = -0.22f - vent * 0.045f;
        add_box(carbine_verts, {-0.026f, -0.006f, vz - 0.014f}, {-0.022f, 0.012f, vz + 0.014f}, col_plasma_cyan, mat_carbine_emissive);
        add_box(carbine_verts, { 0.022f, -0.006f, vz - 0.014f}, { 0.026f, 0.012f, vz + 0.014f}, col_plasma_cyan, mat_carbine_emissive);
    }

    // Heavy Muzzle Brake & Concentric Plasma Emitter Spindle
    add_cylinder(carbine_verts, {0.0f, 0.002f, -0.42f}, 0.018f, 0.050f, 14, col_carbine_rails, mat_carbine_chrome, 2);
    add_cylinder(carbine_verts, {0.0f, 0.002f, -0.44f}, 0.008f, 0.035f, 12, col_plasma_white, mat_carbine_emissive, 2); // White-hot plasma nozzle orifice

    // 4.3 High-Capacity Plasma Power Battery Cell
    add_box(carbine_verts, {-0.018f, -0.145f, -0.16f}, {0.018f, -0.040f, -0.08f}, col_power_cell, mat_carbine_armor);
    // Glowing Charge Indicator Level bars
    add_box(carbine_verts, {-0.019f, -0.130f, -0.13f}, {-0.017f, -0.060f, -0.11f}, col_plasma_cyan, mat_carbine_emissive);
    add_box(carbine_verts, { 0.017f, -0.130f, -0.13f}, { 0.019f, -0.060f, -0.11f}, col_plasma_cyan, mat_carbine_emissive);

    // 4.4 Reflex Holographic Sight
    add_box(carbine_verts, {-0.014f, 0.056f, -0.11f}, {0.014f, 0.076f, -0.03f}, col_chassis_dark, mat_carbine_armor);   // Sight base
    add_box(carbine_verts, {-0.022f, 0.076f, -0.13f}, {0.022f, 0.120f, -0.02f}, col_carbine_shroud, mat_carbine_armor); // Sight hood
    add_box(carbine_verts, {-0.016f, 0.082f, -0.12f}, {0.016f, 0.114f, -0.03f}, col_optic_glass, mat_carbine_glass);    // Glass lens
    // Floating Holographic Dot
    add_box(carbine_verts, {-0.002f, 0.096f, -0.078f}, {0.002f, 0.100f, -0.074f}, col_plasma_cyan, mat_carbine_emissive);

    // 4.5 Pistol Grip & Trigger Assembly
    glm::mat4 grip_mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.085f, 0.02f));
    grip_mat = glm::rotate(grip_mat, glm::radians(20.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    add_transformed_box(carbine_verts, grip_mat, glm::vec3(0.014f, 0.055f, 0.022f), col_carbine_grip, mat_carbine_rubber);
    // Trigger guard & curved trigger blade
    add_box(carbine_verts, {-0.004f, -0.070f, -0.045f}, {0.004f, -0.040f, 0.0f}, col_carbine_rails, mat_carbine_chrome);
    add_box(carbine_verts, {-0.003f, -0.062f, -0.025f}, {0.003f, -0.044f, -0.015f}, col_carbine_rails, mat_carbine_chrome);

    // 4.6 Delver Exo-Suit Arms & Heavy Gauntleted Hands Holding the Carbine
    glm::vec3 r_carb_elbow(0.24f, -0.32f, 0.32f);
    glm::vec3 r_carb_wrist(0.06f, -0.15f, 0.10f);
    glm::vec3 r_carb_palm(0.0f, -0.085f, 0.02f);
    add_forearm_and_gauntlet(carbine_verts, r_carb_elbow, r_carb_wrist, r_carb_palm, col_suit_arm, col_suit_accent, col_suit_glove, true);
    // Right index trigger finger extended onto receiver
    add_capsule(carbine_verts, r_carb_palm + glm::vec3(-0.014f, 0.018f, 0.01f), glm::vec3(-0.012f, -0.055f, -0.025f), 0.007f, 0.006f, 6, col_glove_rubber, mat_rubber);

    // Left arm holding forward tactical shroud under barrel:
    glm::vec3 l_carb_elbow(-0.28f, -0.30f, 0.18f);
    glm::vec3 l_carb_wrist(-0.12f, -0.11f, -0.12f);
    glm::vec3 l_carb_palm(-0.032f, -0.048f, -0.26f);
    add_forearm_and_gauntlet(carbine_verts, l_carb_elbow, l_carb_wrist, l_carb_palm, col_suit_arm, col_suit_accent, col_suit_glove, false);
    for (int lf = 0; lf < 4; ++lf) {
        float lz = -0.24f - lf * 0.014f;
        add_capsule(carbine_verts, l_carb_palm + glm::vec3(-0.005f, 0.005f, lz - l_carb_palm.z), glm::vec3(0.022f, -0.038f, lz), 0.007f, 0.006f, 6, col_glove_rubber, mat_rubber);
    }

    m_carbine_count = carbine_verts.size();

    glGenVertexArrays(1, &m_carbine_vao);
    glGenBuffers(1, &m_carbine_vbo);
    glBindVertexArray(m_carbine_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_carbine_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_carbine_count * sizeof(ViewmodelVertex), carbine_verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, material));
    glBindVertexArray(0);

    // =============================================================
    // 5. FIRST-PERSON MAGMA SCATTERGUN (DEMOLITIONIST HEAVY SHOTGUN)
    // =============================================================
    std::vector<ViewmodelVertex> scatter_verts;
    glm::vec4 col_scatter_body(0.18f, 0.17f, 0.16f, 1.0f);     // Dark cast iron receiver
    glm::vec4 col_scatter_shroud(0.26f, 0.23f, 0.20f, 1.0f);   // Heat-treated steel barrel shroud
    glm::vec4 col_scatter_drum(0.35f, 0.32f, 0.28f, 1.0f);     // Revolving cylinder drum
    glm::vec4 col_scatter_flame(1.0f, 0.55f, 0.08f, 1.0f);     // Molten thermite orange chambers
    glm::vec4 col_scatter_amber(1.0f, 0.80f, 0.20f, 1.0f);     // Burning heat vents
    glm::vec4 col_scatter_grip(0.10f, 0.09f, 0.08f, 1.0f);     // Heat-resistant grip

    // 5.1 Heavy Receiver Box
    add_box(scatter_verts, {-0.034f, -0.052f, -0.16f}, {0.034f, 0.050f, 0.08f}, col_scatter_body, mat_carbine_armor);
    add_box(scatter_verts, {-0.024f, 0.050f, -0.18f}, {0.024f, 0.062f, 0.06f}, col_chassis_dark, mat_carbine_armor); // Top shield rib
    // Heavy stock butt
    add_box(scatter_verts, {-0.026f, -0.040f, 0.08f}, {0.026f, 0.040f, 0.22f}, col_scatter_body, mat_carbine_armor);
    add_box(scatter_verts, {-0.028f, -0.065f, 0.20f}, {0.028f, 0.050f, 0.25f}, col_scatter_grip, mat_carbine_rubber);

    // 5.2 Heavy 6-Round Revolving Cylinder Drum
    add_cylinder(scatter_verts, {0.0f, -0.005f, -0.08f}, 0.036f, 0.095f, 16, col_scatter_drum, mat_carbine_chrome, 2);
    // 6 Chamber Flutes & Thermite Cores
    for (int c = 0; c < 6; ++c) {
        float angle = c * (6.2831853f / 6.0f);
        float cx = std::cos(angle) * 0.022f;
        float cy = std::sin(angle) * 0.022f - 0.005f;
        add_cylinder(scatter_verts, {cx, cy, -0.085f}, 0.009f, 0.098f, 8, col_scatter_flame, mat_carbine_emissive, 2);
    }

    // 5.3 Dual Over-Under Heavy Breaker Barrels
    add_cylinder(scatter_verts, {0.0f, 0.022f, -0.16f}, 0.016f, 0.28f, 14, col_chassis_dark, mat_carbine_chrome, 2);
    add_cylinder(scatter_verts, {0.0f, -0.014f, -0.16f}, 0.016f, 0.28f, 14, col_chassis_dark, mat_carbine_chrome, 2);
    // Muzzle compensator ports
    add_box(scatter_verts, {-0.026f, -0.028f, -0.45f}, {0.026f, 0.036f, -0.42f}, col_scatter_shroud, mat_carbine_armor);
    add_cylinder(scatter_verts, {0.0f, 0.022f, -0.46f}, 0.010f, 0.025f, 10, col_scatter_amber, mat_carbine_emissive, 2);
    add_cylinder(scatter_verts, {0.0f, -0.014f, -0.46f}, 0.010f, 0.025f, 10, col_scatter_amber, mat_carbine_emissive, 2);

    // Vented Heat Radiator Plates along Barrels
    add_box(scatter_verts, {-0.030f, -0.004f, -0.38f}, {0.030f, 0.012f, -0.18f}, col_suit_accent, mat_carbine_armor);
    for (int vent = 0; vent < 3; ++vent) {
        float vz = -0.22f - vent * 0.055f;
        add_box(scatter_verts, {-0.032f, -0.002f, vz - 0.012f}, {-0.028f, 0.010f, vz + 0.012f}, col_scatter_flame, mat_carbine_emissive);
        add_box(scatter_verts, { 0.028f, -0.002f, vz - 0.012f}, { 0.032f, 0.010f, vz + 0.012f}, col_scatter_flame, mat_carbine_emissive);
    }

    // 5.4 Heavy Pump Fore-End Slide & Grip
    add_box(scatter_verts, {-0.028f, -0.058f, -0.32f}, {0.028f, -0.028f, -0.20f}, col_scatter_grip, mat_carbine_rubber);
    // Pistol grip
    glm::mat4 scat_grip = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.088f, 0.03f));
    scat_grip = glm::rotate(scat_grip, glm::radians(22.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    add_transformed_box(scatter_verts, scat_grip, glm::vec3(0.016f, 0.055f, 0.024f), col_scatter_grip, mat_carbine_rubber);

    // 5.5 Demolitionist Forearms, Gauntlets & Hands Holding Scattergun
    glm::vec3 r_scat_elbow(0.24f, -0.32f, 0.32f);
    glm::vec3 r_scat_wrist(0.06f, -0.15f, 0.10f);
    glm::vec3 r_scat_palm(0.0f, -0.088f, 0.03f);
    add_forearm_and_gauntlet(scatter_verts, r_scat_elbow, r_scat_wrist, r_scat_palm, col_suit_arm, col_suit_accent, col_suit_glove, true);

    // Left arm and hand gripping pump fore-end slide
    glm::vec3 l_scat_elbow(-0.28f, -0.30f, 0.18f);
    glm::vec3 l_scat_wrist(-0.12f, -0.12f, -0.12f);
    glm::vec3 l_scat_palm(-0.032f, -0.060f, -0.26f);
    add_forearm_and_gauntlet(scatter_verts, l_scat_elbow, l_scat_wrist, l_scat_palm, col_suit_arm, col_suit_accent, col_suit_glove, false);
    for (int lf = 0; lf < 4; ++lf) {
        float lz = -0.23f - lf * 0.015f;
        add_capsule(scatter_verts, l_scat_palm + glm::vec3(-0.005f, 0.005f, lz - l_scat_palm.z), glm::vec3(0.026f, -0.048f, lz), 0.008f, 0.007f, 6, col_suit_accent, mat_rubber);
    }

    m_scattergun_count = scatter_verts.size();

    glGenVertexArrays(1, &m_scattergun_vao);
    glGenBuffers(1, &m_scattergun_vbo);
    glBindVertexArray(m_scattergun_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_scattergun_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_scattergun_count * sizeof(ViewmodelVertex), scatter_verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, material));
    glBindVertexArray(0);

    // =============================================================
    // 6. FIRST-PERSON NEEDLER RAILGUN (SCOUT MARKSMAN SNIPER)
    // =============================================================
    std::vector<ViewmodelVertex> rail_verts;
    glm::vec4 col_rail_body(0.12f, 0.14f, 0.15f, 1.0f);        // Carbon weave chassis
    glm::vec4 col_rail_conductors(0.75f, 0.80f, 0.85f, 1.0f);  // Chrome superconductor rails
    glm::vec4 col_rail_emerald(0.15f, 1.0f, 0.45f, 1.0f);     // Superconducting emerald magnetic rings
    glm::vec4 col_rail_optic(0.08f, 0.92f, 0.50f, 0.75f);      // Emerald sniper scope lens
    glm::vec4 col_rail_grip(0.08f, 0.09f, 0.10f, 1.0f);

    // 6.1 Slender Precision Receiver
    add_box(rail_verts, {-0.022f, -0.038f, -0.22f}, {0.022f, 0.038f, 0.10f}, col_rail_body, mat_carbine_armor);
    add_box(rail_verts, {-0.018f, -0.030f, 0.10f}, {0.018f, 0.030f, 0.24f}, col_rail_body, mat_carbine_armor); // Slender stock
    add_box(rail_verts, {-0.020f, -0.050f, 0.22f}, {0.020f, 0.035f, 0.26f}, col_rail_grip, mat_carbine_rubber); // Cheekrest/butt

    // 6.2 Extended Dual Magnetic Accelerator Guide Rails
    add_cylinder(rail_verts, {-0.010f, 0.005f, -0.20f}, 0.007f, 0.46f, 10, col_rail_conductors, mat_carbine_chrome, 2);
    add_cylinder(rail_verts, { 0.010f, 0.005f, -0.20f}, 0.007f, 0.46f, 10, col_rail_conductors, mat_carbine_chrome, 2);
    // Center Needle Chamber Core
    add_cylinder(rail_verts, {0.0f, 0.005f, -0.20f}, 0.004f, 0.48f, 10, col_rail_emerald, mat_carbine_emissive, 2);

    // 5 Glowing Superconductor Field Rings along the barrel
    for (int ring = 0; ring < 5; ++ring) {
        float rz = -0.24f - ring * 0.085f;
        add_box(rail_verts, {-0.020f, -0.005f, rz - 0.010f}, {0.020f, 0.015f, rz + 0.010f}, col_rail_emerald, mat_carbine_emissive);
    }

    // Needle Muzzle Stabilizer Tip
    add_cylinder(rail_verts, {0.0f, 0.005f, -0.66f}, 0.014f, 0.035f, 12, col_rail_conductors, mat_carbine_chrome, 2);

    // 6.3 Elevated High-Precision Sniper Scope
    add_box(rail_verts, {-0.008f, 0.038f, -0.14f}, {0.008f, 0.058f, -0.02f}, col_chassis_dark, mat_carbine_armor); // Mount
    add_cylinder(rail_verts, {0.0f, 0.070f, -0.22f}, 0.015f, 0.24f, 14, col_rail_body, mat_carbine_armor, 2);      // Scope tube
    add_cylinder(rail_verts, {0.0f, 0.070f, -0.22f}, 0.018f, 0.025f, 14, col_chassis_dark, mat_carbine_chrome, 2); // Objective bell
    add_cylinder(rail_verts, {0.0f, 0.070f, 0.00f},  0.017f, 0.025f, 14, col_chassis_dark, mat_carbine_chrome, 2); // Ocular bell
    // Lenses
    add_cylinder(rail_verts, {0.0f, 0.070f, -0.225f}, 0.013f, 0.006f, 12, col_rail_optic, mat_carbine_glass, 2);
    add_cylinder(rail_verts, {0.0f, 0.070f, 0.020f},  0.013f, 0.006f, 12, col_rail_optic, mat_carbine_glass, 2);

    // 6.4 Linear Needle Battery Cell
    add_box(rail_verts, {-0.014f, -0.115f, -0.10f}, {0.014f, -0.035f, -0.04f}, col_rail_body, mat_carbine_armor);
    add_box(rail_verts, {-0.015f, -0.105f, -0.08f}, {0.015f, -0.055f, -0.06f}, col_rail_emerald, mat_carbine_emissive);

    // 6.5 Pistol Grip & Scout Gauntleted Hands
    glm::mat4 rail_grip = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.080f, 0.02f));
    rail_grip = glm::rotate(rail_grip, glm::radians(18.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    add_transformed_box(rail_verts, rail_grip, glm::vec3(0.013f, 0.052f, 0.020f), col_rail_grip, mat_carbine_rubber);

    // 6.5 Scout Forearms, Gauntlets & Hands Holding Railgun
    glm::vec3 r_rail_elbow(0.24f, -0.32f, 0.32f);
    glm::vec3 r_rail_wrist(0.06f, -0.14f, 0.09f);
    glm::vec3 r_rail_palm(0.0f, -0.080f, 0.02f);
    add_forearm_and_gauntlet(rail_verts, r_rail_elbow, r_rail_wrist, r_rail_palm, col_suit_arm, col_suit_accent, col_suit_glove, true);

    // Left hand steadying forward shroud
    glm::vec3 l_rail_elbow(-0.28f, -0.29f, 0.16f);
    glm::vec3 l_rail_wrist(-0.12f, -0.10f, -0.14f);
    glm::vec3 l_rail_palm(-0.028f, -0.042f, -0.28f);
    add_forearm_and_gauntlet(rail_verts, l_rail_elbow, l_rail_wrist, l_rail_palm, col_suit_arm, col_suit_accent, col_suit_glove, false);
    for (int lf = 0; lf < 4; ++lf) {
        float lz = -0.26f - lf * 0.012f;
        add_capsule(rail_verts, l_rail_palm + glm::vec3(-0.004f, 0.004f, lz - l_rail_palm.z), glm::vec3(0.018f, -0.032f, lz), 0.006f, 0.005f, 6, col_suit_accent, mat_rubber);
    }

    m_railgun_count = rail_verts.size();

    glGenVertexArrays(1, &m_railgun_vao);
    glGenBuffers(1, &m_railgun_vbo);
    glBindVertexArray(m_railgun_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_railgun_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_railgun_count * sizeof(ViewmodelVertex), rail_verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, material));
    glBindVertexArray(0);

    // =============================================================
    // 5. TACTICAL REMOTE DETONATOR (DEMOLITION SATCHEL CHARGES)
    // (Handheld ruggedized industrial radio clacker & plunger)
    // =============================================================
    std::vector<ViewmodelVertex> det_verts;
    det_verts.reserve(2048);

    glm::vec4 col_det_casing(0.18f, 0.20f, 0.23f, 1.0f);
    glm::vec4 col_det_grip(0.08f, 0.08f, 0.10f, 1.0f);
    glm::vec4 col_det_screen(0.05f, 0.18f, 0.22f, 1.0f);
    glm::vec4 col_det_led_armed(1.0f, 0.15f, 0.08f, 1.0f);
    glm::vec4 col_plunger(0.95f, 0.20f, 0.15f, 1.0f);

    // A. Main Ruggedized Handset Body
    add_box(det_verts, {-0.042f, -0.095f, -0.024f}, {0.042f, 0.055f, 0.024f}, col_det_casing, mat_armor);

    // Ergonomic side bumpers / rubber grips
    add_box(det_verts, {-0.046f, -0.090f, -0.020f}, {-0.038f, 0.045f, 0.020f}, col_det_grip, mat_rubber);
    add_box(det_verts, { 0.038f, -0.090f, -0.020f}, { 0.046f, 0.045f, 0.020f}, col_det_grip, mat_rubber);

    // Side grip ribbed notches
    for (int g = 0; g < 4; ++g) {
        float gy = -0.070f + static_cast<float>(g) * 0.032f;
        add_box(det_verts, {-0.048f, gy - 0.005f, -0.015f}, {-0.044f, gy + 0.005f, 0.015f}, col_chassis_dark, mat_rubber);
        add_box(det_verts, { 0.044f, gy - 0.005f, -0.015f}, { 0.048f, gy + 0.005f, 0.015f}, col_chassis_dark, mat_rubber);
    }

    // Hazard yellow/black diagonal side safety stripes
    add_box(det_verts, {-0.043f, 0.035f, -0.025f}, {-0.035f, 0.048f, 0.025f}, col_hazard_yellow, mat_metal);
    add_box(det_verts, { 0.035f, 0.035f, -0.025f}, { 0.043f, 0.048f, 0.025f}, col_hazard_yellow, mat_metal);

    // B. Top Antenna Mast
    add_cylinder(det_verts, {0.028f, 0.055f, -0.005f}, 0.006f, 0.015f, 8, col_brass, mat_metal, 1);
    add_cylinder(det_verts, {0.028f, 0.070f, -0.005f}, 0.0035f, 0.085f, 8, col_steel_bright, mat_metal, 1);
    add_cylinder(det_verts, {0.028f, 0.155f, -0.005f}, 0.0055f, 0.010f, 8, col_brass, mat_metal, 1);

    // C. Safety Protective Guard Bars (protecting the plunger)
    add_box(det_verts, {-0.028f, 0.055f, -0.020f}, {-0.022f, 0.088f, 0.020f}, col_steel_bright, mat_metal);
    add_box(det_verts, { 0.010f, 0.055f, -0.020f}, { 0.016f, 0.088f, 0.020f}, col_steel_bright, mat_metal);
    add_box(det_verts, {-0.028f, 0.084f, 0.014f}, { 0.016f, 0.088f, 0.020f}, col_steel_bright, mat_metal);

    // D. Front Telemetry Display & Armed Status Beacon
    add_box(det_verts, {-0.034f, -0.015f, -0.026f}, {0.034f, 0.045f, -0.024f}, col_chassis_dark, mat_armor);
    add_box(det_verts, {-0.030f, -0.010f, -0.028f}, {0.030f, 0.040f, -0.025f}, col_det_screen, mat_screen_bg);
    add_box(det_verts, {-0.026f,  0.022f, -0.029f}, {0.026f, 0.025f, -0.027f}, glm::vec4(0.15f, 0.85f, 0.95f, 1.0f), mat_led_emissive);
    add_box(det_verts, {-0.026f,  0.005f, -0.029f}, {0.026f, 0.008f, -0.027f}, glm::vec4(0.15f, 0.85f, 0.95f, 1.0f), mat_led_emissive);

    // Armed Beacon LED (Pulsating warning light)
    add_box(det_verts, {-0.012f, 0.025f, -0.031f}, {0.012f, 0.037f, -0.027f}, col_det_led_armed, mat_led_emissive);

    // Rotary frequency selector dial
    add_cylinder(det_verts, {0.0f, -0.055f, -0.025f}, 0.012f, 0.010f, 10, col_brass, mat_metal, 2);

    // E. Suited Arm and Glove Holding Detonator
    glm::vec3 r_det_elbow(0.24f, -0.32f, 0.30f);
    glm::vec3 r_det_wrist(0.08f, -0.15f, 0.10f);
    glm::vec3 r_det_palm(0.02f, -0.05f, 0.01f);
    add_forearm_and_gauntlet(det_verts, r_det_elbow, r_det_wrist, r_det_palm, col_suit_arm, col_suit_accent, col_suit_glove, true);
    add_capsule(det_verts, {0.040f, -0.040f, 0.02f}, {0.042f, -0.035f, -0.02f}, 0.014f, 0.012f, 6, col_suit_glove, mat_rubber);
    add_capsule(det_verts, {0.040f, -0.070f, 0.02f}, {0.042f, -0.065f, -0.02f}, 0.014f, 0.012f, 6, col_suit_glove, mat_rubber);
    add_capsule(det_verts, {-0.035f, 0.010f, 0.03f}, {-0.010f, 0.050f, 0.01f}, 0.015f, 0.012f, 6, col_suit_glove, mat_rubber);

    m_detonator_count = det_verts.size();
    glGenVertexArrays(1, &m_detonator_vao);
    glGenBuffers(1, &m_detonator_vbo);
    glBindVertexArray(m_detonator_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_detonator_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_detonator_count * sizeof(ViewmodelVertex), det_verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, material));
    glBindVertexArray(0);

    // F. Detonator Plunger Button
    std::vector<ViewmodelVertex> plunger_verts;
    plunger_verts.reserve(256);
    add_cylinder(plunger_verts, {-0.006f, 0.055f, 0.0f}, 0.012f, 0.024f, 10, col_plunger, mat_armor, 1);
    add_cylinder(plunger_verts, {-0.006f, 0.077f, 0.0f}, 0.0135f, 0.005f, 10, col_brass, mat_metal, 1);
    add_box(plunger_verts, {-0.014f, 0.081f, -0.008f}, {0.002f, 0.083f, 0.008f}, glm::vec4(1.0f, 0.85f, 0.20f, 1.0f), mat_led_emissive);

    m_plunger_count = plunger_verts.size();
    glGenVertexArrays(1, &m_plunger_vao);
    glGenBuffers(1, &m_plunger_vbo);
    glBindVertexArray(m_plunger_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_plunger_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_plunger_count * sizeof(ViewmodelVertex), plunger_verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ViewmodelVertex), (void*)offsetof(ViewmodelVertex, material));
    glBindVertexArray(0);
}

void ViewModel::render(
    float dt,
    float aspect,
    bool is_drilling,
    bool is_in_range,
    ToolSlot active_tool,
    const glm::vec3& drill_target_pos,
    bool is_firing,
    bool is_crouching,
    bool is_reloading,
    float reload_progress,
    bool has_placed_charge,
    bool is_melee_shoving,
    float movement_speed,
    bool is_grounded,
    float melee_progress,
    bool is_aiming,
    float zoom_progress
) {
    if (m_chassis_vao == 0 || m_carbine_vao == 0) return;

    m_total_time += dt;
    m_is_reloading = is_reloading;
    m_reload_progress = reload_progress;

    // Advance walking bob phase only while actively moving on solid ground
    bool is_moving = is_grounded && (movement_speed > 0.15f);
    if (is_moving) {
        // Frequency scales smoothly with delver movement speed (4.5 Hz at nominal 7 m/s sprint/run)
        float bob_freq = glm::clamp(movement_speed / 7.0f, 0.75f, 1.6f) * 4.5f;
        m_walk_bob_phase += dt * bob_freq;
        if (m_walk_bob_phase > 6283.1853f) {
            m_walk_bob_phase -= 6283.1853f;
        }
        // Smoothly ramp in walk bob weight
        m_walk_bob_weight = glm::mix(m_walk_bob_weight, 1.0f, 1.0f - std::exp(-10.0f * dt));
    } else {
        // Smoothly decay walk bob weight to zero when stationary or airborne
        m_walk_bob_weight = glm::mix(m_walk_bob_weight, 0.0f, 1.0f - std::exp(-10.0f * dt));
        if (m_walk_bob_weight < 0.001f) {
            m_walk_bob_weight = 0.0f;
        }
    }

    if (active_tool != m_last_tool) {
        on_tool_switched();
        m_last_tool = active_tool;
    }

    // Defensive Quick Melee Timing & Progress Calculation
    if (is_melee_shoving && !m_was_melee_shoving) {
        m_shove_timer = 0.35f;
    }
    m_was_melee_shoving = is_melee_shoving;

    float melee_p = 0.0f;
    if (melee_progress > 0.0f) {
        melee_p = std::clamp(melee_progress, 0.0f, 1.0f);
        m_shove_timer = (1.0f - melee_p) * 0.35f;
    } else if (m_shove_timer > 0.0f) {
        m_shove_timer = std::max(0.0f, m_shove_timer - dt);
        melee_p = std::clamp(1.0f - (m_shove_timer / 0.35f), 0.0f, 1.0f);
    }

    float switch_dip_y   = 0.0f;
    float switch_push_z  = 0.0f;
    float switch_roll_deg = 0.0f;
    if (m_switch_timer > 0.0f) {
        m_switch_timer -= dt;
        float progress = 1.0f - (std::max(0.0f, m_switch_timer) / 0.20f); // 0→1 over 0.2s
        if (progress < 0.5f) {
            // Phase 1: Lower old weapon out (0.1s dip)
            float p    = progress / 0.5f;           // 0→1 over first 0.1s
            float ease = p * p;                     // Quadratic ease-in
            switch_dip_y    = -0.22f * ease;        // Drop down
            switch_push_z   =  0.05f * ease;        // Pull slightly toward screen
            switch_roll_deg = -15.0f * ease;        // Tilt CW as it drops away
        } else {
            // Phase 2: Raise new weapon in (0.1s raise)
            float p    = (progress - 0.5f) / 0.5f; // 0→1 over second 0.1s
            float ease = 1.0f - (1.0f - p) * (1.0f - p); // Quadratic ease-out
            switch_dip_y    = -0.22f * (1.0f - ease);     // Rise from below
            switch_push_z   =  0.05f * (1.0f - ease);     // Settle forward
            switch_roll_deg =  8.0f * (1.0f - ease) * (1.0f - ease);
        }
    }

    // 1. Dedicated Projection Matrix (FOV = 68.0f down to 60.0f in ADS, near = 0.05f, far = 10.0f)
    float vm_fov = glm::mix(68.0f, 60.0f, zoom_progress);
    glm::mat4 proj = glm::perspective(glm::radians(vm_fov), aspect, 0.05f, 10.0f);
    glm::mat4 view = glm::mat4(1.0f);

    // 2. Procedural walking bob & subtle idle breathing sway
    float crouch_offset_y = is_crouching ? -0.08f : 0.0f;
    float crouch_offset_z = is_crouching ? -0.05f : 0.0f;
    float sway_scale = is_crouching ? 0.5f : 1.0f;

    // Locomotion bob (active strictly while moving)
    float walk_bob_x = std::sin(m_walk_bob_phase) * 0.008f * sway_scale * m_walk_bob_weight;
    float walk_bob_y = std::cos(m_walk_bob_phase * 2.0f) * 0.006f * sway_scale * m_walk_bob_weight;

    // Idle breathing sway: very subtle 0.25 Hz physiological rise/fall, blends out during locomotion
    float idle_weight = 1.0f - m_walk_bob_weight;
    float breath_x = std::sin(m_total_time * 1.5f) * 0.0004f * idle_weight;
    float breath_y = (std::sin(m_total_time * 1.5f) * 0.5f + 0.5f) * 0.0008f * idle_weight;

    float ads_bob_scale = 1.0f - 0.75f * zoom_progress;
    float lissajous_x = (walk_bob_x + breath_x) * ads_bob_scale;
    float lissajous_y = (walk_bob_y + breath_y) * ads_bob_scale;

    glDisable(GL_CULL_FACE);

    // Dedicated depth pass with glDepthRange(0.0f, 0.15f) so viewmodel never clips cavern walls
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0.0f, 0.15f);

    m_shader.use();
    m_shader.set_mat4("uProjection", proj);
    m_shader.set_mat4("uView", view);

    if (active_tool == ToolSlot::CombatWeapon) {
        // =============================================================
        // COMBAT VIEWMODEL: CLASS ARHETYPE FIREARMS
        // (Demolitionist: Magma Scattergun, Vanguard: Plasma Carbine, Scout: Needler Railgun)
        // =============================================================
        // Procedural recoil exponential spring decay (decaying back over ~0.12s at rate 22.0f)
        m_recoil_offset = glm::mix(m_recoil_offset, glm::vec3(0.0f), 1.0f - std::exp(-22.0f * dt));
        m_recoil_pitch  = glm::mix(m_recoil_pitch, 0.0f, 1.0f - std::exp(-22.0f * dt));

        if (is_firing) {
            m_muzzle_flash_timer = 0.05f;
            if (m_character_class == CharacterClass::Scout) {
                // Needler Railgun Firing: offset camera-space viewmodel backwards by Z + 0.05m and pitch up by +3.5 deg
                m_recoil_offset.z += 0.05f;
                m_recoil_pitch += 3.5f;
            } else if (m_character_class == CharacterClass::Demolitionist) {
                m_recoil_offset.z += 0.075f;
                m_recoil_pitch += 5.2f;
            } else {
                m_recoil_offset.z += 0.04f;
                m_recoil_pitch += 2.8f;
            }
        }
        if (m_muzzle_flash_timer > 0.0f) {
            m_muzzle_flash_timer -= dt;
        }

        float recoil_kick = m_recoil_offset.z;
        float kick_pitch  = m_recoil_pitch;

        // Multi-Stage Procedural Reload Animation (3-Phase Keyframe Sequence)
        float reload_offset_x = 0.0f;
        float reload_offset_y = 0.0f;
        float reload_offset_z = 0.0f;
        float reload_yaw_deg = 0.0f;
        float reload_pitch_deg = 0.0f;
        float reload_roll_deg = 0.0f;
        float reload_emissive_boost = 0.0f;

        if (is_reloading) {
            float p = std::clamp(reload_progress, 0.0f, 1.0f);
            if (p < 0.30f) {
                // Phase 1: Drop & Eject (0.0s – 0.3s): Gun angles down 25 deg, expended energy cell pops out and falls with gravity
                float t = p / 0.30f; // 0 -> 1
                float ease = t * t * (3.0f - 2.0f * t); // smoothstep
                reload_offset_x = -0.04f * ease;
                reload_offset_y = -0.15f * ease;
                reload_offset_z =  0.06f * ease;
                reload_roll_deg = -18.0f * ease;
                reload_pitch_deg = 25.0f * ease; // Gun angles down 25 deg
                reload_yaw_deg = 6.0f * ease;
            } else if (p < 0.70f) {
                // Phase 2: Insert Cell (0.3s – 0.7s): Left suit hand reaches from off-screen, slams fresh glowing cell into receiver
                float t = (p - 0.30f) / 0.40f; // 0 -> 1
                reload_offset_x = -0.04f * (1.0f - t * 0.25f);
                reload_offset_y = -0.15f + t * 0.05f;
                reload_offset_z =  0.06f - t * 0.02f;
                reload_roll_deg = -18.0f * (1.0f - t * 0.35f);
                reload_pitch_deg = 25.0f * (1.0f - t * 0.35f);
                reload_yaw_deg = 6.0f * (1.0f - t * 0.3f);

                // Heavy mechanical latch slam impulse at t in [0.55, 0.80]
                if (t >= 0.55f && t <= 0.80f) {
                    float slam_t = (t - 0.55f) / 0.25f; // 0 -> 1
                    float impulse = std::sin(slam_t * 3.14159f);
                    reload_offset_y += impulse * 0.04f;
                    reload_offset_z -= impulse * 0.045f;
                    reload_pitch_deg -= impulse * 7.0f;
                    reload_emissive_boost = impulse * 0.75f; // Glowing capacitor lock flash!
                }
            } else {
                // Phase 3: Rack & Ready (0.7s – 1.0s): Gun snaps back to hip/aim stance with subtle settling shake
                float t = (p - 0.70f) / 0.30f; // 0 -> 1
                float ease = 1.0f - (1.0f - t) * (1.0f - t);
                float return_factor = 1.0f - ease;
                // Subtle settling shake
                float settling_shake = std::sin(t * 22.0f) * std::exp(-t * 6.0f) * 0.007f;
                reload_offset_x = -0.028f * return_factor;
                reload_offset_y = -0.09f * return_factor + settling_shake;
                reload_offset_z =  0.04f * return_factor;
                reload_roll_deg = -12.0f * return_factor + settling_shake * 180.0f;
                reload_pitch_deg = 16.0f * return_factor;
                reload_yaw_deg = 4.0f * return_factor;
            }
        }

        // =============================================================
        // Procedural Melee Shove / Butt-Stroke Animation for Firearms
        // =============================================================
        float gun_melee_x = 0.0f;
        float gun_melee_y = 0.0f;
        float gun_melee_z = 0.0f;
        float gun_melee_pitch = 0.0f;
        float gun_melee_yaw = 0.0f;
        float gun_melee_roll = 0.0f;
        float gun_melee_emissive = 0.0f;

        if (melee_p > 0.0f) {
            float p = melee_p;
            if (p < 0.22f) {
                // Phase 1: Wind-up / Coiling back toward delver's chest & shoulder (0.0 -> 0.22)
                float t = p / 0.22f;
                float ease = t * t;
                gun_melee_x = 0.035f * ease;
                gun_melee_y = 0.030f * ease;
                gun_melee_z = 0.070f * ease; // Pull back toward camera
                gun_melee_pitch = 10.0f * ease;
                gun_melee_yaw = 20.0f * ease;
                gun_melee_roll = 15.0f * ease;
            } else if (p < 0.45f) {
                // Phase 2: Explosive Forward Strike & Butt-Stroke Slash (0.22 -> 0.45)
                float t = (p - 0.22f) / 0.23f;
                float snap = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); // Cubic snap forward

                // Weapon archetype strike variations
                float target_z = -0.25f;
                float target_x = -0.09f;
                float target_roll = -22.0f;
                if (m_character_class == CharacterClass::Scout) {
                    // Needler Railgun: Long bayonet-style forward spear thrust
                    target_z = -0.29f;
                    target_x = -0.06f;
                    target_roll = -16.0f;
                } else if (m_character_class == CharacterClass::Demolitionist) {
                    // Magma Scattergun: Heavy bludgeon horizontal sweep
                    target_z = -0.23f;
                    target_x = -0.12f;
                    target_roll = -28.0f;
                }

                gun_melee_x = glm::mix(0.035f, target_x, snap);
                gun_melee_y = glm::mix(0.030f, -0.055f, snap);
                gun_melee_z = glm::mix(0.070f, target_z, snap); // Drives deeply forward into the screen (-Z)
                gun_melee_pitch = glm::mix(10.0f, -16.0f, snap);
                gun_melee_yaw = glm::mix(20.0f, -24.0f, snap);
                gun_melee_roll = glm::mix(15.0f, target_roll, snap);

                if (t >= 0.50f) {
                    float hit_pulse = std::sin((t - 0.50f) / 0.50f * 3.14159f);
                    gun_melee_emissive = 0.45f * hit_pulse;
                }
            } else if (p < 0.58f) {
                // Phase 3: Impact Shudder & Kinetic Shock Deceleration (0.45 -> 0.58)
                float t = (p - 0.45f) / 0.13f;
                float shake = std::sin(t * 38.0f) * std::exp(-t * 6.0f) * 0.007f;
                float rebound_z = 0.035f * t;

                float target_z = (m_character_class == CharacterClass::Scout) ? -0.29f : ((m_character_class == CharacterClass::Demolitionist) ? -0.23f : -0.25f);
                float target_x = (m_character_class == CharacterClass::Demolitionist) ? -0.12f : ((m_character_class == CharacterClass::Scout) ? -0.06f : -0.09f);
                float target_roll = (m_character_class == CharacterClass::Demolitionist) ? -28.0f : ((m_character_class == CharacterClass::Scout) ? -16.0f : -22.0f);

                gun_melee_x = target_x + shake * 0.5f;
                gun_melee_y = -0.055f + shake;
                gun_melee_z = target_z + rebound_z + shake;
                gun_melee_pitch = -16.0f + 2.0f * t;
                gun_melee_yaw = -24.0f + 3.0f * t;
                gun_melee_roll = target_roll + 3.0f * t;
                gun_melee_emissive = 0.35f * (1.0f - t);
            } else {
                // Phase 4: Fluid Return to Ready Hip/Aim Stance (0.58 -> 1.0)
                float t = (p - 0.58f) / 0.42f;
                float ret = (1.0f - t) * (1.0f - t);

                float start_z = ((m_character_class == CharacterClass::Scout) ? -0.29f : ((m_character_class == CharacterClass::Demolitionist) ? -0.23f : -0.25f)) + 0.035f;
                float start_x = (m_character_class == CharacterClass::Demolitionist) ? -0.12f : ((m_character_class == CharacterClass::Scout) ? -0.06f : -0.09f);
                float start_roll = ((m_character_class == CharacterClass::Demolitionist) ? -28.0f : ((m_character_class == CharacterClass::Scout) ? -16.0f : -22.0f)) + 3.0f;

                gun_melee_x = start_x * ret;
                gun_melee_y = -0.055f * ret;
                gun_melee_z = start_z * ret;
                gun_melee_pitch = -14.0f * ret;
                gun_melee_yaw = -21.0f * ret;
                gun_melee_roll = start_roll * ret;
            }
        }

        // Aim-Down-Sights (ADS) centering alignment:
        float target_gun_x = 0.0f;
        float target_gun_y = -0.10f;
        float target_gun_z = -0.30f;
        float target_gun_yaw = 0.0f;
        float target_gun_pitch = 0.0f;
        float target_gun_roll = 0.0f;

        float base_gun_x = glm::mix(0.15f, target_gun_x, zoom_progress);
        float base_gun_y = glm::mix(-0.14f, target_gun_y, zoom_progress);
        float base_gun_z = glm::mix(-0.34f, target_gun_z, zoom_progress);
        float base_gun_yaw = glm::mix(-14.0f, target_gun_yaw, zoom_progress);
        float base_gun_pitch = glm::mix(2.0f, target_gun_pitch, zoom_progress);
        float base_gun_roll = glm::mix(3.0f, target_gun_roll, zoom_progress);

        glm::vec3 gun_pos(
            base_gun_x + lissajous_x + reload_offset_x + gun_melee_x,
            base_gun_y + lissajous_y + switch_dip_y + crouch_offset_y + reload_offset_y + gun_melee_y,
            base_gun_z + recoil_kick + switch_push_z + crouch_offset_z + reload_offset_z + gun_melee_z
        );

        glm::mat4 gun_model = glm::translate(glm::mat4(1.0f), gun_pos);
        gun_model = glm::rotate(gun_model, glm::radians(base_gun_yaw + reload_yaw_deg + gun_melee_yaw), glm::vec3(0.0f, 1.0f, 0.0f));           // Inward yaw
        gun_model = glm::rotate(gun_model, glm::radians(base_gun_pitch - kick_pitch + reload_pitch_deg + gun_melee_pitch), glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
        gun_model = glm::rotate(gun_model, glm::radians(base_gun_roll + switch_roll_deg + reload_roll_deg + gun_melee_roll), glm::vec3(0.0f, 0.0f, 1.0f)); // Cant + switch roll + reload roll

        m_shader.set_mat4("uModel", gun_model);
        float emissive = (m_muzzle_flash_timer > 0.0f) ? 0.95f : (0.35f + reload_emissive_boost + gun_melee_emissive);
        m_shader.set_float("uEmissive", emissive);

        if (m_character_class == CharacterClass::Demolitionist) {
            glBindVertexArray(m_scattergun_vao);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_scattergun_count));
        } else if (m_character_class == CharacterClass::Scout) {
            glBindVertexArray(m_railgun_vao);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_railgun_count));
        } else {
            glBindVertexArray(m_carbine_vao);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_carbine_count));
        }
        glBindVertexArray(0);
    } else if (active_tool == ToolSlot::DemolitionCharge) {
        // =============================================================
        // DEMOLITION VIEWMODEL: TACTICAL REMOTE DETONATOR
        // (Handheld clacker with armed safety beacon and tactile plunger)
        // =============================================================
        // Procedural Melee Animation for Tactical Detonator: Gauntlet Clacker Hammer Punch
        float det_melee_x = 0.0f;
        float det_melee_y = 0.0f;
        float det_melee_z = 0.0f;
        float det_melee_pitch = 0.0f;
        float det_melee_yaw = 0.0f;
        float det_melee_roll = 0.0f;
        float det_melee_plunger = 0.0f;
        float det_melee_emissive = 0.0f;

        if (melee_p > 0.0f) {
            float p = melee_p;
            if (p < 0.20f) {
                // Phase 1: Coiling back like a brass-knuckle fist (0.0 -> 0.20)
                float t = p / 0.20f;
                float ease = t * t;
                det_melee_x = 0.035f * ease;
                det_melee_y = 0.045f * ease;
                det_melee_z = 0.080f * ease;
                det_melee_pitch = 16.0f * ease;
                det_melee_yaw = 14.0f * ease;
                det_melee_roll = 12.0f * ease;
            } else if (p < 0.42f) {
                // Phase 2: Explosive Forward Piston Hammer Punch (0.20 -> 0.42)
                float t = (p - 0.20f) / 0.22f;
                float snap = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                det_melee_x = glm::mix(0.035f, -0.120f, snap); // Drives straight into center screen crosshair
                det_melee_y = glm::mix(0.045f, -0.020f, snap);
                det_melee_z = glm::mix(0.080f, -0.260f, snap); // Deep forward punch
                det_melee_pitch = glm::mix(16.0f, -14.0f, snap);
                det_melee_yaw = glm::mix(14.0f, -12.0f, snap);
                det_melee_roll = glm::mix(12.0f, 22.0f, snap); // Knuckles horizontal locked
                det_melee_plunger = -0.022f * std::sin(t * 3.14159f); // Plunger depresses under force
                if (t >= 0.50f) {
                    float hit_pulse = std::sin((t - 0.50f) / 0.50f * 3.14159f);
                    det_melee_emissive = 0.70f * hit_pulse; // Armed beacon shock flash
                }
            } else if (p < 0.58f) {
                // Phase 3: Punch Rebound & Settle (0.42 -> 0.58)
                float t = (p - 0.42f) / 0.16f;
                float shake = std::sin(t * 35.0f) * std::exp(-t * 5.0f) * 0.006f;
                det_melee_x = -0.120f + 0.025f * t + shake * 0.5f;
                det_melee_y = -0.020f + shake;
                det_melee_z = -0.260f + 0.055f * t + shake;
                det_melee_pitch = -14.0f + 3.0f * t;
                det_melee_yaw = -12.0f + 3.0f * t;
                det_melee_roll = 22.0f - 4.0f * t;
                det_melee_emissive = 0.50f * (1.0f - t);
            } else {
                // Phase 4: Settle back to handheld inspection posture (0.58 -> 1.0)
                float t = (p - 0.58f) / 0.42f;
                float ret = (1.0f - t) * (1.0f - t);
                det_melee_x = -0.095f * ret;
                det_melee_y = -0.020f * ret;
                det_melee_z = -0.205f * ret;
                det_melee_pitch = -11.0f * ret;
                det_melee_yaw = -9.0f * ret;
                det_melee_roll = 18.0f * ret;
            }
        }

        float plunger_offset = (is_firing ? -0.016f : 0.0f) + det_melee_plunger;

        glm::vec3 det_pos(
            0.14f + lissajous_x + det_melee_x,
            -0.13f + lissajous_y + switch_dip_y + crouch_offset_y + det_melee_y,
            -0.32f + switch_push_z + crouch_offset_z + det_melee_z
        );

        glm::mat4 det_model = glm::translate(glm::mat4(1.0f), det_pos);
        det_model = glm::rotate(det_model, glm::radians(-12.0f + det_melee_yaw), glm::vec3(0.0f, 1.0f, 0.0f)); // Inward yaw
        det_model = glm::rotate(det_model, glm::radians(10.0f + det_melee_pitch), glm::vec3(1.0f, 0.0f, 0.0f));  // Upward pitch for LCD readability
        det_model = glm::rotate(det_model, glm::radians(2.0f + switch_roll_deg + det_melee_roll), glm::vec3(0.0f, 0.0f, 1.0f));

        m_shader.set_mat4("uModel", det_model);
        float pulse = 0.5f + 0.5f * std::sin(m_total_time * 15.0f);
        float emissive = (has_placed_charge ? (0.7f + 0.8f * pulse) : 0.35f) + det_melee_emissive;
        m_shader.set_float("uEmissive", emissive);

        glBindVertexArray(m_detonator_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_detonator_count));

        glm::mat4 btn_model = glm::translate(det_model, glm::vec3(0.0f, plunger_offset, 0.0f));
        m_shader.set_mat4("uModel", btn_model);
        glBindVertexArray(m_plunger_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_plunger_count));

        glBindVertexArray(0);
    } else {
        // =============================================================
        // UTILITY VIEWMODEL: MINING DRILL RIG
        // =============================================================
        float recoil_z = 0.0f;
        float piston_z = 0.0f;
        float jitter_x = 0.0f;
        float jitter_y = 0.0f;

        if (is_drilling) {
            m_drill_rotation += 1800.0f * dt;
            if (m_drill_rotation > 360000.0f) m_drill_rotation -= 360000.0f;

            // Recoil oscillation along view vector (Z offset jitter +-0.03m at 45 Hz)
            recoil_z = std::sin(m_total_time * (2.0f * 3.14159265f * 45.0f)) * 0.030f;
            jitter_x = ((static_cast<float>(rand() % 100) / 100.0f) - 0.5f) * 0.003f;
            jitter_y = ((static_cast<float>(rand() % 100) / 100.0f) - 0.5f) * 0.003f;

            piston_z = std::sin(m_total_time * (2.0f * 3.14159265f * 45.0f)) * 0.030f;

            if (is_in_range && m_on_spark) {
                m_spark_timer += dt;
                if (m_spark_timer >= 0.040f) {
                    m_spark_timer = 0.0f;
                    glm::vec3 spark_origin = drill_target_pos;
                    glm::vec3 spark_dir(
                        ((rand() % 100) / 50.0f - 1.0f) * 1.8f,
                        ((rand() % 100) / 50.0f) * 2.2f + 0.4f,
                        ((rand() % 100) / 50.0f - 1.0f) * 1.8f
                    );
                    m_on_spark(spark_origin, spark_dir);
                }
            }
        } else {
            m_spark_timer = 0.0f;
        }

        // =============================================================
        // Procedural Melee Ram Animation for Heavy Mining Rig: Two-Handed Hydraulic Battering Ram
        // =============================================================
        float drill_melee_x = 0.0f;
        float drill_melee_y = 0.0f;
        float drill_melee_z = 0.0f;
        float drill_melee_pitch = 0.0f;
        float drill_melee_yaw = 0.0f;
        float drill_melee_roll = 0.0f;
        float drill_melee_piston_z = 0.0f;
        float drill_melee_emissive = 0.0f;

        if (melee_p > 0.0f) {
            float p = melee_p;
            if (p < 0.24f) {
                // Phase 1: Heavy Kinetic Wind-up & Rev Surge (0.0 -> 0.24)
                float t = p / 0.24f;
                float ease = t * t;
                drill_melee_x = -0.025f * ease;
                drill_melee_y = -0.060f * ease; // Drops into low power stance
                drill_melee_z = 0.110f * ease;  // Pulls heavy 50kg chassis back
                drill_melee_pitch = -14.0f * ease; // Drill snout angles upward
                drill_melee_yaw = 12.0f * ease;
                drill_melee_roll = 8.0f * ease;
                drill_melee_piston_z = -0.040f * ease; // Pistons retract under pressure
                m_drill_rotation += 4200.0f * dt * ease; // Motor spools up with high RPM whine!
            } else if (p < 0.48f) {
                // Phase 2: Massive Hydraulic Battering Ram Strike (0.24 -> 0.48)
                float t = (p - 0.24f) / 0.24f;
                float snap = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                drill_melee_x = glm::mix(-0.025f, -0.095f, snap);
                drill_melee_y = glm::mix(-0.060f, 0.075f, snap); // Powerful upward uppercut ram
                drill_melee_z = glm::mix(0.110f, -0.300f, snap); // Drives -0.30m forward into enemy!
                drill_melee_pitch = glm::mix(-14.0f, 18.0f, snap); // Plunges down into target
                drill_melee_yaw = glm::mix(12.0f, -14.0f, snap);
                drill_melee_roll = glm::mix(8.0f, -12.0f, snap);
                drill_melee_piston_z = glm::mix(-0.040f, 0.080f, snap); // Pistons slam forward to full extension!
                m_drill_rotation += 6500.0f * dt; // Violent rotational surge
                float hit_pulse = std::sin(t * 3.14159f);
                drill_melee_emissive = 0.75f * hit_pulse; // Superheated diamond auger glow

                // Impact sparks trigger at peak extension
                if (t >= 0.70f && t <= 0.85f && m_on_spark) {
                    glm::vec3 spark_dir(
                        ((rand() % 100) / 50.0f - 1.0f) * 2.0f,
                        ((rand() % 100) / 50.0f) * 2.0f + 0.5f,
                        -2.5f
                    );
                    m_on_spark(drill_target_pos, spark_dir);
                }
            } else if (p < 0.68f) {
                // Phase 3: Heavy Machinery Impact Chatter & Pneumatic Exhaust (0.48 -> 0.68)
                float t = (p - 0.48f) / 0.20f;
                float shake = std::sin(t * 42.0f) * std::exp(-t * 4.5f) * 0.012f;
                drill_melee_x = -0.095f + shake * 0.5f;
                drill_melee_y = 0.075f - 0.035f * t + shake;
                drill_melee_z = -0.300f + 0.075f * t + shake;
                drill_melee_pitch = 18.0f - 4.0f * t;
                drill_melee_yaw = -14.0f + 3.0f * t;
                drill_melee_roll = -12.0f + 3.0f * t;
                drill_melee_piston_z = 0.080f * (1.0f - t);
                m_drill_rotation += 3500.0f * dt * (1.0f - t);
                drill_melee_emissive = 0.45f * (1.0f - t);
            } else {
                // Phase 4: Delver Settles Weight Back to Neutral (0.68 -> 1.0)
                float t = (p - 0.68f) / 0.32f;
                float ret = (1.0f - t) * (1.0f - t);
                drill_melee_x = -0.095f * ret;
                drill_melee_y = 0.040f * ret;
                drill_melee_z = -0.225f * ret;
                drill_melee_pitch = 14.0f * ret;
                drill_melee_yaw = -11.0f * ret;
                drill_melee_roll = -9.0f * ret;
            }
        }

        glm::vec3 base_pos(
            0.18f + lissajous_x + jitter_x + drill_melee_x,
            -0.16f + lissajous_y + switch_dip_y + jitter_y + crouch_offset_y + drill_melee_y,
            -0.42f + recoil_z + switch_push_z + crouch_offset_z + drill_melee_z
        );

        glm::mat4 root_model = glm::translate(glm::mat4(1.0f), base_pos);
        root_model = glm::rotate(root_model, glm::radians(-22.0f + drill_melee_yaw), glm::vec3(0.0f, 1.0f, 0.0f)); // Inward yaw
        root_model = glm::rotate(root_model, glm::radians(3.0f + drill_melee_pitch), glm::vec3(1.0f, 0.0f, 0.0f));   // Pitch down
        root_model = glm::rotate(root_model, glm::radians(4.0f + switch_roll_deg + drill_melee_roll), glm::vec3(0.0f, 0.0f, 1.0f)); // Natural cant + switch roll

        // A. Render Chassis, Hands & Gauntlets
        m_shader.set_mat4("uModel", root_model);
        float chassis_emissive = (is_drilling ? (0.45f + 0.10f * m_drill_speed_tier) : 0.18f) + drill_melee_emissive * 0.5f;
        m_shader.set_float("uEmissive", chassis_emissive);

        glBindVertexArray(m_chassis_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_chassis_count));

        // B. Render Reciprocating Dual Pneumatic Pistons
        glm::mat4 piston_model = glm::translate(root_model, glm::vec3(0.0f, 0.0f, piston_z + drill_melee_piston_z));
        m_shader.set_mat4("uModel", piston_model);
        glBindVertexArray(m_piston_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_piston_count));

        // C. Render Rotating Fluted Spiral Auger Drill Bit
        glm::vec3 bit_origin(0.0f, 0.0f, -0.26f);
        glm::mat4 bit_model = root_model;
        bit_model = glm::translate(bit_model, bit_origin);
        bit_model = glm::rotate(bit_model, glm::radians(m_drill_rotation), glm::vec3(0.0f, 0.0f, 1.0f));

        m_shader.set_mat4("uModel", bit_model);
        float bit_emissive = (is_drilling ? (0.75f + 0.15f * m_drill_speed_tier) : (0.12f + 0.05f * m_drill_speed_tier)) + drill_melee_emissive;
        m_shader.set_float("uEmissive", bit_emissive);

        glBindVertexArray(m_bit_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_bit_count));

        glBindVertexArray(0);
    }

    glDepthRange(0.0f, 1.0f);
    glEnable(GL_CULL_FACE);
}

} // namespace Voidfall
