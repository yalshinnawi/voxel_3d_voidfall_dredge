# Subterranean Environmental Hazards & Parkour Mechanics

## 1. Overview & Tactical Philosophy
*Voidfall Dredge* draws inspiration from high-stakes survival excavation and movement-driven extraction games (*Deep Rock Galactic*, *Risk of Rain 2*, *Returnal*, *Spelunky*, *Metroid Prime*). Rather than passive background scenery, subterranean caverns are active, hostile ecosystems where spatial awareness, parkour timing, and suit mobility upgrades are mandatory for survival.

---

## 2. Hazard Taxonomy & Mechanics

### 2.1 Molten Thermite Slag / Lava Lakes (`MAT_THERMITE_SLAG`)
- **Visuals & Atmosphere**: Boiling amber-orange luminescence, rising thermal heat distortion, bubbling slag fissures.
- **Damage Profile**: Continuous thermal burn dealing **$24\,\text{HP/s}$** ($0.4\,\text{HP}$ per $60\,\text{Hz}$ tick).
- **Physical Effects**: Accelerates suit heat buildup, adds camera trauma, and generates high-frequency HUD alarms (`CRITICAL: THERMAL BURNS / LAVA CONTACT (-24 HP/s)`).
- **Encounter Locations**: `MagmaCalderaLake`, `FaultLineCrevasse`, `LaserDefenseFoundry`.
- **Counterplay & Parkour**: Basalt hopping pillars spaced at 3-voxel intervals, diagonal natural basalt bridges, and grapple line crossing over the molten flumes.

### 2.2 Crystalline Spike Pits & Punji Trenches (Voxel Flag `0x0F`)
- **Visuals & Atmosphere**: Needle-sharp obsidian and crystallized mineral spikes protruding from sunken pit trenches.
- **Damage Profile**: Instant puncture trauma dealing **$25\,\text{HP}$** upon contact.
- **Physical Effects**: Upward repulsive vertical impulse ($+5.5\,\text{m/s}$) launching the delver upwards to prevent continuous clipping while punishing missteps, accompanied by HUD warning (`PUNCTURE HAZARD: SPIKE TRAP DETECTED (-25 HP)`).
- **Encounter Locations**: `SpikeTrenchArena`.
- **Counterplay & Parkour**: High balance beam catwalks ($y=6$) spanning the arena with intentional 2-voxel jump gaps requiring timed sprint-jumps or thruster bursts.

### 2.3 Bottomless Void Singularity Rifts (`MAT_AIR` below $y \le 1.8\,\text{m}$)
- **Visuals & Atmosphere**: Pitch-black cosmic abysses where planetary bedrock has collapsed, illuminated only by eerie deep-violet void motes and floating monoliths.
- **Damage Profile**: Gravitational crushing damage dealing **$25\,\text{HP}$**.
- **Physical Effects**: Gravitational expulsion field automatically repels the player upwards to $y=6.5\,\text{m}$ with upward velocity ($+4.0\,\text{m/s}$) and HUD warning (`VOID RIFT: GRAVITATIONAL REPULSION (-25 HP)`).
- **Encounter Locations**: `VoidSingularityRift`, `AbyssalVerticalChasm`.
- **Counterplay & Parkour**: Zero-gravity floating obsidian platforms suspended across the abyss and grapple stalactite reeling.

### 2.4 Toxic Gas Vents & Spore Pockets (`MAT_GAS`)
- **Visuals & Atmosphere**: Dense emerald-green gaseous clouds venting from subterranean fissures or exploding fungal spore pods.
- **Damage Profile**: Obscures visibility and accelerates respiratory suit degradation if lingered in.
- **Physical Effects**: Non-solid gas volume that can be cleared by kinetic charges or drill suction.
- **Encounter Locations**: `FaultLineCrevasse`, `FungoidBioGrotto`.
- **Counterplay & Parkour**: Elevated mushroom cap platforms and perimeter rock catwalks bypassing the gas-filled floor basin.

### 2.5 Radioactive Ore Moats (`MAT_RADIOACTIVE_ORE`)
- **Visuals & Atmosphere**: Pulsing phosphorescent green irradiation with auditory Geiger-counter clicking.
- **Damage Profile**: Proximity radiation accumulating up to **$24\,\text{Rad/s}$** inside a 12-meter radius.
- **Physical Effects**: Radiation sickness reduces maximum stamina and accelerates suit integrity decay.
- **Encounter Locations**: `RadioactiveCoreSanctuary`.
- **Counterplay & Parkour**: Stepping stone paths across the irradiated mineral moat to reach the central Voidite monolith.

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
