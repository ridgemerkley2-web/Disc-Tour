# Architecture

## Design principle
The full game is separated into a **simulation core**, **gameplay rules**, **presentation**, and **content data**. The simulation should be testable with gray boxes. The finished course should be able to change visually without altering disc physics.

## Runtime flow

1. `ADiscGolferPawn` owns player-facing shot setup components.
2. `UDiscBagComponent` selects a mold and plastic.
3. `UThrowControllerComponent` stores power, hyzer, nose, throw style, shot context, and a one-way release-timing meter. Circle 1/Circle 2 lies apply putting-specific pace, launch, angle, and timing ranges.
4. On release, the Pawn sends an `FThrowCommand` to `ADiscGolfTourGameMode`.
5. `UDiscCatalogSubsystem` resolves the selected stable mold/plastic IDs from its validated active catalog into an `FResolvedDiscDefinition`. The active catalog comes from registered cooked Primary Data Assets when the complete set is valid, otherwise from the deterministic source fallback.
6. `DiscGolfCourseRules::ApplyLieEffects` applies the authoritative current-lie profile to a copy of the command before release resolution. It does not alter the solver or the player's stored setup.
7. `DiscGolfMath::ResolveThrowRelease` converts the adjusted intent plus signed timing error into one immutable `FThrowRelease`: grade, quality, speed, spin, aim, hyzer, nose, launch angle, and recorded lie multipliers.
8. GameMode stores that result for feedback, spawns an `ADiscActor`, and configures its `UDiscFlightComponent`.
9. `UDiscFlightComponent` consumes the resolved release without reinterpreting player timing, then integrates translation plus spin-dominant gyroscopic attitude/precession at a fixed step independent of render frame rate.
10. `AWindDirector` supplies the current wind vector.
11. The disc sweeps through the Unreal world for collision while remaining under the custom solver rather than Chaos rigid-body integration. Ground hits resolve through deterministic Impact, Skip, Slide, Edge Roll, and Settled transitions using the contacted physical-response profile and resolved plastic properties. Blocking world fixtures resolve typed tree, rock, and sign impulses; dense-grass volumes use a one-time entry overlap that retains direction while removing speed and spin. The same hit separately records its typed gameplay course surface. Basket overlap predicts closest approach and classifies center chains, weak chains, top band, or tray without handing motion to Chaos.
12. The GameMode increments the throw stroke, starts `ADiscBroadcastCameraDirector`, and listens for settle/hole-out events. The director reads the live disc's newest recorded sample and selects launch, fairway, or finish coverage without writing to simulation.
13. On settle, GameMode traces the authoritative raw course surface. `DiscGolfCourseRules` resolves the playing surface, lie type/context, effect profile, penalty, relief rule, and resulting location. OB searches recorded samples backward for the last legal crossing and places the lie one meter inside; hazard plays from the result. Penalty and total strokes increment once.
14. `UDiscTrajectorySubsystem` snapshots the disc definition, immutable release, 240 Hz samples, ground transitions, final telemetry, and resulting lie state into versioned JSON/CSV before the disc Actor is released.
15. The player is moved to the resulting lie and control returns with the authoritative Drive, Circle 2 Putt, or Circle 1 Putt context. Non-regression practice state is saved.
16. GameMode retains the completed 240 Hz samples as a presentation snapshot. The tracer decimates that snapshot for bounded drawing, while `ADiscReplayActor` interpolates it at 0.75x without resimulation.
17. `ADiscGolfHUD` presents release feedback, lie effects, rules outcome, putting distance/pace/aim, basket outcome, export status, tracer/replay state, live camera mode/line-of-sight status, replay progress, selected physics preset, and development telemetry without requiring UMG assets.

## Why custom disc simulation
A golf disc is not well represented as a generic projectile with drag. Its orientation, angle of attack, spin, gyroscopic response, pitching/rolling aerodynamic moments, turn, and fade all matter. Using a fixed-step component also gives us a place to calibrate against measured trajectories.

