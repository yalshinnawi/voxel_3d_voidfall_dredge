#!/usr/bin/env python3
"""
Voidfall Dredge - Sound Import & DSP Editing Pipeline
Imports high-quality CC0 game audio assets from verified internet repositories (SFXMint & Kenney CC0),
applies custom subterranean DSP filtering, headroom normalization, attack trimming,
and seamless loop crossfades, and outputs game-ready 44.1kHz stereo PCM WAV files into assets/sounds/.
"""

import os
import sys
import math
import struct
import io
import wave
import json
import urllib.request
import urllib.parse
import shutil

TARGET_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "sounds")
BUILD_TARGET_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "Release", "assets", "sounds")
MANIFEST_FILE = os.path.join(TARGET_DIR, "sounds_manifest.json")

# Map SoundCue names to internet source specifications
# All audio licensed under Creative Commons Zero (CC0 1.0 Universal) - Public Domain
SOUND_CATALOG = {
    # ── Mining & Tools ──
    "drill_loop": {
        "role": "engine",
        "fallback_url": "https://sfxmint.com/dl/vehicle-engine-01.wav",
        "description": "Continuous mechanical borer motor and rotary drill teeth",
        "is_loop": True,
        "lowpass_cutoff": 6500.0,
        "peak_dbfs": -5.0,
        "max_duration": 6.0
    },
    "voxel_hit": {
        "role": "metal-hit",
        "fallback_url": "https://sfxmint.com/dl/impact-metal-hit-32.wav",
        "description": "Tactile tungsten chisel / pick impact thud",
        "lowpass_cutoff": 7800.0,
        "peak_dbfs": -4.0,
        "max_duration": 0.40
    },
    "voxel_break_basalt": {
        "role": "crate-break",
        "fallback_url": "https://sfxmint.com/dl/impact-crate-break-01.wav",
        "description": "Crumbly basalt stone fracture and rubble collapse",
        "lowpass_cutoff": 6800.0,
        "peak_dbfs": -4.0,
        "max_duration": 0.75
    },
    "voxel_break_titanium": {
        "role": "metal-hit",
        "fallback_url": "https://sfxmint.com/dl/impact-metal-hit-32.wav",
        "description": "Resonant industrial titanium clang with ringing metallic overtone",
        "lowpass_cutoff": 8500.0,
        "peak_dbfs": -4.0,
        "max_duration": 0.85
    },
    "voxel_break_voidite": {
        "role": "shatter",
        "fallback_url": "https://sfxmint.com/dl/glass-shatter-01.wav",
        "description": "High-energy ethereal crystalline harmonic fracture",
        "lowpass_cutoff": 9000.0,
        "peak_dbfs": -4.5,
        "max_duration": 0.90
    },
    "voxel_break_bulkhead": {
        "role": "crash",
        "fallback_url": "https://sfxmint.com/dl/impact-crash-01.wav",
        "description": "Heavy reinforced bulkhead steel plate buckling and tear",
        "lowpass_cutoff": 7000.0,
        "peak_dbfs": -3.5,
        "max_duration": 1.10
    },
    "voxel_break_radioactive": {
        "role": "crackle",
        "fallback_url": "https://sfxmint.com/dl/ambience-crackle-01.wav",
        "description": "Brittle radioactive isotope fracture with ionizing sizzle",
        "lowpass_cutoff": 8000.0,
        "peak_dbfs": -4.5,
        "max_duration": 0.80
    },
    "bulkhead_deploy": {
        "role": "lock",
        "fallback_url": "https://sfxmint.com/dl/mechanical-lock-01.wav",
        "description": "Hydraulic clamp lock and hermetic magnetic seal",
        "lowpass_cutoff": 7500.0,
        "peak_dbfs": -4.0,
        "max_duration": 0.65
    },
    "bulkhead_dismantle": {
        "role": "servo",
        "fallback_url": "https://sfxmint.com/dl/mechanical-servo-01.wav",
        "description": "Mechanical latch release and pneumatic salvage catch",
        "lowpass_cutoff": 7500.0,
        "peak_dbfs": -4.5,
        "max_duration": 0.60
    },

    # ── Hostile Organisms ──
    "stalker_spotted": {
        "role": "growl",
        "fallback_url": "https://sfxmint.com/dl/horror-growl-04.wav",
        "description": "Predatory alien chitter and guttural throat rattle",
        "lowpass_cutoff": 7000.0,
        "peak_dbfs": -4.0,
        "max_duration": 1.20
    },
    "stalker_lunge": {
        "role": "horror-growl-06",
        "fallback_url": "https://sfxmint.com/dl/horror-growl-06.wav",
        "description": "Visceral predatory attack shriek with chitin scrape",
        "lowpass_cutoff": 7200.0,
        "peak_dbfs": -3.5,
        "max_duration": 0.95
    },
    "stalker_hit": {
        "role": "splat",
        "fallback_url": "https://sfxmint.com/dl/impact-splat-01.wav",
        "description": "Bio-mechanical claws striking armored chassis",
        "lowpass_cutoff": 6500.0,
        "peak_dbfs": -4.0,
        "max_duration": 0.50
    },
    "stalker_die": {
        "role": "enemy-death",
        "fallback_url": "https://sfxmint.com/dl/game-enemy-death-01.wav",
        "description": "Dissolving void entity dissipation hum and energy collapse",
        "lowpass_cutoff": 7000.0,
        "peak_dbfs": -4.5,
        "max_duration": 1.40
    },
    "stalker_echo_screech": {
        "role": "stinger",
        "fallback_url": "https://sfxmint.com/dl/horror-stinger-01.wav",
        "description": "Long-range alien hunting shriek echoing through cavern shafts",
        "lowpass_cutoff": 6800.0,
        "peak_dbfs": -4.0,
        "max_duration": 1.80
    },
    "stalker_chitter": {
        "role": "click",
        "fallback_url": "https://sfxmint.com/dl/ui-click-08.wav",
        "description": "Subtle predatory alien chitinous clicking & mandible twitch in the dark",
        "pitch_shift": 0.70,
        "lowpass_cutoff": 6500.0,
        "peak_dbfs": -8.0,
        "max_duration": 0.75
    },
    "stalker_hiss": {
        "role": "horror-growl-04",
        "fallback_url": "https://sfxmint.com/dl/horror-growl-04.wav",
        "description": "Low menacing predatory throat hiss when stalking or provoked",
        "pitch_shift": 0.85,
        "lowpass_cutoff": 5500.0,
        "peak_dbfs": -6.0,
        "max_duration": 0.70
    },
    "burrower_roar": {
        "role": "growl",
        "fallback_url": "https://sfxmint.com/dl/horror-growl-04.wav",
        "description": "Tectonic subterranean borer roar and earth rumble",
        "lowpass_cutoff": 5500.0,
        "pitch_shift": 0.65,  # Sub-octave downpitch for colossal scale
        "peak_dbfs": -3.5,
        "max_duration": 2.20
    },
    "burrower_grind": {
        "role": "machine",
        "fallback_url": "https://sfxmint.com/dl/mechanical-machine-01.wav",
        "description": "Tectonic grinding teeth chewing through solid stone",
        "lowpass_cutoff": 6000.0,
        "peak_dbfs": -4.0,
        "max_duration": 1.50
    },

    # ── Ambience & Hazards ──
    "ambient_cavern": {
        "role": "drone",
        "fallback_url": "https://sfxmint.com/dl/horror-drone-25.wav",
        "description": "Subterranean sub-bass tension drone and void airflow",
        "is_loop": True,
        "lowpass_cutoff": 5800.0,
        "peak_dbfs": -7.0,
        "max_duration": 10.0
    },
    "ambient_sector1": {
        "role": "drone",
        "fallback_url": "https://sfxmint.com/dl/horror-drone-25.wav",
        "description": "Sector 1 (Perimeter Drift): Crystalline resonant airflow and quartz harmonics",
        "is_loop": True,
        "pitch_shift": 1.25,
        "lowpass_cutoff": 6800.0,
        "peak_dbfs": -7.5,
        "max_duration": 10.0
    },
    "ambient_sector2": {
        "role": "drone",
        "fallback_url": "https://sfxmint.com/dl/ambience-thunder-06.wav",
        "description": "Sector 2 (Volatile Fault): Geothermal magma hum & basalt tectonic strain",
        "is_loop": True,
        "pitch_shift": 0.55,
        "lowpass_cutoff": 4500.0,
        "peak_dbfs": -7.0,
        "max_duration": 10.0
    },
    "ambient_sector3": {
        "role": "drone",
        "fallback_url": "https://sfxmint.com/dl/horror-drone-25.wav",
        "description": "Sector 3 (Void Cradle): Deep abyssal mantle drone, gravitational void pulse & dread",
        "is_loop": True,
        "pitch_shift": 0.42,
        "lowpass_cutoff": 3800.0,
        "peak_dbfs": -7.0,
        "max_duration": 10.0
    },
    "sector_arrival_1": {
        "role": "stinger",
        "fallback_url": "https://sfxmint.com/dl/feedback-level-up-01.wav",
        "description": "Sector 1 arrival stinger: Ethereal crystalline descent harmonic triad",
        "pitch_shift": 0.85,
        "lowpass_cutoff": 7500.0,
        "peak_dbfs": -5.0,
        "max_duration": 3.0
    },
    "sector_arrival_2": {
        "role": "stinger",
        "fallback_url": "https://sfxmint.com/dl/mechanical-siren-02.wav",
        "description": "Sector 2 arrival stinger: Industrial geothermal brass & tectonic pressure swell",
        "pitch_shift": 0.48,
        "lowpass_cutoff": 5000.0,
        "peak_dbfs": -4.5,
        "max_duration": 3.0
    },
    "sector_arrival_3": {
        "role": "stinger",
        "fallback_url": "https://sfxmint.com/dl/horror-stinger-01.wav",
        "description": "Sector 3 arrival stinger: Terrifying abyssal void strike and sub-bass impact",
        "pitch_shift": 0.45,
        "lowpass_cutoff": 4200.0,
        "peak_dbfs": -4.0,
        "max_duration": 3.2
    },
    "crystal_chime": {
        "role": "bell",
        "fallback_url": "https://sfxmint.com/dl/glass-shatter-01.wav",
        "description": "Sector 1 micro-ambience: Delicate quartz crystalline harmonic ring",
        "pitch_shift": 1.40,
        "lowpass_cutoff": 8500.0,
        "peak_dbfs": -7.0,
        "max_duration": 1.2
    },
    "geothermal_vent": {
        "role": "wind",
        "fallback_url": "https://sfxmint.com/dl/ambience-crackle-01.wav",
        "description": "Sector 2 micro-ambience: Pressurized volcanic steam / gas exhaust release",
        "pitch_shift": 0.65,
        "lowpass_cutoff": 5500.0,
        "peak_dbfs": -6.5,
        "max_duration": 1.5
    },
    "void_distortion": {
        "role": "zap",
        "fallback_url": "https://sfxmint.com/dl/energy-zap-01.wav",
        "description": "Sector 3 micro-ambience: Dimensional acoustic phase-warp and abyssal shimmer",
        "pitch_shift": 0.38,
        "lowpass_cutoff": 4500.0,
        "peak_dbfs": -6.5,
        "max_duration": 1.8
    },
    "cavern_drip": {
        "role": "drip",
        "fallback_url": "https://sfxmint.com/dl/water-drip-03.wav",
        "description": "Acoustic cavern droplet strike with natural cavern reverb",
        "lowpass_cutoff": 8500.0,
        "peak_dbfs": -5.0,
        "max_duration": 0.60
    },
    "cavern_groan": {
        "role": "creak",
        "fallback_url": "https://sfxmint.com/dl/horror-creak-17.wav",
        "description": "Deep tectonic stratum compression and structural rock groan",
        "lowpass_cutoff": 5500.0,
        "peak_dbfs": -4.5,
        "max_duration": 2.00
    },
    "seismic_tremor": {
        "role": "thunder",
        "fallback_url": "https://sfxmint.com/dl/ambience-thunder-06.wav",
        "description": "Low-frequency tectonic earthquake rumble and shaking rock",
        "lowpass_cutoff": 5000.0,
        "peak_dbfs": -3.5,
        "max_duration": 2.60
    },
    "geiger_click": {
        "role": "crackle",
        "fallback_url": "https://sfxmint.com/dl/ambience-crackle-01.wav",
        "description": "Poisson ionizing radiation detection click",
        "lowpass_cutoff": 9500.0,
        "peak_dbfs": -5.5,
        "max_duration": 0.15
    },
    "beacon_siren": {
        "role": "siren",
        "fallback_url": "https://sfxmint.com/dl/mechanical-siren-02.wav",
        "description": "Pulsing dual-tone tactical extraction beacon siren",
        "lowpass_cutoff": 7500.0,
        "peak_dbfs": -4.0,
        "max_duration": 1.60
    },
    "evac_touchdown": {
        "role": "rocket",
        "fallback_url": "https://sfxmint.com/dl/scifi-rocket-01.wav",
        "description": "Heavy retro-thruster landing roar and docking clamps",
        "lowpass_cutoff": 6200.0,
        "peak_dbfs": -3.5,
        "max_duration": 2.20
    },

    # ── Room Archetype & Environmental Hazard Atmosphere ──
    "lava_bubble": {
        "role": "liquid",
        "fallback_url": "https://sfxmint.com/dl/ambience-crackle-01.wav",
        "description": "Magma Caldera: Viscous bubbling thermite lava & volcanic churn",
        "pitch_shift": 0.45,
        "lowpass_cutoff": 4500.0,
        "peak_dbfs": -5.5,
        "max_duration": 0.70
    },
    "thermal_hiss": {
        "role": "steam",
        "fallback_url": "https://sfxmint.com/dl/ambience-crackle-01.wav",
        "description": "Fault Crevasse & Lava: Searing thermal steam hiss & volcanic gas",
        "pitch_shift": 0.75,
        "lowpass_cutoff": 5500.0,
        "peak_dbfs": -5.5,
        "max_duration": 0.95
    },
    "radioactive_hum": {
        "role": "hum",
        "fallback_url": "https://sfxmint.com/dl/ambience-hum-01.wav",
        "description": "Radioactive Core Sanctuary: Resonant ionizing electromagnetic hum",
        "pitch_shift": 0.85,
        "lowpass_cutoff": 6000.0,
        "peak_dbfs": -6.0,
        "max_duration": 1.20
    },
    "void_wind": {
        "role": "wind",
        "fallback_url": "https://sfxmint.com/dl/ambience-wind-01.wav",
        "description": "Abyssal Chasm & Void Rift: Eerie howling draft through vertical chasms",
        "pitch_shift": 0.55,
        "lowpass_cutoff": 4500.0,
        "peak_dbfs": -6.5,
        "max_duration": 1.50
    },
    "gravity_distortion": {
        "role": "drone",
        "fallback_url": "https://sfxmint.com/dl/ambience-drone-01.wav",
        "description": "Void Singularity Rift: Pitch-dropping gravitational warp pulse",
        "pitch_shift": 0.35,
        "lowpass_cutoff": 3800.0,
        "peak_dbfs": -5.0,
        "max_duration": 1.30
    },
    "spore_plop": {
        "role": "splat",
        "fallback_url": "https://sfxmint.com/dl/water-drip-03.wav",
        "description": "Fungoid Bio-Grotto: Damp fungal spore drop & bio-organic squelch",
        "pitch_shift": 0.65,
        "lowpass_cutoff": 6500.0,
        "peak_dbfs": -5.5,
        "max_duration": 0.45
    },
    "organic_creak": {
        "role": "creak",
        "fallback_url": "https://sfxmint.com/dl/horror-creak-17.wav",
        "description": "Fungoid Bio-Grotto: Creaking fibrous mycelium & fungal stalk strain",
        "pitch_shift": 0.70,
        "lowpass_cutoff": 5000.0,
        "peak_dbfs": -6.0,
        "max_duration": 0.85
    },
    "industrial_hum": {
        "role": "hum",
        "fallback_url": "https://sfxmint.com/dl/ambience-hum-02.wav",
        "description": "Foundry & Vault: 60Hz transformer electrical hum & generator whir",
        "pitch_shift": 1.0,
        "lowpass_cutoff": 5500.0,
        "peak_dbfs": -6.0,
        "max_duration": 1.40
    },
    "hydraulic_exhaust": {
        "role": "pneumatic",
        "fallback_url": "https://sfxmint.com/dl/mechanical-pneumatic-01.wav",
        "description": "Foundry & Vault: High-pressure pneumatic piston & steam relief exhaust",
        "pitch_shift": 0.80,
        "lowpass_cutoff": 6500.0,
        "peak_dbfs": -5.0,
        "max_duration": 0.75
    },
    "pebble_skitter": {
        "role": "debris",
        "fallback_url": "https://sfxmint.com/dl/impact-dirt-01.wav",
        "description": "Crumbling Canyon & Quarry: Loose stone gravel & debris skittering",
        "pitch_shift": 1.15,
        "lowpass_cutoff": 7500.0,
        "peak_dbfs": -5.5,
        "max_duration": 0.65
    },
    "spike_rattle": {
        "role": "metal-hit",
        "fallback_url": "https://sfxmint.com/dl/impact-metal-hit-32.wav",
        "description": "Spike Trench Arena: Hollow bone/metal rattle echoing through spike beds",
        "pitch_shift": 1.30,
        "lowpass_cutoff": 7500.0,
        "peak_dbfs": -5.0,
        "max_duration": 0.50
    },

    # ── Combat Weapons ──
    "plasma_fire": {
        "role": "laser",
        "fallback_url": "https://sfxmint.com/dl/retro-game-laser-01.wav",
        "description": "Vanguard: punchy coherent thermal plasma bolt discharge",
        "lowpass_cutoff": 8500.0,
        "peak_dbfs": -3.8,
        "max_duration": 0.45
    },
    "plasma_hit": {
        "role": "hit",
        "fallback_url": "https://sfxmint.com/dl/game-hit-01.wav",
        "description": "Kinetic thermal plasma impact and armor scorch",
        "lowpass_cutoff": 7500.0,
        "peak_dbfs": -4.0,
        "max_duration": 0.40
    },
    "scattergun_fire": {
        "role": "gunshot",
        "fallback_url": "https://sfxmint.com/dl/weapon-gunshot-01.wav",
        "description": "Demolitionist: heavy kinetic boom and flechette blast",
        "lowpass_cutoff": 7200.0,
        "peak_dbfs": -3.2,
        "max_duration": 0.65
    },
    "railgun_fire": {
        "role": "arc",
        "fallback_url": "https://sfxmint.com/dl/energy-arc-01.wav",
        "description": "Scout: supersonic electromagnetic whip crack",
        "lowpass_cutoff": 8800.0,
        "peak_dbfs": -3.5,
        "max_duration": 0.55
    },

    # ── Player Movement & Abilities ──
    "footstep": {
        "role": "footsteps-concrete",
        "fallback_url": "https://sfxmint.com/dl/footsteps-concrete-20.wav",
        "description": "Heavy exo-armor boot strike on cavern stone",
        "lowpass_cutoff": 7000.0,
        "peak_dbfs": -5.0,
        "max_duration": 0.35
    },
    "jump": {
        "role": "whoosh",
        "fallback_url": "https://sfxmint.com/dl/movement-whoosh-01.wav",
        "description": "Pneumatic jump thruster boost puff",
        "lowpass_cutoff": 7500.0,
        "peak_dbfs": -4.5,
        "max_duration": 0.40
    },
    "land": {
        "role": "thud",
        "fallback_url": "https://sfxmint.com/dl/impact-thud-01.wav",
        "description": "Kinetic boot impact and suspension recoil thud",
        "lowpass_cutoff": 6800.0,
        "peak_dbfs": -4.0,
        "max_duration": 0.45
    },
    "sonar_pulse": {
        "role": "hologram",
        "fallback_url": "https://sfxmint.com/dl/scifi-hologram-01.wav",
        "description": "Surveyor echolocation ping sweeping cavern corridors",
        "lowpass_cutoff": 8500.0,
        "peak_dbfs": -4.0,
        "max_duration": 0.90
    },
    "explosive_blast": {
        "role": "explosion",
        "fallback_url": "https://sfxmint.com/dl/game-explosion-01.wav",
        "description": "Demolition shaped-charge concussive blast and shockwave",
        "lowpass_cutoff": 6200.0,
        "peak_dbfs": -3.0,
        "max_duration": 1.40
    },
    "tactical_barricade": {
        "role": "shield",
        "fallback_url": "https://sfxmint.com/dl/scifi-shield-01.wav",
        "description": "Vanguard deployable energy barrier anchor hiss",
        "lowpass_cutoff": 7500.0,
        "peak_dbfs": -4.2,
        "max_duration": 0.80
    },
    "tactical_overcharge": {
        "role": "zap",
        "fallback_url": "https://sfxmint.com/dl/energy-zap-01.wav",
        "description": "Scout kinetic dash burst sonic release",
        "lowpass_cutoff": 8000.0,
        "peak_dbfs": -4.0,
        "max_duration": 0.60
    },

    # ── Interface & Audio Feedback ──
    "ui_blip": {
        "role": "click",
        "fallback_url": "https://sfxmint.com/dl/ui-click-08.wav",
        "description": "Crisp non-fatiguing navigation tick for HUD & terminal",
        "lowpass_cutoff": 9500.0,
        "peak_dbfs": -6.0,
        "max_duration": 0.18
    },
    "ui_upgrade": {
        "role": "level-up",
        "fallback_url": "https://sfxmint.com/dl/feedback-level-up-01.wav",
        "description": "Ascending harmonic terminal upgrade chime",
        "lowpass_cutoff": 9000.0,
        "peak_dbfs": -4.5,
        "max_duration": 0.90
    },
    "damage_warning": {
        "role": "heartbeat",
        "fallback_url": "https://sfxmint.com/dl/horror-heartbeat-34.wav",
        "description": "Cardiovascular systolic / diastolic panic pulse",
        "lowpass_cutoff": 5000.0,
        "peak_dbfs": -4.5,
        "max_duration": 1.10
    }
}


