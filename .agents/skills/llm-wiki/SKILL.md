---
name: llm-wiki
description: >-
  Build, query, update, validate, and maintain the Voidfall Dredge markdown game knowledge base (wiki/).
  Use when documenting game mechanics, enemy behaviors, voxel materials, balance changes,
  or validating internal documentation links.
---

# Game Design & Lore Knowledge Base (Wiki)

This skill governs the creation and maintenance of the Voidfall Dredge living game design wiki in [wiki/](file:///d:/Projects/voxel_3d_voidfall_dredge/wiki/).

## 1. Quick Link Validation
Validate that all internal markdown links resolve to existing files:
- Helper Script: [scripts/validate-wiki-links.ps1](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/llm-wiki/scripts/validate-wiki-links.ps1)

```powershell
powershell -ExecutionPolicy Bypass -File .agents/skills/llm-wiki/scripts/validate-wiki-links.ps1
```

---

## 2. Wiki Conventions & Structure
For directory layout (`wiki/index.md`, `wiki/log.md`, `wiki/entities/`, `wiki/mechanics/`) and linking rules:
- Reference: [references/wiki-structure.md](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/llm-wiki/references/wiki-structure.md)

---

## 3. Ingesting Feedback & Balance Changes
For converting playtest logs, crash reports, or weapon feedback into durable documentation:
- Reference: [references/source-ingestion.md](file:///d:/Projects/voxel_3d_voidfall_dredge/.agents/skills/llm-wiki/references/source-ingestion.md)
