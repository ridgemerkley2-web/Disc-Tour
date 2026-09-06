# Disc Golf Tour - Unreal Full Game Project

This repository is the production starting point for a realistic disc-golf game with deep deterministic throwing, original courses, broadcast presentation, and long-term career/tournament systems. The browser pilot is retained only as a behavior reference under `Legacy/BrowserPilot_v0.3`.

## Current status

The v0.5 production-presentation foundation, persistent Pine Ridge course property, continuous shared ground, feathered PBR ground biomes, graded Gallery Lake shoreline, slope-aware wind-reactive HISM grass/litter, dense forest layout, environment/fixture/water slices, strategy routes, resumable Needle Gate playtest telemetry, typed world-fixture collision, and 24-scenario live fixture QA matrix are implemented on Unreal Engine 5.8.1 for Windows. The current throw-form physics pass also hardens stable equipment provenance, launch-before-score transactions, numerical/runtime envelopes, exact wind snapshots, callback-safe overlap movement, and synchronous tap-in contact. Active source uses `PineRidgeCompetitiveV2_Fixtures`; release remains blocked on measured, manual, provenance, legal, packaged-gameplay, and product acceptance.

- Play Pine Ridge Opening (par 3), Needle Gate (par 4), and Gallery Lake (par 4) as one ordered round inside one persistent course world.
- Load the schema-v1 course manifest, including stable shared-world placement, and three independent schema-v1 hole definitions with exact deterministic C++ fallbacks.
- Advance safely by activating the next world-space hole without rebuilding the property; retain round-relative scores, inspect a scorecard, and complete or restart the round.
- Toggle the authored course and the permanent primitive physics/rules regression course.
- Preview each authored hole through a course flyover and use authored launch/fairway/finish broadcast cameras.
- Review explicit Primary, Risk/reward, and Bailout corridors and their landing-zone/scoring intent without changing flight or collision.
- Record 20 intentional Needle Gate attempts on each exact route ID—`NeedlePlacement`, `LateCrosswindAttack`, and `LeftPitchOut`—into resumable, independently validated telemetry without aim assistance or route steering.
- Pick one of five fictional molds and three plastics from cooked Primary Data Assets with a validated source fallback.
- Aim, set power, hyzer/anhyzer, nose angle, and backhand/forehand through remappable Enhanced Input.
- Use a two-press timing release with Perfect/Great/Good/Poor grading.
- Fly through a custom 240 Hz fixed-step aerodynamic and gyroscopic-precession simulation.
- Land through deterministic impact, skip, slide, edge-roll, and settle states driven by surface and plastic.
- Resolve typed tee, fairway, rough, dirt, rock, out-of-bounds, hazard, and water-course surfaces through authoritative lie and penalty rules.
- Hit deterministic tree, boulder, and sign fixtures with distinct energy/spin response, or pass through explicit dense-grass volumes with speed/spin loss; decorative foliage remains collision-free.
- Enter distance-aware Circle 1/Circle 2 putting and resolve center chains, weak chains, top band, and tray outcomes.
- Watch a bounded live tracer, three-mode broadcast camera, and 0.75x replay without presentation affecting simulation.
- Read the responsive broadcast HUD and scorecard at 1280x720 through 1920x1080; scorecard, replay, flyover, active-shot, completion, and regression states now gate conflicting input.
- Move through distinct Play, Flight, Review, Replay, and Complete HUD states; use `DGT_ToggleDeveloperHud` in the console or `-DeveloperHUD` at launch for the normally hidden tuning overlay.
- Follow the original Forest Broadcast presentation bible for tour hierarchy, sparse shot setup, post-release feedback, cameras, motion, accessibility, and third-party IP boundaries.
- Render three DGPT-informed original forest patterns with 1,574 High-tier collision-free instanced firs across the persistent property while retaining all 44 authored trunk authorities, using verified multi-LOD chains, 260 m culling, and near-route shadow partitioning.
- Render one 37,810-triangle course-scale ground with 1,152 feathered fairway triangles, 512 feathered connector triangles plus 36 irregular wear patches, a 576-triangle soil shoreline dressed with 42 rocks, 96 reed clusters, and 14 deadfall pieces, 7,150 High-tier edge-grass clusters / 14,300 masked HISM cards across three wind-reactive species, and 1,980 forest-floor litter clusters across two HISM variants while sealed surfaces retain every lie and collision decision.
- Fit a scanned CC0 boulder, masked shrub clusters, and an original PBR wood sign over authoritative collision proxies without letting visual quality affect contact dynamics.
- Render Gallery Lake with an original animated 2,944-triangle water material while retaining the hidden authored hazard as the sole collision, lie, and penalty authority.
- Emit contextual, bounded presentation events for release, flight, physical/course-surface contact, basket result, penalty, hole flow, round completion, replay, and flyover without changing gameplay authority.
- In non-Shipping Development/Test builds, export completed throws as schema-v5 JSON/CSV with explicit handedness and deterministic wind-phase provenance, and run the schema-v3 six-scenario 30/60/120 FPS regression report. Shipping excludes the diagnostic presets, launcher, reports, and file export.
- Save and restore schema-v10 player-profile/practice state including layout, current hole, round scores, lie, penalties, and selected equipment.

