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

## 3. Chamber Archetypes (30 Distinct Rooms)

Voidfall Dredge features 30 meticulously handcrafted room archetypes categorized by tactical role, verticality, environmental hazards, and parkour challenges:

| Shape Archetype | Sector Gating | Dimensions | Tactical Profile & Hazard Features | Parkour & Traversal Mechanics |
|---|---|---|---|---|
| **Spawn Staging Cavern** | All Sectors (Grid 0,0) | $20\times 12\times 20$ | Arrival staging pad ($y=4$), titanium reinforcement, overhead drop-pod beacon shaft ($y=24$). Safe staging zone. | Stepped mineral training crags and climbing crates. |
| **Mining Pillar Hall** | Sector 1+ | $24\times 12\times 24$ | 4 massive structural support columns with embedded vertical veins of `MAT_VOIDITE_CRYSTAL` and `MAT_TITANIUM`. | Arched stone balance bridge connecting columns at $y=room.floor\_y+4$. |
| **Crystalline Geode** | Sector 1+ | $20\times 16\times 20$ | Hollow ellipsoidal geode grotto. Concave shell lined with emissive violet `MAT_VOIDITE_CRYSTAL`. Central towering crystal spire. | Perimeter stepping-stone ring suspended along the concave walls at $y=8$. |
| **Terraced Quarry** | Sector 1+ | $22\times 14\times 22$ | 4-tiered stepped amphitheater excavation pit ($y=4, 6, 8$) with embedded titanium scrap seams. | Stepped terraces and diagonal ramps allowing fast sprint-climbing. |
| **Industrial Vault Bunker** | Sector 2+ | $20\times 10\times 20$ | Pre-collapse research bunker with titanium wall panels. Sealed by reinforced blast door frames (`MAT_VOLCANIC_BASALT`, anchored). Authentic `MAT_REINFORCED_VAULT_DOOR` blocks are strictly reserved for genuine procedural Precursor Vaults containing treasure caches. | Elevated perimeter catwalk mezzanine ($y=9$) accessible via corner ladder columns. |
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
| **Precursor Sanctum Vault** | Sector 1+ (Dedicated) | $22\times 14\times 22$ | Dedicated precursor vault chamber. Vaulted ceiling arches, runic titanium columns, crystal flora moat with stepping stones, sealed bulkhead portal. | Cross stepping stone path across moat, raised runic altar dais. |
| **Subterranean Vault Bastion** | Sector 2+ (Dedicated) | $24\times 16\times 24$ | Heavily fortified military vault bastion. Perimeter catwalk mezzanine, central obsidian spike & defense trench, reinforced blast gate. | Mezzanine ladder climbs, catwalk sniper perches, narrow central access bridge. |
| **Abyssal Vault Chasm** | Sector 3 Exclusive (Dedicated) | $26\times 24\times 26$ (Mega) | Deep tectonic vault chasm plunging 10m to bedrock. Lethal thermite slag & punji spike pits, irradiated cliff walls, floating monolithic stepping islands, isolated boss roost. | Floating monolith island hops ($y=14$), thruster-assisted void crossings, elevated boss arena. |

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

### 4.5 Organic Wall Texturing, Geological Strata & Surface Clutter
To prevent artificial planar smoothness and create authentic subterranean cave aesthetics:
1. **Multi-Harmonic 3D Wall Carving**:
   - `compute_room_wall_radius` modulates room contours with asymmetric harmonic lobes (frequencies 3 and 5), elevation-dependent vertical swell / strata ripples ($\sin(0.42y)$), and high-frequency cragginess.
   - Corridors modulate width along cardinal and diagonal axes with 3D noise offsets ($\pm 0.42\,\text{m}$), ensuring no two stretches of tunnel feel identical while strictly maintaining $\ge 1.8\,\text{m}$ traversability clearance.
2. **Sedimentary & Igneous Strata Bands**:
   - `check_doorway_or_solid` layers subterranean rock into undulating geological bands: alternating dark volcanic basalt (`MAT_VOLCANIC_BASALT`), fractured granite (`MAT_FRACTURED_GRANITE`), and ancient dredge bedrock (`MAT_DREDGE_BEDROCK`).
   - High-frequency spatial hashing injects mottled mineral inclusions and crumbly rock pockets.
   - Cavern wall bases in moist grottos and aquifer biomes spawn patches of moisture lichen (`MAT_BIOLUMINESCENT_FLORA`).
