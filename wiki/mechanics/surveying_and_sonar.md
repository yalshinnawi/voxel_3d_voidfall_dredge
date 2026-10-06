# Seismic Sonar & Acoustic Surveying (`wiki/mechanics/surveying_and_sonar.md`)

## 1. Overview & Balancing Philosophy
The **Seismic Sonar Pulse** is the primary subterranean scanning tool of the Delver squad. Discharged by pressing `[Q]`, it emits a spherical acoustic compression wave that penetrates deep stone strata and reflects off crystalline and metallic anomalies.

### Why Sonar Was Rebalanced
In initial engine builds, Sonar lacked a recharge cooldown and maintained high base duration, allowing players to spam the ability continuously. This negated the claustrophobic tension of subterranean dredging, removed the need for tactical spatial memory, and trivialize cavern navigation.

To restore deliberate tactical pacing:
1. **Enforced Recharge Cooldown**: Sonar now has a baseline 10.0-second cooldown (8.0s for the Scout archetype). Spamming `[Q]` is blocked while recharging and triggers a HUD recharge warning.
2. **Weaker Base Duration**: Base pulse linger is reduced to 1.8 seconds (down from 2.5s), requiring delvers to pay active attention to ping returns before they fade into the mantle rock.
3. **Gated Mineral Identification**: Uncalibrated sonar (Rank 0 / Rank 1) reflects raw wireframe outlines without rock composition analysis. **Acoustic Spectroscopy (HUD Rock Labels)** is unlocked at **Rank 2+**.

---

## 2. Proficiency & Upgrade Architecture
Sonar scales through two integrated progression paths:
- **Orbital Hub Upgrade Terminal**: Spend Coins and harvested Voidite/Titanium on the *Wide-Spectrum Sonar Transceiver* (`UpgradeType::SonarFrequency`).
- **In-Expedition Delver Proficiency**: Earn Surveying XP by executing scans during missions.

The effective sonar rank is evaluated as:
$$\text{Effective Rank} = \max(\text{upgrades.sonarFrequencyTier}, \text{skills.get\_surveying\_rank()})$$

### Rank & Tier Scaling Table

| Tier / Rank | Cooldown | Pulse Radius (Base / Scout) | Linger Duration | Tactical Capabilities & Unlocks |
|:---:|:---:|:---:|:---:|:---|
| **Rank 0** (Base) | 10.0 s (8.0 s Scout) | 14.0 m / 22.0 m | 1.8 s | Raw acoustic wireframe bounce. **No mineral labels.** |
| **Rank 1** | 9.0 s (7.0 s Scout) | 16.0 m / 24.0 m | 2.1 s | **Extended Frequency**: +2.0m radius, -1.0s cooldown. |
| **Rank 2** | 8.0 s (6.0 s Scout) | 18.0 m / 26.0 m | 2.4 s | **Acoustic Spectroscopy**: Unlocks floating HUD rock labels & distance readouts! |
| **Rank 3** | 7.0 s (5.0 s Scout) | 20.0 m / 28.0 m | 2.7 s | **Deep Cavern Resonance**: High penetration and detection of toxic radioactive anomalies. |
| **Rank 4** | 6.0 s (4.0 s Scout) | 22.0 m / 30.0 m | 3.0 s | Fast capacitor cycling (-4.0s total cooldown reduction). |
| **Rank 5** (Max) | 5.0 s (4.0 s Scout) | 24.0 m / 32.0 m | 3.3 s | **Master Surveyor**: Maximum radius, 5.0s recharge, extended linger. |

---

## 3. Acoustic Spectroscopy & HUD Material Labels (Rank 2+)
When the delver achieves **Rank 2 or higher** in Surveying, the suit's acoustic receiver performs harmonic Fourier analysis on reflected sound waves to identify exact rock and mineral compositions.

### Cluster Extraction & Label Projections
To prevent visual clutter when scanning dense ore veins (which contain dozens of adjacent voxels), [`SurveyingSystem`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/skills/surveying.hpp) groups neighboring voxels of the same material ($\text{distance} \le 3.2\,\text{m}$) into a single [`SurveyedCluster`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/skills/surveying.hpp).

The HUD projects a high-contrast tactical pill marker and distance readout directly at each cluster's 3D world centroid:

- **Voidite Crystals (`MAT_VOIDITE_CRYSTAL`)**:
  - Marker: `[VOIDITE: <dist>m]`
  - Emissive Color: Neon Cyan (`#00F0FF`, `rgb(0, 240, 255)`)
- **Titanium / Bulkheads (`MAT_INDUSTRIAL_BULKHEAD`)**:
  - Marker: `[TITANIUM: <dist>m]`
  - Emissive Color: Industrial Gold / Amber (`#FFB300`, `rgb(255, 179, 0)`)
- **Ancient Vault Doors (`MAT_REINFORCED_VAULT_DOOR`)**:
  - Marker: `[VAULT DOOR: <dist>m]`
  - Emissive Color: Deep Magenta (`#FF00D4`, `rgb(255, 0, 212)`)
- **Radioactive Ores (`MAT_RADIOACTIVE_ORE`)**:
  - Marker: `[RADIOACTIVE: <dist>m]`
  - Emissive Color: Toxic Lime (`#00FF66`, `rgb(0, 255, 102)`)

Labels smoothly fade out synchronously with the acoustic decay curve:
$$\alpha(t) = \text{clamp}\left(\frac{t_{\text{remaining}}}{t_{\text{duration}}}, 0.0, 1.0\right)$$

---

## 4. User Interface Integration
1. **Delver Tactical Ability Badge**:
   - Located directly above the hotbar in the bottom-right HUD corner.
   - States:
     - `[Q] SEISMIC SONAR: READY`: Sonar capacitor fully charged (Cyan).
     - `[Q] SEISMIC SONAR: SCANNING`: Wave pulse active in cavern (Cyan).
     - `[Q] SONAR RECHARGING: X.Xs`: Visual countdown with real-time fill bar (Amber).
2. **Recharge Warning**:
   - Pressing `[Q]` while on cooldown triggers an on-screen warning: `SONAR RECHARGING (X.Xs)` without discharging audio or particles.
3. **Orbital Hub & Debrief Matrices**:
   - Displays active Surveying Rank (`[RANK X/3]`) and detailed perk unlocks in the Delver Proficiency Matrix and debrief manifest.

---

## 5. 2D Tactical Cavern Cartography Map (`[TAB]` Key)

When holding `[TAB]`, the delver deploys the **2D Tactical Cartography Map**, replacing the legacy 3D perspective wireframe with a clean, top-down orthographic navigation overlay designed for subterranean orienteering.

### Cartography Visual Styling & Cavern Geometry
The 2D map formats subterranean geometry into high-clarity floor plans inspired by tactical dungeon cartography:
- **Discovered Cave Floor (`#34302B`)**: Walkable chambers and connecting corridors rendered in deep earthy stone with subtle coordinate variance.
- **Cavern Wall Rim (`#BA8A42` / `#D4A359`)**: Sinuous warm golden-amber cliff contours outlining where open floor meets solid rock, delivering crisp corridor readability.
- **Undiscovered Void (Fog of War)**: Unexplored strata remain in pitch-black void with faint tactical coordinate grid lines. As the delver moves, a 22-meter discovery horizon permanently records visited territory into persistent exploration memory.
- **Hazard Zones (`#D9521E`)**: Molten thermite slag and irradiated pockets tinted with warning hues.

### Navigation Markers & Objective Routing
1. **Extraction Beacon / Landing Pod**:
   - High-vis green (`#2ECC71`) diamond icon with animated expanding radar ping waves.
   - Shows real-time distance and cardinal bearing (e.g., `EXTRACTION [48m] [NW]`).
   - If outside the current map viewport, an edge marker arrow points towards the LZ.
2. **Precursor Relic Vault**:
   - Electric cyan (`#00F2FF`) vault lock marker showing breach/retrieval status and distance.
3. **Active Waypoint Guidance Route**:
   - An animated, flowing dashed route vector connects the player directly to the active objective (Precursor Vault until relic is secured, then Extraction Beacon).
4. **Delver Heading & Avionics**:
   - Sharp safety-amber (`#FFB300`) chevron rotating with player yaw.
   - Translucent $55^\circ$ flashlight view cone extending forward 16 meters.
   - Glowing cyan breadcrumb trail retracing visited paths.
5. **Interactive Controls**:
   - **`[TAB]`**: Open / close map overlay with smooth holographic deployment animation.
   - **`[MOUSE DRAG]`**: Pan the cavern viewport to scout ahead.
   - Defaults to auto-centering directly over the delver's current position.

