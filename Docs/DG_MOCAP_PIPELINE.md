# Disc Golf Throw Motion Pipeline

This document defines the project-owned Session 5 ingestion convention for throw motion. It complements the build-kit guidance in `Docs/07_MOCAP_PIPELINE.md`; the installed UE 5.8 project and its accepted character assets remain the source of truth.

## Authority boundary

Motion assets may supply pose, hand position, and event timing. They do not calculate throw power, spin, aim, hyzer/anhyzer, nose angle, wind, aerodynamics, collision, lie, scoring, camera behavior, or replay behavior. Gameplay release continues through the single Session 3 adapter and the existing authoritative gameplay-disc launch path.

The following accepted assets are frozen inputs:

- `/Game/DiscGolf/Characters/Meshes/SK_DG_Master`
- `/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master`
- `/Game/DiscGolf/Rigs/IK_DG_Master`
- `/Game/DiscGolf/Rigs/CR_DG_Master`
- `/Game/DiscGolf/Animation/ABP_DG_Player`
- `/Game/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype`
- `/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype`

The prototype remains the normal gameplay fallback. Session 5 tooling must never rewrite it while processing a new take.

## Source and rights gate

Every authentic take must pass the read-only intake preflight before Unreal ingests it. A passing preflight permits only staging in `Source` and `Retargeted`; it does not approve cleanup, production promotion, shipping, legal clearance, or public release. The current `SourceArt/DiscGolf/Mocap/motion_source_registry.json` is an immutable, hash-bound historical baseline and must never be appended to or rewritten. Future production source-to-runtime facts require a separately named, append-only production source-and-license manifest. Unknown, missing, or ambiguous permission always resolves to `DO_NOT_SHIP`. Professional footage may be used as visual reference only unless the project retains explicit rights for the motion source, performer, solver output, derivative work, and commercial interactive use.

The authentic-footage intake accepts:

- `PROJECT_OWNED_CAPTURE`
- `WRITTEN_PERMISSION`
- `VALID_COMMERCIAL_LICENSE`

`SYNTHETIC_TEST` exists only in the frozen historical registry and is rejected by the authentic-footage intake preflight. It proves the tooling but can never be promoted as production mocap. A production promotion additionally requires retained evidence identities and the separate production source-and-license manifest.

Pexels footage is prohibited for this project regardless of the displayed license. The preflight rejects a provider name containing `Pexels`, `pexels.com`, and every `pexels.com` subdomain. Renaming or rehosting a prohibited clip does not make it eligible; provenance must identify the canonical provider and URL.

Original source files belong under:

`SourceArt/DiscGolf/Mocap/<MotionId>/`

Do not edit the original file in place. Record its byte length and SHA-256 before any conversion.

### Quarantined development-reference audit

When a locally supplied clip has not passed rights, performer-release, throw-form, capture, and technical review, keep it outside the project and Unreal. It may be inspected only as a local visual form reference. Bind a read-only technical audit to the expected byte count and SHA-256 with:

```powershell
python Scripts/audit_motion_development_reference.py `
  --source <absolute-external-path-to.mov> `
  --expected-bytes <exact-positive-byte-count> `
  --expected-sha256 <64-UPPERCASE-HEX>
