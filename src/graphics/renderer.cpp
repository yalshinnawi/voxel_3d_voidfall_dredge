#include "renderer.hpp"
#include "../entities/dynamic_debris.hpp"
#include "../entities/enemies/void_stalker.hpp"
#include "../entities/enemies/seismic_burrower.hpp"
#include "../entities/carcass_manager.hpp"
#include "../entities/flare.hpp"
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
    m_stalker_shader.load_graphics("assets/shaders/stalker.vert", "assets/shaders/stalker.frag");

    // 2. Initialize Texture Array
    m_texture_array = std::make_unique<TextureArray>(64, 64, 14);

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
    init_stalker_buffers();
}

Renderer::~Renderer() {
    cleanup_framebuffers();
    if (m_quad_vao != 0) glDeleteVertexArrays(1, &m_quad_vao);
    if (m_quad_vbo != 0) glDeleteBuffers(1, &m_quad_vbo);
    if (m_wireframe_vao != 0) glDeleteVertexArrays(1, &m_wireframe_vao);
    if (m_wireframe_vbo != 0) glDeleteBuffers(1, &m_wireframe_vbo);
    if (m_cable_vao != 0) glDeleteVertexArrays(1, &m_cable_vao);
    if (m_cable_vbo != 0) glDeleteBuffers(1, &m_cable_vbo);
    if (m_stalker_vao != 0) glDeleteVertexArrays(1, &m_stalker_vao);
    if (m_stalker_vbo != 0) glDeleteBuffers(1, &m_stalker_vbo);
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

struct StalkerVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec4 material; // x=metallic, y=roughness, z=emissive, w=ao
};

void Renderer::init_stalker_buffers() {
    glGenVertexArrays(1, &m_stalker_vao);
    glGenBuffers(1, &m_stalker_vbo);
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    glBufferData(GL_ARRAY_BUFFER, 16384 * sizeof(StalkerVertex), nullptr, GL_DYNAMIC_DRAW);

    // aPos (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(StalkerVertex), reinterpret_cast<void*>(offsetof(StalkerVertex, position)));

    // aNormal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(StalkerVertex), reinterpret_cast<void*>(offsetof(StalkerVertex, normal)));

    // aColor (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(StalkerVertex), reinterpret_cast<void*>(offsetof(StalkerVertex, color)));

    // aMaterial (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(StalkerVertex), reinterpret_cast<void*>(offsetof(StalkerVertex, material)));

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
    glLineWidth(1.0f);

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
        } else if (v.material_id == MAT_RADIOACTIVE || v.material_id == MAT_RADIOACTIVE_ORE) {
            // Toxic Lime (#00FF66)
            color = glm::vec4(0.0f, 1.0f, 0.4f, alpha);
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
    glLineWidth(1.0f);

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

    glLineWidth(1.0f);
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

    glLineWidth(1.0f);
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

    glLineWidth(1.0f);
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

    bool is_bulkhead = (mat_id == MAT_INDUSTRIAL_BULKHEAD || mat_id == MAT_REINFORCED_VAULT_DOOR);

    // Color palette based on material
    glm::vec4 base_color(0.6f, 0.6f, 0.6f, 1.0f);
    if (mat_id == MAT_VOIDITE_CRYSTAL) {
        base_color = glm::vec4(0.85f, 0.3f, 1.0f, 1.0f);
    } else if (mat_id == MAT_INDUSTRIAL_BULKHEAD) {
        base_color = glm::vec4(0.29f, 0.33f, 0.41f, 0.90f); // #4A5568 dark charcoal / industrial gunmetal
    } else if (mat_id == MAT_REINFORCED_VAULT_DOOR) {
        base_color = glm::vec4(0.29f, 0.33f, 0.41f, 0.90f);
    } else if (mat_id == MAT_RADIOACTIVE_ORE) {
        base_color = glm::vec4(0.25f, 1.0f, 0.4f, 1.0f);
    } else if (mat_id == MAT_VOLCANIC_BASALT) {
        base_color = glm::vec4(0.28f, 0.28f, 0.32f, 1.0f);
    } else if (mat_id == MAT_FRACTURED_GRANITE) {
        base_color = glm::vec4(0.58f, 0.55f, 0.52f, 1.0f);
    } else if (mat_id == MAT_CRYSTAL_AQUIFER) {
        base_color = glm::vec4(0.18f, 0.85f, 0.98f, 0.75f);
    } else if (mat_id == MAT_BIOLUMINESCENT_FLORA) {
        base_color = glm::vec4(0.25f, 0.95f, 0.45f, 1.0f);
    } else if (mat_id == MAT_PRISMATIC_CRYSTAL) {
        base_color = glm::vec4(0.85f, 0.40f, 0.95f, 1.0f);
    } else if (mat_id == MAT_TOXIC_GAS) {
        base_color = glm::vec4(0.35f, 0.88f, 0.20f, 0.70f);
    }

    // Tame Bulkhead Particles: max 14 particles, 0.08m-0.12m, #4A5568 & #FFB300, 0.45s life, high downward gravity (28.0m/s^2)
    int count = is_bulkhead ? 14 : 12;
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float ry = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        glm::vec3 jitter(rx, ry, rz);

        if (is_bulkhead) {
            bool is_spark = (i % 3 == 0);
            if (is_spark) {
                p.color = glm::vec4(1.0f, 0.70f, 0.0f, 1.0f); // #FFB300 subtle spark point
                p.size = 0.04f + static_cast<float>(rand() % 100) / 2500.0f; // tiny spark
            } else {
                p.color = glm::vec4(0.29f, 0.33f, 0.41f, 0.90f); // #4A5568 dark charcoal / industrial gunmetal
                p.size = 0.08f + static_cast<float>(rand() % 100) / 2500.0f; // 0.08m - 0.12m
            }
            p.pos = center + n_dir * 0.15f + jitter * 0.10f;
            p.vel = n_dir * (5.0f + static_cast<float>(rand() % 100) / 25.0f) + jitter * 6.0f;
            p.gravity = 28.0f; // high downward gravity so debris clears camera immediately
            p.drag = 2.0f;
            p.max_life = 0.35f + static_cast<float>(rand() % 100) / 1000.0f; // <= 0.45s
            p.life = p.max_life;
        } else {
            p.pos = center + n_dir * 0.25f + jitter * 0.15f;
            p.vel = n_dir * (2.8f + static_cast<float>(rand() % 100) / 40.0f) + jitter * 3.5f;
            p.color = base_color;
            p.size = 0.10f + static_cast<float>(rand() % 100) / 2500.0f; // ~0.10m - 0.14m (average 0.12m)
            p.max_life = 0.5f + static_cast<float>(rand() % 100) / 500.0f; // ~0.5s - 0.7s (average 0.6s)
            p.life = p.max_life;
        }
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
    } else if (mat_id == MAT_CRYSTAL_AQUIFER) {
        base_color = glm::vec4(0.18f, 0.85f, 0.98f, 0.75f);
    } else if (mat_id == MAT_BIOLUMINESCENT_FLORA) {
        base_color = glm::vec4(0.25f, 0.95f, 0.45f, 1.0f);
    } else if (mat_id == MAT_PRISMATIC_CRYSTAL) {
        base_color = glm::vec4(0.85f, 0.40f, 0.95f, 1.0f);
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

void Renderer::spawn_burrow_particles(const glm::vec3& burrow_pos, const glm::vec3& burrow_dir, uint8_t mat_id, int count) {
    glm::vec3 in_dir = burrow_dir;
    if (glm::length(in_dir) < 0.01f) in_dir = glm::vec3(0.0f, -1.0f, 0.0f);
    else in_dir = glm::normalize(in_dir);

    // Particles spray backward/outward from the borehole face
    glm::vec3 out_dir = -in_dir;

    glm::vec4 base_color(0.55f, 0.52f, 0.48f, 1.0f);
    if (mat_id == MAT_VOLCANIC_BASALT) {
        base_color = glm::vec4(0.28f, 0.28f, 0.32f, 1.0f);
    } else if (mat_id == MAT_VOIDITE_CRYSTAL) {
        base_color = glm::vec4(0.85f, 0.3f, 1.0f, 1.0f);
    } else if (mat_id == MAT_TITANIUM) {
        base_color = glm::vec4(0.75f, 0.75f, 0.85f, 1.0f);
    }

    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float ry = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        glm::vec3 jitter(rx, ry, rz);

        // Positioned at the borehole / rock contact surface
        p.pos = burrow_pos + out_dir * 0.12f + jitter * 0.18f;
        // Spew backward out of the hole with cone dispersion
        p.vel = out_dir * (2.2f + static_cast<float>(rand() % 100) / 35.0f) + jitter * 2.8f;
        p.gravity = 14.0f; // Rapidly arc downward
        p.drag = 0.5f;

        // Subtle shade variation for crushed rock dust and fragments
        float shade = 0.85f + static_cast<float>(rand() % 30) * 0.01f;
        p.color = glm::vec4(base_color.r * shade, base_color.g * shade, base_color.b * shade, 1.0f);
        p.size = 0.07f + static_cast<float>(rand() % 100) / 1500.0f; // 0.07m to 0.13m
        p.size_growth = -0.04f; // Shrink slightly as dust settles
        p.max_life = 0.45f + static_cast<float>(rand() % 100) / 300.0f; // 0.45s to 0.78s
        p.life = p.max_life;
        m_particles.push_back(p);
    }
}

void Renderer::spawn_toxic_gas_cloud(const glm::vec3& block_pos, int count) {
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float ry = static_cast<float>(rand() % 100) / 100.0f - 0.2f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        p.pos = block_pos + glm::vec3(0.5f) + glm::vec3(rx * 0.9f, ry * 0.5f, rz * 0.9f);
        p.vel = glm::vec3(rx * 0.6f, 0.45f + static_cast<float>(rand() % 100) / 250.0f, rz * 0.6f);
        float tint = static_cast<float>(rand() % 100) / 400.0f;
        p.color = glm::vec4(0.38f + tint, 0.88f, 0.18f, 0.65f); // Vibrant emerald-green billowing gas
        p.size = 0.55f + static_cast<float>(rand() % 100) / 300.0f; // 0.55m - 0.88m large plume
        p.max_life = 2.0f + static_cast<float>(rand() % 100) / 100.0f; // 2.0s - 3.0s lifetime
        p.life = p.max_life;
        p.gravity = -0.50f;    // Buoyant rising plume!
        p.drag = 0.80f;        // Gentle viscous drag
        p.size_growth = 0.35f; // Expands outward so player can spot it from 45m away!
        m_particles.push_back(p);
    }
}

void Renderer::spawn_radiation_glimmer(const glm::vec3& block_pos, int count) {
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float ry = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        p.pos = block_pos + glm::vec3(0.5f) + glm::vec3(rx * 1.2f, ry * 1.2f, rz * 1.2f);
        p.vel = glm::vec3(rx * 2.2f, ry * 2.2f, rz * 2.2f);
        p.color = glm::vec4(0.15f, 1.0f, 0.45f, 0.95f); // Brilliant ionizing radioactive gleam
        p.size = 0.18f + static_cast<float>(rand() % 100) / 1000.0f; // 0.18m - 0.28m spark
        p.max_life = 0.65f + static_cast<float>(rand() % 100) / 300.0f;
        p.life = p.max_life;
        p.gravity = 0.0f;     // Weightless energetic spark
        p.drag = 2.0f;        // Quick deceleration
        p.size_growth = -0.04f;
        m_particles.push_back(p);
    }
}

void Renderer::spawn_lava_embers(const glm::vec3& block_pos, int count) {
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        p.pos = block_pos + glm::vec3(0.5f) + glm::vec3(rx * 0.8f, 0.45f, rz * 0.8f);
        p.vel = glm::vec3(rx * 0.8f, 1.2f + static_cast<float>(rand() % 100) / 100.0f, rz * 0.8f);
        p.color = glm::vec4(1.0f, 0.45f, 0.08f, 0.95f);
        p.size = 0.15f + static_cast<float>(rand() % 100) / 1000.0f;
        p.max_life = 0.90f + static_cast<float>(rand() % 100) / 250.0f;
        p.life = p.max_life;
        p.gravity = -0.8f;    // Heat convection float
        p.drag = 1.0f;
        p.size_growth = -0.04f;
        m_particles.push_back(p);
    }
}

void Renderer::spawn_water_mist(const glm::vec3& block_pos, int count) {
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float ry = static_cast<float>(rand() % 100) / 100.0f - 0.2f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        p.pos = block_pos + glm::vec3(0.5f) + glm::vec3(rx * 0.8f, ry * 0.5f, rz * 0.8f);
        p.vel = glm::vec3(rx * 0.6f, 0.40f + static_cast<float>(rand() % 100) / 250.0f, rz * 0.6f);
        p.color = glm::vec4(0.18f, 0.88f, 0.98f, 0.50f); // Sparkling cyan crystal water droplet mist
        p.size = 0.35f + static_cast<float>(rand() % 100) / 400.0f;
        p.max_life = 1.5f + static_cast<float>(rand() % 100) / 200.0f;
        p.life = p.max_life;
        p.gravity = -0.20f;    // Gentle rising vapor
        p.drag = 0.85f;
        p.size_growth = 0.22f; // Expanding droplet mist
        m_particles.push_back(p);
    }
}

void Renderer::spawn_barrel_smoke(const glm::vec3& pos, const glm::vec3& dir, int count) {
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100 - 50) / 150.0f;
        float ry = static_cast<float>(rand() % 100) / 200.0f;
        float rz = static_cast<float>(rand() % 100 - 50) / 150.0f;
        p.pos = pos + dir * 0.15f;
        p.vel = dir * 1.5f + glm::vec3(rx, 0.4f + ry, rz);
        p.color = glm::vec4(0.72f, 0.76f, 0.80f, 0.45f); // Barrel heat smoke
        p.size = 0.22f + static_cast<float>(rand() % 100) / 500.0f;
        p.max_life = 0.5f + static_cast<float>(rand() % 100) / 250.0f;
        p.life = p.max_life;
        p.gravity = -0.5f; // Buoyant upward float
        p.drag = 1.8f;
        p.size_growth = 0.35f;
        m_particles.push_back(p);
    }
}

