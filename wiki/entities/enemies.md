# Enemy Threats & Behaviors

This document lists the hostile life forms native to the void sectors.

---

## 1. Void Stalker
![Void Stalker](../../docs/models/enemy_void_stalker.png)
- **Threat Level**: Medium-High (High in unlit areas).
- **Spawn Conditions**: Ambient caves, dark corners, low illumination, triggered by mining drill acoustic vibrations.
- **Model Anatomy & PBR Shading**:
  - Deep void obsidian carapace (`RGB(0.04, 0.04, 0.05)`, Roughness 0.25, Metallic 0.40).
  - Bioluminescent compound ruby eyes casting dynamic tracer lasers (`RGB(0.92, 0.12, 0.96)`, Emissive 6.0).
  - Segmented articulated tail with stinger, mantis scythe claws, and projectile spine quills.
- **Behavior & Cavern Acoustics**:
  - Clings to walls/ceilings using 3D voxel raycasts.
  - Stalks behind the player, leaping to attack when the player begins mining.
  - **Pacing & Concurrent Chaser Caps**: Active chasers in combat (`Stalking`, `Lunging`, `Circling`) are strictly capped to prevent overwhelming mobbing:
    - Sector 1: Cap of 2 concurrent chasers.
    - Sector 2: Cap of 3 concurrent chasers.
    - Sector 3+: Cap of 4 concurrent chasers.
    - Any excess alerted enemies are diverted into `Investigating` search patterns rather than rushing the player simultaneously.
  - **Combat Recovery Window & Respite**: Eliminating an active Stalker triggers a 5.0s combat recovery window where new chasers are inhibited, and forces an 8.0s post-combat noise meter rapid-drain cooldown so the player has adequate breathing room to reload, heal, and reposition.
  - **Pack Alert & Gunshot Throttling**: Direct hits on a Stalker alert at most 1 nearby packmate within 8.0m with direct line-of-sight (2 in Sector 3+). Gunshot acoustic propagation radii are constrained to 24–34m (Carbine 24m, Scattergun 28m, Railgun 34m) preventing map-wide chain aggro through solid walls.
  - **Behind-Wall Obstruction Detection & Alternate Route Determination**: When attacking or pursuing the player in combat/pursuit (`Stalking` state), Stalkers dynamically analyze line-of-sight and physical path clearance. If an intervening solid wall, sealed room partition, or barricade blocks direct access to the target:
    - **Ground Detour Navigation**: The AI determines a bounded 3D surface detour around the wall through doorways, arches, or adjacent corridors using string-pulled waypoints (`AlternateRouteType::GroundDetour`), smoothly routing around corners without stalling against the rock face.
    - **Vertical Wall-Climbing Flanks**: If the obstruction is a barrier or partition with open headroom, climbing-capable stalkers evaluate vertical viability (`AlternateRouteType::VerticalClimb`) and scale the wall face, traversing over the top to ambush the player from above.
    - **Dynamic LOS Reacquisition**: Stalkers extend their visual loss timer (to 8.0s) during detour maneuvers, advancing through waypoints until line-of-sight is re-established, at which point they instantly clear the route and initiate direct lunges or projectile volleys.
  - **Anti-Stuck Climbing, Downward Traversal & Multi-Enemy Crowd Flocking**: Ambient cavern fauna enforce $\ge 8.0\,\text{m}$ spatial separation, and wave spawns enforce $\ge 6.5\,\text{m}$ separation to eliminate clustered group spawns. Enemies feature sub-block geometric awareness, smoothly traversing half-slabs (bottom and top), 45-degree ramps (cardinal and corner wedges), stairs, and elevation transitions up to $1.35\,\text{m}$ with continuous slope hugging and uncluttered sightlines. Step clambering is forward-gated to movement direction, preventing rear probes from popping descending entities upward when walking down stairs. Wall climbers descending toward floors recognize open ground landing surfaces and cleanly transition to `FLOOR` state ($\le 1.45\,\text{m}$). In multi-enemy packs and narrow bottlenecks, mutual penetration solvers apply tangential slip impulses and spatial boid separation, allowing swarms of 40+ creatures to glide past each other without deadlock or freezing.
  - **Retaliation Pursuit**: When attacked/damaged by the player (e.g. mining drill tip grinding, plasma blaster bolts, or explosive blast damage), the Stalker immediately enrages and relentlessly pursues the player with expanded perception tracking.
  - **Pursuit Break Condition**: The Stalker only loses interest and resumes ambient idling/patrol if the player runs far away (> 32m or breaking line-of-sight across distance) and maintains separation continuously for at least 12 seconds (`pursuit_break_time = 12.0s`).
  - Emits rare distant ambient echo shrieks (`SoundCue::StalkerEchoScreech`) every 60–95s that reverberate down cavern corridors up to 58m. In combat, packs coordinate with a single initial alert screech followed by subtle throat hisses (`SoundCue::StalkerHiss`) and mandible chitters (`SoundCue::StalkerChitter`), governed by pack and per-entity refractory cooldowns (18–25s) to eliminate screech repetition and ear fatigue.
  - High inverse-distance spatial rolloff: at close proximity (< 12m), shrieks are >10x louder with stereo binaural azimuth positioning, triggering a HUD proximity warning.
  - **Natural Physical Burrowing Emergence**: When spawning via waves, dynamic encounters, or spawn managers, Stalkers no longer abruptly pop into existence. They physically dig out from walls, ceilings, or floors with animated claw-digging sweeps, surface-normal alignment, and archetype-differentiated particles:
    - *Melee (Blood Obsidian)*: Rapid 1.2s emergence. Aggressive mantis-claw digging surges, snapping mandibles, crimson void embers, and fractured granite chips.
    - *Shooter (Toxic Jade)*: 1.5s emergence. Sinuous writhing crawl, low profile posture, radiant emerald eye slits peering out first, accompanied by buoyant toxic vapor wisps and emerald crystal flecks.
    - *Chitin Goliath (Heavy Carapace Breacher)*: Slow 2.2s tectonic heave. Massive armored head thrusts up through pulverized rock, generating large basalt boulder debris and heavy dust shockwaves.
  - **Crouch Stealth, Vision Cones & Sneak Attacks**:
    - **Acoustic Silence**: Delvers crouching (`L-Ctrl`) generate $0\,\text{dB}$ movement noise, allowing silent movement through dark chambers.
    - **Vision Cone Gating ($120^\circ$ FOV)**: Unalerted Stalkers in `Idle` or `Investigating` have a $120^\circ$ forward vision cone ($\pm 60^\circ$). When a player is crouching, they can only be spotted visually if they enter both direct line-of-sight and the creature's forward vision cone. Approaching an unalerted stalker from behind or flanks does NOT alert it.
    - **Territory Patrol Zones & Autonomous Cavern Wandering**:
      - Level rooms and natural cavern chambers partition the map into bounded `PatrolZone` habitats (`AberrantAI::PatrolZone`).
      - Stalkers autonomously prowl within their assigned territory zone, navigating between ground-anchored waypoints generated with surface detection and hazard avoidance (avoiding molten lava and obsidian spikes).
      - **Vigilant Scanning Pauses**: Upon reaching a waypoint, the stalker enters a vigilant pause (2.0–4.5s) where it actively sweeps its vision cone left and right ($\pm 55^\circ$, `patrol_scan_yaw`), inspecting cavern tunnels and dark corners.
      - **Autonomous Slumber Awakening**: Pre-placed roosting fauna possess staggered slumber timers (`slumber_timer = 30.0s - 90.0s`). After undisturbed rest, dormant roosting creatures awaken autonomously onto the floor and begin prowling their territory zone, creating an evolving, dynamic environment.
      - **Dynamic Sneak Challenge**: Because wandering predators change facing angles and patrol paths in real-time, the delver must read patrol rhythms, avoid sweeping visual sweeps, and time stealth movements between shadows.
    - **Roosting Fauna Dormancy**: Roosting stalkers cling to surfaces with closed eyes and do not awaken in the dark from crouching footsteps nearby. They strictly awaken if directly illuminated by the delver's headlamp beam, physically collided with ($< 0.85\,\text{m}$), disturbed by loud noise ($> 70\,\text{dB}$) / weapons fire, or upon autonomous slumber expiration.
    - **Sneak Attack Critical Hits**: Melee or ranged strikes landed on unalerted Stalkers trigger guaranteed `SNEAK ATTACK CRITICAL HIT` dealing $2.5\times$ base damage (`StealthSystem::SNEAK_ATTACK_CRIT_MULTIPLIER`), rewarding deliberate stealth infiltration.
  - Weak to Scout Sonar pulses (3.0s stun) and bright light sources.

