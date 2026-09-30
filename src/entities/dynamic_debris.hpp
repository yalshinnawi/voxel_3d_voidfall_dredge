#pragma once
#include "../voxel/packed_vertex.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace Voidfall {

class World;

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
    ~DynamicDebris();

    struct CollisionResult {
        bool hit_player{false};
        float damage{0.0f};
        bool hit_bulkhead{false};
        bool shattered{false};
        glm::vec3 shatter_pos{0.0f};
        uint8_t shatter_mat{0};
    };

    CollisionResult update(float dt, World& world, const glm::vec3& player_pos, bool is_player_sheltered);
    void update(float dt, World& world);
    void render() const;

    uint32_t id() const { return m_id; }
    const glm::vec3& position() const { return m_position; }
    const glm::vec3& velocity() const { return m_velocity; }
    bool is_sleeping() const { return m_sleeping; }
    bool is_destroyed() const { return m_destroyed; }
    uint8_t material_id() const { return m_material_id; }

    bool has_dealt_damage{false};

private:
    uint32_t m_id{0};
    glm::vec3 m_position{0.0f};
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_rotation{0.0f};
    glm::vec3 m_angular_velocity{0.0f};
    uint8_t m_material_id{MAT_FRACTURED_GRANITE};
    size_t m_block_count{1};
    bool m_sleeping{false};
    bool m_destroyed{false};
    float m_life_time{0.0f};

    // GPU mesh for falling boulder
    unsigned int m_vao{0};
    unsigned int m_vbo{0};
    size_t m_vertex_count{0};
};

} // namespace Voidfall
