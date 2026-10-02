#!/usr/bin/env python3
"""
Voidfall Dredge - Screenshot & Visual Diagnostics Fast Analyzer
Reads screenshots/visual_report.json and prints high-level visual health metrics.
"""

import sys
import os
import json

def main():
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

    print("=" * 72)
    print("  VOIDFALL DREDGE - FAST VISUAL DIAGNOSTICS ANALYZER")
    print("=" * 72)
    print(f"  Overall Status:  {'PASS (ALL HEALTHY)' if data.get('all_healthy') else 'FAIL (ANOMALIES DETECTED)'}")
    print(f"  Total Phases:    {data.get('total_phases', 0)}")
    print(f"  Master Montage:  {data.get('montage_png', 'N/A')}")
    print("-" * 72)
    print(f"  {'SLOT':<6} {'PHASE':<20} {'LUMINANCE':<12} {'NON-BLACK':<12} {'STATUS':<10}")
    print("-" * 72)

    for p in data.get("phases", []):
        slot_str = f"[{p.get('slot', 0) + 1}]"
        phase_id = p.get("phase_id", "Unknown")
        lum = f"{p.get('mean_luminance', 0.0):.3f}"
        nb = f"{p.get('non_black_ratio', 0.0) * 100.0:.1f}%"
        status = p.get("status", "UNKNOWN")
        print(f"  {slot_str:<6} {phase_id:<20} {lum:<12} {nb:<12} {status:<10}")

    print("=" * 72)
    print("  Fast Inspection Tip: View 'screenshots/00_all_phases_montage.jpg' in IDE")
    print("  to inspect all 7 phases in a single instant glance.")
    print("=" * 72)
    return 0 if data.get("all_healthy") else 1

if __name__ == "__main__":
    sys.exit(main())
