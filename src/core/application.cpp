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
    });

    // 2. Renderer
    m_renderer = std::make_unique<Renderer>(m_window->width(), m_window->height());

    // 3. Voxel World & Procedural Caverns
    m_world = std::make_unique<World>(m_config.world_seed);

    // 4. Player Controller
    m_player = std::make_unique<PlayerController>(glm::vec3(16.0f, 22.0f, 16.0f));

    m_player->set_on_block_break([this](int x, int y, int z, const glm::ivec3& normal, uint8_t mat) {
        on_block_broken(x, y, z, normal, mat);
    });

    m_player->set_on_block_place([this](int x, int y, int z, uint8_t mat) {
        on_block_placed(x, y, z, mat);
    });

    m_player->set_on_sonar_cast([this](const glm::vec3& origin) {
        on_sonar_cast(origin);
    });

    m_player->set_on_explosive_blast([this](const glm::ivec3& origin, const glm::ivec3& dir, bool is_micro) {
        on_explosive_blast(origin, dir, is_micro);
    });

    // 5. Systems
    m_hazard = std::make_unique<HazardClock>();
    m_hazard->set_on_tremor([this](float intensity) {
        m_trauma = std::min(m_trauma + intensity * 0.4f, 1.0f);
        std::cout << "[Hazard] Seismic Tremor shaking subterranean caverns!" << std::endl;
    });

    m_extraction = std::make_unique<ExtractionSystem>();
    m_extraction->set_on_complete([this]() {
        m_expedition_success = true;
        m_state = GameState::Debrief;
        m_window->set_cursor_locked(false);
        std::cout << "[Extraction] Delver extraction complete! Returning to debrief." << std::endl;
    });

    // 6. UI systems
    m_hud = std::make_unique<HUD>(m_window->width(), m_window->height());
    m_hub_ui = std::make_unique<OrbitalHubUI>(m_window->width(), m_window->height());

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
        << (level == 1 ? " [Crystalline Caverns - Surveying Sector]" :
            level == 2 ? " [Derelict Station Core - Vault Breach]" :
                         " [The Abyssal Vault - 3-Min Collapse Protocol]"));
    m_selected_level = level;
    m_inventory.reset(level == 3 ? 50 : 25);
    m_player->set_position(glm::vec3(16.0f, 22.0f, 16.0f));
    m_player->exo_mut().integrity = 100.0f;
    m_player->exo_mut().power = 100.0f;
    m_player->exo_mut().heat = 0.0f;
    m_player->exo_mut().overheated = false;

    // Reset systems
    m_hazard = std::make_unique<HazardClock>();
    m_hazard->set_on_tremor([this](float intensity) {
        m_trauma = std::min(m_trauma + intensity * 0.4f, 1.0f);
    });

    m_extraction = std::make_unique<ExtractionSystem>();
    m_extraction->set_on_complete([this]() {
        m_expedition_success = true;
        m_state = GameState::Debrief;
        m_window->set_cursor_locked(false);
    });

    if (level == 2) {
        // Build subterranean sealed vault chamber at (22..30, 8..15, 22..30)
        for (int vx = 22; vx <= 30; ++vx) {
            for (int vy = 8; vy <= 14; ++vy) {
                for (int vz = 22; vz <= 30; ++vz) {
                    bool is_wall = (vx == 22 || vx == 30 || vy == 8 || vy == 14 || vz == 22 || vz == 30);
                    if (is_wall) {
                        // Vault door entryway at (26, 9..11, 22)
                        if (vz == 22 && vx >= 25 && vx <= 27 && vy >= 9 && vy <= 11) {
                            m_world->set_voxel(vx, vy, vz, Voxel{MAT_REINFORCED_VAULT_DOOR, 0x10}, false);
                        } else {
                            m_world->set_voxel(vx, vy, vz, Voxel{MAT_INDUSTRIAL_BULKHEAD, 0x10}, false);
                        }
                    } else if (vx == 26 && vy == 9 && vz == 26) {
                        // Relic Hyper-Core in vault center
                        m_world->set_voxel(vx, vy, vz, Voxel{MAT_VOIDITE_CRYSTAL, 0x20}, false);
                    } else {
                        m_world->set_voxel(vx, vy, vz, Voxel{MAT_AIR, 0}, false);
                    }
                }
            }
        }
    } else if (level == 3) {
        m_level3_timer = 180.0f; // 3-minute strict countdown
    }

    m_window->set_cursor_locked(true);
    m_state = GameState::Gameplay;
}

void Application::on_block_broken(int x, int y, int z, const glm::ivec3& normal, uint8_t mat) {
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

    if (mat == MAT_VOIDITE_CRYSTAL) {
        m_inventory.add_voidite(10);
        m_hud->add_floating_loot(popup_pos, "+10 VOIDITE (+50 PTS)", glm::vec4(0.85f, 0.4f, 1.0f, 1.0f));
        m_skills.add_demolitions_xp(15);
    } else if (mat == MAT_INDUSTRIAL_BULKHEAD) {
        m_inventory.add_titanium(5);
        m_hud->add_floating_loot(popup_pos, "+5 TITANIUM (+30 PTS)", glm::vec4(0.4f, 0.85f, 1.0f, 1.0f));
        m_skills.add_demolitions_xp(15);
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

void Application::on_sonar_cast(const glm::vec3& origin) {
    float radius = m_skills.get_sonar_radius();
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
                    on_block_broken(bpos.x, bpos.y, bpos.z, -forward_step, v.material_id);
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
                        on_block_broken(bpos.x, bpos.y, bpos.z, glm::ivec3(0, 1, 0), v.material_id);
                    }
                }
            }
        }
        m_trauma = std::min(m_trauma + 0.45f, 1.0f);
    }
}

