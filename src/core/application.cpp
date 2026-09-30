#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "application.hpp"
#include "logger.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace Voidfall {

Application::Application(const AppConfig& config)
    : m_config(config)
{
    init_systems();
}

Application::~Application() {
}

void Application::init_systems() {
    // 1. Window
    WindowConfig win_cfg;
    win_cfg.title = "Voidfall: Dredge (Multiplayer Edition) - [" +
                    std::string(m_config.is_host ? "HOST" : "CLIENT") + "]";
    win_cfg.width = 1600;
    win_cfg.height = 900;
    m_window = std::make_unique<Window>(win_cfg);

    // Start with cursor unlocked for menus
    m_window->set_cursor_locked(false);

    m_window->set_resize_callback([this](int w, int h) {
        if (m_renderer) m_renderer->resize(w, h);
        if (m_hud) m_hud->resize(w, h);
        if (m_hub_ui) m_hub_ui->resize(w, h);
        if (m_pause_menu) m_pause_menu->resize(w, h);
    });

    // 2. Renderer
    m_renderer = std::make_unique<Renderer>(m_window->width(), m_window->height());

    // 3. Voxel World & Procedural Caverns
    m_world = std::make_unique<World>(m_config.world_seed);

    // 4. Player Controller
    m_player = std::make_unique<PlayerController>(glm::vec3(16.0f, 22.0f, 16.0f));

    m_player->set_on_block_break([this](int x, int y, int z, const glm::ivec3& normal, uint8_t mat, uint8_t flags) {
        on_block_broken(x, y, z, normal, mat, flags);
    });

    m_window->set_key_callback([this](int key, int action) {
        process_input(key, action);
    });

    m_player->set_on_block_place([this](int x, int y, int z, uint8_t mat) {
        on_block_placed(x, y, z, mat);
    });

    m_player->set_on_bulkhead_dismantle([this](int x, int y, int z) {
        on_bulkhead_dismantled(x, y, z);
    });

    m_player->set_can_place_predicate([this]() {
        return m_inventory.has_bulkhead_material();
    });

    m_player->set_on_warning([this](const std::string& msg) {
        if (m_hud) m_hud->show_warning(msg);
    });

    m_player->set_on_sonar_cast([this](const glm::vec3& origin) {
        on_sonar_cast(origin);
    });

    m_player->set_on_explosive_blast([this](const glm::ivec3& origin, const glm::ivec3& dir, bool is_micro) {
        on_explosive_blast(origin, dir, is_micro);
    });

    // 5. Systems
    setup_hazard_system();

    m_extraction = std::make_unique<ExtractionSystem>();
    m_extraction->set_on_complete([this]() {
        m_expedition_success = true;
        m_inventory.finalize_run(m_selected_level, true);
        m_state = GameState::Debrief;
        m_window->set_cursor_locked(false);
        std::cout << "[Extraction] Delver extraction complete! Returning to debrief." << std::endl;
    });

    // 6. UI systems
    m_hud = std::make_unique<HUD>(m_window->width(), m_window->height());
    m_hub_ui = std::make_unique<OrbitalHubUI>(m_window->width(), m_window->height());
    m_pause_menu = std::make_unique<PauseMenu>(m_window->width(), m_window->height());

    // 7. Networking
    if (m_config.is_host) {
        m_host = std::make_unique<NetHost>(m_config.port);
        m_host->start();
        m_host->set_on_block_delta([this](const BlockDeltaPacket& delta) {
            int wx = delta.chunk_x * CHUNK_SIZE + (delta.local_block_idx % CHUNK_SIZE);
            int wy = delta.chunk_y * CHUNK_SIZE + ((delta.local_block_idx / CHUNK_SIZE) % CHUNK_SIZE);
            int wz = delta.chunk_z * CHUNK_SIZE + (delta.local_block_idx / (CHUNK_SIZE * CHUNK_SIZE));
            m_world->set_voxel(wx, wy, wz, Voxel{delta.material_id, delta.flags_and_damage}, true);
        });
    } else {
        m_client = std::make_unique<NetClient>();
        m_client->connect_to_host(m_config.connect_ip, m_config.port, m_config.player_name);
        m_client->set_on_seed_received([this](uint32_t seed) {
            m_world->set_seed(seed);
        });
        m_client->set_on_block_delta([this](const BlockDeltaPacket& delta) {
            int wx = delta.chunk_x * CHUNK_SIZE + (delta.local_block_idx % CHUNK_SIZE);
            int wy = delta.chunk_y * CHUNK_SIZE + ((delta.local_block_idx / CHUNK_SIZE) % CHUNK_SIZE);
            int wz = delta.chunk_z * CHUNK_SIZE + (delta.local_block_idx / (CHUNK_SIZE * CHUNK_SIZE));
            m_world->set_voxel(wx, wy, wz, Voxel{delta.material_id, delta.flags_and_damage}, true);
        });
        m_client->set_on_debris_spawn([this](const DynamicDebrisSpawnPacket& pkt) {
            m_debris.emplace_back(pkt.debris_id, pkt.origin, pkt.linear_velocity,
                                 pkt.angular_velocity, pkt.material_id, pkt.block_count);
        });
        m_client->set_on_sonar_ping([this](const SonarPingPacket& pkt) {
            m_renderer->trigger_sonar_pulse(pkt.origin);
        });
    }

    // Pre-generate initial chunks around spawn
    m_world->update(m_player->position(), 2);
}

