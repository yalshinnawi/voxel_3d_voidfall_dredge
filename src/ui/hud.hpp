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

struct VoidStalker;
struct SeismicBurrower;

struct EnemyAwarenessMarker {
    glm::vec3 world_pos{0.0f};
    int marker_type{0}; // 0 = None, 1 = YellowExclamation, 2 = RedTriangle
    float state_timer{0.0f};
    bool has_los{true};
    float dist{0.0f};
};

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

struct HudNotification {
    std::string text;
    float timer{2.5f};
    float max_timer{2.5f};
    glm::vec4 color{1.0f, 0.45f, 0.15f, 1.0f};
    int count{1};
};

struct HudLayoutRect {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};
};

struct HudTopStackLayout {
    HudLayoutRect hazard_bar;
    HudLayoutRect seismic_banner;
    bool has_seismic_banner{false};
    HudLayoutRect evac_banner;
    bool has_evac_banner{false};
    std::vector<HudLayoutRect> notifications;
};

class HUD {
public:
    HUD(int screen_width, int height, bool headless = false);
    ~HUD();

    void resize(int width, int height);
    void update(float dt);

    void add_floating_loot(const glm::vec3& world_pos, const std::string& text, const glm::vec4& color);
    void add_loot_toast(const std::string& resource_name, const glm::vec4& color, int count = 1, int unit_points = 0);
    void show_warning(const std::string& msg, float duration = 2.0f);
    void trigger_damage_flash(float intensity = 0.5f) { m_damage_flash_timer = intensity; }
    float damage_flash_timer() const { return m_damage_flash_timer; }
    void trigger_enemy_blood_splatter(float intensity = 1.0f);
    size_t blood_splatter_count() const { return m_blood_splatters.size(); }
    void clear_blood_splatters() { m_blood_splatters.clear(); }
    void trigger_radiation_flash(float intensity = 0.5f) { m_radiation_flash_timer = intensity; }
    float radiation_flash_timer() const { return m_radiation_flash_timer; }
    void trigger_toxic_gas_flash(float intensity = 0.5f) { m_toxic_gas_flash_timer = intensity; }
    float toxic_gas_flash_timer() const { return m_toxic_gas_flash_timer; }
    void set_toxic_gas_exposure(float exposure, float dt);
    float toxic_gas_exposure() const { return m_toxic_gas_exposure; }
    void clear_target_info();
    void toggle_help_briefing() { m_show_help_briefing = !m_show_help_briefing; }
    bool is_help_briefing_visible() const { return m_show_help_briefing; }
    void set_death_sequence(bool active, float timer = 0.0f, float max_duration = 3.5f) {
        m_death_active = active;
        m_death_timer = timer;
        m_death_duration = max_duration;
    }

    const std::deque<HudNotification>& notifications() const { return m_notifications; }
    void clear_notifications() { m_notifications.clear(); }
    size_t notification_count() const { return m_notifications.size(); }

    /// Trigger visual reticle hit marker feedback (normal vs sneak attack critical)
    void trigger_hit_marker(bool is_critical = false, float damage = 0.0f);
    bool is_hit_marker_active() const { return m_hit_marker_timer > 0.0f; }
    bool is_hit_marker_critical() const { return m_hit_marker_is_crit; }
    float hit_marker_damage() const { return m_hit_marker_damage; }

    HudTopStackLayout compute_top_stack_layout(
        float ui_scale,
        float screen_w,
        bool is_tremoring,
        bool is_warning,
        ExtractionPhase extraction_phase
    ) const;

    std::vector<EnemyAwarenessMarker> compute_awareness_markers(
        const glm::vec3& cam_pos,
        const World& world,
        const std::vector<VoidStalker>* stalkers = nullptr,
        const std::vector<SeismicBurrower>* burrowers = nullptr
    ) const;

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
        int enemy_count = 0,
        const std::vector<VoidStalker>* stalkers = nullptr,
        const std::vector<SeismicBurrower>* burrowers = nullptr
    );

