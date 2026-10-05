#include "vault_door.hpp"
#include "../voxel/world.hpp"
#include "../core/logger.hpp"
#include <algorithm>

namespace Voidfall {

VaultDoor::VaultDoor(const glm::ivec3& pos, int height)
    : m_door_pos(pos)
    , m_height(height)
    , m_voxel_health(1.0f)
    , m_state(VaultDoorState::Intact)
    , m_relic_secured(false)
{
}

bool VaultDoor::apply_drill_damage(float /*damage*/) {
    // Vault bulkhead door is completely immune to mining drill damage
    m_voxel_health = 1.0f; // Remains 100%
    return false;
}

bool VaultDoor::apply_satchel_blast(const glm::vec3& blast_pos, float blast_radius, World& world) {
    if (m_state == VaultDoorState::Breached) {
        return false;
    }

    glm::vec3 door_center = glm::vec3(m_door_pos) + glm::vec3(0.5f, static_cast<float>(m_height) * 0.5f, 0.5f);
    float dist = glm::distance(door_center, blast_pos);

    if (dist <= blast_radius + 2.0f) {
        m_state = VaultDoorState::Breached;
        m_voxel_health = 0.0f;

        // Convert the 3-block-tall door column to air
        for (int dy = 0; dy < m_height; ++dy) {
            world.set_voxel(m_door_pos.x, m_door_pos.y + dy, m_door_pos.z, Voxel{MAT_AIR, 0}, true);
        }

        VF_LOG_INFO("VaultDoor", "Precursor Vault bulkhead fractured and breached by satchel charge!");
        return true;
    }

    return false;
}

bool VaultDoor::interact_relic(int& out_exp, int& out_titanium) {
    if (!is_relic_interactable()) {
        return false;
    }

    m_relic_secured = true;
    out_exp = 350;
    out_titanium = 5;
    VF_LOG_INFO("VaultDoor", "Precursor Relic Hyper-Core secured! +350 EXP, +5 Titanium Cores.");
    return true;
}

void VaultDoor::reset() {
    m_voxel_health = 1.0f;
    m_state = VaultDoorState::Intact;
    m_relic_secured = false;
}

} // namespace Voidfall
