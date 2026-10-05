#!/usr/bin/env python3
"""
Generate and refine sector arrival stingers and audio files with smooth,
musical fade-in and fade-out curves, completely eliminating abrupt cutoffs.
"""

import os
import io
import math
import wave
import struct
import shutil
import urllib.request

SOUNDS_DIR = os.path.join("assets", "sounds")
BUILD_DIR = os.path.join("build", "Release", "assets", "sounds")
DIST_DIR = os.path.join("dist", "VoidfallDredge", "assets", "sounds")

def fetch_or_load_source():
    src_path = os.path.join("screenshots", "audio_samples", "horror_stinger_source.wav")
    if os.path.exists(src_path):
        with wave.open(src_path, "rb") as w:
            rate = w.getframerate()
            nframes = w.getnframes()
            raw = w.readframes(nframes)
            samples = struct.unpack(f"<{len(raw)//2}h", raw)
            left = [samples[i*2] / 32768.0 for i in range(nframes)]
            right = [samples[i*2+1] / 32768.0 for i in range(nframes)]
            return left, right, rate

    url = "https://sfxmint.com/dl/horror-stinger-30.wav"
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=8) as r:
        data = r.read()
    
    os.makedirs(os.path.dirname(src_path), exist_ok=True)
    with open(src_path, "wb") as f:
        f.write(data)

    w = wave.open(io.BytesIO(data), "rb")
    rate = w.getframerate()
    nframes = w.getnframes()
    raw = w.readframes(nframes)
    samples = struct.unpack(f"<{len(raw)//2}h", raw)
    left = [samples[i*2] / 32768.0 for i in range(nframes)]
    right = [samples[i*2+1] / 32768.0 for i in range(nframes)]
    return left, right, rate

def resample_linear(ch, factor):
    if abs(factor - 1.0) < 0.001:
        return list(ch)
    target_len = int(len(ch) / factor)
    res = [0.0] * target_len
    for i in range(target_len):
        src_pos = i * factor
        idx = int(src_pos)
        frac = src_pos - idx
        if idx + 1 < len(ch):
            res[i] = ch[idx] * (1.0 - frac) + ch[idx + 1] * frac
        elif idx < len(ch):
            res[i] = ch[idx]
    return res

def lowpass_1pole(ch, cutoff, rate=44100):
    omega = 2.0 * math.pi * min(cutoff, rate * 0.45) / rate
    alpha = omega / (1.0 + omega)
    y = 0.0
    out = [0.0] * len(ch)
    for i, x in enumerate(ch):
        y += alpha * (x - y)
        out[i] = y
    return out

def apply_envelope(left, right, target_duration, fade_in_ms, fade_out_ms, rate=44100):
    n = int(target_duration * rate)
    left = left[:n] if len(left) >= n else left + [0.0] * (n - len(left))
    right = right[:n] if len(right) >= n else right + [0.0] * (n - len(right))

    in_frames = int(fade_in_ms * rate / 1000.0)
    for i in range(min(in_frames, n)):
        g = 0.5 * (1.0 - math.cos(math.pi * i / float(in_frames)))
        left[i] *= g
        right[i] *= g

    out_frames = int(fade_out_ms * rate / 1000.0)
    start_out = n - out_frames
    for i in range(out_frames):
        idx = start_out + i
        p = i / float(out_frames)
        # Smooth raised-cosine with natural acoustic decay weighting
        g = 0.5 * (1.0 + math.cos(math.pi * p)) * math.exp(-p * 1.6)
        left[idx] *= g
        right[idx] *= g

    return left, right

def normalize_stereo(left, right, target_dbfs=-4.5):
    peak = max(max(abs(s) for s in left), max(abs(s) for s in right))
    if peak < 1e-5:
        return left, right, -96.0
    target_peak = 10.0 ** (target_dbfs / 20.0)
    scale = target_peak / peak
    left_n = [max(-0.95, min(0.95, s * scale)) for s in left]
    right_n = [max(-0.95, min(0.95, s * scale)) for s in right]
    actual_peak = max(max(abs(s) for s in left_n), max(abs(s) for s in right_n))
    actual_db = 20.0 * math.log10(max(1e-5, actual_peak))
    return left_n, right_n, actual_db

def write_wav(filepath, left, right, rate=44100):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    interleaved = []
    n = min(len(left), len(right))
    for i in range(n):
        l_val = int(max(-32767.0, min(32767.0, left[i] * 32767.0)))
        r_val = int(max(-32767.0, min(32767.0, right[i] * 32767.0)))
        interleaved.append(l_val)
        interleaved.append(r_val)

    with wave.open(filepath, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack(f"<{len(interleaved)}h", *interleaved))

