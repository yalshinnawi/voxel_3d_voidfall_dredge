#pragma once
#include <functional>
#include <glm/glm.hpp>

namespace Voidfall {

class HazardClock {
public:
    HazardClock();

    void update(float dt);

    float radiation_level() const { return m_radiation_level; }
    float tremor_timer() const { return m_tremor_timer; }
    bool is_tremoring() const { return m_is_tremoring; }
    float tremor_intensity() const { return m_tremor_intensity; }

    using TremorCallback = std::function<void(float intensity)>;
    void set_on_tremor(TremorCallback cb) { m_on_tremor = std::move(cb); }

private:
    float m_radiation_level{0.0f};      // 0..100%
    float m_radiation_rate{0.45f};      // Percent increase per second
    float m_tremor_timer{45.0f};        // Countdown to next tremor
    float m_tremor_interval{50.0f};
    bool m_is_tremoring{false};
    float m_tremor_duration{4.0f};
    float m_tremor_elapsed{0.0f};
    float m_tremor_intensity{0.0f};

    TremorCallback m_on_tremor;
};

} // namespace Voidfall
