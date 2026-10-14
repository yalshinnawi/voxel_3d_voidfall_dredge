# Progression & Economy Architecture

Voidfall Dredge implements a dual-stream progression loop decoupling **Player Level (Delver Rank)** from **Upgrade Purchasing Power (Coins)**.

---

## 1. Core Vision & Progression Loop

Prior versions directly consumed raw EXP to buy skill nodes. The revamped system establishes clear separation of concerns:
- **EXP (Delver Experience)**: Earned through drilling, extracting minerals, surviving hazard intervals, and completing expeditions. EXP is permanently banked to increase **Player Level**. EXP is never spent or lost.
- **Player Level (Delver Rank)**: Gatekeeper for game content. Raising rank unlocks higher skill tiers, deeper asteroid sectors, and advanced delver class archetypes.
- **Coins (Void Credits)**: Standard expedition remuneration currency. Awarded at the end of runs and upon level-up. Spent at the Upgrade Terminal to unlock and advance skill tree nodes.
- **Specialty Minerals (Voidite & Titanium)**: High-tier crafting reagents required alongside Coins for Tier 2+ and Tier 3+ upgrades.

```mermaid
graph TD
    Expedition["Asteroid Expedition"] -->|"Drilling / Hazard Survival / Beacon Evac"| Rewards["Expedition Performance"]
    Rewards -->|"EXP Gain"| DelverRank["Player Level (Delver Rank)"]
    Rewards -->|"Extraction Remuneration"| Coins["Coins (Void Credits)"]
    DelverRank -->|"Level Up (+150 Coins)"| Coins
    DelverRank -->|"Gated Access"| Unlocks["Unlocks: Sectors, Classes & Skill Tiers"]
    Coins -->|"Purchasing Power"| UpgradeTerminal["Upgrade Terminal (Skill Matrix)"]
    Unlocks --> UpgradeTerminal
```

---

## 2. EXP & Delver Rank Thresholds

EXP accumulates cumulatively. Reaching a threshold increments the Delver Rank and instantly triggers a **+150 Coin Level-Up Bonus**.

| Delver Rank | Cumulative EXP Required | Delta EXP to Next | Key Unlocks |
|:---:|:---:|:---:|:---|
| **Level 1** | 0 | 300 | Sector 1 (Shallow Vein), Demolitionist Archetype, Tier 1 Skills |
| **Level 2** | 300 | 400 | Sector 2 (Sunken Core), Vanguard Archetype, Tier 2 Skills |
| **Level 3** | 700 | 600 | Scout Archetype, Tier 3 Skills |
| **Level 4** | 1,300 | 900 | Sector 3 (Abyssal Chasm), Tier 4 Skills |
| **Level 5** | 2,200 | 1,500 | Master Mastery, Tier 5 Skills (Capstone) |
| **Level $N \ge 6$** | $2200 + (N - 5) \times 1500$ | 1,500 | Paragon Bonus Remuneration (+150 Coins per rank) |

---

## 3. Coin Remuneration & Economy

Coins are calculated at run completion in `PlayerInventory::finalize_run`:

$$\text{Run Coins} = \text{Base Remuneration} + (\text{Completion \%} \times 1.0) + (V \times 3) + (T \times 5) + (R \times 100)$$

Where:
- $\text{Base Remuneration} = 150$ on successful extraction ($0$ on squad wipe).
- $V = \text{Voidite extracted}$.
- $T = \text{Titanium extracted}$.
- $R = \text{Relics / Anomalies extracted}$.
- **Abandonment / Emergency Pod**: If an expedition is abandoned before the primary extraction beacon holdout succeeds, total payout is discounted by 50%.

---

---

## 4. Interactive 2D Neural Skill Web (22 Specializations, Food-Web Conduits)

The skill system features a free-panning 2D interactive Neural Web radiating organically from the central **Core Nexus**, replacing legacy 1D column scrollbars with a map-like explorative canvas. The tree contains **22 specialized nodes** organized across 6 interconnected disciplines with a total of **72 points** across the full matrix.

Nodes present minimal high-contrast badges: an iconography tag (`<drill>`, `<hazard>`, `<shield>`, `<flare>`, etc.), a concise 1-2 word title (`RAD SHIELD`, `LAVA MESH`, `BOSS SLAYER`, `FALL DAMPENER`, etc.), a clear buff badge, and compact tier pips `[■] [□] [□]`. Full numeric formulas, milestone perks, and lore reside in the right-hand Inspector Panel upon selecting any node.

