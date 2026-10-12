#!/usr/bin/env python3
"""
Voidfall Dredge - Clear / Reset Save Data Tool
Usage:
    python scripts/clear_save.py              # Resets default save (saves/save_data.json)
    python scripts/clear_save.py --test       # Resets test save (saves/test_save_data.json)
    python scripts/clear_save.py --all        # Resets both default & test saves
    python scripts/clear_save.py --delete     # Deletes save files instead of rewriting fresh template
"""

import os
import sys
import shutil
import argparse
from datetime import datetime

DEFAULT_SAVE = os.path.join("saves", "save_data.json")
TEST_SAVE = os.path.join("saves", "test_save_data.json")
LEGACY_ROOT_SAVE = "save_data.json"

FRESH_PROFILE_JSON = """{
  "save_version": 2,
  "last_saved_time": "%(timestamp)s",
  "player_name": "DELVER-01",
  "total_exp": 0,
  "lifetime_exp": 0,
  "total_coins": 200,
  "total_voidite": 12,
  "total_titanium": 8,
  "selected_class_id": 0,
  "selected_class": 0,
  "drill_speed_tier": 0,
  "drill_durability_tier": 0,
  "thruster_tank_tier": 0,
  "kinetic_dynamo_tier": 0,
  "sonar_frequency_tier": 0,
  "reinforced_plating_tier": 0,
  "highest_cleared_sector": 1,
  "master_volume": 1.0,
  "sfx_volume": 1.0,
  "enemy_volume": 1.0,
  "ambient_volume": 1.0,
  "ui_volume": 1.0,
  "mute_all": false,
  "mouse_sensitivity": 0.12,
  "fov": 75,
  "brightness": 1.0,
  "screen_shake": 0.2
}
"""

def clear_file(filepath: str, delete_only: bool = False, make_backup: bool = True):
    if not os.path.exists(filepath):
        if delete_only:
            print(f"  [i] {filepath}: File does not exist (already clear).")
            return
        os.makedirs(os.path.dirname(filepath) or ".", exist_ok=True)
    else:
        if make_backup:
            bak_path = filepath + ".bak"
            shutil.copy2(filepath, bak_path)
            print(f"  [*] Backup saved: {bak_path}")

        if delete_only:
            os.remove(filepath)
            print(f"  [-] Removed save file: {filepath}")
            return

    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    content = FRESH_PROFILE_JSON % {"timestamp": timestamp}
    with open(filepath, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"  [+] Reset {filepath} -> Fresh Delver profile (Level 1, 0 EXP, 0 Upgrades, Sector 1 unlocked).")

def main():
    parser = argparse.ArgumentParser(description="Clear and reset Voidfall Dredge save files.")
    parser.add_argument("--test", action="store_true", help="Clear only the test save file (saves/test_save_data.json)")
    parser.add_argument("--all", action="store_true", help="Clear both default and test save files")
    parser.add_argument("--file", type=str, help="Target a specific save file path")
    parser.add_argument("--delete", action="store_true", help="Delete save file completely instead of writing fresh template")
    parser.add_argument("--no-backup", action="store_true", help="Skip creating a .bak copy before clearing")
    args = parser.parse_args()

    print("=======================================================")
    print("  VOIDFALL DREDGE - SAVE CLEAR & RESET UTILITY")
    print("=======================================================")

    targets = []
    if args.file:
        targets.append(args.file)
    elif args.all:
        targets.extend([DEFAULT_SAVE, TEST_SAVE])
        if os.path.exists(LEGACY_ROOT_SAVE):
            targets.append(LEGACY_ROOT_SAVE)
    elif args.test:
        targets.append(TEST_SAVE)
    else:
        targets.append(DEFAULT_SAVE)
        if os.path.exists(LEGACY_ROOT_SAVE):
            targets.append(LEGACY_ROOT_SAVE)

    for target in targets:
        clear_file(target, delete_only=args.delete, make_backup=not args.no_backup)

    print("-------------------------------------------------------")
    print("  Save clearance complete. You can now test with pristine state.")
    print("=======================================================\n")

if __name__ == "__main__":
    main()
