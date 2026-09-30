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

DynamicDebris::CollisionResult DynamicDebris::update(
    float dt,
    World& world,
    const glm::vec3& player_pos,
    bool is_player_sheltered
) {
    CollisionResult res;
    if (m_sleeping || m_destroyed) return res;

    m_life_time += dt;

    // Apply subterranean acceleration due to gravity
    m_velocity.y -= 19.6f * dt;

    glm::vec3 next_pos = m_position + m_velocity * dt;

    // Sweeping AABB against player capsule / bounding box
    // Player AABB: eye at player_pos, feet at player_pos.y - 1.6f, head at player_pos.y + 0.2f
    glm::vec3 p_min = player_pos - glm::vec3(0.35f, 1.6f, 0.35f);
    glm::vec3 p_max = player_pos + glm::vec3(0.35f, 0.2f, 0.35f);

    glm::vec3 d_min = next_pos - glm::vec3(0.45f);
    glm::vec3 d_max = next_pos + glm::vec3(0.45f);

    bool aabb_overlap = (p_min.x <= d_max.x && p_max.x >= d_min.x) &&
                        (p_min.y <= d_max.y && p_max.y >= d_min.y) &&
                        (p_min.z <= d_max.z && p_max.z >= d_min.z);

    // If an active falling block impacts the player capsule with vertical velocity |v_y| > 4.0 m/s
    if (aabb_overlap && !has_dealt_damage && m_velocity.y < -4.0f) {
        has_dealt_damage = true;
        m_destroyed = true;
        res.shattered = true;
        res.shatter_pos = next_pos;
        res.shatter_mat = m_material_id;

        if (is_player_sheltered) {
            // Bulkhead shelter utility: falling debris strikes bulkhead harmlessly
            res.hit_bulkhead = true;
        } else {
            // Player takes crushing damage: damage = clamp(int(|v_y| * 3.5f), 15, 45)
            res.hit_player = true;
            int dmg = glm::clamp(static_cast<int>(std::abs(m_velocity.y) * 3.5f), 15, 45);
            res.damage = static_cast<float>(dmg);
        }
        return res;
    }

    // Check collision against cavern floor and static voxels
    int bx = static_cast<int>(std::floor(next_pos.x));
    int by = static_cast<int>(std::floor(next_pos.y));
    int bz = static_cast<int>(std::floor(next_pos.z));
    Voxel hit_v = world.get_voxel(bx, by, bz);

    if (hit_v.is_solid()) {
        if (hit_v.material_id == MAT_INDUSTRIAL_BULKHEAD) {
            // Shatter harmlessly on industrial bulkhead
            m_destroyed = true;
            res.shattered = true;
            res.hit_bulkhead = true;
            res.shatter_pos = next_pos;
            res.shatter_mat = m_material_id;
            return res;
        }

        if (hit_v.material_id == MAT_DREDGE_BEDROCK || std::abs(m_velocity.y) > 10.0f) {
            // High velocity impact on hard bedrock or deep crust -> shatter into particles
            m_destroyed = true;
            res.shattered = true;
            res.shatter_pos = next_pos;
            res.shatter_mat = m_material_id;
            return res;
        }

        // Moderate velocity impact on standard rock -> can lodge as new terrain block
        int lodge_x = static_cast<int>(std::floor(m_position.x));
        int lodge_y = static_cast<int>(std::floor(m_position.y));
        int lodge_z = static_cast<int>(std::floor(m_position.z));
        if (lodge_y >= 0 && lodge_y < 128 && !world.get_voxel(lodge_x, lodge_y, lodge_z).is_solid()) {
            world.set_voxel(lodge_x, lodge_y, lodge_z, Voxel{m_material_id, 0}, true);
            m_destroyed = true;
            return res;
        }

        // Bounce with energy loss
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
    if (m_life_time > 20.0f) {
        m_sleeping = true;
    }

    return res;
}

void DynamicDebris::update(float dt, World& world) {
    update(dt, world, glm::vec3(-9999.0f), false);
}

void DynamicDebris::render() const {
    if (m_vao == 0 || m_vertex_count == 0) return;
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertex_count));
    glBindVertexArray(0);
}

} // namespace Voidfall
