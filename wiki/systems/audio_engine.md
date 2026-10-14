# Audio Engine Architecture & Ear Safety Manual (`src/audio/`)

The **Voidfall Dredge** audio subsystem delivers low-latency procedural audio synthesis, 3D spatialization, and an automatic hearing protection mastering pipeline.

---

## 1. Core Principles & Zero-Allocation Synthesis

- **Real-Time Buffer Synthesis**: The audio engine synthesizes audio at 44.1 kHz, 16-bit stereo without disk asset dependencies.
- **Zero-Allocation Audio Thread**: Voice buffers and intermediate mix channels utilize pre-allocated static working arrays (`k_chunk_samples = 1024`). No dynamic allocations (`malloc`, `new`, `std::vector::push_back`) occur in the audio worker thread.
- **Headless / CI Compatibility**: In headless environments (CI containers or test instances without an active sound card), the engine operates in mock synthesis mode, guaranteeing all test suites and telemetry tools run smoothly.

---

## 2. Ear Safety Mastering Chain (Acoustic Comfort)

To protect the player's hearing from sharp clicks, digital overs, and ear-piercing high frequencies during high-intensity cavern collapse and combat sequences, all audio passes through a hardware-modeled 5-stage mastering chain before output:

```
[Raw Voice Mix]
       │
       ▼
1. DC-Blocker High-Pass Filter (10 Hz)
   - Eliminates DC offset bias that causes transducer clicks
       │
       ▼
2. Anti-Harshness Low-Pass Filter (6.5 kHz 2-pole)
   - Tames piercing high-frequency sibilance and harsh digital noise
       │
       ▼
3. Soft-Saturation Stage (tanh curve)
   - Provides analog warmth and natural compressive ceiling
       │
       ▼
4. Fast-Attack / Smooth-Release Peak Limiter
   - Clamps peaks strictly below -1.0 dBFS (threshold = 0.891)
       │
       ▼
5. Output Slew-Rate Limiter (Max delta = 0.15 / sample)
   - Eliminates sample-to-sample discontinuities that produce pops/clicks
       │
       ▼
[Audio Device Output]
```

### Safety Metrics & Standards
- **Peak Level**: Strictly clamped below **-0.70 dBFS** (never touches 0 dBFS hard clipping).
- **High-Frequency Energy Ratio**: Energy above 8 kHz is restricted to $< 10\%$ of total spectral energy.
- **Slew Rate**: Absolute per-sample step $\le 0.15$, ensuring smooth waveforms even during instantaneous weapon firing or explosive fractures.

---

## 3. Procedural Sound Palette & Gameplay Integration

