# Test Plan

## Session 18 Poly Haven derived-runtime provenance closure

Session 18 must prove a durable source-to-runtime identity chain without rewriting
the immutable Session 9–17 history or broadening the result into whole-package
provenance. Run the controlled UE 5.8.1 full-Editor importer, the independent
semantic validator, the strict Session 18 validator and its mutation suite, exact
`DiscGolfTour.Session18.*` automation, the complete `DiscGolfTour.*` suite, and a
fresh non-iterative Development package.

Acceptance:

- the source manifest remains exact at 35 verified CC0 files;
- the durable receipt hash is
  `13ACC125F5B442BE75DE405DBD76D43AA9381436624E12BDDBA3C7BA4F90D858`;
- exactly 32 direct texture imports, five direct mesh imports, and 18 dependent
  materials/material instances form the 55-package Poly Haven-derived set;
- exactly five disjoint project-original packages complete the 60-package Pine
  Ridge runtime root, with no omission or double classification;
- independent semantic validation loads and passes all 60 packages; accepted report
  SHA-256 is
  `1C3D6960E342B58143D55B42D701B18D19E17BFE8D118862BE38A93A15B74035`;
- the strict mutation suite rejects altered authorities, paths, counts, mappings,
  hashes, JSON type confusion/overflow, blocker arrays, and release claims, with
  the accepted suite passing 53/53;
- focused automation passes exactly 3/3 and full automation passes exactly 252/252,
  with zero failures or not-run tests;
- fresh archive `63624166-aa24-49d0-a89f-74a125a36be2` contains 54 files /
  1,918,214,604 bytes, manifest SHA-256
  `0877C04FF8911F0903077F8C68093BF8A783FB6939538D2BFD1E2888D35B1870`,
  and inner executable SHA-256
  `6051C15D6DCF14D4EE92B34215B79B7AA7BCADC5D8F6F5CA8AB24807E89D4346`;
- all 55 derived and five excluded project-original package identities are present
  in the fresh package, while the three quarantined vendor roots remain absent;
- the fresh packaged Pine Ridge play smoke exits 0 with exactly one pass marker,
  1,968 samples, ground contact, authored camera, and local wind active;
- historical Session 9–17 validation and the protected save boundary remain exact.

The package check is identity inclusion evidence only. It does not equate source
`.uasset` hashes with cooked bytes, classify every staged file or dependency, cover
anonymous IoStore chunks, or close
`ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED`. Session 18 resolves one
blocker and leaves exactly thirteen; it is not legal, production-art, shipping, or
public-release approval.

## Session 17 interactive round-flow closure

Session 17 must prove actual native interaction rather than Canvas visibility alone.
Build both Development targets; run exact `DiscGolfTour.Session17.RoundFlow.*`
automation and the complete `DiscGolfTour.*` suite; run rendered D3D12 front-end and
three-hole-results processes from Editor and from a fresh non-iterative package.
Every live run uses a fresh external GUID `-UserDir`.

Acceptance:

- focused automation passes exactly 9/9 and full automation passes exactly 249/249,
  with zero warning-successes, failures, or not-run tests;
- front end and every scorecard/results state resolve from authoritative round data,
  reject contradictions and extra actions, and expose an enabled keyboard-focusable
  initial button;
- visible round flow owns GameAndUI input and cursor while gameplay mapping is
  disabled; hidden round flow restores GameOnly ownership;
- Settings hides the round-flow widget, takes precedence in input/rendering, and
  returns to the exact originating round-flow screen with focus restored;
- Start/Continue and final-results restart execute through the shared controller
  dispatcher and existing GameMode actions, leave one player, restore playable Hole
  1 state, and reject a duplicate action;
- Canvas renders only as a fallback when the native widget is unavailable;
- fresh package completes clean build/cook/stage/Pak/IoStore/archive and both
  packaged interaction processes emit the exact Session 17 PASS markers with exit
  0;
- project validation, reference flight, inherited Session 16 normal/self-test, save
  count/hash, and `git diff --check` remain clean.

