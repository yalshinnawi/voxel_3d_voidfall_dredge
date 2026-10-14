# Voidfall Dredge - Game Design Log

Chronological ledger of balance iterations, mechanics additions, and architectural decisions.

## 2026-10-13

- **Late-Round Exploration Block Break Lag Elimination & Multi-Sector Load Stress Testing (`src/voxel/`, `src/entities/`, `src/core/`, `tests/`, `wiki/`)**:
  - **Player Feedback / Requirement**:
    - "In later levels, the game got insanely laggy after I broke a block near the end of me exploring the level, ensure this doesn’t happen. You may want to have load tests where you generate later levels and in fact levels around all numbers (perhaps level 3, 6, 9, … etc) so that you can test the impact of breaking blocks in later levels for instance."
  - **Root Causes Identified**:
    1. *Unbounded Chunk Meshing Queue Redundancy*: `World::queue_chunk_for_meshing` previously used a raw `std::queue<ChunkPos>` without membership checks or deduplication. Breaking a block or triggering a cave-in dirtied a chunk and its 6 neighbors repeatedly. When multiple blocks fractured in rapid succession, identical chunks were pushed dozens to hundreds of times into the queue, starving worker threads and locking `m_world_mutex` during redundant greedy meshing passes.
    2. *Cascading Neighbor Dirtying in Cave-In Solver*: In `StructuralCheck::solve_cavein`, clearing up to 64 detached ceiling/wall voxels called `world.set_voxel(..., mark_neighbors=true)` individually for every single voxel, triggering recursive neighbor chunk re-mesh requests per block instead of batching.
    3. *Burrower Swept Excavation Chunk Thrashing*: In later sectors (Sectors 3, 6, 9, 12), multiple Seismic Burrowers spawn deep in the level. Breaking a block near the end of the dungeon triggered acoustic fracture alerts, causing all dormant burrowers to wake into active tunneling. While tunneling, each burrower cleared 27-54 blocks per frame with `mark_neighbors=true`, spamming duplicate chunk remesh requests into the queue every tick.
    4. *Burrower Dust Particle Raycast Spikes*: Each digging burrower spawned up to 6 particles per frame in `Application::update`, each performing an independent line-of-sight raycast against the voxel world, producing dozens of raycasts per frame when multiple burrowers tunneled simultaneously.
    5. *Dynamic Debris Multi-Block Re-Voxelization*: When multi-block debris came to rest, `re_voxelize_blocks()` called `set_voxel(..., mark_neighbors=true)` per block instead of batching.
    6. *Fluid Sim Linear Neighbor Scan on Distant Breaches*: `FluidSim::TrySpreadToNeighbor` performed a linear $O(N)$ scan through `m_scheduledSpreads`, scaling quadratically when multiple water/acid reservoirs were breached simultaneously.
  - **Engine Solutions**:
    1. *O(1) Meshing Queue Deduplication*: Added `std::unordered_set<ChunkPos, ChunkPosHash> m_mesh_queued_set` in `World`. `queue_chunk_for_meshing()` checks and inserts atomically (`m_mesh_queued_set.insert(pos).second`); duplicate remesh requests for already-queued chunks are discarded instantly. When the worker pops a chunk, it erases it from the set.
    2. *Batch Chunk Dirtying in Structural Cave-Ins*: `StructuralCheck::solve_cavein` now calls `world.set_voxel(..., mark_neighbors=false)` for all falling blocks, aggregates the unique set of affected chunks and boundary neighbors, and queues each unique chunk exactly once.
    3. *Batch Chunk Dirtying in Seismic Burrower Excavation*: Swept cylinder and kinetic charge excavation in `SeismicBurrower::update` now set voxels with `mark_neighbors=false` and batch-dirty unique affected chunks once per tick.
    4. *Per-Frame Burrow Particle Raycast Throttling*: Added `MAX_BURROW_PARTICLES_PER_FRAME = 12` cap in `Application::update` to bound particle raycasting load during multi-burrower tunneling.
    5. *Dynamic Debris Batching*: `DynamicDebris::re_voxelize_blocks` sets voxels with `mark_neighbors=false` and queues unique chunks once.
    6. *Fluid Sim Hash Indexing & Tick Quota*: Added `std::unordered_set<uint64_t> m_scheduledSpreadMap` for $O(1)$ duplicate spread elimination, capped `m_scheduledSpreads` queue to 1024, and capped spread applications to `MAX_SPREADS_PER_TICK = 96`.
  - **Comprehensive Multi-Sector Load Stress Test Suite (`tests/test_load_and_stress.cpp`)**:
    - Added **Module 11: Late-Round High-Sector Block Break Load & Excavation Stress**.
    - Generates full $120 \times 120$ cavern layouts across Sectors 3, 6, 9, and 12, scaling burrower counts (1, 2, 3, 4).
    - Positions the delver deep in the far corner of exploration `(world_w - 20, 8, world_d - 20)`.
    - Sequentially breaks 12 solid, ceiling, and structural voxels in rapid succession, triggering fracture acoustics that awaken all dormant burrowers into active pursuit.
    - Simulates game ticks, asserting:
      - `mesh_queue_size() <= 48` (strictly bounded).
      - Cumulative block breaking + simulation duration $< 250\,\text{ms}$ (measured: Level 3 in $2.2\,\text{ms}$, Level 6 in $4.9\,\text{ms}$, Level 9 in $7.9\,\text{ms}$, Level 12 in $13.2\,\text{ms}$).
      - Zero NaN/inf velocities or positions across all entities.
  - **Verification**:
    - All 11 modules in `test_load_and_stress.exe` passed cleanly.
    - All 22 test suites in `python scripts/tdd.py` passed in parallel in 2.47s.

- **Ambient Audio Purity, Geiger Ticking Elimination & Swarm Footstep Arbitration (`src/audio/`, `src/core/`, `src/entities/`, `tests/`, `wiki/`)**:
  - **Player Feedback / Requirement**:
    - "There’s a constant ticking noise in the background at all times, it’s a bit annoying, ensure this is not the case, only music and sounds that come form environmental factors should be apparent. Also when enemies get stuck or there’s a lot of foot traffic from enemies the sound can be annoying, find a way to ensure walking sounds of enemies isn’t too crazy, especially when there’s a high load of enemies, find a way to ensure it’s balanced."
  - **Root Causes Identified**:
    1. *Constant Background Ticking Noise*: In `AudioEngine::render_mix`, `SoundCue::GeigerClick` was synthesized continuously based on artificial hazard phase math (`phase_rad = (m_hazard_phase == 1) ? 0.20f : ...`). As the expedition timer escalated, it generated a barrage of 17.5+ Poisson clicks/second in the background everywhere regardless of whether the delver was near radioactive minerals.
    2. *Stuck Enemy Rapid Footstep Looping*: When enemies pushed against solid rock, barricades, or corners, non-zero target velocity caused footsteps to fire repeatedly in place every 0.10s-0.20s without physical progression.
    3. *Dense Pack Footstep Cacophony*: When swarms or groups of enemies converged, each individual entity triggered 3D footstep sounds on identical frames without concurrency limits or volume companding, causing an abrasive wall of sound.
  - **Engine Solutions**:
    1. *Decoupled Ambient Radiation Clicks*: Removed synthetic hazard clock Poisson ticking from `AudioEngine::render_mix`. Geiger clicks are now strictly bound to genuine environmental radiation proximity (`m_radiation_proximity > 0.0f`), guaranteeing pure ambient silence when away from radioactive anomalies.
    2. *Stuck Enemy Footstep Suppression*: Stalkers and Burrowers now check `stuck_timer > 0.15f` and `wall_stuck_timer > 0.15f`. When an entity is wedged against terrain, its footstep accumulator is clamped to zero, completely muting footsteps until free movement resumes.
    3. *Swarm Footstep Arbiter & Distance Prioritization*: In `Application::update_audio_sources`, active footstep candidates from stalkers and burrowers are collected into an arbiter, culled beyond 28m, and sorted by distance to the player.
    4. *Crowd Volume Companding & Pacing*: Pack footsteps are dynamically companded via $1.0 / (1.0 + 0.35 \times \max(0, N-1))$ and throttled with a minimum $65\,\text{ms}$ spacing interval between step voice dispatches.
    5. *Engine-Wide Footstep Voice Concurrency Cap*: `AudioEngine::find_or_steal_voice_slot` limits simultaneous active footstep voices to max 3 across the entire engine, gracefully recycling the oldest footstep voice if a new one is dispatched.
  - **Verification**:
    - `tests/test_audio.cpp` updated and verified environmental radiation proximity gating.
    - `test_enemy_stalker.exe` (all 40 modules) and `test_enemy_burrower.exe` (all 19 modules) passed cleanly.
    - All 22 test suites in `python scripts/tdd.py` passed in parallel in under 3.0 seconds.