| Category | Audio Event | Synthesis & Timbre Characteristics | Trigger Context |
|---|---|---|---|
| **Mining & Voxels** | `VOXEL_BREAK_BASALT` | Low-frequency dull impact (80-140 Hz) with stone crumble noise | Breaking basalt / rock voxels |
| | `VOXEL_BREAK_TITANIUM` | Resonant metallic ping (520 Hz) with high Q mechanical decay | Mining titanium alloy nodes |
| | `VOXEL_BREAK_VOIDITE` | Crystalline ethereal bell (880 Hz harmonic ring) | Extracting voidite crystals |
| | `VOXEL_BREAK_BULKHEAD` | Deep structural heavy metallic clank (120 Hz) | Dismantling deployed bulkheads |
| | `VOXEL_BREAK_RADIOACTIVE` | Unstable sizzle with sub-harmonic distortion | Mining irradiated cores |
| | `DRILL_LOOP` | Multi-sawtooth motor hum with progress-based pitch rise | Active excavation drill hold |
| **Enemy Cues** | `STALKER_CHITTER` | Multi-tap chitinous burst with mandible scrape clicks (3400 Hz) | Ambient prowl, stealth stalking, & burrowing chatter |
| | `MONSTER_DIGGING` | High-torque dual FM rotary grinding friction (135/210 Hz) + 1400 Hz fracture noise | Wall excavation, drilling, & burrowing escape through rock |
| | `STALKER_SNARL` | Aggressive sawtooth growl (160 Hz) with sub-bass distortion | Void Stalker lunge charge |
| | `STALKER_ATTACK_IMPACT` | Dull claw impact slap with suit damage thud | Melee impact on player |
| | `STALKER_STEP` | Quiet chitin claw tip tap (2400 Hz) + gentle stone grit brush (1800 Hz) | Void Stalker stealth prowl (quiet, -12.5 dBFS) |
| | `STALKER_RUN` | Rapid staccato multi-claw scamper (0ms, 42ms, 88ms taps) + 1600 Hz friction scrape | Void Stalker sprinting / stalking / lunging pursuit (0.22s cadence) |
| | `GOLIATH_STEP` | Sub-bass seismic foot stomp (52 Hz) + stone crumble crunch + resonant carapace plate buckle (165 Hz) | Chitin Goliath sentinel advance & juggernaut charge |
| | `BURROWER_STEP` | Subterranean serpentine slither (1050 Hz scale friction, 520 Hz basalt grit slide, 55 Hz body displacement glide) | Seismic Burrower serpentine crawl & charge (quiet, -11 dBFS) |
| **Combat & Weapon Reloads** | `PLASMA_CARBINE_RELOAD` | Multi-stage tactical reload: cell eject latch click & decompression hiss puff, battery rail slide, magnetic lock slam & electronic charge chirp | Vanguard Plasma Carbine reload (1.25s) |
| | `SCATTERGUN_RELOAD` | Heavy mechanical breacher reload: heavy steel breech unlock, rotating multi-tooth drum ratcheting, heavy steel drum slam shut, forepump rack | Demolitionist Magma Scattergun reload (1.60s) |
| | `RAILGUN_RELOAD` | Precision surgical electromagnetic reload: pneumatic needle rack release puff, linear magnetic needle rail slide, magnetic bolt clamp & inductive coil whine | Scout Needler Railgun reload (1.10s) |
| **Combat & Melee** | `MELEE_SWING_DRILL` | Heavy hydraulic piston surge & high-torque motor rev whoosh (120 -> 55 Hz) | Mining Drill rotary thrust strike |
| | `MELEE_SWING_SCATTERGUN` | Heavy steel stock bludgeon roundhouse & air turbulence heave (48 -> 24 Hz) | Magma Scattergun stock cleave |
| | `MELEE_SWING_CARBINE` | Crisp tactical butt-stroke whip (180 -> 60 Hz) + plasma capacitor zap | Plasma Carbine muzzle uppercut |
| | `MELEE_SWING_RAILGUN` | Supersonic bayonet lance thrust (480 -> 95 Hz) + 1650 Hz coil whistle | Needler Railgun spear thrust |
| | `MELEE_SWING_DETONATOR` | Heavy exo-gauntlet punch whoosh (95 -> 40 Hz) + clacker spring latch click | Demolition Charge punch strike |
| | `MELEE_HIT_FLESH` | Visceral chitin fracture crunch (2200 Hz down-sweep) + sub-bass body thump (55 Hz) | Melee strike impact on hostile entity |
| **Hazards & Environment**| `SEISMIC_RUMBLE` | 35-55 Hz sub-audible cavern tremor with brown noise | Structural instability & cave-ins |
| | `GEIGER_CLICK` | Poisson-distributed impulse clicks (strictly environmental proximity; zero artificial background ticking) | Entering proximity of radioactive mineral veins & anomalies |
| | `CAVERN_DRONE` | 3-oscillator detuned infrasonic bed + swept resonant void wind draft | Continuous expedition atmosphere |
| | `CAVERN_DRIP` | Subterranean moisture droplet chirp (1850 -> 1200 Hz) with 130ms cavern slapback | Procedural micro-ambience event |
| | `CAVERN_GROAN` | Deep tectonic rock stress shift (56 -> 42 Hz) with frequency-modulated tremolo | Procedural micro-ambience event |
| **Expedition & UI** | `BEACON_SIREN` | 440-660 Hz alternating warning tone | Holdout evacuation countdown |
| | `DROP_POD_LANDING` | Deep rocket thruster deceleration roar | Evac pod touchdown event |
| | `HEARTBEAT_ALARM` | Dual-pulse low-pass thumps ($S_1/S_2$ cardiac rhythm) | Health $< 25\%$ emergency warning |
| | `UI_CLICK` / `UI_BUY` | Clean softened sine blips (440 Hz / 880 Hz chord) | Terminal navigation and upgrades |

---

## 4. 3D Spatial Audio & Swarm Footstep Arbitration

The engine implements 3D binaural positioning:
$$\text{Pan} = \text{clamp}\left(\frac{\mathbf{v}_{\text{rel}} \cdot \mathbf{R}_{\text{listener}}}{\|\mathbf{v}_{\text{rel}}\|}, -1.0, 1.0\right)$$
$$\text{Attenuation} = \frac{1.0}{1.0 + 0.08 \cdot d + 0.015 \cdot d^2}$$

