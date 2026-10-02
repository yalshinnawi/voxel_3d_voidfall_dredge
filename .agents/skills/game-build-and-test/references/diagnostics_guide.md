# Diagnostics & Troubleshooting Guide

This reference outlines root-cause analysis procedures for common engine crashes and validation errors.

## 1. Runtime Crash Signatures

### Assertion Failures
- **Symptom**: Process terminates abruptly with non-zero exit code.
- **Investigation**: Check the tail of [voidfall.log](file:///d:/Projects/voxel_3d_voidfall_dredge/voidfall.log). Look for `[FATAL]` or `[ERROR]` macros emitted by `LOG_ERROR` in [src/core/logger.hpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/logger.hpp).
- **Common Cause**: Voxel index out of bounds (`idx >= CHUNK_VOLUME`), or chunk coordinates queried without spatial existence in `World::get_chunk`.

### OpenGL State & Buffer Errors
- `GL_INVALID_OPERATION`: Commonly occurs if an OpenGL draw call (`glDrawArrays`, `glDrawElements`) is invoked before binding the VAO, or if an OpenGL function is called on a worker thread instead of the main thread.
- `GL_INVALID_VALUE`: Invalid texture units or shader program handles (`program == 0`).

---

## 2. Test Suites Reference

| Executable | Source File | Scope & Invariants Tested |
| :--- | :--- | :--- |
| `test_unit_all.exe` | [tests/test_unit_all.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_unit_all.cpp) | Chunk allocation, greedy meshing face counts, save file serialization, spatial hashing. |
| `test_e2e_expeditions.exe` | [tests/test_e2e_expeditions.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_e2e_expeditions.cpp) | Full gameplay cycle: player spawn, voxel mining, inventory transfer, extraction countdown, hazard clock progression. |
| `test_progression.exe` | [tests/test_progression.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_progression.cpp) | Upgrade unlock trees, credits balance, perk application across expeditions. |
