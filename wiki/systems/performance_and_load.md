# Engine Performance, Load Scaling & Anti-Clipping Architecture (`wiki/systems/performance_and_load.md`)

This document details the performance optimizations, high-load mitigations, audio starvation defenses, and enemy perception state stability mechanisms designed to keep **Voidfall: Dredge** smooth ($\ge 60\,\text{FPS}$) and rock-solid during late expeditions and heavy computational load.

---

## 1. Late-Round Voxel Generation & Rendering Optimization

### 1.1 The Infinite Phantom Chunk Leak (Resolved)
- **Problem**: When exploring larger sectors (Sector 3: $5\times 5$ rooms / $120\times 120$ voxels), the player moving near perimeter walls caused `World::update(viewer_pos, render_distance)` to query chunk coordinates outside the level boundary ($cx < 0$ or $cx \ge \text{max\_cx}$). Previously, `get_or_create_chunk()` allocated brand new 32x32x32 chunks (32,768 voxels each) filled with solid bedrock/mantle terrain on the main thread, greedy-meshed them, and permanently retained them in `m_chunks`. Over a 10-minute expedition, this generated dozens of redundant boundary chunks, severely degrading frame rates.
- **Mitigation**:
  - `World::update()` strictly clamps horizontal and vertical iteration to the Level Generator spatial bounds:
    $$\text{cx} \in [0, \text{max\_cx}-1], \quad \text{cy} \in [0, \text{max\_cy}-1], \quad \text{cz} \in [0, \text{max\_cz}-1]$$
  - `World::upload_dirty_chunks()` verifies `chunk->has_staged_mesh()` using atomic flag checks before taking chunk locks, skipping empty or untouched chunks.

### 1.2 View-Frustum Culling Pipeline
- **Problem**: In late sectors, all chunks across the entire cavern grid were being submitted to OpenGL every frame, including dozens of chunks $180^\circ$ behind the player camera. Furthermore, empty chunks (chambers carved completely out to air) still incurred OpenGL shader uniform binding overhead.
- **Mitigation**:
  - `Renderer::begin_frame()` extracts the 6 viewing frustum planes from the combined View-Projection matrix via the normalized **Gribb-Hartmann algorithm**:
    $$\pi_i = \mathbf{row}_4 \pm \mathbf{row}_k, \quad k \in \{1, 2, 3\}$$
  - `Renderer::render_chunk()` performs branchless AABB vs frustum plane intersection tests:
    ```cpp
    glm::vec3 min_pt(chunk.position().x * CHUNK_SIZE, chunk.position().y * CHUNK_SIZE, chunk.position().z * CHUNK_SIZE);
    glm::vec3 max_pt = min_pt + glm::vec3(CHUNK_SIZE);
    if (!is_box_in_frustum(min_pt, max_pt)) return;
    ```
  - Chunks with zero staged vertices (`vertex_count == 0`) early-exit before binding VAO or updating shader uniforms.
  - Dynamic debris boulders execute frustum sphere culling prior to drawing.

### 1.3 Simulation Spiral-of-Death Protection
- **Problem**: `Application::run()` previously accumulated variable delta-time without an upper execution ceiling:
  ```cpp
  while (accumulator >= fixed_dt) {
      fixed_tick(fixed_dt);
      accumulator -= fixed_dt;
  }
  ```
  If a frame experienced an OS hitch, background garbage collection, or heavy mesh upload lasting 250ms, the engine would attempt to execute 15 full physics/AI/collision ticks consecutively. If those 15 ticks took longer than 250ms, `accumulator` grew larger on the next frame, causing an unrecoverable freeze cascade ("spiral of death").
- **Mitigation**:
  - Enforced a hard simulation ceiling `MAX_FIXED_TICKS_PER_FRAME = 4`:
  ```cpp
  int fixed_ticks_executed = 0;
  constexpr int MAX_FIXED_TICKS_PER_FRAME = 4;
  while (accumulator >= fixed_dt && fixed_ticks_executed < MAX_FIXED_TICKS_PER_FRAME) {
      fixed_tick(static_cast<float>(fixed_dt));
      accumulator -= fixed_dt;
      fixed_ticks_executed++;
  }
  if (accumulator >= fixed_dt) {
      accumulator = 0.0; // Discard stale simulation backlog
  }
  ```