def smooth_existing_wav_tail(filename, fade_out_ms=350, fade_in_ms=25):
    """Ensures existing sounds that previously ended abruptly have a smooth, clean fade-out."""
    path = os.path.join(SOUNDS_DIR, filename)
    if not os.path.exists(path):
        return
    with wave.open(path, "rb") as w:
        rate = w.getframerate()
        nframes = w.getnframes()
        raw = w.readframes(nframes)
        ch = w.getnchannels()
        if ch != 2:
            return
        samples = struct.unpack(f"<{len(raw)//2}h", raw)
        left = [samples[i*2] / 32768.0 for i in range(nframes)]
        right = [samples[i*2+1] / 32768.0 for i in range(nframes)]

    # Apply fade in
    in_frames = int(fade_in_ms * rate / 1000.0)
    for i in range(min(in_frames, nframes)):
        g = 0.5 * (1.0 - math.cos(math.pi * i / float(in_frames)))
        left[i] *= g
        right[i] *= g

    # Apply fade out
    out_frames = int(min(fade_out_ms * rate / 1000.0, nframes * 0.45))
    start_out = nframes - out_frames
    for i in range(out_frames):
        idx = start_out + i
        p = i / float(out_frames)
        g = 0.5 * (1.0 + math.cos(math.pi * p)) * math.exp(-p * 1.5)
        left[idx] *= g
        right[idx] *= g

    write_wav(path, left, right, rate)
    print(f"  [+] Smoothed fade out on {filename} ({nframes/rate:.2f}s, tail {fade_out_ms}ms)")

