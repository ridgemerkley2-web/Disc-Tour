# Session 19 throw-physics capture coordination

## Purpose and boundary

`Scripts/run_dg_session19_physics_capture.py` is the guarded bridge between the
already sealed Windows Shipping candidate and the two evidence sets that still
require real capture:

- 100 instrumented field throws: five equipment categories, four launch shapes,
  and five accepted repetitions per cell (75 fit and 25 holdout throws);
- 60 human Needle Gate attempts: 20 each for `NeedlePlacement`,
  `LateCrosswindAttack`, and `LeftPitchOut`.

The coordinator does not generate either dataset. It never converts the
validator's synthetic self-test fixture into evidence, never drives gameplay to
manufacture route attempts, and never grants a human, calibration, product, or
release approval.

The sealed Shipping SKU intentionally strips the route-telemetry launch flag and
DeveloperTool annotation commands. Therefore the Shipping archive is immutable
release-comparison context, not the emitter of the 60-attempt playtest report.
The declaration proves that boundary instead of pretending Shipping can run a
Development-only workflow.

## Current prepared campaign

The current append-only declaration is:

`C:\DGTourExternal\PhysicsCapture\Declarations\PhysicsCaptureDeclaration-S19_WindowsShipping_20260827T023919Z_51a1c132f837-654845c4-4625-44cb-b9bd-ca0257d4fd83.json`

Earlier declarations `71704111-a2c3-4233-95d9-07b1ad9b2d93` and
`5233193c-bdba-44be-8d29-a62d3a01b825` are retained append-only for audit but are
superseded. Final-schema preflight rejects each with exit 2 and
`declaration keys differ`; neither may be used for capture.

It binds candidate `S19_WindowsShipping_20260827T023919Z_51a1c132f837`, the
complete live archive inventory, launcher and Shipping executable, the exact UE
5.8 Editor executable/build identity, current runtime/DeveloperTool DLLs, route
data, measured-reference policy/validator, route validator, readiness audit,
coordination policy, and runner. It contains 160 unique `UNRECORDED` slots and no
measurement, attempt, annotation, or approval claim.

The campaign uses one persistent external UUID UserDir so compatible route
sessions can resume without touching project saves. The UserDir and all snapshot
roots remain outside both the project and candidate archive.

## Verify before capture

Run the read-only preflight before every route session or measured snapshot:

```powershell
python Scripts\run_dg_session19_physics_capture.py `
  --preflight-only `
  --declaration "C:\DGTourExternal\PhysicsCapture\Declarations\PhysicsCaptureDeclaration-S19_WindowsShipping_20260827T023919Z_51a1c132f837-654845c4-4625-44cb-b9bd-ca0257d4fd83.json"
```

Preflight fails closed if a bound source or archive byte changes, the declaration
drifts, the UserDir contains a profile save, the partial route report is invalid,
or any path redirects through a reparse point. A successful preflight currently
reports 0/100 measured throws, 0/60 route attempts, and `releaseReady: false`.

An external in-progress measured dataset can be inspected without writing it:

```powershell
python Scripts\run_dg_session19_physics_capture.py `
  --preflight-only `
  --declaration "<absolute declaration path>" `
  --measured-dataset "<absolute external captured dataset path>"
```

## Field-measurement workflow

Capture equipment, environmental conditions, uncertainty, and trajectory data
under `Config/DG_PhysicsMeasuredReferencePolicy.json`. The field operator must
provide the real instrument identities, calibration record, recorder, location,
and measurement values. Every accepted throw must satisfy the predeclared wind,
sample-rate, release-video, uncertainty, trajectory, and landing requirements.

After one or more real throws have been added to an external non-template
dataset, seal an append-only snapshot:

```powershell
python Scripts\run_dg_session19_physics_capture.py `
  --seal-measured-snapshot "<absolute external captured dataset path>" `
  --declaration "<absolute declaration path>"
```

The coordinator re-runs the authoritative measured-reference validator, rejects
empty/template/overfilled evidence, copies the exact bytes into a new UUID
snapshot directory, and writes a hash-bound progress receipt. A partial snapshot
does not claim calibration acceptance. Even a later complete snapshot cannot by
itself grant release readiness.

## Human route-capture workflow

When a human operator is ready to play and annotate real attempts, launch the
visible bound Development Editor game through the guard:

```powershell
python Scripts\run_dg_session19_physics_capture.py `
  --launch-route-session `
  --declaration "<absolute declaration path>"
```

The guard writes an append-only launch intent before process start, launches the
exact bound Unreal Editor executable in visible `-game` mode at Pine Ridge Hole
2 with `-NeedleGateRouteTelemetry`, and uses `-NoLoadExistingSave`,
`-DGNoProfileWrites`, and the declaration's external UserDir. It deliberately
does not use unattended, offscreen, benchmark, fixed-time, or input-automation
arguments. The declaration and every result keep
`shippingCandidateIsEmitter=false`.

In game, select the intended route, set the tradeoff annotation, play the actual
hole, and set the next-shot annotation. Close the game normally when the desired
attempts are complete. A progress snapshot is accepted only when:

- the process exits successfully;
- the complete candidate archive and bound Development route runtime are
  byte-identical before and after;
- no `.sav` appears in the external UserDir;
- the route report is structurally valid;
- all prior attempts remain an exact immutable prefix; and
- the human session added at least one new attempt.

The coordinator then copies the report and process log into a new append-only
launch directory and emits a candidate-context-bound Development receipt. It
will not launch again after the exact 60-attempt set is complete.

## Validation commands

```powershell
python -m py_compile Scripts\run_dg_session19_physics_capture.py
python Scripts\run_dg_session19_physics_capture.py --self-test
python Scripts\validate_dg_physics_measured_reference.py --self-test
python Scripts\validate_route_telemetry.py --self-test
python Scripts\audit_dg_physics_capture_readiness.py --require-complete
```

The final strict measured and route validators must remain failing until genuine
evidence is complete. The current declaration is capture preparation, not closure
of either blocker.
