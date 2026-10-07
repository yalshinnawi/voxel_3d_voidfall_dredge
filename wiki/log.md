# Voidfall Dredge - Game Design Log

Chronological ledger of balance iterations, mechanics additions, and architectural decisions.

## 2026-10-07
- **Late-Round Performance Optimization, Spiral-of-Death Guard & Anti-Clipping Stabilization**:
  - **Identified Late-Round Bottlenecks & Failure Modes**:
    - *Phantom Chunk Generation*: `World::update(viewer_pos, 2)` queried chunks beyond level bounds without clamping, spawning unneeded 32x32x32 solid bedrock chunks on the main thread and bloating memory.
    - *Overdraw & Un-culled Rendering*: All chunks were rendered every frame regardless of view angle, including chunks 180 degrees behind the camera and fully hollow/empty chunks with zero vertices.
    - *Simulation Spiral-of-Death*: Variable delta-time accumulator loop had no execution cap, accumulating up to 15 fixed ticks during momentary frame drops and causing recursive stalls.
    - *Enemy Alert/Combat State Clipping*: Void Stalker visual range inverted from 20m in `Investigating` to 12m in `Stalking`, causing creatures at ~15m to immediately lose LOS on transition. Un-reset `lost_los_timer` caused instant fallback.
    - *Audio Starvation & Dropout*: Windows `waveOut` audio worker thread only replenished buffers on `WAIT_OBJECT_0`, leaving audio dead upon momentary buffer underruns. Low 32-voice capacity caused high-frequency Geiger clicks to evict combat sounds mid-sample.
  - **Engine Performance & Render Pipeline Hardening**:
    - **Bounded World Streaming**: Restricted `World::update()` iteration strictly to Level Generator dimensions `[0..max_cx-1, 0..max_cy-1, 0..max_cz-1]`, preventing phantom chunk leaks.
    - **Gribb-Hartmann Frustum Culling**: Extracted 6 view-frustum planes in `Renderer::begin_frame()`. Filtered chunk bounding boxes and dynamic debris spheres, skipping draw calls and uniform bindings for off-screen and empty chunks.
    - **Simulation Death-Spiral Protection**: Clamped fixed ticks per frame to `MAX_FIXED_TICKS_PER_FRAME = 4`, clearing excess accumulator backlog to ensure immediate recovery from frame spikes.
  - **Enemy AI Perception Envelope & State Hysteresis**:
    - **Unified Perception Range**: Standardized detection range to 20m across `Investigating`, `Stalking`, `Circling`, and `Lunging` states.
    - **State Commitment & Timer Reset**: Reset `lost_los_timer = 0.0f` on entry into combat states and required `state_timer >= 1.0f` before allowing state demotion, eliminating rapid flip-flopping.
  - **Audio Engine Anti-Clipping & Priority Allocation**:
    - **Self-Healing Buffer Replenishment**: Expanded buffer pool to 8 buffers (185ms cushion) and refilled completed headers on both event signals and 20ms timeouts, auto-recovering from underruns.
    - **64-Voice Capacity & Priority Scheduling**: Increased voice limit to 64 and introduced 4-tier priority eviction (`get_cue_priority`), safeguarding monster roars, weapon discharges, and arrival stingers against low-priority ambient and Geiger clicks.
  - **Automated Verification & Telemetry**:
    - Created dedicated test suite `tests/test_load_and_stress.cpp` (`LoadAndStressTests`) validating 5 load modules.
    - All 11 parallel test suites pass cleanly in ~1.05s via `python scripts/tdd.py`.
    - Automated visual test (`--auto-play-test`) confirmed 10/10 phase pass with 0 errors in `voidfall.log`.