def main():
    print("====================================================================")
    print("  VOIDFALL DREDGE - ARRIVAL STINGER & AUDIO FADE OPTIMIZATION")
    print("====================================================================")

    orig_l, orig_r, rate = fetch_or_load_source()
    print(f"[*] Loaded master source stinger ({len(orig_l)/rate:.2f}s @ {rate}Hz)")

    # 1. Sector Arrival 1: Perimeter Crystalline Shriek (4.20s)
    # High-tension crystalline scream echoing through quartz caverns
    l1 = resample_linear(orig_l, 1.05)
    r1 = resample_linear(orig_r, 1.05)
    l1 = lowpass_1pole(l1, 7500, rate)
    r1 = lowpass_1pole(r1, 7500, rate)
    l1, r1 = apply_envelope(l1, r1, target_duration=4.20, fade_in_ms=45, fade_out_ms=1400, rate=rate)
    l1, r1, db1 = normalize_stereo(l1, r1, -5.0)
    out1 = os.path.join(SOUNDS_DIR, "sector_arrival_1.wav")
    write_wav(out1, l1, r1, rate)
    print(f"[+] Saved sector_arrival_1.wav (4.20s, {db1:.1f} dBFS, 1.4s smooth fade out)")

    # 2. Sector Arrival 2: Volatile Fault Subterranean Screamer (4.20s)
    # Deep tectonic roar & scream with 42Hz seismic sub-rumble
    l2 = resample_linear(orig_l, 0.80)
    r2 = resample_linear(orig_r, 0.80)
    l2 = lowpass_1pole(l2, 5000, rate)
    r2 = lowpass_1pole(r2, 5000, rate)
    # Add seismic sub-layer
    for i in range(len(l2)):
        t = i / float(rate)
        sub = math.sin(2.0 * math.pi * 42.0 * t) * math.exp(-t * 0.9) * 0.28
        l2[i] += sub
        r2[i] += sub
    l2, r2 = apply_envelope(l2, r2, target_duration=4.20, fade_in_ms=65, fade_out_ms=1400, rate=rate)
    l2, r2, db2 = normalize_stereo(l2, r2, -4.5)
    out2 = os.path.join(SOUNDS_DIR, "sector_arrival_2.wav")
    write_wav(out2, l2, r2, rate)
    print(f"[+] Saved sector_arrival_2.wav (4.20s, {db2:.1f} dBFS, 1.4s smooth fade out)")

    # 3. Sector Arrival 3: Void Abyssal Wail (4.50s)
    # Pitched down into dark abyssal moan-scream with dimensional comb resonance
    l3 = resample_linear(orig_l, 0.68)
    r3 = resample_linear(orig_r, 0.68)
    l3 = lowpass_1pole(l3, 4000, rate)
    r3 = lowpass_1pole(r3, 4000, rate)
    l3, r3 = apply_envelope(l3, r3, target_duration=4.50, fade_in_ms=80, fade_out_ms=1500, rate=rate)
    l3, r3, db3 = normalize_stereo(l3, r3, -4.0)
    out3 = os.path.join(SOUNDS_DIR, "sector_arrival_3.wav")
    write_wav(out3, l3, r3, rate)
    print(f"[+] Saved sector_arrival_3.wav (4.50s, {db3:.1f} dBFS, 1.5s smooth fade out)")

    # 4. Sector Arrival 4: Cavern Hunter Screech (3.80s)
    # High-pitched distant alien screech with acoustic cavern delay slapback
    l4 = resample_linear(orig_l, 1.15)
    r4 = resample_linear(orig_r, 1.15)
    l4 = lowpass_1pole(l4, 6800, rate)
    r4 = lowpass_1pole(r4, 6800, rate)
    # Add cavern reflection delay tap (130ms)
    delay_frames = int(0.13 * rate)
    l4_echo = [0.0] * len(l4)
    r4_echo = [0.0] * len(r4)
    for i in range(len(l4)):
        if i >= delay_frames:
            l4_echo[i] = l4[i - delay_frames] * 0.32
            r4_echo[i] = r4[i - delay_frames] * 0.32
    for i in range(len(l4)):
        l4[i] = l4[i] * 0.78 + l4_echo[i]
        r4[i] = r4[i] * 0.78 + r4_echo[i]
    l4, r4 = apply_envelope(l4, r4, target_duration=3.80, fade_in_ms=45, fade_out_ms=1300, rate=rate)
    l4, r4, db4 = normalize_stereo(l4, r4, -4.8)
    out4 = os.path.join(SOUNDS_DIR, "sector_arrival_4.wav")
    write_wav(out4, l4, r4, rate)
    print(f"[+] Saved sector_arrival_4.wav (3.80s, {db4:.1f} dBFS, 1.3s smooth fade out)")

    # 5. Sector Arrival 5: Swarm Infrasound Howl (4.20s)
    # Dual-pitch choral scream layer (0.94x + 0.86x) creating a terrifying dissonant alien chorus
    l5_a = resample_linear(orig_l, 0.94)
    r5_a = resample_linear(orig_r, 0.94)
    l5_b = resample_linear(orig_l, 0.86)
    r5_b = resample_linear(orig_r, 0.86)
    n5 = min(len(l5_a), len(l5_b))
    l5 = [l5_a[i] * 0.60 + l5_b[i] * 0.40 for i in range(n5)]
    r5 = [r5_a[i] * 0.60 + r5_b[i] * 0.40 for i in range(n5)]
    l5 = lowpass_1pole(l5, 5800, rate)
    r5 = lowpass_1pole(r5, 5800, rate)
    l5, r5 = apply_envelope(l5, r5, target_duration=4.20, fade_in_ms=55, fade_out_ms=1400, rate=rate)
    l5, r5, db5 = normalize_stereo(l5, r5, -4.5)
    out5 = os.path.join(SOUNDS_DIR, "sector_arrival_5.wav")
    write_wav(out5, l5, r5, rate)
    print(f"[+] Saved sector_arrival_5.wav (4.20s, {db5:.1f} dBFS, 1.4s smooth fade out)")

    # 6. Stalker Echo Screech (2.20s)
    # Long-range hunting screech echoing through cavern tunnels with smooth 0.75s decay
    l_ech = resample_linear(orig_l, 1.0)
    r_ech = resample_linear(orig_r, 1.0)
    l_ech = lowpass_1pole(l_ech, 6800, rate)
    r_ech = lowpass_1pole(r_ech, 6800, rate)
    l_ech, r_ech = apply_envelope(l_ech, r_ech, target_duration=2.20, fade_in_ms=30, fade_out_ms=750, rate=rate)
    l_ech, r_ech, db_ech = normalize_stereo(l_ech, r_ech, -4.0)
    out_ech = os.path.join(SOUNDS_DIR, "stalker_echo_screech.wav")
    write_wav(out_ech, l_ech, r_ech, rate)
    print(f"[+] Saved stalker_echo_screech.wav (2.20s, {db_ech:.1f} dBFS, 0.75s smooth fade out)")

    # Smooth other abruptly cut sounds
    abrupt_cues = [
        ("cavern_groan.wav", 400),
        ("burrower_grind.wav", 300),
        ("beacon_siren.wav", 350),
        ("damage_warning.wav", 250),
        ("gravity_distortion.wav", 300),
        ("jump.wav", 150),
        ("sonar_pulse.wav", 250),
        ("stalker_hiss.wav", 200),
        ("void_wind.wav", 400),
        ("organic_creak.wav", 250),
        ("bulkhead_dismantle.wav", 150),
        ("geothermal_vent.wav", 350),
        ("tactical_barricade.wav", 250),
        ("jump.wav", 200),
        ("stalker_chitter_01.wav", 200),
        ("stalker_chitter_02.wav", 250),
        ("stalker_chitter_03.wav", 200),
        ("stalker_hiss.wav", 280),
        ("stalker_hiss_03.wav", 250),
        ("ui_upgrade.wav", 300),
        ("voxel_break_basalt.wav", 250)
    ]
    for fname, tail_ms in abrupt_cues:
        smooth_existing_wav_tail(fname, fade_out_ms=tail_ms)

    # Sync files to build & dist directories
    for target in [BUILD_DIR, DIST_DIR]:
        if os.path.exists(os.path.dirname(target)):
            os.makedirs(target, exist_ok=True)
            for fname in os.listdir(SOUNDS_DIR):
                s = os.path.join(SOUNDS_DIR, fname)
                d = os.path.join(target, fname)
                if os.path.isfile(s):
                    shutil.copy2(s, d)
            print(f"[+] Synchronized sound assets to: {target}")

    print("====================================================================")
    print("  AUDIO GENERATION & FADE ENVELOPE POLISHING COMPLETE")
    print("====================================================================")

if __name__ == "__main__":
    main()
