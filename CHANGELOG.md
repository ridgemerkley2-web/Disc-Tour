# Changelog

## Pine Ridge Hole 1 gameplay vertical slice

- Split the clean production HUD from the F9 developer overlay and disabled the debug shot tracer by default.
- Added the skippable Hole 1 introduction, optional/skip-safe flyover flow, visibility-aware basket marker, unit-aware distance/wind, disc-bag ratings, shot/putting context, result cards, and restrained score completion.
- Added a bounded 128-entry authoritative throw-history subsystem and capped immutable last-shot replay recording at 1,800 samples while retaining important ground transitions.
- Added tracking and stationary tee replay cameras without rerunning physics.
- Improved lie relocation presentation with a short landing-camera hold and retained existing authoritative OB/penalty/relief logic.
- Added persistent graphics, display, audio, sensitivity, invert-Y, HUD, units, scaling, reduced-motion, contrast, and aiming settings plus gamepad-navigable settings/control pages.
- Added the Editor-only hole-authoring utility and generated `Saved/CourseReports/PineRidgeHole1Validation.json` with exact geometry, environment zones, authored-tree count, route results, and presentation-system readiness.
- Added eight automation tests across authoring, intro gating, metadata/unit conversion, score vocabulary, lie/OB classification, replay bounds, basket marker state, and settings normalization. All 96 project tests pass.
- Captured the clean Hole 1 intro and gameplay views at 1280x720. Final forest visual acceptance remains pending the manual Fab import; no marketplace content was downloaded or fabricated.

## Production PCG forest architecture

- Added one asset-agnostic environment stack for the future 18-hole persistent course: a course-scale PCG controller, spline/radial/box vegetation zones and hard exclusions, 16 mesh categories, material/water/wind slots, category spacing, species mix, deterministic seed, and Performance/High/Cinematic quality settings.
- Added a generated 66-node `PCG_TemperateMountainForest` template with collision-ground sampling plus 16 independent Zone Density -> Density Filter -> Self Pruning -> HISM branches. The custom density node resolves course zones and mesh selection entirely through `DA_TemperateMountainForest_Assets`.
- Added split collision policy for visual foliage, precise trunk/major-branch/rock/log proxies, and query-only canopy/shrub slowdown volumes. The only flight change is a small integration through the existing fixture overlap/impact seams; the aerodynamic solver remains unchanged.
- Added shared environment/disc-flight wind via `AWindDirector` and `MPC_EnvironmentWind`, which contains direction, speed, and gust parameters.
- Created all requested `/Game/Environment` folders and the `DA_TemperateMountainForest` preset without downloading or inventing third-party licenses. Existing documented CC0 fir sapling, shrub, and boulder assets are provisional; every missing free Fab slot is explicitly documented in `Docs/PRODUCTION_ENVIRONMENT_FOREST.md`.

## Packaged Omen performance gate

- Replaced invalid NullRHI/gray-box performance evidence with schema-v2 capture artifacts that require rendered D3D12, exact 1920x1080, the source-controlled Omen gameplay preset, actual adapter identity, a 10-second warm-up, and continuous authored flyover traversal.
- Audited the imported source meshes and found single-LOD firs at 124,743-157,402 triangles plus a 123,976-triangle shoreline boulder; added reproducible four-level fir and three-level boulder reduction chains while preserving full close detail.
- Retained all 1,574 High-tier decorative trees and 44 authored trunk authorities, tuned decorative culling to 260 m, split near-route shadow casters from deep non-shadowing forest mass, and replaced continuous real-time skylight capture with one stable course-load recapture.
- Passed the packaged 1920x1080 D3D12 gate on all three holes at 112.6 / 95.9 / 120.2 average FPS, 11.10 / 15.63 / 9.77 ms P95, zero hitches, and 1.23-1.27 GiB process memory.
- Reverified 76/76 automation, the exact persistent forest/ground counts, 24/24 live fixture contacts, all six physics regressions, the three-hole round, and 1080p close visual QA without changing competitive geometry.

## Trail-wear and shoreline-dressing gate

- Added 36 deterministic irregular PBR wear patches / 288 triangles across both persistent green-to-next-tee connector trails.
- Added 42 fitted CC0 scanned-rock HISM instances, 96 four-stem reed clusters / 384 textured stems, and 14 rounded bark-covered deadfall HISM instances around Gallery Lake.
- Enforced slope limits plus a minimum 1,050 cm clearance from every authored strategy route, with additional tee, green, camera, spectator, and existing-fixture exclusions.
- Added reproducible trail-wear and reed material instances, exact runtime/automation acceptance, and dedicated close visual-QA cameras.
- Verified the final editor build, clean asset import, 76/76 automation, one persistent course, dense forest, water authority, 24/24 fixtures, six physics scenarios, seamless three-hole scoring, validators, and the unchanged 84.5 m reference flight.

