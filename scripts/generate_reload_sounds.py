#!/usr/bin/env python3
"""
Generate weapon reload audio assets for Voidfall Dredge:
1. plasma_carbine_reload.wav (Vanguard: thermal cell ejection, fresh plasma canister slam & charge chirp)
2. scattergun_reload.wav (Demolitionist: heavy drum latch release, rotating ratchet clicks, steel drum slam, forepump rack)
3. railgun_reload.wav (Scout: pneumatic needle rack eject, linear battery insert, magnetic coil whine & charging bolt lock)
"""

import math
import os
import struct
import wave

SAMPLE_RATE = 44100

def write_wav(filename, samples, sample_rate=44100):
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with wave.open(filename, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        raw = bytearray()
        for l, r in samples:
            l_clamped = max(-1.0, min(1.0, l))
            r_clamped = max(-1.0, min(1.0, r))
            l_int = int(l_clamped * 32767.0)
            r_int = int(r_clamped * 32767.0)
            raw.extend(struct.pack('<hh', l_int, r_int))
        w.writeframes(raw)
    print(f"Generated {filename} ({len(samples)} frames, {len(samples)/sample_rate:.2f}s)")

def smooth_step(edge0, edge1, x):
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)

def generate_plasma_carbine_reload():
    # 1.25s duration
    duration = 1.25
    total_frames = int(duration * SAMPLE_RATE)
    samples = []
    
    phase0 = 0.0
    phase1 = 0.0
    phase2 = 0.0
    lp_noise = 0.0
    seed = 1234567

    for f in range(total_frames):
        t = f / SAMPLE_RATE
        dt = 1.0 / SAMPLE_RATE

        # Pseudo-random noise
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        noise = (seed / 1073741824.0) - 1.0

        # Stage 1 (0.0 - 0.28s): Cell eject latch click & pressurized steam decompress
        eject_click = 0.0
        if t < 0.12:
            phase0 += 880.0 * dt
            eject_click = math.sin(2.0 * math.pi * phase0) * math.exp(-t * 42.0) * 0.50
        
        alpha_hiss = 2.0 * math.pi * 2200.0 / (SAMPLE_RATE + 2.0 * math.pi * 2200.0)
        lp_noise += alpha_hiss * (noise - lp_noise)
        hiss = lp_noise * math.exp(-t * 10.0) * 0.32 if t < 0.32 else 0.0

        # Stage 2 (0.45 - 0.72s): Battery insertion slide friction & locking slam
        insert_thud = 0.0
        if 0.45 <= t < 0.72:
            ti = t - 0.45
            phase1 += (250.0 - ti * 350.0) * dt
            body = math.sin(2.0 * math.pi * phase1)
            click = noise * 0.4
            insert_thud = (body * 0.65 + click * 0.35) * math.exp(-ti * 25.0) * 0.85

        # Stage 3 (0.75 - 1.18s): Energizing capacitor charge hum & harmonic ionization chirp
        charge = 0.0
        if 0.75 <= t < 1.18:
            tc = t - 0.75
            charge_freq = 280.0 + tc * 540.0
            phase2 += charge_freq * dt
            envelope_sine = math.sin(math.pi * min(1.0, tc / 0.40))
            hum = math.sin(2.0 * math.pi * phase2) * envelope_sine
            chirp_phase = 2.0 * math.pi * (charge_freq * 2.5) * tc
            chirp = math.sin(chirp_phase) * 0.35 * envelope_sine
            charge = (hum * 0.60 + chirp * 0.40) * 0.65

        total = eject_click + hiss + insert_thud + charge

        # Envelope: Attack 5ms, Release 40ms
        env = 1.0
        if t < 0.005:
            env = t / 0.005
        elif t > duration - 0.040:
            env = max(0.0, (duration - t) / 0.040)
        total *= env * 0.75

        samples.append((total * 0.95, total * 1.0)) # slight stereo spread

    return samples