---

## 2. Seismic Burrower
![Seismic Burrower](../../docs/models/enemy_seismic_burrower.png)
- **Threat Level**: Critical.
- **Spawn Conditions**: Post-Minute 8 (Hazard Phase 3), extraction defense wave escalations.
- **Model Anatomy & PBR Shading**:
  - Rotating conical borer head with 4 radial hardened tungsten cutter teeth (`RGB(0.65, 0.60, 0.55)`, Metallic 0.85).
  - Molten magma apex tip and lateral slag exhaust heat vents (`RGB(2.5, 0.45, 0.05)`, Emissive 6.0).
  - 4-segment articulated serpentine body with sinusoidal crawling articulation and dorsal armor crests.
- **Behavior & Cavern Acoustics**:
  - **Path Obliteration & Kinetic Destruction**: Sweeps and pulverizes all destructible voxels (volcanic basalt, granite, ores, crystal formations) in its path during both subterranean tunneling and high-speed kinetic charges (`BurrowerState::Charging`), creating devastating bore holes straight to the target. Only reinforced industrial bulkheads and dredged bedrock stop its kinetic charge.
  - **Relentless Non-Retreating Tenacity**: Does not get scared or flee when taking heavy weapon fire; when critically damaged (HP $\le$ 35), it refuses to retreat, instead enraging into an aggressive berserk frenzy (`BurrowerState::Enraged`).
  - **Full 3D Elevation Tracking**: Accurately pitches and banks in 3D space, tunneling upward or plunging downward across cavern vertical tiers directly toward delvers.
  - **Proximity Tremors & Horror Audio**: Subterranean presence produces deep low-frequency screen trauma and ceiling dust kickup proportional to proximity (`max_rumble`), paired with continuous stone cutter grinding (`SoundCue::BurrowerGrind`) and digging cues (`SoundCue::MonsterDigging`).
  - **Retaliation Pursuit**: If damaged while dormant or roaming, the Burrower immediately awakens with a roar and enters persistent pursuit mode, directly locking its borer path onto the player and ignoring ambient sound distractions.
  - **Pursuit Break Condition**: The Burrower will only lose interest and cease its pursuit if the player flees far away (> 35m) and maintains that distant separation for an extended duration of at least 15 seconds (`pursuit_break_time = 15.0s`).
  - Emits low-frequency subterranean tectonic roars (`SoundCue::BurrowerRoar`, 36–62 Hz) every 13–27s, cutter tooth grinding (`SoundCue::BurrowerGrind`), and a deafening breach roar upon erupting into open chambers.
  - Proximity alert: roars within 16m trigger HUD alerts and strong sub-bass stereo rumble.
  - **Crouch Stealth, Dormancy & Sneak Attacks**:
    - **Subterranean Territory Wandering**:
      - Unalerted burrowers in `BurrowerState::Burrowing` are assigned subterranean territory patrol zones (`PatrolZone`).
      - Rather than unrealistically homing in on sneaking delvers through solid stone, unalerted burrowers tunnel between autonomous waypoints within their sector zone, creating unpredictable geological shifts and seismic cues.
    - **Acoustic Camouflage**: Unalerted burrowers deep in rock do not lock onto or track crouching delvers through solid walls at $18\,\text{m}$.
    - **Dormancy Safety**: Dormant burrowers do not awaken when approached closely in crouched stealth ($\ge 0.90\,\text{m}$), whereas standing footsteps awaken them at $8.0\,\text{m}$.
    - **Sneak Attack Vulnerability**: Striking an unalerted or dormant burrower triggers guaranteed `SNEAK ATTACK CRITICAL HIT` ($2.5\times$ damage), which stacks multiplicatively with Demolitionist explosive charges for massive devastating strikes ($6.25\times$).
  - Weak to Demolitionist explosive charges (takes 2.5x damage).
  - **Death Animation & Physical Carcass**:
    - **Death Throes (`BurrowerState::Dying`)**: Upon suffering lethal damage, the Burrower enters violent death throes lasting 0.9s. The tungsten rotary cutter head spins down with metallic grinding friction, angular velocity creates a staggered tumble, and gravity accelerates the beast into the cavern floor with settling ground contact.
    - **Persistent Carcass Remains**: Spawns a physical `CarcassType::SeismicBurrower` model featuring a slumped basalt body, jammed cutter head with tilted borer teeth, cooling slag vents, and extinguished core tip. Carcasses persist indefinitely throughout the expedition.
  - **Burrow-In Spawning Emergence**:
    - When spawned dynamically or as hazard waves, Burrowers do not pop into existence; they burst through the rock floor or wall face (`just_breached`) with a 0.6s upward emergence, rotary borer acceleration, rock blast debris, screen shake, and deafening subterranean roar (`SoundCue::BurrowerRoar`).

