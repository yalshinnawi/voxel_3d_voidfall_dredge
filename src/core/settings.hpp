#pragma once

#include <algorithm>
#include <string>

namespace Voidfall {

/// Configurable player settings with audio channel volume sliders,
/// ear safety acoustic comfort options, and rig input/display controls.
struct GameSettings {
    // ── Input & Display ──
    float mouse_sensitivity{0.12f};
    float fov{75.0f};
    float brightness{1.0f};       // 0.40x to 2.00x cavern brightness/exposure modifier
    float screen_shake{0.20f};    // 0.00x to 1.00x screen shake trauma multiplier (comfort setting)

    // ── Granular Audio Channel Options [0.0 .. 1.0] ──
    float master_volume{1.0f};    // Overall output gain
    float sfx_volume{1.0f};       // Mining drill, voxel fractures, explosives, footsteps
    float enemy_volume{1.0f};     // Void stalker chitter, snarling, lunge, death
    float ambient_volume{1.0f};   // Subterranean cavern drone, seismic tremors, geiger radiation
    float ui_volume{1.0f};        // Menu navigation, upgrade terminals, critical health alarms
    bool mute_all{false};         // Instant master mute toggle

    void sanitize() {
        mouse_sensitivity = std::clamp(mouse_sensitivity, 0.02f, 0.50f);
        fov = std::clamp(fov, 60.0f, 110.0f);
        brightness = std::clamp(brightness, 0.40f, 2.00f);
        screen_shake = std::clamp(screen_shake, 0.0f, 1.0f);
        master_volume = std::clamp(master_volume, 0.0f, 1.0f);
        sfx_volume = std::clamp(sfx_volume, 0.0f, 1.0f);
        enemy_volume = std::clamp(enemy_volume, 0.0f, 1.0f);
        ambient_volume = std::clamp(ambient_volume, 0.0f, 1.0f);
        ui_volume = std::clamp(ui_volume, 0.0f, 1.0f);
    }
};

} // namespace Voidfall
