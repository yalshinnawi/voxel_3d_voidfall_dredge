# Hazard Clock & Evacuation Mechanics

Governs the extraction loop, environmental escalation, and expedition pacing in **Voidfall: Dredge** based on [`src/systems/hazard_clock.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/hazard_clock.hpp) and [`src/systems/extraction.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/systems/extraction.hpp).

---

## ☢️ 1. Hazard Progression Loop

The hazard clock activates the moment the player touches down in a sector. Environmental instability escalates across four distinct phases:

- **Phase 1: Stable (0:00 - 4:00)**:
  - Baseline cavern acoustics and nominal tectonic stability.
  - Spatial proximity radiation active: approaching radioactive mineral veins (`MAT_RADIOACTIVE_ORE`) triggers authentic stochastic Geiger counter clicks and HUD millisievert dosimetry.
- **Phase 2: Agitation (4:00 - 8:00)**:
  - Background radiation accumulates ($+0.40\% / \text{sec}$ to $+0.65\% / \text{sec}$).
  - Suit Geiger counter clicks increase with Poisson arrival distribution; subtle HUD chromatic aberration.
- **Phase 3: Critical (8:00 - 12:00)**:
  - Cavern structural faults become volatile: aggressive rock mining and ballistic gunfire rapidly accumulate tectonic seismic strain.
  - When strain exceeds $35\%$, further mining and gunfire roll probabilistic tremor collapses; reaching $100\%$ forces immediate fault failure.
  - Pre-tremor acoustic warning triggers 1.0s before ceiling detachment, followed by screen trauma and falling `DynamicDebris`.
  - Tremor completion relieves accumulated tectonic stress back to $0\%$.
- **Phase 4: Catastrophic (12:00+)**:
  - Radiation spikes ($>1.0\% / \text{sec}$), severe HUD glitching and static.
  - Constant fault instability, relentless tremors, and rapid structural collapses make remaining underground fatal without swift extraction.

---

## ⚡ 1.1 Dynamic Seismic Stress Mechanics

Rather than tremors occurring solely on passive timers, tectonic instability is actively driven by player actions:

1. **Voxel Extraction Stress**:
   - Mining standard basalt/granite rock: $+4.5\%$ seismic stress.
   - Mining dense ore (Titanium, Radioactive Ore): $+6.5\%$ seismic stress.
   - Mining Voidite Crystals: $+3.5\%$ seismic stress.
   - Breaching reinforced vault blast doors: $+18.0\%$ seismic stress.
2. **Firearm Ballistic Impacts**:
   - Weapon projectiles striking solid rock shake the surrounding cavern geometry:
     - Magma Scattergun flechette impact: $+1.0\%$ stress per pellet.
     - Plasma Carbine bolt impact: $+2.5\%$ stress.
     - Needler Railgun hypersonic penetrator impact: $+4.0\%$ stress.
3. **Explosives & Heavy Demolition**:
   - Micro-shaped charge tunnel detonation: $+16.0\%$ concussive shockwave.
   - Heavy 3x3x3 demolition charge detonation: $+32.0\%$ concussive shockwave.
4. **Natural Fault Dissipation**:
   - When the player remains quiet or crouch-walks, seismic strain naturally bleeds off at $-1.8\% / \text{sec}$, rewarding tactical pacing.
5. **Rupture & Cave-In Trigger Mechanism**:
   - $\text{Stress} < 35\%$: Structurally stable.
   - $\text{Stress} \ge 35\%$: Each rock mined or shot impacting rock rolls a trigger probability: $\text{Chance} = (\text{Stress} - 30\%) \times 0.45\%$.
   - $\text{Stress} = 100\%$: Immediate catastrophic fault collapse.
   - **Pre-Tremor Warning**: 1.0s siren klaxon and cavern creaking before overhead blocks detach.
   - **Post-Tremor Reset**: Once the 4.0s tremor ends, tectonic pressure is fully relieved and stress drops to $0\%$.

---

## ☢️ 1.2 Spatial Radiation & Authentic Geiger Counter System

- **Inverse-Distance Radiation Field**:
  - Active radioactive minerals (`MAT_RADIOACTIVE_ORE`) in cavern walls, radioactive moats, and toxic fissures project a 12-block spatial radiation gradient: $\text{Intensity} \propto (1 - d/R)^2$.
- **Poisson Ionization Click Model**:
  - Real Geiger-Müller tubes detect stochastic ionizing events. AudioEngine synthesizes Poisson arrival intervals with average click rate $\lambda$:
    - $\text{Radiation} \le 2\%$: Silent.
    - $5\% - 20\%$: Sporadic clicks ($1.5 - 8\,\text{Hz}$).
    - $20\% - 50\%$: Steady clicking chatter ($8 - 25\,\text{Hz}$).
    - $50\% - 80\%$: Rapid crackling burst ($25 - 55\,\text{Hz}$).
    - $80\% - 100\%$: Frantic screaming Geiger buzz ($55 - 85\,\text{Hz}$).
  - Clicks dynamically accelerate as the player steps into radioactive zones, and decelerate to silence when backing away.
- **HUD Radiation Monitor Pill**:
  - Active whenever radiation exceeds $2\%$: displays calibrated millisievert/sievert dosimetry (e.g. `[!] RAD: 380 mSv/h (38%)`) with dynamic Emerald, Amber, or Crimson color warning bars.

---

## 🚨 2. Extraction Beacon Protocol

- **Beacon Deployment (`Key B`)**: Activated by the player when target Voidite quota is collected or subterranean vault is breached.
- **Sector-Scaled Holdout Duration**:
  - **Sector 1 (Perimeter Drift)**: **20-second survival defense timer** (`initial_countdown() == 20.0f`). Introductory wave pacing designed for starter gear: exactly 2 solitary stalkers spaced across the holdout, zero burrowers, and zero forced ceiling tremors.
  - **Sector 2 (Volatile Fault)**: **30-second survival defense timer** (`initial_countdown() == 30.0f`). Moderate escalation: 4 stalkers, 1 burrower at 70% elapsed, mild final tremor with protected LZ.
  - **Sector 3 (Abyssal Mantle)**: **40-second survival defense timer** (`initial_countdown() == 40.0f`). Endgame apex challenge: 7 stalkers, 1 burrower at 65% elapsed, severe tectonic tremor at 88%.
- **Beacon Defense Perimeter & Damage Dampening**:
  - The deployed beacon generates an active 12-meter kinetic defense perimeter (`is_player_in_perimeter()`).
  - While holding out within the perimeter, Delvers receive substantial incoming damage resistance:
    - **Sector 1**: $+40\%$ damage resistance (applies to claw mauls, spine projectiles, falling debris, and toxic radiation).
    - **Sector 2**: $+25\%$ damage resistance.
    - **Sector 3**: $+15\%$ damage resistance.
- **Landing Zone Overhead Ceiling Shielding**:
  - During holdouts, ceiling tremor rock detachment queries protect the extraction zone. In Sector 1, ceiling spalling is completely suppressed near the beacon; in Sectors 2+, a 6-block radius around the beacon is shielded from falling rocks.
- **Emergency Siren**: High-intensity 360° red emergency strobe alerts hazards and provides dynamic perimeter illumination.
- **Evacuation Pod Touchdown**: At $0.0\text{s}$, the drop pod touches down at the beacon coordinates.
- **Resource Banking**: Stepping into the extraction pod concludes the expedition, banks collected Voidite, Titanium, and EXP in [`saves/save_data.json`](file:///d:/Projects/voxel_3d_voidfall_dredge/saves/save_data.json), and awards sector mastery badges.
