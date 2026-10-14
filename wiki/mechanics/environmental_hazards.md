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
- **Pre-Damage Telegraph**: High-pressure chemical venting hiss (`SoundCue::ToxicGasHiss`) audible within $6\,\text{m}$. Visible yellow-green sulfur mist haze hovering over gas basins. Proactive HUD warning on approach: `"CAUTION: TOXIC ATMOSPHERE AHEAD // BIO-GAS ESCAPING"`.
- **Damage Profile**: Respiratory chemical burn dealing **$1.0\,\text{HP/s}$** (`DamageSource::ToxicGas`).
- **Physical Effects & Audio**: Delver vocal asphyxiation coughing spasms and inspiratory wheeze (`SoundCue::PlayerAsphyxiation`) repeating every $\sim 2.0\,\text{s}$.
- **Screen Shake Isolation**: **Subtle cough shudder only** ($\text{trauma} \le 0.02$). **Zero violent screen shaking** so the delver can see exit paths and read HUD telemetry.
- **Encounter Locations**: `FaultLineCrevasse`, `FungoidBioGrotto`, `ToxicMiasmaSwamp`.
- **Counterplay & Parkour**: Elevated mushroom cap platforms and perimeter rock catwalks bypassing the gas-filled floor basin; clearing pockets with explosive charges or drill suction.

### 2.5 Radioactive Ore Moats & Irradiated Cores (`MAT_RADIOACTIVE_ORE`)
- **Visuals & Atmosphere**: Luminous toxic emerald volumetric mist haze rolling through the chamber (`uToxicHazeFactor`), buoyant phosphorescent green ionization mist wisps (`spawn_radiation_glimmer`), and subtle HUD emerald scanline vignette (`trigger_radiation_flash`).
- **Pre-Damage Telegraph**: Calibrated audio Geiger counter clicks whose frequency accelerates with proximity ($0\,\text{mSv/h} \to 1200\,\text{mSv/h}$). Proactive pre-damage advisory at $25\% - 50\%$ radiation: `"WARNING: ELEVATED RADIATION FIELD - EXOSUIT UNDER STRESS"`. Proactive room entry banner: `"<< HAZARD SECTION: IONIZING RADIATION FIELD DETECTED >>"`.
- **Damage Profile**: Safe buffer up to $74\%$ radiation ($0\,\text{HP/s}$ damage). Gentle warning corrosion begins at $\ge 75\%$ ($0.15 - 0.42\,\text{HP/s}$), scaling moderately to $0.85\,\text{HP/s}$ at $95\%$, and capped at **$1.5\,\text{HP/s}$** at critical $\ge 98\%$ exposure (`DamageSource::Radiation`). Extraction landing zone provides $85\% - 90\%$ radiation shielding resistance and active medical decontamination scrubbers ($25\%/\text{s}$ purge rate).
- **Spatial Isolation (Non-Ubiquitous)**: Radiation is strictly isolated to **radiation-related areas** (within $12\,\text{m}$ proximity of `MAT_RADIOACTIVE_ORE` or irradiated chasms). Safe cavern air outside radioactive zones maintains $0\%$ baseline exposure.
- **Counterplay & Active Dosimeter Venting**: Retreating from radioactive ore zones into clean cavern air allows the exosuit's lead-polymorphic scrubbers to actively **vent and lower accumulated radiation** back to $0\%$ (at $10\%/\text{s}$ base, up to $17.5\%/\text{s}$ with `RadiationShielding` upgrades). Entering the Evac Extraction Pod activates rapid medical decontamination at $25\%/\text{s}$.
- **HUD Telemetry & Venting Status**: Geiger pill displays `"GEIGER: HIGH FIELD (XX%) - RETREAT!"` at $\ge 50\%$, transitioning to dynamic cyan `"GEIGER: VENTING (XX%) - PURGING DOSE"` when retreating to safety, and vanishes completely when clean ($\le 2\%$).
- **Physical Effects & Audio**: Delver groans in physical distress (`SoundCue::PlayerGroan`) rate-limited to every $\sim 4.5\,\text{s}$.
- **Screen Shake Isolation**: **STRICTLY ZERO SCREEN SHAKING** ($\text{trauma} = 0.0$). Radiation damage causes cellular breakdown, not kinetic blunt force. Camera remains perfectly steady while Geiger counter clicks and groans telegraph the hazard.
- **Encounter Locations**: `RadioactiveCoreSanctuary`, deep radioactive ore seams, and `VaultObstacleType::ChasmAbyssRadiation`.
- **Hostile Monster Radiation Immunity (Lore Invariant)**: Subterranean hostiles (Void Stalkers, Seismic Burrowers) are native radioactive organisms formed from deep crustal isotope seams. They are **100% immune to radiation** from `MAT_RADIOACTIVE_ORE` and do not flee from or take damage when traversing irradiated ore clusters.

