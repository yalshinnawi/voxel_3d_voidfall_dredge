# Voidfall Dredge - Game Engine Guidelines & Agent Rules

## 1. Project Overview & Architecture
`Voidfall Dredge` is a 3D extraction-survival voxel game engine built in modern C++20 and OpenGL.
- **Language Standard**: C++20 (`set(CMAKE_CXX_STANDARD 20)`).
- **Graphics & API**: OpenGL (Core Profile via GLAD), GLFW for windowing/input, GLM for mathematics.
- **Shaders**: GLSL (vertex, fragment, and compute shaders in [assets/shaders/](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/)).
- **Subsystems**:
  - `src/core/`: Application lifecycle, GLFW window management, logger, save system ([save_data.json](file:///d:/Projects/voxel_3d_voidfall_dredge/save_data.json)).
  - `src/graphics/`: PBR voxel renderer, post-processing (SSAO, Bloom, Tone mapping), viewmodel, volumetric fog compute shaders.
  - `src/voxel/`: Chunk data structures, greedy mesher (`src/voxel/greedy_mesher.cpp`), world management, structural integrity checks (`src/voxel/structural_check.cpp`).
  - `src/player/`: First-person controller, class archetypes, progression/upgrades.
  - `src/entities/`: Dynamic debris physics and extraction items.
  - `src/systems/`: Hazard clock, expedition timer, extraction beacons.
  - `src/ui/`: Orbital hub, in-game HUD, pause menu, text rendering (bitmap/atlas font renderer).
  - `src/net/`: Optional multiplayer host/client sockets (GameNetworkingSockets / Winsock).

---

## 2. Build & Execution Workflow (User Environment: Git Bash)
- **Primary Shell Convention**: The user uses **Git Bash**. Always format commands with forward slashes (`/`), POSIX paths, and executable invocations formatted for bash (`./build/...`).
- **Build with CMake (MSVC / Windows)**:
  ```bash
  cmake --build build --config Release
  ```
- **Run the Game**:
  - Launch via Git Bash:
    ```bash
    ./build/Release/VoidfallDredge.exe
    ```
- **Run Test Suites (Fastest TDD Commands)**:
  - **Single Command High-Speed Parallel Runner (Recommended)**:
    ```bash
    python scripts/tdd.py
    ```
    *(Builds incrementally and executes all 3 suites in parallel in < 0.1s)*
  - **Parallel CTest**:
    ```bash
    ctest --test-dir build -C Release -j 3 --output-on-failure
    ```
  - **Continuous TDD Watch Mode**:
    ```bash
    python scripts/tdd.py --watch
    ```
  - **Direct Test Executables**:
    ```bash
    ./build/Release/test_unit_all.exe
    ./build/Release/test_e2e_expeditions.exe
    ./build/Release/test_progression.exe
    ```
- **Asset Synchronization**:
  - CMake copies [assets/](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/) to `<TARGET_FILE_DIR>/assets` as a post-build step. When editing shaders or textures, rebuild the target or update the build output directory so changes reflect in the running executable.
- **Diagnostic Logs**:
  - Runtime logs, OpenGL errors, and stack traces are written to [voidfall.log](file:///d:/Projects/voxel_3d_voidfall_dredge/voidfall.log). Always inspect this log when investigating runtime crashes.

### 2.1 Automated Visual Testing & Screenshot Inspection
The engine includes an automated playthrough and visual test harness ([src/core/screenshot.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/screenshot.hpp), [src/core/screenshot.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/screenshot.cpp)) that executes all 7 gameplay phases, calculates luminance/health metrics, and writes both a consolidated contact sheet montage and individual frames.

- **Run Full Automated Visual Playthrough**:
  ```bash
  ./build/Release/VoidfallDredge.exe --auto-play-test
  ```
- **Accelerated Screenshot Analysis Protocol (Fastest Method)**:
  Instead of sequentially inspecting all 7 multi-megabyte PNG files across 7 separate tool calls (which takes minutes and bloats context):
  1. **Instant Metrics**: Run `python scripts/analyze_screenshots.py` or inspect `screenshots/visual_report.txt` (completes in 0.1s).
  2. **Single-Turn Visual Verification**: Call `view_file` on `screenshots/00_all_phases_montage.jpg` (or `.png`). This displays all 7 phases side-by-side in a 4x2 grid with slot labels and health status in a single instant glance!
  3. **Targeted Deep-Dive**: If a specific anomaly is spotted in a phase, inspect that frame's lightweight preview in `screenshots/previews/0X_*.jpg` (~40KB).
- **Master Captured Files**:
  - `screenshots/00_all_phases_montage.jpg` / `.png`: 4x2 contact sheet of all 7 phases + matrix card.
  - `screenshots/visual_report.txt` / `.json`: Luminance and non-black sanity check reports.
  - `screenshots/previews/01_*.jpg` to `07_*.jpg`: Fast lightweight 800x450 previews.
  - `screenshots/01_main_menu.png` to `07_extraction_beacon.png`: Uncompressed 1600x900 masters.
- **Single Screenshot Capture Flag**:
  ```bash
  ./build/Release/VoidfallDredge.exe --screenshot screenshots/target_view.png
  ```
- **Live In-Game Hotkey**:
  Pressing **F12** at any time during gameplay saves a timestamped PNG to `screenshots/screenshot_YYYYMMDD_HHMMSS.png`.

---

## 3. Game Development Conventions & Constraints

### 3.1 Memory & Voxel Performance
- **Zero Allocations in Render/Update Loops**: Do not introduce dynamic allocations (`new`, `malloc`, unbounded `std::vector::push_back`) in per-frame rendering or hot physics loops.
- **Chunk Meshing**: Meshing operations occur via greedy meshing. Pre-allocate or reuse vertex/index buffer staging vectors rather than reallocating per chunk.
- **Coordinate Conventions**:
  - Right-handed coordinate system.
  - **Y-up** (X = East/Right, Y = Up, Z = South/Forward).
  - Chunk coordinates are standard voxel blocks (typically 16x16x16 or 32x32x32).

### 3.2 Shaders & Graphics
- Vertex and fragment shaders follow `#version 330 core` (or `#version 430 core` for compute shaders like [volumetric_fog.comp](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/volumetric_fog.comp)).
- Maintain exact layout compatibility between C++ uniform structs and shader uniform blocks.
- When creating or modifying shaders, verify uniform names match C++ calls in [renderer.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp).

### 3.3 Coding Style & Robustness
- Prefer modern C++20 features (concepts, `std::span`, designated initializers) where clean and supported by MSVC.
- Avoid platform-specific Windows APIs directly unless isolated in platform wrappers (`#ifdef _WIN32`).
- Check null pointers and enforce invariants with asserts or `LOG_ERROR` in [src/core/logger.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/logger.hpp).

---

## 4. Game Design Wiki & Systems Balancing
- **Living Game Design Wiki**: Game mechanics, lore, enemies, and voxel tables reside in [wiki/](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/) (`wiki/index.md`). Maintain traceability whenever changing balance.
- **Progression Math**: Upgrade costs scale exponentially via $100 \times 1.6^{\text{tier}-1}$. Run [simulate_economy.py](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/game-design-and-balancing/scripts/simulate_economy.py) to verify progression math.
- **Hazard Clock**: 15-minute 4-phase escalation curve and 40s beacon holdouts are documented in [wiki/mechanics/hazard_clock.md](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/mechanics/hazard_clock.md).
