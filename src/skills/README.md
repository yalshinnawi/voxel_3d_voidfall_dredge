# Skills & Surveying Subsystem (`src/skills/`)

The `skills` subsystem manages player proficiency trees, specialized perks, and the seismic sonar surveying scanner.

---

## 📁 Source Files

| File | Primary Responsibility | Key Classes / Structs |
| :--- | :--- | :--- |
| [`skill_matrix.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/skills/skill_matrix.hpp) | Defines skill branches (Demolitions, Surveying, Exo-Suit), XP progression, and perk unlocks. | [`SkillMatrix`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/skills/skill_matrix.hpp#L31), [`SkillBranch`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/skills/skill_matrix.hpp#L8) |
| [`surveying.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/skills/surveying.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/skills/surveying.cpp) | Manages seismic sonar pulse scans, ping queries, wireframe highlights of occluded minerals. | [`SurveyingSystem`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/skills/surveying.hpp#L13), [`SurveyedVoxel`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/skills/surveying.hpp#L8) |

---

## 🌐 Skill Branches & Unlocks

1. **Demolitions Branch**:
   - Focus: Drilling and subterranean blasting efficiency.
   - Unlock Perk: **Micro-Charges** (Surgical $1 \times 1 \times 3$ directional blast without destroying fragile mineral veins).
2. **Surveying Branch**:
   - Focus: Seismic sonar cavern mapping and mineral identification.
   - Unlock Perk: **Extended Frequency** (Expands sonar pulse radius from baseline to $30\text{m}$).
3. **Exo-Suit Calibration Branch**:
   - Focus: Hazard endurance, environmental mitigation, and traversal.
   - Unlock Perk: **Kinetic Dynamo** (Sprinting and falling regenerates thruster fuel $25\%$ faster).

---

## 📡 Seismic Sonar Pulse Mechanics

- **Activation (`Key Q`)**: Emits a spherical acoustic wave from the player's position.
- **Objectives & Monsters Only**: Strictly detects primary mission objectives (`MAT_VOIDITE_CRYSTAL`, `MAT_VOIDITE`), subterranean vault doors (`MAT_REINFORCED_VAULT_DOOR`, `MAT_VAULT_DOOR`), and hostile monsters (`VoidStalker`, `SeismicBurrower`). All other non-objective materials (granite, basalt, titanium, radioactive ores, astral minerals, prismatic crystals) are excluded.
- **Vein Clustering**: 26-connectivity flood-fill merges touching target voxels into unified objective clusters (`SurveyedCluster`), eliminating noisy individual block cubes.
- **Smooth Holographic Hulls**: The renderer passes surveyed clusters to [`Renderer::render_sonar_clusters()`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp), generating a smooth Laplacian-smoothed glowing outline and translucent volumetric hull around objective deposits visible through solid rock walls.
- **Hostile Monster Wireframes**: Passes detected monsters to [`Renderer::render_sonar_monsters()`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp), projecting luminous glowing 3D wireframe cages and acoustic waist rings visible through rock strata.
