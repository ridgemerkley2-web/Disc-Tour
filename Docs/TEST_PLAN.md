# Test Plan

## Hole 1 gameplay vertical slice acceptance

The active suite contains 96 passing tests. The new player-experience coverage verifies Hole 1 par/distance metadata, feet/meters conversion, score terminology including arbitrary over-par values, lie/OB result classification, replay sample bounds and state-boundary retention, basket-marker visibility settings, settings normalization, intro throw gating, hole-authoring validation, duplicate-number rejection, and generation of `Saved/CourseReports/PineRidgeHole1Validation.json`.

Current evidence: `Saved/Logs/Automation_VerticalSlice.log` (`96/96`, exit code 0), `Saved/Screenshots/WindowsEditor/PineRidgeHole1_VerticalSlice_Intro_Final_1280x720.png`, and `Saved/Screenshots/WindowsEditor/PineRidgeHole1_VerticalSlice_Gameplay_1280x720.png`.

## Production environment forest gate

Run `Scripts/create-production-environment-assets.py` through Unreal's Python commandlet, build the Editor target, run `DiscGolfTour.Environment.*`, then run the full `DiscGolfTour.` filter and existing Pine Ridge course/fixture/physics gates.

Acceptance:

- all 12 requested content folders exist;
- the TemperateMountainForest asset set exposes exactly 16 unique categories;
- the preset contains all six zone-rule families and ordered Performance/High/Cinematic density/range scales;
- the graph has 16 category branches, shared collision-ground sampling, density filtering, spacing/self-pruning, and collision-free HISM visual output;
- tee and green mature-tree density are zero, while deep rough exceeds fairway density;
- radial and spline clearances honor category-specific tree/brush setbacks and hard exclusions;
- canopy/shrub interaction remains query-only, nonblocking, and bounded to positive velocity/spin retention;
- no Marketplace/Fab path is hard-coded in PCG C++ logic;
- PCG asset generation reports zero errors and warnings;
- the existing reference flight, fixture matrix, persistent course, and six physics regressions do not change.

## v0.5 presentation-foundation release gate

The foundation is accepted only when protected gameplay hashes remain unchanged, source/reference checks pass, 66/66 Unreal automation tests pass, the editor and packaged three-hole/course/play/regression gates pass, HUD captures are readable at 1280x720 and 1920x1080, a packaged real-render/audio-device launch succeeds, and every final log is free of fatal/assert/ensure/crash/runtime-error markers.

The authoritative executable is `Saved/PackagedV05Foundation/Windows/DiscGolfTour.exe`. Final evidence is listed in `PROJECT_STATUS.md` and the swarm architecture record is `Docs/V05_FOUNDATION_SWARM.md`.

Presentation-specific assertions:

- player throw requests cannot bypass scorecard, active-disc, replay, flyover, completed-hole, or regression gates;
- the semantic trace is valid, contextual, deduplicatable, bounded to 64 entries, and unchanged during regression;
- each holed-out lifecycle is ordered basket outcome, hole completion, then optional round completion;
- hole transition precedes the next hole-start event;
- course contact preserves deep rough and distinguishes dry hazard from water;
- missing authored sound assets remain a silent success.

## v0.4 release gate

The three-hole round milestone is accepted only when source validation, Unreal automation, editor runtime gates, visual QA, cook/package, packaged runtime gates, artifact validation, and log audit all pass. The authoritative archived executable is `Saved/PackagedThreeHoleMilestone/Windows/DiscGolfTour.exe`.

Final verified result: 59/59 Unreal automation tests passed; all editor and packaged runtime gates passed; package artifacts validated; no fatal/assert/ensure/runtime-error marker was found.

## Fast source checks

Run before every engine build:

```powershell
python Scripts/validate_project.py
python Scripts/reference_flight_check.py
```

Expected current results:

- project validation passes and discovers 55 `.cpp` plus 47 `.h` files;
- course manifest lists exactly three ordered holes with total par 11;
- Hole 2 has exactly 12 surfaces and 18 trees;
- Hole 3 has exactly 12 surfaces and 14 trees;
- the reference-flight envelope passes at approximately 84.5 m carry.

The active post-foundation development suite contains 76 passing tests. Coverage includes save/equipment restoration, performance budgets, stable persistent-course placement, shared-ground/biome/grass/litter/shore-dressing HISM collision invariance and asset availability, golfer presentation lifecycle, authored strategy routes, route-telemetry geometry/reporting, typed fixture dynamics, production fixture-art invariance, dense-forest contracts, and Gallery Lake water collision invariance/material availability. Current evidence: `Saved/Logs/Automation_ShorelineWear_Final.log`.

## Build and automation

Build `DiscGolfTourEditor Win64 Development`, then run the `DiscGolfTour.` automation filter unattended.

Acceptance:

- compile and Unreal Header Tool complete successfully;
- 76 tests start, 76 succeed, zero fail or not-run;
- the automation queue empties normally;
- no fatal, assert, ensure, access violation, or runtime-error marker appears.

Current active-source evidence: `Saved/Logs/Automation_ShorelineWear_Final.log`. The sealed v0.5 package evidence remains `Saved/Logs/Automation_v05_Foundation_Final.log`.

Coverage includes course manifest parsing/validation, all three external/fallback hole contracts, fallback feature counts, round progression/completion guardrails, input layout, save serialization, rules, release, flight, ground play, putting/basket behavior, presentation, catalog, trajectory exports, and regression reports.

## Three-hole round acceptance

Run the editor and packaged executable with `-ThreeHoleRoundSmokeTest`.

Acceptance:

- the Pine Ridge manifest loads from authored JSON;
- Holes 1, 2, and 3 load in manifest order;
- all three holes already coexist at distinct validated world transforms, and each transition activates the next hole without rebuilding the course property;
- each deterministic smoke throw is recorded once;
- scoring progresses from -2 to -5 to -8;
- final result is three holes played, three strokes, zero penalties, par 11, score -8;
- the round completes only after all three holes have recorded scores;
- the process exits normally.

Evidence:

- editor: `Saved/Logs/ThreeHoleRoundSmoke_Editor_Final.log`;
- packaged: `Saved/Logs/ThreeHoleRoundSmoke_Packaged.log`.

## Authored-course acceptance

Run `-CourseSmokeTest` in editor and package.

Acceptance for each authored hole:

- definition validates and its expected feature counts are built;
- every authored tree, dense-grass, boulder, and sign fixture is present under the active collision profile;
- tree/rock/sign components block `WorldDynamic`; dense grass uses a dedicated `QueryOnly` volume that overlaps `WorldDynamic` while its visual mesh ignores collision;
- a successful fixture-art import creates exactly one rock, one dense-brush, and one sign presentation family; partial family availability is rejected;
- every fixture presentation component ignores collision, overlaps, and navigation, and a competitive proxy is hidden only after its replacement visual configures successfully;
- tee, basket, fairway, light rough, deep rough, OB, and hazard/water coverage exist;
- launch, fairway, and finish cameras exist;
- spectator boundaries, local wind zones, and flyover endpoints are valid;
- flyover starts, completes, and returns camera ownership;
- exactly three persistent authored-hole surface groups coexist with 53 authoritative collision fixtures, nine fixture-visual actors, 39 brush instances, and six wind zones;
- activation changes active hole/flyover/review/water pointers without generated-actor teardown or reconstruction.

Current persistent-course evidence: `Saved/Logs/CourseSmoke_PersistentDenseForest.log`. Historical v0.4 evidence remains at `Saved/Logs/CourseSmoke_v04.log` and `Saved/Logs/CourseSmoke_Packaged_v04.log`.

## Persistent dense-forest acceptance

Run `-DenseForestSmokeTest`, then capture real-D3D tee views for `-Hole=1`, `-Hole=2`, and `-Hole=3`.