Current accepted evidence is recorded in
`Docs/DG_SESSION17_INTERACTIVE_ROUND_FLOW_AUDIT.md`. This gate does not approve
production UI/art, accessibility certification, content, provenance, brand, legal,
shipping, or public-release readiness, and it does not change the fourteen-blocker
ledger.

## Session 16 core-playability technical closure

Session 16 is the current bounded technical playability gate. Build
`DiscGolfTourEditor Win64 Development` and `DiscGolfTour Win64 Development`; run the
exact `DiscGolfTour.Session16.*` focused filter and full `DiscGolfTour.*` automation;
produce a fresh non-iterative build/cook/stage/Pak/IoStore/archive; then run the same
44-check acceptance in Editor and from the fresh packaged executable. Every live run
uses a fresh absolute external GUID `-UserDir` and must preserve project Content,
project SaveGames, the protected profile, and, for packaged acceptance, the staged
package.

Acceptance:

- all 44 blocking checks pass in each mode: Smoke 11/11, CoreLoop 16/16, Round
  7/7, and Persistence 10/10;
- focused Session 16 automation succeeds exactly 9/9 and full `DiscGolfTour.*`
  automation succeeds exactly 240/240, with zero warning-successes, failures, or
  not-run tests;
- Editor Development and Game Development builds pass;
- the fresh non-iterative package completes build/cook/stage/Pak/IoStore/archive and
  its 54-file manifest remains unchanged through packaged acceptance;
- Editor acceptance `d64ee8c4-e1a6-4b49-9d73-d9bb76c834e2` and packaged acceptance
  `07bb2b2a-be53-4686-bf67-088158e3da5c` each emit
  `PASS_CORE_PLAYABILITY_GATE_RELEASE_BLOCKED` and report zero failed, blocked, or
  not-run checks;
- both modes preserve project boundaries and move their isolated external user
  directory recoverably after evidence collection;
- the protected profile remains exactly 5,212 bytes with SHA-256
  `A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`;
- technical closure does not authorize release: the exact inherited fourteen
  public-release blockers remain unresolved.

Current evidence:
`Saved/Session16Reports/Acceptance_editor_d64ee8c4-e1a6-4b49-9d73-d9bb76c834e2/PlayabilityReport.json`
(42,321 bytes, SHA-256
`303EE590BBDD8F3B3A3222782D4AE83B987528258440654D12E89FEB292CC4A4`),
`Saved/Session16Reports/Acceptance_packaged_07bb2b2a-be53-4686-bf67-088158e3da5c/PlayabilityReport.json`,
`Saved/Automation/Session16Focused_PostPackage_5eeeebc6-969b-4d99-a342-e62aeff522f6/index.json`
(SHA-256 `2703FB605C565C68A00403E49F883F236749B39003B51C491CEACEB7C53D4272`),
`Saved/Automation/FullRegression_Session16_PostPackage_d38a9e79-59d3-4852-bb95-81bd8f3097a6/index.json`,
SHA-256 `1D3B3E2F21C944F02BCF469B11FDB2FC5525C327D550C807226645F3F0A4F570`,
and the packaged manifest under the packaged acceptance `RuntimeEvidence` directory.

## Session 14 career and measured-flight AI acceptance

This historical checkpoint superseded Session 11 while preserving every earlier acceptance boundary. Build `DiscGolfTourEditor Win64 Development` and `DiscGolfTour Win64 Development`; run `DiscGolfTour.Session14.*` and full `DiscGolfTour.*`; run project/reference checks and the strict Session 14 validator in normal and release-required modes; then run `-Session14AIGolferSmokeTest` in Editor game and in a fresh non-iterative Development package. Every live invocation must use exactly one fresh absolute external `-UserDir=C:\DGTour_TestRuns\Session14\<GUID>` and must not leave a career slot or project SaveGames residue.

Acceptance:

- focused Session 14 automation succeeds exactly 17/17 (competition/career 7/7, AI 5/5, smoke foundation 5/5), and the full suite succeeds exactly 223/223, with zero warning-successes, failures, or not-run tests;
- one original generic three-hole StrokePlay event validates, project strokes that already include penalties convert without double-counting, and framework total/to-par exactly match project scoring;
- isolated schema-v1 career progress rejects duplicates, invalid histories, and future schemas atomically, while the existing schema-10 player profile and normalized settings authority remain unchanged;
- four detached candidates are measured through the real release resolver, current world/collision/wind, and authoritative fixed-step flight component; deterministic framework selection returns one command and only that command calls the existing `RequestThrow` path once, producing exactly one stroke of gameplay mutation;
- the live career proof round-trips one locally synthetic completed event through a disposable GUID slot, deletes it, and never passes the active gameplay round into career code;
- accepted Editor and packaged runs each emit `SESSION 14 CAREER TOURNAMENT SMOKE PASS` and `SESSION 14 AI GOLFER SMOKE PASS`, request exit status 0, and remove their external GUID run roots after evidence collection;
- a fresh package completes clean rebuild, non-iterative cook, stage, Pak, IoStore, and archive with 985 cooked, zero incrementally skipped, seven platform-skipped, and 992 total packages; its 54 files / 1,916,110,012 bytes match the retained per-file SHA-256 manifest;
- the protected profile finishes at exactly 5,212 bytes / SHA-256 `A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`, with no additional file in the project SaveGames directory;
- normal validation exits 0 as a bounded technical pass, release-required validation exits 2, and the exact ordered release-blocker count is thirteen, ending with `SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING`.

Current evidence: `Saved/Automation/Session14FocusedFinal_e06b4a59-f773-4ca6-9457-4cc57b0486fa/index.json`, `Saved/Automation/Session14FullFinal_133d78f7-5f28-4a0a-b104-865540d4e3d8/index.json`, `Saved/Logs/Session14EditorLiveFinal_9d148f0d-3a52-47b9-a970-2b4c8238027f.log`, `Saved/Logs/Session14PackagedLive_f9872bf4-8319-4625-8426-224fed282076.log`, `Saved/Session14Reports/Session14AIGolferPackagedSmoke_f9872bf4-8319-4625-8426-224fed282076.json`, `Saved/CareerReports/DG_Session14CareerAiTestingAudit.json`, `Saved/Session14Reports/Session14PackageManifest_c84db54d-7d6e-490e-a24b-9f0b0a20f413.json`, and `C:\DGTour_Packages\S14_CareerAI_c84db54d-7d6e-490e-a24b-9f0b0a20f413`.

## Session 11 equipment and Throw Lab acceptance

Build both `DiscGolfTourEditor Win64 Development` and `DiscGolfTour Win64 Development`, run the focused `DiscGolfTour.Session11.*` automation filter, then run the full `DiscGolfTour.` suite. Run the strict contract validator in normal and release-required modes, the project validator, the reference-flight check, a live Editor-game Throw Lab capture, a fresh non-iterative Development package, and the same capture smoke in the packaged executable using a disposable external UUID user directory.

Acceptance:

- focused Session 11 automation succeeds exactly 12/12 and the full suite succeeds exactly 164/164;
- five deterministic generic instances validate, retain stable IDs, persist selection/plastic/favorite state in the isolated schema-v1 equipment slot, and reject invalid loadouts atomically;
- the selected instance ID is snapshotted before release and resolved once through the existing catalog/launch path;
- the 175 g reference leaves catalog aerodynamics unchanged, non-baseline mass scales mass and both inertias only, and wear remains metadata-only;
- Throw Lab consumes one completed authoritative native capture, retains actual boundary samples within the 64-record/2,400-sample/512-transition caps, and supports select/pin/delete/two-record compare/replay/save/load;
- replay uses the existing replay actor and actual captured samples only; no alternate simulation, release, trajectory, rules, scoring, or replay authority exists;
- live and packaged runs both record 1,965 authoritative solver samples into 490 bounded samples and pass Pine Ridge smoke;
- fresh package cook/stage/Pak/IoStore/archive succeeds with 984 packages cooked and zero incrementally skipped;
- the protected schema-10 profile remains 5,212 bytes with SHA-256 `A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`;
- the Session 11 validator's 21 self-tests pass, normal mode exits 0 with ten blockers, and release-required mode exits 2.

