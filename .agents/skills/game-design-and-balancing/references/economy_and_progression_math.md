# Progression Economics, EXP Curves & Voxel Yields

This reference defines the mathematical models governing character upgrades, player levels, coin rewards, and respec costs.

---

## 1. Dual Economy: Player Level & Coins

The progression system separates **Player Level (Rank)** from **Upgrade Purchasing Currency**:

1. **Player Rank & EXP**:
   - Total accumulated EXP advances the player's account level.
   - EXP is **not spent** on upgrades; it serves as a persistent progression threshold.
   - Advancing in level grants access to higher upgrade tiers, new sectors, and delver classes.
   - Leveling up awards a **Level-Up Coin Bonus** (+150 Coins per rank gained).

| Delver Rank | Required Cumulative EXP | Unlocks |
| :--- | :--- | :--- |
| **Level 1** | 0 EXP | Sector 1 (Perimeter Drift), Demolitionist Archetype, Tier 1 Upgrades |
| **Level 2** | 300 EXP | Sector 2 (Volatile Fault), Vanguard Archetype, Tier 2 Upgrades |
| **Level 3** | 700 EXP | Scout Archetype, Tier 3 Upgrades |
| **Level 4** | 1,300 EXP | Sector 3 (Void Cradle), Tier 4 Upgrades |
| **Level 5** | 2,200 EXP | Tier 5 Master Upgrades |

2. **Upgrade Purchasing (Coins & Mineral Cores)**:
   - Upgrades consume **Coins** (earned from completing expeditions and level-up bonuses), **Voidite**, and **Titanium**.
   - Node costs scale exponentially:

$$\text{CoinCost}(\text{tier}) = \text{round}(100 \times 1.6^{\text{tier}-1})$$

### Invariant Test Table (Verified in `test_progression.cpp`)
| Upgrade Tier | Required Player Level | Required Coins | Required Voidite | Required Titanium |
| :--- | :--- | :--- | :--- | :--- |
| **Tier 1** | Level 1 | 100 Coins | 4 Voidite | 3 Titanium |
| **Tier 2** | Level 2 | 160 Coins | 8 Voidite | 6 Titanium |
| **Tier 3** | Level 3 | 256 Coins | 12 Voidite | 9 Titanium |
| **Tier 4** | Level 4 | 410 Coins | 16 Voidite | 12 Titanium |
| **Tier 5** | Level 5 | 656 Coins | 20 Voidite | 15 Titanium |

---

## 2. Respec & Economy Rules
- **Respec Refund Rate**: 85% of total invested Coins refunded on reset.
- **Class Swapping**: Freely switchable in the Orbital Hub once unlocked.
- **Archetype Perk Bonuses**:
  - **Demolitionist** (Unlocked at Lv 1): +25% Bonus ore yield from explosive mining, +40% Blast radius.
  - **Vanguard** (Unlocked at Lv 2): +50% Debris damage mitigation, +20% Exosuit armor plating.
  - **Scout** (Unlocked at Lv 3): +30% Sprint velocity, +50% Grappling hook range and reel speed.

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
