#include "dynamic_debris.hpp"
#include "../voxel/world.hpp"
#include <glad/glad.h>
#include <algorithm>

namespace Voidfall {

DynamicDebris::DynamicDebris(
    uint32_t id,
    const glm::vec3& position,
    const glm::vec3& linear_vel,
    const glm::vec3& angular_vel,
    uint8_t material_id,
    size_t block_count
)
    : m_id(id)
    , m_position(position)
    , m_velocity(linear_vel)
    , m_angular_velocity(angular_vel)
    , m_material_id(material_id)
    , m_block_count(block_count)
{
    // Generate a simple debris boulder mesh
    std::vector<PackedVoxelVertex> verts;
    verts.reserve(36);

    for (int face = 0; face < 6; ++face) {
        PackedVoxelVertex v0 = PackedVoxelVertex::encode(0, 0, 0, face, 3, m_material_id, 1, 1, 0);
        PackedVoxelVertex v1 = PackedVoxelVertex::encode(1, 0, 0, face, 3, m_material_id, 1, 1, 1);
        PackedVoxelVertex v2 = PackedVoxelVertex::encode(1, 1, 0, face, 3, m_material_id, 1, 1, 2);
        PackedVoxelVertex v3 = PackedVoxelVertex::encode(0, 1, 0, face, 3, m_material_id, 1, 1, 3);

        verts.push_back(v0);
        verts.push_back(v1);
        verts.push_back(v2);

        verts.push_back(v0);
        verts.push_back(v2);
        verts.push_back(v3);
    }

    m_vertex_count = verts.size();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(PackedVoxelVertex)), verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 2, GL_UNSIGNED_INT, sizeof(PackedVoxelVertex), reinterpret_cast<const void*>(0));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

DynamicDebris::~DynamicDebris() {
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
}

void DynamicDebris::update(float dt, World& world) {
    if (m_sleeping) return;

    m_life_time += dt;

    // Apply subterranean gravity
    m_velocity.y -= 19.6f * dt;

    glm::vec3 next_pos = m_position + m_velocity * dt;

    // Check voxel collision at base
    int bx = static_cast<int>(std::floor(next_pos.x));
    int by = static_cast<int>(std::floor(next_pos.y));
    int bz = static_cast<int>(std::floor(next_pos.z));

    if (world.get_voxel(bx, by, bz).is_solid()) {
        // Collision reaction: bounce with energy loss
        m_velocity.y = -m_velocity.y * 0.35f;
        m_velocity.x *= 0.6f;
        m_velocity.z *= 0.6f;
        m_angular_velocity *= 0.7f;

        if (std::abs(m_velocity.y) < 0.8f && glm::length(glm::vec2(m_velocity.x, m_velocity.z)) < 0.5f) {
            m_sleeping = true;
            m_velocity = glm::vec3(0.0f);
        }
    } else {
        m_position = next_pos;
    }

    m_rotation += m_angular_velocity * dt;
}

void DynamicDebris::render() const {
    if (m_vao == 0 || m_vertex_count == 0) return;
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertex_count));
    glBindVertexArray(0);
}

} // namespace Voidfall