void Renderer::render_block_cracks(const glm::ivec3& voxel_pos, float progress, const glm::ivec3& face_norm, uint8_t mat_id) {
    if (progress <= 0.05f || m_cable_vao == 0) return;

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
    glLineWidth(1.0f);

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

void Renderer::render_placed_charge(const glm::ivec3& block_pos, const glm::ivec3& normal, float time) {
    if (m_stalker_vao == 0) return;

    glm::vec3 n = glm::vec3(normal);
    if (glm::length(n) < 0.1f) n = glm::vec3(0.0f, 1.0f, 0.0f);
    else n = glm::normalize(n);

    glm::vec3 center = glm::vec3(block_pos) + glm::vec3(0.5f) + n * 0.505f;

    // Tangent axes on face
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

    auto to_world = [&](float u, float v, float w) -> glm::vec3 {
        return center + u * u_dir + v * v_dir + w * n;
    };

    std::vector<StalkerVertex> verts;
    verts.reserve(512);

    auto add_quad = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
                        const glm::vec3& norm, const glm::vec4& color, const glm::vec4& mat) {
        verts.push_back({p0, norm, color, mat});
        verts.push_back({p1, norm, color, mat});
        verts.push_back({p2, norm, color, mat});
        verts.push_back({p0, norm, color, mat});
        verts.push_back({p2, norm, color, mat});
        verts.push_back({p3, norm, color, mat});
    };

    auto add_box = [&](float u_min, float u_max, float v_min, float v_max, float w_min, float w_max,
                       const glm::vec4& color, const glm::vec4& mat) {
        glm::vec3 p000 = to_world(u_min, v_min, w_min);
        glm::vec3 p100 = to_world(u_max, v_min, w_min);
        glm::vec3 p110 = to_world(u_max, v_max, w_min);
        glm::vec3 p010 = to_world(u_min, v_max, w_min);

        glm::vec3 p001 = to_world(u_min, v_min, w_max);
        glm::vec3 p101 = to_world(u_max, v_min, w_max);
        glm::vec3 p111 = to_world(u_max, v_max, w_max);
        glm::vec3 p011 = to_world(u_min, v_max, w_max);

        // Front (+w along normal)
        add_quad(p001, p101, p111, p011, n, color, mat);
        // Back (-w)
        add_quad(p100, p000, p010, p110, -n, color, mat);
        // Right (+u)
        add_quad(p100, p101, p111, p110, u_dir, color, mat);
        // Left (-u)
        add_quad(p001, p000, p010, p011, -u_dir, color, mat);
        // Top (+v)
        add_quad(p010, p110, p111, p011, v_dir, color, mat);
        // Bottom (-v)
        add_quad(p000, p001, p101, p100, -v_dir, color, mat);
    };

    // 1. Backplate & Magnetic Mounting Bracket
    add_box(-0.18f, 0.18f, -0.13f, 0.13f, 0.0f, 0.02f, glm::vec4(0.14f, 0.15f, 0.18f, 1.0f), glm::vec4(0.6f, 0.4f, 0.0f, 0.9f));
    add_box(-0.19f, -0.16f, -0.14f, -0.11f, 0.0f, 0.025f, glm::vec4(0.7f, 0.6f, 0.2f, 1.0f), glm::vec4(0.8f, 0.3f, 0.0f, 1.0f));
    add_box( 0.16f,  0.19f, -0.14f, -0.11f, 0.0f, 0.025f, glm::vec4(0.7f, 0.6f, 0.2f, 1.0f), glm::vec4(0.8f, 0.3f, 0.0f, 1.0f));
    add_box(-0.19f, -0.16f,  0.11f,  0.14f, 0.0f, 0.025f, glm::vec4(0.7f, 0.6f, 0.2f, 1.0f), glm::vec4(0.8f, 0.3f, 0.0f, 1.0f));
    add_box( 0.16f,  0.19f,  0.11f,  0.14f, 0.0f, 0.025f, glm::vec4(0.7f, 0.6f, 0.2f, 1.0f), glm::vec4(0.8f, 0.3f, 0.0f, 1.0f));

    // 2. Dual High-Density Explosive C4 / Voidite Bricks
    add_box(-0.16f, -0.025f, -0.11f, 0.11f, 0.02f, 0.085f, glm::vec4(0.35f, 0.30f, 0.22f, 1.0f), glm::vec4(0.1f, 0.7f, 0.0f, 0.8f));
    add_box( 0.025f,  0.16f, -0.11f, 0.11f, 0.02f, 0.085f, glm::vec4(0.35f, 0.30f, 0.22f, 1.0f), glm::vec4(0.1f, 0.7f, 0.0f, 0.8f));

    // Hazard safety warning stripes across explosive blocks
    add_box(-0.16f, -0.025f,  0.03f,  0.07f, 0.085f, 0.088f, glm::vec4(0.95f, 0.75f, 0.10f, 1.0f), glm::vec4(0.1f, 0.6f, 0.0f, 1.0f));
    add_box(-0.16f, -0.025f, -0.07f, -0.03f, 0.085f, 0.088f, glm::vec4(0.95f, 0.75f, 0.10f, 1.0f), glm::vec4(0.1f, 0.6f, 0.0f, 1.0f));
    add_box( 0.025f,  0.16f,  0.03f,  0.07f, 0.085f, 0.088f, glm::vec4(0.95f, 0.75f, 0.10f, 1.0f), glm::vec4(0.1f, 0.6f, 0.0f, 1.0f));
    add_box( 0.025f,  0.16f, -0.07f, -0.03f, 0.085f, 0.088f, glm::vec4(0.95f, 0.75f, 0.10f, 1.0f), glm::vec4(0.1f, 0.6f, 0.0f, 1.0f));

    // 3. Reinforced Steel Retaining Straps
    add_box(-0.17f, 0.17f,  0.045f,  0.065f, 0.02f, 0.092f, glm::vec4(0.75f, 0.78f, 0.82f, 1.0f), glm::vec4(0.85f, 0.3f, 0.0f, 1.0f));
    add_box(-0.17f, 0.17f, -0.065f, -0.045f, 0.02f, 0.092f, glm::vec4(0.75f, 0.78f, 0.82f, 1.0f), glm::vec4(0.85f, 0.3f, 0.0f, 1.0f));

    // 4. Central Wireless Detonator Receiver Unit
    add_box(-0.025f, 0.025f, -0.09f, 0.09f, 0.02f, 0.105f, glm::vec4(0.12f, 0.13f, 0.15f, 1.0f), glm::vec4(0.4f, 0.5f, 0.0f, 0.9f));
    add_box(-0.005f, 0.005f,  0.09f, 0.18f, 0.05f, 0.065f, glm::vec4(0.80f, 0.70f, 0.30f, 1.0f), glm::vec4(0.9f, 0.2f, 0.0f, 1.0f));

    // 5. Armed Warning Strobe Beacon (Pulsating Ruby-Red / Amber Strobe)
    float pulse = 0.5f + 0.5f * std::sin(time * 12.0f);
    glm::vec4 beacon_col = glm::vec4(1.0f, 0.18f, 0.06f, 1.0f);
    glm::vec4 beacon_mat = glm::vec4(0.0f, 0.1f, 1.0f + 3.5f * pulse, 1.0f);
    add_box(-0.018f, 0.018f, 0.035f, 0.075f, 0.105f, 0.135f, beacon_col, beacon_mat);

    // Render physical 3D satchel charge into HDR buffer with full depth testing
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    m_stalker_shader.use();
    m_stalker_shader.set_mat4("uProjection", m_proj);
    m_stalker_shader.set_mat4("uView", m_view);
    m_stalker_shader.set_mat4("uModel", glm::mat4(1.0f));
    m_stalker_shader.set_vec3("uCamPos", m_cam_pos);
    m_stalker_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_stalker_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_stalker_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_stalker_shader.set_float("uHeadlampEnabled", m_headlamp.enabled ? 1.0f : 0.0f);
    m_stalker_shader.set_float("uStateGlow", time);
    m_stalker_shader.set_int("uState", 0);
    m_stalker_shader.set_float("uDissolveThreshold", 0.0f);
    m_stalker_shader.set_float("u_dissolveThreshold", 0.0f);

    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(StalkerVertex), verts.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size()));
    glBindVertexArray(0);
}

void Renderer::render_flares(const std::vector<ChemicalFlare>& flares, float time) {
    if (flares.empty() || m_stalker_vao == 0) return;

    std::vector<StalkerVertex> verts;
    verts.reserve(flares.size() * 48);

    auto add_box = [&](const glm::vec3& center, const glm::vec3& half_extents,
                       const glm::vec4& color, const glm::vec4& mat) {
        glm::vec3 min_p = center - half_extents;
        glm::vec3 max_p = center + half_extents;

        auto push_quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& n) {
            verts.push_back({a, n, color, mat});
            verts.push_back({b, n, color, mat});
            verts.push_back({c, n, color, mat});
            verts.push_back({a, n, color, mat});
            verts.push_back({c, n, color, mat});
            verts.push_back({d, n, color, mat});
        };

        push_quad({min_p.x, min_p.y, max_p.z}, {max_p.x, min_p.y, max_p.z}, {max_p.x, max_p.y, max_p.z}, {min_p.x, max_p.y, max_p.z}, {0.0f, 0.0f, 1.0f});
        push_quad({max_p.x, min_p.y, min_p.z}, {min_p.x, min_p.y, min_p.z}, {min_p.x, max_p.y, min_p.z}, {max_p.x, max_p.y, min_p.z}, {0.0f, 0.0f, -1.0f});
        push_quad({max_p.x, min_p.y, max_p.z}, {max_p.x, min_p.y, min_p.z}, {max_p.x, max_p.y, min_p.z}, {max_p.x, max_p.y, max_p.z}, {1.0f, 0.0f, 0.0f});
        push_quad({min_p.x, min_p.y, min_p.z}, {min_p.x, min_p.y, max_p.z}, {min_p.x, max_p.y, max_p.z}, {min_p.x, max_p.y, min_p.z}, {-1.0f, 0.0f, 0.0f});
        push_quad({min_p.x, max_p.y, max_p.z}, {max_p.x, max_p.y, max_p.z}, {max_p.x, max_p.y, min_p.z}, {min_p.x, max_p.y, min_p.z}, {0.0f, 1.0f, 0.0f});
        push_quad({min_p.x, min_p.y, min_p.z}, {max_p.x, min_p.y, min_p.z}, {max_p.x, min_p.y, max_p.z}, {min_p.x, min_p.y, max_p.z}, {0.0f, -1.0f, 0.0f});
    };

    for (const auto& f : flares) {
        if (!f.is_alive()) continue;

        // Flare casing (cylindrical stick/box)
        glm::vec4 body_col = glm::vec4(0.22f, 0.24f, 0.26f, 1.0f);
        glm::vec4 body_mat = glm::vec4(0.7f, 0.4f, 0.0f, 1.0f);
        add_box(f.position, glm::vec3(0.04f, 0.16f, 0.04f), body_col, body_mat);

        // Active chemical combustion tip (highly emissive class-colored burn head)
        float burn_pulse = 0.85f + 0.15f * std::sin(time * 30.0f + static_cast<float>(f.id));
        glm::vec4 tip_col = glm::vec4(f.color, 1.0f);
        glm::vec4 tip_mat = glm::vec4(0.0f, 0.1f, 3.5f * burn_pulse, 1.0f);
        add_box(f.position + glm::vec3(0.0f, 0.18f, 0.0f), glm::vec3(0.05f, 0.05f, 0.05f), tip_col, tip_mat);
    }

    if (verts.empty()) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    m_stalker_shader.use();
    m_stalker_shader.set_mat4("uProjection", m_proj);
    m_stalker_shader.set_mat4("uView", m_view);
    m_stalker_shader.set_mat4("uModel", glm::mat4(1.0f));
    m_stalker_shader.set_vec3("uCamPos", m_cam_pos);
    m_stalker_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_stalker_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_stalker_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_stalker_shader.set_float("uHeadlampEnabled", m_headlamp.enabled ? 1.0f : 0.0f);
    m_stalker_shader.set_float("uStateGlow", time);
    m_stalker_shader.set_int("uState", 0);
    m_stalker_shader.set_float("uDissolveThreshold", 0.0f);
    m_stalker_shader.set_float("u_dissolveThreshold", 0.0f);

    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(StalkerVertex), verts.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size()));
    glBindVertexArray(0);
}