Acceptance:

- one running world contains all three named forest patterns: `BrewsterRidgeTreeLine`, `NorthwoodBlackCompression`, and `IdlewildLakeFrame`;
- High tier produces exactly 476, 580, and 518 decorative instances for 1,574 total;
- all decorative HISM components use `NoCollision`, generate no overlaps, and do not affect navigation;
- the 12/18/14 authored trunks remain present for 44 total and their competitive collision signature is invariant;
- every route corridor, tee/green clearing, water buffer, camera pocket, spectator lane, and minimum tree-spacing exclusion remains clear;
- Needle Gate reads as the strongest compression while Gallery Lake's carry and dry bailout remain legible;
- missing or partial fir art fails safe without hiding authoritative trunk proxies.

Evidence: `Saved/Logs/DenseForestPersistentCourse_Final.log`, `Saved/Logs/DenseForestVisualQA_Final_Hole1.log`, `Saved/Logs/DenseForestVisualQA_Final_Hole2.log`, `Saved/Logs/DenseForestVisualQA_Final_Hole3.log`, and the matching `PineRidgeDenseForest_Final_Hole*_1280x720.png` screenshots.

## Persistent ground, biome, shoreline, and grass acceptance

Run `-GroundGrassSmokeTest` at High foliage quality, then capture real-rendered tee views for all three holes and the dedicated Gallery Lake shoreline view.

Acceptance:

- exactly one ground presentation actor serves all three persistent holes;
- the shared height field contains 37,810 triangles, three feathered fairway ribbons contain 1,152 triangles, two feathered connector trails contain 512 triangles, and Gallery Lake's graded bank contains 576 triangles;
- the connectors contain exactly 36 irregular wear patches / 288 triangles;
- Gallery Lake contains exactly 42 rock instances, 96 reed clusters / 384 stems, and 14 deadfall instances across three HISM variants;
- every shoreline accent remains at least 1,050 cm from all authored routes and outside tee, green, camera, spectator, and fixture exclusions;
- High quality produces exactly 7,150 deterministic slope-aligned grass clusters, 14,300 HISM cards, and 28,600 triangles across three wind-reactive species;
- High quality produces exactly 1,980 deterministic slope-aligned litter clusters and 3,960 triangles across two HISM variants;
- High quality uses a 112.5-300 m ground-cover cull range; grass rejects slopes above 32 degrees and litter rejects slopes above 38 degrees;
- terrain, shoreline, grass, and litter components use `NoCollision`, generate no overlaps, and cannot affect navigation;
- fairway, rough, dirt, hazard, water, and fixture authority actors remain present with unchanged collision/rules identity;
- obsolete ground blockout render meshes hide only after the complete shared presentation configures;
- grass/litter stay outside tee/green clearings, Gallery Lake and its bank, camera pockets, and spectator lanes;
- real rendered views have no material fallback, hard biome seam, broken bank, inverted faces, disconnected underlays, or exposed blockout sidewalls.

Evidence: `Saved/Logs/GroundShorelineWear_Final.log`, `Saved/Logs/CourseSmoke_ShorelineWear_Final.log`, `Saved/Logs/ShorelineDressingVisualQA_Final.log`, `Saved/Logs/TrailWearVisualQA_Final.log`, and the matching `PineRidgeShorelineDressing_Final_1280x720.png` and `PineRidgeTrailWear_Final_1280x720.png` screenshots.

## World-fixture dynamics acceptance

Run `DiscGolfTour.Fixtures.CollisionDynamics` as part of the full automation suite, then run the editor with `-game -FixtureCollisionSmokeTest`.

Acceptance:

- tree, boulder, sign, and generic solid contacts reflect a head-on disc away from the surface without increasing translational energy;
- rebound strength orders boulder above sign, sign above tree, and tree above the generic compatibility response;
- glancing rock contact retains tangential motion while losing total energy;
- dense grass preserves travel direction, passes through at 56% speed, and retains 64% spin;
- final schema-v3 telemetry records fixture contact count/type, entry and exit velocity, impact normal, and entry/exit spin;
- the live gate completes exactly 24 unique combinations: four fixture types by three speed classes by head-on/glancing approach;
- each live scenario records exactly one typed contact, loses translational energy and spin, achieves its intended angle class, and matches the pure deterministic response within tolerance;
- dense grass remains a direction-preserving overlap while tree, rock, and sign remain blocking sweeps;
- the independent reference flight and the six-scenario regression suite remain unchanged when no fixture is contacted.

Evidence: `Saved/Logs/FixtureCollisionSmoke_RouteTelemetry_Final.log`, `Saved/FixtureQaReports/LatestFixtureQa.json`, `Saved/Logs/Automation_RouteTelemetry_Final.log`, `Saved/Logs/RegressionSuiteSmoke_RouteTelemetry_Final.log`, and `Saved/PhysicsRegressionReports/LatestPhysicsRegression.json`.

## Needle Gate route-telemetry acceptance

Run `-NeedleGateRouteTelemetrySmokeTest`, then launch a human session with `-NeedleGateRouteTelemetry`.

Engineering acceptance:

- session identity is schema v1, Pine Ridge Championship, Hole 2, and `PineRidgeCompetitiveV2_Fixtures`;
- Primary, Risk/reward, and Bailout routes each cap at exactly 20 attempts;
- only a real first tee throw records a new route attempt;
- route selection changes HUD/review focus only and never changes throw, physics, collision, rules, or scoring;
- every attempt contains finite geometry, lie, penalty, visibility, fixture, release, and disc evidence;
- session-specific and latest reports update after each material event and compatible sessions resume;
- `Scripts/validate_route_telemetry.py` passes partial evidence, while `--require-complete` rejects anything except exact 20/20/20 coverage.

Human acceptance requires 60 real attempts. Understanding and next-shot-clear ratings must be entered by the tester; final score is recorded only when the hole is completed. Do not infer route balance from the empty or partial report.

Evidence: `Saved/Logs/NeedleGateRouteTelemetrySmoke_Final.log`, `Saved/RouteTelemetryReports/LatestRouteTelemetry.json`, `Saved/Logs/NeedleGateRouteTelemetryVisualQA_Final.log`, and `Saved/Screenshots/WindowsEditor/NeedleGateRouteTelemetry_Final_1280x720.png`.

## Gallery Lake water acceptance

Run `-GalleryLakeWaterSmokeTest`, then launch `-GalleryLakeWaterVisualQA -VisualQAScreenshot=GalleryLakeWater_Oblique_Final_1280x720` with real D3D12.

Acceptance:

- Hole 3 builds all 12 authored surfaces and exactly one `LakeWater` gameplay authority;
- the presentation mesh is ready with exactly 2,944 triangles and all collision, overlap, navigation, and shadow effects disabled;
- the authored water authority retains its actor, collision, `Hazard` identity, and penalty behavior after its primitive mesh is hidden;
- failure to load or configure the visual water leaves the authoritative primitive visible;
- the generated material loads without fallback or shader compilation error;
- all fixture, course, fixed-step physics, and reference-flight gates remain unchanged.

Evidence: `Saved/Logs/GalleryLakeWaterSmoke_Final.log`, `Saved/Logs/GalleryLakeWaterVisualQA_Final.log`, and `Saved/Screenshots/WindowsEditor/GalleryLakeWater_Oblique_Final_1280x720.png`.

## Playable-drive acceptance

Run `-PineRidgePlaySmokeTest` in editor and package.

Acceptance:

- one authored drive resolves through normal release, flight, collision, rules, telemetry, and lie paths;
- the shot records 1,968 solver samples and approximately 98.0 m final carry;
- authored broadcast-camera and local-wind-zone coverage are observed;
- at least one contact occurs and the result resolves to deep rough;
- no physics/rules regression fixture is used accidentally.