- **Subterranean AI Anti-Stuck Climbing, Vertical Wall Descent & Dense Swarm Load Stress Testing (`src/ai/`, `src/entities/`, `tests/`, `wiki/`)**:
  - **Player Feedback / Requirement**:
    - "I noticed monsters/enemies get stuck when climbing down things, or when they are near many other enemies, find a way to make it so they can’t get stuck in doing so, have more load testing for this, when there’s many enemies, many obstacles they need to traverse, slopes, stairs, things they have to climb. This will ensure our AI is top notch."
  - **Root Causes Identified**:
    1. *Stuck Descending Walls*: In `AberrantAI::steer_around_obstacles`, downward-probing wall climbers (`up.y <= 0.65f && dir.y < -0.2f`) treated the solid floor beneath the wall as an impassable obstacle rather than a valid landing surface, causing enemies to steer away laterally instead of stepping down to the floor.
    2. *Wall Cling Hysteresis Trap*: In `AberrantAI::sample_surface_normal`, enemies standing on a lower floor near a wall were trapped in `WALL_CLIMBING` because ledge descent logic didn't verify whether they were already firmly grounded (`!has_down || down_dist > 1.20f`).
    3. *Sector Floor Clamp Lock*: In `void_stalker.cpp` and `aberrant_ai.cpp`, sector bounds hard-clamped `pos.y` to a minimum of $4.5\,\text{m}$, preventing any monster from descending to floors at $Y < 4.5\,\text{m}$.
    4. *Downward Stair/Slope Popping*: In `resolve_voxel_collision`, rear radial probes behind descending enemies were triggering upward step-clambering hops, pushing descending entities back up slopes and stairs.
    5. *Crowd Funnel Deadlocks*: In dense bottlenecks, mutual penetration repulsion lacked tangential slip velocity, causing head-on enemy collisions to freeze in place.
  - **Engine Solutions**:
    1. *Ground Landing Recognition*: `steer_around_obstacles` now detects open floor surfaces beneath descending wall climbers, allowing them to smoothly transition to ground traversal without treating the floor as an obstacle.
    2. *Vertical Traversal Elevation Weighting*: `calculate_wall_traversal_direction` heavily weights vertical descent/ascent when the target is on a different level, speeding up downward wall climbing.
    3. *Dynamic Surface Hysteresis & Detachment*: Wall climbers within reach of the floor ($\le 1.45\,\text{m}$) or detached in open air smoothly transition to `FLOOR` state. Ledge climbing requires being ungrounded.
    4. *Forward-Gated Step Clambering*: Step clambering in `resolve_voxel_collision` is strictly gated to forward velocity alignment (`dot(off, vel_xz) >= -0.05f * vel_len`), preventing rear probes from popping descending entities upward.
    5. *Tangential Slip Velocity & Crowd Separation*: Added lateral slip impulses in `resolve_enemy_to_enemy_penetration` and spatial flocking so crowded swarms glide smoothly past one another without deadlock.
  - **Expanded Load & Stress Test Suite (`tests/test_load_and_stress.cpp`)**:
    - *Module 8 (Dense Swarm Slope & Staircase Traversal)*: 40 stalkers descending 5-tier staircases and 45° ramps in active combat pursuit ($37/40$ descended cleanly, $0$ clipping, $0$ NaNs).
    - *Module 9 (Multi-Tier Vertical Wall Descent & Landing)*: 36 stalkers attached to a vertical cliff descending to pursue a delver below ($36/36$ cleanly transitioned to `FLOOR` state upon ground contact).
    - *Module 10 (Narrow Corridor Crowd Flow & Bottleneck Stress)*: 40 stalkers crowded together filtering through a 2-block constriction without deadlock ($26/40$ flowed through bottleneck).
  - **Verification**:
    - All 10 modules in `test_load_and_stress.exe` passed with exit code 0.
    - All 40 test modules in `test_enemy_stalker.exe` passed with exit code 0.
    - Parallel `python scripts/tdd.py` passed across all 22 test suites in 3.94s.

- **Subterranean Liquid Hazard Differentiation & Emergent Neutralization (`src/player/`, `src/ui/`, `src/core/`, `src/entities/`, `tests/`, `wiki/`)**:
  - **Player Feedback / Requirement**:
    - "Going into toxic looking liquids should have a bad effect, and maybe make it interesting, different liquids have different effects".
  - **The Problem**:
    - Previously, `MAT_ACID` and `MAT_COOLANT` were grouped together with `MAT_CRYSTAL_AQUIFER` in `controller.cpp`. Toxic neon acid and cryogenic coolant erroneously acted like clean drinking water, cooling the delver's suit and dealing $0$ damage.
  - **Differentiated Liquid Mechanics & Suit Physics**:
    1. **Corrosive Acid (`MAT_ACID = 15`)**:
       - *Visuals*: Bubbling neon green surface with caustic vapor fumes.
       - *Physics*: High viscosity ($0.55\times$ speed, $4.5\times$ drag).
       - *Exosuit Reaction*: Dissolves energy shields at $-25\,\text{shield/s}$, deals $14\,\text{HP/s}$ direct chemical damage (`DamageSource::CorrosiveAcid`), delays shield recharge, and leaves a $2.0\,\text{s}$ lingering chemical burn ($4.5\,\text{HP/s}$).
       - *HUD Telegraph*: Sizzling neon-green visor etching, caustic border vignette, and dripping acidic visor droplets. Mild camera sizzle trauma ($0.06$).
    2. **Cryogenic Coolant (`MAT_COOLANT = 16`)**:
       - *Visuals*: Frosted sub-zero pale cyan fluid with crystalline vapor.
       - *Physics*: Freezing viscosity ($0.45\times$ speed, $5.5\times$ drag, hover thruster boost completely disabled).
       - *Exosuit Reaction*: Thermal quench (suit heat instantly plunges to $0.0$, clearing overheat), battery power kinetic drain ($-15\,\text{power/s}$), deals $6\,\text{HP/s}$ hypothermia damage (`DamageSource::CryogenicCoolant`), and chills servos for $2.5\,\text{s}$.
       - *HUD Telegraph*: Frosted cyan wash, crystalline border vignette, and jagged frost teeth around visor perimeter. Mild cold shudder trauma ($0.03$).
    3. **Molten Slag / Lava (`MAT_THERMITE_SLAG = 6`)**:
       - *Visuals*: Boiling incandescent magma with rising orange sparks.
       - *Physics*: Extreme plunge drag ($0.40\times$ speed, $8.5\times$ drag).
       - *Exosuit Reaction*: Heat surge ($+40\,\text{heat/s}$) triggering emergency overheat lockout, deals $14\,\text{HP/s}$ thermal burns (`DamageSource::ThermalLava`).
    4. **Crystal Aquifer Water (`MAT_CRYSTAL_AQUIFER = 11`)**:
       - *Visuals*: Translucent cyan pool with buoyant swimming traversal ($0.70\times$ speed, $2.8\times$ drag).
       - *Exosuit Reaction*: Safe haven and rapid heat sink ($-75\,\text{heat/s}$ cooling, clearing overheat lockout). Deals $0\,\text{HP}$ damage.
       - *Emergent Neutralization*: Submerging into clean aquifer water instantly neutralizes and washes away lingering corrosive acid burns (`m_acid_burn_timer = 0.0f`) and rapidly thaws frosted servo joints.
  - **Hostile Entity Hazard Interactions**:
    - Void Stalkers entering cryogenic coolant suffer heavy drag dampening ($0.35\times$ speed), cold crystallization damage ($15\,\text{HP/s}$), and panic/freeze death states.
    - Seismic Burrowers entering cryogenic coolant have kinetic charges stalled into a stunned state (`BurrowerState::Stunned`), take cold damage, and freeze solid if fatal. Burrower excavation feelers steer away from coolant pockets.
  - **Verification & Testing**:
    - Added comprehensive unit test `LiquidHazardDifferentiationAndNeutralizationTest_DifferentiatedLiquidDynamicsAndWaterCleanse` to `tests/test_gameplay_mechanics.cpp` validating all 4 liquid dynamics, damage sources, status timers, and water neutralization.
    - Executed `python scripts/tdd.py` — all 22 test suites built and passed in parallel in 4.60s.
  - **Player Feedback / Requirement**:
    - "We should only hint the player at an objective if they haven’t interacted with the given objective in a long time as well".
  - **Independent Interaction Tracking**:
    - Replaced the single global delay counter with per-objective idle timers (`m_astral_hint_idle_timer`, `m_voidite_hint_idle_timer`, `m_vault_hint_idle_timer`).
    - An objective only qualifies for guidance assistance if unfulfilled AND the delver has not interacted with that given objective for $\ge 75\,\text{s}$ (`OBJECTIVE_HINT_IDLE_THRESHOLD`).
    - If the delver has recently mined, drilled, or interacted with an objective (e.g., mined Voidite 30s ago), that objective does not qualify for a hint even if its quota is still unfulfilled.
  - **Continuous Active Interaction Reset**:
    - Active drill contact (`target_block_damage > 0.0f` on an objective block) continuously keeps that objective's idle timer at $0\,\text{s}$.
    - Breaking or mining an objective material (`on_block_broken`) immediately resets that objective's idle timer and dismisses active hints.
    - Reaching physical proximity ($\le 3.5\,\text{m}$ or direct line of sight within $\le 5.0\,\text{m}$) or collecting the item into inventory resets that objective's timer.
  - **Verification**:
    - Added `ObjectiveHintIdleInteractionRequirementTest` to `tests/test_gameplay_mechanics.cpp`.
    - Executed `python scripts/tdd.py` — all 22 test suites built and passed in parallel.

- **Pitch-Black Dark Zones, Sector Scaling & Torch Illumination Gameplay Overhaul (`src/voxel/`, `src/graphics/`, `src/entities/`, `src/core/`, `src/ui/`, `tests/`, `wiki/`)**:
  - **Player Feedback / Requirement**:
    - "In later levels or more difficult areas, we should have very dark areas, where the torches would actually be needed".
  - **Dynamic Sector Darkness Scaling & Layout Generation (`src/voxel/level_shapes.hpp`, `src/voxel/level_shapes.cpp`)**:
    - Added `is_dark_zone` flag and `darkness_factor` ($0.0 \to 1.0$) to `RoomPlacement`.
    - **Progressive Sector Scaling**:
      - Sector 1 (Perimeter Drift): ~15% chance for an unpowered deep pocket; mostly well-lit with ambient crystal beds.
      - Sector 2 (Volatile Fault): ~35%–45% of chambers are pitch-black dark zones with unpowered corridors.
      - Sector 3+ (Void Cradle): ~65%–75% of chambers are severe blackout caverns.
      - Safe Staging Guarantee: Spawn Staging Cavern $(0, 0)$ and Extraction Landing Bay $(W-1, D-1)$ are strictly 100% lit ($0.0$ darkness factor) across all seeds and sectors.
    - Cavern Luminaries (`generate_luminaries()`): All decorative point lights, geodes, water glows, and starlight canopies are omitted in dark zones. Corridor bulkhead lights leading into dark zones are disabled. Only molten magma/lava flumes retain thermal glow.
    - Added `is_dark_zone_at(pos)`, `get_darkness_at(pos)`, `dark_room_count()`, and `sector_darkness_ratio()` query methods to `LevelGenerator`.
  - **Shader & Zero-Ambient Blackout Pipeline (`assets/shaders/`, `src/graphics/renderer.hpp`, `src/graphics/renderer.cpp`, `src/graphics/surface_clutter.*`)**:
    - Added uniform `uDarknessFactor` across `voxel_pbr.frag`, `clutter.frag`, `stalker.frag`, and `postprocess.frag`.
    - In dark zones (`uDarknessFactor -> 1.0`), subterranean ambient bounce drops to zero, and post-process depth haze/fog ceases glowing, plunging distant cavern geometry into true inky black void.
    - Implemented ocular dark adaptation lerping (`glm::mix(m_area_darkness, m_target_area_darkness, 0.08f)`) for smooth transitions when stepping between lit and dark chambers.
  - **Predator Perception & Chemical Flare Integration (`src/entities/enemies/void_stalker.cpp`)**:
    - Updated `VoidStalkerManager::update()` perception processing: in addition to the delver headlamp cone, monsters actively detect all deployed `ChemicalFlare` instances (`FlareManager::flares()`). If within `flare.light_radius` (22m), `s.is_in_light` is set to true, revealing predators in the pitch dark and provoking light reactions.
  - **UI & Controls Polish (`src/ui/hud.cpp`, `src/ui/pause_menu.cpp`, `src/core/application.cpp`)**:
    - Added throttled suit hazard telemetry toast when entering pitch-black areas without illumination: `"[DARK ZONE: ZERO LUMENS - DEPLOY FLARES [T] / HEADLAMP [F]]"`.
    - Updated HUD and Pause menu keybinding labels to explicitly reference both Flashlight/Torch (`[F]`) and Throwable Flare/Torch (`[T]`).
  - **Automated Testing & Verification**:
    - Added `Module 6: Dark Zones & Torch Illumination Scaling Tests` to `tests/test_level_generation.cpp`.
    - Executed `python scripts/tdd.py` — all 22 test suites built and passed in parallel in 3.18s.
    - Executed `./build/Release/VoidfallDredge.exe --auto-play-test` — all 16 gameplay visual phases passed with 0 errors in voidfall.log.

