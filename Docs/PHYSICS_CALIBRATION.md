# Disc Flight Physics and Calibration

## What the solver currently models
v0.1 uses a **spin-dominant gyroscopic precession model** intended to be stable, tunable, and frame-rate independent while the project is still being calibrated. It models:
- 3D translational velocity.
- A disc-plane attitude represented by world-space forward and normal vectors, rebuilt into an Unreal quaternion for rendering/collision.
- Signed axial spin from the combined throw-style and player-handedness convention.
- Lift and drag from relative airflow.
- Angle of attack.
- Aerodynamic trim/stability moments.
- Gyroscopic precession from torque divided by signed axial angular momentum.
- Explicit spin decay.
- Wind and gusts.
- Opposing high-speed turn and low-speed fade calibration moments.
- Swept Unreal collision with complete ground states, typed solid/vegetation fixtures, and deterministic basket-contact paths.

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
- Positive axial spin is the v0.1 right-hand-backhand convention. The authoritative rotation sign is `throw-style sign * handedness sign`: backhand is `+1`, forehand is `-1`, right hand is `+1`, and left hand is `-1`.
- The same combined sign mirrors lateral timing error, authored hyzer bank, and axial spin. Handedness does not change release-speed magnitude, spin-RPM magnitude, nose feedback, or launch-angle feedback.
- Turn and fade use **opposing** lateral stability moments. Signed spin then mirrors the resulting precession across the negative-sign pair.

| Physical release | Style sign | Hand sign | Rotation sign | Sign relationship |
|---|---:|---:|---:|---|
| RHBH | +1 | +1 | +1 | Positive baseline |
| LHFH | -1 | -1 | +1 | Same sign as RHBH |
| RHFH | -1 | +1 | -1 | Mirrored from RHBH |
| LHBH | +1 | -1 | -1 | Mirrored from RHBH; same sign as RHFH |

“Same sign” is a rotational convention, not a claim that the complete trajectories are identical. Backhand and forehand retain their separately calibrated speed and spin-magnitude ceilings.

If any convention changes, update this document and add/adjust a regression check in the same change.

## Turn/fade convention
For the positive RHBH/LHFH sign pair, high-speed turn and low-speed fade must oppose one another in bank tendency. The negative RHFH/LHBH pair mirrors that tendency through signed axial spin.

The current solver computes a stability torque around the in-plane lateral axis:
- High speed: turn contribution grows above `TurnStartsAboveMps`.
- Low speed: fade contribution grows below `FadeStartsBelowMps`.
- The two contributions use opposite signs.
- Signed axial spin mirrors precession for the RHFH/LHBH negative-sign pair.

Do not replace this with a pre-authored S-curve. The visible flight shape should emerge from attitude, velocity, stability moments, lift/drag, gravity, and wind.

## Release-quality boundary

Release execution is resolved once before aerodynamic integration. `FThrowCommand` represents player intent; `DiscGolfMath::ResolveThrowRelease` produces the immutable `FThrowRelease` consumed by the flight component. This keeps timing behavior deterministic, serializable, visible in telemetry, and independent of render frame rate.

The authoritative GameMode rejects malformed throw commands before disc spawn or gameplay mutation. Direction must be finite, non-zero, and contain a usable horizontal aim component because launch elevation is carried separately by `LaunchAngleDeg`; `ResolveThrowRelease` canonicalizes that aim into the XY plane so immutable release provenance exactly matches the direction simulated by `DiscFlight`. Plastic, throw style, handedness, and shot context must be known enum values; and all scalar inputs must be finite and remain inside the release contract: power `[0, 1]`, hyzer `[-34, 34]` degrees, nose `[-7, 11]` degrees, launch `[-5, 35]` degrees, and normalized timing error `[-1, 1]`. `ResolveThrowRelease` retains its clamps and zero-direction fallback for deterministic direct-math compatibility, but untrusted gameplay input is not repaired at the authoritative boundary.

Player launches also validate the exact stable equipment instance and player provenance: instance ID, mold, plastic, coherent mass in `[130, 200]` g, wear metadata, profile handedness, and current authoritative shot context. Animated transactions lock bag mutation. Spawn/configure/`Throw()` are provisional; `LastRelease`, accepted-shot sequence, stroke, telemetry, camera, and presentation commit only after `Throw()` succeeds and the flight component is authoritatively flying. Invalid configuration or rejected launch rolls back without a scored throw.