## Slope-aware HISM ground-cover gate

- Replaced runtime procedural grass/litter sections with five collision-free HISM components: three grass species and separate leaf/needle litter variants.
- Preserved 7,150 High-tier grass clusters while rendering 14,300 masked cards / 28,600 triangles, plus 1,980 litter clusters / 3,960 triangles.
- Aligned instances to sampled course normals, rejected grass above 32 degrees and litter above 38 degrees, and applied tier-scaled 67.5-180 m, 90-240 m, and 112.5-300 m cull ranges.
- Added reproducible original alpha-mask sources, per-instance shade variation, species tint/proportion variation, and root-anchored grass wind to the Pine Ridge import pipeline.
- Added exact HISM species/variant, cull-range, slope, and collision-invariance acceptance to automation and runtime smoke tests, plus a dedicated close ground-cover visual-QA camera.
- Verified the final editor build, 76/76 automation, one persistent course, dense forest, water authority, 24/24 fixtures, six physics scenarios, seamless three-hole scoring, all artifact validators, the unchanged 84.5 m reference flight, three tee renders, and a close route-edge render.

## Persistent biome blend and shoreline gate

- Rebuilt fairways and green-to-next-tee trails as five-rail procedural ribbons whose vertex alpha blends complete CC0 PBR texture sets into the forest floor; the verified geometry is 1,152 fairway and 512 connector triangles.
- Added a four-rail, 576-triangle Gallery Lake soil/erosion band that grades from below the water surface back to the shared terrain while retaining the hidden authored water hazard as sole gameplay authority.
- Added 1,980 deterministic High-tier forest-floor litter clusters / 3,960 triangles and expanded the lake exclusion so neither grass nor litter intrudes into the shoreline.
- Generated and validated `M_PineRidgeFairwayBlend`, `M_PineRidgeTrailBlend`, and `M_PineRidgeLeafLitter` through the reproducible import pipeline.
- Verified 76/76 automation, the persistent course, dense forest, water, 24/24 fixtures, six physics scenarios, seamless three-hole scoring, all artifact validators, three real-render tee views, and a dedicated shoreline render with no competitive collision change.

## Persistent ground and grass gate

- Replaced three per-hole terrain underlays with one deterministic 37,810-triangle presentation height field spanning the persistent Pine Ridge property.
- Added three 96-triangle fairway ribbons, two 64-triangle green-to-next-tee connector trails, restrained off-route relief, and Gallery Lake basin grading to the same shared ground.
- Added a generated two-sided rough PBR grass material and 7,150 High-tier deterministic edge-grass clusters / 28,600 grass triangles with tee, green, water, camera, and gallery exclusions.
- Hid obsolete fairway/rough/dirt blockout render meshes only after shared-ground success while preserving every authoritative surface actor, trace, lie, collision, and penalty decision.
- Added `-GroundGrassSmokeTest` and the ground/grass bible; verified 76/76 automation, the persistent course, dense forest, water, 24/24 fixtures, six physics scenarios, seamless three-hole scoring, all artifact validators, and three real-D3D tee renders.

## Persistent course and dense-forest gate

- Converted Pine Ridge from per-hole reconstruction to one persistent shared world containing all three holes, terrain beds, forests, fixtures, wind zones, baskets, cameras, and Gallery Lake.
- Added stable manifest world origins/yaws and transformed each validated hole definition into course space without changing hole-local distances or competitive geometry.
- Changed round advancement to activate the next persistent hole and move play to its tee; broadcast cameras now choose the nearest mode anchor for the active release/basket pair.
- Added Brewster Ridge-, Northwood Black-, and Idlewild-informed original forest patterns with route-aware placement and explicit tee, green, water, camera, spectator, corridor, and spacing exclusions.
- Increased the High tier to 476/580/518 decorative firs for 1,574 total while preserving the unchanged 12/18/14 authored trunks as the only tree-contact authority.
- Added the persistent forest design bible and `-DenseForestSmokeTest`, then verified 76/76 automation, all three holes in one process, 53 fixtures, 24/24 fixture cases, six physics scenarios, water authority, final tee-view renders, and the unchanged 84.5 m reference flight.

## Gallery Lake water presentation gate

- Added `ADiscGolfWaterPresentationActor`, a deterministic collision-free 2,944-triangle elliptical surface with animated wave phase and deep/shallow/shore color bands.
- Generated the original `M_GalleryLakeWater` material through the reproducible Pine Ridge import pipeline and recorded it in the asset receipt.
- Preserved the authored water cylinder as the sole hazard, lie, collision, and penalty authority; its primitive mesh hides only after successful presentation setup.
- Hid landing-zone and spectator-boundary blockout markers in normal play while retaining their metadata and opt-in level-design review rendering.
- Added Gallery Lake runtime and visual-QA launch gates, then verified 76/76 automation tests, 24/24 fixture cases, all six frame-rate physics scenarios, artifact validators, and the unchanged 84.5 m reference flight.

