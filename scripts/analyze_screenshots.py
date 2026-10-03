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
        
    return removed, freed_bytes

def main():
    parser = argparse.ArgumentParser(description="Voidfall Dredge - Visual Report & Preview Cleaner")
    parser.add_argument("--keep-previews", action="store_true",
                        help="Preserve preview files in screenshots/previews/ instead of auto-cleaning")
    parser.add_argument("--clean-only", action="store_true",
                        help="Clean preview files immediately without printing report")

    args = parser.parse_args()

    if args.clean_only:
        removed, freed = cleanup_previews_folder()
        print(f"[*] Cleaned up {removed} preview images ({freed / 1024:.1f} KB freed).")
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

    # Post-analysis cleanup of previews to optimize disk space
    if not args.keep_previews:
        removed, freed = cleanup_previews_folder()
        if removed > 0:
            print(f"  [+] Auto-cleaned {removed} preview images ({freed / 1024:.1f} KB freed) from screenshots/previews/ to save space.")
            print("      (Tip: Use --keep-previews if you wish to retain temporary preview files).")
    else:
        print("  [*] Retained preview images in screenshots/previews/ (--keep-previews active).")

    print("=" * 86)
    return 0 if (data.get("all_healthy") and len(errors_found) == 0) else 1

if __name__ == "__main__":
    sys.exit(main())