3. **Multi-Tier Surface & Wall-Mounted Clutter**:
   - Floor rubble and scree scale dynamically across three distinct tiers: fine gravel/scree ($0.32\times$), standard boulders ($0.62\times$), and colossal rock piles ($1.15\times$).
   - Ceiling stalactites scale between delicate drops ($0.35\times$), standard spears ($0.60\times$), and massive formations ($1.05\times$).
   - Dedicated **Wall-Mounted Clutter Pass** scans horizontal perimeter faces ($+X, -X, +Z, -Z$), anchoring protruding rock spurs, downward wall stalactite dripstones, and glowing crevice mineral clusters oriented along surface normal directions.

---

## 5. Dedicated Precursor Vault Chambers, Level Obstacles & Boss Guardians

To elevate vault encounters from generic wall cutouts into atmospheric, high-stakes capstone challenges, the level generator incorporates dedicated Precursor Vault Chambers ([src/voxel/level_shapes.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.hpp), [src/voxel/level_shapes.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.cpp)):

### 5.1 Vault Chamber Archetypes by Sector
1. **Sector 1: Precursor Sanctum Vault (`PrecursorSanctumVault`)**:
   - *Atmosphere*: Ancient sacred reliquary featuring soaring vaulted basalt ceiling arches, glowing titanium runic portal columns, and an elevated relic dais.
   - *Obstacle*: `VaultObstacleType::AquiferSteppingStones` — A circular crystal flora moat (`MAT_BIOLUMINESCENT_FLORA`) spanned by a four-way cross of precursor stepping stones leading to the sealed vault portal.
   - *Guardian*: `VaultGuardianRole::StalkerMelee` — An agitated Void Stalker lurks in the vaulted shadows, ambushing delvers approaching the breach door.
2. **Sector 2: Subterranean Vault Bastion (`SubterraneanVaultBastion`)**:
   - *Atmosphere*: Fortified precursor military garrison. High-elevation perimeter catwalk mezzanine ($y=room.floor\_y+4$), industrial ladders, titanium blast gate portal, and reinforced bulkhead pylons.
   - *Obstacle*: `VaultObstacleType::HazardTrenchSlag` — A sunken perimeter defense trench filled with razor obsidian spikes (`MAT_OBSIDIAN_SPIKES`) and molten thermite slag, crossed via a narrow titanium balance walkway.
   - *Guardian*: `VaultGuardianRole::StalkerShooter` — A specialized Void Stalker perched atop the catwalk vantage point firing long-range void projectiles.
3. **Sector 3: Abyssal Vault Chasm (`AbyssalVaultChasm`)**:
   - *Atmosphere*: Titanic subterranean vault fortress perched over a sheer 10-block tectonic abyss plunging to bedrock ($y=4$). Irradiated cliff walls, glowing radioactive seams, and an isolated titanium boss throne.
   - *Obstacle*: `VaultObstacleType::ChasmAbyssRadiation` — Delvers must navigate floating precursor monolithic stepping platforms ($y=14$) across lethal slag pits and ionizing radiation beds to reach the isolated vault dais.
   - *Boss Guardian*: `VaultGuardianRole::ChitinGoliath` — A colossal 3-meter armored apex aberrant terror guarding the ultimate vault treasure. Possesses a massive health pool, heavy ground slams causing ceiling cave-ins, and impenetrable chitin plating.

### 5.2 Seamless Room Integration & Doorway Clearance Invariants
- **Corridor Seam Collision Prevention**: `configure_vault_placement` verifies `has_unconnected_clear_wall` before picking host rooms. If all 4 cardinal walls have active corridors, the vault portal shifts off-center by $+4$ voxels along X, guaranteeing vault doors never collide with inter-chamber doorways or central traffic routes.
- **Physical Load-Bearing Grounding**: All vault catwalks, arches, and stepping monoliths enforce 6-connected vertical face contact down to solid bedrock or ceiling anchors. Automatic post-generation solvers ensure zero unanchored blocks.
- **Fluid & Hazard Isolation**: Dedicated vault chambers and designated vault host rooms are strictly excluded from procedural random fluid generation (`has_random_fluid`), preventing fluid puddles or slime from encroaching onto vault approaches or blocking breach explosive satchel placement.