private:
    void init_gl();
    void init_font_atlas();
    void draw_rect(float x, float y, float w, float h, const glm::vec4& color);
    void draw_pill(float x, float y, float w, float h, const glm::vec4& border_col = glm::vec4(0.0f, 0.95f, 1.0f, 0.35f));
    void draw_triangle(float x0, float y0, float x1, float y1, float x2, float y2, const glm::vec4& color);
    void draw_line_segment(float x0, float y0, float x1, float y1, float thickness, const glm::vec4& color);
    void draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color);
    void draw_text_centered(const std::string& text, float box_x, float box_y, float box_w, float box_h, float scale, const glm::vec4& color);
    void draw_text_fitted(const std::string& text, float x, float y, float max_w, float base_scale, const glm::vec4& color, float min_scale = 0.50f);
    void draw_text_centered_fitted(const std::string& text, float box_x, float box_y, float box_w, float box_h, float base_scale, const glm::vec4& color, float min_scale = 0.50f);
    void render_crosshair(const PlayerController& player, const World& world);
    void render_floating_loot(const glm::mat4& view, const glm::mat4& proj);
    void render_loot_toasts();
    void render_enemy_awareness_markers(
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::vec3& cam_pos,
        const World& world,
        const std::vector<VoidStalker>* stalkers,
        const std::vector<SeismicBurrower>* burrowers
    );

    int m_width{1600};
    int m_height{900};

    Shader m_ui_shader;
    Shader m_text_shader;

    unsigned int m_rect_vao{0};
    unsigned int m_rect_vbo{0};
    unsigned int m_tri_vao{0};
    unsigned int m_tri_vbo{0};

    unsigned int m_text_vao{0};
    unsigned int m_text_vbo{0};
    unsigned int m_font_tex{0};

    struct OnScreenToxicDroplet {
        float x{0.0f};
        float y{0.0f};
        float vx{0.0f};
        float vy{0.0f};
        float size{6.0f};
        float alpha{0.6f};
        float life{1.0f};
        float max_life{1.0f};
    };

    struct OnScreenBloodSplatter {
        float x{0.0f};
        float y{0.0f};
        float size{24.0f};
        float alpha{0.95f};
        float life{2.2f};
        float max_life{2.2f};
        float drip_speed{10.0f};
        int droplet_count{5};
        float dx[6]{0.0f};
        float dy[6]{0.0f};
        float radii[6]{3.0f};
    };

    std::vector<FloatingLootText> m_floating_loot;
    std::deque<LootToast> m_loot_toasts;
    std::deque<HudNotification> m_notifications;
    float m_damage_flash_timer{0.0f};
    float m_radiation_flash_timer{0.0f};
    float m_toxic_gas_flash_timer{0.0f};
    float m_toxic_gas_exposure{0.0f};
    std::vector<OnScreenToxicDroplet> m_screen_toxic_particles;
    std::vector<OnScreenBloodSplatter> m_blood_splatters;
    float m_total_time{0.0f};
    bool m_show_help_briefing{false};
    float m_briefing_auto_timer{0.0f};
    bool m_death_active{false};
    float m_death_timer{0.0f};
    float m_death_duration{3.5f};
    float m_noise_peak{0.0f};
    float m_noise_peak_timer{0.0f};
    bool m_headless{false};

    // Reticle Hit Marker State
    float m_hit_marker_timer{0.0f};
    float m_hit_marker_duration{0.18f};
    bool m_hit_marker_is_crit{false};
    float m_hit_marker_damage{0.0f};

    // Weapon / Tool Switch Toast State
    ToolSlot m_last_active_tool{ToolSlot::MiningDrill};
    bool m_tool_initialized{false};
    float m_tool_switch_toast_timer{0.0f};
    std::string m_tool_switch_name{""};
    std::string m_tool_switch_details{""};
    glm::vec4 m_tool_switch_color{0.0f, 0.898f, 1.0f, 1.0f};
};

} // namespace Voidfall
