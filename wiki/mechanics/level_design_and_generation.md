# Modular Level Design & Procedural Generation

## 1. Overview & Architectural Philosophy
Prior to this system, subterranean sectors were synthesized via unbounded continuous 3D trigonometric noise, causing disconnected floating blocks, unintended gaping voids, and players clipping into unloaded chunks. 

The **Modular Shape & Chamber Generator** ([src/voxel/level_shapes.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.hpp), [src/voxel/level_shapes.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.cpp)) combines predetermined geometric structures ("shapes that work") with randomized graph layout, progressive sector scaling, hazard mechanics, parkour routes, and mineral distribution.

---

## 2. Progressive Sector Scaling ($3\times3 \to 4\times4 \to 5\times5$)
As delvers delve deeper into Voidfall, sectors expand horizontally, increasing navigation complexity, expedition duration, and hazard density:

| Sector | Grid Size | Chamber Count | World Dimensions | Chunk Footprint | Expected Corridors |
|---|---|---|---|---|---|
| **Sector 1: Perimeter Drift** | $3\times 3$ | 9 Rooms | $72\times 72$ Voxels | $3\times 3$ Chunks ($96\times 96$ allocated) | 12 Corridors |
| **Sector 2: Volatile Fault** | $4\times 4$ | 16 Rooms | $96\times 96$ Voxels | $3\times 3$ Chunks ($96\times 96$ allocated) | 24 Corridors |
| **Sector 3: Void Cradle** | $5\times 5$ | 25 Rooms | $120\times 120$ Voxels | $4\times 4$ Chunks ($128\times 128$ allocated) | 40 Corridors |

*Formula for Lattice Corridors*: $D(W - 1) + W(D - 1)$, guaranteeing fully meshed grid lattice connectivity with multiple alternate exploration paths.

---

## 3. Chamber Archetypes (23 Distinct Rooms)

Voidfall Dredge features 23 meticulously handcrafted room archetypes categorized by tactical role, verticality, environmental hazards, and parkour challenges:

| Shape Archetype | Sector Gating | Dimensions | Tactical Profile & Hazard Features | Parkour & Traversal Mechanics |
|---|---|---|---|---|
| **Spawn Staging Cavern** | All Sectors (Grid 0,0) | $20\times 12\times 20$ | Arrival staging pad ($y=4$), titanium reinforcement, overhead drop-pod beacon shaft ($y=24$). Safe staging zone. | Stepped mineral training crags and climbing crates. |
| **Mining Pillar Hall** | Sector 1+ | $24\times 12\times 24$ | 4 massive structural support columns with embedded vertical veins of `MAT_VOIDITE_CRYSTAL` and `MAT_TITANIUM`. | Arched stone balance bridge connecting columns at $y=room.floor\_y+4$. |
| **Crystalline Geode** | Sector 1+ | $20\times 16\times 20$ | Hollow ellipsoidal geode grotto. Concave shell lined with emissive violet `MAT_VOIDITE_CRYSTAL`. Central towering crystal spire. | Perimeter stepping-stone ring suspended along the concave walls at $y=8$. |
| **Terraced Quarry** | Sector 1+ | $22\times 14\times 22$ | 4-tiered stepped amphitheater excavation pit ($y=4, 6, 8$) with embedded titanium scrap seams. | Stepped terraces and diagonal ramps allowing fast sprint-climbing. |
| **Industrial Vault Bunker** | Sector 2+ | $20\times 10\times 20$ | Pre-collapse research bunker with titanium wall panels. Sealed by reinforced vault blast door frames (`MAT_REINFORCED_VAULT_DOOR`). | Elevated perimeter catwalk mezzanine ($y=9$) accessible via corner ladder columns. |
| **Fault-Line Crevasse** | Sector 2+ | $22\times 12\times 22$ | Tectonic rift bisecting chamber down to bedrock ($y=1$). Toxic gas pockets (`MAT_GAS`) with green luminescence. | Narrow eroded natural stone bridge and overhead grapple anchor points. |
| **Abyssal Vertical Chasm** | Sector 3 Exclusive | $22\times 24\times 22$ | 24-meter deep abyss with unstable hanging ceiling stalactites. Lethal drop hazard. | Spiral rock ledges ascending at $y=9, 14, 19$, requiring thruster hover and grapple tension climbing. |
| **Radioactive Core Sanctuary** | Sector 3 Exclusive | $20\times 14\times 20$ | Dangerous chamber encircled by a moat of `MAT_RADIOACTIVE_ORE` (+24 Rad/s). Central dais with pure Voidite monolith. | Island hopping stepping stones over the irradiated mineral moat. |
| **Extraction Landing Bay** | All Sectors (Grid W-1, D-1) | $22\times 16\times 22$ | Evacuation landing zone. Reinforced beacon cradle, high launch chimney ($y=25$), defensive blast barricades for 40s holdouts. | Barricade vaulting and multi-level cover for squad defense. |
| **Magma Caldera Lake** | Sector 2+ | $22\times 14\times 22$ | Searing volcanic chamber with molten thermite slag lake (`MAT_THERMITE_SLAG`, -28 HP/s burn). Central lava geyser chimney. | Basalt stepping stones and diagonal natural stone arch spanning the molten lake at $y=7$. |
| **Spike Trench Arena** | Sector 2+ | $22\times 14\times 22$ | Sunken arena pit lined with crystalline punji spikes (flag `0x0F`, -25 HP puncture damage + upward impulse). Central reward dais. | High balance beam catwalk bridges spanning at $y=6$ with intentional parkour jump gaps. |
| **Void Singularity Rift** | Sector 3 Exclusive | $24\times 22\times 24$ | Bedrock crust completely collapsed into bottomless cosmic abyss. Central gravitational singularity monolith ($y=7..13$). | Floating zero-gravity obsidian stepping platforms suspended in open void ($y=6, 8, 11, 14$). |
| **Fungoid Bio-Grotto** | Sector 1+ | $22\times 16\times 22$ | Bioluminescent cavern filled with towering subterranean mushrooms and toxic spore pockets (`MAT_GAS`). | Giant phosphorescent tiered mushroom caps ($y=8, 12, 16$) serving as bouncy parkour stepping platforms. |
| **Laser Defense Foundry** | Sector 2+ | $22\times 16\times 22$ | Precursor industrial smelting facility with molten slag drainage flumes and security beacon pylons. | Elevated perimeter gantry catwalks ($y=8$) and overhead crane beam ($y=16$) for grapple traversal. |
| **Crumbling Arch Canyon** | Sector 2+ | $24\times 16\times 24$ | 14-meter deep gorge flanked by high basalt rim cliffs ($y=6$) with hanging ceiling stalactites. | Three natural stone arches spanning the chasm at $y=7, 8, 11$ and ceiling grapple swing anchors. |
| **Subterranean Aquifer Oasis** | Sector 1+ | $26\times 22\times 26$ (Mega) | Verdant crystal oasis with shimmering water pools (`MAT_CRYSTAL_AQUIFER`), cascading vertical waterfall, bioluminescent flora banks, and moisture stalactites. Rapid exosuit cooling. | Basalt stepping stones, shoreline paths, and water cushion descents. |
| **Colossal Abyssal Chasm** | Sector 2+ | $26\times 24\times 26$ (Mega) | Vast vertical drop pit with lethal punji spike trench at bottom. Spanned by a high-tension suspension bridge at $y=room.floor\_y+8$. | 3m-wide titanium suspension bridge with perimeter safety railings and hanging grapple stalactites. |
| **Molten Magma Foundry** | Sector 2+ | $26\times 20\times 26$ (Mega) | Boiling criss-crossing magma flumes and smelting columns. Elevated titanium industrial catwalk crossing the chamber. | Perimeter and cross catwalks ($y=room.floor\_y+3$) allowing safe traversal above searing slag channels. |
| **Toxic Miasma Swamp** | Sector 2+ | $26\times 18\times 26$ (Mega) | Low swamp basin with sinuous pockets of heavy toxic gas (`MAT_TOXIC_GAS`) visible from across the cavern, fungal spore arches, and bioluminescent moss. | Elevated winding granite root bridge ($y=room.floor\_y+3$) routing delvers safely above gas pockets. |
| **Prismatic Crystal Cathedral** | Sector 1+ | $26\times 22\times 26$ (Mega) | Towering vaulted cathedral hall flanked by 4 colossal hexagonal crystal columns, stepped dais, and elevated crystal skybridges ($y=room.floor\_y+7$). | Stepped crystal altar stairs and elevated crystal bridges linking the giant monolith columns. |
| **Ancient Titan Necropolis** | Sector 2+ | $26\times 20\times 26$ (Mega) | Deep fossil excavation site spanned by 5 massive prehistoric skeletal ribcage bone arches (`MAT_TITANIUM`) and wall scaffolding. | Excavated mineral seam trench, wall scaffolds ($y=room.floor\_y+4$), and bone arch grapple swinging. |
| **Bioluminescent Glowworm Grotto** | Sector 1+ | $24\times 18\times 24$ | Starry night cavern ceiling dotted with hundreds of glowing bio-points (`MAT_BIOLUMINESCENT_FLORA`), Voidite clusters, and a pristine reflecting lake. | Winding mossy shoreline trails and soothing subterranean water immersion. |
| **Precursor Coolant Reservoir** | Sector 2+ | $24\times 18\times 24$ | High-tech precursor vault featuring twin liquid coolant reservoirs, ruptured high-pressure overhead pipelines, and blast catwalks. | Central steel aisle, vault catwalks, and twin coolant immersion pools for instant thermal cooling. |
| **Colossal Vaulted Dredge Cathedral** | All Sectors (Mega) | $26\times 22\times 26$ | Soaring ribbed gothic arches stretching 21 meters above delvers ($y=4\to 25$). High suspension gantry catwalk at $y=19$, Voidite dais altar, and hanging stalactite chandeliers. | Elevated catwalk bridge at $y=19$, buttress ladder columns, and grapple swing anchors across the nave. |
| **Tectonic Abyssal Sinkhole** | Sector 2+ (Mega) | $26\times 22\times 26$ | Staggering multi-tiered subterranean sinkhole plunging to bedrock ($y=4$). Glowing magma fissure trough, stepped spiral terraces, central lookout monolith, and suspension cable bridge at $y=18$. | Descending spiral terraces ($y=18, 12, 8$), cable bridge crossing, and monolith parkour spire. |
| **Cyclopean Excavation Silo** | Sector 1+ (Mega) | $26\times 22\times 26$ | Titanic circular precursor excavation shaft with heavy titanium dredging basin. Double-tiered maintenance ring catwalks ($y=12, 20$) and overhead heavy crane girder at $y=23$. | Dual perimeter ring catwalks, vertical conduit pipe climbs, and high crane hoist grapple lines. |
| **Bioluminescent Firmament Abyss** | All Sectors (Mega) | $26\times 22\times 26$ | Vast subterranean celestial vault with an undulating floor of glowing crystal aquifer pools. Canopy at $y=23..25$ embedded with hundreds of emissive starlight crystals and high natural stone arch bridge at $y=19$. | Diagonal soaring stone arch bridge at $y=19$, crystal water cushion descents, and panoramic firmament sightlines. |