## Needle Gate route-telemetry gate

- Added resumable schema-v1 telemetry for exactly 20 intentional attempts on each Needle Gate Primary, Risk/reward, and Bailout route.
- Recorded corridor adherence/deviation, landing hit and miss side, penalty, remaining distance, basket visibility, fixture contacts, disc/release data, optional route-understanding/next-shot ratings, and final score.
- Added automatic report persistence, compatible-session resume, per-route aggregate summaries, stable collision-profile identity, and an independent partial/complete validator.
- Added a Forest Broadcast telemetry panel and selected-route focus while keeping routes presentation metadata with no steering, physics, rules, collision, or scoring authority.
- Added two automation tests and a native runtime smoke gate; verified 76/76 automation, course assembly, 24/24 fixtures, six physics scenarios, trajectory artifacts, and the unchanged 84.5 m reference flight.
- Preserved an honest 0/60 starting report for the upcoming human playtest instead of manufacturing level-design conclusions.

## Production fixture presentation slice

- Expanded the reproducible Poly Haven pipeline to 35 verified CC0 source files across seven assets, adding Boulder 01, Shrub 04, and Weathered Planks at a 1K fixture baseline.
- Added `ADiscGolfFixturePresentationActor` for a component-fitted scanned boulder, deterministic quality-scaled HISM shrubs, and an original two-sided Pine Ridge sign with PBR wood.
- Kept every visual component collision-, overlap-, and navigation-free; authoritative typed proxies are hidden only after successful visual configuration and remain the sole gameplay contact source.
- Extended the authored course smoke to reject partial visual-family loads and verify all three presentation families plus collision invariance.
- Added `-FixturePresentationGallery` and completed a real-D3D12 1280x720 visual review.
- Verified 74/74 automation tests, 24/24 live fixture cases, all six frame-rate physics scenarios, trajectory artifacts, source validation, and the unchanged 84.5 m reference flight.

## Directed live fixture QA matrix

- Added `-FixtureCollisionSmokeTest` with 24 actual disc/fixture contacts covering four fixture types, three speed classes, and head-on/glancing approaches.
- Added fixture entry/exit velocity, impact normal, and entry/exit spin telemetry and serialized it in final schema-v3 trajectory data.
- Added timestamped schema-v1 fixture QA reports and an independent validator for matrix coverage, vector agreement, energy/spin loss, and angle-class behavior.
- Verified 24/24 live fixture scenarios, 74/74 automation tests, the six-scenario physics regression, trajectory artifacts, source validation, and the unchanged 84.5 m reference flight.

## Typed world-fixture collision pass

- Added `PineRidgeCompetitiveV2_Fixtures`, preserving the packaged v0.5 `PineRidgeCompetitiveV1` build as the pre-fixture comparison point.
- Added validated schema-v1 dense-grass, boulder, and sign definitions to every Pine Ridge hole; existing tree trunks now use the same typed fixture actor.
- Added deterministic solid-fixture response with separate normal restitution, tangential retention, and spin retention for tree, rock, and sign impacts.
- Added pass-through dense-grass overlap response at 56% speed and 64% spin retention while keeping decorative foliage collision-free.
- Added fixture contact telemetry to schema-v3 trajectory JSON and included fixtures in the quality-invariant collision signature.
- Added collision-dynamics automation and a live authored-course gate for exact actor counts, collision modes, channel responses, route features, and flyover state.
- Verified 74/74 automation tests, the live 15-fixture course smoke, the six-scenario physics regression, trajectory artifacts, and the unchanged 84.5 m independent reference flight.

## Forest Broadcast presentation direction

- Added a pure state resolver for Play, Flight, Review, Replay, and Complete HUD behavior with focused automation coverage.
- Made live Flight collapse the scorebug and suppress lie, setup, generic help, and developer telemetry while retaining a minimal flight strip.
- Limited live shot feedback to three seconds after authoritative release and gave Replay its own mutually exclusive chrome.
- Hid the tuning/performance overlay by default and added `DGT_ToggleDeveloperHud` plus `-DeveloperHUD` opt-in paths.
- Researched current official DGPT broadcast/event presentation and PGA TOUR 2K25's interactive shot-feedback grammar, then documented an original non-infringing synthesis.
- Added the Forest Broadcast presentation bible covering watch/play/review modes, palette, typography, hierarchy, motion, cameras, audio, accessibility, scalability, IP boundaries, and acceptance criteria.
- Applied the first Canvas HUD skin with course-derived pine neutrals, Signal Teal, Tournament Amber, warm paper text, tour-oriented score labels, and a dedicated shot-feedback card.
- Kept the presentation layer read-only with no changes to simulation, rules, collision, scoring, saves, or trajectory exports.