- **Left / Right Equal-Power Panning**:
  $$G_L = \sqrt{0.5 \times (1.0 - \text{Pan})}, \quad G_R = \sqrt{0.5 \times (1.0 + \text{Pan})}$$
- **Proximity Falloff**: 3D voices attenuate smoothly over distance, preventing jarring volume spikes when enemies approach.

### 4.1 Swarm Footstep Arbiter & Anti-Stuck Suppression
In high-enemy scenarios (swarms, ambushes, or tight corridors), enemy locomotion footsteps can quickly devolve into an abrasive wall of sound if left unthrottled. To guarantee balanced, punchy audio:
1. **Engine-Wide Footstep Voice Cap**:
   - `find_or_steal_voice_slot` enforces a maximum of **3 concurrent active footstep voices** across the entire engine. Any excess footsteps steal the oldest active footstep voice rather than multiplying sound pressure.
2. **Swarm Crowd Companding**:
   - When multiple enemies are walking/sprinting within hearing distance, `Application::update_audio_sources` aggregates step candidates and applies dynamic crowd volume companding:
     $$\text{Volume Compand} = \frac{1.0}{1.0 + 0.35 \times \max(0, N_{\text{walkers}} - 1)}$$
   - This keeps the collective acoustic energy of a dense pack at a comfortable, mastered ceiling without drowning out other cavern SFX or combat cues.
3. **Distance Prioritization & Rate Pacing**:
   - Step candidates are sorted by distance to the player. The closest threats are granted priority dispatch, while footsteps from enemies $> 28\,\text{m}$ away are aggressively culled.
   - A minimum spacing interval ($\Delta t \ge 65\,\text{ms}$) prevents multiple enemies from firing transient clicks on the exact same audio buffer frame.
4. **Stuck Enemy Footstep Suppression**:
   - If an enemy becomes physically wedged against cavern walls, solid voxels, or bulkheads (`stuck_timer > 0.15f` or `wall_stuck_timer > 0.15f`), its footstep timer is immediately clamped to zero. Footsteps are completely silenced until the creature regains free forward movement.
5. **Decoupled Ambient Radiation Clicks**:
   - Geiger clicking is strictly bound to real environmental radiation sources (`m_radiation_proximity > 0.0f`). Synthetic background clicks previously tied to global expedition phase timers have been eliminated to ensure pure ambient silence when delvers are outside hazardous mineral zones.

---

## 5. Automated Verification & Testing

### Running C++ Audio Test Suite (Silent Headless Mode by Default)
```bash
./build/Release/test_audio.exe
```
Or via the parallel TDD runner:
```bash
python scripts/tdd.py
```
> [!NOTE]
> **Developer Ear Protection**: `test_audio.exe` runs 100% silently by default. All 16 testing modules, ear-safety limiters, and DSP filters evaluate against in-memory PCM buffers without opening the physical sound card.
> - To opt-in to audible playback through your headset:
>   ```bash
>   ./build/Release/test_audio.exe --audible
>   ```
> - To launch the game client silently:
>   ```bash
>   ./build/Release/VoidfallDredge.exe --mute
>   # or:
>   ./build/Release/VoidfallDredge.exe --silent
>   ```
> - To enforce silent audio across the entire engine via environment variable:
>   ```bash
>   export VOIDFALL_MUTE_AUDIO=1
>   ```

### Running Ear Safety & Acoustic Analysis
```bash
python scripts/analyze_audio.py
```
This inspects exported waveform captures in `screenshots/audio_samples/` and validates:
1. True peak amplitudes $\le -0.70$ dBFS.
2. Zero digital clipping (contiguous clamped values = 0).
3. Smooth sample slew rate ($\Delta \le 0.15$).
4. Acoustic warmth ratio (high-frequency dominance verification).
5. Generates consolidated report at `screenshots/audio_safety_report.json`.

---

## 6. Sound Channels, Volume Options & Settings Persistence

Players can dynamically attenuate, turn down, or mute sound categories both during expeditions and inside the Orbital Hub.

### 6.1 Audio Buses & Sound Categories
The engine partitions all procedural synthesis into independent sound channels:

| Category | Channel Bus | Affected Sound Cues | Default Level |
|---|---|---|---|
| **Master** | Master Output | Multiplies total output across all active voices | `100%` (`1.0`) |
| **Music Soundtrack** | `SoundCategory::Music` | Extended instrumental sector themes, orbital hub soundtrack, arrival stingers | `100%` (`1.0`) |
| **SFX / Tools** | `SoundCategory::SFX` | Rotary drill loop, voxel break sounds, explosions, kinetic thrusters, footsteps | `100%` (`1.0`) |
| **Hostile Creatures** | `SoundCategory::Enemy` | Void Stalker alerts, predatory lunges, claw impacts, elimination dissolution | `100%` (`1.0`) |
| **Ambience & Hazards** | `SoundCategory::Ambience` | Cavern wind, seismic tremors, Geiger radiation clicks, evac beacon sirens | `90%` (`0.9`) |
| **UI & Alarms** | `SoundCategory::UI` | Terminal blips, upgrade chimes, low-health cardiovascular emergency alarms | `90%` (`0.9`) |

### 6.2 Zero-Overhead Mute & Turn-Down Invariants
- **Instant Mute All**: Toggling `mute_all` or setting `master_volume = 0.0` immediately short-circuits `render_mix()`, outputting pure digital silence (`0.0f`) with zero CPU synthesis overhead.
- **Granular Silence**: When any individual channel (e.g. `music_volume`, `sfx_volume`, `enemy_volume`) is adjusted down to `0.0`, voices on that bus advance their internal timeline without executing sample synthesis or writing energy to the master buffer, preventing memory leaks and orphaned voices.

### 6.3 In-Game & Hub UI Controls
1. **In-Game Pause Menu (`ESC`)**:
   - **Quick Access**: Master volume steppers (`[-]` / `[+]`) and quick `[MUTE]` button on the primary Mission tab, plus an `[OPEN ADVANCED AUDIO MIXER]` jump button.
   - **Advanced Audio Tab**: Dedicated multi-channel mixer with 10% increment steppers, visual VU-style level bars, Master Mute toggle, and an instant `[* PREVIEW AUDIO LEVEL *]` test sound trigger.
2. **Main Menu / Orbital Hub**:
   - `[ AUDIO & RIG SETTINGS ]` button on the main terminal screen opens the full sound and hardware options panel before deploying into an expedition.
3. **Session Persistence**:
   - All audio levels and mute preferences are encapsulated in `GameSettings` (`src/core/settings.hpp`) within `UserProfile` (`src/core/save_system.hpp`).
   - Saved and loaded automatically from `saves/save_data.json` across game sessions.

---

## 7. Extended Instrumental Music Tracks & Non-Repetitive Playback

To prevent auditory fatigue from short, repetitive loops, the ambient music loop architecture integrates extended 2-to-3.5-minute instrumental compositions with zero runtime memory allocations:

### 7.1 Track Roster & Biome Mappings
| Sound Cue | Biome / Game State | Track Title | Artist & License | Duration | File Format |
|---|---|---|---|---|---|
| `AmbientCavern` | Orbital Hub & Station | *Phantom from Space* | Kevin MacLeod (CC-BY 4.0) | 2m 36s (156.4s) | MP3 / 44.1kHz |
| `AmbientSector1` | Sector 1: Perimeter Drift | *Blue Sizzle* | Kevin MacLeod (CC-BY 4.0) | 2m 26s (146.2s) | MP3 / 44.1kHz |
| `AmbientSector2` | Sector 2: Volatile Fault | *Deep Haze* | Kevin MacLeod (CC-BY 4.0) | 2m 02s (122.1s) | MP3 / 44.1kHz |
| `AmbientSector3` | Sector 3: Void Cradle | *Aftermath* | Kevin MacLeod (CC-BY 4.0) | 3m 30s (210.5s) | MP3 / 44.1kHz |

### 7.2 Native MP3 Decoding Engine (`dr_mp3.h`)
- Integrated via single-header `dr_mp3.h` (public domain / MIT-0 by David Reid).
- Decodes MP3 audio streams directly into standard 32-bit float stereo PCM buffers during startup (`AudioEngine::load_sound_samples`).
- Supports automatic fallback to `.wav` files and procedural synthesis if MP3 assets are omitted or missing.
- Respects full loaded duration for ambient music cues without premature truncation.

### 7.3 Automated Waveform Variance Verification
- **Non-Repetitive Assertion**: Test 17 in `test_audio.exe` verifies that consecutive 4-second audio windows sampled from the same track yield a normalized mean-squared difference $> 0.05$ (actual measured: $> 0.89$), proving rich compositional variance rather than monotonous short looping.


