# Hazard Clock & Evacuation Mechanics

Governs the extraction loop and expedition pacing in Voidfall Dredge.

---

## 1. Hazard Progression Loop
The hazard clock begins counting up the moment the player touches down in a sector.
- **Minutes 0:00 - 4:00 (Stable)**: Zero environmental damage. Standard mineral extraction.
- **Minutes 4:00 - 8:00 (Agitation)**: Radiation builds up gradually (+1 rad/sec). Light visual distortion.
- **Minutes 8:00 - 12:00 (Critical)**: Periodic seismic tremors cause unsupported islands to collapse.
- **Minutes 12:00+ (Catastrophic)**: Radiation spikes (+5 rad/sec). Relentless enemy spawns.

---

## 2. Extraction Beacon Defense
- **Beacon Deployment**: Activated by the player when inventory is full or hazard is critical.
- **Holdout Duration**: 40-second survival defense timer.
- **Pod Arrival**: Boarding the pod completes the expedition and securely banks collected resources in [save_data.json](file:///d:/Projects/voxel_3d_voidfall_dredge/save_data.json).