### 1.4 Chunk Mesh GPU Upload Throttling & Driver Amortization
- **Problem**: During seismic tremors, kinetic detonations, or massive ceiling cave-ins, dozens of chunks are modified and remeshed concurrently. If all completed greedy meshes are uploaded to the GPU (`glGenBuffers`, `glBufferData`) in a single render frame, driver pipeline stalls and GPU synchronization bubbles cause noticeable hitching ($\ge 40\,\text{ms}$ spikes).
- **Architecture**:
  - **Asynchronous Completion Queue**: Meshing thread completion is strictly separated from OpenGL driver uploads. When background worker threads finish greedy-meshing, they stage vertices in `Chunk::stage_mesh()` and push completed chunks to an internal upload queue (`World::m_upload_queue`).
  - **Frame Rate Throttling**: In `World::Update()` / `World::update()`, at most **2 chunk VBO/VAO buffers** are uploaded to the GPU per frame (`MAX_CHUNK_UPLOADS_PER_FRAME = 2`), amortizing driver overhead across multiple frames.
  - **Deduplication**: `m_upload_queued_set` guarantees that redundant dirty notifications for the same chunk do not duplicate entries in the upload queue.
  - **Fallback Handling**: If synchronous meshing is used or the worker queue is bypassed, `World::upload_mesh_queue()` scans staged chunks while strictly respecting the per-frame upload budget.

---

## 2. Enemy AI State Stability & Anti-Clipping

### 2.1 The Alert $\leftrightarrow$ Attacking Hysteresis Trap (Resolved)
- **Problem**: Under heavy load or at medium distances ($\sim 15\,\text{m}$), Void Stalkers rapidly flickered/clipped between `Investigating` (alert) and `Stalking`/`Lunging` (attacking).
- **Root Causes**:
  1. **Perception Range Inversion**: `visual_range` was set to $20.0\,\text{m}$ in `Investigating`, but dropped to $12.0\,\text{m}$ upon transitioning into `Stalking` or `Lunging`. If the player was standing at $15\,\text{m}$, the stalker spotted the player (dist $\le 20\,\text{m}$) and transitioned to `Stalking`. But in `Stalking`, the range dropped to $12\,\text{m}$, so the player immediately failed the distance check, triggering "lost line-of-sight" on the very next tick!
  2. **Un-reset Timers**: `s.lost_los_timer` was not initialized to zero upon state entry. If a stalker had accumulated LOS loss time in an earlier encounter, transitioning into `Stalking` immediately tripped the fallback check (`lost_los_timer > los_break_time`), dumping the creature back into `Investigating`.
  3. **Zero State Hysteresis**: A single 1-frame raycast obstruction or coordinate fluctuation could instantly break aggressive combat states.
- **Mitigation**:
  - **Unified Perception Envelope**: All alert and aggressive states (`Investigating`, `Stalking`, `Circling`, `Lunging`) share a consistent $20.0\,\text{m}$ detection range ($25.0\,\text{m}$ under direct headlamp beam).
  - **Timer Sanitization**: All incoming transitions into `Stalking` and `Lunging` explicitly reset `s.lost_los_timer = 0.0f;`.
  - **Commitment Hysteresis**: Stalkers require a minimum sustained state commitment ($\text{state\_timer} \ge 1.0\,\text{s}$) before permitting a fallback to `Investigating`.

---

## 3. Audio Subsystem Underrun & Voice Saturation Defense

### 3.1 Audio Thread Starvation & Clipping Fix
- **Problem**: Under intense CPU load, sounds clipped out or went completely silent.
- **Root Causes**:
  1. The Windows `waveOut` audio worker thread waited on `WaitForSingleObject(h_event, 50)`. It only replenished audio buffers when `wait_res == WAIT_OBJECT_0`. If high frame spikes caused an audio underrun where all buffers finished playing, `h_event` was not re-signaled until new buffers were submitted. On `WAIT_TIMEOUT`, the thread did nothing, resulting in permanent dead air.
  2. Buffer pool size was only 4 buffers $\times 1024$ frames ($\sim 92\,\text{ms}$ total cushion), which was easily starved during large sector loading spikes.
