# Disc Flight Physics and Calibration

## What the solver currently models
v0.1 uses a **spin-dominant gyroscopic precession model** intended to be stable, tunable, and frame-rate independent while the project is still being calibrated. It models:
- 3D translational velocity.
- A disc-plane attitude represented by world-space forward and normal vectors, rebuilt into an Unreal quaternion for rendering/collision.
- Signed axial spin for backhand/forehand handedness.
- Lift and drag from relative airflow.
- Angle of attack.
- Aerodynamic trim/stability moments.
- Gyroscopic precession from torque divided by signed axial angular momentum.
- Explicit spin decay.
- Wind and gusts.
- Opposing high-speed turn and low-speed fade calibration moments.
- Swept Unreal collision and early ground/tree response.

The design is informed by the UC Davis/Hummel flying-disc research, which develops a three-dimensional model driven by aerodynamic forces and moments. That research is a **modeling foundation**, not direct proof that its identified coefficient set describes modern golf drivers.

## Why v0.1 does not integrate the full rigid-body angular-rate equations
A direct explicit-Euler integration of the full rigid-body angular equations is a poor prototype choice at golf-disc spin rates: it can become numerically unstable unless the integrator, state representation, aerodynamic moment model, and timestep are handled carefully. v0.1 therefore uses the dominant gyroscopic relationship directly: torque perpendicular to the disc's spin axis changes the disc normal at a rate proportional to `torque / axial angular momentum`.

This is an intentional engineering tradeoff, not a claim that the rotational physics are complete. Once measured trajectories and spin/attitude data exist, we can evaluate a more complete rigid-body integrator (for example, a stable semi-implicit/RK method) against the simpler precession model. The more complex model should only replace this one if it produces measurably better behavior without harming stability or performance.

## Coordinate/sign conventions
These conventions are critical:
- `VelocityMps` is disc velocity through the Unreal world in meters/second.
- Relative airflow input is `disc velocity - wind velocity`.
- `DiscNormalWorld` points through the top face of the disc.
- Positive gameplay launch angle means upward from the horizontal. Unreal's positive quaternion pitch around the solver's right axis points downward, so the world-boundary rotation intentionally negates the authored angle.
- Positive angle of attack is nose-up relative to the velocity projected into the disc plane.
- Positive axial spin is the v0.1 right-hand-backhand convention; forehand uses the opposite sign.
- Positive player hyzer is mirrored by throw style at launch.
- Turn and fade use **opposing** lateral stability moments. Signed spin then mirrors the resulting precession between backhand and forehand.

If any convention changes, update this document and add/adjust a regression check in the same change.

## Turn/fade convention
For a right-hand backhand throw, high-speed turn and low-speed fade must oppose one another in bank tendency. Forehand mirrors this through spin direction.

The current solver computes a stability torque around the in-plane lateral axis:
- High speed: turn contribution grows above `TurnStartsAboveMps`.
- Low speed: fade contribution grows below `FadeStartsBelowMps`.
- The two contributions use opposite signs.
- Signed axial spin mirrors precession for forehand.

Do not replace this with a pre-authored S-curve. The visible flight shape should emerge from attitude, velocity, stability moments, lift/drag, gravity, and wind.

## Release-quality boundary

Release execution is resolved once before aerodynamic integration. `FThrowCommand` represents player intent; `DiscGolfMath::ResolveThrowRelease` produces the immutable `FThrowRelease` consumed by the flight component. This keeps timing behavior deterministic, serializable, visible in telemetry, and independent of render frame rate.

The timing meter sweeps from 0 to 1 over 1.30 seconds and wraps. Its center target is 0.82. Signed error is `(needle - 0.82) / 0.18`, clamped to `[-1, 1]`; negative is early and positive is late. Equal needle distance on either side therefore produces equal penalty magnitude.

Current authored grade bands:

| Absolute normalized error | Grade |
|---|---|
| 0.00-0.12 | Perfect |
| over 0.12-0.34 | Great |
| over 0.34-0.62 | Good |
| over 0.62-1.00 | Poor |

Perfect releases preserve the pre-v0.2 launch baseline exactly. Outside the Perfect band, a normalized severity curve applies deterministic effects:

- speed falls smoothly to a minimum 84% multiplier,
- spin falls smoothly to a minimum 78% multiplier,
- aim reaches at most 6 degrees and mirrors between RHBH/RHFH,
- hyzer reaches at most 4 degrees of signed player-space offset,
- nose reaches at most 3 degrees of signed offset,
- launch angle reaches at most 2 degrees of signed offset.

For RHBH, an early release moves aim left and a late release moves aim right; RHFH mirrors that aim result. Early/late nose, hyzer, and launch feedback stay in player throw space, then the existing throw-style handedness convention mirrors disc bank and spin in the solver. Effective launch inputs are clamped before flight (`hyzer [-34, 34]`, `nose [-7, 11]`, `launch [-5, 35]` degrees).

These values are gameplay calibration seeds. Tune them only through the resolver and update its automation tests, this document, and the reference envelope together. Do not hide a second timing interpretation inside the flight solver.

## Reference smoke check
`Scripts/reference_flight_check.py` is a dependency-free **envelope test**, not a second authoritative physics engine. It mirrors the major v0.1 equations closely enough to catch catastrophic sign errors, NaNs, impossible carry, and loss of backhand/forehand mirroring before Unreal is available.

Run:

```text
python Scripts/reference_flight_check.py
```

The Unreal implementation remains authoritative once it can be compiled and tested in-engine.

## Critical limitation
Modern disc-golf molds differ dramatically in rim width, dome, nose, mass distribution, surface, plastic stiffness, wear, and stability. The current coefficients are therefore **seeds**. The game should be calibrated against measured golf-disc throws before calling the physics simulation-grade.

## Calibration program

### Stage A — sanity data
Record a controlled set of throws in calm conditions:
- Mold/plastic/weight.
- Throw style.
- Release speed.
- Spin RPM if measurable.
- Hyzer/anhyzer release angle.
- Nose angle.
- Launch angle.
- Approximate wind.
- Trajectory points or high-frame-rate video reconstruction.
- Landing position and peak height.

Use at least:
- neutral putter,
- neutral mid,
- neutral fairway,
- understable control driver,
- overstable distance driver.

### Stage B — fit global coefficients
Fit the coefficient family that should be common or smoothly related across molds: lift slope, baseline drag behavior, trim behavior if supported, and spin decay.

### Stage C — fit mold stability
Fit high-speed turn threshold/magnitude and low-speed fade threshold/magnitude per mold. Avoid fitting every trajectory with arbitrary special cases.

### Stage D — plastics and wear
Treat plastic/wear as modifiers after a mold baseline works. Candidate effects:
- turn onset/magnitude,
- fade magnitude,
- surface drag,
- ground restitution/friction,
- spin retention if measurements support it.

### Stage E — validation
Hold out throws that were not used for fitting and compare:
- carry distance,
- lateral displacement over time,
- peak height/time,
- time of turn onset,
- time of fade onset,
- landing angle and speed.

## Ground-play state machine

Ground play now stays inside the same 240 Hz custom simulation instead of handing the disc to Chaos or applying one generic bounce. The first ground contact records impact speed, approach/incidence angle, disc edge angle, surface, and contact count, then resolves one of these deterministic states:

- **Settled:** total or tangential speed is below the surface threshold.
- **Edge Roll:** the disc is sufficiently edge-on and has enough tangential speed and axial spin.
- **Skip:** a shallow impact clears the surface/plastic-adjusted speed threshold.
- **Slide:** the fallback for energetic ground contacts that are neither a skip nor an edge roll.

Classification order is intentional: low energy settles first, strong edge contact rolls before skip evaluation, shallow qualifying contact skips, and everything else slides. Signed spin uses magnitude for entry thresholds, so RHBH and RHFH share one ground implementation.

Skip response combines plastic/surface restitution with a bounded conversion of shallow tangential speed into upward velocity. Each surface also caps a consecutive skip train; once that budget is spent, another qualifying low hop becomes an energy-absorbing skip-out slide. This suppresses collision chatter and very long post-skip skids while preserving longer skip trains on hard rock and tee surfaces. Slide and edge-roll motion are constrained to the current support plane, receive gravity projected down slope, decelerate at a surface-specific rate, and probe for support so the disc can leave an edge and become airborne again. Edge rolls transition to slides at low speed. Ground play has a 12-second safety timeout.