void Application::start_expedition(int level) {
    VF_LOG_INFO("Expedition", "Launching expedition into Sector " << level
        << (level == 1 ? " [Crystalline Caverns - High Voidite Clusters]" :
            level == 2 ? " [Subterranean Vault - Reinforced Basalt Vaults]" :
                         " [Fault-Line Collapse - Vertical Abyss Fissures]"));
    m_selected_level = level;
    m_inventory.reset(level == 3 ? 50 : 25);
    m_expedition_time = 0.0f;

    // Apply Delver Upgrades
    float drill_mult = 1.0f + 0.20f * m_inventory.upgrades.drill_speed_level;
    m_player->set_drill_speed_multiplier(drill_mult);

    float thruster_mult = 1.0f + 0.25f * m_inventory.upgrades.thruster_energy_level;
    m_player->set_thruster_regen_multiplier(thruster_mult);

    m_inventory.max_bulkheads = 20 + m_inventory.upgrades.max_bulkheads_level * 5;
    m_inventory.bulkheads = std::min(m_inventory.bulkheads, m_inventory.get_max_bulkheads());

    // Generate Level Sector Procedural Topology
    m_world->generate_world(level, m_world->seed());
    m_world->update(m_player->position(), 2);

    m_player->set_position(glm::vec3(16.0f, 22.0f, 16.0f));
    m_player->exo_mut().integrity = 100.0f + 25.0f * m_inventory.upgrades.shield_plating_level;
    m_player->exo_mut().power = 100.0f;
    m_player->exo_mut().heat = 0.0f;
    m_player->exo_mut().overheated = false;

    // Reset systems
    setup_hazard_system();
    m_hazard->set_sector_parameters(level);

    m_extraction = std::make_unique<ExtractionSystem>();
    m_extraction->set_on_complete([this]() {
        m_expedition_success = true;
        m_inventory.finalize_run(m_selected_level, true); // Safe evac!
        m_state = GameState::Debrief;
        m_window->set_cursor_locked(false);
    });

    if (level == 3) {
        m_level3_timer = 180.0f; // 3-minute strict countdown
    }

    m_window->set_cursor_locked(true);
    m_state = GameState::Gameplay;
}

