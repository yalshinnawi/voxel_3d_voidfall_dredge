#include "renderer.hpp"
#include "particle_system.hpp"
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
#ifndef GL_TEXTURE4
#define GL_TEXTURE4 0x84C4
#endif

typedef void (APIENTRY *PFNGLPOLYGONOFFSETPROC)(GLfloat factor, GLfloat units);
static PFNGLPOLYGONOFFSETPROC s_glPolygonOffset = nullptr;

namespace Voidfall {

Renderer::Renderer(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_fog_width(std::max(1, width / 2))
    , m_fog_height(std::max(1, height / 2))
{
    // Initialize default clear color to the dark void tone
    glClearColor(0.015f, 0.018f, 0.024f, 1.0f);

    // 1. Load Shaders
    m_voxel_shader.load_graphics("assets/shaders/voxel_pbr.vert", "assets/shaders/voxel_pbr.frag");
    m_fog_compute_shader.load_compute("assets/shaders/volumetric_fog.comp");
    m_bloom_shader.load_graphics("assets/shaders/fullscreen_quad.vert", "assets/shaders/bloom.frag");
    m_postprocess_shader.load_graphics("assets/shaders/fullscreen_quad.vert", "assets/shaders/postprocess.frag");
    m_wireframe_shader.load_graphics("assets/shaders/wireframe.vert", "assets/shaders/wireframe.frag");
    m_particle_shader.load_graphics("assets/shaders/particle.vert", "assets/shaders/particle.frag");
    m_stalker_shader.load_graphics("assets/shaders/stalker.vert", "assets/shaders/stalker.frag");

    // 2. Initialize Texture Array
    m_texture_array = std::make_unique<TextureArray>(128, 128, 18);

    // 2b. Initialize Clustered Forward Lighting (16x9x24 Frustum Grid)
    m_clustered_lighting = std::make_unique<ClusteredLighting>();
    m_clustered_lighting->init();

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
    init_sonar_buffers();
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
    if (m_sonar_vao != 0) glDeleteVertexArrays(1, &m_sonar_vao);
    if (m_sonar_vbo != 0) glDeleteBuffers(1, &m_sonar_vbo);
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

void Renderer::init_sonar_buffers() {
    glGenVertexArrays(1, &m_sonar_vao);
    glGenBuffers(1, &m_sonar_vbo);
    glBindVertexArray(m_sonar_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_sonar_vbo);
    glBufferData(GL_ARRAY_BUFFER, 65536 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

void Renderer::init_cable_buffer() {
    glGenVertexArrays(1, &m_cable_vao);
    glGenBuffers(1, &m_cable_vbo);
    glBindVertexArray(m_cable_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
    m_cable_vbo_capacity = 16384;
    glBufferData(GL_ARRAY_BUFFER, m_cable_vbo_capacity * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

void Renderer::ensure_cable_vbo_capacity(size_t required_floats) {
    if (required_floats > m_cable_vbo_capacity) {
        m_cable_vbo_capacity = std::max(m_cable_vbo_capacity * 2, required_floats);
        glBindVertexArray(m_cable_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_cable_vbo_capacity * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
        glBindVertexArray(0);
    }
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
    m_stalker_vbo_capacity = 65536;
    glBufferData(GL_ARRAY_BUFFER, m_stalker_vbo_capacity * sizeof(StalkerVertex), nullptr, GL_DYNAMIC_DRAW);

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

void Renderer::ensure_stalker_vbo_capacity(size_t required_verts) {
    if (required_verts > m_stalker_vbo_capacity) {
        m_stalker_vbo_capacity = std::max(m_stalker_vbo_capacity * 2, required_verts);
        glBindVertexArray(m_stalker_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_stalker_vbo_capacity * sizeof(StalkerVertex), nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(StalkerVertex), reinterpret_cast<void*>(offsetof(StalkerVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(StalkerVertex), reinterpret_cast<void*>(offsetof(StalkerVertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(StalkerVertex), reinterpret_cast<void*>(offsetof(StalkerVertex, color)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(StalkerVertex), reinterpret_cast<void*>(offsetof(StalkerVertex, material)));
        glBindVertexArray(0);
    }
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

    // 2. Volumetric Fog Half-Res 2D Image Texture (screenWidth / 2, screenHeight / 2 in GL_RGBA16F)
    m_fog_width = std::max(1, m_width / 2);
    m_fog_height = std::max(1, m_height / 2);
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

void Renderer::render_sonar_clusters(const std::vector<SurveyedCluster>& clusters, float alpha) {
    if (clusters.empty() || alpha <= 0.001f || m_sonar_vao == 0) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive luminous blending

    m_wireframe_shader.use();
    m_wireframe_shader.set_mat4("uProjection", m_proj);
    m_wireframe_shader.set_mat4("uView", m_view);
    m_wireframe_shader.set_vec3("uVoxelPos", glm::vec3(0.0f));
    m_wireframe_shader.set_int("uIsLineOnly", 1); // Pass world coords directly

    glBindVertexArray(m_sonar_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_sonar_vbo);

    for (const auto& cl : clusters) {
        glm::vec4 base_color(0.0f);
        if (cl.material_id == MAT_VOIDITE || cl.material_id == MAT_VOIDITE_CRYSTAL) {
            base_color = glm::vec4(0.0f, 0.94f, 1.0f, alpha); // Neon cyan
        } else if (cl.material_id == MAT_ASTRAL_ORE) {
            base_color = glm::vec4(1.0f, 0.82f, 0.20f, alpha); // Radiant cosmic gold / starlight
        } else if (cl.material_id == MAT_VAULT_DOOR || cl.material_id == MAT_REINFORCED_VAULT_DOOR) {
            base_color = glm::vec4(1.0f, 0.0f, 0.85f, alpha); // Magenta
        } else {
            continue; // Sonar highlights objectives and monsters only, nothing else
        }

        // 1. Render smooth translucent glowing hull surface
        if (!cl.hull_triangles.empty()) {
            glm::vec4 hull_color = base_color;
            hull_color.a *= 0.20f; // Soft luminous volumetric glow
            m_wireframe_shader.set_vec4("uColor", hull_color);

            const size_t byte_size = cl.hull_triangles.size() * sizeof(glm::vec3);
            glBufferData(GL_ARRAY_BUFFER, byte_size, cl.hull_triangles.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(cl.hull_triangles.size()));
        }

        // 2. Render smooth glowing outline
        if (!cl.outline_lines.empty()) {
            glm::vec4 outline_color = base_color;
            outline_color.a *= 0.92f; // High-contrast glowing outline
            m_wireframe_shader.set_vec4("uColor", outline_color);

            const size_t byte_size = cl.outline_lines.size() * sizeof(glm::vec3);
            glBufferData(GL_ARRAY_BUFFER, byte_size, cl.outline_lines.data(), GL_DYNAMIC_DRAW);
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(cl.outline_lines.size()));
            glLineWidth(1.0f);
        }
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::render_sonar_monsters(const std::vector<SurveyedMonster>& monsters, float alpha) {
    if (monsters.empty() || alpha <= 0.001f || m_sonar_vao == 0) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive luminous blending

    m_wireframe_shader.use();
    m_wireframe_shader.set_mat4("uProjection", m_proj);
    m_wireframe_shader.set_mat4("uView", m_view);
    m_wireframe_shader.set_vec3("uVoxelPos", glm::vec3(0.0f));
    m_wireframe_shader.set_int("uIsLineOnly", 1); // Pass world coords directly

    glBindVertexArray(m_sonar_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_sonar_vbo);

    for (const auto& m : monsters) {
        glm::vec4 base_color = m.is_burrower
            ? glm::vec4(1.0f, 0.45f, 0.10f, alpha)   // Fiery Amber for Burrower
            : glm::vec4(1.0f, 0.18f, 0.15f, alpha);  // Hostile Crimson for Void Stalker

        // 1. Render smooth translucent glowing volumetric cage
        if (!m.hull_triangles.empty()) {
            glm::vec4 hull_color = base_color;
            hull_color.a *= 0.25f; // Soft luminous volumetric glow
            m_wireframe_shader.set_vec4("uColor", hull_color);

            const size_t byte_size = m.hull_triangles.size() * sizeof(glm::vec3);
            glBufferData(GL_ARRAY_BUFFER, byte_size, m.hull_triangles.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m.hull_triangles.size()));
        }

        // 2. Render high-contrast wireframe outline & acoustic sensor ring
        if (!m.outline_lines.empty()) {
            glm::vec4 outline_color = base_color;
            outline_color.a *= 0.95f; // Crisp glowing predator outline
            m_wireframe_shader.set_vec4("uColor", outline_color);

            const size_t byte_size = m.outline_lines.size() * sizeof(glm::vec3);
            glBufferData(GL_ARRAY_BUFFER, byte_size, m.outline_lines.data(), GL_DYNAMIC_DRAW);
            glLineWidth(2.5f);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m.outline_lines.size()));
            glLineWidth(1.0f);
        }
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
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

void Renderer::spawn_dust_cloud(const glm::vec3& pos, int count) {
    if (m_particles.size() >= MAX_PARTICLES) return;
    int allowed = std::min(count, static_cast<int>(MAX_PARTICLES - m_particles.size()));
    for (int i = 0; i < allowed; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 50.0f - 1.0f;
        float ry = static_cast<float>(rand() % 100) / 100.0f;
        float rz = static_cast<float>(rand() % 100) / 50.0f - 1.0f;
        p.pos = pos + glm::vec3(rx * 0.6f, ry * 0.2f, rz * 0.6f);
        p.vel = glm::vec3(rx * 2.0f, ry * 1.5f + 0.5f, rz * 2.0f);
        float shade = 0.60f + static_cast<float>(rand() % 30) * 0.01f;
        p.color = glm::vec4(shade, shade * 0.95f, shade * 0.90f, 0.70f);
        p.size = 0.45f + static_cast<float>(rand() % 100) / 200.0f;
        p.max_life = 1.4f + static_cast<float>(rand() % 100) / 100.0f;
        p.life = p.max_life;
        p.gravity = 0.3f;
        p.drag = 1.1f;
        p.size_growth = 0.30f;
        m_particles.push_back(p);
    }
}

void Renderer::spawn_break_particles(const glm::vec3& block_pos, const glm::ivec3& normal, uint8_t mat_id, VoxelShape shape) {
    glm::vec3 center = block_pos + glm::vec3(0.5f);
    glm::vec3 n_dir = glm::vec3(normal);
    if (glm::length(n_dir) < 0.1f) n_dir = glm::vec3(0.0f, 1.0f, 0.0f);
    else n_dir = glm::normalize(n_dir);

    // Shape-aware center of mass and ejection normal calibration
    if (is_ramp_shape(shape)) {
        float y_off = 0.333f;
        float x_off = 0.5f;
        float z_off = 0.5f;
        if (shape == SHAPE_RAMP_EAST) {
            x_off = 0.667f;
            if (normal.y > 0 || normal.x < 0) n_dir = glm::normalize(glm::vec3(-1.0f, 1.0f, 0.0f));
        } else if (shape == SHAPE_RAMP_WEST) {
            x_off = 0.333f;
            if (normal.y > 0 || normal.x > 0) n_dir = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
        } else if (shape == SHAPE_RAMP_SOUTH) {
            z_off = 0.667f;
            if (normal.y > 0 || normal.z < 0) n_dir = glm::normalize(glm::vec3(0.0f, 1.0f, -1.0f));
        } else if (shape == SHAPE_RAMP_NORTH) {
            z_off = 0.333f;
            if (normal.y > 0 || normal.z > 0) n_dir = glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f));
        }
        center = block_pos + glm::vec3(x_off, y_off, z_off);
    } else if (shape == SHAPE_SLAB_BOTTOM) {
        center = block_pos + glm::vec3(0.5f, 0.25f, 0.5f);
    } else if (shape == SHAPE_SLAB_TOP) {
        center = block_pos + glm::vec3(0.5f, 0.75f, 0.5f);
    }

    bool is_bulkhead = (mat_id == MAT_INDUSTRIAL_BULKHEAD || mat_id == MAT_REINFORCED_VAULT_DOOR);

    // Color palette based on material texture & mineral composition
    glm::vec4 base_color(0.6f, 0.6f, 0.6f, 1.0f);
    if (mat_id == MAT_VOIDITE_CRYSTAL) {
        base_color = glm::vec4(0.85f, 0.30f, 1.0f, 1.0f);
    } else if (mat_id == MAT_INDUSTRIAL_BULKHEAD) {
        base_color = glm::vec4(0.29f, 0.33f, 0.41f, 0.90f); // #4A5568 dark charcoal / industrial gunmetal
    } else if (mat_id == MAT_REINFORCED_VAULT_DOOR) {
        base_color = glm::vec4(1.0f, 0.85f, 0.25f, 1.0f); // Titanium-gold alloy
    } else if (mat_id == MAT_RADIOACTIVE_ORE) {
        base_color = glm::vec4(0.25f, 1.0f, 0.40f, 1.0f);
    } else if (mat_id == MAT_VOLCANIC_BASALT) {
        base_color = glm::vec4(0.28f, 0.28f, 0.32f, 1.0f);
    } else if (mat_id == MAT_FRACTURED_GRANITE) {
        base_color = glm::vec4(0.58f, 0.55f, 0.52f, 1.0f);
    } else if (mat_id == MAT_CRYSTAL_AQUIFER) {
        base_color = glm::vec4(0.18f, 0.85f, 0.98f, 0.75f);
    } else if (mat_id == MAT_BIOLUMINESCENT_FLORA) {
        base_color = glm::vec4(0.25f, 0.95f, 0.45f, 1.0f);
    } else if (mat_id == MAT_PRISMATIC_CRYSTAL) {
        base_color = glm::vec4(0.35f, 0.85f, 0.92f, 0.90f); // Soft aquamarine/celestine mineral shards
    } else if (mat_id == MAT_OBSIDIAN_SPIKES) {
        base_color = glm::vec4(0.42f, 0.20f, 0.60f, 1.0f);
    } else if (mat_id == MAT_THERMITE_SLAG) {
        base_color = glm::vec4(1.0f, 0.42f, 0.08f, 1.0f);
    } else if (mat_id == MAT_DREDGE_BEDROCK) {
        base_color = glm::vec4(0.45f, 0.48f, 0.52f, 1.0f);
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

void Renderer::spawn_clutter_break_particles(const glm::vec3& clutter_pos, ClutterType type, const glm::vec4& tint) {
    // Small break-particle burst: 6-8 particles, 0.05m-0.08m debris shards
    glm::vec4 base_color(0.58f, 0.55f, 0.52f, 1.0f); // Default rock/rubble
    if (type == ClutterType::VoiditeCrystal) {
        base_color = (tint.a > 0.0f) ? tint : glm::vec4(0.85f, 0.25f, 1.0f, 1.0f);
    } else if (type == ClutterType::PrismaticCrystal) {
        base_color = (tint.a > 0.0f) ? tint : glm::vec4(0.30f, 0.95f, 1.0f, 1.0f);
    } else if (type == ClutterType::BioluminescentSporePod) {
        base_color = (tint.a > 0.0f) ? tint : glm::vec4(0.20f, 0.95f, 0.70f, 1.0f);
    } else if (type == ClutterType::WallIndustrialLamp) {
        base_color = (tint.a > 0.0f) ? tint : glm::vec4(1.0f, 0.85f, 0.35f, 1.0f);
    } else if (type == ClutterType::CeilingStalactite || type == ClutterType::FloorStalagmiteRubble) {
        base_color = glm::vec4(0.52f, 0.50f, 0.48f, 1.0f);
    }

    int count = 6 + (rand() % 3); // 6 to 8 particles
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float ry = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        glm::vec3 jitter(rx, ry, rz);
        if (glm::length(jitter) > 0.001f) jitter = glm::normalize(jitter);

        p.pos = clutter_pos + jitter * 0.08f;
        p.vel = jitter * (1.8f + static_cast<float>(rand() % 100) / 50.0f) + glm::vec3(0.0f, 0.8f, 0.0f);
        p.color = base_color;
        p.size = 0.05f + static_cast<float>(rand() % 100) / 2500.0f; // 0.05m - 0.09m
        p.gravity = 18.0f;
        p.drag = 1.8f;
        p.max_life = 0.35f + static_cast<float>(rand() % 100) / 800.0f; // 0.35s - 0.47s
        p.life = p.max_life;
        m_particles.push_back(p);
    }
}

void ParticleSystem::SetRenderer(Renderer* renderer) {
    if (renderer) {
        SetCallbacks(
            [renderer](const glm::vec3& pos, uint8_t mat) {
                renderer->spawn_break_particles(pos, glm::ivec3(0, 1, 0), mat);
            },
            [renderer](const glm::vec3& pos, ClutterType type, const glm::vec4& tint) {
                renderer->spawn_clutter_break_particles(pos, type, tint);
            }
        );
    } else {
        Reset();
    }
}

void Renderer::spawn_crack_debris(const glm::vec3& block_pos, const glm::ivec3& normal, float intensity, uint8_t mat_id, VoxelShape shape) {
    glm::vec3 n_dir = glm::vec3(normal);
    if (glm::length(n_dir) < 0.1f) n_dir = glm::vec3(0.0f, 1.0f, 0.0f);
    else n_dir = glm::normalize(n_dir);

    glm::vec3 center = block_pos;
    if (glm::distance(block_pos, glm::floor(block_pos)) < 0.001f) {
        center = block_pos + glm::vec3(0.5f);
    }

    if (is_ramp_shape(shape)) {
        if (shape == SHAPE_RAMP_EAST) {
            if (normal.y > 0 || normal.x < 0) n_dir = glm::normalize(glm::vec3(-1.0f, 1.0f, 0.0f));
        } else if (shape == SHAPE_RAMP_WEST) {
            if (normal.y > 0 || normal.x > 0) n_dir = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
        } else if (shape == SHAPE_RAMP_SOUTH) {
            if (normal.y > 0 || normal.z < 0) n_dir = glm::normalize(glm::vec3(0.0f, 1.0f, -1.0f));
        } else if (shape == SHAPE_RAMP_NORTH) {
            if (normal.y > 0 || normal.z > 0) n_dir = glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f));
        }
    }

    // Material-authentic debris texture color
    glm::vec4 base_color(0.6f, 0.6f, 0.6f, 1.0f);
    if (mat_id == MAT_VOIDITE_CRYSTAL) {
        base_color = glm::vec4(0.85f, 0.30f, 1.0f, 1.0f);
    } else if (mat_id == MAT_INDUSTRIAL_BULKHEAD) {
        base_color = glm::vec4(0.29f, 0.33f, 0.41f, 0.90f); // #4A5568 dark charcoal / industrial gunmetal
    } else if (mat_id == MAT_REINFORCED_VAULT_DOOR) {
        base_color = glm::vec4(1.0f, 0.85f, 0.25f, 1.0f);
    } else if (mat_id == MAT_RADIOACTIVE_ORE) {
        base_color = glm::vec4(0.25f, 1.0f, 0.40f, 1.0f);
    } else if (mat_id == MAT_VOLCANIC_BASALT) {
        base_color = glm::vec4(0.28f, 0.28f, 0.32f, 1.0f);
    } else if (mat_id == MAT_FRACTURED_GRANITE) {
        base_color = glm::vec4(0.68f, 0.64f, 0.58f, 1.0f);
    } else if (mat_id == MAT_CRYSTAL_AQUIFER) {
        base_color = glm::vec4(0.18f, 0.85f, 0.98f, 0.75f);
    } else if (mat_id == MAT_BIOLUMINESCENT_FLORA) {
        base_color = glm::vec4(0.25f, 0.95f, 0.45f, 1.0f);
    } else if (mat_id == MAT_PRISMATIC_CRYSTAL) {
        base_color = glm::vec4(0.35f, 0.85f, 0.92f, 0.90f); // Soft aquamarine/celestine mineral shards
    } else if (mat_id == MAT_OBSIDIAN_SPIKES) {
        base_color = glm::vec4(0.42f, 0.20f, 0.60f, 1.0f);
    } else if (mat_id == MAT_THERMITE_SLAG) {
        base_color = glm::vec4(1.0f, 0.42f, 0.08f, 1.0f);
    } else if (mat_id == MAT_DREDGE_BEDROCK) {
        base_color = glm::vec4(0.45f, 0.48f, 0.52f, 1.0f);
    }

    int count = std::max(1, static_cast<int>(intensity * 3.0f));
    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float ry = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        glm::vec3 jitter(rx, ry, rz);

        if (mat_id == MAT_INDUSTRIAL_BULKHEAD && (i % 2 == 0)) {
            p.color = glm::vec4(1.0f, 0.70f, 0.0f, 1.0f); // #FFB300 subtle spark point
            p.size = 0.035f + static_cast<float>(rand() % 100) / 2500.0f;
        } else if (mat_id == MAT_VOLCANIC_BASALT && (i % 2 == 0)) {
            p.color = glm::vec4(0.95f, 0.48f, 0.12f, 1.0f); // Volcanic amber ember
            p.size = 0.045f + static_cast<float>(rand() % 100) / 2500.0f;
        } else {
            p.color = base_color;
            p.size = 0.05f + static_cast<float>(rand() % 100) / 2000.0f;
        }

        p.pos = center + n_dir * 0.15f + jitter * 0.2f;
        p.vel = n_dir * (1.5f + static_cast<float>(rand() % 100) / 50.0f) + jitter * 2.5f;
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

void Renderer::spawn_monster_emergence_particles(const glm::vec3& pos, const glm::vec3& normal, StalkerRole role, float progress, int count) {
    glm::vec3 n = normal;
    if (glm::length(n) < 0.01f) n = glm::vec3(0.0f, 1.0f, 0.0f);
    else n = glm::normalize(n);

    // Tangent basis for directional dust/particle kickout
    glm::vec3 up = (std::abs(n.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(n, up));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));

    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float rx = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float rz = static_cast<float>(rand() % 100) / 100.0f - 0.5f;
        float speed = 1.8f + static_cast<float>(rand() % 100) / 40.0f;

        p.pos = pos + tangent * (rx * 0.4f) + bitangent * (rz * 0.4f) + n * 0.05f;
        p.vel = n * speed + (tangent * rx + bitangent * rz) * 2.5f;

        if (role == StalkerRole::Melee) {
            // Melee Predator (Crimson): Fractured granite rock shards + fierce blood-red embers
            if (i % 2 == 0) {
                // Blood crimson void ember
                float shade = 0.8f + static_cast<float>(rand() % 20) * 0.01f;
                p.color = glm::vec4(0.95f * shade, 0.08f, 0.12f, 1.0f);
                p.size = 0.06f + static_cast<float>(rand() % 100) / 2000.0f;
                p.gravity = 10.0f;
                p.drag = 0.3f;
                p.size_growth = -0.02f;
                p.max_life = 0.50f + static_cast<float>(rand() % 100) / 300.0f;
            } else {
                // Fractured granite rock chip
                float shade = 0.65f + static_cast<float>(rand() % 30) * 0.01f;
                p.color = glm::vec4(0.52f * shade, 0.48f * shade, 0.44f * shade, 1.0f);
                p.size = 0.08f + static_cast<float>(rand() % 100) / 1200.0f;
                p.gravity = 14.0f;
                p.drag = 0.4f;
                p.size_growth = -0.03f;
                p.max_life = 0.45f + static_cast<float>(rand() % 100) / 350.0f;
            }
        } else if (role == StalkerRole::Shooter) {
            // Shooter (Toxic Jade): Toxic emerald crystal flakes + billowing green gas puffs
            if (i % 2 == 0) {
                // Rising toxic gas puff
                float shade = static_cast<float>(rand() % 100) / 400.0f;
                p.color = glm::vec4(0.20f + shade, 0.95f, 0.28f, 0.65f);
                p.size = 0.18f + static_cast<float>(rand() % 100) / 800.0f;
                p.vel = n * 0.8f + (tangent * rx + bitangent * rz) * 1.2f;
                p.gravity = -1.2f; // Buoyant rising smoke plume
                p.drag = 0.7f;
                p.size_growth = 0.15f;
                p.max_life = 1.0f + static_cast<float>(rand() % 100) / 150.0f;
            } else {
                // Toxic jade crystal chips
                p.color = glm::vec4(0.12f, 0.92f, 0.32f, 0.95f);
                p.size = 0.05f + static_cast<float>(rand() % 100) / 2500.0f;
                p.gravity = 12.0f;
                p.drag = 0.35f;
                p.size_growth = -0.02f;
                p.max_life = 0.55f + static_cast<float>(rand() % 100) / 250.0f;
            }
        } else {
            // Chitin Goliath (Heavy Carapace): Massive obsidian rubble boulders + dense dust shockwave
            if (i % 3 == 0) {
                // Dense grey dust puff
                p.color = glm::vec4(0.35f, 0.33f, 0.32f, 0.55f);
                p.size = 0.30f + static_cast<float>(rand() % 100) / 400.0f;
                p.vel = n * 1.2f + (tangent * rx + bitangent * rz) * 2.2f;
                p.gravity = 2.0f;
                p.drag = 0.6f;
                p.size_growth = 0.20f;
                p.max_life = 0.85f + static_cast<float>(rand() % 100) / 200.0f;
            } else {
                // Heavy dark obsidian boulder fragment
                float shade = 0.8f + static_cast<float>(rand() % 20) * 0.01f;
                p.color = glm::vec4(0.14f * shade, 0.13f * shade, 0.12f * shade, 1.0f);
                p.size = 0.15f + static_cast<float>(rand() % 100) / 800.0f;
                p.gravity = 20.0f; // Heavy downward drop
                p.drag = 0.25f;
                p.size_growth = -0.04f;
                p.max_life = 0.60f + static_cast<float>(rand() % 100) / 250.0f;
            }
        }
        p.life = p.max_life;
        m_particles.push_back(p);
    }
}

void Renderer::spawn_burrower_emergence_particles(const glm::vec3& pos, const glm::vec3& forward_dir, float progress, int count) {
    glm::vec3 fwd = forward_dir;
    if (glm::length(fwd) < 0.01f) fwd = glm::vec3(0.0f, 0.0f, 1.0f);
    else fwd = glm::normalize(fwd);

    glm::vec3 up = (std::abs(fwd.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(fwd, up));
    up = glm::normalize(glm::cross(right, fwd));

    for (int i = 0; i < count; ++i) {
        BreakParticle p;
        float angle = static_cast<float>(rand() % 360) * 0.0174533f;
        float rad = 0.45f + static_cast<float>(rand() % 100) / 250.0f;
        glm::vec3 radial_dir = right * std::cos(angle) + up * std::sin(angle);
        glm::vec3 tangent_dir = -right * std::sin(angle) + up * std::cos(angle);

        p.pos = pos + fwd * 0.35f + radial_dir * rad;

        if (i % 2 == 0) {
            // Molten magma slag spark spinning radially from rotating cutter drill
            p.vel = -fwd * (2.5f + static_cast<float>(rand() % 100) / 35.0f)
                    + radial_dir * (1.8f + static_cast<float>(rand() % 100) / 50.0f)
                    + tangent_dir * 3.5f;
            p.color = glm::vec4(1.0f, 0.50f + static_cast<float>(rand() % 40) * 0.01f, 0.05f, 1.0f);
            p.size = 0.07f + static_cast<float>(rand() % 100) / 1500.0f;
            p.gravity = 14.0f;
            p.drag = 0.3f;
            p.size_growth = -0.03f;
            p.max_life = 0.55f + static_cast<float>(rand() % 100) / 250.0f;
        } else {
            // Crushed volcanic basalt chips & titanium cutter friction sparks
            p.vel = -fwd * (3.0f + static_cast<float>(rand() % 100) / 30.0f)
                    + radial_dir * (1.2f + static_cast<float>(rand() % 100) / 60.0f);
            float shade = 0.65f + static_cast<float>(rand() % 35) * 0.01f;
            p.color = (i % 4 == 1)
                ? glm::vec4(0.80f * shade, 0.78f * shade, 0.75f * shade, 1.0f)
                : glm::vec4(0.25f * shade, 0.24f * shade, 0.26f * shade, 1.0f);
            p.size = 0.09f + static_cast<float>(rand() % 100) / 1000.0f;
            p.gravity = 16.0f;
            p.drag = 0.4f;
            p.size_growth = -0.04f;
            p.max_life = 0.48f + static_cast<float>(rand() % 100) / 300.0f;
        }
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

        // Luminous buoyant radiation mist wisp: clearly visible from distance to telegraph hazard zone
        BreakParticle mist;
        mist.pos = block_pos + glm::vec3(0.5f) + glm::vec3(rx * 0.8f, ry * 0.3f, rz * 0.8f);
        mist.vel = glm::vec3(rx * 0.4f, 0.35f + static_cast<float>(rand() % 100) / 350.0f, rz * 0.4f);
        mist.color = glm::vec4(0.12f, 0.96f, 0.32f, 0.65f); // Phosphorescent emerald danger mist
        mist.size = 0.45f + static_cast<float>(rand() % 100) / 400.0f; // 0.45m - 0.70m visible plume
        mist.max_life = 1.4f + static_cast<float>(rand() % 100) / 200.0f;
        mist.life = mist.max_life;
        mist.gravity = -0.15f; // Gently floats upwards
        mist.drag = 0.90f;
        mist.size_growth = 0.22f; // Expands outward to indicate danger aura
        m_particles.push_back(mist);
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







void Renderer::render_block_cracks(const glm::ivec3& voxel_pos, float progress, const glm::ivec3& face_norm, uint8_t mat_id, VoxelShape shape) {
    if (progress <= 0.05f || m_cable_vao == 0) return;

    std::vector<float> lines;
    glm::vec4 crack_color;
    generate_block_crack_geometry(voxel_pos, progress, face_norm, mat_id, shape, lines, crack_color);

    if (lines.empty()) return;

    // Escalating particle debris as progress exceeds 75%
    if (progress > 0.75f) {
        float prob = (progress - 0.75f) * 2.0f;
        if ((static_cast<float>(rand() % 100) / 100.0f) < prob) {
            glm::vec3 spawn_origin = glm::vec3(lines[0], lines[1], lines[2]);
            spawn_crack_debris(spawn_origin, face_norm, (progress - 0.75f) / 0.25f, mat_id, shape);
        }
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

    ensure_stalker_vbo_capacity(verts.size());
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

    ensure_stalker_vbo_capacity(verts.size());
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(StalkerVertex), verts.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size()));
    glBindVertexArray(0);
}

void Renderer::render_breadcrumbs(const std::vector<glm::vec3>& crumbs, float time) {
    (void)crumbs;
    (void)time;
    // In-world 3D breadcrumb boxes removed: navigation path tracking is rendered exclusively
    // on the 2D Holographic Terrain Scanner ([TAB] overlay) to prevent floating glowing
    // slabs/droplets from littering first-person gameplay when jumping, jetpacking, or grappling.
}

void Renderer::spawn_fireball(const glm::vec3& center, int count) {
    if (m_particles.size() >= MAX_PARTICLES) return;
    int allowed = std::min(count, static_cast<int>(MAX_PARTICLES - m_particles.size()));
    for (int i = 0; i < allowed; ++i) {
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
    update_ambient_spores(dt, m_cam_pos);

    std::erase_if(m_particles, [dt](BreakParticle& p) {
        p.pos += p.vel * dt;
        p.vel.y -= p.gravity * dt;
        p.vel *= std::max(0.0f, 1.0f - p.drag * dt);
        p.size = std::max(0.01f, p.size + p.size_growth * dt);
        p.life -= dt;
        return p.life <= 0.0f;
    });
}

void Renderer::update_ambient_spores(float dt, const glm::vec3& cam_pos) {
    constexpr size_t TARGET_SPORES = 14;
    size_t active_spores = 0;
    for (const auto& p : m_particles) {
        if (p.max_life > 3.0f) {
            active_spores++;
        }
    }

    float t = m_total_time;

    // Organic micro-turbulence for active airborne spores
    for (auto& p : m_particles) {
        if (p.max_life > 3.0f) {
            float seed = p.pos.x * 0.35f + p.pos.z * 0.28f;
            p.vel.x += std::sin(t * 1.2f + seed) * 0.30f * dt;
            p.vel.z += std::cos(t * 1.1f + seed * 1.3f) * 0.30f * dt;
            p.vel.y += std::sin(t * 0.8f + p.pos.y) * 0.18f * dt;
        }
    }

    if (active_spores >= TARGET_SPORES || m_particles.size() >= MAX_PARTICLES - 8) {
        return;
    }

    size_t to_spawn = std::min(size_t(4), TARGET_SPORES - active_spores);
    for (size_t i = 0; i < to_spawn; ++i) {
        BreakParticle p;
        // Distribute within a 2.5m to 16.0m hollow sphere around player/camera
        float theta = static_cast<float>(rand() % 360) * glm::pi<float>() / 180.0f;
        float phi = static_cast<float>(rand() % 180 - 90) * glm::pi<float>() / 180.0f;
        float dist = 2.5f + static_cast<float>(rand() % 100) / 100.0f * 13.5f;

        glm::vec3 offset(
            dist * std::cos(phi) * std::cos(theta),
            dist * std::sin(phi) * 0.65f, // Flatter vertical distribution for cavern feel
            dist * std::cos(phi) * std::sin(theta)
        );
        p.pos = cam_pos + offset;

        // Slow buoyant air drift
        p.vel = glm::vec3(
            (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 0.22f,
            0.12f + static_cast<float>(rand() % 100) / 100.0f * 0.18f, // Gentle thermal upward loft
            (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 0.22f
        );

        // Biome-appropriate spore color
        if (m_sector == 2) {
            // Sector 2: Floating incandescent embers / thermoluminescent volcanic motes
            p.color = glm::vec4(1.0f, 0.58f + static_cast<float>(rand() % 30) / 100.0f, 0.12f, 0.85f);
        } else if (m_sector >= 3) {
            // Sector 3+: Amethyst void spores and phosphorescent viridian motes
            if ((rand() % 2) == 0) {
                p.color = glm::vec4(0.75f, 0.25f + static_cast<float>(rand() % 25) / 100.0f, 0.98f, 0.85f);
            } else {
                p.color = glm::vec4(0.22f, 0.96f, 0.55f, 0.80f);
            }
        } else {
            // Sector 1: Luminous phosphorescent teal / mint bio-spores & faint golden pollen
            if ((rand() % 3) == 0) {
                p.color = glm::vec4(0.95f, 0.84f, 0.28f, 0.75f); // Golden spore
            } else {
                p.color = glm::vec4(0.18f, 0.95f, 0.72f, 0.85f); // Luminous teal
            }
        }

        p.size = 0.12f + static_cast<float>(rand() % 100) / 1000.0f; // 0.12m - 0.22m
        p.life = 6.0f + static_cast<float>(rand() % 100) / 20.0f;    // 6.0s - 11.0s
        p.max_life = p.life;
        p.gravity = -0.05f; // Slight buoyant upward float
        p.drag = 0.40f;
        p.size_growth = 0.01f;
        p.emissive = 2.5f;

        m_particles.push_back(p);
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

    m_particle_vertex_buffer.clear();
    size_t count = std::min(m_particles.size(), MAX_PARTICLES);
    m_particle_vertex_buffer.reserve(count * 54);

    for (size_t i = 0; i < count; ++i) {
        const auto& p = m_particles[i];
        float alpha = 1.0f;
        if (p.max_life > 3.0f) {
            // Spore: smooth organic fade-in / fade-out with gentle breathing pulse
            float life_ratio = p.life / p.max_life;
            float fade = std::min(1.0f - life_ratio, life_ratio) * 6.0f;
            float pulse = 0.85f + 0.15f * std::sin(m_total_time * 2.5f + p.pos.x * 2.0f);
            alpha = std::clamp(fade, 0.0f, 1.0f) * pulse;
        } else {
            alpha = std::clamp(p.life / p.max_life, 0.0f, 1.0f);
        }
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
        m_particle_vertex_buffer.insert(m_particle_vertex_buffer.end(), quad_v, quad_v + 54);
    }

    if (!m_particle_vertex_buffer.empty()) {
        glBindVertexArray(m_particle_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_particle_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(m_particle_vertex_buffer.size() * sizeof(float)), m_particle_vertex_buffer.data());
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_particle_vertex_buffer.size() / 9));
        glBindVertexArray(0);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::add_point_light(const PointLight& light) {
    if (m_point_lights.size() < MAX_SCENE_LIGHTS) {
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
    m_visibleChunks.clear();

    // Smooth ocular dark adaptation
    m_area_darkness = glm::mix(m_area_darkness, m_target_area_darkness, 0.08f);

    // Bind HDR FBO and clear
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdr_fbo);
    glViewport(0, 0, m_width, m_height);
    float dark_mult = std::clamp(1.0f - m_area_darkness, 0.0f, 1.0f);
    glClearColor(0.015f * dark_mult, 0.018f * dark_mult, 0.024f * dark_mult, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Clustered Forward Lighting Compute Pass & Buffer Binding
    bool clustered_active = false;
    if (m_clustered_lighting && m_clustered_lighting->is_initialized()) {
        m_clustered_lighting->update_and_cull(m_view, m_proj, m_point_lights, 0.1f, 250.0f, m_width, m_height);
        m_clustered_lighting->bind_buffers(0, 1);
        clustered_active = true;
    }

    // Setup Voxel Shader
    m_voxel_shader.use();
    m_voxel_shader.set_mat4("uView", m_view);
    m_voxel_shader.set_mat4("uProjection", m_proj);
    m_voxel_shader.set_vec3("uCameraPos", m_cam_pos);
    m_voxel_shader.set_float("uTime", m_total_time);
    m_voxel_shader.set_int("uSector", m_sector);
    m_voxel_shader.set_float("uDarknessFactor", m_area_darkness);
    m_voxel_shader.set_vec2("uScreenSize", glm::vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_voxel_shader.set_float("uNear", 0.1f);
    m_voxel_shader.set_float("uFar", 250.0f);
    m_voxel_shader.set_int("uClusteredLightingEnabled", clustered_active ? 1 : 0);

    // Headlamp
    m_voxel_shader.set_vec3("uHeadlampPos", m_headlamp.position);
    m_voxel_shader.set_vec3("uHeadlampDir", m_headlamp.direction);
    m_voxel_shader.set_vec3("uHeadlampColor", m_headlamp.color);
    m_voxel_shader.set_float("uHeadlampInnerCutoff", m_headlamp.inner_cutoff);
    m_voxel_shader.set_float("uHeadlampOuterCutoff", m_headlamp.outer_cutoff);
    m_voxel_shader.set_float("uHeadlampIntensity", get_effective_headlamp_intensity());
    m_voxel_shader.set_float("uHeadlampRange", m_headlamp.max_range);

    // Point lights (fallback uniform array & fog compatibility)
    m_voxel_shader.set_int("uNumPointLights", static_cast<int>(std::min(m_point_lights.size(), size_t(16))));
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

    // Camera frustum extraction (Gribb-Hartmann method from Projection * View)
    glm::mat4 vp = m_proj * m_view;
    glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
    glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
    glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
    glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

    m_frustum_planes[0] = row3 + row0; // Left
    m_frustum_planes[1] = row3 - row0; // Right
    m_frustum_planes[2] = row3 + row1; // Bottom
    m_frustum_planes[3] = row3 - row1; // Top
    m_frustum_planes[4] = row3 + row2; // Near
    m_frustum_planes[5] = row3 - row2; // Far

    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(m_frustum_planes[i]));
        if (len > 0.00001f) {
            m_frustum_planes[i] /= len;
        }
    }
}

bool Renderer::is_box_in_frustum(const glm::vec3& min_pt, const glm::vec3& max_pt) const {
    for (int i = 0; i < 6; ++i) {
        glm::vec3 p(
            (m_frustum_planes[i].x > 0.0f) ? max_pt.x : min_pt.x,
            (m_frustum_planes[i].y > 0.0f) ? max_pt.y : min_pt.y,
            (m_frustum_planes[i].z > 0.0f) ? max_pt.z : min_pt.z
        );
        if (glm::dot(glm::vec3(m_frustum_planes[i]), p) + m_frustum_planes[i].w < 0.0f) {
            return false;
        }
    }
    return true;
}

void Renderer::begin_solid_pass() {
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void Renderer::render_chunk(const Chunk& chunk) {
    if (chunk.solid_vertex_count() == 0 && chunk.liquid_vertex_count() == 0) return;

    glm::vec3 min_pt = chunk.get_world_pos();
    glm::vec3 max_pt = min_pt + glm::vec3(static_cast<float>(CHUNK_SIZE));
    if (!is_box_in_frustum(min_pt, max_pt)) return;

    m_visibleChunks.push_back(const_cast<Chunk*>(&chunk));

    if (chunk.solid_vertex_count() > 0) {
        glm::mat4 model = glm::mat4(1.0f);
        m_voxel_shader.set_mat4("uModel", model);
        m_voxel_shader.set_vec3("uChunkWorldPos", min_pt);
        chunk.render_solid();
    }
}

void Renderer::render_chunk_liquid(const Chunk& chunk) {
    if (chunk.liquid_vertex_count() == 0) return;

    glm::vec3 min_pt = chunk.get_world_pos();
    glm::vec3 max_pt = min_pt + glm::vec3(static_cast<float>(CHUNK_SIZE));
    if (!is_box_in_frustum(min_pt, max_pt)) return;

    glm::mat4 model = glm::mat4(1.0f);
    m_voxel_shader.set_mat4("uModel", model);
    m_voxel_shader.set_vec3("uChunkWorldPos", min_pt);
    chunk.DrawLiquidMesh();
}

void Renderer::begin_liquid_pass() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Read depth, do not occlude geometry behind water
    glDisable(GL_CULL_FACE); // Temporarily disable face culling or ensure CCW winding

    if (m_liquidShader) {
        m_liquidShader->Bind();
        m_liquidShader->SetMat4("u_ViewProjection", m_proj * m_view);
        m_liquidShader->SetMat4("uViewProj", m_proj * m_view);
        m_liquidShader->SetFloat("u_Time", m_total_time);
        m_liquidShader->SetFloat("uTime", m_total_time);
    }
}

void Renderer::end_liquid_pass() {
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::RenderLiquidPass() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Read depth, do not occlude geometry behind water
    glDisable(GL_CULL_FACE); // Temporarily disable face culling or ensure CCW winding

    if (m_liquidShader) {
        m_liquidShader->Bind();
        m_liquidShader->SetMat4("u_ViewProjection", m_proj * m_view);
        m_liquidShader->SetMat4("uViewProj", m_proj * m_view);
        m_liquidShader->SetFloat("u_Time", m_total_time);
        m_liquidShader->SetFloat("uTime", m_total_time);
    }

    for (Chunk* chunk : m_visibleChunks) {
        if (chunk && chunk->HasLiquidMesh()) {
            glm::vec3 min_pt = chunk->get_world_pos();
            if (m_liquidShader) {
                m_liquidShader->SetMat4("uModel", glm::mat4(1.0f));
                m_liquidShader->SetVec3("uChunkWorldPos", min_pt);
            }
            chunk->DrawLiquidMesh();
        }
    }

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::RenderScene() {
    begin_solid_pass();
    for (Chunk* chunk : m_visibleChunks) {
        if (chunk && chunk->has_solid_mesh()) {
            render_chunk(*chunk);
        }
    }
    RenderLiquidPass();
}

void Renderer::render_chunk_clutter(const Chunk& chunk, const glm::vec3& player_pos) {
    if (chunk.clutter_instances().empty()) return;

    glm::vec3 min_pt = chunk.get_world_pos();
    glm::vec3 max_pt = min_pt + glm::vec3(static_cast<float>(CHUNK_SIZE));
    if (!is_box_in_frustum(min_pt, max_pt)) return;

    if (!m_clutter_system.is_gpu_initialized()) {
        m_clutter_system.init_gpu();
    }

    m_clutter_system.render_chunk_clutter(
        chunk.clutter_instances(),
        player_pos,
        m_view,
        m_proj,
        m_cam_pos,
        m_headlamp.position,
        m_headlamp.direction,
        m_headlamp.color,
        m_headlamp.enabled,
        m_total_time,
        m_area_darkness
    );

    // Restore voxel shader state for subsequent chunk rendering
    m_voxel_shader.use();
}

void Renderer::render_debris(const DynamicDebris& debris) {
    if (debris.is_destroyed() || debris.vertex_count() == 0 || debris.vao() == 0) return;

    glm::vec3 dpos = debris.position();
    glm::vec3 dmin = dpos - glm::vec3(2.0f);
    glm::vec3 dmax = dpos + glm::vec3(2.0f);
    if (!is_box_in_frustum(dmin, dmax)) return;

    m_voxel_shader.use();

    // Model matrix: translate to world position, apply tumbling rotation, offset by -0.5f to center 1x1x1 cube
    glm::mat4 model = glm::translate(glm::mat4(1.0f), debris.position());
    const glm::vec3& rot = debris.rotation();
    model = glm::rotate(model, rot.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, rot.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, rot.z, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::translate(model, glm::vec3(-0.5f, -0.5f, -0.5f) + debris.mesh_offset());

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

static void add_stalker_cylinder(
    std::vector<StalkerVertex>& verts,
    const glm::mat4& transform,
    const glm::vec3& base,
    float radius,
    float length,
    int segments,
    const glm::vec4& color,
    const glm::vec4& material,
    int axis = 2,
    bool cap_ends = true
) {
    glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(transform)));
    float step = 6.2831853f / static_cast<float>(segments);
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
        } else if (axis == 1) {
            p1 = base + glm::vec3(radius * c1, 0.0f, radius * s1);
            p2 = base + glm::vec3(radius * c2, 0.0f, radius * s2);
            p3 = base + glm::vec3(radius * c2, length, radius * s2);
            p4 = base + glm::vec3(radius * c1, length, radius * s1);
            n1 = glm::vec3(c1, 0.0f, s1);
            n2 = glm::vec3(c2, 0.0f, s2);
        } else {
            p1 = base + glm::vec3(0.0f, radius * c1, radius * s1);
            p2 = base + glm::vec3(0.0f, radius * c2, radius * s2);
            p3 = base + glm::vec3(length, radius * c2, radius * s2);
            p4 = base + glm::vec3(length, radius * c1, radius * s1);
            n1 = glm::vec3(0.0f, c1, s1);
            n2 = glm::vec3(0.0f, c2, s2);
        }

        glm::vec3 tp1 = glm::vec3(transform * glm::vec4(p1, 1.0f));
        glm::vec3 tp2 = glm::vec3(transform * glm::vec4(p2, 1.0f));
        glm::vec3 tp3 = glm::vec3(transform * glm::vec4(p3, 1.0f));
        glm::vec3 tp4 = glm::vec3(transform * glm::vec4(p4, 1.0f));
        glm::vec3 tn1 = glm::normalize(normal_mat * n1);
        glm::vec3 tn2 = glm::normalize(normal_mat * n2);

        verts.push_back({tp1, tn1, color, material});
        verts.push_back({tp2, tn2, color, material});
        verts.push_back({tp3, tn2, color, material});
        verts.push_back({tp1, tn1, color, material});
        verts.push_back({tp3, tn2, color, material});
        verts.push_back({tp4, tn1, color, material});

        if (cap_ends) {
            glm::vec3 cap_n_start = (axis == 2) ? glm::vec3(0, 0, 1) : (axis == 1 ? glm::vec3(0, -1, 0) : glm::vec3(-1, 0, 0));
            glm::vec3 cap_n_end = -cap_n_start;
            glm::vec3 tcap_n_start = glm::normalize(normal_mat * cap_n_start);
            glm::vec3 tcap_n_end = glm::normalize(normal_mat * cap_n_end);

            glm::vec3 base_pt = glm::vec3(transform * glm::vec4(base, 1.0f));
            glm::vec3 end_offset = (axis == 2) ? glm::vec3(0, 0, -length) : (axis == 1 ? glm::vec3(0, length, 0) : glm::vec3(length, 0, 0));
            glm::vec3 end_pt = glm::vec3(transform * glm::vec4(base + end_offset, 1.0f));

            verts.push_back({base_pt, tcap_n_start, color, material});
            verts.push_back({tp2, tcap_n_start, color, material});
            verts.push_back({tp1, tcap_n_start, color, material});

            verts.push_back({end_pt, tcap_n_end, color, material});
            verts.push_back({tp4, tcap_n_end, color, material});
            verts.push_back({tp3, tcap_n_end, color, material});
        }
    }
}

static void add_stalker_capsule(
    std::vector<StalkerVertex>& verts,
    const glm::mat4& transform,
    const glm::vec3& p1,
    const glm::vec3& p2,
    float r1,
    float r2,
    int segments,
    const glm::vec4& color,
    const glm::vec4& material
) {
    glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(transform)));
    glm::vec3 axis = p2 - p1;
    float len = glm::length(axis);
    if (len < 0.0001f) return;
    glm::vec3 dir = axis / len;

    glm::vec3 up_hint = (std::abs(dir.y) > 0.95f) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 side = glm::normalize(glm::cross(dir, up_hint));
    glm::vec3 up = glm::cross(side, dir);

    float step = 6.2831853f / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float c2 = std::cos(a2), s2 = std::sin(a2);

        glm::vec3 rad_dir1 = side * c1 + up * s1;
        glm::vec3 rad_dir2 = side * c2 + up * s2;

        glm::vec3 v1 = p1 + rad_dir1 * r1;
        glm::vec3 v2 = p1 + rad_dir2 * r1;
        glm::vec3 v3 = p2 + rad_dir2 * r2;
        glm::vec3 v4 = p2 + rad_dir1 * r2;

        glm::vec3 tv1 = glm::vec3(transform * glm::vec4(v1, 1.0f));
        glm::vec3 tv2 = glm::vec3(transform * glm::vec4(v2, 1.0f));
        glm::vec3 tv3 = glm::vec3(transform * glm::vec4(v3, 1.0f));
        glm::vec3 tv4 = glm::vec3(transform * glm::vec4(v4, 1.0f));
        glm::vec3 tn1 = glm::normalize(normal_mat * rad_dir1);
        glm::vec3 tn2 = glm::normalize(normal_mat * rad_dir2);

        verts.push_back({tv1, tn1, color, material});
        verts.push_back({tv2, tn2, color, material});
        verts.push_back({tv3, tn2, color, material});
        verts.push_back({tv1, tn1, color, material});
        verts.push_back({tv3, tn2, color, material});
        verts.push_back({tv4, tn1, color, material});

        glm::vec3 tdir = glm::normalize(normal_mat * dir);
        glm::vec3 tp1 = glm::vec3(transform * glm::vec4(p1, 1.0f));
        glm::vec3 tp2 = glm::vec3(transform * glm::vec4(p2, 1.0f));

        verts.push_back({tp1, -tdir, color, material});
        verts.push_back({tv2, -tdir, color, material});
        verts.push_back({tv1, -tdir, color, material});

        verts.push_back({tp2, tdir, color, material});
        verts.push_back({tv4, tdir, color, material});
        verts.push_back({tv3, tdir, color, material});
    }
}

static void add_stalker_cone(
    std::vector<StalkerVertex>& verts,
    const glm::mat4& transform,
    const glm::vec3& base,
    float r_base,
    float r_tip,
    float length,
    int segments,
    const glm::vec4& color,
    const glm::vec4& material
) {
    glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(transform)));
    float step = 6.2831853f / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float c2 = std::cos(a2), s2 = std::sin(a2);

        glm::vec3 p1 = base + glm::vec3(r_base * c1, r_base * s1, 0.0f);
        glm::vec3 p2 = base + glm::vec3(r_base * c2, r_base * s2, 0.0f);
        glm::vec3 p3 = base + glm::vec3(r_tip * c2, r_tip * s2, -length);
        glm::vec3 p4 = base + glm::vec3(r_tip * c1, r_tip * s1, -length);

        float dr = r_base - r_tip;
        glm::vec3 n1 = glm::normalize(glm::vec3(c1, s1, dr / std::max(length, 0.001f)));
        glm::vec3 n2 = glm::normalize(glm::vec3(c2, s2, dr / std::max(length, 0.001f)));

        glm::vec3 tp1 = glm::vec3(transform * glm::vec4(p1, 1.0f));
        glm::vec3 tp2 = glm::vec3(transform * glm::vec4(p2, 1.0f));
        glm::vec3 tp3 = glm::vec3(transform * glm::vec4(p3, 1.0f));
        glm::vec3 tp4 = glm::vec3(transform * glm::vec4(p4, 1.0f));
        glm::vec3 tn1 = glm::normalize(normal_mat * n1);
        glm::vec3 tn2 = glm::normalize(normal_mat * n2);

        verts.push_back({tp1, tn1, color, material});
        verts.push_back({tp2, tn2, color, material});
        verts.push_back({tp3, tn2, color, material});
        verts.push_back({tp1, tn1, color, material});
        verts.push_back({tp3, tn2, color, material});
        verts.push_back({tp4, tn1, color, material});
    }
}


/// Orthonormal coordinate basis aligned directly with projectile velocity trajectory vector.
/// Maps local +Z along velocity direction (forward), local -Z backward along trail wake,
/// eliminating Euler gimbal lock, angle wrapping, and lateral skew during strafing.
static glm::mat4 make_projectile_matrix(const glm::vec3& position, const glm::vec3& velocity) {
    glm::vec3 fwd = (glm::length(velocity) > 0.001f) ? glm::normalize(velocity) : glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up_hint = (std::abs(fwd.y) > 0.96f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(up_hint, fwd));
    glm::vec3 up = glm::cross(fwd, right);
    return glm::mat4(
        glm::vec4(right, 0.0f),
        glm::vec4(up, 0.0f),
        glm::vec4(fwd, 0.0f),
        glm::vec4(position, 1.0f)
    );
}

/// Sleek aerodynamic kinetic/plasma projectile:
/// 6-sided faceted nose cone, cylindrical PBR energy sheath, boat-tail exhaust,
/// 4 stabilizing ion vanes, white-hot core spindle, and 3D volumetric energy wake.
static void add_plasma_bolt_mesh(
    std::vector<StalkerVertex>& verts,
    const glm::mat4& transform,
    float r,
    float length,
    const glm::vec4& color,
    const glm::vec4& core_color,
    const glm::vec4& material
) {
    glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(transform)));
    auto xform_pt = [&](float x, float y, float z) -> glm::vec3 {
        return glm::vec3(transform * glm::vec4(x, y, z, 1.0f));
    };
    auto xform_n = [&](const glm::vec3& n) -> glm::vec3 {
        return glm::normalize(normal_mat * n);
    };

    auto push_tri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                        const glm::vec3& n, const glm::vec4& col, const glm::vec4& mat) {
        verts.push_back({a, n, col, mat});
        verts.push_back({b, n, col, mat});
        verts.push_back({c, n, col, mat});
    };

    auto push_quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                         const glm::vec3& n, const glm::vec4& col, const glm::vec4& mat) {
        push_tri(a, b, c, n, col, mat);
        push_tri(a, c, d, n, col, mat);
    };

    // 1. Aerodynamic 6-sided Nose Cone & Cylindrical Body & Tapered Boat-Tail
    constexpr int SEGS = 6;
    float cos_table[SEGS];
    float sin_table[SEGS];
    for (int i = 0; i < SEGS; ++i) {
        float angle = i * (6.2831853f / SEGS);
        cos_table[i] = std::cos(angle);
        sin_table[i] = std::sin(angle);
    }

    float z_apex = length * 0.50f;
    float z_shoulder = length * 0.15f;
    float z_waist = -length * 0.20f;
    float z_tail = -length * 0.50f;
    float r_body = r;
    float r_tail = r * 0.35f;

    // Nose Cone: Apex -> Shoulder ring
    glm::vec3 pt_apex = xform_pt(0.0f, 0.0f, z_apex);
    for (int i = 0; i < SEGS; ++i) {
        int next = (i + 1) % SEGS;
        glm::vec3 p0 = xform_pt(cos_table[i] * r_body, sin_table[i] * r_body, z_shoulder);
        glm::vec3 p1 = xform_pt(cos_table[next] * r_body, sin_table[next] * r_body, z_shoulder);
        glm::vec3 face_n = glm::normalize(glm::cross(p1 - pt_apex, p0 - pt_apex));
        push_tri(pt_apex, p1, p0, face_n, color, material);
    }

    // Cylindrical Body: Shoulder ring -> Waist ring
    for (int i = 0; i < SEGS; ++i) {
        int next = (i + 1) % SEGS;
        glm::vec3 p0 = xform_pt(cos_table[i] * r_body, sin_table[i] * r_body, z_shoulder);
        glm::vec3 p1 = xform_pt(cos_table[next] * r_body, sin_table[next] * r_body, z_shoulder);
        glm::vec3 p2 = xform_pt(cos_table[next] * r_body, sin_table[next] * r_body, z_waist);
        glm::vec3 p3 = xform_pt(cos_table[i] * r_body, sin_table[i] * r_body, z_waist);
        glm::vec3 face_n = xform_n(glm::vec3((cos_table[i] + cos_table[next]) * 0.5f,
                                            (sin_table[i] + sin_table[next]) * 0.5f, 0.0f));
        push_quad(p0, p1, p2, p3, face_n, color, material);
    }

    // Boat-Tail: Waist ring -> Tail ring
    for (int i = 0; i < SEGS; ++i) {
        int next = (i + 1) % SEGS;
        glm::vec3 p0 = xform_pt(cos_table[i] * r_body, sin_table[i] * r_body, z_waist);
        glm::vec3 p1 = xform_pt(cos_table[next] * r_body, sin_table[next] * r_body, z_waist);
        glm::vec3 p2 = xform_pt(cos_table[next] * r_tail, sin_table[next] * r_tail, z_tail);
        glm::vec3 p3 = xform_pt(cos_table[i] * r_tail, sin_table[i] * r_tail, z_tail);
        glm::vec3 face_n = xform_n(glm::vec3((cos_table[i] + cos_table[next]) * 0.5f,
                                            (sin_table[i] + sin_table[next]) * 0.5f, -0.4f));
        push_quad(p0, p1, p2, p3, face_n, color, material);
    }

    // 2. White-Hot Emissive Core Spindle (intense radiant center)
    glm::vec4 core_mat(0.0f, 0.1f, 8.5f, 1.0f);
    float r_core = r * 0.45f;
    float z_core_fwd = length * 0.35f;
    float z_core_back = -length * 0.35f;
    glm::vec3 c_fwd = xform_pt(0.0f, 0.0f, z_core_fwd);
    glm::vec3 c_back = xform_pt(0.0f, 0.0f, z_core_back);
    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        float a0 = i * 1.570796f;
        float a1 = next * 1.570796f;
        glm::vec3 cp0 = xform_pt(std::cos(a0) * r_core, std::sin(a0) * r_core, 0.0f);
        glm::vec3 cp1 = xform_pt(std::cos(a1) * r_core, std::sin(a1) * r_core, 0.0f);
        glm::vec3 n_fwd = glm::normalize(glm::cross(cp1 - c_fwd, cp0 - c_fwd));
        glm::vec3 n_back = glm::normalize(glm::cross(cp0 - c_back, cp1 - c_back));
        push_tri(c_fwd, cp1, cp0, n_fwd, core_color, core_mat);
        push_tri(c_back, cp0, cp1, n_back, core_color, core_mat);
    }

    // 3. Four Aerodynamic Ion Stabilizing Fins (+X, -X, +Y, -Y)
    glm::vec4 fin_color = glm::mix(color, glm::vec4(0.05f, 0.08f, 0.12f, 1.0f), 0.45f);
    glm::vec4 fin_mat(0.8f, 0.25f, 2.0f, 1.0f);
    float fin_span = r * 2.2f;
    float fin_root = r * 0.85f;
    float fin_z0 = -length * 0.12f;
    float fin_z1 = -length * 0.48f;
    float fin_th = r * 0.12f;

    // Lateral Fins (+X and -X)
    push_quad(xform_pt(fin_root, 0.0f, fin_z0), xform_pt(fin_span, 0.0f, fin_z1),
              xform_pt(fin_span, 0.0f, fin_z1 - fin_th), xform_pt(fin_root, 0.0f, fin_z0 - fin_th),
              xform_n(glm::vec3(0, 1, 0)), fin_color, fin_mat);
    push_quad(xform_pt(-fin_root, 0.0f, fin_z0), xform_pt(-fin_span, 0.0f, fin_z1),
              xform_pt(-fin_span, 0.0f, fin_z1 - fin_th), xform_pt(-fin_root, 0.0f, fin_z0 - fin_th),
              xform_n(glm::vec3(0, 1, 0)), fin_color, fin_mat);
    // Vertical Fins (+Y and -Y)
    push_quad(xform_pt(0.0f, fin_root, fin_z0), xform_pt(0.0f, fin_span, fin_z1),
              xform_pt(0.0f, fin_span, fin_z1 - fin_th), xform_pt(0.0f, fin_root, fin_z0 - fin_th),
              xform_n(glm::vec3(1, 0, 0)), fin_color, fin_mat);
    push_quad(xform_pt(0.0f, -fin_root, fin_z0), xform_pt(0.0f, -fin_span, fin_z1),
              xform_pt(0.0f, -fin_span, fin_z1 - fin_th), xform_pt(0.0f, -fin_root, fin_z0 - fin_th),
              xform_n(glm::vec3(1, 0, 0)), fin_color, fin_mat);

    // 4. Volumetric 3D Trail Wake (Cross-Ribbon trailing behind projectile)
    float wake_len = 2.4f;
    float wake_r = r * 1.1f;
    glm::vec4 wake_color = glm::mix(color, glm::vec4(1.0f), 0.35f);
    glm::vec4 wake_mat(0.0f, 0.9f, 4.5f, 1.0f);
    push_quad(xform_pt(wake_r, 0.0f, z_tail), xform_pt(-wake_r, 0.0f, z_tail),
              xform_pt(-wake_r * 0.15f, 0.0f, z_tail - wake_len), xform_pt(wake_r * 0.15f, 0.0f, z_tail - wake_len),
              xform_n(glm::vec3(0, 1, 0)), wake_color, wake_mat);
    push_quad(xform_pt(0.0f, wake_r, z_tail), xform_pt(0.0f, -wake_r, z_tail),
              xform_pt(0.0f, -wake_r * 0.15f, z_tail - wake_len), xform_pt(0.0f, wake_r * 0.15f, z_tail - wake_len),
              xform_n(glm::vec3(1, 0, 0)), wake_color, wake_mat);
}

/// Dangerous bio-crystalline quill fired by Void Stalkers:
/// Hexagonal diamond needle barb, backward-swept recurved chitin barbs,
/// radiant toxic emerald core spindle, and volumetric virulent green vapor wake.
static void add_void_spike_mesh(
    std::vector<StalkerVertex>& verts,
    const glm::mat4& transform,
    float r,
    float length,
    const glm::vec4& crystal_color,
    const glm::vec4& core_color,
    const glm::vec4& material
) {
    glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(transform)));
    auto xform_pt = [&](float x, float y, float z) -> glm::vec3 {
        return glm::vec3(transform * glm::vec4(x, y, z, 1.0f));
    };
    auto xform_n = [&](const glm::vec3& n) -> glm::vec3 {
        return glm::normalize(normal_mat * n);
    };

    auto push_tri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                        const glm::vec3& n, const glm::vec4& col, const glm::vec4& mat) {
        verts.push_back({a, n, col, mat});
        verts.push_back({b, n, col, mat});
        verts.push_back({c, n, col, mat});
    };

    auto push_quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                         const glm::vec3& n, const glm::vec4& col, const glm::vec4& mat) {
        push_tri(a, b, c, n, col, mat);
        push_tri(a, c, d, n, col, mat);
    };

    // 1. Hexagonal Crystalline Spine Profile
    constexpr int SEGS = 6;
    float cos_table[SEGS];
    float sin_table[SEGS];
    for (int i = 0; i < SEGS; ++i) {
        float angle = i * (6.2831853f / SEGS);
        cos_table[i] = std::cos(angle);
        sin_table[i] = std::sin(angle);
    }

    float z_apex = length * 0.50f;
    float z_shoulder = length * 0.12f;
    float z_waist = -length * 0.15f;
    float z_tail = -length * 0.50f;
    float r_shoulder = r * 1.35f;
    float r_waist = r * 0.85f;

    // Apex Diamond Barb: Apex needle point -> Shoulder flare
    glm::vec3 pt_apex = xform_pt(0.0f, 0.0f, z_apex);
    for (int i = 0; i < SEGS; ++i) {
        int next = (i + 1) % SEGS;
        glm::vec3 p0 = xform_pt(cos_table[i] * r_shoulder, sin_table[i] * r_shoulder, z_shoulder);
        glm::vec3 p1 = xform_pt(cos_table[next] * r_shoulder, sin_table[next] * r_shoulder, z_shoulder);
        glm::vec3 face_n = glm::normalize(glm::cross(p1 - pt_apex, p0 - pt_apex));
        push_tri(pt_apex, p1, p0, face_n, crystal_color, material);
    }

    // Mid Crystal Shaft: Shoulder flare -> Waist
    for (int i = 0; i < SEGS; ++i) {
        int next = (i + 1) % SEGS;
        glm::vec3 p0 = xform_pt(cos_table[i] * r_shoulder, sin_table[i] * r_shoulder, z_shoulder);
        glm::vec3 p1 = xform_pt(cos_table[next] * r_shoulder, sin_table[next] * r_shoulder, z_shoulder);
        glm::vec3 p2 = xform_pt(cos_table[next] * r_waist, sin_table[next] * r_waist, z_waist);
        glm::vec3 p3 = xform_pt(cos_table[i] * r_waist, sin_table[i] * r_waist, z_waist);
        glm::vec3 face_n = xform_n(glm::vec3((cos_table[i] + cos_table[next]) * 0.5f,
                                            (sin_table[i] + sin_table[next]) * 0.5f, 0.0f));
        push_quad(p0, p1, p2, p3, face_n, crystal_color, material);
    }

    // Tapered Crystal Tail: Waist -> Tail tip
    glm::vec3 pt_tail = xform_pt(0.0f, 0.0f, z_tail);
    for (int i = 0; i < SEGS; ++i) {
        int next = (i + 1) % SEGS;
        glm::vec3 p0 = xform_pt(cos_table[i] * r_waist, sin_table[i] * r_waist, z_waist);
        glm::vec3 p1 = xform_pt(cos_table[next] * r_waist, sin_table[next] * r_waist, z_waist);
        glm::vec3 face_n = glm::normalize(glm::cross(p0 - pt_tail, p1 - pt_tail));
        push_tri(pt_tail, p0, p1, face_n, crystal_color, material);
    }

    // 2. Lateral Backward-Curving Chitinous Barbs (+X and -X)
    glm::vec4 barb_color = glm::vec4(0.08f, 0.22f, 0.10f, 1.0f);
    glm::vec4 barb_mat(0.4f, 0.5f, 2.5f, 1.0f);
    float barb_out = r * 2.1f;
    float barb_z_tip = -length * 0.22f;
    // Right barb (+X)
    push_tri(xform_pt(r_shoulder, 0.0f, z_shoulder),
             xform_pt(barb_out, 0.0f, barb_z_tip),
             xform_pt(r_waist, 0.0f, z_waist),
             xform_n(glm::vec3(0, 1, 0)), barb_color, barb_mat);
    // Left barb (-X)
    push_tri(xform_pt(-r_shoulder, 0.0f, z_shoulder),
             xform_pt(-r_waist, 0.0f, z_waist),
             xform_pt(-barb_out, 0.0f, barb_z_tip),
             xform_n(glm::vec3(0, 1, 0)), barb_color, barb_mat);

    // 3. Radiant Toxic Emerald Core Spindle
    glm::vec4 core_mat(0.0f, 0.1f, 7.5f, 1.0f);
    float r_core = r * 0.40f;
    float z_c_fwd = length * 0.35f;
    float z_c_back = -length * 0.35f;
    glm::vec3 c_fwd = xform_pt(0.0f, 0.0f, z_c_fwd);
    glm::vec3 c_back = xform_pt(0.0f, 0.0f, z_c_back);
    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        float a0 = i * 1.570796f;
        float a1 = next * 1.570796f;
        glm::vec3 cp0 = xform_pt(std::cos(a0) * r_core, std::sin(a0) * r_core, 0.0f);
        glm::vec3 cp1 = xform_pt(std::cos(a1) * r_core, std::sin(a1) * r_core, 0.0f);
        glm::vec3 n_fwd = glm::normalize(glm::cross(cp1 - c_fwd, cp0 - c_fwd));
        glm::vec3 n_back = glm::normalize(glm::cross(cp0 - c_back, cp1 - c_back));
        push_tri(c_fwd, cp1, cp0, n_fwd, core_color, core_mat);
        push_tri(c_back, cp0, cp1, n_back, core_color, core_mat);
    }

    // 4. Volumetric Toxic Green Wake Ribbon
    float wake_len = 1.8f;
    float wake_r = r * 0.85f;
    glm::vec4 wake_color = glm::vec4(0.20f, 1.0f, 0.35f, 0.85f);
    glm::vec4 wake_mat(0.0f, 0.9f, 4.0f, 1.0f);
    push_quad(xform_pt(wake_r, 0.0f, z_tail), xform_pt(-wake_r, 0.0f, z_tail),
              xform_pt(-wake_r * 0.10f, 0.0f, z_tail - wake_len), xform_pt(wake_r * 0.10f, 0.0f, z_tail - wake_len),
              xform_n(glm::vec3(0, 1, 0)), wake_color, wake_mat);
    push_quad(xform_pt(0.0f, wake_r, z_tail), xform_pt(0.0f, -wake_r, z_tail),
              xform_pt(0.0f, -wake_r * 0.10f, z_tail - wake_len), xform_pt(0.0f, wake_r * 0.10f, z_tail - wake_len),
              xform_n(glm::vec3(1, 0, 0)), wake_color, wake_mat);
}

void Renderer::render_stalkers(const std::vector<VoidStalker>& stalkers) {
    static const std::vector<VoidSpikeProjectile> s_empty_proj;
    render_stalkers(stalkers, s_empty_proj);
}

void Renderer::render_stalkers(const std::vector<VoidStalker>& stalkers, const std::vector<VoidSpikeProjectile>& projectiles) {
    if ((stalkers.empty() && projectiles.empty()) || m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_stalker_verts;
    s_stalker_verts.clear();
    size_t needed_vert_cap = std::max(static_cast<size_t>(16384), stalkers.size() * 1100 + projectiles.size() * 64);
    if (s_stalker_verts.capacity() < needed_vert_cap) {
        s_stalker_verts.reserve(needed_vert_cap);
    }

    m_stalker_staging_lines.clear();
    size_t needed_line_cap = std::max(static_cast<size_t>(4096), stalkers.size() * 64 + projectiles.size() * 12);
    if (m_stalker_staging_lines.capacity() < needed_line_cap) {
        m_stalker_staging_lines.reserve(needed_line_cap);
    }

    int active_stalker_count = 0;
    for (const auto& s : stalkers) {
        if (s.is_dead()) continue;
        if (std::isnan(s.position.x) || std::isnan(s.position.y) || std::isnan(s.position.z)) continue;
        active_stalker_count++;

        // Pose computation from dynamic animation controller
        StalkerPoseParameters pose = s.anim_controller.compute_blended_pose(s.pitch);

        // Base transform: translate to stalker world position + surface snapping offset,
        // and apply smooth quaternion orientation slerp (supporting floor, vertical wall, and inverted ceiling traversal)
        glm::vec3 surf_offset = s.surface_offset;
        if (std::isnan(surf_offset.x) || std::isnan(surf_offset.y) || std::isnan(surf_offset.z)) {
            surf_offset = glm::vec3(0.0f);
        }
        glm::quat rot = s.m_currentRotation;
        if (std::isnan(rot.x) || std::isnan(rot.y) || std::isnan(rot.z) || std::isnan(rot.w) ||
            glm::length(glm::vec4(rot.x, rot.y, rot.z, rot.w)) < 0.001f) {
            rot = glm::angleAxis(s.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        glm::mat4 base_model = glm::translate(glm::mat4(1.0f), s.position + surf_offset);
        base_model = base_model * glm::mat4_cast(rot);

        // State-based body posture modifier
        float crouch_y = pose.carapace_offset_y;
        float lunge_forward = 0.0f;
        float mandible_spread = 0.0f;

        if (s.state == StalkerState::Lunging) {
            crouch_y -= 0.1f;
            lunge_forward = 0.35f;
            mandible_spread = 0.35f; // Jaws flared wide open to strike!
        } else if (s.state == StalkerState::GroundSlam) {
            float windup_frac = std::clamp(s.state_timer / std::max(0.1f, s.boss_attack_windup), 0.0f, 1.0f);
            crouch_y += (1.0f - windup_frac) * 0.35f - windup_frac * 0.15f;
            mandible_spread = 0.55f;
        } else if (s.state == StalkerState::Charging) {
            crouch_y -= 0.22f;
            lunge_forward = 0.40f;
            mandible_spread = 0.45f;
        } else if (s.state == StalkerState::Stalking) {
            crouch_y -= 0.15f; // Low predatory stalk
        } else if (s.state == StalkerState::Stunned) {
            // Smooth disoriented stagger posture (subtle head roll, no 35Hz spazzing jitter)
            float stagger = std::sin(s.glow_phase * 4.0f) * 0.04f;
            base_model = glm::rotate(base_model, stagger, glm::vec3(0.0f, 1.0f, 0.0f));
            crouch_y -= 0.2f;
        } else if (s.state == StalkerState::Burrowing) {
            // Rhythmic excavation drilling surge pulse (forward-back bore thrust, no roll spazzing)
            float drill_pulse = std::sin(s.state_timer * 18.0f) * 0.025f;
            base_model = glm::translate(base_model, glm::vec3(0.0f, 0.0f, drill_pulse));
            crouch_y -= 0.20f;
            lunge_forward = 0.25f;
            mandible_spread = 0.40f + std::abs(std::sin(s.state_timer * 24.0f)) * 0.20f;
        } else if (s.state == StalkerState::Emerging) {
            float progress = std::clamp(s.state_timer / std::max(0.1f, s.burrow_duration), 0.0f, 1.0f);
            if (s.role == StalkerRole::Melee) {
                // Melee: frantic claw digging surge, snapping mandibles, aggressive predatory crawl
                float dig_surge = std::sin(progress * 28.0f) * 0.035f * (1.0f - progress);
                base_model = glm::translate(base_model, glm::vec3(0.0f, 0.0f, dig_surge));
                crouch_y -= 0.15f * (1.0f - progress);
                lunge_forward = 0.20f * progress;
                mandible_spread = 0.45f * (1.0f - progress) + std::abs(std::sin(s.state_timer * 22.0f)) * 0.25f;
            } else if (s.role == StalkerRole::Shooter) {
                // Shooter: toxic serpent sinuous slither wiggle emerging low from shadows
                float slither = std::sin(progress * 18.0f) * 0.08f * (1.0f - progress);
                base_model = glm::rotate(base_model, slither, glm::vec3(0.0f, 1.0f, 0.0f));
                crouch_y -= 0.22f * (1.0f - progress);
                mandible_spread = 0.20f + std::sin(s.state_timer * 14.0f) * 0.15f;
            } else if (s.role == StalkerRole::ChitinGoliath) {
                // Goliath: heavy tectonic heave, massive armored carapace pushing stone open
                float heave = std::sin(progress * 12.0f) * 0.05f * (1.0f - progress);
                base_model = glm::translate(base_model, glm::vec3(0.0f, 0.0f, heave));
                crouch_y -= 0.30f * (1.0f - progress);
                mandible_spread = 0.50f;
            }
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
        } else if (s.state == StalkerState::Emerging) {
            float progress = std::clamp(s.state_timer / std::max(0.1f, s.burrow_duration), 0.0f, 1.0f);
            // Physical burrowing emergence: starts recessed beneath surface plane and expands outward into room
            float submerge_depth = (s.role == StalkerRole::ChitinGoliath) ? 0.90f :
                                   (s.role == StalkerRole::Shooter) ? 0.65f : 0.55f;
            float current_depth = (1.0f - progress) * submerge_depth;
            float push_out = (progress - 1.0f) * 0.50f; // enters forward out of borehole
            base_model = glm::translate(base_model, glm::vec3(0.0f, -current_depth, push_out));
            float emerge_scale = glm::mix(0.28f, 1.0f, glm::smoothstep(0.0f, 1.0f, progress));
            base_model = glm::scale(base_model, glm::vec3(emerge_scale));
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

        // Enraged Boss Apex Frenzy visuals (scorched magma veins & radiant spikes)
        if (is_goliath && s.is_enraged) {
            float enrage_pulse = 0.8f + 0.2f * std::sin(s.glow_phase * 6.0f);
            chitin_color = glm::mix(chitin_color, glm::vec4(0.35f, 0.08f, 0.04f, 1.0f), 0.65f);
            spine_color = glm::mix(spine_color, glm::vec4(0.95f, 0.35f, 0.05f, 1.0f), 0.75f * enrage_pulse);
            claw_color = glm::mix(claw_color, glm::vec4(1.0f, 0.25f, 0.1f, 1.0f), 0.80f * enrage_pulse);
            spine_mat.z += 2.5f * enrage_pulse;
            claw_mat.z += 3.0f * enrage_pulse;
        }

        // 4. Bioluminescent Eye Color
        glm::vec4 eye_color = s.get_eye_color();
        glm::vec4 eye_mat(0.0f, 0.02f, 6.5f, 1.0f); // Massive emissive boost for bloom!

        // White emissive hit-flash shader pulse (0.08s) - active combatants only, suppressed on death/dying
        if (s.hit_flash_timer > 0.0f && s.state != StalkerState::Dying) {
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

        // When dying, extinguish bioluminescent life signs (eyes & core fade out to dark)
        if (s.state == StalkerState::Dying) {
            float death_progress = std::clamp(s.death_timer / std::max(0.01f, s.death_duration), 0.0f, 1.0f);
            float life_fade = 1.0f - death_progress;
            eye_mat.z *= life_fade;
            core_mat.z *= life_fade;
            eye_color = glm::mix(glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), eye_color, life_fade);
            core_color = glm::mix(glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), core_color, life_fade);
            // Settle chitin colors naturally into a collapsed carcass tone
            chitin_color = glm::mix(chitin_color, chitin_color * 0.75f, death_progress);
            spine_color  = glm::mix(spine_color,  spine_color * 0.75f,  death_progress);
            claw_color   = glm::mix(claw_color,   claw_color * 0.75f,   death_progress);
        }

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
                    : (s.state == StalkerState::Burrowing || s.state == StalkerState::Emerging)
                        ? (-0.55f + std::sin(s.state_timer * (s.role == StalkerRole::Melee ? 32.0f : 20.0f) + (is_left ? 0.0f : 3.14f)) * 0.55f)
                        : (-0.35f + pose.front_claw_pitch_offset + crawl_swing * 0.2f);
                float strike_yaw = side * (pose.front_claw_yaw + (s.state == StalkerState::Lunging ? 0.3f : ((s.state == StalkerState::Burrowing || s.state == StalkerState::Emerging) ? -0.15f : 0.0f)));

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
        glm::mat4 p_mat = make_projectile_matrix(proj.position, proj.velocity);

        glm::vec4 proj_color(0.18f, 1.0f, 0.35f, 1.0f); // Vivid radiant toxic green crystal
        glm::vec4 core_color(0.70f, 1.0f, 0.80f, 1.0f); // White-green radiant core
        glm::vec4 proj_mat(0.2f, 0.8f, 5.0f, 1.0f);      // Highly emissive with green bloom
        add_void_spike_mesh(s_stalker_verts, p_mat, 0.06f, 0.70f, proj_color, core_color, proj_mat);

        // Luminous streak trail behind projectile
        glm::vec3 trail_end = proj.position - p_dir * 1.8f;
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
        m_stalker_shader.set_float("uDissolveThreshold", 0.0f);
        m_stalker_shader.set_float("u_dissolveThreshold", 0.0f);
        m_stalker_shader.set_float("uDarknessFactor", m_area_darkness);

        // Upload to dynamic VBO (expanded dynamically so zero monsters or projectiles are ever dropped)
        ensure_stalker_vbo_capacity(s_stalker_verts.size());
        glBindVertexArray(m_stalker_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
        GLsizeiptr byte_size = static_cast<GLsizeiptr>(s_stalker_verts.size() * sizeof(StalkerVertex));
        glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_stalker_verts.data());

        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_stalker_verts.size()));
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

        ensure_cable_vbo_capacity(m_stalker_staging_lines.size());
        glBindVertexArray(m_cable_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
        GLsizeiptr line_bytes = static_cast<GLsizeiptr>(m_stalker_staging_lines.size() * sizeof(float));
        glBufferSubData(GL_ARRAY_BUFFER, 0, line_bytes, m_stalker_staging_lines.data());

        glLineWidth(2.5f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_stalker_staging_lines.size() / 3));

        glBindVertexArray(0);
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }
}

void Renderer::render_carcasses(const std::vector<EnemyCarcass>& carcasses) {
    if (carcasses.empty() || m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_carcass_verts;
    s_carcass_verts.clear();
    size_t needed_vert_cap = std::max(static_cast<size_t>(8192), carcasses.size() * 512);
    if (s_carcass_verts.capacity() < needed_vert_cap) {
        s_carcass_verts.reserve(needed_vert_cap);
    }

    for (const auto& c : carcasses) {
        if (c.alpha <= 0.001f) continue;

        // Base transform for carcass: world position and death orientation
        glm::mat4 base_model = glm::translate(glm::mat4(1.0f), c.position);
        base_model = base_model * glm::mat4_cast(c.rotation);
        base_model = glm::scale(base_model, glm::vec3(c.scale));
        // Flat collapsed death pose
        base_model = glm::translate(base_model, glm::vec3(0.0f, -0.15f, 0.0f));

        if (c.is_burrower()) {
            glm::vec4 armor_color(0.08f, 0.07f, 0.06f, c.alpha);
            glm::vec4 armor_mat(0.35f, 0.45f, 0.0f, 1.0f);

            glm::vec4 cutter_color(0.55f, 0.50f, 0.45f, c.alpha);
            glm::vec4 cutter_mat(0.75f, 0.25f, 0.0f, 1.0f);

            // Cold extinguished magma core/slag
            glm::vec4 core_color(0.22f, 0.07f, 0.02f, c.alpha);
            glm::vec4 core_mat(0.1f, 0.05f, 0.2f, 1.0f);

            glm::vec4 obsidian_color(0.03f, 0.03f, 0.04f, c.alpha);
            glm::vec4 obsidian_mat(0.60f, 0.20f, 0.0f, 1.0f);

            // Conical drill borer (slumped / jammed against cavern floor)
            glm::mat4 head_m = glm::translate(base_model, glm::vec3(0.0f, -0.05f, 0.60f));
            head_m = glm::rotate(head_m, 0.35f, glm::vec3(0.0f, 0.0f, 1.0f));
            add_stalker_box(s_carcass_verts, head_m, glm::vec3(0.35f, 0.35f, 0.45f), cutter_color, cutter_mat);

            for (int i = 0; i < 4; ++i) {
                float a = static_cast<float>(i) * glm::half_pi<float>() + 0.35f;
                glm::mat4 tooth_m = glm::translate(head_m, glm::vec3(std::cos(a) * 0.22f, std::sin(a) * 0.22f, 0.24f));
                tooth_m = glm::rotate(tooth_m, a, glm::vec3(0.0f, 0.0f, 1.0f));
                add_stalker_box(s_carcass_verts, tooth_m, glm::vec3(0.14f, 0.08f, 0.28f), cutter_color, cutter_mat);
            }
            add_stalker_box(s_carcass_verts, glm::translate(head_m, glm::vec3(0.0f, 0.0f, 0.30f)), glm::vec3(0.18f, 0.18f, 0.15f), core_color, core_mat);

            // Armored Thorax
            glm::mat4 thorax_m = glm::translate(base_model, glm::vec3(0.0f, 0.0f, 0.0f));
            add_stalker_box(s_carcass_verts, thorax_m, glm::vec3(0.55f, 0.45f, 0.50f), armor_color, armor_mat);
            add_stalker_box(s_carcass_verts, glm::translate(thorax_m, glm::vec3(0.0f, 0.22f, 0.0f)), glm::vec3(0.12f, 0.22f, 0.42f), obsidian_color, obsidian_mat);

            // Abdomen Segments slumped against floor
            for (int i = 0; i < 3; ++i) {
                float seg_z = -0.45f - static_cast<float>(i) * 0.42f;
                float seg_scale = 1.0f - static_cast<float>(i) * 0.16f;
                glm::mat4 seg_m = glm::translate(base_model, glm::vec3(0.0f, -0.02f, seg_z));
                add_stalker_box(s_carcass_verts, seg_m, glm::vec3(0.48f * seg_scale, 0.38f * seg_scale, 0.38f), armor_color, armor_mat);
                add_stalker_box(s_carcass_verts, glm::translate(seg_m, glm::vec3(0.0f, 0.18f * seg_scale, 0.0f)), glm::vec3(0.08f, 0.16f * seg_scale, 0.28f), obsidian_color, obsidian_mat);
            }
            continue;
        }

        bool is_shooter = (c.role == StalkerRole::Shooter);
        bool is_goliath = (c.role == StalkerRole::ChitinGoliath);

        glm::vec4 chitin_color = is_goliath
            ? glm::vec4(0.16f, 0.14f, 0.12f, c.alpha)
            : (is_shooter
                ? glm::vec4(0.03f, 0.16f, 0.06f, c.alpha)
                : glm::vec4(0.18f, 0.03f, 0.03f, c.alpha));
        glm::vec4 chitin_mat(is_goliath ? 0.65f : 0.30f, 0.35f, 0.0f, 1.0f);

        glm::vec4 spine_color = is_goliath
            ? glm::vec4(0.40f, 0.28f, 0.10f, c.alpha)
            : (is_shooter
                ? glm::vec4(0.06f, 0.30f, 0.10f, c.alpha)
                : glm::vec4(0.30f, 0.04f, 0.04f, c.alpha));
        glm::vec4 spine_mat(0.35f, 0.30f, 0.0f, 0.9f);

        // Build limp collapsed death model (carapace, collapsed head, abdomen, splayed legs)
        glm::mat4 thorax_m = glm::translate(base_model, glm::vec3(0.0f, 0.05f, 0.0f));
        add_stalker_box(s_carcass_verts, thorax_m, glm::vec3(0.26f, 0.12f, 0.35f), chitin_color, chitin_mat);

        glm::mat4 skull_m = glm::translate(base_model, glm::vec3(0.0f, 0.02f, 0.35f));
        add_stalker_box(s_carcass_verts, skull_m, glm::vec3(0.20f, 0.09f, 0.20f), chitin_color, chitin_mat);

        if (is_goliath) {
            glm::mat4 crown_m = glm::translate(base_model, glm::vec3(0.0f, 0.16f, 0.10f));
            add_stalker_box(s_carcass_verts, crown_m, glm::vec3(0.30f, 0.12f, 0.28f), spine_color, spine_mat);
        }

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
    m_stalker_shader.set_float("uDissolveThreshold", 0.0f);
    m_stalker_shader.set_float("u_dissolveThreshold", 0.0f);

    ensure_stalker_vbo_capacity(s_carcass_verts.size());
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    GLsizeiptr byte_size = static_cast<GLsizeiptr>(s_carcass_verts.size() * sizeof(StalkerVertex));
    glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_carcass_verts.data());

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_carcass_verts.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void Renderer::render_plasma_bolts(const std::vector<PlayerPlasmaBolt>& bolts) {
    if (bolts.empty() || m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_bolt_verts;
    s_bolt_verts.clear();
    if (s_bolt_verts.capacity() < 4096) s_bolt_verts.reserve(4096);

    static std::vector<float> s_bolt_lines;
    s_bolt_lines.clear();
    if (s_bolt_lines.capacity() < 2048) s_bolt_lines.reserve(2048);

    for (const auto& bolt : bolts) {
        if (!bolt.active) continue;

        glm::vec3 b_dir = (glm::length(bolt.velocity) > 0.001f) ? glm::normalize(bolt.velocity) : glm::vec3(0.0f, 0.0f, -1.0f);
        glm::mat4 b_mat = make_projectile_matrix(bolt.position, bolt.velocity);

        glm::vec4 bolt_color = bolt.color;
        glm::vec4 core_color = glm::mix(bolt_color, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.75f); // White-hot radiant core
        glm::vec4 bolt_mat(0.1f, 0.9f, 6.0f, 1.0f);    // Ultra-bright emissive with HDR bloom
        float r = (bolt.radius > 0.01f) ? bolt.radius : 0.05f;
        float bolt_len = std::max(0.45f, r * 7.5f);

        add_plasma_bolt_mesh(s_bolt_verts, b_mat, r, bolt_len, bolt_color, core_color, bolt_mat);

        // Luminous streak tracer line along true 3D trajectory
        glm::vec3 trail_end = bolt.position - b_dir * 2.2f;
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

    ensure_stalker_vbo_capacity(s_bolt_verts.size());
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    GLsizeiptr byte_size = static_cast<GLsizeiptr>(s_bolt_verts.size() * sizeof(StalkerVertex));
    glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_bolt_verts.data());

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_bolt_verts.size()));
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

        ensure_cable_vbo_capacity(s_bolt_lines.size());
        glBindVertexArray(m_cable_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
        GLsizeiptr line_bytes = static_cast<GLsizeiptr>(s_bolt_lines.size() * sizeof(float));
        glBufferSubData(GL_ARRAY_BUFFER, 0, line_bytes, s_bolt_lines.data());

        glLineWidth(3.0f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s_bolt_lines.size() / 3));

        glBindVertexArray(0);
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }
}

void Renderer::render_burrowers(const std::vector<SeismicBurrower>& burrowers) {
    if (burrowers.empty() || m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_burrower_verts;
    s_burrower_verts.clear();
    size_t needed_vert_cap = std::max(static_cast<size_t>(16384), burrowers.size() * 900);
    if (s_burrower_verts.capacity() < needed_vert_cap) {
        s_burrower_verts.reserve(needed_vert_cap);
    }

    int active_burrower_count = 0;
    for (const auto& b : burrowers) {
        if (b.is_dead()) continue;
        if (std::isnan(b.position.x) || std::isnan(b.position.y) || std::isnan(b.position.z)) continue;
        active_burrower_count++;

        float yaw = std::isnan(b.yaw) ? 0.0f : b.yaw;
        float pitch = std::isnan(b.pitch) ? 0.0f : b.pitch;

        // Base transform: translate to burrower world position and rotate to facing yaw + pitch
        glm::mat4 base_model = glm::translate(glm::mat4(1.0f), b.position);
        base_model = glm::rotate(base_model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        base_model = glm::rotate(base_model, -pitch, glm::vec3(1.0f, 0.0f, 0.0f));

        if (b.state == BurrowerState::Stunned) {
            float jitter = std::sin(b.pulse_phase * 40.0f) * 0.12f;
            base_model = glm::rotate(base_model, jitter, glm::vec3(0.0f, 0.0f, 1.0f));
        } else if (b.state == BurrowerState::Dying) {
            float death_progress = std::clamp(b.death_timer / std::max(0.1f, b.death_duration), 0.0f, 1.0f);
            float spasm = std::sin(b.death_timer * 32.0f) * (1.0f - death_progress) * 0.14f;
            base_model = glm::rotate(base_model, spasm, glm::vec3(0.0f, 0.0f, 1.0f));
            base_model = glm::translate(base_model, glm::vec3(0.0f, -0.16f * death_progress, 0.0f));
        } else if (b.state == BurrowerState::Breaching) {
            float progress = std::clamp(b.state_timer / 1.2f, 0.0f, 1.0f);
            // Burrowing emergence: starts recessed backwards inside bedrock and pushes out into the cavern
            float push = -(1.0f - progress) * 2.8f;
            base_model = glm::translate(base_model, glm::vec3(0.0f, 0.0f, push));
            float breach_scale = glm::mix(0.35f, 1.0f, glm::smoothstep(0.0f, 1.0f, progress));
            base_model = glm::scale(base_model, glm::vec3(breach_scale));
            // Heavy drill vibration
            float vib = std::sin(b.state_timer * 45.0f) * 0.035f * (1.0f - progress);
            base_model = glm::rotate(base_model, vib, glm::vec3(0.0f, 0.0f, 1.0f));
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
        float pulse = (b.state == BurrowerState::Dying)
            ? (std::max(0.0f, 1.0f - (b.death_timer / b.death_duration)) * (0.85f + 0.35f * std::sin(b.pulse_phase * 6.0f)))
            : (0.85f + 0.35f * std::sin(b.pulse_phase * 6.0f));
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

    ensure_stalker_vbo_capacity(s_burrower_verts.size());
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    GLsizeiptr byte_size = static_cast<GLsizeiptr>(s_burrower_verts.size() * sizeof(StalkerVertex));
    glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_burrower_verts.data());

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_burrower_verts.size()));
    glBindVertexArray(0);
}

void Renderer::render_quad() {
    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Renderer::end_frame(float delta_time, float radiation_level, float shield_timer,
                         const glm::vec3& hazard_haze_color, float hazard_haze_factor,
                         float suffocation_darkness) {
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
        m_fog_compute_shader.set_float("uHeadlampRange", m_headlamp.max_range);

        if (m_dust_timer > 0.0f) {
            m_dust_timer = std::max(0.0f, m_dust_timer - delta_time);
        }
        float dust_boost = (m_dust_timer > 0.0f) ? (0.075f * (m_dust_timer / 3.0f)) : 0.0f;

        float effective_haze = (hazard_haze_factor >= 0.0f) ? hazard_haze_factor : glm::clamp(radiation_level / 100.0f, 0.0f, 1.0f);
        m_fog_compute_shader.set_float("uFogDensity", 0.035f + dust_boost);
        m_fog_compute_shader.set_float("uToxicHazeFactor", effective_haze);
        m_fog_compute_shader.set_vec3("uHazardHazeColor", hazard_haze_color);
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

    // 2. Kawase Bloom Blur Pass (4-pass down/up Kawase chain on GL_COLOR_ATTACHMENT1 / m_bright_tex)
    unsigned int final_bloom_tex = m_bright_tex;
    if (m_bloom_fbo[0] != 0 && m_bloom_fbo[1] != 0 && m_bright_tex != 0) {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glViewport(0, 0, m_fog_width, m_fog_height);

        m_bloom_shader.use();
        m_bloom_shader.set_int("uImage", 0);

        // 4-pass down/up Kawase chain:
        // Pass 0 (Down 1): initial downsample from full-res emissive buffer
        // Pass 1 (Down 2): expansion/diffusion
        // Pass 2 (Up 1): wide gathering
        // Pass 3 (Up 2): smooth consolidation
        const float kawase_offsets[4] = { 0.0f, 1.0f, 2.0f, 1.0f };

        unsigned int input_tex = m_bright_tex;
        int dest_idx = 0;

        for (int pass = 0; pass < 4; ++pass) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_bloom_fbo[dest_idx]);
            m_bloom_shader.set_float("uOffset", kawase_offsets[pass]);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, input_tex);

            render_quad();

            input_tex = m_bloom_tex[dest_idx];
            dest_idx = 1 - dest_idx;
        }

        final_bloom_tex = input_tex; // m_bloom_tex[1] after 4 passes
    }

    // 3. Post-processing Tonemapping Composite to Screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_postprocess_shader.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_color_tex);
    m_postprocess_shader.set_int("uSceneColor", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, final_bloom_tex);
    m_postprocess_shader.set_int("uBloomColor", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_fog_tex);
    m_postprocess_shader.set_int("uVolumetricFog", 2);

    // Default white for SSAO placeholder if ssao pass not active
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_bright_tex);
    m_postprocess_shader.set_int("uSSAO", 3);