Current evidence: `Saved/Automation/Session11FocusedPersistence_ebae02cb-31a5-47d3-bf8f-bd5402f84c27/index.json`, `Saved/Automation/Session11FullClosure_2ca2924c-7fe3-40ac-957b-66bf47c471fb/index.json`, `Saved/Logs/Session11Live_89df456b-cf09-4103-a430-c48f317ee25f.log`, `Saved/Logs/Session11PackagedFinal_2f108006-2f2d-4478-a36e-2063a9b7293c.log`, `Saved/EquipmentReports/DG_Session11EquipmentThrowLabAudit.json`, and `C:\DGTour_Packages\S11_EquipmentThrowLab_Final_03294b72-f2f8-469f-90aa-75e13a907f45`.

## Session 15 integrated Hole 1 vertical-slice gate

The historical Session 15 bounded technical gate is a four-process `Setup -> Drive -> Finish -> Verify` acceptance using exactly one fresh external GUID `UserDir`. Run the same gate in Editor Development and from a fresh packaged executable at D3D12 1920x1080. The implementation must use the existing creator/controller, MetaHuman backend, animated RHBH drive release, fixed-step flight, wind, camera/tracer, lie/rules/score, replay, Throw Lab, presentation-audio, save, settings, and bag authorities.

Acceptance:

- the generic original identity is active while premium presenting-brand content remains dormant;
- stable Apex/Tour and Touch/Base instances persist across fresh processes;
- one natural CenterPlacement drive settles in Circle 2, the Touch continuation settles in Circle 1, and the third actual throw is caught for a three-stroke Hole 1 score;
- normal replay and Throw Lab replay both consume actual samples, including the caught-basket terminal state;
- the packaged stabilized post-interaction render segment records at least 120 samples at p95 <= 22 ms, zero hitches, and <= 3.5 GiB physical memory;
- focused Session 15 automation is exactly 8/8 and full `DiscGolfTour` automation is exactly 231/231;
- a clean full cook reports 985 cooked packages and zero incrementally skipped packages;
- Editor and packaged acceptance both emit `PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED`;
- project `Content`, the staged package, and the protected 5,212-byte A999 profile remain unchanged;
- `python Scripts/validate_dg_session15_vertical_slice.py --require-technical` exits 0, while `--require-release` remains blocked by the ordered 14 production-readiness blockers.

Canonical evidence is bound in `Config/DG_Session15VerticalSliceContract.json`; the audit is `Docs/DG_SESSION15_VERTICAL_SLICE_AUDIT.md`, and visual captures are hashed in `Saved/Session15Reports/Session15VisualManifest.json`.

This animation evidence is deliberately narrow: only the right-handed backhand drive has accepted visible throw motion. Left-handed backhand, both hands' forehands, and all putts use the synchronous gameplay fallback. Physics and telemetry tests for those combinations are not visual-animation acceptance, and no mirrored left-hand, forehand, or putt montage may be claimed from this gate.

## Historical Hole 1 gameplay vertical slice acceptance

The historical Hole 1 suite contains 96 passing tests. Its player-experience coverage verifies Hole 1 par/distance metadata, feet/meters conversion, score terminology including arbitrary over-par values, lie/OB result classification, replay sample bounds and state-boundary retention, basket-marker visibility settings, settings normalization, intro throw gating, hole-authoring validation, duplicate-number rejection, and generation of `Saved/CourseReports/PineRidgeHole1Validation.json`.

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

- project validation passes and discovers 112 `.cpp` plus 92 `.h` files;
- course manifest lists exactly three ordered holes with total par 11;
- Hole 2 has exactly 12 surfaces and 18 trees;
- Hole 3 has exactly 12 surfaces and 14 trees;
- the reference-flight envelope passes at approximately 84.5 m carry.

The historical post-foundation development suite contains 76 passing tests. Coverage includes save/equipment restoration, performance budgets, stable persistent-course placement, shared-ground/biome/grass/litter/shore-dressing HISM collision invariance and asset availability, golfer presentation lifecycle, authored strategy routes, route-telemetry geometry/reporting, typed fixture dynamics, production fixture-art invariance, dense-forest contracts, and Gallery Lake water collision invariance/material availability. Historical evidence: `Saved/Logs/Automation_ShorelineWear_Final.log`.