- **Radiation Spatial Isolation & Active Exosuit Dosimeter Venting Balance Overhaul (`src/player/`, `src/core/`, `src/ui/`, `tests/`, `wiki/`)**:
  - **Root Cause Analysis (Unavoidable Radiation Buildup & Slow Deaths)**:
    - In `src/core/application.cpp`, player exosuit radiation was unconditionally overwritten every tick to `std::max(amb_rad, prox_rad)` where `amb_rad = m_hazard->radiation_level()`.
    - Because `HazardClock::radiation_level()` continuously increments over time across the sector, ambient radiation climbed into dangerous levels ($75\% \to 100\%$) globally throughout the entire map, even when players were far away from radioactive areas.
    - Because radiation was directly clamped to the monotonic clock and lacked any venting mechanism, radiation could never decrease, dealing unavoidable corrosion damage over time until death.
  - **Spatial Radiation Isolation (Non-Ubiquitous Hazard)**:
    - Radiation exposure is now strictly localized to **radiation-related areas** (within $12\,\text{m}$ proximity of `MAT_RADIOACTIVE_ORE` or irradiated chasms).
    - Clean cavern air outside radioactive zones maintains $0\%$ baseline exposure, ensuring delvers can explore, mine, and fight normally without suffering ambient radiation buildup.
  - **Active Exosuit Dosimeter Venting & Counterplay**:
    - Added `PlayerController::update_radiation(target_exposure, dt, in_decon_zone)` in [src/player/controller.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.hpp) and [src/player/controller.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.cpp).
    - When retreating from radioactive ore zones into clean cavern air, suit scrubbers actively vent and purge accumulated radiation dose at a rate of $10\%/\text{s}$ base, up to $17.5\%/\text{s}$ with `RadiationShielding` upgrades.
    - Inside the Extraction Pod perimeter, active medical decontamination scrubbers purge accumulated radiation dose rapidly at $25\%/\text{s}$.
  - **HUD Dosimeter Telemetry & Dynamic Venting State**:
    - In [src/ui/hud.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/hud.cpp), updated the Geiger monitor pill to detect dose venting (`is_venting`), displaying reassuring cyan `GEIGER: VENTING (XX%) - PURGING DOSE` while recovering, and cleanly hiding the pill once dosage clears ($\le 2\%$).
  - **Regression Testing**:
    - Added `RadiationDissipationAndSpatialLocalizationTest.VentDosimeterAndCleanZoneSafety` to [tests/test_gameplay_mechanics.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_gameplay_mechanics.cpp) verifying 0% buildup in clean air, active venting dissipation upon retreat, extraction pod decontamination, and upgrade scaling. All 22 test suites passing cleanly via `python scripts/tdd.py`.

- **Monster Swarm Anti-Invisibility & Dynamic GPU Vertex Buffer Expansion (`src/graphics/`, `src/core/`, `tests/`, `scripts/`, `wiki/`)**:
  - **Root Cause Analysis (Disappearing / Invisible Monsters)**:
    - In `src/graphics/renderer.cpp`, `render_stalkers()` had a hardcoded upload cap of `16,384` vertices (`size_t vert_count = std::min(s_stalker_verts.size(), static_cast<size_t>(16384));`).
    - Each Void Stalker model comprises 916 vertices (Melee/Shooter) to 952 vertices (Chitin Goliath with shield). Once more than 17 stalkers existed or when mixed with projectile spines, the vertex count exceeded 16,384, causing all subsequent monsters to be completely omitted from GPU upload and rendered invisible.
    - Similarly, `render_burrowers()` was capped at 16,384 vertices (~22 burrowers max), `render_carcasses()` at 8,192 vertices (~18 carcasses max), and `render_plasma_bolts()` at 4,096 vertices.
    - Additionally, initial static GPU VBO capacities were fixed (`m_stalker_vbo` allocated 32,768 vertices in `init_stalker_buffers()`; `m_cable_vbo` allocated 8,192 floats).
  - **Dynamic GPU Buffer Capacity Scaling**:
    - Added `Renderer::ensure_stalker_vbo_capacity(size_t required_verts)` in [src/graphics/renderer.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.hpp) and [src/graphics/renderer.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp) to track `m_stalker_vbo_capacity` (initialized to 65,536 vertices) and dynamically double and reallocate the VBO with re-bound VAO attribute pointers whenever required vertices exceed capacity.
    - Added `Renderer::ensure_cable_vbo_capacity(size_t required_floats)` for dynamic line and arc tracers.
    - Eliminated all artificial `std::min(...)` vertex caps across `render_stalkers`, `render_burrowers`, `render_carcasses`, `render_plasma_bolts`, `render_delver`, and `render_extraction_pod`.
    - Added dynamic staging vector reservations (`s_stalker_verts`, `s_burrower_verts`, `s_carcass_verts`, `m_stalker_staging_lines`) based on entity counts to eliminate runtime dynamic heap allocations in hot render frames.
  - **Automated Monster Swarm Load Testing & Verification**:
    - **C++ Headless Load Test Suite (`tests/test_load_and_stress.cpp`)**: Added Module 7 stress-testing 120 Void Stalkers (40 Melee, 40 Shooter, 40 Chitin Goliath), 30 Seismic Burrowers, 40 Void Spike projectiles, and 30 Carcasses (> 140,000 vertices demand) across 20 simulation ticks. Verified 0 NaNs, 0 entity drops, and dynamic VBO capacity scaling.
    - **Engine Visual Staging Harness (`--test-enemy-load`)**: Added visual load staging displaying 35 Stalkers across all 3 roles, 6 Burrowers, 12 Void Spikes, and 8 Carcasses simultaneously in forward visual range (40,052 vertices), writing `screenshots/enemy_load_test.png` and `screenshots/enemy_load_report.json`.
    - **Automated Runner Support (`python scripts/test_enemy.py --load`)**: Integrated `--load` flag in test runner with log auditing and metric reporting. All 22 test suites passing cleanly via `python scripts/tdd.py`.

## 2026-10-12
- **Chitin Goliath Apex Boss AI Overhaul, Formidable Moveset & Phase 2 Enrage (`src/ai/`, `src/entities/enemies/`, `src/ui/`, `src/graphics/`, `src/core/`, `tests/`, `wiki/`)**:
  - **Awake & Vigilant Sentinel**:
    - Precursor Vault Boss (`StalkerRole::ChitinGoliath`) never sleeps passively or ignores the player.
    - Senses player up to $28\,\text{m}$ upon vault entry and immediately awakens into active combat with a terrifying battle roar (`SoundCue::StalkerEchoScreech`).
    - Completely disabled fleeing (`flee_hp_threshold = 0.0f`) and passive circling around the player; the boss actively corners and presses the player.
  - **Formidable Boss Moveset & Special Attacks**:
    - **Titan Ground Slam (`StalkerState::GroundSlam`)**: Close-range ($\le 5.5\,\text{m}$) ground slam with concussive shockwave dealing up to 48 AOE damage across a 6.5m radius, screen trauma camera shudder, dust kickup, and ceiling debris cave-in hazard.
    - **Juggernaut Bull Charge (`StalkerState::Charging`)**: Mid-range (6.0m to 22.0m) high-speed kinetic ram sprint ($10.5\,\text{m/s}$ base, $13.5\,\text{m/s}$ enraged) dealing 52 damage.
    - **Wall-Crash Stun & Armor Breach**: Dodging or baiting the charge into a solid rock wall or metal bulkhead crashes the boss, stunning it for 2.4s and completely breaching its 85% frontal armor deflection (granting a full-damage vulnerability window).
    - **Carapace Quill Barrage**: Anti-kiting countermeasure against elevated players ($> 1.8\,\text{m}$ above) or ranged combat ($> 8.0\,\text{m}$), firing a 3-spike fan of armor-piercing quills (14 damage each).
    - **Heavy Cleave**: Sweeping claw strike dealing 38 base melee damage.
  - **Phase 2 Apex Frenzy Enrage ($\le 50\%$ HP)**:
    - Below 110 HP, enters berserk frenzy: movement speed increases to $3.6\,\text{m/s}$, charge speed to $13.5\,\text{m/s}$, attack cooldowns drop by 35–40%, eyes ignite in infernal radiant crimson, and carapace pulses with scorching magma veins.
  - **HUD Boss Health Bar & Status Banners**:
    - Top-stacked elite boss health bar with dynamic state banners (`[ APEX GOLIATH: ENRAGED ]`, `[ APEX GOLIATH: BREACHED ]`, `[ APEX GOLIATH: GROUND SLAM ]`, `[ APEX GOLIATH: CHARGING ]`) and real-time HP telemetry.
  - **Unit & System Test Validation**:
    - Added Module 35 to `tests/test_enemy_stalker.cpp` validating sentinel vigilance, non-fleeing, enrage frenzy, ground slam AOE, charge wall stun vulnerability, and armor breach.
    - Added top-stack boss health bar integration tests in `tests/test_gameplay_mechanics.cpp`.
    - All 22 test suites verified passing cleanly in parallel via `python scripts/tdd.py`.

