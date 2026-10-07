#include "clustered_lighting.hpp"
#include "../core/logger.hpp"
#include <glad/glad.h>
#include <cmath>
#include <algorithm>

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif

namespace Voidfall {

ClusteredLighting::ClusteredLighting() {
    m_gpu_lights_staging.reserve(MAX_SCENE_LIGHTS);
}

ClusteredLighting::~ClusteredLighting() {
    shutdown();
}

bool ClusteredLighting::init() {
    if (!glBindBufferBase || !glDispatchCompute) {
        VF_LOG_WARN("Lighting", "OpenGL compute / SSBO extensions not loaded; ClusteredLighting running in CPU fallback mode.");
        return false;
    }

    if (!m_cull_shader.load_compute("assets/shaders/cluster_cull.comp")) {
        VF_LOG_ERROR("Lighting", "Failed to compile assets/shaders/cluster_cull.comp");
        return false;
    }

    // 1. Allocate Light SSBO (up to MAX_SCENE_LIGHTS * sizeof(GpuPointLight))
    glGenBuffers(1, &m_light_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_light_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_SCENE_LIGHTS * sizeof(GpuPointLight), nullptr, GL_DYNAMIC_DRAW);

    // 2. Allocate Cluster Grid SSBO (TOTAL_CLUSTERS * sizeof(ClusterRecord))
    glGenBuffers(1, &m_cluster_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_cluster_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, TOTAL_CLUSTERS * sizeof(ClusterRecord), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_initialized = true;
    VF_LOG_INFO("Lighting", "Clustered Forward Lighting initialized: 16x9x24 grid (3456 clusters), max 256 lights, 64 lights/cluster.");
    return true;
}

void ClusteredLighting::shutdown() {
    if (m_light_ssbo != 0) {
        glDeleteBuffers(1, &m_light_ssbo);
        m_light_ssbo = 0;
    }
    if (m_cluster_ssbo != 0) {
        glDeleteBuffers(1, &m_cluster_ssbo);
        m_cluster_ssbo = 0;
    }
    m_initialized = false;
}

void ClusteredLighting::update_and_cull(
    const glm::mat4& view,
    const glm::mat4& proj,
    const std::vector<PointLight>& lights,
    float near_z,
    float far_z,
    int screen_width,
    int screen_height
) {
    if (!m_initialized) return;

    size_t count = std::min(lights.size(), static_cast<size_t>(MAX_SCENE_LIGHTS));
    m_gpu_lights_staging.resize(count);
    for (size_t i = 0; i < count; ++i) {
        m_gpu_lights_staging[i].position_radius = glm::vec4(lights[i].position, lights[i].radius);
        m_gpu_lights_staging[i].color_intensity = glm::vec4(lights[i].color, lights[i].intensity);
    }

    // Upload light data to SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_light_ssbo);
    if (count > 0) {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(GpuPointLight), m_gpu_lights_staging.data());
    }

    // Bind SSBOs for compute shader (Binding 0 = Lights, Binding 1 = Clusters)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_light_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_cluster_ssbo);

    // Dispatch compute shader (16x9 threads per workgroup, 24 depth slices)
    m_cull_shader.use();
    m_cull_shader.set_mat4("uView", view);
    m_cull_shader.set_mat4("uProj", proj);
    m_cull_shader.set_float("uNear", near_z);
    m_cull_shader.set_float("uFar", far_z);
    m_cull_shader.set_int("uNumLights", static_cast<int>(count));

    glDispatchCompute(1, 1, CLUSTERS_Z);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void ClusteredLighting::bind_buffers(uint32_t light_binding, uint32_t cluster_binding) const {
    if (!m_initialized) return;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, light_binding, m_light_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, cluster_binding, m_cluster_ssbo);
}

uint32_t ClusteredLighting::compute_cluster_index(uint32_t x, uint32_t y, uint32_t z) {
    return x + y * CLUSTERS_X + z * (CLUSTERS_X * CLUSTERS_Y);
}

void ClusteredLighting::get_cluster_coords(uint32_t index, uint32_t& x, uint32_t& y, uint32_t& z) {
    z = index / (CLUSTERS_X * CLUSTERS_Y);
    uint32_t rem = index % (CLUSTERS_X * CLUSTERS_Y);
    y = rem / CLUSTERS_X;
    x = rem % CLUSTERS_X;
}

uint32_t ClusteredLighting::depth_to_slice(float view_depth, float near_z, float far_z) {
    float zn = std::max(near_z, 0.01f);
    float zf = std::max(far_z, zn + 1.0f);
    if (view_depth <= zn) return 0;
    if (view_depth >= zf) return CLUSTERS_Z - 1;
    float depth_ratio = view_depth / zn;
    float depth_norm = std::log(depth_ratio) / std::log(zf / zn);
    uint32_t slice = static_cast<uint32_t>(depth_norm * static_cast<float>(CLUSTERS_Z));
    return std::min(slice, CLUSTERS_Z - 1);
}

void ClusteredLighting::slice_to_depth_range(uint32_t slice_z, float near_z, float far_z, float& out_near, float& out_far) {
    float zn = std::max(near_z, 0.01f);
    float zf = std::max(far_z, zn + 1.0f);
    slice_z = std::min(slice_z, CLUSTERS_Z - 1);
    float log_ratio = std::log(zf / zn);
    float norm_min = static_cast<float>(slice_z) / static_cast<float>(CLUSTERS_Z);
    float norm_max = static_cast<float>(slice_z + 1) / static_cast<float>(CLUSTERS_Z);
    out_near = zn * std::exp(norm_min * log_ratio);
    out_far  = zn * std::exp(norm_max * log_ratio);
}

void ClusteredLighting::compute_cluster_aabb_view(
    uint32_t x, uint32_t y, uint32_t z,
    const glm::mat4& proj,
    float near_z, float far_z,
    glm::vec3& out_min, glm::vec3& out_max
) {
    float z_near = std::max(near_z, 0.01f);
    float z_far = std::max(far_z, z_near + 1.0f);
    float log_ratio = std::log(z_far / z_near);

    float slice_min_norm = static_cast<float>(z) / static_cast<float>(CLUSTERS_Z);
    float slice_max_norm = static_cast<float>(z + 1) / static_cast<float>(CLUSTERS_Z);

    float z_tile_near = z_near * std::exp(slice_min_norm * log_ratio);
    float z_tile_far  = z_near * std::exp(slice_max_norm * log_ratio);

    float x_min_ndc = -1.0f + (static_cast<float>(x) / static_cast<float>(CLUSTERS_X)) * 2.0f;
    float x_max_ndc = -1.0f + (static_cast<float>(x + 1) / static_cast<float>(CLUSTERS_X)) * 2.0f;

    float y_min_ndc = -1.0f + (static_cast<float>(y) / static_cast<float>(CLUSTERS_Y)) * 2.0f;
    float y_max_ndc = -1.0f + (static_cast<float>(y + 1) / static_cast<float>(CLUSTERS_Y)) * 2.0f;

    float inv_p00 = 1.0f / std::max(proj[0][0], 0.0001f);
    float inv_p11 = 1.0f / std::max(proj[1][1], 0.0001f);

    glm::vec2 x_near = glm::vec2(x_min_ndc, x_max_ndc) * (z_tile_near * inv_p00);
    glm::vec2 x_far  = glm::vec2(x_min_ndc, x_max_ndc) * (z_tile_far * inv_p00);
    glm::vec2 y_near = glm::vec2(y_min_ndc, y_max_ndc) * (z_tile_near * inv_p11);
    glm::vec2 y_far  = glm::vec2(y_min_ndc, y_max_ndc) * (z_tile_far * inv_p11);

    out_min.x = std::min({x_near.x, x_near.y, x_far.x, x_far.y});
    out_min.y = std::min({y_near.x, y_near.y, y_far.x, y_far.y});
    out_min.z = -z_tile_far;

    out_max.x = std::max({x_near.x, x_near.y, x_far.x, x_far.y});
    out_max.y = std::max({y_near.x, y_near.y, y_far.x, y_far.y});
    out_max.z = -z_tile_near;
}

bool ClusteredLighting::test_sphere_aabb(
    const glm::vec3& sphere_pos_view,
    float radius,
    const glm::vec3& aabb_min,
    const glm::vec3& aabb_max
) {
    glm::vec3 closest = glm::clamp(sphere_pos_view, aabb_min, aabb_max);
    glm::vec3 diff = sphere_pos_view - closest;
    return glm::dot(diff, diff) <= (radius * radius);
}

} // namespace Voidfall