---

## 3. Chitin Goliath (Apex Vault Boss)
- **Threat Level**: Lethal / Boss Encounter.
- **Spawn Conditions**: Deep Precursor Vault chambers in Sector 3+, guarding high-tier alien artifacts and dense voidite geodes.
- **Model Anatomy & PBR Shading**:
  - Massive $2.2\times$ scale colossus with impenetrable frontal basalt plates and barbed dorsal spines.
  - Baseline eyes: Menacing amber-gold (`RGB(0.95, 0.70, 0.10)`).
  - Enraged eyes: Infernal radiant crimson (`RGB(1.0, 0.05, 0.02)`), with heated magma veins pulsing across the carapace.
- **Sentinel Vigilance & Apex Predator AI**:
  - **No Passive Dormancy**: Never stays dormant or sleeps passively while the player is inside the vault chamber. Senses players within $28\,\text{m}$ and immediately awakens with a terrifying apex battle roar (`SoundCue::StalkerEchoScreech`).
  - **Relentless Combat Focus**: Never flees at low health (`flee_hp_threshold = 0.0f`) and never paces passively in circles around the player. Actively engages, presses advantages, and corners the player.
  - **Frontal Armor Deflection**: Heavy chitin carapace deflects 85% of incoming frontal weapon damage (taking only 15% damage from the front). Players must flank the weak dorsal vent or bait a wall stun to deal full damage.
