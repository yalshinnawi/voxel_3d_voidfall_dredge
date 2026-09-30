#include "renderer.hpp"
#include <glad/glad.h>
#include <iostream>

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
    m_bloom_shader.load_graphics("assets/shaders/voxel_pbr.vert", "assets/shaders/bloom.frag"); // reuse vert or quad
    m_postprocess_shader.load_graphics("assets/shaders/voxel_pbr.vert", "assets/shaders/postprocess.frag");

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
}

Renderer::~Renderer() {
    cleanup_framebuffers();
    if (m_quad_vao != 0) glDeleteVertexArrays(1, &m_quad_vao);
    if (m_quad_vbo != 0) glDeleteBuffers(1, &m_quad_vbo);
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

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Renderer] HDR Framebuffer is incomplete!" << std::endl;
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

        m_fog_compute_shader.set_float("uFogDensity", 0.035f);
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