Fresh candidate `S19_WindowsShipping_ThrowPhysicsAudit_20260826T040100Z` completed clean Shipping build/cook/stage/Pak/IoStore/archive and candidate verification as `PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING`. It is not promoted or release-ready: normal three-hole acceptance, candidate-bound supplemental receipts/scope rebinding, whole-archive provenance, and legal/distribution/product approvals remain pending. The sealed v0.5 and v0.4 comparison builds remain preserved at their documented paths.

## Engine target

- Unreal Engine 5.8 (verified with 5.8.1)
- Windows 10/11
- Visual Studio 2022 C++ toolchain
- DX12 / SM6
- Moderate development scalability for an HP Omen-class laptop

## First launch

1. Install Unreal Engine 5.8 and Visual Studio 2022 with Game Development with C++.
2. Place the repository at a short local path.
3. Run `Scripts\run-checks.bat`.
4. Generate Visual Studio project files if needed.
5. Open `DiscGolfTour.uproject`, allow Unreal to compile, and press Play.

The game launches Pine Ridge Hole 1 by default. Use `-Course=PineRidge -Hole=2` or `-Hole=3` for direct authored-hole development starts.

For level-design review, use `DGT_ToggleLevelDesignReview` in the developer console or add `-LevelDesignReview` at launch. Cyan is Primary, orange is Risk/reward, and green is Bailout.

For the directed collision gate, launch the editor commandlet with `-game -FixtureCollisionSmokeTest`. It exercises 24 real fixture contacts and writes `Saved/FixtureQaReports/LatestFixtureQa.json`; validate it with `python Scripts/validate_fixture_qa.py`.

For isolated fixture-art review, launch with `-FixturePresentationGallery -VisualQAScreenshot=FixturePresentationGallery_1280x720`. The gallery shows rock, brush, and sign presentation on the regression floor while leaving its gameplay geometry untouched.

For the Needle Gate playtest, launch with `-NeedleGateRouteTelemetry`. The latest compatible session resumes automatically and selects the first incomplete route. Use `DGT_SelectTelemetryRoute <routeId>`, `DGT_TelemetryTradeoffUnderstood 0|1` before the tee shot, and `DGT_TelemetryNextShotClear 0|1` after the lie. Reset between attempts with `R`; reports are written after every material change under `Saved/RouteTelemetryReports`. Validate a finished 60-attempt set with `python Scripts/validate_route_telemetry.py --require-complete`.

## Controls