void Application::on_block_broken(int x, int y, int z, const glm::ivec3& normal, uint8_t mat, uint8_t flags) {
    if (m_host) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        uint16_t idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        m_host->broadcast_block_delta(cx, cy, cz, idx, MAT_AIR, 0);
    } else if (m_client) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        BlockDeltaPacket pkt;
        pkt.chunk_x = cx; pkt.chunk_y = cy; pkt.chunk_z = cz;
        pkt.local_block_idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        pkt.material_id = MAT_AIR;
        pkt.flags_and_damage = 0;
        m_client->send_block_delta(pkt);
    }

    // Spawn 12 dynamic billboard debris quads
    m_renderer->spawn_break_particles(glm::vec3(x, y, z), normal, mat);

    // Update inventory, floating text, and award Demolitions XP
    glm::vec3 popup_pos(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.8f, static_cast<float>(z) + 0.5f);

    // Anti-exploit check: Player-placed voxels
    bool is_player_placed = (flags & VOXEL_FLAG_PLAYER_PLACED) != 0;
    if (is_player_placed) {
        // Award 0 score, 0 XP, and 0 rare loot drops for player-placed voxels.
        // Only refund the base 1 Bulkhead plate if dismantled/broken with tool.
        if (mat == MAT_INDUSTRIAL_BULKHEAD) {
            m_inventory.refund_bulkhead();
            m_hud->add_floating_loot(popup_pos, "+1 BULKHEAD REFUNDED", glm::vec4(0.7f, 0.8f, 0.9f, 0.9f));
        }
        return;
    }

    if (mat == MAT_VOIDITE_CRYSTAL) {
        m_inventory.add_voidite(10);
        m_hud->add_floating_loot(popup_pos, "+10 VOIDITE (+50 PTS)", glm::vec4(0.0f, 0.94f, 1.0f, 1.0f)); // Glowing Bright Cyan (#00F0FF)
        m_skills.add_demolitions_xp(15);
    } else if (mat == MAT_INDUSTRIAL_BULKHEAD) {
        m_inventory.add_titanium(2); // Grants +2 Titanium Cores
        m_hud->add_floating_loot(popup_pos, "+2 TITANIUM (+12 PTS)", glm::vec4(1.0f, 0.70f, 0.0f, 1.0f)); // Bright Golden-Orange (#FFB300)
        m_skills.add_demolitions_xp(15);
        if (m_inventory.bulkheads == 0 && m_inventory.titanium >= 1) {
            m_inventory.bulkheads++;
            m_inventory.titanium--;
            if (m_hud) m_hud->show_warning("+1 BULKHEAD FABRICATED FROM TITANIUM", 2.5f);
        }
    } else if (mat == MAT_FRACTURED_GRANITE) {
        if (m_inventory.add_scrap_from_granite()) {
            if (m_hud) m_hud->show_warning("+1 BULKHEAD FABRICATED FROM SCRAP", 2.5f);
        }
        m_hud->add_floating_loot(popup_pos, "+1 SCRAP METAL", glm::vec4(0.85f, 0.85f, 0.9f, 1.0f));
        m_skills.add_demolitions_xp(5);
    } else if (mat == MAT_RADIOACTIVE_ORE) {
        m_inventory.add_salvage(15);
        m_hud->add_floating_loot(popup_pos, "+15 RADIOACTIVE (+80 PTS)", glm::vec4(0.2f, 1.0f, 0.35f, 1.0f));
        m_skills.add_demolitions_xp(20);
    } else if (mat == MAT_REINFORCED_VAULT_DOOR) {
        m_inventory.vault_breached = true;
        m_inventory.add_relic();
        m_hud->add_floating_loot(popup_pos, "+1 RELIC HYPER-CORE (+250 PTS)", glm::vec4(1.0f, 0.82f, 0.2f, 1.0f));
        m_skills.add_demolitions_xp(50);
        m_trauma = std::min(m_trauma + 0.35f, 1.0f);
    } else {
        m_inventory.add_salvage(1);
        m_hud->add_floating_loot(popup_pos, "+1 SALVAGE (+10 PTS)", glm::vec4(0.7f, 0.75f, 0.8f, 1.0f));
        m_skills.add_demolitions_xp(5);
    }

    // Authoritative Anchored Island BFS Cave-in Solver
    if (m_config.is_host) {
        auto unanchored = StructuralCheck::solve_cavein(*m_world, x, y, z, 512);
        for (const auto& island : unanchored) {
            uint32_t did = m_next_debris_id++;
            glm::vec3 vel(0.0f, -1.5f, 0.0f);
            glm::vec3 rot(0.2f, 0.5f, 0.1f);
            m_debris.emplace_back(did, island.center_of_mass, vel, rot, island.primary_material, island.blocks.size());
            if (m_host) {
                m_host->broadcast_debris_spawn(did, island.center_of_mass, vel, island.primary_material, island.blocks.size());
            }
        }
    }
}

void Application::on_block_placed(int x, int y, int z, uint8_t mat) {
    if (mat == MAT_INDUSTRIAL_BULKHEAD) {
        m_inventory.consume_bulkhead();
    }

    if (m_host) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        uint16_t idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        m_host->broadcast_block_delta(cx, cy, cz, idx, mat, 0);
    } else if (m_client) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        BlockDeltaPacket pkt;
        pkt.chunk_x = cx; pkt.chunk_y = cy; pkt.chunk_z = cz;
        pkt.local_block_idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        pkt.material_id = mat;
        pkt.flags_and_damage = 0;
        m_client->send_block_delta(pkt);
    }
}

void Application::on_bulkhead_dismantled(int x, int y, int z) {
    // 1. Remove voxel cleanly from world without triggering cave-in check
    m_world->set_voxel(x, y, z, Voxel{MAT_AIR, 0}, true);

    // 2. Refund bulkhead material to inventory
    m_inventory.refund_bulkhead();

    // 3. Floating feedback
    glm::vec3 popup_pos(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.8f, static_cast<float>(z) + 0.5f);
    if (m_hud) {
        m_hud->add_floating_loot(popup_pos, "+1 BULKHEAD RETRIEVED", glm::vec4(1.0f, 0.70f, 0.0f, 1.0f));
    }

    // 4. Particle effect
    if (m_renderer) {
        m_renderer->spawn_break_particles(glm::vec3(x, y, z), glm::ivec3(0, 1, 0), MAT_INDUSTRIAL_BULKHEAD);
    }

    // 5. Network broadcast
    if (m_host) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        uint16_t idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        m_host->broadcast_block_delta(cx, cy, cz, idx, MAT_AIR, 0);
    } else if (m_client) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        BlockDeltaPacket pkt;
        pkt.chunk_x = cx; pkt.chunk_y = cy; pkt.chunk_z = cz;
        pkt.local_block_idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        pkt.material_id = MAT_AIR;
        pkt.flags_and_damage = 0;
        m_client->send_block_delta(pkt);
    }
}

