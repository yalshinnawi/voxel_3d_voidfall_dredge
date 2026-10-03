# Voxel Material Specifications

This document catalogs all voxel types, hardness ratings, drop rates, and physical attributes in **Voidfall: Dredge** based on [`src/voxel/packed_vertex.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/packed_vertex.hpp).

---

## 🪨 Complete Material Index Table

| ID | Material Name | Hardness | Sound Type | Primary Drop | Secondary Drop / Properties |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **0** | `MAT_AIR` | 0 Hits | None | None | Gas / Empty Space |
| **1** | `MAT_FRACTURED_GRANITE` | 2 Hits | Stone Impact | Common Stone | Trace Voidite (5%), generates tumbling debris |
| **2** | `MAT_VOLCANIC_BASALT` | 4 Hits | Dense Clink | Basalt Slab | Raw Voidite (15%), heavy debris |
| **3** | `MAT_VOIDITE_CRYSTAL` | 3 Hits | Glass Shatter | High-Yield Voidite | Emissive violet glow, volatile crystal fragments |
| **4** | `MAT_INDUSTRIAL_BULKHEAD` | 6 Hits | Heavy Metallic | Bulkhead Alloy | Player-placed support, absorbs cave-in debris |
| **5** | `MAT_REINFORCED_VAULT_DOOR` | Indestructible | Solid Resonant | Vault Core Relic | Indestructible structural anchor node |
| **6** | `MAT_THERMITE_SLAG` | 1 Hit | Sizzle/Crumble | Scrap Slag | Molten residue from shaped demolition charges |
| **7** | `MAT_RADIOACTIVE_ORE` | 5 Hits | Toxic Hiss | Radioactive Isotope | Emissive green glow, increases local radiation |
| **8** | `MAT_DREDGE_BEDROCK` | Indestructible | Deep Thud | None | Planetary foundation, primary BFS structural anchor |
| **9** | `MAT_GAS` | 0 Hits | Hiss | None | Toxic gas pocket, causes suit degradation |
| **10**| `MAT_VOLATILE_SMOKE` | 0 Hits | None | None | Visual obscurant from detonations |

---

## 🏗️ Structural Anchoring & Integrity Rules

- **Anchor Nodes**: Blocks marked `MAT_DREDGE_BEDROCK`, `MAT_REINFORCED_VAULT_DOOR`, or possessing the `VOXEL_FLAG_ANCHORED` flag serve as immovable root anchors in the BFS connectivity solver ([`src/voxel/structural_check.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp)).
- **Overhang Stability**: Excavating Granite (`MAT_FRACTURED_GRANITE`) beneath heavy Basalt layers (`MAT_VOLCANIC_BASALT`) risks severing the BFS path to bedrock, triggering a dynamic cave-in.
- **Bulkhead Fortification**: Delvers can place `MAT_INDUSTRIAL_BULKHEAD` (Right Click) to create synthetic support columns. Ceilings supported by adjacent bulkheads are immune to seismic tremor detachment.