## Classes

### Simulation/data
- `DiscGolfTypes.h`: shared enums and structs.
- `DiscGolfMath.h`: deterministic, side-effect-free drive/putting release resolution, basket evaluation, and shared physics/course math.
- `UDiscMoldDataAsset` / `UDiscPlasticDataAsset`: item-like cooked definitions with stable Asset Manager identities.
- `UDiscCatalogSubsystem`: Primary Asset discovery/loading, catalog validation, atomic fallback selection, and mold/plastic resolution.
- `UDiscFlightComponent`: aerodynamic integration, surface lookup, ground-state integration, and collision response.
- `UDiscTrajectorySubsystem`: durable trajectory capture/export, regression-preset loading/evaluation, and frame-rate suite reporting.
- `AWindDirector`: global/gust wind plus deterministic authored local-zone modifiers.
- `ADiscActor`: physical/visual disc Actor and chase camera.

### Player
- `ADiscGolferPawn`: current developer player pawn and input wiring.
- `UDiscGolfInputConfig`: data-asset contract for the gameplay mapping context and named Input Actions; also builds the source-only fallback layout.
- `UDiscBagComponent`: selected mold/plastic.
- `UThrowControllerComponent`: shot setup, one-way timing meter, and two-tap release command capture.
- `UDiscGolferPresentationComponent`: animation-facing drive/approach/putt family plus setup/windup/release/follow-through phase; it observes accepted release state and never triggers physics.
- `ADiscGolfTourPlayerController`: installs the Enhanced Input mapping context, registers remappable mappings with per-player user settings, owns the safe fallback lifetime, and drives the source-only controls/remapping state.

### Course/rules
- `DiscGolfCourseRules`: pure typed-surface resolution, physical-response mapping, lie classification/effects, penalties, display text, and relief placement.
- `ADiscGolfCourseSurfaceActor`: authored/runtime typed surface with stable compatibility tags.
- `ADiscGolfWorldFixtureActor`: quality-invariant tree, dense-grass, rock, and sign collision proxy with validated block/overlap contracts.
- `ADiscGolfFixturePresentationActor`: optional collision-free scanned rock, deterministic HISM shrub, and original PBR sign presentation fitted to unchanged fixture bounds.
- `ADiscGolfHoleActor`: tee, basket, par, hole metadata, effective distance.
- `ABasketActor`: source-only basket visual and deterministic center-chain/weak-chain/band/tray interaction boundary.
- `DiscGolfCourseDefinition`: schema-v1 course-manifest and external/fallback hole parsing, including validated world fixtures and Primary/RiskReward/Bailout strategy-route metadata.
- `DiscGolfRoundState`: side-effect-free per-hole score, total, progression, and completion contract.
- `DiscGolfCoursePresentationDefinition`: optional staged terrain/foliage/water plan and visual-quality tiers; invalid if any tier changes competitive collision.
- `ADiscGolfFoliagePresentationActor`: deterministic HISM sapling/authored-tree visuals with quality-scaled density and culling, with collision permanently disabled.
- `UDiscGolfEnvironmentAssetSet` / `UDiscGolfForestPreset`: asset-path abstraction, zone rules, species mix, materials, collision policy, wind, and Performance/High/Cinematic environment presets.
- `ADiscGolfEnvironmentController`: one partitioned, on-demand course-scale PCG owner that evaluates the preset and shares wind with `AWindDirector`.
- `ADiscGolfEnvironmentZoneActor`: spline/radial/box fairway, tee, green, rough, OB, and hard-exclusion control with category-specific tree/brush setbacks.
- `UPCGDiscGolfZoneDensitySettings`: data-driven PCG point node that applies authored zone density, deterministic selection, spacing bounds, and asset-set mesh attributes without hard-coded Marketplace paths.
- `ADiscGolfVegetationInteractionActor`: query-only canopy/shrub volume that feeds configurable velocity/spin reduction into the existing flight overlap seam.
- `ADiscGolfTerrainPresentationActor`: one deterministic property-scale height field with three fairway ribbons, two connector trails, Gallery Lake basin grading, and five quality-scaled slope-aware HISM ground-cover components; collision, navigation, and overlaps remain disabled.
- `ADiscGolfWaterPresentationActor`: deterministic 128x12-ring Gallery Lake mesh with animated material parameters; it is collision-, overlap-, navigation-, and shadow-free and never owns hazard identity.
- `ADiscGolfLevelDesignReviewActor`: opt-in collision-free route/corridor and landing-zone debug drawing for designer review; it is never consulted by flight, rules, or scoring.
- `DiscGolfRouteTelemetry`: side-effect-free route/landing evaluation plus schema-v1 session serialization; GameMode feeds it completed real tee-shot samples but never feeds its result back into play.
- `ADiscGolfCourseFeatureActor`: reusable landing-zone, camera-anchor, and spectator-boundary metadata/visual marker.
- `ADiscGolfWindZoneActor`: deterministic box-volume wind scaling/addition consumed by `AWindDirector`.
- `ADiscGolfFlyoverRouteActor`: stationary authored world spline with a presentation-only moving camera.
- `ADevCourseBootstrap`: generalized authored Pine Ridge hole assembler plus source-only regression course with typed fairway, tee, light/deep rough, dirt, rock, OB, and hazard fixtures.
- `ADiscGolfTourGameMode`: throw authority, release/lie resolution, penalties, course/hole transitions, round score, active disc, save snapshot, and camera handoff.

