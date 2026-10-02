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
    output_msg = out if code != 0 else ""
    return key, (code == 0), dt, output_msg

def run_tests_parallel(test_keys, fail_fast=False):
    t_start = time.time()
    results = {}

    with ThreadPoolExecutor(max_workers=min(len(test_keys), 4)) as executor:
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
    parser.add_argument("--test", choices=["unit", "progression", "e2e", "all"], default="all",
                        help="Specific test target to run")
    parser.add_argument("--no-build", action="store_true", help="Skip cmake build step")
    parser.add_argument("--fail-fast", action="store_true", help="Stop immediately on first test failure")
    parser.add_argument("--watch", action="store_true", help="Watch files for changes and re-run automatically")
    parser.add_argument("--visual", action="store_true", help="Run auto-play-test and print visual report")

    args = parser.parse_args()

    selected_tests = list(TESTS.keys()) if args.test == "all" else [args.test]

    if args.visual:
        print("[*] Building game executable...")
        ok, _ = build_targets("VoidfallDredge")
        if not ok: return 1
        print("[*] Running automated 7-phase visual playthrough...")
        exe = os.path.join(BUILD_DIR, CONFIG, f"VoidfallDredge{EXE_EXT}")
        run_cmd([exe, "--auto-play-test"])
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
    if not args.no_build:
        ok, build_time = build_targets()
        if not ok:
            return 1

    results, test_time, all_passed = run_tests_parallel(selected_tests, args.fail_fast)
    print_test_summary(results, build_time, test_time)
    return 0 if all_passed else 1

if __name__ == "__main__":
    sys.exit(main())