void Application::on_sonar_cast(const glm::vec3& origin) {
    float radius = m_skills.get_sonar_radius() + 5.0f * m_inventory.upgrades.sonar_range_level;
    m_surveying.trigger_scan(origin, *m_world, radius);
    m_renderer->trigger_sonar_pulse(origin);
    m_skills.add_surveying_xp(40);

    if (m_host) {
        m_host->broadcast_sonar_ping(1, origin, radius * 2.0f);
    } else if (m_client) {
        m_client->send_sonar_ping(origin);
    }
}

void Application::on_explosive_blast(const glm::ivec3& origin, const glm::ivec3& dir, bool is_micro) {
    if (is_micro) {
        // Surgical 1x1x3 directional blast that removes a tunnel without destroying adjacent fragile ore
        glm::ivec3 forward_step = (glm::length(glm::vec3(dir)) > 0.1f) ? -dir : glm::ivec3(0, 0, -1);
        for (int step = 0; step < 3; ++step) {
            glm::ivec3 bpos = origin + forward_step * step;
            Voxel v = m_world->get_voxel(bpos.x, bpos.y, bpos.z);
            if (v.is_solid() && v.material_id != MAT_DREDGE_BEDROCK) {
                // Do not destroy adjacent voidite ore surgically
                if (v.material_id != MAT_VOIDITE_CRYSTAL) {
                    m_world->set_voxel(bpos.x, bpos.y, bpos.z, Voxel{MAT_AIR, 0}, true);
                    on_block_broken(bpos.x, bpos.y, bpos.z, -forward_step, v.material_id, v.flags_and_damage);
                }
            }
        }
        m_trauma = std::min(m_trauma + 0.2f, 1.0f);
    } else {
        // Heavy Demolition Charge: 3x3x3 spherical blast radius
        if (m_inventory.demolition_charges <= 0) return;
        m_inventory.demolition_charges--;

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    glm::ivec3 bpos = origin + glm::ivec3(dx, dy, dz);
                    Voxel v = m_world->get_voxel(bpos.x, bpos.y, bpos.z);
                    if (v.is_solid() && v.material_id != MAT_DREDGE_BEDROCK) {
                        m_world->set_voxel(bpos.x, bpos.y, bpos.z, Voxel{MAT_AIR, 0}, true);
                        on_block_broken(bpos.x, bpos.y, bpos.z, glm::ivec3(0, 1, 0), v.material_id, v.flags_and_damage);
                    }
                }
            }
        }
        m_trauma = std::min(m_trauma + 0.45f, 1.0f);
    }
}

void Application::setup_hazard_system() {
    m_hazard = std::make_unique<HazardClock>();

    // 0.75 seconds before ceiling blocks detach:
    m_hazard->set_on_tremor_warning([this]() {
        if (m_hud) {
            m_hud->show_warning("[!] INCOMING TECTONIC SPALL - EVADE [!]", 2.0f);
        }
        glm::vec3 p = m_player->position();
        for (int i = 0; i < 6; ++i) {
            float ox = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 3.5f;
            float oz = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 3.5f;
            float oy = 5.0f + static_cast<float>(rand() % 5);
            m_renderer->spawn_break_particles(p + glm::vec3(ox, oy, oz), glm::ivec3(0, -1, 0), MAT_FRACTURED_GRANITE);
        }
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(2.0f);
        }
    });

    m_hazard->set_on_tremor([this](float intensity) {
        m_player->add_trauma(intensity * 0.45f);
        m_trauma = std::min(m_trauma + intensity * 0.45f, 1.0f);
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(3.5f);
        }

        // Seismic Detachment Pass: locate 3-8 candidate blocks 3-12 units above player within 6-block radius
        auto detach_blocks = StructuralCheck::query_seismic_detachment_blocks(
            *m_world,
            m_player->position(),
            3, 8,   // 3 to 8 blocks
            6.0f,   // 6-block radius
            3, 12   // 3 to 12 units directly above
        );

        for (const auto& b : detach_blocks) {
            Voxel v = m_world->get_voxel(b.x, b.y, b.z);
            m_world->set_voxel(b.x, b.y, b.z, Voxel{MAT_AIR, 0}, true);
            on_block_broken(b.x, b.y, b.z, glm::ivec3(0, -1, 0), v.material_id, v.flags_and_damage);

            uint32_t did = m_next_debris_id++;
            float rvx = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 1.5f;
            float rvz = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 1.5f;
            glm::vec3 vel(rvx, -2.5f - static_cast<float>(rand() % 100) / 50.0f, rvz);
            glm::vec3 rot(0.3f, 0.6f, 0.2f);
            m_debris.emplace_back(did, glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f), vel, rot, v.material_id, 1);
            if (m_host) {
                m_host->broadcast_debris_spawn(did, glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f), vel, v.material_id, 1);
            }
        }
    });
}