### Presentation/persistence
- `ADiscGolfHUD`: debug/broadcast-influenced HUD drawn directly to Canvas.
- `DiscGolfBroadcastCameraMath`: side-effect-free mode selection and shot-context-aware framing plans.
- `ADiscBroadcastCameraDirector`: live-sample camera consumer with interpolation, line-of-sight recovery, visual-bounds avoidance, and lifecycle-safe tracking.
- `DiscGolfPresentationMath`: side-effect-free replay interpolation/orientation and state-aware tracer sample selection.
- `ADiscReplayActor`: collision-free replay ghost, recorded-time playback, and replay chase camera.
- `UDiscGolfTourGameInstance` / `UDiscGolfSaveGame`: profile foundation.

### Presentation boundary

The active flight component and completed trajectory export are authoritative. Camera, tracer, and replay only read `FDiscTrajectorySample` values. The live director converts the latest sample plus release/basket context into a camera plan; it never predicts by rerunning physics. The tracer uses a bounded, endpoint-preserving decimator and draws transient lines each frame. The replay actor uses a binary-search time lookup plus deterministic interpolation; neither presentation actor owns a `UDiscFlightComponent`, collision, score, basket, or export path. Settle, hole-out, cancellation, and reset all restore GameMode's player-camera ownership.

The director advances only from launch to fairway to finish. It smooths drive transitions, cuts directly to finish after a brief putting launch view, adjusts field of view by mode, raises/offsets blocked plans using line traces, and excludes expanded static-mesh visual bounds. The extra bounds test is required because the primitive course intentionally contains collision-disabled crown meshes that still obstruct a camera visually.

The golfer pawn now owns an optional `USkeletalMeshComponent` plus a presentation-state component. When no skeletal asset is assigned, the existing capsule/cylinder/sphere golfer remains visible and the HUD reports `PLACEHOLDER`. The first throw press begins a looping visual windup. Only after GameMode has resolved and launched the immutable `FThrowRelease` does it commit the visual release phase; animation notifies cannot launch or alter a disc. Distance selects drive versus controlled-approach presentation, while Circle 1/2 select putting. Release grade/timing are exposed for future animation variation.

