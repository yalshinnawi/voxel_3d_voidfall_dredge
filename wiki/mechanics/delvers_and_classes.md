# Delver Contractors & Archetype Specifications

This document catalogs the 3 specialized subterranean contractor archetypes in **Voidfall Dredge**, including their 3D suit models, baseline performance attributes, unique traits, and equipment loadouts.

---

## Master Roster Visual Matrix

![Voidfall Dredge 3D Model Showcase](../../docs/models/models_roster_showcase.jpg)

---

## 1. Demolitionist — Kaelen

![Kaelen Demolitionist](../../docs/models/character_demolitionist_kaelen.png)

- **Role**: Heavy Breacher // Volatile Mineral Specialist
- **Visual Model & Aesthetics**:
  - Charcoal ballistic pressure weave (`RGB(0.18, 0.20, 0.24)`).
  - Industrial hazard amber primary accents (`RGB(0.95, 0.55, 0.05)`).
  - Amber glowing blast visor (`RGB(1.0, 0.65, 0.10)`, Emissive 5.0).
  - Angled blast deflector pauldrons.
- **Specifications**:
  - **Base Health**: 100 HP
  - **Excavation Speed**: 1.40x (Drill cuts soft and medium rock significantly faster)
  - **Movement Speed**: 1.00x
  - **Primary Combat Firearm**: **Magma Scattergun** (`[2]` or `[X]`)
    - *Capacity*: 6-round revolving drum (manual reload)
    - *Fire Mode*: 5-flechette thermite buckshot cone (37.5 max burst dmg, 38 m/s, amber tracer)
    - *Tactical Role*: Close-quarters predator breaker with heavy concussive knockback.
  - **Tactical Ability [C]**: **Concussion Shockwave** (18.0s recharge; omni-directional seismic disruption pulse that stuns nearby stalkers/burrowers and shatters loose ceiling rubble)
  - **Breaching Ordnance [4]**: **Deployable Satchel Charge & Remote Detonator** (plants physical C-4/voidite demolition block onto voxel faces; remote radio detonator triggers remote detonation to blast open Reinforced Vault Doors or blow giant tunnel cavities)
  - **Passive Trait**: *Surgical Blast & Volatile Refining* (+25% extra yield from Volatile Quartz clusters; directional demolition charges avoid collateral damage to fragile minerals).

---

## 2. Vanguard — Rhodes

![Rhodes Vanguard](../../docs/models/character_vanguard_rhodes.png)

- **Role**: Armored Stabilizer // Cave-in Defense Fortress
- **Visual Model & Aesthetics**:
  - Dark alloy armored suit plates with cyan tectonic accents (`RGB(0.20, 0.85, 1.00)`).
  - Heavy ballistic reinforced faceplate with narrow slit optics visor (`RGB(0.20, 0.90, 1.00)`, Emissive 5.0).
  - Double-layered fortress shoulder pauldrons and magnetic anchor boots.
- **Specifications**:
  - **Base Health**: 135 HP
  - **Excavation Speed**: 1.00x
  - **Movement Speed**: 0.88x (Heavy armor trade-off)
  - **Max Placed Bulkheads**: 12 (Industry standard support brackets)
  - **Primary Combat Firearm**: **Plasma Carbine** (`[2]` or `[X]`)
    - *Capacity*: 16-round capacitor battery (manual reload required; zero passive trickle)
    - *Fire Mode*: Rapid-pulse coherent plasma bolt (22 dmg, 52 m/s, electric-cyan tracer)
    - *Tactical Role*: Sustained mid-range suppression against charging Stalkers and Burrowers.
  - **Tactical Ability**: Kinetic Repulsor Field (20.0s recharge; omni-directional pulse repelling enemies with knockback and stun, deflecting falling debris, and restoring 25 HP suit integrity)
  - **Passive Trait**: *Tectonic Bulkhead Plating* (50% passive damage reduction from falling roof collapse debris; placed industrial bulkheads deflect ceiling cave-in boulders).

---

## 3. Scout — Vesper

![Vesper Scout](../../docs/models/character_scout_vesper.png)

- **Role**: Acoustic Pathfinder // High-Mobility Reconnaissance
- **Visual Model & Aesthetics**:
  - Lightweight aerodynamic composite weave with emerald survey accents (`RGB(0.15, 1.00, 0.45)`).
  - Curved panoramic reconnaissance visor with high-luminance survey optics (`RGB(0.15, 1.00, 0.45)`, Emissive 5.0).
  - Twin vector thruster nozzles mounted on Exo-Pack; wrist-mounted sonar emitter and grapple winch spool.
