#!/usr/bin/env python3
"""
Voidfall Dredge - Audio Asset Expansion Generator
Generates procedural, ear-safe 16-bit 44.1kHz stereo audio assets for:
- Void Stalker vocalization pools (roars 01-04, chitters 01-03, hisses 01-03)
- Subterranean cavern ambiance stems (groans 01-04, drips 01-04, settling 01-03, rock slides 01-03)
- Multi-strata mining drill variations (titanium, voidite, basalt 01-03)
- Player death, suit puncture, and respiratory vitals
All files include smooth cosine attack/release envelopes (0 clicks) and peak limiting.
"""

import os
import math
import struct
import wave
import json

SAMPLE_RATE = 44100
SOUNDS_DIR = os.path.join("assets", "sounds")
MANIFEST_PATH = os.path.join(SOUNDS_DIR, "sounds_manifest.json")

def write_wav(filename, samples_l, samples_r, sample_rate=44100):
    filepath = os.path.join(SOUNDS_DIR, filename)
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    
    # Peak normalization to -4.5 dBFS (~0.60 peak amplitude) for ear safety
    peak = max(max(abs(s) for s in samples_l), max(abs(s) for s in samples_r), 0.001)
    target_peak = 0.58
    scale = (target_peak / peak) if peak > target_peak else 1.0
    
    raw = bytearray()
    for l, r in zip(samples_l, samples_r):
        sl = int(max(-32767, min(32767, l * scale * 32767.0)))
        sr = int(max(-32767, min(32767, r * scale * 32767.0)))
        raw.extend(struct.pack("<hh", sl, sr))
        
    with wave.open(filepath, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(raw)
    print(f"  [+] Wrote {filename} ({len(samples_l)/sample_rate:.2f}s)")

def cosine_window(num_samples, attack_s=0.008, release_s=0.025, sample_rate=44100):
    att_samples = int(attack_s * sample_rate)
    rel_samples = int(release_s * sample_rate)
    env = [1.0] * num_samples
    for i in range(min(att_samples, num_samples)):
        env[i] = 0.5 * (1.0 - math.cos(math.pi * i / max(1, att_samples)))
    for i in range(min(rel_samples, num_samples)):
        idx = num_samples - 1 - i
        env[idx] = 0.5 * (1.0 - math.cos(math.pi * i / max(1, rel_samples)))
    return env

def generate_stalker_roars():
    # 4 distinct roar variants: different sweep curves, guttural resonance, and duration
    configs = [
        ("void_stalker_roar_01.wav", 1.10, 480.0, 160.0, 42.0, 0.40),
        ("void_stalker_roar_02.wav", 1.35, 540.0, 140.0, 38.0, 0.55),
        ("void_stalker_roar_03.wav", 0.95, 420.0, 180.0, 46.0, 0.35),
        ("void_stalker_roar_04.wav", 1.25, 590.0, 150.0, 40.0, 0.50),
    ]
    for name, dur, start_f, end_f, sub_f, rasp_amt in configs:
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [], []
        phase, phase_sub, phase_rasp = 0.0, 0.0, 0.0
        seed = 12345 + int(start_f)
        for i in range(n):
            t = i * dt
            progress = t / dur
            freq = start_f * math.exp(-progress * 2.8) + end_f
            phase += freq * dt
            phase_sub += sub_f * dt
            phase_rasp += 85.0 * dt
            
            carrier = math.sin(2.0 * math.pi * phase)
            sub = math.sin(2.0 * math.pi * phase_sub) * 0.45
            rasp = math.sin(2.0 * math.pi * phase_rasp) * rasp_amt
            
            # Formant / noise texture
            seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
            noise = ((seed & 0xFFFF) / 65535.0) * 2.0 - 1.0
            
            amp = math.exp(-progress * 2.4)
            val = (carrier * (1.0 + rasp) + sub + noise * 0.22) * amp
            sl.append(val)
            sr.append(val * 0.95)
            
        win = cosine_window(n, 0.015, 0.08)
        write_wav(name, [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

def generate_stalker_chitters():
    configs = [
        ("stalker_chitter_01.wav", 0.45, 12, 1600.0),
        ("stalker_chitter_02.wav", 0.60, 16, 1450.0),
        ("stalker_chitter_03.wav", 0.38, 9, 1800.0),
    ]
    for name, dur, num_clicks, click_freq in configs:
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [0.0] * n, [0.0] * n
        click_spacing = dur / (num_clicks + 1)
        for c in range(num_clicks):
            c_time = click_spacing * (c + 1) + (c % 3 - 1) * 0.008
            c_start = int(c_time * SAMPLE_RATE)
            c_dur = int(0.012 * SAMPLE_RATE)
            f_rand = click_freq * (0.92 + 0.16 * (c % 4) / 4.0)
            for j in range(c_dur):
                idx = c_start + j
                if idx < n:
                    tj = j * dt
                    val = math.sin(2.0 * math.pi * f_rand * tj) * math.exp(-tj * 420.0) * 0.70
                    sl[idx] += val
                    sr[idx] += val * 0.92
        win = cosine_window(n, 0.005, 0.02)
        write_wav(name, [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

def generate_stalker_hisses():
    configs = [
        ("stalker_hiss_01.wav", 0.65, 2400.0, 0.8),
        ("stalker_hiss_02.wav", 0.85, 2100.0, 1.2),
        ("stalker_hiss_03.wav", 0.55, 2800.0, 0.6),
    ]
    for name, dur, cutoff, pitch_drop in configs:
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [], []
        seed = 445566
        filt = 0.0
        for i in range(n):
            t = i * dt
            progress = t / dur
            alpha = (2.0 * math.pi * (cutoff - progress * 400.0) / SAMPLE_RATE)
            alpha = max(0.01, min(0.4, alpha))
            seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
            noise = ((seed & 0xFFFF) / 65535.0) * 2.0 - 1.0
            filt += alpha * (noise - filt)
            
            # Menacing inhalation swell
            env = math.sin(math.pi * progress) if progress < 0.4 else math.exp(-(progress - 0.4) * 3.5)
            sl.append(filt * env * 0.65)
            sr.append(filt * env * 0.60)
        win = cosine_window(n, 0.02, 0.05)
        write_wav(name, [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

def generate_cavern_ambience():
    # Settling, groans, drips, rock slides
    for i, dur in enumerate([0.75, 1.10, 0.90], 1):
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [], []
        f0 = 42.0 + i * 4.0
        phase = 0.0
        filt = 0.0
        seed = 7711 + i * 333
        for j in range(n):
            t = j * dt
            prog = t / dur
            phase += (f0 - prog * 10.0) * dt
            sub = math.sin(2.0 * math.pi * phase) * 0.6
            seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
            noise = ((seed & 0xFFFF) / 65535.0) * 2.0 - 1.0
            filt += 0.04 * (noise - filt)
            env = math.exp(-prog * 4.2)
            val = (sub + filt * 0.4) * env
            sl.append(val)
            sr.append(val * 0.9)
        win = cosine_window(n, 0.02, 0.05)
        write_wav(f"cavern_settling_0{i}.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

    for i, dur in enumerate([0.80, 1.25, 0.95, 1.40], 1):
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [], []
        f0 = 55.0 - i * 3.0
        phase = 0.0
        for j in range(n):
            t = j * dt
            prog = t / dur
            phase += (f0 - prog * 20.0) * dt
            groan = math.sin(2.0 * math.pi * phase)
            env = math.exp(-prog * 3.8)
            sl.append(groan * env * 0.7)
            sr.append(groan * env * 0.65)
        win = cosine_window(n, 0.03, 0.08)
        write_wav(f"cavern_groan_0{i}.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

    for i, f_start in enumerate([1850.0, 2100.0, 1650.0, 1950.0], 1):
        dur = 0.35
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [], []
        phase = 0.0
        for j in range(n):
            t = j * dt
            freq = f_start * math.exp(-t * 65.0) + 900.0
            phase += freq * dt
            ping = math.sin(2.0 * math.pi * phase) * math.exp(-t * 24.0)
            echo = math.sin(2.0 * math.pi * (freq * 0.85) * max(0.0, t - 0.12)) * math.exp(-max(0.0, t - 0.12) * 18.0) * 0.3 if t > 0.12 else 0.0
            val = (ping + echo) * 0.65
            sl.append(val)
            sr.append(val * 0.85)
        win = cosine_window(n, 0.002, 0.02)
        write_wav(f"cavern_drip_0{i}.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

    for i, dur in enumerate([0.85, 1.20, 0.95], 1):
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [], []
        seed = 998877 + i * 111
        filt = 0.0
        for j in range(n):
            t = j * dt
            prog = t / dur
            seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
            noise = ((seed & 0xFFFF) / 65535.0) * 2.0 - 1.0
            alpha = max(0.02, min(0.35, 0.08 + 0.15 * math.sin(math.pi * prog)))
            filt += alpha * (noise - filt)
            env = math.sin(math.pi * prog) * math.exp(-prog * 1.5)
            sl.append(filt * env * 0.6)
            sr.append(filt * env * 0.55)
        win = cosine_window(n, 0.04, 0.08)
        write_wav(f"rock_slide_0{i}.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

def generate_drill_strata():
    # Titanium (metallic clangs & overtones)
    for i in range(1, 4):
        dur = 0.55
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [], []
        f0 = 850.0 + i * 120.0
        f1 = f0 * 2.14
        for j in range(n):
            t = j * dt
            clang = math.sin(2.0 * math.pi * f0 * t) * 0.6 + math.sin(2.0 * math.pi * f1 * t) * 0.35
            env = math.exp(-t * 14.0)
            sl.append(clang * env * 0.7)
            sr.append(clang * env * 0.65)
        win = cosine_window(n, 0.003, 0.02)
        write_wav(f"drill_strata_titanium_0{i}.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

    # Voidite (crystalline brittle harmonics)
    for i in range(1, 4):
        dur = 0.60
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [], []
        f0 = 1420.0 + i * 180.0
        f1 = f0 * 1.618
        for j in range(n):
            t = j * dt
            shatter = math.sin(2.0 * math.pi * f0 * t) * 0.55 + math.sin(2.0 * math.pi * f1 * t) * 0.40
            env = math.exp(-t * 11.0)
            sl.append(shatter * env * 0.65)
            sr.append(shatter * env * 0.60)
        win = cosine_window(n, 0.003, 0.02)
        write_wav(f"drill_strata_voidite_0{i}.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

    # Basalt (dense granite crunch)
    for i in range(1, 4):
        dur = 0.50
        n = int(dur * SAMPLE_RATE)
        dt = 1.0 / SAMPLE_RATE
        sl, sr = [], []
        f0 = 95.0 + i * 15.0
        seed = 332211 + i * 444
        filt = 0.0
        for j in range(n):
            t = j * dt
            thud = math.sin(2.0 * math.pi * f0 * t) * 0.5
            seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
            noise = ((seed & 0xFFFF) / 65535.0) * 2.0 - 1.0
            filt += 0.06 * (noise - filt)
            env = math.exp(-t * 16.0)
            sl.append((thud + filt * 0.5) * env * 0.7)
            sr.append((thud + filt * 0.5) * env * 0.65)
        win = cosine_window(n, 0.003, 0.02)
        write_wav(f"drill_strata_basalt_0{i}.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

def generate_player_vitals():
    # Player death: heart flatline + pneumatic depressurization
    dur = 1.65
    n = int(dur * SAMPLE_RATE)
    dt = 1.0 / SAMPLE_RATE
    sl, sr = [], []
    phase = 0.0
    filt = 0.0
    seed = 884422
    for j in range(n):
        t = j * dt
        prog = t / dur
        # Deep sinking resonant sub-bass
        phase += (65.0 - prog * 35.0) * dt
        sub = math.sin(2.0 * math.pi * phase) * 0.4
        # Suit decompression hiss
        seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
        noise = ((seed & 0xFFFF) / 65535.0) * 2.0 - 1.0
        filt += 0.05 * (noise - filt)
        hiss = filt * math.exp(-prog * 2.2) * 0.5
        # Flatline hum fading out
        flatline = math.sin(2.0 * math.pi * 750.0 * t) * math.exp(-prog * 3.5) * 0.25
        val = (sub * math.exp(-prog * 1.8) + hiss + flatline) * 0.75
        sl.append(val)
        sr.append(val * 0.95)
    win = cosine_window(n, 0.01, 0.12)
    write_wav("player_death_01.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

    # Suit puncture: sharp pneumatic burst
    dur = 0.55
    n = int(dur * SAMPLE_RATE)
    dt = 1.0 / SAMPLE_RATE
    sl, sr = [], []
    seed = 991133
    filt = 0.0
    for j in range(n):
        t = j * dt
        seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
        noise = ((seed & 0xFFFF) / 65535.0) * 2.0 - 1.0
        filt += 0.18 * (noise - filt)
        env = math.exp(-t * 9.0)
        val = filt * env * 0.75
        sl.append(val)
        sr.append(val * 0.9)
    win = cosine_window(n, 0.003, 0.03)
    write_wav("suit_puncture_01.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

    # Player breathing: respirator cycle
    dur = 1.10
    n = int(dur * SAMPLE_RATE)
    dt = 1.0 / SAMPLE_RATE
    sl, sr = [], []
    seed = 557799
    filt = 0.0
    for j in range(n):
        t = j * dt
        prog = t / dur
        seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
        noise = ((seed & 0xFFFF) / 65535.0) * 2.0 - 1.0
        filt += 0.04 * (noise - filt)
        env = math.sin(math.pi * prog) * 0.65
        val = filt * env
        sl.append(val)
        sr.append(val * 0.92)
    win = cosine_window(n, 0.05, 0.05)
    write_wav("player_breathing_01.wav", [s * w for s, w in zip(sl, win)], [s * w for s, w in zip(sr, win)])

def main():
    print("[*] Generating expanded audio assets for Voidfall Dredge...")
    generate_stalker_roars()
    generate_stalker_chitters()
    generate_stalker_hisses()
    generate_cavern_ambience()
    generate_drill_strata()
    generate_player_vitals()
    print("[+] All 30 expanded audio variations successfully generated in assets/sounds/!")

if __name__ == "__main__":
    main()