On an authored course, the same launch/fairway/finish state machine first resolves a matching `ADiscGolfCourseFeatureActor` camera anchor. The anchor overrides location/FOV but not look-at, mode timing, smoothing, line-of-sight recovery, or gameplay state. With no matching anchor, the deterministic procedural plan remains the fallback.

## Authored course runtime

`Data/PineRidgeCourse.json` is the NonUFS schema-v1 course manifest. It owns course/layout identity, ordered hole references, total-par expectations, stable world origins/yaws, and duplicate/order/placement validation. `Data/PineRidgeHole1.json`, `PineRidgeHole2.json`, and `PineRidgeHole3.json` are independent schema-v1 hole definitions. Each owns tee/basket metadata, typed primitive surfaces, trees, landing zones, Primary/RiskReward/Bailout strategy routes, camera anchors, spectator boundaries, local wind zones, and flyover points in local hole space. `DiscGolfCourseDefinition::PlaceHoleInCourse` transforms the complete validated hole into shared course space while preserving all internal distances. Validation rejects bad identity/version/order, overlapping manifest placement, incomplete feature or route coverage, missing/duplicate IDs, routes without real landing zones, invalid transforms/extents, incomplete camera modes, unsafe wind values, and route/flyover endpoints that do not cover tee and basket. Matching source fallbacks keep every hole bootable if external data is missing or invalid; the HUD exposes the active source.

`ADevCourseBootstrap` is the current assembly boundary. For Pine Ridge it builds all three transformed holes once, stores per-hole surfaces/flyovers/review/foliage/water state, then builds one shared ground actor and switches only active pointers/review visibility during the round. The ground, forests, fixtures, baskets, cameras, wind zones, and Gallery Lake therefore coexist as one persistent property. The separate regression course still uses an isolated rebuild path so permanent physics fixtures cannot contaminate authored play. Runtime-generated static-mesh components are made movable before mesh assignment. Lighting is shared and spawned once. Unknown course or hole IDs fail without silently selecting a different target. For rock, dense grass, and sign fixtures, the bootstrap may attach an `ADiscGolfFixturePresentationActor`; it hides a primitive proxy mesh only after the matching presentation configures successfully and never removes or mutates the proxy actor or its collision component.

GameMode owns course/hole selection, round state, and presentation cleanup. A Pine Ridge switch is blocked during an active throw/regression, stops replay/broadcast/flyover presentation, activates the requested persistent hole, refreshes active wind zones, resets the lie, moves the golfer to the world-space tee, and updates status without rebuilding the property. A failed transition rolls back to the previous valid hole. `DiscGolfRoundState` records a hole exactly once and requires every manifest entry before completion, so direct Hole 2/3 starts cannot falsely end a round. Physics/rules fixtures always switch to `RegressionCourse` first, preserving established geometry and baselines.

`Data/PineRidgePresentation.json` supplies production-art references without modifying hole gameplay data. Its Low/Medium/High tiers may change foliage/grass density and cull distance only. Each hole names an original forest pattern and records depth, route-edge buffer, and tee/green clearing dimensions. `ADiscGolfFoliagePresentationActor` distributes deterministic instances around the primary route while excluding every strategy corridor, water, cameras, spectator lanes, tee/green clearings, and minimum neighbor spacing. All three fir variants must load before it hides any trunk proxy. Typed surfaces plus 44 deterministic tree-trunk proxies remain in the hole definitions, and automation proves that every quality tier resolves the same competitive-geometry signature. Missing production assets remain an honest fail-safe blockout state. See `Docs/PINE_RIDGE_FOREST_DESIGN.md`.

Fixture presentation is a separate optional runtime layer. The boulder mesh is component-fitted and pivot-corrected inside the authored spherical proxy; shrubs are deterministic masked HISM instances contained by the dense-grass proxy; the Pine Ridge sign is an original cube/text assembly using a CC0 wood material. These components use `NoCollision`, ignore all channels, disable overlap generation, and cannot affect navigation. Course smoke accepts a source-only zero-visual fallback or the complete three-family set, but rejects partial presentation so missing imports stay obvious.

