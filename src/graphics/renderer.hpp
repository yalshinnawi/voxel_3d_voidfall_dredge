#pragma once
#include "shader.hpp"
#include "texture_array.hpp"
#include "clustered_lighting.hpp"
#include "../voxel/chunk.hpp"
#include "../skills/surveying.hpp"
#include "../player/character_class.hpp"
#include "../player/loadout.hpp"
#include "../entities/enemies/void_stalker.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>
#include <algorithm>

namespace Voidfall {

class DynamicDebris;
struct VoidStalker;
struct SeismicBurrower;
struct EnemyCarcass;


struct Headlamp {
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    glm::vec3 color{0.95f, 0.98f, 1.0f}; // Crisp high-intensity halogen LED
    float inner_cutoff{glm::cos(glm::radians(18.0f))};
    float outer_cutoff{glm::cos(glm::radians(32.0f))};
    float intensity{4.5f};
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
    float gravity{16.0f};    // Downward gravity acceleration (negative = buoyant float)
    float drag{1.5f};        // Velocity damping
    float size_growth{0.0f}; // Expansion over lifetime (for vapor/smoke)
    float emissive{0.0f};
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
    void render_debris(const DynamicDebris& debris);
    void render_stalkers(const std::vector<VoidStalker>& stalkers);
    void render_stalkers(const std::vector<VoidStalker>& stalkers, const std::vector<VoidSpikeProjectile>& projectiles);
    void render_carcasses(const std::vector<EnemyCarcass>& carcasses);
    void render_burrowers(const std::vector<SeismicBurrower>& burrowers);
    void render_plasma_bolts(const std::vector<PlayerPlasmaBolt>& bolts);
    void render_delver(const glm::vec3& pos, float yaw, CharacterClass cls, float anim_time = 0.0f);
    void end_frame(float delta_time, float radiation_level = 0.0f);

    void trigger_sonar_pulse(const glm::vec3& origin);
    void render_sonar_wireframes(const std::vector<SurveyedVoxel>& voxels, float alpha);
    void render_grapple_cable(const glm::vec3& start, const glm::vec3& end);
    void render_extraction_beacon(const glm::vec3& beacon_pos, float siren_pulse, float time, bool is_pod_landed);
    void render_extraction_pod(const glm::vec3& beacon_pos, float drill_progress, float ramp_extension, float time, bool is_anchored);
    void trigger_dust_kickup(float duration = 3.0f);
    void spawn_dust_cloud(const glm::vec3& pos, int count = 24);

    void spawn_break_particles(const glm::vec3& block_pos, const glm::ivec3& normal, uint8_t mat_id);
    void spawn_crack_debris(const glm::vec3& block_pos, const glm::ivec3& normal, float intensity, uint8_t mat_id);
    void spawn_burrow_particles(const glm::vec3& burrow_pos, const glm::vec3& burrow_dir, uint8_t mat_id = 1, int count = 8);
    void spawn_toxic_gas_cloud(const glm::vec3& block_pos, int count = 3);
    void spawn_radiation_glimmer(const glm::vec3& block_pos, int count = 2);
    void spawn_lava_embers(const glm::vec3& block_pos, int count = 2);
    void spawn_water_mist(const glm::vec3& block_pos, int count = 2);
    void spawn_barrel_smoke(const glm::vec3& pos, const glm::vec3& dir, int count = 3);
    size_t active_particle_count() const { return m_particles.size(); }
    const std::vector<BreakParticle>& particles() const { return m_particles; }

    void render_block_cracks(const glm::ivec3& voxel_pos, float progress, const glm::ivec3& face_norm, uint8_t mat_id = 1);
    void render_placed_charge(const glm::ivec3& block_pos, const glm::ivec3& normal, float time);
    void render_flares(const std::vector<struct ChemicalFlare>& flares, float time);
    void render_breadcrumbs(const std::vector<glm::vec3>& crumbs, float time);
    void spawn_fireball(const glm::vec3& center, int count = 24);
    void update_particles(float dt);
    void render_particles();

    Headlamp& headlamp() { return m_headlamp; }
    const Headlamp& headlamp() const { return m_headlamp; }

    void set_sector(int sector) { m_sector = sector; }
    int sector() const { return m_sector; }

    void add_point_light(const PointLight& light);
    void clear_point_lights();

    ClusteredLighting* clustered_lighting() { return m_clustered_lighting.get(); }
    const ClusteredLighting* clustered_lighting() const { return m_clustered_lighting.get(); }

    void set_brightness(float b) { m_brightness = std::clamp(b, 0.4f, 2.5f); }
    float brightness() const { return m_brightness; }

    void set_headlamp_flicker(float f) { m_headlamp_flicker = std::clamp(f, 0.0f, 1.0f); }
    float headlamp_flicker() const { return m_headlamp_flicker; }
    float get_effective_headlamp_intensity() const;

    int width() const { return m_width; }
    int height() const { return m_height; }
    const glm::mat4& view_matrix() const { return m_view; }
    const glm::mat4& proj_matrix() const { return m_proj; }

    static glm::mat4 create_projection(float aspect, float fov_deg = 75.0f) {
        return glm::perspective(glm::radians(fov_deg), aspect, 0.1f, 250.0f);
    }

private:
    void init_framebuffers();
    void cleanup_framebuffers();
    void render_quad();
    void init_wireframe_cube();
    void init_cable_buffer();
    void init_particle_buffers();
    void init_stalker_buffers();

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
    std::unique_ptr<ClusteredLighting> m_clustered_lighting;

    // Shaders
    Shader m_voxel_shader;
    Shader m_fog_compute_shader;
    Shader m_bloom_shader;
    Shader m_postprocess_shader;
    Shader m_wireframe_shader;
    Shader m_particle_shader;
    Shader m_stalker_shader;

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

    // Void Stalker 3D predator model buffers
    unsigned int m_stalker_vao{0};
    unsigned int m_stalker_vbo{0};
    std::vector<float> m_stalker_staging_lines;

    // Break particles
    unsigned int m_particle_vao{0};
    unsigned int m_particle_vbo{0};
    std::vector<BreakParticle> m_particles;

    float m_total_time{0.0f};
    float m_dust_timer{0.0f};
    int m_sector{1};
    float m_brightness{1.0f};
    float m_headlamp_flicker{0.0f};

    // Camera frustum culling
    std::array<glm::vec4, 6> m_frustum_planes{};
    bool is_box_in_frustum(const glm::vec3& min_pt, const glm::vec3& max_pt) const;
};

} // namespace Voidfall
