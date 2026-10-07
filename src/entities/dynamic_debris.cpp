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
    size_t block_count,
    World* world
)
    : m_id(id)
    , m_position(position)
    , m_velocity(linear_vel)
    , m_angular_velocity(angular_vel)
    , m_material_id(material_id)
    , m_block_count(std::max(size_t(1), block_count))
    , m_world(world)
{
    m_blocks.push_back(DebrisBlock{glm::ivec3(0), Voxel{m_material_id, 0}});
    build_mesh();
}

DynamicDebris::DynamicDebris(
    uint32_t id,
    const glm::vec3& position,
    const glm::vec3& linear_vel,
    const glm::vec3& angular_vel,
    const std::vector<DebrisBlock>& blocks,
    uint8_t primary_material,
    World* world
)
    : m_id(id)
    , m_position(position)
    , m_velocity(linear_vel)
    , m_angular_velocity(angular_vel)
    , m_material_id(primary_material)
    , m_block_count(std::max(size_t(1), blocks.size()))
    , m_blocks(blocks)
    , m_world(world)
{
    if (m_blocks.empty()) {
        m_blocks.push_back(DebrisBlock{glm::ivec3(0), Voxel{m_material_id, 0}});
    }
    build_mesh();
}

void DynamicDebris::build_mesh() {
    if (m_blocks.empty()) return;

    glm::ivec3 min_o = m_blocks[0].local_offset;
    for (const auto& b : m_blocks) {
        min_o = glm::min(min_o, b.local_offset);
    }
    m_mesh_offset = glm::vec3(min_o);

    std::vector<PackedVoxelVertex> verts;
    verts.reserve(m_blocks.size() * 36);

    for (const auto& b : m_blocks) {
        int ox = b.local_offset.x - min_o.x;
        int oy = b.local_offset.y - min_o.y;
        int oz = b.local_offset.z - min_o.z;
        uint8_t mat = (b.voxel.material_id != MAT_AIR) ? b.voxel.material_id : m_material_id;

        // Face 0: +X (norm_idx = 0)
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 0, oz + 1, 0, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 0, oz + 0, 0, 0, mat, 1, 1, 1));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 1, oz + 0, 0, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 0, oz + 1, 0, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 1, oz + 0, 0, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 1, oz + 1, 0, 0, mat, 1, 1, 3));

        // Face 1: -X (norm_idx = 1)
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 0, oz + 0, 1, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 0, oz + 1, 1, 0, mat, 1, 1, 1));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 1, oz + 1, 1, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 0, oz + 0, 1, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 1, oz + 1, 1, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 1, oz + 0, 1, 0, mat, 1, 1, 3));

        // Face 2: +Y (norm_idx = 2)
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 1, oz + 1, 2, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 1, oz + 1, 2, 0, mat, 1, 1, 1));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 1, oz + 0, 2, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 1, oz + 1, 2, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 1, oz + 0, 2, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 1, oz + 0, 2, 0, mat, 1, 1, 3));

        // Face 3: -Y (norm_idx = 3)
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 0, oz + 0, 3, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 0, oz + 0, 3, 0, mat, 1, 1, 1));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 0, oz + 1, 3, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 0, oz + 0, 3, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 0, oz + 1, 3, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 0, oz + 1, 3, 0, mat, 1, 1, 3));

        // Face 4: +Z (norm_idx = 4)
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 0, oz + 1, 4, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 0, oz + 1, 4, 0, mat, 1, 1, 1));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 1, oz + 1, 4, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 0, oz + 1, 4, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 1, oz + 1, 4, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 1, oz + 1, 4, 0, mat, 1, 1, 3));

        // Face 5: -Z (norm_idx = 5)
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 0, oz + 0, 5, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 0, oz + 0, 5, 0, mat, 1, 1, 1));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 1, oz + 0, 5, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 0, oz + 0, 5, 0, mat, 1, 1, 0));
        verts.push_back(PackedVoxelVertex::encode(ox + 0, oy + 1, oz + 0, 5, 0, mat, 1, 1, 2));
        verts.push_back(PackedVoxelVertex::encode(ox + 1, oy + 1, oz + 0, 5, 0, mat, 1, 1, 3));
    }

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
    , m_blocks(std::move(other.m_blocks))
    , m_mesh_offset(other.m_mesh_offset)
    , m_isSleeping(other.m_isSleeping)
    , m_low_velocity_timer(other.m_low_velocity_timer)
    , m_sleep_timer(other.m_sleep_timer)
    , m_destroyed(other.m_destroyed)
    , m_life_time(other.m_life_time)
    , m_world(other.m_world)
    , m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_vertex_count(other.m_vertex_count)
    , has_dealt_damage(other.has_dealt_damage)
{
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_vertex_count = 0;
    other.m_world = nullptr;
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
        m_blocks = std::move(other.m_blocks);
        m_mesh_offset = other.m_mesh_offset;
        m_isSleeping = other.m_isSleeping;
        m_low_velocity_timer = other.m_low_velocity_timer;
        m_sleep_timer = other.m_sleep_timer;
        m_destroyed = other.m_destroyed;
        m_life_time = other.m_life_time;
        m_world = other.m_world;
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_vertex_count = other.m_vertex_count;
        has_dealt_damage = other.has_dealt_damage;

        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_vertex_count = 0;
        other.m_world = nullptr;
    }
    return *this;
}

