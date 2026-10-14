#!/usr/bin/env python3
"""
Voidfall Dredge - Enhanced Online Audio Ingestion & Multi-Stage Sound Design Pipeline

Downloads CC0 audio assets from verified internet sources (SFXMint),
applies subterranean DSP filtering, precise multi-track layering, envelope shaping,
and headroom mastering to generate game-ready 44.1kHz stereo WAVs for:

1. Enemy Locomotion & Vocalizations:
   - burrower_step.wav: Subterranean serpentine slither (scale friction, basalt grit slide, quiet organic crawl)
   - stalker_step.wav: Stealthy chitinous claw tip tap & quiet scuttle
   - stalker_run.wav: Rapid predatory multi-legged pursuit scuttle
   - goliath_step.wav: Armored apex vault guardian seismic footstep
   - stalker_lunge.wav: Visceral predatory attack shriek & leap
   - stalker_die.wav: Dissolving alien entity death dissipation
   - stalker_echo_screech.wav: Long-range cavern echo hunting screech

2. Weapon Reloads:
   - plasma_carbine_reload.wav (Vanguard): Thermal cell ejection, fresh canister rail slide, magnetic lock & electronic charge chirp
   - scattergun_reload.wav (Demolitionist): Heavy breech latch release, rotating cylinder ratchet clicks, steel drum slam, forepump rack
   - railgun_reload.wav (Scout): Pneumatic needle rack eject puff, magnetic needle cell rail insert, electromagnetic coil charge & bolt lock
"""

import os
import sys
import math
import struct
import io
import wave
import json
import urllib.request
import shutil

SAMPLE_RATE = 44100
TARGET_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "sounds")
BUILD_TARGET_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "Release", "assets", "sounds")
CACHE_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "sounds", ".cache")

ONLINE_SOURCES = {
    "creature_claw_scrape": "https://sfxmint.com/dl/creature-claw-scrape-01.wav",
    "concrete_scrape": "https://sfxmint.com/dl/concrete-scrape-01.wav",
    "steel_spike_scrape": "https://sfxmint.com/dl/steel-spike-scrape-01.wav",
    "stone_mill_grinding": "https://sfxmint.com/dl/stone-mill-grinding-01.wav",
    "monster_attack": "https://sfxmint.com/dl/growth-d9363e83aed2d7193fbd717a-part-1-v1.wav",
    "monster_death": "https://sfxmint.com/dl/growth-5f35e2215b50d5f6bc2a0886-part-1-v1.wav",
    "bat_screech": "https://sfxmint.com/dl/bat-screech-01.wav",
    "ratchet_wrench": "https://sfxmint.com/dl/hand-ratchet-wrench-01.wav",
    "metal_unfolding": "https://sfxmint.com/dl/growth-837d6051031a7ec47ed98ddc-part-1-v1.wav",
    "electronic_lock": "https://sfxmint.com/dl/owner-continuous-20260922-electronic-door-lock.wav",
    "carabiner_click": "https://sfxmint.com/dl/carabiner-gate-click-01.wav",
    "card_slide": "https://sfxmint.com/dl/card-deal-slide-01.wav",
    "switch_on": "https://sfxmint.com/dl/growth-9b29ebc2d4afaf902acb1bbd-part-1-v1.wav",
    "switch_off": "https://sfxmint.com/dl/growth-ebc3167dc5a581b1bc4f9861-part-1-v1.wav",
}

def fetch_cached(name, url):
    os.makedirs(CACHE_DIR, exist_ok=True)
    cache_file = os.path.join(CACHE_DIR, f"{name}.wav")
    if os.path.exists(cache_file) and os.path.getsize(cache_file) > 1000:
        with open(cache_file, "rb") as f:
            return f.read()
    print(f"  Downloading online asset: {name} from {url}...")
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 VoidfallAudioPipeline"})
    with urllib.request.urlopen(req, timeout=12) as resp:
        data = resp.read()
    with open(cache_file, "wb") as f:
        f.write(data)
    return data