---

## 4. Multi-Floor Verticality & Dynamic Level Layouts

Voidfall Dredge implements multi-floor level layouts with non-square footprints, dynamic room sizes, and organic corridor networks:

### 4.1 Multi-Floor Assignments
Chambers are assigned vertical floor tiers during grid layout generation:
- **Floor 0 (Lower Caverns)**: Base floor at $y=4$, standard ceiling at $y=13..16$, with a 40% chance of a **High-Vault Atrium** expanding ceiling to $y=20..24$ for soaring clearance even in regular chambers.
- **Floor 1 (Upper Mezzanines)**: Elevated terrace chambers with base floor at $y=14$, ceiling at $y=25$.
- **Floor 2 (Multi-Floor Grand Vaults & Colossal Expanses)**: Vast high-verticality chambers starting at $y=4$ and soaring up to $y=25$ (up to 21 meters of continuous headroom!) with perimeter catwalks, observation galleries, suspension bridges, and climbing terraces.

### 4.2 Sloping Stepped Ramps & Elevation Transitions
When connecting chambers on different floor levels ($Floor_A \ne Floor_B$):
- Corridors dynamically carve stepped stone ramps connecting the lower and upper floor elevations.
- Ramps enforce a gradual slope of $\le 1$ block elevation gain per step ($min\_run = |\Delta y| + 2$), ensuring smooth auto-stepping.
- Ramps extend seamlessly into chamber threshold doorways so there are no sheer vertical drop-offs or clipping seams.

