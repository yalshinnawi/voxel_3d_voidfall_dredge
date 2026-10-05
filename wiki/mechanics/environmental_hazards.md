# Subterranean Environmental Hazards & Parkour Mechanics

## 1. Overview & Tactical Philosophy
*Voidfall Dredge* draws inspiration from high-stakes survival excavation and movement-driven extraction games (*Deep Rock Galactic*, *Risk of Rain 2*, *Returnal*, *Spelunky*, *Metroid Prime*). Rather than passive background scenery, subterranean caverns are active, hostile ecosystems where spatial awareness, parkour timing, and suit mobility upgrades are mandatory for survival.

---

## 2. Hazard Taxonomy, Pre-Damage Telegraphs & Mechanics

Every subterranean hazard is strictly designed with multi-sensory **pre-damage telegraphing** (visual, auditory, and HUD telemetry) so delvers are never surprised by unexpected environmental damage. Furthermore, each hazard type has isolated trauma/screen-shake characteristics: non-kinetic hazards (radiation, toxic gas) never trigger violent disorientation trauma.

### 2.1 Molten Thermite Slag / Lava Lakes (`MAT_THERMITE_SLAG`)
- **Visuals & Atmosphere**: Boiling amber-orange luminescence, rising thermal heat distortion, bubbling slag fissures, and buoyant rising orange sparks (`spawn_lava_embers`).
- **Pre-Damage Telegraph**: Thermal bubbling and steam hiss (`SoundCue::LavaBubble`, `SoundCue::ThermalHiss`) audible within $10\,\text{m}$. Proactive room entry advisory banner: `"CRITICAL ENVIRONMENT: VOLATILE MAGMA CALDERA DETECTED"`.
- **Damage Profile**: Continuous thermal burn dealing **$24\,\text{HP/s}$** ($0.4\,\text{HP}$ per $60\,\text{Hz}$ tick).
- **Physical Effects & Trauma**: Moderate heat tremor ($\text{trauma} \in [0.10, 0.35]$), accelerated suit heat buildup, and HUD alarms (`CRITICAL: THERMAL BURNS / LAVA CONTACT (-24 HP/s)`).
- **Encounter Locations**: `MagmaCalderaLake`, `FaultLineCrevasse`, `LaserDefenseFoundry`.
- **Counterplay & Parkour**: Basalt hopping pillars spaced at 3-voxel intervals, diagonal natural basalt bridges, and grapple line crossing over the molten flumes.

### 2.2 Obsidian Punji Spikes (`MAT_OBSIDIAN_SPIKES = 14`)
- **Visuals & Atmosphere**: Lethal needle-sharp obsidian punji spikes with incandescent blood-crimson needle tips (`VOXEL_FLAG_EMISSIVE`, PBR layer 14). Spikes are razor-sharp, glistening with high specular reflection.
- **Pre-Damage Telegraph**: Distinctive red glowing needle tips visible across the cavern, hollow metallic clatter (`SoundCue::SpikeRattle`) within $7\,\text{m}$, and proactive HUD advisory on approach within $3\,\text{m}$: `"CAUTION: PUNCTURE HAZARD — OBSIDIAN SPIKES AHEAD"`.
- **Damage Profile**: Instant puncture trauma dealing **$25\,\text{HP}$** upon contact (`DamageSource::Spikes`) with a $0.65\,\text{s}$ debounce timer preventing instant death.
- **Physical Effects & Trauma**: Heavy camera trauma ($\text{trauma} = 0.45$), ground crawl drag, and a sharp upward hop recoil ($+2.2\,\text{m/s}$) with HUD warning (`PUNCTURE HAZARD: SPIKE TRAP DETECTED (-25 HP)`).
- **Placement & Safety Rules**:
  - **Spawn Protection**: Spikes are **strictly prohibited** from the spawn staging cavern (Grid 0, 0), room doorway thresholds, and mandatory corridor walks.
  - **Recessed Pit Confinement**: In `SpikeTrenchArena` and `ColossalAbyssalChasm`, spikes are exclusively confined to recessed floor pits. Perimeter ledges and doorways remain 100% safe basalt/granite walkways.
  - **Safe Navigation Bridges**: High balance beam titanium bridges ($y = \text{floor}_y + 2$) and high-tension suspension bridges ($y = \text{floor}_y + 8$) span across the trenches with railings, allowing skilled traversal without damage.