def decode_wav(raw_bytes):
    with wave.open(io.BytesIO(raw_bytes), "rb") as wf:
        n_channels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        sample_rate = wf.getframerate()
        n_frames = wf.getnframes()
        data = wf.readframes(n_frames)

    samples = []
    if sampwidth == 2:
        count = n_frames * n_channels
        raw_samples = struct.unpack(f"<{count}h", data)
        samples = [s / 32768.0 for s in raw_samples]
    elif sampwidth == 3:
        for i in range(0, len(data), 3):
            val = int.from_bytes(data[i:i+3], byteorder='little', signed=True)
            samples.append(val / 8388608.0)
    elif sampwidth == 4:
        count = n_frames * n_channels
        samples = list(struct.unpack(f"<{count}f", data))
    else:
        raise ValueError(f"Unsupported sample width: {sampwidth}")

    channels = [[] for _ in range(n_channels)]
    for i, s in enumerate(samples):
        channels[i % n_channels].append(s)

    left = channels[0]
    right = channels[1] if n_channels > 1 else list(channels[0])
    return left, right, sample_rate

def resample_linear(ch, orig_rate, target_rate=44100):
    if orig_rate == target_rate:
        return ch
    ratio = float(orig_rate) / float(target_rate)
    target_len = int(len(ch) / ratio)
    resampled = [0.0] * target_len
    for i in range(target_len):
        src_pos = i * ratio
        idx = int(src_pos)
        frac = src_pos - idx
        if idx + 1 < len(ch):
            resampled[i] = ch[idx] * (1.0 - frac) + ch[idx + 1] * frac
        elif idx < len(ch):
            resampled[i] = ch[idx]
    return resampled

def pitch_shift_linear(ch, factor):
    if abs(factor - 1.0) < 0.005:
        return ch
    target_len = int(len(ch) / factor)
    shifted = [0.0] * target_len
    for i in range(target_len):
        src_pos = i * factor
        idx = int(src_pos)
        frac = src_pos - idx
        if idx + 1 < len(ch):
            shifted[i] = ch[idx] * (1.0 - frac) + ch[idx + 1] * frac
        elif idx < len(ch):
            shifted[i] = ch[idx]
    return shifted

def lowpass(ch, cutoff_hz=7000.0, sample_rate=44100):
    omega = 2.0 * math.pi * min(cutoff_hz, sample_rate * 0.45) / sample_rate
    alpha = omega / (1.0 + omega)
    filtered = [0.0] * len(ch)
    y = 0.0
    for i, x in enumerate(ch):
        y += alpha * (x - y)
        filtered[i] = y
    return filtered

