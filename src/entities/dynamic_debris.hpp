#pragma once
#include "../voxel/packed_vertex.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace Voidfall {

class World;

struct DebrisBlock {
    glm::ivec3 local_offset{0}; // Relative to cluster center of mass
    Voxel voxel{MAT_FRACTURED_GRANITE, 0};
};

class DynamicDebris {
public:
    DynamicDebris(
        uint32_t id,
        const glm::vec3& position,
        const glm::vec3& linear_vel,
        const glm::vec3& angular_vel,
        uint8_t material_id,
        size_t block_count
    );

    DynamicDebris(
        uint32_t id,
        const glm::vec3& position,
        const glm::vec3& linear_vel,
        const glm::vec3& angular_vel,
        const std::vector<DebrisBlock>& blocks,
        uint8_t primary_material = MAT_FRACTURED_GRANITE
    );

    ~DynamicDebris();

    // Non-copyable (owns OpenGL GPU resources)
    DynamicDebris(const DynamicDebris&) = delete;
    DynamicDebris& operator=(const DynamicDebris&) = delete;

    // Movable
    DynamicDebris(DynamicDebris&& other) noexcept;
    DynamicDebris& operator=(DynamicDebris&& other) noexcept;

    struct CollisionResult {
        bool hit_player{false};
        float damage{0.0f};
        bool hit_bulkhead{false};
        bool placed_on_ground{false};
        glm::ivec3 place_pos{0};
        bool shattered{false};
        glm::vec3 shatter_pos{0.0f};
        uint8_t shatter_mat{0};
        bool hit_enemy{false};
        float enemy_damage{0.0f};
        glm::vec3 enemy_hit_pos{0.0f};
        bool spawned_dust_cloud{false};
    };

    CollisionResult update(
        float dt,
        World& world,
        const glm::vec3& player_pos,
        bool is_player_sheltered,
        const std::vector<glm::vec3>& enemy_positions = {}
    );
    void update(float dt, World& world);
    void render() const;

    uint32_t id() const { return m_id; }
    const glm::vec3& position() const { return m_position; }
    const glm::vec3& velocity() const { return m_velocity; }
    const glm::vec3& rotation() const { return m_rotation; }
    bool is_sleeping() const { return m_sleeping; }
    bool is_destroyed() const { return m_destroyed; }
    uint8_t material_id() const { return m_material_id; }
    unsigned int vao() const { return m_vao; }
    size_t vertex_count() const { return m_vertex_count; }
    size_t block_count() const { return m_block_count; }
    const std::vector<DebrisBlock>& blocks() const { return m_blocks; }
    const glm::vec3& mesh_offset() const { return m_mesh_offset; }

    void apply_impulse(const glm::vec3& impulse) { m_velocity += impulse; m_sleeping = false; }

    bool has_dealt_damage{false};

private:
    void build_mesh();

    uint32_t m_id{0};
    glm::vec3 m_position{0.0f};
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_rotation{0.0f};
    glm::vec3 m_angular_velocity{0.0f};
    uint8_t m_material_id{MAT_FRACTURED_GRANITE};
    size_t m_block_count{1};
    std::vector<DebrisBlock> m_blocks;
    glm::vec3 m_mesh_offset{0.0f};
    bool m_sleeping{false};
    bool m_destroyed{false};
    float m_life_time{0.0f};

    // GPU mesh for falling boulder / composite cluster
    unsigned int m_vao{0};
    unsigned int m_vbo{0};
    size_t m_vertex_count{0};
};

} // namespace Voidfall
