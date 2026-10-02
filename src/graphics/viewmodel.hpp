#pragma once
#include "shader.hpp"
#include "../player/loadout.hpp"
#include "../player/character_class.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <functional>

namespace Voidfall {

struct ViewmodelVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec4 color;
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
        const glm::vec3& drill_target_pos = glm::vec3(0.0f)
    );

private:
    void init_geometry();
    void add_box(std::vector<ViewmodelVertex>& verts, const glm::vec3& min_p, const glm::vec3& max_p, const glm::vec4& color);
    void add_cylinder(std::vector<ViewmodelVertex>& verts, const glm::vec3& base, float radius, float length, int segments, const glm::vec4& color, int axis = 2);
    void add_cone(std::vector<ViewmodelVertex>& verts, const glm::vec3& base, float radius, float length, int segments, const glm::vec4& color);

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

    float m_total_time{0.0f};
    float m_drill_rotation{0.0f};
    float m_switch_timer{0.0f};
    ToolSlot m_last_tool{ToolSlot::MiningDrill};
    CharacterClass m_character_class{CharacterClass::Demolitionist};
    int m_drill_speed_tier{0};
    int m_drill_durability_tier{0};
    float m_spark_timer{0.0f};

    std::function<void(const glm::vec3&, const glm::vec3&)> m_on_spark;
};

} // namespace Voidfall
