#pragma once
#include "shader.hpp"
#include "../player/loadout.hpp"
#include "../player/character_class.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <functional>
#include <algorithm>

namespace Voidfall {

struct ViewmodelVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec4 material; // x: metallic, y: roughness, z: emissive, w: ao
};

class ViewModel {
public:
    ViewModel();
    ~ViewModel();

    void set_on_spark_callback(std::function<void(const glm::vec3&, const glm::vec3&)> cb) {
        m_on_spark = std::move(cb);
    }

    void on_tool_switched();

    void set_character_class(CharacterClass cls);
    void set_drill_speed_tier(int tier) { m_drill_speed_tier = tier; }
    void set_drill_durability_tier(int tier) { m_drill_durability_tier = tier; }

    void render(
        float dt,
        float aspect,
        bool is_drilling,
        bool is_in_range,
        ToolSlot active_tool,
        const glm::vec3& drill_target_pos = glm::vec3(0.0f),
        bool is_firing = false,
        bool is_crouching = false,
        bool is_reloading = false,
        float reload_progress = 0.0f,
        bool has_placed_charge = false,
        bool is_melee_shoving = false,
        float movement_speed = 0.0f,
        bool is_grounded = true,
        float melee_progress = 0.0f,
        bool is_aiming = false,
        float zoom_progress = 0.0f
    );

    bool is_reloading() const { return m_is_reloading; }
    float reload_progress() const { return m_reload_progress; }
    bool is_melee_shoving() const { return m_shove_timer > 0.0f; }
    float melee_shove_progress() const {
        return m_shove_timer > 0.0f ? glm::clamp(1.0f - (m_shove_timer / 0.35f), 0.0f, 1.0f) : 0.0f;
    }
    const glm::vec3& recoil_offset() const { return m_recoil_offset; }
    float recoil_pitch() const { return m_recoil_pitch; }

private:
    void init_geometry();

    void add_box(
        std::vector<ViewmodelVertex>& verts,
        const glm::vec3& min_p,
        const glm::vec3& max_p,
        const glm::vec4& color,
        const glm::vec4& material = glm::vec4(0.0f, 0.6f, 0.0f, 1.0f)
    );

    void add_transformed_box(
        std::vector<ViewmodelVertex>& verts,
        const glm::mat4& transform,
        const glm::vec3& half_extents,
        const glm::vec4& color,
        const glm::vec4& material = glm::vec4(0.0f, 0.6f, 0.0f, 1.0f)
    );

    void add_capsule(
        std::vector<ViewmodelVertex>& verts,
        const glm::vec3& p1,
        const glm::vec3& p2,
        float r1,
        float r2,
        int segments,
        const glm::vec4& color,
        const glm::vec4& material = glm::vec4(0.0f, 0.6f, 0.0f, 1.0f)
    );

    void add_cylinder(
        std::vector<ViewmodelVertex>& verts,
        const glm::vec3& base,
        float radius,
        float length,
        int segments,
        const glm::vec4& color,
        const glm::vec4& material = glm::vec4(0.0f, 0.6f, 0.0f, 1.0f),
        int axis = 2,
        bool cap_ends = true
    );

    void add_cone(
        std::vector<ViewmodelVertex>& verts,
        const glm::vec3& base,
        float radius_base,
        float radius_tip,
        float length,
        int segments,
        const glm::vec4& color,
        const glm::vec4& material = glm::vec4(0.0f, 0.6f, 0.0f, 1.0f)
    );

    void add_tube(
        std::vector<ViewmodelVertex>& verts,
        const glm::vec3& base,
        float radius_inner,
        float radius_outer,
        float length,
        int segments,
        const glm::vec4& color,
        const glm::vec4& material = glm::vec4(0.0f, 0.6f, 0.0f, 1.0f),
        int axis = 2
    );

    void add_lens_disc(
        std::vector<ViewmodelVertex>& verts,
        const glm::vec3& center,
        float radius,
        int segments,
        const glm::vec4& color,
        const glm::vec4& material = glm::vec4(0.9f, 0.05f, 0.2f, 1.0f),
        int axis = 2
    );

    void add_forearm_and_gauntlet(
        std::vector<ViewmodelVertex>& verts,
        const glm::vec3& elbow_origin,
        const glm::vec3& wrist_pos,
        const glm::vec3& palm_pos,
        const glm::vec4& sleeve_col,
        const glm::vec4& accent_col,
        const glm::vec4& glove_col,
        bool is_right_arm
    );

    Shader m_shader;

    unsigned int m_chassis_vao{0};
    unsigned int m_chassis_vbo{0};
    size_t m_chassis_count{0};

    unsigned int m_bit_vao{0};
    unsigned int m_bit_vbo{0};
    size_t m_bit_count{0};

    unsigned int m_piston_vao{0};
    unsigned int m_piston_vbo{0};
    size_t m_piston_count{0};

    unsigned int m_carbine_vao{0};
    unsigned int m_carbine_vbo{0};
    size_t m_carbine_count{0};

    unsigned int m_scattergun_vao{0};
    unsigned int m_scattergun_vbo{0};
    size_t m_scattergun_count{0};

    unsigned int m_railgun_vao{0};
    unsigned int m_railgun_vbo{0};
    size_t m_railgun_count{0};

    unsigned int m_detonator_vao{0};
    unsigned int m_detonator_vbo{0};
    size_t m_detonator_count{0};

    unsigned int m_plunger_vao{0};
    unsigned int m_plunger_vbo{0};
    size_t m_plunger_count{0};

    float m_total_time{0.0f};
    float m_drill_rotation{0.0f};
    float m_switch_timer{0.0f};
    float m_muzzle_flash_timer{0.0f};
    ToolSlot m_last_tool{ToolSlot::MiningDrill};
    CharacterClass m_character_class{CharacterClass::Demolitionist};
    int m_drill_speed_tier{0};
    int m_drill_durability_tier{0};
    float m_spark_timer{0.0f};
    bool m_is_reloading{false};
    float m_reload_progress{0.0f};
    glm::vec3 m_recoil_offset{0.0f};
    float m_recoil_pitch{0.0f};
    float m_shove_timer{0.0f};
    bool m_was_melee_shoving{false};
    float m_walk_bob_phase{0.0f};
    float m_walk_bob_weight{0.0f};

    std::function<void(const glm::vec3&, const glm::vec3&)> m_on_spark;
};

} // namespace Voidfall
