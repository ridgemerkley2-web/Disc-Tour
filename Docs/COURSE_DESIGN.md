# Course Design Framework

## Goal
Create original courses that feel like real championship disc golf while supporting readable video-game strategy, broadcast cameras, spectators, and multiple difficulty layouts.

PDGA course-development material is the baseline reference for course-design thinking: player skill level, safety, acreage, par, equipment, and route design. The game may heighten spectacle, but believable holes should still feel intentionally designed.

## Hole design checklist
Every production hole should define:
- intended skill level/layout,
- par,
- tee positions,
- pin positions,
- measured/effective distance,
- primary scoring route,
- optional risk/reward route,
- landing zones,
- obstacle/gap intent,
- OB/hazard geometry,
- prevailing wind relationship,
- safe spectator zones,
- broadcast camera anchors,
- replay/drone path,
- AI shot-plan metadata,
- pin visibility/readability,
- bailout/scramble options.

## Strategy-route contract

Every production hole must author a Primary, Risk/reward, and Bailout route. A route binds human-readable intent and target-stroke/risk/reward expectations to a real landing zone and a world-space corridor. This data supports level-design review, future AI planning, and telemetry only; it never guides the disc or changes scoring/collision.

The Pine Ridge hole-by-hole intent, review colors, and evidence gate for geometry changes are maintained in `Docs/PINE_RIDGE_LEVEL_DESIGN.md`.

## Effective distance
The prototype isolates an elevation adjustment in `DiscGolfMath::EffectiveHoleDistanceFeet`. It currently follows the previously used rule of approximately three feet of effective distance per foot of elevation change. Keep it configurable; do not bury course-rating assumptions in camera/UI code.

## Vertical slice
The first three production holes are designed to test different gameplay:

### 1 — Pine Ridge Opening
Par 3, roughly 360 ft. Open tee shot into a guarded green. Teaches timing, wind, stability, and ground skip.

**v0.3 gray-box status: complete.** The playable definition measures 362 ft and contains an elevated pin, opening/landing fairways, green, light/deep rough, dirt skip shelf, rock guard, tournament-rope OB, pine-needle hazard, primary/bailout landing zones, tree corridor, gallery boundaries, launch/fairway/finish cameras, two local wind zones, and a six-point flyover. The checked-in JSON is the authored source and has an exact validated C++ fallback.

### 2 — Needle Gate
Par 4, roughly 640 ft. Wooded placement hole. Tests gap control, landing-zone planning, scramble, forehand, and midrange play.

**v0.4 gray-box status: complete.** The validated definition uses 12 typed surfaces and 18 tree proxies to create a readable placement corridor, primary and bailout landing zones, OB/hazard pressure, launch/fairway/finish cameras, three gallery boundaries, two local wind zones, and a seven-point flyover.

### 3 — Gallery Lake
Par 4, roughly 725 ft. Downhill water-carry showcase. Tests wind, risk/reward, spectators, ropes, broadcast cameras, and tournament atmosphere.

**v0.4 gray-box status: complete.** The validated definition uses 12 typed surfaces and 14 tree proxies around a downhill fairway and explicit water hazard, with primary and bailout landing zones, launch/fairway/finish cameras, three gallery boundaries, two local wind zones, and a seven-point flyover.

## Video-game readability
Real forests can become visually noisy. Preserve believable vegetation while ensuring:
- intended gaps read from the tee,
- basket/landing-zone information can be discovered without neon markers,
- camera does not hide critical branches,
- collision matches visible geometry,
- foliage quality scaling does not change competitive collision geometry.

## Ground-surface contract

The source-only regression hole includes typed tee-pad, fairway, light-rough, deep-rough, dirt, rock, out-of-bounds, and hazard patches. Production terrain can expose the same `ECourseSurfaceType` categories with `ADiscGolfCourseSurfaceActor`, stable `Surface.*` tags, or Physical Materials named `PM_Surface_<Type>`. Skip, slide, edge roll, spin retention, and settle remain independent of visual meshes and object names. Surface collision must stay stable across visual-quality presets.

Production art is planned independently in `Data/PineRidgePresentation.json`. Visual tiers may scale non-colliding foliage density, decorative grass density, and cull distance. They all bind to the active `PineRidgeCompetitiveV2_Fixtures` profile; any tier that claims to affect collision or changes that profile is invalid. Current tree definitions remain deterministic trunk proxies even after instanced visual trees replace blockout crowns. Only explicit `collisionFixtures` entries own boulder, sign, or dense-grass gameplay response; ordinary visual grass never collides. Scanned boulder, masked shrub, and original PBR sign actors may cover those fixtures visually, but never replace their gameplay components.

Gameplay identity and landing response are deliberately separate. Light/deep rough and hazard currently use the rough physical profile, OB uses fairway response, and tee/dirt/rock retain dedicated ground profiles. This lets course designers change a rules boundary without forking flight physics.

## Lie and penalty contract

- Tee/fairway/dirt/rock are clean stances at 100% power and normal timing sensitivity.
- Light rough applies 96% power and 110% timing-error sensitivity on the next throw.
- Deep rough applies 88% power and 125% timing-error sensitivity.
- Hazard adds one stroke, keeps the lie at the result, and applies 92% power / 115% timing sensitivity.
- Out-of-bounds adds one stroke, searches the recorded trajectory for the last legal crossing, and places the lie one meter inside the boundary on the legal side.
- Basket makes override landing rules and complete without an added surface penalty.

Production OB shapes must have an unambiguous legal side and enough continuous collision coverage for the one-meter relief point. Complex islands, drop zones, mandatory rethrows, and tournament-specific rules should become explicit authored rule data instead of object-name conventions.

## Course data should ultimately include
A production `CourseDefinition` should reference hole definitions, environment/biome, weather ranges, tournament dressing, layouts, pars by target skill, spawn areas, and performance budgets. Hole definitions should be independent enough to support practice mode and individual-hole loading.

## Current authored-hole contract

`Data/PineRidgeCourse.json` uses schema `disc_golf_course_manifest`, version 1. It defines stable course/layout identity, ordered hole references, and expected total par. Runtime validation rejects missing, duplicate, out-of-order, or identity-mismatched entries.

`Data/PineRidgeHole1.json`, `PineRidgeHole2.json`, and `PineRidgeHole3.json` use schema `disc_golf_hole_blockout`, version 1. Every authored file must provide stable course/layout/hole identity plus valid transforms and globally unique IDs for surfaces, landing zones, strategy routes, camera anchors, spectator boundaries, and wind zones. A hole requires fairway, tee, light rough, deep rough, OB, and hazard coverage; Primary and Bailout strategy coverage; launch, fairway, and finish camera coverage; and route/flyover endpoints that cover tee and basket.

The runtime course markers are intentionally reusable and non-authoritative where appropriate:

- typed surface actors own collision and feed the existing rules/ground contracts;
- landing zones communicate strategy and later AI planning but do not alter flight;
- shot routes describe intended corridors and scoring tradeoffs but do not steer throws;
- camera anchors affect framing only;
- spectator boundaries are blockout/safety metadata and visible rails;
- wind zones modify sampled wind deterministically;
- flyover points drive presentation only.

Round state is deliberately separate from the course files. `FDiscGolfRoundState` binds scores to the manifest's ordered hole entries, while authored hole definitions remain independently loadable for practice and visual QA. The separate presentation definition now owns per-layout art/scalability metadata without embedding mutable round state.