void DynamicDebris::re_voxelize_blocks(World& world) {
    CollisionResult res;
    re_voxelize_blocks(world, res);
}

void DynamicDebris::re_voxelize_blocks(World& world, CollisionResult& res) {
    int bx = static_cast<int>(std::floor(m_position.x));
    int bz = static_cast<int>(std::floor(m_position.z));
    int center_place_y = static_cast<int>(std::floor(m_position.y));

    if (m_blocks.size() > 1) {
        for (const auto& b : m_blocks) {
            int px_b = bx + b.local_offset.x;
            int pz_b = bz + b.local_offset.z;
            int py_b = center_place_y + b.local_offset.y;

            while (py_b > 0 && !world.is_solid(px_b, py_b - 1, pz_b)) {
                py_b--;
            }
            while (py_b < 127 && world.is_solid(px_b, py_b, pz_b)) {
                py_b++;
            }

            if (py_b >= 0 && py_b < 128) {
                uint8_t bmat = (b.voxel.material_id != MAT_AIR) ? b.voxel.material_id : m_material_id;
                world.SetBlock(px_b, py_b, pz_b, bmat, b.voxel.flags_and_damage);
            }
        }
        res.placed_on_ground = true;
        res.place_pos = glm::ivec3(bx, center_place_y, bz);
    } else {
        int place_x = bx;
        int place_y = center_place_y;
        int place_z = bz;

        if (place_y < 0) place_y = 0;
        if (place_y > 127) place_y = 127;

        while (place_y < 127 && world.is_solid(place_x, place_y, place_z)) {
            place_y++;
        }

        if (place_y >= 0 && place_y < 128) {
            world.SetBlock(place_x, place_y, place_z, m_material_id, 0);
            res.placed_on_ground = true;
            res.place_pos = glm::ivec3(place_x, place_y, place_z);
        }
    }

    res.shattered = false;
    res.shatter_pos = glm::vec3(res.place_pos.x + 0.5f, res.place_pos.y + 0.5f, res.place_pos.z + 0.5f);
    res.shatter_mat = m_material_id;
    res.spawned_dust_cloud = true;
}

