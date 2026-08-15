# Pine Ridge Hole 1 Production Benchmark

## Acceptance role

Pine Ridge Opening is the visual, gameplay, collision, and performance benchmark for the future 18-hole property. It lives in the existing persistent Pine Ridge course world. Holes 2-18 must not be mass-produced from this benchmark until Hole 1 passes final imported-asset visual and performance review.

## Scorecard and orientation

- Hole: 1
- Par: 3
- Measured tee-to-basket distance: 361.9 feet
- Tee: authored 4.2 m x 1.8 m rectangular pad
- Green protection: 1,067 cm / 35 ft, covering the standard 10 m putting circle
- Primary fairway: 1,680 cm / 55 ft plus 610 cm tree setback and 275 cm brush setback
- Alternate fairway: 1,370 cm / 45 ft plus 480 cm tree setback and 225 cm brush setback
- Opening camera: existing `TeeBroadcast` anchor; existing sparse scorebug shows Hole 1, Par 3, and distance
- Tee sign: existing presentation sign now displays `HOLE 1 / PAR 3 / 362 FT`

## Playing strategy

`CenterPlacement` is a controlled flat or slight-hyzer backhand. The launch corridor is generous, then conifers progressively frame the mid fairway before a limited number of late guardians ask for speed and angle control.

`SkipShelfAttack` is the aggressive stable-hyzer choice. It uses the left-center dirt shelf for a direct birdie/ace look and carries higher authored risk without becoming a narrow artificial tunnel.

`RightBailout` is presented in-game as `TURNOVER / FOREHAND`. It gives a genuinely different window through the right apron and asks the disc to shape back toward the forest-framed green.

PCG creates ecology around those lines; the 12 authored trunks create strategy. Random generation does not own scoring difficulty.

## Zone plan

| Zone | Priority | Shape | Core | Falloff | Purpose |
|---|---:|---|---:|---:|---|
| `H01_DeepRoughEcology` | 10 | box | 152 m x 124 m | 5 m | Dense mixed-age forest background |
| `H01_SemiRoughTransition` | 30 | spline | 45 m | 9 m | Recoverable progressive rough |
| `H01_TurnoverFairway` | 45 | spline | 13.7 m | 5.5 m | Alternate right-side shape |
| `H01_PrimaryFairway` | 50 | spline | 16.8 m | 6.5 m | Main maintained corridor |
| `H01_TeeClear` | 100 | radial hard exclusion | 10.67 m | 3 m | Clean throwing/camera area |
| `H01_GreenClear` | 100 | radial hard exclusion | 10.67 m | 3.5 m | Unobstructed primary putting area |

## Collision acceptance

- Existing authored tree primitives remain the only competitive trunk authority until approved per-species proxies replace them.
- Tree and rock contacts are solid and use the existing fixture response.
- The authored dense-grass fixture remains pass-through slowdown.
- Visual CC0 foliage, generated grass, ferns, litter, and future Fab visual meshes remain collision-free.
- Final canopies use `ADiscGolfVegetationInteractionActor` query overlaps; they reduce speed/spin and never behave as solid leaf walls.

## Reports and repeatable checks

- `Saved/EnvironmentReports/PineRidgeHole1EnvironmentStatistics.json`
- `Saved/EnvironmentReports/PineRidgeHole1FlightRoutes.json`
- `Saved/Developer/EnvironmentAssetBindingReport.json`

Use `-Hole1FlightRouteSmokeTest` to execute flat backhand, hyzer, turnover/anhyzer, and forehand drives with the existing 240 Hz solver. Use `DiscGolfTour.Environment.*` automation for clearance, deterministic plan, collision, scalability, wind, and binder gates.

## Pending visual acceptance

Final visual approval remains pending until the owner imports the products in `FAB_ASSET_MANIFEST.md`, records the actual acquisition license/provenance, approves binder candidates, creates simplified proxies, adapts foliage wind non-destructively, regenerates PCG, and profiles Performance, High, and Cinematic in Editor. The authored layout and competitive collision remain stable through that swap.
