# Disc Golf Tour Roadmap

## v0.1 - Source-first full-game foundation - COMPLETE

Moved the browser proof of concept into a buildable Unreal Engine 5.8 C++ project with fixed-step disc flight, a source-only practice course, throwing controls, basic scoring, camera, HUD, save scaffolding, validation, and tests.

## v0.2 - Simulation feel vertical slice - COMPLETE

Productionized Enhanced Input, deterministic release quality, trajectory export, surface-driven ground play, authoritative lies and penalties, putting and basket interaction, tracer/replay, broadcast cameras, cooked disc data, and a six-scenario 30/60/120 FPS regression suite.

## v0.3 - First authored hole blockout - COMPLETE

Delivered Pine Ridge Opening as a validated schema-v1 authored gray-box with typed surfaces, OB/hazard, landing zones, trees, local wind, camera anchors, spectator boundaries, and a flyover. The permanent primitive regression course remains independently selectable.

## v0.4 - Three-hole round shell - COMPLETE

Delivered the first complete round-shaped experience:

- schema-v1 Pine Ridge course manifest with ordered independent hole files;
- Pine Ridge Opening, Needle Gate, and Gallery Lake gray-box definitions;
- safe hole loading and transitions with direct-hole launch support;
- authoritative three-hole round scoring and completion guardrails;
- scorecard, round-complete, next-hole, and restart flows;
- schema-v5 practice saves containing course, hole, round state, lie, penalties, and selected equipment;
- 47 Enhanced Input mappings across 22 ordered actions;
- deterministic editor and packaged three-hole acceptance runs;
- a verified Windows package at `Saved/PackagedThreeHoleMilestone/Windows/DiscGolfTour.exe`.

The v0.4 exit gate is satisfied: the three-hole shell is understandable and playable without changing the accepted flight, ground, rules, putting, or regression baselines.

## Session 16 - Core playability technical gate - COMPLETE

The final integrated playability gate is accepted in fresh Editor run `d64ee8c4-e1a6-4b49-9d73-d9bb76c834e2` and fresh packaged run `07bb2b2a-be53-4686-bf67-088158e3da5c`. Each passed all 44 blocking checks across Smoke 11/11, CoreLoop 16/16, Round 7/7, and Persistence 10/10. Final post-package automation passed exactly 9/9 focused and 240/240 full, and the fresh non-iterative package remained unchanged through packaged acceptance.

This closes the bounded technical core-playability gate and allows continued polish under that rule. It does not close production, content, provenance, brand, legal, shipping, or public-release readiness; the exact fourteen inherited public-release blockers remain unresolved.

## Session 17 - Interactive round flow - COMPLETE

Replaced the visible-but-noninteractive Canvas-only front end/results path with a
focusable/clickable native widget driven by validated value snapshots and exact
action whitelists. Settings returns to its originating round-flow screen, gameplay
input is disabled while UI owns focus, Canvas remains a fail-safe fallback, and all
commands reuse the existing GameMode/round authority. Fresh builds, 9/9 focused and
249/249 full automation, rendered Editor gates, a fresh 985-package archive, and
packaged front-end/results gates pass. This is technical interaction closure only;
the fourteen release blockers and the v0.5 production-presentation exit gate remain
open.

## Session 18 - Poly Haven derived-runtime provenance - COMPLETE

Closed exactly `POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE` with a
source-controlled receipt that binds 35 verified CC0 inputs to 55 derived Unreal
packages and separately classifies the five project-original packages in the same
60-package Pine Ridge root. The receipt SHA-256 is
`13ACC125F5B442BE75DE405DBD76D43AA9381436624E12BDDBA3C7BA4F90D858`, and the
passing semantic report SHA-256 is
`1C3D6960E342B58143D55B42D701B18D19E17BFE8D118862BE38A93A15B74035`.
Focused automation passes 3/3, and full automation passes 252/252. Fresh archive
`63624166-aa24-49d0-a89f-74a125a36be2` retains all 55 derived plus five excluded
package identities in a 54-file / 1,918,214,604-byte archive. Its sorted tab-line
manifest SHA-256 is
`0877C04FF8911F0903077F8C68093BF8A783FB6939538D2BFD1E2888D35B1870`; the inner
executable SHA-256 is
`6051C15D6DCF14D4EE92B34215B79B7AA7BCADC5D8F6F5CA8AB24807E89D4346`.
The packaged Pine Ridge play smoke exits 0 with 1,968 samples, authored camera,
local wind, and ground contact. Immutable Session 18 evidence is retained under
`Evidence/Session18`; the adversarial validator suite passes 53/53.

