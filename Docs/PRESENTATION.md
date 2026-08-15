# Broadcast and Camera Presentation

## Goal
Make a throw feel like a televised sports moment while preserving shot readability.

The active art-direction contract is `Docs/FOREST_BROADCAST_STYLE.md`. It translates professional disc-golf broadcast hierarchy and modern interactive golf feedback into an original package with explicit IP, accessibility, state, color, motion, camera, and acceptance rules. `AGENTS.md` remains the project-wide development authority.

## Camera states
The current `ADiscBroadcastCameraDirector` automatically chooses:

- `LAUNCH`: a readable tee/over-shoulder release angle;
- `FAIRWAY TRACK`: a long-lens side view with velocity lead;
- `BASKET / LANDING`: a basket-side finish, or a predicted landing-zone finish for long misses.

Putting briefly preserves the release and then cuts to a wider basket-side finish so a short flight is not consumed by camera interpolation. Drive state changes use damped position/rotation/FOV movement. State transitions are monotonic for each shot.

Future production expansion can add:

- alternate authored launch and landing candidates,
- disc chase,
- Drone/flyover
- Replay orbit
- dedicated event/reaction angles.

## Selection inputs
Camera selection should use:
- shot type,
- release direction,
- distance to basket,
- expected landing zone,
- obstacle density,
- disc altitude/speed,
- whether the shot is putting/approach/drive,
- whether a camera's line of sight is obstructed.

The current selector implements shot context, release/travel distance, distance to basket, ground state, altitude/descent, velocity lead, and procedural line-of-sight recovery. It tests both collision and expanded static-mesh visual bounds so non-colliding foliage crowns remain camera obstacles. All three Pine Ridge holes provide authored launch, fairway, and finish anchors; missing anchors fall back to the deterministic procedural plan. Multi-candidate screen-space scoring remains future work.

## Tracer
The tracer should be a presentation layer fed from actual trajectory samples. It must never drive physics.

Recommended samples:
- timestamp,
- world position,
- speed,
- spin,
- disc orientation,
- optional wind.

That same trajectory buffer powers replay and physics analysis. The current implementation consumes the authoritative 240 Hz `UDiscFlightComponent` samples without resimulating a shot.

### Implemented tracer

- `ADiscGolfTourGameMode` draws the active trajectory during flight and retains the last completed trajectory at the lie.
- Airborne segments are cyan, ground-play segments are orange, and replay flight segments are gold.
- `DiscGolfPresentationMath::BuildTracerSampleIndices` preserves endpoints and ground/contact boundaries while bounding the debug path to 320 points.
- `T` or controller Left Shoulder toggles the tracer. The state and replay readiness remain visible in the HUD.

The current lines are a source-only development presentation. A production pass should replace debug lines with a pooled ribbon/material effect while preserving the same sample selection and no-feedback boundary.

### Implemented instant replay

- Every completed shot snapshots the full recorded samples before the live disc is released.
- `ADiscReplayActor` evaluates recorded time deterministically, interpolates position/velocity/normal/spin/angle of attack, and rebuilds orientation from the recorded flight plane.
- Playback runs at 0.75x, holds briefly on the final frame, then returns the camera to the golfer automatically.
- The replay actor is collision-free and presentation-only. It never invokes flight, scoring, ground, putting, basket, or trajectory-export logic.
- `V` or controller Right Shoulder starts and cancels replay. Reset, a new regression run, cancellation, and natural completion all destroy the ghost and restore the player camera.
- The HUD shows replay rate, elapsed time, duration, progress, and the cancel hint.

### Implemented live broadcast director

- `ADiscBroadcastCameraDirector` consumes the live disc's newest authoritative trajectory sample plus immutable release and basket context.
- `DiscGolfBroadcastCameraMath` owns testable mode selection and framing plans without requiring a world or modifying game state.
- GameMode creates the director after release, blends to it, and destroys/stops it on settle, hole-out, reset, or replacement.
- The HUD reports the active mode and whether line of sight was clear or raised.
- The director cannot collide with or steer the disc and cannot update score, lie, basket state, trajectory exports, or regression envelopes.

The v0.4 three-hole round also adds a readable Canvas scorecard and round-complete surface, but these remain development presentation. The v0.5 production pass should replace the scorebug, scorecard, hole intro, tracer material, replay dressing, course art, golfer animation, gallery response, and audio while retaining the current no-feedback boundary and deterministic visual-QA captures.

## Golfer animation contract

- The pawn has an optional non-colliding skeletal mesh and retains the primitive golfer as an asset-independent fallback.
- Presentation families are Drive, Approach, and Putt. A clean shot at 55 meters or less selects Approach; Circle 1/2 always select Putt.
- Each family moves through Setup, Windup, Release, and Follow Through.
- The first timing press begins Windup. Cancelling timing or opening the scorecard returns to Setup.
- GameMode commits Release only after the authoritative disc has spawned and consumed its immutable release state.
- Release grade, early/on-time/late result, handedness, family, phase, and normalized phase time are available to a future Animation Blueprint.
- Missing skeletal/animation assets are reported honestly as `PLACEHOLDER`; they do not block gameplay.

## Broadcast UI
Use an original package with:
- player/event identity,
- hole/par/distance,
- current score,
- lie/distance remaining,
- disc name + flight numbers,
- wind,
- throw type,
- optional release telemetry on replay.

### Implemented state-driven Canvas proof

- A pure resolver selects Play, Flight, Review, Replay, or Complete from read-only gameplay state.
- Play shows current lie, shot setup, and help; live Flight removes all three and uses a compact scorebug plus flight strip.
- Shot feedback is transient for three seconds after authoritative release rather than occupying the screen until the next throw.
- Replay suppresses live feedback and owns dedicated progress/cancel chrome.
- Completion preserves scoring/status but removes setup and generic help.
- The tuning/performance panel is opt-in through `DGT_ToggleDeveloperHud` or `-DeveloperHUD`.
- The resolver cannot mutate simulation, scoring, collision, saves, cameras, or trajectory exports and has focused automation coverage.

## Crowd system
Do not start with individually expensive AI spectators. Early production can use authored clusters/animation states and event triggers. Crowd density must scale independently of collision and gameplay.