### Surface calibration seeds

| Surface | Restitution scale | Friction scale | Slide decel | Roll decel | Skip gate | Skip cap |
|---|---:|---:|---:|---:|---:|---:|
| Fairway | 1.00 | 1.00 | 2.20 m/s² | 1.15 m/s² | 7.5 m/s, 19° | 3 |
| Tee pad | 1.10 | 0.72 | 1.50 m/s² | 0.80 m/s² | 6.5 m/s, 21° | 4 |
| Rough | 0.55 | 1.65 | 5.50 m/s² | 3.40 m/s² | 12.0 m/s, 10° | 1 |
| Dirt | 0.82 | 0.90 | 2.80 m/s² | 1.50 m/s² | 9.0 m/s, 16° | 2 |
| Rock | 1.55 | 0.55 | 1.20 m/s² | 0.65 m/s² | 5.5 m/s, 26° | 5 |

The existing plastic modifiers remain authoritative inputs. Base plastic lowers restitution and raises friction; Crystal raises restitution and lowers friction. This can change both skip classification and post-impact speed, while Tour remains the neutral baseline.

The source-only course uses typed `ADiscGolfCourseSurfaceActor` fixtures and stable compatibility tags. Production landscapes can use those actors, tags, or Physical Materials named `PM_Surface_<Type>`. `ECourseSurfaceType` is mapped explicitly to `EGroundSurfaceType`, so a gameplay rules category never introduces object-name checks or a second ground solver.

## Discrete world-fixture response

Trees, boulders, signs, and dense vegetation use `ADiscGolfWorldFixtureActor` instead of object-name checks. Solid fixtures remain sweep-blocking geometry and resolve normal and tangential velocity separately. Dense grass is an overlap volume: entering it retains the incoming direction while reducing speed and spin, so a disc can penetrate brush rather than bounce off an invisible wall. Decorative HISM foliage and quality-scaled grass remain collision-free.

| Fixture | Normal restitution | Tangential retention | Spin retention | Interaction |
|---|---:|---:|---:|---|
| Tree trunk | 0.22 | 0.46 | 0.56 | Blocking deflection |
| Boulder | 0.46 | 0.72 | 0.82 | Blocking hard rebound |
| Sign | 0.31 | 0.55 | 0.68 | Blocking medium rebound |
| Dense grass | n/a | n/a | 0.64 | Pass-through at 0.56 speed retention |

These are deterministic gameplay calibration seeds, not measured coefficients for bark, granite, sign construction, or plant density. They guarantee non-increasing translational energy, ordered rebound strength, retained glancing motion, and frame-rate-independent solver state. They do not yet model disc flex, branch geometry, bark compliance, rock shape microstructure, vegetation density gradients, or randomized kick direction. Fixture contact count/type, entry and exit velocity, impact normal, and entry/exit spin are included in final trajectory telemetry.

`-FixtureCollisionSmokeTest` is the repeatable engineering baseline. It spawns actual discs and fixture actors away from course geometry and runs 24 swept/overlap cases: tree, boulder, sign, and dense grass at 8, 16, and 24 m/s with head-on and glancing approaches. Each recorded live response is compared with `DiscGolfMath`, then written to `Saved/FixtureQaReports/LatestFixtureQa.json`. `Scripts/validate_fixture_qa.py` independently verifies the matrix and response evidence. A green report proves implementation consistency; it does not prove that the seed coefficients match real materials or feel believable to players.

### Remaining ground limitations

- Edge roll uses a constrained ground state and visual disc-plane target, not a complete contact-patch rigid-body wheel model.
- Surface values are gameplay/calibration seeds pending recorded landing data.
- Mud, roots, loose rock, water, disc flex, branch/canopy contact, and random-looking-but-deterministic kick distributions are not implemented.
- Physical Material naming is supported, but production Landscape layers/material assets still need to be authored for Pine Ridge Hole 1.