void Application::fixed_tick(float dt) {
    m_current_tick++;

    if (m_state != GameState::Gameplay) {
        return;
    }

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

    // 3. Falling Debris simulation
    for (auto& d : m_debris) {
        d.update(dt, *m_world);
    }

    // 4. Update Hazard Clock, Extraction, Surveying, and Particles
    m_hazard->update(dt);
    m_extraction->update(dt, m_player->position());
    m_surveying.update(dt);
    m_hud->update(dt);
    m_renderer->update_particles(dt);

    // Check Level 3 strict countdown
    if (m_selected_level == 3) {
        m_level3_timer -= dt;
        if (m_level3_timer <= 0.0f) {
            // Tectonic collapse failure
            m_expedition_success = false;
            m_state = GameState::Debrief;
            m_window->set_cursor_locked(false);
        }
    }

    // Check player health
    if (m_player->exo().integrity <= 0.0f) {
        m_expedition_success = false;
        m_state = GameState::Debrief;
        m_window->set_cursor_locked(false);
    }

    // 5. Stream chunks around player
    m_world->update(m_player->position(), 2);
}

void Application::render(float dt) {
    m_world->upload_dirty_chunks();

    glm::mat4 view;
    glm::vec3 cam_pos;

    if (m_state == GameState::Gameplay) {
        view = m_player->get_view_matrix();
        cam_pos = m_player->position();

        // Camera view setup with non-linear impulse trauma screen shake
        if (m_trauma > 0.001f) {
            float shake_amount = m_trauma * m_trauma * 0.35f;
            float shake_x = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * shake_amount;
            float shake_y = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * shake_amount;
            view = glm::translate(view, glm::vec3(shake_x, shake_y, 0.0f));
            m_trauma = (std::max)(0.0f, m_trauma - dt * 0.8f);
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

    glm::mat4 proj = glm::perspective(
        glm::radians(75.0f),
        m_window->aspect_ratio(),
        0.1f,
        250.0f
    );

    // Setup dynamic point lights (e.g. extraction beacon)
    m_renderer->clear_point_lights();
    if (m_extraction->phase() == ExtractionPhase::BeaconDeployed) {
        PointLight beacon_light;
        beacon_light.position = m_extraction->beacon_position();
        beacon_light.color = glm::vec3(1.0f, 0.15f, 0.1f);
        beacon_light.radius = 25.0f;
        beacon_light.intensity = 4.0f * m_extraction->siren_pulse();
        m_renderer->add_point_light(beacon_light);
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

    // Render Volumetric Sonar Ping with X-Ray Wireframe (Module 2)
    if (m_surveying.is_active()) {
        m_renderer->render_sonar_wireframes(m_surveying.surveyed_voxels(), m_surveying.alpha());
    }

    // End HDR Frame & Post-Processing Tonemap
    m_renderer->end_frame(dt, m_hazard->radiation_level());

    // Render State-Specific Overlay
    if (m_state == GameState::MainMenu) {
        m_hub_ui->render_main_menu(m_selected_level);
    } else if (m_state == GameState::OrbitalHub) {
        m_hub_ui->render_orbital_hub(m_selected_level, m_skills, m_inventory);
    } else if (m_state == GameState::Debrief) {
        m_hub_ui->render_debrief(m_expedition_success, m_selected_level, m_inventory, m_skills);
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
        accumulator += frame_time;

        m_window->poll_events();

        // 1. MAIN MENU INPUTS
        if (m_state == GameState::MainMenu) {
            if (m_window->is_key_down(GLFW_KEY_1)) m_selected_level = 1;
            if (m_window->is_key_down(GLFW_KEY_2)) m_selected_level = 2;
            if (m_window->is_key_down(GLFW_KEY_3)) m_selected_level = 3;

            static bool enter_down_last = false;
            bool enter_now = m_window->is_key_down(GLFW_KEY_ENTER) ||
                             m_window->is_key_down(GLFW_KEY_SPACE) ||
                             m_window->is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT);

            if (enter_now && !enter_down_last) {
                m_state = GameState::OrbitalHub;
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
                              m_window->is_key_down(GLFW_KEY_SPACE) ||
                              m_window->is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT);

            if (launch_now && !launch_down_last) {
                start_expedition(m_selected_level);
            }
            launch_down_last = launch_now;

            static bool hub_back_down_last = false;
            bool hub_back_now = m_window->is_key_down(GLFW_KEY_TAB) || m_window->is_key_down(GLFW_KEY_ESCAPE);
            if (hub_back_now && !hub_back_down_last) {
                m_state = GameState::MainMenu;
            }
            hub_back_down_last = hub_back_now;
        }
        // 3. DEBRIEF INPUTS
        else if (m_state == GameState::Debrief) {
            static bool debrief_down_last = false;
            bool debrief_now = m_window->is_key_down(GLFW_KEY_ENTER) ||
                               m_window->is_key_down(GLFW_KEY_SPACE) ||
                               m_window->is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT);

            if (debrief_now && !debrief_down_last) {
                m_state = GameState::OrbitalHub;
            }
            debrief_down_last = debrief_now;

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
            bool tab_now = m_window->is_key_down(GLFW_KEY_TAB) || m_window->is_key_down(GLFW_KEY_ESCAPE);
            if (tab_now && !tab_down_last) {
                m_window->set_cursor_locked(!m_window->is_cursor_locked());
            }
            tab_down_last = tab_now;

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
