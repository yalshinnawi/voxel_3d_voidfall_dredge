# GLSL Shaders Pipeline (`assets/shaders/`)

This directory contains the complete shader suite for **Voidfall: Dredge**. Shaders target `#version 330 core` for rasterization passes and `#version 430 core` for volumetric compute passes.

---

## 📁 Shader Catalog

| Shader File | Stage | Purpose & Uniforms |
| :--- | :--- | :--- |
| [`voxel_pbr.vert`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/voxel_pbr.vert) / [`voxel_pbr.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/voxel_pbr.frag) | Rasterization | Unpacks 8-byte vertex format (`uvec2`), samples `GL_TEXTURE_2D_ARRAY`, computes clustered lighting, headlamp spotlight, and PBR BRDF. |
| [`volumetric_fog.comp`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/volumetric_fog.comp) | Compute (`#version 430`) | Half-resolution 3D raymarching computing Mie forward scattering through particulate dust. |
| [`ssao.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/ssao.frag) | Fragment | Calculates Screen-Space Ambient Occlusion from scene depth buffer with normal-oriented hemisphere sampling. |
| [`bloom.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/bloom.frag) | Fragment | Two-pass Gaussian downsample/upsample blur extracting high-luminance emissive fragments (crystals, beacons, flares). |
| [`postprocess.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/postprocess.frag) | Fragment | Composites HDR scene, bloom texture, and SSAO; applies ACES filmic tone mapping and radiation noise. |
| [`sonar_pulse.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/sonar_pulse.frag) | Fragment | Renders the spherical acoustic wavefront overlay highlighting solid surfaces. |
| [`wireframe.vert`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/wireframe.vert) / [`wireframe.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/wireframe.frag) | Rasterization | Draws vibrant neon wireframe bounding boxes around surveyed mineral pockets through occluded terrain. |
| [`viewmodel.vert`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/viewmodel.vert) / [`viewmodel.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/viewmodel.frag) | Rasterization | First-person mining drill, pneumatic pistons, class-specific suit sleeves and glove shading. |
| [`particle.vert`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/particle.vert) / [`particle.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/particle.frag) | Rasterization | Billboarded particle sparks from drill teeth, stone cracking debris, and environmental dust. |
| [`ui.vert`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/ui.vert) / [`ui.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/ui.frag) | Rasterization | 2D orthographic pass rendering gauges, health meters, icons, buttons, and backgrounds. |
| [`text.vert`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/text.vert) / [`text.frag`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/text.frag) | Rasterization | Instanced glyph renderer for the 8x8 bitmap font system. |
| [`fullscreen_quad.vert`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/fullscreen_quad.vert) | Vertex | Generates a full-screen triangle without vertex buffers for screen-space post-processing passes. |

---

## ⚡ Uniform Matching Invariants

When adding or modifying uniforms:
1. Ensure GLSL variable names match the C++ string lookups in [`src/graphics/renderer.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp) (e.g. `u_View`, `u_Proj`, `u_Headlamp.position`, `u_RadiationLevel`).
2. Keep uniform block definitions contiguous and aligned according to `std140` rules if using UBOs.
