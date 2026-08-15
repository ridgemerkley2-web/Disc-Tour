# Production Environment / Forest System

## Purpose and authority boundary

This is the production environment architecture for the first realistic course. It keeps the existing fixed-step disc flight, player, throwing, basket, camera, course-rule, and scoring systems intact.

The rule is simple: environmental art is replaceable; competitive physics is explicit.

- Visual trees, grass, ferns, litter, and debris are PCG-created HISM presentation.
- Trunks, major branches, logs, stumps, and rocks use separately prepared collision proxies.
- Canopies and shrubs use nonblocking overlap volumes that reduce disc velocity/spin.
- Grass, ferns, leaf cards, and ordinary ground cover do not stop a flying disc.
- The existing typed course surfaces remain authoritative for lie and penalty resolution.

## Content organization

The Editor generation script creates and preserves this hierarchy:

```text
Content/Environment/
  Forest/
    Trees/
    Shrubs/
    Ferns/
    Grass/
    GroundCover/
    Logs/
    Rocks/
    Materials/
    PCG/
  Water/
  Course/
  Optimization/
```

Generated production assets:

- `/Game/Environment/Forest/DA_TemperateMountainForest`
- `/Game/Environment/Forest/DA_TemperateMountainForest_Assets`
- `/Game/Environment/Forest/PCG/PCG_TemperateMountainForest`
- `/Game/Environment/Forest/Materials/MPC_EnvironmentWind`

`Scripts/create-production-environment-assets.py` is idempotent. It reconstructs the starter PCG graph from the data-asset schema and does not download files.

## Runtime architecture

`ADiscGolfEnvironmentController` owns the course-scale generation bounds, selected `UDiscGolfForestPreset`, quality level, seed, and partitioned on-demand PCG component. It also synchronizes the preset's wind with the existing `AWindDirector` and `MPC_EnvironmentWind`.

`ADiscGolfEnvironmentZoneActor` is the level-design control surface. Use spline corridors for fairways; radial zones for tees and greens; boxes or splines for semi-rough, deep rough, OB, and protected natural areas. Zones are evaluated from low to high priority, so the highest-priority authored intent is applied last. `bHardExclusion` blends to zero density. `BlendFalloffCm` feathers each edge to avoid artificial vegetation walls.

`UPCGDiscGolfZoneDensitySettings` is the reusable PCG node. It:

- evaluates the winning authored zone at every sampled point;
- applies preset density, quality, species mix, and the controller seed;
- rejects points deterministically;
- writes per-category spacing bounds;
- selects a weighted visual mesh from the configured asset-set slot;
- writes `DiscGolfMesh` for the PCG by-attribute spawner;
- never contains a Marketplace/Fab asset path.

`PCG_TemperateMountainForest` contains one ground query/sampler and 16 category branches. Every branch uses Zone Density -> Density Filter -> Self Pruning -> Static Mesh Spawner. The output is collision-free HISM by default. A populated `CollisionProxyMesh` creates a separate hidden physical HISM branch when the graph is regenerated. Runtime PCG generation remains on-demand until final asset bindings pass review; the current CC0 HISM presentation keeps the course playable meanwhile.

The persistent Pine Ridge builder now places one course-scale controller plus six benchmark Hole 1 zones in the existing course world: deep rough, feathered semi-rough, primary fairway, turnover/forehand fairway, tee clear, and green clear. These actors are tagged `Environment.Benchmark.Hole1`; no disconnected demo map is created.

## Zone behavior

The preset ships with these authored defaults:

| Zone | Trees / brush | Ground layer | Intent |
|---|---|---|---|
| Tee | No random trees, shrubs, ferns, or debris | Short grass/dirt only | Clean throw setup and camera space |
| Fairway | Low, controlled density outside setbacks | Short grass, sparse cover | Preserve the authored flight corridor |
| Semi-rough | Young trees, saplings, shrubs, ferns | Longer grass | Recoverable but visibly penalizing |
| Deep rough | Mixed-age/mixed-species trees, deadfall, rock, undergrowth | Heavy cover/litter | Dense, realistic forest |
| Green / circle | No random mature trees; minimal brush | Maintained grass/ground | Clear putting and basket access |
| OB / natural | Configurably denser than deep rough | Heavy natural cover | Strong boundary and habitat reading |

Important settings are exposed in the preset/controller/zone actors: fairway width, tree setback, brush setback, green radius, tee radius, vegetation density, minimum tree spacing, species mix, random seed, quality, wind direction, wind speed, and gust strength.

GroundCover is also the slot for low weeds. Log is the slot for fallen trunks. ForestDebris is for cones, sticks, bark pieces, and small deadfall; LeafLitter is for leaf/needle clusters or cards.

## Free Fab asset hookup

Do not paste asset paths into C++ or PCG nodes. After importing an approved free Fab product, open `DA_TemperateMountainForest_Assets`, find the matching category, add variants, and assign the fields below.

For every variant:

1. `VisualMesh`: the visible production mesh.
2. `Weight`: relative frequency within this category.
3. `UniformScaleRange`: restrained real-world variation.
4. `bNaniteSuitable`: true only after validating the actual asset and target tier.
5. `CollisionProxyMesh`: optional separate simple trunk/branch/solid proxy.
6. `InteractionProxyMesh`: optional canopy or shrub envelope used to size nonblocking drag volumes.

Run `ValidateEnvironmentAssetReadiness` before regeneration. Its assigned-slot report treats `VisualMesh` as presentation, requires a separate simple `CollisionProxyMesh` for blocking slot policies, and requires `InteractionProxyMesh` for overlap policies. Grass, fern, ground-cover, debris, and litter visuals still fail validation if they carry blocking collision. Then save the data asset and rerun `Scripts/create-production-environment-assets.py` from Unreal's Python commandlet or regenerate the graph in Editor. The 16 visual HISM branches update without rewriting logic.

### Exact slots

| Asset-set slot | Planned free-asset use | Current state |
|---|---|---|
| TreeConiferLarge | Mature spruce/fir/pine | Empty |
| TreeConiferMedium | Mid-story spruce/fir/pine | Empty |
| TreeConiferYoung | Young conifers | Provisional existing CC0 fir saplings A/B/C |
| TreeDeciduousLarge | Mature beech/aspen/temperate deciduous | Empty |
| TreeDeciduousMedium | Mid-story beech/aspen/deciduous | Empty |
| Sapling | Small conifer/deciduous regeneration | Empty |
| Shrub | Forest-edge and rough shrubs | Provisional existing CC0 Shrub 04 |
| Fern | Multiple fern forms | Empty |
| Grass | Optimized multi-blade grass clumps | Empty |
| GroundCover | Weeds, flowers, moss, low plants | Empty |
| Log | Fallen trees and large deadfall | Empty |
| Stump | Cut/broken/rotting stumps | Empty |
| RockSmall | Talus and scatter rocks | Empty |
| RockLarge | Exposed boulders/outcrops | Provisional existing CC0 Boulder 01 |
| ForestDebris | Twigs, cones, bark, small deadfall | Empty |
| LeafLitter | Leaf and conifer-needle scatter | Empty |

The three provisional meshes were already present in this repository's documented CC0 Pine Ridge asset set. This system does not make any new claim about Fab licensing. Record the exact Fab listing URL, license shown at acquisition time, acquisition date, creator, and imported product version in the asset manifest before production use.

### Material slots

Open `DA_TemperateMountainForest` and populate `Materials`:

| Preset slot | Planned use | Current state |
|---|---|---|
| ForestFloor | Forest soil/needles/leaves Landscape layer | Provisional Pine Ridge forest-floor material |
| OpenFairway | Maintained grass/fairway Landscape layer | Provisional Pine Ridge fairway material |
| CreekWater | Small creek/river material | Provisional Gallery Lake water; replace or adapt for flowing water |
| FoliageWindMaster | Common wind-capable foliage master | Empty |

`MPC_EnvironmentWind` already contains `WindDirection`, `WindSpeedMps`, and `GustStrengthMps`. `ADiscGolfEnvironmentController::SynchronizeWindDirector` writes those same preset values to the authoritative disc-flight `AWindDirector` and the MPC when Pine Ridge loads. Connect imported foliage through material instances, project-owned wrappers, or a small material function; do not edit vendor masters unless necessary. Preserve a vendor's Pivot Painter and interaction data in the wrapper and convert meters-per-second only there.

## Collision authoring contract

### Trees

- `VisualMesh` must not use its leaves/cards as a solid wall.
- `CollisionProxyMesh` should contain simple convex/capsule-like trunk shapes and only major branches that a disc could credibly hit.
- Tag-driven PCG collision routes tree proxies through the existing tree impact response.
- Do not use complex collision on dense instanced forest meshes as the default.

### Canopy and shrubs

- Use `ADiscGolfVegetationInteractionActor` as a query-only overlap.
- `LightCanopy`, `DenseCanopy`, and `Shrub` profiles expose velocity, spin, and re-entry cooldown.
- Size a species volume from `InteractionProxyMesh`; do not spawn one actor per leaf cluster.
- Generate canopy interaction only near playable corridors/camera-interest areas at scale. Distant canopy stays purely visual.

### Grass and ferns

- Collision disabled, overlaps disabled, navigation disabled.
- They may affect the final lie only through the existing authored surface/rough authority, not visual cards.

### Logs, stumps, and rocks

- Assign separate simple `CollisionProxyMesh` variants.
- Rock proxies use the existing rock response. Tree/wood solids use the existing tree response until a measured wood/deadfall profile is deliberately added and tested.

After collision proxies are assigned, rerun the graph generator and perform the live fixture QA matrix plus direct corridor throws. The current coefficients remain calibration seeds, not measured material claims.

## Level designer workflow

