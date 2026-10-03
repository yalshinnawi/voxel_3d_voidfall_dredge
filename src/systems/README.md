# Mission Systems & Environmental Hazards (`src/systems/`)

The `systems` subsystem orchestrates environmental escalation, radiation accumulation, periodic seismic tremors, and the mission extraction holdout sequence.

---

## 📁 Source Files

| File | Primary Responsibility | Key Classes / Structs |
| :--- | :--- | :--- |
| [`hazard_clock.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/hazard_clock.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/hazard_clock.cpp) | Tracks expedition radiation buildup, tremor countdowns, and triggers cave-ins. | [`HazardClock`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/hazard_clock.hpp#L7) |
| [`extraction.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/extraction.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/extraction.cpp) | Manages emergency beacon deployment, 40-second defense timer, siren pulses, and evacuation pod landing. | [`ExtractionSystem`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/extraction.hpp#L14), [`ExtractionPhase`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/extraction.hpp#L7) |

---

## ☢️ Hazard Clock Escalation Curve

The hazard clock escalates continuously from landing:

1. **Phase 1: Stable (0:00 - 4:00)**: Baseline environmental conditions, zero suit radiation damage.
2. **Phase 2: Agitation (4:00 - 8:00)**: Suit geiger counter clicks, low radiation accumulation ($+0.45\% / \text{sec}$), distant acoustic rumble cues.
3. **Phase 3: Critical (8:00 - 12:00)**: Heavy tremors trigger at fixed intervals ($\approx 50\text{s}$), causing unsupported ceiling stone to detach and collapse.
4. **Phase 4: Catastrophic (12:00+)**: Severe suit HUD glitching, radiation spikes, relentless tremors.

---

## 🚨 Extraction Beacon Protocol

1. **Deployment (`Key B`)**: Player deploys the portable beacon at their current location. The extraction state transitions to `ExtractionPhase::BeaconDeployed`.
2. **40-Second Holdout**:
   - The beacon countdown begins from $40.0\text{s}$.
   - High-intensity emergency siren flashes red with dynamic quadratic falloff.
   - Seismic tremors and hostile activity peak around the perimeter.
3. **Pod Touchdown**: At $0.0\text{s}$, the drop pod touches down (`ExtractionPhase::PodLanded`).
4. **Evacuation**: Moving inside the beacon pod radius triggers the completion callback, banking all gathered minerals and displaying the mission debrief.