## Golfer animation foundation

- Added an optional non-colliding skeletal golfer mesh with automatic primitive fallback when no authored character is assigned.
- Added presentation-only Drive/Approach/Putt families and Setup/Windup/Release/Follow Through phases with bounded event-driven ticking.
- Made the first timing press start visual windup while authoritative GameMode release starts the visual release phase only after disc launch.
- Exposed release grade, timing direction, handedness, family, phase, and normalized phase time for future Animation Blueprint clips.
- Added scorecard/cancellation cleanup, HUD asset/state honesty, and deterministic animation-contract automation coverage.

## Production course-art contract

- Added a separate staged Pine Ridge presentation definition for biome identity, pending terrain/foliage/water asset paths, per-hole visual plans, and Low/Medium/High density/cull tiers.
- Bound every visual tier to one `PineRidgeCompetitiveV1` collision profile and reject any tier that can affect collision.
- Added deterministic surface/tree collision signatures proving that visual quality does not change competitive geometry.
- Exposed authored/fallback and asset-readiness status while leaving the primitive course playable when art assets are absent.

## Presentation performance gate

- Added a bounded, presentation-only rolling performance tracker with average FPS, P95/max frame time, hitch rate, and process physical-memory telemetry.
- Added provisional HP Omen PASS/WARN/FAIL thresholds and surfaced the live result in the development HUD without feeding any gameplay system.
- Added `DGT_ResetPerformanceTelemetry`, `DGT_CapturePerformance`, and `-PerformanceCaptureSeconds=N` for schema-v1 JSON evidence under `Saved/PerformanceCaptures`.
- Isolated automated performance captures from local practice saves and added deterministic budget/window automation coverage.

## Practice-session restoration

- Restored valid schema-v5 practice snapshots on ordinary startup, including authored course/hole selection, round scores, current lie, strokes, penalties, completion state, and selected mold/plastic.
- Migrated older profiles to the Apex/Tour equipment fallback and added atomic bag-selection validation so unavailable saved equipment cannot leave a partial selection.
- Added defensive identity, score, schema, coordinate, and course-distance checks so stale or malformed saves fall back to a clean start.
- Kept command-line course/hole overrides, visual captures, smoke tests, and physics regression runs isolated from local player saves.

## Full Game v0.1 — Codex handoff baseline

### Added
- Unreal Engine 5.8 source project and Windows development configuration.
- Runtime-generated primitive practice hole so the first build does not depend on custom binary assets.
- Five fictional disc molds and three plastic profiles.
- Fixed-step aerodynamic disc component with lift, drag, wind, angle of attack, signed spin, turn/fade stability moments, and gyroscopic precession.
- Backhand/forehand, power, hyzer/anhyzer, nose angle, launch angle, and two-tap timing controls.
- Stroke/lie loop, basket trigger, chase camera, wind director, developer HUD, save/profile scaffolding, and course metadata.
- Unreal automation tests for course effective distance and turn/fade stability-moment sign conventions.
- Fixed-rate trajectory sample buffer as the future source for tracer/replay/trajectory export.
- Source-only validator and a dependency-free reference-flight envelope smoke check.
- Codex instructions, milestone tasks, architecture, calibration, course, presentation, and test documentation.
- Browser Pilot 0.3 retained as a read-only behavior reference.

### Physics audit fixes before handoff
- Corrected aerodynamic lift/angle-of-attack sign conventions in the full-game solver.
- Defined nose angle relative to launch trajectory instead of the horizon.
- Replaced an unstable prototype explicit rigid-body angular-rate step with a spin-dominant gyroscopic precession model suitable for v0.1 calibration.
- Ensured high-speed turn and low-speed fade are opposing moments and backhand/forehand mirror through signed spin.

### Unreal 5.8.1 stabilization
- Updated game and editor targets to Unreal 5.8's V7 default build settings.
- Updated the sky-atmosphere include for the Unreal 5.8 header layout.
- Corrected the automation test's module-relative math header include.
- Verified a successful `DiscGolfTourEditor Win64 Development` build.
- Passed all four `DiscGolfTour.` Unreal automation tests.
- Launched the generated practice hole in Play-In-Editor and a standalone game window.

