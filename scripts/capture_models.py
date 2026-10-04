#!/usr/bin/env python3
"""
Voidfall Dredge - 3D Character & Enemy Model Showcase Capture Harness
Builds the engine, executes --capture-models, verifies and catalogs
up-to-date reference images of all characters, enemies, and weapons in docs/models/.
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

MODELS_DIR = "docs/models"
REPORT_JSON = os.path.join(MODELS_DIR, "models_visual_report.json")
REPORT_TXT = os.path.join(MODELS_DIR, "models_visual_report.txt")
MONTAGE_PNG = os.path.join(MODELS_DIR, "models_roster_showcase.png")
MONTAGE_JPG = os.path.join(MODELS_DIR, "models_roster_showcase.jpg")

EXPECTED_MODELS = [
    ("enemy_void_stalker.png", "Hostile Entity", "Void Stalker"),
    ("enemy_seismic_burrower.png", "Hostile Entity", "Seismic Burrower"),
    ("system_viewmodel_drill.png", "Primary Rig", "Mining Drill Rig"),
    ("character_demolitionist_kaelen.png", "Delver Contractor", "Kaelen (Demolitionist)"),
    ("character_vanguard_rhodes.png", "Delver Contractor", "Rhodes (Vanguard)"),
    ("character_scout_vesper.png", "Delver Contractor", "Vesper (Scout)"),
]

def run_cmd(cmd, cwd=None, timeout=None):
    res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
    return res.returncode, res.stdout, res.stderr

def build_engine():
    print("[*] Building Voidfall Dredge engine for Model Showcase...")
    t0 = time.time()
    cmd = ["cmake", "--build", BUILD_DIR, "--config", CONFIG, "--target", "VoidfallDredge"]
    code, out, err = run_cmd(cmd)
    dt = time.time() - t0
    if code != 0:
        print(f"[!] Build FAILED in {dt:.2f}s:")
        print(out)
        print(err)
        return False
    print(f"[+] Build succeeded in {dt:.2f}s.")
    return True

def run_model_capture():
    print("\n" + "=" * 75)
    print("  EXECUTING DEDICATED 3D MODEL SHOWCASE CAPTURE (--capture-models)")
    print("=" * 75)
    if not os.path.exists(ENGINE_EXE):
        print(f"[!] Engine executable not found: {ENGINE_EXE}")
        return False

    os.makedirs(MODELS_DIR, exist_ok=True)

    print(f"[*] Launching {ENGINE_EXE} --capture-models...")
    t0 = time.time()
    try:
        code, out, err = run_cmd([ENGINE_EXE, "--capture-models", "--mute"], timeout=30)
    except subprocess.TimeoutExpired:
        print("[!] Model capture staging timed out after 30s!")
        return False

    dt = time.time() - t0
    print(f"[*] Model showcase capture finished in {dt:.2f}s (Exit code: {code}).")
    if code != 0:
        print("[!] Engine error during model capture:")
        print(err)
        return False

    return True

def verify_and_report():
    print("\n" + "=" * 80)
    print("  VOIDFALL DREDGE - 3D ROSTER & MODEL VISUAL ASSET CATALOG")
    print("=" * 80)

    all_found = True
    print(f"  {'FILE':<38} {'CATEGORY':<20} {'SIZE':<10} {'STATUS':<8}")
    print("-" * 80)

    for filename, cat, label in EXPECTED_MODELS:
        path = os.path.join(MODELS_DIR, filename)
        if os.path.exists(path):
            sz_kb = os.path.getsize(path) / 1024.0
            print(f"  {filename:<38} {cat:<20} {sz_kb:>7.1f} KB  [+] PASS")
        else:
            print(f"  {filename:<38} {cat:<20} {'MISSING':>10}  [-] FAIL")
            all_found = False

    print("-" * 80)
    if os.path.exists(MONTAGE_PNG):
        sz_kb = os.path.getsize(MONTAGE_PNG) / 1024.0
        print(f"  {'models_roster_showcase.png':<38} {'Master Montage':<20} {sz_kb:>7.1f} KB  [+] PASS")
    if os.path.exists(MONTAGE_JPG):
        sz_kb = os.path.getsize(MONTAGE_JPG) / 1024.0
        print(f"  {'models_roster_showcase.jpg':<38} {'Lightweight Montage':<20} {sz_kb:>7.1f} KB  [+] PASS")

    if os.path.exists(REPORT_JSON):
        with open(REPORT_JSON, "r", encoding="utf-8") as f:
            data = json.load(f)
        all_healthy = data.get("all_healthy", False)
        print(f"\n  TELEMETRY AUDIT: {'PASS (ALL HEALTHY)' if all_healthy else 'FAIL'}")
        print(f"  MODELS VERIFIED: {data.get('total_models', 0)} / 6")
        for m in data.get("models", []):
            print(f"    - Slot [{m['slot'] + 1}] {m['model_label']:<24}: Lum={m['mean_luminance']:.3f}, NonBlack={m['non_black_ratio']*100:.1f}% -> {m['status']}")
    else:
        print("[!] Visual report JSON missing!")
        all_found = False

    print("=" * 80)
    return all_found

def main():
    parser = argparse.ArgumentParser(description="Capture & verify 3D character and enemy models.")
    parser.add_argument("--no-build", action="store_true", help="Skip CMake compilation")
    args = parser.parse_args()

    if not args.no_build:
        if not build_engine():
            sys.exit(1)

    if not run_model_capture():
        sys.exit(1)

    if not verify_and_report():
        sys.exit(1)

    print("\n[+] All character and enemy reference model images are up to date in docs/models/!")

if __name__ == "__main__":
    main()