## Telemetry and trajectory exports
Never remove the developer telemetry path. `FDiscFlightTelemetry` carries the exact resolved release alongside live speed, spin, angle of attack, carry, ground state/surface, impact count/angles, and ground travel.

Every completed throw is now exported under `Saved/TrajectoryExports/` in schema-v3 JSON and CSV. The default recorder rate is the solver rate (240 Hz), so each fixed step carries:

- relative SI position and Unreal world position,
- SI velocity and sampled wind,
- disc top-face normal, spin, and angle of attack,
- ground state, physical surface, gameplay course surface, and cumulative contact count.

Discrete ground transitions are recorded separately with time/location, prior and next states, physical/gameplay surfaces, contact number, impact speed, incidence angle, and disc edge angle. The export also embeds the resolved disc/aero profile, immutable release with applied lie multipliers, final telemetry, air/final carry, apex, lateral displacement, air time, ground distance, raw/playing surface, resulting lie/location, penalty, relief rule, and completion state. Physics preset/report schemas remain version 2.

## Lie effects stay outside the solver

Lie effects modify a copy of `FThrowCommand` before `ResolveThrowRelease`; `UDiscFlightComponent` still consumes one immutable release and has no knowledge of strokes or relief. Current profiles are light rough 96% power / 110% timing sensitivity, deep rough 88% / 125%, and hazard 92% / 115%. A perfect zero-error release remains centered even when timing sensitivity is higher. Any tuning change must update the rules automation tests and HUD text, while the established clean-lie reference envelope must remain unchanged.

`Data/PhysicsRegressionPresets.json` defines source-controlled launch commands and acceptance envelopes. The calm Apex baseline is repeated at 30/60/120 FPS with wind and gusts disabled. A suite passes only when every scenario remains in its envelope and those three matched throws agree with the 60 FPS result within 0.35 m final carry, 0.10 m apex, 0.25 m lateral finish, 0.50 m ground distance, and identical ground-contact count. These are regression thresholds, not claims of measurement accuracy.

The first exported baseline exposed a world-boundary sign error: positive authored launch angle had produced downward initial velocity in Unreal even though the independent reference treated it as upward. `LaunchDirectionFromFlat` now explicitly maps positive gameplay launch to positive world Z and has an automation test. The corrected calm 82% Apex RHBH baseline records approximately 84.37 m air carry, 85.02 m final carry, 8.37 m apex above release, and 7.35 s air time in the current runtime course.

## Putting and basket calibration

Circle 1 (10 m or less) and Circle 2 (more than 10 m through 20 m) are authoritative shot contexts selected from the current lie. Putting does not weaken the drive formula: it uses a separate release branch with lower speed/spin ceilings, smaller timing-miss angle penalties, constrained hyzer/nose input, context-specific launch, and a distance-derived recommended power. The HUD converts current power back into an estimated range and reports the pace difference plus signed lateral aim error at the basket.

The source-only basket is a deterministic interaction model, not rigid-body chain simulation. It projects the incoming disc to closest horizontal approach over a short horizon and classifies:

- center chains: radius, height, speed, and vertical pace are all in the capture window;
- weak chains: off-center or excessive-pace chain contact loses most speed and deflects outward/down;
- top band: a high line loses horizontal speed and rejects downward;
- tray: a low line loses horizontal speed and rejects upward.

The verified calm baselines use the Touch/Base putter. Circle 1 starts at 7 m and catches after 6.30 m of trajectory travel at 7.36 m/s; Circle 2 starts at 14 m and catches after 13.30 m at 8.05 m/s. Both record `Caught`, zero ground contacts, and a holed-out result. These are gameplay calibration seeds pending measured putting data and structured playtesting.

## Presentation consumers

The live/retained tracer and instant replay consume the same recorded solver samples described above. They are deliberately outside the physics calibration loop: tracer decimation does not remove export samples, replay interpolation does not run forces or collisions, and replay actors cannot affect scores, lies, basket outcomes, or future captures. If a presentation change alters a regression summary or export, treat it as an architecture violation rather than a calibration adjustment.