### 4.3 Diagonal Corridors, Ravines & Non-Square Layouts
- **Dynamic Centerline Interpolation**: Corridors interpolate between room centers $(x_A, z_A)$ and $(x_B, z_B)$, supporting diagonal shafts, angled intersections, and winding paths.
- **Ravines & Chasms**: Select inter-chamber connections carve wide ravines ($> 9$ voxels wide) featuring natural stone balance bridges and exposed mineral veins.
- **Dynamic Room Dimensions**: Chambers vary dynamically between compact storage grottos ($14 \times 8 \times 14$), standard halls ($20 \times 12 \times 20$), and massive grand caverns ($26 \times 18 \times 26$).
- **Organic Boundary Jitter**: Chamber perimeters apply deterministic 3D noise offsets to eliminate sterile blocky boxes while preserving structural envelope containment.

### 4.4 Player Auto-Step Physics
To support natural climbing of stepped ramps and terraces without requiring jump inputs:
- Horizontal collision resolution (`PlayerController::resolve_axis_collision(0)` and `(2)`) checks for 1-block obstacles ($\le 1.05\,\text{m}$ step height).
- If clear headroom exists above the step, the controller smoothly steps up onto the ledge.
- Ground detection checks underfoot blocks to ensure continuous grounded state when traversing stepped ramps horizontally.

---

## 5. Collision & Boundary Integrity
1. **Outer Boundary Containment**:
   - Voxel queries outside sector bounds ($x \le 3$ or $x \ge \text{width} - 4$, $z \le 3$ or $z \ge \text{depth} - 4$, $y \le 3$ or $y \ge 26$) return `true` (solid bedrock mantle) rather than `false` (air).
   - In `VoidSingularityRift`, the central void drop intentionally removes bedrock at $y \le 3$ so players plunge into the void rift hazard field.
2. **Continuous Collision Sub-Stepping & Directional Axis Clamping**:
   - `PlayerController::resolve_voxel_collisions` partitions movement into sub-steps of $\le 0.12$ voxels (up to 16 sub-steps per frame).
   - `resolve_axis_collision` isolates leading contact faces (checking only the head on upward movement, only feet on downward movement, and only leading faces horizontally) to prevent misclassifying wall voxels as floors or ceilings.
   - `depenetrate` evaluates minimum translation vectors to smoothly eject any overlapping geometry back into walkable airspace.
3. **Downward Floor Clamping**:
   - `PlayerController::clamp_to_surface` scans downwards from the player's current vertical elevation, guaranteeing safe placement on the room floor with $+0.95\,\text{m}$ clearance rather than on the external cavern roof.

---

## 6. GPU Greedy Meshing & Bitfield Vertex Resolution
In modern greedy chunk meshing ([src/voxel/greedy_mesher.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/greedy_mesher.cpp)), vertices on chunk boundaries lie at local coordinate 32 ($x, y, z \in [0, 32]$).
- **6-Bit Coordinate Encoding**:
  - `PackedVoxelVertex::data0` allocates 6 bits each for `localX`, `localY`, and `localZ` (values $0..63$).
  - Prevents coordinate truncation where $32 \pmod{32} = 0$ previously caused boundary quads to stretch backward across the chunk, occluding sightlines into adjacent rooms.
