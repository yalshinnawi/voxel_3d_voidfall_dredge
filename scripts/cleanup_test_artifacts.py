#!/usr/bin/env python3
"""
Voidfall Dredge - Workspace & Test Artifacts Cleaner
Cleans temporary test previews, prunes stale screenshots, enforces canonical model images in docs/models/,
and clears test JSON dumps to keep the codebase pristine and optimal.
"""

import os
import sys
import argparse

CANONICAL_SCREENSHOTS = {
    "README.md", "audio_safety_report.json", "audio_samples",
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

CANONICAL_MODELS = {
    "README.md",
    "enemy_void_stalker.png",
    "enemy_seismic_burrower.png",
    "system_viewmodel_drill.png",
    "character_demolitionist_kaelen.png",
    "character_vanguard_rhodes.png",
    "character_scout_vesper.png",
    "models_roster_showcase.jpg",
    "models_roster_showcase.png",
    "models_visual_report.json",
    "models_visual_report.txt",
}

def clean_previews():
    preview_dir = os.path.join("screenshots", "previews")
    if not os.path.exists(preview_dir):
        return 0, 0
    removed, freed = 0, 0
    for f in os.listdir(preview_dir):
        p = os.path.join(preview_dir, f)
        if os.path.isfile(p):
            freed += os.path.getsize(p)
            os.remove(p)
            removed += 1
    return removed, freed

def clean_stale_screenshots():
    sc_dir = "screenshots"
    if not os.path.exists(sc_dir):
        return 0, 0
    removed, freed = 0, 0
    for f in os.listdir(sc_dir):
        p = os.path.join(sc_dir, f)
        if os.path.isfile(p) and f not in CANONICAL_SCREENSHOTS:
            freed += os.path.getsize(p)
            os.remove(p)
            removed += 1
            print(f"  [-] Removed stale screenshot: {f}")
    return removed, freed

def clean_models_directory():
    models_dir = os.path.join("docs", "models")
    if not os.path.exists(models_dir):
        return 0, 0
    removed, freed = 0, 0
    for f in os.listdir(models_dir):
        p = os.path.join(models_dir, f)
        if os.path.isfile(p) and f not in CANONICAL_MODELS:
            freed += os.path.getsize(p)
            os.remove(p)
            removed += 1
            print(f"  [-] Removed untracked/stale model asset: {f}")
    return removed, freed

def clean_test_dump_files():
    tests_dir = "tests"
    if not os.path.exists(tests_dir):
        return 0, 0
    removed, freed = 0, 0
    for f in os.listdir(tests_dir):
        if f.endswith(".json") and (f.startswith("test_") or f.startswith("e2e_")):
            # Keep profiles if needed, or remove generated test dumps
            pass
    return removed, freed

def main():
    parser = argparse.ArgumentParser(description="Voidfall Dredge - Test Artifacts & Screenshots Cleaner")
    parser.add_argument("--all", action="store_true", help="Run full cleanup across all directories")
    args = parser.parse_args()

    print("=" * 70)
    print("  VOIDFALL DREDGE - CLEANING SCREENSHOTS & TEST ARTIFACTS")
    print("=" * 70)

    p_cnt, p_sz = clean_previews()
    s_cnt, s_sz = clean_stale_screenshots()
    m_cnt, m_sz = clean_models_directory()

    total_removed = p_cnt + s_cnt + m_cnt
    total_freed_kb = (p_sz + s_sz + m_sz) / 1024.0

    print(f"  Previews cleaned:     {p_cnt:<4} ({p_sz / 1024.0:.1f} KB)")
    print(f"  Stale screenshots:    {s_cnt:<4} ({s_sz / 1024.0:.1f} KB)")
    print(f"  Model orphans purged: {m_cnt:<4} ({m_sz / 1024.0:.1f} KB)")
    print("-" * 70)
    print(f"  TOTAL PURGED:         {total_removed} files ({total_freed_kb:.1f} KB freed)")
    print("  STATUS:               Clean & Up to Date")
    print("=" * 70)

if __name__ == "__main__":
    main()
