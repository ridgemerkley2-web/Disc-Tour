# Session 19 Shipping PCG Separation

## Decision

The v0.5 Windows Shipping SKU does not execute or cook Unreal PCG. Its forest
authority is the ordered `trees` arrays in `Data/PineRidgeHole1.json` through
`Data/PineRidgeHole3.json`: 44 placements with canonical combined SHA-256
`98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6`.
Runtime loading remains fail-closed and does not fall back to source-time
procedural generation.

PCG remains an Editor authoring facility. `DiscGolfTourEditor` owns the PCG
module dependency, custom density node, on-demand authoring controller, and
source graph. `DiscGolfTour` owns none of those types or generation entry
points. The PCG plugin targets Editor only, the preset graph reference is
editor-only data, and `/Game/Environment/Forest/PCG` is excluded from cook.
The preserved source graph is not a Shipping runtime dependency.

## Evidence boundary

`Scripts/validate_dg_session19_shipping_pcg_separation.py` proves the source,
module, plugin, configuration, graph identity, and authored JSON boundary. On a
fresh final Windows archive it also inventories IoStore, dumps the cooked asset
registry, scans cooked project assets and Shipping binaries for project-PCG
markers, records exact container/binary identities, and creates a
candidate-bound receipt. `Scripts/run-session19-shipping-candidate.ps1` invokes
both phases fail-closed.

This strategy intentionally does not claim an Unreal PCG generate/save/reopen
bake. Authored JSON is the Shipping authority, so the editor graph is excluded
instead of being treated as live or cooked runtime content. Until a new clean
Shipping candidate passes the archive phase, cooked absence remains pending.
Collision/visual review, three-hole performance and soak acceptance, manual
gameplay/play-feel approval, provenance/legal/distribution approval, Session 13
blocker closure, and release readiness all remain false.