1. Use the one `DiscGolfEnvironmentController` created by the persistent Pine Ridge course builder. For a future authored map, place exactly one controller for the entire property.
2. Assign `DA_TemperateMountainForest`; set bounds for the complete course, not a single hole.
3. Place a Fairway `DiscGolfEnvironmentZoneActor` and edit its spline through the authored route. Set `WidthCm`, `TreeSetbackCm`, and `BrushSetbackCm`.
4. Place radial Tee and Green zones at every tee/basket using the preset radii. Give them higher priority than rough.
5. Add SemiRough/DeepRough/OB zones where the design needs explicit gradients. Uncovered ground defaults to DeepRough.
6. Add hard-exclusion zones for landing zones, spectator lanes, camera positions, signs, bridges, maintenance access, and safety space.
7. Generate at Performance first, inspect corridor/green clearance, then High. Use Cinematic only for capture/hero validation.
8. Run collision and camera checks before accepting a seed. A visually pleasing seed is not acceptable if it blocks authored shot choices.

## Large-course optimization strategy

- Use one persistent 18-hole world. World Partition becomes appropriate when the production Landscape/map is authored; do not split holes into separate gameplay maps.
- Keep the controller's PCG component partitioned and on-demand. Generate/bake stable course foliage in editor when possible.
- Visual foliage uses HISM. Avoid hundreds of individual foliage Actors.
- Nanite is suitable for validated high-poly trunks, logs, rocks, and some opaque/modern foliage assets. Do not blindly enable it on every masked grass/leaf asset.
- Use slot-specific cull start/end distances. Grass, ferns, litter, and debris cull first; mature silhouettes remain longest.
- Keep grass shadows off in Performance and High by default. Restrict deep-canopy dynamic shadows by distance; preserve near-play contact readability.
- Performance scales density to 0.58, culling to 0.72, and shadows to 0.62. High is the 1.0 baseline. Cinematic uses 1.30 density, 1.35 culling, and 1.45 shadow distance.
- All quality changes are presentation-only. Collision proxies, authored fixtures, course surfaces, lie rules, and disc physics must remain invariant.

## TemperateMountainForest art direction

The target is a believable North American mountain/temperate property: conifer-dominant forest with restrained deciduous breaks, open maintained grassy corridors, dense wooded rough, ferns and shrubs in moist/shaded areas, exposed rock, fallen timber, and optional small creek segments. The composition should feel like a professionally maintained championship disc-golf venue presented with PGA-style restraint, while remaining original and not copying proprietary branding/course data.

Species mix defaults are conifer-heavy with lower-frequency deciduous trees and active regeneration. Final species choices must match the chosen fictional region, elevation, slope aspect, moisture, and maintenance story.

## Unreal Editor work still required

- Import approved free Fab assets manually and record their provenance/license evidence.
- Populate the empty mesh and material slots listed above.
- Prepare LOD/Nanite settings and simple collision proxy meshes per source asset.
- Create per-species canopy/shrub interaction-volume templates from the interaction proxy meshes.
- Review the controller and six generated Hole 1 zones in the persistent course world; they are already integrated by the runtime builder.
- Convert/author the final Landscape and enable/configure World Partition for the full 18-hole property when that map exists.
- Profile a representative wooded hole on Performance and High before adopting final cull/shadow values.

## Hole 1 production benchmark

Pine Ridge Opening is par 3 at 361.9 feet. The professional 4.2 m by 1.8 m tee pad points directly into an open launch corridor. Its intended scoring shapes are:

- controlled flat/slight-hyzer backhand through `CenterPlacement`;
- committed stable hyzer toward `SkipShelfAttack`;
- wider turnover/anhyzer or forehand through `RightBailout`, presented as the alternate right window.

The first 120-150 feet remain readable, mature authored trunks create strategic late guardians, and both the tee and basket receive 1,067 cm hard clearances with feathered transitions. The green's rock and forest frame sit outside the primary putting surface.

Runtime acceptance commands:

```text
-Course=PineRidge -Hole=1 -Hole1FlightRouteSmokeTest -unattended -NullRHI
-Course=PineRidge -Hole=1 -PineRidgePlaySmokeTest -unattended -NullRHI
```

The first command records four real solver throws to `Saved/EnvironmentReports/PineRidgeHole1FlightRoutes.json`. Course construction writes `Saved/EnvironmentReports/PineRidgeHole1EnvironmentStatistics.json` with exact per-hole provisional HISM attribution, collision proxies, interaction volumes, culling, scalability, zone, and PCG status.

## Validation gates

- C++ Editor and Game targets build.
- `Scripts/validate_project.py` and `Scripts/reference_flight_check.py` pass.
- All `DiscGolfTour.` automation tests pass, including `DiscGolfTour.Environment.*`.
- Existing Pine Ridge course, fixture collision, and physics regression smoke gates remain green.
- PCG asset generation reports all 12 folders, 16 branches, and zero errors/warnings.
- Final acceptance additionally requires visual QA, collision-proxy review, and an Omen-class packaged performance capture.