### Numerical and callback safety

The flight component snapshots the accepted configuration before integration and rejects later drift. The snapshot includes an exact ordered wind authority: director, base/gust fields, and every local zone's actor, stable ID, modifiers, and world bounds. It is checked across the launch callback boundary and during flight. Initial acceleration and all runtime velocity, displacement, wind, spin, backlog, contact, timeout, and state values must remain finite and inside bounded envelopes.

The nominal 240 Hz step is configurable only within 1 ms through 1/30 s. A tick processes no more than 128 fixed steps and contributes no more than 100 ms of render delta, so a stall cannot create an unbounded catch-up impulse. Sampling cadence is anchored to solver time rather than render grouping. Pre-spawn scale validation and the live collision transform both require the canonical unit scale.

Airborne, ground, and basket-capture moves use deferred `FScopedMovementUpdate`. The solver validates the actual applied transform and publishes it to the authoritative owner-transform latch before overlap callbacks are dispatched, then validates callback effects. This prevents legitimate overlap callbacks from observing a stale latch without weakening the rule: a later finite external teleport still fails closed and restores the last authoritative transform. Vegetation re-entry cooldown uses `Telemetry.FlightTimeSeconds`, never render/world time.

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
- aim reaches at most 6 degrees and follows the four-way rotation-sign contract,
- hyzer reaches at most 4 degrees of signed player-space offset,
- nose reaches at most 3 degrees of signed offset,
- launch angle reaches at most 2 degrees of signed offset.

For RHBH and LHFH, an early release moves aim left and a late release moves aim right; RHFH and LHBH mirror that aim result. Early/late nose, hyzer, and launch feedback stay in player throw space, then the combined throw-style/handedness convention mirrors disc bank and spin in the solver. Effective launch inputs are clamped before flight (`hyzer [-34, 34]`, `nose [-7, 11]`, `launch [-5, 35]` degrees).

These values are gameplay calibration seeds. Tune them only through the resolver and update its automation tests, this document, and the reference envelope together. Do not hide a second timing interpretation inside the flight solver.

## Reference smoke check
`Scripts/reference_flight_check.py` is a dependency-free **envelope test**, not a second authoritative physics engine. It mirrors the major v0.1 equations closely enough to catch catastrophic sign errors, NaNs, impossible carry, and loss of backhand/forehand mirroring before Unreal is available.

Run:

```text
python Scripts/reference_flight_check.py
```

The Unreal implementation remains authoritative once it can be compiled and tested in-engine.

### Fixed-step independence

The architecture rule that disc flight must not depend on frame rate previously had no source-only guard: the 30/60/120 FPS regression suite proves it in-engine, so a checkout without Unreal could not test it at all. The reference check now sweeps one Apex RHBH throw across 30, 60, 120, 240, 480, and 960 Hz and asserts convergence rather than mere similarity.

Measured behaviour of the reference model, coarsest step first:

| Step | Carry (m) | Peak (m) | Lateral (m) | Flight (s) |
| --- | --- | --- | --- | --- |
| 1/30 | 84.036 | 9.825 | -24.251 | 7.400 |
| 1/60 | 84.344 | 9.849 | -24.390 | 7.417 |
| 1/120 | 84.478 | 9.861 | -24.370 | 7.417 |
| 1/240 | 84.546 | 9.868 | -24.360 | 7.417 |
| 1/480 | 84.585 | 9.871 | -24.377 | 7.419 |
| 1/960 | 84.604 | 9.872 | -24.385 | 7.420 |

Total carry spread is 0.569 m (0.67 per cent) and each halving of the step moves the result roughly half as far as the previous halving did, which is the first-order convergence a correctly time-scaled explicit integrator produces. The guard therefore asserts three things: carry spread within 1.0 m, lateral spread within 0.5 m with no sign change, and each successive delta no more than 0.75 of the one before it. The last condition is what distinguishes convergence from coincidence — a solver can sit inside a tolerance band while still varying arbitrarily with step size.

`python Scripts/reference_flight_check.py --self-test` proves the guard has teeth. The solver decays spin as `exp(-k*dt)` per step, retaining `exp(-k*T)` over a flight of length `T` at any step size. Applying that decay once per step instead would retain `exp(-k*T/dt)` — the classic frame-rate bug — and passing `k = k/dt` at each rate reproduces exactly that through the real integrator. Faulted, the same throw ranges over 51.1 m to 75.8 m: a **24.679 m spread against the correct 0.569 m**, and non-monotone, so it fails both the band and the ratio condition. The self-test fails if the guard ever accepts it.