void Application::process_input(int key, int action) {
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE) {
        if (m_state == GameState::Gameplay) {
            m_state = GameState::Paused;
            m_window->set_cursor_locked(false);
        } else if (m_state == GameState::Paused) {
            m_state = GameState::Gameplay;
            m_window->set_cursor_locked(true);
        } else if (m_state == GameState::OrbitalHub) {
            m_state = GameState::MainMenu;
        } else if (m_state == GameState::Debrief) {
            m_state = GameState::OrbitalHub;
        } else if (m_state == GameState::MainMenu) {
            glfwSetWindowShouldClose(m_window->handle(), GLFW_TRUE);
        }
    }
}

void Application::fixed_tick(float dt) {
    m_current_tick++;

    if (m_state != GameState::Gameplay) {
        return;
    }

    m_expedition_time += dt;

    // 1. Poll networking
    if (m_host) m_host->poll_network_events();
    if (m_client) {
        m_client->poll_network_events();
        PlayerInputPacket input = m_player->build_input_packet(m_current_tick, dt);
        m_client->send_input(input);
    }

    // Synchronize skill perks to PlayerController
    m_player->set_reel_speed_multiplier(m_skills.get_grapple_reel_multiplier());
    m_player->set_thruster_regen_multiplier(m_skills.get_thruster_regen_multiplier());
    m_player->set_allow_micro_charges(m_skills.has_micro_charges());

    // 2. Physics & Player simulation
    m_player->update_physics(dt, *m_world);

    // Progression XP: Acrobatics for grappling, Suit for sprinting
    if (m_player->grapple().active) {
        m_skills.add_acrobatics_xp(1);
    }
    m_skills.add_suit_xp(1);

    // 3. Falling Debris simulation & player hazard damage
    glm::vec3 player_pos = m_player->position();
    int p_head_x = static_cast<int>(std::floor(player_pos.x));
    int p_head_y = static_cast<int>(std::floor(player_pos.y));
    int p_head_z = static_cast<int>(std::floor(player_pos.z));

    // Check Bulkhead shelter utility: player standing beneath a placed MAT_BULKHEAD
    bool covered_by_bulkhead = false;
    for (int check_y = p_head_y + 1; check_y <= p_head_y + 14; ++check_y) {
        for (int cdx = -1; cdx <= 1; ++cdx) {
            for (int cdz = -1; cdz <= 1; ++cdz) {
                Voxel v = m_world->get_voxel(p_head_x + cdx, check_y, p_head_z + cdz);
                if (v.material_id == MAT_INDUSTRIAL_BULKHEAD) {
                    covered_by_bulkhead = true;
                    break;
                }
            }
            if (covered_by_bulkhead) break;
        }
        if (covered_by_bulkhead) break;
    }

    for (auto it = m_debris.begin(); it != m_debris.end();) {
        auto res = it->update(dt, *m_world, player_pos, covered_by_bulkhead);

        if (res.shattered) {
            m_renderer->spawn_break_particles(res.shatter_pos, glm::ivec3(0, 1, 0), res.shatter_mat);
        }

        if (res.hit_player) {
            m_player->exo_mut().integrity = std::max(0.0f, m_player->exo_mut().integrity - res.damage);
            m_player->add_trauma(0.35f);
            m_trauma = std::min(m_trauma + 0.35f, 1.0f);
            m_hud->trigger_damage_flash(0.7f);
            m_hud->show_warning("CRITICAL IMPACT: DELVER CRUSHED BY FALLING DEBRIS (-" + std::to_string(static_cast<int>(res.damage)) + "%)!", 3.0f);
        } else if (res.hit_bulkhead) {
            m_hud->show_warning("TITANIUM BULKHEAD DEFLECTED FALLING DEBRIS!", 2.0f);
        }

        if (it->is_destroyed()) {
            it = m_debris.erase(it);
        } else {
            ++it;
        }
    }

    // 4. Update Hazard Clock, Extraction, Surveying, and Particles
    m_hazard->update(dt);
    m_extraction->update(dt, m_player->position());
    m_surveying.update(dt);
    m_hud->update(dt);
    m_renderer->update_particles(dt);

    // Radiation hazard damage at 100%
    if (m_hazard->radiation_level() >= 100.0f) {
        m_player->exo_mut().integrity = std::max(0.0f, m_player->exo_mut().integrity - dt * 25.0f);
    }

    // Check Level 3 strict countdown
    if (m_selected_level == 3) {
        m_level3_timer -= dt;
        if (m_level3_timer <= 0.0f) {
            m_inventory.suit_failed = true;
            m_inventory.finalize_run(m_selected_level, false);
            m_expedition_success = false;
            m_state = GameState::Debrief;
            m_window->set_cursor_locked(false);
            m_hud->show_warning("TECTONIC COLLAPSE PROTOCOL EXPIRED // DELVER M.I.A.", 4.0f);
        }
    }

    // Check player health / suit integrity failure
    if (m_player->exo().integrity <= 0.0f) {
        m_inventory.suit_failed = true;
        m_inventory.finalize_run(m_selected_level, false);
        m_expedition_success = false;
        m_state = GameState::Debrief;
        m_window->set_cursor_locked(false);
        m_hud->show_warning("SUIT INTEGRITY COMPROMISED // DELVER M.I.A.", 4.0f);
    }

    // 5. Stream chunks around player
    m_world->update(m_player->position(), 2);
}

