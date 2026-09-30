#pragma once
#include "../graphics/shader.hpp"
#include "../player/controller.hpp"
#include "../player/loadout.hpp"
#include "../skills/skill_matrix.hpp"
#include "../systems/hazard_clock.hpp"
#include "../systems/extraction.hpp"
#include "../voxel/world.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Voidfall {

struct FloatingLootText {
    glm::vec3 world_pos;
    std::string text;
    glm::vec4 color;
    float timer{1.5f};
    float max_timer{1.5f};
};

class HUD {
public:
    HUD(int screen_width, int height);
    ~HUD();

    void resize(int width, int height);
    void update(float dt);

    void add_floating_loot(const glm::vec3& world_pos, const std::string& text, const glm::vec4& color);

    void render(
        const PlayerController& player,
        const World& world,
        const HazardClock& hazard,
        const ExtractionSystem& extraction,
        const PlayerInventory& inventory,
        const SkillMatrix& skills,
        int current_level,
        const glm::mat4& view,
        const glm::mat4& proj
    );

private:
    void init_gl();
    void init_font_atlas();
    void draw_rect(float x, float y, float w, float h, const glm::vec4& color);
    void draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color);
    void render_crosshair(const PlayerController& player, const World& world);
    void render_floating_loot(const glm::mat4& view, const glm::mat4& proj);

    int m_width{1600};
    int m_height{900};

    Shader m_ui_shader;
    Shader m_text_shader;

    unsigned int m_rect_vao{0};
    unsigned int m_rect_vbo{0};

    unsigned int m_text_vao{0};
    unsigned int m_text_vbo{0};
    unsigned int m_font_tex{0};

    std::vector<FloatingLootText> m_floating_loot;
};

} // namespace Voidfall
