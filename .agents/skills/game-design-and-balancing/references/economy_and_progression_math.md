# Progression Economics, EXP Curves & Voxel Yields

This reference defines the mathematical models governing character upgrades, material drop rates, and respec costs.

---

## 1. Exponential Upgrade Cost Formula
Character upgrade nodes in [src/player/upgrades.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/upgrades.cpp) follow an exponential scaling model:

$$\text{Cost}(\text{tier}) = \text{round}(100 \times 1.6^{\text{tier}-1})$$

### Invariant Test Table (Verified in `test_progression.cpp`)
| Upgrade Tier | Required EXP | Cumulative EXP |
| :--- | :--- | :--- |
| **Tier 1** | 100 EXP | 100 EXP |
| **Tier 2** | 160 EXP | 260 EXP |
| **Tier 3** | 256 EXP | 516 EXP |
| **Tier 4** | 410 EXP | 926 EXP |
| **Tier 5** | 656 EXP | 1,582 EXP |

---

## 2. Respec & Economy Rules
- **Respec Refund Rate**: 85% of total invested EXP refunded on reset.
- **Class Swapping**: Freely switchable in the Orbital Hub ([src/ui/orbital_hub.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/ui/orbital_hub.cpp)).
- **Archetype Perk Bonuses**:
  - **Vanguard**: +50% Debris damage mitigation, +20% Exosuit armor plating.
  - **Scout**: +30% Sprint velocity, +50% Grappling hook range and reel speed.
  - **Demolitionist**: +25% Bonus ore yield from explosive mining, +40% Blast radius.

---

## 3. Voxel Material Yields & Rarity
Layer indices match [src/graphics/texture_array.cpp](file:///d:/Projects/voxel_3d_voidfall_dredge/src/graphics/texture_array.cpp):

| Voxel ID | Material Name | Hardness (Hits) | EXP Yield | Blast Resistance | Drop Items |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **0** | Air | 0 | 0 | 0 | None |
| **1** | Fractured Granite | 2 hits | 1 EXP | Low | Stone Fragments |
| **2** | Volcanic Basalt | 4 hits | 3 EXP | High | Basalt Slag |
| **3** | Volatile Crystal | 3 hits | 15 EXP | Destabilizes on hit | Volatile Shards (Explosive craft) |
| **4** | Dense Void Ore | 6 hits | 40 EXP | Very High | Void Alloy Ingot |
