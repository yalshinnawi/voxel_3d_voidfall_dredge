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
        size_t block_count,
        World* world = nullptr
    );

    DynamicDebris(
        uint32_t id,
        const glm::vec3& position,
        const glm::vec3& linear_vel,
        const glm::vec3& angular_vel,
        const std::vector<DebrisBlock>& blocks,
        uint8_t primary_material = MAT_FRACTURED_GRANITE,
        World* world = nullptr
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
    void Update(float dt);
    void Update(float dt, World& world);
    CollisionResult Update(
        float dt,
        World& world,
        const glm::vec3& player_pos,
        bool is_player_sheltered,
        const std::vector<glm::vec3>& enemy_positions = {}
    );

    void re_voxelize_blocks(World& world);
    void re_voxelize_blocks(World& world, CollisionResult& res);

    void render() const;

    uint32_t id() const { return m_id; }
    const glm::vec3& position() const { return m_position; }
    const glm::vec3& velocity() const { return m_velocity; }
    void set_velocity(const glm::vec3& v) { m_velocity = v; }
    const glm::vec3& rotation() const { return m_rotation; }
    bool is_sleeping() const { return m_isSleeping; }
    bool isSleeping() const { return m_isSleeping; }
    void set_sleeping(bool s) { m_isSleeping = s; }
    float sleep_timer() const { return m_sleep_timer; }
    float low_velocity_timer() const { return m_low_velocity_timer; }
    bool is_destroyed() const { return m_destroyed; }
    uint8_t material_id() const { return m_material_id; }
    unsigned int vao() const { return m_vao; }
    size_t vertex_count() const { return m_vertex_count; }
    size_t block_count() const { return m_block_count; }
    const std::vector<DebrisBlock>& blocks() const { return m_blocks; }
    const glm::vec3& mesh_offset() const { return m_mesh_offset; }
    World* world() const { return m_world; }
    void set_world(World* w) { m_world = w; }

    void apply_impulse(const glm::vec3& impulse) {
        m_velocity += impulse;
        m_isSleeping = false;
        m_low_velocity_timer = 0.0f;
        m_sleep_timer = 0.0f;
    }

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
    bool m_isSleeping{false};
    float m_low_velocity_timer{0.0f};
    float m_sleep_timer{0.0f};
    bool m_destroyed{false};
    float m_life_time{0.0f};
    World* m_world{nullptr};

    // GPU mesh for falling boulder / composite cluster
    unsigned int m_vao{0};
    unsigned int m_vbo{0};
    size_t m_vertex_count{0};
};

} // namespace Voidfall