def resolve_wav_url(cue_name, spec):
    """Query SFXMint role endpoint to get active direct WAV URL, falling back to spec URL."""
    role = spec.get("role")
    if role:
        try:
            api_url = f"https://sfxmint.com/api/v1/roles/{role}?via=skill"
            req = urllib.request.Request(api_url, headers={"User-Agent": "Mozilla/5.0 VoidfallAudioPipeline"})
            with urllib.request.urlopen(req, timeout=4) as resp:
                data = json.loads(resp.read().decode("utf-8"))
                wav_url = data.get("wav_url")
                if wav_url:
                    return wav_url
        except Exception:
            pass
    return spec.get("fallback_url")


def fetch_audio_bytes(url):
    """Download audio file bytes with timeout and user-agent."""
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 VoidfallAudioPipeline"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        return resp.read()


def decode_wav(raw_bytes):
    """Decode raw WAV bytes into float samples [-1.0 .. 1.0], sample_rate, and channels."""
    with wave.open(io.BytesIO(raw_bytes), "rb") as wf:
        n_channels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        sample_rate = wf.getframerate()
        n_frames = wf.getnframes()
        data = wf.readframes(n_frames)

    samples = []
    if sampwidth == 2:  # 16-bit PCM
        count = n_frames * n_channels
        fmt = f"<{count}h"
        raw_samples = struct.unpack(fmt, data)
        samples = [s / 32768.0 for s in raw_samples]
    elif sampwidth == 3:  # 24-bit PCM
        for i in range(0, len(data), 3):
            val = int.from_bytes(data[i:i+3], byteorder='little', signed=True)
            samples.append(val / 8388608.0)
    elif sampwidth == 1:  # 8-bit unsigned
        samples = [(b - 128) / 128.0 for b in data]
    elif sampwidth == 4:  # 32-bit float
        count = n_frames * n_channels
        fmt = f"<{count}f"
        samples = list(struct.unpack(fmt, data))
    else:
        raise ValueError(f"Unsupported sample width: {sampwidth}")

    # Separate into channels: list of [ch0_samples, ch1_samples]
    channels = [[] for _ in range(n_channels)]
    for i, s in enumerate(samples):
        ch = i % n_channels
        channels[ch].append(s)

    # Standardize to stereo
    if n_channels == 1:
        stereo_left = list(channels[0])
        stereo_right = list(channels[0])
    else:
        stereo_left = list(channels[0])
        stereo_right = list(channels[1])

    return stereo_left, stereo_right, sample_rate


