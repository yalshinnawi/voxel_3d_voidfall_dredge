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
    // Generate a complete 6-face 3D cube mesh (1x1x1 unit cube)
    std::vector<PackedVoxelVertex> verts;
    verts.reserve(36);

    // Face 0: +X (norm_idx = 0)
    verts.push_back(PackedVoxelVertex::encode(1, 0, 1, 0, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(1, 0, 0, 0, 3, m_material_id, 1, 1, 1));
    verts.push_back(PackedVoxelVertex::encode(1, 1, 0, 0, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(1, 0, 1, 0, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(1, 1, 0, 0, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(1, 1, 1, 0, 3, m_material_id, 1, 1, 3));

    // Face 1: -X (norm_idx = 1)
    verts.push_back(PackedVoxelVertex::encode(0, 0, 0, 1, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(0, 0, 1, 1, 3, m_material_id, 1, 1, 1));
    verts.push_back(PackedVoxelVertex::encode(0, 1, 1, 1, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(0, 0, 0, 1, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(0, 1, 1, 1, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(0, 1, 0, 1, 3, m_material_id, 1, 1, 3));

    // Face 2: +Y (norm_idx = 2)
    verts.push_back(PackedVoxelVertex::encode(0, 1, 1, 2, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(1, 1, 1, 2, 3, m_material_id, 1, 1, 1));
    verts.push_back(PackedVoxelVertex::encode(1, 1, 0, 2, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(0, 1, 1, 2, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(1, 1, 0, 2, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(0, 1, 0, 2, 3, m_material_id, 1, 1, 3));

    // Face 3: -Y (norm_idx = 3)
    verts.push_back(PackedVoxelVertex::encode(0, 0, 0, 3, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(1, 0, 0, 3, 3, m_material_id, 1, 1, 1));
    verts.push_back(PackedVoxelVertex::encode(1, 0, 1, 3, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(0, 0, 0, 3, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(1, 0, 1, 3, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(0, 0, 1, 3, 3, m_material_id, 1, 1, 3));

    // Face 4: +Z (norm_idx = 4)
    verts.push_back(PackedVoxelVertex::encode(0, 0, 1, 4, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(1, 0, 1, 4, 3, m_material_id, 1, 1, 1));
    verts.push_back(PackedVoxelVertex::encode(1, 1, 1, 4, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(0, 0, 1, 4, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(1, 1, 1, 4, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(0, 1, 1, 4, 3, m_material_id, 1, 1, 3));

    // Face 5: -Z (norm_idx = 5)
    verts.push_back(PackedVoxelVertex::encode(1, 0, 0, 5, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(0, 0, 0, 5, 3, m_material_id, 1, 1, 1));
    verts.push_back(PackedVoxelVertex::encode(0, 1, 0, 5, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(1, 0, 0, 5, 3, m_material_id, 1, 1, 0));
    verts.push_back(PackedVoxelVertex::encode(0, 1, 0, 5, 3, m_material_id, 1, 1, 2));
    verts.push_back(PackedVoxelVertex::encode(1, 1, 0, 5, 3, m_material_id, 1, 1, 3));

    m_vertex_count = verts.size();

    if (glad_glGenVertexArrays && glad_glGenBuffers) {
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
}

DynamicDebris::~DynamicDebris() {
    if (glad_glDeleteVertexArrays && m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (glad_glDeleteBuffers && m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
}

DynamicDebris::DynamicDebris(DynamicDebris&& other) noexcept
    : m_id(other.m_id)
    , m_position(other.m_position)
    , m_velocity(other.m_velocity)
    , m_rotation(other.m_rotation)
    , m_angular_velocity(other.m_angular_velocity)
    , m_material_id(other.m_material_id)
    , m_block_count(other.m_block_count)
    , m_sleeping(other.m_sleeping)
    , m_destroyed(other.m_destroyed)
    , m_life_time(other.m_life_time)
    , m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_vertex_count(other.m_vertex_count)
    , has_dealt_damage(other.has_dealt_damage)
{
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_vertex_count = 0;
}

DynamicDebris& DynamicDebris::operator=(DynamicDebris&& other) noexcept {
    if (this != &other) {
        if (glad_glDeleteVertexArrays && m_vao != 0) glDeleteVertexArrays(1, &m_vao);
        if (glad_glDeleteBuffers && m_vbo != 0) glDeleteBuffers(1, &m_vbo);

        m_id = other.m_id;
        m_position = other.m_position;
        m_velocity = other.m_velocity;
        m_rotation = other.m_rotation;
        m_angular_velocity = other.m_angular_velocity;
        m_material_id = other.m_material_id;
        m_block_count = other.m_block_count;
        m_sleeping = other.m_sleeping;
        m_destroyed = other.m_destroyed;
        m_life_time = other.m_life_time;
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_vertex_count = other.m_vertex_count;
        has_dealt_damage = other.has_dealt_damage;

        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_vertex_count = 0;
    }
    return *this;
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

    // Apply subterranean acceleration due to gravity (g = 18.0 m/s^2)
    m_velocity.y -= 18.0f * dt;

    glm::vec3 next_pos = m_position + m_velocity * dt;

    // Sweeping AABB against player capsule / bounding box
    glm::vec3 p_min = glm::vec3(player_pos.x - 0.35f, std::min(player_pos.y - 0.95f, player_pos.y - 1.6f), player_pos.z - 0.35f);
    glm::vec3 p_max = glm::vec3(player_pos.x + 0.35f, std::max(player_pos.y + 0.95f, player_pos.y + 0.2f), player_pos.z + 0.35f);

    glm::vec3 d_min = next_pos - glm::vec3(0.45f);
    glm::vec3 d_max = next_pos + glm::vec3(0.45f);

    bool aabb_overlap = (p_min.x <= d_max.x && p_max.x >= d_min.x) &&
                        (p_min.y <= d_max.y && p_max.y >= d_min.y) &&
                        (p_min.z <= d_max.z && p_max.z >= d_min.z);

    // If an active falling block impacts the player capsule with downward vertical velocity
    if (aabb_overlap && !has_dealt_damage && m_velocity.y < -3.0f) {
        has_dealt_damage = true;
        if (is_player_sheltered) {
            // Bulkhead shelter utility: falling debris strikes bulkhead harmlessly
            m_destroyed = true;
            res.shattered = true;
            res.hit_bulkhead = true;
            res.shatter_pos = next_pos;
            res.shatter_mat = m_material_id;
            return res;
        } else {
            // Proportional kinetic crushing damage: 15 HP - 45 HP based on fall velocity
            res.hit_player = true;
            res.damage = std::clamp(15.0f + std::abs(m_velocity.y) * 1.5f, 15.0f, 45.0f);
            m_velocity.x += ((static_cast<float>(rand() % 100) / 50.0f) - 1.0f) * 1.5f;
            m_velocity.z += ((static_cast<float>(rand() % 100) / 50.0f) - 1.0f) * 1.5f;
        }
    }

    // Check collision against cavern floor and static voxels along downward trajectory
    int bx = static_cast<int>(std::floor(next_pos.x));
    int bz = static_cast<int>(std::floor(next_pos.z));
    int curr_bottom_y = static_cast<int>(std::floor(m_position.y - 0.45f));
    int next_bottom_y = static_cast<int>(std::floor(next_pos.y - 0.45f));

    bool hit_ground = false;
    int hit_solid_y = next_bottom_y;

    for (int check_y = curr_bottom_y; check_y >= next_bottom_y; --check_y) {
        if (check_y < 0) {
            hit_ground = true;
            hit_solid_y = 0;
            break;
        }
        if (world.is_solid(bx, check_y, bz)) {
            hit_ground = true;
            hit_solid_y = check_y;
            break;
        }
    }

    if (hit_ground) {
        Voxel hit_v = world.get_voxel(bx, hit_solid_y, bz);
        if (hit_v.material_id == MAT_INDUSTRIAL_BULKHEAD) {
            // Shatter harmlessly on industrial bulkhead fortification
            m_destroyed = true;
            res.shattered = true;
            res.hit_bulkhead = true;
            res.shatter_pos = glm::vec3(bx + 0.5f, hit_solid_y + 1.0f, bz + 0.5f);
            res.shatter_mat = m_material_id;
            return res;
        }

        // Find the lowest unoccupied air block resting directly on the solid terrain
        int place_x = bx;
        int place_z = bz;
        int place_y = hit_solid_y + 1;
        while (place_y < 127 && world.is_solid(place_x, place_y, place_z)) {
            place_y++;
        }

        // Avoid embedding the placed block inside the player's occupied space
        int px = static_cast<int>(std::floor(player_pos.x));
        int py = static_cast<int>(std::floor(player_pos.y));
        int pz = static_cast<int>(std::floor(player_pos.z));
        if (place_x == px && place_z == pz && std::abs(place_y - py) <= 1) {
            const int offsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& off : offsets) {
                int nx = place_x + off[0];
                int nz = place_z + off[1];
                if (!world.is_solid(nx, place_y, nz) && world.is_solid(nx, place_y - 1, nz)) {
                    place_x = nx;
                    place_z = nz;
                    break;
                }
            }
        }

        // Place the physical block permanently on the ground
        if (place_y >= 0 && place_y < 128) {
            world.set_voxel(place_x, place_y, place_z, Voxel{m_material_id, 0}, true);
            res.placed_on_ground = true;
            res.place_pos = glm::ivec3(place_x, place_y, place_z);
        }

        m_destroyed = true;
        res.shattered = false;
        res.shatter_pos = glm::vec3(place_x + 0.5f, place_y + 0.5f, place_z + 0.5f);
        res.shatter_mat = m_material_id;
        return res;
    }

    m_position = next_pos;
    m_rotation += m_angular_velocity * dt;

    if (m_life_time > 15.0f || m_position.y < 0.0f) {
        m_destroyed = true;
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
