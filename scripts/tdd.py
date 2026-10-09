#!/usr/bin/env python3
"""
Voidfall Dredge - Ultra-Fast TDD Runner & File Watcher
High-speed parallel test execution, build coordination, and instant feedback loop.
"""

import sys
import os
import subprocess
import time
import argparse
from concurrent.futures import ThreadPoolExecutor

# Always enforce silent headless audio and bypass background mesher for all test child processes
os.environ["VOIDFALL_MUTE_AUDIO"] = "1"
os.environ["VOIDFALL_HEADLESS_TEST"] = "1"

CONFIG = "Release"
BUILD_DIR = "build"
EXE_EXT = ".exe" if sys.platform == "win32" else ""

TESTS = {
    "unit": {
        "target": "test_unit_all",
        "exe": f"test_unit_all{EXE_EXT}",
        "name": "Comprehensive Unit Tests (19 tests)"
    },
    "progression": {
        "target": "test_progression",
        "exe": f"test_progression{EXE_EXT}",
        "name": "Progression & Upgrades Tests"
    },
    "e2e": {
        "target": "test_e2e_expeditions",
        "exe": f"test_e2e_expeditions{EXE_EXT}",
        "name": "E2E Expeditions Lifecycle (5 scenarios)"
    },
    "collision": {
        "target": "test_level_collision",
        "exe": f"test_level_collision{EXE_EXT}",
        "name": "Level Design, Base Shapes, Sightlines & Mesh Integrity (9 modules)"
    },
    "enemy": {
        "target": "test_enemy_stalker",
        "exe": f"test_enemy_stalker{EXE_EXT}",
        "name": "Void Stalker Enemy AI, Combat, Perception & Waves (6 modules)"
    },
    "burrower": {
        "target": "test_enemy_burrower",
        "exe": f"test_enemy_burrower{EXE_EXT}",
        "name": "Seismic Burrower Voxel Excavation, Kinetic Charges & Cave-Ins (6 modules)"
    },
    "audio": {
        "target": "test_audio",
        "exe": f"test_audio{EXE_EXT}",
        "name": "Audio Engine, 3D Spatial SFX & Ear Safety Mastering (9 modules)"
    },
    "audio_system": {
        "target": "test_audio_system",
        "exe": f"test_audio_system{EXE_EXT}",
        "name": "Audio Subsystem, Ambience Crossfade, Voice Decay & Teardown (6 modules)"
    },
    "combat_omni_ai": {
        "target": "test_combat_omni_ai",
        "exe": f"test_combat_omni_ai{EXE_EXT}",
        "name": "Combat Lunge Separation, Persistent Carcasses, Omni AI & Stance (6 modules)"
    },
    "gameplay_mechanics": {
        "target": "test_gameplay_mechanics",
        "exe": f"test_gameplay_mechanics{EXE_EXT}",
        "name": "Gameplay Mechanics: Agitation Loop, Crouch Noise, Aiming Parallax & Monster Death Lifecycle (4 modules)"
    },
    "load_and_stress": {
        "target": "test_load_and_stress",
        "exe": f"test_load_and_stress{EXE_EXT}",
        "name": "Load, Stress & Late-Round Pacing / Anti-Clipping (5 modules)"
    },
    "voxel_geometry": {
        "target": "test_voxel_geometry",
        "exe": f"test_voxel_geometry{EXE_EXT}",
        "name": "Watertight Voxel Geometry, Topo Smoothing & Ramp Traversal (5 modules)"
    },
    "fluid": {
        "target": "test_fluid_simulation",
        "exe": f"test_fluid_simulation{EXE_EXT}",
        "name": "Cellular Automaton Fluid Sim, Sub-Block Waterlogging & Slope Conformance (7 modules)"
    },
    "permeation": {
        "target": "test_fluid_permeation",
        "exe": f"test_fluid_permeation{EXE_EXT}",
        "name": "Liquid Permeation, Ledge Cascades, Infilling & Cross-Chunk Meshing (5 modules)"
    },
    "fluid_physics": {
        "target": "test_fluid_physics",
        "exe": f"test_fluid_physics{EXE_EXT}",
        "name": "Fluid Physics: Anti-Stacking, Bullet Safety, Drainage & Translucent Meshing (4 modules)"
    },
    "fluid_geometry": {
        "target": "test_fluid_geometry",
        "exe": f"test_fluid_geometry{EXE_EXT}",
        "name": "Generalized Sub-Block Fluid Filling & Watertight Inter-Level Flow (4 modules)"
    },
    "minecraft_fluid": {
        "target": "test_minecraft_fluid",
        "exe": f"test_minecraft_fluid{EXE_EXT}",
        "name": "Minecraft Corner Fluid Meshing, Flow Fields & Non-Full Block Infilling (3 modules)"
    }
}