| Keyboard / mouse | Action |
|---|---|
| A / D | Aim left/right |
| W / S | Power |
| Q / E | Hyzer / anhyzer |
| Z / X | Nose down / up |
| F or right mouse | Toggle RHBH / RHFH |
| 1-5 | Select disc |
| P | Cycle Base / Tour / Crystal plastic |
| Space or left mouse | Start timing; press again to release |
| R | Reset current hole / restart completed round |
| N | Advance to the next hole after completion |
| Tab | Toggle scorecard |
| K | Toggle Pine Ridge / regression course |
| L | Start authored course flyover |
| T | Toggle live/retained tracer |
| V | Start/cancel last-shot replay |
| C / G / H | Development/non-Shipping only: select preset / run preset / run full regression suite |
| Escape | Open controls and remapping screen |

Controller equivalents include sticks for shot setup, bottom face button for timing/release, Menu for reset, Right Trigger for next hole, Left Trigger for scorecard, shoulders for tracer/replay, and stick clicks for course/flyover. The complete source fallback contains 47 mappings across 22 remappable actions.

## Repository map

- `Source/DiscGolfTour/DiscFlightComponent.*` - fixed-step aerodynamic, precession, and ground-play simulation.
- `Source/DiscGolfTour/DiscGolfMath.h` - deterministic release, putting, basket, and shared math.
- `Source/DiscGolfTour/DiscGolfCourseRules.*` - typed surfaces, lies, effects, penalties, and relief.
- `Source/DiscGolfTour/DiscGolfCourseDefinition.*` - course manifest plus validated external/fallback authored-hole contracts.
- `Source/DiscGolfTour/DevCourseBootstrap.*` - persistent three-hole course assembly and active-hole switching without per-hole teardown.
- `Source/DiscGolfTour/DiscGolfWorldFixtureActor.*` - quality-invariant tree, dense-grass, rock, and sign proxies with validated collision channels.
- `Source/DiscGolfTour/DiscGolfFixturePresentationActor.*` - collision-free scanned rock, instanced shrub, and original PBR sign visuals fitted over sealed fixture proxies.
- `Source/DiscGolfTour/DiscGolfFixtureQaRunner.*` - headless 24-scenario live fixture-contact matrix and schema-v1 report writer.
- `Source/DiscGolfTour/DiscGolfLevelDesignReviewActor.*` - opt-in collision-free route, corridor, and landing-zone visualization.
- `Source/DiscGolfTour/DiscGolfRouteTelemetry.*` - observational route geometry evaluation plus resumable schema-v1 human playtest reports.
- `Source/DiscGolfTour/DiscGolfTerrainPresentationActor.*` - one deterministic presentation-only course height field with feathered fairway/trail biomes, Gallery Lake basin/shoreline grading, and quality-scaled slope-aligned HISM grass/litter with tier culling; it never owns collision.
- `Source/DiscGolfTour/DiscGolfWaterPresentationActor.*` - deterministic animated Gallery Lake surface fitted over, but never replacing, the authoritative water hazard.
- `Source/DiscGolfTour/DiscGolfRoundState.*` - side-effect-free round scoring, progression, and completion guardrails.
- `Source/DiscGolfTour/DiscGolfCoursePresentationDefinition.*` - presentation asset/quality tiers kept separate from invariant competitive collision.
- `Source/DiscGolfTour/DevCourseBootstrap.*` - authored-hole assembly and permanent primitive regression course.
- `Source/DiscGolfTour/DiscGolfTourGameMode.*` - throw, lie, course, hole-transition, round, scorecard, save, and camera authority.
- `Source/DiscGolfTour/DiscGolfInputConfig.*` - Enhanced Input actions, mappings, metadata, and source fallback.
- `Source/DiscGolfTour/DiscGolferPresentationComponent.*` - presentation-only drive/approach/putt animation family and phase state.
- `Source/DiscGolfTour/DiscBroadcastCameraDirector.*` - presentation-only live-shot camera.
- `Source/DiscGolfTour/DiscReplayActor.*` and `DiscGolfPresentationMath.h` - immutable-sample replay and tracer support.
- `Source/DiscGolfTour/DiscTrajectorySubsystem.*` - non-Shipping trajectory file export, preset diagnostics, schema-v3 regression reports, and Shipping-safe in-memory summaries.
- `Source/DiscGolfTour/DiscGolfHUD.*` - asset-free development HUD, scorecard, round-complete, and controls surfaces.
- `Source/DiscGolfTour/DiscGolfPresentationAudio.*` - pure semantic presentation-event vocabulary, context, validation, deduplication, and bounded trace support.
- `Docs/FOREST_BROADCAST_STYLE.md` - original production presentation art direction and interaction contract.
- `Docs/PINE_RIDGE_LEVEL_DESIGN.md` - route intent, scoring tradeoffs, review workflow, and geometry-change evidence gate.
- `Source/DiscGolfTour/DiscGolfGameplayGate.h` - pure throw-lifecycle gate shared by runtime and automation.
- `Source/DiscGolfTour/DiscGolfTourGameInstance.*` - schema-v10 save/profile foundation and migration.
- `Content/Data/Discs/` - five cooked mold assets and three cooked plastic assets.
- `Data/` - course manifest, three hole definitions, presentation contract, and source physics-regression presets; Shipping staging explicitly excludes the diagnostic preset file.
- `Data/PineRidgePresentation.json` - staged biome/art plan, partially populated asset paths, and collision-invariant quality tiers.
- `SourceArt/PineRidge/` - licensed source provenance, 35-file checksum manifest, and reproducible Poly Haven download/import pipeline.
- `Docs/PINE_RIDGE_ENVIRONMENT_ASSETS.md` - selected CC0 environment sources, import policy, and runtime ownership boundary.
- `Docs/PINE_RIDGE_FOREST_DESIGN.md` - one-map rule, DGPT-informed forest patterns, clearing/exclusion contract, density targets, and realism boundary.
- `Docs/PINE_RIDGE_GROUND_GRASS.md` - continuous-ground geometry, PBR material, connector, grass-density, exclusion, authority, QA, and realism contract.
- `Docs/PRODUCTION_ENVIRONMENT_FOREST.md` - production PCG forest architecture, course zones/exclusions, collision split, wind, optimization presets, and exact Fab hookup slots.
- `Docs/FAB_ASSET_MANIFEST.md` - final free Fab acquisition, provenance, import-root, collision, wind, and LOD/Nanite integration manifest.
- `Docs/PINE_RIDGE_HOLE1_VERTICAL_SLICE.md` - production Hole 1 HUD, intro, replay, throw history, settings, controls, and course-authoring workflow.
- `Docs/PINE_RIDGE_HOLE1_PRODUCTION.md` - 361.9-foot benchmark-hole strategy, production zone plan, collision contract, reports, and final visual acceptance gate.
- `Scripts/` - source, trajectory, fixture-QA, route-telemetry, presentation, performance, and reference-flight validators.
- `Docs/` - architecture, design, setup, roadmap, and test documentation.
- `Docs/PERFORMANCE_BUDGETS.md` - Omen frame-time/memory thresholds and repeatable capture contract.
- `PROJECT_STATUS.md` - exact handoff state, artifacts, limitations, and next action.

## Simulation convention

Unreal world coordinates are centimeters. Aerodynamics and ground response use SI units internally (meters, seconds, kilograms, Newtons, radians) and convert only at the world boundary. Preserve this convention and the fixed 240 Hz simulation step.

## What this is not yet

The three-hole round is not yet a visually finished commercial slice. Pine Ridge is now one persistent property with a continuous course-scale ground, feathered green-to-forest biomes, worn green-to-next-tee trails, a graded and dressed soil shoreline, slope-aware wind/cull HISM grass/litter, dense multi-LOD forest composition, fitted rock/brush/sign presentation, animated Gallery Lake water, and a passing historical packaged 1920x1080 Omen gate. Technical MetaHuman/motion assets and the RHBH gameplay binding exist, but manual animation/contact approval remains 0/8 and authored LH/forehand/putt coverage is not accepted. It still needs broader environment/art/audio/UI production work and normal fresh-Shipping gameplay acceptance. Basket chains are a deterministic gameplay approximation and disc coefficients are calibration seeds rather than measured laboratory data.

The exact next milestone is documented in `PROJECT_STATUS.md` and `ROADMAP.md`.