def resample_linear(ch_samples, orig_rate, target_rate=44100):
    """Linear resample channel samples to 44.1 kHz."""
    if orig_rate == target_rate:
        return ch_samples
    ratio = float(orig_rate) / float(target_rate)
    target_len = int(len(ch_samples) / ratio)
    resampled = [0.0] * target_len
    for i in range(target_len):
        src_pos = i * ratio
        idx = int(src_pos)
        frac = src_pos - idx
        if idx + 1 < len(ch_samples):
            resampled[i] = ch_samples[idx] * (1.0 - frac) + ch_samples[idx + 1] * frac
        elif idx < len(ch_samples):
            resampled[i] = ch_samples[idx]
    return resampled


def pitch_shift_linear(ch_samples, pitch_factor):
    """Pitch shift by simple resample (faster = higher, slower = lower)."""
    if abs(pitch_factor - 1.0) < 0.01:
        return ch_samples
    target_len = int(len(ch_samples) / pitch_factor)
    shifted = [0.0] * target_len
    for i in range(target_len):
        src_pos = i * pitch_factor
        idx = int(src_pos)
        frac = src_pos - idx
        if idx + 1 < len(ch_samples):
            shifted[i] = ch_samples[idx] * (1.0 - frac) + ch_samples[idx + 1] * frac
        elif idx < len(ch_samples):
            shifted[i] = ch_samples[idx]
    return shifted