This is one bounded provenance closure. The package result proves identity inclusion
only and does not classify every staged file, dependency, or IoStore chunk. Actual
staged-package provenance and exactly twelve other public-release blockers remain,
for an exact current total of thirteen.

## v0.5 - Production presentation pass - IN PROGRESS

Replace gray-box presentation while preserving the verified gameplay contracts:

Foundation complete:

- one continuous 37,810-triangle property-scale ground with five-rail PBR fairway/trail blends, a 576-triangle Gallery Lake soil bank, 7,150 High-tier slope-aligned grass clusters / 14,300 masked HISM cards across three wind-reactive species, and 1,980 litter clusters across two HISM variants, all presentation-only, distance-culled, and collision invariant;
- one persistent Pine Ridge course property containing all three world-space holes, the shared ground, forests, fixtures, baskets, cameras, wind zones, and Gallery Lake, with active-hole transitions instead of per-hole teardown;
- three original DGPT-informed forest patterns totaling 1,574 deterministic collision-free decorative firs at High around 44 unchanged authoritative trunks, with route, water, tee, green, camera, spectator, and spacing exclusions;
- original Forest Broadcast style bible plus first-pass Canvas palette, tour score hierarchy, and shot-feedback language;
- explicit Primary/Risk/reward/Bailout strategy routes for all three Pine Ridge holes, with validated landing-zone bindings and an opt-in collision-free review overlay;
- first quality-invariant world-fixture pass with typed tree, dense-grass, boulder, and sign response under `PineRidgeCompetitiveV2_Fixtures`;
- directed 24-scenario live fixture QA matrix with independently validated entry/exit velocity, impact-normal, spin, energy, and contact-angle evidence;
- first production fixture-art slice with a scanned CC0 boulder, deterministic masked-shrub instances, and an original PBR wood sign fitted over unchanged collision proxies;
- durable Poly Haven source-to-runtime provenance accepted for the exact 55-derived / five-project-original Pine Ridge package partition, with independent semantic validation and fresh packaged identity inclusion;
- original animated Gallery Lake water presentation fitted over the unchanged authoritative hazard, with dedicated runtime, render, and collision-invariance gates;
- resumable Needle Gate route-telemetry workflow with selected-route focus, exact 20-attempt caps, schema-v1 evidence, and independent partial/complete validation;
- responsive broadcast HUD and scorecard verified at 1280x720 and 1920x1080;
- safe throw/scorecard/transition/replay/flyover/regression input lifecycle;
- deterministic contextual presentation-event contract with bounded trace and regression isolation;
- editor and packaged verification with 66/66 automation and all four runtime gates;
- separate verified Windows archive at `Saved/PackagedV05Foundation/Windows/DiscGolfTour.exe`.
- bounded Session 15 Pine Ridge Hole 1 integration proof accepted in fresh Editor run `cf218615-ed96-46b9-8f55-f0976780eecf` and packaged run `6875a60a-5875-4de0-8988-9d8d0ffe2613`: one authoritative drive, natural Circle 2 continuation, two stable-instance Touch/Base putts, three total strokes, actual-sample replay/Throw Lab, settings/save continuity, 8/8 focused and 231/231 full automation, and zero-hitch 1080p D3D12 performance; fresh package `18d49b6c-2efc-4d38-b5e0-6f5d400aabc9` retained 54 manifest-bound files after 985 cooked and zero incremental skips.

Remaining production pass:

- Pine Ridge Hole 1 production environment benchmark complete at 361.9 ft / par 3: feathered authored ecology zones, approval-gated Fab asset binding/validation, deterministic performance statistics, synchronized environment wind, exact collision-proxy separation, and four live representative flight-route gates; final Fab import, proxy authoring, material adaptation, and visual sign-off remain;
- production environment architecture complete: 16-category data assets, course-scale PCG controller, authored zone/exclusion actors, generated HISM graph template, collision/interaction split, shared wind, and three quality presets; approved final Fab assets, full-course Landscape/World Partition authoring, per-species proxy preparation, and packaged profiling remain;
- human fixture plausibility calibration using authored-course repetitions and measured/reference video before any coefficient revision, plus collection and analysis of the real 60-attempt Needle Gate dataset before any route geometry revision;
- first CC0 PBR ground materials, continuous course-scale terrain, feathered biome edges, worn visual hole connectors, graded/dressed soil shoreline with rock/reed/deadfall accents, slope-aware wind/cull HISM grass/litter, dense route-aware fir composition, fitted boulder/brush/sign presentation, animated lake water, and typed fixture proxies complete; authored multi-blade/species ground-cover atlases or meshes, broader microsurface/erosion breakup, camera-distance fade and Omen profiling, connector navigation reconciliation, mature-conifer LOD/Nanite assets, and tree canopy/trunk visual reconciliation remain;
- player skeletal mesh, drive/putt animation set, and readable release poses: v007 is deterministic, authored as seven isolated assets, independently asset-validated, and accepted through both technical runtime lanes while `ActiveVersion` remains v006. The clavicle-aware solve closes release arm collapse without stretch, but the first uninterrupted 60 Hz review fails visual approval for upright pivoting, crossed feet, weak brace/weight transfer, and a rigid throwing-arm shelf. Replace or substantially revise the procedural motion using the authentic cleared Capture Manager/MetaHuman body-solve path, then complete calibrated palm/disc binding, ground-contact IK, compatible footwear/wardrobe, supported-target expansion, Shipping inventory, and human product-art/contact approval;
- tournament ropes, signage, spectators, and course dressing;
- original broadcast scorebug, scorecard, hole intro, replay, and accessibility UI;
- environmental audio, basket/ground reactions, and presentation hooks;
- graphics presets and performance budgets tuned on the Omen development floor;
- packaged 1920x1080 Omen gameplay preset and all-three-hole performance capture complete, including source-controlled fir/boulder LOD chains, near/deep canopy shadow partitioning, exact RHI/preset validation, and zero-hitch 95-120 FPS results;
- visual regression captures for intended gaps, water-carry readability, camera clearance, and UI safe zones.

**Exit gate:** a new player can play the complete three-hole round and read every route, result, transition, and score state through production-quality presentation.

Sessions 16–18 satisfy their bounded core-playability, interaction, and Poly Haven derived-runtime provenance gates. The v0.5 production exit gate remains open: exactly thirteen public-release blockers, including actual staged-package provenance, production character/motion/environment/audio/content approvals, brand clearance, and final product judgment, are unresolved.

## v0.6 - Round and tournament systems

Build 9/18-hole flow, layouts and pins, weather/time progression, leaderboard/AI-field simulation, event rules, practice modes, bag management, and broader save-reliability coverage.

## v0.7 - Career shell

Build player profile/progression, a fictional equipment ecosystem, event ladder, winnings/XP, goal systems, assists, and character/cosmetic hooks without turning deterministic physics into RPG randomness.

## v0.8 - Full first course

Build one 18-hole course to production standard. Validate World Partition/streaming, foliage/PCG, collision consistency, camera coverage, and performance.

## v0.9 - Alpha

Add enough courses and systems for external testing, then focus on accessibility, settings, controller support, telemetry, balance, and bug fixing.

## Post-alpha - Multiplayer / online layer

Evaluate asynchronous score challenges first, then card-based multiplayer. Replicate authoritative throw commands/results; never make physics depend on network update rate.

## 1.0 - Commercial-ready target

Requires final content scope, licensing decisions, audio/commentary plan, platform targets, legal review, QA, store/publishing work, localization/accessibility, and performance certification.
