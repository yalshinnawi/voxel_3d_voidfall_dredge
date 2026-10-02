---
name: shader-pipeline
description: >-
  Use this skill when creating, modifying, or debugging GLSL shaders in assets/shaders/,
  tuning post-processing effects (bloom, SSAO, volumetric fog), or connecting shader uniforms to C++ renderers.
---

# Shader Pipeline & Graphics Development

This skill governs GLSL shader workflows, post-processing filters, and synchronization between GPU shaders and C++ rendering code.

## 1. Quick Iteration & Asset Sync
When making adjustments to fragment/vertex shaders, you do not need to run a full CMake recompile. You can mirror changes immediately using the sync helper script:
- Helper Script: [scripts/sync_assets.ps1](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/shader-pipeline/scripts/sync_assets.ps1)

```powershell
powershell -ExecutionPolicy Bypass -File .agents/skills/shader-pipeline/scripts/sync_assets.ps1
```

---

## 2. Directory Structure & Layout
All shaders are located in [assets/shaders/](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/):
- **Voxel PBR**: [voxel_pbr.vert](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/voxel_pbr.vert), [voxel_pbr.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/voxel_pbr.frag)
- **Volumetric Fog**: [volumetric_fog.comp](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/volumetric_fog.comp) (Compute shader, OpenGL 4.3+)
- **Post-Processing & Lighting**:
  - Bloom: [bloom.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/bloom.frag)
  - SSAO: [ssao.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/ssao.frag)
  - Final Compositing: [postprocess.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/postprocess.frag)
  - Sonar Pulse: [sonar_pulse.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/sonar_pulse.frag)
- **UI & HUD**:
  - Text: [text.vert](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/text.vert), [text.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/text.frag)
  - UI Quads: [ui.vert](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/ui.vert), [ui.frag](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/ui.frag)

---

## 3. Uniform & Pipeline Reference
For uniform naming rules, matrix transposition conventions, texture slots, and compute shader barrier rules, consult:
- Reference Guide: [references/uniform_conventions.md](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/shader-pipeline/references/uniform_conventions.md)