def bandpass(ch, center_hz, q=3.0, sample_rate=44100):
    # 2-pole resonant bandpass filter
    w0 = 2.0 * math.pi * center_hz / sample_rate
    alpha = math.sin(w0) / (2.0 * q)
    b0 = alpha
    b1 = 0.0
    b2 = -alpha
    a0 = 1.0 + alpha
    a1 = -2.0 * math.cos(w0)
    a2 = 1.0 - alpha

    filtered = [0.0] * len(ch)
    x1, x2, y1, y2 = 0.0, 0.0, 0.0, 0.0
    for i, x in enumerate(ch):
        y = (b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2 = x1
        x1 = x
        y2 = y1
        y1 = y
        filtered[i] = y
    return filtered

def trim_silence(left, right, threshold=0.015, preroll_ms=4):
    preroll = int(preroll_ms * SAMPLE_RATE / 1000.0)
    start = 0
    for i in range(len(left)):
        if abs(left[i]) > threshold or abs(right[i]) > threshold:
            start = max(0, i - preroll)
            break
    return left[start:], right[start:]

def normalize_headroom(left, right, target_dbfs=-6.0):
    peak = max(max(abs(s) for s in left) if left else 0.0,
               max(abs(s) for s in right) if right else 0.0)
    if peak < 1e-6:
        return left, right, -96.0
    target_peak = 10.0 ** (target_dbfs / 20.0)
    scale = target_peak / peak
    norm_l = [max(-0.96, min(0.96, s * scale)) for s in left]
    norm_r = [max(-0.96, min(0.96, s * scale)) for s in right]
    actual_peak = max(max(abs(s) for s in norm_l), max(abs(s) for s in norm_r))
    actual_db = 20.0 * math.log10(max(1e-5, actual_peak))
    return norm_l, norm_r, actual_db

def apply_fades(left, right, in_ms=4, out_ms=10):
    in_frames = int(in_ms * SAMPLE_RATE / 1000.0)
    out_frames = int(out_ms * SAMPLE_RATE / 1000.0)
    n = len(left)
    for i in range(min(in_frames, n)):
        t = i / float(in_frames)
        g = 0.5 * (1.0 - math.cos(math.pi * t))
        left[i] *= g
        right[i] *= g
    for i in range(min(out_frames, n)):
        idx = n - 1 - i
        t = i / float(out_frames)
        g = 0.5 * (1.0 - math.cos(math.pi * t))
        left[idx] *= g
        right[idx] *= g

def write_stereo_wav(filepath, left, right):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    n_frames = min(len(left), len(right))
    interleaved = []
    for i in range(n_frames):
        l_pcm = int(max(-32767.0, min(32767.0, left[i] * 32767.0)))
        r_pcm = int(max(-32767.0, min(32767.0, right[i] * 32767.0)))
        interleaved.append(l_pcm)
        interleaved.append(r_pcm)
    with wave.open(filepath, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        data = struct.pack(f"<{len(interleaved)}h", *interleaved)
        wf.writeframes(data)

def mix_track(master_l, master_r, src_l, src_r, start_sec, gain=1.0, pitch=1.0, pan=0.0):
    if pitch != 1.0:
        src_l = pitch_shift_linear(src_l, pitch)
        src_r = pitch_shift_linear(src_r, pitch)
    start_frame = int(start_sec * SAMPLE_RATE)
    req_frames = start_frame + len(src_l)
    if len(master_l) < req_frames:
        master_l.extend([0.0] * (req_frames - len(master_l)))
        master_r.extend([0.0] * (req_frames - len(master_r)))

    pan_l = math.cos((pan + 1.0) * math.pi * 0.25)
    pan_r = math.sin((pan + 1.0) * math.pi * 0.25)

    for i in range(len(src_l)):
        master_l[start_frame + i] += src_l[i] * gain * pan_l
        master_r[start_frame + i] += src_r[i] * gain * pan_r

def load_processed_source(name):
    raw = fetch_cached(name, ONLINE_SOURCES[name])
    l, r, sr = decode_wav(raw)
    l = resample_linear(l, sr, SAMPLE_RATE)
    r = resample_linear(r, sr, SAMPLE_RATE)
    l, r = trim_silence(l, r)
    return l, r

# ==============================================================================
# SOUND DESIGN BUILDERS
# ==============================================================================

def build_burrower_slither(sources):
    """
    Subterranean serpentine slither noise (accurate to 4-segment serpentine carapace):
    Carapace scales sliding over cavern stone grit with organic frictional rasp.
    Designed to be eerie, tactile, and QUIETER than industrial stomps.
    """
    master_l, master_r = [], []
    conc_l, conc_r = sources["concrete_scrape"]
    claw_l, claw_r = sources["creature_claw_scrape"]
    spike_l, spike_r = sources["steel_spike_scrape"]
    mill_l, mill_r = sources["stone_mill_grinding"]

    # Slice 0.32s segments for cyclical segmented crawler gait
    dur = 0.32
    frames = int(dur * SAMPLE_RATE)

    # 1. Low earth/grit sliding body friction (stone mill excerpt filtered around 450 Hz)
    sub_l = lowpass(mill_l[:frames], 480.0)
    sub_r = lowpass(mill_r[:frames], 480.0)
    mix_track(master_l, master_r, sub_l, sub_r, 0.0, gain=0.45, pitch=0.85, pan=-0.1)

    # 2. Segmented scale sliding friction (concrete scrape bandpassed at 1200 Hz with slow swell)
    scrape_l = bandpass(conc_l[:frames], 1200.0, q=2.0)
    scrape_r = bandpass(conc_r[:frames], 1200.0, q=2.0)
    # Apply sinusoidal serpentine crawl envelope (swells in middle)
    for i in range(len(scrape_l)):
        env = math.sin(math.pi * (i / float(frames)))
        scrape_l[i] *= env
        scrape_r[i] *= env
    mix_track(master_l, master_r, scrape_l, scrape_r, 0.02, gain=0.40, pitch=1.05, pan=0.1)

    # 3. Soft chitinous scale edge rasp (claw scrape filtered at 2200 Hz)
    rasp_l = lowpass(claw_l[:frames], 2200.0)
    rasp_r = lowpass(claw_r[:frames], 2200.0)
    for i in range(len(rasp_l)):
        env = math.sin(math.pi * (i / float(frames))) ** 1.5
        rasp_l[i] *= env * 0.32
        rasp_r[i] *= env * 0.32
    mix_track(master_l, master_r, rasp_l, rasp_r, 0.05, gain=0.35, pitch=0.75, pan=0.0)

    # Master envelope: smooth fade in/out
    apply_fades(master_l, master_r, in_ms=15, out_ms=35)
    # Substantially quieter headroom normalization: -11.0 dBFS
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-11.0)
    return norm_l, norm_r

def build_stalker_step(sources):
    """
    Void Stalker stealth locomotion: delicate chitinous claw tip tap on stone
    with subtle scale brush in the dark. Very quiet and eerie.
    """
    master_l, master_r = [], []
    claw_l, claw_r = sources["creature_claw_scrape"]
    spike_l, spike_r = sources["steel_spike_scrape"]

    dur = 0.16
    frames = int(dur * SAMPLE_RATE)

    # 1. High-frequency chitin tip impact transient
    tip_l = bandpass(spike_l[:frames], 2600.0, q=3.5)
    tip_r = bandpass(spike_r[:frames], 2600.0, q=3.5)
    for i in range(len(tip_l)):
        env = math.exp(-i / (SAMPLE_RATE * 0.025))
        tip_l[i] *= env
        tip_r[i] *= env
    mix_track(master_l, master_r, tip_l, tip_r, 0.0, gain=0.55, pitch=1.15, pan=-0.15)

    # 2. Soft rock grit friction brush
    brush_l = lowpass(claw_l[:frames], 1800.0)
    brush_r = lowpass(claw_r[:frames], 1800.0)
    for i in range(len(brush_l)):
        env = math.exp(-i / (SAMPLE_RATE * 0.045))
        brush_l[i] *= env
        brush_r[i] *= env
    mix_track(master_l, master_r, brush_l, brush_r, 0.008, gain=0.38, pitch=0.90, pan=0.1)

    # Secondary micro-claw click at 32ms
    sec_l = bandpass(spike_l[:frames], 3100.0, q=3.0)
    sec_r = bandpass(spike_r[:frames], 3100.0, q=3.0)
    for i in range(len(sec_l)):
        env = math.exp(-i / (SAMPLE_RATE * 0.020))
        sec_l[i] *= env * 0.4
        sec_r[i] *= env * 0.4
    mix_track(master_l, master_r, sec_l, sec_r, 0.032, gain=0.40, pitch=1.28, pan=0.2)

    apply_fades(master_l, master_r, in_ms=4, out_ms=25)
    # Stealth quiet: -12.5 dBFS
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-12.5)
    return norm_l, norm_r

