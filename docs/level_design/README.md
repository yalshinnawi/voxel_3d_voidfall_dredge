# Voidfall Dredge - Level Design & Room Archetype Visual Reference Catalog

This catalog documents the **15 room archetypes** and the **connecting corridor bulkhead vault** in *Voidfall Dredge*. It serves as an interactive visual reference guide for level designers, environment artists, and gameplay programmers.

---

## 1. Master 4x4 Visual Showcase Montage

![Level Shapes 4x4 Montage](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/level_shapes_showcase.jpg)

*Consolidated 4x4 contact sheet ($1600 \times 900$) capturing all 16 architectural components under calibrated cavern illumination and sector-specific PBR tonemapping. Generated automatically via `python scripts/capture_level_shapes.py`.*

---

## 2. Dynamic Level Scaling by Sector

Levels scale dynamically in physical area, room count, corridor complexity, and hazard density across the 3 expedition sectors:

| Sector | Name | Grid Size | Rooms | Corridors | World Size ($X \times Z$) | Active Voxel Chunks | Primary Palette |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **Sector 1** | *Crystalline Caverns* | $3 \times 3$ | 9 | 12 | $72 \times 72$ | $3 \times 3$ ($96 \times 96$) | Voidite crystals, raw basalt, granitic rock |
| **Sector 2** | *Subterranean Vault* | $4 \times 4$ | 16 | 24 | $96 \times 96$ | $3 \times 3$ ($96 \times 96$) | Industrial steel plating, magma flumes, spikes |
| **Sector 3** | *Fault-Line Collapse* | $5 \times 5$ | 25 | 40 | $120 \times 120$ | $4 \times 4$ ($128 \times 128$) | Cosmic void rift, radioactive ores, abyssal drops |

---

## 3. Complete Architectural Archetype Catalog

### Slot 1: Spawn Staging Cavern
![Spawn Staging Cavern](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/01_spawn_staging_cavern.png)
- **Sector Availability**: All Sectors (Mandatory initial deployment room at grid `[0, 0]`)
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Reinforced central titanium arrival pad with clearance floodlights.
  - Overhead vertical insertion shaft in ceiling mantle ($y=24..26$).
  - Modular climbing crates and supply containers ($y=5..7$) enabling mantle reconnaissance.
- **Gameplay Role**: Safe start zone; guaranteed 28m predator buffer preventing initial ambush.

---

### Slot 2: Mining Pillar Hall
![Mining Pillar Hall](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/02_mining_pillar_hall.png)
- **Sector Availability**: Sector 1+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - 4 massive monolithic structural load-bearing columns at $(\pm 4, \pm 4)$.
  - High arched natural stone bridge spanning the cavern ceiling at $y=11$.
  - Rich vertical veins of `MAT_VOIDITE_ORE` and `MAT_TITANIUM_ORE`.
- **Parkour & Mobility**: Grapple anchors along the arched bridge; climbable rock rungs on pillars.
- **Gameplay Role**: High-value mining chamber; requires vertical excavation and careful structural collapse awareness.

---

### Slot 3: Crystalline Geode
![Crystalline Geode](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/03_crystalline_geode.png)
- **Sector Availability**: Sector 1+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Hollow ellipsoidal crystal shell carved smoothly with radial noise.
  - Central Voidite crystal spire at $(x=36, z=36)$ emitting vibrant violet bioluminescence.
  - Perimeter stepping-stone ring walkway at $y=8$.
- **Parkour & Mobility**: Spiral jumping around perimeter stone ring; grapple swing points across central crystal spire.
- **Gameplay Role**: Prime Voidite harvesting zone; excellent acoustic resonance for Sonar Pulse pings.

---

### Slot 4: Terraced Quarry
![Terraced Quarry](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/04_terraced_quarry.png)
- **Sector Availability**: Sector 1+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - 4-tier amphitheater excavation pit stepping down into the bedrock.
  - Tiered ledges at $y=5, 7, 9, 11$ with titanium scrap seams.
  - Spiral pedestrian ramps carved into the perimeter walls.
- **Parkour & Mobility**: Ledge-to-ledge drop-downs; thruster-assisted vertical ascends between tiers.
- **Gameplay Role**: Multi-level sightlines; Delvers have high ground advantage against patrolling Void Stalkers.

---

### Slot 5: Industrial Vault Bunker
![Industrial Vault Bunker](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/05_industrial_vault_bunker.png)
- **Sector Availability**: Sector 2+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Reinforced precursor blast walls and perimeter bulkheads (`MAT_INDUSTRIAL_BULKHEAD`).
  - Open 4m-wide doorway portals on all 4 cardinal faces.
  - Suspended mezzanine catwalk at $y=9$ with glowing computer control consoles.
