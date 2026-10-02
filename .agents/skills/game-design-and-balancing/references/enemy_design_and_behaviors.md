# Voidfall Dredge: Enemy Archetypes & Spatial AI Design

This reference establishes the threat hierarchy, behavior trees, and voxel interactions for enemies in Voidfall Dredge.

---

## 1. Threat Taxonomy

### A. Void Stalker (Infiltrator / Skirmisher)
- **Role**: High-mobility ambush predator that punishes isolated miners in dark tunnels.
- **Health / Armor**: Low HP (75), zero armor.
- **Speed**: 1.4x Player Sprint.
- **Behaviors**:
  - Wall-running & ceiling clinging along voxel surfaces.
  - Retreats into unlit caves when hit by high-intensity light (sonar / flashlight).
  - Leaps through mined 1x2 voxel corridors.
- **Counters**: Scout seismic sonar reveals silhouette through walls; light sources induce temporary blind state.

### B. Seismic Burrower (Brute / Destructor)
- **Role**: Heavy subterranean entity that modifies voxel topology dynamically.
- **Health / Armor**: High HP (450), 40% frontal kinetic armor.
- **Speed**: 0.7x Player Sprint.
- **Behaviors**:
  - Chews through low-hardness voxels (Granite / Dirt), carving tunnels toward the player.
  - Slams ground to trigger local voxel cave-ins and seismic detachments ([src/voxel/structural_check.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp)).
- **Counters**: Demolitionist Volatile Satchel charges bypass armor; Vanguard shield blocks shockwave.

### C. Volatile Crystalline Leech (Swarm / Area Denial)
- **Role**: Small explosive nuisance attracted to freshly mined mineral dust.
- **Health**: 25 HP.
- **Speed**: 1.1x Player Sprint.
- **Behaviors**:
  - Spawns from freshly broken Volatile Crystal nodes.
  - Self-detonates on contact, dealing AoE kinetic/radiation damage and destroying nearby voxels.
- **Counters**: Long-range pickaxe swings, grappling hook displacement.

---

## 2. 3D Voxel Spatial AI & Navigation
- **Grid Coordinates**: Uses chunk-based 3D coordinates aligned with `ChunkPos` (32x32x32).
- **Pathfinding**: 3D A* with voxel clearance masks:
  - Entities require 1x2x1 empty voxel clearance to traverse.
  - Jump height capability is capped at 1.2 voxel blocks.
- **Gravity & Fall Damage**: Non-flying enemies obey multi-axis Swept AABB gravity similar to the player controller ([src/player/controller.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.cpp)). Falling into the void kills non-flying entities instantly.