### Enhanced Input productionization
- Replaced legacy action/axis bindings with named Unreal Enhanced Input actions and a gameplay mapping context.
- Preserved all keyboard controls and added mouse throw/style controls plus a complete controller layout.
- Added per-action remapping metadata and registration with Unreal 5.8's Enhanced Input user-settings system.
- Added an optional `UDiscGolfInputConfig` asset contract for future authored `IA_*` and `IMC_*` content.
- Added a visible, logged runtime fallback when the asset is absent or incomplete, avoiding a missing-content crash.
- Added explicit 0.25 controller stick dead zones while keeping opposite keyboard axes cumulative.
- Added the `DiscGolfTour.Input.EnhancedInputFallback` automation test; all five project tests pass.
- Verified a clean Unreal Editor build and standalone runtime startup with the generated fallback context.
- Added a pause-safe Canvas controls/remapping screen opened by Escape or controller View/Back.
- Added action and binding-slot navigation, arbitrary key/axis capture, automatic per-player saving, selected-action reset, and full-profile reset.
- Added reserved-key, device-type, axis-type, and duplicate-binding validation with visible feedback.
- Registered user settings before applying the mapping context so saved remaps are available during the first control rebuild.
- Expanded the Enhanced Input test with ordered-action and remap-compatibility assertions.
- Visually verified the controls list, selection state, listening state, cancellation, and return to gameplay.

### Explicit release quality

- Replaced hidden timing modifiers in the flight launch path with an immutable `FThrowRelease` resolved once by shared deterministic math.
- Changed the timing needle to a clear left-to-right sweep with symmetric signed error around the target.
- Added Perfect/Great/Good/Poor grades and early/on-time/late feedback.
- Added bounded, monotonic timing effects for release speed, spin, aim, hyzer, nose, and launch angle while preserving the Perfect-release flight baseline.
- Mirrored timing-driven aim error between RHBH and RHFH while retaining the established signed-spin/bank conventions.
- Attached the resolved release to flight telemetry and retained the last release in GameMode for post-shot feedback.
- Reworked the timing HUD with nested quality bands, live grade preview, and an explanatory release card.
- Added seven release automation tests covering normalization, grade boundaries, neutral baseline, early/late symmetry, handedness, monotonic bounds, and input sanitization.
- Expanded the dependency-free reference flight check to cover release-quality invariants.
- Verified a clean Unreal 5.8.1 editor build and all twelve `DiscGolfTour.` automation tests.

### Surface-driven ground play

- Replaced the generic bounce/friction hit response with explicit Impact, Skip, Slide, Edge Roll, and Settled states inside the 240 Hz custom simulation.
- Added deterministic impact classification from speed, incidence angle, disc edge angle, spin magnitude, plastic restitution/friction, and surface profile.
- Added fairway, tee-pad, rough, dirt, and rock calibration profiles with distinct restitution, friction, spin retention, skip gates, deceleration, and settle thresholds.
- Added per-surface consecutive-skip budgets and an energy-absorbing skip-out transition to prevent collision chatter and implausibly long post-skip skids.
- Added slope-projected gravity, support probing, ledge-to-air transitions, slide/roll attitude targets, and a bounded ground-play timeout.
- Preserved the existing Base/Tour/Crystal modifiers so plastic can change skip classification and ground distance.
- Tagged the runtime regression course and added raised rough, dirt, and rock landing patches without binary assets.
- Expanded trajectory and live telemetry with ground state, surface, impact count/angles, ground time, and ground distance; final telemetry persists after settle.
- Added active and post-shot ground telemetry to the Canvas HUD plus concise transition logs.
- Added seven ground automation tests and expanded the dependency-free reference envelope with surface/plastic/handedness ground invariants.
- Verified a clean Unreal 5.8.1 editor build and all nineteen `DiscGolfTour.` automation tests.
- Verified the tuned state chain in standalone play: three fairway skips, one skip-out slide, and settle with no runtime error/assertion markers.

### Trajectory export and physics regression presets

- Added `UDiscTrajectorySubsystem` as the durable completed-shot capture and regression-report boundary.
- Raised default trajectory capture to the 240 Hz solver rate and added sampled wind plus cumulative contact count to each row.
- Added explicit ground-transition events with from/to state, surface, contact number, impact speed, incidence, and disc edge angle.
- Added self-contained schema-versioned JSON and CSV exports with resolved disc/aero data, immutable release, SI/world coordinates, live samples, transitions, final telemetry, and derived flight summary.
- Added timestamped capture history plus stable `LatestTrajectory.json/.csv` targets under `Saved/TrajectoryExports/`.
- Added four source-authored presets: matched calm Apex RHBH baselines at 30/60/120 FPS and a calm 60 FPS RHFH mirror.
- Added automatic preset envelopes, a cross-frame-rate comparison report, environment restoration, timestamped reports, and `LatestPhysicsRegression.json`.
- Added developer HUD status plus remappable C/G/H controls for preset selection, one-preset run, and the complete suite.
- Added three trajectory automation tests and an independent Python validator for presets, JSON/CSV parity, sample integrity, transitions, reports, envelopes, and frame-rate comparisons.
- Fixed the Unreal launch-angle world-boundary sign after the first export proved that positive authored launch was producing negative world-Z velocity; added a deterministic sign test.
- Verified the corrected calm baseline at 84.37 m air carry, 85.02 m final carry, 8.37 m apex, and identical 30/60/120 outcomes.
- Verified the four-scenario standalone suite passes and produces valid report/export artifacts without runtime error or assertion markers.

