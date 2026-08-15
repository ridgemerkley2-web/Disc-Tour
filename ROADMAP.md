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
- original animated Gallery Lake water presentation fitted over the unchanged authoritative hazard, with dedicated runtime, render, and collision-invariance gates;
- resumable Needle Gate route-telemetry workflow with selected-route focus, exact 20-attempt caps, schema-v1 evidence, and independent partial/complete validation;
- responsive broadcast HUD and scorecard verified at 1280x720 and 1920x1080;
- safe throw/scorecard/transition/replay/flyover/regression input lifecycle;
- deterministic contextual presentation-event contract with bounded trace and regression isolation;
- editor and packaged verification with 66/66 automation and all four runtime gates;
- separate verified Windows archive at `Saved/PackagedV05Foundation/Windows/DiscGolfTour.exe`.

Remaining production pass:

- Pine Ridge Hole 1 production environment benchmark complete at 361.9 ft / par 3: feathered authored ecology zones, approval-gated Fab asset binding/validation, deterministic performance statistics, synchronized environment wind, exact collision-proxy separation, and four live representative flight-route gates; final Fab import, proxy authoring, material adaptation, and visual sign-off remain;
- production environment architecture complete: 16-category data assets, course-scale PCG controller, authored zone/exclusion actors, generated HISM graph template, collision/interaction split, shared wind, and three quality presets; approved final Fab assets, full-course Landscape/World Partition authoring, per-species proxy preparation, and packaged profiling remain;
- human fixture plausibility calibration using authored-course repetitions and measured/reference video before any coefficient revision, plus collection and analysis of the real 60-attempt Needle Gate dataset before any route geometry revision;
- first CC0 PBR ground materials, continuous course-scale terrain, feathered biome edges, worn visual hole connectors, graded/dressed soil shoreline with rock/reed/deadfall accents, slope-aware wind/cull HISM grass/litter, dense route-aware fir composition, fitted boulder/brush/sign presentation, animated lake water, and typed fixture proxies complete; authored multi-blade/species ground-cover atlases or meshes, broader microsurface/erosion breakup, camera-distance fade and Omen profiling, connector navigation reconciliation, mature-conifer LOD/Nanite assets, and tree canopy/trunk visual reconciliation remain;
- player skeletal mesh, drive/putt animation set, and readable release poses;
- tournament ropes, signage, spectators, and course dressing;
- original broadcast scorebug, scorecard, hole intro, replay, and accessibility UI;
- environmental audio, basket/ground reactions, and presentation hooks;
- graphics presets and performance budgets tuned on the Omen development floor;
- packaged 1920x1080 Omen gameplay preset and all-three-hole performance capture complete, including source-controlled fir/boulder LOD chains, near/deep canopy shadow partitioning, exact RHI/preset validation, and zero-hitch 95-120 FPS results;
- visual regression captures for intended gaps, water-carry readability, camera clearance, and UI safe zones.

**Exit gate:** a new player can play the complete three-hole round and read every route, result, transition, and score state through production-quality presentation.

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
