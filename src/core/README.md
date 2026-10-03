# Engine Core Subsystem (`src/core/`)

The `core` subsystem manages application lifecycle, windowing, platform OS interfaces, persistence, logging, crash handling, and visual test capture.

---

## 📁 Source Files

| File | Primary Responsibility | Key Classes / Structs |
| :--- | :--- | :--- |
| [`application.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/application.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/application.cpp) | Top-level game orchestrator, state machine, fixed tick, and render loop. | [`Application`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/application.hpp#L43), [`AppConfig`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/application.hpp#L26), [`GameState`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/orbital_hub.hpp#L16) |
| [`window.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/window.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/window.cpp) | GLFW 3.3+ window abstraction, OpenGL context initialization, input capture. | [`Window`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/window.hpp#L12) |
| [`logger.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/logger.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/logger.cpp) | Thread-safe logging to stdout and `voidfall.log`, Windows SEH crash handling. | [`Logger`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/logger.hpp#L16), `VF_LOG_INFO`, `VF_LOG_ERROR` |
| [`save_system.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/save_system.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/save_system.cpp) | Serialization & deserialization of player progression, upgrades, and sector stats. | [`UserProfile`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/save_system.hpp#L34), [`SaveSystem`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/save_system.hpp#L52) |
| [`screenshot.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/screenshot.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/screenshot.cpp) | Framebuffer capture, STB image write, 10-phase 4x3 montage builder, luminance analysis. | [`VisualTestHarness`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/screenshot.hpp#L27), [`FrameVisualMetrics`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/screenshot.hpp#L8) |
| [`pch.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/pch.hpp) | Precompiled header aggregating standard library headers and GLM mathematics. | Common standard library includes |
| [`glad/glad.c`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/glad/glad.c) | GLAD OpenGL 4.5 Core Profile function loader. | OpenGL API bindings |

---

## ⚙️ Game State Machine

Managed in [`Application::m_state`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/application.hpp#L91):
- `GameState::MainMenu`: Title screen, continue button, sector selection.
- `GameState::OrbitalHub`: Persistent station terminal, upgrade spending, class selection, sector expedition launcher.
- `GameState::Gameplay`: Active 3D voxel mining, hazards, player simulation.
- `GameState::Paused`: In-game pause menu with resume, restart, and hub abandonment options.
- `GameState::Debrief`: Mission outcome screen (Victory/Defeat, banked resources, sector badges).

---

## 📸 Automated Visual Testing & Screenshot Pipeline

The `VisualTestHarness` captures frames during `--auto-play-test`:
- Captures uncompressed 1600x900 PNG masters for each phase.
- Generates a consolidated **1600x675 4x3 contact sheet montage** (`screenshots/00_all_phases_montage.png`) with slot banners, health status, and timestamps.
- Produces diagnostics in `screenshots/visual_report.txt` and `screenshots/visual_report.json`.
- Evaluates luminance (`mean_luminance`) and verifies non-black ratio (`non_black_ratio >= 0.15`).

---

## 💾 Save System Schema

Player profile is persisted to `saves/save_data.json` with:
- `save_version`: Schema version for backward compatibility migrations.
- `total_exp`, `total_voidite`, `total_titanium`: Persistent currencies.
- `selected_class_id`: Current character archetype (0: Demolitionist, 1: Vanguard, 2: Scout).
- `upgrades`: 6 upgrade tiers (0..5).
- `sector_records`: Best completion rate and badge for each sector (1..3).