- **Specifications**:
  - **Base Health**: 80 HP (High-mobility lightweight rig trade-off)
  - **Excavation Speed**: 1.00x
  - **Movement Speed**: 1.20x (+20% sprint/walk velocity)
  - **Grapple Pull Speed**: 1.50x
  - **Sonar Scan Sphere**: 22-meter radius (vs. 14m default)
  - **Primary Combat Firearm**: **Needler Railgun** (`[2]` or `[X]`)
    - *Capacity*: 8-round needle cartridge (manual reload)
    - *Fire Mode*: Hyper-velocity electromagnetic needle (42 dmg, 110 m/s, zero spread, emerald tracer)
    - *Tactical Role*: Long-distance cavern sniper; neutralizes ceiling ambush predators before they descend.
  - **Tactical Ability**: Seismic Sonar Overdrive (12.0s recharge; emits 360-degree sonic shockwave that stuns Stalkers for 3.0s and reveals all mineral veins through solid rock)
  - **Passive Trait**: *Acoustic Resonance & High-Tension Winch* (Extended scan outline linger, rapid grapple ascension to escape ambushes).

---

## 4. Defensive Quick Melee & Viewmodel Weapon Animations (`[V]` or `[Middle Click]`)

All delver classes possess a defensive close-quarters melee shove ($0.8\,\text{s}$ cooldown, $2.5\,\text{m}$ conical reach, $15$ damage, interrupt knockback, and $0.6\,\text{s}$ stalker stun). Each weapon slot features a bespoke 4-phase kinetic first-person viewmodel animation:

| Weapon Category | Animation Style | Kinematics & Mechanics | Visual Feedback |
| :--- | :--- | :--- | :--- |
| **Gun (`CombatWeapon`)** | Tactical Butt-Stroke & Horizontal Slash | - Phase 1 ($0.0-0.22$): Coils back to shoulder ($Z+0.07\,\text{m}$, roll $+15^\circ$, yaw $+20^\circ$)<br>- Phase 2 ($0.22-0.45$): Explosive forward butt smash ($Z-0.25\,\text{m}$, horizontal sweep $X-0.09\,\text{m}$, roll $-22^\circ$, pitch down $-16^\circ$)<br>- Phase 3 ($0.45-0.58$): $38\,\text{Hz}$ metal-on-chitin impact vibration<br>- Phase 4 ($0.58-1.0$): Fluid quadratic recovery to ready stance | - Demolitionist Scattergun: Wider horizontal bludgeon ($X-0.12\,\text{m}$, roll $-28^\circ$)<br>- Scout Railgun: Long bayonet lunge ($Z-0.29\,\text{m}$)<br>- Vanguard Carbine: High-speed tactical rifle strike<br>- Capacitor emissive pulse on contact |
| **Demo (`DemolitionCharge`)** | Gauntlet Clacker Hammer-Fist Punch | - Phase 1 ($0.0-0.20$): Braced coil tight to chest like a brass-knuckle fist ($Z+0.08\,\text{m}$, pitch $+16^\circ$)<br>- Phase 2 ($0.20-0.42$): Forward piston punch straight into crosshair ($Z-0.26\,\text{m}$, $X-0.12\,\text{m}$, roll $+22^\circ$)<br>- Phase 3 ($0.42-0.58$): Elastic rebound ($+0.055\,\text{m}$) and settling shake<br>- Phase 4 ($0.58-1.0$): Settle back to handheld inspection stance | - Tactile detonator plunger depresses inward under force ($-0.022\,\text{m}$)<br>- Safety beacon flares with a high-intensity shock flash ($+0.70$ emissive) on impact |
| **Mining Drill (`MiningDrill`)** | Two-Handed Hydraulic Battering Ram | - Phase 1 ($0.0-0.24$): Low hip-braced wind-up ($Z+0.11\,\text{m}$, $Y-0.06\,\text{m}$, pitch $-14^\circ$), pistons retract ($-0.04\,\text{m}$), auger motor RPM spools up ($+4200\,\text{deg/s}$)<br>- Phase 2 ($0.24-0.48$): Devastating hydraulic battering ram ($Z-0.30\,\text{m}$, $Y+0.075\,\text{m}$ uppercut heave, pitch $+18^\circ$ down-plunge), dual pistons slam forward to full extension ($+0.08\,\text{m}$), auger RPM peaks ($+6500\,\text{deg/s}$)<br>- Phase 3 ($0.48-0.68$): $42\,\text{Hz}$ heavy machinery shudder and pneumatic exhaust vent<br>- Phase 4 ($0.68-1.0$): Heavy mechanical settle back to waist hold | - Superheated diamond auger emissive flare ($+0.75$ glow)<br>- Pneumatic dual pistons visibly surge and extend<br>- Directional impact spark discharge at peak extension |

---

## Model Reference Generation

All character and enemy reference captures are maintained in [`docs/models/`](file:///d:/Projects/voxel_3d_voidfall_dredge/docs/models/) and regenerated automatically via:

```bash
python scripts/capture_models.py
```
