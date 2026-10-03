# Automated Test Suites (`tests/`)

This directory contains the automated test suites for **Voidfall: Dredge**, covering unit testing, progression economics, and end-to-end (E2E) mission lifecycles.

---

## 📁 Test Targets & Files

| Test File | Test Target | Description |
| :--- | :--- | :--- |
| [`test_unit_all.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_unit_all.cpp) | `test_unit_all.exe` | **19 Comprehensive Unit Tests**: Voxel indexing, 16-bit packing, greedy mesher AO, structural BFS, dynamic debris, grapple physics, hazard timers, extraction beacon, save serialization, network packets, and surveying sonar. |
| [`test_progression.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_progression.cpp) | `test_progression.exe` | **Progression & Balance Tests**: Delver class archetypes (Demolitionist/Vanguard/Scout), upgrade purchasing logic, exponential EXP curves ($100 \times 1.6^{\text{tier}-1}$), and respec refund integrity. |
| [`test_e2e_expeditions.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_e2e_expeditions.cpp) | `test_e2e_expeditions.exe` | **5 End-to-End Mission Scenarios**: Complete simulated runs testing mineral banking, bulkhead sheltering during cave-ins, radiation hazard breach, class loadouts, and multi-sector unlocks. |
| `test_*.json` | Test Fixtures | Mock player profiles used to validate save schema migrations and edge cases without mutating real saves. |

---

## ⚡ Execution Commands

The fastest way to compile and run all suites in parallel is via the TDD automation runner:
```bash
python scripts/tdd.py
```

Alternatively, invoke individual test binaries directly:
```bash
./build/Release/test_unit_all.exe
./build/Release/test_progression.exe
./build/Release/test_e2e_expeditions.exe
```

Or via CMake CTest:
```bash
ctest --test-dir build -C Release -j 3 --output-on-failure
```

---

## 🛡️ Adding New Tests

- Unit tests must be fast, self-contained, and runnable in headless/CI environments without requiring an active GLFW window or OpenGL context.
- Use `TEST_CHECK(expr, msg)` to assert invariants.
- Keep execution time below $100\text{ms}$ per suite.