def trim_leading_silence(left, right, threshold_ratio=0.08, min_thresh=0.015, preroll_ms=4, sample_rate=44100, **kwargs):
    """Find start of sound and trim dead air to preserve instantaneous responsive playback."""
    peak = 0.0
    for s in left:
        abs_s = abs(s)
        if abs_s > peak: peak = abs_s
    for s in right:
        abs_s = abs(s)
        if abs_s > peak: peak = abs_s

    threshold = max(min_thresh, peak * threshold_ratio)
    preroll_frames = int(preroll_ms * sample_rate / 1000.0)
    start_frame = 0
    for i in range(len(left)):
        if abs(left[i]) > threshold or abs(right[i]) > threshold:
            start_frame = max(0, i - preroll_frames)
            break
    return left[start_frame:], right[start_frame:]


def apply_subterranean_lowpass(ch_samples, cutoff_hz=7500.0, sample_rate=44100):
    """1-pole low-pass filter to remove harsh digital sizzle and embed sound in dense cavern rock."""
    omega = 2.0 * math.pi * min(cutoff_hz, sample_rate * 0.45) / sample_rate
    alpha = omega / (1.0 + omega)
    filtered = [0.0] * len(ch_samples)
    y = 0.0
    for i, x in enumerate(ch_samples):
        y += alpha * (x - y)
        filtered[i] = y
    return filtered