## Build and automation

For the current gate, build both Development targets and run the `DiscGolfTour.` automation filter unattended. The 76- and 223-test assertions elsewhere in this document are historical checkpoints; the current active-source gate is 295 tests, with a focused 15-test physics filter.

Acceptance:

- compile and Unreal Header Tool complete successfully;
- the current 295 tests start and succeed with zero fail or not-run; focused `DiscGolfTour.Physics.*` succeeds 15/15; the historical checkpoints retain their recorded counts;
- the automation queue empties normally;
- no fatal, assert, ensure, access violation, or runtime-error marker appears.

Current active-source evidence: `C:\DGTour_TestRuns\ThrowPhysicsAuditFinalIntegrity_20260826T043510Z\Automation_Full.log` (295/295, exit 0, zero saves) and `C:\DGTour_TestRuns\ThrowPhysicsAuditPostShipping_20260826T041900Z\Physics\Automation_Physics.log` (15/15, exit 0). Historical Session 14, shoreline, and sealed v0.5 evidence retains its original checkpoint meaning.

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

- one running world contains all three project-original forest patterns: `OpeningBroadTreeLine`, `NeedleCanopyCompression`, and `GalleryLakeFrame`;
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
- final trajectory schema v5 telemetry records handedness and deterministic wind-phase provenance plus fixture contact count/type, entry and exit velocity, impact normal, and entry/exit spin;
- the live gate completes exactly 24 unique combinations: four fixture types by three speed classes by head-on/glancing approach;
- each live scenario records exactly one typed contact, loses translational energy and spin, achieves its intended angle class, and matches the pure deterministic response within tolerance;
- dense grass remains a direction-preserving overlap while tree, rock, and sign remain blocking sweeps;
- the independent reference flight and the six-scenario regression suite remain unchanged when no fixture is contacted.

Evidence: `Saved/Logs/FixtureCollisionSmoke_RouteTelemetry_Final.log`, `Saved/FixtureQaReports/LatestFixtureQa.json`, `Saved/Logs/Automation_RouteTelemetry_Final.log`, `Saved/Logs/RegressionSuiteSmoke_RouteTelemetry_Final.log`, and `Saved/PhysicsRegressionReports/LatestPhysicsRegression.json`.

## Needle Gate route-telemetry acceptance

Run `-NeedleGateRouteTelemetrySmokeTest`, then launch a human session with `-NeedleGateRouteTelemetry`.

Engineering acceptance:

- session identity is schema v1, Pine Ridge Championship, Hole 2, and `PineRidgeCompetitiveV2_Fixtures`;
- exact route IDs `NeedlePlacement`, `LateCrosswindAttack`, and `LeftPitchOut` each cap at exactly 20 attempts;
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

Run `-RegressionSuiteSmokeTest` in Editor and, when package-level diagnostic coverage is required, in a non-Shipping Development/Test package. Use a fresh external `-UserDir` and explicit launch-time `-NoLoadExistingSave -DGNoProfileWrites`.

Acceptance:

- all six named drive/putting scenarios pass;
- matched 30/60/120 FPS drive captures agree within configured envelopes;
- putting scenarios retain required make/miss and basket-contact outcomes;
- player launch validates the exact stable equipment instance, coherent mass, mold/plastic, player handedness, and authoritative shot context; an animated transaction prevents bag mutation;
- spawn/configure/`Throw()` remain provisional and stroke, accepted-shot sequence, feedback, telemetry, camera, and presentation commit only after authoritative flight acceptance; invalid configuration or launch rolls back cleanly;
- launch captures an exact ordered director/base/gust/local-zone wind snapshot, rechecks it after launch callbacks, and fails closed on later drift;
- fixed-step configuration, render-delta contribution, backlog, velocity, acceleration, displacement, spin, wind, contact, and timeout limits reject invalid/non-finite state;
- deferred airborne/ground/catch movement publishes the applied authoritative transform before overlap callbacks, while a later external transform mutation still terminates/restores fail-closed;
- a disc starting inside the basket overlap is evaluated synchronously at solver time zero without a render tick;
- course switching isolates the suite on `RegressionCourse`;
- no round score, rules penalty, or authored-course geometry contaminates results;
- no existing profile is loaded and no `.sav` file is created or changed in the isolated user directory;
- previous frame cap and wind state restore after completion.