After all three Pine Ridge holes are placed, bootstrap creates one shared course presentation actor. Its height samples the Primary routes and two generated green-to-next-tee connector splines, adds restrained deterministic relief away from play, and blends Gallery Lake into the same field. Three PBR fairway ribbons and two path-material connector ribbons sit above the height field; irregular procedural patches add visual wear. Three grass-species, two litter-variant, and three shoreline-dressing HISM components distribute cards, rocks, reed stems, and deadfall with slope/protected-space rejection and culling. Shore dressing additionally enforces clearance from every authored strategy route and existing fixture. The actor never supplies collision or lies; the sealed `ADiscGolfCourseSurfaceActor` primitives remain authoritative. Successful setup hides only the obsolete fairway/rough/dirt blockout meshes. See `Docs/PINE_RIDGE_GROUND_GRASS.md`.

Gallery Lake water is another optional presentation layer. Bootstrap identifies the authored `LakeWater` surface, configures a fitted procedural water actor, and hides only the authority's primitive mesh after successful setup. The underlying `ADiscGolfCourseSurfaceActor`, collision, `Hazard`/water identity, rules lookup, and penalty behavior remain present and unchanged. If the material or mesh cannot configure, presentation fails safe and the primitive remains visible. The current hard elliptical bank is intentionally scheduled for shoreline grading and Landscape/blockout reconciliation without resizing or replacing that gameplay volume.

On a normal settle or hole-out, GameMode records strokes and penalties and saves schema-v5 practice state. Ordinary startup validates and restores the course, active hole, round scores, lie, penalties, completion state, and selected mold/plastic; explicit development launches and automated gates ignore the local snapshot. The scorecard is a read-only HUD projection of manifest metadata plus round state. Restart clears all hole scores and reloads Hole 1. Presentation, input, and saves consume this state but never own scoring authority.

`AWindDirector` sorts authored zones by stable ID and layers each containing zone over global/gust wind as `wind * scale + additive`. With no authored zones it is behaviorally identical to the v0.2 director. The HUD reports the active zone at the player location.

The flyover actor never moves its spline owner after authoring world-space points. Its attached camera travels along the spline, looks toward the basket, broadcasts completion, and returns GameMode camera ownership to the golfer. It owns no scoring, collision, or shot state.

The performance-capture launch path applies `OmenGameplay1080pHighFoliageV1` before persistent-course assembly,
temporarily stretches the active authored flyover into one continuous warm-up/sample traversal, and restores
normal process state by exiting after evidence is written. Schema-v2 evidence records rendered RHI/adapter,
resolution, runtime mode, quality levels, warm-up, route, duration, budget, and summary. The validator rejects
NullRHI and wrong-preset evidence. This monitor only observes tick delta and process memory; it never feeds
simulation, course assembly, collision, rules, or scoring.

This boundary is intentional: adding effects, playback speeds, ghost materials, or new broadcast framing must not alter solver state or completed-shot results.

### Trajectory and regression boundary

The flight component owns only live simulation state. It records one `FDiscTrajectorySample` per 240 Hz solver step plus explicit `FDiscGroundTransition` events. GameMode owns shot lifecycle and asks the GameInstance trajectory subsystem to serialize a completed shot before the disc Actor is released.

Trajectory exports use schema version 3 and are self-contained: engine/schema metadata, resolved disc/aero values, immutable release with shot context and lie multipliers, SI relative position, Unreal world position, velocity, attitude normal, wind, spin, angle of attack, physical ground state/surface, gameplay course surface, contact count, transition impact geometry, basket contact/result, and final rules summary. Preset and regression-report schemas remain version 2. Timestamped files preserve history while `LatestTrajectory.json/.csv` make tools and tests easy to target.