def build_stalker_run(sources):
    """
    Rapid predatory scuttling / sprint clatter in the dark:
    Multi-claw staccato clicks with aggressive stone gouge.
    """
    master_l, master_r = [], []
    claw_l, claw_r = sources["creature_claw_scrape"]
    spike_l, spike_r = sources["steel_spike_scrape"]
    conc_l, conc_r = sources["concrete_scrape"]

    dur = 0.22
    frames = int(dur * SAMPLE_RATE)

    # Rapid sequence of 3 claw taps
    for k, delay in enumerate([0.0, 0.042, 0.088]):
        p = 1.0 + k * 0.12
        pan = -0.25 + k * 0.25
        tap_l = bandpass(spike_l[:frames], 2400.0 + k * 300.0, q=3.0)
        tap_r = bandpass(spike_r[:frames], 2400.0 + k * 300.0, q=3.0)
        for i in range(len(tap_l)):
            env = math.exp(-i / (SAMPLE_RATE * 0.025))
            tap_l[i] *= env
            tap_r[i] *= env
        mix_track(master_l, master_r, tap_l, tap_r, delay, gain=0.65 - k * 0.08, pitch=p, pan=pan)

    # Underneath: fast friction scrape
    scr_l = bandpass(conc_l[:frames], 1600.0, q=2.0)
    scr_r = bandpass(conc_r[:frames], 1600.0, q=2.0)
    for i in range(len(scr_l)):
        env = math.exp(-i / (SAMPLE_RATE * 0.080))
        scr_l[i] *= env
        scr_r[i] *= env
    mix_track(master_l, master_r, scr_l, scr_r, 0.02, gain=0.45, pitch=1.10, pan=0.0)

    apply_fades(master_l, master_r, in_ms=4, out_ms=25)
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-8.5)
    return norm_l, norm_r

