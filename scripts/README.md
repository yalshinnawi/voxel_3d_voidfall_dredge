# Developer & Test Automation Scripts (`scripts/`)

This directory contains developer automation tools for high-speed test-driven development (TDD), screenshot diagnostics, and economy balance simulations.

---

## 🛠️ Script Catalog

| Script | Purpose | Execution Command |
| :--- | :--- | :--- |
| [`tdd.py`](file:///d:/Projects/voxel_3d_voidfall_dredge/scripts/tdd.py) | **Primary TDD Tool**: Builds incrementally and executes all 3 test suites in parallel (<0.1s). | `python scripts/tdd.py` (or `python scripts/tdd.py --watch` for continuous file watching) |
| [`analyze_screenshots.py`](file:///d:/Projects/voxel_3d_voidfall_dredge/scripts/analyze_screenshots.py) | **Visual Diagnostic Tool**: Parses automated playthrough frame metrics, tests luminance, and cleans preview cache. | `python scripts/analyze_screenshots.py` (pass `--keep-previews` to retain temporary preview images) |
| [`simulate_economy.py`](file:///d:/Projects/voxel_3d_voidfall_dredge/scripts/simulate_economy.py) | **Balancing Tool**: Models the exponential EXP scaling curve, mineral drop rates, and progression tiers. | `python scripts/simulate_economy.py` |
| [`package_release.py`](file:///d:/Projects/voxel_3d_voidfall_dredge/scripts/package_release.py) | **Distribution Packager**: Builds Release binary, bundles runtime DLLs & assets, and generates a standalone ZIP archive for sharing. | `python scripts/package_release.py` (or double-click `package_game.bat`) |

---

## 🚀 Recommended Workflows

### 1. High-Speed Code & Test Cycle
Whenever modifying C++ code in `src/`:
```bash
python scripts/tdd.py
```
This automatically invokes CMake, builds out-of-date targets, runs `test_unit_all.exe`, `test_progression.exe`, and `test_e2e_expeditions.exe` concurrently, and returns formatted pass/fail metrics in milliseconds.

### 2. Full Visual Playthrough Verification
To test graphics rendering and gameplay state transitions end-to-end:
```bash
# 1. Run automated playthrough (captures 10 phases + contact sheet montage)
./build/Release/VoidfallDredge.exe --auto-play-test

# 2. Parse metrics & clean preview cache in 0.1s
python scripts/analyze_screenshots.py

# 3. View the master montage contact sheet
# (Open screenshots/00_all_phases_montage.jpg or screenshots/00_all_phases_montage.png)
```

### 3. Packaging & Distributing to Friends
To build a clean, self-contained standalone ZIP package for friends:
```bash
python scripts/package_release.py
# Or on Windows, double-click root:
./package_game.bat
```
This automatically compiles the Release binary, bundles all shaders, textures, and sounds, co-locates necessary Microsoft VC++ runtime DLLs, and produces `dist/VoidfallDredge_v1.0.zip`.
