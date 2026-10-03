#!/usr/bin/env python3
"""
Voidfall Dredge - Release Packaging Utility
Bundles the executable, assets, launch scripts, and player guide into a standalone,
distribution-ready folder and ZIP archive ready to share with friends.
"""

import os
import sys
import shutil
import zipfile
import subprocess
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT_DIR / "build" / "Release"
EXE_PATH = BUILD_DIR / "VoidfallDredge.exe"
ASSETS_DIR = ROOT_DIR / "assets"
DIST_DIR = ROOT_DIR / "dist"
PACKAGE_DIR = DIST_DIR / "VoidfallDredge"
ZIP_OUTPUT = DIST_DIR / "VoidfallDredge_v1.0.zip"

HOW_TO_PLAY_TEXT = """================================================================================
                           VOIDFALL: DREDGE
                3D Extraction-Survival Voxel Engine
================================================================================

QUICK START:
  1. Simply double-click "VoidfallDredge.exe" (or "Play_Voidfall_Dredge.bat") to play!
  2. No installation required. All game assets and saves are self-contained.

SYSTEM REQUIREMENTS:
  - OS: Windows 10 / 11 (64-bit)
  - Graphics: OpenGL 3.3 compatible GPU (NVIDIA, AMD, or Intel HD Graphics)
  - Microsoft Visual C++ 2015-2022 Redistributable (x64)
    * If you get an error about "MSVCP140.dll" or "VCRUNTIME140.dll missing",
      download and install the official runtime here:
      https://aka.ms/vs/17/release/vc_redist.x64.exe

CONTROLS:
  - W, A, S, D       : Move Delver
  - Mouse            : Look around
  - Spacebar         : Jump / Climb
  - Left Shift       : Sprint (consumes Delver stamina)
  - Left Click       : Mine voxels / Attack with pickaxe
  - Right Click      : Skill / Secondary action (Seismic pulse / Sonar)
  - E                : Interact (Evac Beacon, Terminals, Drops)
  - F                : Toggle Helmet Flashlight
  - Tab              : Delver Skill Matrix & Upgrades overview
  - Escape           : Pause Menu / Settings / Return to Orbital Hub

GAMEPLAY LOOP:
  1. Orbital Hub: Select your Delver Class (Scout, Demolitionist, Engineer, Bulwark).
  2. Expedition: Drop into the asteroid cavern to mine Voidite and Titanium voxels.
  3. Hazard Clock: The cavern escalates through 4 hazard phases (Phase I to Critical).
     Tremors, ceiling collapses, and hostile Void Stalkers will increase over time.
  4. Extraction: Locate the Evac Beacon, trigger the transmission, survive the holdout,
     and extract before the sector caves in!
  5. Progression: Bank your resources in the Orbital Hub to unlock permanent stat perks!

MULTIPLAYER CO-OP:
  - To Host:
      Launch "Play_Voidfall_Dredge.bat" (runs in Host mode on port 27015).
      Ensure your port 27015 (UDP/TCP) is forwarded or you are using LAN / Radmin / Hamachi.
  - To Join a Friend's Game:
      Right-click "Join_Multiplayer_Game.bat", edit your friend's IP address, and save.
      Or run in command prompt / terminal:
      VoidfallDredge.exe --client <Friend_IP_Address> --port 27015

SAVES:
  - Your progression and character stats are saved to the "saves/" folder in this directory.
================================================================================
"""

LAUNCH_BAT = """@echo off
title Voidfall Dredge
start "" "%~dp0VoidfallDredge.exe"
"""

JOIN_BAT = """@echo off
title Voidfall Dredge - Join Multiplayer
set /p SERVER_IP="Enter Host IP address (default 127.0.0.1): "
if "%SERVER_IP%"=="" set SERVER_IP=127.0.0.1
start "" "%~dp0VoidfallDredge.exe" --client %SERVER_IP% --port 27015
"""

def main():
    print("=" * 64)
    print("  VOIDFALL DREDGE - RELEASE PACKAGER")
    print("=" * 64)

    # 1. Check / Build executable
    if not EXE_PATH.exists():
        print("[!] Executable not found at:", EXE_PATH)
        print("[*] Invoking CMake to build Release target...")
        subprocess.check_call(["cmake", "--build", "build", "--config", "Release", "--target", "VoidfallDredge"], cwd=ROOT_DIR)

    if not EXE_PATH.exists():
        print("[x] Error: Build failed or executable still missing.")
        sys.exit(1)

    # 2. Prepare destination directory
    print(f"[*] Cleaning previous distribution directory: {DIST_DIR}")
    if PACKAGE_DIR.exists():
        shutil.rmtree(PACKAGE_DIR)
    PACKAGE_DIR.mkdir(parents=True, exist_ok=True)

    # 3. Copy Executable
    print(f"[*] Copying VoidfallDredge.exe...")
    shutil.copy2(EXE_PATH, PACKAGE_DIR / "VoidfallDredge.exe")

    # 4. Copy Assets
    print(f"[*] Copying assets (shaders, sounds, textures)...")
    shutil.copytree(ASSETS_DIR, PACKAGE_DIR / "assets", dirs_exist_ok=True)

    # 5. Write Guides and Launchers
    print(f"[*] Writing HOW_TO_PLAY.txt...")
    (PACKAGE_DIR / "HOW_TO_PLAY.txt").write_text(HOW_TO_PLAY_TEXT, encoding="utf-8")

    print(f"[*] Writing helper batch launchers...")
    (PACKAGE_DIR / "Play_Voidfall_Dredge.bat").write_text(LAUNCH_BAT, encoding="utf-8")
    (PACKAGE_DIR / "Join_Multiplayer_Game.bat").write_text(JOIN_BAT, encoding="utf-8")

    # 6. Create ZIP archive
    print(f"[*] Compressing into ZIP archive: {ZIP_OUTPUT.name}...")
    if ZIP_OUTPUT.exists():
        ZIP_OUTPUT.unlink()

    with zipfile.ZipFile(ZIP_OUTPUT, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for root, dirs, files in os.walk(PACKAGE_DIR):
            for file in files:
                abs_file = Path(root) / file
                rel_file = abs_file.relative_to(DIST_DIR)
                zf.write(abs_file, rel_file)

    zip_size_mb = ZIP_OUTPUT.stat().st_size / (1024 * 1024)
    print("\n" + "=" * 64)
    print("  PACKAGE SUCCESSFUL!")
    print("=" * 64)
    print(f"  Standalone Folder: {PACKAGE_DIR}")
    print(f"  Sharable ZIP Archive: {ZIP_OUTPUT} ({zip_size_mb:.2f} MB)")
    print("=" * 64)
    print("You can send 'VoidfallDredge_v1.0.zip' directly to your friend!")

if __name__ == "__main__":
    main()