## 2026-10-06
- **3D Weapon Scope Optical Lenses & Crouch-Zoom Alignment Integration**:
  - **Identified Immersion & ADS Visibility Bottleneck**:
    - Firearms lacked see-through optical scopes on the 3D viewmodels: solid meshes and unblended render passes blocked the center of the screen when zooming in.
    - When crouching, viewmodel hipfire posture offsets (-0.08m Y, -0.05m Z) remained active during ADS zoom, pulling the sight line down and misaligning the optic from the player's eye level.
  - **Hollow Tubular Scopes & Multi-Coated Optical Glass Lenses**:
    - Added `ViewModel::add_tube` and `ViewModel::add_lens_disc` procedural geometry generators to construct true hollow bores and two-sided optical glass discs.
    - **Vanguard Plasma Carbine**: Replaced solid reflex sight with an open, hollow protective hood, rubberized lens gasket, transparent polarized cyan anti-glare glass pane (28% alpha), and floating illuminated holographic reticle dot/gate brackets.
    - **Scout Needler Railgun**: Built elevated high-precision marksman sniper scope with hollow bore tube, knurled diopter focus ring on ocular bell, objective bell hood, elevation/windage turrets, dual emerald optical glass lenses, and etched illuminated hairline mil-dot reticle. Removed viewmodel disappearance on full zoom.
    - **Demolitionist Magma Scattergun**: Integrated heavy ruggedized tubular combat thermal scope with cast-iron cantilever clamp, knurled heat rim, transparent amber thermal quartz lens (28% alpha), and glowing thermite ghost-ring reticle.
  - **True Optical Transparency & Blending**:
    - Enabled `GL_BLEND` (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`) within `ViewModel::render` so transparent lens elements render seamlessly over the 3D cavern scene without clipping or occluding background geometry.
  - **Seamless Crouch-Zoom Alignment**:
    - Dynamically blended crouch offsets (`effective_crouch_y = glm::mix(crouch_offset_y, 0.0f, zoom_progress)`) so the weapon optic smoothly rises directly to eye level ($y=\text{target\_gun\_y}, x=0$) whether the delver is standing or crouching.
    - Updated HUD laser rangefinder raycasting in `src/ui/hud.cpp` to use `player.eye_position()`, guaranteeing 100% elevation accuracy in both stances.
  - **Automated Verification & Testing**:
    - All 10 parallel test suites pass cleanly in ~1.1s via `python scripts/tdd.py`.
    - Extended `WeaponZoomAndFireMechanicsTest` in `tests/test_gameplay_mechanics.cpp` to validate simultaneous crouched zooming and firing across all 3 weapon archetypes.
    - Executed `./build/Release/VoidfallDredge.exe --auto-play-test` and confirmed all 10 visual phases pass with 100% subsystem health.

- **Colossal Vertical Chambers & Procedural Level Diversity Overhaul**:
  - **Player Experience & Spatial Scale Objectives**:
    - Addressed feedback requesting dramatic vertical scale where ceiling and ground are vastly separated, giving delvers the sensation of traversing titanic, atmospheric cavern expanses.
    - Expanded procedural shape repertoire from 23 to 27 chamber archetypes with soaring vertical clearances (up to 21 meters, $y=4 \to 25$).
  - **4 Colossal Vertical Archetypes Implemented**:
    1. **Colossal Vaulted Dredge Cathedral** (`RoomShapeType::ColossalVaultedDredgeCathedral`):
       - Towering cathedral cavern with ribbed gothic arches soaring 21m above delvers.
       - Elevated observation gantry and titanium suspension catwalk at $y=19$ spanning East-West with safe $3\text{m}$ clear deck and perimeter safety railings.
       - Central ceremonial Voidite dais altar and hanging stalactite chandeliers.
    2. **Tectonic Abyssal Sinkhole** (`RoomShapeType::TectonicAbyssalSinkhole`):
       - Multi-tiered subterranean sinkhole plunging directly to bedrock ($y=4$).
       - Glowing molten magma fissure trough running along the central trough bed.
       - Descending stepped spiral terraces at $y=18, 12, 8$, central stepping spire monolith lookout at $y=14$, and high-tension suspension cable bridge at $y=18$.
    3. **Cyclopean Excavation Silo** (`RoomShapeType::CyclopeanExcavationSilo`):
       - Titanic circular industrial precursor excavation shaft.
       - Heavy titanium dredging hopper basin at $y=4$, double-tiered maintenance ring catwalks at $y=12$ and $y=20$, vertical guide conduit columns, and overhead heavy crane girder at $y=23$.
    4. **Bioluminescent Firmament Abyss** (`RoomShapeType::BioluminescentFirmamentAbyss`):
       - Vast subterranean celestial vault with reflecting crystal aquifer pools at $y=4$.
       - Vault ceiling canopy at $y=23..25$ embedded with hundreds of emissive starlight crystals (`MAT_PRISMATIC_CRYSTAL`, `MAT_VOIDITE_CRYSTAL`).
       - Diagonal soaring natural stone arch bridge at $y=19$ for aerial panoramic traversal.
  - **Procedural Level Variety & Verticality**:
    - **High-Vault Atrium System**: Regular Floor 0 chambers roll a 40% chance of a high-vault ceiling ($y=20..24$) expanding verticality even across ordinary exploratory routes.
    - **Non-Square Organic Dimensions**: Chamber widths and depths randomly vary across non-square aspect ratios ($15\times 17$, $17\times 19$, $19\times 17$).
    - **Doorway Clearance Guarantees**: Enforced strict `is_doorway_floor` and `is_doorway_air` checks across all chamber geometries, guaranteeing 100% unobstructed transitions through inter-chamber corridors.
    - **Thematic Luminaries**: Added `GrandVaultRadiance`, `FirmamentStarlight`, and `CyclopeanFloodlight` cavern light beacons.
  - **Verification & Testing**:
    - All 10 parallel test suites pass cleanly via `python scripts/tdd.py`.
    - Executed 10-phase visual test harness (`./build/Release/VoidfallDredge.exe --auto-play-test --mute`) and verified 10/10 phase pass rate via `python scripts/analyze_screenshots.py`.

- **Sector 2 Difficulty & Encounter Pacing Rebalance**:
  - **Identified Pacing & Difficulty Bottlenecks**:
    - Players faced sudden overwhelming hostile difficulty upon reaching Sector 2 (Volatile Fault). Root causes identified:
      1. Uncapped initial fauna: Expeditions spawned ambient stalkers in every room on start without taking player progress or sector limits into account, leading to 28–30+ active hostiles in the dungeon.
      2. Cascade aggro: Stalker alert screams propagated up to 20m through solid rock, instantly alerting adjacent packs across multiple chambers.
      3. Zero grace period: Noise meter bursts instantly triggered hostile swarm waves in Sector 2 even during the first 30 seconds of an expedition.
      4. Impossible holdouts: Sector 2 extraction holdout immediately spawned 120 HP Seismic Burrowers while swarming players with multiple stalkers, making evacuation near impossible.
      5. Softlocked extraction quotas: Sector 2 extraction required securing the Precursor Relic, forcing players to play Demolitionist to breach vault bulkheads or fail extraction.
  - **Balance Overhaul & Pacing Tuning**:
    - **Initial Fauna & Active Monster Caps**:
      - Removed redundant room-by-room fauna spawner at expedition launch. Initial fauna is now cleanly managed by `SpawnManager` with safe spawn distances ($\ge 18\text{m}$ away from drop pod).
      - Enforced strict active enemy caps per sector in [src/ai/spawn_manager.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ai/spawn_manager.cpp): Sector 1 capped at 1–2 hostiles; Sector 2 clamped to 2 (early) and 4 (late); Sector 3 clamped to 3 (early) and 6 (late).
      - Added guaranteed opening grace periods (35s in Sector 2, 25s in Sector 3) during which random wave spawns are suppressed so delvers can scout and orient themselves.
      - Noise meter swarm waves are clamped against maximum allowed enemy limits.
    - **Pack Alert Propagation**:
      - Reduced Stalker alert shout radius from 20m down to 10m in [src/entities/enemies/void_stalker.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/enemies/void_stalker.cpp), preventing cascading whole-dungeon chain pulls through cavern walls.
    - **Player Survivability — Suit Nanite Field Stabilizer**:
      - Added an out-of-combat health regeneration mechanic to [src/player/controller.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.cpp): when out of combat for $\ge 12\,\text{s}$, suit nanites slowly restore health up to $75\%$ max HP ($+3.5\,\text{HP/s}$), providing delvers a recovery window between intense engagements.
    - **Sector 2 Extraction Quota & Holdout Balancing**:
      - Rebalanced Sector 2 extraction quota: players can extract by gathering 30 Voidite OR securing the Precursor Relic, removing the vault softlock for non-Demolitionist classes.
      - Tuned Sector 2 beacon holdout: delayed 120 HP Seismic Burrower spawns during extraction holdouts to Sector 3. Sector 2 holdout now features manageable waves of 1–2 stalkers with defensive perimeter warnings.
    - **Level Generation Doorway Fixes**:
      - Fixed `SubterraneanAquiferOasis` in [src/voxel/level_shapes.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.cpp) to prevent prismatic crystals from generating within doorway boundaries (`is_doorway_air`), eliminating doorway obstruction issues.
  - **Verification**:
    - All 10 parallel test suites pass with 100% success rate via `python scripts/tdd.py`.
    - Executed 10-phase automated visual test harness (`./build/Release/VoidfallDredge.exe --auto-play-test --mute`) and verified clean execution in `voidfall.log` and `screenshots/00_all_phases_montage.png`.

- **Low-Health Biometric Audio Comfort (Constant Wash Noise Elimination)**:
  - **Identified Root Cause**:
    - Under low health ($< 35\%$), `AudioEngine::update_biometrics()` scaled `m_biometric_stress`. In [src/audio/audio_engine.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/audio/audio_engine.cpp), the real-time biometric synthesis loop generated continuous white noise via `fast_rand(m_breath_seed)` filtered at $650\,\text{Hz}$ with a raised cosine respiration envelope (`0.5 * (1.0 - cos(...))`).
    - Because the envelope was non-zero throughout the entire breathing cycle and fed directly into the stereo output channels without rest, it produced a constant, undulating ocean/static "wash noise" in the player's headset. This caused auditory fatigue and masked crucial environmental cues such as enemy footsteps and stalker chitters.
  - **Fixes & Acoustic Polish**:
    - Removed the synthetic white-noise respiration wash generator from [src/audio/audio_engine.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/audio/audio_engine.cpp).
    - Preserved the clean, visceral, sub-bass cardiac "lub-dub" heartbeat pulse ($46\,\text{Hz} / 92\,\text{Hz}$ systolic, $58\,\text{Hz} / 116\,\text{Hz}$ diastolic) that scales dynamically from $68$ to $150\,\text{BPM}$ with stress.
    - Preserved complete acoustic transparency and silence during the inter-beat rest interval ($\sim 64\%$ of each cardiac cycle), restoring clear situational awareness while maintaining dramatic low-health tension.
    - Removed unused breath synthesis fields from [src/audio/audio_engine.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/audio/audio_engine.hpp).
    - Added automated verification in [tests/test_audio.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_audio.cpp) (Test 25) validating that low-health biometrics produce clean rhythmic cardiac pulses with $> 300\,\text{ms}$ of clean inter-beat silence and zero continuous noise wash floor.
- **Enemy Death Visual Artifact Elimination (White Wash-Out Fix)**:
  - **Identified Root Causes**:
    - When taking fatal damage, hostile entities were assigned `hit_flash_timer = 0.08f` and transitioned into `StalkerState::Dying`. However, because the main enemy update loop skipped general timer decays when in `Dying` state, `hit_flash_timer` remained frozen $\ge 0.08\,\text{s}$ for the entire collapse animation ($0.75\,\text{s}$).
    - The PBR stalker renderer mixed 92% pure white into all carapace, spine, and claw vertices and added $+8.5$ to emissive whenever `hit_flash_timer > 0.0f`.
    - In [assets/shaders/stalker.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/stalker.frag), `emissiveMultiplier` was scaling linearly with elapsed total game time (`1.0 + uStateGlow * 1.5`), magnifying the non-decaying emissive boost into an extreme blown-out white silhouette.
  - **Fixes & Visual Polish**:
    - Fatal damage across firearms, melee shove, explosive biogas ignition, and repulsor pulses now clears `hit_flash_timer = 0.0f` immediately on transition to `Dying`.
    - `Renderer::render_stalkers()` explicitly gates hit-flash rendering to active combatants (`s.hit_flash_timer > 0.0f && s.state != StalkerState::Dying`).
    - Added graceful death visual decay: as a dying stalker collapses, its compound eyes and void core smoothly fade out to dark (`life_fade`), and its chitinous carapace settles into dead carcass tones.
    - Updated [assets/shaders/stalker.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/stalker.frag) to extinguish predatory rim light and emissive in `Dying` and `Dead` states (`uState == 8 || uState == 9`), and bounded emissive multiplier.
    - Added regression tests in [tests/test_unit_all.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_unit_all.cpp) and [tests/test_gameplay_mechanics.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_gameplay_mechanics.cpp) enforcing `hit_flash_timer == 0.0f` on lethal blow and during dying collapse.
- **Interactive Precursor Vault Relic Pickup ("[E] To Retrieve")**:
  - **Eliminated Auto-Pickup on Walkover**: Walking near the pedestal inside a breached precursor vault no longer vacuums or automatically collects the relic. The player must actively interact with the relic on the pedestal.
  - **Proximity & Crosshair Interaction Query**:
    - Added `MissionSystem::can_interact_relic(player_pos, max_dist)` and `MissionSystem::interact_relic(world, player_pos, max_dist)`.
    - Added `PlayerController::set_on_interact()` callback with key press edge detection on `GLFW_KEY_E`.
    - Implemented `Application::try_interact()` which verifies proximity ($\le 3.8\,\text{m}$) or direct crosshair raycast targeting of the relic block before retrieving it, awarding inventory relic status, $+250$ run points, $+50$ Demolitions XP, synchronized audio cues, and visual spark particles.
    - Updated `Application::on_block_broken()` so destroying the entrance door column awards bulkhead breach score rather than pre-emptively granting the relic.
  - **HUD Interactive Prompts & Compass Waypoint Labeling**:
    - Added dynamic center crosshair prompt `"[E] RETRIEVE PRECURSOR RELIC"` in `Typography::COLOR_CYAN` when aiming at or standing within interaction reach of the relic pedestal (active across both mining drills and combat firearms).
    - Updated 3D-to-2D waypoint diamond HUD label from `[INTERACT TO SECURE]` to `[PRESS E TO RETRIEVE]`.
    - Updated controls binding references in both the in-game Pause Menu and Contractor Field Manual ([H] briefing) to document `[E] Interact / Retrieve Relic / Reel Grapple`.
- **Jetpack Audio Ear-Safety & Harshness Reduction**:
  - **Further Playback Gain Toning**:
    - Reduced `SoundCue::JetpackLoop` voice playback volume from $0.45$ down to $0.28$ in [src/audio/audio_engine.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/audio/audio_engine.cpp), preventing acoustic fatigue during prolonged hovering.
  - **Gentle Acoustic Frequency Shaping**:
    - Retuned low-pass filter cutoff on exhaust hiss from $1050\,\text{Hz}$ down to $800\,\text{Hz}$ to eliminate sizzling high-frequency noise.
    - Softened combustion sub-bass harmonics ($54\,\text{Hz} / 108\,\text{Hz}$), smoothed flutter modulation to $22\,\text{Hz}$, and trimmed mix weights to provide a warm, comfortable, non-intrusive rocket rumble.
- **Screen Motion Stabilization & Vibration Comfort Tuning**:
  - **Wavy Screen Movements & Post-Process Distortion Elimination**:
    - Removed high-frequency horizontal UV sine wave distortion (`sin(uv.y * 120.0 + uTime * 40.0)`) from [assets/shaders/postprocess.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/postprocess.frag). Radiation is now represented cleanly and comfortably via the suit HUD's ionizing vignette and geiger audio cues without nausea-inducing screen warping.
    - Stabilized dynamic subterranean ambient lighting in [assets/shaders/voxel_pbr.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/voxel_pbr.frag), removing undulating spatial coordinate waves (`sin(uTime * 0.45 + vWorldPos.x * 0.08 + vWorldPos.z * 0.08)`) and high-frequency sector shimmers that washed moving light/dark ripples across cavern voxels.
  - **Camera Trauma & Vibration Overhaul**:
    - Replaced harsh 60 Hz white-noise `rand() % 100` camera jitter and disorienting camera roll (previously rotating the horizon by up to $5\times$ shake amount) with smooth harmonic oscillation and zero roll in [src/core/application.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/application.cpp).
    - Reduced peak shake translation amplitude from $0.18\,\text{m}$ to $0.045\,\text{m}$, and increased impulse decay from $0.8\,\text{s}^{-1}$ to $2.8\,\text{s}^{-1}$ so impacts settle cleanly and crisply without buzzing.
    - Scaled camera shake by a dedicated user comfort setting `m_settings.screen_shake`.
  - **Continuous Routine Micro-Trauma Elimination**:
    - Removed continuous camera trauma vibration during routine gameplay:
      - Mining drill contact: removed continuous `add_trauma(0.015f * dt)` and shatter trauma in [src/player/controller.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.cpp).
      - Normal jumping & landing: eliminated camera trauma on non-damaging drops ($< 13\,\text{m/s}$); heavy fall damage impacts ($\ge 13\,\text{m/s}$) retain punchy physical feedback.
      - Ledge mantling & vaulting: eliminated camera trauma when pulling up onto ledges.
      - Jetpack air-braking, toxic gas traversal, and close-quarters bullet impacts on cavern walls no longer vibrate the camera.
      - Ambient burrower subterranean rumble no longer clamps continuous camera trauma.
  - **Viewmodel Motion Smoothing**:
    - Reduced locomotion walk bobbing amplitude by $50\%$ in [src/graphics/viewmodel.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/viewmodel.cpp).
    - Replaced aggressive $45\,\text{Hz}$ drill vibration oscillation and random vertex jitter with a subtle, smooth $24\,\text{Hz}$ mechanical hum.
  - **Configurable Comfort Settings & Persistence**:
    - Added `screen_shake` ($0.0\times$ to $1.0\times$, defaulting to comfortable $0.20\times$) in [src/core/settings.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/settings.hpp) and saved/loaded in [src/core/save_system.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/save_system.cpp).
    - Added an interactive `SCREEN SHAKE` stepper in both the in-game Pause Menu ([src/ui/pause_menu.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/pause_menu.cpp)) and Orbital Hub ([src/ui/orbital_hub.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/orbital_hub.cpp)), allowing players to freely tone down or completely turn off ($0\%$) screen shake.
- **Jetpack Thruster Audio Ear-Comfort Balancing**:
  - **Playback Gain Toning**:
    - Reduced `SoundCue::JetpackLoop` voice playback volume from $0.72$ down to $0.45$ (~$38\%$ reduction in amplitude, $-4.1\,\text{dB}$) in [src/audio/audio_engine.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/audio/audio_engine.cpp). This prevents continuous hovering from overpowering environmental acoustics or causing listening fatigue during long ascents.
  - **Procedural Synthesis Softening**:
    - Retuned high-frequency exhaust hiss filter: lowered low-pass cutoff frequency from $1450\,\text{Hz}$ to $1050\,\text{Hz}$ to eliminate abrasive white noise sizzle in the ear's most sensitive frequency range.
    - Reduced exhaust hiss component mix weight from $0.38$ to $0.26$, and air turbulence rumble to $0.20$ (with $200\,\text{Hz}$ LP filter), re-centering the sound profile around deep, warm $58\,\text{Hz}$ combustion rumble and $24\,\text{Hz}$ flame flutter without harsh ear-fatiguing artifacts.
- **Subterranean Liquid Dynamics: True Fluid Permeability, Buoyant Viscous Drag & Slow Sinking Physics**:
  - **Voxel Permeability & Mesher Occlusion**:
    - `Voxel::is_solid()` updated to treat liquids (`MAT_THERMITE_SLAG` / `MAT_MOLTEN_MAGMA` / `MAT_LAVA` and `MAT_CRYSTAL_AQUIFER` / `MAT_WATER`) as non-solid permeable volumes that entities and delvers can enter and sink through.
    - Added `Voxel::is_liquid()` and `Voxel::is_renderable()` helpers.
    - Updated `GreedyMesher` face occlusion rules: liquid blocks occlude neighboring liquid faces of the identical fluid type as well as solid terrain, while retaining visible boundaries at air-liquid and liquid-solid interfaces. Chunk meshing and empty-chunk early-outs now check `is_renderable()` so liquid pools remain fully meshed and visible.
  - **Fluid Dynamics & Slow Sinking Physics**:
    - Delver AABB spatial query checks intersecting voxels and immediate underfoot fluid contact, tracking `m_is_in_liquid`, `m_is_in_lava`, `m_is_in_water`, and fractional immersion ratio `m_liquid_submersion`.
    - **Dense Molten Slag / Lava**: Plunge braking rapidly decelerates high fall velocities; buoyant fluid resistance reduces downward sinking acceleration to a slow, viscous terminal crawl (capped at $-1.5\,\text{m/s}$). Severe horizontal drag ($8.5\times$) dampens lateral movement. Heat accumulation and thermal burning apply continuously while submerged.
    - **Subterranean Crystal Aquifer / Coolant Water**: Buoyant fluid drag reduces sinking velocity to $-3.0\,\text{m/s}$ with moderate lateral drag ($2.8\times$), rapidly dissipates exo-suit heat, and clears overheat lockouts.
    - **Swimming / Upward Paddling**: Holding [Space] / `BTN_JUMP` while submerged allows delvers to struggle upward through fluid ($+14.0\,\text{m/s}^2$ in molten rock up to $2.0\,\text{m/s}$; $+20.0\,\text{m/s}^2$ in clear water up to $3.8\,\text{m/s}$), assisted by thruster power bursts if fuel is available.
    - **Kinetic Impact Cushioning**: Entering fluid cushions kinetic fall impacts by $75\%$ (`impact_speed *= 0.25f`), preventing catastrophic blunt trauma on deep pool dives.
- **Tactical Hologram Map Overhaul: Focused Controls, Click-to-Drag Panning, Opacity Overhaul & Extraction Gating**:
  - **Focused Map Controls & Click-to-Drag Panning**:
    - When the delver opens the holographic tactical map via [Tab] or [M], player camera look, movement, and combat tool actions are completely paused (`m_player->set_combat_inputs_paused(true)`).
    - Player ground velocity is zeroed (`m_velocity.x = 0; m_velocity.z = 0`) to immediately prevent drift or sliding during map inspection.
    - System cursor unlocks (`m_window->set_cursor_locked(false)`) allowing free pointer movement across UI elements.
    - Replaced disorienting free-mouse map movement with smooth **Click-to-Drag panning**: map panning (`m_pan_x`, `m_pan_y`) only activates while holding the Left Mouse Button (LMB), clamped to the cavern boundary (`MAP_SPAN * 0.85f`).
    - Added mouse scroll wheel zoom support (`m_terrain_scanner.zoom()`) with smooth zoom clamping between `[3.5f, 22.0f]`.
    - Pressing [Tab], [M], or [Escape] closes the map modal, restores combat controls, and re-locks the mouse cursor.
  - **Map Legibility & Opacity Overhaul**:
    - Introduced a full-screen dark atmospheric scrim `glm::vec4(0.012f, 0.016f, 0.024f, 0.92f)` behind the map modal to mute distracting background 3D cavern rendering.
    - Converted map frame backdrops, canvas viewports, header bars, and sidebar cards from semi-transparent layers to solid 100% opacities (`1.0f`) with high-contrast borders (`BORDER_CYAN` and `BORDER_DARK`), completely eliminating see-through bleed.
    - Implemented strict 2D geometry clipping (`add_clipped_quad`) to keep cavern floors and fog-of-war quads firmly inside the map canvas viewport.
  - **Extraction Map & Compass Label Gating**:
    - Extraction beacon diamond markers, distance badge (`EXTRACTION [X m]`), pulsating radar ring waves, and off-screen directional arrows (`>> EXTRACT`) are now gated strictly behind active extraction phases (`ExtractionPhase::BeaconDeployed` or `ExtractionPhase::PodLanded`).
    - The player-to-extraction waypoint route line is hidden while extraction is dormant (preventing erroneous routes to `(0, 0, 0)`).
    - Sidebar cards dynamically display `MISSION DIRECTIVE // SURVEY & MINE CAVERN` and `EVACUATION STATUS [STANDBY] - STATUS: STANDBY (NOT DEPLOYED)` until the extraction drill beacon is planted.
    - HUD compass ribbon extraction blip is similarly gated to active extraction phases.
- **Enhanced Enemy Acoustic Perception, Immediate Awakenings & Retaliation When Shot**:
  - **Acoustic Perception Tuning**:
    - Expanded audible ranges across player actions: sprinting footsteps (20m, was 14m), walking footsteps (10m, was 6m), jump landings (10-24m), industrial drill grinding (24m, was 18m), voxel rock fractures (20-32m), gunshot blasts (42m Scattergun / 50m Carbine / 58m Railgun), bullet impacts (16m), and reload clacks (12m).
    - Subterranean rock acoustic vibration transmission factor improved from 0.65x to 0.80x.
    - Added priority salience multipliers: Gunshots (3.5x), Drilling (2.5x), Bullet Impacts (2.2x), and Footsteps (1.5x-2.0x).
  - **Sluggish Delay Elimination & Immediate Awakenings**:
    - Removed 8.0s dormancy delay in `StalkerState::Roosting`: roosting stalkers now awaken immediately upon hearing gunfire or taking damage, detaching from ceilings/walls to the floor.
    - Dormant `SeismicBurrower` instances now wake up immediately upon acoustic sound events (`b.has_sound_target`) or weapon impacts.
  - **Immediate Retaliation When Shot**:
    - Updated `damage_nearest` across Void Stalkers and Seismic Burrowers to receive true `attacker_pos` and `shot_direction`.
    - Applied bullet travel knockback along the shot vector.
    - Target enemy immediately snaps orientation toward shooter and triggers relentless retaliation pursuit.
    - Melee stalkers within 9m trigger an enraged counter-lunge; distant stalkers sprint charge; shooter stalkers return spine volley fire; seismic burrowers lock yaw/pitch toward the shooter and tunnel aggressively.
    - Pack alert: damaging an enemy alerts all nearby squad members within a 20m radius.
  - **Testing & Verification**:
    - Added Test 19 and Test 20 to [test_enemy_stalker.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_enemy_stalker.cpp).
    - Added Test 10 to [test_enemy_burrower.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_enemy_burrower.cpp).
    - Verified all 10 engine test suites pass in parallel via `python scripts/tdd.py`.
- **Fix Floating White Droplets / In-World 3D Breadcrumb Box Elimination**:
  - Identified the root cause of floating white/cyan droplets/slabs: [Renderer::render_breadcrumbs](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp#L1114) was rendering 3D physical box meshes (`0.12 x 0.02 x 0.12`) into the first-person game world at every recorded player breadcrumb position.
  - Whenever the player jumped, used vertical thrusters, grappled, or traversed varied terrain elevations, recorded breadcrumbs suspended in mid-air and rendered as floating emissive white/cyan boxes/droplets.
  - Removed in-world 3D breadcrumb rendering completely while preserving visited trail path visualization on the 2D Hologram Terrain Scanner ([TAB] map overlay via [TerrainScanner](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/terrain_scanner.cpp#L544)).
  - Verified through automated playthrough, unit test suites, and screenshot contact sheet inspection.
- **Firearm Aim-Down-Sights (ADS) Zoom Mechanics with Right-Click**:
  - Implemented hold-to-zoom Aim-Down-Sights (ADS) on right-click for all delver combat firearms ([ToolSlot::CombatWeapon](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/loadout.hpp#L11)).
  - Parameterized archetype-specific zoom magnification and transition rates in [WeaponStats](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/loadout.hpp#L23):
    - **Demolitionist Magma Scattergun**: 0.78x FOV multiplier (~1.28x zoom), 0.20s ADS time.
    - **Vanguard Plasma Carbine**: 0.65x FOV multiplier (~1.54x zoom), 0.18s ADS time.
    - **Scout Needler Railgun**: 0.40x FOV multiplier (~2.50x sniper marksman scope), 0.22s ADS time.
  - Enabled continuous shooting while zooming: firing works seamlessly during ADS, providing 50% tighter spread cone accuracy and centered muzzle raycast convergence.
  - Scaled mouse look sensitivity proportionally with FOV zoom ratio for smooth, precise aiming.
  - Aligned viewmodel procedural transforms in ADS: weapon models shift from hipfire `(0.15, -0.14, -0.34)` to centered sightline `(0.0, -0.10, -0.30)`, with reduced weapon sway and stabilized bobbing.
  - Enhanced tactical HUD reticle: reticle brackets tighten dynamically with zoom progress, the Needler Railgun displays precision sniper scope lines with optical mil-dots when scoped, and the ammo indicator renders real-time magnification badges (`[1.5X]`, `[2.5X]`).
  - Added comprehensive automated test suite `WeaponZoomAndFireMechanicsTest` in [test_gameplay_mechanics.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_gameplay_mechanics.cpp) validating all 3 weapon archetypes, zoom transitions, tool switching isolation, and firing while zoomed.
- **Weapon Reload Enforcement & Elimination of Passive Ammo Regeneration**:
  - Disabled passive trickle recharge across all weapons (`WeaponStats::auto_recharge = false` strictly enforced for Demolitionist Magma Scattergun, Vanguard Plasma Carbine, and Scout Needler Railgun).
  - Removed capacitor passive recharge tick loop from `PlayerController::update_physics`.
  - Enforced that ammo only replenishes upon completing a full reload cycle (`reload_weapon()`, [R] key or empty-chamber trigger).
  - Strengthened assertions in `test_combat_omni_ai.cpp` and added comprehensive zero-trickle assertions in `test_unit_all.cpp` across all 3 delver classes.
- **Codebase Optimization, File Structure Standardization & Screenshot Catalog Integrity**:
  - **Screenshot & 3D Model Catalog Integrity**:
    - Purged 17+ MB of stale/legacy screenshots from `screenshots/` and 6 orphaned combat action frames from `docs/models/`.
    - Enforced that `docs/models/` contains strictly the canonical 6 models (Void Stalker, Seismic Burrower, Drill Rig, Demolitionist, Vanguard, Scout) plus showcase montages and telemetry reports.
    - Updated `scripts/capture_models.py` with automatic pruning (`prune_stale_model_artifacts()`).
    - Enhanced `scripts/analyze_screenshots.py` with automated preview and stale screenshot cleanup.
    - Added dedicated workspace artifact cleanup tool: `scripts/cleanup_test_artifacts.py`.
    - Added `screenshots/previews/` and test dump JSONs to `.gitignore`.
  - **Dead Code & File Structure Cleanup**:
    - Removed 5 obsolete mockup/forwarding files in `src/entities/` (`void_stalker.hpp`, `void_stalker.cpp`, `enemy.hpp`, `enemy_movement.cpp`, `animation_state.hpp`).
    - Standardized external header includes (e.g. `<font8x8.h>`) across all UI modules.
    - Fixed missing `selected_class_id` in `PauseMenu::render` in `src/core/application.cpp`.
  - **High-Performance Voxel Math & Compiler Optimizations**:
    - Branchless bounds checking: `((x | y | z) & ~(CHUNK_SIZE - 1)) == 0`.
    - Bit-shifted coordinate indexing: `(x & 31) | ((y & 31) << 5) | ((z & 31) << 10)`.
    - Inlined `Chunk::get_voxel` and `Chunk::get_voxel_idx` in `chunk.hpp`.
    - Branchless `>> 5` and `& 31` chunk coordinates in `world.cpp`.
    - Fast interior slice face testing in `GreedyMesher::generate_mesh`.
    - Preallocated BFS graph sets in `StructuralCheck::solve_cavein`.
    - Added MSVC Release optimizations `/O2 /Oi /Ot /Gy` and `/OPT:REF /OPT:ICF` in `CMakeLists.txt`.
    - Full 10 test suites passing with reduced unit test execution time (~280ms down from ~750ms).
  - **TDD Test & Build Acceleration Pipeline**:
    - **Header Precompilation Expansion**: Added `<chrono>`, `<filesystem>`, `<functional>`, `<sstream>`, `<fstream>`, `<array>`, `<queue>`, `<unordered_set>`, `<thread>`, `<mutex>`, `<atomic>`, `<cstdint>`, `<cstddef>` to `src/core/pch.hpp`.
    - **PCH Reuse for All Test Targets**: Configured `target_precompile_headers(${tgt} REUSE_FROM voidfall_engine_core)` across all 11 test executables in `CMakeLists.txt`, slashing test compilation times.
    - **Collision Test Meshing Bypass**: Added `enable_background_meshing = true` parameter to `World` (and `VOIDFALL_HEADLESS_TEST=1` env guard), preventing background worker threads and greedy mesher loops from running during headless collision tests. Dropped `test_level_collision` runtime from 8.3s down to <1.0s.
    - **Automated Mtime Staleness Guard in `scripts/tdd.py`**: Added file modification timestamp comparisons between sources (`src/`, `include/`, `tests/`) and target test binaries. If `--no-build` is passed but code has been modified, `tdd.py` automatically overrides `--no-build` and executes a fresh build, ensuring agents never test stale binaries.
    - **Total Test Suite Speed**: All 10 parallel test suites now execute in **< 1.0 second** (down from 8.3s).

## 2026-10-04
- **Cavern Darkness Overhaul, Flashlight Crucial Mechanics & Controls Remap ([F] Light, [G] Grapple, [T] Flare)**:
  - **Subterranean Illumination & Darkness Tuning**:
    - Darkened base cavern subterranean ambient light across all sectors in `voxel_pbr.frag` from `vec3(0.07, 0.09, 0.12)` down to `vec3(0.012, 0.016, 0.022)` (Sector 1), `vec3(0.022, 0.012, 0.007)` (Sector 2), and `vec3(0.007, 0.020, 0.011)` (Sector 3).
    - Deepened vertical depth attenuation falloff down to `0.12` min clamp in bedrock fissures.
    - Reduced ambient volumetric mist inscatter in `volumetric_fog.comp` from `0.15` to `0.035` and viewmodel ambient in `viewmodel.frag` to prevent washed-out fog in unlit chambers.
    - Deep unlit caverns plunge into pitch-black darkness, transforming the player's flashlight and chemical flares into indispensable tools for survival and orientation.
  - **Flashlight Spotlight & Controls Remapping**:
    - Key **`[F]`** is now the primary toggle for the Exosuit Flashlight / Headlamp (with `[L]` retained as secondary).
    - Aligned headlamp spotlight emission in `Application` to `player.eye_position()` and tuned intensity to $4.5$ with a focused 18°-32° beam.
    - Key **`[G]`** is now dedicated to launching the Grappling Hook tether (`BTN_GRAPPLE_FIRE`), with `[E]` strictly reeling the winch cable.
    - Key **`[T]`** (with `[Z]` secondary) deploys Throwable Chemical Flares.
    - Increased flare point light radius from $16.0\,\text{m}$ / $18.0\,\text{m}$ to $22.0\,\text{m}$ and intensity to $5.2$ so throwing a flare illuminates vast 360-degree chambers in vibrant class-colored light.
    - Updated HUD badges (`[* F: LIGHT ON]`, `[T] CHEMICAL FLARE: X/3`) and pause menu keybindings table.

- **Removal of Clunky Fortress Barricade Action & Addition of Kinetic Repulsor Field**:
  - Removed the Vanguard's physical barricade placement action (`[C]` ability that previously spawned a 3x2 barrier of bulkheads in front of the player, obstructing movement in narrow mine shafts).
  - Replaced it with the **Kinetic Repulsor Field**:
    - Emits a high-energy radial repulsor pulse ($8.5\,\text{m}$ radius) that blasts away approaching Void Stalkers with strong knockback impulse, applies a $2.0\,\text{s}$ stun, and inflicts $40\,\text{HP}$ damage.
    - Disrupts and stuns burrowing hostiles (Seismic Burrowers) within the epicenter radius.
    - Deflects all active falling `DynamicDebris` within $10.0\,\text{m}$ outward and upward away from the squad.
    - Clears player trauma and provides immediate Armored Stabilizer suit integrity restoration ($+25\,\text{HP}$).
    - Updated HUD tactical ability label from `[C] BARRICADE` to `[C] REPULSOR`.
    - Added dedicated `SoundCue::TacticalRepulsor` with deep resonant magnetic thump and sweeping sonic shockwave.

- **First-Person Viewmodel Melee Animations Overhaul (Gun, Demo, Mining Drill)**:
  - Designed and implemented bespoke, multi-phase kinetic melee strike animations for each weapon category (`ToolSlot::CombatWeapon`, `ToolSlot::DemolitionCharge`, `ToolSlot::MiningDrill`):
    - **Gun**: Tactical rifle butt-stroke and diagonal slash with archetype variations (Demolitionist Scattergun horizontal wide bludgeon sweep, Scout Railgun long bayonet spear thrust, Vanguard Carbine fast rifle strike). Includes $38\,\text{Hz}$ impact deceleration vibration and capacitor pulse.
    - **Demo**: Heavy brass-knuckle hammer-fist punch using the reinforced handheld detonator clacker, with physical plunger compression ($-0.022\,\text{m}$) and tactical safety beacon shock flash ($+0.70$ emissive) on contact.
    - **Mining Drill**: Heavy two-handed hydraulic auger battering ram with rapid auger motor spool-up ($+4200-6500\,\text{deg/s}$), forward piston extension ($+0.08\,\text{m}$), superheated diamond auger glow ($+0.75$ emissive), impact sparks, and $42\,\text{Hz}$ mechanical chatter.
  - Added `player.melee_shove_progress()` contract and plumbing to `PlayerController` and `ViewModel::render`.
  - Added regression test 10 to `test_combat_omni_ai.cpp` and unit test assertions to `test_unit_all.cpp`.
  - Updated wiki documentation in `wiki/mechanics/delvers_and_classes.md`.

- **Decommissioning and Removal of Void Drifter (Aerial Harasser)**:
  - Completely decommissioned the aerial Void Drifter archetype (`StalkerRole::VoidDrifter`) from the engine, game loop, and level generators due to flight, collision, and player attachment issues.
  - Removed drifter roosting and ambient spawn hooks in `World::PopulateCavernFauna` and `Application::InitWorld`.
  - Stripped drifter 3D boid flight, dive-bombing, voxel penetration mitigations, and tendril rendering from `VoidStalkerManager`, `VoidStalker`, and `Renderer`.
  - Re-anchored enemy encounter balance around the core trio: Wall/Ceiling Stalkers (Melee & Shooter archetypes), Chitin Goliaths, and Seismic Burrowers.

- **Enemy Perception Overhaul, Flying Enemy (VoidDrifter) Wall Collision & Traversal Audio Balancing**:
  - **Acoustic Traversal Balancing (Grapple Hook vs Jetpack Thrusters)**:
    - Fixed critical bug where grapple hook emitted continuous $4.5\,\text{intensity}$ sound events per frame without $dt$ (generating $270\,\text{noise/sec}$ and immediately alarming all cavern hostiles).
    - Rebalanced Grapple Hook to be an agile stealth mobility tool:
      - Launch: Quiet pneumatic one-shot (`SoundCue::GrappleFire`, $1.2\,\text{intensity}$, $5.0\,\text{m}$ radius).
      - Reel: Subtle high-torque electric winch whine (`SoundCue::GrappleReel`, $2.2\,\text{noise/sec}$, $5.5\,\text{m}$ alert radius, 0.35s period).
    - Synthesized dedicated Jetpack thruster audio (`SoundCue::JetpackLoop`, 55Hz sub-bass combustion burn + pink noise exhaust hiss + 24Hz flame flutter).
    - Jetpack thruster emits $14.0\,\text{noise/sec}$ with a prominent $20.0\,\text{m}$ alert radius (over $6\times$ louder and $3.6\times$ larger acoustic signature than the grapple reel).
    - Both tools now have distinct procedural audio synthesis and fully decoupled volume/noise scaling.
  - **Flying Enemy (VoidDrifter) Solid Voxel Collision & Anti-Tunneling**:
    - Resolved bug where flying enemies bypassed world collision during dive-bombs and clipped through solid rock walls.
    - Implemented full 3D per-axis AABB voxel collision against `world.is_solid()`, allowing drifters to deflect smoothly along cavern walls, ceilings, and columns.
    - Added line-of-sight checks to dive-bombing: drifters abort dives if the player breaks line of sight behind solid geometry or if the dive exceeds 3.5s.
  - **Anti-Attachment Constraint & Disengagement Recoil**:
    - Enforced a minimum $1.35\,\text{m}$ spherical clearance zone preventing drifters from clipping or attaching permanently to the player's model.
    - Added an explosive $12.0\,\text{m/s}$ upward/backward recoil bounce post-hit with a $2.2\,\text{s}$ attack refractory cooldown, returning the creature to ceiling hover altitude.
  - **Distant Enemy Perception & Stealth Gating**:
    - VoidDrifters now require high noise ($> 60\%$) within proximity ($<24\,\text{m}$ with LOS, $<14\,\text{m}$ through strata), direct headlamp illumination, close tactile contact ($<3\,\text{m}$), or damage retaliation to dive.
    - Distant enemies no longer alert or dive blindly across the level when players maneuver quietly.
  - **Verification**:
    - Added Test 24 to `test_audio.cpp` and Test 19 to `test_enemy_stalker.cpp`.
    - All 10 test suites pass cleanly via `python scripts/tdd.py` in silent headless mode.

## 2026-10-03
- **Subterranean Audio Expansion, Sector-Specific Soundscapes & Subtle Enemy Sound Design**:
  - **Subterranean Acoustic Expansion & Multi-Biome Audio Variety**:
    - Expanded audio assets from 37 to 48 sound cues across all sectors and subsystems.
    - Implemented unique continuous ambient drones for all 3 sectors:
      - **Sector 1 (Perimeter Drift)**: `SoundCue::AmbientSector1` (8.44s crystalline airflow, resonant quartz shimmer).
      - **Sector 2 (Volatile Fault)**: `SoundCue::AmbientSector2` (9.75s geothermal sub-bass magma rumble, pressurized steam).
      - **Sector 3 (Void Cradle)**: `SoundCue::AmbientSector3` (9.75s infrasonic abyssal gravitational void pulse).
    - Added dedicated sector descent arrival stingers: `SoundCue::SectorArrival1/2/3` (crystalline harmonic triad, industrial geothermal brass swell, abyssal void strike).
    - Expanded sector-specific procedural micro-ambience events:
      - Sector 1: `SoundCue::CrystalChime` (delicate quartz resonance), drips, rock groans.
      - Sector 2: `SoundCue::GeothermalVent` (pressurized volcanic steam hiss), basalt groans.
      - Sector 3: `SoundCue::VoidDistortion` (dimensional acoustic phase warp), abyssal echoes.
  - **Subtle & Scary Enemy Audio Overhaul**:
    - Eliminated loud repetitive shriek spamming: centralized stalker screeches to a global manager cooldown (35–55s) triggered only at long distance (>18m) in unlit chambers.
    - Added `SoundCue::StalkerChitter`: subtle, bone-dry chitinous mandible clicks (volume 0.45) emitted when stalking players in shadows (5–16m).
    - Added `SoundCue::StalkerHiss`: low, sibilant predatory threat exhalation on detection transition.
    - Implemented dynamic threat ducking ($0.28$ attenuation floor) on `StalkerLunge` jump attacks, dropping background cavern drone for visceral negative-space contrast.
    - Removed spammy HUD text notifications for distant monster sounds, directing delvers to rely on 3D binaural spatial hearing.
  - **100% Quality & Ear Safety Assurance**:
    - Synthesized and balanced all 48 sound cues with strict peak limiting ($< 0.95$, $-1.0\,\text{dBFS}$ ceiling) and anti-harshness lowpass filters ($< 6.5\,\text{kHz}$).
    - Expanded `test_audio.cpp` to 15 modules covering sector ambients, stingers, micro-ambience, and stealth cues.
    - Verified all 7 test suites pass in parallel via `python scripts/tdd.py` and packaged updated standalone release ZIP (`dist/VoidfallDredge_v1.0.zip`).
- **Level Design Expansion, Progressive Sector Scaling & Environmental Hazards**:
  - **Progressive Sector Scaling ($3\times3 \to 4\times4 \to 5\times5$)**:
    - Expanded cavern layout generation dynamically per sector:
      - **Sector 1 (Perimeter Drift)**: $3\times3$ grid (9 rooms, $72\times72$ voxels, 12 corridors, $3\times3$ chunk footprint).
      - **Sector 2 (Volatile Fault)**: $4\times4$ grid (16 rooms, $96\times96$ voxels, 24 corridors, $3\times3$ chunk footprint).
      - **Sector 3 (Void Cradle)**: $5\times5$ grid (25 rooms, $120\times120$ voxels, 40 corridors, $4\times4$ chunk footprint).
    - `World` chunk management updated to allocate chunk bounds dynamically based on `LevelGenerator::world_width()` and `world_depth()`.
  - **Expanded to 15 Distinct Structural Room Archetypes**:
    - Expanded from 9 to 15 unique cavern archetypes in `LevelGenerator` ([src/voxel/level_shapes.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.hpp), [src/voxel/level_shapes.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.cpp)):
      1. `SpawnStagingCavern` (improved drop pad, climbing crates).
      2. `MiningPillarHall` (improved columns, parkour arch bridge).
      3. `CrystallineGeode` (improved stepping stone rings).
      4. `TerracedQuarry` (stepped 4-tier amphitheater).
      5. `IndustrialVaultBunker` (mezzanine catwalks, blast doors).
      6. `FaultLineCrevasse` (magma trench, eroded bridge).
      7. `AbyssalVerticalChasm` (24m vertical spiral ledges).
      8. `RadioactiveCoreSanctuary` (toxic ore moat, stepping stones).
      9. `ExtractionLandingBay` (holdout barricades, launch chimney).
      10. **`MagmaCalderaLake`** (NEW: molten thermite slag lake, basalt hopping pillars, central lava geyser).
      11. **`SpikeTrenchArena`** (NEW: crystalline punji spike beds, high balance beam catwalks with jump gaps).
      12. **`VoidSingularityRift`** (NEW: bottomless void abyss, zero-g floating platforms & monolith).
      13. **`FungoidBioGrotto`** (NEW: giant bouncy mushroom cap platforms, toxic gas pods).
      14. **`LaserDefenseFoundry`** (NEW: precursor smelting vats, overhead crane gantry).
      15. **`CrumblingArchCanyon`** (NEW: 14m canyon with natural stone arches & grapple stalactites).
    - Added dedicated dynamic animated cavern luminaries for each archetype with custom light colors, pulse speeds, and flicker rates.
  - **Dynamic Environmental Hazards & Continuous Damage System**:
    - **Molten Thermite Slag (`MAT_THERMITE_SLAG`)**: Deals $-24\,\text{HP/s}$ thermal burn, increases camera trauma, HUD warning.
    - **Puncture Spike Traps (Flag `0x0F`)**: Deals $-25\,\text{HP}$ puncture damage, upward $+5.5\,\text{m/s}$ impulse leap, HUD warning.
    - **Void Singularity Abyss ($y \le 1.8\,\text{m}$)**: Deals $-25\,\text{HP}$ gravitational damage, expels player upwards to $y=6.5\,\text{m}$ ($+4.0\,\text{m/s}$), HUD warning.
  - **Test Suite Modernization & 100% Verification**:
    - Updated `tests/test_unit_all.cpp` Module 12 to test luminaries across all 15 room archetypes.
    - Updated `tests/test_level_collision.cpp` across all 9 modules to support dynamic grid dimensions, 760 corridor seams, 3D BFS reachability, and portal-to-portal sightlines.
    - Verified all 7 test suites pass in parallel with 100% success rate via `python scripts/tdd.py`.
    - Executed 10-phase automated visual test harness (`./build/Release/VoidfallDredge.exe --auto-play-test`) and visual diagnostic analysis (`python scripts/analyze_screenshots.py`) with zero warnings or errors.

## 2026-10-02
- **Class-Specific Delver Combat Firearms & Ballistics Architecture**:
  - Implemented 3 asymmetric class-specific firearms equipped via hotbar slot `[2]` or quick-draw key `[X]`:
    1. **Demolitionist (Kaelen) — Magma Scattergun**: Heavy breaker shotgun with revolving 6-round thermite drum, dual fluted over-under barrels, and heavy pump slide. Fires a 5-flechette thermite buckshot cone (37.5 max burst dmg, 38 m/s, fiery amber tracers) with concussive knockback (`SoundCue::ScattergunFire`).
    2. **Vanguard (Rhodes) — Plasma Carbine**: Tactical rapid-pulse plasma rifle with twin chrome magnetic accelerator rails, reflex holographic optic, and 16-round capacitor battery with passive trickle recharge. Fires focused electric-cyan plasma bolts (22 dmg, 52 m/s, 0.16s cyclic cooldown) (`SoundCue::PlasmaFire`).
    3. **Scout (Vesper) — Needler Railgun**: Hyper-velocity precision marksman sniper rifle with elongated superconductor rails, elevated precision scope, and 8-round needle cartridge. Fires pinpoint emerald green needles (42 dmg, 110 m/s, zero spread, extreme range) (`SoundCue::RailgunFire`).
  - Added dedicated first-person PBR viewmodels for all 3 weapons with custom meshes, gauntlets, and contractor accent color palettes.
  - Dynamically updated in-game HUD: custom reticles (wide spread ring for Scattergun, tactical bracket for Carbine, sniper chevrons for Railgun) and responsive hotbar slot labels (`[2] SCAT: X`, `[2] CARB: X`, `[2] RAIL: X`).
  - Fully verified across all 7 test suites via `python scripts/tdd.py` and visual testing harness with zero errors.
- **Enemy Signature Vocalizations, Cavern Echoes & Proximity Scaling**:
  - Implemented unique procedural audio synthesis cues for both hostile enemy species:
    - **Void Stalker**: `SoundCue::StalkerEchoScreech` (downward 560Hz->260Hz FM modulated insectoid shriek with multi-tap cavern echoes at 180ms and 420ms).
    - **Seismic Burrower**: `SoundCue::BurrowerRoar` (subterranean undulating 36–62 Hz tectonic roar + overtone saturation) and `SoundCue::BurrowerGrind` (chitinous rock cutter tooth grinding).
  - Extended 3D spatial acoustic propagation up to 58.0m in cavern corridors with steep inverse-distance rolloff (`ref_dist = 4.0m`).
  - Distant shrieks/roars (> 30–50m) reverberate faintly down tunnels (attenuation ~0.05–0.10), while proximity vocalizations (< 12–16m) hit with >10x louder amplitude, clear stereo binaural azimuth positioning, and HUD proximity alert banners.
  - Added randomized staggered vocalization timers in `VoidStalkerManager` (11–24s) and `SeismicBurrowerManager` (13–27s).
  - Verified 100% test pass rate across all 7 test suites via `python scripts/tdd.py` and ear-safety limiter adherence in `test_audio.exe`.
- **Wiki Initialization**: Created living game design knowledge base following the `llm-wiki` pattern.
- **Threat Hierarchy Defined**: Cataloged Void Stalkers (ambush), Seismic Burrowers (terrain deformation), and Volatile Leeches (hazard response).
- **Progression Invariants Documented**: Formally recorded exponential upgrade cost formula ($100 \times 1.6^{\text{tier}-1}$), 85% respec refund, and 50% expedition abandon penalty.
- **Engine Subsystem Documentation**: Added dedicated `README.md` manuals across all 11 project folders (`src/`, `src/core/`, `src/voxel/`, `src/graphics/`, `src/player/`, `src/entities/`, `src/skills/`, `src/systems/`, `src/ui/`, `src/net/`, `assets/`, `assets/shaders/`, `scripts/`, `tests/`, `saves/`, `screenshots/`).
- **Code & Docs Harmonization**: Updated 40-second extraction defense holdout, 10-phase visual test pipeline, and all 11 voxel materials to match active engine code.
- **Modular Level Generation Architecture**: Replaced unbounded trigonometric continuous noise with a modular shape grammar and connected room/corridor graph (`LevelGenerator` in `src/voxel/level_shapes.hpp`). Implemented 9 distinct structural room archetypes (Spawn Staging, Mining Pillar Hall, Crystalline Geode, Terraced Quarry, Industrial Vault Bunker, Fault-Line Crevasse, Abyssal Vertical Chasm, Radioactive Core Sanctuary, Extraction Landing Bay) with level-gated access (Level 2 vault doors/gas, Level 3 abyssal drops/radiation).
- **Collision & Wall-Clipping Elimination**: Implemented continuous physics sub-stepping ($\le 0.20$ voxels per step) in `PlayerController::resolve_voxel_collisions`, impenetrable perimeter bounds in `World::is_solid`, and downward floor scanning in `clamp_to_surface`. Fully verified with zero errors in `voidfall.log` and 100% test pass rate.
- **Progression & Economy Revamp**: Decoupled EXP from skill node purchases. Introduced **Coins** as run remuneration currency for upgrading skills ($100 \times 1.6^{\text{tier}-1}$ cost formula, 85% respec refund). Dedicated **EXP** to cumulative **Player Level (Delver Rank)** with progressive thresholds (300, 700, 1300, 2200 EXP) and +150 Coin bonus per rank promotion. Enforced strict level gating on skill tiers (Tier $N$ requires Level $N$), sectors (Sector 1 at Lv 1, Sector 2 at Lv 2, Sector 3 at Lv 4), and delver archetypes (Demolitionist at Lv 1, Vanguard at Lv 2, Scout at Lv 3). Updated Hub UI, quick upgrades, tests, and documentation.
- **Automated Level Collision Test Suite & Anti-Clipping Overhaul**:
  - Diagnosed root causes of player clipping through ceilings and blocks: fixed `resolve_axis_collision` to isolate directional contact faces (checking head upward, feet downward, leading horizontal faces) so touching ceilings or walls no longer pops players above ceilings or through blocks.
  - Implemented static minimum-translation-vector `depenetrate()` pass in `PlayerController` and verified `is_penetrating_solid(world) == false` under all high-speed conditions.
  - Sealed all room archetype ceilings in `LevelGenerator` (Vault Bunker, Abyssal Chasm, Quarry, Pillar Hall) to guarantee zero upper air pockets beneath mantle bedrock ($y \ge 26$).
  - Built dedicated `tests/test_level_collision.cpp` executable covering: 1) Level topology, 2) High-speed (15 m/s) wall sprinting across all 9 rooms + outer perimeter bedrock boundaries, 3) Vertical thruster burns & grapple reel against ceilings with zero roof popping, 4) Mining drill block breaking + seamless traversal through carved doorways + impenetrable door jambs, and 5) $45^\circ$ corner sliding & static depenetration recovery.
  - Integrated into `scripts/tdd.py` (executing all 4 test suites in parallel in ~0.11s) and verified visual test harness across all 10 gameplay screens (`screenshots/00_all_phases_montage.jpg`).
- **Base Shapes Stress & Random Permutations Integration Testing**:
  - Implemented Module 6 in `tests/test_level_collision.cpp`: isolated stress testing of all 9 room archetypes with structural invariant verification (bedrock $y \le 3$, mantle $y \ge 26$, perimeter enclosure), 8-direction horizontal sprinting at $15.0\,\text{m/s}$, vertical thruster burns at $25.0\,\text{m/s}$, and downward impact at $-35.0\,\text{m/s}$ with zero clipping.
  - Implemented Module 7 in `tests/test_level_collision.cpp`: testing 30 random procedural permutations across Sectors 1, 2, and 3 (10 seeds each). Audited all 360 corridor threshold seams for floor support and headroom, executed bidirectional walking across room-corridor seams, and solved 3D BFS pathfinding from Spawn Pad $(16, 5, 16)$ to Evacuation Pad $(56, 5, 56)$ proving 100% reachability across every permutation.
  - Refined `CrystallineGeode` and `TerracedQuarry` geometry in `LevelGenerator` to guarantee seamless ground-level corridor transitions and full accessibility for delver traversal.
- **Seismic Tremor & Physical Falling Debris Overhaul**:
  - Improved tremor behavior so that when seismic tremors trigger, overhead ceiling stone blocks actually detach and are removed from the ceiling (`MAT_AIR`).
  - Implemented complete 6-face 3D cube geometry for [`DynamicDebris`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/dynamic_debris.hpp) with GPU buffer generation and headless OpenGL safety checks.
  - Implemented [`Renderer::render_debris`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.hpp#L61) with centroid offset, tumbling rotation, and normal transformation (`vNormal = normalize(mat3(uModel) * NORMALS[normIdx])`) in [voxel_pbr.vert](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/voxel_pbr.vert) so falling blocks are clearly visible, textured, and dynamically lit.
  - Sinking / ground placement physics: falling blocks sweep downward along gravity trajectories and, upon reaching the solid cavern floor, settle permanently as placed solid terrain blocks (`world.set_voxel(place_pos, Voxel{mat, 0}, true)`).
  - Added dust kickup, rock fracture particles, trauma feedback, and multiplayer delta synchronization on landing. Retained bulkhead fortification protection and deflection.
  - Added comprehensive automated unit test Module 9 in [tests/test_unit_all.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_unit_all.cpp) (24/24 unit tests passing) and verified with the 10-phase automated visual test harness.
- **Room Visibility, Sightline Corridors & Mesh Integrity Overhaul**:
  - **Identified Critical Root Cause of Room Occlusion Bug**: Fixed vertex coordinate bitfield truncation in `PackedVoxelVertex` (`src/voxel/packed_vertex.hpp`) and `voxel_pbr.vert`. Previously, local chunk coordinates were masked with 5 bits (`0x1Fu`), causing boundary quads reaching coordinate 32 ($X=32$ or $Z=32$ on chunk borders) to wrap to 0. This generated massive cross-chunk inverted polygon sheets that completely blinded player view into adjacent rooms and corridors.
  - **Upgraded to 6-Bit Vertex Bitfield Packing**: Reallocated `data0` to give 6 bits each to `localX`, `localY`, and `localZ` ($0..63$, holding $0..32$), 3 bits for normal index ($0..5$), 2 bits for baked AO ($0..3$), 8 bits for texture array layers ($0..255$), and 1 bit for surveyed highlight. Correctly decoded in `voxel_pbr.vert`.
  - **Widened & Elevated Corridors with Illuminated Sightline Portals**:
    - Increased corridor dimensions from cramped $4\times 5$ tunnels to grand $7\times 10$ connector halls (`half_width = 3`, $y \in [4, 14]$).
    - Added structural bulkhead portal arches with emissive `MAT_VOIDITE_CRYSTAL` marker lamps.
    - Repositioned spawn cavern mineral outcrop to an offset corner ($dx, dz \in [4, 6]$) to leave the main corridor approach wide open.
    - Framed vault bunker and geode entrance gateways with open walk-through portals, eliminating solid wall culling at room boundaries.
  - **Thread-Safe Whole-Sector Chunk Pre-Population**:
    - Upgraded `m_chunks` in `World` to `std::shared_ptr<Chunk>` and implemented safe queue flushing in `generate_world` and `set_level_generator`.
    - Pre-generates all 9 sector chunks in memory upfront so neighbor voxel matrices are always available to the greedy mesher, eliminating boundary void leaks and seam artifacts.
  - **Automated Verification (Modules 8 & 9 in `test_level_collision`)**:
    - **Module 8**: Executes full sector tour visiting 100% of all 9 rooms via sequential corridor traversal; evaluates DDA line-of-sight raycasts from corridor midpoints into target rooms, asserting $\ge 85\%$ open air visibility and 0 clipping.
    - **Module 9**: Executes 120 frames of continuous diagonal flight across chunk borders ($X=32, Z=32$) while carving $3\times 3\times 3$ spherical cavities; re-meshes all chunks and audits 9,800+ quads, verifying valid coordinates $[0..32]$, valid normals, non-degenerate triangles, and continuous texture mapping.
  - **Results**: All 4 test suites pass in parallel in $< 0.8\,\text{s}$ via `python scripts/tdd.py`, and the visual test harness captures 10/10 flawless screens (`screenshots/00_all_phases_montage.jpg`).
- **Seismic Sonar Balance, Upgrade Progression & Spectroscopic Rock Labels**:
  - **Addressed Overpowered Sonar Spam**: Implemented strict 10.0s cooldown on `[Q]` Sonar pulse (8.0s for Scout with Acoustic Pathfinder trait) and reduced baseline scan duration from 2.5s to 1.8s, eliminating infinite X-ray vision spam.
  - **Multi-Rank Progression Architecture**: Integrated Delver Proficiency Matrix (`SkillMatrix`) and Hub Upgrade Store (`UpgradeType::SonarFrequency`) so effective rank scales cooldown (down to 5.0s at Tier 5), linger duration (up to 3.3s), and pulse radius (+2m per tier).
  - **Acoustic Spectroscopy & Mineral Identification (Rank 2+)**:
    - Uncalibrated Sonar (Rank 0 / 1) displays raw wireframe reflections only without mineral identification.
    - At Rank 2+, the suit's acoustic receiver unlocks harmonic resonance analysis, automatically grouping adjacent ore blocks into mineral clusters and projecting high-contrast floating 3D tactical HUD labels (`[VOIDITE: Xm]`, `[TITANIUM: Xm]`, `[VAULT DOOR: Xm]`, `[RADIOACTIVE: Xm]`) that smoothly fade with the wave pulse.
  - **Tactical UI Integration**:
    - Added dynamic Sonar status badge above the hotbar showing `[Q] SEISMIC SONAR: READY`, `[Q] SEISMIC SONAR: SCANNING`, or `[Q] SONAR RECHARGING: X.Xs` with real-time fill bar.
    - Added on-screen warning when attempting to fire while recharging.
  - **Automated Verification**: Added comprehensive unit tests in `test_unit_all.cpp` (anti-spam cooldown enforcement, time decay, rank progression thresholds, mineral clustering, upgrade tier identification gating). All 4 test suites pass in parallel in $< 0.8\,\text{s}$.
- **Dynamic Seismic Stress Mechanics & Fault-Line Tremor Overhaul**:
  - Replaced passive random timer tremors with interactive tectonic stress accumulation (`m_seismic_stress`, 0%–100%) in `HazardClock`.
  - Delver actions directly build seismic strain: mining rock voxels (+3.5% for stone, up to +18.0% for heavy minerals), continuous drill vibration (+1.5%/s), weapon projectile impacts on cavern walls (+1.0% to +4.0% per shot), and explosive detonations (+16.0% to +32.0%).
  - Passive geological baseline creeps slowly in deep hazard escalation (+0.25%/s to +0.85%/s during Hazard Phases 3 & 4), while calm inactivity dissipates stress at -1.8%/s down to the geological baseline.
  - Exceeding 35% stress enables probabilistic fault slip rolls on every destructive impact (probability scaling with excess stress); reaching 100% triggers a guaranteed fault rupture.
  - Rupture triggers a 1.0s warning phase (siren `SoundCue::TremorWarning`, HUD flashing `! FAULT RUPTURE !`), followed by a 4.0s violent tremor phase with camera trauma and physical ceiling block collapse (`DynamicDebris`), resetting tectonic stress to 0% upon completion.
  - Top HUD pill displays real-time `SEISMIC: XX%` color-coded strain gauge (cyan -> amber -> crimson).
- **Spatial Geiger-Müller Radiation Counter Audio & Proximity Detection**:
  - Implemented spatial radiation source tracking in `World` (`m_radioactive_sources`), dynamically updated as chunks generate and voxels are mined or placed.
  - Added `World::query_radiation_proximity(pos, max_radius = 12.0m)` calculating inverse-distance squared falloff based on nearest radioactive ores (`MAT_RADIOACTIVE_ORE`).
  - Enhanced `AudioEngine` with authentic stochastic Geiger-Müller tube discharge audio (`SoundCue::GeigerClick` with 0.3ms transient pulse, 3.2kHz ionization ping, and exponential crackle decay).
  - Implemented real-time Poisson process click generation in `render_mix`: event rate $\lambda$ dynamically scales from quiet background ticks (~1.5 Hz) when clear to rapid, frantic crackling (~85 Hz) as the player steps closer to radioactive ore deposits or when hazard escalation surges.
  - Added dedicated Geiger Radiation Monitor pill above HUD vitals (`[!] RAD: XX mSv/h (XX%)`) with toxic emerald, amber, and crimson warning styling.
- **Combat Audio Fix, Annoying Noise Elimination & Gameplay Sound Polish**:
  - **Resolved Continuous Screaming Death Noise Bug**: Diagnosed root cause where `just_died` flags on `VoidStalker` and `SeismicBurrower` were never reset because `update()` skipped dead entities via `continue;`, and `remove_dead()` was never invoked in `Application::update_gameplay`. This caused `m_audio->play_sound_3d(SoundCue::StalkerDie)` to re-trigger 60 times/second indefinitely, saturating all 32 hardware audio voice slots with overlapping drone loops.
  - **Robust One-Shot Notification Lifecycle**: Moved per-frame flag resets to the top of the update loops across all entity managers; explicitly consumed and cleared `just_died` immediately upon sound invocation in `application.cpp`; and integrated per-frame `remove_dead()` cleanup to purge defeated entities from active memory.
  - **Audio Synthesis Polish (Zero Annoying Noises)**:
    - `SoundCue::StalkerDie`: Added smooth fade-to-zero envelope ($t \le 0.55\,\text{s}$), eliminating the abrupt 21% amplitude cut-off pop. Calibrated sub-harmonic dissolution tone and reduced high-frequency hiss from 2.2 kHz to 1.6 kHz for atmospheric subterranean decay.
    - `SoundCue::DrillLoop`: Replaced discontinuous rectangular chatter pulses with smooth half-sine cutting impulses ($\sin(\pi \phi)$), and lowered friction rock hiss to 1750 Hz to prevent high-frequency ear fatigue during prolonged mining.
    - `SoundCue::Footstep`: Calibrated sprinting cadence (0.32s at $12\,\text{m/s}$, 0.35f vol) and walking cadence (0.45s at $7\,\text{m/s}$, 0.28f vol) with mid-stride and airborne reset logic to eliminate double-step clicks on landing.
    - `SoundCue::ScattergunFire`: Replaced rectangular gating on mechanical ratchet click with a smooth half-sine window to eliminate transient step discontinuities.
    - `SoundCue::BurrowerGrind`: Added an 8ms smooth attack ramp and calibrated gain to eliminate initial step slew pops.
  - **Automated Verification**: Added Test 12 in `tests/test_audio.cpp` asserting immediate voice deactivation on entity death, drill toggle cleanup, and sprint cadence. Verified 100% ear safety compliance across all 19 reference WAV samples via `python scripts/analyze_audio.py` (all peak $\le -0.50\,\text{dBFS}$, zero clipping, slew $\le 0.25$), and passed visual testing harness (`python scripts/analyze_screenshots.py`).

### 2026-10-03: Dynamic Level Scaling, 15 Room Archetypes & 4x4 Visual Showcase Catalog
- **Progressive Sector Scaling Architecture**:
  - Implemented dynamic grid sizing across expedition sectors:
    - **Sector 1 (Perimeter Drift)**: $3\times3$ grid, 9 rooms, 12 corridors, $72\times72$ voxels ($3\times3$ chunks).
    - **Sector 2 (Volatile Vault)**: $4\times4$ grid, 16 rooms, 24 corridors, $96\times96$ voxels ($3\times3$ chunks).
    - **Sector 3 (Void Cradle)**: $5\times5$ grid, 25 rooms, 40 corridors, $120\times120$ voxels ($4\times4$ chunks).
  - Formula: $D(W - 1) + W(D - 1)$ ensures complete grid lattice connectivity with multiple exploration pathways.
- **15 Handcrafted Room Archetypes with Environmental Hazards & Parkour**:
  - Expanded `RoomShapeType` from 9 to 15 distinct archetypes in `level_shapes.hpp` and `level_shapes.cpp`:
    1. `SpawnStagingCavern`: Arrival pad, drop shaft, climbing crates, safe initial buffer.
    2. `MiningPillarHall`: 4 load-bearing columns, arched stone bridge at $y=11$, rich ore veins.
    3. `CrystallineGeode`: Hollow ellipsoidal shell, central Voidite crystal spire, perimeter stepping-stone ring at $y=8$.
    4. `TerracedQuarry`: 4-tier stepped amphitheater pit with titanium scrap seams and diagonal ramps.
    5. `IndustrialVaultBunker`: Precursor bunker with blast walls, open doorways, mezzanine catwalk at $y=9$, control consoles.
    6. `FaultLineCrevasse`: Tectonic magma fissure down to $y=2$, toxic gas vents, stone bridge.
    7. `AbyssalVerticalChasm`: 24m deep drop shaft, spiral catwalk ledges at $y=9, 14, 19$, hanging grapple stalactites.
    8. `RadioactiveCoreSanctuary`: Irradiated moat, stepping stones, central Voidite monolith.
    9. `ExtractionLandingBay`: Beacon touchdown cradle, launch chimney, holdout defense barricades.
    10. `MagmaCalderaLake`: Molten thermite slag lake, basalt stepping pillars, central island.
    11. `SpikeTrenchArena`: Sunken floor with punji spikes, high balance beam catwalks with jump gaps at $y=6$.
    12. `VoidSingularityRift`: Bottomless void abyss, zero-g floating platforms at $y=10, 14$, suspended core pyramid.
    13. `FungoidBioGrotto`: Giant bouncy bioluminescent mushroom caps ($y=8, 12, 16$), toxic spore pods.
    14. `LaserDefenseFoundry`: Molten smelting flumes, gantry catwalks at $y=8$, overhead crane beam at $y=16$.
    15. `CrumblingArchCanyon`: 14m deep gorge, high rim cliffs at $y=6$, 3 natural stone arches at $y=7, 8, 11$.
  - Plus `CorridorBulkheadVault`: Reinforced connecting corridor with structural ribs and heavy blast door bulkhead.
- **Environmental Hazard & Movement Mechanics**:
  - **Lava Slag (`MAT_THERMITE_SLAG`)**: $-24\,\text{HP/s}$ thermal burn, heat buildup, screen trauma, and warning HUD.
  - **Punji Spikes (`0x0F`)**: $-25\,\text{HP}$ puncture damage with $+5.5\,\text{m/s}$ vertical upward impulse.
  - **Void Singularity Rift ($y \le 1.8\,\text{m}$)**: $-25\,\text{HP}$ gravitational damage with $+4.0\,\text{m/s}$ upward repulsion.
- **Automated Visual Shapes & Level Design Showcase Catalog**:
  - Added `--capture-level-shapes` CLI flag and automated capture harness in `Application::run()`.
  - Created `scripts/capture_level_shapes.py` for one-command build, capture, and telemetry verification.
  - Generates 16 uncompressed master PNGs in `docs/level_design/shapes/`.
  - Composites 4x4 master contact sheet montage (`docs/level_design/level_shapes_showcase.jpg` and `.png`) with telemetry headers and footers.
  - Generated comprehensive developer visual catalog in `docs/level_design/README.md`.
- **Validation**: All 16 slots verified at 100.0% non-black ratio and healthy luminance; all 7 test suites pass in $< 1.1\,\text{s}$ via `python scripts/tdd.py`.

