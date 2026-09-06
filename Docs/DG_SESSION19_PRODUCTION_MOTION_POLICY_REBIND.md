# Session 19 production-motion policy rebind

The base `DG_Session19ProductionMotionAuthoringPolicy.json` is historical
candidate authority. It must not be edited to make the current tree pass.
Identity drift in that file is useful: it prevents an older Shipping candidate
from being presented as evidence for newer motion, rig, runtime, or tool bytes.

## Current drift classification

The read-only audit currently locates 12 changed bindings in four later
development workstreams:

- pipeline documentation: 1;
- runtime motion/retarget integration: 4;
- DGMaster skeleton, Control Rig, and player AnimBP reauthoring: 3;
- production-motion authoring and validation tooling: 4.

This provenance classification does not approve the changed bytes. In
particular, a changed serialized `.uasset` identity cannot prove that its
contents are correct. All 12 changes continue to block the historical policy,
and the current human animation/contact, live-profile, cook/archive, and release
approvals remain outstanding.

Run the read-only audit with:

```powershell
python Scripts/audit_dg_session19_production_motion_policy_drift.py --root C:\DGTour
```

The expected current result is
`BLOCKED_HISTORICAL_POLICY_IDENTITY_DRIFT`; the command exits nonzero whenever
any binding has drifted, any drift is unclassified, or the audit input omits or
adds a binding, supplies a malformed identity, or records a non-positive byte
count.

## Future candidate-scoped rebind

Only after a new Shipping build has produced a real, passing
`Evidence/Session19/CandidateContent-<candidate-id>.json` receipt may the current
identities be bound into a new append-only policy:

```powershell
python Scripts/validate_dg_session19_production_motion.py `
  --root C:\DGTour `
  --candidate-id <candidate-id> `
  --motion-version v6 `
  --generate-candidate-policy Config/DG_Session19ProductionMotionAuthoringPolicy-<candidate-id>.json
```

The generator refuses the historical candidate ID, paths outside the canonical
candidate-scoped filename, missing candidate-content receipts, invalid current
contracts, and existing destinations. It creates a policy only; it does not
create Shipping evidence, infer human approval, or make the project release
ready. Validate the resulting policy against the same candidate ID before any
technical receipt is considered.
