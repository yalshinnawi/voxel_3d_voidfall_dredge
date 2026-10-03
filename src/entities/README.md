# Entities & Physics Subsystem (`src/entities/`)

The `entities` subsystem handles dynamic rigid-body entities in the voxel world, falling debris physics resulting from cave-ins, and the hostile subterranean life forms native to the void sectors.

---

## 📁 Subsystem Structure

| File / Folder | Primary Responsibility | Key Classes / Structs |
| :--- | :--- | :--- |
| [`dynamic_debris.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/dynamic_debris.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/dynamic_debris.cpp) | Rigid-body simulation for falling rock clusters sliced from the voxel terrain. | [`DynamicDebris`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/dynamic_debris.hpp#L10), [`DynamicDebris::CollisionResult`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/dynamic_debris.hpp#L22) |
| [`enemies/`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/enemies/) | Subterranean alien predator behavioral architectures and spawn rules. | Hostile entities cataloged in design wiki |

---

## 🪨 Dynamic Debris Simulation

When a cave-in is triggered (via excavation or seismic tremor):
1. **Instantiation**: Sliced unanchored voxel clusters are instantiated as [`DynamicDebris`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/dynamic_debris.hpp#L10) objects with random tumbling angular velocity and initial linear momentum.
2. **Physics Tick**:
   - Accelerates under downward gravity ($-24.0 \text{ m/s}^2$).
   - Raycasts against the static [`World`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/world.hpp) to detect terrain collisions.
   - Bounces off solid stone with restitution damping or shatters into dust and break particles upon high-velocity impact.
3. **Player Collision & Sheltering**:
   - Deals blunt trauma damage to delvers on impact.
   - If the player is standing directly beneath a player-placed industrial bulkhead (`MAT_INDUSTRIAL_BULKHEAD`), the bulkhead absorbs the kinetic impact, sheltering the player from lethal crushing damage.
