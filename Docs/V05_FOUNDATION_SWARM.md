# v0.5 Foundation Agent Swarm

## Objective

Begin the production-presentation milestone with parallel, reviewable work while preserving the sealed v0.4 gameplay baseline.

## Roles

- Coordinating lead: owns task routing, shared integration, build/package gates, and final handoff.
- Supervisor agent: freezes scope and architecture, reviews integrated changes, and rejects boundary violations.
- Independent QA agent: audits claims, identifies regressions, and verifies evidence without implementing features.
- HUD implementation agent: owns only `DiscGolfHUD.*` and an optional isolated HUD layout test.
- Presentation-audio implementation agent: owns only new `DiscGolfPresentationAudio.*` files and their isolated tests.

## Foundation scope

- Improve the source-only Canvas HUD's broadcast hierarchy and 1280x720 safety while retaining every development surface.
- Establish a silent, deterministic presentation-audio event vocabulary/resolver with no hard audio assets.
- Add minimal one-way GameMode hooks only after authoritative gameplay mutations.
- Keep the implementation reusable by later Common UI/UMG and an event-driven audio director.

## Frozen boundaries

- No change to the 240 Hz solver, release tuning, ground response, basket evaluation, rules, collision, course data, round authority, schema-v4 saves, or 47-mapping/22-action input profile.
- HUD rendering remains read-only.
- Audio consumes immutable gameplay results and cannot award strokes, alter lies, move actors, or trigger transitions.
- Missing sound assets are silent success.
- No event dispatch from inside the fixed-step solver.
- The v0.4 package remains untouched; any accepted result ships to a separate v0.5 foundation archive.

The hashes of protected v0.4 files are recorded in `Saved/AgentSwarm_v05_Baseline.json` for post-integration drift checks.

## Integration order

1. Capture the protected-file baseline and source/reference-flight results.
2. Run HUD and audio implementation in parallel with disjoint ownership.
3. Inspect uploaded changes and add only minimal shared lifecycle wiring.
4. Return the integrated result to the supervisor for architecture review.
5. Run independent QA review and address substantiated findings.
6. Build, run all automation, capture visual states, and rerun the complete v0.4 editor gates.
7. Cook a separate v0.5 foundation package, rerun packaged gates, audit logs, and seal checksums.

## Acceptance

- Protected-file hashes remain unchanged.
- All 59 existing tests and all new tests pass.
- Reference flight remains approximately 84.5 m.
- Three-hole smoke remains three strokes against par 11 for -8.
- Six-scenario 30/60/120 FPS regression remains unchanged.
- HUD is legible and overlap-free at 1280x720 and 1920x1080 in tee, timing, flight, penalty, scorecard, replay, and round-complete states.
- Audio events are stable, bounded, ordered, deduplicatable, and harmless without assets.
- Editor and packaged runtime logs contain no fatal/assert/ensure/crash/runtime-error markers.

## Final outcome

The swarm completed the foundation and independently reviewed the release candidate. Supervisor and QA holds were resolved before sealing: the public regression throw bypass was removed, runtime event payload/timing and regression isolation were completed, scorecard/next-hole/reset gates were hardened, water identity was separated from dry hazard, and holed-out event order was corrected to basket outcome -> hole completion -> round completion with an end-to-end assertion.

Final evidence:

- 66/66 automation tests passed;
- all four editor and all four packaged runtime gates passed;
- real D3D12 and WASAPI packaged launch passed;
- final HUD captures passed at 1280x720 and 1920x1080;
- reference flight remained 84.5 m and the six-scenario regression remained unchanged;
- all 15 protected-file hashes matched the v0.4 baseline;
- the separate archive is `Saved/PackagedV05Foundation/Windows/DiscGolfTour.exe`.
