#pragma once
#include "../graphics/shader.hpp"
#include "../player/loadout.hpp"
#include "../skills/skill_matrix.hpp"
#include <glm/glm.hpp>
#include <string>

#include "../player/character_class.hpp"
#include "../player/upgrades.hpp"
#include "../core/save_system.hpp"

#include "main_menu.hpp"

namespace Voidfall {

enum class GameState {
    MainMenu = 0,
    OrbitalHub = 1,
    Gameplay = 2,
    Paused = 3,
    Debrief = 4,
    STATE_MAIN_MENU = 0,
    STATE_ORBITAL_HUB = 1,
    STATE_GAMEPLAY = 2,
    STATE_PAUSED = 3,
    STATE_DEBRIEF = 4
};

enum class DebriefAction {
    None,
    ReturnToHub,
    LaunchNextSector
};

enum class MenuSubView {
    Main = 0,
    SectorSelect = 1,
    Upgrades = 2
};

enum class HubTab {
    SectorSelect = 0,
    DelverRoster = 1,
    UpgradeTerminal = 2
};

class OrbitalHubUI {
public:
    OrbitalHubUI(int screen_width, int screen_height);
    ~OrbitalHubUI();

    void resize(int width, int height);

    MainMenuAction render_main_menu(int& selected_level, UserProfile& profile, bool has_save, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);
    bool render_main_menu(int& selected_level, const PlayerInventory& inventory, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);
    bool render_main_menu(int& selected_level, UserProfile& profile, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);

    bool render_orbital_hub(int selected_level, const SkillMatrix& skills, const PlayerInventory& inventory, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);
    bool render_orbital_hub(int selected_level, const SkillMatrix& skills, const PlayerInventory& inventory, UserProfile& profile, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);

    DebriefAction render_debrief(bool success, int level, PlayerInventory& inventory, const SkillMatrix& skills, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);
    DebriefAction render_debrief(bool success, int level, PlayerInventory& inventory, const SkillMatrix& skills, UserProfile& profile, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);

    MenuSubView subview() const { return m_subview; }
    void set_subview(MenuSubView sv) { m_subview = sv; }

    HubTab active_tab() const { return m_active_tab; }
    void set_active_tab(HubTab tab) { m_active_tab = tab; }

    bool has_profile_changed() const { return m_profile_dirty; }
    void clear_profile_changed() { m_profile_dirty = false; }

    bool wants_return_to_main_menu() const { return m_return_to_title; }
    void clear_return_to_main_menu() { m_return_to_title = false; }

private:
    void init_gl();
    void init_font_atlas();
    void draw_rect(float x, float y, float w, float h, const glm::vec4& color);
    void draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color);

    void render_sector_select_carousel(int& selected_level, UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked, MainMenuAction& action);
    void render_delver_roster(UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked);
    void render_upgrade_terminal(UserProfile& profile, float mouse_x, float mouse_y, bool mouse_clicked);

    int m_width{1600};
    int m_height{900};

    MenuSubView m_subview{MenuSubView::Main};
    HubTab m_active_tab{HubTab::SectorSelect};
    bool m_profile_dirty{false};
    bool m_return_to_title{false};
    float m_purchase_pulse_timer{0.0f};
    std::string m_terminal_msg;
    glm::vec4 m_terminal_msg_col{0.2f, 0.95f, 0.4f, 1.0f};

    Shader m_ui_shader;
    Shader m_text_shader;

    unsigned int m_rect_vao{0};
    unsigned int m_rect_vbo{0};

    unsigned int m_text_vao{0};
    unsigned int m_text_vbo{0};
    unsigned int m_font_tex{0};
};

} // namespace Voidfall
