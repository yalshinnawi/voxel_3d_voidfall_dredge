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
- **40-Second Beacon Defense:** Deploy the emergency extraction beacon (`B`), hold off subterranean hazards under pulsating red emergency flares, and evacuate into the landing pod.

---

## 📁 Project Structure

```
d:/Projects/voxel_3d_voidfall_dredge/
├── CMakeLists.txt              # Root build configuration (C++20, static core, PCH, CTest, /MP)
├── README.md                   # Technical documentation & architecture manual
├── GEMINI.md                   # Agent guidelines & fast TDD execution protocols
├── play.bat                    # Fast Windows launcher
├── .gitignore                  # Build artifact and compiler filters
├── include/                    # External single-header dependencies
│   ├── font8x8.h               # 8x8 bitmap font renderer for UI & montage overlays
│   ├── stb_image_write.h       # PNG/JPEG image exporter
│   └── glad/                   # OpenGL 4.5 Core function loader
├── scripts/                    # Developer, TDD, and diagnostic automation
│   ├── tdd.py                  # High-speed parallel test runner & live watch mode
│   ├── analyze_screenshots.py  # Instant visual test metrics parser (<0.1s)
│   └── simulate_economy.py     # Progression & economy mathematical model simulator
├── tests/                      # Automated test targets
│   ├── test_unit_all.cpp       # 19 comprehensive unit test modules
│   ├── test_progression.cpp    # Archetypes, respec refunds, and upgrade tree tests
│   └── test_e2e_expeditions.cpp# 5 end-to-end mission lifecycle scenarios
├── saves/                      # Player profile persistence
│   └── save_data.json          # Active expedition progress, unlocked tiers, and resources
├── screenshots/                # Visual verification & automated test captures
│   ├── 00_all_phases_montage.jpg # 4x3 consolidated contact sheet of all 10 test screens
│   ├── visual_report.txt/.json # Luminance & health diagnostic matrix
│   └── previews/               # 800x450 lightweight JPEG frame previews (~40KB each)
├── wiki/                       # Living Game Design Wiki
│   ├── index.md                # Central wiki landing page
│   ├── mechanics/              # Hazard clock, evacuation holdouts
│   └── entities/               # Enemy profiles and voxel material tables
├── assets/
│   ├── shaders/                # PBR voxel, compute fog, bloom, SSAO, viewmodel, UI shaders
│   ├── textures/               # PBR material layers
│   └── sounds/                 # Subterranean audio cues
└── src/
    ├── core/                   # Application lifecycle, window, logger, screenshot, saves, PCH
    ├── graphics/               # HDR Framebuffer, PBR voxel renderer, viewmodel, post-processing
    ├── voxel/                  # Chunk storage (32x32x32), greedy mesher, world, structural BFS
    ├── player/                 # First-person controller, class archetypes, upgrade tree
    ├── entities/               # Rigid-body dynamic debris boulder entities
    ├── systems/                # Hazard clock, radiation buildup, extraction beacons
    ├── skills/                 # Delver surveying sonar, mineral outlines, skill matrix
    ├── ui/                     # Orbital hub carousel, HUD gauges, pause menu
    ├── net/                    # UDP sockets, packet serialization, delta sync
    └── main.cpp                # CLI entry point
```

---

## 👥 3D Characters, Enemies & Models Visual Catalog

All in-game models (enemies, delver contractor classes, tools) have up-to-date reference captures stored in [`docs/models/`](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/models/):

![3D Model Roster Showcase](docs/models/models_roster_showcase.jpg)

- **Hostile Entities**: Void Stalker ([docs/models/enemy_void_stalker.png](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/models/enemy_void_stalker.png)), Seismic Burrower ([docs/models/enemy_seismic_burrower.png](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/models/enemy_seismic_burrower.png))
- **Delver Contractors**: Demolitionist Kaelen ([docs/models/character_demolitionist_kaelen.png](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/models/character_demolitionist_kaelen.png)), Vanguard Rhodes ([docs/models/character_vanguard_rhodes.png](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/models/character_vanguard_rhodes.png)), Scout Vesper ([docs/models/character_scout_vesper.png](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/models/character_scout_vesper.png))
- **Primary Tool Rig**: Modular Mining Drill Rig ([docs/models/system_viewmodel_drill.png](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/models/system_viewmodel_drill.png))
- **Regenerate Anytime**: `python scripts/capture_models.py` (or `./build/Release/VoidfallDredge.exe --capture-models`)

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
| **`B`** | Deploy 40-Second Emergency Extraction Beacon |
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