void Application::render(float dt) {
    m_world->upload_dirty_chunks();

    glm::mat4 view;
    glm::vec3 cam_pos;

    if (m_state == GameState::Gameplay || m_state == GameState::Paused) {
        view = m_player->get_view_matrix();
        cam_pos = m_player->position();

        // Camera view setup with non-linear impulse trauma screen shake (clamped to prevent wall clipping)
        if (m_trauma > 0.001f) {
            float shake_amount = std::min(m_trauma * m_trauma * 0.35f, 0.15f);
            float shake_x = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * shake_amount;
            float shake_y = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * shake_amount;
            view = glm::translate(view, glm::vec3(shake_x, shake_y, 0.0f));
            if (m_state == GameState::Gameplay) {
                m_trauma = (std::max)(0.0f, m_trauma - dt * 0.8f);
            }
        }

        m_renderer->headlamp().position = m_player->position();
        m_renderer->headlamp().direction = m_player->forward();
    } else {
        // Cinematic rotating cavern background for Main Menu, Orbital Hub, and Debrief
        static float menu_cam_angle = 0.0f;
        menu_cam_angle += dt * 0.15f;
        float radius = 10.0f;
        cam_pos = glm::vec3(16.0f + std::sin(menu_cam_angle) * radius, 22.0f, 16.0f + std::cos(menu_cam_angle) * radius);
        view = glm::lookAt(cam_pos, glm::vec3(16.0f, 18.0f, 16.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_renderer->headlamp().position = cam_pos;
        m_renderer->headlamp().direction = glm::normalize(glm::vec3(16.0f, 18.0f, 16.0f) - cam_pos);
    }

    glm::mat4 proj = Renderer::create_projection(m_window->aspect_ratio(), m_settings.fov);

    // Setup dynamic point lights (e.g. extraction beacon rotating siren)
    m_renderer->clear_point_lights();
    if (m_extraction->phase() == ExtractionPhase::BeaconDeployed || m_extraction->phase() == ExtractionPhase::PodLanded) {
        float rot_time = static_cast<float>(glfwGetTime()) * 4.0f;
        glm::vec3 siren_offset(std::cos(rot_time) * 1.5f, 2.0f, std::sin(rot_time) * 1.5f);

        PointLight beacon_light;
        beacon_light.position = m_extraction->beacon_position() + siren_offset;
        beacon_light.color = glm::vec3(1.0f, 0.1f, 0.05f); // Emergency rotating red siren
        beacon_light.radius = 28.0f;
        beacon_light.intensity = 5.0f * (0.6f + 0.4f * m_extraction->siren_pulse());
        m_renderer->add_point_light(beacon_light);

        if (m_extraction->phase() == ExtractionPhase::PodLanded) {
            PointLight pod_light;
            pod_light.position = m_extraction->beacon_position() + glm::vec3(0.0f, 3.5f, 0.0f);
            pod_light.color = glm::vec3(0.1f, 0.9f, 1.0f);
            pod_light.radius = 35.0f;
            pod_light.intensity = 4.0f;
            m_renderer->add_point_light(pod_light);
        }
    }

    // Begin HDR Frame
    m_renderer->begin_frame(view, proj, cam_pos);

    // Render Chunks
    for (const auto& [pos, chunk] : m_world->chunks()) {
        m_renderer->render_chunk(*chunk);
    }

    // Render Falling Dynamic Debris
    for (const auto& d : m_debris) {
        d.render();
    }

    // Render 3D Extraction Beacon (tripod base, light column, drifting rings, rotating siren)
    if (m_extraction->phase() == ExtractionPhase::BeaconDeployed || m_extraction->phase() == ExtractionPhase::PodLanded) {
        bool is_landed = (m_extraction->phase() == ExtractionPhase::PodLanded);
        m_renderer->render_extraction_beacon(m_extraction->beacon_position(), m_extraction->siren_pulse(), static_cast<float>(glfwGetTime()), is_landed);
    }

    // Render Grapple Cable (Module 4)
    if (m_player->grapple().active) {
        glm::vec3 tool_pos = m_player->position() + m_player->forward() * 0.4f + m_player->right() * 0.25f - glm::vec3(0.0f, 0.2f, 0.0f);
        m_renderer->render_grapple_cable(tool_pos, m_player->grapple().anchor_point);
    }

    // Render Volumetric Sonar Ping with X-Ray Wireframe (Module 2)
    if (m_surveying.is_active()) {
        m_renderer->render_sonar_wireframes(m_surveying.surveyed_voxels(), m_surveying.alpha());
    }

    // End HDR Frame & Post-Processing Tonemap
    m_renderer->end_frame(dt, m_hazard->radiation_level());

    // Mouse coordinates and clicks for responsive menus
    glm::dvec2 cur_pos = m_window->get_cursor_pos();
    float mouse_x = static_cast<float>(cur_pos.x);
    float mouse_y = static_cast<float>(cur_pos.y);
    bool mouse_down = m_window->is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT);
    bool mouse_clicked = mouse_down && !m_mouse_down_last;
    m_mouse_down_last = mouse_down;

    // Render State-Specific Overlay
    if (m_state == GameState::MainMenu) {
        if (m_hub_ui->render_main_menu(m_selected_level, m_inventory, mouse_x, mouse_y, mouse_clicked)) {
            start_expedition(m_selected_level);
        }
    } else if (m_state == GameState::OrbitalHub) {
        if (m_hub_ui->render_orbital_hub(m_selected_level, m_skills, m_inventory, mouse_x, mouse_y, mouse_clicked)) {
            start_expedition(m_selected_level);
        }
    } else if (m_state == GameState::Debrief) {
        DebriefAction action = m_hub_ui->render_debrief(m_expedition_success, m_selected_level, m_inventory, m_skills, mouse_x, mouse_y, mouse_clicked);
        if (action == DebriefAction::LaunchNextSector) {
            m_selected_level = std::min(3, m_selected_level + 1);
            start_expedition(m_selected_level);
        } else if (action == DebriefAction::ReturnToHub) {
            m_state = GameState::OrbitalHub;
        }
    } else if (m_state == GameState::Paused) {
        // 1. Render game HUD behind pause modal
        m_hud->render(*m_player, *m_world, *m_hazard, *m_extraction, m_inventory, m_skills, m_selected_level, view, proj);

        // 2. Render Pause Menu Modal
        PauseMenuAction action = m_pause_menu->render(
            m_selected_level,
            m_expedition_time,
            m_inventory,
            m_settings,
            mouse_x,
            mouse_y,
            mouse_clicked
        );

        if (action == PauseMenuAction::Resume) {
            m_state = GameState::Gameplay;
            m_window->set_cursor_locked(true);
        } else if (action == PauseMenuAction::Abandon) {
            m_inventory.apply_abandon_penalty();
            m_inventory.finalize_run(m_selected_level, false);
            m_expedition_success = false;
            m_state = GameState::Debrief;
            m_window->set_cursor_locked(false);
            m_hud->show_warning("EXPEDITION ABANDONED // 50% SALVAGE PENALTY APPLIED", 4.0f);
        } else if (action == PauseMenuAction::ReturnToHub) {
            m_inventory.apply_abandon_penalty();
            m_inventory.finalize_run(m_selected_level, false);
            m_expedition_success = false;
            m_state = GameState::OrbitalHub;
            m_window->set_cursor_locked(false);
        }
    } else {
        // Render HUD Overlay (Module 4)
        m_hud->render(*m_player, *m_world, *m_hazard, *m_extraction, m_inventory, m_skills, m_selected_level, view, proj);
    }
}

void Application::run() {
    double last_time = glfwGetTime();
    double accumulator = 0.0;
    const double fixed_dt = 1.0 / 60.0;

    std::cout << "\n========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE (TACTICAL VOXEL EXTRACTION)" << std::endl;
    std::cout << "  State Machine Active: Main Menu -> Orbital Hub -> Gameplay -> Debrief" << std::endl;
    std::cout << "========================================================\n" << std::endl;

    while (!m_window->should_close()) {
        double current_time = glfwGetTime();
        double frame_time = current_time - last_time;
        if (frame_time > 0.25) frame_time = 0.25;
        last_time = current_time;

        // Halt physics ticks, voxel meshing, and extraction timers during GameState::Paused
        if (m_state != GameState::Paused) {
            accumulator += frame_time;
        }

        m_window->poll_events();

        // Enforce cursor unlock during Menu, Hub, Debrief, and Paused states
        if (m_state != GameState::Gameplay) {
            if (m_window->is_cursor_locked()) {
                m_window->set_cursor_locked(false);
            }
        }

        // 1. MAIN MENU INPUTS
        if (m_state == GameState::MainMenu) {
            if (m_window->is_key_down(GLFW_KEY_1)) m_selected_level = 1;
            if (m_window->is_key_down(GLFW_KEY_2)) m_selected_level = 2;
            if (m_window->is_key_down(GLFW_KEY_3)) m_selected_level = 3;

            static bool enter_down_last = false;
            bool enter_now = m_window->is_key_down(GLFW_KEY_ENTER) ||
                             m_window->is_key_down(GLFW_KEY_SPACE);

            if (enter_now && !enter_down_last) {
                start_expedition(m_selected_level);
            }
            enter_down_last = enter_now;

            if (m_window->is_key_down(GLFW_KEY_ESCAPE)) {
                break;
            }
        }
        // 2. ORBITAL HUB INPUTS
        else if (m_state == GameState::OrbitalHub) {
            static bool launch_down_last = false;
            bool launch_now = m_window->is_key_down(GLFW_KEY_ENTER) ||
                              m_window->is_key_down(GLFW_KEY_SPACE);

            if (launch_now && !launch_down_last) {
                start_expedition(m_selected_level);
            }
            launch_down_last = launch_now;

            static bool hub_back_down_last = false;
            bool hub_back_now = m_window->is_key_down(GLFW_KEY_TAB);
            if (hub_back_now && !hub_back_down_last) {
                m_state = GameState::MainMenu;
            }
            hub_back_down_last = hub_back_now;
        }
        // 3. DEBRIEF INPUTS
        else if (m_state == GameState::Debrief) {
            static bool debrief_down_last = false;
            bool debrief_now = m_window->is_key_down(GLFW_KEY_ENTER) ||
                               m_window->is_key_down(GLFW_KEY_SPACE);

            if (debrief_now && !debrief_down_last) {
                m_selected_level = std::min(3, m_selected_level + 1);
                start_expedition(m_selected_level);
            }
            debrief_down_last = debrief_now;

            static bool hub_back_down_last = false;
            bool hub_back_now = m_window->is_key_down(GLFW_KEY_TAB);
            if (hub_back_now && !hub_back_down_last) {
                m_state = GameState::OrbitalHub;
            }
            hub_back_down_last = hub_back_now;

            static bool retry_down_last = false;
            bool retry_now = m_window->is_key_down(GLFW_KEY_R);
            if (retry_now && !retry_down_last) {
                start_expedition(m_selected_level);
            }
            retry_down_last = retry_now;
        }
        // 4. GAMEPLAY INPUTS
        else if (m_state == GameState::Gameplay) {
            static bool b_down_last = false;
            bool b_now = m_window->is_key_down(GLFW_KEY_B);
            if (b_now && !b_down_last) {
                m_extraction->deploy_beacon(m_player->position());
            }
            b_down_last = b_now;

            static bool h_down_last = false;
            bool h_now = m_window->is_key_down(GLFW_KEY_H);
            if (h_now && !h_down_last) {
                m_renderer->headlamp().enabled = !m_renderer->headlamp().enabled;
            }
            h_down_last = h_now;

            static bool tab_down_last = false;
            bool tab_now = m_window->is_key_down(GLFW_KEY_TAB);
            if (tab_now && !tab_down_last) {
                m_window->set_cursor_locked(!m_window->is_cursor_locked());
            }
            m_player->set_mouse_sensitivity(m_settings.mouse_sensitivity);
            m_player->handle_input(*m_window, static_cast<float>(frame_time));
        }

        while (accumulator >= fixed_dt) {
            fixed_tick(static_cast<float>(fixed_dt));
            accumulator -= fixed_dt;
        }

        render(static_cast<float>(frame_time));
        m_window->swap_buffers();
    }
}

} // namespace Voidfall
