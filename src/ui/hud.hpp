#pragma once
#include "../graphics/shader.hpp"
#include "../player/controller.hpp"
#include "../player/loadout.hpp"
#include "../skills/skill_matrix.hpp"
#include "../systems/hazard_clock.hpp"
#include "../systems/extraction.hpp"
#include "../systems/noise_meter.hpp"
#include "../voxel/world.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <deque>

namespace Voidfall {

struct FloatingLootText {
    glm::vec3 world_pos;
    std::string text;
    std::string resource_name;
    int amount{0};
    int score{0};
    glm::vec4 color;
    float timer{1.8f};
    float max_timer{1.8f};
};

struct LootToast {
    std::string label;
    std::string resource_name;
    int count{1};
    int unit_points{0};
    glm::vec4 color{1.0f};
    float lifetime{2.5f};      // Total duration (e.g., 2.5s)
    float maxLifetime{2.5f};
};

class HUD {
public:
    HUD(int screen_width, int height);
    ~HUD();

    void resize(int width, int height);
    void update(float dt);

    void add_floating_loot(const glm::vec3& world_pos, const std::string& text, const glm::vec4& color);
    void add_loot_toast(const std::string& resource_name, const glm::vec4& color, int count = 1, int unit_points = 0);
    void show_warning(const std::string& msg, float duration = 2.0f);
    void trigger_damage_flash(float intensity = 0.5f) { m_damage_flash_timer = intensity; }
    void clear_target_info();
    void toggle_help_briefing() { m_show_help_briefing = !m_show_help_briefing; }
    bool is_help_briefing_visible() const { return m_show_help_briefing; }
    void set_death_sequence(bool active, float timer = 0.0f, float max_duration = 3.5f) {
        m_death_active = active;
        m_death_timer = timer;
        m_death_duration = max_duration;
    }

    void render(
        const PlayerController& player,
        const World& world,
        const HazardClock& hazard,
        const ExtractionSystem& extraction,
        const PlayerInventory& inventory,
        const SkillMatrix& skills,
        int current_level,
        const glm::mat4& view,
        const glm::mat4& proj,
        const class SurveyingSystem* surveying = nullptr,
        const NoiseMeter* noise_meter = nullptr,
        int enemy_count = 0
    );

private:
    void init_gl();
    void init_font_atlas();
    void draw_rect(float x, float y, float w, float h, const glm::vec4& color);
    void draw_pill(float x, float y, float w, float h, const glm::vec4& border_col = glm::vec4(0.0f, 0.95f, 1.0f, 0.35f));
    void draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color);
    void draw_text_centered(const std::string& text, float box_x, float box_y, float box_w, float box_h, float scale, const glm::vec4& color);
    void render_crosshair(const PlayerController& player, const World& world);
    void render_floating_loot(const glm::mat4& view, const glm::mat4& proj);
    void render_loot_toasts();

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
    std::deque<LootToast> m_loot_toasts;
    float m_warning_timer{0.0f};
    std::string m_warning_message;
    float m_damage_flash_timer{0.0f};
    float m_total_time{0.0f};
    bool m_show_help_briefing{false};
    float m_briefing_auto_timer{0.0f};
    bool m_death_active{false};
    float m_death_timer{0.0f};
    float m_death_duration{3.5f};
};

} // namespace Voidfall
