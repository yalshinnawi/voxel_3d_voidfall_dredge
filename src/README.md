# Voidfall Dredge - Engine Source (`src/`)

This directory contains the complete source code for the **Voidfall: Dredge** voxel extraction engine written in C++20 and OpenGL 4.5 Core Profile.

---

## 🏛️ Subsystem Architecture

The engine is decomposed into modular subsystems under `src/`:

```
src/
├── main.cpp         # Application entry point & CLI flag dispatcher
├── core/            # Engine lifecycle, GLFW window, logger, crash handler, save system, visual harness
├── voxel/           # 32x32x32 chunk storage, 8-byte packed vertices, greedy mesher, structural BFS
├── graphics/        # Clustered renderer, PBR voxel shaders, viewmodel, post-processing (SSAO, Bloom, Fog)
├── entities/        # Dynamic debris rigid-body physics, falling voxel clusters, enemy structures
├── player/          # First-person controller, grapple physics, jetpack, class archetypes, upgrades
├── skills/          # Skill matrix, XP trees, seismic sonar surveying pulse
├── systems/         # Hazard clock, radiation buildup, cave tremors, extraction beacon defense
├── ui/              # Orbital hub carousel, in-game HUD, pause menu, debrief, 8x8 font rendering
└── net/             # Client-server UDP networking, delta-compressed block sync, state snapshots
```

---

## 🔄 Frame Lifecycle & Simulation Loop

The engine executes in [`src/core/application.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/application.cpp) via a semi-fixed timestep update loop:

```
                  ┌──────────────────────┐
                  │      GLFW Input      │
                  └──────────┬───────────┘
                             │
                             ▼
             ┌───────────────────────────────┐
             │   fixed_tick(1/60s)           │
             │   • Net Host / Client sync    │
             │   • Player Physics & Grapple  │
             │   • Dynamic Debris Collisions │
             │   • Hazard Clock & Radiation  │
             │   • Extraction Beacon Timer   │
             └───────────────┬───────────────┘
                             │
                             ▼
             ┌───────────────────────────────┐
             │   render(float dt)            │
             │   • Upload Dirty Meshes       │
             │   • PBR Voxel Geometry Pass   │
             │   • Volumetric Fog Compute    │
             │   • SSAO & Dual-MRT Bloom     │
             │   • ViewModel (Mining Drill)  │
             │   • HUD / Hub 2D Ortho Pass   │
             │   • Swap Buffers              │
             └───────────────────────────────┘
```

---

## 📜 Key Architectural Constraints & Invariants

1. **Zero Dynamic Allocations in Hot Paths**:
   - No `malloc`, `new`, or unbound `std::vector::push_back` inside per-frame `render()` or `fixed_tick()`.
   - Voxel meshing stages reuse vertex buffers via `stage_mesh()` in [`src/voxel/chunk.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/chunk.hpp).
2. **Coordinate Conventions**:
   - Right-handed coordinate system.
   - **Y-Up**: $+X$ = East/Right, $+Y$ = Up, $+Z$ = South/Forward.
   - Chunks are $32 \times 32 \times 32$ voxels.
3. **OpenGL Context Invariant**:
   - All OpenGL buffer uploads (`glBufferData`, `glBindVertexArray`) and drawing calls must execute on the **main render thread**.
4. **Fast TDD Pipeline**:
   - All modules are tested via unit tests in [`tests/test_unit_all.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_unit_all.cpp), progression tests in [`tests/test_progression.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_progression.cpp), and end-to-end mission tests in [`tests/test_e2e_expeditions.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_e2e_expeditions.cpp).
