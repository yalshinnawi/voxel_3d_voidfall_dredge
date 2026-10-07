#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include "enemies/void_stalker.hpp"

namespace Voidfall {

class World;

/// Tangible physical remains of a slain aberrant entity
struct EnemyCarcass {
    uint32_t id{0};
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angular_velocity{0.0f};
    float scale{1.3f};
    bool is_sleeping{false};
    float hp{20.0f};
    float max_hp{20.0f};
    float alpha{1.0f};
    bool is_dissolving{false};
    float dissolve_timer{0.0f};
    float lifetime{0.0f};
    float max_lifetime{45.0f}; // Persists on cavern floor for 45s before decaying into ash
    StalkerRole role{StalkerRole::Melee};
    glm::vec3 lethal_hit_dir{0.0f};
    bool harvested{false};

    bool isSleeping() const { return is_sleeping; }
};

/// Manages persistent physical enemy carcasses, settling ragdoll physics,
/// harvest/salvage interactions, and capped circular pool dissolver decay.
class CarcassManager {
public:
    static constexpr size_t MAX_CARCASSES = 48;

    static CarcassManager& instance();

    CarcassManager() = default;
    ~CarcassManager() = default;

    /// Spawns a physical settling carcass from a slain enemy
    uint32_t spawn_carcass(
        const glm::vec3& position,
        const glm::quat& rotation,
        const glm::vec3& lethal_hit_dir = glm::vec3(0.0f, 0.0f, 0.0f),
        StalkerRole role = StalkerRole::Melee,
        float scale = 1.3f
    );

    /// Step carcass physics (gravity, collision with world voxels, settling, and dissolver fade)
    void update(float dt, World& world);

    /// Monsters and carcasses cannot be mined for chitin with the drill.
    /// Always returns false, preserving carcasses without yielding chitin drops.
    bool harvest_nearest(
        const glm::vec3& origin,
        float radius,
        float drill_damage,
        int& out_carapace,
        int& out_biomass,
        glm::vec3* out_shatter_pos = nullptr
    );

    /// Access active carcasses
    const std::vector<EnemyCarcass>& carcasses() const { return m_carcasses; }
    std::vector<EnemyCarcass>& carcasses_mut() { return m_carcasses; }

    /// Active carcass count (for regression testing)
    int GetActiveCount() const { return static_cast<int>(m_carcasses.size()); }
    int active_count() const { return GetActiveCount(); }

    /// Reset for new expedition
    void clear();

private:
    std::vector<EnemyCarcass> m_carcasses;
    uint32_t m_next_id{1};
};

} // namespace Voidfall
