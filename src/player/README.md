# Player & Progression Subsystem (`src/player/`)

The `player` subsystem implements the first-person delver controller, exo-suit physics, tension-cable grappling hook, class archetypes, progression upgrades, and expedition inventory tracking.

---

## 📁 Source Files

| File | Primary Responsibility | Key Classes / Structs |
| :--- | :--- | :--- |
| [`controller.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.cpp) | First-person movement, AABB voxel collision, thruster hovering, tension-cable grappling hook. | [`PlayerController`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.hpp#L32), [`GrappleHook`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.hpp#L13), [`ExoStatus`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/controller.hpp#L22) |
| [`character_class.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/character_class.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/character_class.cpp) | Delver archetypes, baseline attribute multipliers, viewmodel suit palette customization. | [`CharacterClass`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/character_class.hpp#L7), [`CharacterAttributes`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/character_class.hpp#L16) |
| [`upgrades.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/upgrades.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/upgrades.cpp) | 6-branch upgrade tree, exponential cost scaling, purchasing logic, and respec refund solver. | [`UpgradeTree`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/upgrades.hpp#L26), [`UpgradeType`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/upgrades.hpp#L7), [`UpgradeInfo`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/upgrades.hpp#L17) |
| [`loadout.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/loadout.hpp) | Active tool slots, minerals, demolition charges, bulkheads, sector records, mission badges. | [`PlayerInventory`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/loadout.hpp#L26), [`ToolSlot`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/loadout.hpp#L7), [`SectorRecord`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/loadout.hpp#L21) |

---

## 🎖️ Delver Class Archetypes

The game features three distinct classes:

| Class | Specialization | Key Attributes | Trait |
| :--- | :--- | :--- | :--- |
| **Demolitionist** | Heavy excavation & breaching | `baseMineSpeed: 1.35x`, `maxBulkheads: 15`, `fallingDamageReduction: 30%` | *Kinetic Dampening:* Significant damage mitigation from falling ceiling rocks. |
| **Vanguard** | Heavy armored exploration & defense | `suitIntegrity: 150 HP`, `fallingDamageReduction: 50%`, `maxBulkheads: 20` | *Fortified Frame:* Heavy reinforced armor plates withstand cave-in collapses. |
| **Scout** | Rapid mobility & surveying | `moveSpeed: 1.25x`, `grapplePullSpeed: 1.5x`, `sonarRadius: 22m`, `scanLinger: +1.5s` | *Acoustic Array:* Expanded seismic scanning range with longer highlight linger. |

---

## 🛠️ Upgrade Matrix & Progression Economics

Each upgrade path maxes out at **Tier 5**:

1. **Drill Speed**: $+12\%$ excavation speed per tier.
2. **Drill Durability**: $-15\%$ heat buildup and faster cooldown per tier.
3. **Thruster Tank**: $+20\%$ jetpack hover fuel capacity per tier.
4. **Kinetic Dynamo**: Sprinting and falling recharges thruster fuel $+15\%$ faster per tier.
5. **Sonar Frequency**: $+2\text{m}$ sonar pulse radius and $-0.5\text{s}$ cooldown per tier.
6. **Reinforced Plating**: $+15\text{ HP}$ max integrity and $-10\%$ cave-in damage per tier.

### Cost Formulas
- **EXP Cost**: $100 \times 1.6^{\text{current\_tier}}$ (Tier 0 $\to$ 1 = 100 EXP, Tier 1 $\to$ 2 = 160 EXP, etc.)
- **Voidite Cost**: $3 \times (\text{current\_tier} + 1)$
- **Titanium Cost**: $2 \times (\text{current\_tier} + 1)$
- **Respec**: 85% refund of all invested EXP (`std::round(total_spent * 0.85f)`) via [`UpgradeTree::respec()`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/player/upgrades.cpp#L90).

---

## 🧗 Grapple Hook & Thruster Physics

- **Tension Cable**: Raycast finds anchor on solid rock within $45\text{m}$. Simulates damped spring tension physics pulling the player toward the anchor point.
- **Thruster Jetpack**: Hold `SPACE` to engage hover thrusters against gravity, consuming exo-suit power while keeping heat under the critical threshold.