def build_goliath_step(sources):
    """
    Chitin Goliath Apex Vault Guardian seismic armored footstep.
    """
    master_l, master_r = [], []
    mill_l, mill_r = sources["stone_mill_grinding"]
    spike_l, spike_r = sources["steel_spike_scrape"]
    conc_l, conc_r = sources["concrete_scrape"]

    dur = 0.35
    frames = int(dur * SAMPLE_RATE)

    # Heavy sub-bass stone impact
    sub_l = lowpass(mill_l[:frames], 180.0)
    sub_r = lowpass(mill_r[:frames], 180.0)
    for i in range(len(sub_l)):
        env = math.exp(-i / (SAMPLE_RATE * 0.090))
        sub_l[i] *= env * 1.5
        sub_r[i] *= env * 1.5
    mix_track(master_l, master_r, sub_l, sub_r, 0.0, gain=0.90, pitch=0.65, pan=0.0)

    # Crushed stone crunch
    crunch_l = bandpass(conc_l[:frames], 750.0, q=1.8)
    crunch_r = bandpass(conc_r[:frames], 750.0, q=1.8)
    for i in range(len(crunch_l)):
        env = math.exp(-i / (SAMPLE_RATE * 0.070))
        crunch_l[i] *= env
        crunch_r[i] *= env
    mix_track(master_l, master_r, crunch_l, crunch_r, 0.015, gain=0.65, pitch=0.85, pan=-0.1)

    # Exo-carapace plate clang
    plate_l = bandpass(spike_l[:frames], 950.0, q=2.5)
    plate_r = bandpass(spike_r[:frames], 950.0, q=2.5)
    for i in range(len(plate_l)):
        env = math.exp(-i / (SAMPLE_RATE * 0.040))
        plate_l[i] *= env
        plate_r[i] *= env
    mix_track(master_l, master_r, plate_l, plate_r, 0.005, gain=0.55, pitch=0.70, pan=0.1)

    apply_fades(master_l, master_r, in_ms=4, out_ms=30)
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-4.5)
    return norm_l, norm_r

def build_stalker_lunge(sources):
    """
    Predatory attack shriek with creature attack snarl and chitin scrape.
    """
    master_l, master_r = [], []
    att_l, att_r = sources["monster_attack"]
    claw_l, claw_r = sources["creature_claw_scrape"]

    dur = 0.95
    frames = int(dur * SAMPLE_RATE)

    # 1. Organic visceral creature snarl
    snarl_l = lowpass(att_l[:frames], 6000.0)
    snarl_r = lowpass(att_r[:frames], 6000.0)
    mix_track(master_l, master_r, snarl_l, snarl_r, 0.0, gain=0.85, pitch=1.12, pan=0.0)

    # 2. Razor claw lunge swipe
    swipe_l = bandpass(claw_l[:frames], 2400.0, q=1.5)
    swipe_r = bandpass(claw_r[:frames], 2400.0, q=1.5)
    mix_track(master_l, master_r, swipe_l, swipe_r, 0.08, gain=0.65, pitch=1.25, pan=0.2)

    apply_fades(master_l, master_r, in_ms=5, out_ms=40)
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-4.0)
    return norm_l, norm_r

def build_stalker_die(sources):
    """
    Dissolving alien entity dissipation hum and energy collapse.
    """
    master_l, master_r = [], []
    death_l, death_r = sources["monster_death"]

    dur = 1.60
    frames = int(dur * SAMPLE_RATE)
    d_l = lowpass(death_l[:frames], 5500.0)
    d_r = lowpass(death_r[:frames], 5500.0)
    mix_track(master_l, master_r, d_l, d_r, 0.0, gain=0.90, pitch=0.88, pan=0.0)

    apply_fades(master_l, master_r, in_ms=10, out_ms=80)
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-4.5)
    return norm_l, norm_r

def build_stalker_screech(sources):
    """
    Echoing cavern hunting screech echoing through cavern shafts.
    """
    master_l, master_r = [], []
    bat_l, bat_r = sources["bat_screech"]
    att_l, att_r = sources["monster_attack"]

    dur = 2.10
    frames = int(dur * SAMPLE_RATE)

    # 1. Distant cavern hunter scream
    scr_l = lowpass(bat_l[:frames], 6500.0)
    scr_r = lowpass(bat_r[:frames], 6500.0)
    mix_track(master_l, master_r, scr_l, scr_r, 0.0, gain=0.75, pitch=0.82, pan=-0.1)

    # 2. Resonant throat growl body
    gr_l = lowpass(att_l[:int(1.2 * SAMPLE_RATE)], 3200.0)
    gr_r = lowpass(att_r[:int(1.2 * SAMPLE_RATE)], 3200.0)
    mix_track(master_l, master_r, gr_l, gr_r, 0.15, gain=0.60, pitch=0.70, pan=0.1)

    apply_fades(master_l, master_r, in_ms=8, out_ms=60)
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-4.0)
    return norm_l, norm_r

# ==============================================================================
# WEAPON RELOAD SOUND DESIGN BUILDERS
# ==============================================================================

