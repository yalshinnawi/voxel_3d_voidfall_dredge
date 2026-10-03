# User Interface Subsystem (`src/ui/`)

The `ui` subsystem manages in-game HUD displays, the interactive Orbital Hub station menus, the pause menu, mission debriefing screens, and bitmap font rendering.

---

## 📁 Source Files

| File | Primary Responsibility | Key Classes / Structs |
| :--- | :--- | :--- |
| [`hud.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/hud.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/hud.cpp) | In-game HUD: suit integrity, radiation gauge, thruster power/heat, crosshair, loot toasts. | [`HUD`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/hud.hpp#L18), [`LootToast`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/hud.hpp#L11) |
| [`orbital_hub.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/orbital_hub.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/orbital_hub.cpp) | Hub screens, sector selection, character roster, and upgrade terminal interface. | [`OrbitalHubUI`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/orbital_hub.hpp#L47), [`GameState`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/orbital_hub.hpp#L16), [`HubTab`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/orbital_hub.hpp#L41) |
| [`pause_menu.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/pause_menu.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/pause_menu.cpp) | Escape pause menu overlay with resume, mission abandonment, and settings actions. | [`PauseMenu`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/pause_menu.hpp#L15), [`PauseMenuAction`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/pause_menu.hpp#L6) |
| [`main_menu.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/main_menu.hpp) | Dispatched action definitions for title screen interactions. | [`MainMenuAction`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/main_menu.hpp#L5) |
| [`font_atlas.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/font_atlas.hpp) & [`font_renderer.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/font_renderer.hpp) | 2D orthographic text rendering engine powered by lightweight 8x8 bitmap glyphs. | [`FontRenderer`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/font_renderer.hpp#L14) |

---

## 🖥️ Screen & Menu Hierarchy

```
[Main Menu] (STATE_MAIN_MENU)
  ├── Continue Expedition (loads save_data.json)
  ├── Sector Selection Carousel (Sectors 1..3)
  └── Upgrade Terminal

[Orbital Hub] (STATE_ORBITAL_HUB)
  ├── Tab 0: Sector Select (hazard tiers, depth, reward multipliers)
  ├── Tab 1: Delver Roster (Demolitionist / Vanguard / Scout)
  └── Tab 2: Upgrade Terminal (spend EXP / Voidite / Titanium, Respec)

[In-Game Gameplay] (STATE_GAMEPLAY)
  ├── HUD: Power/Heat/Integrity Gauges, Reticle, Minimap, Loot Toasts
  ├── Hazard & Tremor Warning Banners
  └── Emergency Extraction 40s Countdown

[Pause Menu] (STATE_PAUSED)
  ├── Resume Expedition
  ├── Abandon to Hub (saves partial progress)
  └── Audio / Display Settings

[Mission Debrief] (STATE_DEBRIEF)
  ├── Expedition Outcome (Extracted / Suit Breach)
  ├── Total Voidite, Titanium, and EXP Banked
  └── Sector Mastery Badge & Completion Score
```

---

## 🔤 Font Rendering Performance

UI text is rendered via an orthographic 2D shader (`assets/shaders/text.vert` and `assets/shaders/text.frag`) drawing instanced quads from an 8x8 ASCII font bitmap. This eliminates external TTF/Freetype library dependencies while guaranteeing sub-millisecond draw calls.
