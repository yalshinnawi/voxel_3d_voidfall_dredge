#!/usr/bin/env python3
"""
Voidfall Dredge - Screenshot & Visual Diagnostics Fast Analyzer
Reads screenshots/visual_report.json, prints visual health metrics with time/date stamps,
and automatically cleans up temporary preview screenshots to save disk space.
"""

import sys
import os
import json
import argparse

def cleanup_previews_folder():
    preview_dir = os.path.join("screenshots", "previews")
    if not os.path.exists(preview_dir):
        return 0, 0
    
    removed = 0
    freed_bytes = 0
    try:
        for f in os.listdir(preview_dir):
            p = os.path.join(preview_dir, f)
            if os.path.isfile(p):
                freed_bytes += os.path.getsize(p)
                os.remove(p)
                removed += 1
    except OSError as e:
        print(f"[!] Warning cleaning preview directory: {e}")
        
CANONICAL_SCREENSHOT_FILES = {
    "README.md", "audio_safety_report.json",
    "00_all_phases_montage.jpg", "00_all_phases_montage.png",
    "01_main_menu.png", "02_level_select.png", "03_character_select.png",
    "04_upgrades_terminal.png", "05_gameplay_cavern.png", "06_drilling_cracks.png",
    "07_abilities_loot.png", "08_esc_menu.png", "09_extraction_beacon.png",
    "10_mission_debrief.png", "visual_report.json", "visual_report.txt",
    "enemy_visual_montage.jpg", "enemy_visual_montage.png",
    "enemy_01_floor_crawl.png", "enemy_02_wall_climb.png", "enemy_03_ceiling_crawl.png",
    "enemy_04_surface_transition.png", "enemy_05_aggressive_lunge.png", "enemy_06_sonar_stun.png",
    "enemy_visual_report.json", "enemy_visual_report.txt"
}

def cleanup_stale_screenshots():
    sc_dir = "screenshots"
    if not os.path.exists(sc_dir):
        return 0, 0
    removed = 0
    freed_bytes = 0
    try:
        for f in os.listdir(sc_dir):
            p = os.path.join(sc_dir, f)
            if os.path.isfile(p) and f not in CANONICAL_SCREENSHOT_FILES:
                freed_bytes += os.path.getsize(p)
                os.remove(p)
                removed += 1
    except OSError as e:
        print(f"[!] Warning cleaning stale screenshots: {e}")
    return removed, freed_bytes

def main():
    parser = argparse.ArgumentParser(description="Voidfall Dredge - Visual Report & Preview Cleaner")
    parser.add_argument("--keep-previews", action="store_true",
                        help="Preserve preview files in screenshots/previews/ instead of auto-cleaning")
    parser.add_argument("--clean-only", action="store_true",
                        help="Clean preview files and stale screenshots immediately without printing report")
    parser.add_argument("--clean-stale", action="store_true",
                        help="Prune any non-canonical or stale screenshots")

    args = parser.parse_args()

    if args.clean_only or args.clean_stale:
        p_removed, p_freed = cleanup_previews_folder()
        s_removed, s_freed = cleanup_stale_screenshots()
        total_freed = (p_freed + s_freed) / 1024.0
        print(f"[*] Cleaned up {p_removed} previews and {s_removed} stale screenshots ({total_freed:.1f} KB freed).")
        if args.clean_only:
            return 0

    report_json_path = os.path.join("screenshots", "visual_report.json")
    report_txt_path = os.path.join("screenshots", "visual_report.txt")

    if not os.path.exists(report_json_path):
        if os.path.exists(report_txt_path):
            with open(report_txt_path, "r", encoding="utf-8") as f:
                print(f.read())
            return 0
        print(f"[ERROR] No visual test report found at {report_json_path}.")
        print("Run: ./build/Release/VoidfallDredge.exe --auto-play-test first.")
        return 1

    try:
        with open(report_json_path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except Exception as e:
        print(f"[ERROR] Failed to parse {report_json_path}: {e}")
        return 1

    print("=" * 86)
    print("  VOIDFALL DREDGE - FAST VISUAL DIAGNOSTICS & SCREENSHOT ANALYZER")
    print("=" * 86)
    print(f"  Overall Status:  {'PASS (ALL HEALTHY)' if data.get('all_healthy') else 'FAIL (ANOMALIES DETECTED)'}")
    print(f"  Total Phases:    {data.get('total_phases', 0)}")
    print(f"  Master Montage:  {data.get('montage_png', 'N/A')}")
    print("-" * 86)
    print(f"  {'SLOT':<6} {'PHASE':<19} {'TIMESTAMP':<21} {'LUM':<8} {'NON-BLACK':<11} {'STATUS':<8}")
    print("-" * 86)

    for p in data.get("phases", []):
        slot_str = f"[{p.get('slot', 0) + 1}]"
        phase_id = p.get("phase_id", "Unknown")
        timestamp = p.get("timestamp", "N/A")
        lum = f"{p.get('mean_luminance', 0.0):.3f}"
        nb = f"{p.get('non_black_ratio', 0.0) * 100.0:.1f}%"
        status = p.get("status", "UNKNOWN")
        print(f"  {slot_str:<6} {phase_id:<19} {timestamp:<21} {lum:<8} {nb:<11} {status:<8}")

    # Diagnostic Error Log Audit (voidfall.log)
    log_file = "voidfall.log"
    errors_found = []
    warnings_found = []
    if os.path.exists(log_file):
        try:
            with open(log_file, "r", encoding="utf-8", errors="replace") as lf:
                for line_idx, line in enumerate(lf, 1):
                    line_str = line.strip()
                    if "[ERROR]" in line_str or "[FATAL]" in line_str or "OpenGL Error" in line_str:
                        errors_found.append((line_idx, line_str))
                    elif "[WARN]" in line_str and "Pixel transfer is synchronized" not in line_str:
                        warnings_found.append((line_idx, line_str))
        except Exception as e:
            print(f"[!] Warning reading log file {log_file}: {e}")

    print("  DIAGNOSTIC ERROR LOG AUDIT (voidfall.log):")
    if not errors_found and not warnings_found:
        print("  [+] Clean Execution: Zero (0) Errors or Invariant Warnings Detected in voidfall.log")
    else:
        if errors_found:
            print(f"  [!] {len(errors_found)} ERROR(S) DETECTED IN RUNTIME LOG:")
            for lno, ltxt in errors_found[:10]:
                print(f"      Line {lno}: {ltxt}")
        if warnings_found:
            print(f"  [*] {len(warnings_found)} Warning(s) Detected in Runtime Log:")
            for lno, ltxt in warnings_found[:5]:
                print(f"      Line {lno}: {ltxt}")

    print("=" * 86)

    # Post-analysis cleanup of previews and stale test artifacts to optimize disk space
    if not args.keep_previews:
        removed, freed = cleanup_previews_folder()
        stale_cnt, stale_freed = cleanup_stale_screenshots()
        total_freed = freed + stale_freed
        if (removed + stale_cnt) > 0:
            print(f"  [+] Auto-cleaned {removed} preview images and {stale_cnt} stale files ({total_freed / 1024:.1f} KB freed) to keep screenshots pristine.")
            print("      (Tip: Use --keep-previews if you wish to retain temporary preview files).")
    else:
        print("  [*] Retained preview images in screenshots/previews/ (--keep-previews active).")

    print("=" * 86)
    return 0 if (data.get("all_healthy") and len(errors_found) == 0) else 1

if __name__ == "__main__":
    sys.exit(main())
