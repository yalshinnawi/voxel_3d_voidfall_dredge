#include "renderer.hpp"
#include "../core/logger.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <cstdlib>

#ifndef GL_POLYGON_OFFSET_LINE
#define GL_POLYGON_OFFSET_LINE 0x2A02
#endif
#ifndef GL_POLYGON_OFFSET_FILL
#define GL_POLYGON_OFFSET_FILL 0x8037
#endif

typedef void (APIENTRY *PFNGLPOLYGONOFFSETPROC)(GLfloat factor, GLfloat units);
static PFNGLPOLYGONOFFSETPROC s_glPolygonOffset = nullptr;

namespace Voidfall {

Renderer::Renderer(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_fog_width(width / 2)
    , m_fog_height(height / 2)
{
    // 1. Load Shaders
    m_voxel_shader.load_graphics("assets/shaders/voxel_pbr.vert", "assets/shaders/voxel_pbr.frag");
    m_fog_compute_shader.load_compute("assets/shaders/volumetric_fog.comp");
    m_bloom_shader.load_graphics("assets/shaders/fullscreen_quad.vert", "assets/shaders/bloom.frag");
    m_postprocess_shader.load_graphics("assets/shaders/fullscreen_quad.vert", "assets/shaders/postprocess.frag");
    m_wireframe_shader.load_graphics("assets/shaders/wireframe.vert", "assets/shaders/wireframe.frag");
    m_particle_shader.load_graphics("assets/shaders/particle.vert", "assets/shaders/particle.frag");

    // 2. Initialize Texture Array
    m_texture_array = std::make_unique<TextureArray>(64, 64, 9);

    // 3. Initialize Framebuffers and Quad
    init_framebuffers();

    // Quad geometry (NDC coordinates)
    float quad_vertices[] = {
        // pos (x, y)   uv (u, v)
        -1.0f,  1.0f,   0.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,

        -1.0f,  1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);
    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);

    init_wireframe_cube();
    init_cable_buffer();
    init_particle_buffers();
}

Renderer::~Renderer() {
    cleanup_framebuffers();
    if (m_quad_vao != 0) glDeleteVertexArrays(1, &m_quad_vao);
    if (m_quad_vbo != 0) glDeleteBuffers(1, &m_quad_vbo);
    if (m_wireframe_vao != 0) glDeleteVertexArrays(1, &m_wireframe_vao);
    if (m_wireframe_vbo != 0) glDeleteBuffers(1, &m_wireframe_vbo);
    if (m_cable_vao != 0) glDeleteVertexArrays(1, &m_cable_vao);
    if (m_cable_vbo != 0) glDeleteBuffers(1, &m_cable_vbo);
    if (m_particle_vao != 0) glDeleteVertexArrays(1, &m_particle_vao);
    if (m_particle_vbo != 0) glDeleteBuffers(1, &m_particle_vbo);
}

void Renderer::init_wireframe_cube() {
    float cube_lines[] = {
        // Bottom square
        0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 0.0f,

        // Top square
        0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,

        // 4 Vertical Pillars
        0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_wireframe_vao);
    glGenBuffers(1, &m_wireframe_vbo);
    glBindVertexArray(m_wireframe_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_wireframe_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_lines), cube_lines, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

void Renderer::init_cable_buffer() {
    glGenVertexArrays(1, &m_cable_vao);
    glGenBuffers(1, &m_cable_vbo);
    glBindVertexArray(m_cable_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
    glBufferData(GL_ARRAY_BUFFER, 8192 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

void Renderer::init_particle_buffers() {
    glGenVertexArrays(1, &m_particle_vao);
    glGenBuffers(1, &m_particle_vbo);
    glBindVertexArray(m_particle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particle_vbo);
    glBufferData(GL_ARRAY_BUFFER, 1024 * 6 * 9 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // aPos (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(0));

    // aColor (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    // aUV (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(7 * sizeof(float)));

    glBindVertexArray(0);
}

void Renderer::init_framebuffers() {
    // 1. HDR Scene FBO
    glGenFramebuffers(1, &m_hdr_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);

    // Color attachment 0: HDR scene color
    glGenTextures(1, &m_color_tex);
    glBindTexture(GL_TEXTURE_2D, m_color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color_tex, 0);

    // Color attachment 1: Bright color for bloom extraction
    glGenTextures(1, &m_bright_tex);
    glBindTexture(GL_TEXTURE_2D, m_bright_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_bright_tex, 0);

    // Depth attachment
    glGenTextures(1, &m_depth_tex);
    glBindTexture(GL_TEXTURE_2D, m_depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth_tex, 0);

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fbo_status != GL_FRAMEBUFFER_COMPLETE) {
        VF_LOG_ERROR("Renderer", "HDR Framebuffer is incomplete! Status: 0x" << std::hex << fbo_status);
    } else {
        VF_LOG_INFO("Renderer", "HDR Framebuffer created successfully (" << m_width << "x" << m_height << ")");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2. Volumetric Fog Half-Res 2D Image Texture
    m_fog_width = m_width / 2;
    m_fog_height = m_height / 2;
    glGenTextures(1, &m_fog_tex);
    glBindTexture(GL_TEXTURE_2D, m_fog_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_fog_width, m_fog_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 3. Bloom Ping-Pong FBOs (half-res for efficiency)
    for (int i = 0; i < 2; ++i) {
        glGenFramebuffers(1, &m_bloom_fbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloom_fbo[i]);
        glGenTextures(1, &m_bloom_tex[i]);
        glBindTexture(GL_TEXTURE_2D, m_bloom_tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_fog_width, m_fog_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloom_tex[i], 0);

        GLenum bloom_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (bloom_status != GL_FRAMEBUFFER_COMPLETE) {
            VF_LOG_ERROR("Renderer", "Bloom Ping-Pong FBO " << i << " is incomplete! Status: 0x" << std::hex << bloom_status);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void Renderer::cleanup_framebuffers() {
    if (m_hdr_fbo != 0) glDeleteFramebuffers(1, &m_hdr_fbo);
    if (m_color_tex != 0) glDeleteTextures(1, &m_color_tex);
    if (m_bright_tex != 0) glDeleteTextures(1, &m_bright_tex);
    if (m_depth_tex != 0) glDeleteTextures(1, &m_depth_tex);
    if (m_fog_tex != 0) glDeleteTextures(1, &m_fog_tex);

    for (int i = 0; i < 2; ++i) {
        if (m_bloom_fbo[i] != 0) glDeleteFramebuffers(1, &m_bloom_fbo[i]);
        if (m_bloom_tex[i] != 0) glDeleteTextures(1, &m_bloom_tex[i]);
    }
}

void Renderer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    m_width = width;
    m_height = height;
    cleanup_framebuffers();
    init_framebuffers();
}

void Renderer::trigger_sonar_pulse(const glm::vec3& origin) {
    m_sonar.origin = origin;
    m_sonar.current_radius = 0.0f;
    m_sonar.active = true;
}

void Renderer::render_sonar_wireframes(const std::vector<SurveyedVoxel>& voxels, float alpha) {
    if (voxels.empty() || alpha <= 0.001f) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending
    glLineWidth(2.0f);

    m_wireframe_shader.use();
    m_wireframe_shader.set_mat4("uProjection", m_proj);
    m_wireframe_shader.set_mat4("uView", m_view);
    m_wireframe_shader.set_int("uIsLineOnly", 0);

    glBindVertexArray(m_wireframe_vao);

    for (const auto& v : voxels) {
        glm::vec4 color(0.0f);
        if (v.material_id == MAT_VOIDITE || v.material_id == MAT_VOIDITE_CRYSTAL) {
            // Neon cyan (#00F0FF)
            color = glm::vec4(0.0f, 0.94f, 1.0f, alpha);
        } else if (v.material_id == MAT_TITANIUM || v.material_id == MAT_INDUSTRIAL_BULKHEAD) {
            // Gold (#FFB300)
            color = glm::vec4(1.0f, 0.70f, 0.0f, alpha);
        } else if (v.material_id == MAT_VAULT_DOOR || v.material_id == MAT_REINFORCED_VAULT_DOOR) {
            // Magenta (#FF00D4)
            color = glm::vec4(1.0f, 0.0f, 0.85f, alpha);
        } else {
            continue; // Plain rock is not wireframed
        }

        m_wireframe_shader.set_vec3("uVoxelPos", glm::vec3(v.pos));
        m_wireframe_shader.set_vec4("uColor", color);

        glDrawArrays(GL_LINES, 0, 24);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::render_grapple_cable(const glm::vec3& start, const glm::vec3& end) {
    if (m_cable_vao == 0) return;

    float vertices[6] = {
        start.x, start.y, start.z,
        end.x, end.y, end.z
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive luminous cable
    glLineWidth(2.5f);

    m_wireframe_shader.use();
    m_wireframe_shader.set_mat4("uProjection", m_proj);
    m_wireframe_shader.set_mat4("uView", m_view);
    m_wireframe_shader.set_vec3("uVoxelPos", glm::vec3(0.0f));
    m_wireframe_shader.set_int("uIsLineOnly", 1);
    m_wireframe_shader.set_vec4("uColor", glm::vec4(0.2f, 0.95f, 1.0f, 0.95f));

    glBindVertexArray(m_cable_vao);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::render_extraction_beacon(const glm::vec3& beacon_pos, float siren_pulse, float time, bool is_pod_landed) {
    if (m_cable_vao == 0) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive luminous visuals

    m_wireframe_shader.use();
    m_wireframe_shader.set_mat4("uProjection", m_proj);
    m_wireframe_shader.set_mat4("uView", m_view);
    m_wireframe_shader.set_vec3("uVoxelPos", glm::vec3(0.0f));
    m_wireframe_shader.set_int("uIsLineOnly", 1);

    glBindVertexArray(m_cable_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);

    // 1. Grounded Tripod and Cylinder Base Entity
    std::vector<float> base_lines;
    base_lines.reserve(128);

    // 3 Tripod legs
    for (int i = 0; i < 3; ++i) {
        float angle = glm::radians(i * 120.0f);
        glm::vec3 foot = beacon_pos + glm::vec3(std::cos(angle) * 0.95f, 0.0f, std::sin(angle) * 0.95f);
        glm::vec3 hub = beacon_pos + glm::vec3(0.0f, 0.75f, 0.0f);
        base_lines.insert(base_lines.end(), {foot.x, foot.y, foot.z, hub.x, hub.y, hub.z});

        // Cross-brace between feet
        float next_angle = glm::radians(((i + 1) % 3) * 120.0f);
        glm::vec3 next_foot = beacon_pos + glm::vec3(std::cos(next_angle) * 0.95f, 0.0f, std::sin(next_angle) * 0.95f);
        base_lines.insert(base_lines.end(), {foot.x, foot.y, foot.z, next_foot.x, next_foot.y, next_foot.z});
    }

    // Octagonal central cylinder base
    const int segs = 8;
    for (int i = 0; i < segs; ++i) {
        float a0 = glm::radians(i * (360.0f / segs));
        float a1 = glm::radians((i + 1) * (360.0f / segs));
        glm::vec3 b0 = beacon_pos + glm::vec3(std::cos(a0) * 0.42f, 0.0f, std::sin(a0) * 0.42f);
        glm::vec3 b1 = beacon_pos + glm::vec3(std::cos(a1) * 0.42f, 0.0f, std::sin(a1) * 0.42f);
        glm::vec3 t0 = beacon_pos + glm::vec3(std::cos(a0) * 0.42f, 0.75f, std::sin(a0) * 0.42f);
        glm::vec3 t1 = beacon_pos + glm::vec3(std::cos(a1) * 0.42f, 0.75f, std::sin(a1) * 0.42f);

        base_lines.insert(base_lines.end(), {b0.x, b0.y, b0.z, b1.x, b1.y, b1.z}); // bottom edge
        base_lines.insert(base_lines.end(), {t0.x, t0.y, t0.z, t1.x, t1.y, t1.z}); // top edge
        base_lines.insert(base_lines.end(), {b0.x, b0.y, b0.z, t0.x, t0.y, t0.z}); // vertical strut
    }

    glLineWidth(2.5f);
    glBufferSubData(GL_ARRAY_BUFFER, 0, base_lines.size() * sizeof(float), base_lines.data());
    m_wireframe_shader.set_vec4("uColor", glm::vec4(1.0f, 0.70f, 0.0f, 0.95f)); // industrial gold-orange
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(base_lines.size() / 3));

    // 2. Pulsating Vertical Light Column with Upward-Drifting Particle Rings
    std::vector<float> beam_lines;
    beam_lines.reserve(512);

    float pulse = 0.7f + 0.3f * std::sin(time * 5.0f);
    glm::vec4 beam_color = is_pod_landed ?
        glm::vec4(0.1f, 1.0f, 0.45f, 0.90f * pulse) :
        glm::vec4(0.0f, 0.94f, 1.0f, 0.90f * pulse);

    // Center core laser
    glm::vec3 core_start = beacon_pos + glm::vec3(0.0f, 0.75f, 0.0f);
    glm::vec3 core_end   = beacon_pos + glm::vec3(0.0f, 32.0f, 0.0f);
    beam_lines.insert(beam_lines.end(), {core_start.x, core_start.y, core_start.z, core_end.x, core_end.y, core_end.z});

    // Outer column struts
    for (int i = 0; i < 6; ++i) {
        float a = glm::radians(i * 60.0f + time * 30.0f);
        glm::vec3 p0 = beacon_pos + glm::vec3(std::cos(a) * 0.35f, 0.75f, std::sin(a) * 0.35f);
        glm::vec3 p1 = beacon_pos + glm::vec3(std::cos(a) * 0.35f, 32.0f, std::sin(a) * 0.35f);
        beam_lines.insert(beam_lines.end(), {p0.x, p0.y, p0.z, p1.x, p1.y, p1.z});
    }

    // Upward-drifting particle rings
    for (int r = 0; r < 5; ++r) {
        float ring_y = 1.0f + std::fmod(time * 4.5f + r * 5.0f, 25.0f);
        float ring_rad = 0.5f + 0.03f * ring_y;

        const int ring_pts = 12;
        for (int p = 0; p < ring_pts; ++p) {
            float pa0 = glm::radians(p * (360.0f / ring_pts));
            float pa1 = glm::radians((p + 1) * (360.0f / ring_pts));
            glm::vec3 r0 = beacon_pos + glm::vec3(std::cos(pa0) * ring_rad, ring_y, std::sin(pa0) * ring_rad);
            glm::vec3 r1 = beacon_pos + glm::vec3(std::cos(pa1) * ring_rad, ring_y, std::sin(pa1) * ring_rad);
            beam_lines.insert(beam_lines.end(), {r0.x, r0.y, r0.z, r1.x, r1.y, r1.z});
        }
    }

    glLineWidth(2.0f);
    glBufferSubData(GL_ARRAY_BUFFER, 0, beam_lines.size() * sizeof(float), beam_lines.data());
    m_wireframe_shader.set_vec4("uColor", beam_color);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(beam_lines.size() / 3));

    // 3. Rotating Emergency Siren Light (sweeping beam)
    std::vector<float> siren_lines;
    float siren_ang = time * 7.5f;
    glm::vec3 siren_origin = beacon_pos + glm::vec3(0.0f, 0.85f, 0.0f);
    glm::vec3 siren_dir(std::cos(siren_ang), 0.08f, std::sin(siren_ang));
    siren_dir = glm::normalize(siren_dir);

    // Siren head light
    glm::vec3 siren_tip = siren_origin + siren_dir * 0.55f;
    siren_lines.insert(siren_lines.end(), {siren_origin.x, siren_origin.y, siren_origin.z, siren_tip.x, siren_tip.y, siren_tip.z});

    // Siren sweeping spotlight rays on cavern walls
    glm::vec3 sweep_end = siren_origin + siren_dir * 20.0f;
    siren_lines.insert(siren_lines.end(), {siren_origin.x, siren_origin.y, siren_origin.z, sweep_end.x, sweep_end.y, sweep_end.z});

    // Fanned side rays
    glm::vec3 right_ray = glm::normalize(siren_dir + glm::vec3(-siren_dir.z, 0, siren_dir.x) * 0.15f) * 18.0f;
    glm::vec3 left_ray  = glm::normalize(siren_dir - glm::vec3(-siren_dir.z, 0, siren_dir.x) * 0.15f) * 18.0f;
    glm::vec3 r_end = siren_origin + right_ray;
    glm::vec3 l_end = siren_origin + left_ray;
    siren_lines.insert(siren_lines.end(), {siren_origin.x, siren_origin.y, siren_origin.z, r_end.x, r_end.y, r_end.z});
    siren_lines.insert(siren_lines.end(), {siren_origin.x, siren_origin.y, siren_origin.z, l_end.x, l_end.y, l_end.z});

    glLineWidth(3.0f);
    glBufferSubData(GL_ARRAY_BUFFER, 0, siren_lines.size() * sizeof(float), siren_lines.data());
    m_wireframe_shader.set_vec4("uColor", glm::vec4(1.0f, 0.15f, 0.1f, 0.95f)); // emergency red siren
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(siren_lines.size() / 3));

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::trigger_dust_kickup(float duration) {
    m_dust_timer = duration;
}

void Renderer::spawn_break_particles(const glm::vec3& block_pos, const glm::ivec3& normal, uint8_t mat_id) {
    glm::vec3 center = block_pos + glm::vec3(0.5f);
    glm::vec3 n_dir = glm::vec3(normal);
    if (glm::length(n_dir) < 0.1f) n_dir = glm::vec3(0.0f, 1.0f, 0.0f);
    else n_dir = glm::normalize(n_dir);

    // Color palette based on material
    glm::vec4 base_color(0.6f, 0.6f, 0.6f, 1.0f);
    if (mat_id == MAT_VOIDITE_CRYSTAL) {
        base_color = glm::vec4(0.85f, 0.3f, 1.0f, 1.0f);
    } else if (mat_id == MAT_INDUSTRIAL_BULKHEAD) {
        base_color = glm::vec4(0.4f, 0.85f, 1.0f, 1.0f);
    } else if (mat_id == MAT_REINFORCED_VAULT_DOOR) {
        base_color = glm::vec4(1.0f, 0.85f, 0.2f, 1.0f);
    } else if (mat_id == MAT_RADIOACTIVE_ORE) {
        base_color = glm::vec4(0.25f, 1.0f, 0.4f, 1.0f);
    } else if (mat_id == MAT_VOLCANIC_BASALT) {
        base_color = glm::vec4(0.28f, 0.28f, 0.32f, 1.0f);
    } else if (mat_id == MAT_FRACTURED_GRANITE) {
        base_color = glm::vec4(0.58f, 0.55f, 0.52f, 1.0f);
    }

    // Spawn 12 dynamic billboard debris quads with reduced size (0.12m) and 0.6s lifetime
    for (int i = 0; i < 12; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float ry = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        glm::vec3 jitter(rx, ry, rz);

        p.pos = center + n_dir * 0.25f + jitter * 0.15f;
        p.vel = n_dir * (2.8f + static_cast<float>(rand() % 100) / 40.0f) + jitter * 3.5f;
        p.color = base_color;
        p.size = 0.10f + static_cast<float>(rand() % 100) / 2500.0f; // ~0.10m - 0.14m (average 0.12m)
        p.max_life = 0.5f + static_cast<float>(rand() % 100) / 500.0f; // ~0.5s - 0.7s (average 0.6s)
        p.life = p.max_life;
        m_particles.push_back(p);
    }
}

void Renderer::spawn_crack_debris(const glm::vec3& block_pos, const glm::ivec3& normal, float intensity, uint8_t mat_id) {
    glm::vec3 n_dir = glm::vec3(normal);
    if (glm::length(n_dir) < 0.1f) n_dir = glm::vec3(0.0f, 1.0f, 0.0f);
    else n_dir = glm::normalize(n_dir);

    glm::vec4 base_color(0.6f, 0.6f, 0.6f, 1.0f);
    if (mat_id == MAT_VOIDITE_CRYSTAL) {
        base_color = glm::vec4(0.85f, 0.3f, 1.0f, 1.0f);
    } else if (mat_id == MAT_INDUSTRIAL_BULKHEAD) {
        base_color = glm::vec4(0.4f, 0.85f, 1.0f, 1.0f);
    } else if (mat_id == MAT_REINFORCED_VAULT_DOOR) {
        base_color = glm::vec4(1.0f, 0.85f, 0.2f, 1.0f);
    } else if (mat_id == MAT_RADIOACTIVE_ORE) {
        base_color = glm::vec4(0.25f, 1.0f, 0.4f, 1.0f);
    } else if (mat_id == MAT_VOLCANIC_BASALT) {
        base_color = glm::vec4(0.28f, 0.28f, 0.32f, 1.0f);
    } else if (mat_id == MAT_FRACTURED_GRANITE) {
        base_color = glm::vec4(0.58f, 0.55f, 0.52f, 1.0f);
    }

    int count = std::max(1, static_cast<int>(intensity * 3.0f));
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float ry = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        glm::vec3 jitter(rx, ry, rz);

        p.pos = block_pos + n_dir * 0.15f + jitter * 0.2f;
        p.vel = n_dir * (1.5f + static_cast<float>(rand() % 100) / 50.0f) + jitter * 2.5f;
        p.color = base_color;
        p.size = 0.05f + static_cast<float>(rand() % 100) / 2000.0f;
        p.max_life = 0.3f + static_cast<float>(rand() % 100) / 500.0f;
        p.life = p.max_life;
        m_particles.push_back(p);
    }
}

void Renderer::render_block_cracks(const glm::ivec3& voxel_pos, float progress, const glm::ivec3& face_norm, uint8_t mat_id) {
    if (progress <= 0.0f || m_cable_vao == 0) return;

    progress = glm::clamp(progress, 0.0f, 1.0f);
    int stage = std::min(5, static_cast<int>(progress * 6.0f));

    glm::vec3 n = glm::vec3(face_norm);
    if (glm::length(n) < 0.1f) n = glm::vec3(0.0f, 1.0f, 0.0f);
    else n = glm::normalize(n);

    glm::vec3 center = glm::vec3(voxel_pos) + glm::vec3(0.5f) + n * 0.501f;

    // Escalating particle debris as progress exceeds 75%
    if (progress > 0.75f) {
        float prob = (progress - 0.75f) * 2.0f;
        if ((static_cast<float>(rand() % 100) / 100.0f) < prob) {
            spawn_crack_debris(center, face_norm, (progress - 0.75f) / 0.25f, mat_id);
        }
    }

    // Tangent axes on targeted face
    glm::vec3 u_dir, v_dir;
    if (std::abs(n.y) > 0.8f) {
        u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
        v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
    } else if (std::abs(n.x) > 0.8f) {
        u_dir = glm::vec3(0.0f, 0.0f, 1.0f);
        v_dir = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
        u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
        v_dir = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    auto to_world = [&](float u, float v) -> glm::vec3 {
        return center + u * u_dir + v * v_dir;
    };

    std::vector<float> lines;
    lines.reserve(512);

    auto add_line = [&](float u1, float v1, float u2, float v2) {
        glm::vec3 p1 = to_world(u1, v1);
        glm::vec3 p2 = to_world(u2, v2);
        lines.insert(lines.end(), {p1.x, p1.y, p1.z, p2.x, p2.y, p2.z});
    };

    // Stage 0: Hairline fracture core
    add_line( 0.00f,  0.00f,   0.12f,  0.15f);
    add_line( 0.00f,  0.00f,  -0.14f,  0.11f);
    add_line( 0.00f,  0.00f,  -0.11f, -0.15f);
    add_line( 0.00f,  0.00f,   0.15f, -0.12f);
    add_line( 0.00f,  0.00f,   0.02f,  0.18f);
    add_line( 0.00f,  0.00f,  -0.03f, -0.17f);

    // Stage 1: Primary radial fissures reaching edges
    if (stage >= 1) {
        add_line( 0.12f,  0.15f,   0.28f,  0.32f);
        add_line( 0.28f,  0.32f,   0.42f,  0.40f);
        add_line(-0.14f,  0.11f,  -0.31f,  0.22f);
        add_line(-0.31f,  0.22f,  -0.44f,  0.35f);
        add_line(-0.11f, -0.15f,  -0.25f, -0.30f);
        add_line(-0.25f, -0.30f,  -0.38f, -0.42f);
        add_line( 0.15f, -0.12f,   0.32f, -0.26f);
        add_line( 0.32f, -0.26f,   0.44f, -0.36f);
        add_line( 0.02f,  0.18f,   0.06f,  0.45f);
        add_line(-0.03f, -0.17f,  -0.08f, -0.44f);
    }

    // Stage 2: Lateral cross-cracks & fragment boundaries
    if (stage >= 2) {
        add_line( 0.28f,  0.32f,  -0.14f,  0.11f);
        add_line(-0.31f,  0.22f,  -0.25f, -0.30f);
        add_line(-0.25f, -0.30f,   0.15f, -0.12f);
        add_line( 0.32f, -0.26f,   0.28f,  0.32f);
        add_line( 0.12f,  0.15f,   0.35f,  0.08f);
        add_line(-0.14f,  0.11f,  -0.42f,  0.02f);
        add_line(-0.11f, -0.15f,  -0.05f, -0.38f);
        add_line( 0.15f, -0.12f,   0.40f, -0.10f);
    }

    // Stage 3: Spiderweb circumferential perimeter loop
    if (stage >= 3) {
        add_line( 0.42f,  0.40f,   0.06f,  0.45f);
        add_line( 0.06f,  0.45f,  -0.44f,  0.35f);
        add_line(-0.44f,  0.35f,  -0.42f,  0.02f);
        add_line(-0.42f,  0.02f,  -0.38f, -0.42f);
        add_line(-0.38f, -0.42f,  -0.08f, -0.44f);
        add_line(-0.08f, -0.44f,   0.44f, -0.36f);
        add_line( 0.44f, -0.36f,   0.40f, -0.10f);
        add_line( 0.40f, -0.10f,   0.42f,  0.40f);
    }

    // Stage 4: Corner cleavage and face detachment
    if (stage >= 4) {
        add_line( 0.42f,  0.40f,   0.48f,  0.48f);
        add_line(-0.44f,  0.35f,  -0.48f,  0.48f);
        add_line(-0.38f, -0.42f,  -0.48f, -0.48f);
        add_line( 0.44f, -0.36f,   0.48f, -0.48f);
        // Perimeter detachment boundary
        add_line(-0.48f, -0.48f,   0.48f, -0.48f);
        add_line( 0.48f, -0.48f,   0.48f,  0.48f);
        add_line( 0.48f,  0.48f,  -0.48f,  0.48f);
        add_line(-0.48f,  0.48f,  -0.48f, -0.48f);
    }

    // Stage 5: Total pulverization network with diagonal shears
    if (stage >= 5) {
        add_line( 0.00f,  0.00f,   0.20f,  0.00f);
        add_line( 0.00f,  0.00f,  -0.20f,  0.00f);
        add_line( 0.00f,  0.00f,   0.00f,  0.20f);
        add_line( 0.00f,  0.00f,   0.00f, -0.20f);
        add_line(-0.20f,  0.00f,   0.00f,  0.20f);
        add_line( 0.00f,  0.20f,   0.20f,  0.00f);
        add_line( 0.20f,  0.00f,   0.00f, -0.20f);
        add_line( 0.00f, -0.20f,  -0.20f,  0.00f);
        add_line( 0.28f,  0.32f,   0.48f, -0.48f);
        add_line(-0.31f,  0.22f,   0.48f,  0.48f);
        add_line(-0.25f, -0.30f,   0.42f,  0.40f);
        add_line( 0.15f, -0.12f,  -0.48f,  0.48f);
    }

    if (lines.empty()) return;

    // Color gradient across stages: electric cyan -> glowing safety amber -> molten incandescent white
    glm::vec4 crack_color;
    if (stage < 2) {
        crack_color = glm::vec4(0.0f, 0.95f, 1.0f, 0.85f); // Electric cyan
    } else if (stage < 4) {
        crack_color = glm::vec4(1.0f, 0.75f, 0.15f, 0.92f); // Safety amber
    } else {
        crack_color = glm::vec4(1.0f, 0.92f, 0.70f, 1.0f); // Molten incandescent core
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); // Don't write depth for crack decal overlay

    // Clean overlay without Z-fighting using glPolygonOffset
    if (!s_glPolygonOffset) {
        s_glPolygonOffset = reinterpret_cast<PFNGLPOLYGONOFFSETPROC>(glfwGetProcAddress("glPolygonOffset"));
    }
    glEnable(GL_POLYGON_OFFSET_LINE);
    if (s_glPolygonOffset) {
        s_glPolygonOffset(-1.0f, -1.0f);
    }

    // Additive blending for luminous energy discharge
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(2.5f);

    m_wireframe_shader.use();
    m_wireframe_shader.set_mat4("uProjection", m_proj);
    m_wireframe_shader.set_mat4("uView", m_view);
    m_wireframe_shader.set_vec3("uVoxelPos", glm::vec3(0.0f));
    m_wireframe_shader.set_int("uIsLineOnly", 1);
    m_wireframe_shader.set_vec4("uColor", crack_color);

    glBindVertexArray(m_cable_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, lines.size() * sizeof(float), lines.data());

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size() / 3));
    glBindVertexArray(0);

    glDisable(GL_POLYGON_OFFSET_LINE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::update_particles(float dt) {
    for (auto it = m_particles.begin(); it != m_particles.end();) {
        it->pos += it->vel * dt;
        it->vel.y -= 16.0f * dt; // gravity drop
        it->vel *= (1.0f - 1.5f * dt); // air resistance
        it->life -= dt;

        if (it->life <= 0.0f) {
            it = m_particles.erase(it);
        } else {
            ++it;
        }
    }
}

void Renderer::render_particles() {
    if (m_particles.empty()) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    m_particle_shader.use();
    m_particle_shader.set_mat4("uProjection", m_proj);
    m_particle_shader.set_mat4("uView", m_view);

    // Camera right and up
    glm::vec3 cam_right = glm::vec3(m_view[0][0], m_view[1][0], m_view[2][0]);
    glm::vec3 cam_up    = glm::vec3(m_view[0][1], m_view[1][1], m_view[2][1]);

    std::vector<float> vdata;
    vdata.reserve(m_particles.size() * 54);

    for (const auto& p : m_particles) {
        float alpha = std::clamp(p.life / p.max_life, 0.0f, 1.0f);
        glm::vec4 col = p.color;
        col.a *= alpha;

        glm::vec3 r = cam_right * (p.size * 0.5f);
        glm::vec3 u = cam_up * (p.size * 0.5f);

        glm::vec3 p0 = p.pos - r - u;
        glm::vec3 p1 = p.pos + r - u;
        glm::vec3 p2 = p.pos + r + u;
        glm::vec3 p3 = p.pos - r + u;

        // Quad 2 triangles (6 vertices)
        float quad_v[54] = {
            p0.x, p0.y, p0.z, col.r, col.g, col.b, col.a, 0.0f, 0.0f,
            p1.x, p1.y, p1.z, col.r, col.g, col.b, col.a, 1.0f, 0.0f,
            p2.x, p2.y, p2.z, col.r, col.g, col.b, col.a, 1.0f, 1.0f,

            p0.x, p0.y, p0.z, col.r, col.g, col.b, col.a, 0.0f, 0.0f,
            p2.x, p2.y, p2.z, col.r, col.g, col.b, col.a, 1.0f, 1.0f,
            p3.x, p3.y, p3.z, col.r, col.g, col.b, col.a, 0.0f, 1.0f
        };
        vdata.insert(vdata.end(), quad_v, quad_v + 54);
    }

    if (!vdata.empty()) {
        glBindVertexArray(m_particle_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_particle_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vdata.size() * sizeof(float)), vdata.data());
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vdata.size() / 9));
        glBindVertexArray(0);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::add_point_light(const PointLight& light) {
    if (m_point_lights.size() < 16) {
        m_point_lights.push_back(light);
    }
}

void Renderer::clear_point_lights() {
    m_point_lights.clear();
}

void Renderer::begin_frame(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cam_pos) {
    m_view = view;
    m_proj = proj;
    m_cam_pos = cam_pos;

    // Bind HDR FBO and clear
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.015f, 0.018f, 0.024f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Setup Voxel Shader
    m_voxel_shader.use();
    m_voxel_shader.set_mat4("uView", m_view);
    m_voxel_shader.set_mat4("uProjection", m_proj);
    m_voxel_shader.set_vec3("uCameraPos", m_cam_pos);

    // Headlamp
    m_voxel_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_voxel_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_voxel_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_voxel_shader.set_float("uHeadlampInnerCutoff", m_headlamp.inner_cutoff);
    m_voxel_shader.set_float("uHeadlampOuterCutoff", m_headlamp.outer_cutoff);
    m_voxel_shader.set_float("uHeadlampIntensity", m_headlamp.enabled ? m_headlamp.intensity : 0.0f);

    // Point lights
    m_voxel_shader.set_int("uNumPointLights", static_cast<int>(m_point_lights.size()));
    for (size_t i = 0; i < m_point_lights.size() && i < 16; ++i) {
        std::string base = "uPointLights[" + std::to_string(i) + "]";
        m_voxel_shader.set_vec3(base + ".position", m_point_lights[i].position);
        m_voxel_shader.set_vec3(base + ".color", m_point_lights[i].color);
        m_voxel_shader.set_float(base + ".radius", m_point_lights[i].radius);
        m_voxel_shader.set_float(base + ".intensity", m_point_lights[i].intensity);
    }

    // Seismic Sonar
    m_voxel_shader.set_vec3("uSonarOrigin", m_sonar.origin);
    m_voxel_shader.set_float("uSonarRadius", m_sonar.current_radius);
    m_voxel_shader.set_float("uSonarExpansionSpeed", m_sonar.expansion_speed);
    m_voxel_shader.set_int("uSonarActive", m_sonar.active ? 1 : 0);

    // Bind texture arrays
    if (m_texture_array) {
        m_texture_array->bind_albedo(0);
        m_texture_array->bind_normal(1);
        m_texture_array->bind_rough_metal(2);
        m_texture_array->bind_emissive(3);
        m_voxel_shader.set_int("uAlbedoArray", 0);
        m_voxel_shader.set_int("uNormalArray", 1);
        m_voxel_shader.set_int("uRoughMetalArray", 2);
        m_voxel_shader.set_int("uEmissiveArray", 3);
        m_voxel_shader.set_int("uUseTextureArray", 1);
    }
}

void Renderer::render_chunk(const Chunk& chunk) {
    if (chunk.is_empty()) return;
    glm::mat4 model = glm::mat4(1.0f);
    m_voxel_shader.set_mat4("uModel", model);
    m_voxel_shader.set_vec3("uChunkWorldPos", chunk.get_world_pos());
    chunk.render();
}

void Renderer::render_quad() {
    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Renderer::end_frame(float delta_time, float radiation_level) {
    m_total_time += delta_time;

    // Update sonar pulse radius
    if (m_sonar.active) {
        m_sonar.current_radius += m_sonar.expansion_speed * delta_time;
        if (m_sonar.current_radius > m_sonar.max_radius) {
            m_sonar.active = false;
        }
    }

    // Render 3D break particles inside HDR scene FBO
    render_particles();

    // 1. Half-Resolution Volumetric Fog Compute Pass
    if (m_fog_compute_shader.id() != 0 && glad_glDispatchCompute) {
        m_fog_compute_shader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_depth_tex);
        m_fog_compute_shader.set_int("uDepthTexture", 0);

        glm::mat4 invProj = glm::inverse(m_proj);
        glm::mat4 invView = glm::inverse(m_view);
        m_fog_compute_shader.set_mat4("uInverseProj", invProj);
        m_fog_compute_shader.set_mat4("uInverseView", invView);
        m_fog_compute_shader.set_vec3("uCameraPos", m_cam_pos);

        m_fog_compute_shader.set_vec3("uHeadlampPos", m_headlamp.position);
        m_fog_compute_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
        m_fog_compute_shader.set_vec3("uHeadlampColor", m_headlamp.color);
        m_fog_compute_shader.set_float("uHeadlampInnerCutoff", m_headlamp.inner_cutoff);
        m_fog_compute_shader.set_float("uHeadlampOuterCutoff", m_headlamp.outer_cutoff);
        m_fog_compute_shader.set_float("uHeadlampIntensity", m_headlamp.enabled ? m_headlamp.intensity : 0.0f);

        if (m_dust_timer > 0.0f) {
            m_dust_timer = std::max(0.0f, m_dust_timer - delta_time);
        }
        float dust_boost = (m_dust_timer > 0.0f) ? (0.075f * (m_dust_timer / 3.0f)) : 0.0f;

        m_fog_compute_shader.set_float("uFogDensity", 0.035f + dust_boost);
        m_fog_compute_shader.set_float("uToxicHazeFactor", glm::clamp(radiation_level / 100.0f, 0.0f, 1.0f));
        m_fog_compute_shader.set_float("uTime", m_total_time);

        // Bind image unit 0
        glBindImageTexture(0, m_fog_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        GLuint groupsX = (m_fog_width + 15) / 16;
        GLuint groupsY = (m_fog_height + 15) / 16;
        glDispatchCompute(groupsX, groupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    // 2. Post-processing Tonemapping Composite to Screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_postprocess_shader.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_color_tex);
    m_postprocess_shader.set_int("uSceneColor", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_bright_tex);
    m_postprocess_shader.set_int("uBloomColor", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_fog_tex);
    m_postprocess_shader.set_int("uVolumetricFog", 2);

    // Default white for SSAO placeholder if ssao pass not active
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_bright_tex);
    m_postprocess_shader.set_int("uSSAO", 3);

    m_postprocess_shader.set_float("uExposure", 1.25f);
    m_postprocess_shader.set_float("uBloomIntensity", 0.8f);
    m_postprocess_shader.set_float("uRadiationGlitch", glm::clamp(radiation_level / 100.0f, 0.0f, 1.0f));
    m_postprocess_shader.set_float("uTime", m_total_time);

    render_quad();
}

} // namespace Voidfall