def apply_boundary_fades(left, right, fade_in_ms=3, fade_out_ms=6, sample_rate=44100):
    """Cosine fades at file boundaries to guarantee 0 clicks or pops on voice triggers and cuts."""
    in_frames = int(fade_in_ms * sample_rate / 1000.0)
    out_frames = int(fade_out_ms * sample_rate / 1000.0)
    n = len(left)

    for i in range(min(in_frames, n)):
        t = i / float(in_frames)
        gain = 0.5 * (1.0 - math.cos(math.pi * t))
        left[i] *= gain
        right[i] *= gain

    for i in range(min(out_frames, n)):
        idx = n - 1 - i
        t = i / float(out_frames)
        gain = 0.5 * (1.0 - math.cos(math.pi * t))
        left[idx] *= gain
        right[idx] *= gain


def apply_loop_crossfade(left, right, crossfade_ms=250, sample_rate=44100):
    """Apply equal-power circular crossfade at endpoints so continuous audio loops seamlessly."""
    xf_frames = int(crossfade_ms * sample_rate / 1000.0)
    n = len(left)
    if n <= xf_frames * 2:
        return left, right

    out_left = list(left[:-xf_frames])
    out_right = list(right[:-xf_frames])

    for i in range(xf_frames):
        t = i / float(xf_frames)
        fade_out = math.cos(0.5 * math.pi * t)
        fade_in = math.sin(0.5 * math.pi * t)

        tail_idx = n - xf_frames + i
        head_idx = i

        out_left[head_idx] = left[head_idx] * fade_in + left[tail_idx] * fade_out
        out_right[head_idx] = right[head_idx] * fade_in + right[tail_idx] * fade_out

    return out_left, out_right


