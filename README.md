# Voidfall: Dredge (Multiplayer Edition)

A high-performance subterranean sci-fi voxel action game and multiplayer engine built in modern **C++20** and **OpenGL 4.5 Core Profile**.

![Voidfall: Dredge Architecture](https://raw.githubusercontent.com/yalshinnawi/voxel_3d_voidfall_dredge/main/assets/banner.png)

---

## 🌌 Overview

**Voidfall: Dredge** bridges custom high-density voxel architectures with an atmospheric subterranean rendering pipeline and client-server multiplayer synchronization. Delvers descend into volatile planetary depths to extract Voidite crystals, breach sealed industrial vaults, survive escalating radiation tremors, and defend extraction beacons against cave-ins and anomalies.

---

## ⚡ Core Engine Features

### 1. High-Density Voxel & Graphics Pipeline
- **$32 \times 32 \times 32$ Chunk Storage:** 16-bit packed voxel memory layout (`uint8_t material_id`, `uint8_t flags_and_damage`).
- **Bit-Packed 8-Byte Vertex Format:** Packed local position $(X, Y, Z)$, normal index, baked vertex AO, texture array layer ID, and greedy quad dimensions in two 32-bit words (`uvec2`).
- **Greedy Meshing with Baked Vertex AO:** Multi-threaded greedy mesher evaluating corner and edge occlusion from neighboring blocks with diagonal anisotropy compensation.
- **PBR 2D Texture Arrays (`GL_TEXTURE_2D_ARRAY`):** Albedo, Normal, Roughness/Metallic, and Emissive channels across all geological tiers (Fractured Granite, Volcanic Basalt, Voidite Crystals, Industrial Bulkheads, Reinforced Vault Doors, Molten Thermite Slag, Radioactive Ore, and Bedrock).
- **Forward Clustered Lighting:** High-intensity player headlamp spotlights, dynamic flares, thermite burns, and extraction beacon emergency sirens.
- **Compute Shader Volumetric Fog:** Half-resolution raymarched fog volume (`rgba16f`) evaluating particulate dust and Henyey-Greenstein Mie forward scattering.
- **Post-Processing Chain:** Dual MRT thresholded HDR Bloom, Screen-Space Ambient Occlusion (SSAO), and ACES Filmic tonemapping.
- **Seismic Sonar Pulse:** Holographic edge-detection and wireframe silhouette pass highlighting occluded mineral veins and structural faults through solid rock.

### 2. Physics & Dynamic Destruction Engine
- **Server-Authoritative Anchored-Island BFS:** Breadth-first search tracing structural connectivity to bedrock or vault anchors upon block destruction.
- **Dynamic Debris Physics:** Disconnected floating voxel clusters are sliced from static terrain and converted into synchronized rigid-body physics entities (`DynamicDebris`) that tumble, bounce, and crush terrain.
- **Tension-Cable Grappling Hook:** Raycast-anchored cable physics allowing delvers to swing over deep chasms, hoist up vertical shafts, and reel into vantage points.
- **Exo-Suit Physics:** Momentum-based ground movement, sprint, mantling, and vertical thruster hovering with power consumption and thermal dissipation meters.

### 3. Multiplayer Architecture & Networking
- **Client-Server UDP Transport:** Low-latency packet serialization supporting authoritative hosts and dedicated servers.
- **Deterministic Procedural Caverns:** Clients initialize terrain from a shared 32-bit world seed; only dynamic voxel mutations (`BlockDeltaPacket`) are transmitted across the network.
- **Delta-Compressed Block Synchronization:** Fast bitfield packets synchronizing mining drills, shaped charges, and structural bulkhead placements.
- **Client-Side Prediction:** First-person movement and thrusters are simulated client-side with server snapshot reconciliation.

### 4. Gameplay Loop
- **Cavern Descent:** Drop into procedurally generated 3D noise cavern networks with winding tunnels, ore pockets, and ancient industrial vault ruins.
- **Void Hazard Clock:** Escalating radiation levels cause suit HUD glitching and trigger synchronized seismic cave tremors that shake the camera and trigger dynamic cave-ins.
- **90-Second Beacon Defense:** Deploy the emergency extraction beacon (`B`), hold off subterranean hazards under pulsating red emergency flares, and evacuate into the landing pod.

---

## 📁 Project Structure

```
d:/Projects/voxel_3d_voidfall_dredge/
├── CMakeLists.txt              # Root build configuration (C++20, OpenGL 4.5, GLFW, GLAD, GLM)
├── README.md                   # Technical documentation & architecture manual
├── .gitignore                  # Build artifact and compiler filters
├── assets/
│   ├── shaders/
│   │   ├── voxel_pbr.vert      # Bit-packed vertex input, AO & sonar wave calculation
│   │   ├── voxel_pbr.frag      # PBR texture array, forward clustered lights & bloom MRT
│   │   ├── volumetric_fog.comp # Half-res raymarched dust fog & light shaft compute shader
│   │   ├── ssao.frag           # Screen-Space Ambient Occlusion
│   │   ├── sonar_pulse.frag    # Seismic Sonar pulse wireframe edge overlay
│   │   ├── bloom.frag          # Gaussian blur downsampling/upsampling passes
│   │   ├── postprocess.frag    # ACES tonemapping, fog composite & radiation glitch
│   │   ├── ui.vert             # HUD orthographic vertex shader
│   │   └── ui.frag             # HUD gauge fragment shader
│   ├── textures/               # PBR material layers
│   └── sounds/                 # Subterranean audio cues
└── src/
    ├── core/
    │   ├── glad/               # OpenGL 4.5 Core loader
    │   ├── window.hpp/.cpp     # GLFW windowing & input system
    │   └── application.hpp/.cpp# Game loop, 60 Hz physics tick & rendering orchestrator
    ├── graphics/
    │   ├── shader.hpp/.cpp     # Graphics & compute shader compiler/linker
    │   ├── texture_array.hpp/.cpp # 2D Texture Array loader & procedural material generator
    │   └── renderer.hpp/.cpp   # Framebuffer orchestration, HDR pipeline & post-processing
    ├── voxel/
    │   ├── packed_vertex.hpp   # 8-byte vertex format & 16-bit packed voxel state
    │   ├── chunk.hpp/.cpp      # 32x32x32 voxel container with GPU buffer management
    │   ├── greedy_mesher.hpp/.cpp # 6-face greedy mesher with baked vertex AO
    │   ├── world.hpp/.cpp      # Procedural 3D noise caverns & DDA raycasting
    │   └── structural_check.hpp/.cpp # Anchored Island BFS cave-in solver
    ├── player/
    │   └── controller.hpp/.cpp # First-person controller, jetpack & grapple physics
    ├── entities/
    │   └── dynamic_debris.hpp/.cpp # Rigid-body falling boulder entities
    ├── systems/
    │   ├── hazard_clock.hpp/.cpp # Radiation clock & seismic cave tremor generator
    │   └── extraction.hpp/.cpp   # 90-second extraction beacon sequence
    ├── ui/
    │   └── hud.hpp/.cpp        # Suit gauges, depth meter, radiation bar & crosshair
    ├── net/
    │   ├── packet_types.hpp    # Binary packet serialization & player inputs
    │   ├── net_host.hpp/.cpp   # Authoritative UDP host session manager
    │   └── net_client.hpp/.cpp # Client connection & delta synchronization
    └── main.cpp                # CLI entry point
```

---

## 🎮 Controls

| Key / Input | Action |
| :--- | :--- |
| **`W, A, S, D`** | First-Person Delver Traversal |
| **`SPACE`** | Jump / Hold for Exo-Suit Jetpack Hover Thrusters |
| **`LEFT SHIFT`** | Sprint |
| **`LEFT CLICK`** | Subterranean Mining Drill (breaches voxels & triggers cave-ins) |
| **`RIGHT CLICK`** | Place Structural Industrial Bulkhead |
| **`F` / `MIDDLE CLICK`** | Fire Tension-Cable Grappling Hook |
| **`E`** | Reel In Grappling Hook Cable |
| **`Q`** | Seismic Sonar Pulse Scan (Surveying skill wireframe highlight) |
| **`B`** | Deploy 90-Second Emergency Extraction Beacon |
| **`H`** | Toggle Headlamp Spotlight |
| **`TAB` / `ESC`** | Toggle Mouse Cursor Capture |

---

## 🛠️ Build & Compilation

### Requirements
- **C++20 Compiler** (MSVC v143 / Visual Studio 2022, GCC 11+, or Clang 13+)
- **CMake 3.20+**
- **OpenGL 4.5 Core Profile** compatible graphics hardware

### Windows (Visual Studio 2022 / Ninja)
```powershell
# Generate build configuration
cmake -B build -S .

# Build target executable (Release)
cmake --build build --config Release

# Run Host Server
./build/Release/VoidfallDredge.exe --host

# Or connect as multiplayer client
./build/Release/VoidfallDredge.exe --client 127.0.0.1 --name "Delver_2"
```

---

## 📜 License
MIT License. Created by [yalshinnawi](https://github.com/yalshinnawi).
