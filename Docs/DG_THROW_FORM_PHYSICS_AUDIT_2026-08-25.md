# Throw-form physics audit — 2026-08-25

## Verdict

The audited deterministic throw/release/flight/contact implementation passes the current code, automation, runtime-regression, artifact, and fresh Shipping build/archive engineering gates. The identified solver/lifecycle defects are closed within those tested boundaries; this is not a claim that every packaged gameplay, measured-realism, or release gate has passed.

This is not a public-release approval. Measured field calibration, route telemetry, human motion/contact review, normal Shipping gameplay acceptance, and the wider Session 19 release decisions remain pending. Those results must be collected; they must not be inferred from deterministic tests or fabricated.

## Authority boundary

Gameplay physics is command-driven, not pose-extracted. The committed player transaction supplies one validated `FThrowCommand`; `ResolveThrowRelease` turns it into one immutable `FThrowRelease`; the flight component consumes that release. Animation mirrors the transaction and supplies release timing/phase presentation, but skeletal pose is not a second velocity, spin, or aim authority.

This separation prevents a missing montage or retarget failure from silently changing competitive physics. It also means visual throw-form quality requires its own human motion and disc-contact review even when the numerical solver passes.

## Audited chain

| Boundary | Accepted state |
| --- | --- |
| Player command | Non-finite directions/scalars, unknown enums, zero direction, direction without usable XY aim, and out-of-contract values are rejected. The committed animation transaction must match the command exactly. |
| Spawn and scoring | The authoritative contract, lifecycle gate, exact stable equipment instance, coherent mass, and player provenance must all resolve before provisional spawn. `LastRelease`, accepted-shot sequence, stroke, telemetry, camera, feedback, and presentation commit only after `Throw()` succeeds and authoritative flight is active. |
| Release | Direction provenance is canonicalized to normalized XY. Launch elevation lives only in `EffectiveLaunchAngleDeg`. Timing error drives bounded speed, spin, aim, hyzer, nose, and launch offsets. |
| Handedness | Rotation sign is RHBH `+1`, RHFH `-1`, LHBH `-1`, LHFH `+1`; speed/spin magnitudes remain style-dependent rather than hand-dependent. Hand is retained in the immutable release and trajectory evidence. |
| Animation bridge | Throw intent, phase, active state, and handedness are projected into the runtime animation instance. A committed release remains single-owner/single-fire. Physics does not depend on a visual montage successfully playing. |
| Airborne solver | The disc advances at a nominal 240 Hz fixed step with bounded step/backlog/render-delta and finite runtime envelopes. Aerodynamic force uses disc-relative air velocity, exact snapshotted wind authority, lift/drag coefficients, angle of attack, gravity, spin decay, and immutable release orientation. |
| Contacts | Ground impact can transition through skip, slide, edge roll, and settle. Authoritative moves defer overlap dispatch until the applied transform latch is published; later external transform mutation still fails closed/restores. Fixture cooldown uses solver time, and launch-inside-basket contact is synchronous at solver time zero. |
| Evidence | Non-Shipping builds export schema-v5 JSON/CSV plus preset-v2/report-v3 evidence with required handedness, wind phase, canonical samples, and source SHA-1. Shipping retains in-memory summaries and intentionally disables diagnostic launchers, preset staging, reports, and file export. |
| Regression | Six source-controlled scenarios must be unique, complete, finite, preset-sourced, outcome-correct, and within envelopes. The 30/60/120 FPS calm drive must remain frame invariant. |
| Persistence | The current profile authority is schema v10. `-NoLoadExistingSave` prevents profile load and profile writes; `-DGNoProfileWrites` permits read-only profile load while suppressing writes. Suppressed writes are successful in-memory commits. Validation-slot grammar fails closed. |

## Defects closed in this audit