Physics presets live in `Data/PhysicsRegressionPresets.json` and are staged as a packaged NonUFS runtime dependency. A preset freezes command, plastic, throw style, shot context, optional starting distance/aim offset, wind, gusts, and render cap while preserving the 240 Hz solver. Individual envelopes can require a make/miss and exact basket outcome; suite reports compare matched 30/60/120 FPS captures against the 60 FPS result with tighter positional tolerances. The prior wind and frame cap are restored after a run.

`ADiscGolfFixtureQaRunner` is an automated acceptance actor, not gameplay authority. The `-FixtureCollisionSmokeTest` launch path spawns it outside the course, where it drives 24 real disc/fixture contacts through the same sweep and overlap callbacks used in play. It consumes flight telemetry only after each case, compares the recorded exit to the pure `DiscGolfMath` response, writes a schema-v1 report, and exits. It never changes course definitions, solver coefficients, rules, scores, or player saves.

### Route-telemetry boundary

`-NeedleGateRouteTelemetry` starts or resumes a session bound to course, layout, hole, and collision-profile identity. Only the first real tee throw of an attempt is evaluated against the currently selected authored polyline and landing zone. The analyzer measures two-dimensional distance to the corridor, landing-zone containment/miss side, and observed shot evidence; GameMode adds the authoritative lie, penalty, fixture, visibility, release, and eventual hole-score data. Reports are saved after every material annotation or outcome.

The selected route only focuses the collision-free review overlay and labels the HUD. It does not rotate the golfer, change the throw command, alter wind, modify collision, affect lie/rules/scoring, select a disc, or influence the solver. A route stops accepting samples at exactly 20. The latest report may be resumed only when schema, course, layout, Hole 2, and `PineRidgeCompetitiveV2_Fixtures` all match; otherwise a new session is created.

### Putting and basket boundary

GameMode owns shot context because lie and basket distance are rules, not player-authored physics input. `UThrowControllerComponent::SetShotContext` resets stale timing state and installs the appropriate suggested pace and putting setup. GameMode writes the authoritative context into the command again at throw time so external callers cannot launch a drive model from Circle 1.

Basket interaction is deliberately deterministic and testable. The basket's broad overlap asks `DiscGolfMath::EvaluateBasketContact` for the disc's short-horizon closest approach. Center height/radius/pace catches; outer or over-speed chains remove energy and deflect; high and low lines reject from band/tray. `UDiscFlightComponent` applies that response to the custom solver and records basket outcome in final telemetry. The visible chain strands are presentation only and never drive physics.

### Course-surface and lie boundary

`ECourseSurfaceType` is gameplay identity; `EGroundSurfaceType` is physical landing response. Their mapping is explicit. Light/deep rough and hazard currently use rough physics, OB uses fairway physics, and dirt/rock/tee retain their dedicated profiles. Course identity can come from `ADiscGolfCourseSurfaceActor`, stable `Surface.*` tags, or a Physical Material named `PM_Surface_<Type>`.

`FDiscGolfLieState` is the serializable handoff between one completed throw and the next. It stores raw and playing surfaces, raw/result locations, lie type, shot context, penalty/relief, penalty strokes, and effect profile. GameMode owns this state; the flight component reports evidence but never awards strokes or moves the player. Regression scenarios require no rules penalty so course-authoring changes cannot silently contaminate established physics envelopes.

## Production evolution

### Disc data
The active production catalog uses five `UDiscMoldDataAsset` and three `UDiscPlasticDataAsset` packages under `/Game/Data/Discs`. Each asset overrides `GetPrimaryAssetId`: mold identity is `DiscMold:<MoldId>` and plastic identity is `DiscPlastic:<PlasticId>`. Asset Manager scans both directories and applies `AlwaysCook`; the project packaging settings also retain the directory as an explicit cook root.

