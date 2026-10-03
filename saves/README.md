# Player Save Profiles (`saves/`)

This directory stores persistent player profile data, currency banks, upgrade investments, and sector completion records.

---

## 📄 File: `save_data.json`

The default profile is saved in [`saves/save_data.json`](file:///d:/Projects/voxel_3d_voidfall_dredge/saves/save_data.json).

### JSON Schema

```json
{
  "save_version": 1,
  "player_name": "DELVER-01",
  "last_saved_time": "2026-10-02 02:00:15",
  "total_exp": 300,
  "total_voidite": 12,
  "total_titanium": 8,
  "selected_class_id": 0,
  "upgrades": {
    "drill_speed": 1,
    "drill_durability": 0,
    "thruster_tank": 1,
    "kinetic_dynamo": 0,
    "sonar_frequency": 0,
    "reinforced_plating": 0
  },
  "sector_records": [
    { "sector": 1, "completion_rate": 85, "badge": "GOLD" },
    { "sector": 2, "completion_rate": 0, "badge": "UNEXPLORED" },
    { "sector": 3, "completion_rate": 0, "badge": "UNEXPLORED" }
  ]
}
```

---

## 💾 Serialization Rules & Safety

1. Managed by [`Voidfall::SaveSystem`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/core/save_system.hpp#L52).
2. Saved upon completing an expedition debrief, purchasing upgrades at the Orbital Hub terminal, or switching classes.
3. Automatically creates parent directories if missing.
4. Schema migrations are keyed off `save_version`.