DynamicDebris::CollisionResult DynamicDebris::update(
    float dt,
    World& world,
    const glm::vec3& player_pos,
    bool is_player_sheltered,
    const std::vector<glm::vec3>& enemy_positions
) {
    CollisionResult res;
    m_world = &world;
    if (m_destroyed) return res;

    // Sleeping state: If sleeping for > 2.5s, convert debris block positions back into static voxels in World::SetBlock() and remove the entity
    if (m_isSleeping) {
        m_sleep_timer += dt;
        if (m_sleep_timer > 2.5f) {
            re_voxelize_blocks(world, res);
            m_destroyed = true;
        }
        return res; // Halt physics integration
    }

    // Velocity low-speed detection: If linear velocity glm::length(m_velocity) < 0.08f for > 1.0s, set m_isSleeping = true and halt physics integration
    if (glm::length(m_velocity) < 0.08f) {
        m_low_velocity_timer += dt;
        if (m_low_velocity_timer > 1.0f) {
            m_isSleeping = true;
        }
        return res; // Halt physics integration while linear velocity is below threshold
    } else {
        m_low_velocity_timer = 0.0f;
    }

    // Check if resting on cavern floor/solid blocks
    int curr_bx = static_cast<int>(std::floor(m_position.x));
    int curr_bz = static_cast<int>(std::floor(m_position.z));
    int curr_by_below = static_cast<int>(std::floor(m_position.y - 0.45f));
    bool on_ground = (curr_by_below < 0 || world.is_solid(curr_bx, curr_by_below, curr_bz));
    if (on_ground && m_velocity.y <= 0.0f) {
        m_velocity.y = 0.0f;
        m_velocity.x *= 0.85f;
        m_velocity.z *= 0.85f;
        if (glm::length(m_velocity) < 0.01f) {
            m_velocity = glm::vec3(0.0f);
        }
    }

    m_life_time += dt;

    // Apply subterranean acceleration due to gravity (g = 18.0 m/s^2)
    m_velocity.y -= 18.0f * dt;

    glm::vec3 next_pos = m_position + m_velocity * dt;

    // Compute cluster bounding extents
    glm::vec3 cluster_min_offset(-0.45f);
    glm::vec3 cluster_max_offset(0.45f);
    if (!m_blocks.empty()) {
        glm::ivec3 min_b = m_blocks[0].local_offset;
        glm::ivec3 max_b = m_blocks[0].local_offset;
        for (const auto& b : m_blocks) {
            min_b = glm::min(min_b, b.local_offset);
            max_b = glm::max(max_b, b.local_offset);
        }
        cluster_min_offset = glm::vec3(min_b) - glm::vec3(0.45f);
        cluster_max_offset = glm::vec3(max_b) + glm::vec3(0.45f);
    }

    glm::vec3 d_min = next_pos + cluster_min_offset;
    glm::vec3 d_max = next_pos + cluster_max_offset;

    // Proportional kinetic crushing damage: 20 HP - 50 HP based on fall speed & cluster mass
    float fall_speed = std::abs(m_velocity.y);
    float base_crush = 20.0f + (fall_speed - 3.0f) * 1.8f;
    if (m_block_count > 1) {
        base_crush += std::min(10.0f, static_cast<float>(m_block_count - 1) * 0.5f);
    }
    float kinetic_crush_damage = std::clamp(base_crush, 20.0f, 50.0f);

    // Sweeping AABB against player capsule / bounding box
    glm::vec3 p_min = glm::vec3(player_pos.x - 0.35f, std::min(player_pos.y - 0.95f, player_pos.y - 1.6f), player_pos.z - 0.35f);
    glm::vec3 p_max = glm::vec3(player_pos.x + 0.35f, std::max(player_pos.y + 0.95f, player_pos.y + 0.2f), player_pos.z + 0.35f);

    bool player_overlap = (p_min.x <= d_max.x && p_max.y >= d_min.y) &&
                          (p_min.y <= d_max.y && p_max.y >= d_min.y) &&
                          (p_min.z <= d_max.z && p_max.z >= d_min.z);

    // Impact with player
    if (player_overlap && !has_dealt_damage && m_velocity.y < -3.0f) {
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
            res.hit_player = true;
            res.damage = kinetic_crush_damage;
            m_velocity.x += ((static_cast<float>(rand() % 100) / 50.0f) - 1.0f) * 1.5f;
            m_velocity.z += ((static_cast<float>(rand() % 100) / 50.0f) - 1.0f) * 1.5f;
        }
    }

    // Impact with enemies caught in cluster trajectory
    if (!has_dealt_damage && m_velocity.y < -3.0f) {
        for (const auto& epos : enemy_positions) {
            glm::vec3 e_min = epos - glm::vec3(0.5f, 0.5f, 0.5f);
            glm::vec3 e_max = epos + glm::vec3(0.5f, 1.2f, 0.5f);
            bool enemy_overlap = (e_min.x <= d_max.x && e_max.x >= d_min.x) &&
                                 (e_min.y <= d_max.y && e_max.y >= d_min.y) &&
                                 (e_min.z <= d_max.z && e_max.z >= d_min.z);
            if (enemy_overlap) {
                res.hit_enemy = true;
                res.enemy_damage = kinetic_crush_damage;
                res.enemy_hit_pos = epos;
                has_dealt_damage = true;
                break;
            }
        }
    }

    // Check collision against cavern floor and static voxels along downward trajectory
    int bx = static_cast<int>(std::floor(next_pos.x));
    int bz = static_cast<int>(std::floor(next_pos.z));
    bool hit_ground = false;
    int hit_solid_y = 0;

    if (m_blocks.size() <= 1) {
        int curr_bottom_y = static_cast<int>(std::floor(m_position.y - 0.45f));
        int next_bottom_y = static_cast<int>(std::floor(next_pos.y - 0.45f));
        hit_solid_y = next_bottom_y;
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
    } else {
        for (const auto& b : m_blocks) {
            int check_bx = static_cast<int>(std::floor(next_pos.x + b.local_offset.x));
            int check_bz = static_cast<int>(std::floor(next_pos.z + b.local_offset.z));
            int curr_by = static_cast<int>(std::floor(m_position.y + b.local_offset.y - 0.45f));
            int next_by = static_cast<int>(std::floor(next_pos.y + b.local_offset.y - 0.45f));
            for (int cy = curr_by; cy >= next_by; --cy) {
                if (cy < 0) {
                    hit_ground = true;
                    hit_solid_y = std::max(hit_solid_y, 0);
                    break;
                }
                if (world.is_solid(check_bx, cy, check_bz)) {
                    hit_ground = true;
                    hit_solid_y = std::max(hit_solid_y, cy - b.local_offset.y);
                    break;
                }
            }
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
            res.spawned_dust_cloud = true;
            return res;
        }

        // Place physical blocks permanently on cavern floor
        if (m_blocks.size() > 1) {
            int center_place_y = hit_solid_y + 1;
            for (const auto& b : m_blocks) {
                int px_b = bx + b.local_offset.x;
                int pz_b = bz + b.local_offset.z;
                int py_b = center_place_y + b.local_offset.y;

                while (py_b > 0 && !world.is_solid(px_b, py_b - 1, pz_b)) {
                    py_b--;
                }
                while (py_b < 127 && world.is_solid(px_b, py_b, pz_b)) {
                    py_b++;
                }

                if (py_b >= 0 && py_b < 128) {
                    uint8_t bmat = (b.voxel.material_id != MAT_AIR) ? b.voxel.material_id : m_material_id;
                    world.SetBlock(px_b, py_b, pz_b, bmat, b.voxel.flags_and_damage);
                }
            }
            res.placed_on_ground = true;
            res.place_pos = glm::ivec3(bx, center_place_y, bz);
        } else {
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
                world.SetBlock(place_x, place_y, place_z, m_material_id, 0);
                res.placed_on_ground = true;
                res.place_pos = glm::ivec3(place_x, place_y, place_z);
            }
        }

        m_destroyed = true;
        res.shattered = false;
        res.shatter_pos = glm::vec3(res.place_pos.x + 0.5f, res.place_pos.y + 0.5f, res.place_pos.z + 0.5f);
        res.shatter_mat = m_material_id;
        res.spawned_dust_cloud = true;
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

void DynamicDebris::Update(float dt) {
    if (m_world) {
        update(dt, *m_world);
    } else {
        if (m_destroyed) return;
        if (m_isSleeping) {
            m_sleep_timer += dt;
            if (m_sleep_timer > 2.5f) {
                m_destroyed = true;
            }
            return;
        }

        if (glm::length(m_velocity) < 0.08f) {
            m_low_velocity_timer += dt;
            if (m_low_velocity_timer > 1.0f) {
                m_isSleeping = true;
                return;
            }
        } else {
            m_low_velocity_timer = 0.0f;
            m_velocity.y -= 18.0f * dt;
            m_position += m_velocity * dt;
        }
    }
}

void DynamicDebris::Update(float dt, World& world) {
    update(dt, world);
}

DynamicDebris::CollisionResult DynamicDebris::Update(
    float dt,
    World& world,
    const glm::vec3& player_pos,
    bool is_player_sheltered,
    const std::vector<glm::vec3>& enemy_positions
) {
    return update(dt, world, player_pos, is_player_sheltered, enemy_positions);
}

void DynamicDebris::render() const {
    if (m_vao == 0 || m_vertex_count == 0) return;
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertex_count));
    glBindVertexArray(0);
}

} // namespace Voidfall