The six source-controlled regression presets are right-handed compatibility baselines. In particular, the 30/60/120 FPS Apex cases remain RHBH and retain their existing numeric envelopes; adding left-handed physics coverage must not silently relabel or retune them.

Current evidence: `C:\DGTour_TestRuns\ThrowPhysicsAuditPostShipping_20260826T041900Z\Regression\RegressionSuite.log`, `Saved\PhysicsRegressionReports\LatestPhysicsRegression.json` beneath that isolated UserDir, and the matching `Saved\TrajectoryExports` JSON/CSV files. The run passes 6/6 with preset schema v2, regression-report schema v3, trajectory schema v5, canonical preset SHA-1 `193C48DBCEDDBC629FA5873892F061C30BA4FF21`, and zero `.sav` files. The `Saved/Logs/*v04*` captures are historical compatibility evidence only.

The Shipping target intentionally excludes the regression smoke launcher, canonical preset staging, developer runners, regression-report path, and trajectory JSON/CSV file output. Shipping acceptance therefore uses normal throw/gameplay execution plus build and content-policy validation: confirm the shared runtime release, flight, ground, basket, and in-memory trajectory-summary paths function, and confirm the excluded diagnostic launchers, presets, reports, and exports remain absent. Do not report `RegressionSuiteSmokeTest` or trajectory-file serialization as having run in Shipping.

## Throw handedness physics and presentation acceptance

Run `DiscGolfTour.Release.HandednessMirrorsAim`, `DiscGolfTour.Release.CommandContract.Valid`, `DiscGolfTour.Release.CommandContract.RejectsInvalid`, and `DiscGolfTour.Trajectory.Serialization`, then run the focused release/physics/trajectory filters and `-RegressionSuiteSmokeTest`.

Acceptance:

- the combined rotation sign is exactly RHBH `+1`, LHFH `+1`, RHFH `-1`, and LHBH `-1`;
- lateral timing error, authored hyzer bank, and signed axial spin use that same sign, while changing hand alone preserves release-speed magnitude, spin-RPM magnitude, nose feedback, and launch-angle feedback;
- an unknown handedness enum is rejected at the authoritative command boundary before disc spawn, stroke mutation, or trajectory export;
- a non-finite, zero, vertical-only, or otherwise unusable horizontal aim direction is rejected at that same boundary because launch elevation has its own field;
- a valid aim containing a finite Z component is canonicalized into the XY plane in the immutable release, so exported direction provenance exactly matches the flight component's simulated heading;
- the existing six right-handed regression scenarios, including the 30/60/120 FPS RHBH baseline, remain within their established envelopes;
- every new trajectory export is schema v5; JSON release/summary/final telemetry and CSV metadata agree on a finite deterministic wind phase in `[0, 4096)`, JSON `release.handedness` is exactly `Right` or `Left`, CSV contains the matching `# handedness=Right|Left` line, and validators reject missing, unknown, mismatched, or legacy provenance;
- authored right-handed backhand Drive, Approach, and Putt families may use the guarded animation route; LHBH, RHFH, and LHFH must complete through the synchronous gameplay fallback, and no route claims final visible-motion acceptance until the manual production-motion review passes.

## v006 production-motion recovery and live-flight closure

Run the focused `DiscGolfTour.Physics.SolverBoundary.LauncherCollisionExclusion`, `GroundSupportLossTelemetrySync`, and `RuntimeSafety` tests, the production-motion and recipe self-tests, native read-only v006 asset validation, and the MetaHuman production visual runner on a fresh Editor binary and isolated `UserDir`.