- Rejected malformed throw commands before disc spawn or stroke mutation.
- Rejected directions that are finite and nonzero but have no usable horizontal aim.
- Canonicalized release direction to XY so recorded provenance now matches the direction actually simulated; `(3,4,12)` resolves to `(0.6,0.8,0)`.
- Propagated handedness through command, release, aim/hyzer/spin signs, animation matching, regression rows, and schema-v5 trajectory evidence.
- Bound launches to the exact stable equipment instance/player provenance, resolved exact coherent mass in `[130, 200]` g, and locked animated bag mutation.
- Moved scoring, accepted-shot, telemetry, feedback, camera, and presentation commit after accepted authoritative flight; failed launch/configuration rolls back without a scored throw.
- Routed menu/controller/pawn/RHBH ingress through the complete lifecycle gate and made non-authority ingress fail closed.
- Added immutable configuration/runtime envelopes, effective timeout checks, canonical collision scale, bounded backlog/render delta, and anchored solver-time sampling.
- Captured exact ordered wind authority—including every zone actor/ID/modifier/bounds—and rechecked it after launch callbacks and during flight.
- Moved vegetation re-entry cooldown to solver time.
- Deferred authoritative movement callbacks until the applied transform latch is published, closing the live overlap race while preserving external-teleport failure/restoration.
- Evaluated accepted launch-inside-basket contact synchronously and bounded the finite capture snap.
- Restored the runtime animation-instance throw bridge.
- Canonicalized same-time trajectory samples and made regression/artifact acceptance fail closed for incomplete, duplicate, fallback, malformed, non-finite, wrong-sign, or frame-divergent input.
- Hardened the measured-reference protocol without converting an empty template into claimed field evidence.
- Added process-level profile read/write policy, early policy capture, fail-closed validation-slot handling, current settings normalization detection, and safe in-memory transactions for explicitly suppressed writes.
- Corrected documentation that previously implied Shipping runs the non-Shipping regression launcher and emits trajectory files.

## Final post-package evidence

All paths below are retained locally. Times inside Unreal evidence are UTC on 2026-08-26.

### Editor and runtime physics

- Final Editor DLL: `Binaries/Win64/UnrealEditor-DiscGolfTour.dll`, 4,567,552 bytes, SHA-256 `D93EC22D36EA9E58BF5D7B23D8B633663E5B2AA12832B7329EBB34ADFC40C1EB`.
- Focused physics: 15/15, exit 0, including `DeferredOverlapTransformLatch` and `SpawnInsideBasketAtLaunch`; `C:/DGTour_TestRuns/ThrowPhysicsAuditPostShipping_20260826T041900Z/Physics/Automation_Physics.log`, SHA-256 `B96678F19ECF10AF5EBF602B5409AD02039A4DEAA75B3067136A1099FE468755`.
- Final active-source/config automation: 295/295, zero non-success/error results, exit 0, zero saves; `C:/DGTour_TestRuns/ThrowPhysicsAuditFinalIntegrity_20260826T043510Z/Automation_Full.log`, SHA-256 `B34CEB79BAC3F06E80B94F947AD75A8CB981993C4B563C583B75DBE58213A8C1`.
- Live six-scenario runtime: PASS, exit 0; `C:/DGTour_TestRuns/ThrowPhysicsAuditPostShipping_20260826T041900Z/Regression/RegressionSuite.log`, SHA-256 `6AC4F02F2F4CC225A19930DED70CCC37AD097F0461755560DCD08615020A9D70`.
- Regression report: schema v3, `run_state=completed`, `passed=true`, authoritative presets loaded, presentation trace unchanged, 6/6 scenarios, zero comparison failures; `C:/DGTour_TestRuns/ThrowPhysicsAuditPostShipping_20260826T041900Z/Regression/Saved/PhysicsRegressionReports/LatestPhysicsRegression.json`, SHA-256 `9FBA3143E341E1649EFA7CB22B3A6D0F1C32DC3812D101BC92CD2F9C79510D03`.
- Matching latest Touch Circle 2 schema-v5 JSON/CSV passed exact consistency validation: SHA-256 `A7BCF50D2ADE34B6D9DE1144DA1EF24E9F2F3DCC526822EE36841896EC7A4C31` / `DA67AB54FB4A736DEA3E81F1DB24224A78A08013879896B4F90B9A057AA46BD7`.
- Canonical preset source: schema v2, six scenarios, SHA-1 `193C48DBCEDDBC629FA5873892F061C30BA4FF21`.
- Every post-package automation/runtime UserDir contains zero `.sav` files.

