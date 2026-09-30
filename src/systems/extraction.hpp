#pragma once
#include <glm/glm.hpp>
#include <functional>

namespace Voidfall {

enum class ExtractionPhase {
    Dormant,
    BeaconDeployed, // 90s countdown with siren and landing pod recall
    PodLanded,      // Evacuate squad into pod
    Complete
};

class ExtractionSystem {
public:
    ExtractionSystem();

    void deploy_beacon(const glm::vec3& beacon_pos);
    void update(float dt, const glm::vec3& player_pos);

    ExtractionPhase phase() const { return m_phase; }
    float countdown() const { return m_countdown; }
    const glm::vec3& beacon_position() const { return m_beacon_pos; }
    float siren_pulse() const { return m_siren_pulse; }

    using ExtractionCompleteCallback = std::function<void()>;
    void set_on_complete(ExtractionCompleteCallback cb) { m_on_complete = std::move(cb); }

private:
    ExtractionPhase m_phase{ExtractionPhase::Dormant};
    glm::vec3 m_beacon_pos{0.0f};
    float m_countdown{90.0f};
    float m_siren_pulse{0.0f};
    ExtractionCompleteCallback m_on_complete;
};

} // namespace Voidfall
