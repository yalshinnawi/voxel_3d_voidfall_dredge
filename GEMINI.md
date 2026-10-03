# Voidfall Dredge - Game Engine Guidelines & Agent Rules

## 1. Project Overview & Architecture
`Voidfall Dredge` is a 3D extraction-survival voxel game engine built in modern C++20 and OpenGL.
- **Language Standard**: C++20 (`set(CMAKE_CXX_STANDARD 20)`).
- **Graphics & API**: OpenGL (Core Profile via GLAD), GLFW for windowing/input, GLM for mathematics.
- **Shaders**: GLSL (vertex, fragment, and compute shaders in [assets/shaders/](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/)).
- **Subsystems**:
  - `src/core/`: Application lifecycle, GLFW window management, logger, save system ([save_data.json](file:///d:/Projects/voxel_3d_voidfall_dredge/saves/save_data.json)).
  - `src/graphics/`: PBR voxel renderer, post-processing (SSAO, Bloom, Tone mapping), viewmodel, volumetric fog compute shaders.
  - `src/voxel/`: Chunk data structures, greedy mesher (`src/voxel/greedy_mesher.cpp`), world management, structural integrity checks (`src/voxel/structural_check.cpp`).
  - `src/player/`: First-person controller, class archetypes, progression/upgrades.
  - `src/skills/`: Delver skill matrix, proficiency branches, seismic sonar pulse scanner.
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
The engine includes an automated playthrough and visual test harness ([src/core/screenshot.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/screenshot.hpp), [src/core/screenshot.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/screenshot.cpp)) that executes all 10 gameplay phases, calculates luminance/health metrics, and writes both a consolidated contact sheet montage and individual frames.

- **Run Full Automated Visual Playthrough**:
  ```bash
  ./build/Release/VoidfallDredge.exe --auto-play-test
  ```
- **Accelerated Screenshot Analysis Protocol (Fastest Method)**:
  Instead of sequentially inspecting multi-megabyte PNG files across separate tool calls:
  1. **Instant Metrics & Preview Auto-Cleanup**: Run `python scripts/analyze_screenshots.py` (completes in 0.1s). This parses all 10 phase metrics, verifies luminance, and automatically cleans up temporary files in `screenshots/previews/` to save disk space (pass `--keep-previews` to retain them).
  2. **Single-Turn Visual Verification**: Call `view_file` on `screenshots/00_all_phases_montage.jpg` (or `.png`). This displays all 10 screens side-by-side in a 4x3 grid with slot labels, subsystem health, and visual matrix in a single instant glance!
  3. **Targeted Deep-Dive**: If a specific anomaly is spotted in a phase, inspect that frame's lightweight preview in `screenshots/previews/<phase>_YYYYMMDD_HHMMSS.jpg` (~40KB, stamped with in-image date/time label) using `--keep-previews`.
- **Master Captured Files**:
  - `screenshots/00_all_phases_montage.jpg` / `.png`: 4x3 contact sheet of all 10 game screens + system telemetry + visual matrix card.
  - `screenshots/visual_report.txt` / `.json`: Luminance, timestamp, and non-black sanity check reports.
  - `screenshots/previews/<phase>_YYYYMMDD_HHMMSS.jpg`: Lightweight 800x450 previews stamped with date/time banners (auto-cleaned after analysis).
  - `screenshots/01_main_menu.png` to `10_mission_debrief.png`: Uncompressed 1600x900 masters.
- **Single Screenshot Capture Flag**:
  ```bash
  ./build/Release/VoidfallDredge.exe --screenshot screenshots/target_view.png
  ```
- **Live In-Game Testing Hotkeys (Active ONLY in Test Mode via `--test`, `--test-save`, or `--auto-play-test`)**:
  - **F5**: Grant Testing EXP & Resources (+1000 EXP, +25 Voidite, +10 Titanium).
  - **F6**: Execute Diagnostic Upgrade & Respec Cycle (purchases upgrades, validates stats, tests 85% refund).
  - **F7**: Force Seismic Tremor (triggers screen trauma, HUD warning, ceiling debris).
  - **F8**: Force Evac Pod Touchdown (drops beacon holdout countdown to $\le 1\,\text{s}$).
  - **F9**: Force Escape Evacuation (extracts squad immediately into Debrief screen).
- **Global Screenshot Hotkey (Always Active)**:
  - **F12**: Capture timestamped PNG to `screenshots/screenshot_YYYYMMDD_HHMMSS.png`.

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
- **Progression Math**: Upgrade costs scale exponentially via $100 \times 1.6^{\text{tier}-1}$. Run [simulate_economy.py](file:///d:/Projects/voxel_3d_voidfall_dredge/scripts/simulate_economy.py) (`python scripts/simulate_economy.py`) to verify progression math.
- **Hazard Clock**: 15-minute 4-phase escalation curve and 40s beacon holdouts are documented in [wiki/mechanics/hazard_clock.md](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/mechanics/hazard_clock.md).
