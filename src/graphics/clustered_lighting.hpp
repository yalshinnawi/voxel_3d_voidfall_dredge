#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <glm/glm.hpp>
#include "shader.hpp"

namespace Voidfall {

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float radius{15.0f};
    float intensity{1.5f};
};

struct alignas(16) GpuPointLight {
    glm::vec4 position_radius{0.0f}; // xyz = world position, w = radius
    glm::vec4 color_intensity{0.0f}; // xyz = color, w = intensity
};

// Clustered Forward Lighting Grid Dimensions: 16x9x24
static constexpr uint32_t CLUSTERS_X = 16;
static constexpr uint32_t CLUSTERS_Y = 9;
static constexpr uint32_t CLUSTERS_Z = 24;
static constexpr uint32_t TOTAL_CLUSTERS = CLUSTERS_X * CLUSTERS_Y * CLUSTERS_Z; // 3456
static constexpr uint32_t MAX_LIGHTS_PER_CLUSTER = 64;
static constexpr uint32_t MAX_SCENE_LIGHTS = 256;

// SSBO layout for cluster tile: std430 aligned
struct alignas(16) ClusterRecord {
    uint32_t count{0};
    uint32_t pad0{0};
    uint32_t pad1{0};
    uint32_t pad2{0};
    uint32_t light_indices[MAX_LIGHTS_PER_CLUSTER]{0};
};

class ClusteredLighting {
public:
    ClusteredLighting();
    ~ClusteredLighting();

    ClusteredLighting(const ClusteredLighting&) = delete;
    ClusteredLighting& operator=(const ClusteredLighting&) = delete;

    bool init();
    void shutdown();

    // Bin point lights into 3D cluster frustum (16x9x24) via compute shader
    void update_and_cull(
        const glm::mat4& view,
        const glm::mat4& proj,
        const std::vector<PointLight>& lights,
        float near_z,
        float far_z,
        int screen_width,
        int screen_height
    );

    // Bind SSBO buffers for fragment shader sampling (0 = Lights, 1 = Clusters)
    void bind_buffers(uint32_t light_binding = 0, uint32_t cluster_binding = 1) const;

    bool is_initialized() const { return m_initialized; }
    uint32_t light_ssbo() const { return m_light_ssbo; }
    uint32_t cluster_ssbo() const { return m_cluster_ssbo; }

    // Static CPU evaluation utilities for deterministic testing & offline validation
    static uint32_t compute_cluster_index(uint32_t x, uint32_t y, uint32_t z);
    static void get_cluster_coords(uint32_t index, uint32_t& x, uint32_t& y, uint32_t& z);
    static uint32_t depth_to_slice(float view_depth, float near_z, float far_z);
    static void slice_to_depth_range(uint32_t slice_z, float near_z, float far_z, float& out_near, float& out_far);

    // View-space frustum AABB & sphere intersection (CPU mirror of cluster_cull.comp)
    static void compute_cluster_aabb_view(
        uint32_t x, uint32_t y, uint32_t z,
        const glm::mat4& proj,
        float near_z, float far_z,
        glm::vec3& out_min, glm::vec3& out_max
    );

    static bool test_sphere_aabb(
        const glm::vec3& sphere_pos_view,
        float radius,
        const glm::vec3& aabb_min,
        const glm::vec3& aabb_max
    );

private:
    bool m_initialized{false};
    Shader m_cull_shader;
    uint32_t m_light_ssbo{0};
    uint32_t m_cluster_ssbo{0};

    // Staging vector to avoid heap allocations per frame
    std::vector<GpuPointLight> m_gpu_lights_staging;
};

} // namespace Voidfall