### 5.3 Precursor Relic 3D Model & Gyroscopic Levitation Animation
Inside breached vault chambers, the Precursor Relic objective transcends static voxel cubes through dynamic PBR rendering and multi-axis gyroscopic animations ([src/graphics/renderer.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.hpp), [src/graphics/renderer.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp)):
- **Custom Model Voxel Decoupling (`VOXEL_FLAG_CUSTOM_MODEL`)**:
  - The relic block (`MAT_PRISMATIC_CRYSTAL`) is flagged with `VOXEL_FLAG_CUSTOM_MODEL | VOXEL_FLAG_EMISSIVE`.
  - In [src/voxel/packed_vertex.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/packed_vertex.hpp), `Voxel::is_custom_model()` suppresses static chunk greedy meshing (`is_renderable() == false`) while retaining full spatial solidity (`is_solid() == true`).
  - This preserves world collision and player crosshair interaction raycasts (`look_hit.block_pos == mission.vault().relic_pos`), seamlessly delegating visual presentation to the specialized PBR rendering pipeline.
- **Relic Dais & Focusing Prongs**:
  - An octagonal stepped dais socket anchored to the pedestal floor with recessed glowing cyan runic energy conduits.
  - 4 gilded precursor focusing claws arching inward around the dais, tipped with luminous cyan emitter crystals that generate the levitation field.
- **Counter-Rotating Astrolabe Gimbal Rings**:
  - **Outer Ring**: Gilded precursor gold band tilted $24^\circ$ off-axis, revolving at $+1.1\,\text{rad/s}$ with 4 cardinal runic quadrature nodes.
  - **Inner Ring**: Polished obsidian-titanium band tilted $-32^\circ$ in opposition, counter-revolving at $-1.6\,\text{rad/s}$ with 4 glowing violet flux nodes.
- **Floating Octahedral Hyper-Core**:
  - Smooth sinusoidal vertical hovering: $Y(t) = y_{\text{base}} + 0.54\,\text{m} + 0.065\,\text{m} \times \sin(2.2\,t)$.
  - Multi-faceted celestial octahedron crystal spinning axially at $\omega = 2.0\,\text{rad/s}$, housing a high-emissive white-gold singularity core.
  - 4 orbiting satellite crystal shards revolving at orbital radius $0.44\,\text{m}$ with synchronized axial wobble.
- **Additive Energy Containment Field**:
  - Dynamic laser containment vector beams anchoring each focusing claw to the hovering hyper-core.
  - Ascending pulsing containment energy rings drifting vertically along the dais axis.

---

## 6. Collision & Boundary Integrity
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

## 7. GPU Greedy Meshing & Bitfield Vertex Resolution
In modern greedy chunk meshing ([src/voxel/greedy_mesher.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/greedy_mesher.cpp)), vertices on chunk boundaries lie at local coordinate 32 ($x, y, z \in [0, 32]$).
- **6-Bit Coordinate Encoding**:
  - `PackedVoxelVertex::data0` allocates 6 bits each for `localX`, `localY`, and `localZ` (values $0..63$).
  - Prevents coordinate truncation where $32 \pmod{32} = 0$ previously caused boundary quads to stretch backward across the chunk, occluding sightlines into adjacent rooms.
- **Dynamic Sector Chunk Allocation**:
  - `World::generate_world` dynamically calculates chunk dimensions: $\lceil\text{width}/32\rceil \times 1 \times \lceil\text{depth}/32\rceil$ (e.g., $4\times 1\times 4$ for Sector 3).
  - Pre-instantiates and samples all sector chunks prior to greedy meshing, guaranteeing neighbor chunk context for all internal border faces.

---

## 8. Automated Level Design & Collision Test Suite (`test_level_collision`)
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

## 9. Visual Shape Catalog & Automated 4x4 Showcase Harness

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

## 10. Endless Progression System

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

### 9.5 Persistence & Seed Immutability

`UserProfile::sector_records[N]` (N = 1..31) stores per-sector progress, performance records, and persistent procedural generation topology:

```cpp
struct SectorRecord {
    int highest_completion_rate{0};  // 0-100%
    std::string best_badge{"UNEXPLORED"};
    uint32_t saved_seed{0};          // Locked procedural seed once cleared
    bool completed{false};           // True if sector was successfully evacuated
};
```

