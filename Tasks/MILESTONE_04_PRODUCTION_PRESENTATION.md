# Task - Milestone 04: Production Presentation Pass

## Objective

Replace the verified three-hole gray-box presentation with a coherent production-quality visual, animation, audio, and UI layer while preserving all gameplay contracts and deterministic acceptance gates.

## Required work

- Author scalable terrain, materials, foliage, and collision proxies for all three holes.
- Preserve intended gaps, water-carry readability, bailout routes, typed surfaces, and identical competitive collision across quality levels.
- Add a skeletal golfer with readable drive, approach, and putting animation coverage.
- Build an original scorebug, scorecard, hole intro, round-complete screen, controls UI, platform glyphs, and accessibility settings.
- Replace debug tracer/replay visuals with production presentation that still reads immutable trajectory samples only.
- Add environmental beds and deterministic hooks for throw, flight, ground, basket, gallery, and transition audio.
- Dress ropes, signage, tee areas, baskets, galleries, and camera-safe spectator zones.
- Define Omen performance budgets and capture repeatable visual/performance evidence.

## Constraints

- Do not alter the 240 Hz solver, accepted release/ground/basket tuning, typed rules, or regression geometry to solve an art problem.
- Keep `RegressionCourse` independently selectable.
- Keep authored course data and mutable round/save state separate.
- All v0.4 editor and packaged gates must continue to pass.

## Acceptance

A new player can play all three holes and understand route, lie, penalty, score, transition, and completion states without explanation. Visual-quality changes do not alter collision or results, all 59+ tests pass, editor/package smoke gates pass, and measured Omen frame-time/memory budgets are documented.