### 2.3 Bottomless Void Singularity Rifts (`MAT_AIR` below $y \le 1.8\,\text{m}$)
- **Visuals & Atmosphere**: Pitch-black cosmic abysses where planetary bedrock has collapsed, illuminated only by eerie deep-violet void motes and floating monoliths.
- **Pre-Damage Telegraph**: Gravitational warp drone (`SoundCue::GravityDistortion`) and hollow wind within $8\,\text{m}$. Proactive room advisory: `"WARNING: ABYSSAL VOID SINGULARITY RIFT DETECTED"`.
- **Damage Profile**: Gravitational crushing damage dealing **$35\,\text{HP}$** (`DamageSource::VoidSingularity`).
- **Physical Effects & Trauma**: Heavy camera trauma ($\text{trauma} \ge 0.25$), gravitational expulsion field automatically repelling the player upwards to $y=6.5\,\text{m}$ with upward velocity ($+4.0\,\text{m/s}$) and HUD warning (`VOID RIFT: GRAVITATIONAL REPULSION (-35 HP)`).
- **Encounter Locations**: `VoidSingularityRift`, `AbyssalVerticalChasm`.
- **Counterplay & Parkour**: Zero-gravity floating obsidian platforms suspended across the abyss and grapple stalactite reeling.

### 2.4 Toxic Gas Vents & Spore Pockets (`MAT_GAS`)
- **Visuals & Atmosphere**: Continuous buoyant yellowish-green vapor plumes (`spawn_toxic_gas_cloud`) with aerodynamic drag and upward drift ($g < 0$). On-screen visor toxic condensation droplets (`OnScreenToxicDroplet`) and sickly vapor vignette.
- **Pre-Damage Telegraph**: High-pressure chemical venting hiss (`SoundCue::ToxicGasHiss`) audible within $6\,\text{m}$. Proactive HUD warning on approach: `"CAUTION: TOXIC ATMOSPHERE AHEAD - VENTILATE OR CIRCUMVENT"`.
- **Damage Profile**: Respiratory chemical burn dealing **$3.5\,\text{HP/s}$** (`DamageSource::ToxicGas`).
- **Physical Effects & Audio**: Delver vocal asphyxiation coughing spasms and inspiratory wheeze (`SoundCue::PlayerAsphyxiation`) repeating every $\sim 1.35\,\text{s}$.
- **Screen Shake Isolation**: **Subtle cough shudder only** ($\text{trauma} \le 0.02$). **Zero violent screen shaking** so the delver can see exit paths and read HUD telemetry.
- **Encounter Locations**: `FaultLineCrevasse`, `FungoidBioGrotto`.
- **Counterplay & Parkour**: Elevated mushroom cap platforms and perimeter rock catwalks bypassing the gas-filled floor basin; clearing pockets with explosive charges or drill suction.

### 2.5 Radioactive Ore Moats & Irradiated Cores (`MAT_RADIOACTIVE_ORE`)
- **Visuals & Atmosphere**: Pulsing phosphorescent green luminescence, energetic green ionization sparks (`spawn_radiation_glimmer`), and HUD emerald scanline static vignette (`trigger_radiation_flash`).
- **Pre-Damage Telegraph**: Calibrated audio Geiger counter clicks whose frequency accelerates with proximity ($0\,\text{mSv/h} \to 1200\,\text{mSv/h}$). Proactive pre-damage advisory at $25\% - 50\%$ radiation: `"WARNING: ELEVATED RADIATION FIELD - EXOSUIT UNDER STRESS"` before tissue corrosion damage begins at $50\%$. Proactive room entry banner: `"RADIATION HAZARD: RADIOACTIVE CORE SANCTUARY"`.
- **Damage Profile**: Deep tissue corrosion dealing up to **$5\,\text{HP/s}$** at $\ge 50\%$ radiation exposure (`DamageSource::Radiation`).
- **Physical Effects & Audio**: Delver groans in physical distress (`SoundCue::PlayerGroan`) repeating every $\sim 1.75\,\text{s}$.
- **Screen Shake Isolation**: **STRICTLY ZERO SCREEN SHAKING** ($\text{trauma} = 0.0$). Radiation damage causes cellular breakdown, not kinetic blunt force. Camera remains perfectly steady while Geiger counter clicks and groans telegraph the hazard.
- **Encounter Locations**: `RadioactiveCoreSanctuary`.
- **Counterplay & Parkour**: Stepping stone paths across the irradiated mineral moat to reach the central Voidite monolith; retreating to low-radiation zones to vent dosimeter buildup.

