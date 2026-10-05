#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include "../voxel/packed_vertex.hpp"

namespace Voidfall {

class World;

enum class VaultDoorState : uint8_t {
    Intact = 0,
    Breached
};

class VaultDoor {
public:
    VaultDoor() = default;
    VaultDoor(const glm::ivec3& pos, int height = 3);

    // Door properties
    const glm::ivec3& position() const { return m_door_pos; }
    int height() const { return m_height; }
    VaultDoorState state() const { return m_state; }
    bool is_breached() const { return m_state == VaultDoorState::Breached; }
    bool is_drill_immune() const { return true; }
    float voxel_health() const { return m_voxel_health; }

    // Drill resistance: drill damage is resisted, health remains 100% (1.0f)
    bool apply_drill_damage(float damage);

    // Satchel blast breach: converts door voxels to air and marks door breached
    bool apply_satchel_blast(const glm::vec3& blast_pos, float blast_radius, World& world);

    // Relic pedestal interaction
    bool is_relic_interactable() const { return m_state == VaultDoorState::Breached && !m_relic_secured; }
    bool is_relic_secured() const { return m_relic_secured; }
    const glm::ivec3& relic_position() const { return m_relic_pos; }
    void set_relic_position(const glm::ivec3& pos) { m_relic_pos = pos; }

    bool interact_relic(int& out_exp, int& out_titanium);

    void reset();

private:
    glm::ivec3 m_door_pos{0};
    glm::ivec3 m_relic_pos{0};
    int m_height{3};
    float m_voxel_health{1.0f}; // 100%
    VaultDoorState m_state{VaultDoorState::Intact};
    bool m_relic_secured{false};
};

} // namespace Voidfall