    // Full-resolution depth texture for bilateral upsampling of half-resolution volumetric fog
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_depth_tex);
    m_postprocess_shader.set_int("uDepthTexture", 4);

    m_postprocess_shader.set_float("uExposure", 1.15f * m_brightness);
    m_postprocess_shader.set_float("uBloomIntensity", 0.75f);
    m_postprocess_shader.set_float("uRadiationGlitch", glm::clamp(radiation_level / 100.0f, 0.0f, 1.0f));
    m_postprocess_shader.set_float("uTime", m_total_time);
    m_postprocess_shader.set_float("uNear", 0.1f);
    m_postprocess_shader.set_float("uFar", 250.0f);
    m_postprocess_shader.set_int("uSector", m_sector);
    m_postprocess_shader.set_float("uDarknessFactor", m_area_darkness);
    m_postprocess_shader.set_float("uSuffocationDarkness", suffocation_darkness);
    m_postprocess_shader.set_float("u_ShieldActive", shield_timer > 0.0f ? 1.0f : 0.0f);
    m_postprocess_shader.set_float("u_ShieldTimer", shield_timer);

    render_quad();
}

void Renderer::render_delver(const glm::vec3& pos, float yaw, CharacterClass cls, float anim_time) {
    if (m_stalker_vao == 0) return;

    static std::vector<StalkerVertex> s_delver_verts;
    s_delver_verts.clear();
    if (s_delver_verts.capacity() < 32768) {
        s_delver_verts.reserve(32768);
    }

    auto attr = get_character_attributes(cls);

    // Base transform: translate to world position and rotate
    glm::mat4 base_model = glm::translate(glm::mat4(1.0f), pos);
    base_model = glm::rotate(base_model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));

    // Breathing idle animation bob
    float breath_y = std::sin(anim_time * 2.5f) * 0.015f;
    base_model = glm::translate(base_model, glm::vec3(0.0f, breath_y, 0.0f));

    // Colors & High-Quality PBR Materials
    glm::vec4 suit_color = attr.suitSleeveColor;
    glm::vec4 accent_color = attr.primaryAccentColor;
    glm::vec4 glove_color = attr.gloveColor;
    glm::vec4 helmet_shell_color(0.84f, 0.86f, 0.90f, 1.0f);

    glm::vec4 col_dark_chassis(0.12f, 0.13f, 0.16f, 1.0f);
    glm::vec4 col_steel(0.72f, 0.75f, 0.80f, 1.0f);
    glm::vec4 col_steel_dark(0.38f, 0.42f, 0.46f, 1.0f);
    glm::vec4 col_brass(0.86f, 0.68f, 0.24f, 1.0f);
    glm::vec4 col_rubber(0.08f, 0.08f, 0.09f, 1.0f);
    glm::vec4 col_hazard_yellow(0.96f, 0.82f, 0.12f, 1.0f);
    glm::vec4 col_hazard_dark(0.10f, 0.10f, 0.12f, 1.0f);

    glm::vec4 mat_cloth(0.06f, 0.78f, 0.0f, 0.85f);
    glm::vec4 mat_armor(0.72f, 0.28f, 0.0f, 0.95f);
    glm::vec4 mat_dark_frame(0.68f, 0.32f, 0.0f, 0.90f);
    glm::vec4 mat_steel(0.90f, 0.18f, 0.0f, 1.0f);
    glm::vec4 mat_metal(0.88f, 0.20f, 0.0f, 1.0f);
    glm::vec4 mat_brass(0.85f, 0.24f, 0.0f, 0.95f);
    glm::vec4 mat_chrome(0.96f, 0.08f, 0.0f, 1.0f);
    glm::vec4 mat_rubber(0.04f, 0.74f, 0.0f, 0.80f);
    glm::vec4 mat_visor(0.12f, 0.05f, 1.45f, 1.0f);
    glm::vec4 mat_led(0.0f, 0.10f, 3.5f, 1.0f);


    // Glowing Visor Color
    glm::vec4 visor_color;
    if (cls == CharacterClass::Demolitionist) {
        visor_color = glm::vec4(1.0f, 0.64f, 0.12f, 1.0f); // Warm golden amber blast visor
    } else if (cls == CharacterClass::Vanguard) {
        visor_color = glm::vec4(0.20f, 0.88f, 1.0f, 1.0f);  // Heavy ballistic cyan slit visor
    } else {
        visor_color = glm::vec4(0.18f, 0.98f, 0.48f, 1.0f);  // Recon pathfinder emerald visor
    }

    // =========================================================
    // 1. TORSO, PECTORAL PLATING & TACTICAL HARNESS
    // =========================================================
    {
        // Core pressure suit inner chest
        glm::mat4 chest_m = glm::translate(base_model, glm::vec3(0.0f, 1.04f, 0.0f));
        add_stalker_box(s_delver_verts, chest_m, glm::vec3(0.22f, 0.20f, 0.15f), suit_color, mat_cloth);

        // Contoured Left and Right Pectoral Armor Plates
        glm::mat4 pect_l = glm::translate(chest_m, glm::vec3(-0.10f, 0.04f, 0.13f));
        add_stalker_box(s_delver_verts, pect_l, glm::vec3(0.09f, 0.12f, 0.035f), accent_color, mat_armor);
        glm::mat4 pect_r = glm::translate(chest_m, glm::vec3(0.10f, 0.04f, 0.13f));
        add_stalker_box(s_delver_verts, pect_r, glm::vec3(0.09f, 0.12f, 0.035f), accent_color, mat_armor);

        // Center Sternum Telemetry Core & Life Support Display
        glm::mat4 stern_m = glm::translate(chest_m, glm::vec3(0.0f, 0.04f, 0.15f));
        add_stalker_box(s_delver_verts, stern_m, glm::vec3(0.05f, 0.11f, 0.025f), col_dark_chassis, mat_dark_frame);
        // Telemetry LED display bars
        glm::mat4 led1 = glm::translate(stern_m, glm::vec3(0.0f, 0.04f, 0.026f));
        add_stalker_box(s_delver_verts, led1, glm::vec3(0.032f, 0.008f, 0.004f), visor_color, mat_led);
        glm::mat4 led2 = glm::translate(stern_m, glm::vec3(0.0f, 0.015f, 0.026f));
        add_stalker_box(s_delver_verts, led2, glm::vec3(0.032f, 0.008f, 0.004f), visor_color, mat_led);

        // Heavy Tactical Shoulder Harness Yoke Straps
        glm::mat4 yoke_l = glm::translate(chest_m, glm::vec3(-0.14f, 0.14f, 0.04f));
        add_stalker_box(s_delver_verts, yoke_l, glm::vec3(0.045f, 0.06f, 0.12f), col_rubber, mat_rubber);
        glm::mat4 yoke_r = glm::translate(chest_m, glm::vec3(0.14f, 0.14f, 0.04f));
        add_stalker_box(s_delver_verts, yoke_r, glm::vec3(0.045f, 0.06f, 0.12f), col_rubber, mat_rubber);
        // Alloy Harness Buckles
        glm::mat4 bkl_l = glm::translate(yoke_l, glm::vec3(0.0f, -0.03f, 0.11f));
        add_stalker_box(s_delver_verts, bkl_l, glm::vec3(0.025f, 0.02f, 0.015f), col_steel, mat_chrome);
        glm::mat4 bkl_r = glm::translate(yoke_r, glm::vec3(0.0f, -0.03f, 0.11f));
        add_stalker_box(s_delver_verts, bkl_r, glm::vec3(0.025f, 0.02f, 0.015f), col_steel, mat_chrome);

        // Class-Specific Chest Attachments
        if (cls == CharacterClass::Demolitionist) {
            // Blast Deflector Collar flaring out around neck
            glm::mat4 collar_m = glm::translate(chest_m, glm::vec3(0.0f, 0.18f, 0.08f));
            add_stalker_box(s_delver_verts, collar_m, glm::vec3(0.18f, 0.04f, 0.08f), accent_color, mat_armor);
            // Twin Ordnance Satchel Canisters mounted on chest
            add_stalker_cylinder(s_delver_verts, chest_m, glm::vec3(-0.16f, -0.04f, 0.15f), 0.026f, 0.08f, 10, col_dark_chassis, mat_dark_frame, 1);
            add_stalker_cylinder(s_delver_verts, chest_m, glm::vec3(-0.16f, -0.04f, 0.15f), 0.028f, 0.012f, 10, col_hazard_yellow, mat_armor, 1);
            add_stalker_cylinder(s_delver_verts, chest_m, glm::vec3(0.16f, -0.04f, 0.15f), 0.026f, 0.08f, 10, col_dark_chassis, mat_dark_frame, 1);
            add_stalker_cylinder(s_delver_verts, chest_m, glm::vec3(0.16f, -0.04f, 0.15f), 0.028f, 0.012f, 10, col_hazard_yellow, mat_armor, 1);
            // Caution hazard chevron diagonal stripes on right breast
            add_stalker_box(s_delver_verts, glm::translate(pect_r, glm::vec3(0.0f, 0.04f, 0.036f)), glm::vec3(0.07f, 0.015f, 0.005f), col_hazard_yellow, mat_armor);
            add_stalker_box(s_delver_verts, glm::translate(pect_r, glm::vec3(0.0f, 0.00f, 0.036f)), glm::vec3(0.07f, 0.015f, 0.005f), col_hazard_dark, mat_armor);
        } else if (cls == CharacterClass::Vanguard) {
            // Reinforced Interlocking Tectonic Bulkhead Trauma Slabs
            glm::mat4 slab_m = glm::translate(chest_m, glm::vec3(0.0f, 0.06f, 0.16f));
            add_stalker_box(s_delver_verts, slab_m, glm::vec3(0.24f, 0.14f, 0.03f), col_dark_chassis, mat_dark_frame);
            // Center Heavy Kinetic Absorption Power Core
            add_stalker_cylinder(s_delver_verts, chest_m, glm::vec3(0.0f, 0.05f, 0.18f), 0.045f, 0.025f, 12, accent_color, mat_armor, 2);
            add_stalker_cylinder(s_delver_verts, chest_m, glm::vec3(0.0f, 0.05f, 0.20f), 0.025f, 0.010f, 12, visor_color, mat_led, 2);
            // Heavy Torso Rivet Studs
            for (int r = -1; r <= 1; r += 2) {
                add_stalker_box(s_delver_verts, glm::translate(chest_m, glm::vec3(r * 0.18f, 0.12f, 0.18f)), glm::vec3(0.015f, 0.015f, 0.015f), col_steel, mat_chrome);
                add_stalker_box(s_delver_verts, glm::translate(chest_m, glm::vec3(r * 0.18f, -0.02f, 0.18f)), glm::vec3(0.015f, 0.015f, 0.015f), col_steel, mat_chrome);
            }
        } else {
            // Scout: Sleek Aerodynamic Carbon-Composite Harness & Sonar Transceiver
            glm::mat4 dish_m = glm::translate(chest_m, glm::vec3(0.0f, 0.05f, 0.16f));
            add_stalker_cylinder(s_delver_verts, dish_m, glm::vec3(0.0f), 0.042f, 0.018f, 14, col_dark_chassis, mat_dark_frame, 2);
            add_stalker_cylinder(s_delver_verts, dish_m, glm::vec3(0.0f, 0.0f, 0.015f), 0.025f, 0.012f, 12, visor_color, mat_led, 2);
        }

        // Abdominal Flexible Flex-Suit (3-tier ribbed corrugated seals)
        for (int rib = 0; rib < 3; ++rib) {
            float ry = 0.88f - rib * 0.045f;
            glm::mat4 rib_m = glm::translate(base_model, glm::vec3(0.0f, ry, 0.0f));
            float r_scale = 1.0f - rib * 0.04f;
            add_stalker_box(s_delver_verts, rib_m, glm::vec3(0.19f * r_scale, 0.018f, 0.13f * r_scale), col_rubber, mat_rubber);
        }

        // Heavy Tactical Utility Belt
        glm::mat4 belt_m = glm::translate(base_model, glm::vec3(0.0f, 0.73f, 0.0f));
        add_stalker_box(s_delver_verts, belt_m, glm::vec3(0.21f, 0.045f, 0.15f), glove_color, mat_rubber);
        // Heavy Industrial Steel Belt Buckle
        glm::mat4 bkl_m = glm::translate(belt_m, glm::vec3(0.0f, 0.0f, 0.155f));
        add_stalker_box(s_delver_verts, bkl_m, glm::vec3(0.045f, 0.038f, 0.015f), col_steel, mat_chrome);

        // Modular Belt Pouches (Left, Right, Rear)
        glm::mat4 pch_l = glm::translate(belt_m, glm::vec3(-0.22f, -0.01f, 0.02f));
        add_stalker_box(s_delver_verts, pch_l, glm::vec3(0.035f, 0.06f, 0.09f), accent_color, mat_armor);
        glm::mat4 pch_r = glm::translate(belt_m, glm::vec3(0.22f, -0.01f, 0.02f));
        add_stalker_box(s_delver_verts, pch_r, glm::vec3(0.035f, 0.06f, 0.09f), accent_color, mat_armor);
        glm::mat4 pch_b = glm::translate(belt_m, glm::vec3(0.0f, -0.01f, -0.155f));
        add_stalker_box(s_delver_verts, pch_b, glm::vec3(0.12f, 0.06f, 0.035f), col_dark_chassis, mat_dark_frame);
    }

    // =========================================================
    // 2. HELMET, VISOR & RESPIRATOR HEADGEAR
    // =========================================================
    {
        // Neck Collar Pressure Gasket Ring
        glm::mat4 neck_m = glm::translate(base_model, glm::vec3(0.0f, 1.28f, 0.0f));
        add_stalker_cylinder(s_delver_verts, neck_m, glm::vec3(0.0f), 0.15f, 0.05f, 16, col_dark_chassis, mat_dark_frame, 1);

        // EVA Helmet Shell Center
        glm::mat4 helm_m = glm::translate(base_model, glm::vec3(0.0f, 1.48f, 0.0f));
        // Main cranial composite dome
        add_stalker_box(s_delver_verts, helm_m, glm::vec3(0.18f, 0.16f, 0.18f), helmet_shell_color, mat_armor);

        // Angled Brow Shield & Cheek Armor Plates
        glm::mat4 brow_m = glm::translate(helm_m, glm::vec3(0.0f, 0.14f, 0.06f));
        add_stalker_box(s_delver_verts, brow_m, glm::vec3(0.19f, 0.035f, 0.14f), col_dark_chassis, mat_dark_frame);
        glm::mat4 cheek_l = glm::translate(helm_m, glm::vec3(-0.17f, -0.04f, 0.08f));
        add_stalker_box(s_delver_verts, cheek_l, glm::vec3(0.03f, 0.10f, 0.08f), col_dark_chassis, mat_dark_frame);
        glm::mat4 cheek_r = glm::translate(helm_m, glm::vec3(0.17f, -0.04f, 0.08f));
        add_stalker_box(s_delver_verts, cheek_r, glm::vec3(0.03f, 0.10f, 0.08f), col_dark_chassis, mat_dark_frame);

        // Central Crown Ridge Fin
        glm::mat4 crown_m = glm::translate(helm_m, glm::vec3(0.0f, 0.175f, 0.0f));
        add_stalker_box(s_delver_verts, crown_m, glm::vec3(0.045f, 0.025f, 0.16f), accent_color, mat_armor);

        // Visor: Forward Glowing Optical Faceplate with Recessed Frame
        float visor_h = (cls == CharacterClass::Vanguard) ? 0.065f : 0.115f;
        float visor_w = (cls == CharacterClass::Vanguard) ? 0.13f : 0.145f;
        glm::mat4 visor_frame = glm::translate(helm_m, glm::vec3(0.0f, 0.01f, 0.165f));
        add_stalker_box(s_delver_verts, visor_frame, glm::vec3(visor_w + 0.02f, visor_h + 0.02f, 0.025f), col_rubber, mat_rubber);
        // Optical Visor Glass (Clean, non-blown-out glow!)
        glm::mat4 visor_glass = glm::translate(visor_frame, glm::vec3(0.0f, 0.0f, 0.02f));
        add_stalker_box(s_delver_verts, visor_glass, glm::vec3(visor_w, visor_h, 0.015f), visor_color, mat_visor);
        // Inner HUD scanner reticle bar
        glm::mat4 hud_bar = glm::translate(visor_glass, glm::vec3(0.0f, 0.0f, 0.012f));
        add_stalker_box(s_delver_verts, hud_bar, glm::vec3(visor_w * 0.7f, 0.008f, 0.005f), visor_color * 1.2f, mat_led);

        // Temple Searchlight Pods (Left & Right)
        add_stalker_cylinder(s_delver_verts, helm_m, glm::vec3(-0.19f, 0.06f, 0.08f), 0.022f, 0.055f, 12, col_steel_dark, mat_dark_frame, 2);
        add_stalker_cylinder(s_delver_verts, helm_m, glm::vec3(-0.19f, 0.06f, 0.13f), 0.018f, 0.010f, 10, glm::vec4(1.0f, 0.96f, 0.85f, 1.0f), mat_led, 2);
        add_stalker_cylinder(s_delver_verts, helm_m, glm::vec3(0.19f, 0.06f, 0.08f), 0.022f, 0.055f, 12, col_steel_dark, mat_dark_frame, 2);
        add_stalker_cylinder(s_delver_verts, helm_m, glm::vec3(0.19f, 0.06f, 0.13f), 0.018f, 0.010f, 10, glm::vec4(1.0f, 0.96f, 0.85f, 1.0f), mat_led, 2);

        // Dual Rebreather Respirator Filter Canisters on Lower Jaw
        add_stalker_cylinder(s_delver_verts, helm_m, glm::vec3(-0.11f, -0.11f, 0.14f), 0.026f, 0.06f, 10, col_dark_chassis, mat_dark_frame, 2);
        add_stalker_cylinder(s_delver_verts, helm_m, glm::vec3(-0.11f, -0.11f, 0.19f), 0.024f, 0.012f, 10, col_steel, mat_chrome, 2);
        add_stalker_cylinder(s_delver_verts, helm_m, glm::vec3(0.11f, -0.11f, 0.14f), 0.026f, 0.06f, 10, col_dark_chassis, mat_dark_frame, 2);
        add_stalker_cylinder(s_delver_verts, helm_m, glm::vec3(0.11f, -0.11f, 0.19f), 0.024f, 0.012f, 10, col_steel, mat_chrome, 2);
        // Chin Exhalation Grille
        glm::mat4 chin_m = glm::translate(helm_m, glm::vec3(0.0f, -0.11f, 0.17f));
        add_stalker_box(s_delver_verts, chin_m, glm::vec3(0.045f, 0.025f, 0.025f), col_steel_dark, mat_steel);
    }

    // =========================================================
    // 3. BACKPACK LIFE SUPPORT & THRUSTER UNIT (EXO-PACK)
    // =========================================================
    {
        glm::mat4 pack_m = glm::translate(base_model, glm::vec3(0.0f, 1.06f, -0.21f));
        // Main Pack Chassis
        add_stalker_box(s_delver_verts, pack_m, glm::vec3(0.18f, 0.24f, 0.08f), col_dark_chassis, mat_dark_frame);

        // Top Heat Dissipation Radiator Fins
        for (int fin = 0; fin < 3; ++fin) {
            float fz = -0.04f + fin * 0.035f;
            glm::mat4 fin_m = glm::translate(pack_m, glm::vec3(0.0f, 0.25f, fz));
            add_stalker_box(s_delver_verts, fin_m, glm::vec3(0.14f, 0.015f, 0.008f), col_steel, mat_steel);
        }

        // Dual High-Pressure Breathing / Propellant Cylinders
        add_stalker_cylinder(s_delver_verts, pack_m, glm::vec3(-0.09f, -0.12f, -0.06f), 0.042f, 0.28f, 12, col_steel_dark, mat_armor, 1);
        add_stalker_cylinder(s_delver_verts, pack_m, glm::vec3(-0.09f, 0.16f, -0.06f), 0.024f, 0.025f, 10, col_brass, mat_brass, 1);
        add_stalker_cylinder(s_delver_verts, pack_m, glm::vec3(0.09f, -0.12f, -0.06f), 0.042f, 0.28f, 12, col_steel_dark, mat_armor, 1);
        add_stalker_cylinder(s_delver_verts, pack_m, glm::vec3(0.09f, 0.16f, -0.06f), 0.024f, 0.025f, 10, col_brass, mat_brass, 1);

        // Lower Vector Thruster Bell Nozzles
        glm::mat4 noz_l = glm::translate(pack_m, glm::vec3(-0.13f, -0.25f, -0.02f));
        noz_l = glm::rotate(noz_l, 0.22f, glm::vec3(1.0f, 0.0f, 0.0f));
        add_stalker_cone(s_delver_verts, noz_l, glm::vec3(0.0f), 0.036f, 0.052f, 0.07f, 12, accent_color, mat_armor);
        add_stalker_cylinder(s_delver_verts, noz_l, glm::vec3(0.0f, 0.0f, -0.065f), 0.028f, 0.012f, 10, visor_color, mat_led, 2);

        glm::mat4 noz_r = glm::translate(pack_m, glm::vec3(0.13f, -0.25f, -0.02f));
        noz_r = glm::rotate(noz_r, 0.22f, glm::vec3(1.0f, 0.0f, 0.0f));
        add_stalker_cone(s_delver_verts, noz_r, glm::vec3(0.0f), 0.036f, 0.052f, 0.07f, 12, accent_color, mat_armor);
        add_stalker_cylinder(s_delver_verts, noz_r, glm::vec3(0.0f, 0.0f, -0.065f), 0.028f, 0.012f, 10, visor_color, mat_led, 2);

        // Communications & Sonar Mast Antenna on Right Shoulder
        glm::mat4 ant_m = glm::translate(pack_m, glm::vec3(0.15f, 0.26f, -0.04f));
        add_stalker_cylinder(s_delver_verts, ant_m, glm::vec3(0.0f), 0.009f, 0.22f, 8, col_steel, mat_chrome, 1);
        add_stalker_cylinder(s_delver_verts, ant_m, glm::vec3(0.0f, 0.22f, 0.0f), 0.014f, 0.018f, 8, visor_color, mat_led, 1);
    }

    // =========================================================
    // 4. PAULDRONS, ARMS, GAUNTLETS & HELD EQUIPMENT
    // =========================================================
    {
        // Armored Pauldrons (Shoulders)
        float pauld_w = (cls == CharacterClass::Vanguard) ? 0.17f : (cls == CharacterClass::Demolitionist ? 0.14f : 0.11f);
        float pauld_h = (cls == CharacterClass::Vanguard) ? 0.15f : 0.11f;

        // Left Pauldron
        glm::mat4 sh_l = glm::translate(base_model, glm::vec3(-0.31f, 1.20f, 0.01f));
        sh_l = glm::rotate(sh_l, 0.16f, glm::vec3(0.0f, 0.0f, 1.0f));
        add_stalker_box(s_delver_verts, sh_l, glm::vec3(pauld_w, pauld_h, 0.13f), accent_color, mat_armor);
        add_stalker_box(s_delver_verts, glm::translate(sh_l, glm::vec3(0.0f, -0.06f, 0.0f)), glm::vec3(pauld_w * 0.85f, 0.035f, 0.11f), col_dark_chassis, mat_dark_frame);

        // Right Pauldron
        glm::mat4 sh_r = glm::translate(base_model, glm::vec3(0.31f, 1.20f, 0.01f));
        sh_r = glm::rotate(sh_r, -0.16f, glm::vec3(0.0f, 0.0f, 1.0f));
        add_stalker_box(s_delver_verts, sh_r, glm::vec3(pauld_w, pauld_h, 0.13f), accent_color, mat_armor);
        add_stalker_box(s_delver_verts, glm::translate(sh_r, glm::vec3(0.0f, -0.06f, 0.0f)), glm::vec3(pauld_w * 0.85f, 0.035f, 0.11f), col_dark_chassis, mat_dark_frame);

        // Left Arm (Tactical Support / Stabilizing Stance)
        glm::vec3 l_elbow(-0.33f, 0.94f, 0.02f);
        glm::vec3 l_wrist(-0.24f, 0.74f, 0.22f);
        glm::vec3 l_palm(-0.16f, 0.72f, 0.32f);

        add_stalker_capsule(s_delver_verts, base_model, glm::vec3(-0.29f, 1.15f, 0.01f), l_elbow, 0.065f, 0.052f, 10, suit_color, mat_cloth);
        // Left Elbow Armor Guard
        add_stalker_box(s_delver_verts, glm::translate(base_model, l_elbow + glm::vec3(-0.015f, 0.0f, -0.02f)), glm::vec3(0.045f, 0.045f, 0.045f), col_dark_chassis, mat_dark_frame);
        // Left Forearm Gauntlet & Wrist PDA Terminal
        add_stalker_capsule(s_delver_verts, base_model, l_elbow, l_wrist, 0.052f, 0.044f, 10, col_dark_chassis, mat_dark_frame);
        // Wrist PDA Tactical Display Screen (Facing player)
        glm::mat4 pda_m = glm::translate(base_model, l_wrist + glm::vec3(0.01f, 0.04f, 0.02f));
        add_stalker_box(s_delver_verts, pda_m, glm::vec3(0.035f, 0.015f, 0.045f), col_steel_dark, mat_dark_frame);
        add_stalker_box(s_delver_verts, glm::translate(pda_m, glm::vec3(0.0f, 0.016f, 0.0f)), glm::vec3(0.028f, 0.005f, 0.038f), visor_color, mat_led);
        // Left Work Glove & Clamping Fingers
        add_stalker_capsule(s_delver_verts, base_model, l_wrist, l_palm, 0.042f, 0.036f, 8, glove_color, mat_rubber);
        // 4 Clamping fingers wrapping around tool stabilizer
        for (int f = 0; f < 4; ++f) {
            float f_offset = -0.03f + f * 0.018f;
            add_stalker_capsule(s_delver_verts, base_model, l_palm + glm::vec3(f_offset, 0.01f, 0.0f), l_palm + glm::vec3(f_offset, -0.035f, 0.06f), 0.012f, 0.010f, 6, glove_color, mat_rubber);
        }

        // Right Arm (Forward Holding Equipment)
        glm::vec3 r_elbow(0.33f, 0.96f, 0.08f);
        glm::vec3 r_wrist(0.22f, 0.78f, 0.30f);
        glm::vec3 r_palm(0.12f, 0.76f, 0.38f);

        add_stalker_capsule(s_delver_verts, base_model, glm::vec3(0.29f, 1.15f, 0.01f), r_elbow, 0.065f, 0.052f, 10, suit_color, mat_cloth);
        // Right Elbow Armor Guard
        add_stalker_box(s_delver_verts, glm::translate(base_model, r_elbow + glm::vec3(0.015f, 0.0f, -0.02f)), glm::vec3(0.045f, 0.045f, 0.045f), col_dark_chassis, mat_dark_frame);
        // Right Forearm Gauntlet & Grip
        add_stalker_capsule(s_delver_verts, base_model, r_elbow, r_wrist, 0.052f, 0.044f, 10, col_dark_chassis, mat_dark_frame);
        add_stalker_capsule(s_delver_verts, base_model, r_wrist, r_palm, 0.042f, 0.036f, 8, glove_color, mat_rubber);
        // Knuckle Impact Guard
        add_stalker_box(s_delver_verts, glm::translate(base_model, r_palm + glm::vec3(0.0f, 0.025f, 0.01f)), glm::vec3(0.038f, 0.014f, 0.025f), accent_color, mat_armor);

        // =========================================================
        // HELD CLASS EQUIPMENT (High-Detail 3D Industrial Rig)
        // =========================================================
        glm::mat4 tool_m = glm::translate(base_model, glm::vec3(0.0f, 0.75f, 0.44f));

        if (cls == CharacterClass::Demolitionist) {
            // Heavy Rotary Breacher Mining Drill
            // Heavy Engine Chassis
            add_stalker_box(s_delver_verts, tool_m, glm::vec3(0.075f, 0.085f, 0.16f), col_dark_chassis, mat_dark_frame);
            // Engine Top Cowling & Hazard Stripes
            glm::mat4 cowl_m = glm::translate(tool_m, glm::vec3(0.0f, 0.085f, 0.0f));
            add_stalker_box(s_delver_verts, cowl_m, glm::vec3(0.08f, 0.025f, 0.14f), accent_color, mat_armor);
            add_stalker_box(s_delver_verts, cowl_m, glm::vec3(0.07f, 0.027f, 0.05f), col_hazard_yellow, mat_armor);
            // Left Pneumatic Pressure Canister
            add_stalker_cylinder(s_delver_verts, tool_m, glm::vec3(-0.09f, -0.02f, 0.08f), 0.032f, 0.16f, 12, col_steel_dark, mat_armor, 2);
            add_stalker_cylinder(s_delver_verts, tool_m, glm::vec3(-0.09f, -0.02f, 0.10f), 0.018f, 0.020f, 10, col_brass, mat_brass, 2);
            // Transmission Bell Housing & Rotary Chuck Collar
            add_stalker_cone(s_delver_verts, tool_m, glm::vec3(0.0f, 0.0f, 0.16f), 0.075f, 0.065f, 0.06f, 14, col_dark_chassis, mat_dark_frame);
            add_stalker_cylinder(s_delver_verts, tool_m, glm::vec3(0.0f, 0.0f, 0.22f), 0.065f, 0.07f, 14, col_steel, mat_steel, 2);
            // 4 Steel Chuck Locking Jaws
            for (int j = 0; j < 4; ++j) {
                float ja = j * 1.5707963f;
                glm::mat4 j_m = glm::translate(tool_m, glm::vec3(0.0f, 0.0f, 0.25f));
                j_m = glm::rotate(j_m, ja, glm::vec3(0.0f, 0.0f, 1.0f));
                j_m = glm::translate(j_m, glm::vec3(0.062f, 0.0f, 0.0f));
                add_stalker_box(s_delver_verts, j_m, glm::vec3(0.012f, 0.016f, 0.025f), col_steel, mat_chrome);
            }
            // Spinning Multi-Fluted Tungsten Drill Borer Bit!
            glm::mat4 bit_m = glm::translate(tool_m, glm::vec3(0.0f, 0.0f, 0.29f));
            bit_m = glm::rotate(bit_m, anim_time * 12.0f, glm::vec3(0.0f, 0.0f, 1.0f));
            add_stalker_cone(s_delver_verts, bit_m, glm::vec3(0.0f), 0.055f, 0.016f, 0.28f, 14, col_steel, mat_steel);
            // High-wear carbide cutting teeth flutes
            for (int cf = 0; cf < 3; ++cf) {
                float ca = cf * 2.0943951f;
                glm::mat4 flute_m = glm::rotate(bit_m, ca, glm::vec3(0.0f, 0.0f, 1.0f));
                add_stalker_cone(s_delver_verts, flute_m, glm::vec3(0.038f, 0.0f, 0.05f), 0.018f, 0.008f, 0.20f, 8, col_brass, mat_metal);
            }
        } else if (cls == CharacterClass::Vanguard) {
            // Heavy Fortified Tectonic Bastion Drill
            add_stalker_box(s_delver_verts, tool_m, glm::vec3(0.09f, 0.095f, 0.18f), col_dark_chassis, mat_dark_frame);
            // Lateral Deflector Armor Shields
            add_stalker_box(s_delver_verts, glm::translate(tool_m, glm::vec3(-0.11f, 0.0f, 0.02f)), glm::vec3(0.02f, 0.08f, 0.15f), accent_color, mat_armor);
            add_stalker_box(s_delver_verts, glm::translate(tool_m, glm::vec3(0.11f, 0.0f, 0.02f)), glm::vec3(0.02f, 0.08f, 0.15f), accent_color, mat_armor);
            // Heavy Hydraulic Piston Cylinders
            add_stalker_cylinder(s_delver_verts, tool_m, glm::vec3(0.0f, 0.085f, 0.04f), 0.025f, 0.18f, 10, col_steel, mat_chrome, 2);
            // Quad Diamond-Tipped Boring Head
            glm::mat4 bit_m = glm::translate(tool_m, glm::vec3(0.0f, 0.0f, 0.26f));
            bit_m = glm::rotate(bit_m, anim_time * 10.0f, glm::vec3(0.0f, 0.0f, 1.0f));
            add_stalker_cone(s_delver_verts, bit_m, glm::vec3(0.0f), 0.065f, 0.024f, 0.24f, 12, col_steel, mat_steel);
        } else {
            // Scout: Sleek High-RPM Ultrasonic Acoustic Pulse Drill
            add_stalker_box(s_delver_verts, tool_m, glm::vec3(0.06f, 0.065f, 0.18f), col_dark_chassis, mat_dark_frame);
            // Integrated Sonar Transceiver Dish on side
            add_stalker_cylinder(s_delver_verts, tool_m, glm::vec3(-0.08f, 0.02f, 0.06f), 0.038f, 0.016f, 12, accent_color, mat_armor, 0);
            add_stalker_cylinder(s_delver_verts, tool_m, glm::vec3(-0.09f, 0.02f, 0.06f), 0.020f, 0.010f, 10, visor_color, mat_led, 0);
            // Precision High-RPM Spindle
            glm::mat4 bit_m = glm::translate(tool_m, glm::vec3(0.0f, 0.0f, 0.22f));
            bit_m = glm::rotate(bit_m, anim_time * 24.0f, glm::vec3(0.0f, 0.0f, 1.0f));
            add_stalker_cone(s_delver_verts, bit_m, glm::vec3(0.0f), 0.045f, 0.008f, 0.32f, 12, col_steel, mat_chrome);
            // Laser Guide Pointer Beam Diode
            add_stalker_cylinder(s_delver_verts, tool_m, glm::vec3(0.0f, 0.065f, 0.14f), 0.010f, 0.04f, 8, visor_color, mat_led, 2);
        }
    }

    // =========================================================
    // 5. PELVIS, LEGS, ARTICULATED KNEES & MAGNETIC BOOTS
    // =========================================================
    {
        // Armored Pelvis & Groin Plate
        glm::mat4 pelv_m = glm::translate(base_model, glm::vec3(0.0f, 0.64f, 0.0f));
        add_stalker_box(s_delver_verts, pelv_m, glm::vec3(0.18f, 0.06f, 0.14f), col_dark_chassis, mat_dark_frame);
        add_stalker_box(s_delver_verts, glm::translate(pelv_m, glm::vec3(0.0f, -0.04f, 0.09f)), glm::vec3(0.08f, 0.07f, 0.035f), accent_color, mat_armor);

        // Left & Right Thighs (Pressurized suit + Exoskeleton struts)
        for (int side = -1; side <= 1; side += 2) {
            float tx = side * 0.14f;
            glm::vec3 th_top(tx, 0.62f, 0.01f);
            glm::vec3 th_bot(tx, 0.38f, 0.02f);
            add_stalker_capsule(s_delver_verts, base_model, th_top, th_bot, 0.082f, 0.068f, 10, suit_color, mat_cloth);

            // Outer Mechanical Exoskeleton Thigh Strut
            glm::mat4 exo_th = glm::translate(base_model, glm::vec3(tx + side * 0.08f, 0.50f, 0.01f));
            add_stalker_box(s_delver_verts, exo_th, glm::vec3(0.018f, 0.10f, 0.035f), col_dark_chassis, mat_dark_frame);

            // Right Thigh Modular Equipment Holster
            if (side == 1) {
                glm::mat4 hols_m = glm::translate(base_model, glm::vec3(tx + 0.085f, 0.50f, 0.02f));
                add_stalker_box(s_delver_verts, hols_m, glm::vec3(0.025f, 0.08f, 0.06f), accent_color, mat_armor);
                add_stalker_cylinder(s_delver_verts, hols_m, glm::vec3(0.01f, 0.02f, 0.0f), 0.018f, 0.08f, 8, visor_color, mat_led, 1);
            }

            // Articulated Heavy Knee Guard with Impact Rivets
            glm::mat4 knee_m = glm::translate(base_model, glm::vec3(tx, 0.34f, 0.085f));
            add_stalker_box(s_delver_verts, knee_m, glm::vec3(0.072f, 0.065f, 0.045f), accent_color, mat_armor);
            add_stalker_box(s_delver_verts, glm::translate(knee_m, glm::vec3(0.0f, 0.02f, 0.025f)), glm::vec3(0.055f, 0.025f, 0.015f), col_steel, mat_chrome);

            // Front Shin Armor Plates
            glm::vec3 shin_top(tx, 0.30f, 0.03f);
            glm::vec3 shin_bot(tx, 0.12f, 0.03f);
            add_stalker_capsule(s_delver_verts, base_model, shin_top, shin_bot, 0.068f, 0.058f, 10, col_dark_chassis, mat_dark_frame);
            // Contoured Shin Strike Plate
            glm::mat4 shin_plate = glm::translate(base_model, glm::vec3(tx, 0.22f, 0.082f));
            add_stalker_box(s_delver_verts, shin_plate, glm::vec3(0.062f, 0.09f, 0.025f), accent_color, mat_armor);

            // Rear Calf Hydraulic Suspension Cylinder
            add_stalker_cylinder(s_delver_verts, base_model, glm::vec3(tx, 0.14f, -0.05f), 0.014f, 0.14f, 8, col_steel, mat_chrome, 1);

            // Heavy Industrial Magnetic Lock Boots
            glm::mat4 boot_m = glm::translate(base_model, glm::vec3(tx, 0.06f, 0.04f));
            // Main Boot Body
            add_stalker_box(s_delver_verts, boot_m, glm::vec3(0.082f, 0.055f, 0.14f), glove_color, mat_rubber);
            // Reinforced Steel Toe Cap
            glm::mat4 toe_m = glm::translate(boot_m, glm::vec3(0.0f, -0.01f, 0.12f));
            add_stalker_box(s_delver_verts, toe_m, glm::vec3(0.084f, 0.045f, 0.045f), col_steel, mat_steel);
            // Deep Tread Sole Lugs
            glm::mat4 sole_m = glm::translate(boot_m, glm::vec3(0.0f, -0.052f, 0.01f));
            add_stalker_box(s_delver_verts, sole_m, glm::vec3(0.086f, 0.018f, 0.155f), col_rubber, mat_rubber);
            // Rear Heel Mag-Lock Status Indicator LED
            glm::mat4 mag_led = glm::translate(boot_m, glm::vec3(0.0f, 0.0f, -0.145f));
            add_stalker_box(s_delver_verts, mag_led, glm::vec3(0.035f, 0.015f, 0.008f), visor_color, mat_led);
        }
    }

    // =========================================================
    // 6. DRAW 3D PBR DELVER MESH (USING DEDICATED uState == 10)
    // =========================================================
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
        m_stalker_shader.set_int("uState", 10); // Delver Contractor Character Mode!

        ensure_stalker_vbo_capacity(s_delver_verts.size());
        glBindVertexArray(m_stalker_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
        GLsizeiptr byte_size = static_cast<GLsizeiptr>(s_delver_verts.size() * sizeof(StalkerVertex));
        glBufferSubData(GL_ARRAY_BUFFER, 0, byte_size, s_delver_verts.data());

        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_delver_verts.size()));
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

    // Airlock Door Hatch: sealed blast door when ramp is closed, slides up into header when ramp extends
    if (ramp_extension < 0.95f) {
        float door_open_ratio = glm::clamp(ramp_extension, 0.0f, 1.0f);
        float door_lift = door_open_ratio * (hull_top - hull_bottom - 0.70f);
        glm::mat4 door_m = glm::translate(base_model, glm::vec3(0.0f, hull_mid_y + door_lift, hull_radius * 0.92f));
        add_stalker_box(s_pod_verts, door_m, glm::vec3(0.70f, (hull_top - hull_bottom) * 0.45f, 0.08f), col_hazard_orange, mat_armor);
        // Heavy steel locking bars across blast door
        glm::mat4 bar_m = glm::translate(door_m, glm::vec3(0.0f, 0.0f, 0.06f));
        add_stalker_box(s_pod_verts, bar_m, glm::vec3(0.65f, 0.10f, 0.03f), col_steel, mat_metal);
    }

    // Interior pressurized cabin chamber (solid floor, ceiling, and bulkheads)
    glm::mat4 cabin_floor = glm::translate(base_model, glm::vec3(0.0f, hull_bottom + 0.08f, 0.0f));
    add_stalker_box(s_pod_verts, cabin_floor, glm::vec3(0.95f, 0.08f, 0.95f), col_steel, mat_armor);
    glm::mat4 cabin_ceil = glm::translate(base_model, glm::vec3(0.0f, hull_top - 0.20f, 0.0f));
    add_stalker_box(s_pod_verts, cabin_ceil, glm::vec3(0.95f, 0.10f, 0.95f), col_hull, mat_armor);
    glm::mat4 cabin_back = glm::translate(base_model, glm::vec3(0.0f, hull_mid_y, -0.95f));
    add_stalker_box(s_pod_verts, cabin_back, glm::vec3(0.95f, (hull_top - hull_bottom) * 0.45f, 0.10f), col_inner_hull, mat_armor);
    glm::mat4 cabin_left = glm::translate(base_model, glm::vec3(-0.95f, hull_mid_y, 0.0f));
    add_stalker_box(s_pod_verts, cabin_left, glm::vec3(0.10f, (hull_top - hull_bottom) * 0.45f, 0.95f), col_inner_hull, mat_armor);
    glm::mat4 cabin_right = glm::translate(base_model, glm::vec3(0.95f, hull_mid_y, 0.0f));
    add_stalker_box(s_pod_verts, cabin_right, glm::vec3(0.10f, (hull_top - hull_bottom) * 0.45f, 0.95f), col_inner_hull, mat_armor);

    // Interior glowing status console mounted on back bulkhead
    glm::mat4 console_m = glm::translate(base_model, glm::vec3(0.0f, hull_mid_y, -0.85f));
    add_stalker_box(s_pod_verts, console_m, glm::vec3(0.50f, 0.35f, 0.06f), col_cabin_glow, mat_cabin_led);
    // Interior overhead status illumination
    glm::mat4 light_l = glm::translate(base_model, glm::vec3(-0.75f, hull_top - 0.35f, 0.0f));
    add_stalker_box(s_pod_verts, light_l, glm::vec3(0.04f, 0.04f, 0.35f), col_cabin_glow, mat_cabin_led);
    glm::mat4 light_r = glm::translate(base_model, glm::vec3(0.75f, hull_top - 0.35f, 0.0f));
    add_stalker_box(s_pod_verts, light_r, glm::vec3(0.04f, 0.04f, 0.35f), col_cabin_glow, mat_cabin_led);

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

    ensure_stalker_vbo_capacity(s_pod_verts.size());
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, s_pod_verts.size() * sizeof(StalkerVertex), s_pod_verts.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_pod_verts.size()));
    glBindVertexArray(0);
}

