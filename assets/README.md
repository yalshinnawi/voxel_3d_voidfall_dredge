# Game Assets Directory (`assets/`)

This directory houses all runtime assets for **Voidfall: Dredge**, including GLSL shaders, procedural texture arrays, and sound effects.

---

## 📁 Directory Structure

```
assets/
├── shaders/   # Complete GLSL rendering pipeline (vertex, fragment, and compute shaders)
├── textures/  # PBR material layers, normal maps, and look-up tables (LUTs)
└── sounds/    # Subterranean audio samples, geiger clicks, siren loops, and ambient rumbles
```

---

## 🔄 Build Asset Synchronization

In CMake ([`CMakeLists.txt`](file:///d:/Projects/voxel_3d_voidfall_dredge/CMakeLists.txt)):
- A post-build command copies `assets/` directly to `<TARGET_FILE_DIR>/assets`.
- When modifying shaders or textures in `assets/`, rebuild via `cmake --build build --config Release` or launch directly so changes propagate to the binary folder.

---

## 🎨 Asset Guidelines

1. **Shaders**: Must adhere to `#version 330 core` for standard rasterization passes, and `#version 430 core` for compute shaders (`volumetric_fog.comp`). Detailed conventions are in [`assets/shaders/README.md`](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/README.md).
2. **Textures**: Voxel terrain materials are packed into `GL_TEXTURE_2D_ARRAY` slices (512x512 per slice) containing Albedo, Normal, Roughness/Metallic, and Emissive channels.
3. **Sounds**: Low-frequency rumble cues, acoustic echoes, and tension audio matching the subterranean atmosphere.
