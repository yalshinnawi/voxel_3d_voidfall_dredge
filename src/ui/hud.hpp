#pragma once
#include "../graphics/shader.hpp"
#include "../player/controller.hpp"
#include "../systems/hazard_clock.hpp"
#include "../systems/extraction.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Voidfall {

struct MissionStats {
    int crystals_mined{0};
    int target_crystals{10};
    int sonar_scans_performed{0};
    bool beacon_deployed{false};
};

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

    void record_crystal_mined() { m_stats.crystals_mined++; }
    void record_sonar_scan() { m_stats.sonar_scans_performed++; }
    const MissionStats& stats() const { return m_stats; }

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

    MissionStats m_stats;
};

} // namespace Voidfall
