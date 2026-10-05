#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include "../player/character_class.hpp"

namespace Voidfall {

class World;

struct ChemicalFlare {
    uint32_t id{0};
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 color{0.05f, 0.85f, 1.0f};
    CharacterClass owner_class{CharacterClass::Scout};
    float lifetime{60.0f};
    float max_lifetime{60.0f};
    float light_radius{16.0f};
    bool is_grounded{false};

    bool is_alive() const { return lifetime > 0.0f; }
    float life_fraction() const { return max_lifetime > 0.0f ? (lifetime / max_lifetime) : 0.0f; }
};

class FlareManager {
public:
    static FlareManager& instance();

    void reset();
    void clear() { reset(); }
    void spawn_flare(const glm::vec3& origin, const glm::vec3& forward_dir, CharacterClass cls);
    void update(float dt, const World& world);

    const std::vector<ChemicalFlare>& flares() const { return m_flares; }
    std::vector<ChemicalFlare>& flares_mut() { return m_flares; }
    size_t active_count() const { return m_flares.size(); }

    static glm::vec3 get_flare_color(CharacterClass cls);

private:
    FlareManager() = default;
    std::vector<ChemicalFlare> m_flares;
    uint32_t m_next_id{1};
};

} // namespace Voidfall
