# Graphics & Rendering Subsystem (`src/graphics/`)

The `graphics` subsystem implements a subterranean PBR rendering pipeline in OpenGL 4.5 Core Profile, complete with compute shader volumetric fog, clustered dynamic lights, Screen-Space Ambient Occlusion (SSAO), HDR Bloom, and first-person procedural tool viewmodels.

---

## 📁 Source Files

| File | Primary Responsibility | Key Classes / Structs |
| :--- | :--- | :--- |
| [`renderer.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp) | Master rendering engine, HDR framebuffers, clustered lighting, post-processing pipeline, particle FX. | [`Renderer`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.hpp#L47), [`Headlamp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.hpp#L20), [`PointLight`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.hpp#L13) |
| [`shader.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/shader.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/shader.cpp) | Shader compilation, program linking, compute shader dispatch, and uniform caching. | [`Shader`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/shader.hpp#L13) |
| [`texture_array.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/texture_array.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/texture_array.cpp) | 2D Texture Array (`GL_TEXTURE_2D_ARRAY`) generator for geological PBR material layers. | [`TextureArray`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/texture_array.hpp#L9) |
| [`viewmodel.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/viewmodel.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/viewmodel.cpp) | Procedural 3D first-person mining drill geometry, animations, class color themes, and impact sparks. | [`ViewModel`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/viewmodel.hpp#L18) |

---

## 🎨 Rendering Pipeline Overview

Each frame follows an ordered pass sequence:

```
1. Geometry Pass:
   • Bind HDR G-Buffer / Scene FBO (GL_RGBA16F)
   • Draw Voxel Chunks using assets/shaders/voxel_pbr.vert & voxel_pbr.frag
   • Draw DynamicDebris Falling Boulders
   • Render Extraction Beacon & Pulsing Siren

2. Volumetric Fog Pass:
   • Dispatch assets/shaders/volumetric_fog.comp compute shader
   • Half-resolution 3D raymarching with Henyey-Greenstein Mie scattering

3. SSAO & Post-Processing Pass:
   • Compute Screen-Space Ambient Occlusion via assets/shaders/ssao.frag
   • Multi-pass Gaussian blur on occlusion buffer
   • Dual MRT thresholding extracting bright emissive fragments for Bloom
   • Two-pass ping-pong downsampling & upsampling bloom chain

4. ViewModel & HUD Pass:
   • Render first-person mining drill viewmodel with depth range clamping
   • Render 2D UI overlay (Gauges, Reticle, Minimap, Loot Toasts, Menus)
   • Final ACES Filmic Tone Mapping and radiation distortion pass to screen backbuffer
```

---

## 🔦 Dynamic Lighting Specifications

- **Delver Headlamp**: High-intensity spotlight attached to camera direction (`inner_cutoff = 18°`, `outer_cutoff = 32°`, halogen LED white/cyan tint).
- **Extraction Beacon**: Pulsating 360° red emergency strobe with dynamic quadratic attenuation.
- **Volatile Crystal & Radioactive Ore**: PBR emissive surface glow contributing to local voxel illumination and bloom buffer.
- **Seismic Sonar Pulse**: Expanding spherical wave overlay rendering holographic wireframes over occluded mineral veins.

---

## ⚠️ Important Developer Rules

- **Shader Uniform Compatibility**: Uniform names in GLSL shaders must match C++ uniform locations in [`renderer.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/renderer.cpp).
- **Zero Texture Swapping**: All voxel block types are rendered using a single `GL_TEXTURE_2D_ARRAY`, eliminating OpenGL texture state changes during chunk rendering.
- **Main Thread Invariant**: All OpenGL buffer allocations, deletions, and drawing calls must execute on the main thread owning the GLFW OpenGL context.