- **Mitigation**:
  - Expanded the ring buffer pool from 4 to 8 buffers ($185\,\text{ms}$ cushion).
  - The worker loop replenishes all completed buffers (`WHDR_DONE`) on **both** event signals and timeout wakeups ($20\,\text{ms}$ interval), providing autonomous self-healing if an underrun occurs:
  ```cpp
  while (m_platform->running) {
      DWORD wait_res = WaitForSingleObject(m_platform->h_event, 20);
      if (!m_platform->running) break;

      for (size_t i = 0; i < PlatformData::NUM_BUFFERS; ++i) {
          if (m_platform->headers[i].dwFlags & WHDR_DONE) {
              render_mix_i16(m_platform->buffers[i].data(), PlatformData::BUFFER_FRAMES);
              waveOutWrite(m_platform->h_wave_out, &m_platform->headers[i], sizeof(WAVEHDR));
          }
      }
  }
  ```

### 3.2 64-Voice Capacity & Priority-Based Eviction
- **Problem**: High-frequency environmental cues (e.g. Geiger clicks in radioactive zones, multiple cavern drips) saturated voice channels, stealing active voices mid-sample. Monster screams and weapon blasts were abruptly truncated with audible clicks.
- **Mitigation**:
  - Increased `MAX_VOICES` from 32 to 64.
  - Implemented 4-tier sound cue priority scheduling (`get_cue_priority`):
    - **Priority 3 (Critical Horror & Feedback)**: `BurrowerRoar`, `StalkerLunge`, `StalkerDie`, `PlayerDeath`, `SectorArrival`, `ExplosiveBlast`.
    - **Priority 2 (Combat & Weapons)**: `PlasmaFire`, `ScattergunFire`, `RailgunFire`, `StalkerSpotted`, `VoxelBreakTitanium`, `UIUpgrade`.
    - **Priority 1 (Player SFX)**: `Footstep`, `Jump`, `Land`, `VoxelHit`, `PlayerGroan`.
    - **Priority 0 (Micro-Ambience)**: `GeigerClick`, `CavernDrip`, `SporePlop`, `PebbleSkitter`.
  - When all 64 voices are busy, incoming cues can **only** evict lower-priority sounds or sounds of equal priority that have already played $\ge 0.12\,\text{s}$. If all channels are busy with higher-priority sounds, low-priority cues drop silently rather than corrupting existing audio.

---

## 4. Late-Round Playability & Pacing Balance

### 4.1 Encounter Capacity Scaling
To ensure late rounds remain challenging without overwhelming frame rates with entity pathfinding:
| Sector | Room Count | Grid Size | Grace Period | Early Threat Cap ($0-2\,\text{m}$) | Max Simultaneous Enemies |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Sector 1: Perimeter Drift** | 9 rooms | $3\times 3$ ($72\times 72$ voxels) | $45\,\text{s}$ | 1 enemy | 2 enemies |
| **Sector 2: Volatile Fault** | 16 rooms | $4\times 4$ ($96\times 96$ voxels) | $35\,\text{s}$ | 2 enemies | 4 enemies |
| **Sector 3+: Void Cradle** | 25 rooms | $5\times 5$ ($120\times 120$ voxels) | $25\,\text{s}$ | 3 enemies | 6 enemies |

- **Quarantine Zone**: Enemies never spawn within $28\,\text{m}$ (`SPAWN_SAFE_RADIUS`) of the Delver insertion pod.
- **Telegraphed Emergence**: In late sectors, hostiles telegraph emergence $2.0\,\text{s}$ in advance via acoustic skittering and dust particle bursts before solidifying, ensuring fair reaction times under high tension.

---

## 5. Automated Load & Stress Testing (`test_load_and_stress`)

The test suite [tests/test_load_and_stress.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/tests/test_load_and_stress.cpp) runs automatically with the TDD harness (`python scripts/tdd.py`) and validates:
1. **Module 1**: Sector 3, 4, 5 generation bounding and zero phantom chunk generation.
2. **Module 2**: 300ms frame lag spike clamping and accumulator backlog discard.
3. **Module 3**: 30-frame simulated variable $dt$ jitter across $15\,\text{m}$ distance asserting zero Stalker state flip-flops.
4. **Module 4**: 64-voice saturation test verifying high-priority sounds resist eviction against 100+ Geiger clicks.
5. **Module 5**: Expedition grace period and post-ramp capacity verification across all sectors.
