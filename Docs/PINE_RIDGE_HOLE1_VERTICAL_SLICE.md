# Pine Ridge Hole 1 Gameplay Vertical Slice

Status: gameplay/presentation benchmark ready. Final environment visual acceptance remains pending Fab import.

## Player flow

Pine Ridge Opening is a par 3 measuring 361.855 feet from authored tee to basket and displayed as 362 FT (110 M in metric mode). A short, skippable introduction presents course, hole, par, and distance. The existing authored flyover remains optional and can be started or skipped with the course-preview input.

Normal play uses the production HUD only. It shows the compact scorebug, score relative to par, authoritative distance and wind, the selected fictional disc and flight ratings, shot number, putting-circle context, and release setup. Raw solver, collision, route, PCG, asset-source, and environment telemetry are restricted to the F9 developer overlay. The shot tracer is disabled by default.

After an authoritative settle, the result card classifies Fairway, Semi-Rough, Deep Rough, Green, Out of Bounds, or Hazard from existing lie/rule data. It displays carry, total, and remaining distance without frame-by-frame analytics. The existing rules implementation remains authoritative for penalties and relief. A short landing-camera hold masks relocation before the player camera blends to the new lie.

Hole completion shows the real result vocabulary (Ace through arbitrary over-par values), locks further throws, and keeps replay/scorecard optional. The scorecard lists authored holes only; unavailable future holes are not fabricated.

## Controls

- `Space` / gamepad bottom: continue the introduction; existing two-stage throw/release during play.
- `L` / right stick: start or skip the optional hole preview.
- `V` / right shoulder: play or cancel the last throw replay.
- `T` / left shoulder during replay: switch Tracking and Tee replay cameras.
- `Tab` / left trigger: scorecard.
- `Escape` / View: settings and controller-remapping menu.
- `F9`: developer HUD.

The pause screen supports keyboard and conventional gamepad navigation. Its Settings page includes graphics quality, resolution/window mode, audio values, mouse/controller sensitivity, invert Y, camera shake, HUD/basket-marker visibility, imperial/metric units, HUD/text scale, reduced motion, high-contrast marker, and aiming-indicator visibility. Display settings apply through `UGameUserSettings`; audio fields are persistent integration hooks for the eventual mix assets.

## Data and replay

`UDiscGolfThrowHistorySubsystem` retains a bounded 128-entry throw history. Each entry stores the hole/shot identity, selected disc, release parameters, wind, carry/total/remaining distance, landing classification, penalty, and lie. Full flight paths are not duplicated in history.

Last-shot replay uses recorded immutable trajectory samples and never reruns physics. `BuildBoundedReplaySamples` keeps endpoints and ground-state boundaries while capping the replay at 1,800 samples. Tracking and stationary tee-camera presentations are available.

## Hole authoring

`UDiscGolfHoleAuthoringUtility` is Editor-only. Blueprint/Python/Editor Utility callers can create a draft, assign tee/basket, draw the primary fairway spline, set par/clearance/width, assign intro camera and preview spline, calculate distance, and validate it. Validation identifies missing transforms, implausible distance, overlapping clearance, missing/mismatched splines and cameras, green obstructions, metadata errors, and duplicate hole numbers. It never creates Holes 2–18 automatically.

The current benchmark report is `Saved/CourseReports/PineRidgeHole1Validation.json`. It combines course geometry, environmental zone settings, strategic-tree count, OB coverage, camera/flyover status, and the four-route solver acceptance result.

## Known art limitation

FINAL ENVIRONMENT VISUAL ACCEPTANCE — PENDING FAB IMPORT.

The provisional forest remains intentionally in place. No marketplace content was downloaded, fabricated, substituted, or redistributed. Environment swaps continue through the existing asset abstraction layer and do not require changes to HUD, replay, throw history, rules, or hole-authoring code.
