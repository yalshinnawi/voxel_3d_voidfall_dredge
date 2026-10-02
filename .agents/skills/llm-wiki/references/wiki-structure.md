# Wiki Structure & Conventions

A markdown-based game knowledge base for Voidfall Dredge should follow a traceable, durable structure.

## Directory Layout
```text
wiki/
├── index.md             # Top-level index of all topics, entities, and mechanics
├── log.md               # Append-only chronological changelog of updates and decisions
├── mechanics/           # Game systems (hazard clock, mining, extraction, progression)
├── entities/            # Enemies, classes, voxel materials, items
└── sources/             # Raw balance notes, playtest logs, design proposals
```

## Traceability Rules
1. Every major mechanic claim in `mechanics/` or `entities/` should cite its corresponding C++ implementation or design source.
2. When balance or formulas change, append an entry to `wiki/log.md` detailing what changed, the date, and the rationale.
3. Keep `wiki/index.md` updated with links to newly created pages.