def run_cmd(cmd, cwd=None):
    res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, shell=isinstance(cmd, str))
    return res.returncode, res.stdout, res.stderr

def build_targets(target=None):
    t0 = time.time()
    cmd = ["cmake", "--build", BUILD_DIR, "--config", CONFIG]
    if target:
        cmd.extend(["--target", target])
    
    code, out, err = run_cmd(cmd)
    dt = time.time() - t0
    if code != 0:
        print("\n[x] BUILD FAILED:")
        print(out)
        print(err)
        return False, dt
    return True, dt

def check_sources_staleness(selected_test_keys):
    """
    Returns (True, reason) if any source/header file is newer than any selected test binary,
    or if any test binary is missing.
    """
    watch_dirs = ["src", "include", "tests"]
    max_src_mtime = 0.0
    for d in watch_dirs:
        if not os.path.exists(d):
            continue
        for root, _, files in os.walk(d):
            for f in files:
                if f.endswith((".cpp", ".c", ".hpp", ".h", ".glsl", ".comp")):
                    p = os.path.join(root, f)
                    try:
                        m = os.path.getmtime(p)
                        if m > max_src_mtime:
                            max_src_mtime = m
                    except OSError:
                        pass

    for k in selected_test_keys:
        info = TESTS[k]
        bin_path = os.path.join(BUILD_DIR, CONFIG, info["exe"])
        if not os.path.exists(bin_path):
            bin_path = os.path.join(BUILD_DIR, info["exe"])
        if not os.path.exists(bin_path):
            return True, f"Executable missing: {bin_path}"
        try:
            bin_mtime = os.path.getmtime(bin_path)
            if max_src_mtime > bin_mtime:
                t_src = time.strftime('%H:%M:%S', time.localtime(max_src_mtime))
                t_bin = time.strftime('%H:%M:%S', time.localtime(bin_mtime))
                return True, f"Source edited at {t_src} after binary build at {t_bin} ({info['exe']})"
        except OSError:
            return True, "Unable to inspect binary timestamp"

    return False, "Binaries are fully up to date"

def execute_single_test(key):
    info = TESTS[key]
    bin_path = os.path.join(BUILD_DIR, CONFIG, info["exe"])
    if not os.path.exists(bin_path):
        bin_path = os.path.join(BUILD_DIR, info["exe"])
    
    if not os.path.exists(bin_path):
        return key, False, 0.0, f"Executable not found: {bin_path}"

    t0 = time.time()
    code, out, err = run_cmd([bin_path], cwd=os.path.dirname(bin_path))
    dt = time.time() - t0
    output_msg = (out + ("\n" + err if err else "")).strip() if code != 0 else ""
    return key, (code == 0), dt, output_msg

def run_tests_parallel(test_keys, fail_fast=False):
    t_start = time.time()
    results = {}
    max_threads = min(len(test_keys), os.cpu_count() or 8)
    with ThreadPoolExecutor(max_workers=max_threads) as executor:
        futures = {executor.submit(execute_single_test, k): k for k in test_keys}
        for future in futures:
            key, passed, dt, msg = future.result()
            results[key] = (passed, dt, msg)
            if fail_fast and not passed:
                executor.shutdown(wait=False, cancel_futures=True)
                break

    total_time = time.time() - t_start
    all_passed = all(r[0] for r in results.values()) and len(results) == len(test_keys)
    return results, total_time, all_passed

def print_test_summary(results, build_time, total_test_time):
    print("\n" + "=" * 68)
    print("  VOIDFALL DREDGE - TDD TEST SUITE RESULTS")
    print("=" * 68)
    print(f"  Build Time: {build_time:.2f}s  |  Test Execution Time: {total_test_time:.2f}s (parallel)")
    print("-" * 68)
    print(f"  {'STATUS':<10} {'SUITE':<36} {'TIME':<12}")
    print("-" * 68)

    for k, (passed, dt, msg) in results.items():
        name = TESTS[k]["name"]
        status = "[+] PASS" if passed else "[-] FAIL"
        print(f"  {status:<10} {name:<36} {dt*1000:>6.1f} ms")
        if not passed and msg:
            print(f"\n--- Output from {k} ---")
            print(msg.strip()[:1000])
            print("------------------------\n")

    print("=" * 68)
    overall = all(r[0] for r in results.values())
    print(f"  OVERALL RESULT: {'ALL TESTS PASSED' if overall else 'TEST FAILURES DETECTED'}")
    print("=" * 68 + "\n")
    return overall