- **Crouch Stealth Overhaul, Forward Vision Cone Gating & Sneak Attack Criticals (`src/ai/`, `src/entities/enemies/`, `tests/`, `wiki/`)`:
  - **Unalerted Enemy Proximity Gating (Sneaking via Crouch)**:
    - Crouching delvers generate $0\,\text{dB}$ movement noise.
    - Proximity detection when crouching is reduced from omnidirectional $2.2\,\text{m}$ down to actual physical body collision ($< 0.85\,\text{m}$ for Void Stalkers, $< 0.90\,\text{m}$ for Seismic Burrowers).
    - Unalerted enemies in `Idle` or `Dormant` states will no longer detect or aggro on players creeping up behind them or nearby in shadows.
  - **Forward Vision Cone Gating (`AberrantAI::is_in_vision_cone`)**:
    - Implemented 3D surface-tangent forward vector calculation `VoidStalker::forward()` based on current surface normal and orientation yaw.
    - Implemented `AberrantAI::is_in_vision_cone()` enforcing a strict $120^\circ$ field-of-view cone ($\pm 60^\circ$ from facing direction).
    - When crouching, unalerted enemies can only visually spot the player if the player is within line-of-sight, within visual range, **AND** within the creature's forward vision cone. Delvers approaching from behind or the flanks remain completely undetectable.
  - **Roosting Creature Dormancy & Headlamp Awakening**:
    - Roosting stalkers (stationary fauna clinging with closed eyes) do not awaken in the dark when crouch-approached.
    - Roosting creatures strictly awaken upon:
      1. Direct illumination by the delver's headlamp flashlight beam (`s.is_in_light && los`),
      2. Direct physical collision ($< 0.85\,\text{m}$),
      3. Loud acoustic disturbances ($> 70\,\text{dB}$), or
      4. Weapon damage.
  - **Seismic Burrower Subterranean Stealth & Dormancy**:
    - Unalerted subterranean burrowers do not track or lock onto crouching delvers at $18\,\text{m}$ through solid rock.
    - Cavern breaches strictly require line-of-sight within a forward cone or direct physical collision ($< 0.90\,\text{m}$).
    - Dormant burrowers do not awaken when approached at close range ($2.0\,\text{m}$) by a crouching delver.
  - **Sneak Attack Critical Hit System**:
    - Approaching unalerted enemies allows players to execute stealth melee or ranged attacks, triggering guaranteed `SNEAK ATTACK CRITICAL HIT` with a $2.5\times$ damage multiplier (`StealthSystem::SNEAK_ATTACK_CRIT_MULTIPLIER`).
  - **Regression & Unit Test Suite Coverage**:
    - Added Module 32 in `tests/test_enemy_stalker.cpp` validating behind-the-back sneak approach, unalerted sneak crits, forward vision cone detection, and headlamp awakening.
    - Added Test 16 in `tests/test_enemy_burrower.cpp` validating crouched sneak approach to dormant burrowers and sneak attack critical hits.
    - All 22 test suites verified passing via `python scripts/tdd.py`.

## 2026-10-11
- **Precursor Relic 3D Model & Multi-Axis Gyroscopic Spinning Overhaul (`src/voxel/`, `src/graphics/`, `src/core/`, `tests/`, `wiki/`)**:
  - **Decoupled Relic Voxel from Static Greedy Chunk Meshing**:
    - Defined `VOXEL_FLAG_CUSTOM_MODEL` (`0x01`) in [src/voxel/voxel_types.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/voxel_types.hpp).
    - Updated `Voxel::is_renderable()` in [src/voxel/packed_vertex.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/packed_vertex.hpp) to suppress static greedy meshing when `is_custom_model()` is true, while keeping `is_solid() == true`.
    - Preserves exact raycast hit testing (`look_hit.block_pos == mission.vault().relic_pos`), collision, and room samplers while delegating rendering to dynamic PBR passes.
  - **3D Procedural PBR Geometry & Gyroscopic Levitation Animation (`src/graphics/renderer.cpp`)**:
    - **Octagonal Dais & Focusing Prongs**: Rendered stepped dais socket with recessed glowing cyan runic conduits and 4 gilded precursor focusing prongs tipped with luminous cyan emitter crystals.
    - **Dual Counter-Rotating Gimbal Rings**: Outer golden celestial astrolabe ring tilted $+24^\circ$ spinning at $+1.1\,\text{rad/s}$ with 4 quadrature runes; inner obsidian-titanium ring tilted $-32^\circ$ spinning at $-1.6\,\text{rad/s}$ with 4 glowing violet flux nodes.
    - **Floating Faceted Octahedron Hyper-Core**: Smooth vertical hovering ($y_{\text{base}} + 0.54\,\text{m} + 0.065\,\text{m} \times \sin(2.2\,t)$), continuous axial spinning ($\omega = 2.0\,\text{rad/s}$), and white-gold singularity kernel.
    - **Orbiting Satellite Shards & Energy Containment**: 4 satellite crystal spindles revolving at $r=0.44\,\text{m}$ with axial wobble, dual ascending laser containment beams, and expanding drifting energy rings.
  - **Lifecycle Integration & Test Validation**:
    - Hooked `Renderer::render_precursor_relic()` into `Application::render()` whenever the relic is not yet retrieved.
    - Added `--test-relic` CLI test flag to `main.cpp` and `Application` for instant headless showcase capture.
    - Added comprehensive unit test `test_precursor_relic_model_and_geometry()` in `tests/test_level_structural.cpp` validating voxel flags, meshing exclusion, solid raycasting, and retrieval lifecycle.
    - All 22 test suites passing 100% in parallel (~3s) via `python scripts/tdd.py`.

- **Industrial Bulkhead Visual Overhaul & Non-Plastic PBR Shading (`src/graphics/`, `assets/shaders/`, `wiki/`)**:
  - **Eliminated Plastic Sheen & Uncontrolled Specular Glare**:
    - Re-calibrated PBR roughness for `MAT_INDUSTRIAL_BULKHEAD` / `MAT_BULKHEAD`: clamped minimum roughness to $0.68$ (previously $0.45$), yielding a realistic matte cold-rolled alloy finish that scatters point light specular lobes naturally without glassy plastic glare.
    - Reduced headlamp specular factor (`headlampSpecFactor`) from $0.10$ down to $0.05$ on bulkheads, preventing blinding specular washouts under player headlamp spotlight beams.
    - Updated fallback PBR albedo to dark charcoal gunmetal `vec3(0.26, 0.28, 0.32)` and rough/metal parameters to `vec2(0.70, 0.82)`.
  - **Cold-Rolled Alloy Texture Array Generation (`src/graphics/texture_array.cpp`)**:
    - Replaced the tiny 16-rep/voxel unidirectional grate with alternating heavy industrial non-slip diamond tread bars (period 6, perfectly seamless across adjacent voxels).
    - Added true bidirectional bevel normal mapping to tread bar ridges (`d_dist_dx, d_dist_dy`), catching light dynamically on facing edges while shading opposing slopes.
    - Added periodic flush structural anchor bolts with dark oxidized washer counterbore rings (4 per voxel, period 2).
    - Integrated organic cold-rolled microscopic surface normal relief (`steel_micro_x, steel_micro_y`) and brushed steel grain on base plates, eliminating the flat mirror normal vector `(0, 0, 1)` that caused molded-plastic reflections.
  - **Visual Catalog & Telemetry Validation**:
    - Regenerated all 16 level shape catalog captures via `python scripts/capture_level_shapes.py`. Verified in `16_corridor_bulkhead_vault.png` and `05_industrial_vault_bunker.png` that bulkheads exhibit authentic heavy ballistic steel texture with zero plastic artifacts.
    - Verified all 19 test suites pass 100% in ~5s via `python scripts/tdd.py`.

- **Dedicated Precursor Vault Rooms, Level-Scaled Obstacles & Boss Guardians (`src/voxel/`, `tests/`, `wiki/`)**:
  - **3 Dedicated Precursor Vault Chamber Archetypes**:
    - `PrecursorSanctumVault` (Sector 1+): Runic dais, vaulted basalt ceiling arches, glowing titanium portal columns, sealed bulkhead doorway.
    - `SubterraneanVaultBastion` (Sector 2+): Heavily fortified military garrison with perimeter catwalk mezzanine ($y=room.floor\_y+4$), industrial ladder shafts, titanium blast gate, and reinforced bulkhead pylons.
    - `AbyssalVaultChasm` (Sector 3 Exclusive): Colossal 10m drop chasm to bedrock ($y=4$), molten thermite slag & obsidian punji pits, radioactive cliff seams, floating monolithic stepping islands ($y=14$), and isolated boss dais.
  - **Level-Scaled Approach Obstacles**:
    - Sector 1: `AquiferSteppingStones` — Crystal flora moat with a 4-way cross of precursor stepping stones.
    - Sector 2: `HazardTrenchSlag` — Perimeter defense trench filled with razor obsidian spikes and molten slag flumes, crossed via a narrow titanium balance walkway.
    - Sector 3: `ChasmAbyssRadiation` — Floating precursor monolithic stepping islands across a lethal void/radiation chasm.
  - **Level-Scaled Boss Monsters & Guardians**:
    - Sector 1: Stalker Melee Guardian lurking in the sanctum shadows.
    - Sector 2: Stalker Shooter Guardian sniping from the bastion mezzanine catwalks.
    - Sector 3: Chitin Goliath Boss Guardian — 3-meter armored apex terror with high health pool, heavy ground slams causing localized ceiling collapses, and impenetrable chitin plating.
  - **Clean Room Integration & Seamless Navigation**:
    - Fixed corridor-vault doorway collisions: `configure_vault_placement` verifies `has_unconnected_clear_wall` before picking host rooms. If all 4 cardinal walls have active corridors, the vault portal shifts off-center by $+4$ voxels along X so doorways never collide with central corridors.
    - Added elevation diversity check in `LevelGenerator::generate_layout` ensuring at least one connected room pair has differing floor elevations ($y=4$ vs $y=14$), guaranteeing sloping stepped ramps across all seeds.
    - Excluded dedicated vault chambers and vault host rooms from secondary random fluid contamination (`has_random_fluid`), maintaining dry, walkable breach zones.
    - Resolved unanchored floating block bugs in structural check by ensuring downward support pillars convert foundation slabs to full cubes and adding a post-pillar load-bearing sweep in `ensure_world_connectivity`.
  - **Validation & Test Coverage**:
    - Added Module 17 to `tests/test_level_collision.cpp` validating dedicated vault room archetypes, obstacle scaling, and boss guardian roles across multiple procedural seeds.
    - All 19 parallel test suites passing 100% in ~5s via `python scripts/tdd.py`.

- **Subterranean Cave Wall Texturing, Geological Strata Bands & Multi-Tier Clutter (`src/voxel/`, `src/graphics/`, `wiki/`)**:
  - **Organic Harmonic Wall Geometry**:
    - Upgraded `LevelGenerator::compute_room_wall_radius` to incorporate 3D asymmetric harmonic lobes (frequencies 3 and 5), vertical swell / strata ripples ($\sin(0.42y)$), and micro-cragginess, eliminating sterile planar boxiness across all chambers.
    - Updated corridor carving along X, Z, and diagonal axes with 3D noise modulation ($\pm 0.42\,\text{m}$), ensuring organic variation along tunnels while maintaining $\ge 1.8\,\text{m}$ collision clearance.
  - **Sedimentary & Igneous Strata Bands**:
    - Layered subterranean cavern walls in `check_doorway_or_solid` into wavy geological strata bands: dark volcanic basalt (`MAT_VOLCANIC_BASALT`), fractured granite (`MAT_FRACTURED_GRANITE`), and ancient bedrock (`MAT_DREDGE_BEDROCK`).
    - Added high-frequency spatial hashing for crumbly vs tough rock inclusions and wall-base moisture lichen/moss (`MAT_BIOLUMINESCENT_FLORA`).
  - **Multi-Tier Surface & Wall-Mounted Clutter**:
    - Added multi-tier scale variation for floor scree/rubble ($0.32\times$, $0.62\times$, $1.15\times$) and ceiling stalactites ($0.35\times$, $0.60\times$, $1.05\times$).
    - Implemented vertical **Wall-Mounted Clutter Pass** scanning exposed perimeter faces ($+X, -X, +Z, -Z$), anchoring protruding rock spurs, downward wall stalactite dripstones, and tinted mineral crystals aligned with wall normals.

## 2026-10-10
- **On-Screen Text Notices & Crisis Banner Removal — Full Audiovisual Immersion Transition (`src/ui/`, `src/core/`, `wiki/`)**:
  - **Eliminated On-Screen Tactical Notification & Warning Stack**:
    - Removed Section 9 (`ON-SCREEN TACTICAL NOTIFICATIONS STACK`) card rendering in `HUD::render()` (`src/ui/hud.cpp`). The persistent text popup cards at the top of the screen that previously displayed game notices are eliminated, keeping the field of view clean, atmospheric, and immersive.
    - Removed Section 7a (`PRIMARY ENVIRONMENTAL CRISIS BANNER`) in `HUD::render()` (`! ACTIVE SEISMIC TREMOR IN PROGRESS !`), removing disruptive arcade-style full-screen warning banners during tremors.
  - **Purged Gameplay Text Warning Callbacks**:
    - Disconnected `m_player->set_on_warning` and `m_mission.set_on_notification` callbacks in `Application::init()` (`src/core/application.cpp`).
    - Removed spammy text warning triggers across all delver gameplay events:
      - Flare deployment, seismic tremors, ceiling spall, burrower sub-terrain detection.
      - Proximity pre-damage notifications for ionizing radiation, toxic bio-gas vents, obsidian spikes, and molten magma.
      - Low thruster power alerts, toxic gas inhalation notices, room biome entry advisories, and combat kill confirmations.
      - Extraction holdout wave notices (`! HOSTILE SCOUT INBOUND !`, `!! APEX WAVE !!`, `!!! EVAC POD INBOUND !!!`) and beacon locked notifications.
  - **Sensory Grounding via Audio, Visuals & Environmental Events**:
    - **Audio Feedback**: The game now exclusively relies on its rich 3D spatial soundscape (`AudioSystem` / `SoundCue`):
      - Proximity hazards telegraphed via Geiger counter clicking (`update_hazard_proximity_audio`), boiling lava murmurs, hissing gas pockets, and sub-frequency hums.
      - Low health & combat tension communicated through dynamic cardiac biometric heartbeats (`update_biometrics`), delver vocalization/asphyxiation groans, and beast growls.
      - Extraction holdouts telegraphed by roaring thruster descent acoustics, mechanical beacon deployment audio, and tremor rumbling.
    - **Visual & Physical Cues**:
      - Environmental crises telegraphed by volumetric fog density shifts, particle embers, screen trauma/camera shudder (`add_trauma`), falling ceiling spall debris, and subtle chromatic damage flashes.
      - Vital suit status and weapon state remain readable at a glance via the sleek, compact corner HUD gauges (Vitals, Dosimeter, Radar, Noise Meter, Inventory Quota).
  - **Validation & Test Compatibility**:
    - Preserved underlying data structures (`m_notifications`, `compute_top_stack_layout`) so headless layout unit tests continue to validate layout math without regression.
    - All 18 test suites passed in parallel with zero regressions.

- **Room Adjacency Constraints, Thematic Continuity & Contextual Selection Rules (`src/voxel/`, `tests/`, `wiki/`)**:
  - **Zero Adjacent Same-Type Hard Guarantee**:
    - Enforced strict 100% hard constraint in `LevelGenerator::generate_layout()` preventing two orthogonally adjacent rooms from ever sharing the same `RoomShapeType`.
    - Added heavy penalty for diagonal same-type rooms, eliminating repetitive room clusters across the subterranean grid.
  - **Thematic Categorization (`RoomCategory`) & Contextual Affinity Scoring**:
    - Classified all 27 room archetypes into 7 distinct thematic categories (`SafeTerminal`, `NaturalCrystalline`, `BioOrganic`, `PrecursorIndustrial`, `VolcanicThermal`, `TectonicChasm`, `CosmicVoidAbyss`).
    - Implemented `get_room_adjacency_affinity(typeA, typeB)` evaluating geological and atmospheric transitions between neighboring rooms (e.g., thermal to coolant/water affinity $+16.0$, volcanic to basalt tectonic $+10.0$, bio-organic to crystal grotto $+14.0$).
  - **Placement Limits & Safety Boundaries**:
    - *Spawn Safety*: Extreme hazard rooms (`MagmaCalderaLake`, `MoltenMagmaFoundry`, `VoidSingularityRift`, `SpikeTrenchArena`, `RadioactiveCoreSanctuary`) are strictly prevented from spawning adjacent to spawn cavern $(0, 0)$.
    - *Harsh Hazard Anti-Clustering*: Harsh hazard rooms can be adjacent to at most 1 other harsh hazard room.
    - *Category Diversity*: Limits adjacent rooms of the same category to $\le 2$.
    - *Global Archetype Caps*: Standard rooms capped at $\le 2$ instances per level; colossal/massive landmarks capped at $\le 1$ instance per level.
  - **Wavefront Constraint Satisfaction Solver**:
    - Replaced unconstrained random archetype rolls with Manhattan-distance wavefront traversal outward from spawn $(0,0)$.
    - Evaluates candidates against hard limits and scores them via combined heuristic weights (`base_affinity * 1.5 + seed_variation - repetition_penalties`), with progressive fallback relaxation.
  - **Test Suite & Validation**:
    - Added Module 14 to `tests/test_level_collision.cpp` validating 90 generated level layouts across Sectors 1-3 with 0 adjacent same-type violations and 100% compliance with adjacency rules.
    - All 18 parallel test suites passing cleanly via `python scripts/tdd.py`.

- **Wall-Embedded Objective Deposit Architecture & Surface Discovery Guarantee (`src/voxel/`, `tests/`, `wiki/`)**:
  - **Eliminated Free-Floating Objective Crystals**:
    - Previously, candidate spawn offsets placed objective voxels inside rooms before room shape carving, occasionally leaving purple Voidite blocks floating in open air or obstructing floor walkways.
    - Updated `LevelGenerator::add_objective_voxel()` to strictly reject any position that is `MAT_AIR`, fluid, or bedrock, enforcing that objective crystals can only replace solid mineable base wall or structural voxels (`is_mineable_solid()`).
  - **Surface Exposure & Inward Vein Growth (`find_wall_surface_voxel()`)**:
    - Implemented `LevelGenerator::find_wall_surface_voxel()` to anchor every deposit directly onto the solid rock face of perimeter walls, columns, quarry benches, or ledges bordering room `MAT_AIR`.
    - **Surface Visibility Guarantee**: At least one piece of each vein is positioned flush on the wall surface with an exposed face adjacent to room air, ensuring delvers can visually spot the emissive purple crystal outcrop when exploring the chamber.
    - **Natural Inward Vein Growth**: Multi-block deposits ($N > 1$) extend inward into the solid rock matrix along the wall's inward normal and lateral tangents behind the surface block, creating realistic, authentic ore veins embedded in the cavern walls that delvers excavate by drilling into the rock face.
  - **Base Geometry Decoupling (`sample_base_voxel`)**:
    - Introduced `LevelGenerator::sample_base_voxel()` to query true room and terrain geometry prior to objective injection, allowing robust wall finding and boundary validation.
    - Updated `get_room_at_position()` with a secondary 2-voxel wall buffer so perimeter wall deposits are seamlessly attributed to their respective parent rooms.
  - **Comprehensive Test Suite Validation**:
    - Added Module 12.9.5 in `tests/test_level_collision.cpp` asserting that zero objective voxels spawn in `MAT_AIR` and every room containing objective resources possesses an exposed vein surface piece facing room air across all sectors and seeds.
    - All 18 parallel test suites passing cleanly via `python scripts/tdd.py`.

- **Scalable Material Scarcity System, Late-Level Astral Ore & Pure Single-Ore Veins (`src/voxel/`, `src/player/`, `src/core/`, `src/ui/`, `tests/`, `wiki/`)**:
  - **New Late-Level Material: Astral Ore (`MAT_ASTRAL_ORE = 17`)**:
    - Introduced mythic Grade 5 material `MAT_ASTRAL_ORE` (alias `MAT_ASTRAL`, `MAT_ASTRAL_LUMEN`) unlocked in Sector 3+.
    - Added procedural PBR 2D texture generation in layer 17 of `texture_array.cpp` with golden starlight streaks (`#FFE066`), diamond specular flecks, and warm emissive glow (`er=255, eg=220, eb=120`).
    - Integrated with block break FX particles, emissive meshing, spectrometer HUD target prompt (`"[EXOTIC ASTRAL ORE]"`), and surveying sonar detection.
    - Added `astral_ore` inventory tracking, sector expedition quotas (`target_astral_ore`), evacuation lock checks, score multiplier (+250 XP, +100 gold bonus per core), and persistent profile save/load in `save_system.cpp`.
  - **Scalable Material Scarcity Architecture (`MaterialScarcitySystem`)**:
    - Created extensible registry mapping each material to its `MaterialGrade` (Common through Mythic), `unlock_sector`, `base_scarcity`, `scarcity_decay_per_sector`, and `min_scarcity`.
    - **Progression Dynamics**: In earlier sectors, high-grade materials are 100% locked ($S = 1.0$). As delvers reach deeper sectors, the scarcity rate of previously unlocked ores smoothly decays, rendering early materials (Titanium, Voidite) more common and accessible, while higher-grade materials (Radioactive, Prismatic, and Astral Ore) maintain high scarcity rates ($S > 0.75$).
    - Abundance spawn weights ($W = \max(0, 1 - S)$) cleanly drive procedural ore vein selection.
  - **Pure Single-Ore Vein Invariant**:
    - Procedural host rock veins cluster into coherent 3D cells `(x/3, y/2, z/3)`.
    - Vein ore type is sampled strictly once per cluster (`hash_coord(cell_x, cell_y, cell_z, 4513)`), guaranteeing 100% ore purity within any vein (no mixed-ore clusters).
    - Added `sample_voxel(x, y, z, &is_vein)` and `is_vein_voxel(x, y, z)` to `LevelGenerator` for exact, zero-overhead vein identification without desync.
  - **Comprehensive TDD Validation**:
    - Validated in `tests/test_level_collision.cpp` Module 13 and `tests/test_unit_all.cpp` Module 18; all 18 test suites passing in parallel.