The calm Apex drive produced identical results at 30, 60, and 120 FPS: 1,832 samples, 84.3604 m air carry, 85.0248 m final carry, 8.3677 m apex, 7.3458 s air time, one ground contact, and 2.6585 m ground travel. The forehand produced 1,703 samples / 74.51 m air carry. Touch Circle 1 and 2 each recorded exactly one `Caught` contact and holed out at 7.36 m/s / 1.2 cm predicted radius and 8.05 m/s / 4.3 cm, respectively.

The earlier `C:/DGTour_TestRuns/ThrowPhysicsAuditRegression_20260826T035100Z` run is explicitly superseded and must not be cited as passing. It exposed the stale transform-latch overlap race on both touch-circle cases; the scoped-movement fix above was added, rebuilt, and covered by the new focused/full/live evidence.

### Profile isolation compatibility

The focused Session 7 visual runner is retained as historical migration compatibility evidence. Its `schema8_migration_reloaded` field is a migration probe; current player-profile authority is schema v10. It completed its persistence proof before its separate cosmetic capture gate:

- `creator_apply_saved=true`
- `creator_apply_reloaded=true`
- `creator_cancel_restored_applied=true`
- `schema8_migration_reloaded=true`
- `validation_temp_slot_deleted=true`
- residual external `.sav` count `0`

Manifest: `C:/DGTour_TestRuns/Session7Persistence_6d1e62ed203d44ed952ad7cbecb16066/Saved/CharacterFramework/Screenshots/Session7_FullCharacter/Session7_FullCharacter_CaptureManifest.json`, SHA-256 `E1AE9B4C618D842AF164B6B9CA625090D626E708AD37747119EAA7C0D801EF5E`.

The later visual capture failed because required cosmetic presentation components were absent. That is a separate character-presentation issue, not a failed Apply/write/reload/delete transaction or a throw-physics failure.

### Fresh Shipping candidate

Candidate `S19_WindowsShipping_ThrowPhysicsAudit_20260826T040100Z` completed a clean dual-target build followed by a full non-iterative Shipping cook, stage, Pak, IoStore, and archive with UAT exit `0`.

- Candidate verification: `PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING`.
- Archive: 31 files, 1,245,992,983 bytes; canonical archive manifest SHA-256 `067BC11688403E06A985E2BD927948B940C3830BC006A9DC88F37591BC3A3BF1`.
- Shipping executable: 162,598,912 bytes, SHA-256 `781B27D670716163A2774BB517340B70DABDA8DC0DBF765A93DE77FFEE8AC8DC`.
- Main containers: Pak SHA-256 `44D6270AB6EB0F135999A7B5783BD393ABCA5A667AA27E16ED9EB2B5926ED014`; UTOC `EAB9A3E7DAEFE6DD83FE6AA25752A237898AD071D919560549188BE8A3234D62`; UCAS `B7FA11DBCE2088BCC838F68E53162D9E5182BD27710363DAEE1002DBA32BF074`.
- Verification receipt: `Evidence/Session19/ShippingCandidateVerification-S19_WindowsShipping_ThrowPhysicsAudit_20260826T040100Z.json`, SHA-256 `BA5592CF39C58A6FB921A9ADD16777DD779E6DB62ADD1DB5CF70BFFE04FD1E11`.
- Technical notices, plugin/dependency closure, baked-PCG source closure, candidate-bound authored-data checks, and fresh Shipping PCG absence pass with their explicit human/legal qualifications. The archive and post-package test roots contain zero `.sav` files.
- The candidate has not received normal fresh-install three-hole gameplay acceptance or the supplemental candidate-bound release receipts. It is not promoted and does not replace the older release-scope candidate identity.

