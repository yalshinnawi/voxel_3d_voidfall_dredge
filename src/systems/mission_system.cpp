#include "mission_system.hpp"
#include "../voxel/world.hpp"
#include "../voxel/packed_vertex.hpp"
#include "../entities/enemies/void_stalker.hpp"
#include "../core/logger.hpp"
#include <algorithm>
#include <cmath>

namespace Voidfall {

MissionSystem::MissionSystem() {
    reset(1);
}

void MissionSystem::reset(int current_level) {
    m_level = current_level;
    m_vault = PrecursorVault{};
}

void MissionSystem::embed_precursor_vault(World& world, int level) {
    m_level = level;
    m_vault = PrecursorVault{};
    m_vault.exists = true;

    // Anchor the vault chamber in rock formation
    int vx = (level == 1) ? 46 : (level == 2 ? 40 : 48);
    int vy = 12;
    int vz = (level == 1) ? 44 : (level == 2 ? 48 : 38);

    m_vault.chamber_min = glm::ivec3(vx - 2, vy, vz - 2);
    m_vault.chamber_max = glm::ivec3(vx + 2, vy + 3, vz + 2);
    m_vault.door_pos = glm::ivec3(vx - 3, vy, vz);
    m_vault.relic_pos = glm::ivec3(vx, vy + 1, vz);
    m_vault.state = VaultObjectiveState::Undiscovered;

    // Build reinforced vault bulkhead enclosure
    for (int x = vx - 3; x <= vx + 3; ++x) {
        for (int y = vy - 1; y <= vy + 4; ++y) {
            for (int z = vz - 3; z <= vz + 3; ++z) {
                bool is_boundary = (x == vx - 3 || x == vx + 3 ||
                                    y == vy - 1 || y == vy + 4 ||
                                    z == vz - 3 || z == vz + 3);
                if (is_boundary) {
                    world.set_voxel(x, y, z, Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED}, false);
                } else {
                    world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, false);
                }
            }
        }
    }

    // Embed sealed Precursor Vault Bulkhead door at entry (2x3 portal)
    for (int dy = 0; dy < 3; ++dy) {
        for (int dz = -1; dz <= 0; ++dz) {
            world.set_voxel(m_vault.door_pos.x, vy + dy, vz + dz,
                            Voxel{MAT_REINFORCED_VAULT_DOOR, VOXEL_FLAG_ANCHORED}, false);
        }
    }

    // Place Relic Pedestal & Relic Hyper-Core
    world.set_voxel(vx, vy, vz, Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_ANCHORED}, false);
    world.set_voxel(vx, vy + 1, vz, Voxel{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE}, false);

    VF_LOG_INFO("MissionSystem", "Precursor Vault embedded at (" << vx << ", " << vy << ", " << vz
                << ") with reinforced vault door at X=" << m_vault.door_pos.x);
}

bool MissionSystem::check_satchel_vault_breach(World& world, const glm::vec3& blast_pos, float blast_radius) {
    if (!m_vault.exists || m_vault.state == VaultObjectiveState::Breached || m_vault.state == VaultObjectiveState::RelicRetrieved) {
        return false;
    }

    float dist_to_door = glm::distance(glm::vec3(m_vault.door_pos) + glm::vec3(0.5f, 1.5f, 0.0f), blast_pos);
    if (dist_to_door <= blast_radius + 1.5f) {
        // Breach the vault bulkhead door
        int vx = m_vault.door_pos.x;
        int vy = m_vault.door_pos.y;
        int vz = m_vault.door_pos.z;

        for (int dy = 0; dy < 3; ++dy) {
            for (int dz = -1; dz <= 0; ++dz) {
                world.set_voxel(vx, vy + dy, vz + dz, Voxel{MAT_AIR, 0}, true);
            }
        }

        m_vault.state = VaultObjectiveState::Breached;
        if (m_on_notification) {
            m_on_notification("SECONDARY OBJECTIVE: PRECURSOR VAULT BREACHED! RETRIEVE RELIC HYPER-CORE", 5.0f);
        }
        VF_LOG_INFO("MissionSystem", "Precursor Vault breached by satchel blast!");
        return true;
    }

    return false;
}

