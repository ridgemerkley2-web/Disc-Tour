# Disc Golf Tour — Session 11 Equipment and Throw Lab Audit

## Scope and acceptance posture

Session 11 is a source-first technical development gate for generic disc instances, authoritative bag selection, and a bounded Throw Lab record adapter. It is not production-equipment approval, public-name clearance, calibrated weight/wear approval, shipping UI acceptance, or release approval.

The machine-readable authority is `Config/DG_Session11EquipmentThrowLabContract.json`. The bounded technical implementation and its package evidence are complete: `feature_complete=true`, `releaseBlocked=true`, and `release_ready=false`. This is a technical exit, not production readiness. The validator requires the full bounded artifact set and its authority tokens; a partial implementation fails with named missing files or token groups.

## Authority boundary

The project-owned runtime path remains authoritative end to end:

- `UDiscCatalogSubsystem` resolves the existing five generic molds and three generic plastics from the project Primary Asset catalog.
- `UDiscBagComponent` owns player selection and adapts a selected `FDGDiscInstance` DTO into the existing resolved-disc structure.
- `DiscGolfMath::ResolveThrowRelease` and `ADiscGolfTourGameMode::LaunchThrow` remain the one accepted release and launch path.
- `ADiscActor` and `UDiscFlightComponent` remain the single gameplay disc and fixed-step SI flight authority.
- `UDiscTrajectorySubsystem` remains the trajectory, summary, export, and physics-regression authority.
- `ADiscReplayActor` remains the replay renderer for actual recorded trajectory samples.
- `UDiscGolfTourGameInstance` and `UDiscGolfSaveGame` remain the production player-profile authority. `UDiscEquipmentSaveGame` and `UDiscThrowLabSaveGame` are isolated schema-v1 development payloads and never replace or migrate the protected schema-10 profile.

The installed framework's `FDGDiscInstance` and `FDGDiscBagLoadout` are reusable data-transfer types only. Project runtime source must not instantiate or wire `UDiscGolfBagComponent`, `UDiscGolfTelemetryComponent`, or `UDiscGolfShotReplayComponent`, and must not include their component headers. Those components would create parallel bag, recording, or replay authorities.

## Generic equipment contract

The accepted development catalog remains exactly `Apex`, `Vector`, `Line`, `Compass`, and `Touch`, with `Base`, `Tour`, and `Crystal` plastics. These are development names pending public clearance; no real manufacturer or product data is accepted. The Session 9 generic brand identity remains `dg_generic`, while all Premium Disc Golf donor identifiers remain dormant and forbidden from active runtime wiring.

A player-owned disc snapshot carries stable instance identity, definition and plastic identity, mass, wear, color, stamp, nickname, and favorite state. Validation is fail-closed: instance IDs must be valid and unique, the selected instance must exist in the bounded loadout, referenced generic catalog IDs must resolve, numeric values must be finite and bounded, and invalid loadouts recover atomically rather than partially applying selection.

The default 175 g, wear 0.0 instance must preserve the established catalog physics exactly. Mass may be represented explicitly in the resolved snapshot, but the current bounded gate does not claim calibrated non-default mass envelopes. Wear remains metadata and presentation-only; `bWearAffectsPhysics` must remain false. No equipment field may mutate a disc after the accepted launch transaction.

## Throw Lab adapter contract

The Throw Lab is a project-owned adapter over the completed authoritative throw. It begins only from a successful existing launch transaction and records only after the existing settled, holed-out, or out-of-bounds completion authority closes the throw. Rejected and cancelled throws produce no record.

Each record is built by value from the exact resolved disc snapshot, immutable release, trajectory summary, actual solver samples, ground transitions or impacts, and final telemetry. Stored records are bounded to 64 entries, 2,400 replay samples per record, at most 60 Hz, and 512 ground transitions. Downsampling must be deterministic and retain endpoints and discrete transition evidence. The existing sample payload keeps world positions in Unreal centimeters, linear velocity in meters per second, and spin in RPM. The contract reserves radians per second for a future full angular-velocity field, but the current `FDiscTrajectorySample` does not expose that vector; Session 11 must not fabricate it, and closing that production field remains within the Session 11 readiness blocker. Recording and comparison never feed back into simulation.

Development operations may select, pin, delete, compare two compatible records, save/load a separate Throw Lab payload, and pass selected actual samples to the existing replay actor. Comparison must report incompatibility explicitly instead of silently comparing unrelated records. None of this is a shipping UI claim or authorization to mutate the protected production profile during automation.

The bag loadout uses a separate schema-v1 equipment slot. A full candidate loadout is validated before mutation; unsupported schema, missing content, duplicate identity, and selected-instance mismatch fail atomically. The old profile's mold/plastic fields remain the compatibility fallback, so Session 11 does not bump or rewrite the protected profile schema.

## Technical closure evidence

- Editor and Game Development builds: pass.
- Focused `DiscGolfTour.Session11.*`: 12/12 pass.
- Full `DiscGolfTour.*`: 164/164 pass.
- Live editor-game throw: 1,965 solver samples, 490 actual bounded Throw Lab samples, pass.
- Fresh non-iterative package: 984 cooked, 0 incrementally skipped, pass.
- Packaged live throw: 1,965 solver samples, 490 bounded samples, pass.

## Frozen continuity and blockers

The validator binds the exact frozen Session 9 and Session 10 artifacts:

- Session 9 contract: `3EF3CBAD7E66D89D41CFB8C8FF59516484D8258995CAFAF28F8D58C4D8EB2001`
- Session 9 validator: `69867A9CE89D8F7720E2A124BEEB09CE4FFDC02D6F95BED8A11D281EF0EB7550`
- Session 10 contract: `FB352F9E215D1D9E6D232C9D5DE45BAA7EA03588746352D31B9B717B3A2B17A9`
- Session 10 validator: `B5650D3AEDD9E28976EEF4631AB9C2072A9E58E94EFE4B8AFF47CD482B2805B0`

The exact nine inherited blockers remain, and Session 11 adds one blocker without replacing or weakening them:

1. `FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED`
2. `EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE`
3. `CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED`
4. `DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE`
5. `QUARANTINED_IMPORT_RECEIPTS_PENDING`
6. `POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE`
7. `ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED`
8. `MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED`
9. `SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY`
10. `SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING`

The protected production profile remains byte- and hash-bound by the contract. Session 10 presentation-only environment behavior, dormant PCG, and all three quarantine roots remain unchanged.

## Nonwriting verification

Run from the project root:

```powershell
python Scripts/validate_dg_session11_equipment_throw_lab.py --self-test
python Scripts/validate_dg_session11_equipment_throw_lab.py --no-report
python Scripts/validate_dg_session11_equipment_throw_lab.py --no-report --release-required
```

The self-test mutates release, authority, frozen donor/profile binding, telemetry-unit, simulation-neutrality, regression, UI-claim, wear-calibration, and blocker invariants, exercises all forbidden plugin-wiring tokens, and checks exit semantics. Normal mode exits 0 only when the technical contract and any active bounded implementation are coherent; it always reports `releaseBlocked=true` and all ten blockers. Release-required mode exits 2 after a technical pass while those blockers remain. Technical errors exit 1. Omitting `--no-report` writes only `Saved/EquipmentReports/DG_Session11EquipmentThrowLabAudit.json`.

Unreal compile, focused `DiscGolfTour.Session11.*` automation, full `DiscGolfTour.*` automation, live throw coverage, and a fresh non-iterative Development package remain separate evidence gates recorded by the closure process. The validator does not launch Unreal, save assets, cook, stage, or modify the production player profile.
