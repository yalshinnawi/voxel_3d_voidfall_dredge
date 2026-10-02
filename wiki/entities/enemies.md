# Enemy Threats & Behaviors

This document lists the hostile life forms native to the void sectors.

---

## 1. Void Stalker
- **Threat Level**: Medium (High in unlit areas).
- **Spawn Conditions**: Ambient caves, dark corners, low illumination.
- **Behavior**:
  - Clings to walls/ceilings using 3D voxel raycasts.
  - Stalks behind the player, leaping to attack when the player begins mining.
  - Weak to Scout Sonar pulses and bright light sources.

---

## 2. Seismic Burrower
- **Threat Level**: High.
- **Spawn Conditions**: Post-Minute 8 (Hazard Phase 3).
- **Behavior**:
  - Destroys soft voxels in its direct path to reach the player.
  - Triggers local cave-ins ([src/voxel/structural_check.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp)).
  - Weak to Demolitionist explosive charges.

---

## 3. Volatile Crystalline Leech
- **Threat Level**: Low-Medium (Hazardous in groups).
- **Spawn Conditions**: Triggered by breaking Volatile Crystal nodes.
- **Behavior**:
  - Swarms toward mined mineral debris.
  - Explodes on death or contact, chaining detonations with nearby volatile voxels.