Acceptance requires exactly five 1920x1080 checkpoint PNGs; one legitimate montage instance whose committed engine ID, anim-instance path, montage path, actual blend weight, and desired blend weight are recorded at every checkpoint; finite actual and desired weights each within 0.999–1.001; no pose seek, playback-rate override, or synthetic phase injection; every checkpoint within 0.75 frame of its target; Recovery at frame 132 with its curve gate active; one release and one stroke; natural `ThrowFinished` recovery; a terminal non-invalid flight with positive airborne carry, positive duration, internally consistent samples/contacts, and fresh trajectory publication; capture-lane trajectory deferral followed by discard; and zero trajectory JSON/CSV files. The moving disc root may ignore exactly the launching golfer and must retain collision with every unrelated actor, course surface, and fixture.

Current schema-v5 technical evidence passes at `C:\DGTour_TestRuns\AnimationFluidity\MetaHumanProductionVisual-v006-ExactWeightProof-db7d5db0-9109-49af-9a79-99fe2847220f`: all five exact-instance actual/desired weights are 1.000000; Recovery is frame 131.949/132 with `DG_RecoveryBeatAlpha=0.9989`; flight is 1,723 samples over 7.175 seconds with 68.46 m airborne and 76.37 m final carry; four ground contacts settle it; five PNGs and no trajectory JSON/CSV are present. This does not approve visual realism. Human review must still reject or correct arm/torso and clothing intersection, hand/disc ergonomics, foot/ground penetration, deformation, cloth/hair behavior, and between-checkpoint temporal continuity.

## Artifact validation

Run `Scripts/validate_trajectory_artifacts.py` against the latest JSON/CSV trajectory, regression report, and source presets. Acceptance requires trajectory schema v5, preset schema v2, report schema v3, `run_state=completed`, `passed=true`, authoritative presets loaded from canonical SHA-1 `193C48DBCEDDBC629FA5873892F061C30BA4FF21`, an unchanged presentation trace, valid and matching JSON/CSV handedness and deterministic wind-phase provenance, sample monotonicity, JSON/CSV parity, transition consistency, six report scenarios with distinct valid phase origins, and all configured comparison/envelope results passing. Historical schema-v3/v4 captures are retained evidence but cannot satisfy the current artifact gate.

### Non-automated physics and motion gates

Automated solver/regression success does not satisfy measured realism or play-feel approval. Release calibration requires the complete real 100-throw measured-reference dataset plus its named capture-quality, physics, playtest, and product approvals. The empty/partial template must continue to fail `--require-complete`.

Technical MetaHuman/motion assets, cook presence, and binding tests do not approve animation or disc/hand contact. The eight named manual review gates remain separately required; an honest 0/8 result is blocking evidence, not an automation failure.

Run `Scripts/validate_fixture_qa.py` against `Saved/FixtureQaReports/LatestFixtureQa.json`. Acceptance requires the exact 24-combination matrix, the active collision-profile identity, one typed contact per case, finite normalized evidence, non-increasing energy, expected spin retention, angle-class coverage, and agreement between recorded and independently recomputed response vectors.

Historical packaged schema-v3 artifact result, retained for provenance only:

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
5. Complete Hole 3 in a full round and verify round-complete plus `N` / Right Trigger restart returns to a clean Hole 1.
6. Launch directly with `-Hole=3`; verify unplayed prior holes remain unplayed and cannot produce false round completion.
7. Open the remapping screen at 1280x720 and confirm all 22 actions fit, can be rebound, persist, and reset without footer overlap.

## Packaging acceptance

Every Shipping archive must include:

- `DiscGolfTour.exe`;
- expected pak/ucas/utoc containers;
- `DiscGolfTour/Data/PineRidgeCourse.json`;
- `DiscGolfTour/Data/PineRidgeHole1.json`;
- `DiscGolfTour/Data/PineRidgeHole2.json`;
- `DiscGolfTour/Data/PineRidgeHole3.json`;
- candidate-bound UFS/NonUFS manifests and `NOTICES.txt`;
- the expected Pak/IoStore identities and a candidate verification receipt.

Shipping must exclude `PhysicsRegressionPresets.json`, the regression launcher, developer runners, regression reports, trajectory JSON/CSV, and package-local diagnostic output. Development/Test diagnostic packages may stage presets and generate runtime artifacts, but those contents are not Shipping requirements and must never be cited as Shipping regression execution.

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