- **Boss Moveset & Special Attacks**:
  - **Titan Ground Slam (Close Range Shockwave AOE)**:
    - *Trigger*: Player within $\le 5.5\,\text{m}$.
    - *Telegraph*: Rears up massive scythe claws for 0.7s (0.45s in enrage).
    - *Impact*: Slams ground with concussive force, dealing up to 48 damage with 6.5m radial falloff, heavy screen trauma, dust shockwaves, and triggering localized ceiling debris cave-ins.
  - **Juggernaut Bull Charge (Mid-Range Kinetic Ram)**:
    - *Trigger*: Player at mid-range (6.0m to 22.0m) on cavern floor.
    - *Telegraph*: Scrapes talons against basalt and aims rush vector for 0.6s (0.40s in enrage).
    - *Rush*: Sprints in a high-speed ramming vector at $10.5\,\text{m/s}$ ($13.5\,\text{m/s}$ in enrage). Impacts dealing 52 direct kinetic damage and knocking player back.
    - **Wall-Crash Stun & Armor Breach**: If the player dodges or baits the charge into a solid rock wall or metal bulkhead, the boss crashes with violent impact, stunning itself for 2.4 seconds (`StalkerState::Stunned`). While stunned, its frontal armor is completely breached, allowing weapons to deal 100% full damage from all angles!
  - **Carapace Quill Barrage (Anti-Kiting / Elevated Target Defense)**:
    - *Trigger*: Player is elevated on ledges/pillars ($> 1.8\,\text{m}$ above) or attempts to kite at range ($> 8.0\,\text{m}$).
    - *Effect*: Fires a 3-spike fan of hardened armor-piercing quills (center, $-12^\circ$ left, $+12^\circ$ right) dealing 14 damage per spine.
  - **Heavy Cleave (Close Melee Strike)**:
    - Sweeping claw strike dealing 38 base melee damage with a 1.4s cooldown (0.9s in enrage).