- **Dynamic Sector Chunk Allocation**:
  - `World::generate_world` dynamically calculates chunk dimensions: $\lceil\text{width}/32\rceil \times 1 \times \lceil\text{depth}/32\rceil$ (e.g., $4\times 1\times 4$ for Sector 3).
  - Pre-instantiates and samples all sector chunks prior to greedy meshing, guaranteeing neighbor chunk context for all internal border faces.

---

## 7. Automated Level Design & Collision Test Suite (`test_level_collision`)
To ensure zero clipping and complete solidity across all 15 room shapes, corridors, and dynamic sector grids, the test suite executes in $\approx 1.2\,\text{s}$:

```bash
# Direct test execution
./build/Release/test_level_collision.exe

# High-speed parallel runner across all 7 suites
python scripts/tdd.py
```

### Test Coverage Matrix (10 Modules)
1. **Module 1: Level Design Archetypes & Room Geometry**: Validates dynamic grid sizes ($3\times3$, $4\times4$, $5\times5$), bedrock, mantle, and sector gating.
2. **Module 2: High-Speed Wall Sprinting & Perimeter Bounds**: Sprints against walls in all rooms at $15\,\text{m/s}$ for 60 frames.
3. **Module 3: Ceiling Flying, Jetpack Thrusters & Grapple Reel**: Thrusts at $12\,\text{m/s}$ into ceilings and reels at $25\,\text{m/s}$.
4. **Module 4: Block Breaking & Broken Block Traversal**: Drills through walls, validates new doorway traversal and intact jambs.
5. **Module 5: Diagonal Corner Sliding & Static Depenetration**: Validates $45^\circ$ inner wall sliding and recovery from embedded states.
6. **Module 6: Isolated Base Shapes Stress (All 15 Archetypes)**: Verifies all 15 room shapes in isolation under multi-directional stress.
7. **Module 7: Random Shape Permutations & Integration Testing (30 Seeds)**: Audits 989 corridor threshold seams and verifies 100% 3D BFS topological reachability from spawn to extraction.
8. **Module 8: Full Sector Room-to-Room Sightlines & Traversal Tour**:
   - Delver executes an S-curve tour visiting 100% of all rooms across Sectors 1, 2, and 3.
   - Evaluates DDA line-of-sight raycasts through corridor portals, asserting $\ge 85\%$ open air visibility.
   - Walks player through each corridor from threshold to threshold without clipping into geometry.
9. **Module 9: Flight Carving, Block Breaking & Mesh Integrity Stress Test**:
   - Flies along a continuous 3D diagonal trajectory over 120 frames carving a $3\times 3\times 3$ tunnel through chunk borders ($X=32, Z=32$).
   - Generates and audits 9,000+ greedy mesh quads across all chunks.
   - Validates vertex coordinates $[0..32]$, normal indices $[0..5]$, AO $[0..3]$, material IDs, and asserts zero degenerate quads.
10. **Module 10: Environmental Room Dangers & Lethality Stress**:
   - Lava hazard immersion: validates viscous drag, suit thermal accumulation (+50 heat/s), burn damage (-28 HP/s), and lethal incineration.
   - Spike trench puncture: validates debounce protection timer and trapped impalement lethality.
   - High cavern drop impact: verifies survivable injury (~20-55 HP) vs high-velocity concussive downward impact fatality.
   - Void chasm singularity: verifies lethal abyss consumption on low-health fall.

---

## 8. Visual Shape Catalog & Automated 4x4 Showcase Harness

To make level iteration and visual inspection fast and accessible without requiring manual navigation across all 25 chambers:

```bash
# Automated 4x4 Shape Showcase Capture & Catalog Generation
python scripts/capture_level_shapes.py

# Direct Engine Execution
./build/Release/VoidfallDredge.exe --capture-level-shapes
```

