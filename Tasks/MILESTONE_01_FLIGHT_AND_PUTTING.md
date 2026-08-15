# Task — Milestone 01: Flight, Ground Play, Putting

## Objective
Make the Unreal gameplay feel superior to browser Pilot 0.3 before production art.

## Work packages
1. Trajectory capture/export (complete: automatic versioned 240 Hz CSV/JSON with ground transitions and validation).
2. Explicit, explainable release-quality model (complete).
3. Physics regression presets (complete: source-authored scenarios, per-run envelopes, HUD controls, and 30/60/120 suite report).
4. Surface-driven landing response and authoritative lie rules (complete for typed tee, fairway, light/deep rough, dirt, rock, OB, and hazard; includes effects, penalties, relief, saves, telemetry, fixtures, and tests).
5. Skip/slide/edge-roll states (complete with fixed-step motion, slope support, telemetry, and tests).
6. Putting context inside configurable radius (complete: Circle 1/Circle 2 release model, pace/aim feedback, HUD, and regression presets).
7. Better basket/chains approximation (complete: center catches plus deterministic weak-chain, band, and tray rejection outcomes).
8. Shot tracer and instant replay (complete: live/retained tracer, deterministic recorded-sample playback, replay camera/HUD, lifecycle controls, tests, and artifact validation).
9. Enhanced Input migration and remapping screen (complete).

## Acceptance
- No canned trajectory curves.
- RHBH/RHFH mirror correctly.
- Release misses are deterministic, graded, bounded, and explained in the HUD.
- Disc selection visibly changes flight.
- Putting is a distinct skill mode.
- Ground play reacts to angle/surface/plastic.
- OB/hazard scoring and placement are deterministic, and rough/hazard effects apply only to the next throw command.
- Recorded calm-air test throws are consistent across common frame rates.