- **Phase 2: Apex Frenzy Enrage ($\le 50\%$ HP)**:
  - When reduced below 110 HP, the boss enters an enraged berserk frenzy:
    - Movement speed escalates to $3.6\,\text{m/s}$ (charging speed jumps to $13.5\,\text{m/s}$).
    - Special attack cooldowns and windup telegraphs are reduced by 35–40%.
    - Eyes ignite in blazing crimson and carapace pulses with scorching magma heat.
- **HUD Boss Health Bar**:
  - An elite armored boss health bar is displayed at the top of the HUD during vault encounters, showing dynamic status banners (`[ APEX GOLIATH: ENRAGED ]`, `[ APEX GOLIATH: BREACHED ]`, `[ APEX GOLIATH: GROUND SLAM ]`, `[ APEX GOLIATH: CHARGING ]`) and precise HP telemetry.
  - **Visibility Invariants**: The boss health bar only appears when the boss is in **close range** ($\le 28\,\text{m}$) and in **obvious sight** (direct unobstructed raycast line-of-sight from the player's eye and positioned within the player's forward view cone, $\text{dot}(\mathbf{fwd}, \mathbf{dir}) \ge 0.25$). If the boss is out of range, behind solid cavern walls, burrowing underground, or behind the player's back, the health bar is strictly hidden to preserve tactical tension and avoid spoiler wall-bleed.

---

## 4. Volatile Crystalline Leech
- **Threat Level**: Low-Medium (Hazardous in groups).
- **Spawn Conditions**: Triggered by breaking Volatile Crystal nodes.
- **Behavior**:
  - Swarms toward mined mineral debris.
  - Explodes on death or contact, chaining detonations with nearby volatile voxels.

---

## 5. Locomotion Kinematics & Anti-Teleportation Invariants

To guarantee lifelike predator motion and eliminate unnatural coordinate popping or teleportation, enemy movement and physics adhere to strict mathematical invariants across all sectors:

- **Continuous Closest-Point AABB Depenetration (`AberrantAI::resolve_voxel_collision`)**:
  - Horizontal solid voxel collision calculates the exact closest point on each candidate voxel box $(c_x, c_z) = (\text{clamp}(p_x, x, x+1), \text{clamp}(p_z, z, z+1))$.
  - If horizontal penetration occurs ($\text{dist} < \text{radius}$), the entity is depenetrated smoothly along the contact normal by $\text{overlap} = (\text{radius} + 0.005) - \text{dist}$, with normal velocity zeroed and tangential sliding velocity preserved.
  - Replaces legacy multi-whisker bounding snaps which previously caused 0.4m–0.8m sudden horizontal coordinate jerks when grazing blocks.
- **Encased Voxel Recovery as Strict Last Resort**:
  - Emergency upward ejection is strictly gated behind total entity encasement (`feet_solid && center_solid && head_solid`), executed only after standard kinematic and depenetration passes fail.
  - Entities merely grazing walls or walking into solid blocks no longer trigger emergency ejections into the ceiling.
