#pragma once
#include "../graphics/shader.hpp"
#include "../player/controller.hpp"
#include "../systems/hazard_clock.hpp"
#include "../systems/extraction.hpp"
#include <glm/glm.hpp>

namespace Voidfall {

class HUD {
public:
    HUD(int screen_width, int height);
    ~HUD();

    void resize(int width, int height);
    void render(
        const PlayerController& player,
        const HazardClock& hazard,
        const ExtractionSystem& extraction
    );

private:
    void init_gl();
    void draw_rect(float x, float y, float w, float h, const glm::vec4& color);

    int m_width{1600};
    int m_height{900};

    Shader m_ui_shader;
    unsigned int m_vao{0};
    unsigned int m_vbo{0};
};

} // namespace Voidfall
