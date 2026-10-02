# Hazard Clock, Escalation Curves & Extraction Pacing

This reference defines the extraction survival loop, time limits, hazard stages, and beacon defense mechanics.

---

## 1. The Hazard Clock (15-Minute Expedition Loop)
The expedition pressure operates on four distinct hazard phases governed by [src/systems/hazard_clock.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/hazard_clock.cpp):

| Phase | Time Elapsed | Environmental State | Threat / Enemy Level | Recommended Player Action |
| :--- | :--- | :--- | :--- | :--- |
| **Phase 1: Survey** | 0:00 - 4:00 | Stable islands, clear visibility. | Passive critters, single Void Stalkers. | Scout resource veins, set anchor points. |
| **Phase 2: Agitation** | 4:00 - 8:00 | Radiation buildup (+1 rad/sec), light fog. | Swarm spawns increase (+50%). | Deep mining, deploy structural supports. |
| **Phase 3: Critical** | 8:00 - 12:00 | Seismic tremors, random island cave-ins. | Burrowers spawn; aggressive flanking. | Prepare extraction route, bank high-tier ore. |
| **Phase 4: Collapse** | 12:00 - 15:00+ | Lethal radiation (+5 rad/sec), island fracture. | Relentless swarms, void rifts open. | Call extraction beacon immediately or perish. |

---

## 2. Extraction Beacon Mechanics
Governed by [src/systems/extraction.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/extraction.cpp):
1. **Beacon Deployment**:
   - Requires flat or mined 3x3 voxel clearance.
   - Deploys a high-visibility sonar beacon ping visible across the sector.
2. **40-Second Holdout**:
   - Player must defend the perimeter within a 15-meter radius.
   - Beacon attracts nearby entities (swarms & Burrowers).
3. **Evac Pod Landing**:
   - Evac Pod touches down. Player must board within 3.0 meters.
   - Boarding completes expedition, triggering inventory appraisal and banking.
4. **Abandonment Penalties**:
   - Quitting via menu or dying before extraction applies a **50% currency penalty** to unbanked materials.