- **Parkour & Mobility**: Vertical wall climbing to reach the mezzanine; overhead gantry walks.
- **Gameplay Role**: Tactical holdout bastion; bulkheads shield against falling ceiling debris. In Sector 2, houses the Vault Relic.

---

### Slot 6: Fault Line Crevasse
![Fault Line Crevasse](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/06_fault_line_crevasse.png)
- **Sector Availability**: Sector 2+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Deep tectonic magma fissure running through the chamber floor down to $y=2$.
  - Toxic gas fissures and molten slag pockets (`MAT_THERMITE_SLAG`).
  - Fragile natural stone bridge spanning the fissure at $y=4$.
- **Environmental Hazards**: Thermal burn ($-24\,\text{HP/s}$) upon stepping into molten slag; toxic gas clouds.
- **Gameplay Role**: High-risk chokepoint; forces deliberate navigation over the bridge or precision thruster jumps.

---

### Slot 7: Abyssal Vertical Chasm
![Abyssal Vertical Chasm](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/07_abyssal_vertical_chasm.png)
- **Sector Availability**: Sector 3 (Exclusive high-tier abyss)
- **Dimensions**: $18 \times 18 \times 25\,\text{m}$ (Floor: $y=2$, Ceiling: $y=26$)
- **Key Features**:
  - Immense 24-meter vertical shaft plunging down through the bedrock.
  - Spiral perimeter catwalk ledges at $y=9, 14, 19$.
  - Hanging grapple stalactites suspended from the ceiling mantle at $y=22..25$.
- **Parkour & Mobility**: Demands Grappling Hook maneuvers, controlled thruster descents, and wall-running along spiral ledges.
- **Gameplay Role**: Extreme vertical platforming arena; falling from the top without thruster braking causes severe fall damage.

---

### Slot 8: Radioactive Core Sanctuary
![Radioactive Core Sanctuary](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/08_radioactive_core_sanctuary.png)
- **Sector Availability**: Sector 3
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Searing moat of irradiated liquid and toxic ore (`MAT_RADIOACTIVE_ORE`).
  - Stepping-stone pillars crossing the moat.
  - Towering central Voidite Core Monolith glowing with dense gamma emissions.
- **Environmental Hazards**: Proximity radiation buildup; exposure drains player stamina and triggers Geiger counter audio.
- **Gameplay Role**: High-risk, high-reward extraction zone with the densest cluster of Pure Voidite in the game.

---

### Slot 9: Extraction Landing Bay
![Extraction Landing Bay](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/09_extraction_landing_bay.png)
- **Sector Availability**: All Sectors (Target evacuation objective at the farthest corner of the grid)
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Evacuation beacon touchdown cradle at $(x=36, y=4, z=36)$.
  - High unobstructed exhaust launch chimney extending through the ceiling ($y=24..27$).
  - Perimeter blast barriers and defensive holdout barricades.
- **Gameplay Role**: Beacon deployment arena; initiates the 40s holdout defense against relentless swarms of Void Stalkers.

---

### Slot 10: Magma Caldera Lake
![Magma Caldera Lake](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/10_magma_caldera_lake.png)
- **Sector Availability**: Sector 2+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Vast molten thermite slag lake ($y=4$) filling the entire lower chamber.
  - Basalt stepping pillars with dynamic height variance ($y=4..5$).
  - Central rock island housing rich mineral veins.
- **Environmental Hazards**: Stepping into lava inflicts $-24\,\text{HP/s}$ thermal damage, screen trauma, and thermal heat buildup.
- **Parkour & Mobility**: Precision hopping across basalt pillars; grappling to overhead ceiling anchors to cross wide gaps.
- **Gameplay Role**: Severe movement restriction; creates tension when stalkers flank from multiple angles.

---

### Slot 11: Spike Trench Arena
![Spike Trench Arena](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/11_spike_trench_arena.png)
- **Sector Availability**: Sector 2+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Sunken trench floor lined with deadly bone and punji spikes (`VOXEL_FLAG_SPIKE`).
  - Elevated high-altitude balance beam catwalks at $y=6$ with jump gaps.
  - Safe perimeter arrival walkways at room entry thresholds.
- **Environmental Hazards**: Falling onto spikes deals instant $-25\,\text{HP}$ puncture damage and repels the player upwards ($+5.5\,\text{m/s}$).
- **Parkour & Mobility**: Tightrope walking on narrow beams; timing thruster bursts across jump gaps.
- **Gameplay Role**: High-tension combat; dodging enemy plasma bolts while maintaining footing on narrow beams.