These tolerances bound the reference model's numerics. They are not a claim about the Unreal solver, which is authoritative and separately covered by the in-engine regression suite.

## Critical limitation
Modern disc-golf molds differ dramatically in rim width, dome, nose, mass distribution, surface, plastic stiffness, wear, and stability. The current coefficients are therefore **seeds**. The game should be calibrated against measured golf-disc throws before calling the physics simulation-grade.

## Calibration program

The source-controlled capture authority is `Config/DG_PhysicsMeasuredReferencePolicy.json`. It predeclares the equipment/launch matrix, measurement-quality ceilings, fit-versus-holdout split, comparison tolerances, and the 60-attempt Needle Gate route matrix. `Evidence/Session19/PhysicsMeasuredReferenceDataset.template.json` is deliberately empty and is not accepted calibration evidence.

Validate the capture protocol before field work:

```text
python Scripts/validate_dg_physics_measured_reference.py
python Scripts/validate_dg_physics_measured_reference.py --self-test
python Scripts/validate_dg_physics_measured_reference.py --dataset <captured-dataset.json> --require-complete
```

The last command must fail closed until all 100 predeclared throws, instrument identities, uncertainty records, and named reviews are present. Passing it accepts a measured dataset for calibration comparison only; it does not independently approve play feel or release readiness.

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

Classification order is intentional: low energy settles first, strong edge contact rolls before skip evaluation, shallow qualifying contact skips, and everything else slides. Signed spin uses magnitude for entry thresholds, so all four style/hand combinations share one ground implementation.

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

In non-Shipping builds, every completed throw is exported under `Saved/TrajectoryExports/` in trajectory schema v5 JSON and CSV. Shipping intentionally retains the same in-memory trajectory summaries while disabling JSON/CSV file output. Schema v5 keeps player-hand provenance mandatory and adds replayable gust-phase provenance: JSON records `release.handedness` as exactly `Right` or `Left` and `release.wind_phase_origin_s` in `[0, 4096)`, the summary and final telemetry repeat the phase, and CSV records matching `# handedness=Right|Left` and `# wind_phase_origin_s=...` metadata. Consumers must reject missing, unknown, non-finite, out-of-range, or disagreeing provenance. The default recorder rate is the solver rate (240 Hz), so each fixed step carries:

- relative SI position and Unreal world position,
- SI velocity and sampled wind,
- disc top-face normal, spin, and angle of attack,
- ground state, physical surface, gameplay course surface, and cumulative contact count.

Discrete ground transitions are recorded separately with time/location, prior and next states, physical/gameplay surfaces, contact number, impact speed, incidence angle, and disc edge angle. The export also embeds the resolved disc/aero profile, immutable release with handedness and applied lie multipliers, final telemetry, air/final carry, apex, lateral displacement, air time, ground distance, raw/playing surface, resulting lie/location, penalty, relief rule, and completion state. The physics-preset schema is version 2 and the regression-report schema is version 3.

## Lie effects stay outside the solver

Lie effects modify a copy of `FThrowCommand` before `ResolveThrowRelease`; `UDiscFlightComponent` still consumes one immutable release and has no knowledge of strokes or relief. Current profiles are light rough 96% power / 110% timing sensitivity, deep rough 88% / 125%, and hazard 92% / 115%. A perfect zero-error release remains centered even when timing sensitivity is higher. Any tuning change must update the rules automation tests and HUD text, while the established clean-lie reference envelope must remain unchanged.

`Data/PhysicsRegressionPresets.json` defines source-controlled launch commands and acceptance envelopes. Its canonical raw-file SHA-1 is `193C48DBCEDDBC629FA5873892F061C30BA4FF21`, and a report is authoritative only when it records that exact source identity. Existing preset commands are explicitly bound to right-handed execution; the calm RHBH Apex baseline remains the numeric compatibility authority and is repeated at 30/60/120 FPS with wind and gusts disabled. A suite passes only when every scenario remains in its envelope, each accepted throw retains a distinct deterministic phase origin, and those three matched throws agree with the 60 FPS result within 0.35 m final carry, 0.10 m apex, 0.25 m lateral finish, 0.50 m ground distance, and identical ground-contact count. Component-level nonzero-gust coverage separately drives the real 240 Hz accumulator at 30/60/120 FPS while advancing the presentation clock; position, velocity, sample count, and every sampled wind must agree. Four-way handedness/sign tests supplement this baseline; they do not silently retune or reinterpret it. These are regression thresholds, not claims of measurement accuracy. The preset file and regression launcher are Development/Test diagnostics and are deliberately unavailable in Shipping.

