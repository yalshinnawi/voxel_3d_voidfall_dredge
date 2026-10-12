# Voxel Material Specifications

This document catalogs all voxel types, hardness ratings, drop rates, and physical attributes in **Voidfall: Dredge** based on [`src/voxel/packed_vertex.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/packed_vertex.hpp).

---

## 🪨 Complete Material Index Table

| ID | Material Name | Hardness | Sound Type | Primary Drop | Secondary Drop / Properties |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **0** | `MAT_AIR` | 0 Hits | None | None | Gas / Empty Space |
| **1** | `MAT_FRACTURED_GRANITE` | 2 Hits | Stone Impact | Common Stone | Trace Voidite (5%), generates tumbling debris |
| **2** | `MAT_VOLCANIC_BASALT` | 4 Hits | Dense Clink | Basalt Slab | Raw Voidite (15%), heavy debris |
| **3** | `MAT_VOIDITE_CRYSTAL` | 3 Hits | Glass Shatter | High-Yield Voidite | Emissive violet glow, volatile crystal fragments, primary objective quota |
| **4** | `MAT_INDUSTRIAL_BULKHEAD` | 6 Hits | Heavy Metallic | Bulkhead Alloy | Player-placed support (`MAT_TITANIUM`), absorbs cave-in debris |
| **5** | `MAT_REINFORCED_VAULT_DOOR` | Indestructible | Solid Resonant | Vault Core Relic | Indestructible structural anchor node |
| **6** | `MAT_THERMITE_SLAG` | 1 Hit | Sizzle/Crumble | Scrap Slag | Molten residue from shaped demolition charges (`MAT_LAVA`) |
| **7** | `MAT_RADIOACTIVE_ORE` | 5 Hits | Toxic Hiss | Radioactive Isotope | Emissive toxic green glow, increases local radiation |
| **8** | `MAT_DREDGE_BEDROCK` | Indestructible | Deep Thud | None | Planetary foundation, primary BFS structural anchor |
| **9** | `MAT_GAS` | 0 Hits | Hiss | None | Toxic gas pocket, causes suit degradation |
| **10**| `MAT_VOLATILE_SMOKE` | 0 Hits | None | None | Visual obscurant from detonations |
| **11**| `MAT_CRYSTAL_AQUIFER` | 0 Hits | Splash | None | Pure subterranean liquid aquifer water |
| **12**| `MAT_BIOLUMINESCENT_FLORA` | 1 Hit | Soft Crunch | Bio Flora Sample | Radiant green/teal subterranean moss and fungi |
| **13**| `MAT_PRISMATIC_CRYSTAL` | 4 Hits | Crystal Resonant | Rare Salvage Gem | Emissive cyan/diamond refraction, decorative cavern crystal |
| **14**| `MAT_OBSIDIAN_SPIKES` | 3 Hits | Sharp Clink | Obsidian Shard | Lethal puncturing ground spikes |
| **15**| `MAT_ACID` | 0 Hits | Sizzle | None | Corrosive liquid acid hazard |
| **16**| `MAT_COOLANT` | 0 Hits | Slosh | None | Cryogenic coolant liquid |
| **17**| `MAT_ASTRAL_ORE` | 6 Hits | Cosmic Shimmer | Astral Ore Core | Deep-space mythic mineral (unlocked in Sector 3+); radiant golden starlight glow, required for late-level extraction quotas and exotic tech |

---

## 💎 Scalable Material Scarcity & Progression Architecture

Materials are governed by a dynamic tier-based scarcity system ([`src/voxel/material_scarcity.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/material_scarcity.hpp)):

1. **Grade-Based Scarcity Rate ($S \in [0.0, 1.0]$)**:
   - **Grade 1 (Common)**: Titanium Alloy ($S = 0.30 \to 0.02$), Flora ($S = 0.45 \to 0.05$).
   - **Grade 2 (Uncommon)**: Voidite Crystal ($S = 0.60 \to 0.10$).
   - **Grade 3 (Rare)**: Radioactive Ore ($S = 0.75 \to 0.20$, unlocks Sector 2).
   - **Grade 4 (Exotic)**: Prismatic Crystal ($S = 0.85 \to 0.35$, unlocks Sector 2).
   - **Grade 5 (Mythic)**: Astral Ore ($S = 0.92 \to 0.50$, unlocks Sector 3).

2. **Progression Dynamics**:
   - In earlier sectors, high-grade materials are completely locked ($S = 1.0$, $W = 0.0$).
   - As players advance to deeper sectors, the scarcity of previously unlocked minerals decays smoothly ($S_{\text{next}} = \max(S_{\text{min}}, S - \Delta)$), making earlier materials increasingly abundant and easier to harvest.
   - Newly introduced higher-grade materials (such as Astral Ore in Sector 3+) maintain high scarcity, remaining rare and precious.

3. **Pure Single-Ore Vein Invariant**:
   - Every procedural ore vein in the host rock matrix hashes strictly on its 3D spatial cell cluster `(cell_x, cell_y, cell_z)`.
   - In any given vein, **100% of the generated ore blocks are identical** (no mixed-ore clusters). A vein is exclusively Titanium, Voidite, Radioactive, Prismatic, or Astral Ore.

---

## 🏗️ Structural Anchoring & Integrity Rules

- **Anchor Nodes**: Blocks marked `MAT_DREDGE_BEDROCK`, `MAT_REINFORCED_VAULT_DOOR`, or possessing the `VOXEL_FLAG_ANCHORED` flag serve as immovable root anchors in the BFS connectivity solver ([`src/voxel/structural_check.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp)).
- **Overhang Stability**: Excavating Granite (`MAT_FRACTURED_GRANITE`) beneath heavy Basalt layers (`MAT_VOLCANIC_BASALT`) risks severing the BFS path to bedrock, triggering a dynamic cave-in.
- **Bulkhead Fortification**: Delvers can place `MAT_INDUSTRIAL_BULKHEAD` (Right Click) to create synthetic support columns. Ceilings supported by adjacent bulkheads are immune to seismic tremor detachment. Textures feature matte cold-rolled gunmetal alloy plating with alternating non-slip diamond tread bars, flush structural anchor bolts, and controlled satin dispersion (roughness $\ge 0.68$, eliminating plastic specular hotspots).
