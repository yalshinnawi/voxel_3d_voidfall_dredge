#pragma once
#include "window.hpp"
#include "../graphics/renderer.hpp"
#include "../voxel/world.hpp"
#include "../voxel/structural_check.hpp"
#include "../player/controller.hpp"
#include "../systems/hazard_clock.hpp"
#include "../systems/extraction.hpp"
#include "../ui/hud.hpp"
#include "../net/net_host.hpp"
#include "../net/net_client.hpp"
#include "../entities/dynamic_debris.hpp"
#include <vector>
#include <memory>
#include <string>

namespace Voidfall {

struct AppConfig {
    bool is_host{true};
    std::string connect_ip{"127.0.0.1"};
    uint16_t port{27015};
    uint32_t world_seed{1337};
    std::string player_name{"Delver"};
};

class Application {
public:
    explicit Application(const AppConfig& config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void init_systems();
    void fixed_tick(float dt);
    void render(float dt);

    void on_block_broken(int x, int y, int z);
    void on_block_placed(int x, int y, int z, uint8_t mat);
    void on_sonar_cast(const glm::vec3& origin);

    AppConfig m_config;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<World> m_world;
    std::unique_ptr<PlayerController> m_player;
    std::unique_ptr<HazardClock> m_hazard;
    std::unique_ptr<ExtractionSystem> m_extraction;
    std::unique_ptr<HUD> m_hud;

    std::unique_ptr<NetHost> m_host;
    std::unique_ptr<NetClient> m_client;

    std::vector<DynamicDebris> m_debris;
    uint32_t m_next_debris_id{1};

    uint32_t m_current_tick{0};
    float m_screen_shake{0.0f};
};

} // namespace Voidfall