void Renderer::render_breadcrumbs(const std::vector<glm::vec3>& crumbs, float time) {
    if (crumbs.empty() || m_stalker_vao == 0) return;

    std::vector<StalkerVertex> verts;
    verts.reserve(crumbs.size() * 36);

    auto add_box = [&](const glm::vec3& center, const glm::vec3& half_extents,
                       const glm::vec4& color, const glm::vec4& mat) {
        glm::vec3 min_p = center - half_extents;
        glm::vec3 max_p = center + half_extents;

        auto push_quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& n) {
            verts.push_back({a, n, color, mat});
            verts.push_back({b, n, color, mat});
            verts.push_back({c, n, color, mat});
            verts.push_back({a, n, color, mat});
            verts.push_back({c, n, color, mat});
            verts.push_back({d, n, color, mat});
        };

        push_quad({min_p.x, min_p.y, max_p.z}, {max_p.x, min_p.y, max_p.z}, {max_p.x, max_p.y, max_p.z}, {min_p.x, max_p.y, max_p.z}, {0.0f, 0.0f, 1.0f});
        push_quad({max_p.x, min_p.y, min_p.z}, {min_p.x, min_p.y, min_p.z}, {min_p.x, max_p.y, min_p.z}, {max_p.x, max_p.y, min_p.z}, {0.0f, 0.0f, -1.0f});
        push_quad({max_p.x, min_p.y, max_p.z}, {max_p.x, min_p.y, min_p.z}, {max_p.x, max_p.y, min_p.z}, {max_p.x, max_p.y, max_p.z}, {1.0f, 0.0f, 0.0f});
        push_quad({min_p.x, min_p.y, min_p.z}, {min_p.x, min_p.y, max_p.z}, {min_p.x, max_p.y, max_p.z}, {min_p.x, max_p.y, min_p.z}, {-1.0f, 0.0f, 0.0f});
        push_quad({min_p.x, max_p.y, max_p.z}, {max_p.x, max_p.y, max_p.z}, {max_p.x, max_p.y, min_p.z}, {min_p.x, max_p.y, min_p.z}, {0.0f, 1.0f, 0.0f});
        push_quad({min_p.x, min_p.y, min_p.z}, {max_p.x, min_p.y, min_p.z}, {max_p.x, min_p.y, max_p.z}, {min_p.x, min_p.y, max_p.z}, {0.0f, -1.0f, 0.0f});
    };

    for (size_t i = 0; i < crumbs.size(); ++i) {
        float pulse = 0.5f + 0.5f * std::sin(time * 3.0f + static_cast<float>(i) * 0.4f);
        glm::vec4 crumb_col = glm::vec4(0.05f, 0.85f, 0.95f, 0.8f);
        glm::vec4 crumb_mat = glm::vec4(0.0f, 0.2f, 1.8f * pulse, 1.0f);
        add_box(crumbs[i] + glm::vec3(0.0f, 0.03f, 0.0f), glm::vec3(0.12f, 0.02f, 0.12f), crumb_col, crumb_mat);
    }

    if (verts.empty()) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    m_stalker_shader.use();
    m_stalker_shader.set_mat4("uProjection", m_proj);
    m_stalker_shader.set_mat4("uView", m_view);
    m_stalker_shader.set_mat4("uModel", glm::mat4(1.0f));
    m_stalker_shader.set_vec3("uCamPos", m_cam_pos);
    m_stalker_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_stalker_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_stalker_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_stalker_shader.set_float("uHeadlampEnabled", m_headlamp.enabled ? 1.0f : 0.0f);
    m_stalker_shader.set_float("uStateGlow", time);
    m_stalker_shader.set_int("uState", 0);
    m_stalker_shader.set_float("uDissolveThreshold", 0.0f);
    m_stalker_shader.set_float("u_dissolveThreshold", 0.0f);

    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(StalkerVertex), verts.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size()));
    glBindVertexArray(0);
}

void Renderer::spawn_fireball(const glm::vec3& center, int count) {
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        p.pos = center + glm::vec3(
            ((rand() % 100) / 100.0f - 0.5f) * 1.5f,
            ((rand() % 100) / 100.0f - 0.5f) * 1.5f,
            ((rand() % 100) / 100.0f - 0.5f) * 1.5f
        );
        glm::vec3 rdir(
            (rand() % 100) / 50.0f - 1.0f,
            (rand() % 100) / 50.0f - 0.2f,
            (rand() % 100) / 50.0f - 1.0f
        );
        if (glm::length(rdir) > 0.01f) rdir = glm::normalize(rdir);
        p.vel = rdir * (4.0f + (rand() % 100) / 20.0f);
        p.color = glm::vec4(1.0f, 0.45f + (rand() % 30) / 100.0f, 0.05f, 1.0f);
        p.size = 0.25f + (rand() % 20) / 100.0f;
        p.life = 0.6f + (rand() % 30) / 100.0f;
        p.max_life = p.life;
        p.gravity = -4.0f; // Buoyant upward rising flames
        p.drag = 1.2f;
        p.size_growth = 0.4f;
        p.emissive = 3.0f;
        m_particles.push_back(p);
    }
}

