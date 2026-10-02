# Voxel Material Specifications

This document catalogs the voxel types, hardness ratings, and drop tables.

---

## Material Index Table

| Layer | Material | Hardness | Sound Type | Primary Drop | Secondary Drop |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **1** | Fractured Granite | 2 Hits | Stone Impact | Common Stone | Trace Voidite |
| **2** | Volcanic Basalt | 4 Hits | Dense Clink | Basalt Slab | Raw Voidite (5%) |
| **3** | Volatile Crystal | 3 Hits | Glass Shatter | Volatile Shards | Explosive Dust |
| **4** | Dense Void Ore | 6 Hits | Metallic Ring | Void Alloy | High-Yield Voidite |

---

## Structural Anchoring Rules
- Materials with higher hardness serve as primary load-bearing anchors in the island BFS connectivity solver ([src/voxel/structural_check.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp)).
- Mining Granite below Basalt layers risks destabilizing the overhang above.
