#pragma once
#include "shader.hpp"
#include "texture_array.hpp"
#include "clustered_lighting.hpp"
#include "surface_clutter.hpp"
#include "../voxel/chunk.hpp"
#include "../voxel/block_break_fx.hpp"
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
enum class StalkerRole : uint8_t;
struct VoidStalker;
struct SeismicBurrower;
struct EnemyCarcass;


struct Headlamp {
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    glm::vec3 color{0.95f, 0.98f, 1.0f}; // Crisp high-intensity halogen LED
    float inner_cutoff{glm::cos(glm::radians(18.0f))};
    float outer_cutoff{glm::cos(glm::radians(28.0f))}; // Tightened beam: 18 deg inner / 28 deg outer
    float intensity{4.5f};
    float max_range{22.0f}; // Clamped range to 22m with quadratic falloff
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
    void begin_solid_pass();
    void render_chunk(const Chunk& chunk);
    void render_chunk_liquid(const Chunk& chunk);
    void begin_liquid_pass();
    void end_liquid_pass();
    void RenderLiquidPass();
    void RenderScene();
    template<typename RenderSolidFn, typename RenderLiquidFn>
    void RenderScene(RenderSolidFn&& render_solid, RenderLiquidFn&& render_liquid) {
        begin_solid_pass();
        render_solid();

        begin_liquid_pass();
        render_liquid();
        RenderLiquidPass();
        end_liquid_pass();
    }
    std::vector<Chunk*>& visible_chunks() { return m_visibleChunks; }
    const std::vector<Chunk*>& visible_chunks() const { return m_visibleChunks; }
    void add_visible_chunk(Chunk* chunk) { m_visibleChunks.push_back(chunk); }
    Shader* liquid_shader() { return m_liquidShader; }
    void render_chunk_clutter(const Chunk& chunk, const glm::vec3& player_pos);
    SurfaceClutterSystem& clutter_system() { return m_clutter_system; }
    const SurfaceClutterSystem& clutter_system() const { return m_clutter_system; }
    bool is_box_in_frustum(const glm::vec3& min_pt, const glm::vec3& max_pt) const;
    const std::array<glm::vec4, 6>& frustum_planes() const { return m_frustum_planes; }
    void render_debris(const DynamicDebris& debris);
    void render_stalkers(const std::vector<VoidStalker>& stalkers);
    void render_stalkers(const std::vector<VoidStalker>& stalkers, const std::vector<VoidSpikeProjectile>& projectiles);
    void render_carcasses(const std::vector<EnemyCarcass>& carcasses);
    void render_burrowers(const std::vector<SeismicBurrower>& burrowers);
    void render_plasma_bolts(const std::vector<PlayerPlasmaBolt>& bolts);
    void render_delver(const glm::vec3& pos, float yaw, CharacterClass cls, float anim_time = 0.0f);
    void ensure_stalker_vbo_capacity(size_t required_verts);
    void ensure_cable_vbo_capacity(size_t required_floats);
    size_t stalker_vbo_capacity() const { return m_stalker_vbo_capacity; }
    size_t cable_vbo_capacity() const { return m_cable_vbo_capacity; }
    void end_frame(float delta_time, float radiation_level = 0.0f, float shield_timer = 0.0f,
                   const glm::vec3& hazard_haze_color = glm::vec3(0.0f), float hazard_haze_factor = -1.0f,
                   float suffocation_darkness = 0.0f);

    void trigger_sonar_pulse(const glm::vec3& origin);
    void render_sonar_clusters(const std::vector<SurveyedCluster>& clusters, float alpha);
    void render_sonar_monsters(const std::vector<SurveyedMonster>& monsters, float alpha);
    void render_sonar_wireframes(const std::vector<SurveyedCluster>& clusters, float alpha) {
        render_sonar_clusters(clusters, alpha);
    }
    void render_sonar_wireframes(const std::vector<SurveyedVoxel>& voxels, float alpha);
    void render_grapple_cable(const glm::vec3& start, const glm::vec3& end);
    void render_extraction_beacon(const glm::vec3& beacon_pos, float siren_pulse, float time, bool is_pod_landed);
    void render_extraction_pod(const glm::vec3& beacon_pos, float drill_progress, float ramp_extension, float time, bool is_anchored);
    void render_precursor_relic(const glm::vec3& base_pos, float time);
    void trigger_dust_kickup(float duration = 3.0f);
    void spawn_dust_cloud(const glm::vec3& pos, int count = 24);