### 2.6 Falling Debris & Seismic Collapses (`DamageSource::FallingDebris`)
- **Localized Stress & Fault Rupture**: Seismic stress accumulates specifically in excavation zones where rock was mined, drilled, or blasted. Remote zones remain completely stable ($0\%$ stress). Screen shake, dust kickup, and ceiling spall particles only affect the delver if they are inside or near the destabilized zone. Extended $75\,\text{s}$ refractory cooldown prevents repetitive tremor loops.
- **Controlled Primary Cave-in Wave**: Upon fault rupture, the ceiling undergoes a controlled localized collapse, detaching **$6 - 12$ physical solid voxels** across the affected zone. Blocks tumble with punchy downward velocity ($v_y \le -5.0\,\text{m/s}$), high angular spin, and particle trails.
- **Sustained Tremor & Aftershock Cascades**: During the 5-second sustained tremor rumble, localized dust kickup and ceiling spall warn the delver of falling rock trajectories.
- **Physical Rubble Placement & Cavern Geometry**: Every falling block that strikes the cavern floor permanently settles into the voxel grid as physical rubble, altering cavern navigability and creating dynamic cover or obstacles.
- **Hostile Crushing Impact**: Crashing blocks deal **$20 - 50\,\text{HP}$ crushing impact damage** to nearby hostiles (Void Stalkers, Seismic Burrowers within $2.4 - 2.6\,\text{m}$), rewarding players who lure enemies into destabilized fault zones.
- **Delver Damage & Shelter Counterplay**: Falling debris deals balanced **$7 - 17.5\,\text{HP}$** damage to the player upon direct impact with a $1.2\,\text{s}$ multi-hit debouncing window to prevent instant cluster deaths. Players sheltered beneath player-placed Industrial Bulkheads take $0$ damage (`TITANIUM BULKHEAD DEFLECTED FALLING DEBRIS!`). In Sector 1, extraction landing zones are fully shielded during evacuation defense.
- **Physical Effects & Audio**: Cavern groaning structural strain (`SoundCue::CavernGroan`), concussive explosive blast cracks (`SoundCue::ExplosiveBlast`), and heavy stone crushing impacts (`SoundCue::DebrisArmorImpact`).
- **Blood Splatter Policy**: **STRICTLY ZERO BLOOD SPLATTERS**. Debris impacts damage exosuit bulkheads and mineral plating without lacerating organic tissue through the suit.

### 2.7 High-Velocity Fall Impacts (`DamageSource::FallImpact`)
- **Visuals & Atmosphere**: Rapid velocity deceleration, ground impact dust shockwave, and heavy camera downward tilt recoil.
- **Pre-Damage Telegraph**: Gravitational falling acceleration (> 13 m/s threshold before impact injury occurs).
- **Damage Profile**: High kinetic deceleration trauma dealing **$18 - 50\,\text{HP}$** based on terminal descent velocity.
- **Physical Effects & Audio**: Visceral organic bone cracking fracture with dull kinetic body thud (`SoundCue::BoneCrack`).
- **Blood Splatter Policy**: **STRICTLY ZERO BLOOD SPLATTERS**. Internal skeletal stress and structural joint trauma do not cause external visor blood splatters.

### 2.8 Subterranean Liquid Hazard Matrix & Emergent Neutralization
Subterranean caverns feature 4 procedural liquids, each with highly differentiated viscosity, suit physics, hazard profiles, HUD visual feedback, and cross-liquid neutralization:

| Liquid Material | Visual Identity | Viscosity & Mobility | Exosuit Reaction & Hazard Profile | HUD Visual Telegraph | Emergent Cleanse / Counterplay |
|---|---|---|---|---|---|
| **Corrosive Acid**<br/>(`MAT_ACID = 15`) | Bubbling toxic neon-green pool with rising caustic fumes | $0.55\times$ speed, $4.5\times$ drag, limited jump velocity | **Shield Dissolution**: $-25\,\text{shield/s}$.<br/>**Direct HP Damage**: $14\,\text{HP/s}$ active (`DamageSource::CorrosiveAcid`).<br/>**Chemical Burn**: Leaves $2.0\,\text{s}$ lingering burn ($4.5\,\text{HP/s}$). | Neon-green visor wash, caustic border etching, dripping acidic visor droplets | **Aquifer Water Immersion**: Submerging into clean aquifer water instantly washes and neutralizes acid burn! |
| **Cryogenic Coolant**<br/>(`MAT_COOLANT = 16`) | Frosted sub-zero pale cyan fluid with crystalline fog | $0.45\times$ speed, $5.5\times$ drag, **thrusters locked out** | **Thermal Quench**: Suit heat instantly plunges to $0.0$.<br/>**Power Drain**: $-15\,\text{power/s}$ battery drain.<br/>**Hypothermic Damage**: $6\,\text{HP/s}$ (`DamageSource::CryogenicCoolant`).<br/>**Frost Chill**: Servos chilled for $2.5\,\text{s}$. | Frosted cyan wash, crystalline border vignette, sharp perimeter frost teeth | **Thermal Thaw**: Magma heat or aquifer water immersion rapidly clears frost chill. |
| **Molten Slag / Lava**<br/>(`MAT_THERMITE_SLAG = 6`) | Boiling viscous magma with buoyant rising embers | $0.40\times$ speed, $8.5\times$ drag, plunge braking | **Extreme Overheat**: $+40\,\text{heat/s}$, triggers emergency overheat lockout.<br/>**Thermal Burns**: $14\,\text{HP/s}$ active burn damage (`DamageSource::ThermalLava`). | Searing amber heat wash, bubbling heat tremor, red perimeter burn vignetting | **Water Cool**: Aquifer water dissipates heat ($-75\,\text{heat/s}$) and clears overheat lockout. |
| **Crystal Aquifer Water**<br/>(`MAT_CRYSTAL_AQUIFER = 11`) | Clear translucent azure pool with aquatic splash beads | $0.70\times$ speed, $2.8\times$ drag, buoyant swimming | **Safe Haven & Heat Sink**: $-75\,\text{heat/s}$ rapid cooling.<br/>**Damage**: Strictly $0\,\text{HP}$ damage.<br/>**Universal Cleanse**: Neutralizes acid and thaws frost. | Translucent cyan water vignette, visor mist droplets | Safe navigation medium, fall impact cushioning, and hazard cleansing. |

### 2.9 Crystal Aquifer Waterfalls & Subterranean Lakes (`MAT_CRYSTAL_AQUIFER`)
- **Visuals & Atmosphere**: Translucent cyan water ripple shader with dynamic refraction, aquatic splash droplets, and cascading vertical water spray mist (`spawn_water_mist`).
- **Tactical Dynamics**: 
  - **Exosuit Rapid Cooling**: Water immersion rapidly dissipates thermal buildup ($-75.0\,\text{heat/s}$), instantly clearing thruster/drill overheating status.
  - **Universal Hazard Neutralization**: Submerging in aquifer water instantly neutralizes lingering corrosive chemical burns (`m_acid_burn_timer = 0.0f`) and rapidly thaws frosted servo joints.
  - **Fluid Buoyancy & Drag**: Introduces viscous fluid drag limiting horizontal terminal velocity and cushioning vertical descents (buoyant terminal velocity clamped to $\ge -4.5\,\text{m/s}$), providing safe landing zones when jumping down chasms.
- **Encounter Locations**: `SubterraneanAquiferOasis`, `BioluminescentGlowwormGrotto`, `PrecursorCoolantReservoir`.

### 2.10 Long-Range Hazard Telegraphing (Distant Cavern Particle Simulation)
- **45m Active Simulation Radius**: The renderer and world engine continuously track active hazard voxels (gas vents, radiation deposits, lava pools, and aquifer streams) up to $45\,\text{m}$ away from the player.
- **Visual Clarity from Afar**:
  - **Toxic Gas Clouds**: Heavy billows ($0.55 - 0.88\,\text{m}$ diameter) lingering for $2.0 - 3.0\,\text{s}$ with upwards buoyancy drift, telegraphing poisonous pockets well before delvers step foot into lower basins.
  - **Radiation Fields**: Energetic green ionization sparks ($0.18 - 0.28\,\text{m}$) flickering around radioactive deposits, revealing ore veins and deadly irradiated moats across grand cavern halls.
  - **Aquifer Cascades**: Light aquatic spray mist ($0.35\,\text{m}$) telegraphed around subterranean waterfalls and reflecting pools.

