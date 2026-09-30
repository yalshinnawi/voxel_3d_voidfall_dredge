#include "application.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

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

    m_window->set_resize_callback([this](int w, int h) {
        if (m_renderer) m_renderer->resize(w, h);
        if (m_hud) m_hud->resize(w, h);
    });

    // 2. Renderer
    m_renderer = std::make_unique<Renderer>(m_window->width(), m_window->height());

    // 3. Voxel World & Procedural Caverns
    m_world = std::make_unique<World>(m_config.world_seed);

    // 4. Player Controller
    m_player = std::make_unique<PlayerController>(glm::vec3(16.0f, 20.0f, 16.0f));

    m_player->set_on_block_break([this](int x, int y, int z) {
        on_block_broken(x, y, z);
    });

    m_player->set_on_block_place([this](int x, int y, int z, uint8_t mat) {
        on_block_placed(x, y, z, mat);
    });

    m_player->set_on_sonar_cast([this](const glm::vec3& origin) {
        on_sonar_cast(origin);
    });

    // 5. Systems
    m_hazard = std::make_unique<HazardClock>();
    m_hazard->set_on_tremor([this](float intensity) {
        m_screen_shake = intensity * 0.4f;
        std::cout << "[Hazard] Seismic Tremor shaking subterranean caverns!" << std::endl;
    });

    m_extraction = std::make_unique<ExtractionSystem>();

    // 6. HUD
    m_hud = std::make_unique<HUD>(m_window->width(), m_window->height());

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

    // Pre-generate initial chunks around player spawn
    m_world->update(m_player->position(), 2);
}

void Application::on_block_broken(int x, int y, int z) {
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
    m_renderer->trigger_sonar_pulse(origin);
    if (m_host) {
        m_host->broadcast_sonar_ping(1, origin, 65.0f);
    } else if (m_client) {
        m_client->send_sonar_ping(origin);
    }
}

void Application::fixed_tick(float dt) {
    m_current_tick++;

    // 1. Poll networking
    if (m_host) m_host->poll_network_events();
    if (m_client) {
        m_client->poll_network_events();
        PlayerInputPacket input = m_player->build_input_packet(m_current_tick, dt);
        m_client->send_input(input);
    }

    // 2. Physics & Player simulation
    m_player->update_physics(dt, *m_world);

    // 3. Falling Debris simulation
    for (auto& d : m_debris) {
        d.update(dt, *m_world);
    }

    // 4. Update Hazard Clock & Extraction
    m_hazard->update(dt);
    m_extraction->update(dt, m_player->position());

    // 5. Stream chunks around player
    m_world->update(m_player->position(), 2);
}

void Application::render(float dt) {
    m_world->upload_dirty_chunks();

    // Camera view setup with subtle tremor screen shake
    glm::mat4 view = m_player->get_view_matrix();
    if (m_screen_shake > 0.001f) {
        float shake_x = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * m_screen_shake;
        float shake_y = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * m_screen_shake;
        view = glm::translate(view, glm::vec3(shake_x, shake_y, 0.0f));
        m_screen_shake = std::max(0.0f, m_screen_shake - dt * 0.8f);
    }

    glm::mat4 proj = glm::perspective(
        glm::radians(75.0f),
        m_window->aspect_ratio(),
        0.1f,
        250.0f
    );

    // Update headlamp position to camera
    m_renderer->headlamp().position = m_player->position();
    m_renderer->headlamp().direction = m_player->forward();

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
    m_renderer->begin_frame(view, proj, m_player->position());

    // Render Chunks
    for (const auto& [pos, chunk] : m_world->chunks()) {
        m_renderer->render_chunk(*chunk);
    }

    // Render Falling Dynamic Debris
    for (const auto& d : m_debris) {
        d.render();
    }

    // End HDR Frame & Post-Processing Tonemap
    m_renderer->end_frame(dt, m_hazard->radiation_level());

    // Render HUD Overlay
    m_hud->render(*m_player, *m_hazard, *m_extraction);
}

void Application::run() {
    double last_time = glfwGetTime();
    double accumulator = 0.0;
    const double fixed_dt = 1.0 / 60.0;

    std::cout << "\n========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE (MULTIPLAYER EDITION)" << std::endl;
    std::cout << "  Controls:" << std::endl;
    std::cout << "    [W, A, S, D]      : First-Person Movement" << std::endl;
    std::cout << "    [SPACE]           : Jump / Exo-Suit Thrusters (Hover)" << std::endl;
    std::cout << "    [LEFT CLICK]      : Subterranean Mining Drill" << std::endl;
    std::cout << "    [RIGHT CLICK]     : Place Structural Bulkhead" << std::endl;
    std::cout << "    [F / MIDDLE CLICK]: Fire Tension Grapple Hook" << std::endl;
    std::cout << "    [E]               : Reel In Grapple Cable" << std::endl;
    std::cout << "    [Q]               : Seismic Sonar Pulse Scan" << std::endl;
    std::cout << "    [B]               : Deploy 90s Extraction Beacon" << std::endl;
    std::cout << "    [H]               : Toggle Headlamp" << std::endl;
    std::cout << "    [TAB / ESC]       : Toggle Mouse Capture" << std::endl;
    std::cout << "========================================================\n" << std::endl;

    while (!m_window->should_close()) {
        double current_time = glfwGetTime();
        double frame_time = current_time - last_time;
        if (frame_time > 0.25) frame_time = 0.25;
        last_time = current_time;
        accumulator += frame_time;

        m_window->poll_events();

        // Key shortcuts
        static bool b_pressed = false;
        if (m_window->is_key_down(GLFW_KEY_B)) {
            if (!b_pressed) {
                b_pressed = true;
                m_extraction->deploy_beacon(m_player->position());
            }
        } else {
            b_pressed = false;
        }

        static bool h_pressed = false;
        if (m_window->is_key_down(GLFW_KEY_H)) {
            if (!h_pressed) {
                h_pressed = true;
                m_renderer->headlamp().enabled = !m_renderer->headlamp().enabled;
            }
        } else {
            h_pressed = false;
        }

        static bool tab_pressed = false;
        if (m_window->is_key_down(GLFW_KEY_TAB) || m_window->is_key_down(GLFW_KEY_ESCAPE)) {
            if (!tab_pressed) {
                tab_pressed = true;
                m_window->set_cursor_locked(!m_window->is_cursor_locked());
            }
        } else {
            tab_pressed = false;
        }

        m_player->handle_input(*m_window, static_cast<float>(frame_time));

        while (accumulator >= fixed_dt) {
            fixed_tick(static_cast<float>(fixed_dt));
            accumulator -= fixed_dt;
        }

        render(static_cast<float>(frame_time));
        m_window->swap_buffers();
    }
}

} // namespace Voidfall