Automated physics launches must pass an external `-UserDir` plus explicit launch-time `-NoLoadExistingSave -DGNoProfileWrites`. The GameInstance snapshots those flags before GameInstance subsystem initialization: `NoLoadExistingSave` skips the existing profile and makes the in-memory profile ephemeral, while either profile suppression flag makes profile save calls succeed without disk I/O. `DGDeveloperToolNoSave` remains a separate legacy guard for practice-round snapshot writes and is not a blanket profile-write policy. The external user directory remains a separate containment boundary for trajectory exports, logs, configuration, and persistence domains outside the profile save.

The first exported baseline exposed a world-boundary sign error: positive authored launch angle had produced downward initial velocity in Unreal even though the independent reference treated it as upward. `LaunchDirectionFromFlat` now explicitly maps positive gameplay launch to positive world Z and has an automation test. The corrected calm 82% Apex RHBH baseline records approximately 84.37 m air carry, 85.02 m final carry, 8.37 m apex above release, and 7.35 s air time in the current runtime course.

## Putting and basket calibration

Circle 1 (10 m or less) and Circle 2 (more than 10 m through 20 m) are authoritative shot contexts selected from the current lie. Putting does not weaken the drive formula: it uses a separate release branch with lower speed/spin ceilings, smaller timing-miss angle penalties, constrained hyzer/nose input, context-specific launch, and a distance-derived recommended power. The HUD converts current power back into an estimated range and reports the pace difference plus signed lateral aim error at the basket.

Circle 1 launch is distance-calibrated: lies at 2 m or closer use 10 degrees, the angle increases linearly to 14 degrees at 7 m, and farther Circle 1 lies retain 14 degrees. Circle 2 remains at 12 degrees. The flatter tap-in release preserves the accepted natural-putt chain-entry window instead of sending a close putt into the top band; this is a gameplay calibration seed, not a measured biomechanical claim. A disc already inside the basket overlap is evaluated synchronously at solver time zero after the authoritative launch transaction commits. No render tick is involved. A caught disc may use a finite capture snap bounded to 250 cm; the seam does not enlarge the catch volume or reclassify legitimate band, tray, weak-chain, or excessive-pace contacts.

The source-only basket is a deterministic interaction model, not rigid-body chain simulation. It projects the incoming disc to closest horizontal approach over a short horizon and classifies:

- center chains: radius, height, speed, and vertical pace are all in the capture window;
- weak chains: off-center or excessive-pace chain contact loses most speed and deflects outward/down;
- top band: a high line loses horizontal speed and rejects downward;
- tray: a low line loses horizontal speed and rejects upward.

The verified calm baselines use the Touch/Base putter. Circle 1 starts at 7 m and catches after 6.30 m of trajectory travel at 7.36 m/s; Circle 2 starts at 14 m and catches after 13.30 m at 8.05 m/s. Both record `Caught`, zero ground contacts, and a holed-out result. These are gameplay calibration seeds pending measured putting data and structured playtesting.

## Presentation consumers

The live/retained tracer and instant replay consume the same recorded solver samples described above. They are deliberately outside the physics calibration loop: tracer decimation does not remove export samples, replay interpolation does not run forces or collisions, and replay actors cannot affect scores, lies, basket outcomes, or future captures. If a presentation change alters a regression summary or export, treat it as an architecture violation rather than a calibration adjustment.

Physics handedness support must not be presented as human-approved animation coverage. Automated technical coverage proves guarded right-handed backhand Drive, Approach, and Putt bindings plus synchronous gameplay fallbacks; it does not approve motion quality or disc/hand contact. Left-handed backhand and right- or left-handed forehand remain outside authored-motion coverage. Those fallback releases still use the same immutable command, four-way rotation-sign physics, scoring, and schema-v5 provenance. MetaHuman authentication, asset presence, cook presence, and automated binding tests do not satisfy the eight named manual motion/contact/product decisions; current human approval remains 0/8.