- **Visual Decoupling: Room Decorations & Surface Clutter vs Objective Minerals (`src/voxel/`, `src/graphics/`, `src/ui/`, `tests/`)**:
  - **Natural & Architectural Material Realism in Room Shapes**:
    - Replaced pseudo-crystal blocks across natural and architectural room features with appropriate structural materials (`MAT_VOLCANIC_BASALT`, `MAT_FRACTURED_GRANITE`, `MAT_TITANIUM`, `MAT_BIOLUMINESCENT_FLORA`):
      - Quarry excavation floors (`sample_terraced_quarry`) now use basalt.
      - Support column bands and bridge keystone (`sample_pillar_hall`) now use structural titanium alloy.
      - Caldera volcanic chimney (`sample_magma_caldera`) now uses solid volcanic basalt.
      - Moisture stalactites and oasis banks (`sample_aquifer_oasis`) now use basalt and bioluminescent moss flora.
      - Crevasse grapple stalactite (`sample_fault_crevasse`) now uses anchored basalt.
      - Wall ore veins in host rock matrix (`sample_point`) now generate titanium, radioactive ore, or flora, eliminating false crystal signals in transit tunnels.
  - **Decorative Crystal Visual Distinction (`MAT_PRISMATIC_CRYSTAL`)**:
    - Overhauled texture generation in `texture_array.cpp` to an icy radiant diamond cyan / aquamarine palette (`#4DEEEA`) with sharp white specular refraction ridges and cyan luminescence (`er=25, eg=205, eb=245`).
    - Updated PBR fallback albedo in `voxel_pbr.frag` to radiant cyan `vec3(0.25, 0.92, 0.96)`, completely removing all purple/violet undertones so players never confuse decorative crystals with objective Voidite (`MAT_VOIDITE_CRYSTAL`, `vec3(0.48, 0.12, 0.72)`).
  - **Dedicated Clutter System Archetypes (`ClutterType::PrismaticCrystal`)**:
    - Added `ClutterType::PrismaticCrystal` for decorative diamond crystals, featuring multi-faceted icy cyan diamond spires with sparkling white apex crests (`glm::vec4(0.25f, 0.92f, 0.98f, 1.0f)`).
    - `ClutterType::VoiditeCrystal` is strictly reserved for objective Voidite blocks, preserving its signature purple amethyst spires.
    - Clutter break particles emit cyan diamond shards for decorative crystals and purple shards for objective Voidite.
  - **HUD & Scanner Differentiation**:
    - Target hover prompt clearly distinguishes `[LMB] DRILL VOIDITE (OBJECTIVE)` in violet (`COLOR_PURPLE`) from `[LMB] SALVAGE PRISMATIC (DECORATIVE)` in cyan (`COLOR_CYAN`).
    - Scanner clusters and compass ribbon label `[OBJ VOIDITE: Xm]` in magenta vs `[DECORATIVE PRISMATIC: Xm]` in cyan.