- **Random Generation on Open**: When entering an uncompleted level (`completed == false`), a fresh, non-zero entropy seed is generated on every launch via `SaveSystem::generate_random_seed(sector)`. Failed or abandoned runs do not lock the seed.
- **Completion Lock & User Persistence**: Once a delver successfully completes an expedition (`safe_evac == true`), the level is marked completed and the seed used for that successful run is permanently locked into `saved_seed` and saved to `saves/save_data.json`.
- **Topological Immutability**: All subsequent expeditions into a completed sector by that user load the saved seed (`m_user_profile.sector_records[N].saved_seed`), ensuring the cavern layout, mineral veins, and chamber geometry remain permanently stable for that player.

---

## 11. Procedural Generation Dynamics & Layout Variation System

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

### 10.6 Dynamic Fluid Structures & Watertight Containment
To eliminate repetitive static fluid pools while ensuring fluids remain strictly contained and dormant at spawn (zero leaks, zero CPU cost at rest), the level generator procedurally samples six distinct fluid structure archetypes based on room archetype and seed:
- **`ContainedPool`**: Secluded corner basins with basalt bed ($y=floor\_y-1$), fluid surface ($y=floor\_y$), and elevated retaining rims ($y=floor\_y+1$).
- **`SteppingStonePool`**: Hazardous chemical or thermal pools featuring a central prize/mineral plinth ($y=floor\_y+2$) and hopping stepping stones allowing agile delvers to cross safely.
- **`WallCascade`**: Vertical wall-mounted conduits and stone flume buttresses rising up the wall to an emissive crystal/bulkhead discharge spout, splashing into an enclosed plunge basin with localized acoustic spray.
- **`CascadingTwinPools`**: Dual interconnected basins in a dumbbell/capsule topology separated by a solid stone weir ridge and traversal bridge.
- **`HazardTrench`**: Elongated security or drainage ditches of molten thermite slag, coolant, or caustic acid cutting across rooms with mid-point crossing stepping stones.
- **`DropChasmSump`**: Massive reservoir pools at the bottom of deep drops (`AbyssalVerticalChasm`, `ColossalAbyssalChasm`, `TectonicAbyssalSinkhole`) featuring central extraction/refinery platforms.
- **Watertight Invariants**: All fluid voxels are guaranteed non-air support beneath, full cardinal lateral containment on all 4 horizontal sides, and raised solid retaining rims exceeding liquid surface height. Room centers and doorway corridors remain dry and unobstructed.

### 10.7 Objective Material Budgeting, Spatial Gating & Scarcity Validation
To ensure that procedural levels demand genuine cave exploration rather than trivial goal fulfillment in starting areas:
- **Decoupling from Room Shapes**: Hardcoded objective voxels (`MAT_VOIDITE_CRYSTAL`) are completely removed from room shape meshes (geodes, pillar halls, cathedrals, training niches, firmaments). Fixed room meshes now feature decorative crystals (`MAT_PRISMATIC_CRYSTAL`, `MAT_TITANIUM`, `MAT_BIOLUMINESCENT_FLORA`), and mining `MAT_PRISMATIC_CRYSTAL` rewards rare salvage (+10 Salvage, +50 pts) instead of Voidite, strictly preventing players from bypassing the level quota by mining decorative structures.
- **Quota-Aware Tight Budget**: The level generator receives the mission target quota $T$. Total spawned objective material across the entire level is strictly budgeted to $T + 2$ (capped at $T + 3$), guaranteeing completability without resource abundance.
- **Strict Exploration Gating & Extraction Immunity**: Rooms with Manhattan grid distance $D \le 1$ from spawn (the spawn staging cavern and all directly adjacent rooms) contain strictly 0 objective materials. The Extraction Landing Bay also contains strictly 0 objective materials, requiring players to search the cavern network rather than camp near extraction.
- **Empty Rooms & Natural Scarcity**: To prevent uniform distribution where every room feels identical, objective materials are placed up to per-room capacity (2 to 4 voxels in Sectors 2+, up to 5 in Sector 1) before moving to subsequent rooms. Once the level quota goal is reached, remaining distant chambers receive 0 deposits. Across each level, $40\%\dots 52\%$ of all rooms have ZERO objective materials (`rooms_with_zero_objective_count()`), forcing delvers to traverse the cave system.
- **Pre-Determined Candidate Spawn Offsets across All 27 Archetypes**: Deposits are placed at discrete candidate nodes along perimeter wall crevices, high ledges, pillar flanks, and corner alcoves via `get_room_candidate_spawn_offsets()`, tailored specifically for each of the 27 room archetypes.
- **Wall-Embedded Vein Architecture & Surface Discovery Guarantee**:
  - *Zero Floating Crystals*: Objective Voidite crystals are strictly prevented from spawning in open air or obstructing floor walkways (`add_objective_voxel` enforces `is_mineable_solid(sample_base_voxel)`).
  - *Exposed Surface Outcrop*: `find_wall_surface_voxel()` anchors the primary piece of every deposit directly on the solid wall or structural face adjacent to room `MAT_AIR`, guaranteeing that the glowing purple crystal is visually discoverable by players exploring the room.
  - *Inward Vein Penetration*: For deposits of size $> 1$, additional crystals penetrate deeper into the solid wall/rock matrix along inward normals and lateral tangents behind the surface block. Delvers who spot the exposed outcrop on the wall drill into the rock face to uncover and extract the entire deposit.