void Renderer::render_precursor_relic(const glm::vec3& base_pos, float time) {
    if (m_stalker_vao == 0) return;

    // Frustum cull against the relic's bounding volume
    glm::vec3 bmin = base_pos - glm::vec3(1.2f, 0.2f, 1.2f);
    glm::vec3 bmax = base_pos + glm::vec3(1.2f, 1.6f, 1.2f);
    if (!is_box_in_frustum(bmin, bmax)) return;

    static std::vector<StalkerVertex> s_relic_verts;
    s_relic_verts.clear();
    if (s_relic_verts.capacity() < 4096) {
        s_relic_verts.reserve(4096);
    }

    static std::vector<float> s_relic_lines;
    s_relic_lines.clear();
    if (s_relic_lines.capacity() < 1024) {
        s_relic_lines.reserve(1024);
    }

    // Material definitions & color schemes
    const glm::vec4 col_base_alloy(0.18f, 0.17f, 0.22f, 1.0f);      // Ancient basalt alloy
    const glm::vec4 col_precursor_gold(0.96f, 0.78f, 0.24f, 1.0f);  // Gilded celestial precursor metal
    const glm::vec4 col_titanium_trim(0.38f, 0.44f, 0.52f, 1.0f);   // Subterranean titanium casing
    const glm::vec4 col_crystal_violet(0.78f, 0.28f, 1.0f, 1.0f);   // Resonant prismatic violet core
    const glm::vec4 col_crystal_kernel(0.98f, 0.94f, 0.72f, 1.0f);   // White-gold singularity heart
    const glm::vec4 col_cyan_glyph(0.15f, 0.95f, 1.0f, 1.0f);       // Radiant celestial cyan glyphs
    const glm::vec4 col_satellite(0.35f, 0.88f, 1.0f, 1.0f);        // Orbital satellite crystal shards

    const float pulse = 0.5f + 0.5f * std::sin(time * 3.2f);
    const float pulse_emissive = 1.6f + 0.9f * pulse;
    const float pulse_high = 2.4f + 1.2f * pulse;

    const glm::vec4 mat_base(0.70f, 0.35f, 0.0f, 0.85f);
    const glm::vec4 mat_gold(0.92f, 0.18f, 0.0f, 0.95f);
    const glm::vec4 mat_titanium(0.85f, 0.25f, 0.0f, 0.90f);
    const glm::vec4 mat_crystal(0.25f, 0.08f, pulse_emissive, 0.98f);
    const glm::vec4 mat_kernel(0.10f, 0.05f, 3.2f, 1.0f);
    const glm::vec4 mat_glyph_cyan(0.20f, 0.10f, pulse_high, 1.0f);
    const glm::vec4 mat_satellite(0.30f, 0.12f, pulse_high, 0.95f);

    auto push_tri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                        const glm::vec3& n, const glm::vec4& col, const glm::vec4& mat) {
        s_relic_verts.push_back({a, n, col, mat});
        s_relic_verts.push_back({b, n, col, mat});
        s_relic_verts.push_back({c, n, col, mat});
    };

    auto push_quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                         const glm::vec3& n, const glm::vec4& col, const glm::vec4& mat) {
        push_tri(a, b, c, n, col, mat);
        push_tri(a, c, d, n, col, mat);
    };

    auto add_box_transformed = [&](const glm::mat4& transform, const glm::vec3& half_extents,
                                   const glm::vec4& col, const glm::vec4& mat) {
        add_stalker_box(s_relic_verts, transform, half_extents, col, mat);
    };

    // ─────────────────────────────────────────────────────────────
    // 1. Pedestal Dais Socket (Octagonal Foundation Collar)
    // ─────────────────────────────────────────────────────────────
    constexpr int OCT_SEGS = 8;
    float oct_cos[OCT_SEGS], oct_sin[OCT_SEGS];
    for (int i = 0; i < OCT_SEGS; ++i) {
        float a = i * (6.2831853f / OCT_SEGS);
        oct_cos[i] = std::cos(a);
        oct_sin[i] = std::sin(a);
    }

    const float r_collar_out = 0.38f;
    const float r_collar_in  = 0.26f;
    const float h_collar     = 0.065f;

    // Collar beveled ring & inner runic groove
    for (int i = 0; i < OCT_SEGS; ++i) {
        int next = (i + 1) % OCT_SEGS;
        glm::vec3 b0 = base_pos + glm::vec3(oct_cos[i] * r_collar_out, 0.0f, oct_sin[i] * r_collar_out);
        glm::vec3 b1 = base_pos + glm::vec3(oct_cos[next] * r_collar_out, 0.0f, oct_sin[next] * r_collar_out);
        glm::vec3 t0 = base_pos + glm::vec3(oct_cos[i] * r_collar_out, h_collar, oct_sin[i] * r_collar_out);
        glm::vec3 t1 = base_pos + glm::vec3(oct_cos[next] * r_collar_out, h_collar, oct_sin[next] * r_collar_out);
        glm::vec3 ti0 = base_pos + glm::vec3(oct_cos[i] * r_collar_in, h_collar, oct_sin[i] * r_collar_in);
        glm::vec3 ti1 = base_pos + glm::vec3(oct_cos[next] * r_collar_in, h_collar, oct_sin[next] * r_collar_in);

        // Outer beveled rim quad
        glm::vec3 n_rim = glm::normalize(glm::cross(b1 - b0, t0 - b0));
        push_quad(b0, b1, t1, t0, n_rim, col_base_alloy, mat_base);

        // Top deck quad
        push_quad(ti0, ti1, t1, t0, glm::vec3(0.0f, 1.0f, 0.0f), col_precursor_gold, mat_gold);

        // Sunken glowing rune conduit
        glm::vec3 bi0 = base_pos + glm::vec3(oct_cos[i] * r_collar_in, 0.015f, oct_sin[i] * r_collar_in);
        glm::vec3 bi1 = base_pos + glm::vec3(oct_cos[next] * r_collar_in, 0.015f, oct_sin[next] * r_collar_in);
        push_quad(bi0, bi1, ti1, ti0, glm::normalize(glm::vec3(-oct_cos[i], 0.0f, -oct_sin[i])), col_cyan_glyph, mat_glyph_cyan);
    }

    // ─────────────────────────────────────────────────────────────
    // 2. 4 Gilded Precursor Energy Claws / Focusing Prongs
    // ─────────────────────────────────────────────────────────────
    for (int i = 0; i < 4; ++i) {
        float angle = i * 1.5707963f; // 0, 90, 180, 270 deg
        glm::vec3 dir(std::cos(angle), 0.0f, std::sin(angle));
        glm::vec3 right(-std::sin(angle), 0.0f, std::cos(angle));

        // Base foot anchor on pedestal
        glm::vec3 foot_pos = base_pos + dir * 0.34f + glm::vec3(0.0f, 0.04f, 0.0f);
        glm::mat4 foot_m = glm::translate(glm::mat4(1.0f), foot_pos);
        foot_m = glm::rotate(foot_m, -angle, glm::vec3(0.0f, 1.0f, 0.0f));
        add_box_transformed(foot_m, glm::vec3(0.045f, 0.035f, 0.055f), col_base_alloy, mat_base);

        // Curved upward strut reaching toward the levitating relic
        glm::vec3 mid_pos = base_pos + dir * 0.30f + glm::vec3(0.0f, 0.15f, 0.0f);
        glm::mat4 mid_m = glm::translate(glm::mat4(1.0f), mid_pos);
        mid_m = glm::rotate(mid_m, -angle, glm::vec3(0.0f, 1.0f, 0.0f));
        mid_m = glm::rotate(mid_m, glm::radians(22.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        add_box_transformed(mid_m, glm::vec3(0.032f, 0.09f, 0.032f), col_precursor_gold, mat_gold);

        // Inward pointing focus tip claw
        glm::vec3 tip_pos = base_pos + dir * 0.22f + glm::vec3(0.0f, 0.25f, 0.0f);
        glm::mat4 tip_m = glm::translate(glm::mat4(1.0f), tip_pos);
        tip_m = glm::rotate(tip_m, -angle, glm::vec3(0.0f, 1.0f, 0.0f));
        tip_m = glm::rotate(tip_m, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        add_box_transformed(tip_m, glm::vec3(0.024f, 0.05f, 0.024f), col_precursor_gold, mat_gold);

        // Glowing emitter node at tip
        glm::vec3 emit_pos = base_pos + dir * 0.18f + glm::vec3(0.0f, 0.29f, 0.0f);
        glm::mat4 emit_m = glm::translate(glm::mat4(1.0f), emit_pos);
        add_box_transformed(emit_m, glm::vec3(0.018f), col_cyan_glyph, mat_glyph_cyan);
    }

    // ─────────────────────────────────────────────────────────────
    // 3. Levitating Core Center & Smooth Floating Bob Animation
    // ─────────────────────────────────────────────────────────────
    const float hover_y = base_pos.y + 0.54f + 0.065f * std::sin(time * 2.2f);
    const glm::vec3 core_center(base_pos.x, hover_y, base_pos.z);

    // ─────────────────────────────────────────────────────────────
    // 4. Outer Rotating Astrolabe Gimbal Ring (Celestial Gold)
    // ─────────────────────────────────────────────────────────────
    {
        constexpr int RING_SEGS = 16;
        const float r_out = 0.38f;
        const float r_in  = 0.33f;
        const float h_half = 0.018f;

        glm::mat4 ring1_m = glm::translate(glm::mat4(1.0f), core_center);
        ring1_m = glm::rotate(ring1_m, glm::radians(24.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // 24° pitch
        ring1_m = glm::rotate(ring1_m, time * 1.1f, glm::vec3(0.0f, 1.0f, 0.0f));        // Continuous rotation

        glm::mat3 norm_m1 = glm::transpose(glm::inverse(glm::mat3(ring1_m)));
        auto xform1 = [&](float x, float y, float z) -> glm::vec3 {
            return glm::vec3(ring1_m * glm::vec4(x, y, z, 1.0f));
        };
        auto xform_n1 = [&](const glm::vec3& n) -> glm::vec3 {
            return glm::normalize(norm_m1 * n);
        };

        for (int i = 0; i < RING_SEGS; ++i) {
            float a0 = i * (6.2831853f / RING_SEGS);
            float a1 = (i + 1) * (6.2831853f / RING_SEGS);
            float c0 = std::cos(a0), s0 = std::sin(a0);
            float c1 = std::cos(a1), s1 = std::sin(a1);

            // Ring quad coordinates in local space
            glm::vec3 to0 = xform1(c0 * r_out,  h_half, s0 * r_out);
            glm::vec3 to1 = xform1(c1 * r_out,  h_half, s1 * r_out);
            glm::vec3 ti0 = xform1(c0 * r_in,   h_half, s0 * r_in);
            glm::vec3 ti1 = xform1(c1 * r_in,   h_half, s1 * r_in);

            glm::vec3 bo0 = xform1(c0 * r_out, -h_half, s0 * r_out);
            glm::vec3 bo1 = xform1(c1 * r_out, -h_half, s1 * r_out);
            glm::vec3 bi0 = xform1(c0 * r_in,  -h_half, s0 * r_in);
            glm::vec3 bi1 = xform1(c1 * r_in,  -h_half, s1 * r_in);

            // Top ring face
            push_quad(ti0, ti1, to1, to0, xform_n1({0.0f, 1.0f, 0.0f}), col_precursor_gold, mat_gold);
            // Bottom ring face
            push_quad(bo0, bo1, bi1, bi0, xform_n1({0.0f, -1.0f, 0.0f}), col_precursor_gold, mat_gold);
            // Outer wall
            glm::vec3 mid_n_out = xform_n1({(c0 + c1) * 0.5f, 0.0f, (s0 + s1) * 0.5f});
            push_quad(bo0, to0, to1, bo1, mid_n_out, col_precursor_gold, mat_gold);
            // Inner wall
            glm::vec3 mid_n_in = xform_n1({-(c0 + c1) * 0.5f, 0.0f, -(s0 + s1) * 0.5f});
            push_quad(bi1, ti1, ti0, bi0, mid_n_in, col_precursor_gold, mat_gold);

            // 4 Quadrature Inlaid Glyphs
            if (i % 4 == 0) {
                glm::vec3 glyph_pos = (to0 + to1 + ti0 + ti1) * 0.25f + xform_n1({0.0f, 0.005f, 0.0f});
                glm::mat4 gm = glm::translate(glm::mat4(1.0f), glyph_pos);
                gm = glm::rotate(gm, -a0, xform_n1({0.0f, 1.0f, 0.0f}));
                add_box_transformed(gm, glm::vec3(0.016f, 0.008f, 0.022f), col_cyan_glyph, mat_glyph_cyan);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 5. Inner Counter-Rotating Astrolabe Gimbal Ring (Titanium-Obsidian)
    // ─────────────────────────────────────────────────────────────
    {
        constexpr int RING_SEGS = 16;
        const float r_out = 0.28f;
        const float r_in  = 0.24f;
        const float h_half = 0.014f;

        glm::mat4 ring2_m = glm::translate(glm::mat4(1.0f), core_center);
        ring2_m = glm::rotate(ring2_m, glm::radians(-32.0f), glm::vec3(0.0f, 0.0f, 1.0f)); // -32° roll
        ring2_m = glm::rotate(ring2_m, -time * 1.6f, glm::vec3(0.0f, 1.0f, 0.0f));        // Counter-rotation!

        glm::mat3 norm_m2 = glm::transpose(glm::inverse(glm::mat3(ring2_m)));
        auto xform2 = [&](float x, float y, float z) -> glm::vec3 {
            return glm::vec3(ring2_m * glm::vec4(x, y, z, 1.0f));
        };
        auto xform_n2 = [&](const glm::vec3& n) -> glm::vec3 {
            return glm::normalize(norm_m2 * n);
        };

        for (int i = 0; i < RING_SEGS; ++i) {
            float a0 = i * (6.2831853f / RING_SEGS);
            float a1 = (i + 1) * (6.2831853f / RING_SEGS);
            float c0 = std::cos(a0), s0 = std::sin(a0);
            float c1 = std::cos(a1), s1 = std::sin(a1);

            glm::vec3 to0 = xform2(c0 * r_out,  h_half, s0 * r_out);
            glm::vec3 to1 = xform2(c1 * r_out,  h_half, s1 * r_out);
            glm::vec3 ti0 = xform2(c0 * r_in,   h_half, s0 * r_in);
            glm::vec3 ti1 = xform2(c1 * r_in,   h_half, s1 * r_in);

            glm::vec3 bo0 = xform2(c0 * r_out, -h_half, s0 * r_out);
            glm::vec3 bo1 = xform2(c1 * r_out, -h_half, s1 * r_out);
            glm::vec3 bi0 = xform2(c0 * r_in,  -h_half, s0 * r_in);
            glm::vec3 bi1 = xform2(c1 * r_in,  -h_half, s1 * r_in);

            push_quad(ti0, ti1, to1, to0, xform_n2({0.0f, 1.0f, 0.0f}), col_titanium_trim, mat_titanium);
            push_quad(bo0, bo1, bi1, bi0, xform_n2({0.0f, -1.0f, 0.0f}), col_titanium_trim, mat_titanium);
            glm::vec3 mid_n_out = xform_n2({(c0 + c1) * 0.5f, 0.0f, (s0 + s1) * 0.5f});
            push_quad(bo0, to0, to1, bo1, mid_n_out, col_titanium_trim, mat_titanium);
            glm::vec3 mid_n_in = xform_n2({-(c0 + c1) * 0.5f, 0.0f, -(s0 + s1) * 0.5f});
            push_quad(bi1, ti1, ti0, bi0, mid_n_in, col_titanium_trim, mat_titanium);

            if (i % 4 == 0) {
                glm::vec3 node_pos = (to0 + to1 + ti0 + ti1) * 0.25f;
                glm::mat4 nm = glm::translate(glm::mat4(1.0f), node_pos);
                add_box_transformed(nm, glm::vec3(0.014f), col_crystal_violet, mat_crystal);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 6. Central Precursor Hyper-Core (Faceted Radiant Gem Octahedron)
    // ─────────────────────────────────────────────────────────────
    {
        glm::mat4 core_m = glm::translate(glm::mat4(1.0f), core_center);
        core_m = glm::rotate(core_m, time * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)); // Continuous core spin
        core_m = glm::rotate(core_m, 0.12f * std::sin(time * 2.5f), glm::vec3(1.0f, 0.0f, 1.0f));

        glm::mat3 core_norm_m = glm::transpose(glm::inverse(glm::mat3(core_m)));
        auto xform_core = [&](float x, float y, float z) -> glm::vec3 {
            return glm::vec3(core_m * glm::vec4(x, y, z, 1.0f));
        };
        auto xform_core_n = [&](const glm::vec3& n) -> glm::vec3 {
            return glm::normalize(core_norm_m * n);
        };

        const float apex_y = 0.22f;
        const float r_gem  = 0.135f;
        glm::vec3 apex_top = xform_core(0.0f,  apex_y, 0.0f);
        glm::vec3 apex_bot = xform_core(0.0f, -apex_y, 0.0f);

        constexpr int GEM_SEGS = 8;
        glm::vec3 eq[GEM_SEGS];
        for (int i = 0; i < GEM_SEGS; ++i) {
            float a = i * (6.2831853f / GEM_SEGS);
            eq[i] = xform_core(std::cos(a) * r_gem, 0.0f, std::sin(a) * r_gem);
        }

        // Upper and lower faceted crystal pyramids
        for (int i = 0; i < GEM_SEGS; ++i) {
            int next = (i + 1) % GEM_SEGS;
            // Upper facet (apex_top, eq[i], eq[next])
            glm::vec3 n_top = glm::normalize(glm::cross(eq[next] - apex_top, eq[i] - apex_top));
            push_tri(apex_top, eq[i], eq[next], n_top, col_crystal_violet, mat_crystal);

            // Lower facet (apex_bot, eq[next], eq[i])
            glm::vec3 n_bot = glm::normalize(glm::cross(eq[i] - apex_bot, eq[next] - apex_bot));
            push_tri(apex_bot, eq[next], eq[i], n_bot, col_crystal_violet, mat_crystal);
        }

        // Inner white-hot resonant kernel (radiant hyper-cube)
        glm::mat4 kernel_m = glm::rotate(core_m, glm::radians(45.0f), glm::vec3(1.0f, 1.0f, 0.0f));
        add_box_transformed(kernel_m, glm::vec3(0.045f), col_crystal_kernel, mat_kernel);
    }

    // ─────────────────────────────────────────────────────────────
    // 7. 4 Orbiting Satellite Shards / Runic Spindles
    // ─────────────────────────────────────────────────────────────
    for (int k = 0; k < 4; ++k) {
        float orb_angle = time * 1.25f + k * 1.5707963f;
        float y_bob = 0.04f * std::sin(time * 3.0f + k * 1.5707963f);
        const float r_orb = 0.44f;
        glm::vec3 shard_pos = core_center + glm::vec3(std::cos(orb_angle) * r_orb, y_bob, std::sin(orb_angle) * r_orb);

        glm::mat4 shard_m = glm::translate(glm::mat4(1.0f), shard_pos);
        shard_m = glm::rotate(shard_m, -orb_angle, glm::vec3(0.0f, 1.0f, 0.0f));
        shard_m = glm::rotate(shard_m, time * 3.5f, glm::vec3(0.0f, 0.0f, 1.0f)); // Self-spin along spindle axis

        // Central diamond prism
        add_box_transformed(shard_m, glm::vec3(0.018f, 0.045f, 0.018f), col_satellite, mat_satellite);

        // Pointed tips
        glm::mat4 tip_top = glm::translate(shard_m, glm::vec3(0.0f, 0.055f, 0.0f));
        add_box_transformed(tip_top, glm::vec3(0.010f, 0.018f, 0.010f), col_cyan_glyph, mat_glyph_cyan);
        glm::mat4 tip_bot = glm::translate(shard_m, glm::vec3(0.0f, -0.055f, 0.0f));
        add_box_transformed(tip_bot, glm::vec3(0.010f, 0.018f, 0.010f), col_cyan_glyph, mat_glyph_cyan);
    }

    // ─────────────────────────────────────────────────────────────
    // 8. Additive Luminous Energy Containment Rings & Vector Beams
    // ─────────────────────────────────────────────────────────────
    if (m_cable_vao != 0) {
        constexpr int RING_PTS = 16;
        // Two ascending energy rings drifting upward
        for (int r = 0; r < 2; ++r) {
            float phase = std::fmod(time * 0.35f + r * 0.5f, 1.0f);
            float y_ring = base_pos.y + 0.12f + phase * 0.95f;
            float r_ring = 0.36f - 0.12f * phase; // Gently tapers inward as it rises

            for (int i = 0; i < RING_PTS; ++i) {
                float a0 = i * (6.2831853f / RING_PTS);
                float a1 = (i + 1) * (6.2831853f / RING_PTS);
                glm::vec3 p0 = glm::vec3(base_pos.x + std::cos(a0) * r_ring, y_ring, base_pos.z + std::sin(a0) * r_ring);
                glm::vec3 p1 = glm::vec3(base_pos.x + std::cos(a1) * r_ring, y_ring, base_pos.z + std::sin(a1) * r_ring);
                s_relic_lines.insert(s_relic_lines.end(), {p0.x, p0.y, p0.z, p1.x, p1.y, p1.z});
            }
        }

        // 4 Vertical laser containment vector beams rising from claw tips to apex
        for (int i = 0; i < 4; ++i) {
            float angle = i * 1.5707963f;
            glm::vec3 claw_tip = base_pos + glm::vec3(std::cos(angle) * 0.18f, 0.29f, std::sin(angle) * 0.18f);
            glm::vec3 beam_apex = core_center + glm::vec3(0.0f, 0.38f, 0.0f);
            s_relic_lines.insert(s_relic_lines.end(), {claw_tip.x, claw_tip.y, claw_tip.z, beam_apex.x, beam_apex.y, beam_apex.z});
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 9. Render Solid PBR Geometry into HDR Scene Buffer
    // ─────────────────────────────────────────────────────────────
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

    ensure_stalker_vbo_capacity(s_relic_verts.size());
    glBindVertexArray(m_stalker_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_stalker_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, s_relic_verts.size() * sizeof(StalkerVertex), s_relic_verts.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_relic_verts.size()));
    glBindVertexArray(0);

    // ─────────────────────────────────────────────────────────────
    // 10. Render Additive Luminous Containment Lattice
    // ─────────────────────────────────────────────────────────────
    if (m_cable_vao != 0 && !s_relic_lines.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive luminous glow

        m_wireframe_shader.use();
        m_wireframe_shader.set_mat4("uProjection", m_proj);
        m_wireframe_shader.set_mat4("uView", m_view);
        m_wireframe_shader.set_vec3("uVoxelPos", glm::vec3(0.0f));
        m_wireframe_shader.set_int("uIsLineOnly", 1);
        m_wireframe_shader.set_vec4("uColor", glm::vec4(0.70f, 0.35f, 1.0f, 0.85f * pulse));

        glBindVertexArray(m_cable_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_cable_vbo);
        size_t line_floats = std::min(s_relic_lines.size(), static_cast<size_t>(8192));
        glBufferSubData(GL_ARRAY_BUFFER, 0, line_floats * sizeof(float), s_relic_lines.data());
        glLineWidth(1.5f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(line_floats / 3));
        glBindVertexArray(0);

        glDisable(GL_BLEND);
    }
}

} // namespace Voidfall