void Renderer::update_particles(float dt) {
    for (auto it = m_particles.begin(); it != m_particles.end();) {
        it->pos += it->vel * dt;
        it->vel.y -= it->gravity * dt;
        it->vel *= std::max(0.0f, 1.0f - it->drag * dt);
        it->size = std::max(0.01f, it->size + it->size_growth * dt);
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
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

float Renderer::get_effective_headlamp_intensity() const {
    if (!m_headlamp.enabled) return 0.0f;
    if (m_headlamp_flicker <= 0.001f) return m_headlamp.intensity;

    // Atmospheric non-linear brownout/flicker pattern
    float wave1 = std::sin(m_total_time * 37.0f);
    float wave2 = std::cos(m_total_time * 73.0f);
    float wave3 = std::sin(m_total_time * 19.5f);
    float noise = (wave1 * 0.5f + wave2 * 0.3f + wave3 * 0.2f);
    float factor = 1.0f - m_headlamp_flicker * (0.6f + 0.4f * noise);

    // Micro-blackout stutter under high flicker stress (tremors / near death)
    if (m_headlamp_flicker > 0.35f && (wave1 > 0.72f && wave2 < -0.35f)) {
        factor *= 0.08f;
    }
    return std::max(0.0f, m_headlamp.intensity * std::clamp(factor, 0.0f, 1.25f));
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
    m_voxel_shader.set_float("uTime", m_total_time);
    m_voxel_shader.set_int("uSector", m_sector);

    // Headlamp
    m_voxel_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_voxel_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_voxel_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_voxel_shader.set_float("uHeadlampInnerCutoff", m_headlamp.inner_cutoff);
    m_voxel_shader.set_float("uHeadlampOuterCutoff", m_headlamp.outer_cutoff);
    m_voxel_shader.set_float("uHeadlampIntensity", get_effective_headlamp_intensity());

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

void Renderer::render_debris(const DynamicDebris& debris) {
    if (debris.is_destroyed() || debris.vertex_count() == 0 || debris.vao() == 0) return;

    m_voxel_shader.use();

    // Model matrix: translate to world position, apply tumbling rotation, offset by -0.5f to center 1x1x1 cube
    glm::mat4 model = glm::translate(glm::mat4(1.0f), debris.position());
    const glm::vec3& rot = debris.rotation();
    model = glm::rotate(model, rot.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, rot.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, rot.z, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::translate(model, glm::vec3(-0.5f, -0.5f, -0.5f));

    m_voxel_shader.set_mat4("uModel", model);
    m_voxel_shader.set_vec3("uChunkWorldPos", glm::vec3(0.0f));

    debris.render();

    // Reset model to identity for subsequent rendering
    m_voxel_shader.set_mat4("uModel", glm::mat4(1.0f));
}

static void add_stalker_box(
    std::vector<StalkerVertex>& verts,
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

    // Front (+Z)
    push_quad(xform_pt(-1, -1,  1), xform_pt( 1, -1,  1), xform_pt( 1,  1,  1), xform_pt(-1,  1,  1), xform_n({0, 0, 1}));
    // Back (-Z)
    push_quad(xform_pt( 1, -1, -1), xform_pt(-1, -1, -1), xform_pt(-1,  1, -1), xform_pt( 1,  1, -1), xform_n({0, 0, -1}));
    // Right (+X)
    push_quad(xform_pt( 1, -1,  1), xform_pt( 1, -1, -1), xform_pt( 1,  1, -1), xform_pt( 1,  1,  1), xform_n({1, 0, 0}));
    // Left (-X)
    push_quad(xform_pt(-1, -1, -1), xform_pt(-1, -1,  1), xform_pt(-1,  1,  1), xform_pt(-1,  1, -1), xform_n({-1, 0, 0}));
    // Top (+Y)
    push_quad(xform_pt(-1,  1,  1), xform_pt( 1,  1,  1), xform_pt( 1,  1, -1), xform_pt(-1,  1, -1), xform_n({0, 1, 0}));
    // Bottom (-Y)
    push_quad(xform_pt(-1, -1, -1), xform_pt( 1, -1, -1), xform_pt( 1, -1,  1), xform_pt(-1, -1,  1), xform_n({0, -1, 0}));
}

void Renderer::render_stalkers(const std::vector<VoidStalker>& stalkers) {
    static const std::vector<VoidSpikeProjectile> s_empty_proj;
    render_stalkers(stalkers, s_empty_proj);
}

void Renderer::render_stalkers(const std::vector<VoidStalker>& stalkers, const std::vector<VoidSpikeProjectile>& projectiles) {
    if ((stalkers.empty() && projectiles.empty()) || m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_stalker_verts;
    s_stalker_verts.clear();
    if (s_stalker_verts.capacity() < 16384) {
        s_stalker_verts.reserve(16384);
    }

    m_stalker_staging_lines.clear();
    if (m_stalker_staging_lines.capacity() < 4096) {
        m_stalker_staging_lines.reserve(4096);
    }

    int active_stalker_count = 0;
    for (const auto& s : stalkers) {
        if (s.is_dead()) continue;
        active_stalker_count++;

        // Pose computation from dynamic animation controller
        StalkerPoseParameters pose = s.anim_controller.compute_blended_pose(s.pitch);

        // Base transform: translate to stalker world position + surface snapping offset,
        // and apply smooth quaternion orientation slerp (supporting floor, vertical wall, and inverted ceiling traversal)
        glm::mat4 base_model = glm::translate(glm::mat4(1.0f), s.position + s.surface_offset);
        base_model = base_model * glm::mat4_cast(s.m_currentRotation);

        // State-based body posture modifier
        float crouch_y = pose.carapace_offset_y;
        float lunge_forward = 0.0f;
        float mandible_spread = 0.0f;

        if (s.state == StalkerState::Lunging) {
            crouch_y -= 0.1f;
            lunge_forward = 0.35f;
            mandible_spread = 0.35f; // Jaws flared wide open to strike!
        } else if (s.state == StalkerState::Stalking) {
            crouch_y -= 0.15f; // Low predatory stalk
        } else if (s.state == StalkerState::Stunned) {
            // Violent shivering spasm
            float jitter = std::sin(s.glow_phase * 35.0f) * 0.08f;
            base_model = glm::rotate(base_model, jitter, glm::vec3(0.0f, 0.0f, 1.0f));
            crouch_y -= 0.2f;
        } else if (s.state == StalkerState::Burrowing) {
            // High-frequency drilling / rock excavation tremor
            float drill_jitter = std::sin(s.state_timer * 48.0f) * 0.07f;
            base_model = glm::rotate(base_model, drill_jitter, glm::vec3(0.0f, 0.0f, 1.0f));
            crouch_y -= 0.20f;
            lunge_forward = 0.25f;
            mandible_spread = 0.40f + std::abs(std::sin(s.state_timer * 36.0f)) * 0.25f;
        }

        // Apply scale, surface pose pitch & crouch
        base_model = glm::rotate(base_model, pose.carapace_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
        base_model = glm::scale(base_model, glm::vec3(s.scale));
        base_model = glm::translate(base_model, glm::vec3(0.0f, crouch_y, lunge_forward));

        // Submerge progressively into the rock wall and shrink as it enters the borehole to escape
        if (s.state == StalkerState::Burrowing) {
            float progress = std::clamp(s.state_timer / std::max(0.1f, s.burrow_duration), 0.0f, 1.0f);
            base_model = glm::translate(base_model, glm::vec3(0.0f, -progress * 0.35f, progress * 0.95f));
            float burrow_scale = std::max(0.05f, 1.0f - progress * 0.82f);
            base_model = glm::scale(base_model, glm::vec3(burrow_scale));
        }

        // Material palettes based on dedicated combat archetype
        bool is_shooter = (s.role == StalkerRole::Shooter);
        bool is_goliath = (s.role == StalkerRole::ChitinGoliath);

        // 1. Armored Chitin Carapace
        glm::vec4 chitin_color = is_goliath
            ? glm::vec4(0.18f, 0.16f, 0.14f, 1.0f) // Heavy reinforced dark obsidian-iron
            : (is_shooter
               ? glm::vec4(0.04f, 0.22f, 0.08f, 1.0f)   // Dark toxic jade chitin
               : glm::vec4(0.26f, 0.04f, 0.04f, 1.0f)); // Deep blood obsidian chitin
        glm::vec4 chitin_mat(is_goliath ? 0.70f : 0.40f, 0.25f, 0.0f, 1.0f); // metallic, roughness, emissive, ao

        // 2. Secondary Chitin / Ribs / Spines
        glm::vec4 spine_color = is_goliath
            ? glm::vec4(0.45f, 0.30f, 0.10f, 1.0f) // Bronze armor reinforcing plates
            : (is_shooter
               ? glm::vec4(0.10f, 0.50f, 0.15f, 1.0f)   // Acid emerald quills
               : glm::vec4(0.48f, 0.06f, 0.06f, 1.0f)); // Jagged crimson spines
        glm::vec4 spine_mat(0.50f, 0.20f, 0.0f, 0.9f);

        // 3. Serrated Fangs / Claws
        glm::vec4 claw_color = is_goliath
            ? glm::vec4(0.65f, 0.55f, 0.45f, 1.0f) // Heavy iron ram horn
            : (is_shooter
               ? glm::vec4(0.35f, 0.65f, 0.35f, 1.0f)   // Toxic jade bone
               : glm::vec4(0.68f, 0.20f, 0.20f, 1.0f)); // Razor blood bone
        glm::vec4 claw_mat(0.75f, 0.18f, 0.0f, 1.0f);

        // 4. Bioluminescent Eye Color
        glm::vec4 eye_color = s.get_eye_color();
        glm::vec4 eye_mat(0.0f, 0.02f, 6.5f, 1.0f); // Massive emissive boost for bloom!

        // White emissive hit-flash shader pulse (0.08s)
        if (s.hit_flash_timer > 0.0f) {
            float flash_k = std::clamp(s.hit_flash_timer / 0.08f, 0.0f, 1.0f);
            chitin_color = glm::mix(chitin_color, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), flash_k * 0.92f);
            spine_color  = glm::mix(spine_color,  glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), flash_k * 0.92f);
            claw_color   = glm::mix(claw_color,   glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), flash_k * 0.92f);
            chitin_mat.z += 8.5f * flash_k;
            spine_mat.z  += 8.5f * flash_k;
            claw_mat.z   += 8.5f * flash_k;
        }

        // 5. Pulsing Void Core (beating heart)
        float core_pulse = 0.85f + 0.35f * std::sin(s.glow_phase * 5.0f);
        glm::vec4 core_color = eye_color;
        glm::vec4 core_mat(0.1f, 0.05f, 5.0f * core_pulse, 1.0f);

        // === BODY ASSEMBLY ===

        // A. Main Torso / Thorax (Central Armored Carapace)
        {
            glm::mat4 thorax_m = glm::translate(base_model, glm::vec3(0.0f, 0.30f, 0.0f));
            add_stalker_box(s_stalker_verts, thorax_m, glm::vec3(0.26f, 0.18f, 0.35f), chitin_color, chitin_mat);
        }

        // B. Pulsating Void Core (Visible inside the ribcage / glowing heart)
        {
            float c_scale = 0.12f * core_pulse;
            glm::mat4 core_m = glm::translate(base_model, glm::vec3(0.0f, 0.28f, 0.02f));
            core_m = glm::rotate(core_m, s.glow_phase * 2.0f, glm::vec3(0.5f, 1.0f, 0.2f));
            add_stalker_box(s_stalker_verts, core_m, glm::vec3(c_scale), core_color, core_mat);
        }

        // C. Dorsal Chitin Spines (4 jagged quills along spine)
        for (int q = 0; q < 4; ++q) {
            float qz = -0.25f + q * 0.16f;
            float qh = 0.14f + (q == 1 || q == 2 ? 0.08f : 0.0f);
            glm::mat4 quill_m = glm::translate(base_model, glm::vec3(0.0f, 0.48f, qz));
            quill_m = glm::rotate(quill_m, -0.35f - q * 0.12f, glm::vec3(1.0f, 0.0f, 0.0f));
            add_stalker_box(s_stalker_verts, quill_m, glm::vec3(0.04f, qh, 0.05f), spine_color, spine_mat);
        }

        // D. Segmented Abdomen & Stinger Tail (curves back and up)
        {
            glm::mat4 tail1_m = glm::translate(base_model, glm::vec3(0.0f, 0.25f, -0.42f));
            tail1_m = glm::rotate(tail1_m, 0.25f, glm::vec3(1.0f, 0.0f, 0.0f));
            add_stalker_box(s_stalker_verts, tail1_m, glm::vec3(0.20f, 0.15f, 0.18f), chitin_color, chitin_mat);

            glm::mat4 tail2_m = glm::translate(tail1_m, glm::vec3(0.0f, -0.05f, -0.22f));
            tail2_m = glm::rotate(tail2_m, 0.45f, glm::vec3(1.0f, 0.0f, 0.0f));
            add_stalker_box(s_stalker_verts, tail2_m, glm::vec3(0.14f, 0.11f, 0.16f), chitin_color, chitin_mat);

            glm::mat4 stinger_m = glm::translate(tail2_m, glm::vec3(0.0f, 0.10f, -0.18f));
            stinger_m = glm::rotate(stinger_m, 0.6f, glm::vec3(1.0f, 0.0f, 0.0f));
            add_stalker_box(s_stalker_verts, stinger_m, glm::vec3(0.05f, 0.05f, 0.14f), claw_color, claw_mat);
        }

        // E. Armored Head / Cranial Skull (with downward-arching predatory neck tracking toward player)
        glm::mat4 head_base = glm::translate(base_model, glm::vec3(0.0f, 0.32f, 0.40f));
        head_base = glm::rotate(head_base, pose.head_pitch_offset, glm::vec3(1.0f, 0.0f, 0.0f));
        {
            glm::mat4 skull_m = glm::rotate(head_base, 0.15f, glm::vec3(1.0f, 0.0f, 0.0f));
            add_stalker_box(s_stalker_verts, skull_m, glm::vec3(0.22f, 0.14f, 0.22f), chitin_color, chitin_mat);

            glm::mat4 brow_m = glm::translate(skull_m, glm::vec3(0.0f, 0.12f, 0.02f));
            add_stalker_box(s_stalker_verts, brow_m, glm::vec3(0.24f, 0.05f, 0.18f), spine_color, spine_mat);

            // Chitin Goliath: Heavy front reinforced blast armor plates
            if (is_goliath) {
                glm::mat4 shield_m = glm::translate(skull_m, glm::vec3(0.0f, -0.02f, 0.26f));
                glm::vec4 shield_mat(0.85f, 0.20f, 0.0f, 1.0f);
                add_stalker_box(s_stalker_verts, shield_m, glm::vec3(0.38f, 0.32f, 0.08f), glm::vec4(0.24f, 0.22f, 0.20f, 1.0f), shield_mat);
            }

        }

        // F. Piercing Bioluminescent Compound Eyes
        glm::vec3 left_eye_pos(0.0f);
        glm::vec3 right_eye_pos(0.0f);
        {
            glm::mat4 eye_l = glm::translate(head_base, glm::vec3(-0.13f, 0.08f, 0.20f));
            eye_l = glm::rotate(eye_l, 0.25f, glm::vec3(0.0f, 0.0f, 1.0f));
            add_stalker_box(s_stalker_verts, eye_l, glm::vec3(0.06f, 0.025f, 0.04f), eye_color, eye_mat);
            left_eye_pos = glm::vec3(eye_l * glm::vec4(0, 0, 0, 1));

            glm::mat4 eye_r = glm::translate(head_base, glm::vec3(0.13f, 0.08f, 0.20f));
            eye_r = glm::rotate(eye_r, -0.25f, glm::vec3(0.0f, 0.0f, 1.0f));
            add_stalker_box(s_stalker_verts, eye_r, glm::vec3(0.06f, 0.025f, 0.04f), eye_color, eye_mat);
            right_eye_pos = glm::vec3(eye_r * glm::vec4(0, 0, 0, 1));

            glm::mat4 eye2_l = glm::translate(head_base, glm::vec3(-0.07f, 0.14f, 0.14f));
            add_stalker_box(s_stalker_verts, eye2_l, glm::vec3(0.035f, 0.02f, 0.03f), eye_color, eye_mat);

            glm::mat4 eye2_r = glm::translate(head_base, glm::vec3(0.07f, 0.14f, 0.14f));
            add_stalker_box(s_stalker_verts, eye2_r, glm::vec3(0.035f, 0.02f, 0.03f), eye_color, eye_mat);
        }

        // G. Serrated Mandibles / Fangs
        {
            float m_angle = 0.20f + mandible_spread + std::sin(s.glow_phase * 6.0f) * 0.06f;

            glm::mat4 mand_l = glm::translate(head_base, glm::vec3(-0.12f, -0.06f, 0.20f));
            mand_l = glm::rotate(mand_l, -m_angle, glm::vec3(0.0f, 1.0f, 0.0f));
            mand_l = glm::rotate(mand_l, 0.35f, glm::vec3(1.0f, 0.0f, 0.0f));
            add_stalker_box(s_stalker_verts, mand_l, glm::vec3(0.04f, 0.04f, 0.18f), claw_color, claw_mat);

            glm::mat4 mand_r = glm::translate(head_base, glm::vec3(0.12f, -0.06f, 0.20f));
            mand_r = glm::rotate(mand_r, m_angle, glm::vec3(0.0f, 1.0f, 0.0f));
            mand_r = glm::rotate(mand_r, 0.35f, glm::vec3(1.0f, 0.0f, 0.0f));
            add_stalker_box(s_stalker_verts, mand_r, glm::vec3(0.04f, 0.04f, 0.18f), claw_color, claw_mat);
        }

        // H. 6 Articulated Arachnid / Mantis Legs (with distinct wall-climb and ceiling splay adaptations)
        for (int l = 0; l < 6; ++l) {
            bool is_left = (l % 2 == 0);
            int row = l / 2; // 0=front, 1=mid, 2=rear
            float side = is_left ? -1.0f : 1.0f;

            float root_z = (row == 0) ? 0.20f : (row == 1) ? 0.0f : -0.22f;
            float root_x = side * 0.24f * pose.limb_splay_multiplier;
            float root_y = 0.26f;

            glm::mat4 hip_m = glm::translate(base_model, glm::vec3(root_x, root_y, root_z));

            float phase_offset = (row * 1.57f) + (is_left ? 0.0f : 3.14f);
            float crawl_swing = std::sin(s.walk_cycle + phase_offset);
            float crawl_lift = std::max(0.0f, std::cos(s.walk_cycle + phase_offset)) * 0.15f;

            if (row == 0) {
                // FRONT LEGS: Predatory Mantis Scythe Claws!
                float strike_pitch = (s.state == StalkerState::Lunging)
                    ? -0.85f
                    : (s.state == StalkerState::Burrowing)
                        ? (-0.55f + std::sin(s.state_timer * 28.0f + (is_left ? 0.0f : 3.14f)) * 0.50f)
                        : (-0.35f + pose.front_claw_pitch_offset + crawl_swing * 0.2f);
                float strike_yaw = side * (pose.front_claw_yaw + (s.state == StalkerState::Lunging ? 0.3f : (s.state == StalkerState::Burrowing ? -0.15f : 0.0f)));

                glm::mat4 femur_m = glm::rotate(hip_m, strike_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                femur_m = glm::rotate(femur_m, strike_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
                femur_m = glm::translate(femur_m, glm::vec3(0.0f, 0.18f * pose.front_claw_reach, 0.14f * pose.front_claw_reach));
                add_stalker_box(s_stalker_verts, femur_m, glm::vec3(0.045f, 0.18f * pose.front_claw_reach, 0.05f), chitin_color, chitin_mat);

                float tibia_pitch = (s.state == StalkerState::Lunging) ? 1.4f : (1.05f + pose.tibia_pitch_offset);
                glm::mat4 tibia_m = glm::translate(femur_m, glm::vec3(0.0f, 0.16f * pose.front_claw_reach, 0.08f));
                tibia_m = glm::rotate(tibia_m, tibia_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
                tibia_m = glm::translate(tibia_m, glm::vec3(0.0f, -0.18f * pose.front_claw_reach, 0.0f));
                add_stalker_box(s_stalker_verts, tibia_m, glm::vec3(0.035f, 0.20f * pose.front_claw_reach, 0.04f), claw_color, claw_mat);

                glm::mat4 tip_m = glm::translate(tibia_m, glm::vec3(0.0f, -0.20f * pose.front_claw_reach, 0.02f));
                tip_m = glm::rotate(tip_m, -0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
                add_stalker_box(s_stalker_verts, tip_m, glm::vec3(0.025f, 0.08f, 0.03f), claw_color, claw_mat);

                // Lunging speed streaks trailing behind the front claws
                if (s.state == StalkerState::Lunging) {
                    glm::vec3 claw_world = glm::vec3(tip_m * glm::vec4(0, 0, 0, 1));
                    glm::vec3 trail_back = claw_world - s.velocity * 0.12f;
                    m_stalker_staging_lines.insert(m_stalker_staging_lines.end(), {
                        claw_world.x, claw_world.y, claw_world.z,
                        trail_back.x, trail_back.y, trail_back.z
                    });
                }
            } else {
                // MID & REAR LEGS: Splayed arachnid legs with crawling IK
                float leg_base_yaw = (row == 1) ? (side * 1.57f) : (side * 2.35f);
                leg_base_yaw *= (1.0f + (pose.limb_splay_multiplier - 1.0f) * 0.25f);
                float leg_swing = (s.state == StalkerState::Burrowing)
                    ? (std::sin(s.state_timer * 24.0f + phase_offset) * 0.35f)
                    : (crawl_swing * 0.25f);
                leg_base_yaw += leg_swing;

                float femur_pitch = -0.55f + pose.femur_pitch_offset + crawl_lift;
                glm::mat4 femur_m = glm::rotate(hip_m, leg_base_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                femur_m = glm::rotate(femur_m, femur_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
                femur_m = glm::translate(femur_m, glm::vec3(0.0f, 0.16f, 0.10f));
                add_stalker_box(s_stalker_verts, femur_m, glm::vec3(0.04f, 0.18f, 0.045f), chitin_color, chitin_mat);

                float tibia_pitch = 1.35f + pose.tibia_pitch_offset - crawl_lift * 0.8f;
                glm::mat4 tibia_m = glm::translate(femur_m, glm::vec3(0.0f, 0.16f, 0.06f));
                tibia_m = glm::rotate(tibia_m, tibia_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
                tibia_m = glm::translate(tibia_m, glm::vec3(0.0f, -0.22f, 0.0f));
                add_stalker_box(s_stalker_verts, tibia_m, glm::vec3(0.03f, 0.24f, 0.035f), claw_color, claw_mat);

                // Stunned electrical arcs jumping between leg joints
                if (s.state == StalkerState::Stunned) {
                    glm::vec3 joint_pt = glm::vec3(femur_m * glm::vec4(0, 0, 0, 1));
                    glm::vec3 ground_pt = glm::vec3(tibia_m * glm::vec4(0, -0.2f, 0, 1));
                    glm::vec3 mid_arc = (joint_pt + ground_pt) * 0.5f + glm::vec3(
                        std::sin(s.glow_phase * 40.0f + l * 2.0f) * 0.15f,
                        std::cos(s.glow_phase * 35.0f + l * 2.0f) * 0.15f,
                        0.0f
                    );
                    m_stalker_staging_lines.insert(m_stalker_staging_lines.end(), {
                        joint_pt.x, joint_pt.y, joint_pt.z, mid_arc.x, mid_arc.y, mid_arc.z,
                        mid_arc.x, mid_arc.y, mid_arc.z, ground_pt.x, ground_pt.y, ground_pt.z
                    });
                }
            }
        }

        // I. Twin Eye Tracer Beams (Predator laser sight scanning forward into shadows)
        {
            glm::vec3 fwd = glm::vec3(
                std::sin(s.yaw) * std::cos(s.pitch),
                std::sin(s.pitch),
                std::cos(s.yaw) * std::cos(s.pitch)
            );
            float beam_len = (s.state == StalkerState::Lunging) ? 4.0f : 2.0f;
            glm::vec3 left_beam_end = left_eye_pos + fwd * beam_len;
            glm::vec3 right_beam_end = right_eye_pos + fwd * beam_len;

            m_stalker_staging_lines.insert(m_stalker_staging_lines.end(), {
                left_eye_pos.x, left_eye_pos.y, left_eye_pos.z,
                left_beam_end.x, left_beam_end.y, left_beam_end.z,
                right_eye_pos.x, right_eye_pos.y, right_eye_pos.z,
                right_beam_end.x, right_beam_end.y, right_beam_end.z
            });
        }

        // Predatory razor claw slash arcs during lunges or melee strikes
        if (s.slash_fx_timer > 0.0f || s.state == StalkerState::Lunging) {
            glm::vec3 slash_fwd = -glm::vec3(base_model[2]);
            glm::vec3 slash_up = glm::vec3(base_model[1]);
            glm::vec3 slash_rgt = glm::vec3(base_model[0]);
            glm::vec3 slash_center = s.position + slash_fwd * 0.95f + slash_up * 0.15f;

            for (int claw = -1; claw <= 1; ++claw) {
                float claw_offset = claw * 0.18f;
                int segments = 4;
                glm::vec3 prev_pt = slash_center + slash_rgt * (-0.45f + claw_offset) + slash_up * 0.35f;
                for (int seg = 1; seg <= segments; ++seg) {
                    float t = static_cast<float>(seg) / segments;
                    float arc_x = -0.45f + claw_offset + t * 0.9f;
                    float arc_y = 0.35f - t * 0.7f + std::sin(t * 3.14159f) * 0.12f;
                    glm::vec3 next_pt = slash_center + slash_rgt * arc_x + slash_up * arc_y + slash_fwd * (std::sin(t * 3.14159f) * 0.22f);
                    m_stalker_staging_lines.insert(m_stalker_staging_lines.end(), {
                        prev_pt.x, prev_pt.y, prev_pt.z,
                        next_pt.x, next_pt.y, next_pt.z
                    });
                    prev_pt = next_pt;
                }
            }
        }
    }

    // === VOID SPINE PROJECTILES ===
    for (const auto& proj : projectiles) {
        if (!proj.active) continue;
        active_stalker_count++;

        glm::vec3 p_dir = (glm::length(proj.velocity) > 0.001f) ? glm::normalize(proj.velocity) : glm::vec3(0.0f, 0.0f, -1.0f);
        glm::mat4 p_mat = glm::translate(glm::mat4(1.0f), proj.position);
        float p_yaw = std::atan2(-p_dir.x, -p_dir.z);
        float p_pitch = std::asin(std::clamp(p_dir.y, -1.0f, 1.0f));
        p_mat = glm::rotate(p_mat, p_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        p_mat = glm::rotate(p_mat, -p_pitch, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::vec4 proj_color(0.20f, 1.0f, 0.35f, 1.0f); // Vivid radiant toxic green crystal
        glm::vec4 proj_mat(0.1f, 0.9f, 4.5f, 1.0f);      // Highly emissive with green bloom
        add_stalker_box(s_stalker_verts, p_mat, glm::vec3(0.05f, 0.05f, 0.35f), proj_color, proj_mat);

        // Luminous streak trail behind projectile
        glm::vec3 trail_end = proj.position - p_dir * 1.1f;
        m_stalker_staging_lines.insert(m_stalker_staging_lines.end(), {
            proj.position.x, proj.position.y, proj.position.z,
            trail_end.x, trail_end.y, trail_end.z
        });
    }

    if (active_stalker_count == 0) return;

    // === PASS 1: SOLID 3D PBR SHADED MESH ===
    if (!s_stalker_verts.empty()) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        m_stalker_shader.use();
        m_stalker_shader.set_mat4("uProjection", m_proj);
        m_stalker_shader.set_mat4("uView", m_view);
        m_stalker_shader.set_mat4("uModel", glm::mat4(1.0f));
        m_stalker_shader.set_vec3("uCamPos", m_cam_pos);
        m_stalker_shader.set_vec3("uHeadlampPos", m_headlamp.position);
        m_stalker_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
        m_stalker_shader.set_vec3("uHeadlampColor", m_headlamp.color);
        m_stalker_shader.set_float("uHeadlampEnabled", m_headlamp.enabled ? 1.0f : 0.0f);
        m_stalker_shader.set_float("uStateGlow", m_total_time);
        int state_val = !stalkers.empty() ? static_cast<int>(stalkers[0].state) : 1;
        m_stalker_shader.set_int("uState", state_val);

        // Upload to dynamic VBO (capped to buffer capacity)
        size_t vert_count = std::min(s_stalker_verts.size(), static_cast<size_t>(16384));
        glBindVertexArray(m_stalker_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
        GLsizeiptr byte_size = static_cast<GLsizeiptr>(vert_count * sizeof(StalkerVertex));
        glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_stalker_verts.data());

        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vert_count));
        glBindVertexArray(0);
    }

    // === PASS 2: PREDATORY LUMINOUS EYE TRACERS & ELECTRIC ARCS ===
    if (!m_stalker_staging_lines.empty() && m_cable_vao != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive luminous overlay

        m_wireframe_shader.use();
        m_wireframe_shader.set_mat4("uProjection", m_proj);
        m_wireframe_shader.set_mat4("uView", m_view);
        m_wireframe_shader.set_vec3("uVoxelPos", glm::vec3(0.0f));
        m_wireframe_shader.set_int("uIsLineOnly", 1);

        glm::vec4 line_color = !stalkers.empty() ? stalkers[0].get_eye_color() : glm::vec4(0.92f, 0.12f, 0.96f, 1.0f);
        line_color.a = 0.85f;
        m_wireframe_shader.set_vec4("uColor", line_color);

        size_t line_floats = std::min(m_stalker_staging_lines.size(), static_cast<size_t>(8192));
        glBindVertexArray(m_cable_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
        GLsizeiptr line_bytes = static_cast<GLsizeiptr>(line_floats * sizeof(float));
        glBufferSubData(GL_ARRAY_BUFFER, 0, line_bytes, m_stalker_staging_lines.data());

        glLineWidth(2.5f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(line_floats / 3));

        glBindVertexArray(0);
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }
}

void Renderer::render_carcasses(const std::vector<EnemyCarcass>& carcasses) {
    if (carcasses.empty() || m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_carcass_verts;
    s_carcass_verts.clear();
    if (s_carcass_verts.capacity() < 8192) {
        s_carcass_verts.reserve(8192);
    }

    for (const auto& c : carcasses) {
        if (c.alpha <= 0.001f) continue;

        // Base transform for carcass: world position and death orientation
        glm::mat4 base_model = glm::translate(glm::mat4(1.0f), c.position);
        base_model = base_model * glm::mat4_cast(c.rotation);
        base_model = glm::scale(base_model, glm::vec3(c.scale));
        // Flat collapsed death pose
        base_model = glm::translate(base_model, glm::vec3(0.0f, -0.15f, 0.0f));

        bool is_shooter = (c.role == StalkerRole::Shooter);
        glm::vec4 chitin_color = is_shooter
            ? glm::vec4(0.03f, 0.16f, 0.06f, c.alpha)
            : glm::vec4(0.18f, 0.03f, 0.03f, c.alpha);
        glm::vec4 chitin_mat(0.30f, 0.35f, 0.0f, 1.0f);

        glm::vec4 spine_color = is_shooter
            ? glm::vec4(0.06f, 0.30f, 0.10f, c.alpha)
            : glm::vec4(0.30f, 0.04f, 0.04f, c.alpha);
        glm::vec4 spine_mat(0.35f, 0.30f, 0.0f, 0.9f);

        // Build limp collapsed death model (carapace, collapsed head, abdomen, splayed legs)
        glm::mat4 thorax_m = glm::translate(base_model, glm::vec3(0.0f, 0.05f, 0.0f));
        add_stalker_box(s_carcass_verts, thorax_m, glm::vec3(0.26f, 0.12f, 0.35f), chitin_color, chitin_mat);

        glm::mat4 skull_m = glm::translate(base_model, glm::vec3(0.0f, 0.02f, 0.35f));
        add_stalker_box(s_carcass_verts, skull_m, glm::vec3(0.20f, 0.09f, 0.20f), chitin_color, chitin_mat);

        glm::mat4 abd_m = glm::translate(base_model, glm::vec3(0.0f, 0.04f, -0.35f));
        add_stalker_box(s_carcass_verts, abd_m, glm::vec3(0.22f, 0.10f, 0.25f), chitin_color * 0.9f, chitin_mat);

        // Limp, collapsed legs resting flat against floor voxels
        for (int side : {-1, 1}) {
            for (int seg = 0; seg < 3; ++seg) {
                float lz = -0.15f + static_cast<float>(seg) * 0.18f;
                glm::mat4 leg_m = glm::translate(base_model, glm::vec3(side * 0.38f, -0.02f, lz));
                leg_m = glm::rotate(leg_m, glm::radians(side * 25.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                add_stalker_box(s_carcass_verts, leg_m, glm::vec3(0.22f, 0.035f, 0.035f), spine_color, spine_mat);
            }
        }
    }

    if (s_carcass_verts.empty()) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_stalker_shader.use();
    m_stalker_shader.set_mat4("uProjection", m_proj);
    m_stalker_shader.set_mat4("uView", m_view);
    m_stalker_shader.set_mat4("uModel", glm::mat4(1.0f));
    m_stalker_shader.set_vec3("uCamPos", m_cam_pos);
    m_stalker_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_stalker_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_stalker_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_stalker_shader.set_float("uHeadlampEnabled", m_headlamp.enabled ? 1.0f : 0.0f);
    m_stalker_shader.set_float("uStateGlow", 0.0f);
    m_stalker_shader.set_int("uState", 0);

    size_t vert_count = std::min(s_carcass_verts.size(), static_cast<size_t>(8192));
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    GLsizeiptr byte_size = static_cast<GLsizeiptr>(vert_count * sizeof(StalkerVertex));
    glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_carcass_verts.data());

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vert_count));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void Renderer::render_plasma_bolts(const std::vector<PlayerPlasmaBolt>& bolts) {
    if (bolts.empty() || m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_bolt_verts;
    s_bolt_verts.clear();
    if (s_bolt_verts.capacity() < 2048) s_bolt_verts.reserve(2048);

    static std::vector<float> s_bolt_lines;
    s_bolt_lines.clear();
    if (s_bolt_lines.capacity() < 1024) s_bolt_lines.reserve(1024);

    for (const auto& bolt : bolts) {
        if (!bolt.active) continue;

        glm::vec3 b_dir = (glm::length(bolt.velocity) > 0.001f) ? glm::normalize(bolt.velocity) : glm::vec3(0.0f, 0.0f, -1.0f);
        glm::mat4 b_mat = glm::translate(glm::mat4(1.0f), bolt.position);
        float b_yaw = std::atan2(-b_dir.x, -b_dir.z);
        float b_pitch = std::asin(std::clamp(b_dir.y, -1.0f, 1.0f));
        b_mat = glm::rotate(b_mat, b_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        b_mat = glm::rotate(b_mat, -b_pitch, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::vec4 bolt_color = bolt.color;
        glm::vec4 bolt_mat(0.1f, 0.9f, 6.0f, 1.0f);    // Ultra-bright emissive
        float r = (bolt.radius > 0.01f) ? bolt.radius : 0.05f;
        add_stalker_box(s_bolt_verts, b_mat, glm::vec3(r, r, r * 8.0f), bolt_color, bolt_mat);

        // Core energy capsule
        glm::vec4 core_color = glm::mix(bolt_color, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.70f); // White-hot core
        add_stalker_box(s_bolt_verts, b_mat, glm::vec3(r * 0.5f, r * 0.5f, r * 5.0f), core_color, bolt_mat);

        // Luminous streak trail
        glm::vec3 trail_end = bolt.position - b_dir * 1.4f;
        s_bolt_lines.insert(s_bolt_lines.end(), {
            bolt.position.x, bolt.position.y, bolt.position.z,
            trail_end.x, trail_end.y, trail_end.z
        });
    }

    if (s_bolt_verts.empty()) return;

    // Render 3D bolt meshes
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    m_stalker_shader.use();
    m_stalker_shader.set_mat4("uProjection", m_proj);
    m_stalker_shader.set_mat4("uView", m_view);
    m_stalker_shader.set_mat4("uModel", glm::mat4(1.0f));
    m_stalker_shader.set_vec3("uCamPos", m_cam_pos);
    m_stalker_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_stalker_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_stalker_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_stalker_shader.set_float("uHeadlampEnabled", m_headlamp.enabled ? 1.0f : 0.0f);
    m_stalker_shader.set_float("uStateGlow", m_total_time);
    m_stalker_shader.set_int("uState", 1);

    size_t vert_count = std::min(s_bolt_verts.size(), static_cast<size_t>(2048));
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    GLsizeiptr byte_size = static_cast<GLsizeiptr>(vert_count * sizeof(StalkerVertex));
    glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_bolt_verts.data());

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vert_count));
    glBindVertexArray(0);

    // Render luminous tracer lines
    if (!s_bolt_lines.empty() && m_cable_vao != 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        m_wireframe_shader.use();
        m_wireframe_shader.set_mat4("uProjection", m_proj);
        m_wireframe_shader.set_mat4("uView", m_view);
        m_wireframe_shader.set_vec3("uVoxelPos", glm::vec3(0.0f));
        m_wireframe_shader.set_int("uIsLineOnly", 1);
        m_wireframe_shader.set_vec4("uColor", glm::vec4(0.0f, 0.95f, 1.0f, 0.95f));

        size_t line_floats = std::min(s_bolt_lines.size(), static_cast<size_t>(1024));
        glBindVertexArray(m_cable_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
        GLsizeiptr line_bytes = static_cast<GLsizeiptr>(line_floats * sizeof(float));
        glBufferSubData(GL_ARRAY_BUFFER, 0, line_bytes, s_bolt_lines.data());

        glLineWidth(3.0f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(line_floats / 3));

        glBindVertexArray(0);
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }
}

void Renderer::render_burrowers(const std::vector<SeismicBurrower>& burrowers) {
    if (burrowers.empty() || m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_burrower_verts;
    s_burrower_verts.clear();
    if (s_burrower_verts.capacity() < 16384) {
        s_burrower_verts.reserve(16384);
    }

    int active_burrower_count = 0;
    for (const auto& b : burrowers) {
        if (b.is_dead()) continue;
        active_burrower_count++;

        // Base transform: translate to burrower world position and rotate to facing yaw + pitch
        glm::mat4 base_model = glm::translate(glm::mat4(1.0f), b.position);
        base_model = glm::rotate(base_model, b.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        base_model = glm::rotate(base_model, -b.pitch, glm::vec3(1.0f, 0.0f, 0.0f));

        if (b.state == BurrowerState::Stunned) {
            float jitter = std::sin(b.pulse_phase * 40.0f) * 0.12f;
            base_model = glm::rotate(base_model, jitter, glm::vec3(0.0f, 0.0f, 1.0f));
        }

        base_model = glm::scale(base_model, glm::vec3(b.scale));

        // Material Palettes
        // 1. Heavy Armored Basalt Crust
        glm::vec4 armor_color(0.08f, 0.07f, 0.06f, 1.0f);
        glm::vec4 armor_mat(0.35f, 0.45f, 0.0f, 1.0f); // metallic, roughness, emissive, ao

        // 2. Hardened Titanium Borer Teeth
        glm::vec4 cutter_color(0.65f, 0.60f, 0.55f, 1.0f);
        glm::vec4 cutter_mat(0.85f, 0.15f, 0.0f, 1.0f);

        // 3. Molten Magma Core / Slag Vents
        glm::vec4 core_color = b.get_core_color();
        float pulse = 0.85f + 0.35f * std::sin(b.pulse_phase * 6.0f);
        glm::vec4 core_mat(0.1f, 0.05f, 6.0f * pulse, 1.0f); // Intense bloom emission

        // 4. Secondary Obsidian Plates
        glm::vec4 obsidian_color(0.03f, 0.03f, 0.04f, 1.0f);
        glm::vec4 obsidian_mat(0.60f, 0.20f, 0.0f, 1.0f);

        // === 1. ROTATING CONICAL DRILL BORER (HEAD) ===
        {
            glm::mat4 head_m = glm::translate(base_model, glm::vec3(0.0f, 0.0f, 0.65f));
            head_m = glm::rotate(head_m, b.cutter_angle, glm::vec3(0.0f, 0.0f, 1.0f));

            // Central borer cone
            add_stalker_box(s_burrower_verts, head_m, glm::vec3(0.35f, 0.35f, 0.45f), cutter_color, cutter_mat);

            // Rotating cutting teeth (radial)
            for (int i = 0; i < 4; ++i) {
                float rad_angle = static_cast<float>(i) * 1.5707963f;
                glm::mat4 tooth_m = glm::rotate(head_m, rad_angle, glm::vec3(0.0f, 0.0f, 1.0f));
                tooth_m = glm::translate(tooth_m, glm::vec3(0.30f, 0.0f, 0.15f));
                add_stalker_box(s_burrower_verts, tooth_m, glm::vec3(0.14f, 0.08f, 0.28f), cutter_color, cutter_mat);
            }

            // Molten grinder core at the apex tip
            glm::mat4 core_tip = glm::translate(head_m, glm::vec3(0.0f, 0.0f, 0.48f));
            add_stalker_box(s_burrower_verts, core_tip, glm::vec3(0.18f, 0.18f, 0.15f), core_color, core_mat);
        }

        // === 2. PRIMARY MAIN THORAX / CARAPACE ===
        {
            glm::mat4 thorax_m = glm::translate(base_model, glm::vec3(0.0f, 0.0f, 0.10f));
            add_stalker_box(s_burrower_verts, thorax_m, glm::vec3(0.55f, 0.45f, 0.50f), armor_color, armor_mat);

            // Dorsal Armor Spikes (Tectonic Crest)
            glm::mat4 crest_m = glm::translate(thorax_m, glm::vec3(0.0f, 0.36f, 0.0f));
            add_stalker_box(s_burrower_verts, crest_m, glm::vec3(0.12f, 0.22f, 0.42f), obsidian_color, obsidian_mat);

            // Lateral Heat Vents (Glowing exhaust ports)
            glm::mat4 vent_l = glm::translate(thorax_m, glm::vec3(-0.35f, 0.12f, 0.05f));
            add_stalker_box(s_burrower_verts, vent_l, glm::vec3(0.12f, 0.12f, 0.25f), core_color, core_mat);
            glm::mat4 vent_r = glm::translate(thorax_m, glm::vec3(0.35f, 0.12f, 0.05f));
            add_stalker_box(s_burrower_verts, vent_r, glm::vec3(0.12f, 0.12f, 0.25f), core_color, core_mat);
        }

        // === 3. ARTICULATED SUBTERRANEAN BODY SEGMENTS ===
        for (int seg = 1; seg <= 4; ++seg) {
            float seg_z = -0.35f - static_cast<float>(seg) * 0.42f;
            float seg_scale = 1.0f - static_cast<float>(seg) * 0.12f;
            float wiggle_x = std::sin(b.segment_wiggle + static_cast<float>(seg) * 0.9f) * 0.15f;
            float wiggle_y = std::cos(b.segment_wiggle * 0.7f + static_cast<float>(seg) * 0.6f) * 0.08f;

            glm::mat4 seg_m = glm::translate(base_model, glm::vec3(wiggle_x, wiggle_y, seg_z));
            add_stalker_box(s_burrower_verts, seg_m, glm::vec3(0.48f * seg_scale, 0.38f * seg_scale, 0.38f), armor_color, armor_mat);

            // Dorsal ridge for each segment
            glm::mat4 seg_crest = glm::translate(seg_m, glm::vec3(0.0f, 0.30f * seg_scale, 0.0f));
            add_stalker_box(s_burrower_verts, seg_crest, glm::vec3(0.08f, 0.16f * seg_scale, 0.28f), obsidian_color, obsidian_mat);

            // Inter-segment molten seams
            if (seg % 2 == 1) {
                glm::mat4 seam_m = glm::translate(seg_m, glm::vec3(0.0f, -0.15f * seg_scale, 0.18f));
                add_stalker_box(s_burrower_verts, seam_m, glm::vec3(0.32f * seg_scale, 0.08f, 0.06f), core_color, core_mat);
            }
        }
    }

    if (active_burrower_count == 0 || s_burrower_verts.empty()) return;

    // Render Burrower PBR Meshes
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    m_stalker_shader.use();
    m_stalker_shader.set_mat4("uProjection", m_proj);
    m_stalker_shader.set_mat4("uView", m_view);
    m_stalker_shader.set_mat4("uModel", glm::mat4(1.0f));
    m_stalker_shader.set_vec3("uCamPos", m_cam_pos);
    m_stalker_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_stalker_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_stalker_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_stalker_shader.set_float("uHeadlampEnabled", m_headlamp.enabled ? 1.0f : 0.0f);
    m_stalker_shader.set_float("uStateGlow", m_total_time);
    m_stalker_shader.set_int("uState", 3); // Emissive boost state

    size_t vert_count = std::min(s_burrower_verts.size(), static_cast<size_t>(16384));
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    GLsizeiptr byte_size = static_cast<GLsizeiptr>(vert_count * sizeof(StalkerVertex));
    glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_burrower_verts.data());

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vert_count));
    glBindVertexArray(0);
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
        m_fog_compute_shader.set_float("uHeadlampIntensity", get_effective_headlamp_intensity());

        if (m_dust_timer > 0.0f) {
            m_dust_timer = std::max(0.0f, m_dust_timer - delta_time);
        }
        float dust_boost = (m_dust_timer > 0.0f) ? (0.075f * (m_dust_timer / 3.0f)) : 0.0f;

        m_fog_compute_shader.set_float("uFogDensity", 0.035f + dust_boost);
        m_fog_compute_shader.set_float("uToxicHazeFactor", glm::clamp(radiation_level / 100.0f, 0.0f, 1.0f));
        m_fog_compute_shader.set_float("uTime", m_total_time);
        m_fog_compute_shader.set_int("uSector", m_sector);

        // Upload up to 8 point lights for atmospheric volumetric fog scattering
        int fog_lights = std::min(static_cast<int>(m_point_lights.size()), 8);
        m_fog_compute_shader.set_int("uNumPointLights", fog_lights);
        for (int i = 0; i < fog_lights; ++i) {
            std::string base = "uPointLights[" + std::to_string(i) + "]";
            m_fog_compute_shader.set_vec3(base + ".position", m_point_lights[i].position);
            m_fog_compute_shader.set_vec3(base + ".color", m_point_lights[i].color);
            m_fog_compute_shader.set_float(base + ".radius", m_point_lights[i].radius);
            m_fog_compute_shader.set_float(base + ".intensity", m_point_lights[i].intensity);
        }

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

    m_postprocess_shader.set_float("uExposure", 1.15f * m_brightness);
    m_postprocess_shader.set_float("uBloomIntensity", 0.75f);
    m_postprocess_shader.set_float("uRadiationGlitch", glm::clamp(radiation_level / 100.0f, 0.0f, 1.0f));
    m_postprocess_shader.set_float("uTime", m_total_time);

    render_quad();
}

void Renderer::render_delver(const glm::vec3& pos, float yaw, CharacterClass cls, float anim_time) {
    if (m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_delver_verts;
    s_delver_verts.clear();
    if (s_delver_verts.capacity() < 16384) {
        s_delver_verts.reserve(16384);
    }

    auto attr = get_character_attributes(cls);

    // Base transform: translate to world position and rotate
    glm::mat4 base_model = glm::translate(glm::mat4(1.0f), pos);
    base_model = glm::rotate(base_model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));

    // Breathing idle animation bob
    float breath_y = std::sin(anim_time * 2.5f) * 0.015f;
    base_model = glm::translate(base_model, glm::vec3(0.0f, breath_y, 0.0f));

    // Colors & Materials
    glm::vec4 suit_color = attr.suitSleeveColor;
    glm::vec4 suit_mat(0.15f, 0.65f, 0.0f, 1.0f); // metallic, roughness, emissive, ao

    glm::vec4 accent_color = attr.primaryAccentColor;
    glm::vec4 plate_mat(0.70f, 0.25f, 0.0f, 1.0f);

    glm::vec4 dark_armor_color(0.10f, 0.11f, 0.13f, 1.0f);
    glm::vec4 dark_armor_mat(0.80f, 0.20f, 0.0f, 1.0f);

    glm::vec4 glove_color = attr.gloveColor;
    glm::vec4 glove_mat(0.40f, 0.40f, 0.0f, 1.0f);

    glm::vec4 helmet_shell_color(0.82f, 0.85f, 0.88f, 1.0f);
    glm::vec4 helmet_mat(0.60f, 0.30f, 0.0f, 1.0f);

    // Glowing Visor
    glm::vec4 visor_color;
    if (cls == CharacterClass::Demolitionist) {
        visor_color = glm::vec4(1.0f, 0.65f, 0.10f, 1.0f); // Amber / orange blast visor
    } else if (cls == CharacterClass::Vanguard) {
        visor_color = glm::vec4(0.20f, 0.90f, 1.0f, 1.0f);  // Cyan heavy optical visor
    } else {
        visor_color = glm::vec4(0.15f, 1.0f, 0.45f, 1.0f);  // Emerald sonar pathfinder visor
    }
    float visor_pulse = 0.9f + 0.1f * std::sin(anim_time * 4.0f);
    glm::vec4 visor_mat(0.1f, 0.05f, 5.0f * visor_pulse, 1.0f);

    // Glowing chest LED telemetry
    glm::vec4 led_mat(0.0f, 0.0f, 4.5f, 1.0f);

    // === 1. TORSO & LIFE SUPPORT MODULE ===
    {
        // Core pressure suit chest
        glm::mat4 chest_m = glm::translate(base_model, glm::vec3(0.0f, 1.05f, 0.0f));
        add_stalker_box(s_delver_verts, chest_m, glm::vec3(0.25f, 0.28f, 0.16f), suit_color, suit_mat);

        // Armored chest plate with class accent
        glm::mat4 plate_m = glm::translate(chest_m, glm::vec3(0.0f, 0.04f, 0.14f));
        add_stalker_box(s_delver_verts, plate_m, glm::vec3(0.22f, 0.20f, 0.04f), accent_color, plate_mat);

        // Life support module mounted on chest plate
        glm::mat4 life_m = glm::translate(plate_m, glm::vec3(0.0f, 0.0f, 0.05f));
        add_stalker_box(s_delver_verts, life_m, glm::vec3(0.12f, 0.10f, 0.03f), dark_armor_color, dark_armor_mat);

        // Chest status LED bar
        glm::mat4 led_m = glm::translate(life_m, glm::vec3(0.0f, 0.05f, 0.035f));
        add_stalker_box(s_delver_verts, led_m, glm::vec3(0.08f, 0.015f, 0.01f), visor_color, led_mat);

        // Lower abdominal flex segment
        glm::mat4 abdomen_m = glm::translate(base_model, glm::vec3(0.0f, 0.82f, 0.0f));
        add_stalker_box(s_delver_verts, abdomen_m, glm::vec3(0.21f, 0.10f, 0.14f), dark_armor_color, dark_armor_mat);

        // Heavy utility belt
        glm::mat4 belt_m = glm::translate(base_model, glm::vec3(0.0f, 0.72f, 0.0f));
        add_stalker_box(s_delver_verts, belt_m, glm::vec3(0.23f, 0.05f, 0.16f), glove_color, glove_mat);

        // Belt accessory pouches
        glm::mat4 pouch_l = glm::translate(belt_m, glm::vec3(-0.24f, 0.0f, 0.02f));
        add_stalker_box(s_delver_verts, pouch_l, glm::vec3(0.03f, 0.06f, 0.10f), accent_color, plate_mat);
        glm::mat4 pouch_r = glm::translate(belt_m, glm::vec3(0.24f, 0.0f, 0.02f));
        add_stalker_box(s_delver_verts, pouch_r, glm::vec3(0.03f, 0.06f, 0.10f), accent_color, plate_mat);
    }

    // === 2. HELMET & GLOWING VISOR ===
    {
        // Neck collar / pressure ring
        glm::mat4 neck_m = glm::translate(base_model, glm::vec3(0.0f, 1.34f, 0.0f));
        add_stalker_box(s_delver_verts, neck_m, glm::vec3(0.18f, 0.04f, 0.18f), dark_armor_color, dark_armor_mat);

        // Outer EVA helmet shell
        glm::mat4 helm_m = glm::translate(base_model, glm::vec3(0.0f, 1.54f, 0.0f));
        add_stalker_box(s_delver_verts, helm_m, glm::vec3(0.20f, 0.18f, 0.20f), helmet_shell_color, helmet_mat);

        // Class accent helmet crown ridge
        glm::mat4 crown_m = glm::translate(helm_m, glm::vec3(0.0f, 0.17f, 0.0f));
        add_stalker_box(s_delver_verts, crown_m, glm::vec3(0.08f, 0.03f, 0.18f), accent_color, plate_mat);

        // Visor: Forward glowing faceplate
        glm::mat4 visor_m = glm::translate(helm_m, glm::vec3(0.0f, 0.0f, 0.16f));
        float visor_h = (cls == CharacterClass::Vanguard) ? 0.07f : 0.12f; // Vanguard has heavy slit optics
        add_stalker_box(s_delver_verts, visor_m, glm::vec3(0.15f, visor_h, 0.06f), visor_color, visor_mat);

        // Lateral audio / radio ear pods
        glm::mat4 ear_l = glm::translate(helm_m, glm::vec3(-0.21f, 0.0f, -0.02f));
        add_stalker_box(s_delver_verts, ear_l, glm::vec3(0.03f, 0.08f, 0.08f), dark_armor_color, dark_armor_mat);
        glm::mat4 ear_r = glm::translate(helm_m, glm::vec3(0.21f, 0.0f, -0.02f));
        add_stalker_box(s_delver_verts, ear_r, glm::vec3(0.03f, 0.08f, 0.08f), dark_armor_color, dark_armor_mat);
    }

    // === 3. BACKPACK THRUSTER UNIT (EXO-PACK) ===
    {
        glm::mat4 pack_m = glm::translate(base_model, glm::vec3(0.0f, 1.08f, -0.24f));
        add_stalker_box(s_delver_verts, pack_m, glm::vec3(0.20f, 0.26f, 0.10f), dark_armor_color, dark_armor_mat);

        // Lateral thruster nozzles
        glm::mat4 noz_l = glm::translate(pack_m, glm::vec3(-0.14f, -0.24f, 0.0f));
        noz_l = glm::rotate(noz_l, 0.2f, glm::vec3(1.0f, 0.0f, 0.0f));
        add_stalker_box(s_delver_verts, noz_l, glm::vec3(0.05f, 0.08f, 0.05f), accent_color, plate_mat);

        glm::mat4 noz_r = glm::translate(pack_m, glm::vec3(0.14f, -0.24f, 0.0f));
        noz_r = glm::rotate(noz_r, 0.2f, glm::vec3(1.0f, 0.0f, 0.0f));
        add_stalker_box(s_delver_verts, noz_r, glm::vec3(0.05f, 0.08f, 0.05f), accent_color, plate_mat);

        // Thruster exhaust glow
        glm::mat4 ex_l = glm::translate(noz_l, glm::vec3(0.0f, -0.06f, 0.0f));
        add_stalker_box(s_delver_verts, ex_l, glm::vec3(0.03f, 0.02f, 0.03f), visor_color, led_mat);
        glm::mat4 ex_r = glm::translate(noz_r, glm::vec3(0.0f, -0.06f, 0.0f));
        add_stalker_box(s_delver_verts, ex_r, glm::vec3(0.03f, 0.02f, 0.03f), visor_color, led_mat);

        // Emergency beacon antenna
        glm::mat4 ant_m = glm::translate(pack_m, glm::vec3(0.16f, 0.32f, 0.0f));
        add_stalker_box(s_delver_verts, ant_m, glm::vec3(0.015f, 0.14f, 0.015f), accent_color, plate_mat);
    }

    // === 4. ARMORED PAULDRONS (SHOULDERS) ===
    {
        float pauldron_w = (cls == CharacterClass::Vanguard) ? 0.16f : 0.11f;
        float pauldron_h = (cls == CharacterClass::Vanguard) ? 0.14f : 0.10f;

        glm::mat4 sh_l = glm::translate(base_model, glm::vec3(-0.34f, 1.24f, 0.0f));
        sh_l = glm::rotate(sh_l, 0.15f, glm::vec3(0.0f, 0.0f, 1.0f));
        add_stalker_box(s_delver_verts, sh_l, glm::vec3(pauldron_w, pauldron_h, 0.14f), accent_color, plate_mat);

        glm::mat4 sh_r = glm::translate(base_model, glm::vec3(0.34f, 1.24f, 0.0f));
        sh_r = glm::rotate(sh_r, -0.15f, glm::vec3(0.0f, 0.0f, 1.0f));
        add_stalker_box(s_delver_verts, sh_r, glm::vec3(pauldron_w, pauldron_h, 0.14f), accent_color, plate_mat);
    }

    // === 5. ARMS & GAUNTLETS ===
    {
        // Left Arm (Relaxed / Tactical Support)
        glm::mat4 arm_l = glm::translate(base_model, glm::vec3(-0.35f, 1.00f, 0.04f));
        add_stalker_box(s_delver_verts, arm_l, glm::vec3(0.08f, 0.18f, 0.08f), suit_color, suit_mat);

        glm::mat4 fore_l = glm::translate(base_model, glm::vec3(-0.34f, 0.76f, 0.10f));
        fore_l = glm::rotate(fore_l, -0.2f, glm::vec3(1.0f, 0.0f, 0.0f));
        add_stalker_box(s_delver_verts, fore_l, glm::vec3(0.07f, 0.14f, 0.07f), dark_armor_color, dark_armor_mat);

        glm::mat4 hand_l = glm::translate(fore_l, glm::vec3(0.0f, -0.14f, 0.0f));
        add_stalker_box(s_delver_verts, hand_l, glm::vec3(0.06f, 0.07f, 0.06f), glove_color, glove_mat);

        // Right Arm (Forward Held Drill / Class Weapon)
        glm::mat4 arm_r = glm::translate(base_model, glm::vec3(0.34f, 1.04f, 0.10f));
        arm_r = glm::rotate(arm_r, -0.35f, glm::vec3(1.0f, 0.0f, 0.0f));
        add_stalker_box(s_delver_verts, arm_r, glm::vec3(0.08f, 0.18f, 0.08f), suit_color, suit_mat);

        glm::mat4 fore_r = glm::translate(base_model, glm::vec3(0.32f, 0.82f, 0.28f));
        fore_r = glm::rotate(fore_r, -0.55f, glm::vec3(1.0f, 0.0f, 0.0f));
        add_stalker_box(s_delver_verts, fore_r, glm::vec3(0.07f, 0.14f, 0.07f), dark_armor_color, dark_armor_mat);

        glm::mat4 hand_r = glm::translate(fore_r, glm::vec3(0.0f, -0.14f, 0.0f));
        add_stalker_box(s_delver_verts, hand_r, glm::vec3(0.06f, 0.07f, 0.06f), glove_color, glove_mat);

        // Held Class Equipment (Right Hand)
        glm::mat4 tool_m = glm::translate(hand_r, glm::vec3(0.0f, 0.0f, 0.18f));
        // Tool Chassis
        add_stalker_box(s_delver_verts, tool_m, glm::vec3(0.08f, 0.09f, 0.16f), dark_armor_color, dark_armor_mat);
        // Tool Accent Housing
        glm::mat4 tool_h = glm::translate(tool_m, glm::vec3(0.0f, 0.06f, 0.0f));
        add_stalker_box(s_delver_verts, tool_h, glm::vec3(0.09f, 0.03f, 0.12f), accent_color, plate_mat);
        // Rotating Drill Borer Bit (held in front)
        glm::mat4 bit_m = glm::translate(tool_m, glm::vec3(0.0f, 0.0f, 0.22f));
        bit_m = glm::rotate(bit_m, anim_time * 12.0f, glm::vec3(0.0f, 0.0f, 1.0f));
        add_stalker_box(s_delver_verts, bit_m, glm::vec3(0.06f, 0.06f, 0.12f), helmet_shell_color, helmet_mat);
    }

    // === 6. LEGS & PRESSURE BOOTS ===
    {
        // Left Thigh & Right Thigh
        glm::mat4 th_l = glm::translate(base_model, glm::vec3(-0.13f, 0.52f, 0.0f));
        add_stalker_box(s_delver_verts, th_l, glm::vec3(0.09f, 0.18f, 0.10f), suit_color, suit_mat);

        glm::mat4 th_r = glm::translate(base_model, glm::vec3(0.13f, 0.52f, 0.0f));
        add_stalker_box(s_delver_verts, th_r, glm::vec3(0.09f, 0.18f, 0.10f), suit_color, suit_mat);

        // Knee Guards with class accent
        glm::mat4 knee_l = glm::translate(base_model, glm::vec3(-0.13f, 0.35f, 0.10f));
        add_stalker_box(s_delver_verts, knee_l, glm::vec3(0.08f, 0.06f, 0.04f), accent_color, plate_mat);
        glm::mat4 knee_r = glm::translate(base_model, glm::vec3(0.13f, 0.35f, 0.10f));
        add_stalker_box(s_delver_verts, knee_r, glm::vec3(0.08f, 0.06f, 0.04f), accent_color, plate_mat);

        // Shins
        glm::mat4 shin_l = glm::translate(base_model, glm::vec3(-0.13f, 0.20f, 0.01f));
        add_stalker_box(s_delver_verts, shin_l, glm::vec3(0.08f, 0.14f, 0.09f), dark_armor_color, dark_armor_mat);
        glm::mat4 shin_r = glm::translate(base_model, glm::vec3(0.13f, 0.20f, 0.01f));
        add_stalker_box(s_delver_verts, shin_r, glm::vec3(0.08f, 0.14f, 0.09f), dark_armor_color, dark_armor_mat);

        // Heavy Magnetic Lock Boots
        glm::mat4 boot_l = glm::translate(base_model, glm::vec3(-0.13f, 0.05f, 0.04f));
        add_stalker_box(s_delver_verts, boot_l, glm::vec3(0.10f, 0.06f, 0.15f), glove_color, glove_mat);
        glm::mat4 boot_r = glm::translate(base_model, glm::vec3(0.13f, 0.05f, 0.04f));
        add_stalker_box(s_delver_verts, boot_r, glm::vec3(0.10f, 0.06f, 0.15f), glove_color, glove_mat);
    }

    // === DRAW 3D PBR DELVER MESH ===
    if (!s_delver_verts.empty()) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        m_stalker_shader.use();
        m_stalker_shader.set_mat4("uProjection", m_proj);
        m_stalker_shader.set_mat4("uView", m_view);
        m_stalker_shader.set_mat4("uModel", glm::mat4(1.0f));
        m_stalker_shader.set_vec3("uCamPos", m_cam_pos);
        m_stalker_shader.set_vec3("uHeadlampPos", m_headlamp.position);
        m_stalker_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
        m_stalker_shader.set_vec3("uHeadlampColor", m_headlamp.color);
        m_stalker_shader.set_float("uHeadlampEnabled", m_headlamp.enabled ? 1.0f : 0.0f);
        m_stalker_shader.set_float("uStateGlow", anim_time);
        m_stalker_shader.set_int("uState", 1);

        size_t vert_count = std::min(s_delver_verts.size(), static_cast<size_t>(16384));
        glBindVertexArray(m_stalker_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
        GLsizeiptr byte_size = static_cast<GLsizeiptr>(vert_count * sizeof(StalkerVertex));
        glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_delver_verts.data());

        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vert_count));
        glBindVertexArray(0);
    }
}

void Renderer::render_extraction_pod(const glm::vec3& beacon_pos, float drill_progress, float ramp_extension, float time, bool is_anchored) {
    if (m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_pod_verts;
    s_pod_verts.clear();
    if (s_pod_verts.capacity() < 8192) {
        s_pod_verts.reserve(8192);
    }

    drill_progress = glm::clamp(drill_progress, 0.0f, 1.0f);
    ramp_extension = glm::clamp(ramp_extension, 0.0f, 1.0f);

    float descent_y = beacon_pos.y + (1.0f - drill_progress) * 16.0f;
    glm::vec3 pod_base = glm::vec3(beacon_pos.x, descent_y, beacon_pos.z);
    glm::mat4 base_model = glm::translate(glm::mat4(1.0f), pod_base);

    glm::vec4 col_hull(0.20f, 0.22f, 0.26f, 1.0f);           // Heavy titanium composite armor
    glm::vec4 col_inner_hull(0.12f, 0.14f, 0.16f, 1.0f);     // Dark interior cabin
    glm::vec4 col_hazard_orange(0.95f, 0.55f, 0.08f, 1.0f);  // Industrial extraction orange
    glm::vec4 col_hazard_yellow(0.96f, 0.82f, 0.12f, 1.0f);  // Warning chevron stripe
    glm::vec4 col_hazard_dark(0.08f, 0.08f, 0.09f, 1.0f);
    glm::vec4 col_steel(0.55f, 0.58f, 0.62f, 1.0f);
    glm::vec4 col_drill_teeth(0.78f, 0.72f, 0.65f, 1.0f);    // Tungsten carbide teeth
    glm::vec4 col_cabin_glow(0.20f, 0.95f, 1.0f, 1.0f);      // Cyan airlock cabin illumination
    glm::vec4 col_amber_warning(1.0f, 0.68f, 0.10f, 1.0f);   // Pulsating amber warning strip

    glm::vec4 mat_armor(0.45f, 0.40f, 0.0f, 0.90f);
    glm::vec4 mat_metal(0.72f, 0.35f, 0.0f, 0.90f);
    float pulse = 0.65f + 0.35f * std::sin(time * 8.0f);
    glm::vec4 mat_amber_led(0.0f, 0.2f, 4.0f * pulse, 1.0f);
    glm::vec4 mat_cabin_led(0.0f, 0.1f, 3.5f, 1.0f);

    // 1. MAIN CYLINDRICAL EXTRACTION CAPSULE HULL (8 faceted armor bulkheads)
    const int num_sides = 8;
    float hull_radius = 1.45f;
    float hull_bottom = 0.45f;
    float hull_top = 4.20f;
    float hull_mid_y = (hull_bottom + hull_top) * 0.5f;

    for (int i = 0; i < num_sides; ++i) {
        float a0 = glm::radians(i * (360.0f / num_sides));
        float a1 = glm::radians((i + 1) * (360.0f / num_sides));
        float mid_a = (a0 + a1) * 0.5f;

        // Skip front panel (+Z) where airlock door is situated
        if (i == 1 || i == 2) {
            // Door frame header & threshold
            glm::mat4 header_m = glm::translate(base_model, glm::vec3(std::cos(mid_a) * hull_radius * 0.95f, hull_top - 0.35f, std::sin(mid_a) * hull_radius * 0.95f));
            header_m = glm::rotate(header_m, mid_a, glm::vec3(0.0f, 1.0f, 0.0f));
            add_stalker_box(s_pod_verts, header_m, glm::vec3(0.45f, 0.35f, 0.12f), col_hazard_orange, mat_armor);

            glm::mat4 thresh_m = glm::translate(base_model, glm::vec3(std::cos(mid_a) * hull_radius * 0.95f, hull_bottom + 0.15f, std::sin(mid_a) * hull_radius * 0.95f));
            thresh_m = glm::rotate(thresh_m, mid_a, glm::vec3(0.0f, 1.0f, 0.0f));
            add_stalker_box(s_pod_verts, thresh_m, glm::vec3(0.45f, 0.15f, 0.14f), col_hazard_orange, mat_armor);
            continue;
        }

        glm::vec3 c_pos = glm::vec3(std::cos(mid_a) * hull_radius, hull_mid_y, std::sin(mid_a) * hull_radius);
        glm::mat4 panel_m = glm::translate(base_model, c_pos);
        panel_m = glm::rotate(panel_m, mid_a, glm::vec3(0.0f, 1.0f, 0.0f));

        // Outer reinforced bulkhead panel
        add_stalker_box(s_pod_verts, panel_m, glm::vec3(0.55f, (hull_top - hull_bottom) * 0.5f, 0.10f), col_hull, mat_armor);

        // Orange reinforced rib across the middle
        glm::mat4 rib_m = glm::translate(panel_m, glm::vec3(0.0f, 0.0f, 0.08f));
        add_stalker_box(s_pod_verts, rib_m, glm::vec3(0.50f, 0.16f, 0.04f), col_hazard_orange, mat_metal);

        // Warning chevrons near bottom
        glm::mat4 chv_m = glm::translate(panel_m, glm::vec3(0.0f, -1.20f, 0.08f));
        glm::vec4 chv_col = (i % 2 == 0) ? col_hazard_yellow : col_hazard_dark;
        add_stalker_box(s_pod_verts, chv_m, glm::vec3(0.48f, 0.12f, 0.03f), chv_col, mat_armor);
    }

    // Interior warm cabin chamber (visible through open door)
    glm::mat4 cabin_m = glm::translate(base_model, glm::vec3(0.0f, 2.0f, 0.0f));
    add_stalker_box(s_pod_verts, cabin_m, glm::vec3(0.85f, 1.4f, 0.85f), col_inner_hull, mat_armor);
    // Interior glowing status console
    glm::mat4 console_m = glm::translate(cabin_m, glm::vec3(0.0f, 0.2f, -0.70f));
    add_stalker_box(s_pod_verts, console_m, glm::vec3(0.60f, 0.45f, 0.08f), col_cabin_glow, mat_cabin_led);

    // 2. TOP CONICAL AUGER DRILL BIT (Rotates down from ceiling)
    float drill_rot = is_anchored ? 0.0f : (time * 18.0f);
    glm::mat4 drill_base = glm::translate(base_model, glm::vec3(0.0f, hull_top, 0.0f));
    drill_base = glm::rotate(drill_base, drill_rot, glm::vec3(0.0f, 1.0f, 0.0f));

    // Conical tiers
    for (int t = 0; t < 4; ++t) {
        float ty = t * 0.65f;
        float trad = hull_radius * (1.0f - t * 0.22f);
        glm::mat4 tier_m = glm::translate(drill_base, glm::vec3(0.0f, ty + 0.32f, 0.0f));
        add_stalker_box(s_pod_verts, tier_m, glm::vec3(trad, 0.32f, trad), col_steel, mat_metal);

        // Fluted diamond cutting teeth along periphery
        for (int tooth = 0; tooth < 4; ++tooth) {
            float ta = glm::radians(tooth * 90.0f + t * 25.0f);
            glm::mat4 tooth_m = glm::translate(tier_m, glm::vec3(std::cos(ta) * trad * 1.02f, 0.0f, std::sin(ta) * trad * 1.02f));
            tooth_m = glm::rotate(tooth_m, ta, glm::vec3(0.0f, 1.0f, 0.0f));
            add_stalker_box(s_pod_verts, tooth_m, glm::vec3(0.12f, 0.24f, 0.12f), col_drill_teeth, mat_metal);
        }
    }
    // Drill apex borer spike
    glm::mat4 spike_m = glm::translate(drill_base, glm::vec3(0.0f, 2.80f, 0.0f));
    add_stalker_box(s_pod_verts, spike_m, glm::vec3(0.20f, 0.45f, 0.20f), col_drill_teeth, mat_metal);

    // 3. FOUR HEAVY HYDRAULIC ANCHORING STRUTS & GROUND PADS
    for (int leg = 0; leg < 4; ++leg) {
        float leg_ang = glm::radians(leg * 90.0f + 45.0f);
        float lx = std::cos(leg_ang);
        float lz = std::sin(leg_ang);

        // Upper angled shoulder boom
        glm::vec3 boom_start = glm::vec3(lx * (hull_radius * 0.85f), 2.2f, lz * (hull_radius * 0.85f));
        glm::vec3 boom_end = glm::vec3(lx * 2.10f, 1.4f, lz * 2.10f);
        glm::mat4 boom_m = glm::translate(base_model, (boom_start + boom_end) * 0.5f);
        boom_m = glm::rotate(boom_m, leg_ang, glm::vec3(0.0f, 1.0f, 0.0f));
        add_stalker_box(s_pod_verts, boom_m, glm::vec3(0.16f, 0.18f, 0.55f), col_hazard_orange, mat_armor);

        // Vertical hydraulic cylinder
        glm::vec3 cyl_pos = glm::vec3(lx * 2.10f, 0.70f, lz * 2.10f);
        glm::mat4 cyl_m = glm::translate(base_model, cyl_pos);
        add_stalker_box(s_pod_verts, cyl_m, glm::vec3(0.12f, 0.65f, 0.12f), col_steel, mat_metal);

        // Heavy footing pad anchored on bedrock
        glm::vec3 foot_pos = glm::vec3(lx * 2.10f, 0.08f, lz * 2.10f);
        glm::mat4 foot_m = glm::translate(base_model, foot_pos);
        add_stalker_box(s_pod_verts, foot_m, glm::vec3(0.35f, 0.08f, 0.35f), col_hull, mat_armor);
    }

    // 4. SIDE AIRLOCK BOARDING RAMP (Pivots down toward +Z when anchored)
    if (ramp_extension > 0.01f) {
        float ramp_len = 2.4f * ramp_extension;
        float ramp_pitch = glm::radians(18.0f); // Incline from threshold to floor

        glm::vec3 hinge_pos = glm::vec3(0.0f, 0.45f, hull_radius * 0.95f);
        glm::mat4 ramp_m = glm::translate(base_model, hinge_pos);
        ramp_m = glm::rotate(ramp_m, ramp_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 ramp_deck = glm::translate(ramp_m, glm::vec3(0.0f, -0.04f, ramp_len * 0.5f));

        // Walkway deck
        add_stalker_box(s_pod_verts, ramp_deck, glm::vec3(0.55f, 0.04f, ramp_len * 0.5f), col_steel, mat_armor);

        // Hazard chevron stripes on deck
        for (int s = 0; s < 5; ++s) {
            float sz = (s - 2) * (ramp_len * 0.18f);
            glm::mat4 chv_bar = glm::translate(ramp_deck, glm::vec3(0.0f, 0.045f, sz));
            glm::vec4 chv_c = (s % 2 == 0) ? col_hazard_yellow : col_hazard_dark;
            add_stalker_box(s_pod_verts, chv_bar, glm::vec3(0.48f, 0.005f, 0.08f), chv_c, mat_armor);
        }

        // Left and Right Handrails with Pulsating Amber LED Warning Strips
        glm::mat4 rail_l = glm::translate(ramp_deck, glm::vec3(-0.55f, 0.35f, 0.0f));
        add_stalker_box(s_pod_verts, rail_l, glm::vec3(0.04f, 0.32f, ramp_len * 0.5f), col_hazard_orange, mat_metal);
        glm::mat4 strip_l = glm::translate(rail_l, glm::vec3(0.045f, 0.28f, 0.0f));
        add_stalker_box(s_pod_verts, strip_l, glm::vec3(0.015f, 0.03f, ramp_len * 0.48f), col_amber_warning, mat_amber_led);

        glm::mat4 rail_r = glm::translate(ramp_deck, glm::vec3(0.55f, 0.35f, 0.0f));
        add_stalker_box(s_pod_verts, rail_r, glm::vec3(0.04f, 0.32f, ramp_len * 0.5f), col_hazard_orange, mat_metal);
        glm::mat4 strip_r = glm::translate(rail_r, glm::vec3(-0.045f, 0.28f, 0.0f));
        add_stalker_box(s_pod_verts, strip_r, glm::vec3(0.015f, 0.03f, ramp_len * 0.48f), col_amber_warning, mat_amber_led);
    }

    // 5. EMERGENCY STEAM / PRESSURE VENT PORTS
    for (int vent = 0; vent < 2; ++vent) {
        float vx = (vent == 0) ? -hull_radius * 0.95f : hull_radius * 0.95f;
        glm::mat4 vent_m = glm::translate(base_model, glm::vec3(vx, hull_top - 0.40f, 0.0f));
        add_stalker_box(s_pod_verts, vent_m, glm::vec3(0.12f, 0.18f, 0.35f), col_steel, mat_metal);
        glm::mat4 vent_grate = glm::translate(vent_m, glm::vec3((vent == 0 ? -0.13f : 0.13f), 0.0f, 0.0f));
        add_stalker_box(s_pod_verts, vent_grate, glm::vec3(0.02f, 0.14f, 0.28f), col_amber_warning, mat_amber_led);
    }

    if (s_pod_verts.empty()) return;

    // Render physical 3D extraction pod into HDR buffer with full depth testing
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    m_stalker_shader.use();
    m_stalker_shader.set_mat4("uProjection", m_proj);
    m_stalker_shader.set_mat4("uView", m_view);
    m_stalker_shader.set_mat4("uModel", glm::mat4(1.0f));
    m_stalker_shader.set_vec3("uCamPos", m_cam_pos);
    m_stalker_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_stalker_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_stalker_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_stalker_shader.set_float("uHeadlampEnabled", m_headlamp.enabled ? 1.0f : 0.0f);
    m_stalker_shader.set_float("uStateGlow", time);
    m_stalker_shader.set_int("uState", 0);
    m_stalker_shader.set_float("uDissolveThreshold", 0.0f);
    m_stalker_shader.set_float("u_dissolveThreshold", 0.0f);

    size_t vert_count = std::min(s_pod_verts.size(), static_cast<size_t>(16384));
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_count * sizeof(StalkerVertex), s_pod_verts.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vert_count));
    glBindVertexArray(0);
}

} // namespace Voidfall
