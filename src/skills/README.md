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
- **Occluded Mineral Reveal**: High-value materials (`MAT_VOIDITE_CRYSTAL`, `MAT_RADIOACTIVE_ORE`, `MAT_REINFORCED_VAULT_DOOR`) within the scan radius have their bounding boxes captured into `m_surveyed`.
- **Holographic Rendering**: The renderer passes surveyed voxel coordinates to [`Renderer::render_sonar_wireframes()`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp#L440), drawing glowing wireframe outlines visible through solid rock walls.
