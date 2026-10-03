#pragma once
#include <functional>
#include <glm/glm.hpp>

namespace Voidfall {

class HazardClock {
public:
    HazardClock();

    void update(float dt);
    void set_sector_parameters(int sector);
    void reset();
    void force_tremor(float intensity = 1.0f);

    // Dynamic seismic stress disturbance hooks (mining rocks, shooting rocks, explosions)
    void on_rock_mined(uint8_t material_id);
    void on_weapon_impact(float stress_amount = 2.5f);
    void on_explosive_detonation(float stress_amount = 25.0f);
    void add_seismic_stress(float amount);
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
    float seismic_stress() const { return m_seismic_stress; }
    int rocks_mined_since_tremor() const { return m_rocks_mined; }
    int bullet_hits_since_tremor() const { return m_bullet_hits; }

    using TremorCallback = std::function<void(float intensity)>;
    using TremorWarningCallback = std::function<void()>;
    void set_on_tremor(TremorCallback cb) { m_on_tremor = std::move(cb); }
    void set_on_tremor_warning(TremorWarningCallback cb) { m_on_tremor_warning = std::move(cb); }

private:
    float m_radiation_level{0.0f};      // 0..100%
    float m_radiation_rate{0.45f};      // Percent increase per second
    float m_tremor_timer{45.0f};        // Estimated seconds to fault slip
    float m_tremor_interval{50.0f};

    // Dynamic Seismic Stress System
    float m_seismic_stress{0.0f};       // 0..100% accumulated tectonic strain
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

    TremorCallback m_on_tremor;
    TremorWarningCallback m_on_tremor_warning;
};

} // namespace Voidfall
