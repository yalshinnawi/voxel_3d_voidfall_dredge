# Hostile Entities & Alien AI (`src/entities/enemies/`)

This directory is designated for hostile subterranean entities native to the void sectors. The behaviors, threat tiers, and spawn conditions are modeled after the living design wiki in [`wiki/entities/enemies.md`](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/entities/enemies.md).

---

## 👾 Enemy Roster & Architectural Specs

### 1. Void Stalker
- **Threat Level**: Medium (High in pitch black caverns).
- **Spawn Rules**: Naturally active in unlit cavern shafts and deep crevices below $Y = 16$.
- **AI Behavior**:
  - Clings to ceiling and cave walls using multi-raycast surface normals.
  - Stalks delvers from behind; charges when drilling noise is detected.
  - Disoriented by delver headlamp beams and Scout seismic sonar pings.

### 2. Seismic Burrower
- **Threat Level**: High.
- **Spawn Rules**: Emerges during Phase 3 & 4 of the Hazard Clock (post-8 minutes or during extraction holdouts).
- **AI Behavior**:
  - Breaches and excavates soft voxels (`MAT_FRACTURED_GRANITE`, `MAT_THERMITE_SLAG`) directly towards the extraction beacon.
  - Triggers localized secondary cave-ins via [`StructuralCheck::solve_cavein()`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp#L24).
  - Vulnerable to Demolitionist shaped charges.

### 3. Volatile Crystalline Leech
- **Threat Level**: Low-Medium (Dangerous in swarms).
- **Spawn Rules**: Disturbed when Voidite Crystal clusters (`MAT_VOIDITE_CRYSTAL`) are mined.
- **AI Behavior**:
  - Rapid erratic hopping pathfinding toward loose mineral drops.
  - Detonates on lethal damage, triggering chain reactions with nearby volatile crystals.

---

## 🔌 Integration Hook Points

When implementing full entity state machines for enemies:
- Update loop belongs in [`Application::fixed_tick()`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/application.cpp#L58).
- Synchronize network states via `PacketType::DynamicDebrisSync` or new dedicated enemy packet types in [`src/net/packet_types.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/packet_types.hpp).
- Avoid per-frame allocations during enemy pathfinding or raycasts.
