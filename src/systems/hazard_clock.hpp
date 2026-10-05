#pragma once
#include <functional>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include <glm/glm.hpp>

namespace Voidfall {

struct SeismicZone {
    int cell_x{0};
    int cell_z{0};
    glm::vec3 epicenter{0.0f};
    float stress{0.0f};           // 0..100% accumulated tectonic strain
    int rocks_mined{0};
    int bullet_hits{0};
    float damage_weight{0.0f};
};

class HazardClock {
public:
    static constexpr float ZONE_SIZE = 18.0f;       // Horizontal dimension of an excavation zone
    static constexpr float TREMOR_RADIUS = 24.0f;   // Radius of tremor spall and shaking from epicenter

    HazardClock();

    void update(float dt);
    void set_sector_parameters(int sector);
    void reset();
    void force_tremor(float intensity = 1.0f, const std::optional<glm::vec3>& pos = std::nullopt);

    // Player position synchronization for localized HUD meter and camera trauma
    void set_player_position(const glm::vec3& pos) { m_player_pos = pos; }
    glm::vec3 player_position() const { return m_player_pos; }

    // Dynamic seismic stress disturbance hooks (mining rocks, shooting walls, explosions)
    void on_rock_mined(const glm::vec3& pos, uint8_t material_id);
    void on_rock_mined(uint8_t material_id);

    void on_weapon_impact(const glm::vec3& pos, float stress_amount = 2.5f);
    void on_weapon_impact(float stress_amount = 2.5f);

    void on_explosive_detonation(const glm::vec3& pos, float stress_amount = 25.0f);
    void on_explosive_detonation(float stress_amount = 25.0f);

    void add_seismic_stress(const glm::vec3& pos, float amount);
    void add_seismic_stress(float amount);

    void trigger_tremor_sequence_at(const glm::vec3& epicenter, float intensity = 1.0f);
    void trigger_tremor_sequence();

    float radiation_level() const { return m_radiation_level; }
    float tremor_timer() const { return m_tremor_timer; }
    bool is_tremoring() const { return m_is_tremoring; }
    bool is_warning() const { return m_warning_phase; }
    float warning_timer() const { return m_warning_timer; }
    float warning_duration() const { return m_warning_duration; }
    float tremor_elapsed() const { return m_tremor_elapsed; }
    float tremor_duration() const { return m_tremor_duration; }
    float tremor_intensity() const { return m_tremor_intensity; }
    float tremor_cooldown_timer() const { return m_tremor_cooldown_timer; }
    void set_tremor_cooldown_timer(float t) { m_tremor_cooldown_timer = t; }

    // Localized seismic stress queries
    float seismic_stress() const;
    float seismic_stress_at(const glm::vec3& pos) const;
    float highest_seismic_stress() const;
    const SeismicZone* highest_stress_zone() const;
    const std::unordered_map<uint64_t, SeismicZone>& zones() const { return m_zones; }

    // Active tremor spatial queries
    glm::vec3 active_tremor_epicenter() const { return m_active_epicenter; }
    float active_tremor_radius() const { return m_active_radius; }
    float active_tremor_proximity(const glm::vec3& pos) const;
    bool is_player_in_tremor_zone() const { return active_tremor_proximity(m_player_pos) > 0.05f; }

    int rocks_mined_since_tremor() const { return m_rocks_mined; }
    int bullet_hits_since_tremor() const { return m_bullet_hits; }

    // Callbacks supporting both spatial and legacy signatures
    using SpatialTremorCallback = std::function<void(float intensity, const glm::vec3& epicenter, float radius)>;
    using SpatialTremorWarningCallback = std::function<void(const glm::vec3& epicenter, float radius)>;
    using LegacyTremorCallback = std::function<void(float intensity)>;
    using LegacyTremorWarningCallback = std::function<void()>;

    void set_on_tremor(SpatialTremorCallback cb) { m_on_spatial_tremor = std::move(cb); }
    void set_on_tremor(LegacyTremorCallback cb) { m_on_legacy_tremor = std::move(cb); }
    void set_on_tremor_warning(SpatialTremorWarningCallback cb) { m_on_spatial_warning = std::move(cb); }
    void set_on_tremor_warning(LegacyTremorWarningCallback cb) { m_on_legacy_warning = std::move(cb); }

    static uint64_t compute_zone_key(int cell_x, int cell_z) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cell_x)) << 32) |
                static_cast<uint64_t>(static_cast<uint32_t>(cell_z));
    }

private:
    SeismicZone& get_or_create_zone(const glm::vec3& pos);
    void finish_tremor();

    float m_radiation_level{0.0f};      // 0..100%
    float m_radiation_rate{0.45f};      // Percent increase per second
    float m_tremor_timer{45.0f};        // Estimated seconds to fault slip
    float m_tremor_interval{50.0f};
    float m_tremor_cooldown_timer{0.0f}; // Minimum refractory period between tremors

    // Localized Seismic Zones System
    std::unordered_map<uint64_t, SeismicZone> m_zones;
    glm::vec3 m_player_pos{0.0f};
    float m_stress_decay_rate{1.8f};    // Natural stress dissipation per second when calm
    float m_geological_creep{0.25f};    // Slow background fault creep per second
    int m_rocks_mined{0};               // Rocks mined since last tremor
    int m_bullet_hits{0};               // Projectile impacts on rocks since last tremor

    bool m_warning_phase{false};
    float m_warning_timer{0.0f};
    const float m_warning_duration{2.5f};

    bool m_is_tremoring{false};
    float m_tremor_duration{5.0f};
    float m_tremor_elapsed{0.0f};
    float m_tremor_intensity{0.0f};

    glm::vec3 m_active_epicenter{0.0f};
    float m_active_radius{TREMOR_RADIUS};

    SpatialTremorCallback m_on_spatial_tremor;
    SpatialTremorWarningCallback m_on_spatial_warning;
    LegacyTremorCallback m_on_legacy_tremor;
    LegacyTremorWarningCallback m_on_legacy_warning;
};

} // namespace Voidfall
