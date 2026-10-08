#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <array>
#include <memory>
#include "../voxel/clutter_types.hpp"
#include "../voxel/chunk.hpp"
#include "shader.hpp"

namespace Voidfall {

struct Headlamp;

struct ClutterVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec4 material; // x=metallic, y=roughness, z=emissive, w=ao
};

struct ClutterInstanceGPUData {
    glm::mat4 model;
    glm::vec4 tint; // rgb = color tint, a = emissive boost
};

struct ClutterMeshPrototype {
    unsigned int vao{0};
    unsigned int vbo{0};
    unsigned int ebo{0};
    unsigned int instance_vbo{0};
    size_t index_count{0};
};

class SurfaceClutterSystem {
public:
    SurfaceClutterSystem();
    ~SurfaceClutterSystem();

    SurfaceClutterSystem(const SurfaceClutterSystem&) = delete;
    SurfaceClutterSystem& operator=(const SurfaceClutterSystem&) = delete;

    void init_gpu();
    void cleanup();

    // Procedural decoration pass: scans chunk faces and spawns anchored non-cubic props
    static std::vector<ClutterInstance> generate_chunk_clutter(const Chunk& chunk, uint32_t seed);

    // Instanced GPU rendering per chunk, distance-gated at 32 meters from the player
    void render_chunk_clutter(
        const std::vector<ClutterInstance>& instances,
        const glm::vec3& player_pos,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::vec3& cam_pos,
        const glm::vec3& headlamp_pos,
        const glm::vec3& headlamp_dir,
        const glm::vec3& headlamp_color,
        bool headlamp_enabled,
        float time = 0.0f
    );

    bool is_gpu_initialized() const { return m_gpu_initialized; }

private:
    void build_stalactite_mesh(ClutterMeshPrototype& proto);
    void build_stalagmite_mesh(ClutterMeshPrototype& proto);
    void build_crystal_mesh(ClutterMeshPrototype& proto);

    Shader m_shader;
    std::array<ClutterMeshPrototype, static_cast<size_t>(ClutterType::Count)> m_prototypes{};
    bool m_gpu_initialized{false};

    // Preallocated staging buffer for zero-allocation per-frame GPU upload (Rule 3.1)
    std::vector<ClutterInstanceGPUData> m_staging_instances;
};

} // namespace Voidfall