def build_plasma_carbine_reload(sources):
    """
    Vanguard Plasma Carbine: 1.25s tactical multi-stage reload
    Stage 1 (0.00s): Spent thermal cell ejection latch click & pressure release
    Stage 2 (0.42s): Fresh plasma canister rail alignment & tactile slide insert
    Stage 3 (0.78s): Solid magnetic canister clamp slam & electronic charge initialization chirp
    """
    master_l, master_r = [], []
    click_l, click_r = sources["carabiner_click"]
    slide_l, slide_r = sources["card_slide"]
    lock_l, lock_r = sources["electronic_lock"]
    sw_off_l, sw_off_r = sources["switch_off"]
    sw_on_l, sw_on_r = sources["switch_on"]

    # Stage 1: Eject latch click
    c_l = bandpass(click_l[:int(0.25 * SAMPLE_RATE)], 2400.0, q=2.0)
    c_r = bandpass(click_r[:int(0.25 * SAMPLE_RATE)], 2400.0, q=2.0)
    mix_track(master_l, master_r, c_l, c_r, 0.0, gain=0.75, pitch=1.10, pan=-0.2)

    swo_l = lowpass(sw_off_l[:int(0.20 * SAMPLE_RATE)], 4500.0)
    swo_r = lowpass(sw_off_r[:int(0.20 * SAMPLE_RATE)], 4500.0)
    mix_track(master_l, master_r, swo_l, swo_r, 0.04, gain=0.65, pitch=0.95, pan=-0.1)

    # Stage 2: Canister slide into battery receiver
    sl_l = bandpass(slide_l[:int(0.35 * SAMPLE_RATE)], 1800.0, q=1.5)
    sl_r = bandpass(slide_r[:int(0.35 * SAMPLE_RATE)], 1800.0, q=1.5)
    mix_track(master_l, master_r, sl_l, sl_r, 0.40, gain=0.70, pitch=1.05, pan=0.1)

    swi_l = lowpass(sw_on_l[:int(0.18 * SAMPLE_RATE)], 3500.0)
    swi_r = lowpass(sw_on_r[:int(0.18 * SAMPLE_RATE)], 3500.0)
    mix_track(master_l, master_r, swi_l, swi_r, 0.65, gain=0.60, pitch=1.15, pan=0.15)

    # Stage 3: Magnetic canister lock & electronic charge confirmation chirp
    lk_l = lowpass(lock_l[:int(0.50 * SAMPLE_RATE)], 6500.0)
    lk_r = lowpass(lock_r[:int(0.50 * SAMPLE_RATE)], 6500.0)
    mix_track(master_l, master_r, lk_l, lk_r, 0.78, gain=0.85, pitch=1.08, pan=0.0)

    # Master envelope and trim
    max_frames = int(1.25 * SAMPLE_RATE)
    master_l = master_l[:max_frames]
    master_r = master_r[:max_frames]
    apply_fades(master_l, master_r, in_ms=4, out_ms=30)
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-4.0)
    return norm_l, norm_r

def build_scattergun_reload(sources):
    """
    Demolitionist Magma Scattergun: 1.60s heavy mechanical drum reload
    Stage 1 (0.00s): Heavy breech latch release & swing open
    Stage 2 (0.36s): Multi-tooth rotating cylinder ratchet clicks
    Stage 3 (0.82s): Heavy steel drum slam into receiver & lock
    Stage 4 (1.22s): Fore-end pump rack forward & chamber
    """
    master_l, master_r = [], []
    ratch_l, ratch_r = sources["ratchet_wrench"]
    unfold_l, unfold_r = sources["metal_unfolding"]
    click_l, click_r = sources["carabiner_click"]
    sw_off_l, sw_off_r = sources["switch_off"]

    # Stage 1: Breech latch release & heavy metal swing
    uf1_l = bandpass(unfold_l[:int(0.35 * SAMPLE_RATE)], 1400.0, q=1.5)
    uf1_r = bandpass(unfold_r[:int(0.35 * SAMPLE_RATE)], 1400.0, q=1.5)
    mix_track(master_l, master_r, uf1_l, uf1_r, 0.0, gain=0.80, pitch=0.85, pan=-0.2)

    # Stage 2: Isolated crisp cylinder ratchet clicks
    rt_l = bandpass(ratch_l[:int(0.45 * SAMPLE_RATE)], 2200.0, q=2.0)
    rt_r = bandpass(ratch_r[:int(0.45 * SAMPLE_RATE)], 2200.0, q=2.0)
    mix_track(master_l, master_r, rt_l, rt_r, 0.38, gain=0.90, pitch=1.05, pan=0.1)

    # Stage 3: Heavy steel drum slam into receiver
    slam_l = lowpass(unfold_l[int(0.8 * SAMPLE_RATE):int(1.25 * SAMPLE_RATE)], 2800.0)
    slam_r = lowpass(unfold_r[int(0.8 * SAMPLE_RATE):int(1.25 * SAMPLE_RATE)], 2800.0)
    mix_track(master_l, master_r, slam_l, slam_r, 0.82, gain=0.95, pitch=0.78, pan=0.0)

    # Stage 4: Forepump rack forward
    cl_l = bandpass(click_l[:int(0.25 * SAMPLE_RATE)], 1600.0, q=1.8)
    cl_r = bandpass(click_r[:int(0.25 * SAMPLE_RATE)], 1600.0, q=1.8)
    mix_track(master_l, master_r, cl_l, cl_r, 1.22, gain=0.85, pitch=0.90, pan=0.15)

    max_frames = int(1.60 * SAMPLE_RATE)
    master_l = master_l[:max_frames]
    master_r = master_r[:max_frames]
    apply_fades(master_l, master_r, in_ms=4, out_ms=35)
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-3.5)
    return norm_l, norm_r

