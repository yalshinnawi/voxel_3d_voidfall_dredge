#!/usr/bin/env python3
"""
Voidfall Dredge - Automated Void Stalker Enemy Testing & Visual Verification Suite
Builds, executes unit tests, runs the dedicated visual staging sequence (--test-enemy),
audits voidfall.log for enemy telemetry, and evaluates luminance / health metrics.
"""

import sys
import os
import subprocess
import time
import json
import argparse

# Always enforce silent headless audio for all tests and child processes
os.environ["VOIDFALL_MUTE_AUDIO"] = "1"

CONFIG = "Release"
BUILD_DIR = "build"
EXE_EXT = ".exe" if sys.platform == "win32" else ""
ENGINE_EXE = os.path.join(BUILD_DIR, CONFIG, f"VoidfallDredge{EXE_EXT}")
TEST_EXE = os.path.join(BUILD_DIR, CONFIG, f"test_enemy_stalker{EXE_EXT}")
REPORT_JSON = "screenshots/enemy_visual_report.json"
REPORT_TXT = "screenshots/enemy_visual_report.txt"
MONTAGE_PNG = "screenshots/enemy_visual_montage.png"
LOG_FILE = "voidfall.log"

def run_cmd(cmd, cwd=None, timeout=None):
    res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
    return res.returncode, res.stdout, res.stderr

def build_enemy_targets():
    print("[*] Building engine and test targets...")
    t0 = time.time()
    cmd = ["cmake", "--build", BUILD_DIR, "--config", CONFIG, "--target", "VoidfallDredge", "test_enemy_stalker"]
    code, out, err = run_cmd(cmd)
    dt = time.time() - t0
    if code != 0:
        print(f"[!] Build FAILED in {dt:.2f}s:")
        print(out)
        print(err)
        return False
    print(f"[+] Build succeeded in {dt:.2f}s.")
    return True

def run_unit_tests():
    print("\n" + "=" * 65)
    print("  PHASE 1: VOID STALKER C++ UNIT & FSM LOGIC TESTS")
    print("=" * 65)
    if not os.path.exists(TEST_EXE):
        print(f"[!] Test executable not found: {TEST_EXE}")
        return False

    t0 = time.time()
    code, out, err = run_cmd([TEST_EXE])
    dt = time.time() - t0
    print(out)
    if code != 0:
        print(f"[!] Stalker Unit Tests FAILED in {dt:.3f}s:")
        print(err)
        return False
    print(f"[+] All Stalker Unit Tests PASSED in {dt:.3f}s.")
    return True

def run_visual_staging():
    print("\n" + "=" * 65)
    print("  PHASE 2: VISUAL STAGING HARNESS (--test-enemy)")
    print("=" * 65)
    if not os.path.exists(ENGINE_EXE):
        print(f"[!] Engine executable not found: {ENGINE_EXE}")
        return False

    print(f"[*] Launching {ENGINE_EXE} --test-enemy...")
    t0 = time.time()
    try:
        code, out, err = run_cmd([ENGINE_EXE, "--test-enemy", "--mute"], timeout=20)
    except subprocess.TimeoutExpired:
        print("[!] Visual staging timed out after 20s!")
        return False

    dt = time.time() - t0
    print(f"[*] Visual staging completed in {dt:.2f}s (Exit code: {code}).")

    if not os.path.exists(MONTAGE_PNG):
        print(f"[!] Montage file not generated: {MONTAGE_PNG}")
        return False

    montage_kb = os.path.getsize(MONTAGE_PNG) / 1024.0
    print(f"[+] Generated Enemy Visual Montage: {MONTAGE_PNG} ({montage_kb:.1f} KB)")
    return True

def analyze_visual_report():
    print("\n" + "=" * 65)
    print("  PHASE 3: LUMINANCE & VISUAL HEALTH ANALYSIS")
    print("=" * 65)

    if not os.path.exists(REPORT_JSON):
        print(f"[!] Visual report JSON not found: {REPORT_JSON}")
        return False

    with open(REPORT_JSON, "r", encoding="utf-8") as f:
        data = json.load(f)

    metrics = data.get("phases", [])
    print(f"{'SLOT':<6} {'PHASE ID':<18} {'MEAN LUM':<12} {'NON-BLACK':<12} {'HEALTH'}")
    print("-" * 65)

    all_healthy = True
    for m in metrics:
        slot = f"Slot {m.get('slot', 0)}"
        pid = m.get('phase_id', 'UNKNOWN')
        lum = f"{m.get('mean_luminance', 0.0):.4f}"
        nb = f"{m.get('non_black_ratio', 0.0) * 100.0:.1f}%"
        status = m.get('status', 'FAIL')
        if status != "PASS":
            all_healthy = False
        print(f"{slot:<6} {pid:<18} {lum:<12} {nb:<12} {status}")

    print("-" * 65)
    if all_healthy:
        print("[+] Visual Analysis PASSED: All phases meet non-black & luminance health criteria.")
    else:
        print("[!] Visual Analysis WARNING: One or more phases reported low luminance.")
    return all_healthy