### 2.11 Visceral Enemy Attacks vs. Environmental Damage Feedback
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
  | **Corrosive Acid** | Caustic hiss + chemical fizz | **NO** (Neon-green visor wash & drips) | Sizzle shimmer ($0.06$) |
  | **Cryogenic Coolant** | Cryo hiss + frost creak | **NO** (Sub-zero frost crystals & teeth) | Cold shudder ($0.03$) |
  | **Thermal Lava Slag** | `SoundCue::LavaBubble` + `SoundCue::ThermalHiss` | **NO** (Amber heat distortion) | Moderate ($\le 0.35$) |
  | **Spike Pit Puncture** | `SoundCue::SpikeRattle` + metallic impact | **NO** (Zero splatters) | High ($\ge 0.25$ + vertical impulse) |
  | **Aquifer Water Drag** | `SoundCue::WaterSplash` + bubbling | **NO** (Visor water mist beads) | **ZERO** ($0.0$ - soothing cooling) |

### 2.11 Precursor Vault Obstacles & Guardian Boss Encounters
Subterranean Precursor Vaults feature dedicated level-scaled obstacles guarding the approach to the breach blast door:
- **Sector 1: Crystal Aquifer Moat & Stepping Stones (`AquiferSteppingStones`)**:
  - Encircles the sanctum dais with deep bioluminescent flora and crystal water. Delvers must navigate a 4-way cross of stepping stones while under attack by an agitated Stalker Melee Guardian.
- **Sector 2: Molten Slag Trench & Obsidian Spikes (`HazardTrenchSlag`)**:
  - Sunken defense perimeter lined with razor obsidian spikes and molten slag flumes. Delvers must cross via a narrow titanium balance beam while Stalker Shooter Guardians snipe from elevated catwalk perches.
- **Sector 3: Abyssal Chasm, Radiation & Chitin Goliath (`ChasmAbyssRadiation`)**:
  - A sheer 10-meter drop plunging into molten slag and punji pits with ionizing radioactive seams embedded in the cliff faces. Delvers must hop floating precursor stepping monoliths ($y=14$) across open airspace to reach the isolated vault dais.
  - **Boss Guardian (Chitin Goliath)**: A towering apex aberrant boss monster with massive armor plating, heavy ground pounds triggering localized ceiling cave-ins, and high-damage melee strikes guarding the Sector 3 vault treasure.

### 2.12 Hostile Entity Hazard Fear & Tactical Environmental Exploitation
Enemies actively perceive environmental dangers and will not blindly run into or lunge across lethal hazards:
- **Obstacle Fear & Whisker Steering (`AberrantAI::steer_around_obstacles`)**:
  - Void Stalkers and Seismic Burrowers project directional feeler rays. If their forward trajectory encounters lethal fluids (`MAT_THERMITE_SLAG`, `MAT_ACID`) or physical traps (`MAT_OBSIDIAN_SPIKES`), the AI immediately steers laterally away via multi-angle avoidance whiskers.
- **Lunge Suppression Across Lethal Gaps (`AberrantAI::has_hazard_between`)**:
  - Stalkers suppress aggressive combat lunges if a pool of lava or acid separates them from the delver, transitioning instead to `Circling` or flanking behaviors rather than committing suicidal leaps.
- **Environmental Monster Damage & Fatalities**:
  - If knocked, blasted, or pressured into hazards (e.g. via kinetic shotgun blasts, explosive charge concussions, or collapsing cave-in debris), monsters take rapid continuous damage:
    - **Lava / Thermite Slag**: $35 - 45\,\text{HP/s}$ thermal burn, causing panic shrieks, scramble escapes, and fatal incineration into ash.
    - **Corrosive Acid**: $25 - 30\,\text{HP/s}$ dissolution damage, stunning charges and dissolving chitinous carapaces.
    - **Obsidian Punji Spikes**: Instant $20 - 25\,\text{HP}$ puncture trauma and severe crawl slowdown.
- **Tactical Delver Play**: Delvers can use these mechanics defensively by positioning themselves behind hazard pools, baiting burrower charges into lava lakes to trigger self-stuns, or blasting stalkers into punji spike pits.

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