### Circle 1/Circle 2 putting and basket interaction

- Activated the existing Circle 1/Circle 2 lie boundaries as authoritative Drive/Circle 2 Putt/Circle 1 Putt shot contexts.
- Added putting-specific speed, spin, timing, angle, and launch ranges while leaving the verified drive release branch unchanged.
- Added distance-derived recommended putting power plus live HUD pace, distance, launch, and signed basket aim feedback.
- Replaced the speed-only catch trigger with deterministic closest-approach classification for center chains, weak/over-speed chains, top band, and tray.
- Added visible source-only chain strands and custom-solver deflection responses without introducing nondeterministic rigid-body chain physics.
- Added basket contact count/result to telemetry, schema-v2 JSON/CSV exports, regression summaries, and reports.
- Expanded the source-authored suite to six scenarios with verified seven-meter Circle 1 and fourteen-meter Circle 2 center-chain makes.
- Added five putting/basket automation tests; all twenty-eight `DiscGolfTour.` tests pass.
- Verified the full six-scenario standalone suite passes with the prior 30/60/120 drive results unchanged and no runtime error/assertion markers.

### Shot tracer and instant replay

- Added a live trajectory tracer that remains available after settle, colors flight and ground play distinctly, and can be toggled from keyboard or controller.
- Added deterministic, state-aware sample decimation that preserves capture endpoints and ground/contact boundaries while bounding drawing work to 320 points.
- Added a collision-free replay ghost and chase camera driven only by immutable recorded trajectory samples at 0.75x playback.
- Added binary-search replay interpolation for position, velocity, disc normal, spin, angle of attack, ground state, and presentation orientation.
- Added replay progress, rate, readiness, completion, and cancellation feedback to the HUD, including a dedicated progress bar.
- Added remappable `T`/Left Shoulder tracer and `V`/Right Shoulder replay actions, expanding the fallback layout to 39 mappings across 18 ordered actions.
- Added reset, cancellation, and natural-completion cleanup that destroys the replay actor and restores the player camera without touching simulation or scoring.
- Added four presentation automation tests and an independent latest-capture validator; all thirty-two `DiscGolfTour.` tests pass.
- Verified the source validators, Unreal 5.8.1 editor build, trajectory artifacts, six-scenario physics suite, and presentation capture contract.

### Three-mode live broadcast camera

- Added `ADiscBroadcastCameraDirector`, a camera-only actor that reads the live disc's recorded solver samples and owns no physics, collision response, scoring, lie, or export authority.
- Added deterministic `Launch`, `Fairway`, and `Finish` selection with monotonic transitions based on shot context, travel, altitude, descent, ground state, and distance to the basket.
- Added distinct launch, long-lens fairway, predicted landing, and basket-side putting plans with smoothed drive transitions and a deliberate close-putt camera cut.
- Added collision line-of-sight recovery plus static-mesh visual-bounds avoidance so collision-disabled tree crowns cannot swallow the camera.
- Integrated camera creation, view-target blending, settle/hole-out teardown, reset cleanup, fallback behavior, and live HUD mode/LOS diagnostics in GameMode.
- Fixed reset during a regression throw so it cancels the suite, clears its queue, restores the prior wind/frame cap, and reports a ready state instead of deadlocking future runs.
- Added five broadcast-camera automation tests; all thirty-seven `DiscGolfTour.` tests pass.
- Verified launch/fairway/finish coverage, close-putt framing, camera restoration, reset cleanup, and tree avoidance in standalone play.
- Re-ran the full six-scenario standalone suite after integration; every scenario passed with the authoritative simulation baselines unchanged.

### Cooked Primary Data Asset equipment catalog