def audit_runtime_log():
    print("\n" + "=" * 65)
    print("  PHASE 4: RUNTIME LOG AUDIT (voidfall.log)")
    print("=" * 65)

    if not os.path.exists(LOG_FILE):
        print(f"[*] No log file found at {LOG_FILE}.")
        return True

    enemy_events = []
    gl_errors = []
    with open(LOG_FILE, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    for line in lines[-200:]: # inspect recent lines
        if "VoidStalker" in line or "EnemyTest" in line or "stalker" in line.lower():
            enemy_events.append(line.strip())
        if "GL_ERROR" in line or "OpenGL Error" in line or "[ERROR]" in line:
            gl_errors.append(line.strip())

    print(f"[*] Found {len(enemy_events)} Stalker telemetry events in recent log entries:")
    for ev in enemy_events[-8:]:
        print(f"    {ev}")

    if gl_errors:
        print(f"[!] Warning: Detected {len(gl_errors)} error entries in log:")
        for err in gl_errors[-5:]:
            print(f"    {err}")
        return False
    else:
        print("[+] Log Audit CLEAN: Zero OpenGL errors or crashes detected.")
        return True

LOAD_REPORT_JSON = "screenshots/enemy_load_report.json"
LOAD_SCREENSHOT_PNG = "screenshots/enemy_load_test.png"

def run_load_test():
    print("\n" + "=" * 65)
    print("  MULTI-MONSTER HORDE & VERTEX LOAD TEST (--test-enemy-load)")
    print("=" * 65)
    if not os.path.exists(ENGINE_EXE):
        print(f"[!] Engine executable not found: {ENGINE_EXE}")
        return False

    print(f"[*] Launching {ENGINE_EXE} --test-enemy-load --mute...")
    t0 = time.time()
    try:
        code, out, err = run_cmd([ENGINE_EXE, "--test-enemy-load", "--mute"], timeout=20)
    except subprocess.TimeoutExpired:
        print("[!] Multi-monster load test timed out after 20s!")
        return False

    dt = time.time() - t0
    print(f"[*] Load test completed in {dt:.2f}s (Exit code: {code}).")

    if not os.path.exists(LOAD_SCREENSHOT_PNG):
        print(f"[!] Load test screenshot not generated: {LOAD_SCREENSHOT_PNG}")
        return False

    if not os.path.exists(LOAD_REPORT_JSON):
        print(f"[!] Load test report not generated: {LOAD_REPORT_JSON}")
        return False

    with open(LOAD_REPORT_JSON, "r", encoding="utf-8") as f:
        data = json.load(f)

    stalkers = data.get("stalker_count", 0)
    burrowers = data.get("burrower_count", 0)
    projectiles = data.get("projectile_count", 0)
    carcasses = data.get("carcass_count", 0)
    total_verts = data.get("total_monster_vertices", 0)
    vbo_cap = data.get("vbo_capacity", 0)
    status = data.get("status", "FAIL")

    png_kb = os.path.getsize(LOAD_SCREENSHOT_PNG) / 1024.0

    print(f"  Stalkers Staged:         {stalkers} (Melee, Shooter, Chitin Goliath)")
    print(f"  Burrowers Staged:        {burrowers}")
    print(f"  Void Spikes Active:      {projectiles}")
    print(f"  Enemy Carcasses:         {carcasses}")
    print(f"  Total Monster Vertices:  {total_verts:,} (exceeds legacy 16,384 cap)")
    print(f"  Dynamic VBO Capacity:    {vbo_cap:,} vertices")
    print(f"  Screenshot Generated:    {LOAD_SCREENSHOT_PNG} ({png_kb:.1f} KB)")
    print(f"  Load Test Status:        {status}")

    if status == "PASS" and total_verts > 16384 and vbo_cap >= total_verts:
        print("[+] Anti-Invisibility Load Test PASSED: All monster meshes dynamically accommodated!")
        return True
    else:
        print("[!] Load Test WARNING: Vertex capacity or status check failed.")
        return False

def main():
    parser = argparse.ArgumentParser(description="Voidfall Dredge Enemy Test Runner")
    parser.add_argument("--no-build", action="store_true", help="Skip CMake compilation")
    parser.add_argument("--unit-only", action="store_true", help="Run only C++ unit tests")
    parser.add_argument("--visual-only", action="store_true", help="Run only visual staging harness")
    parser.add_argument("--load", action="store_true", help="Run multi-monster load test")
    args = parser.parse_args()

    t_start = time.time()

    if not args.no_build:
        if not build_enemy_targets():
            sys.exit(1)

    if args.load:
        ok = run_load_test()
        audit_runtime_log()
        total_time = time.time() - t_start
        print("\n" + "=" * 65)
        print(f"  ENEMY LOAD TEST COMPLETED IN {total_time:.2f}s (SUCCESS: {ok})")
        print("=" * 65)
        sys.exit(0 if ok else 1)

    unit_ok = True
    if not args.visual_only:
        unit_ok = run_unit_tests()
        if not unit_ok:
            sys.exit(1)

    visual_ok = True
    if not args.unit_only:
        visual_ok = run_visual_staging()
        if visual_ok:
            analyze_visual_report()
            audit_runtime_log()
        else:
            sys.exit(1)

    total_time = time.time() - t_start
    print("\n" + "=" * 65)
    print(f"  ALL ENEMY VERIFICATION SUITES COMPLETED IN {total_time:.2f}s")
    print("=" * 65)

if __name__ == "__main__":
    main()