The Shipping target intentionally excludes `RegressionSuiteSmokeTest`, its canonical presets and report path, developer runners, and trajectory JSON/CSV output. Therefore the six-scenario and serialization passes above are Editor/non-Shipping evidence executed after the package build. Shipping confidence comes from the same compiled runtime release/flight/contact paths, the verified build/content boundaries, in-memory trajectory summaries, and future normal packaged-gameplay acceptance; it is not a Shipping regression-export claim.

## Independent validator results

- Independent reference envelope: PASS — 84.5 m carry, 9.9 m peak, 7.42 s flight; left/right backhand and forehand mirrors accepted.
- Presentation capture: PASS — 333 samples, 20 tracer points, 1.383 s, median 240.0 Hz.
- Measured-reference protocol: structurally valid; 38/38 mutation self-test passes; real measurements and named approvals remain absent.
- Production-motion technical preparation: 3/3 families, 8 semantic phases, 7/7 authored targets, 4/4 prerequisites, runtime binding and AlwaysCook accepted; the validator now requires the fail-closed `RequestThrow`/presentation-cancel seam and its 29/29 mutation self-test passes. R2 evidence SHA-256 is `DC5D784D0ACEC48D48F0F3CE6700A825BE5037039F7D0FEEC21691816925CB98`; human motion/contact and release readiness remain false.
- Manual release-review packet: valid; human approvals 0/8.
- Route telemetry: structurally valid partial evidence; 0/60 attempts, with 0/20 on each route.
- Release-scope validator: `PASS_AUTHORITY_RELEASE_BLOCKED`; 202/202 self-test passes; `--require-release-ready` correctly fails with 13 blockers, 1 pending gate, and 11 partial gates. The contract remains bound to the older immutable candidate.
- Project validator: PASS — 176 C++ files and 150 headers. The apparent Session 9/11/14/15/16 relocation failures were a deliberate fail-closed cascade while the Session 19 production-motion identity was stale; after the current-source policy/evidence refresh restored valid Session 19 authority, the existing narrowly scoped quarantine/Developer-module supersession checks accepted the intentional reorganization without weakening the frozen historical gates.
- `git diff --check`: no whitespace errors; only line-ending conversion warnings.

## Remaining blockers and next evidence

1. Collect the real 100-throw measured-reference dataset and required named approvals. The template is intentionally not accepted as complete.
2. Collect all 60 Needle Gate route attempts: 20 `NeedlePlacement`, 20 `LateCrosswindAttack`, and 20 `LeftPitchOut`.
3. Complete MetaHuman/profile-matrix single-release validation plus human animation and disc-contact review. MetaHuman Cloud authentication grants access only; asset/cook presence and the numerical solver do not substitute for pose/contact quality approval. Manual approval is currently 0/8.
4. Complete the eight named manual release decisions and the wider environment, equipment, presentation/audio, course/PCG, career/AI, and vertical-slice human/product gates represented by the 13 release blockers.
5. Run normal fresh-install three-hole gameplay/throw acceptance on the new Shipping package. The bounded startup check is not that evidence.
6. Generate the missing T040100Z-specific ShippingBinary, CandidateContent, FreshUserDir, feature/equipment, environment, presentation/audio, motion-cook, provenance, and ThreeHoleTechnicalAcceptance receipts, then explicitly rebind and revalidate the Session 19 release-scope contract. The currently valid contract still binds the older immutable candidate and must not be described as having promoted T040100Z.
7. Complete whole-archive/container provenance plus independent legal/distribution approval.

## Protected save authority

`Saved/SaveGames/DiscGolfTour_Profile_0.sav` remains 5,212 bytes with SHA-256 `A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`.
