---
name: game-design-and-balancing
description: >-
  Use this skill when designing or balancing enemies, hazard pacing, level generation, 
  voxel drop rates, weapon tuning, or progression economics in Voidfall Dredge.
---

# Game Design & Balancing

This skill establishes the design rules, enemy behaviors, hazard pacing, and progression economy for Voidfall Dredge.

## 1. Quick Invariant & Simulation Check
To simulate player progression, EXP burn rates, and verify upgrade costs against engine invariants:
- Helper Script: [scripts/simulate_economy.py](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/game-design-and-balancing/scripts/simulate_economy.py)

```powershell
python .agents/skills/game-design-and-balancing/scripts/simulate_economy.py
```

---

## 2. Enemy & Threat Design
For threat taxonomy (Void Stalkers, Seismic Burrowers, Volatile Leeches) and 3D voxel A* navigation:
- Reference: [references/enemy_design_and_behaviors.md](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/game-design-and-balancing/references/enemy_design_and_behaviors.md)

---

## 3. Hazard Clock & Extraction Pacing
For the 15-minute 4-phase expedition escalation curve and 40-second beacon defense parameters:
- Reference: [references/hazard_and_extraction_pacing.md](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/game-design-and-balancing/references/hazard_and_extraction_pacing.md)

---

## 4. Economy, EXP Costs & Voxel Yields
For the exponential upgrade formula ($100 \times 1.6^{\text{tier}-1}$), voidite costs, 85% respec refund rules, and voxel drop tables:
- Reference: [references/economy_and_progression_math.md](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/game-design-and-balancing/references/economy_and_progression_math.md)
