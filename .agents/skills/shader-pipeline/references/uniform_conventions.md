# Shader Uniforms & Graphics Pipeline Conventions

This reference defines the GLSL layout requirements, uniform conventions, and post-process FBO structure used in Voidfall Dredge.

## 1. Uniform Layout Guidelines
- **Precision**: Specify float precision explicitly where relevant.
- **Matrix Layout**: Column-major (`glm::mat4` default matches GLSL column-major). Pass with `GL_FALSE` transpose flag:
  ```cpp
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(matrix));
  ```
- **Samplers**:
  - `u_TextureArray` (GL_TEXTURE0): Main voxel block texture array.
  - `u_FontTexture` (GL_TEXTURE0): 8x8 bitmap glyph atlas in [src/ui/font_renderer.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/font_renderer.hpp).
  - `u_SceneColor` (GL_TEXTURE0): HDR scene color for post-process passes.
  - `u_BloomTexture` (GL_TEXTURE1): Downsampled bright-pass blur.

---

## 2. Compute Shader Rules ([volumetric_fog.comp](file:///d:/Projects/voxel_3d_voidfall_dredge/assets/shaders/volumetric_fog.comp))
- **OpenGL Version**: `#version 430 core`.
- **Local Workgroup Size**: `layout (local_size_x = 8, local_size_y = 8, local_size_z = 4) in;`.
- **Memory Barrier**: When writing to 3D fog textures with `imageStore()`, ensure `glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)` is issued prior to sampling in voxel fragment passes.
