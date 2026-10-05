#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Voidfall {

class World;
class VoidStalkerManager;

enum class VaultObjectiveState {
    Undiscovered,
    Discovered,
    Breached,
    RelicRetrieved
};

struct PrecursorVault {
    glm::ivec3 chamber_min{0};
    glm::ivec3 chamber_max{0};
    glm::ivec3 door_pos{0};
    glm::ivec3 relic_pos{0};
    VaultObjectiveState state{VaultObjectiveState::Undiscovered};
    bool exists{false};
};

class MissionSystem {
public:
    static constexpr int VAULT_BONUS_XP = 350;
    static constexpr int VAULT_BONUS_TITANIUM = 5;

    MissionSystem();

    void reset(int current_level = 1);

    /// Embeds a sealed precursor vault during world generation or level init
    void embed_precursor_vault(World& world, int level);

    /// Checks if a satchel charge detonation at (blast_pos, radius) breaches the vault door
    bool check_satchel_vault_breach(World& world, const glm::vec3& blast_pos, float blast_radius);

    /// Checks player proximity to relic or vault door to update objective discovery and pickup
    void update(float dt, World& world, const glm::vec3& player_pos);

    /// Trigger reactive gas ignition: burns away MAT_GAS in blast radius, incinerates nearby stalkers
    int ignite_gas_pocket(World& world, const glm::vec3& blast_pos, float blast_radius, VoidStalkerManager& stalkers);

    // Callbacks
    using NotificationCallback = std::function<void(const std::string&, float duration)>;
    using GasIgniteCallback = std::function<void(const glm::vec3& center, float radius)>;
    void set_on_notification(NotificationCallback cb) { m_on_notification = std::move(cb); }
    void set_on_gas_ignited(GasIgniteCallback cb) { m_on_gas_ignited = std::move(cb); }

    // State queries
    const PrecursorVault& vault() const { return m_vault; }
    VaultObjectiveState vault_state() const { return m_vault.state; }
    bool is_relic_retrieved() const { return m_vault.state == VaultObjectiveState::RelicRetrieved; }
    bool is_vault_breached() const { return m_vault.state == VaultObjectiveState::Breached || m_vault.state == VaultObjectiveState::RelicRetrieved; }

    int total_bonus_xp() const { return is_relic_retrieved() ? VAULT_BONUS_XP : 0; }
    int total_bonus_titanium() const { return is_relic_retrieved() ? VAULT_BONUS_TITANIUM : 0; }

    std::string get_secondary_objective_text() const;

private:
    int m_level{1};
    PrecursorVault m_vault;
    NotificationCallback m_on_notification;
    GasIgniteCallback m_on_gas_ignited;
};

} // namespace Voidfall
