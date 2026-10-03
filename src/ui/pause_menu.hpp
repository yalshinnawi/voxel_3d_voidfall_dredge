#pragma once
#include "../graphics/shader.hpp"
#include "../player/loadout.hpp"
#include "../core/settings.hpp"
#include <glm/glm.hpp>
#include <string>

namespace Voidfall {

enum class PauseMenuAction {
    None,
    Resume,
    Abandon,
    ReturnToHub,
    ReturnToStartup
};

enum class PauseTab {
    Mission = 0,
    AudioSettings = 1,
    ControlsBriefing = 2
};

class PauseMenu {
public:
    PauseMenu(int screen_width, int screen_height);
    ~PauseMenu();

    void resize(int width, int height);

    PauseTab active_tab() const { return m_active_tab; }
    void set_active_tab(PauseTab tab) { m_active_tab = tab; }

    bool check_and_clear_test_sound() {
        bool req = m_test_sound_requested;
        m_test_sound_requested = false;
        return req;
    }

    PauseMenuAction render(
        int sector,
        float time_elapsed,
        const PlayerInventory& inventory,
        GameSettings& settings,
        float mouse_x,
        float mouse_y,
        bool mouse_clicked
    );

private:
    void init_gl();
    void init_font_atlas();
    void draw_rect(float x, float y, float w, float h, const glm::vec4& color);
    void draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color);
    void draw_text_fitted(const std::string& text, float x, float y, float max_w, float base_scale, const glm::vec4& color);
    void draw_text_centered(const std::string& text, float box_x, float box_y, float box_w, float box_h, float scale, const glm::vec4& color);
    void draw_text_centered_fitted(const std::string& text, float box_x, float box_y, float box_w, float box_h, float base_scale, const glm::vec4& color);
    void draw_panel_with_border(float x, float y, float w, float h, const glm::vec4& bg_col, const glm::vec4& border_col, float border_thick = 1.0f);

    int m_width{1600};
    int m_height{900};

    Shader m_ui_shader;
    Shader m_text_shader;

    unsigned int m_rect_vao{0};
    unsigned int m_rect_vbo{0};

    unsigned int m_text_vao{0};
    unsigned int m_text_vbo{0};
    unsigned int m_font_tex{0};

    PauseTab m_active_tab{PauseTab::Mission};
    bool m_test_sound_requested{false};
};

} // namespace Voidfall
