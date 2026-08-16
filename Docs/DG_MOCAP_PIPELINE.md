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

Every take must have a registry entry before import. Unknown, missing, or ambiguous permission always resolves to `DO_NOT_SHIP`. Professional footage may be used as visual reference only unless the project retains explicit rights for the motion source, performer, solver output, derivative work, and commercial interactive use.

Accepted source classifications are:

- `PROJECT_OWNED_CAPTURE`
- `WRITTEN_PERMISSION`
- `VALID_COMMERCIAL_LICENSE`
- `SYNTHETIC_TEST`

`SYNTHETIC_TEST` proves the tooling but can never be promoted as production mocap. A production promotion additionally requires a retained evidence file and matching evidence hash.

Original source files belong under:

`SourceArt/DiscGolf/Mocap/<MotionId>/`

Do not edit the original file in place. Record its byte length and SHA-256 before any conversion.

## Technical import convention

| Property | Project convention |
|---|---|
| Coordinate system | Unreal: left-handed, `+X` forward, `+Y` right, `+Z` up |
| Units | Centimeters in Unreal; importer converts the declared source units |
| Frame rate | Preserve the registry rate; prefer 60 fps or higher for capture |
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

## Repeatable processing procedure

1. Add and validate the registry entry, source hash, rights evidence, coordinate metadata, frame rate, handedness, and throw type.
2. Import or create the raw sequence on a distinct source skeleton. Do not modify the original source file or an earlier raw package.
3. Create a source IK Rig with explicit root, pelvis, spine, neck, arm, leg, foot, and hand chains.
4. Create an IK Retargeter targeting the accepted `IK_DG_Master`. Map chains explicitly; do not accept fuzzy auto-mapping as evidence.
5. Batch-retarget into the `Retargeted` directory without overwriting an existing result.
6. Duplicate the retargeted sequence into `Cleaned`. Apply bounded root-drift, foot/brace, shoulder, elbow, knee, spine, and hand/grip corrections non-destructively. The upstream packages remain byte-stable.
7. Duplicate the accepted cleaned result into a versioned production-candidate sequence. Add the six DG curves and phase metadata.
8. Create a montage with queued phase events and exactly one branching-point `DG Release Disc` plus exactly one branching-point `DG Throw Finished`.
9. Register the montage in `DA_DG_AnimationLibrary`. A `SYNTHETIC_TEST` entry remains non-default and `DO_NOT_SHIP`.
10. Run strict asset/no-write validation, the three-profile compatibility matrix, the single-authority throw smoke, the full regression suite, and rendered review before promotion.

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
- the registry status permits commercial shipping and derivatives;
- source, retargeted, cleaned, and production packages are distinct;
- strict phase and event cardinality passes;
- the three-profile compatibility matrix passes;
- single-authority release and gameplay regressions pass;
- manual animation and disc-contact review passes.

Until then, the asset is a pipeline fixture or production candidate. Session 5 does not imply that a production-quality mocap performance has been integrated.