Evidence: `Saved/Logs/PineRidgePlaySmoke_v04.log` and `Saved/Logs/PineRidgePlaySmoke_Packaged_v04.log`.

## Physics regression acceptance

Run `-RegressionSuiteSmokeTest` in editor and package.

Acceptance:

- all six named drive/putting scenarios pass;
- matched 30/60/120 FPS drive captures agree within configured envelopes;
- putting scenarios retain required make/miss and basket-contact outcomes;
- course switching isolates the suite on `RegressionCourse`;
- no round score, rules penalty, or authored-course geometry contaminates results;
- previous frame cap and wind state restore after completion.

Evidence: `Saved/Logs/RegressionSuiteSmoke_v04.log`, `Saved/Logs/RegressionSuiteSmoke_Packaged_v04.log`, and `Saved/PhysicsRegressionReports/LatestPhysicsRegression.json` or the matching packaged artifact.

## Artifact validation

Run `Scripts/validate_trajectory_artifacts.py` against the latest JSON/CSV trajectory, regression report, and source presets. Acceptance requires schema integrity, sample monotonicity, JSON/CSV parity, transition consistency, six report scenarios, and all configured comparison/envelope results passing.

Run `Scripts/validate_fixture_qa.py` against `Saved/FixtureQaReports/LatestFixtureQa.json`. Acceptance requires the exact 24-combination matrix, the active collision-profile identity, one typed contact per case, finite normalized evidence, non-increasing energy, expected spin retention, angle-class coverage, and agreement between recorded and independently recomputed response vectors.

Current packaged artifact result:

- latest trajectory: 335 samples, zero ground transitions for the final captured scenario;
- regression report: six passing scenarios;
- preset source: six valid presets.

Use `Scripts/validate_presentation_capture.py` after any replay/tracer change.

Use `Scripts/validate_performance_capture.py` after a timed performance run. It verifies schema-v2 field
integrity, rendered D3D12, exact 1920x1080, actual GPU/RHI identity, the source-controlled Omen quality
profile, continuous authored flyover, 10-second warm-up, bounded samples, frame/FPS and hitch-rate consistency,
monotonic thresholds, and independently recomputes PASS/WARN/FAIL. NullRHI evidence is rejected; production
acceptance requires rendered packaged captures on the Omen for all three holes.

## Visual QA

Use `-VisualQAScreenshot=<name>` with `-Hole=2` and `-Hole=3`; add `-ShowScorecard` for the scorecard and `-CaptureRoundComplete` for the final state.

Use `-FixturePresentationGallery -VisualQAScreenshot=FixturePresentationGallery_1280x720` after any rock, shrub, sign, import, material, scaling, pivot, or masking change.

Use `-DenseForestVisualQA -Hole=<1|2|3> -VisualQAScreenshot=<name>` after any forest density, route exclusion, placement, scale, or asset change.

Use `-GroundGrassVisualQA -Hole=<1|2|3> -VisualQAScreenshot=<name>` after any shared-ground, fairway, connector, grass/litter, material, or terrain-height change. Add `-GroundCoverVisualQA` for the close route-edge card/mask/slope inspection. Use `-GalleryLakeWaterVisualQA -Hole=3` after any shoreline or lake-exclusion change.

Acceptance:

- Needle Gate's intended wooded corridor and landing-zone shape read from the player view;
- Gallery Lake's downhill water carry, bailout, and green read clearly;
- the Gallery Lake surface reads as water from the oblique gameplay-review camera with no cyan/gold blockout markers, material fallback, or visible authority cylinder top;
- scorecard rows, totals, selected state, and safe margins are legible at 1280x720;
- round-complete view shows all three played scores and correct -8 total;
- no camera is inside foliage or below the course;
- placeholder status remains visually honest.
- the fixture gallery shows recognizable rock relief, clean shrub alpha masking, readable sign lettering on both approach faces, and no visible primitive replacement mesh.
- close ground-cover QA shows non-rectangular masks, restrained species variation, terrain-normal alignment, visible leaf/needle breakup, and no dressing inside the playable route opening.