void MissionSystem::update(float /*dt*/, World& world, const glm::vec3& player_pos) {
    if (!m_vault.exists) return;

    // Check proximity for discovery
    if (m_vault.state == VaultObjectiveState::Undiscovered) {
        float dist_to_door = glm::distance(glm::vec3(m_vault.door_pos) + glm::vec3(0.5f), player_pos);
        if (dist_to_door < 8.0f) {
            m_vault.state = VaultObjectiveState::Discovered;
            if (m_on_notification) {
                m_on_notification("SECTOR ANOMALY: SEALED PRECURSOR VAULT DETECTED (BREACH WITH SATCHEL CHARGE)", 5.0f);
            }
        }
    }

    // Check relic retrieval
    if (m_vault.state == VaultObjectiveState::Breached) {
        float dist_to_relic = glm::distance(glm::vec3(m_vault.relic_pos) + glm::vec3(0.5f), player_pos);
        if (dist_to_relic < 2.5f) {
            // Pick up relic
            world.set_voxel(m_vault.relic_pos.x, m_vault.relic_pos.y, m_vault.relic_pos.z, Voxel{MAT_AIR, 0}, true);
            m_vault.state = VaultObjectiveState::RelicRetrieved;
            if (m_on_notification) {
                m_on_notification("RELIC HYPER-CORE RETRIEVED! (+350 EXP, +5 TITANIUM CORES ON EXTRACTION)", 5.0f);
            }
            VF_LOG_INFO("MissionSystem", "Relic Hyper-Core collected by player!");
        }
    }
}

int MissionSystem::ignite_gas_pocket(World& world, const glm::vec3& blast_pos, float blast_radius, VoidStalkerManager& stalkers) {
    int min_x = static_cast<int>(std::floor(blast_pos.x - blast_radius));
    int max_x = static_cast<int>(std::ceil(blast_pos.x + blast_radius));
    int min_y = std::max(0, static_cast<int>(std::floor(blast_pos.y - blast_radius)));
    int max_y = std::min(127, static_cast<int>(std::ceil(blast_pos.y + blast_radius)));
    int min_z = static_cast<int>(std::floor(blast_pos.z - blast_radius));
    int max_z = static_cast<int>(std::ceil(blast_pos.z + blast_radius));

    int ignited_voxels = 0;
    glm::vec3 gas_centroid(0.0f);

    for (int y = min_y; y <= max_y; ++y) {
        for (int z = min_z; z <= max_z; ++z) {
            for (int x = min_x; x <= max_x; ++x) {
                glm::vec3 voxel_center(x + 0.5f, y + 0.5f, z + 0.5f);
                if (glm::distance(voxel_center, blast_pos) <= blast_radius) {
                    Voxel v = world.get_voxel(x, y, z);
                    if (v.material_id == MAT_GAS || v.material_id == MAT_VOLATILE_SMOKE) {
                        world.set_voxel(x, y, z, Voxel{MAT_AIR, 0}, true);
                        ignited_voxels++;
                        gas_centroid += voxel_center;
                    }
                }
            }
        }
    }

    if (ignited_voxels > 0) {
        gas_centroid /= static_cast<float>(ignited_voxels);
        float fireball_radius = blast_radius * 1.6f;

        // Incinerate nearby aberrant swarms caught in the fireball
        for (auto& s : stalkers.stalkers_mut()) {
            if (s.is_dead()) continue;
            float dist = glm::distance(s.position, gas_centroid);
            if (dist <= fireball_radius) {
                // Lethal fire damage: instantly ignites / bursts hostile swarms
                s.hp = 0.0f;
                s.state = StalkerState::Dying;
                s.just_died = true;
                s.hit_flash_timer = 0.15f;
                s.velocity += glm::normalize(s.position - gas_centroid + glm::vec3(0.0f, 0.5f, 0.0f)) * 8.0f;
            }
        }

        if (m_on_gas_ignited) {
            m_on_gas_ignited(gas_centroid, fireball_radius);
        }

        if (m_on_notification) {
            m_on_notification("VOLATILE BIO-GAS IGNITED: FIREBALL INCINERATED ADJACENT SWARM!", 3.5f);
        }
    }

    return ignited_voxels;
}

std::string MissionSystem::get_secondary_objective_text() const {
    switch (m_vault.state) {
        case VaultObjectiveState::Undiscovered:
            return "SEC: LOCATE PRECURSOR VAULT";
        case VaultObjectiveState::Discovered:
            return "SEC: BREACH VAULT BULKHEAD (SATCHEL)";
        case VaultObjectiveState::Breached:
            return "SEC: RETRIEVE RELIC HYPER-CORE";
        case VaultObjectiveState::RelicRetrieved:
            return "SEC: RELIC SECURED (+350 EXP / +5 Ti)";
    }
    return "";
}

} // namespace Voidfall