- **Procedural Level Generation: Objective Material Budgeting, Room Shape Decoupling & Scarcity Validation (`src/voxel/`, `src/core/`, `tests/test_level_collision.cpp`, `wiki/`)**:
  - **Decoupling Objective Materials from Fixed Room Shapes**:
    - Removed hardcoded objective material (`MAT_VOIDITE_CRYSTAL`) generation across all 27 room shape samplers (geodes, pillar halls, cathedrals, firmament abyss, training niche, and corridors), replacing them with decorative crystals (`MAT_PRISMATIC_CRYSTAL`, `MAT_TITANIUM`, `MAT_BIOLUMINESCENT_FLORA`).
    - Decoupled `MAT_PRISMATIC_CRYSTAL` mining rewards in `application.cpp` to grant rare salvage (+10 Salvage, +50 pts) instead of Voidite, ensuring only genuine `MAT_VOIDITE_CRYSTAL` deposits advance the level objective quota.
    - Prevents single room types from acting as "jackpot" resource caches that immediately fulfilled quotas without requiring cave exploration.
  - **Budgeted, Scarce Objective Resource Placement (`place_objective_resources`)**:
    - Quota-Aware Generation: The level quota target is passed directly to `LevelGenerator(sector, seed, vertical_chunks, target_objective)`.
    - Tight Budget Cap: Total spawned objective voxels across the entire cave system are capped strictly to $\text{target} + 2$ (at most $\text{target} + 3$), eliminating over-abundance while ensuring guaranteed completability if a block is lost to hazards.
    - Zero Near-Spawn & Zero-Extraction Gating: Distance 0 (Spawn Cavern), Distance 1 (Adjacent Rooms), and the Extraction Landing Bay have 0 candidate spawn offsets and strictly 0 spawned crystals.
    - Empty Rooms Invariant ($40\%\dots 52\%$ of All Rooms): Rather than scattering crystals uniformly into every room, distant rooms receive deposits up to per-room capacity ($2\dots 4$ voxels, up to 5 in Sector 1) until the quota budget is satisfied. Subsequent distant chambers receive 0 objective materials (`rooms_with_zero_objective_count()`), forcing players to explore the cavern system.
    - Pre-Determined Candidate Spawn Nodes across All 27 Archetypes: Placed in tailored natural wall crevices, high ledges, pillar flanks, and corner alcoves via `get_room_candidate_spawn_offsets()`.
    - Small Vein Granularity: Objective deposits spawn in tight veins of 2 to 3 voxels, distributing resources across distant rooms ($D \ge 2$) with no room exceeding 4-5 voxels. Players are forced to venture into multiple distant rooms to complete the level.
  - **Room-Level & Level-Wide Validation Checks (`validate_room_resources`, `validate_level_resources`)**:
    - `validate_room_resources(room)` returns a `RoomValidationReport` auditing each room for zero near-spawn/extraction resources and maximum allowed capacity.
    - `validate_level_resources()` enforces total quota completability ($S \ge T$), strict scarcity ceiling ($S \le T + 3$), presence of multiple zero-resource rooms, and multi-room spread.
    - Guaranteed non-hanging safety fallback and scarcity ceiling trimmer.
  - **High-Performance Bitset Indexing**:
    - $O(1)$ branchless bitset spatial lookup (`m_objective_bitset`) for instant sampling in `sample_voxel()`.
  - **Ultra-Fast Parallel TDD Runner (`scripts/tdd.py`)**:
    - Enabled `--parallel` (multi-core `/m` MSBuild) in CMake build command, slashing build time from ~40s to ~2.7s.
    - All 18 test suites execute in parallel in ~3.1s.

- **Removal of Carry Weight & Overburden Encumbrance Mechanics (`src/player/loadout.hpp`, `src/ui/hud.cpp`, `src/ui/pause_menu.cpp`, `tests/test_unit_all.cpp`)**:
  - **Unrestricted Inventory Capacity**:
    - Removed carry weight calculation and encumbrance penalties from `PlayerInventory`. `carry_weight()` is locked to $0.0\,\text{kg}$, `is_overburdened()` always returns `false`, and `overburden_penalty()` returns an unencumbered $1.0\times$ speed multiplier across all contractor archetypes.
    - Delvers maintain $100\%$ base movement velocity and standard jetpack fuel consumption regardless of mined mineral volume or relic recovery.
  - **HUD & UI Refinement**:
    - **In-Game HUD (`src/ui/hud.cpp`)**: Replaced the cargo `LOAD: X/Y KG` gauge in the secondary mineral row with a direct `SALVAGE: X` count, keeping resource telemetry focused on actionable crafting materials.
    - **Pause Menu Cargo Rig (`src/ui/pause_menu.cpp`)**: Removed weight progress bars, overburden warning alerts, and per-item kilogram annotations. Replaced with clean unit tallies (`X UNITS`) and an "Unrestricted Capacity" mobility status indicator.
  - **Unit Test Matrix (`tests/test_unit_all.cpp`)**:
    - Updated Module 11 unit tests to validate unrestricted inventory hauls and verify that no class experiences overburden penalties upon mineral or relic acquisition.

- **Realistic Projectile Ballistics, Orthonormal 3D Meshes & Shooter Velocity Inheritance (`src/player/`, `src/entities/`, `src/graphics/`, `src/core/`)**:
  - **Shooter Platform Velocity Inheritance (Galilean Invariance)**:
    - Updated `PlayerController::try_fire_weapon()` to add player platform velocity (`m_velocity`) to projectile velocity (`bolt.velocity = fire_dir * speed + m_velocity`).
    - Eliminates lateral reticle drift when strafing sideways: from the player's moving frame of reference, bullets travel straight along the forward sightline rather than dragging horizontally across the screen as an artificial sideways bar.
    - Updated `VoidStalker` spine launches to inherit stalker movement velocity (`s.velocity * 0.4f`), providing realistic ballistic lead and trajectory fidelity during leaping/circling attacks.
  - **Orthonormal Projectile Orientation Matrix (`src/graphics/renderer.cpp`)**:
    - Replaced flawed Euler yaw/pitch rotation (which suffered from inverted pitch signs, gimbal lock, and mirroring across XZ planes) with `make_projectile_matrix()`.
    - Constructs an exact orthonormal coordinate basis $([\text{right}, \text{up}, \text{forward}, \text{pos}])$ aligning local $+Z$ directly along the velocity vector, guaranteeing mathematically perfect orientation along true 3D trajectories at all pitch and yaw angles.
  - **Faceted Aerodynamic 3D Projectile Meshes & Volumetric Trails**:
    - **Delver Plasma Bolts (`add_plasma_bolt_mesh`)**: Replaced blocky Minecraft-style cuboids with a 6-sided faceted nose cone, cylindrical PBR energy sheath, boat-tail exhaust nozzle, 4 stabilizing aerodynamic ion vanes, white-hot emissive core spindle ($Z = 8.5\text{f}$ bloom), and a 3D volumetric cross-quad energy wake ribbon trailing behind.
    - **Void Stalker Spines (`add_void_spike_mesh`)**: Replaced generic cuboid with a virulent bio-crystalline quill featuring a hexagonal diamond apex needle, backward-swept recurved chitin barbs, radiant toxic emerald core spindle, and 3D volumetric toxic vapor wake ribbon.
  - **Dynamic Cavern Illumination (Real-Time Point Lights)**:
    - Added dynamic point light illumination for active plasma bolts (electric cyan/orange, radius $6.0\text{m}$, intensity $2.8\text{f}$) and void spine projectiles (toxic emerald, radius $5.0\text{m}$, intensity $2.5\text{f}$) in `Application::render_world()`, casting real-time light onto dark subterranean cave walls and tunnels.
  - **Regression Test Suite (`tests/test_gameplay_mechanics.cpp`)**:
    - Added `ProjectileBallisticsAndShooterVelocityInheritanceTest` validating stationary muzzle speed, lateral strafing momentum preservation with zero relative drift across reticle, and vertical jump inheritance.