def normalize_headroom(left, right, target_dbfs=-4.0):
    """Peak-normalize samples to target dBFS with safety clamping."""
    peak = 0.0
    for s in left:
        abs_s = abs(s)
        if abs_s > peak: peak = abs_s
    for s in right:
        abs_s = abs(s)
        if abs_s > peak: peak = abs_s

    if peak < 1e-5:
        return left, right, -96.0

    target_peak = 10.0 ** (target_dbfs / 20.0)
    scale = target_peak / peak

    # Soft analog saturation clamp above 0.95
    norm_left = [max(-0.95, min(0.95, s * scale)) for s in left]
    norm_right = [max(-0.95, min(0.95, s * scale)) for s in right]
    actual_peak = max(max(abs(s) for s in norm_left), max(abs(s) for s in norm_right))
    actual_dbfs = 20.0 * math.log10(max(1e-5, actual_peak))

    return norm_left, norm_right, actual_dbfs


def write_stereo_wav(filepath, left, right, sample_rate=44100):
    """Write 16-bit PCM stereo WAV file."""
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
        wf.setframerate(sample_rate)
        data = struct.pack(f"<{len(interleaved)}h", *interleaved)
        wf.writeframes(data)


def main():
    print("====================================================================")
    print("  VOIDFALL DREDGE - SOUND IMPORT & DSP EDITING PIPELINE")
    print("====================================================================")
    print(f"Target Output Directory: {TARGET_DIR}")
    os.makedirs(TARGET_DIR, exist_ok=True)

    manifest = {
        "version": "1.0",
        "license": "CC0 1.0 Universal (Creative Commons Zero)",
        "sounds": {}
    }

    success_count = 0
    total_count = len(SOUND_CATALOG)

    for idx, (cue_name, spec) in enumerate(SOUND_CATALOG.items(), 1):
        filename = f"{cue_name}.wav"
        dest_path = os.path.join(TARGET_DIR, filename)

        print(f"[{idx:02d}/{total_count:02d}] Processing '{cue_name}' ({spec['description']})...", flush=True)

        url = resolve_wav_url(cue_name, spec)
        if not url:
            print(f"  [x] Failed to resolve URL for {cue_name}")
            continue

        try:
            raw_bytes = fetch_audio_bytes(url)
            left, right, orig_rate = decode_wav(raw_bytes)

            # 1. Resample to standard 44.1 kHz
            left = resample_linear(left, orig_rate, 44100)
            right = resample_linear(right, orig_rate, 44100)

            # 2. Optional pitch shift
            if "pitch_shift" in spec:
                left = pitch_shift_linear(left, spec["pitch_shift"])
                right = pitch_shift_linear(right, spec["pitch_shift"])

            # 3. Trim leading dead air
            left, right = trim_leading_silence(left, right, threshold_ratio=0.08, min_thresh=0.015, preroll_ms=4)

            # 4. Cap max duration if specified
            max_dur = spec.get("max_duration", 5.0)
            max_frames = int(max_dur * 44100)
            if len(left) > max_frames:
                left = left[:max_frames]
                right = right[:max_frames]

            # 5. Subterranean cavern low-pass filter
            cutoff = spec.get("lowpass_cutoff", 7500.0)
            left = apply_subterranean_lowpass(left, cutoff, 44100)
            right = apply_subterranean_lowpass(right, cutoff, 44100)

            # 6. Looping crossfade if continuous sound
            if spec.get("is_loop", False):
                left, right = apply_loop_crossfade(left, right, crossfade_ms=250, sample_rate=44100)

            # 7. Boundary anti-click fades
            apply_boundary_fades(left, right, fade_in_ms=3, fade_out_ms=8)

            # 8. Headroom normalization
            target_dbfs = spec.get("peak_dbfs", -4.0)
            left, right, actual_dbfs = normalize_headroom(left, right, target_dbfs)

            # 9. Save edited WAV
            write_stereo_wav(dest_path, left, right, 44100)

            dur_sec = len(left) / 44100.0
            print(f"  [+] Saved {filename} ({dur_sec:.2f}s, {actual_dbfs:.1f} dBFS, filter {cutoff:.0f}Hz)")

            manifest["sounds"][cue_name] = {
                "file": filename,
                "duration_seconds": round(dur_sec, 3),
                "peak_dbfs": round(actual_dbfs, 2),
                "lowpass_cutoff_hz": cutoff,
                "is_loop": spec.get("is_loop", False),
                "source_url": url,
                "description": spec["description"]
            }
            success_count += 1

        except Exception as e:
            print(f"  [!] Error processing {cue_name}: {e}")

    # Write manifest
    with open(MANIFEST_FILE, "w") as mf:
        json.dump(manifest, mf, indent=2)
    print(f"\n[+] Manifest written to {MANIFEST_FILE}")

    # Copy to build directory if it exists
    if os.path.exists(os.path.dirname(BUILD_TARGET_DIR)):
        os.makedirs(BUILD_TARGET_DIR, exist_ok=True)
        for fname in os.listdir(TARGET_DIR):
            src_f = os.path.join(TARGET_DIR, fname)
            dst_f = os.path.join(BUILD_TARGET_DIR, fname)
            if os.path.isfile(src_f):
                shutil.copy2(src_f, dst_f)
        print(f"[+] Assets synchronized to build directory: {BUILD_TARGET_DIR}")

    print("====================================================================")
    print(f"  IMPORT COMPLETE: {success_count}/{total_count} sounds imported & edited!")
    print("====================================================================")
    return 0 if success_count > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
