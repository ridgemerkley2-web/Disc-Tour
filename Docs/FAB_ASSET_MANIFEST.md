# Final Free Fab Environment Asset Manifest

## Policy

This manifest is the acquisition and integration plan for `DA_TemperateMountainForest_Assets`. It is not a redistribution list and it does not grant or infer a license. The project owner must add each product through Fab, record the license shown at acquisition time, acquisition date, account/organization, product version, and final Unreal content root, then retain that evidence with the project provenance records.

The listing identity and technical claims below were checked against the live Fab pages on 2026-08-13. “Listed free” records only the price state visible on that date. It is not a substitute for the license accepted by the owner.

No vendor content path becomes a permanent C++ or gameplay dependency. Imported packs should live under configurable vendor roots, and approved meshes/materials are copied or referenced through the project-owned environment data assets.

## Planned stack

| Publisher | Listing | Fab listing ID | Intended slot/use | Expected project destination | Provenance | Import | Collision | Wind | LOD / Nanite |
|---|---|---|---|---|---|---|---|---|---|
| Project Nature | [temperate Vegetation: Spruce Forest](https://www.fab.com/listings/f8044501-17a2-498f-b198-5f1bc71ee87a) | `f8044501-17a2-498f-b198-5f1bc71ee87a` | TreeConiferLarge, TreeConiferMedium, TreeConiferYoung | Planned project-owned integration: `/Game/Environment/Vendors/ProjectNature/SpruceForest`; actual vendor import root: `/Game/PN_interactiveSpruceForest` | Listing verified; listed free; owner license receipt pending | Imported through the in-editor Fab flow; 363 files found and 361 assets scanned for plugin dependencies; vendor demo/mannequin content is present and excluded from proposals; UE 5.8.1 Editor build and automated checks passed, while rendered acceptance remains pending | Advisory inspection found one simple collision shape on surfaced visual meshes, but candidate scanning cannot approve that as the required dedicated gameplay proxy; prepare project-owned trunk/major-branch proxies and keep the canopy nonblocking | Automated inspection detected no exposed scalar/vector parameter containing `Wind` or `Gust`; preserve the vendor implementation and adapt it non-destructively to `MPC_EnvironmentWind` after material review | Surfaced meshes report 4-5 LODs, 2-4 material slots, and Nanite disabled; bounds-based ecological ranking is approval-gated, and visual/performance acceptance remains pending |
| Quixel Megascans | [European Beech](https://www.fab.com/listings/d11cc01d-9422-41b7-950f-416c9ce79caf) | `d11cc01d-9422-41b7-950f-416c9ce79caf` | TreeDeciduousLarge, TreeDeciduousMedium, selected young deciduous variants | `/Game/Environment/Vendors/Megascans/EuropeanBeech` | Listing verified; listed free; owner license receipt pending | Not imported | Inspect supplied collision, but create/approve dedicated trunk/major-branch proxies for disc play | Listing reports simple and Pivot Painter wind; bridge by instance/wrapper | Listing reports unique LODs and shader tiers; inspect Nanite per mesh |
| Project Nature | [Temperate Vegetation: conifer Bushes & Saplings I](https://www.fab.com/listings/b2b5e215-1641-4f46-a9a7-3526ebd56d60) | `b2b5e215-1641-4f46-a9a7-3526ebd56d60` | Sapling, Shrub, additional TreeConiferYoung | `/Game/Environment/Vendors/ProjectNature/ConiferBushesSaplings01` | Listing verified; listed free; owner license receipt pending | Not imported | Saplings/shrubs use overlap interaction; disable blocking visual collision | Vendor advertises shader wind; MPC adaptation pending | Inspect LOD count and masked overdraw after import; Nanite requires asset-specific review |
| Project Nature | [Temperate Vegetation: Fern Collection](https://www.fab.com/listings/b778bd8f-524c-42b6-b60c-4caac59029c1) | `b778bd8f-524c-42b6-b60c-4caac59029c1` | Fern | `/Game/Environment/Vendors/ProjectNature/FernCollection` | Listing verified; listed free; owner license receipt pending | Not imported | Flying-disc collision must be disabled | Vendor advertises shader wind; MPC adaptation pending | Inspect LODs/overdraw; ordinary fern cards should not be blindly converted to Nanite |
| Project Nature | [temperate Vegetation: Foliage Collection](https://www.fab.com/listings/6a5ae8db-d80f-4b23-b276-87da390cfe56) | `6a5ae8db-d80f-4b23-b276-87da390cfe56` | Grass, GroundCover, weeds, meadow and rough vegetation | `/Game/Environment/Vendors/ProjectNature/FoliageCollection` | Listing verified; listed free; owner license receipt pending | Not imported | Flying-disc collision and blocking overlaps must be disabled | Vendor interaction exists; shared foliage wind adaptation requires inspection | Review each cluster for LOD, overdraw and culling; do not blindly enable Nanite |
| Project Nature | [Forest Landscape Materials Vol.I](https://www.fab.com/listings/92c16f34-474e-4750-9ad2-a02b1aa9beb2) | `92c16f34-474e-4750-9ad2-a02b1aa9beb2` | ForestFloor, conifer dirt/needles and woodland transitions | `/Game/Environment/Vendors/ProjectNature/ForestLandscapeMaterials01` | Listing verified; listed free; owner license receipt pending | Not imported | Material only; existing typed surfaces remain gameplay authority | Not applicable | Profile displacement method and texture residency; use instances/layers, not destructive source edits |
| Shadowmire Studios | [Environment - Rock Collection 04 (Free)](https://www.fab.com/listings/a51e61ac-98fa-4c54-ab23-fc533687afb7) | `a51e61ac-98fa-4c54-ab23-fc533687afb7` | RockSmall, RockLarge, boulders | `/Game/Environment/Vendors/Shadowmire/RockCollection04` | Listing verified; title says Free; owner license selection/receipt pending | Not imported | Create simple/convex gameplay proxies; reject complex-as-simple by default | Not applicable | Listing explicitly says no LODs and reports 9.9k–24.9k tris; mandatory authored LOD or validated Nanite review |
| tharlevfx | [Water Materials](https://www.fab.com/listings/063155ea-d9d2-4f29-b09f-33270b0bc861) | `063155ea-d9d2-4f29-b09f-33270b0bc861` | CreekWater, small streams, future pond/water hazards | Planned project-owned integration: `/Game/Environment/Vendors/tharlevfx/WaterMaterials`; actual vendor import root: `/Game/WaterMaterials` | Listing verified; listed free; owner license receipt pending | Imported through the in-editor Fab compatibility flow using the 5.7 package; 107 files found and 106 assets scanned for plugin dependencies; UE 5.8.1 Editor build and automated checks passed, while rendered acceptance remains pending | Manual integration only: visual materials are intentionally excluded from static-mesh proposals; water hazard, lie, and collision remain authoritative course data | Not applicable | Create project-owned instances/wrappers, then review UE 5.8 shader behavior, rendering cost, and visual quality in Editor |
| GreenBugGames | [Stump Scanned](https://www.fab.com/listings/5f433961-8d90-49ab-a991-e4c7a78acccf) | `5f433961-8d90-49ab-a991-e4c7a78acccf` | Optional Stump and deadwood detail | Planned project-owned integration: `/Game/Environment/Vendors/GreenBugGames/StumpScanned`; actual vendor import root: `/Game/Stump_Scanned` | Listing verified; listed free; owner license receipt pending | Imported through the in-editor Fab compatibility flow using the 5.7 package; 63 files found and 62 assets scanned for plugin dependencies; UE 5.8.1 Editor build and automated checks passed, while rendered acceptance remains pending | Advisory inspection found one simple collision shape on surfaced meshes, but candidate scanning cannot approve it as the dedicated solid gameplay proxy; inspect, simplify if necessary, and explicitly approve a project-owned proxy | Not applicable | Surfaced meshes report 4-5 LODs, one material slot, and Nanite disabled; static LOD/material review passed, while collision, performance, scale, and visual acceptance remain pending |
| Existing project CC0 set | Poly Haven Pine Ridge provisional collection | Project provenance: `SourceArt/PineRidge/PolyHaven/asset_manifest.json` | Temporary Log, ForestDebris, LeafLitter and any still-empty slot | Existing `/Game/Presentation/Course/PineRidge` assets or project-owned derived environment assets | Existing manifest/checksum evidence; no new claim made here | Imported where current receipt confirms | Existing visual assets remain collision-free; sealed course proxies own gameplay contact | Existing generated grass has wind; final MPC wrapper pending | Existing fir/boulder LOD chains remain provisional and performance-tested |

## Verified local import facts

- `temperate Vegetation: Spruce Forest` is present under `/Game/PN_interactiveSpruceForest`. The in-editor Fab flow found 363 files and scanned 361 assets for plugin dependencies. The imported root includes vendor demo/mannequin content; those assets are not environment-slot candidates and must not be bound.
- `Water Materials` is present under `/Game/WaterMaterials`. The in-editor Fab flow selected the 5.7 compatibility package for the UE 5.8 project, found 107 files, and scanned 106 assets for plugin dependencies.
- `Stump Scanned` is present under `/Game/Stump_Scanned`. The in-editor Fab flow selected the 5.7 compatibility package for the UE 5.8 project, found 63 files, and scanned 62 assets for plugin dependencies.

These facts confirm only local package import and dependency discovery. They do not establish release provenance, license-receipt retention, UE 5.8 rendering compatibility, visual acceptance, gameplay collision suitability, approved environment-slot bindings, or performance acceptance. Those states remain pending until their respective reviews are completed.

## Post-import acceptance snapshot

The 2026-08-13 post-import engineering pass established the following facts without approving or binding any marketplace asset:

- `DiscGolfTourEditor` Win64 Development built successfully with Unreal Engine 5.8.1 after the three local imports.
- The focused `DiscGolfTour.Environment.` automation filter passed 15/15 tests with zero failures. Evidence: `Saved/Logs/Automation_Environment_FabIntegration.log`.
- The complete `DiscGolfTour.` automation filter passed 99/99 tests with zero failures. Evidence: `Saved/Logs/Automation_PostFab_Full.log`.
- Scan/proposal evidence is preserved separately at `Saved/Developer/EnvironmentAssetBindingCandidates.json`.
- Assigned-slot readiness evidence is preserved at `Saved/Developer/EnvironmentAssetBindingReport.json`; it reports `readiness_evaluated=true`, `structurally_complete=true`, and `production_ready=false` because final slots and project-owned proxies are not yet approved.
- Both reports record `automatic_apply_performed=false`, and the workflow log records `applied=false`. No proposed Fab candidate was written into `DA_TemperateMountainForest_Assets`.
- `/Game/WaterMaterials` is a manual-integration source. It is intentionally excluded from static-mesh proposals and must be adapted through reviewed project-owned material instances, wrappers, or functions.

These automated results validate the integration architecture, report separation, safety gates, and existing gameplay regressions. They are not visual, shader, collision-proxy, wind-adaptation, or performance approval for the imported art.

**FINAL ENVIRONMENT VISUAL ACCEPTANCE — PENDING REMAINING FAB IMPORTS AND UNREAL EDITOR REVIEW.**

## Required acquisition record

For each Fab product, add this record only after the owner imports it:

```text
listing_id:
listing_url:
publisher:
listing_name:
acquired_utc:
license_name_shown_at_acquisition:
license_or_order_receipt_location:
imported_product_version:
unreal_engine_version:
actual_content_root:
integrator:
```

Never enter a guessed license name. If the receipt cannot be recovered, the provenance state remains pending and the asset is not accepted for a release build.

## Binding workflow

1. Add products through the owner’s Fab/Unreal workflow; do not copy downloaded packages into scripts or source control without license review.
2. Keep each product under its configured `/Game/Environment/Vendors/...` root where practical.
3. In Editor, call `DiscGolfEnvironmentAssetBinder.ScanEnvironmentAssets` or `ProposeBindings` with reviewed mesh roots and `DA_TemperateMountainForest_Assets`. The non-mutating runner keeps the planned `/Game/Environment/Vendors/...` destinations configured, adds the verified imported mesh roots `/Game/PN_interactiveSpruceForest` and `/Game/Stump_Scanned`, de-duplicates them case-insensitively, and scans only roots that currently exist. `/Game/WaterMaterials` and the planned water/forest-floor material roots are logged as manual-integration sources and intentionally excluded from static-mesh proposals, so demonstration river rocks and water planes cannot be mistaken for gameplay environment slots. Additional reviewed mesh roots may be supplied as a semicolon-separated `DISC_GOLF_ENVIRONMENT_VENDOR_ROOTS` environment variable; invalid or missing package roots are reported and never applied.
4. Inspect the advisory candidates in `Saved/Developer/EnvironmentAssetBindingCandidates.json`. Scanning/proposal never mutates the data asset, and later readiness validation does not overwrite this evidence.
5. Fix collision, LOD/Nanite, wind, scale, bounds and material-count issues in project-owned derivatives or instances.
6. Supply only human-approved mesh paths to `ApplyApprovedBindings`. Reapproving an existing visual mesh preserves its weight, scale range, collision proxy, interaction proxy, and reviewed Nanite flag. The supplied list remains the explicit replacement selection for that category; a persistent data asset is saved immediately, and a failed save restores the prior in-memory slot.
7. Rerun `ValidateEnvironmentAssetReadiness` and inspect `Saved/Developer/EnvironmentAssetBindingReport.json`, then run the PCG generator, environment automation, Hole 1 route checks, fixture QA and performance capture. Readiness evaluates the complete assigned variant: blocking slots require a separate simple `CollisionProxyMesh`, overlap slots require `InteractionProxyMesh`, and grass/fern/nonblocking foliage still rejects embedded blocking collision. `report_generated` confirms only that inspection completed; `production_ready` becomes true only when every assigned variant passes the static slot checks. The legacy `ValidateEnvironmentAssets` entry point remains available for existing tooling.

Automation tests write binder artifacts below `Saved/Automation`. An explicit production scan/proposal writes `Saved/Developer/EnvironmentAssetBindingCandidates.json`; assigned-slot validation writes the separate `Saved/Developer/EnvironmentAssetBindingReport.json`. Rerunning `create-production-environment-assets.py` preserves every non-empty slot so approved meshes, collision/interaction proxies, weights, and scale ranges survive starter-graph regeneration.

Candidate classification uses only the asset path below the matched vendor root, so a pack name such as `PN_interactiveSpruceForest` or `Stump_Scanned` cannot classify every mesh in that pack. Demo, example, showcase, map, and mannequin subtrees are excluded from proposals. Tree and rock categories require a matching species/family anchor before stage words can affect ranking. Generic quality/form tokens such as `full`, `half`, `high`, and `low` are deliberately neutral. Conifer candidates then receive a soft, overlapping height-fit score: Young 3–9 m, Medium 8–15 m, and Large 13–60 m. Mature candidates remain `AMBIGUOUS` and require visual approval; bounds never authorize an automatic binding. Thus names such as `rock_small`, `SM_Large_Plane`, or an undersized `spruce_small` cannot enter an inappropriate tree slot merely because of a token in the path. These rules only shape the advisory report; they do not approve or bind an asset.

## Wind adaptation

Do not destructively rewrite vendor masters merely to match this project. Prefer, in order:

1. project-owned material instances when the vendor exposes wind controls;
2. a project-owned wrapper/master that consumes vendor textures/functions;
3. a small project-owned material function that reads `MPC_EnvironmentWind`;
4. a documented derived duplicate only when the first three approaches cannot work.

The bridge consumes `WindDirection`, `WindSpeedMps`, and `GustStrengthMps`. Preserve vendor-specific Pivot Painter or interaction data and convert the shared SI values only inside the project-owned adapter.