```

The command opens the source read-only, uses the same built-in same-handle hash/probe/rehash implementation as production intake, and emits JSON to standard output. It accepts only an absolute `.mov` or `.mp4` path whose resolved target remains outside the project root. The JSON omits the source directory, makes no performer identity or rights claim, and always has either `QUARANTINED_DO_NOT_SHIP` or `AUDIT_FAILED_DO_NOT_USE` status. Exit code zero means only that the read-only audit completed; it never means staging eligibility. Even a technically conforming clip remains staging-ineligible. This audit does not copy media, create project files, run a solver, import Unreal assets, authorize derivative animation, or replace the authentic-footage preflight. If its stdout is retained as a receipt, keep that receipt below a uniquely named `C:\DGTour_TestRuns\MotionReferenceAudit\...` folder rather than in the repository.

Run its fail-closed tests with:

```powershell
python Scripts/audit_motion_development_reference.py --self-test
```

Only move a source below `SourceArt`, ingest it, or run body solving after the separate authentic-footage preflight below passes with real rights/release evidence and named human reviews. Never treat the development-reference audit output as a promotion receipt.

### Authentic-footage preflight

Copy `Docs/DG_MOTION_SOURCE_INTAKE.template.json`, fill every field, place the `.mov` or `.mp4` plus rights and performer-release evidence below the exact `MotionId` root, then run:

```powershell
python Scripts/validate_motion_source_registry.py --preflight <path-to-intake.json>
```

The input is an exact schema: extra fields, duplicate JSON keys, path traversal, noncanonical separators, missing files, byte/hash mismatches, placeholder identities, or ambiguous booleans fail it. It requires all of the following:

- `PROJECT_OWNED_CAPTURE`, `WRITTEN_PERMISSION`, or `VALID_COMMERCIAL_LICENSE` classification;
- named provider, creator, acquisition method, and date; project-owned capture requires `DIRECT_PROJECT_CAPTURE` and a `null` source URL, while either external classification requires a canonical HTTPS URL with plausible-public DNS-form syntax (the syntax check does not verify public ownership or live DNS);
- explicit commercial-interactive, derivative-animation, and solver-output permission;
- a named performer plus a separately hash-pinned, verified release;
- named rights and throw-form reviewers with staging dispositions;
- an authentic continuous single-performer throw with no cuts, stationary camera, full body, both feet, throwing hand, and disc visible;
- a constant frame rate of at least 60 fps, at least 1920x1080 in either orientation, one of `H264`, `H265`, `PRORES_422`, or `PRORES_422_HQ`, and 1-30 seconds duration;
- planned leaf packages exactly named `/Game/DiscGolf/Animation/Mocap/Source/A_<source_id>_<asset_revision>_RAW` and `/Game/DiscGolf/Animation/Mocap/Retargeted/A_<source_id>_<asset_revision>_RTG`; `Cleaned` and `Production` must remain `null`;
- `DGMASTER` runtime authority and `production_promotion_allowed: false`.

The command writes no project files. It hashes and rehashes the source through the same open file handle used by the built-in ISO-BMFF probe, checks that the handle identity and metadata stayed stable, and compares the declared codec, dimensions, constant frame rate, and duration with the parsed `.mov`/`.mp4` sample structure. The probe validates an accepted ISO/QuickTime file-type brand, exact sample-to-`mdat` extents, and minimally parsed `avcC`/`hvcC` decoder configuration for AVC/H.265 entries; it is not a full frame decoder and cannot prove that every frame decodes, visual authenticity, public URL ownership, or legal rights. Named human attestations are bound by the intake hash only on a valid pass and remain human claims. Consume the JSON only when `status` is `PASS_STAGING_ELIGIBLE`, `staging_eligible` is `true`, and `disk_mutation` is `NONE`. The output deliberately keeps production, release, and legal approval false.

## Technical import convention

| Property | Project convention |
|---|---|
| Coordinate system | Unreal: left-handed, `+X` forward, `+Y` right, `+Z` up |
| Scale | Mono video has no declared physical units; calibrate from performer body height/proportions and the MetaHuman/SMPL source meshes, then verify the DGMaster export in Unreal centimeters |
| Frame rate | Preserve the preflight-bound constant source rate; authentic intake requires 60 fps or higher |
| Root | One root named `root`; horizontal motion may be preserved for review, but gameplay pawn/capsule authority is unchanged |
| Floor | Ground plane at `Z=0`; both feet and the full body must remain visible in source reference |
| Handedness | Explicit registry metadata; never infer from the filename |
| Hand orientation | Throwing-hand palm/grip orientation is reviewed against `disc_grip_r` or `disc_grip_l`; Session 5 gameplay coverage remains RHBH only |
| Naming | Stable `MotionId`, source ID, take ID, semantic throw descriptor, and monotonically increasing version |
| Source skeleton | Retained as a distinct project-owned source skeleton; never force its hierarchy onto DG Master |
| Target skeleton | Always the accepted `SKEL_DG_Master` hierarchy |

## Content stages

The four stages are distinct packages:

```text
/Game/DiscGolf/Animation/Mocap/Source
/Game/DiscGolf/Animation/Mocap/Retargeted
/Game/DiscGolf/Animation/Mocap/Cleaned
/Game/DiscGolf/Animation/Mocap/Production
```

The Session 5 synthetic pipeline fixture uses:

- source animation: `/Game/DiscGolf/Animation/Mocap/Source/A_DG_RHBH_SyntheticSource_RAW`
- source mesh: `/Game/DiscGolf/Animation/Mocap/Source/SK_DG_RHBH_SyntheticSource`
- source skeleton: `/Game/DiscGolf/Animation/Mocap/Source/SKEL_DG_RHBH_SyntheticSource`
- source IK Rig: `/Game/DiscGolf/Animation/Mocap/Rigs/IK_DG_RHBH_SyntheticSource`
- retargeter: `/Game/DiscGolf/Animation/Mocap/Rigs/RTG_DG_RHBH_Synthetic_To_Master`
- retargeted animation: `/Game/DiscGolf/Animation/Mocap/Retargeted/A_DG_RHBH_Synthetic_RTG`
- cleaned animation: `/Game/DiscGolf/Animation/Mocap/Cleaned/A_DG_RHBH_Synthetic_CLN`
- pipeline-test sequence: `/Game/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001`
- pipeline-test montage: `/Game/DiscGolf/Animation/Mocap/Production/AM_DG_RHBH_SyntheticPipelineTest_v001`
- test-only animation library: `/Game/DiscGolf/Animation/Mocap/Production/DA_DG_AnimationLibrary`

These names identify a validation fixture, not production mocap. A legitimate take receives its own immutable source packages and versioned downstream packages.

## MetaHuman-to-DGMaster retarget contract

The assembled capture-side endpoints currently installed in this project are:

- source body: `/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/Body/SKM_MHC_DG_Golfer_Default_BodyMesh`;
- source skeleton: `/Game/DiscGolf/Characters/MetaHuman/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel`;
- source IK Rig: `/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig`;
- target mesh/skeleton/IK Rig: the frozen `SK_DG_Master`, `SKEL_DG_Master`, and `IK_DG_Master` assets;
- installed editor-authoring one-way retargeter: `/Game/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster`.

`RTG_DGMaster_To_MetaHuman` remains the runtime presentation retargeter and must not be reversed, duplicated over, or modified for capture ingestion. Use `Scripts/inspect_dg_metahuman_to_master_inputs.py` for a read-only live input audit. `Scripts/author_dg_metahuman_to_dgmaster_retargeter.py` validates the installed reverse retargeter without writes; creating it when absent additionally requires exact environment opt-in `DG_METAHUMAN_TO_DGMASTER_AUTHORING=1`. The author refuses to overwrite an existing package, binds explicit non-fuzzy Root/Spine/Neck/Arm/Leg mappings, requires the five UE 5.8 offline retarget-operation classes to be present and enabled, protects both meshes, both skeletons, and both IK rigs by pre/post hash, and permits exactly one new `.uasset`. The accepted package is 14,068 bytes with SHA-256 `B66227838C0857416119BB8A17534A106704081DAA63833F02D99A9AF4F21CD8`; a fresh-process reload validated that bounded contract without writes.

`Scripts/run_dg_metahuman_to_dgmaster_retargeter_inspection.py --execute` is the guarded host path for the mutation-free per-operation inspector. It freezes UE 5.8.2 CL 56702186, the exact commandlet/script/switch, one external UUID `UserDir`, the source/target rigs and preview meshes, strict JSON, fatal Unreal/Python/core error-marker rejection, and pre/post identities for 32 package/sidecar paths. Live run `a200c1f3-752d-41b2-a511-1ed9fb92b231` passed with zero fatal markers and zero drift; the byte-identical retained receipt is `Evidence/Session19/MetaHumanToDGMasterRetargeterInspectionReceipt.json`, 31,178 bytes, SHA-256 `75ED86A657F1634C594A17CC65456324E750F19AB0176DE0CA0AA68FA7C565C3`.

The inspection confirms five enabled unique operations; exact identical FK and Run-IK mappings for Root, Spine, Neck, both arms, and both legs; explicit `None` mappings for both target foot chains; full-weight interpolated FK with no translation; full-weight arm/leg IK; zero pelvis offsets with floor constraint `0`, horizontal IK influence `1`, and vertical IK influence `0`; and root motion copying source `pelvis` to target `root` with source height, maintained pelvis offset, propagation enabled, and pelvis rotation disabled. Curve copy/remap is enabled with no explicit pairs. These are serialized-settings facts, not proof that the values are suitable for a solved clip. This editor-authoring asset is not runtime gameplay authority and does not itself require or grant Shipping inclusion.

The installed MetaHuman IK Rig has no independent foot/toe chains. Its leg chains extend through `ball_l` and `ball_r`, while DGMaster keeps `Foot_L` and `Foot_R` separate. The reverse retargeter therefore leaves those two DG target chains explicitly unmapped rather than inventing an ambiguous double mapping. Toe articulation, planted-foot locking, ground contact, and disc contact remain mandatory non-destructive cleanup and human-review work; authoring the retargeter does not approve animation quality or production use.

Two additional topology mismatches remain explicit blockers. The source `Head` chain is not independently mapped because DGMaster currently folds `head` into its `Neck` chain; source head/look rotation is therefore not proved to survive this mapping. Also, the source legs end at `ball_l` and `ball_r` while the mapped DGMaster leg chains end at `foot_l` and `foot_r`. A solved-clip export must demonstrate head motion, ankle placement, pelvis/root behavior, and the absence of double motion before cleanup or promotion. The enabled Root Motion operation is not by itself a root-motion policy or quality proof.

After a cleared solve and cleanup candidate exists, run the two body-profile lanes separately. `Scripts/run-session4-profile-smokes.py` is the five-profile DGMaster gameplay authority. `Scripts/run-session19-v006-metahuman-profile-matrix.py` is the fixed curated MetaHuman target proof: it accepts only Baseline because that target has no runtime body-profile mapping, while `--include-unsupported-probes` may collect the other four profiles without counting them as passes. The MetaHuman runner requires exact v6/v006 montage identity, five distinct hash-bound checkpoint PNGs, schema-v6 absolute-root/unit-world-scale evidence, an isolated external UserDir, no load/write access to the production profile, and explicitly false human/Shipping/release claims. A future profile-aware target expansion needs its own assets and contract; it must not be simulated by scaling the fixed preset. Run `Scripts/validate_dg_session19_production_motion_cook.py --preflight` before creating a fresh Shipping candidate. The source-only cook preflight proves only the configured runtime/editor boundary; it leaves `shippingCookPresenceAccepted` false until a fresh Windows Shipping IoStore and AssetRegistry are audited.

### Append-only procedural v007 lane

`SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v7.json` is a project-authored procedural source candidate, not performer mocap or a promoted production asset. Run `Scripts/generate_dg_session19_production_motion_v7.py --check` and `Scripts/validate_dg_session19_production_motion_v7.py --source-only` before any Unreal mutation. The guarded author and read-only validator require the exact v007 recipe identity and source-motion contract, protect all current-v006 asset/evidence identities, and target seven separately named v007 assets. Runtime selection remains v006; `-DGProductionMotionCandidate=v007` is developer-only, requires the exact unattended Session 19 capture contract, and is unavailable in Shipping. The seven v007 assets and authoring evidence exist; fresh-process validation, six-checkpoint DGMaster capture, five-checkpoint fixed-MetaHuman capture, and focused retarget-policy automation pass. Shipping inventory, 60 Hz temporal review, calibrated target grip/contact, and named human animation/contact approval are still required before promotion.

The dedicated `-Session19TemporalFrameDump` developer lane is the temporal gate, not a promotion mechanism. It runs the live fixed-MetaHuman throw without checkpoint pauses, records exact phase markers, and exits at Recovery after one authoritative release. The first v007 run produced 139 full-HD frames and verified H.264 clips but failed human-facing review for upright pivoting, crossed feet, weak brace transfer, and a rigid throwing-arm shelf. Technical checkpoint acceptance therefore does not supersede temporal rejection.

## Repeatable processing procedure

1. Pass the authentic-footage preflight and retain its exact source, rights, release, review, and technical facts. Do not ingest a failed or synthetic substitute.
2. In the UE 5.8 Editor, ingest the cleared `.mov` or `.mp4` through Capture Manager as mono video, create a `MetaHumanPerformance` with `MONO_FOOTAGE`, enable body tracking, and process it in blocking/offline mode.
3. Verify that the performance contains BODY animation data and a nonzero processed-frame range before export. Starting the pipeline alone is not success.
4. Export the body solution with the performer-proportioned MetaHuman and SMPL source meshes, then retarget through the dedicated `RTG_MetaHuman_To_DGMaster` asset into the planned `Retargeted` package. `RTG_DGMaster_To_MetaHuman` is the wrong direction and remains frozen runtime presentation input.
5. Retain the raw sequence on its distinct source skeleton. Do not modify the original source file or an earlier raw package.
6. Verify the exact installed source and target IK Rig chain/bone/goal contracts, use the seven explicit Root/Spine/Neck/Arm/Leg mappings, and keep DGMaster `Foot_L`/`Foot_R` unmapped until non-destructive toe/contact cleanup. Do not accept fuzzy auto-mapping as evidence.
7. Duplicate the retargeted sequence into `Cleaned`. Apply bounded root-drift, foot/brace, shoulder, elbow, knee, spine, and hand/grip corrections non-destructively. The upstream packages remain byte-stable.
8. Duplicate the accepted cleaned result into a versioned production-candidate sequence. Add the 19 required DG curves and phase metadata.
9. Create a montage with queued phase events and exactly one branching-point `DG Release Disc` plus exactly one branching-point `DG Throw Finished`.
10. Register the montage in `DA_DG_AnimationLibrary`. A `SYNTHETIC_TEST` entry remains non-default and `DO_NOT_SHIP`.
11. Run strict asset/no-write validation, all five DGMaster gameplay-profile smokes, the supported fixed-MetaHuman Baseline proof, any explicitly non-acceptance compatibility probes, the single-authority throw smoke, the full regression suite, and rendered review before promotion.

MetaHuman body tracking places body keypoints automatically; there is no supported workflow for manually pinning arbitrary nodes onto video frames. The installed tracker is a Beta/Experimental, Editor-only authoring path. It does not track the disc or solve hand-to-disc contact. Grip, wrist orientation, release timing, foot locking, occlusion defects, and disc contact still require DGMaster Control Rig/IK cleanup and frame-by-frame human review. It is not runtime gameplay authority.

## Cleanup and contact requirements

Cleanup is evaluated on the accepted master rig and must keep raw, retargeted, and cleaned packages separate. Review at least:

- planted-foot translation and rotation;
- brace stability and root drift;
- throwing-hand trajectory and disc-grip alignment;
- shoulder volume and spine rotation;
- mirrored elbow and knee bend direction;
- follow-through and recovery continuity;
- finite transforms and bounded segment lengths on ShortCompact, Baseline, and TallLongArms.

The Control Rig is a presentation/cleanup layer. It must not resize the gameplay capsule or alter the cached throw command.

## Phase and release contract

A gameplay-ready RHBH montage must contain ordered events for setup/aim, run-up, reachback, plant, acceleration, follow-through, and recovery. It must contain exactly one `DG Release Disc` and exactly one `DG Throw Finished`.

The release frame is selected through animation and grip inspection, not copied blindly from video. At runtime the notify supplies only the grip transform to the existing adapter. The cached project throw command remains the source of every flight input.

## Promotion rule

A clip may be labeled production only when all of the following are true:

- source and rights evidence are complete and hash-pinned;
- the separate append-only production source-and-license manifest binds the exact cleared source, rights, release, preflight result, and source-to-runtime asset chain without changing the frozen registry;
- source, retargeted, cleaned, and production packages are distinct;
- strict phase and event cardinality passes;
- all five DGMaster gameplay-profile smokes and the supported fixed-MetaHuman target proof pass without counting unsupported target/body combinations;
- single-authority release and gameplay regressions pass;
- manual animation and disc-contact review passes.

Until then, the asset is a pipeline fixture or production candidate. Session 5 does not imply that a production-quality mocap performance has been integrated.
