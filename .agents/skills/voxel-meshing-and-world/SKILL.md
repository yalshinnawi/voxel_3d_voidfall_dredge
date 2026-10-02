---
name: voxel-meshing-and-world
description: >-
  Use this skill when modifying voxel data structures, the greedy mesher, chunk generation,
  structural collapse checks, or world raycasting/mining mechanics.
---

# Voxel Meshing & World Architecture

This skill documents the voxel representation, threading model, and meshing rules for Voidfall Dredge.

## 1. Core Data Structures
- **Chunk Size**: 32x32x32 voxels (`CHUNK_SIZE = 32`, 32,768 voxels per chunk in [src/voxel/chunk.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/chunk.hpp)).
- **Coordinate System**:
  - Local voxel index: `x + y * 32 + z * 1024` via `Chunk::to_index(x, y, z)`.
  - World position: `glm::vec3(m_pos.x * 32, m_pos.y * 32, m_pos.z * 32)`.
  - World Y is vertical height.
- **Vertex Layout**: Packed format via `PackedVoxelVertex` in [src/voxel/packed_vertex.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/packed_vertex.hpp) to minimize VRAM bandwidth and cache misses.

## 2. Multi-Threading & Meshing Lifecycle
1. **Dirty Flags**:
   - Voxel modification triggers `mark_mesh_dirty()` and optionally `mark_structural_dirty()`.
2. **Greedy Meshing**:
   - Background mesher threads run `GreedyMesher` in [src/voxel/greedy_mesher.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/greedy_mesher.cpp).
   - Once completed, worker threads stage vertices via `Chunk::stage_mesh(std::vector<PackedVoxelVertex>&&)`.
3. **GPU Upload**:
   - `Chunk::upload_mesh()` runs **strictly on the main thread** inside the render loop before issuing draw calls (`m_vao`, `m_vbo`). Never call OpenGL buffer uploads from worker threads.

## 3. Structural Integrity & Collapse
- Structural stability calculations reside in [src/voxel/structural_check.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/structural_check.cpp).
- When voxels are mined, floating or unsupported clusters are identified and converted into physics entities ([src/entities/dynamic_debris.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/dynamic_debris.cpp)).