### 2.6 Falling Debris & Seismic Collapses (`DamageSource::FallingDebris`)
- **Localized Stress & Fault Rupture**: Seismic stress accumulates specifically in excavation zones where rock was mined, drilled, or blasted. Remote zones remain completely stable ($0\%$ stress). Screen shake, dust kickup, and ceiling spall particles only affect the delver if they are inside or near the destabilized zone.
- **Massive Primary Cave-in Wave**: Upon fault rupture, the ceiling undergoes a catastrophic collapse, detaching **$18 - 36$ physical solid voxels** across a $15\,\text{m}$ radius. Blocks tumble with punchy downward velocity ($v_y \le -5.0\,\text{m/s}$), high angular spin, and particle trails.
- **Sustained Tremor & Aftershock Cascades**: During the 5-second sustained tremor rumble, periodic secondary aftershock waves collapse **$4 - 8$ additional ceiling blocks every $0.85\,\text{s}$**, creating continuous structural disintegration throughout the event.
- **Physical Rubble Placement & Cavern Geometry**: Every falling block that strikes the cavern floor permanently settles into the voxel grid as physical rubble, altering cavern navigability and creating dynamic cover or obstacles.
- **Hostile Crushing Impact**: Crashing blocks deal **$40 - 45\,\text{HP}$ crushing impact damage** to nearby hostiles (Void Stalkers, Seismic Burrowers within $2.4 - 2.6\,\text{m}$), rewarding players who lure enemies into destabilized fault zones.
- **Delver Damage & Shelter Counterplay**: Falling debris deals **$18\,\text{HP}$** suit integrity damage upon direct impact. Players sheltered beneath player-placed Industrial Bulkheads take $0$ damage as falling blocks shatter harmlessly against the plating (`TITANIUM BULKHEAD DEFLECTED FALLING DEBRIS!`). In Sector 1, extraction landing zones are fully shielded during evacuation defense.
- **Physical Effects & Audio**: Cavern groaning structural strain (`SoundCue::CavernGroan`), concussive explosive blast cracks (`SoundCue::ExplosiveBlast`), and heavy stone crushing impacts (`SoundCue::DebrisArmorImpact`).
- **Blood Splatter Policy**: **STRICTLY ZERO BLOOD SPLATTERS**. Debris impacts damage exosuit bulkheads and mineral plating without lacerating organic tissue through the suit.

### 2.7 High-Velocity Fall Impacts (`DamageSource::FallImpact`)
- **Visuals & Atmosphere**: Rapid velocity deceleration, ground impact dust shockwave, and heavy camera downward tilt recoil.
- **Pre-Damage Telegraph**: Gravitational falling acceleration (> 13 m/s threshold before impact injury occurs).
- **Damage Profile**: High kinetic deceleration trauma dealing **$18 - 50\,\text{HP}$** based on terminal descent velocity.
- **Physical Effects & Audio**: Visceral organic bone cracking fracture with dull kinetic body thud (`SoundCue::BoneCrack`).
- **Blood Splatter Policy**: **STRICTLY ZERO BLOOD SPLATTERS**. Internal skeletal stress and structural joint trauma do not cause external visor blood splatters.

### 2.8 Crystal Aquifer Waterfalls & Subterranean Lakes (`MAT_CRYSTAL_AQUIFER`)
- **Visuals & Atmosphere**: Translucent cyan water ripple shader with dynamic refraction, aquatic splash droplets, and cascading vertical water spray mist (`spawn_water_mist`).
- **Tactical Dynamics**: 
  - **Exosuit Rapid Cooling**: Water immersion rapidly dissipates thermal buildup ($-75.0\,\text{heat/s}$), instantly clearing thruster/drill overheating status.
  - **Fluid Buoyancy & Drag**: Introduces viscous fluid drag limiting horizontal terminal velocity and cushioning vertical descents (buoyant terminal velocity clamped to $\ge -4.5\,\text{m/s}$), providing safe landing zones when jumping down chasms.
- **Encounter Locations**: `SubterraneanAquiferOasis`, `BioluminescentGlowwormGrotto`, `PrecursorCoolantReservoir`.