`UDiscCatalogSubsystem` explicitly scans and synchronously loads both Primary Asset types during GameInstance initialization, sorts by stable ID, then validates the candidate before exposing it. Validation rejects empty or duplicate IDs, missing Base/Tour/Crystal plastic coverage, missing required v0.2 mold IDs, non-finite values, non-positive physical coefficients/modifiers, and out-of-range speed/glide ratings. Candidate selection is atomic: one invalid or missing definition selects the complete source fallback rather than mixing asset and source rows.

The fallback intentionally remains code-authored so broken/missing cooked content cannot prevent the regression course from launching. `DiscGolfTour.Data.Catalog.AssetFallbackParity` resolves all 15 mold/plastic combinations through both paths and compares every aerodynamic and ground-response field. `Scripts/generate_disc_data_assets.py` is an idempotent editor-only generator for rebuilding the checked-in assets; the generator's plugins are restricted to editor targets.

### Input
Gameplay uses Unreal Enhanced Input end to end. `ADiscGolferPawn` binds named actions from `UDiscGolfInputConfig`; `ADiscGolfTourPlayerController` adds the gameplay mapping context to the local-player subsystem and registers it with Enhanced Input user settings for remapping.

The native controller currently has no binary input asset assigned, so it generates a complete keyboard/mouse/controller mapping context at runtime. That path logs and displays an explicit warning instead of crashing. Production content can assign an authored `UDiscGolfInputConfig` containing `IA_*` and `IMC_*` assets to the controller class defaults; incomplete assets are rejected in favor of the same safe fallback.

The Canvas HUD includes a regression-grade controls screen opened with Escape or controller View/Back. Opening it pauses play and temporarily removes the gameplay mapping context, while direct controller input handles menu navigation and key capture. Rebindings are validated for device and axis compatibility, checked for conflicts/reserved keys, applied through `UEnhancedInputUserSettings`, and saved per local player. This screen should eventually be reskinned in Common UI/UMG, but its behavior remains useful as an asset-free regression path.

### UI
The Canvas HUD is a regression/debug HUD. Production UI should move to Common UI/UMG while preserving a telemetry overlay that can be enabled for physics tuning.

### Courses
The primitive runtime hole remains forever as a fast regression scene. Pine Ridge proves a source-controlled authored data/blockout pipeline. Production courses can replace primitive visuals with World Partition/Landscape/PCG content while retaining explicit hole data, typed collision, camera anchors, wind zones, and acceptance gates.

Pine Ridge's active presentation uses one course-scale `ADiscGolfTerrainPresentationActor` built after all three holes are transformed into persistent world space. Its height field, five-rail vertex-alpha fairway/trail blends, Gallery Lake soil bank, and five grass/litter HISM components are visual-only, with collision, overlaps, and navigation disabled. Authored surface actors, fixtures, and the hidden water volume remain the independent gameplay authority; presentation can fail without changing lie, penalty, or disc-contact behavior.

### Multiplayer
Do not network the project yet. First make the authoritative single-player shot state deterministic and serializable. Multiplayer can then replicate throw commands and authoritative results instead of trying to network every render-frame transform.

## Coordinate systems
- Unreal position: centimeters.
- Aerodynamics: meters, seconds, kilograms, Newtons, radians.
- `VelocityMps`: world-space meters/second.
- `SpinRateRadPerSec`: signed axial spin in radians/second.
- `DiscNormalWorld`: world-space disc top-face normal used by aerodynamics/precession.
- `DiscForwardWorld`: forward direction constrained to the disc plane.
- `BodyToWorld`: render/collision quaternion rebuilt from the forward/normal attitude state.
- `GroundNormalWorld`: current supporting surface normal for slope acceleration and constrained ground motion.

Ground response remains in SI units. Surface tags select a profile; plastic-adjusted restitution/friction and impact geometry select the state. Only the final swept Actor movement is converted back to centimeters.

Document any sign-convention changes before modifying the torque model.