- **Continuous Lunge & Standoff Approach**:
  - During aggressive lunge maneuvers (`StalkerState::Lunging`), movement advances continuously step-by-step ($v_{\text{lunge}} \times \Delta t$).
  - When reaching melee strike range, step travel is clamped to the exact remaining distance to the standoff boundary and forward momentum is canceled via `AberrantAI::cancel_forward_momentum`, completely eliminating hard coordinate snapping ($\Delta x \le 1.25\,\text{m}$) into the player.
- **Soft Territory Boundary Deflection**:
  - When an entity reaches the boundary of its assigned cavern `PatrolZone`, position is never hard-clamped.
  - Instead, the entity smoothly steers its velocity vector back toward the zone center, producing fluid, continuous patrol arcs along cavern perimeters.
- **Impulse-Driven Step Clambering**:
  - When negotiating 1-block elevation steps or clearing minor obstacles, entities apply vertical velocity impulses ($v_y \mathrel{+}= 2.5\,\text{m/s}$) rather than instantaneous coordinate displacement ($y \mathrel{+}= 0.30\,\text{m}$), allowing the physics simulation and gravity to resolve clambering smoothly.
- **Velocity Clamping & Tunneling Prevention**:
  - Physics impulses and explosive knockbacks enforce terminal entity velocity caps ($14.0\,\text{m/s}$ for Goliath, $16.0\,\text{m/s}$ for standard Stalkers), preventing high-velocity tunneling across voxel boundaries.

---

## 6. High-Density Swarm Scaling & GPU Vertex Capacity Invariants

To guarantee that massive hordes of enemies never flicker, clip, or become invisible when high enemy counts emerge in late-round hazard escalation and extraction holdouts, the rendering and memory subsystems enforce the following invariants:

- **Entity Mesh Vertex Footprint**:
  - `VoidStalker` (Melee & Shooter): 916 vertices (26 individual articulated box elements).
  - `VoidStalker` (Chitin Goliath Vault Boss): 952 vertices (27 box elements including reinforced frontal carapace shield).
  - `SeismicBurrower`: 720 vertices (segmented borer anatomy, cutter teeth, and exhaust cowlings).
  - `VoidSpikeProjectile`: 60 vertices + trailing laser line vertices.
  - `EnemyCarcass`: 324 vertices.
