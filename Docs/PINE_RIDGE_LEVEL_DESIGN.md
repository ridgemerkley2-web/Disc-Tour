# Pine Ridge Level Design Bible

## Purpose

Pine Ridge is the three-hole competitive level-design slice. Its job is to make disc choice, line choice, landing position, wind, and recovery decisions readable before final environment art is allowed to hide the blockout.

All three holes belong to one persistent course world. Stable placement, round transitions, forest patterning, and the next continuous-terrain/connector-trail pass must obey `Docs/PINE_RIDGE_FOREST_DESIGN.md`; no hole may become a disposable standalone map.

The authored route data is strategy metadata. It may drive review tools, flyovers, AI planning, and future telemetry, but it does not steer discs, change physics, award strokes, or own collision. The packaged v0.5 foundation preserves `PineRidgeCompetitiveV1` as the sealed pre-fixture baseline. Active source uses the separately named `PineRidgeCompetitiveV2_Fixtures` revision for explicit tree, dense-grass, boulder, and sign collision.

## Route contract

Every production hole must expose at least these choices:

- **Primary:** the intended scoring line for the target player, with a clear landing objective and a repeatable next shot.
- **Risk/reward:** a narrower or more exposed line whose scoring benefit is visible before release and whose miss has a meaningful consequence.
- **Bailout:** a lower-variance line that removes a major danger while conceding distance, angle, or a stroke of scoring expectation.

Each route binds to a real landing-zone ID and records a stable ID, display label, shot intent, target strokes, 1-5 risk/reward ratings, corridor width, and at least three world-space waypoints from tee to basket. Validation rejects missing coverage, duplicate IDs, missing landing zones, implausible widths/lengths, or endpoints that do not cover the tee and basket.

## Hole plans

| Hole | Route | Type | Target | Risk / reward | Corridor | Strategic promise |
| --- | --- | --- | ---: | --- | ---: | --- |
| 1 - Pine Ridge Opening | Center Placement | Primary | 2 | 3 / 4 | 15 m | Shape through the opening and leave a clean green approach. |
| 1 - Pine Ridge Opening | Skip-Shelf Attack | Risk/reward | 2 | 5 / 5 | 9 m | Challenge the shelf with a low stable line for the most direct scoring look. |
| 1 - Pine Ridge Opening | Right-Side Bailout | Bailout | 3 | 1 / 2 | 14 m | Remove the skip shelf from play and accept a longer finish. |
| 2 - Needle Gate | Needle-Gate Placement | Primary | 3 | 3 / 4 | 10 m | Hit the first gate at controlled speed and play the next shot from center. |
| 2 - Needle Gate | Late-Crosswind Attack | Risk/reward | 3 | 5 / 5 | 7.5 m | Push deeper through the narrow corridor to shorten the exposed second shot. |
| 2 - Needle Gate | Left Pitch-Out Par | Bailout | 4 | 1 / 2 | 12.5 m | Use the wider pocket, concede distance, and preserve a clean three-shot plan. |
| 3 - Gallery Lake | Lake-Carry Attack | Primary | 3 | 4 / 5 | 14 m | Carry the lake and land beyond the beach for the shortest birdie approach. |
| 3 - Gallery Lake | Direct Water Attack | Risk/reward | 3 | 5 / 5 | 9.5 m | Take the widest exposed carry to maximize distance and eagle opportunity. |
| 3 - Gallery Lake | Right-Shore Placement | Bailout | 4 | 2 / 3 | 15 m | Stay on the dry shelf and approach from the gallery side. |

### Hole 1 - Pine Ridge Opening

This is the teaching hole. The center line establishes ordinary shot shaping, the left-center shelf introduces ground-play ambition, and the right apron demonstrates that a safe line should cost angle and scoring expectation rather than feel like a hidden failure state.

### Hole 2 - Needle Gate

This is the first geometry-tuning target because its choices depend on obstacle spacing, visibility, and landing depth. The primary line should reward speed control, the attack should be recognizably tighter and deeper, and the pitch-out must remain playable without becoming the automatic scoring route.

### Hole 3 - Gallery Lake

This is the spectacle and commitment hole. Water danger must read from the tee and broadcast camera. The primary carry should be demanding but fair, the direct attack must earn a measurable distance advantage, and the right-shore route must stay fully dry while giving up approach quality.

## In-engine review

Use `DGT_ToggleLevelDesignReview` in the developer console, or launch with `-LevelDesignReview`.

- Cyan: Primary route
- Orange: Risk/reward route
- Green: Bailout route
- Paired lines: authored corridor width
- Spheres/arrows: waypoints and travel direction
- Boxes: route-bound landing zones

The review actor is opt-in, non-authoritative, and collision-free. Normal play and packaged presentation are unchanged while it is disabled.

## Playtest gates

Before changing competitive geometry, record at least 20 intentional attempts per route with a representative stable driver or fairway driver. Capture:

- route selected and whether the player understood its tradeoff after the flyover;
- landing-zone hit rate and miss side;
- penalty rate;
- remaining distance and basket visibility;
- whether the next intended shot is unobstructed;
- final hole score.

A route is not validated merely because it can be completed. It must be readable, meaningfully distinct, and produce the intended risk/scoring relationship. Broadcast launch, fairway, and finish coverage must also preserve the critical obstacle, landing window, and consequence of the miss.

## Needle Gate telemetry workflow

Launch with `-NeedleGateRouteTelemetry`. A compatible `Saved/RouteTelemetryReports/LatestRouteTelemetry.json` resumes automatically; otherwise a new schema-v1 session starts. The selected route stays bright in the collision-free review overlay while comparison routes remain dim.

For each attempt:

1. Select the intended route if needed: `DGT_SelectTelemetryRoute NeedlePlacement`, `LateCrosswindAttack`, or `LeftPitchOut`.
2. After reading the flyover/route promise, enter `DGT_TelemetryTradeoffUnderstood 1` for understood or `0` for unclear.
3. Throw the real first shot from the tee. No route code changes the setup or release.
4. After the lie, enter `DGT_TelemetryNextShotClear 1` or `0` from the intended next-shot perspective.
5. Finish the hole when practical so final score is attached to that attempt, then press `R` to reset. If an attempt is abandoned, reset without inventing a score.
6. Continue until the selected route reaches 20/20; the tool focuses the first incomplete route automatically. Manual route selection remains available.

The report auto-saves after each outcome or rating. `python Scripts/validate_route_telemetry.py` checks a partial session. Only `python Scripts/validate_route_telemetry.py --require-complete` certifies the exact 60-attempt evidence set. The starting report is intentionally 0/60; it proves pipeline readiness, not route quality.

Automated fields include corridor adherence and maximum deviation, landing-window result/miss side, penalty, remaining distance, basket visibility, fixture contacts, and disc/release data. Tradeoff understanding and next-shot clarity remain explicit human ratings. Final score remains unset unless the tester completes the hole.

## Next geometry iteration

Keep the packaged `PineRidgeCompetitiveV1` archive sealed as the pre-fixture comparison point. Collect route telemetry against active `PineRidgeCompetitiveV2_Fixtures`, starting with Needle Gate because corridor readability is its core mechanic. If the 20-attempt samples show routes collapsing into one dominant line, invisible consequences, or an unusable bailout, author another named collision-profile revision and compare it against both baselines. Apply the same evidence gate to Gallery Lake after Needle Gate is stable.