Evidence:

- `Saved/Screenshots/WindowsEditor/NeedleGateVisualQA_v04.png`;
- `Saved/Screenshots/WindowsEditor/GalleryLakeVisualQA_v04.png`;
- `Saved/Screenshots/WindowsEditor/RoundScorecardVisualQA_v04.png`;
- `Saved/Screenshots/WindowsEditor/RoundCompleteVisualQA_v04.png`.
- `Saved/Screenshots/WindowsEditor/FixturePresentationGallery_Final_1280x720.png`.
- `Saved/Screenshots/WindowsEditor/NeedleGateRouteTelemetry_Final_1280x720.png`.
- `Saved/Screenshots/WindowsEditor/GalleryLakeWater_Oblique_Final_1280x720.png`.
- `Saved/Screenshots/WindowsEditor/PineRidgeDenseForest_Final_Hole1_1280x720.png`, `PineRidgeDenseForest_Final_Hole2_1280x720.png`, and `PineRidgeDenseForest_Final_Hole3_1280x720.png`.
- `Saved/Screenshots/WindowsEditor/PineRidgeBiomeBlend_Final_Hole1_1280x720.png`, `PineRidgeBiomeBlend_Final_Hole2_1280x720.png`, `PineRidgeBiomeBlend_Final_Hole3_1280x720.png`, and `PineRidgeBiomeBlend_Final_Shoreline_1280x720.png`.

## Manual input and round-flow pass

The source fallback must report 47 mappings across 22 ordered actions.

1. Verify aim, power, hyzer, nose, throw style, five disc slots, plastic cycling, and two-press release on keyboard/mouse and controller.
2. Finish a hole and verify `N` / Right Trigger advances exactly once.
3. Verify next-hole input is rejected while a throw, flyover, replay, regression, or unfinished hole is active.
4. Toggle `Tab` / Left Trigger and verify scorecard input does not launch a throw.
5. Complete Hole 3 in a full round and verify round-complete plus `R` restart returns to a clean Hole 1.
6. Launch directly with `-Hole=3`; verify unplayed prior holes remain unplayed and cannot produce false round completion.
7. Open the remapping screen at 1280x720 and confirm all 22 actions fit, can be rebound, persist, and reset without footer overlap.

## Packaging acceptance

The archive must include:

- `DiscGolfTour.exe`;
- expected pak/ucas/utoc containers;
- `DiscGolfTour/Data/PineRidgeCourse.json`;
- `DiscGolfTour/Data/PineRidgeHole1.json`;
- `DiscGolfTour/Data/PineRidgeHole2.json`;
- `DiscGolfTour/Data/PineRidgeHole3.json`;
- `DiscGolfTour/Data/PhysicsRegressionPresets.json`;
- package-local runtime logs/artifacts from the final acceptance run;
- a verified package-local `SHA256SUMS.txt`.

After every package run, audit logs for `Fatal error`, `Assertion failed`, `ensure condition failed`, `Unhandled Exception`, `access violation`, and unexpected runtime `Error:` lines.

## Performance gate for v0.5

The current packaged 1920x1080 D3D12 gate passes all three holes with High foliage population, 10-second
warm-up, continuous authored traversal, zero hitches, and 95.9-120.2 average FPS. Preserve the three named
JSON artifacts and logs. After any terrain, foliage, water, lighting, camera, animation, or UI cost increase,
repeat all three captures plus dense-forest, ground, fixture, physics-regression, and three-hole smoke gates.
Visual scalability must never alter competitive collision geometry or fixed-step outcomes.

The next profiling expansion should add Unreal Insights or equivalent game/render/RHI/GPU thread timing,
draw-call/triangle counters, streaming-pool residency, and a thermal-soak packaged run. The lightweight gate
remains a regression tripwire, not a full platform certification.