- **Dynamic GPU Buffer Capacity Scaling (`Renderer::ensure_stalker_vbo_capacity`)**:
  - Static upload caps (such as the legacy 16,384 vertex cap that caused monsters beyond monster #17 to vanish) have been completely removed.
  - The enemy VBO (`m_stalker_vbo`) starts at an initial capacity of 65,536 vertices and dynamically doubles whenever the active swarm vertex demand exceeds current capacity, automatically re-binding VAO vertex attribute pointers.
  - CPU staging vectors (`s_stalker_verts`, `s_burrower_verts`, `s_carcass_verts`, `m_stalker_staging_lines`) dynamically reserve storage based on active entity counts to eliminate dynamic heap reallocations during frame rendering.
- **Automated Verification Harnesses**:
  - **Module 7 in `tests/test_load_and_stress.cpp`**: Simulates 120 Stalkers (40 Melee, 40 Shooter, 40 Goliath), 30 Burrowers, 40 Projectiles, and 30 Carcasses (> 140,000 vertices demand) across 20 simulation ticks, confirming zero NaNs and zero entity dropping.
  - **Engine Visual Staging Harness (`--test-enemy-load`)**: Stages 35 Stalkers, 6 Burrowers, 12 Void Spikes, and 8 Carcasses (40,052 vertices) simultaneously in camera view, capturing `screenshots/enemy_load_test.png` and verifying complete rendering fidelity via `python scripts/test_enemy.py --load`.

---

## 7. Locomotion Auditory Cues & Spatial Awareness

To give delvers clear auditory feedback and early situational awareness of approaching threats in dark cavern corridors, all hostile fauna feature dedicated, procedurally synthesized walking, scurrying, and charging sounds.

### 7.1 Entity Sound Profiles & Cadence

| Entity / Variant | Movement State | Sound Cue | Cadence Interval | Auditory Signature & Synthesis |
| :--- | :--- | :--- | :--- | :--- |
| **Void Stalker** (Melee/Shooter) | Walking / Prowling | `SoundCue::StalkerStep` | 0.38s ($\pm 0.04\text{s}$) | Sharp 2400 Hz chitin claw tap on basalt stone + subtle 320 Hz foot thump. |
| **Void Stalker** (Melee/Shooter) | Sprinting / Lunging | `SoundCue::StalkerRun` | 0.20s ($\pm 0.03\text{s}$) | Rapid multi-claw scamper sequence (0ms, 35ms, 70ms impacts) + 2200 Hz friction scrape. |
| **Chitin Goliath** (Apex Boss) | Walking / Stalking | `SoundCue::GoliathStep` | 0.62s ($\pm 0.04\text{s}$) | Earth-shaking 52 Hz sub-bass thump + crushed rock crackle + 165 Hz plate buckle. |
| **Chitin Goliath** (Apex Boss) | Charging / Ramming | `SoundCue::GoliathStep` | 0.30s ($\pm 0.03\text{s}$) | Rapid heavy thunderous thumps with pitch-shifted bass resonance. |
| **Seismic Burrower** | Crawling / Roaming | `SoundCue::BurrowerStep` | 0.44s ($\pm 0.04\text{s}$) | Low 88 Hz crawler tread thump (24 Hz AM modulation) + 620 Hz rotary teeth friction grind. |
| **Seismic Burrower** | Kinetic Charge / Frenzy | `SoundCue::BurrowerStep` | 0.22s ($\pm 0.03\text{s}$) | High-rpm rhythmic borer churn, stone shearing, and mechanical chassis vibration. |

### 7.2 Adaptive Proximity & Velocity Scaling
Hostile footstep volume dynamically scales based on delver proximity and approach vector:
- **Velocity Gating**: Sounds only trigger when the entity's horizontal velocity exceeds $\ge 0.25\,\text{m/s}$, preventing spurious step triggers during stationary turning or idle scanning.
- **Proximity Volume Curve**: Base volume is modulated by normalized proximity $\text{prox} = 1.0 - \text{clamp}(d / 38.0, 0.0, 1.0)$:
  $$\text{Gain} = 0.50 + 0.90 \times \text{prox}^{1.15}$$
  Faraway footsteps (~32m) play as faint, eerie scrapes; point-blank footsteps (< 5m) deliver full impact (1.35x–1.40x gain).
- **Approach Velocity Boost**: When an enemy is closing in on the player ($\mathbf{v} \cdot \mathbf{dir} > 0.5\,\text{m/s}$), an additional $+15\%$ gain boost is applied to give urgent directional warning.
- **3D Spatial Attenuation**: 3D audio uses inverse distance roll-off with roll-off factor $1.15$ and reference distances calibrated per creature size ($3.0\,\text{m}$ for Stalkers, $4.0\,\text{m}$ for Goliath/Burrower) with stereo azimuth panning.

### 7.3 Acoustic System Isolation Invariants
Hostile locomotion footsteps are strictly isolated from all engine detection and environmental mechanics:
1. **Zero Self or Friendly Acoustic Reaction**: Locomotion sounds are routed directly to the master audio mixer (`AudioEngine::play_sound_3d`) and do **NOT** invoke `NoiseMeter::emit_sound()` or write to `recent_sounds()`. Enemies cannot hear their own footsteps or the footsteps of packmates.
2. **Zero Tremor & Hazard Clock Escalation**: Locomotion footsteps do not affect tectonic stability, cave-in risk, or the 15-minute `HazardClock` escalation timer.
3. **Player Noise Meter Unaffected**: The player's HUD `NoiseMeter` gauges solely the delver's acoustic emissions (drilling, sprint footsteps, weapons fire, explosives) and remains completely unaffected by enemy locomotion.
