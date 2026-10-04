#pragma once
#include "window.hpp"
#include "../graphics/renderer.hpp"
#include "../graphics/viewmodel.hpp"
#include "../voxel/world.hpp"
#include "../voxel/structural_check.hpp"
#include "../player/controller.hpp"
#include "../player/loadout.hpp"
#include "../skills/skill_matrix.hpp"
#include "../skills/surveying.hpp"
#include "../systems/hazard_clock.hpp"
#include "../systems/extraction.hpp"
#include "../systems/noise_meter.hpp"
#include "../entities/enemies/void_stalker.hpp"
#include "../entities/enemies/seismic_burrower.hpp"
#include "../ui/hud.hpp"
#include "../ui/orbital_hub.hpp"
#include "../ui/pause_menu.hpp"
#include "../net/net_host.hpp"
#include "../net/net_client.hpp"
#include "../entities/dynamic_debris.hpp"
#include "../audio/audio_engine.hpp"
#include "save_system.hpp"
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
    bool auto_play_test{false};
    bool hidden_window{false};
    std::string single_screenshot_path{""};
    std::string save_file_path{""};
    bool fresh_save{false};
    bool is_test_save{false};
    bool test_mode{false};
    bool test_enemy{false};
    bool capture_models{false};
    bool capture_level_shapes{false};
    bool mute_audio{false};
    bool force_audible{false};
    int window_width{0};
    int window_height{0};
    bool auto_screen_size{true};
    bool fullscreen{false};
};

constexpr GameState STATE_MAIN_MENU   = GameState::MainMenu;
constexpr GameState STATE_ORBITAL_HUB = GameState::OrbitalHub;
constexpr GameState STATE_GAMEPLAY    = GameState::Gameplay;
constexpr GameState STATE_PAUSED      = GameState::Paused;
constexpr GameState STATE_DEBRIEF     = GameState::Debrief;

class Application {
public:
    using GameState = Voidfall::GameState;

    explicit Application(const AppConfig& config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();
    const std::string& active_save_file() const { return m_active_save_file; }
    bool is_test_mode() const { return m_config.test_mode || m_config.auto_play_test || m_config.is_test_save || m_config.test_enemy || m_config.capture_level_shapes; }
    AudioEngine* audio() { return m_audio.get(); }

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
    void on_tactical_ability(CharacterClass cls, const glm::vec3& pos, const glm::vec3& dir);
    void setup_hazard_system();
    void sync_profile_with_player();
    void sync_audio_settings();

    // Testing & Progression diagnostics
    void grant_testing_resources(int exp = 1000, int voidite = 25, int titanium = 10);
    void test_upgrade_and_respec_cycle();

    AppConfig m_config;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<ViewModel> m_viewmodel;
    std::unique_ptr<World> m_world;
    std::unique_ptr<PlayerController> m_player;
    std::unique_ptr<HazardClock> m_hazard;
    std::unique_ptr<ExtractionSystem> m_extraction;
    std::unique_ptr<HUD> m_hud;
    std::unique_ptr<OrbitalHubUI> m_hub_ui;
    std::unique_ptr<PauseMenu> m_pause_menu;
    std::unique_ptr<AudioEngine> m_audio;
    GameSettings m_settings;
    float m_expedition_time{0.0f};

    std::unique_ptr<NetHost> m_host;
    std::unique_ptr<NetClient> m_client;

    std::vector<DynamicDebris> m_debris;
    uint32_t m_next_debris_id{1};

    // Mining Noise & Enemy Threat Systems
    NoiseMeter m_noise_meter;
    VoidStalkerManager m_stalkers;
    SeismicBurrowerManager m_burrowers;
    std::vector<PlayerPlasmaBolt> m_plasma_bolts;

    // Game state machine & progression
    GameState m_state{GameState::MainMenu};
    int m_selected_level{1};
    UserProfile m_user_profile;
    PlayerInventory m_inventory;
    SkillMatrix m_skills;
    SurveyingSystem m_surveying;

    uint32_t m_current_tick{0};
    float m_screen_shake{0.0f};
    float m_trauma{0.0f};

    // Cinematic Death Sequence
    bool m_death_sequence{false};
    float m_death_timer{0.0f};
    const float DEATH_SEQUENCE_DURATION{3.5f};
    float m_debrief_input_lock{0.0f};

    // Tremor visual & sustained shaking tracker
    float m_tremor_spall_timer{0.0f};

    // Level 3 collapse timer
    float m_level3_timer{180.0f};
    float m_proximity_radiation{0.0f};
    float m_effective_radiation{0.0f};
    bool m_expedition_success{false};
    bool m_mouse_down_last{false};
    int m_auto_test_frame{0};
    std::string m_active_save_file{SaveSystem::DEFAULT_SAVE_FILE};

    // 3D Model Showcase Staging
    bool m_showcase_render_delver{false};
    CharacterClass m_showcase_delver_class{CharacterClass::Demolitionist};
    glm::vec3 m_showcase_delver_pos{0.0f};
    float m_showcase_delver_yaw{0.0f};
};

} // namespace Voidfall
