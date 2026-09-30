#pragma once
#include "../graphics/shader.hpp"
#include "../player/loadout.hpp"
#include "../skills/skill_matrix.hpp"
#include <glm/glm.hpp>
#include <string>

namespace Voidfall {

enum class GameState {
    MainMenu,
    OrbitalHub,
    Gameplay,
    Debrief
};

class OrbitalHubUI {
public:
    OrbitalHubUI(int screen_width, int screen_height);
    ~OrbitalHubUI();

    void resize(int width, int height);

    bool render_main_menu(int& selected_level, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);
    bool render_orbital_hub(int selected_level, const SkillMatrix& skills, const PlayerInventory& inventory, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);
    bool render_debrief(bool success, int level, const PlayerInventory& inventory, const SkillMatrix& skills, float mouse_x = -1.0f, float mouse_y = -1.0f, bool mouse_clicked = false);

private:
    void init_gl();
    void init_font_atlas();
    void draw_rect(float x, float y, float w, float h, const glm::vec4& color);
    void draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color);

    int m_width{1600};
    int m_height{900};

    Shader m_ui_shader;
    Shader m_text_shader;

    unsigned int m_rect_vao{0};
    unsigned int m_rect_vbo{0};

    unsigned int m_text_vao{0};
    unsigned int m_text_vbo{0};
    unsigned int m_font_tex{0};
};

} // namespace Voidfall
