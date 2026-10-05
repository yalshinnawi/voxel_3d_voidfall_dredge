#pragma once
#include <glm/glm.hpp>
#include <functional>

namespace Voidfall {

enum class ExtractionPhase {
    Dormant,
    BeaconDeployed, // Holdout countdown with siren and landing pod recall
    PodLanded,      // Evacuate squad into 3D pod
    Complete
};

class ExtractionSystem {
public:
    ExtractionSystem();

    void deploy_beacon(const glm::vec3& beacon_pos, float holdout_time = 40.0f);
    void update(float dt, const glm::vec3& player_pos);
    void force_evacuation_pod();
    void force_escape();

    ExtractionPhase phase() const { return m_phase; }
    float countdown() const { return m_countdown; }
    float initial_countdown() const { return m_initial_countdown; }
    const glm::vec3& beacon_position() const { return m_beacon_pos; }
    float siren_pulse() const { return m_siren_pulse; }

    float pod_drill_progress() const { return m_pod_drill_progress; }
    float ramp_extension() const { return m_ramp_extension; }
    bool is_pod_anchored() const { return m_pod_anchored; }
    bool is_ceiling_drill_active() const { return m_ceiling_drill_active; }
    float steam_timer() const { return m_steam_timer; }

    float defense_radius() const { return 12.0f; }
    bool is_player_in_perimeter(const glm::vec3& player_pos) const {
        return (m_phase == ExtractionPhase::BeaconDeployed || m_phase == ExtractionPhase::PodLanded) &&
               glm::distance(player_pos, m_beacon_pos) <= defense_radius();
    }

    using ExtractionCompleteCallback = std::function<void()>;
    using CeilingWarningCallback = std::function<void(const glm::vec3& beacon_pos)>;
    using PodArrivalCallback = std::function<void(const glm::vec3& beacon_pos)>;
    using SteamVentCallback = std::function<void(const glm::vec3& beacon_pos)>;

    void set_on_complete(ExtractionCompleteCallback cb) { m_on_complete = std::move(cb); }
    void set_on_ceiling_warning(CeilingWarningCallback cb) { m_on_ceiling_warning = std::move(cb); }
    void set_on_pod_arrival(PodArrivalCallback cb) { m_on_pod_arrival = std::move(cb); }
    void set_on_steam_vent(SteamVentCallback cb) { m_on_steam_vent = std::move(cb); }

private:
    ExtractionPhase m_phase{ExtractionPhase::Dormant};
    glm::vec3 m_beacon_pos{0.0f};
    float m_initial_countdown{40.0f};
    float m_countdown{40.0f};
    float m_siren_pulse{0.0f};

    // 3D Physical Extraction Pod arrival & boarding state
    float m_pod_drill_progress{0.0f}; // 0 = at ceiling, 1 = grounded on floor
    float m_ramp_extension{0.0f};     // 0 = closed, 1 = fully extended ramp
    bool m_pod_anchored{false};
    bool m_ceiling_drill_active{false};
    bool m_ceiling_drill_started{false};
    float m_steam_timer{0.0f};

    ExtractionCompleteCallback m_on_complete;
    CeilingWarningCallback m_on_ceiling_warning;
    PodArrivalCallback m_on_pod_arrival;
    SteamVentCallback m_on_steam_vent;
};

} // namespace Voidfall