- Added `UDiscMoldDataAsset` and `UDiscPlasticDataAsset` contracts with stable `DiscMold:<MoldId>` and `DiscPlastic:<PlasticId>` Primary Asset identities.
- Created and checked in five mold assets and three plastic assets under `/Game/Data/Discs` while preserving the calibrated source definitions as a deterministic recovery path.
- Reworked `UDiscCatalogSubsystem` to discover, synchronously load, sort, validate, and atomically select the complete cooked catalog during GameInstance initialization.
- Added validation for empty, duplicate, missing, non-finite, non-positive, and out-of-range definition fields; an invalid candidate can never partially mix with fallback rows.
- Replaced the plastic switch with validated data definitions and preserved every existing turn, fade, restitution, and friction modifier.
- Added an explicit HUD source indicator so runtime verification distinguishes `DATA ASSET` from `SOURCE FALLBACK`.
- Added an idempotent Unreal Python generator and editor-only scripting plugins for repeatable asset regeneration.
- Registered both Primary Asset types with Asset Manager, assigned `AlwaysCook`, and retained `/Game/Data/Discs` as an explicit package cook root.
- Staged `Data/PhysicsRegressionPresets.json` as a NonUFS runtime dependency so packaged builds retain all six regression scenarios.
- Added five catalog automation tests covering registration, exact 15-combination fallback parity, duplicate/missing rejection, and invalid-candidate fallback; all forty-two `DiscGolfTour.` tests pass.
- Completed a clean Windows build/cook/archive, confirmed all eight packages in the cooked referenced set, and launched the archived executable.
- Verified the packaged HUD loads `DATA ASSET` plus six presets and the packaged six-scenario suite passes with unchanged 30/60/120 FPS physics results and no runtime error/assertion markers.

### Authoritative course surfaces, lies, and penalties

- Added `ECourseSurfaceType`, `EDiscGolfPenaltyType`, `EDiscGolfReliefRule`, `FLieEffectProfile`, and the schema-v1 serializable `FDiscGolfLieState` contract.
- Added `DiscGolfCourseRules` as the side-effect-free rules boundary for course-to-ground mapping, distance-aware lie classification, lie effects, penalty resolution, display text, and one-meter in-bounds relief placement.
- Added typed `ADiscGolfCourseSurfaceActor` fixtures while retaining stable Actor tags and Physical Material naming compatibility for authored production terrain.
- Split physical ground response from gameplay identity: light/deep rough and hazard use rough landing physics, while OB uses fairway landing physics, without adding a second flight implementation.
- Added authoritative next-throw profiles: light rough 96% power / 110% timing sensitivity, deep rough 88% / 125%, and hazard 92% / 115%.
- Added +1 out-of-bounds with last-in-bounds relief and +1 hazard with play-from-result placement; penalty and total strokes increment once.
- Added backward trajectory search, boundary bisection, and one-meter legal-side relief for OB results.
- Added practice-round save snapshots with schema-v2 save data, current lie state, penalty strokes, selected equipment, and regression-run isolation.
- Expanded the primitive course with typed deep-rough, OB, and hazard fixtures plus `DGT_RunRulesFixture OB|Hazard` runtime acceptance commands.
- Expanded HUD and release telemetry with raw/playing course surface, resulting lie, penalty, relief, penalty strokes, and applied lie power/timing multipliers.
- Raised trajectory exports to schema version 3 and added rules data to JSON, CSV, samples, transitions, summaries, and validators. Physics preset/report schemas remain version 2.
- Added six rules automation tests covering surface mapping, lie classification, penalties, lie effects, OB relief, and save serialization; all 48 project tests pass.
- Verified unchanged 84.5 m reference-flight behavior and a penalty-free six-scenario 30/60/120 FPS suite.
- Built, cooked, and archived `Saved/PackagedCourseRulesMilestone/Windows/DiscGolfTour.exe`; the packaged suite, OB fixture, hazard fixture, schema validators, and log audit all pass with no fatal/assert/ensure/runtime-error marker.

## Pine Ridge authored gray-box v0.3

- Added schema-v1 `Data/PineRidgeHole1.json` plus a deterministic source fallback and strict validation for identity, counts, IDs, transforms, typed surfaces, trees, landing zones, camera modes, spectator boundaries, wind, and flyover endpoints.
- Built the 362 ft par-3 Pine Ridge Opening gray-box with 11 typed surfaces, 12 trees, two landing zones, three broadcast anchors, three spectator boundaries, two wind zones, and a six-point flyover.
- Added reusable course feature, local wind-zone, and flyover-route actors without moving flight, rules, scoring, or lie authority out of existing systems.
- Added runtime course load/toggle commands, `-Course=PineRidge`, safe generated-actor teardown/rebuild, source/feature HUD status, and active wind-zone reporting.
- Added remappable `K`/left-stick-click course toggle and `L`/right-stick-click flyover controls, expanding the fallback layout to 43 mappings across 20 actions.
- Made the broadcast director consume authored launch/fairway/finish locations and FOV while retaining deterministic mode selection, look-at, smoothing, and line-of-sight recovery.
- Made `AWindDirector` discover and deterministically apply sorted local zone modifiers while preserving no-zone regression behavior.
- Added the stationary world-space spline flyover, automatic camera return, and throw blocking during preview.
- Raised practice save data to schema v3 with course identity and load-time migration from older profiles.
- Kept rules fixtures and physics suites isolated by switching to the permanent regression course before execution.
- Fixed runtime course switching by making generated static-mesh components movable before assigning meshes; this preserves collision when rebuilding after `BeginPlay`.
- Added five authored-course automation tests; all 53 `DiscGolfTour.` tests pass.
- Added unattended authored-course, playable-drive, and regression-suite smoke flags. The playable drive verified 1,968 solver samples, 98.0 m final carry, authored camera usage, local wind entry, collision, and deep-rough lie resolution.
- Built/cooked/archived `Saved/PackagedPineRidgeMilestone/Windows/DiscGolfTour.exe` (983.3 MiB) with both external JSON contracts staged.
- Verified packaged course/flyover, playable drive, six-scenario regression suite, schema-v3 trajectory artifacts, schema-v2 report, presentation capture, and clean runtime logs.