def watch_loop(selected_tests, fail_fast):
    watch_dirs = ["src", "include", "tests"]
    print("\n[*] Starting TDD File Watcher... (Monitoring src/, include/, tests/)")
    print("[*] Press Ctrl+C to stop.\n")

    last_mtimes = {}

    def get_max_mtime():
        max_t = 0.0
        for d in watch_dirs:
            if not os.path.exists(d): continue
            for root, _, files in os.walk(d):
                for f in files:
                    if f.endswith((".cpp", ".c", ".hpp", ".h", ".glsl", ".comp")):
                        p = os.path.join(root, f)
                        try:
                            t = os.path.getmtime(p)
                            if t > max_t: max_t = t
                        except OSError:
                            pass
        return max_t

    last_t = get_max_mtime()
    
    # Run once at startup
    ok, b_dt = build_targets()
    if ok:
        res, t_dt, _ = run_tests_parallel(selected_tests, fail_fast)
        print_test_summary(res, b_dt, t_dt)

    while True:
        time.sleep(0.5)
        cur_t = get_max_mtime()
        if cur_t > last_t:
            last_t = cur_t
            print(f"\n[!] File change detected at {time.strftime('%H:%M:%S')}. Recompiling & Testing...")
            ok, b_dt = build_targets()
            if ok:
                res, t_dt, _ = run_tests_parallel(selected_tests, fail_fast)
                print_test_summary(res, b_dt, t_dt)
            else:
                print("[x] Fix compilation errors to resume test run.")

def main():
    parser = argparse.ArgumentParser(description="Voidfall Dredge - Ultra-Fast TDD Runner")
    all_test_choices = list(TESTS.keys()) + ["all", "fast", "quick"]
    parser.add_argument("--test", choices=all_test_choices, default="all",
                        help="Specific test target to run ('fast' or 'quick' runs all sub-second tests)")
    parser.add_argument("--fast", "--quick", dest="is_fast", action="store_true",
                        help="Run all tests except heavy level generation collision tests for sub-second feedback")
    parser.add_argument("--no-build", action="store_true", help="Skip cmake build step")
    parser.add_argument("--fail-fast", action="store_true", help="Stop immediately on first test failure")
    parser.add_argument("--watch", action="store_true", help="Watch files for changes and re-run automatically")
    parser.add_argument("--visual", action="store_true", help="Run auto-play-test and print visual report")

    args = parser.parse_args()

    if args.is_fast or args.test in ("fast", "quick"):
        selected_tests = [k for k in TESTS.keys() if k != "collision"]
    elif args.test == "all":
        selected_tests = list(TESTS.keys())
    else:
        selected_tests = [args.test]

    if args.visual:
        print("[*] Building game executable...")
        ok, _ = build_targets("VoidfallDredge")
        if not ok: return 1
        print("[*] Running automated 7-phase visual playthrough...")
        exe = os.path.join(BUILD_DIR, CONFIG, f"VoidfallDredge{EXE_EXT}")
        run_cmd([exe, "--auto-play-test", "--mute"])
        # Run visual analyzer
        analyzer = os.path.join("scripts", "analyze_screenshots.py")
        subprocess.run([sys.executable, analyzer])
        return 0

    if args.watch:
        try:
            watch_loop(selected_tests, args.fail_fast)
        except KeyboardInterrupt:
            print("\n[*] Exiting TDD watcher.")
        return 0

    build_time = 0.0
    should_build = not args.no_build
    if args.no_build:
        is_stale, reason = check_sources_staleness(selected_tests)
        if is_stale:
            print(f"\n[!] STALENESS GUARD: Overriding --no-build ({reason})")
            print("    Automatically compiling fresh build to guarantee test fidelity...")
            should_build = True

    if should_build:
        target_to_build = TESTS[args.test]["target"] if args.test in TESTS else None
        ok, build_time = build_targets(target_to_build)
        if not ok:
            return 1

    results, test_time, all_passed = run_tests_parallel(selected_tests, args.fail_fast)
    print_test_summary(results, build_time, test_time)
    return 0 if all_passed else 1

if __name__ == "__main__":
    sys.exit(main())
