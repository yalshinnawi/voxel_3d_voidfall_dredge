#pragma once
#include "shader.hpp"
#include "texture_array.hpp"
#include "../voxel/chunk.hpp"
#include "../skills/surveying.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Voidfall {

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float radius{15.0f};
    float intensity{1.5f};
};

struct Headlamp {
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    glm::vec3 color{0.95f, 0.98f, 1.0f}; // Crisp high-intensity halogen LED
    float inner_cutoff{glm::cos(glm::radians(18.0f))};
    float outer_cutoff{glm::cos(glm::radians(32.0f))};
    float intensity{3.5f};
    bool enabled{true};
};

struct SonarPulseState {
    glm::vec3 origin{0.0f};
    float current_radius{0.0f};
    float max_radius{70.0f};
    float expansion_speed{35.0f};
    bool active{false};
};

struct BreakParticle {
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec4 color;
    float size{0.25f};
    float life{0.8f};
    float max_life{0.8f};
};

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void resize(int width, int height);

    void begin_frame(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cam_pos);
    void render_chunk(const Chunk& chunk);
    void end_frame(float delta_time, float radiation_level = 0.0f);

    void trigger_sonar_pulse(const glm::vec3& origin);
    void render_sonar_wireframes(const std::vector<SurveyedVoxel>& voxels, float alpha);
    void render_grapple_cable(const glm::vec3& start, const glm::vec3& end);
    void trigger_dust_kickup(float duration = 3.0f);

    void spawn_break_particles(const glm::vec3& block_pos, const glm::ivec3& normal, uint8_t mat_id);
    void update_particles(float dt);
    void render_particles();

    Headlamp& headlamp() { return m_headlamp; }
    const Headlamp& headlamp() const { return m_headlamp; }

    void add_point_light(const PointLight& light);
    void clear_point_lights();

    int width() const { return m_width; }
    int height() const { return m_height; }
    const glm::mat4& view_matrix() const { return m_view; }
    const glm::mat4& proj_matrix() const { return m_proj; }

private:
    void init_framebuffers();
    void cleanup_framebuffers();
    void render_quad();
    void init_wireframe_cube();
    void init_cable_buffer();
    void init_particle_buffers();

    int m_width{1600};
    int m_height{900};

    // Camera matrices
    glm::mat4 m_view{1.0f};
    glm::mat4 m_proj{1.0f};
    glm::vec3 m_cam_pos{0.0f};

    // Lighting
    Headlamp m_headlamp;
    std::vector<PointLight> m_point_lights;
    SonarPulseState m_sonar;

    // Shaders
    Shader m_voxel_shader;
    Shader m_fog_compute_shader;
    Shader m_bloom_shader;
    Shader m_postprocess_shader;
    Shader m_wireframe_shader;
    Shader m_particle_shader;

    // Textures
    std::unique_ptr<TextureArray> m_texture_array;

    // HDR Scene FBO
    unsigned int m_hdr_fbo{0};
    unsigned int m_color_tex{0};
    unsigned int m_bright_tex{0};
    unsigned int m_depth_tex{0};

    // Volumetric Fog Texture
    unsigned int m_fog_tex{0};
    int m_fog_width{800};
    int m_fog_height{450};

    // Bloom Ping-Pong FBOs
    unsigned int m_bloom_fbo[2]{0, 0};
    unsigned int m_bloom_tex[2]{0, 0};

    // Fullscreen Quad
    unsigned int m_quad_vao{0};
    unsigned int m_quad_vbo{0};

    // Wireframe unit cube for sonar X-ray
    unsigned int m_wireframe_vao{0};
    unsigned int m_wireframe_vbo{0};

    // Grapple cable line
    unsigned int m_cable_vao{0};
    unsigned int m_cable_vbo{0};

    // Break particles
    unsigned int m_particle_vao{0};
    unsigned int m_particle_vbo{0};
    std::vector<BreakParticle> m_particles;

    float m_total_time{0.0f};
    float m_dust_timer{0.0f};
};

} // namespace Voidfall