---

### Slot 12: Void Singularity Rift
![Void Singularity Rift](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/12_void_singularity_rift.png)
- **Sector Availability**: Sector 3
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=2$, Ceiling: $y=25$)
- **Key Features**:
  - Bottomless cosmic void abyss dropping below bedrock.
  - Suspended inverted pyramid monolith floating at $y=12$.
  - Disconnected floating obsidian stepping platforms at $y=10$ and $y=14$.
- **Environmental Hazards**: Falling below $y \le 1.8\,\text{m}$ triggers Gravitational Rift damage ($-25\,\text{HP}$) and upward repulsion ($+4.0\,\text{m/s}$).
- **Parkour & Mobility**: Zero-G platform hopping; slingshot grappling between floating monoliths.
- **Gameplay Role**: Pinnacle endgame challenge combining platforming precision with abyssal void hazards.

---

### Slot 13: Fungoid Bio Grotto
![Fungoid Bio Grotto](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/13_fungoid_bio_grotto.png)
- **Sector Availability**: Sector 1+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Subterranean fungal garden filled with giant bouncy bioluminescent mushroom caps at $y=8, 12, 16$.
  - Vibrant emerald bioluminescent spore illumination.
  - Volatile spore gas pods hanging along the walls.
- **Parkour & Mobility**: Bouncing on giant mushroom caps launches the Delver high into the air; bypasses ground obstacles.
- **Gameplay Role**: High-speed vertical traversal; spore bounce enables rapid escapes from aggressive stalker packs.

---

### Slot 14: Laser Defense Foundry
![Laser Defense Foundry](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/14_laser_defense_foundry.png)
- **Sector Availability**: Sector 2+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Ancient precursor smelting facility with molten slag flumes.
  - High industrial gantry catwalks at $y=8$.
  - Heavy overhead crane beam spanning across the ceiling at $y=16$.
- **Parkour & Mobility**: Ladder rungs leading up to crane beams; tight catwalk sprint routes.
- **Gameplay Role**: Industrial cover-shooter arena; catwalk railings offer firing positions overlooking ground patrols.

---

### Slot 15: Crumbling Arch Canyon
![Crumbling Arch Canyon](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/15_crumbling_arch_canyon.png)
- **Sector Availability**: Sector 2+
- **Dimensions**: $18 \times 18 \times 22\,\text{m}$ (Floor: $y=4$, Ceiling: $y=25$)
- **Key Features**:
  - Deep 14-meter gorge sliced through the rock from north to south.
  - High rim cliffs at $y=6$ flanking both sides.
  - 3 natural stone arches spanning across the gorge at $y=7, 8, 11$.
  - Hanging stalactites ideal for grapple swing anchors.
- **Parkour & Mobility**: Crossing via crumbling arches; swinging across the gorge with the grapple hook.
- **Gameplay Role**: Long-range sightlines; ideal for Scout sniper rifles and long-range sonar reconnaissance.

---

### Slot 16: Corridor Bulkhead Vault
![Corridor Bulkhead Vault](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/16_corridor_bulkhead_vault.png)
- **Sector Availability**: All Sectors (Connecting seams between rooms)
- **Dimensions**: $4 \times 4\,\text{m}$ Cross-Section (Floor: $y=4$, Ceiling: $y=9$)
- **Key Features**:
  - Reinforced structural ribs every 2 meters lining the tunnel walls.
  - Heavy blast door bulkhead (`MAT_INDUSTRIAL_BULKHEAD`) sealing the corridor midsection.
  - Amber bulkhead warning beacons (`LuminaryType::CorridorBulkheadLight`) casting warm clearance illumination.
- **Gameplay Role**: Chokepoints, defensible retreat positions, and natural shelter against ceiling cave-in debris.

---

## 4. Re-running Visual Capture & Telemetry Verification

To regenerate all 16 reference screenshots and the master montage after altering room geometries or voxel materials:

```bash
python scripts/capture_level_shapes.py
```

This autonomously:
1. Rebuilds `VoidfallDredge.exe` via CMake MSVC Release.
2. Stages each of the 16 architectural components in sequence with pristine lighting.
3. Computes mean luminance and non-black ratios to verify zero blackouts.
4. Writes 16 uncompressed $1600 \times 900$ master PNGs to `docs/level_design/shapes/`.
5. Composites and outputs `docs/level_design/level_shapes_showcase.jpg` and `level_shapes_showcase.png`.
6. Generates `docs/level_design/level_shapes_report.txt` and `level_shapes_report.json`.
