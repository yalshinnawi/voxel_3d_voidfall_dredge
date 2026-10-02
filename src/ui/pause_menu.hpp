#pragma once
#include "../graphics/shader.hpp"
#include "../player/loadout.hpp"
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

struct GameSettings {
    float mouse_sensitivity{0.12f};
    float fov{75.0f};
    float master_volume{1.0f};
};

class PauseMenu {
public:
    PauseMenu(int screen_width, int screen_height);
    ~PauseMenu();

    void resize(int width, int height);

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