## Three-hole round shell v0.4

- Added schema-v1 `Data/PineRidgeCourse.json` with ordered independent hole references, identity validation, duplicate/order guardrails, and expected total par 11.
- Added schema-v1 `Data/PineRidgeHole2.json` for Needle Gate, a roughly 640 ft par-4 wooded placement hole with 12 typed surfaces, 18 trees, two landing zones, three camera anchors, three spectator boundaries, two wind zones, and a seven-point flyover.
- Added schema-v1 `Data/PineRidgeHole3.json` for Gallery Lake, a roughly 725 ft par-4 downhill water-carry hole with 12 typed surfaces, 14 trees, two landing zones, three camera anchors, three spectator boundaries, two wind zones, and a seven-point flyover.
- Generalized `DiscGolfCourseDefinition` and `ADevCourseBootstrap` to load, validate, assemble, tear down, and rebuild any authored Pine Ridge hole while retaining the Hole 1 compatibility wrapper and exact C++ fallbacks.
- Added `DiscGolfRoundState` as a side-effect-free round contract for per-hole scores, totals, progression, labels, and completion guardrails.
- Added safe runtime hole transitions, direct `-Hole=N` starts, score recording on hole-out, next-hole flow, round restart, and rollback on failed transitions.
- Added `DGT_LoadHole`, `DGT_NextHole`, `DGT_Scorecard`, and `DGT_RestartRound` development commands.
- Added a Canvas scorecard with per-hole par/strokes/relative score, totals, round status, and a distinct round-complete surface.
- Added remappable `N`/Right Trigger next-hole and `Tab`/Left Trigger scorecard actions, expanding the fallback layout to 47 mappings across 22 actions.
- Raised practice saves to schema v4 with layout identity, current authored hole number, and serialized round state plus migration from older profiles.
- Added six manifest, authored-hole, fallback-coverage, round-progression, and guardrail automation tests; all 59 `DiscGolfTour.` tests pass.
- Added unattended `-ThreeHoleRoundSmokeTest` and deterministic visual-QA capture flags for direct holes, scorecard, and round completion.
- Verified editor three-hole completion at three strokes across par 11 (-8), authored course/flyover, a 1,968-sample playable drive, and the unchanged six-scenario physics suite.
- Built, cooked, and archived `Saved/PackagedThreeHoleMilestone/Windows/DiscGolfTour.exe` (approximately 984 MiB before runtime artifacts) with the course manifest, all three hole definitions, presets, and expected containers.
- Verified the packaged three-hole round, course/flyover, playable drive, physics suite, trajectory/report artifacts, and clean runtime logs.

## Production presentation foundation v0.5

- Deployed a coordinated supervisor, independent QA, HUD, and presentation-audio agent swarm with disjoint implementation ownership and protected gameplay boundaries.
- Reworked the Canvas HUD and scorecard for broadcast hierarchy, physical-canvas scrim coverage, bounded status text, and verified 1280x720/1920x1080 layouts.
- Added a pure throw-lifecycle gate and hardened scorecard, active-shot, next-hole, replay, flyover, completed-round, reset, and regression interactions.
- Added a deterministic semantic presentation-audio contract with immutable context, validation, stable IDs, deduplication keys, a bounded 64-event trace, and silent behavior without assets.
- Wired release, airborne flight, actual ground transitions, physical/course surfaces, water identity, basket outcomes, penalties, hole flow, round completion, replay, and flyover after authoritative gameplay mutations.
- Enforced regression presentation isolation and causal basket -> hole-completion -> round-completion ordering in runtime acceptance.
- Expanded automation from 59 to 66 tests; retained the accepted 84.5 m reference flight and six-scenario 30/60/120 FPS regression.
- Passed editor and packaged three-hole, authored-course/flyover, playable-drive, and regression gates with clean logs.
- Built and verified the separate Windows archive at `Saved/PackagedV05Foundation/Windows/DiscGolfTour.exe`; the sealed v0.4 archive was not modified.
- Captured and reviewed Needle Gate, Gallery Lake, scorecard, and round-complete visual QA images.