### 2.9 Long-Range Hazard Telegraphing (Distant Cavern Particle Simulation)
- **45m Active Simulation Radius**: The renderer and world engine continuously track active hazard voxels (gas vents, radiation deposits, lava pools, and aquifer streams) up to $45\,\text{m}$ away from the player.
- **Visual Clarity from Afar**:
  - **Toxic Gas Clouds**: Heavy billows ($0.55 - 0.88\,\text{m}$ diameter) lingering for $2.0 - 3.0\,\text{s}$ with upwards buoyancy drift, telegraphing poisonous pockets well before delvers step foot into lower basins.
  - **Radiation Fields**: Energetic green ionization sparks ($0.18 - 0.28\,\text{m}$) flickering around radioactive deposits, revealing ore veins and deadly irradiated moats across grand cavern halls.
  - **Aquifer Cascades**: Light aquatic spray mist ($0.35\,\text{m}$) telegraphed around subterranean waterfalls and reflecting pools.

### 2.10 Visceral Enemy Attacks vs. Environmental Damage Feedback
- **Visuals & Atmosphere**: Hostile bio-mechanical predators (Void Stalkers, Seismic Burrowers) rending the delver trigger on-screen arterial blood splatters (`OnScreenBloodSplatter`). Visor splatter clusters feature coagulated dark ruby edges, bright crimson centers, and downward gravity-induced dripping droplets.
- **Audio Feedback**: Organic claw laceration with tearing wet flesh and high-frequency slash transients (`SoundCue::EnemyFleshHit`).
- **Visual Isolation Matrix**:
  | Damage Source | Audio Cue | Screen Blood Splatters | Screen Shake / Trauma |
  |---|---|---|---|
  | **Enemy Melee Attack** | `SoundCue::EnemyFleshHit` (Laceration / wet tearing) | **YES** (Visor arterial blood droplets & drips) | High ($\ge 0.30$) |
  | **Fall Impact** | `SoundCue::BoneCrack` (Sharp fracture snap + dull thud) | **NO** (Zero splatters) | High ($\ge 0.25$) |
  | **Falling Debris** | `SoundCue::DebrisArmorImpact` (Crushed stone + armor deflection) | **NO** (Zero splatters) | Moderate ($\ge 0.15$) |
  | **Radiation Corrosion**| `SoundCue::PlayerGroan` + Geiger counter clicks | **NO** (Emerald scanline static) | **ZERO** ($0.0$) |
  | **Toxic Gas Inhalation**| `SoundCue::PlayerAsphyxiation` (Coughing spasms) | **NO** (Toxic condensation droplets) | Micro-shudder ($\le 0.02$) |
  | **Thermal Lava Slag** | `SoundCue::LavaBubble` + `SoundCue::ThermalHiss` | **NO** (Amber heat distortion) | Moderate ($\le 0.35$) |
  | **Spike Pit Puncture** | `SoundCue::SpikeRattle` + metallic impact | **NO** (Zero splatters) | High ($\ge 0.25$ + vertical impulse) |
  | **Aquifer Water Drag** | `SoundCue::WaterSplash` + bubbling | **NO** (Visor water mist beads) | **ZERO** ($0.0$ - soothing cooling) |

---

## 3. Parkour Traversal Systems & Delver Archetypes

| Delver Class | Signature Movement Perk | Ideal Environmental Counterplay |
|---|---|---|
| **Vesper (Scout)** | High-Tension Winch & Acoustic Resonance ($30\times$ faster grapple reel, $+20\%$ move speed). | Effortless traversal across `AbyssalVerticalChasm` spiral ledges and `CrumblingArchCanyon` stalactites. |
| **Kaelen (Demolitionist)** | Volatile Blast Refining & Kinetic Charges. | Blasts away corridor rubble collapses and breaches `IndustrialVaultBunker` reinforced blast gates. |
| **Rhodes (Vanguard)** | Tectonic Bulkhead Plating ($-30\%$ cave-in damage, bulkhead crafting). | Deploys instant titanium bridge plates across `MagmaCalderaLake` and `SpikeTrenchArena` pits. |

---

## 4. Design Inspirations & Comparative Reference
- **Deep Rock Galactic**: Environmental biome personality (Magma Core heat mechanics, Fungus Bogs toxic spores, Hollow Bough thorny spikes).
- **Risk of Rain 2**: Vertical circular arenas, jumping stone pads, timed jump arcs over lethal floor basins.
- **Returnal**: High-speed dash/jump mechanics over laser security flumes and bottomless cosmic chasms.
- **Spelunky**: High-tension environmental hazards (punji spikes, collapsing floors) that demand deliberate footwork.
- **Metroid Prime**: Atmospheric subterranean scanning, morph-tunnel routes, and industrial precursor installations with molten metal sluices.