    void spawn_break_particles(const glm::vec3& block_pos, const glm::ivec3& normal, uint8_t mat_id, VoxelShape shape = SHAPE_CUBE);
    void spawn_clutter_break_particles(const glm::vec3& clutter_pos, ClutterType type, const glm::vec4& tint = glm::vec4(1.0f));
    void spawn_crack_debris(const glm::vec3& block_pos, const glm::ivec3& normal, float intensity, uint8_t mat_id, VoxelShape shape = SHAPE_CUBE);
    void spawn_burrow_particles(const glm::vec3& burrow_pos, const glm::vec3& burrow_dir, uint8_t mat_id = 1, int count = 8);
    void spawn_monster_emergence_particles(const glm::vec3& pos, const glm::vec3& normal, StalkerRole role, float progress = 0.5f, int count = 6);
    void spawn_burrower_emergence_particles(const glm::vec3& pos, const glm::vec3& forward_dir, float progress = 0.5f, int count = 10);
    void spawn_toxic_gas_cloud(const glm::vec3& block_pos, int count = 3);
    void spawn_radiation_glimmer(const glm::vec3& block_pos, int count = 2);
    void spawn_lava_embers(const glm::vec3& block_pos, int count = 2);
    void spawn_water_mist(const glm::vec3& block_pos, int count = 2);
    void spawn_barrel_smoke(const glm::vec3& pos, const glm::vec3& dir, int count = 3);
    size_t active_particle_count() const { return m_particles.size(); }
    const std::vector<BreakParticle>& particles() const { return m_particles; }

    static glm::vec4 get_material_crack_color(uint8_t mat_id, int stage) {
        return BlockBreakFX::get_material_crack_color(mat_id, stage);
    }
    static void generate_block_crack_geometry(
        const glm::ivec3& voxel_pos,
        float progress,
        const glm::ivec3& face_norm,
        uint8_t mat_id,
        VoxelShape shape,
        std::vector<float>& out_lines,
        glm::vec4& out_color
    ) {
        BlockBreakFX::generate_block_crack_geometry(voxel_pos, progress, face_norm, mat_id, shape, out_lines, out_color);
    }

    void render_block_cracks(const glm::ivec3& voxel_pos, float progress, const glm::ivec3& face_norm, uint8_t mat_id = 1, VoxelShape shape = SHAPE_CUBE);
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

    void set_area_darkness(float darkness) { m_target_area_darkness = std::clamp(darkness, 0.0f, 1.0f); }
    float area_darkness() const { return m_area_darkness; }

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
    void init_sonar_buffers();
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
    Shader* m_liquidShader{&m_voxel_shader};
    Shader m_fog_compute_shader;
    Shader m_bloom_shader;
    Shader m_postprocess_shader;
    Shader m_wireframe_shader;
    Shader m_particle_shader;
    Shader m_stalker_shader;

    // Chunks
    std::vector<Chunk*> m_visibleChunks;

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

    // Smooth sonar hull & outline dynamic buffer
    unsigned int m_sonar_vao{0};
    unsigned int m_sonar_vbo{0};

    // Grapple cable line
    unsigned int m_cable_vao{0};
    unsigned int m_cable_vbo{0};
    size_t m_cable_vbo_capacity{16384};

    // Void Stalker 3D predator model buffers
    unsigned int m_stalker_vao{0};
    unsigned int m_stalker_vbo{0};
    size_t m_stalker_vbo_capacity{65536};
    std::vector<float> m_stalker_staging_lines;

    // Break particles
    static constexpr size_t MAX_PARTICLES = 1024;
    unsigned int m_particle_vao{0};
    unsigned int m_particle_vbo{0};
    std::vector<BreakParticle> m_particles;
    std::vector<float> m_particle_vertex_buffer;

    float m_total_time{0.0f};
    float m_dust_timer{0.0f};
    int m_sector{1};
    float m_brightness{1.0f};
    float m_headlamp_flicker{0.0f};
    float m_area_darkness{0.0f};
    float m_target_area_darkness{0.0f};

    // Camera frustum culling
    std::array<glm::vec4, 6> m_frustum_planes{};

    // Surface clutter decoration system
    SurfaceClutterSystem m_clutter_system;
};

} // namespace Voidfall
