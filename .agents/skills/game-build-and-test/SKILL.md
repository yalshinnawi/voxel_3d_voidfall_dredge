---
name: game-build-and-test
description: >-
  Use this skill whenever building the Voidfall Dredge C++ engine, executing unit or E2E tests,
  troubleshooting compilation/linker errors, or diagnosing runtime crashes via voidfall.log.
---

# Game Build, Test & Diagnostics

This skill outlines the procedures for compiling and validating the Voidfall Dredge game engine.

## 1. Building the Project

The project uses CMake with MSVC on Windows.

### Standard Build Command
```powershell
cmake --build build --config Release
```

For incremental builds of a specific test target:
```powershell
cmake --build build --config Release --target test_unit_all
cmake --build build --config Release --target test_e2e_expeditions
cmake --build build --config Release --target test_progression
```

### Full Clean Reconfigure
```powershell
cmake -B build -S .
cmake --build build --config Release
```

---

## 2. Running Automated Tests

### A. High-Speed Parallel TDD Runner (Recommended & Mandatory for Agents)
Use [scripts/tdd.py](file:///d:/Projects/voxel_3d_voidfall_dredge/scripts/tdd.py) for instantaneous parallel test execution and auto-builds:
```bash
# MANDATORY FOR AGENTS: Builds incrementally and runs ALL 10 test suites in parallel (~1s):
python scripts/tdd.py

# Continuous TDD watch mode (recompiles & re-tests automatically on file save):
python scripts/tdd.py --watch

# Developer rapid micro-iterations (bypasses heavy collision tests):
python scripts/tdd.py --fast

# Target a specific suite:
python scripts/tdd.py --test unit
python scripts/tdd.py --test progression
python scripts/tdd.py --test e2e
python scripts/tdd.py --test collision
python scripts/tdd.py --test enemy
python scripts/tdd.py --test burrower
python scripts/tdd.py --test audio
```
> [!NOTE]
> `scripts/tdd.py` contains an automatic **Mtime Staleness Guard**. Even if `--no-build` is requested, it detects if source/header files were modified after the binaries and automatically forces an incremental build to ensure test fidelity.

### B. Parallel CTest Execution
Run all test suites concurrently via CTest:
```bash
ctest --test-dir build -C Release -j 3 --output-on-failure
```

### C. Direct Test Binaries
- `./build/Release/test_unit_all.exe`
- `./build/Release/test_e2e_expeditions.exe`
- `./build/Release/test_progression.exe`
- `./build/Release/test_level_collision.exe`
- `./build/Release/test_enemy_stalker.exe`
- `./build/Release/test_enemy_burrower.exe`
- `./build/Release/test_audio.exe`
- `./build/Release/test_audio_system.exe`
- `./build/Release/test_combat_omni_ai.exe`
- `./build/Release/test_gameplay_mechanics.exe`
- `./build/Release/test_spawn_safety.exe`

---

## 3. Automated Visual Testing & Playthroughs

The engine supports automated visual frame capture via `--auto-play-test` and `--screenshot`:
```bash
./build/Release/VoidfallDredge.exe --auto-play-test
```
This executes an automated 10-phase gameplay loop, computes visual metrics, and writes:
- **Consolidated Contact Sheet Montage**: `screenshots/00_all_phases_montage.jpg` (or `.png`):
  All 10 gameplay screens (Menus, Level Select, Delver Roster, Upgrades, Cavern, Drilling, Abilities, Esc Menu, Extraction, Debrief) + Subsystem Telemetry + Visual Health Matrix card stitched into a single 4x3 grid.
- **Fast Diagnostics Report**: `screenshots/visual_report.txt` and `screenshots/visual_report.json`
- **Lightweight Previews**: `screenshots/previews/<phase>_YYYYMMDD_HHMMSS.jpg` (~40KB each, stamped with in-image date/time label; auto-removed during analysis)
- **Master PNGs**: `screenshots/01_main_menu.png` through `10_mission_debrief.png`

### Fast Screenshot Analysis Protocol (Optimized for Speed)
> [!IMPORTANT]
> **Do NOT sequentially inspect individual PNGs across multiple tool turns.** Loading 10 full 1600x900 PNGs consumes tens of megabytes of context and takes several minutes of round-trips.
>
> **Follow this 1-step workflow instead:**
> 1. Run `python scripts/analyze_screenshots.py` for instantaneous metrics and **automatic cleanup of temporary previews** (saving disk space). Pass `--keep-previews` to retain them.
> 2. Call `view_file` on `screenshots/00_all_phases_montage.jpg` to visually inspect all 10 screens simultaneously in a **single turn (< 1 second)**!
### 3.1 Workspace Test Artifact & Screenshot Cleanup
To prevent stale test files and unmanaged preview captures from dirtying the workspace:
- **Full Workspace Test Artifact Cleanup**:
  ```bash
  python scripts/cleanup_test_artifacts.py
  ```
- **3D Model Reference Catalog**:
  - Maintained strictly in `docs/models/` via:
    ```bash
    python scripts/capture_models.py
    ```
  - Automatically prunes any non-canonical files outside the 6 models, montages, and reports.

---

## 4. Silent Testing & Non-Audible Execution Policy

To protect developer hearing and prevent audio driver contention:
- **All automated tests run non-audible by default**: `python scripts/tdd.py`, `test_audio.exe`, and `python scripts/analyze_audio.py` render to internal memory buffers without opening the physical hardware audio device.
- **Automated game playthroughs are muted**: Running `VoidfallDredge.exe --auto-play-test`, `--capture-models`, or `--capture-level-shapes` automatically suppresses sound output.
- **Silent Game Launch**: Use `./build/Release/VoidfallDredge.exe --mute` or `--silent` to launch the game client with zero audio.
- **Global Environment Override**: Set `export VOIDFALL_MUTE_AUDIO=1` to disable hardware audio engine-wide.
- **Opt-in Audible Playback**: Pass `--audible` to `test_audio.exe` or `VoidfallDredge.exe` only when explicitly intending to listen through speakers/headset.

---

## 5. Investigating Runtime Crashes & Invariants

When investigating crashes, assertion failures, or OpenGL state bugs, consult:
- Reference Guide: [references/diagnostics_guide.md](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/game-build-and-test/references/diagnostics_guide.md)
- Log File: [voidfall.log](file:///d:/Projects/voxel_3d_voidfall_dredge/voidfall.log)
- Persistent Data: [save_data.json](file:///d:/Projects/voxel_3d_voidfall_dredge/save_data.json)
