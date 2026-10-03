#!/usr/bin/env python3
"""
Voidfall Dredge - Level Design & Room Archetype Visual Showcase Capture Harness
Builds the engine, executes --capture-level-shapes, verifies and catalogs
up-to-date reference images of all 15 room archetypes + corridor bulkhead vault
in docs/level_design/shapes/ and generates docs/level_design/level_shapes_showcase.jpg.
"""

import sys
import os
import subprocess
import time
import json
import argparse

CONFIG = "Release"
BUILD_DIR = "build"
EXE_EXT = ".exe" if sys.platform == "win32" else ""
ENGINE_EXE = os.path.join(BUILD_DIR, CONFIG, f"VoidfallDredge{EXE_EXT}")

SHAPES_DIR = "docs/level_design/shapes"
DOCS_DIR = "docs/level_design"
REPORT_JSON = os.path.join(DOCS_DIR, "level_shapes_report.json")
REPORT_TXT = os.path.join(DOCS_DIR, "level_shapes_report.txt")
MONTAGE_PNG = os.path.join(DOCS_DIR, "level_shapes_showcase.png")
MONTAGE_JPG = os.path.join(DOCS_DIR, "level_shapes_showcase.jpg")

EXPECTED_SHAPES = [
    ("01_spawn_staging_cavern.png", "Arrival Bay", "Spawn Staging Cavern"),
    ("02_mining_pillar_hall.png", "Resource Cavern", "Mining Pillar Hall"),
    ("03_crystalline_geode.png", "Crystal Sphere", "Crystalline Geode"),
    ("04_terraced_quarry.png", "Excavation Pit", "Terraced Quarry"),
    ("05_industrial_vault_bunker.png", "Precursor Vault", "Industrial Vault Bunker"),
    ("06_fault_line_crevasse.png", "Magma Crevasse", "Fault Line Crevasse"),
    ("07_abyssal_vertical_chasm.png", "Vertical Abyss", "Abyssal Vertical Chasm"),
    ("08_radioactive_core_sanctuary.png", "Radiation Moat", "Radioactive Core Sanctuary"),
    ("09_extraction_landing_bay.png", "Beacon Pad", "Extraction Landing Bay"),
    ("10_magma_caldera_lake.png", "Lava Lake", "Magma Caldera Lake"),
    ("11_spike_trench_arena.png", "Spike Parkour", "Spike Trench Arena"),
    ("12_void_singularity_rift.png", "Cosmic Rift", "Void Singularity Rift"),
    ("13_fungoid_bio_grotto.png", "Spore Grotto", "Fungoid Bio Grotto"),
    ("14_laser_defense_foundry.png", "Smelting Flume", "Laser Defense Foundry"),
    ("15_crumbling_arch_canyon.png", "Natural Arch", "Crumbling Arch Canyon"),
    ("16_corridor_bulkhead_vault.png", "Bulkhead Seam", "Corridor Bulkhead Vault"),
]

def run_cmd(cmd, cwd=None, timeout=None):
    res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
    return res.returncode, res.stdout, res.stderr

def build_engine():
    print("[*] Building Voidfall Dredge engine for Level Shapes Showcase...")
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

def run_shapes_capture():
    print("\n" + "=" * 80)
    print("  EXECUTING LEVEL DESIGN & ROOM SHAPES SHOWCASE CAPTURE (--capture-level-shapes)")
    print("=" * 80)
    if not os.path.exists(ENGINE_EXE):
        print(f"[!] Engine executable not found: {ENGINE_EXE}")
        return False

    os.makedirs(SHAPES_DIR, exist_ok=True)

    print(f"[*] Launching {ENGINE_EXE} --capture-level-shapes...")
    t0 = time.time()
    try:
        code, out, err = run_cmd([ENGINE_EXE, "--capture-level-shapes"], timeout=45)
    except subprocess.TimeoutExpired:
        print("[!] Level shapes capture staging timed out after 45s!")
        return False

    dt = time.time() - t0
    print(f"[*] Shapes showcase capture finished in {dt:.2f}s (Exit code: {code}).")
    if code != 0:
        print("[!] Engine error during level shapes capture:")
        print(err)
        return False

    return True

def verify_and_report():
    print("\n" + "=" * 85)
    print("  VOIDFALL DREDGE - LEVEL DESIGN & ROOM ARCHETYPES VISUAL ASSET CATALOG")
    print("=" * 85)

    all_found = True
    print(f"  {'FILE':<38} {'CATEGORY':<20} {'SIZE':<10} {'STATUS':<8}")
    print("-" * 85)

    for filename, cat, label in EXPECTED_SHAPES:
        path = os.path.join(SHAPES_DIR, filename)
        if os.path.exists(path):
            sz_kb = os.path.getsize(path) / 1024.0
            print(f"  {filename:<38} {cat:<20} {sz_kb:>7.1f} KB  [+] PASS")
        else:
            print(f"  {filename:<38} {cat:<20} {'MISSING':>10}  [-] FAIL")
            all_found = False

    print("-" * 85)
    if os.path.exists(MONTAGE_PNG):
        sz_kb = os.path.getsize(MONTAGE_PNG) / 1024.0
        print(f"  {'level_shapes_showcase.png':<38} {'Master 4x4 Montage':<20} {sz_kb:>7.1f} KB  [+] PASS")
    if os.path.exists(MONTAGE_JPG):
        sz_kb = os.path.getsize(MONTAGE_JPG) / 1024.0
        print(f"  {'level_shapes_showcase.jpg':<38} {'Compact 4x4 Montage':<20} {sz_kb:>7.1f} KB  [+] PASS")

    if os.path.exists(REPORT_JSON):
        with open(REPORT_JSON, "r", encoding="utf-8") as f:
            data = json.load(f)
        all_healthy = data.get("all_healthy", False)
        print(f"\n  TELEMETRY AUDIT: {'PASS (ALL HEALTHY)' if all_healthy else 'FAIL'}")
        print(f"  SHAPES VERIFIED: {data.get('total_shapes', 0)} / 16")
        for m in data.get("shapes", []):
            print(f"    - Slot [{m['slot'] + 1:>2}] {m['shape_label']:<26}: Lum={m['mean_luminance']:.3f}, NonBlack={m['non_black_ratio']*100:.1f}% -> {m['status']}")
    else:
        print("[!] Visual report JSON missing!")
        all_found = False

    print("=" * 85)
    return all_found

def main():
    parser = argparse.ArgumentParser(description="Capture and verify 3D level shapes & room archetypes showcase.")
    parser.add_argument("--skip-build", action="store_true", help="Skip CMake compilation")
    args = parser.parse_args()

    if not args.skip_build:
        if not build_engine():
            sys.exit(1)

    if not run_shapes_capture():
        sys.exit(1)

    if not verify_and_report():
        sys.exit(1)

    print("\n[+] Level design & room shapes showcase successfully captured and validated!\n")

if __name__ == "__main__":
    main()