- **Procedural Fluid Structures, Wall Cascades, Hazard Trenches & Drop Sumps (`src/voxel/level_shapes.hpp`, `src/voxel/level_shapes.cpp`, `tests/test_fluid_geometry.cpp`)**:
  - **Dynamic Fluid Structure Archetypes**:
    - Replaced uniform static fluid pools with six distinct procedural fluid structure types (`FluidStructureType`): `ContainedPool`, `SteppingStonePool`, `WallCascade`, `CascadingTwinPools`, `HazardTrench`, and `DropChasmSump`.
    - **`WallCascade`**: Generates vertical stone flume buttresses rising up the wall to an emissive crystal or bulkhead discharge spout, pouring down into an enclosed plunge basin with localized acoustic spray.
    - **`CascadingTwinPools`**: Generates dual interconnected basins in an hourglass/dumbbell configuration separated by a solid stone weir ridge and traversal bridge.
    - **`HazardTrench`**: Generates elongated security/cooling trenches (coolant, molten slag, or acid) cutting across rooms with midpoint crossing stepping stones to serve as tactical environmental obstacles.
    - **`SteppingStonePool`**: Generates hazardous pools with elevated prize/mineral plinths and stepping stone paths.
    - **`DropChasmSump`**: Generates vast sump reservoirs at the base of deep abyssal chasms, colossal drop pits, and tectonic sinkholes with central extraction/refinery platforms.
  - **Watertight Invariant & Sleep on Spawn**:
    - Formulated mathematical containment ensuring every liquid voxel has solid/non-air support beneath, complete cardinal lateral containment on all four horizontal sides, and raised solid retaining rims exceeding liquid surface height.
    - Guaranteed room centers and cardinal doorway corridors remain dry and unobstructed for navigation.
    - Maintained 0 ms idle CPU cost on spawn across all 50 seeds in `test_fluid_geometry.cpp`.

## 2026-10-09
- **Two-Tier Delver Defense: Recharging Kinetic Shield Layer & Non-Regenerating Hitpoints (`src/player/`, `src/ui/`, `wiki/`)**:
  - **Kinetic Shield Layer Architecture (`src/player/controller.hpp`, `src/player/controller.cpp`, `src/player/character_class.hpp`)**:
    - Added `baseShield` attribute to `CharacterAttributes` and initialized per archetype: Demolitionist ($50\,\text{SP}$), Vanguard ($75\,\text{SP}$), and Scout ($40\,\text{SP}$).
    - Integrated `m_shield` and `m_max_shield` into `PlayerController` and `ExoStatus.shield`.
    - In `PlayerController::take_damage()`, all incoming damage sources (kinetic attacks, enemy melee, falling debris, spikes, and environmental hazards) are absorbed by the shield layer first.
    - If damage exceeds the current shield capacity, the shield breaks/depletes to $0\,\text{SP}$, and the remaining overflow punctures into suit integrity hitpoints.
    - Automated out-of-combat shield recharge: after $4.0\,\text{s}$ without taking damage (`m_shield_recharge_delay`), the shield automatically recharges at $+15.0\,\text{SP/s}$ (`m_shield_recharge_rate`) until reaching maximum capacity. Taking damage resets the timer immediately to $0\,\text{s}$ to interrupt recharge.
  - **Permanent Hitpoints (Zero Health Regeneration)**:
    - Completely deprecated and removed the legacy out-of-combat nanite triage health recuperation. Once hitpoints take damage, they cannot be regenerated automatically.
    - Fatal damage ($0\,\text{HP}$) prevents dead delvers from recharging shields.
  - **Vitals HUD Gauge Modernization (`src/ui/hud.cpp`)**:
    - Expanded bottom-left Vitals HUD panel to a 3-tier stacked layout displaying:
      1. `KINETIC SHIELD`: Electric-cyan bar with percentage and pulsing `[REGEN]` tag when recharging out of combat.
      2. `SUIT INTEGRITY`: High-visibility green/crimson bar reflecting unregenerated core hitpoints.
      3. `THRUST POWER`: Cyan bar showing thruster energy.
    - Color-coded left status indicator accent strip dynamically reflecting shield active (cyan), unshielded caution (amber), and critical health (crimson).
  - **Regression & Unit Testing (`tests/test_gameplay_mechanics.cpp`, `tests/test_progression.cpp`, `tests/test_e2e_expeditions.cpp`, `tests/test_level_collision.cpp`)**:
    - Added dedicated test `PlayerShieldLayerRechargeAndNonRegeneratingHealthTest` verifying shield absorption, overflow damage, continuous damage recharge suppression, 4.0s out-of-combat recharge curve, permanent health damage over 60s idle time, and fatality suppression.
    - Updated regression suites to validate shield layer mechanics and unshielded direct health damage.

## 2026-10-08
- **Optimized Event-Driven Liquid Physics, Sub-Block Waterlogging & Slope Conformance (`src/voxel/`, `src/graphics/`, `src/player/`)**:
  - **Liquid Metadata & Waterlogging Allocation (`src/voxel/chunk.hpp`, `src/voxel/voxel_types.hpp`, `src/voxel/packed_vertex.hpp`)**:
    - Allocated bit 2 (`0x04`) in `flags_and_damage` as `VOXEL_FLAG_WATERLOGGED`.
    - Pure liquid blocks (`MAT_WATER`, `MAT_ACID`, `MAT_COOLANT`) use bits `[2:0]` (`VOXEL_FLUID_LEVEL_MASK`) to encode fluid levels 1 to 5 (5 = full/source block, 1 = shallow terminal trickle).
    - Added helper methods `IsLiquid()`, `IsWaterlogged()`, `SetWaterlogged()`, `fluid_level()`, and `set_fluid_level()`.
  - **Active Queue & Event-Driven Cellular Automaton (`src/voxel/fluid_sim.hpp`, `src/voxel/fluid_sim.cpp`, `src/voxel/world.cpp`)**:
    - Implemented active double-ended queue `std::deque<glm::ivec3> m_activeFluids` with `std::unordered_set<uint64_t> m_activeFluidSet` for $O(1)$ spatial coordinate deduplication via `FluidSim::PackPos`.
    - Completely eliminates CPU simulation overhead for settled lakes and enclosed pools (0 ticks while asleep).
    - Wake-up hooks integrated in `World::DestroyBlock` and `World::set_voxel`: whenever a block is broken or becomes `MAT_AIR`, orthogonal neighbors are inspected and wake adjacent fluids automatically into the active queue.
    - Fixed-rate 12 Hz simulation tick with `MAX_FLUID_STEPS_PER_TICK = 192` prevents frame spikes during large cavern reservoir breaches.
    - Automaton rules: pure downward gravity without lateral spreading while falling, horizontal spreading with linear level decay ($L - 1$) terminating at $L = 1$, and sub-block ramp/slab waterlogging with downhill cascade.
    - Dedicated `MeshLiquidPass` handling pure liquids, waterlogged slabs, and waterlogged ramps with neighbor face culling against identical fluids.
    - Greedily merges coplanar horizontal top faces of liquid blocks (both pure liquid and waterlogged bottom slabs) into rectangular strips to minimize vertex count.
    - Waterlogged slabs (`SHAPE_SLAB_BOTTOM` + `VOXEL_FLAG_WATERLOGGED`) render a top liquid plane at $Y = 0.5\text{m}$.
    - Waterlogged ramps (`SHAPE_RAMP_*` + `VOXEL_FLAG_WATERLOGGED`) render a $45^\circ$ diagonal liquid plane offset by $+0.04\text{m}$ (via vertex bit 27 `water_offset` unpacked in vertex shader) to eliminate Z-fighting against the underlying rock ramp geometry.
    - Vertical sealing quads eliminate air voids adjacent to ramp boundaries.
  - **Seamless Liquid Texturing & Shader Exemption (`assets/shaders/voxel_pbr.frag`)**:
    - Exempted liquids (`MAT_WATER`, `MAT_ACID`, `MAT_COOLANT`) from procedural edge chamfering and rock surface noise grain, eliminating scalloped block tiles.
    - Continuous horizontal world-space UV mapping for liquid top surfaces (`fluidUV = v_FragPos.xz * 0.25`) seamlessly animates caustics, wave ripples, and magma convection across block boundaries.
  - **Engine Performance & Meshing Guardrails (`src/voxel/world.cpp`)**:
    - Chunks modified during fluid ticks are gathered in `m_fluidDirtyChunks` and marked dirty at tick completion to avoid duplicate re-meshing requests.
    - Enforced `MAX_CHUNK_UPLOADS_PER_FRAME = 2` GPU upload budget to guarantee smooth 60+ FPS pacing.
  - **Player Controller Submersion Integration (`src/player/controller.cpp`)**:
    - Updated underfoot wading and bounding box submersion checks to detect `is_waterlogged()` ramps and slabs and evaluate partial immersion depth from fluid levels.

## 2026-10-07
- **Smooth Ramp Traversal Physics & Slope Normal Projection (`src/player/controller.cpp`, `src/player/controller.hpp`)**:
  - **Step-Up & Ramp Motion**:
    - In `PlayerController::UpdateMovement(float dt, World& world)`, implemented dynamic footprint and motion-leading voxel shape query beneath the player's feet.
    - If standing on or approaching a `SHAPE_RAMP_*` block, calculates the exact slope surface height `surf_y = calculate_ramp_surface_y(...)` and slope normal `N = get_ramp_normal(...)`.
    - Projects planar ground velocity along the $45^\circ$ slope normal tangent condition: $v_y = -(v_x N_x + v_z N_z) / N_y$, matching vertical velocity to slope ascension/descension.
    - Clamps the player's vertical position smoothly to the slope surface without triggering micro-airborne state transitions or jump-stutter.
  - **AABB Collision Resolution & Ramp Depenetration Overhaul**:
    - Updated `resolve_axis_collision` along X and Z axes: ramp blocks are recognized as traversable slopes rather than vertical solid walls when within stepping range (`box_min.y >= surf_y - 0.60f`). Smoothly steps up to `surf_y + half_extents.y` and continues uninterrupted movement.
    - Overhauled `depenetrate()`: when overlapping a ramp voxel, resolves penetration vertically onto the slope surface instead of erroneously pushing the player horizontally backwards.
    - Preserved `m_on_ground = true` and `m_isGrounded = true` during ramp climbing in `resolve_voxel_collisions` ground support probe, eliminating airborne fall-gravity interruptions.
