#pragma once
#include "window.hpp"
#include "../graphics/renderer.hpp"
#include "../voxel/world.hpp"
#include "../voxel/structural_check.hpp"
#include "../player/controller.hpp"
#include "../player/loadout.hpp"
#include "../skills/skill_matrix.hpp"
#include "../skills/surveying.hpp"
#include "../systems/hazard_clock.hpp"
#include "../systems/extraction.hpp"
#include "../ui/hud.hpp"
#include "../ui/orbital_hub.hpp"
#include "../ui/pause_menu.hpp"
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

constexpr GameState STATE_MAIN_MENU   = GameState::MainMenu;
constexpr GameState STATE_ORBITAL_HUB = GameState::OrbitalHub;
constexpr GameState STATE_GAMEPLAY    = GameState::Gameplay;
constexpr GameState STATE_PAUSED      = GameState::Paused;
constexpr GameState STATE_DEBRIEF     = GameState::Debrief;

class Application {
public:
    explicit Application(const AppConfig& config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void init_systems();
    void start_expedition(int level);
    void fixed_tick(float dt);
    void render(float dt);
    void process_input(int key, int action);

    void on_block_broken(int x, int y, int z, const glm::ivec3& normal, uint8_t mat, uint8_t flags);
    void on_block_placed(int x, int y, int z, uint8_t mat);
    void on_bulkhead_dismantled(int x, int y, int z);
    void on_sonar_cast(const glm::vec3& origin);
    void on_explosive_blast(const glm::ivec3& origin, const glm::ivec3& dir, bool is_micro);
    void setup_hazard_system();

    AppConfig m_config;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<World> m_world;
    std::unique_ptr<PlayerController> m_player;
    std::unique_ptr<HazardClock> m_hazard;
    std::unique_ptr<ExtractionSystem> m_extraction;
    std::unique_ptr<HUD> m_hud;
    std::unique_ptr<OrbitalHubUI> m_hub_ui;
    std::unique_ptr<PauseMenu> m_pause_menu;
    GameSettings m_settings;
    float m_expedition_time{0.0f};

    std::unique_ptr<NetHost> m_host;
    std::unique_ptr<NetClient> m_client;

    std::vector<DynamicDebris> m_debris;
    uint32_t m_next_debris_id{1};

    // Game state machine & progression
    GameState m_state{GameState::MainMenu};
    int m_selected_level{1};
    PlayerInventory m_inventory;
    SkillMatrix m_skills;
    SurveyingSystem m_surveying;

    uint32_t m_current_tick{0};
    float m_screen_shake{0.0f};
    float m_trauma{0.0f};

    // Level 3 collapse timer
    float m_level3_timer{180.0f};
    bool m_expedition_success{false};
    bool m_mouse_down_last{false};
};

} // namespace Voidfall