- **Room-Level & Level-Wide Validation Checks**:
  1. `validate_room_resources(room)` audits every room upon layout completion, verifying that near-spawn and extraction rooms have 0 materials, and no room exceeds the maximum capacity of 4-5 voxels.
  2. `validate_level_resources()` audits the entire level, verifying $S \ge T$ (total spawned meets level quota), $S \le T + 3$ (strict scarcity ceiling), presence of zero-resource rooms, multi-room spread, zero floating blocks in air, and that every deposit position satisfies `sample_voxel(x, y, z) == MAT_VOIDITE_CRYSTAL`.
  3. Deterministic termination (no infinite loops or hanging threads).

### 10.8 Room Decoration & Surface Clutter Visual Decoupling
To eliminate player confusion between objective resources and purely decorative cavern features:
- **Architectural Material Realism**:
  - Non-mineral room decorations (excavation floors in Terraced Quarry, support pillar collars in Mining Pillar Hall, caldera chimneys in Magma Caldera Lake, bridge arches in Crumbling Arch Canyon, moisture stalactites in Aquifer Oasis) are constructed strictly from structural/natural materials (`MAT_VOLCANIC_BASALT`, `MAT_FRACTURED_GRANITE`, `MAT_TITANIUM`, `MAT_BIOLUMINESCENT_FLORA`).
  - Rock tunnel wall veins generated in `sample_point` host rock matrix use `MAT_TITANIUM`, `MAT_RADIOACTIVE_ORE`, or `MAT_BIOLUMINESCENT_FLORA` rather than crystal blocks, eliminating false-positive objective signals in transit corridors.
- **Visual Decoupling & Natural Subtlety of Decorative Crystals (`MAT_PRISMATIC_CRYSTAL`)**:
  - `MAT_PRISMATIC_CRYSTAL` is textured and shaded as soft celestine / natural aquamarine (`#268EBA`, subdued albedo `vec3(0.15, 0.56, 0.71)`, gentle ambient mineral luminescence `er=10, eg=48, eb=68`, and matte-crystalline roughness `0.45` to prevent blinding specular flash under headlamps).
  - It contains zero violet or purple undertones, completely distinguishing it from the deep amethyst/violet voidite objective crystals (`MAT_VOIDITE_CRYSTAL`, albedo `vec3(0.48, 0.12, 0.72)`).
  - **Natural Room Integration (Eliminating Center-of-Attention Distractions)**:
    - Decorative crystals never form central floor platforms, giant central spires, or floating monoliths in the middle of rooms.
    - Floors and central daises (such as the Prismatic Cathedral floor and stepped altar) are constructed from natural carved granite (`MAT_FRACTURED_GRANITE`).
    - Decorative crystals are subtly integrated as mineral seams and natural druses nestled into upper column fluting, wall crevices, cavern arches, and the outer geode shell (`r_norm >= 0.86f`).
- **Dedicated Clutter Archetypes (`ClutterType::PrismaticCrystal` vs `ClutterType::VoiditeCrystal`)**:
  - `ClutterType::VoiditeCrystal` is strictly reserved for objective Voidite blocks (`MAT_VOIDITE_CRYSTAL`), rendering deep purple amethyst spires with pulsing purple emissive radiance.
  - `ClutterType::PrismaticCrystal` renders delicate celestine/aquamarine mineral clusters with subtle ambient luminescence (`0.35x` emissive radiance, well below bloom blow-out thresholds, with compact organic scale `0.68..0.84x`).
  - Stalactites and stalagmite rubble props (`CeilingStalactite`, `FloorStalagmiteRubble`) maintain zero emissive boost and high matte roughness (0.95), preventing specular glints that could mimic glowing minerals under the headlamp.