def build_railgun_reload(sources):
    """
    Scout Needler Railgun: 1.10s surgical electromagnetic reload
    Stage 1 (0.00s): Pneumatic needle rack release puff & spring ejection
    Stage 2 (0.38s): Magnetic needle rail slide insertion
    Stage 3 (0.72s): Magnetic locking bolt clamp & electromagnetic inductive charging whine
    """
    master_l, master_r = [], []
    click_l, click_r = sources["carabiner_click"]
    slide_l, slide_r = sources["card_slide"]
    lock_l, lock_r = sources["electronic_lock"]
    sw_on_l, sw_on_r = sources["switch_on"]

    # Stage 1: Pneumatic rack ejection
    c_l = bandpass(click_l[:int(0.22 * SAMPLE_RATE)], 2800.0, q=2.5)
    c_r = bandpass(click_r[:int(0.22 * SAMPLE_RATE)], 2800.0, q=2.5)
    mix_track(master_l, master_r, c_l, c_r, 0.0, gain=0.80, pitch=1.20, pan=-0.2)

    # Stage 2: Precision needle rail slide
    sl_l = bandpass(slide_l[:int(0.32 * SAMPLE_RATE)], 2200.0, q=1.8)
    sl_r = bandpass(slide_r[:int(0.32 * SAMPLE_RATE)], 2200.0, q=1.8)
    mix_track(master_l, master_r, sl_l, sl_r, 0.36, gain=0.75, pitch=1.15, pan=0.1)

    sw_l = lowpass(sw_on_l[:int(0.18 * SAMPLE_RATE)], 4800.0)
    sw_r = lowpass(sw_on_r[:int(0.18 * SAMPLE_RATE)], 4800.0)
    mix_track(master_l, master_r, sw_l, sw_r, 0.55, gain=0.65, pitch=1.25, pan=0.15)

    # Stage 3: Bolt clamp & high-frequency inductive charging
    lk_l = lowpass(lock_l[:int(0.40 * SAMPLE_RATE)], 7200.0)
    lk_r = lowpass(lock_r[:int(0.40 * SAMPLE_RATE)], 7200.0)
    mix_track(master_l, master_r, lk_l, lk_r, 0.72, gain=0.88, pitch=1.20, pan=0.0)

    max_frames = int(1.10 * SAMPLE_RATE)
    master_l = master_l[:max_frames]
    master_r = master_r[:max_frames]
    apply_fades(master_l, master_r, in_ms=4, out_ms=30)
    norm_l, norm_r, db = normalize_headroom(master_l, master_r, target_dbfs=-3.8)
    return norm_l, norm_r

# ==============================================================================
# MAIN PIPELINE
# ==============================================================================