```mermaid
graph TD
    Nexus(["CORE NEXUS (Delver Suit)"])
    
    %% Excavation Branch
    Nexus --> Drill["DRILL SURGE (Drill Velocity)"]
    Drill --> Heat["HEAT SINKS (Heat Dissipation)"]
    Heat --> Micro["MICRO BLAST (Ore Yield)"]
    
    %% Traversal Branch
    Nexus --> Fuel["FUEL TANK (Thruster Fuel)"]
    Fuel --> Dynamo["DYNAMO COIL (Fuel Recovery)"]
    Dynamo --> Sonar["SEISMIC EYE (Sonar Survey)"]
    Fuel --> Fall["FALL DAMPENER (Impact Absorber)"]
    
    %% Defense & Hazards Branch
    Nexus --> Armor["ARMOR PLATING (Suit HP & Integrity)"]
    Armor --> Shield["KINETIC SHIELD (Shield Buffer)"]
    Shield --> Defib["SECOND WIND (Defibrillator)"]
    Armor --> Rad["RAD SHIELD (Radiation Resistance)"]
    Armor --> Lava["LAVA MESH (Molten Lava Shield)"]
    
    %% Ballistics & Boss Branch
    Nexus --> Slugs["HYPER SLUGS (Ballistic Damage)"]
    Slugs --> Rapid["RAPID BOLT (Fire Rate & Reload)"]
    Rapid --> Crit["CRIT PIERCER (Weakpoint DMG)"]
    Slugs --> Boss["BOSS SLAYER (Apex Predator DMG)"]
    
    %% Melee Branch
    Nexus --> Ram["PNEUMATIC RAM (Melee Shove)"]
    Ram --> Cleave["CLEAVE WAVE (Shockwave Stun)"]
    Cleave --> Berserk["SIPHON STRIKE (Berserker Overdrive)"]
    
    %% Stealth Branch
    Nexus --> Stealth["SILENT TREAD (Acoustic Dampening)"]
    Stealth --> Veil["SHADOW VEIL (Optical Camo)"]
    Veil --> Ambush["AMBUSH PROTOCOL (Surprise DMG)"]
    
    %% Cross-Branch Food-Web Synergies
    Heat <-->|"Thermal Dissipation"| Lava
    Rad <-->|"Hazard Telemetry"| Sonar
    Fall <-->|"Kinetic Grounding"| Armor
    Boss <-->|"Apex Target Vulnerability"| Crit
    Ram <-->|"Excavation Kinetic Punch"| Drill
    Stealth <-->|"Infiltration Agility"| Fuel
```

### 4.1 Disciplines & Specialized Hazard Adaptations

Each discipline branches into specialized hazard mitigations directly tuned to asteroid challenges (radiation, lava, fall damage, Chitin Goliath boss battles):

| Discipline | Foundation (Max 5) | Specialization (Max 3) | Capstone / Branch B (Max 2-3) |
|---|---|---|---|
| **Excavation** | `Drill Surge` (+12%/tier mining speed) | `Heat Sinks` (-15% heat, +20% cooling) | `Micro Blast` (+20%/tier high-grade ore yield) |
| **Traversal** | `Fuel Tank` (+20%/tier thruster fuel) | `Dynamo Coil` (+15%/tier sprint/fall fuel recovery) | `Seismic Eye` (+2m sonar radius, HUD tags)<br>`Fall Dampener` (-25%/tier fall impact damage, up to 75% mitigation) |
| **Defense & Hazards** | `Armor Plating` (+15 HP, -10% debris damage) | `Kinetic Shield` (+12 Shield, -0.5s recharge delay) | `Second Wind` (Survive lethal damage at 35 HP)<br>`Rad Shield` (-25%/tier radiation exposure & damage, up to 75% mitigation)<br>`Lava Mesh` (-25%/tier molten lava burn damage, up to 75% mitigation) |
| **Ballistics & Apex** | `Hyper Slugs` (+12%/tier ballistic damage) | `Rapid Bolt` (+8% fire rate, +10% reload speed) | `Crit Piercer` (+35%/tier weakpoint headshot damage)<br>`Boss Slayer` (+25%/tier damage against Alpha predators & Chitin Goliath) |
| **Melee** | `Pneumatic Ram` (+30%/tier shove damage & stagger)| `Cleave Wave` (90° cleave arc & shock stun) | `Siphon Strike` (+12 Shield siphon & speed burst) |
| **Stealth** | `Silent Tread` (-20%/tier noise radius) | `Shadow Veil` (-15%/tier optical sightline range) | `Ambush Protocol` (+35%/tier unalerted strike damage) |

### 4.2 Campaign Point Cap (28 Pts) vs. Post-Game Grandmaster Matrix (72 Pts)

To preserve tactical tension, build diversity, and specialization:
- **Campaign Point Cap (28 Points Max)**: Prior to completing Sector 3 (The Void Cradle), players can allocate a maximum of **28 points** (out of 72). Even with surplus EXP, additional nodes are locked with `CampaignCapReached` (`NodeWebState::CampaignCapped`).
- **Respec Freedom**: Respec at the Upgrade Terminal refunds **100% of all spent EXP**, resetting assigned points to 0 so players can freely experiment with different hazard adaptations (e.g. respecing into `Lava Mesh` for Sector 4 magma faults or `Boss Slayer` for Alpha Goliath encounters).
- **Post-Game Grandmaster Matrix (72 Points)**: Clearing Sector 3 expands the cap to **72 points**, allowing complete 100% mastery across all 22 nodes.

### 4.3 Exponential EXP Cost Scaling Formula
$$\text{Cost}(\text{tier}) = \left\lfloor 100 \times 1.6^{\text{tier}-1} \right\rfloor$$
- **Tier 1**: 100 EXP
- **Tier 2**: 160 EXP
- **Tier 3**: 256 EXP
- **Tier 4**: 410 EXP
- **Tier 5**: 656 EXP

Upgrades strictly consume Delver EXP. Mining minerals (Voidite & Titanium) and Credits remain untouched.

---

## 5. Sector & Archetype Level Gating

Content access in the Orbital Hub dynamically enforces Player Level constraints:

### Sectors
- **Sector 1 (The Shallow Vein)**: Unlocked at **Level 1** (default). Open rooms, basic basalt, low radiation escalation.
- **Sector 2 (The Sunken Core)**: Unlocked at **Level 2**. Reinforced vault doors, caustic gas pockets, seismic tremor intervals.
- **Sector 3 (The Abyssal Chasm)**: Unlocked at **Level 4**. Radioactive sanctuary cores, vertical abyss drops, lethal Void Stalker ambushes.

### Delver Archetypes
- **Demolitionist**: Unlocked at **Level 1** (Heavy drill, seismic explosive charges).
- **Vanguard**: Unlocked at **Level 2** (Reinforced bulkhead shield, heavy kinetic ram).
- **Scout**: Unlocked at **Level 3** (High-mobility grapple thrusters, broad-spectrum sonar surveyor).
