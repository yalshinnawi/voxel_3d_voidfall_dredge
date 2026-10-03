# Level Design Analysis & Improvement Plan

**Date**: 2026-10-03  
**Scope**: Full audit of [level_shapes.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.hpp) and [level_shapes.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.cpp) (2,019 lines), visual outputs, and test suite data.

---

## 1. Executive Assessment

The level generation system is **structurally sound** — 100% BFS reachability, zero clipping across 30 random permutations, 989 corridor seams verified, and all 10 collision modules passing. The foundation (15 room archetypes, multi-floor layouts, diagonal corridors, stepped ramps, organic jitter) is solid.

However, several areas limit **perceived randomness**, **visual freshness across replays**, and **tactical depth** when scrutinised at the experience level:

| Dimension | Current Grade | Notes |
|---|---|---|
| Structural Integrity | **A+** | 100% traversable, 0 clipping, bedrock/mantle invariant enforced |
| Room Archetype Diversity | **B+** | 15 distinct shapes, but internal furniture is mostly static per type |
| Layout Randomness | **B-** | Room *selection* shuffles, but grid topology is always rectangular lattice |
| Corridor Variety | **B** | 4 corridor types exist, but visually corridors feel samey within a sector |
| Replay Distinctiveness | **C+** | Same seed = same level, different seeds feel similar after 3-4 runs |
| Vertical Surprise | **B** | Multi-floor exists but floor assignment is a simple `(gx+gz+seed)%2` pattern |
| Mineral/Loot Distribution | **C** | Minerals are architecturally baked into room shape, not randomly scattered |
| Environmental Storytelling | **C+** | Rooms read as functional arenas, not as places with history/narrative |

---

## 2. Detailed Findings

### 2.1 Layout Topology: Regular Grid Limits Surprise

**Observation**: Every sector is a strict W x D rectangular grid. Rooms are always at `center = 16 + i * 20` with +/-2 jitter. The player quickly internalises the "go right, go down" navigation pattern.

