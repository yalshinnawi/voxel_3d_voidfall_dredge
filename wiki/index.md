# Voidfall Dredge - Living Game Design Wiki (`wiki/`)

Welcome to the central design, mechanics, and systems knowledge base for **Voidfall: Dredge**.

---

## 🧭 Systems & Mechanics

- **[Delver Contractors & Archetypes](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/mechanics/delvers_and_classes.md)**: Specifications, traits, loadouts, and 3D suit models for Demolitionist, Vanguard, and Scout.
- **[Modular Level Design & Generation](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/mechanics/level_design_and_generation.md)**: Predetermined room shapes (15 archetypes), progressive sector scaling ($3\times3 \to 4\times4 \to 5\times5$), connected corridor graph, and collision boundaries.
- **[Environmental Hazards & Parkour Mechanics](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/mechanics/environmental_hazards.md)**: Molten thermite slag lakes, punji spike trenches, bottomless void rifts, toxic gas vents, radioactive moats, and parkour traversal systems.
- **[Hazard Clock & Expedition Loop](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/mechanics/hazard_clock.md)**: 4-phase radiation escalation, seismic tremors, and 40-second extraction beacon holdout.
- **[Progression & Economy Architecture](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/mechanics/progression_and_economy.md)**: Decoupled EXP (Player Level / Delver Rank) and Coins (skill upgrades), exponential cost curves ($100 \times 1.6^{\text{tier}-1}$), and level-gated sectors and classes.
- **[Economy Simulator & Math](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/game-design-and-balancing/references/economy_and_progression_math.md)**: Progression mathematical formulas, Voidite/Titanium costs, and respec refund mechanics.
- **[Acoustic Stealth & Explosive Distractions](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/mechanics/stealth_and_noise.md)**: Sound salience weighting, deployable satchel charges, gunfire noise, ballistic block destruction, and localized tremors.
- **[Seismic Sonar & Surveying](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/mechanics/surveying_and_sonar.md)**: Acoustic pulse scans, cooldown recharge curves, and Rank 2 spectroscopic material labels.

---

## 👾 Entities & Materials

- **[Enemy Threats & Behaviors](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/entities/enemies.md)**: Void Stalker, Seismic Burrower, and Volatile Crystalline Leech profiles.
- **[3D Model & Roster Catalog](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/models/README.md)**: Canonical, up-to-date visual reference captures and specifications for all character, enemy, and tool models.
- **[Voxel Material Specifications](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/entities/voxel_materials.md)**: Hardness, drops, emissives, and structural anchor definitions for all 11 voxel tiers.
- **[Dynamic Debris Physics](file:///d:/Projects/voxel_3d_voidfall_dredge/src/entities/README.md)**: Rigid-body simulation of unanchored cave-in boulders.

---

## 🏛️ Code Subsystem Technical Manuals

- **[Core Subsystem](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/README.md)**: Application lifecycle, window, logging, save system, visual harness.
- **[Voxel Engine](file:///d:/Projects/voxel_3d_voidfall_dredge/src/voxel/README.md)**: 32x32x32 chunks, 8-byte packed vertices, greedy meshing, BFS cave-ins.
- **[Graphics Pipeline](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/README.md)**: Clustered PBR voxel shaders, volumetric fog, SSAO, bloom.
- **[Player Controller](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/README.md)**: Movement, grapple hook, thruster jetpack, class archetypes.
- **[Multiplayer Networking](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/README.md)**: UDP packets, deterministic seeds, delta block sync.
- **[Audio Engine & Ear Safety](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/systems/audio_engine.md)**: 3D spatialization, procedural synthesis, 5-stage ear safety mastering chain.
- **[User Interface & Menus](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/README.md)**: Orbital hub, HUD, debrief, bitmap font renderer.

---

## 📜 Meta & Project History

- **[Changelog & Decisions Log](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/log.md)**: Record of balance adjustments and system updates.
