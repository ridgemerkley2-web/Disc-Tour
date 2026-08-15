# Codex — Start Here

The repository is deliberately prepared so Codex can work on it as a normal engineering project. Open the **repository folder**, not an individual C++ file. Read `PROJECT_STATUS.md` before the first change.

## First Codex task: compile stabilization

Paste this as the first task after Unreal 5.8 is installed on the Omen:

> Read AGENTS.md, README.md, Docs/ARCHITECTURE.md, and Docs/PHYSICS_CALIBRATION.md. Run `python Scripts/validate_project.py` and `python Scripts/reference_flight_check.py`. Locate my Unreal Engine 5.8 installation and build `DiscGolfTourEditor` for Win64 Development using `Scripts/build-unreal.ps1`. Fix compile/UHT errors with the smallest changes possible; do not redesign the systems or alter flight tuning unless compilation requires it. Rebuild until it succeeds. Then run the `DiscGolfTour.` automation tests if possible. Finally report exactly what you changed, build/test results, and any runtime risks I should test in Play-In-Editor.

That first task is intentionally boring. A verified compile is the gate before Codex starts adding features.

## Second Codex task: runtime smoke test

After the editor build succeeds:

> Launch the project in Unreal 5.8 and verify the runtime-generated practice hole works in Play-In-Editor. Test: player spawn, A/D aim, W/S power, Q/E hyzer, Z/X nose, F backhand/forehand, 1-5 disc selection, P plastic, two-press Space release, chase camera, ground/tree collision, new lie, R reset, and basket completion. Fix runtime crashes or broken control flow. Do not add final art. Keep the primitive course as a fast regression test.

## Third Codex task: Enhanced Input productionization

**Status: completed and verified on Unreal Engine 5.8.1.** The source fallback and Canvas remapping screen remain intentional until authored binary input and production UI assets are introduced.

v0.1 intentionally uses config-backed input mappings because it allows a source-only project to run without binary input assets. Once the project compiles:

> Migrate the player controls to Unreal 5.8 Enhanced Input while preserving all existing controls and behavior. Use clear Input Actions and Mapping Contexts suitable for keyboard/mouse and controller remapping. Keep a developer-safe path so missing content assets produce an obvious error instead of a crash. Update docs and run build/tests.

Epic describes Enhanced Input as the UE5 system for advanced input and runtime control remapping, so it is the correct long-term input layer.

## Fourth Codex task: first real vertical-slice map

**Status: completed and packaged in v0.3.** The gameplay regression hole remains independently selectable, and Pine Ridge now loads through validated external/fallback data with all requested course features and unattended acceptance gates.

> Create the production vertical-slice course framework for Pine Ridge Hole 1. Keep the runtime primitive hole available as a test mode. Build explicit actors/data for tee, basket, OB/hazard, landing zones, lie surfaces, camera anchors, spectator boundaries, and environmental wind zones. Do not attempt final photorealistic art yet; use clean blockout geometry and prove the course-data pipeline first.

## Fifth Codex task: three-hole round shell

**Status: completed, packaged, and verified in v0.4.** The course manifest, Needle Gate, Gallery Lake, guarded transitions, round state, scorecard, save migration, editor/package acceptance, and visual QA are complete.

> Generalize the single-hole loader into a course manifest and build Needle Gate plus Gallery Lake with the validated v0.3 feature contracts. Add authoritative hole transitions, round-relative scoring, a simple scorecard, and round-complete/reset flow. Preserve Pine Ridge as an individual-hole target and the primitive regression course as the permanent physics/rules target. Add deterministic editor and packaged three-hole acceptance runs before production art.

## Sixth Codex task: production presentation pass

**Status: current next milestone.**

> Build v0.5 as a production presentation pass over the verified three-hole shell. Preserve the course manifest, typed collision, round authority, save schema, 240 Hz solver, and every existing acceptance gate. Replace gray-box terrain/foliage with a scalable authored visual pipeline, add a skeletal golfer and readable drive/putt animations, create original production scorebug/scorecard/hole-intro/accessibility UI, add environmental and gameplay audio, and dress ropes/signage/gallery zones. Establish visual and Omen performance budgets without changing deterministic physics or competitive collision across quality presets.

## Working style

Good Codex tasks for this project are scoped around one subsystem with a clear test. Examples:
- "Add edge-roll ground physics and a deterministic regression test."
- "Add Circle 1 putting mode without changing drive physics."
- "Add three broadcast camera anchors and automatic shot camera selection."
- "Map course surfaces into authoritative lies and add deterministic OB/hazard penalties without changing flight physics."

Bad tasks are vague and unbounded:
- "Finish the whole game."
- "Make it AAA."
- "Make physics perfect."

Use the roadmap to give Codex a sequence of verifiable chunks.