def main():
    print("====================================================================")
    print("  VOIDFALL DREDGE - ENHANCED SOUND IMPORT & DSP EDITING PIPELINE")
    print("====================================================================")
    print(f"Target Output Directory: {TARGET_DIR}")
    os.makedirs(TARGET_DIR, exist_ok=True)
    if os.path.exists(os.path.dirname(BUILD_TARGET_DIR)):
        os.makedirs(BUILD_TARGET_DIR, exist_ok=True)

    # 1. Download & cache all online audio sources
    print("\n[*] Ingesting CC0 online audio sources from SFXMint...")
    sources = {}
    for name in ONLINE_SOURCES:
        try:
            sources[name] = load_processed_source(name)
            dur = len(sources[name][0]) / float(SAMPLE_RATE)
            print(f"  [+] Loaded source '{name}' ({dur:.2f}s, 44.1kHz stereo)")
        except Exception as e:
            print(f"  [!] Failed loading '{name}': {e}")
            return 1

    # 2. Build sound cues
    print("\n[*] Synthesizing & mastering enhanced sound cues...")
    burrower_base = build_burrower_slither(sources)
    stalker_step_base = build_stalker_step(sources)
    stalker_run_base = build_stalker_run(sources)
    goliath_base = build_goliath_step(sources)

    cues_to_build = {
        # Enemy locomotion (slithering, crawling, footsteps)
        "burrower_step.wav": burrower_base,
        "burrower_step_01.wav": burrower_base,
        "burrower_step_02.wav": (pitch_shift_linear(burrower_base[0], 0.94), pitch_shift_linear(burrower_base[1], 0.94)),

        "stalker_step.wav": stalker_step_base,
        "stalker_step_01.wav": stalker_step_base,
        "stalker_step_02.wav": (pitch_shift_linear(stalker_step_base[0], 1.06), pitch_shift_linear(stalker_step_base[1], 1.06)),
        "stalker_step_03.wav": (pitch_shift_linear(stalker_step_base[0], 0.95), pitch_shift_linear(stalker_step_base[1], 0.95)),

        "stalker_run.wav": stalker_run_base,
        "stalker_run_01.wav": stalker_run_base,
        "stalker_run_02.wav": (pitch_shift_linear(stalker_run_base[0], 1.08), pitch_shift_linear(stalker_run_base[1], 1.08)),
        "stalker_run_03.wav": (pitch_shift_linear(stalker_run_base[0], 0.94), pitch_shift_linear(stalker_run_base[1], 0.94)),

        "goliath_step.wav": goliath_base,
        "goliath_step_01.wav": goliath_base,
        "goliath_step_02.wav": (pitch_shift_linear(goliath_base[0], 0.92), pitch_shift_linear(goliath_base[1], 0.92)),

        # Enemy vocalizations
        "stalker_lunge.wav": build_stalker_lunge(sources),
        "stalker_die.wav": build_stalker_die(sources),
        "stalker_echo_screech.wav": build_stalker_screech(sources),

        # Weapon Reloads
        "plasma_carbine_reload.wav": build_plasma_carbine_reload(sources),
        "scattergun_reload.wav": build_scattergun_reload(sources),
        "railgun_reload.wav": build_railgun_reload(sources),
    }

    manifest_entries = {}
    for filename, (left, right) in cues_to_build.items():
        dur = len(left) / float(SAMPLE_RATE)
        peak = max(max(abs(s) for s in left), max(abs(s) for s in right))
        db = 20.0 * math.log10(max(1e-5, peak))

        # Write to target dir
        dst_path = os.path.join(TARGET_DIR, filename)
        write_stereo_wav(dst_path, left, right)
        print(f"  [+] Output '{filename}': {dur:.2f}s, peak {db:.1f} dBFS")

        # Copy to build target dir
        if os.path.exists(BUILD_TARGET_DIR):
            dst_build = os.path.join(BUILD_TARGET_DIR, filename)
            shutil.copy2(dst_path, dst_build)

        manifest_entries[filename.replace(".wav", "")] = {
            "file": filename,
            "duration_seconds": round(dur, 3),
            "peak_dbfs": round(db, 2),
            "sample_rate": SAMPLE_RATE,
            "channels": 2
        }

    # Update sounds_manifest.json
    manifest_path = os.path.join(TARGET_DIR, "sounds_manifest.json")
    if os.path.exists(manifest_path):
        try:
            with open(manifest_path, "r") as mf:
                manifest = json.load(mf)
            for k, v in manifest_entries.items():
                manifest["sounds"][k] = v
            with open(manifest_path, "w") as mf:
                json.dump(manifest, mf, indent=2)
            if os.path.exists(BUILD_TARGET_DIR):
                shutil.copy2(manifest_path, os.path.join(BUILD_TARGET_DIR, "sounds_manifest.json"))
            print(f"\n[+] Updated manifest: {manifest_path}")
        except Exception as e:
            print(f"[!] Warning: failed updating manifest: {e}")

    print("\n====================================================================")
    print("  AUDIO PIPELINE COMPLETE: All enemy and reload audio assets deployed!")
    print("====================================================================")
    return 0

if __name__ == "__main__":
    sys.exit(main())
