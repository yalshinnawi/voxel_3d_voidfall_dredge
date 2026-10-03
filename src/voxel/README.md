# Voxel & World Subsystem (`src/voxel/`)

The `voxel` subsystem implements high-density 3D voxel chunk storage, an optimized greedy mesher with baked vertex ambient occlusion, procedural cavern generation, and server-authoritative structural integrity BFS solver.

---

## 📁 Source Files

| File | Primary Responsibility | Key Classes / Structs |
| :--- | :--- | :--- |
| [`chunk.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/chunk.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/chunk.cpp) | $32 \times 32 \times 32$ chunk memory layout (32,768 voxels), spatial hashing, dirty flags, GPU buffer upload. | [`Chunk`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/chunk.hpp#L39), [`ChunkPos`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/chunk.hpp#L14), [`ChunkPosHash`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/chunk.hpp#L28) |
| [`packed_vertex.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/packed_vertex.hpp) | 16-bit packed voxel struct and 8-byte bit-packed GPU vertex format. | [`Voxel`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/packed_vertex.hpp#L40), [`PackedVoxelVertex`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/packed_vertex.hpp#L72), [`MaterialID`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/packed_vertex.hpp#L8) |
| [`greedy_mesher.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/greedy_mesher.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/greedy_mesher.cpp) | Slice-plane greedy quad merging with 2-bit vertex AO and diagonal anisotropy compensation. | [`GreedyMesher`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/greedy_mesher.hpp#L12) |
| [`structural_check.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp) | Breadth-first search tracing structural bedrock connectivity; slices floating islands into debris entities. | [`StructuralCheck`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.hpp#L20), [`UnanchoredIsland`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.hpp#L13) |
| [`world.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/world.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/world.cpp) | World chunk manager, 3D simplex/Perlin noise cavern generation, fast DDA raycasting, worker meshing pool. | [`World`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/world.hpp#L22), [`RaycastResult`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/world.hpp#L14) |

---

## 🧱 16-Bit Voxel Memory Representation

Every block in the world is packed into exactly 2 bytes (`sizeof(Voxel) == 2`):

```cpp
struct Voxel {
    uint8_t material_id;      // Material enum (0..10)
    uint8_t flags_and_damage; // Bit 0..3: Damage tier (0..15)
                              // Bit 4:    VOXEL_FLAG_ANCHORED (0x10)
                              // Bit 5:    VOXEL_FLAG_EMISSIVE (0x20)
                              // Bit 6:    VOXEL_FLAG_SURVEYED (0x40)
                              // Bit 7:    VOXEL_FLAG_PLAYER_PLACED (0x80)
};
```

### Material Hierarchy

| ID | Name | Category | Hardness | Properties |
| :--- | :--- | :--- | :--- | :--- |
| `0` | `MAT_AIR` | Gas | 0 | Non-solid |
| `1` | `MAT_FRACTURED_GRANITE` | Stone | 2 Hits | Common cavern rock, produces debris |
| `2` | `MAT_VOLCANIC_BASALT` | Stone | 4 Hits | Dense heat-resistant crust |
| `3` | `MAT_VOIDITE_CRYSTAL` | Mineral | 3 Hits | Emissive violet, primary expedition resource |
| `4` | `MAT_INDUSTRIAL_BULKHEAD`| Synthetic | 6 Hits | Player-crafted structural support, prevents cave-ins |
| `5` | `MAT_REINFORCED_VAULT_DOOR` | Synthetic | Indestructible | Indestructible anchor node for BFS structural tree |
| `6` | `MAT_THERMITE_SLAG` | Debris | 1 Hit | Heated molten residue from explosive mining |
| `7` | `MAT_RADIOACTIVE_ORE` | Mineral | 5 Hits | Emissive green, emits hazard radiation |
| `8` | `MAT_DREDGE_BEDROCK` | Crust | Indestructible | Deep subterranean anchor floor |
| `9` | `MAT_GAS` | Hazard | 0 | Dense toxic pocket |
| `10`| `MAT_VOLATILE_SMOKE` | Hazard | 0 | Obscuring smoke screen |

---

## ⚡ 8-Byte Bit-Packed Vertex Layout

To minimize VRAM footprint and bandwidth, greedy mesher vertices are bit-packed into two 32-bit unsigned integers (`uvec2`, 8 bytes total):

- **`data0` (32 bits)**:
  - `[0..4]` $X$ position within chunk (0..31, 5 bits)
  - `[5..9]` $Y$ position within chunk (0..31, 5 bits)
  - `[10..14]` $Z$ position within chunk (0..31, 5 bits)
  - `[15..17]` Normal index: $0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z$ (3 bits)
  - `[18..19]` Baked Ambient Occlusion (0..3, 2 bits)
  - `[20..27]` Texture array slice layer ID (0..255, 8 bits)
  - `[28..31]` PBR auxiliary flags (roughness/metallic modifier) (4 bits)
- **`data1` (32 bits)**:
  - `[0..5]` Greedy quad width $U$ (1..32, 6 bits)
  - `[6..11]` Greedy quad height $V$ (1..32, 6 bits)
  - `[12..13]` Quad corner index (0..3, 2 bits)
  - `[14..17]` Damage crack tier (0..15, 4 bits)
  - `[18..25]` Emissive intensity (0..255, 8 bits)
  - `[26..31]` Reserved

---

## 🏗️ Structural Integrity BFS & Cave-In Mechanics

When a voxel is mined or destroyed:
1. [`StructuralCheck::solve_cavein()`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp#L24) initiates a 6-directional flood fill BFS exploring adjacent solid voxels.
2. The search seeks connectivity to an **anchored root** (`MAT_DREDGE_BEDROCK`, `MAT_REINFORCED_VAULT_DOOR`, or blocks flagged `VOXEL_FLAG_ANCHORED`).
3. If an island of voxels cannot reach an anchor within `max_search_nodes = 1024`, the entire cluster is carved out of the static chunk terrain and converted into an [`UnanchoredIsland`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.hpp#L13).
4. Sliced islands are instantiated as rigid-body [`DynamicDebris`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/dynamic_debris.hpp#L10) entities that tumble under gravity and crush unprotected players.