def generate_scattergun_reload():
    # 1.60s duration
    duration = 1.60
    total_frames = int(duration * SAMPLE_RATE)
    samples = []
    
    phase0 = 0.0
    phase1 = 0.0
    phase2 = 0.0
    phase3 = 0.0
    phase4 = 0.0
    seed = 7654321

    for f in range(total_frames):
        t = f / SAMPLE_RATE
        dt = 1.0 / SAMPLE_RATE

        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        noise = (seed / 1073741824.0) - 1.0

        # Stage 1 (0.0 - 0.28s): Drum latch release: mechanical spring clack + hollow steel ring
        latch_clack = 0.0
        if t < 0.22:
            phase0 += 560.0 * dt
            latch_clack = math.sin(2.0 * math.pi * phase0) * math.exp(-t * 24.0) * 0.60

        # Stage 2 (0.35 - 0.75s): Rotary cylinder ratchet clicks (3 rapid clicks)
        ratchet = 0.0
        for k in range(3):
            click_time = 0.38 + k * 0.11
            if click_time <= t < click_time + 0.06:
                tr = t - click_time
                phase1 += (900.0 + k * 150.0) * dt
                ratchet += math.sin(2.0 * math.pi * phase1) * math.exp(-tr * 70.0) * 0.45

        # Stage 3 (0.75 - 1.15s): Massive heavy drum slam into receiver + titanium clamp snap
        drum_slam = 0.0
        if 0.78 <= t < 1.12:
            ts = t - 0.78
            phase2 += max(40.0, 115.0 - ts * 120.0) * dt
            phase3 += 1250.0 * dt
            punch = math.sin(2.0 * math.pi * phase2) * 0.80
            snap = math.sin(2.0 * math.pi * phase3) * 0.40
            drum_slam = (punch + snap + noise * 0.2) * math.exp(-ts * 18.0) * 0.90

        # Stage 4 (1.18 - 1.50s): Fore-end pump rack forward
        pump = 0.0
        if 1.20 <= t < 1.48:
            tp = t - 1.20
            phase4 += max(200.0, 720.0 - tp * 320.0) * dt
            pump = math.sin(2.0 * math.pi * phase4) * math.exp(-tp * 30.0) * 0.55

        total = latch_clack + ratchet + drum_slam + pump

        # Master envelope
        env = 1.0
        if t < 0.005:
            env = t / 0.005
        elif t > duration - 0.040:
            env = max(0.0, (duration - t) / 0.040)
        total *= env * 0.75

        samples.append((total * 1.0, total * 0.92))

    return samples

def generate_railgun_reload():
    # 1.10s duration
    duration = 1.10
    total_frames = int(duration * SAMPLE_RATE)
    samples = []
    
    phase0 = 0.0
    phase1 = 0.0
    phase2 = 0.0
    phase3 = 0.0
    lp_noise = 0.0
    seed = 9876543

    for f in range(total_frames):
        t = f / SAMPLE_RATE
        dt = 1.0 / SAMPLE_RATE

        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        noise = (seed / 1073741824.0) - 1.0

        # Stage 1 (0.0 - 0.22s): Pneumatic needle rack eject: gas puff + slide click
        eject = 0.0
        if t < 0.20:
            phase0 += 1180.0 * dt
            click = math.sin(2.0 * math.pi * phase0) * math.exp(-t * 36.0)
            alpha = 2.0 * math.pi * 1600.0 / (SAMPLE_RATE + 2.0 * math.pi * 1600.0)
            lp_noise += alpha * (noise - lp_noise)
            eject = (click * 0.55 + lp_noise * 0.45) * math.exp(-t * 18.0) * 0.65

        # Stage 2 (0.38 - 0.66s): Linear needle battery insert: magnetic rail snap
        insert = 0.0
        if 0.38 <= t < 0.66:
            ti = t - 0.38
            phase1 += max(350.0, 1450.0 - ti * 1900.0) * dt
            body = math.sin(2.0 * math.pi * phase1)
            insert = body * math.exp(-ti * 26.0) * 0.75

        # Stage 3 (0.68 - 1.05s): Superconducting charging coil whine & slide lock
        charge = 0.0
        if 0.68 <= t < 1.05:
            tc = t - 0.68
            freq = 380.0 + tc * 1450.0
            phase2 += freq * dt
            whine = math.sin(2.0 * math.pi * phase2) * math.sin(math.pi * min(1.0, tc / 0.35))
            lock = 0.0
            if tc >= 0.24:
                tl = tc - 0.24
                phase3 += 1850.0 * dt
                lock = math.sin(2.0 * math.pi * phase3) * math.exp(-tl * 45.0) * 0.65
            charge = whine * 0.45 + lock * 0.55

        total = eject + insert + charge

        # Master envelope
        env = 1.0
        if t < 0.005:
            env = t / 0.005
        elif t > duration - 0.040:
            env = max(0.0, (duration - t) / 0.040)
        total *= env * 0.75

        samples.append((total * 0.94, total * 0.98))

    return samples

def main():
    dirs = [
        "assets/sounds",
        "build/Release/assets/sounds",
        "dist/VoidfallDredge/assets/sounds"
    ]
    
    carbine = generate_plasma_carbine_reload()
    scatter = generate_scattergun_reload()
    rail = generate_railgun_reload()

    for d in dirs:
        write_wav(os.path.join(d, "plasma_carbine_reload.wav"), carbine)
        write_wav(os.path.join(d, "scattergun_reload.wav"), scatter)
        write_wav(os.path.join(d, "railgun_reload.wav"), rail)

    print("All weapon reload WAV files successfully generated and deployed.")

if __name__ == "__main__":
    main()