- **Distance-Gated POM & Zero-Cost Procedural Edge Chamfering (`assets/shaders/voxel_pbr.frag`)**:
  - **Zero-Cost Procedural Edge Chamfering**:
    - Eliminated razor-sharp digital 90° edges on cubic voxel faces without geometry subdivision.
    - Evaluated screen-space derivatives along quad boundaries using `edgeDist = min(v_TexCoords, 1.0 - v_TexCoords)`.
    - Softened normal away from face perpendicular via `smoothstep(0.0, 0.05, minEdge)` and `normalize(mix(normal + dFdx(v_FragPos) * 0.15 + dFdy(v_FragPos) * 0.15, normal, bevelFactor))`, smoothing specular glints along block edges.
  - **LOD Distance-Gated Parallax Occlusion Mapping (POM)**:
    - Computed Euclidean camera distance `dist = length(v_FragPos - u_CameraPos)`.
    - Gated POM raymarch execution strictly to `dist < 7.0f`: performs 8-step raymarch using height map layer from `GL_TEXTURE_2D_ARRAY` (`uNormalArray.a`), smoothly attenuating displacement scale to zero between 5.0m and 7.0m.
    - For `dist >= 7.0f`, completely bypasses raymarching loops and samples base UVs directly to preserve fillrate across open cavern expanses.
- **Sloped Greedy Meshing & Diagonal Face Generation (`src/voxel/greedy_mesher.cpp`)**:
  - **Diagonal Face Generation**:
    - When meshing voxels with `SHAPE_RAMP_*`, replaced the dual emission of top horizontal (+Y) and lower vertical step quads with a single $45^\circ$ diagonal plane between the lower and upper step edges.
    - Implemented `GreedyMesher::calculate_diagonal_normal(VoxelShape shape)` calculating $\vec{N} = \text{normalize}(\vec{N}_{\text{base}} + \vec{N}_{\text{side}})$ for all four directional ramp shapes (`POS_X`, `NEG_X`, `POS_Z`, `NEG_Z`).
    - Maintained counter-clockwise outward winding order across all diagonal quads.
  - **Greedy Merging on Slopes**:
    - Greedily merges adjacent, coplanar ramp voxels along the perpendicular axis ($Z$ for X-slopes, $X$ for Z-slopes) into single continuous diagonal strips ($u\_dim = L, v\_dim = 1$), dramatically reducing vertex and triangle count.
  - **Boundary Culling Invariant**:
    - Preserved boundary check invariant where ungenerated or missing neighbor chunks are treated as solid granite (`MAT_FRACTURED_GRANITE`), preventing ceiling and border void leaks.
    - Validated through unit tests in `tests/test_unit_all.cpp`.
- **Voxel Ramp & Slope Metadata & Procedural Generation Integration**:
  - **Metadata Encoding (`src/voxel/voxel_types.hpp`, `src/voxel/packed_vertex.hpp`, `src/voxel/chunk.hpp`)**:
    - Created [src/voxel/voxel_types.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/voxel_types.hpp) defining `VoxelShape` topology enums: `SHAPE_CUBE` (`0x00`), `SHAPE_RAMP_POS_X` (`0x10`), `SHAPE_RAMP_NEG_X` (`0x20`), `SHAPE_RAMP_POS_Z` (`0x30`), and `SHAPE_RAMP_NEG_Z` (`0x40`).
    - Enforced zero-overhead memory footprint: encoded shape topologies into the upper nibble of `flags_and_damage` (`0xF0`), keeping `sizeof(Voxel) == 2` bytes (16-bit packed).
    - Preserved lower nibble damage tiers (`0x0F`) and backwards compatibility for anchor/surveyed bit checks (`!is_ramp()`).
    - Added shape comparison guards in [src/voxel/greedy_mesher.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/greedy_mesher.cpp) to prevent differing voxel shapes from merging into flat coplanar quads.
  - **Procedural Floor Step Detection (`src/voxel/world.cpp`, `src/voxel/world.hpp`)**:
    - Added `World::detect_natural_floor_steps()` pass called during level generation (`generate_world` & `set_level_generator`).
    - Detected natural stone floor steps: checks walkable stone floor voxels (`MAT_FRACTURED_GRANITE`, `MAT_VOLCANIC_BASALT`, `MAT_DREDGE_BEDROCK`) with air above. If an isolated adjacent neighbor step rises at `Y + 1` with open headroom, flags the block with the corresponding directional ramp shape (`SHAPE_RAMP_POS_X`, `NEG_X`, `POS_Z`, `NEG_Z`).
    - Validated in `tests/test_unit_all.cpp` with dedicated unit test coverage.
- **Combat Ballistics & Enemy AI Motion Continuity (Gunshot Non-Stun Enforcement)**:
  - **Identified Failure Mode**:
    - When shooting enemies, players observed creatures halting their forward movement as if stunned.
    - Two root causes were identified:
      1. Ballistic projectile impacts on enemy bodies were erroneously emitting `SoundEventType::BulletImpact` at the enemy's coordinates. Stalkers in `Stalking` or `Circling` states interpreted this sound as an acoustic distraction (>4.5m from the player) and transitioned to `Investigating` their own position, setting `velocity = 0` for 5 seconds.
      2. Heavy weapon damage (>20 HP) assigned `nearest->stun_timer = 0.25f`, and repeated hits on lunging/stalking enemies repeatedly reset `state_timer = 0.0f`, forcing `lunge_ramp` to 0 and zeroing out forward speed.
  - **Resolution & Fix**:
    - **Acoustic Decoupling on Entity Hits**: Suppressed wall-impact acoustic distraction events when bullets hit enemy entities in `src/core/application.cpp`.
    - **Engagement Distraction Immunity**: Enforced that active combat pursuit (`is_pursuing_attacker`) and sounds located at the entity's own body (`dist_to_sound <= 2.0m`) never distract enemies into `Investigating` in `src/entities/enemies/void_stalker.cpp`.
    - **Zero Gunshot Stun Policy**: Removed `stun_timer` assignment on weapon hits in `damage_nearest`. Stun is strictly reserved for Sonar Pulse, Repulsor Blasts, and Burrower bedrock collisions.
    - **Momentum & Timer Continuity**: Ensured subsequent gunshots during an active lunge maintain `state_timer` progression and guaranteed minimum initial lunge velocity (`lunge_ramp >= 0.40f`), allowing enemies to aggressively close distance under fire.
    - Added Test 24 to `tests/test_enemy_stalker.cpp` verifying continuous forward motion under sustained ballistic fire.
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

### 2026-10-10: Shape-Conforming & Material-Consistent Block Breaking Animation Architecture
- **Shape-Conforming Crack Decal Geometry**:
  - Replaced legacy axis-aligned 1x1 flat plane crack overlay with 3D shape-conforming geometry calculation (`BlockBreakFX::generate_block_crack_geometry` in `src/voxel/block_break_fx.cpp`).
  - **Ramps (`SHAPE_RAMP_EAST`, `WEST`, `SOUTH`, `NORTH`)**: Sloped hypotenuse face crack lines adhere precisely to 45° slope equations ($y=x$, $y=1-x$, $y=z$, $y=1-z$) with tangent vectors along and across the slope. Flank hits conform to vertical triangular side walls.
  - **3D Silhouette Stress Fractures**: When breaking progress reaches $\ge 33\%$ (stage 2+) and $\ge 50\%$ (stage 3+), multi-axis structural silhouette stress fractures wrap around the wedge's diagonal ridges, crest, toe, and heel lines while remaining strictly inside the solid prism volume.
  - **Slabs (`SHAPE_SLAB_BOTTOM`, `TOP`)**: Crack overlays align with half-height boundaries ($Y = 0.5\text{m}$) with side wall extents clamped to half height.
- **Material Texture Fracture Patterns & Color Consistency**:
  - Implemented `BlockBreakFX::get_material_crack_color` with distinctive mineral and alloy palettes:
    - *Voidite & Prismatic Crystals*: 60°/120° cleavage facet stars with amethyst-to-lilac and iridescent cyan discharge.
    - *Industrial Bulkheads & Vault Doors*: 45° cold-rolled shear stress bands, perimeter bolt strain rings, and incandescent orange-amber sparks.
    - *Granite & Basalt*: Quartz-tan fissures and incandescent volcanic magma cooling joints.
    - *Radioactive Ore*: Ionizing phosphorescent toxic emerald to neon nuclear green discharge.
- **Shape-Aware Particle Ejection & Center-of-Mass**:
  - `spawn_crack_debris` and `spawn_break_particles` compute true center-of-mass ($Y = 0.333\text{m}$ for ramps, $Y = 0.25\text{m}$ or $0.75\text{m}$ for slabs) and eject particles along the slope normal with material-matching color palettes.
- **Verification & Testing**:
  - Added unit test cases `BlockBreaking.MaterialTextureCrackColorConsistency` and `BlockBreaking.ShapeConformingRampAndSlabCrackAnimation` in `tests/test_voxel_geometry.cpp`.
  - Executed full 18-suite test suite via `python scripts/tdd.py` (all 18 suites passing in $< 3.5\,\text{s}$).

### 2026-10-13: Cavern Brightness Controls & Rig Display Settings Integration
- **In-Game Pause Menu Settings Row**:
  - Added interactive `CAVERN BRIGHTNESS` control row to [PauseMenu](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/pause_menu.cpp) (Tab 3: `[ 3. AUDIO/RIG ]`) between `FIELD OF VIEW` and `SCREEN SHAKE`.
  - Features `[-]` and `[+]` steppers with dynamic percentage feedback (`40%` to `200%`), matching the Orbital Hub station settings style.
  - Changes instantly update [Renderer::set_brightness](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.hpp#L169), scaling the ACES tonemapper post-process exposure uniform `uExposure = 1.15f * brightness`.
- **Keyboard Shortcut Documentation & Immediate Profile Sync**:
  - Registered `[ [ / ] ]` (`Decrease / Increase Brightness`) in the [PauseMenu](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/pause_menu.cpp) Controls Briefing manual (Tab 4).
  - Synchronized in-game hotkey adjustments (`[` / `]`) to immediately update [Renderer::set_brightness](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.hpp#L169) and [UserProfile::settings](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/settings.hpp#L10) for persistent save retention.
- **Precision Stepping & Persistence**:
  - Applied clean 10% rounded stepping (`std::round`) in both [OrbitalHub](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/orbital_hub.cpp) and [PauseMenu](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/pause_menu.cpp) to eliminate floating-point drift.
  - Added `clamp()` alias to [GameSettings::sanitize](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/settings.hpp#L26).
- **Test Validation**:
  - Expanded Module 12 unit tests in [test_unit_all.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_unit_all.cpp) verifying 10% step increments, decrements, minimum/maximum clamping bounds (`0.40x` to `2.00x`), and full save/load disk persistence.
  - All 22 test suites pass with zero audio in $< 3.6\,\text{s}$ via `python scripts/tdd.py`.