- **HUD & Scanner Transparency**:
  - Reticle look-target prompt identifies objective blocks as `[LMB] DRILL VOIDITE (OBJECTIVE)` in violet (`COLOR_PURPLE`), and decorative crystals as `[LMB] SALVAGE PRISMATIC (DECORATIVE)` in cyan (`COLOR_CYAN`).
  - Sonar scanner and compass ribbons label anomalies as `[OBJ VOIDITE: Xm]` in magenta vs `[DECORATIVE PRISMATIC: Xm]` in cyan.

### 10.9 Shape-Conforming & Material-Consistent Block Breaking Visuals
To provide tactile, immersion-preserving feedback during mining and excavation:
- **Voxel Shape Conformance**:
  - Crack lines adhere strictly to the target block's physical geometry rather than a flat axis-aligned quad floating in mid-air.
  - **Ramps (`SHAPE_RAMP_EAST/WEST/SOUTH/NORTH`)**: The sloped hypotenuse face crack plane adheres to the 45° slope equations ($y = x$, $y = 1 - x$, $y = z$, $y = 1 - z$) with local tangent vectors oriented along and across the incline. Side flanks adhere to vertical triangular wall boundaries. In advanced breaking stages (stage $\ge 2$), 3D silhouette stress fractures wrap around the wedge's diagonal ridges, crest, toe, and heel lines while remaining entirely within the solid prism volume.
  - **Slabs (`SHAPE_SLAB_BOTTOM/TOP`)**: Crack overlays align with half-height planes ($Y = 0.5\text{m}$ for bottom slabs, $Y = 0.5\text{m}$ bottom face for top slabs), and side faces clamp to the half-height bounding box.
- **Material Texture & Mineral Consistency**:
  - Breaking animations and fracture patterns reflect the mineral composition:
    - *Voidite & Prismatic Crystals*: 60°/120° cleavage facet stars with amethyst-to-lilac resonant energy discharge.
    - *Industrial Bulkheads & Vault Doors*: 45° cold-rolled shear stress bands, perimeter bolt strain rings, and incandescent orange-amber sparks.
    - *Granite & Basalt*: Quartz-tan fissures and incandescent volcanic magma cooling joints.
    - *Radioactive Ore*: Ionizing phosphorescent toxic emerald to neon nuclear green discharge.
  - Breaking debris ejections (`spawn_crack_debris`, `spawn_break_particles`) eject along the slope normal and originate from the shape's true center of mass ($Y = 0.333\text{m}$ for ramps, $Y = 0.25\text{m}$ or $0.75\text{m}$ for slabs) with material-matched colors and spark dynamics.