### Visual Assets & Documentation
- **Showcase Catalog**: [docs/level_design/README.md](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/README.md) - Detailed breakdown of all 16 architectural components with dimensions, hazards, voxel IDs, and parkour routes.
- **Master 4x4 Contact Sheet**: [docs/level_design/level_shapes_showcase.jpg](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/level_shapes_showcase.jpg) - 16-slot $1600 \times 900$ visual matrix capturing all 15 room archetypes + corridor bulkhead vault with real-time luminance telemetry.
- **Individual High-Res Masters**: [docs/level_design/shapes/](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/level_design/shapes/) (`01_spawn_staging_cavern.png` through `16_corridor_bulkhead_vault.png`).
- **Telemetry Verification Reports**: `docs/level_design/level_shapes_report.txt` and `docs/level_design/level_shapes_report.json` verifying $100\%$ non-black ratios and healthy luminance across all chambers.

---

## 9. Endless Progression System

Voidfall Dredge supports up to **31 playable sectors** (indices 1–31, stored in `PlayerInventory::MAX_SECTOR_RECORDS = 32`). Sectors are generated procedurally at runtime from a compact descriptor system rather than hand-authored data.

### 9.1 Sector Unlock Gating

| Sector | Required Player Level | Additional Prerequisite |
|--------|----------------------|------------------------|
| 1 | 1 | — |
| 2 | 2 | — |
| 3 | 4 | — |
| 4 | 6 | Sector 3 cleared (completion rate > 0) |
| 5 | 8 | Sector 4 cleared |
| N (≥ 4) | `min(4 + (N−3)×2, 30)` | Sector N−1 cleared |

Classic sectors 1–3 use level-only gating (backward compatible with existing saves). Sectors 4+ require both the level threshold **and** the prior sector to have a non-zero `highest_completion_rate` in `UserProfile::sector_records`.

### 9.2 Procedural Sector Descriptors

Each sector card in the UI is synthesised from 12-entry cyclic lookup tables:

| Property | Formula / Cycle |
|----------|----------------|
| **Name** | Cycles through 12 biome names (Perimeter Drift → Resonance Vault), appending `[CYCLE N]` for repeats |
| **Depth** | `800 + (sector − 1) × 600` metres |
| **Voidite objective** | S1=25, S2=35, S3=50, S4+= `min(50+(S−3)×15, 200)` |
| **Special objective** | Every 5th sector: neutralise Alpha Units |
| **Hazard** | Cycles through 12 hazard types |
| **EXP multiplier** | `1.0 + (sector − 1) × 0.25×` |
| **Tier badge** | I (S1) / II (S2) / III (S3) / IV (S4–6) / V (S7–10) / VI (S11+) |
| **Accent colour** | Cyan / Amber / Crimson / Violet / Orange / Neon Green |

### 9.3 Sector Select Carousel (Paginated UI)

The Orbital Hub's sector select screen shows **3 cards per page** with `< PREV` / `NEXT >` navigation:

- Visible pool: sectors 1 through `min(highest_cleared_sector + 1, 31)`
- `m_carousel_page` persists per session
- Ghost slots render as `-- UNCHARTED TERRITORY --` on partial pages
- The **LAUNCH** button label dynamically reads `[ LAUNCH SECTOR N ]`
- Completed sectors display their best badge and completion % beneath the EXP multiplier

### 9.4 In-Expedition Scaling

| Parameter | Formula |
|-----------|---------|
| **Voidite target** | S1=25, S2=35, S3=50, S4+= `min(50+(S−3)×15, 200)` |
| **Countdown timer** | S3=180s, S4=170s, … floor at 60s (`max(180−(S−3)×10, 60)`) |
| **Ambient stalkers** | 1 per room; +1 per even-indexed room (S≥2); +1 per mod-3 room (S≥5) |
| **Enemy aggression** | `1.0 + (S − 1) × 0.08` |
| **Burrowers** | `max(0, 1 + (S−3)/3)` starting from Sector 3 |
| **Audio theme** | Cycles across 3 theme banks every 3 sectors |

### 9.5 Persistence

`UserProfile::sector_records[N]` (N = 1..31) stores per-sector best data:

```cpp
struct SectorRecord {
    int highest_completion_rate{0};  // 0-100%
    std::string best_badge{"UNEXPLORED"};
};
```

