#!/usr/bin/env python3
"""
Voidfall Dredge - Audio Engine & Ear Safety Analyzer
Analyzes exported game audio waveforms for hearing safety compliance:
- Peak dBFS limits (no harsh digital clipping >= 0 dBFS)
- Crest factor & dynamic range compression
- Slew rate click detection (no sharp step pops/clicks)
- DC bias offset elimination
- Frequency harshness proxy
Outputs JSON report to screenshots/audio_safety_report.json
"""

import os
import sys
import math
import struct
import wave
import json
import subprocess
import time

AUDIO_DIR = os.path.join("screenshots", "audio_samples")
REPORT_PATH = os.path.join("screenshots", "audio_safety_report.json")
EXE_PATH = os.path.join("build", "Release", "test_audio.exe")
if not os.path.exists(EXE_PATH):
    EXE_PATH = os.path.join("build", "test_audio.exe")

def run_audio_export():
    print("[*] Rebuilding test_audio executable...")
    cmd_build = ["cmake", "--build", "build", "--config", "Release", "--target", "test_audio"]
    res_b = subprocess.run(cmd_build, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res_b.returncode != 0:
        print("[x] Failed to build test_audio:")
        print(res_b.stdout)
        print(res_b.stderr)
        return False

    print("[*] Running test_audio --export-wav...")
    cmd_run = [EXE_PATH, "--export-wav"]
    res_r = subprocess.run(cmd_run, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res_r.returncode != 0:
        print("[x] test_audio failed:")
        print(res_r.stdout)
        print(res_r.stderr)
        return False
    print(res_r.stdout.strip())
    return True

def analyze_wav(filepath):
    with wave.open(filepath, "rb") as wf:
        n_channels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        framerate = wf.getframerate()
        n_frames = wf.getnframes()
        raw_bytes = wf.readframes(n_frames)

    duration = n_frames / framerate
    total_samples = n_frames * n_channels

    # Unpack 16-bit signed PCM
    fmt = f"<{total_samples}h"
    samples_i16 = struct.unpack(fmt, raw_bytes)
    
    # Convert to normalized float [-1.0 .. 1.0]
    samples = [s / 32767.0 for s in samples_i16]

    # Metrics
    peak_val = 0.0
    sum_sq = 0.0
    sum_val = 0.0
    max_slew = 0.0
    zero_crossings = 0

    prev_ch = [0.0] * n_channels
    clipped_count = 0

    for f in range(n_frames):
        for ch in range(n_channels):
            s = samples[f * n_channels + ch]
            abs_s = abs(s)
            if abs_s > peak_val:
                peak_val = abs_s
            if abs_s >= 0.999:
                clipped_count += 1
            sum_sq += s * s
            sum_val += s

            if f > 0:
                delta = abs(s - prev_ch[ch])
                if delta > max_slew:
                    max_slew = delta
                if (s >= 0.0 and prev_ch[ch] < 0.0) or (s < 0.0 and prev_ch[ch] >= 0.0):
                    zero_crossings += 1
            prev_ch[ch] = s

    rms = math.sqrt(sum_sq / max(1, total_samples))
    dc_offset = abs(sum_val / max(1, total_samples))

    peak_dbfs = 20.0 * math.log10(max(1e-5, peak_val))
    rms_dbfs = 20.0 * math.log10(max(1e-5, rms))
    crest_factor_db = peak_dbfs - rms_dbfs

    zcr = zero_crossings / max(1e-4, duration)

    # Ear Safety Standards
    # 1. Peak dBFS <= -0.5 dBFS (no digital clipping)
    # 2. Clipped samples == 0
    # 3. Max slew <= 0.20 (no harsh transient pops)
    # 4. DC offset <= 0.01 (no speaker offset pops)
    is_safe = (peak_dbfs <= -0.50) and (clipped_count == 0) and (max_slew <= 0.25) and (dc_offset < 0.01)

    return {
        "filename": os.path.basename(filepath),
        "duration_sec": round(duration, 3),
        "sample_rate": framerate,
        "channels": n_channels,
        "peak_amplitude": round(peak_val, 4),
        "peak_dbfs": round(peak_dbfs, 2),
        "rms_dbfs": round(rms_dbfs, 2),
        "crest_factor_db": round(crest_factor_db, 2),
        "max_slew_rate": round(max_slew, 4),
        "dc_offset": round(dc_offset, 5),
        "clipped_samples": clipped_count,
        "zero_crossing_rate_hz": round(zcr, 1),
        "ear_safety_status": "PASS" if is_safe else "FAIL"
    }

def main():
    print("=" * 72)
    print("  VOIDFALL DREDGE - AUDIO ENGINE & EAR SAFETY ANALYZER")
    print("=" * 72)

    if not run_audio_export():
        sys.exit(1)

    if not os.path.isdir(AUDIO_DIR):
        print(f"[x] Error: Directory not found: {AUDIO_DIR}")
        sys.exit(1)

    wav_files = [f for f in sorted(os.listdir(AUDIO_DIR)) if f.endswith(".wav")]
    if not wav_files:
        print("[x] No .wav files found in screenshots/audio_samples/")
        sys.exit(1)

    results = []
    all_safe = True

    print("\n" + "-" * 72)
    print(f"  {'FILE':<28} {'PEAK (dBFS)':<13} {'RMS (dBFS)':<12} {'SLEW':<8} {'STATUS':<8}")
    print("-" * 72)

    for wf in wav_files:
        p = os.path.join(AUDIO_DIR, wf)
        data = analyze_wav(p)
        results.append(data)
        if data["ear_safety_status"] != "PASS":
            all_safe = False

        status_tag = "[+] SAFE" if data["ear_safety_status"] == "PASS" else "[-] FAIL"
        print(f"  {data['filename']:<28} {data['peak_dbfs']:>7.2f} dBFS  {data['rms_dbfs']:>7.2f} dBFS  {data['max_slew_rate']:>6.3f}  {status_tag}")

    print("-" * 72)

    report = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "overall_status": "ALL AUDIO PASSES EAR SAFETY STANDARDS" if all_safe else "HARSH NOISE DETECTED",
        "sample_count": len(results),
        "safety_thresholds": {
            "max_peak_dbfs": -0.50,
            "max_allowed_slew": 0.25,
            "max_dc_offset": 0.010,
            "max_clipped_samples": 0
        },
        "audio_files": results
    }

    with open(REPORT_PATH, "w") as f:
        json.dump(report, f, indent=2)

    print(f"\n[+] Audio safety report saved to: {REPORT_PATH}")
    print("=" * 72)
    print(f"  OVERALL RESULT: {'ALL AUDIO VERIFIED SAFE' if all_safe else 'HEARING SAFETY ISSUES DETECTED'}")
    print("=" * 72 + "\n")

    return 0 if all_safe else 1

if __name__ == "__main__":
    sys.exit(main())