### 10.10 Room Adjacency Constraints, Thematic Continuity & Contextual Selection Rules
To eliminate jarring level generation artifacts (such as two identical chambers appearing side-by-side, or lethal environmental hazards placed directly at the player's spawn door), the level generator implements a **Wavefront Constraint-Satisfaction Solver** ([src/voxel/level_shapes.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.cpp)):

1. **Zero Adjacent Same-Type Invariant (100% Hard Guarantee)**:
   - Orthogonally adjacent rooms ($|\Delta x| + |\Delta z| = 1$) are strictly forbidden from sharing the same `RoomShapeType`.
   - Diagonal neighbors ($|\Delta x| = 1, |\Delta z| = 1$) also penalize identical room types heavily, preventing repetitive room clusters across the subterranean grid.
2. **Thematic Categorization (`RoomCategory`)**:
   - Every chamber archetype is categorized into one of seven distinct geological and architectural domains:
     - `SafeTerminal`: Spawn Cavern, Extraction Landing Bay.
     - `NaturalCrystalline`: Crystalline Geode, Prismatic Crystal Cathedral, Bioluminescent Firmament Abyss.
     - `BioOrganic`: Fungoid Bio-Grotto, Subterranean Aquifer Oasis, Bioluminescent Glowworm Grotto.
     - `PrecursorIndustrial`: Mining Pillar Hall, Industrial Vault Bunker, Laser Defense Foundry, Precursor Coolant Reservoir, Cyclopean Excavation Silo.
     - `VolcanicThermal`: Terraced Quarry, Magma Caldera Lake, Molten Magma Foundry.
     - `TectonicChasm`: Fault-Line Crevasse, Abyssal Vertical Chasm, Spike Trench Arena, Crumbling Arch Canyon, Colossal Abyssal Chasm, Tectonic Abyssal Sinkhole.
     - `CosmicVoidAbyss`: Radioactive Core Sanctuary, Void Singularity Rift, Ancient Titan Necropolis, Colossal Vaulted Dredge Cathedral.
3. **Contextual Adjacency Affinity Scoring (`get_room_adjacency_affinity`)**:
   - Evaluates smooth geological and atmospheric transitions between adjacent chambers with scores from $0.05$ (highly dissonant) to $0.95$ (optimal thematic harmony):
     - *Harmonious pairings*: `BioOrganic` $\leftrightarrow$ `NaturalCrystalline` ($0.90$), `PrecursorIndustrial` $\leftrightarrow$ `VolcanicThermal` ($0.88$), `TectonicChasm` $\leftrightarrow$ `CosmicVoidAbyss` ($0.85$).
     - *Dissonant pairings*: `BioOrganic` $\leftrightarrow$ `VolcanicThermal` ($0.15$), `SafeTerminal` $\leftrightarrow$ `CosmicVoidAbyss` ($0.10$).
4. **Placement Limits & Environmental Safety Rules**:
   - **Spawn Immunity from Extreme Hazards**: Chambers adjacent to spawn $(0, 0)$ can never be extreme hazard environments (`MagmaCalderaLake`, `MoltenMagmaFoundry`, `VoidSingularityRift`, `SpikeTrenchArena`, `RadioactiveCoreSanctuary`), guaranteeing safe initial gear preparation.
   - **Harsh Hazard Anti-Clustering**: A harsh hazard chamber can never be adjacent to more than 1 other harsh hazard chamber, preventing overwhelming death gauntlets.
   - **Same-Category Diversity Limit**: Chambers cannot be adjacent to more than 2 chambers of the same `RoomCategory`, forcing high aesthetic diversity along every corridor.
   - **Global Archetype Caps**: Small and standard room types are capped at $\le 2$ instances per level, while massive and colossal landmarks are capped at $\le 1$ instance per level.
5. **Wavefront Solver Architecture**:
   - Generates chambers outward in order of Manhattan distance from spawn:
     - Step 0: Anchor `SpawnCavern` at $(0, 0)$ and `ExtractionLandingBay` at $(W-1, D-1)$.
     - Step 1..N: For each unplaced cell, filter candidates against all hard constraints, calculate combined heuristic scores (`base_affinity * 1.5 + seed_variation - repetition_penalties`), and select the top candidate.
     - Fallback safety: If an over-constrained grid cell has no candidate passing all soft criteria, constraints relax progressively while maintaining the 100% hard prohibition on identical adjacent room types.

---

## 12. Structural Integrity & Floating Block Prevention

To eliminate unnatural floating geometry, suspended specks, and disconnected clutter across procedurally generated subterranean levels, the engine executes automated post-generation **Structural Foundation & Connectivity Solvers** ([src/voxel/structural_check.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.hpp), [src/voxel/structural_check.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp)):

### 11.1 Structural Connectivity Invariants
1. **Bedrock Anchor Foundations**:
   - Blocks located at the level boundaries ($y \le 3$, $y \ge \text{chunk\_ceiling}$, $x \le 3$, $x \ge W - 4$, $z \le 3$, $z \ge D - 4$), blocks with `VOXEL_FLAG_ANCHORED`, and `MAT_DREDGE_BEDROCK` serve as immutable structural anchors.
   - A multi-source 6-connected BFS floods from all anchor points across adjacent solid voxels. Any solid block unreached by this traversal is categorized as an unanchored floating component.
2. **Stepped Ramp 6-Connectivity Invariant**:
   - Diagonal stepped corridor ramps (`CorridorType::SlopingRamp`) gain elevation diagonally across $(x, y)$ or $(z, y)$. Standard single-block steps only touch corner-to-corner, failing 6-face connectivity.
   - Corridors enforce a 2-block thick stair slab (`y == floor_y || (c.type == CorridorType::SlopingRamp && y == floor_y - 1)`). Adjacent steps share a full vertical voxel face, ensuring 100% continuous 6-connectivity from lower cavern floors to elevated upper tiers without filling deep chasms down to bedrock.
3. **Deep Chasm Abyss Preservation (`is_in_chasm_abyss`)**:
   - Corridors entering high-elevation chambers adjacent to deep tectonic rifts (`FaultLineCrevasse`, `CrumblingArchCanyon`, `SpikeTrenchArena`, `AbyssalVerticalChasm`, `ColossalAbyssalChasm`, `TectonicAbyssalSinkhole`) suppress corridor floor solid fills inside the chasm's interior drop below `room.floor_y`, ensuring vertical abyss plunges remain open and unobstructed while maintaining full doorway walkability.
4. **Stray Speck & Noise Purging**:
   - Disconnected islands with $\le 2$ blocks in open airspace are recognized as procedural noise specks and purged to `MAT_AIR`, ensuring chambers look pristine and natural. Specks within 2 voxels of the ceiling are grounded upward as hanging stalactites.
5. **Architectural Pylon & Ledge Grounding**:
   - Larger ungrounded architectural features (bridges, ledges, platforms) are automatically connected to the nearest solid floor below via material-matched structural columns (`MAT_INDUSTRIAL_BULKHEAD` for man-made facilities, `MAT_VOLCANIC_BASALT` or `MAT_FRACTURED_GRANITE` for natural caverns, `MAT_PRECURSOR_STONE` for ancient ruins). If suspended over a bottomless void, they are suspended from solid cavern ceilings above.
6. **Clutter Prop Surface Pruning**:
   - Decoration clutter meshes (`World::cleanup_all_disconnected_clutter`) verify their underlying support voxel; any prop whose host block is air or destroyed is automatically pruned.

---

## 13. Dark Zones & Blackout Caverns (Torch & Flare Illumination Mechanics)

In deeper sectors and high-difficulty subterranean pockets, geological power grids fail, crystal luminescence is absent, and caverns plunge into **Pitch-Black Dark Zones** where player illumination tools (Exosuit Flashlight `[F]` and Throwable Chemical Flares `[T]`) are essential for survival:

### 13.1 Progressive Sector Darkness Scaling
The prevalence of blackout chambers scales dynamically with sector tier:
- **Sector 1 (Perimeter Drift)**: Gentle introductory curve. ~15% chance for an isolated unpowered deep pocket; the majority of caverns feature radiant luminaries, crystal beds, and functional bulkhead lighting.
- **Sector 2 (Volatile Fault)**: Moderate challenge. ~35%–45% of chambers are pitch-black dark zones. Bulkhead lights along corridors fail, and natural crystal luminescence fades.
- **Sector 3+ (Void Cradle & Deep Sectors)**: Severe blackout caverns. ~65%–75% of all chambers are completely unlit, creating terrifying voids where flashlights and throwable chemical flares are mandatory.
- **Immutable Safe Zones**: The **Spawn Staging Cavern** $(0, 0)$ and **Extraction Landing Bay** $(W-1, D-1)$ are unconditionally exempt from darkness scaling ($0.0$ darkness factor, 100% lit) across all seeds and sectors to guarantee safe staging and evacuation defense.

### 13.2 Environmental & Shader Darkness Architecture
1. **Complete Luminary Omission**:
   - Dark zone chambers omit all decorative point lights (`CavernLuminary`), crystal geodes, glowing waterfalls, and ceiling starlight arrays.
   - Corridors passing through dark zones omit electrical bulkhead clearance lights.
   - Only high-hazard molten magma/lava pools retain thermal glow.
2. **Zero Ambient Floor (`uDarknessFactor`)**:
   - Standard subterranean voxels maintain a baseline ambient bounce. When delvers enter a dark zone, the renderer interpolates `uDarknessFactor` from $0.0 \to 1.0$, completely extinguishing subterranean ambient floor light in `voxel_pbr.frag`, `clutter.frag`, and `stalker.frag`.
   - Distant volumetric fog and depth haze (`postprocess.frag`) fade from subtle blue/violet mist into an inky black abyss.

### 13.3 Gameplay & Tactical Impact
- **Predator Camouflage & Flare Revelation**:
  - Hostile predators like the **Void Stalker** blend seamlessly into the shadows in dark zones.
  - In addition to the exosuit headlamp cone, the AI perception system actively tracks all deployed **Chemical Flares** (`FlareManager::flares()`). Throwing a flare (`[T]`) illuminates monsters within its 22m radius, instantly exposing stalkers and triggering light evasion or combat reactions.
- **HUD Telemetry & Caution Advisories**:
  - Entering a pitch-black zone (`darkness >= 0.75`) without headlamp illumination or active flares triggers an automated suit warning toast:
    `[DARK ZONE: ZERO LUMENS - DEPLOY FLARES [T] / HEADLAMP [F]]`.