`highest_cleared_sector` is incremented by `PlayerInventory::finalize_run()` on successful extraction and serialised to `saves/save_data.json`.

---

## 10. Procedural Generation Dynamics & Layout Variation System

To guarantee high replayability across repeat expeditions while maintaining strict 3D topological reachability and zero clipping, the generation pipeline integrates five advanced procedural systems:

### 10.1 Weighted Probabilistic Room Selection & Decay
Replaces static archetype pools with dynamic seeded weighting:
- **Sector Filtering**: Sector 1 restricts selection to natural caverns (`MiningPillarHall`, `TerracedQuarry`, `CrystallineGeode`, `FungoidBioGrotto`). Sectors 2+ unlock volcanic calderas, industrial bunkers, laser foundries, and void singularities.
- **Dynamic Decay**: When an archetype is placed, its subsequent draw weight decays by $50\%$ to prevent visual monotony, enforcing a hard limit of $\le 2$ of any single archetype per expedition.
- **Guaranteed Archetype Quotas**: Sectors 2+ enforce at least one hazard chamber (magma/radiation/spikes) and at least one high-ceiling parkour chamber.

### 10.2 Elevation Biomes & Sloping Stepped Ramps
Rather than rigid single-cell checkerboarding, elevations are partitioned via dual-anchor Voronoi clustering:
- Lowland clusters ($y=4$) and highland plateaus ($y=14$) form organic multi-room biomes.
- Adjacent rooms sharing the same floor level connect via flat corridors.
- Inter-biome connections automatically construct sloping stepped ramps with $1.0\times$ rise-over-run, reinforced basalt footings, and solid mantle headroom.

### 10.3 Dynamic Massive Multi-Deck Placement
Large grand chambers ($21\times 21$ footprint, spanning $y=4\dots 25$):
- Dynamically select randomized interior grid coordinates rather than static deterministic slots.
- Enforce strict adjacency rules: massive chambers never share a boundary with another massive room or touch perimeter borders.
- Bounding geometry is clamped to `half_width = 10, half_depth = 10` with zero jitter, preserving corridor threshold seams.

### 10.4 Coherent 3D Mineral Seams in Host Rock Matrices
Host rock matrices surrounding rooms and corridors contain discoverable, drillable ore veins generated via 3D coherent spatial hashing:
- **Clustering**: $3\times 2\times 3$ voxel spatial cells create organic $3\text{--}6$ block ore streaks rather than scattered single voxels.
- **Sector Stratification**:
  - *Sector 1*: $75\%$ Titanium, $25\%$ Voidite Crystal.
  - *Sector 2*: $45\%$ Titanium, $40\%$ Voidite Crystal, $15\%$ Radioactive Ore.
  - *Sector 3+*: $30\%$ Titanium, $35\%$ Voidite Crystal, $35\%$ Radioactive Ore.

### 10.5 Seed-Driven Architectural Room Internals
Each room archetype evaluates a per-room coordinate seed hash `hash_coord(room.center.x, room.center.z, 0, salt)` to procedurally vary interior features:
- **Pillar Hall**: Varies column spacing between $3\dots 5$ voxels, switches bridge alignment between X-axis and Z-axis, and adjusts elevated parkour bridge height relative to `room.floor_y + 7`.
- **Crystalline Geode**: Adjusts inner parkour ring stepping stone radius ($4.2\dots 5.0\,\text{m}$), anchors rings relative to floor height, and offsets the central glowing crystal spire.
- **Terraced Quarry**: Rotates the concentric spiral access ramp across all 4 quadrants (NE, NW, SW, SE).
- **Magma Caldera**: Alternates basalt bridge arch trajectories between NE-SW and NW-SE diagonals and phase-shifts the stepping stone grid across the lava lake.
- **Spike Trench**: Switches balance beam bridge axes (X-only, Z-only, or full cross bridge) and alternates spike hazard grid parity.
- **Fungoid Grotto**: Seeds mushroom stem locations across quadrants, varies cap heights ($+4$, $+7$, $+10$ above floor), and adjusts phosphorescent cap diameters without obstructing doorway paths.