**Evidence** ([level_shapes.cpp L182-186](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.cpp#L182-L186)):
```cpp
for (int i = 0; i < m_grid_w; ++i) {
    grid_coords.push_back(16 + i * 20);
}
```

**Impact**: After 2-3 expeditions, players develop a mental map template. The diagonal corridors help but still connect adjacent grid cells — no long-range shortcuts or dead-end surprise wings exist.

### 2.2 Room Selection: Shuffled Pool, but Constrained Distribution

**Observation**: Room types are drawn from a pre-built pool (7 entries for Sector 1, 14 for Sector 2, 23 for Sector 3) that's shuffled then consumed sequentially. This means:
- Sector 1 (9 rooms - 2 fixed = 7 variable slots, pool size = 7) -> **every Sector 1 run has the same room type set**, just reordered.
- Sector 2 (14 variable slots, pool size = 14) -> same issue.
- Duplicates are hardcoded into the pool (e.g., `MiningPillarHall` appears twice in Sector 1 pool).

**Evidence** ([level_shapes.cpp L189-200](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/level_shapes.cpp#L189-L200)):
```cpp
available_pool = {
    RoomShapeType::MiningPillarHall,
    RoomShapeType::TerracedQuarry,
    RoomShapeType::CrystallineGeode,
    RoomShapeType::FungoidBioGrotto,
    RoomShapeType::MiningPillarHall,   // duplicate
    RoomShapeType::TerracedQuarry,     // duplicate
    RoomShapeType::CrystallineGeode    // duplicate
};
```

**Impact**: Players will encounter identical room-type distributions every run in Sector 1. Only spatial placement varies.

### 2.3 Floor Assignment: Deterministic Checkerboard

**Observation**: Non-massive, non-fixed rooms get floor assignment via:
```cpp
int floor_assign = (gx + gz + (m_seed & 1)) % 2;
```
This creates a **strict checkerboard** pattern (Floor 0, Floor 1, Floor 0, ...). The seed only flips the entire checkerboard by 1 bit. In Sector 1 (3x3), this means exactly 4 rooms on Floor 0 and 3 on Floor 1 (or vice versa) — every single time.

**Impact**: Verticality feels systematic rather than organic. Players never encounter two adjacent rooms on the same elevated floor connected by a flat corridor, or a cluster of lower caverns creating a "lowlands" sub-region.

### 2.4 Room Internals: Static Furniture per Archetype

**Observation**: Each room archetype's internal geometry is **deterministic** — the Magma Caldera always has stepping stones at `(abs(dx) % 3 == 0 && abs(dz) % 3 == 0)`, the Fungoid Grotto always has mushroom stems at `(+/-3, +/-3)`, the Abyssal Chasm always has ledges at y=7/11/15/19 at fixed angular ranges.

**Evidence**: All `sample_*` functions use hardcoded offsets, not seed-driven placement.

**Impact**: Once a player has seen each archetype once, there is zero internal surprise on revisits. A "Crystalline Geode" looks identical every time, regardless of seed.

### 2.5 Corridor Interiors: Functional but Visually Monotone

**Observation**: Corridors are carved as simple rectangular tubes with optional bulkhead arches every 6 voxels, optional vault doors at midpoint, and optional rubble. Ravines add width jitter via `pseudo_rand`, but the wall profiles are smooth tubes.

**Impact**: Long corridors (especially diagonal ones spanning 20+ voxels) feel like identical tunnels. There's no sense of navigating through distinct passage segments.

### 2.6 Mineral Distribution: Architecturally Fixed

**Observation**: Voidite crystals, titanium, and radioactive ore are placed as part of room architecture (pillar veins, altar monoliths, training niches) rather than scattered via procedural distribution. There's no loose mineral seam generation in host rock walls.

**Impact**: Mining feels like "find the designated resource node in each room" rather than "explore the cavern walls for exposed veins."

### 2.7 Massive Room Placement: Deterministic Positions

**Observation**: Massive rooms are always at specific grid coordinates:
- Sector 1: `(1,1)` only
- Sector 2: `(1,1), (2,2), (1,2)`
- Sector 3: `(1,1), (2,2), (3,3), (1,3), (3,1)`

**Impact**: Experienced players always know exactly where the grand chambers are.

---

## 3. Prioritised Improvement Plan

### Tier 1: High Impact, Moderate Effort

#### 3.1 Randomised Room Internal Variation (Priority: Critical)
**Goal**: Same archetype, different experience each seed.

- **Approach**: Pass the room's `(grid_x, grid_z, seed)` through `hash_coord` to derive per-room variation parameters:
  - **Pillar Hall**: Randomise pillar count (3-5), pillar positions (offset from center by hash), bridge height.
  - **Magma Caldera**: Vary stepping stone grid offset, rotate basalt arch angle, shift geyser chimney.
  - **Fungoid Grotto**: Randomise mushroom cap count (2-5), cap positions, heights (floor+3 to floor+11).
  - **Spike Trench**: Vary spike density, bridge axis (X vs Z vs diagonal), gap width.
  - **Crystalline Geode**: Rotate crystal spire position, vary stepping stone ring height.
- **Constraint**: Maintain traversability invariant — all variations must still pass Module 6 and Module 7.
- **Estimated LOE**: 3-4 hours per room archetype (15 archetypes x 3h = 45h total, can be phased).

#### 3.2 Weighted Probabilistic Room Selection (Priority: High)
**Goal**: Different room-type distributions each run.

- **Approach**: Replace the fixed pool+shuffle with weighted random draws:
  - For each grid slot: roll weighted by `{archetype: weight}`, pick archetype, reduce its weight by decay factor.
- Add **exclusion rules**: no more than 2 of the same archetype per sector, at least 1 hazard room, at least 1 parkour room.
- Allow Sector 1 to sometimes include 1 "teaser" Sector 2+ archetype (e.g., a small Fault Crevasse) at reduced scale.
- **Estimated LOE**: 2-3 hours.

#### 3.3 Procedural Mineral Seam Generation (Priority: High)
**Goal**: Discoverable mineral veins in host rock walls.

- **Approach**: In `sample_voxel`, after determining a voxel is host rock and it's adjacent to a room or corridor air boundary, apply a seeded noise function to potentially replace it with `MAT_VOIDITE_CRYSTAL`, `MAT_TITANIUM`, or `MAT_RADIOACTIVE_ORE`.
  - Seam probability: 3-8% depending on sector depth.
  - Seam clustering: Use 3D hash coherence so veins form 3-6 voxel streaks rather than isolated dots.
  - Sector-specific distribution: Sector 1 favors titanium, Sector 2 favors voidite, Sector 3 adds radioactive veins.
- **Estimated LOE**: 3-4 hours.

---

### Tier 2: Medium Impact, Medium Effort

#### 3.4 Broken Grid Topology: Optional Room Removal & Dead Ends (Priority: Medium-High)
**Goal**: Break the predictable rectangular lattice.

- **Approach**:
  - In Sector 2+, randomly remove 1-2 interior grid cells (replace with solid rock), creating L-shaped or T-shaped sector footprints.
  - Ensure BFS reachability still passes from Spawn to Extraction after removal.
  - Removed cells create mystery: "what's behind this wall?" encourages drill exploration.
  - Add 1 optional "secret alcove" corridor that dead-ends into a small treasure niche (2-3 voxels wide, no through-passage).
- **Estimated LOE**: 4-5 hours (must re-verify BFS invariant with removal).

#### 3.5 Floor Assignment Clustering (Priority: Medium)
**Goal**: Create organic floor "biomes" instead of strict checkerboard.

- **Approach**: Replace `(gx+gz+seed)%2` with a flood-fill clustering algorithm:
  1. Randomly pick 2-3 "seed" grid cells, assign them Floor 0 or Floor 1.
  2. Flood-fill outward: neighbors adopt the same floor with 70% probability, flip with 30%.
  3. Massive/multi-floor rooms override their assignment.
  4. Result: "lowlands" clusters of 3-4 adjacent Floor 0 rooms, "highlands" clusters of Floor 1 rooms, with transition ramps between.
- **Estimated LOE**: 3 hours.

#### 3.6 Corridor Detail Segments (Priority: Medium)
**Goal**: Break up long monotone corridors into distinct passage segments.

- **Approach**: Subdivide corridors longer than 8 voxels into 2-3 segments with different cross-section profiles:
  - **Narrow squeeze** (half_width = 1): claustrophobic bottleneck.
  - **Wide cavern pocket** (half_width = 5): corridor widens into a small pocket with mineral veins.
  - **Collapsed section**: rubble on floor, cracked ceiling, reduced headroom.
  - **Flooded section**: 1-block deep water/sludge on corridor floor.
- Segment boundaries marked by material transitions (granite -> basalt -> bulkhead).
- **Estimated LOE**: 4-5 hours.

#### 3.7 Randomised Massive Room Placement (Priority: Medium)
**Goal**: Grand chambers appear in unpredictable locations.

- **Approach**: Instead of fixed `(1,1), (2,2)`, etc., randomly select N interior grid cells (where N scales with sector size) that are not Spawn or Extraction. Apply adjacency rules: massive rooms should not share a wall with another massive room.
- **Estimated LOE**: 1-2 hours.

---

### Tier 3: Polish & Long-Term

#### 3.8 Environmental Storytelling Props (Priority: Low-Medium)
**Goal**: Rooms feel like they have history.

- **Approach**: Introduce 8-12 small "decoration voxel clusters" (2x2x2 to 4x4x4) that can be placed within room airspace:
  - Collapsed drill rig (titanium + scrap).
  - Abandoned supply crate cluster.
  - Previous expedition corpse marker (single emissive voxel).
  - Collapsed ceiling rubble pile.
  - Dried-up water pool basin.
- Each room rolls 0-2 decorations from the pool, placed at hash-determined offsets from center.
- **Estimated LOE**: 6-8 hours (new prop carving functions).

#### 3.9 Non-Rectangular Grid Topologies (Priority: Low)
**Goal**: Fundamentally different sector shapes.

- **Approach**: For Sectors 5+, support hexagonal grids, diamond grids, or cross-shaped footprints. Requires reworking the `m_grid_room_indices` structure.
- **Estimated LOE**: 15-20 hours (significant refactor).

#### 3.10 Room Rotation & Mirroring (Priority: Low)
**Goal**: Same archetype, 4x visual variation.

- **Approach**: Each room gets a random rotation (0, 90, 180, 270 degrees) and optional X/Z mirror. Transform `(dx, dz)` in each `sample_*` function before applying shape logic.
- **Estimated LOE**: 4-6 hours (must update doorway integration helpers).

#### 3.11 Dynamic Corridor Pathing (Curved / S-Bend Corridors) (Priority: Low)
**Goal**: Corridors that curve instead of going straight.

- **Approach**: Add a `CorridorType::Curved` that uses 2-3 control points with Catmull-Rom interpolation instead of linear `start_pos -> end_pos`. The `sample_voxel` distance check already uses parametric `t`, so this is a natural extension.
- **Estimated LOE**: 5-6 hours.

---

## 4. Implementation Priority Matrix

| ID | Improvement | Impact | Effort | Priority |
|----|------------|--------|--------|------|
| 3.1 | Room Internal Variation | 5/5 | 3/5 | **P0** |
| 3.2 | Weighted Room Selection | 4/5 | 2/5 | **P0** |
| 3.3 | Procedural Mineral Seams | 4/5 | 2/5 | **P0** |
| 3.4 | Broken Grid / Dead Ends | 4/5 | 3/5 | **P1** |
| 3.5 | Floor Clustering | 3/5 | 2/5 | **P1** |
| 3.6 | Corridor Detail Segments | 3/5 | 3/5 | **P1** |
| 3.7 | Random Massive Placement | 3/5 | 1/5 | **P1** |
| 3.8 | Environmental Props | 3/5 | 4/5 | **P2** |
| 3.10 | Room Rotation/Mirror | 3/5 | 3/5 | **P2** |
| 3.11 | Curved Corridors | 2/5 | 3/5 | **P2** |
| 3.9 | Non-Rectangular Grids | 3/5 | 5/5 | **P3** |

---

## 5. Recommended Sprint Sequence

### Sprint 1: "No Two Runs Feel the Same" [COMPLETED]
1. [x] **3.2**: Weighted probabilistic room selection with exclusion rules & decay limits
2. [x] **3.7**: Randomised massive room placement in interior grid cells
3. [x] **3.5**: Floor assignment clustering via dual-anchor Voronoi flooding
4. [x] Re-run `python scripts/tdd.py` — verified all 7 suites pass in parallel (1.45s)

### Sprint 2: "Every Room is a Discovery" [COMPLETED]
1. [x] **3.1**: Randomised room internals across core archetypes (`sample_pillar_hall`, `sample_crystalline_geode`, `sample_terraced_quarry`, `sample_magma_caldera`, `sample_spike_trench`, `sample_fungoid_grotto`)
2. [x] **3.3**: Procedural 3D mineral seam generation in host rock matrix (Titanium, Voidite Crystal, Radioactive Ore)
3. [x] Re-run `test_level_collision.exe` — verified all 13 modules (988 corridor seams, 100% 3D BFS reachable, 0 clipping)

### Sprint 3: "The Journey Matters & Polish" [IN PROGRESS]
1. [ ] **3.6**: Corridor detail segments (squeeze, pocket, collapse, flooded)
2. [ ] **3.4**: Broken grid topology with dead ends and treasure niches
3. [ ] **3.10**: Room rotation and mirroring
4. [x] Full visual playthrough: `./build/Release/VoidfallDredge.exe --auto-play-test` verified (10/10 screens PASS, 0 errors in `voidfall.log`)
5. [x] Documentation synchronized in `wiki/mechanics/level_design_and_generation.md` (Section 10 added)

---

## 6. Test Verification Strategy

Every improvement **must** maintain:
- 100% 3D BFS reachability from Spawn (0,0) to Extraction (W-1, D-1)
- Zero clipping across 30+ random permutation seeds (Module 7)
- All 15 room archetypes pass isolated stress tests (Module 6)
- Full room-to-room sightline traversal tour (Module 8)
- Bedrock base (y <= 3) and ceiling mantle (y >= 26) invariants
- Zero dynamic allocations in per-frame update/render loops

For each sprint, the verification protocol is:
```bash
# Build and test all suites
python scripts/tdd.py

# Visual playthrough capture
./build/Release/VoidfallDredge.exe --auto-play-test
python scripts/analyze_screenshots.py

# Multi-seed stress (new test): generate 50 seeds, verify BFS + collision
# (to be implemented as Module 11 in test_level_collision.cpp)
```
