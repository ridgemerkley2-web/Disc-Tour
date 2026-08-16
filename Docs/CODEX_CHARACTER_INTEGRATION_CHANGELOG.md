# Character Framework Integration Changelog

## Session 1 — project audit and plugin compile

Date: 2026-08-14
Status: **SESSION 1: PASS**

This session followed only `CODEX/01_AUDIT_AND_PLUGIN.md`. Sessions 2–16 remain deferred. The plugin is compiled and loadable, but its gameplay-adjacent systems are intentionally dormant.

## Package identity and provenance

| Field | Verified value |
|---|---|
| Package | Disc Golf Core Playability Kit v1.5.0 only |
| User-supplied ZIP | `C:\Users\ridge\DiscGolfCorePlayabilityKit_v1.5.zip` |
| ZIP SHA-256 | `016AF0321958EE15CC4CCE8B2B9297D321A480224E2DF07AB23687CA243B009D` |
| Project-local kit | `C:\DGTour\_BuildKit\DiscGolfCorePlayabilityKit_v1.5` |
| Kit verification | 252 files / 460,209 bytes; all 251 `FILE_INVENTORY.json` records matched by byte size and SHA-256 |
| Installed plugin | `C:\DGTour\Plugins\DiscGolfCharacterFramework` |
| Installed donor payload | 96 files before generated `Binaries`/`Intermediate` output |

The supplied package has no `LICENSE`, `COPYING`, `NOTICE`, canonical publisher URL, or source repository. Its descriptor says only `CreatedBy: "Disc Golf Project"` and marks the plugin beta. No license, authorship, or redistribution right is inferred. `VALIDATION_REPORT.json` contains stale v1.4/234-file metadata, while the verified archive is v1.5/252 files. The embedded final build report also states that Unreal compilation had not previously been run.

## Real-project audit

| Area | Existing authoritative implementation |
|---|---|
| Project | `C:\DGTour\DiscGolfTour.uproject` |
| Engine | Unreal Engine 5.8.1, CL 56057345 |
| Project modules | `DiscGolfTour` runtime and `DiscGolfTourEditor` editor module |
| Player | `ADiscGolferPawn` |
| Input | Enhanced Input through `UDiscGolfInputConfig` and pawn bindings |
| Aim/release intent | `UThrowControllerComponent`; two-press timing input reaches `ADiscGolfTourGameMode::RequestThrow` |
| Release resolution | `DiscGolfMath::ResolveThrowRelease` |
| Gameplay launch | `ADiscGolfTourGameMode::LaunchThrow` -> `ADiscActor::InitializeDisc` / `ADiscActor::Throw` |
| Flight physics | `UDiscFlightComponent::Launch`; custom fixed-step 240 Hz simulation |
| Existing animation presentation | `UDiscGolferPresentationComponent`, which follows the authoritative release and does not own physics |

The current pawn uses placeholder static cylinder/head presentation. It has a skeletal-mesh component that remains hidden unless an asset is assigned. No production character skeletal mesh, character Animation Blueprint, montage, IK Rig, Control Rig, or throwing-animation asset was found. Those are Session 2/3 work, not Session 1 work.

The audit found zero exact native/reflected symbol collisions between the plugin and project. It did find broad conceptual overlap: throw/release, bag/catalog, course, camera/replay/tracer, telemetry, scoring, save/settings, UI, audio, environment, and playability. The existing project remains authoritative for every one of those systems.

## Installation boundary

- Added the verified v1.5 kit beside the project under `_BuildKit`.
- Installed the complete runtime plugin under `Plugins/DiscGolfCharacterFramework`.
- Enabled `DiscGolfCharacterFramework` in `DiscGolfTour.uproject`.
- Did **not** add `DiscGolfCharacterFramework` to `DiscGolfTour.Build.cs`.
- Did not include plugin headers from project gameplay code.
- Did not add plugin components or notifies to the pawn, GameMode, animation, disc, course, or save flow.
- Did not create or modify any `.uasset`, rig, skeleton, Animation Blueprint, montage, character, or proxy content.

The plugin's release notify and throw component must not be wired beside the existing release path without an explicit Session 3 adapter, because that could create two release authorities. The plugin's bag, course, camera, replay, scoring, save, settings, UI, environment, and playability implementations likewise remain unused until their ordered sessions decide whether to adapt or reject them.

Before the full plugin install, a premature project-native copy of the v1.5 playability types was removed so Unreal would have only one owner for those reflected names. The independent schema-6 save/restore correction already made in the project was retained; it does not depend on or activate the plugin.

## UE 5.8 compatibility changes

Only the installed plugin was adapted. The project-local `_BuildKit` donor remains a verified, unchanged reference.

1. Added `class UMaterialInterface;` to `DiscGolfOutfitTypes.h` so its soft material pointer is declared under strict IWYU compilation.
2. Added `class AActor;` to `DiscGolfAvatarBackendProfile.h` so its soft actor class pointer is declared under strict IWYU compilation.
3. Renamed plugin `DiscGolfCourseDefinition.h` to `DGFrameworkCourseDefinition.h`, updated its generated-header include, and updated `DiscGolfCourseValidatorLibrary.cpp`.
4. Renamed plugin `DiscGolfEnvironmentTypes.h` to `DGFrameworkEnvironmentTypes.h`, updated its generated-header include, and updated `DiscGolfEnvironmentBridgeComponent.h`.

The two renames were required by Unreal Header Tool, which rejects duplicate header basenames anywhere in one target. The original names collided with the existing project headers even though the reflected type names themselves were unique. The first build stopped at this UHT guard; the renamed installed-plugin headers resolved it without altering class names or gameplay behavior.

## Verification evidence

### Source-only checks

| Check | Result |
|---|---|
| `python Scripts/validate_project.py` | PASS — 70 project C++ files / 57 project headers |
| `python Scripts/reference_flight_check.py` | PASS — reference flight, release, mirror, and ground envelopes unchanged |
| `python -m py_compile Scripts/verify_character_framework_session1.py` | PASS |

### UE editor build

Command:

```powershell
.\Scripts\build-unreal.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
```

Result: PASS. UnrealBuildTool completed 43 actions after UHT, compiled all 31 plugin `.cpp` files, and linked:

`C:\DGTour\Plugins\DiscGolfCharacterFramework\Binaries\Win64\UnrealEditor-DiscGolfCharacterFramework.dll`

Final markers: `Result: Succeeded`; `Unreal editor build succeeded.` Total execution time was 217.43 seconds.

### Plugin load and reflection

Probe: `Scripts/verify_character_framework_session1.py`  
Log: `Saved/Logs/CharacterFramework_Session1_Reflection.log`

Result: PASS, exit code 0. The editor mounted the project plugin, loaded its DLL, and resolved all required reflected objects:

- `/Script/DiscGolfCharacterFramework.DiscGolfThrowComponent`
- `/Script/DiscGolfCharacterFramework.DiscGolfAnimInstance`
- `/Script/DiscGolfCharacterFramework.AnimNotify_DiscRelease`
- `/Script/DiscGolfCharacterFramework.DiscGolfCharacterProfile`
- `/Script/DiscGolfCharacterFramework.DGBodyProfile`
- `/Script/DiscGolfCharacterFramework.DGThrowStyle`
- `/Script/DiscGolfCharacterFramework.DGThrowIntent`
- `/Script/DiscGolfCharacterFramework.DGReleaseData`

The commandlet completed with `0 error(s), 0 warning(s)` and no missing-module, fatal, assertion, ensure, exception, or crash marker.

### Full project automation

Log: `Saved/Logs/Automation_CharacterFramework_Session1_Full.log`

Result: PASS — 102/102 `DiscGolfTour.` automation tests succeeded; exit code 0. The log independently records the plugin mount and DLL load before the tests.

### Post-install gameplay smoke

Log: `Saved/Logs/ThreeHoleRoundSmoke_CharacterFramework_Session1.log`

Result: PASS — the plugin mounted and loaded, then the existing real gameplay path completed the three-hole validation round at 3/3 holes, 3 strokes on par 11 (-8). Existing physics, basket detection, scoring, transitions, scorecard state, and save snapshot behavior remained operational.

An earlier, pre-plugin `PineRidgePlaySmokeTest` resolved a legal deep-rough lie but missed that smoke test's final-Z acceptance threshold at -4.2 cm. That observation predates plugin installation, is unrelated to Session 1, and was not hidden or changed to obtain the passing plugin acceptance evidence.

## Deferred work

- Session 2: master skeleton, proxy character, IK Rig, Control Rig, and Animation Blueprint.
- Session 3: one complete RHBH animation, exact release notify, and a single adapter into the existing physics authority.
- Sessions 4–7: character creator, mocap pipeline, outfits, and full customization.
- Sessions 8–10: MetaHuman, brand/license audit, and optional quality/environment adapters.
- Sessions 11–14: equipment/Throw Lab, presentation adapters, course tooling, competition/AI/save expansion.
- Session 15: final vertical-slice integration.
- Session 16: adapted playability monitoring and complete Smoke/Core Loop/Round/Persistence gates.

At the Session 1 checkpoint, no Session 2–16 integration had been started.

---

## Session 2 — master character rig and IK foundation

Date: 2026-08-14
Status: **SESSION 2 STRUCTURAL ACCEPTANCE: PASS**

This session followed `CODEX/02_MASTER_RIG_AND_IK.md` only. It establishes an unassigned character, skeleton, IK, Control Rig, and Animation Blueprint foundation. Session 3 throw/release integration and Session 4 runtime body-profile deformation remain deferred. The existing pawn, input, throw controller, release resolver, disc launch/flight, scoring, course, replay, and save systems remain authoritative.

### Proxy source and project setup

No compatible project-owned master character existed. The only prior skeletal content was an incompatible UE4 mannequin embedded in a vegetation vendor pack, so it was not modified or adopted as the DG master.

- Generated `SourceArt/DiscGolf/Characters/SK_DG_Master_Proxy.blend` and `.fbx` from the v1.5 proxy generator in an isolated Blender 5.2.0 LTS process.
- The generated character is a blocky, rigid-weighted validation proxy, not production character art.
- Enabled Control Rig, IK Rig, and Full Body IK. Skeletal Mesh Modeling Tools is enabled only for Editor targets.
- Added deterministic source validation, import, rig-authoring, and strict live-asset validation scripts under `Scripts/`.
- One behavior-neutral project-source fix was required by a unity build: the private foliage constant `BasicShapeHalfExtentCm` was renamed to `FoliageBasicShapeHalfExtentCm` in `DiscGolfFoliagePresentationActor.cpp`. Its value and both uses are unchanged. No player, input, throw, release, flight, lie, scoring, course, replay, save, or configuration behavior changed.

The source validation report records the earlier development-mirror path. The current `C:\DGTour` source files are byte-identical:

| Source | SHA-256 |
|---|---|
| `SK_DG_Master_Proxy.blend` | `24E5B920A5EDC70E4DB51D4CFE4E02EEB65ED9D57C30640E2899C12AA320A40C` |
| `SK_DG_Master_Proxy.fbx` | `82ED7FC89A572A346460CCB5530EA493B8397BC0D8BECBC2E22C64B8A31CD683` |

### Assets created

| Role | Object path |
|---|---|
| Master skeletal mesh | `/Game/DiscGolf/Characters/Meshes/SK_DG_Master` |
| Master skeleton | `/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master` |
| IK Rig | `/Game/DiscGolf/Rigs/IK_DG_Master` |
| Control Rig | `/Game/DiscGolf/Rigs/CR_DG_Master` |
| Animation Blueprint | `/Game/DiscGolf/Animation/ABP_DG_Player` |
| Baseline character profile | `/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter` |
| Short test fixture | `/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact` |
| Tall test fixture | `/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms` |

Exactly these eight Session 2 assets were added. No montage, animation sequence, throw notify, release callback, skeletal assignment, or gameplay adapter was created.

### Master skeleton contract

`Saved/CharacterFramework/Session2Validation.json` validates the live UE assets against v1.5 `DG_MasterSkeletonContract.json` version 1, SHA-256 `E2223DE774CE26900BD482130939EAAB6F813023EBB689447FCB00697C34A7B2`.

- Exact 69/69 required bones and all 69 parent relationships.
- Sole root is `root`; no extra Blender `Armature` root exists.
- Imported proxy bounds are 168.0 x 22.5 x 171.25 cm.
- All six required curve metadata names are present with no extras: `DG_FootPlant_L`, `DG_FootPlant_R`, `DG_ReachbackAlpha`, `DG_BraceAlpha`, `DG_ReleaseApproachAlpha`, and `DG_FollowThroughAlpha`.
- Helper and grip bones are non-deforming and have no weighted vertex groups.
- `disc_grip_l` and `disc_grip_r` are direct children of their matching hands. Their origins are mirrored at approximately `[+81.0, 3.5, 144.0]` and `[-81.0, 3.5, 144.0]` cm with 0.0 cm mirror error.
- The source convention is a 5 cm palm-local helper whose primary axis points palm-outward toward the fingertips (`+X` left, `-X` right in the neutral T-pose). A real disc fit/orientation check remains a manual visual gate.

### IK Rig

`IK_DG_Master` uses `SK_DG_Master` as its preview mesh and contains exactly one enabled `/Script/IKRig.IKRigFBIKController`.

- Solver root and retarget root: `pelvis`.
- Root-motion bone: `root`.
- Root behavior: Free; stretch disabled; 20 iterations, 10 sub-iterations, zero global pull-chain alpha.
- Four connected goals, each with chain depth 2: `hand_l_Goal -> hand_l`, `hand_r_Goal -> hand_r`, `foot_l_Goal -> foot_l`, and `foot_r_Goal -> foot_r`.
- Nine exact chains: Root (`root -> root`), Spine (`spine_01 -> spine_04`), Neck (`neck_01 -> head`), Arm L/R (`upperarm -> hand`), Leg L/R (`thigh -> foot`), and Foot L/R (`foot -> ball`). Chain ancestry is validated.

### Control Rig and plant-foot foundation

`CR_DG_Master` compiles up to date and imports all 69 skeleton bones and six curves. Its Forward Solve graph contains one pelvis-rooted `/Script/PBIK.RigUnit_PBIK` node with four effectors.

- Hand transforms come from `ctrl_hand_l/r`. Their position and rotation alphas come from `dg_hand_ik_alpha_l/r`, both defaulting to `0.0` so the unused foundation cannot pin hands in the reference pose.
- Foot transforms come from `ctrl_foot_l/r`. Their position and rotation alphas are driven by `DG_FootPlant_L/R`.
- PBIK uses Free root behavior, stretch disabled, 10 sub-iterations, zero global pull-chain alpha, and chain depth 2.
- Profile input controls exist for height, wingspan, shoulder width, torso length, leg length, and hand scale.

This is a structurally validated plant-foot correction strategy, not a claim of stable feet during a keyed throw. The straight proxy reference limbs use non-Mannequin local axes, so mirrored elbow/knee preferred bend directions must be chosen from an in-editor compression test rather than copied from Epic mannequin defaults.

### Animation Blueprint

`ABP_DG_Player` targets `SKEL_DG_Master`, has AnimGraph and EventGraph foundations, generates `ABP_DG_Player_C`, and compiles up to date. Its parent class is `/Script/DiscGolfCharacterFramework.DiscGolfAnimInstance`.

The Animation Blueprint is intentionally not assigned to `ADiscGolferPawn`. No plugin throw component, release notify, `OnDiscRelease` callback, or second release authority was added.

### Body-profile fixtures

All three fixtures share the same intended master mesh, skeleton, and Control Rig foundation. No alternate skeleton was created.

| Profile | Height | Wingspan | Shoulder | Torso | Leg | Hand |
|---|---:|---:|---:|---:|---:|---:|
| ShortCompact | 155 cm | 0.94 | 0.94 | 0.96 | 0.95 | 0.95 |
| Baseline | 183 cm | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| TallLongArms | 205 cm | 1.07 | 1.05 | 1.04 | 1.05 | 1.05 |

The profile data and matching rig-input controls pass. Runtime profile-to-rig plumbing, actual mesh deformation, throw-style tuning, creator UI, and persistence are Session 4 work and are not claimed here.

### Verification evidence

| Gate | Result | Evidence |
|---|---|---|
| Python syntax | PASS | Session 2 production scripts compiled with UE/Python tooling |
| Project validator | PASS | `Scripts/validate_project.py` |
| Reference flight check | PASS | Existing flight, release, mirror, and ground envelopes unchanged |
| Trajectory artifacts | PASS | 178 samples across 6 scenarios/presets |
| UE Editor build | PASS | `DiscGolfTourEditor Win64 Development`, 109.89 s |
| UE runtime build | PASS | `DiscGolfTour Win64 Development`, 295.54 s; linked `Binaries/Win64/DiscGolfTour.exe` |
| Framework reflection | PASS | `Saved/Logs/CharacterFramework_Session2_Reflection.log`; 4 classes + 4 structs; 0 errors/warnings |
| Strict live-asset validation | PASS | `Saved/Logs/CharacterFramework_Session2_Validation_Final.log`; 69 bones, 4 goals, 4 effectors, 3 profiles, `gameplay_wiring=NONE_SESSION_2`; 0 errors/warnings |
| Validation-only rerun | PASS | `Saved/Logs/CharacterFramework_Session2_NoWriteRerun.log`; exit 0, 8/8 generated packages unchanged by SHA-256, `ChangedPackages=0`, 0 errors/warnings |
| Full automation | PASS | `Saved/Logs/Automation_CharacterFramework_Session2_Full.log`; 102/102 successes, 0 failures/not-run |
| Three-hole gameplay smoke | PASS | `Saved/Logs/ThreeHoleRoundSmoke_CharacterFramework_Session2.log`; 3/3 holes, 3 strokes on par 11 (-8) |

Visual evidence:

- `Saved/CharacterFramework/Screenshots/Session2_SK_DG_Master_Front_Skeleton.png`
- `Saved/CharacterFramework/Screenshots/Session2_DiscGrip_L_Closeup.png`
- `Saved/CharacterFramework/Screenshots/Session2_DiscGrip_R_Closeup.png`
- `Saved/CharacterFramework/Screenshots/Session2_IK_DG_Master_Solver.png`
- `Saved/CharacterFramework/Screenshots/Session2_CR_DG_Master_PBIK_Graph.png`

### Preservation and deferred work

The original 605 Content files retain baseline digest `92D95B055243221174413EE6AEC2B0564E8A7331E77B6486183773513142A561`. `DiscGolfTour.Build.cs` remains unchanged. `ADiscGolferPawn` remains the default pawn, and `RequestThrow -> LaunchThrow -> InitializeDisc/Throw` remains the only release/flight authority. The known pre-existing Pine Ridge final-Z miss remains untouched.

The remaining Session 2 rig and provisional-disc checks were completed in the visual closeout below. The proxy remains validation geometry rather than production character art, and its block hands cannot establish production grip ergonomics.

- Session 3: first RHBH animation/montage, phase/release/finished notifies, held-disc attachment, single one-way handoff into existing release authority, and gameplay assignment.
- Session 4: runtime body/style deformation, profile plumbing, creator/save integration, and the three-profile animation matrix.
- Session 5+: production mocap/retargeting, art, outfits, and later customization work.

No Session 3 integration was started.

### Session 2 visual closeout

Date: 2026-08-15
Status: **SESSION 2 VISUAL ACCEPTANCE: PASS FOR THE VALIDATION PROXY AND CURRENT PROVISIONAL GAMEPLAY DISC — PRODUCTION CHARACTER/DISC ART AND RUNTIME BODY DEFORMATION REMAIN DEFERRED**

The protected structural baseline was committed first as `01a998bf51d551c7c2ae9c9ae75415d9c33c0418` (`Session 2: add DG master proxy rig and animation foundation`). The accidentally dirty `SK_DG_Master*` tab was closed without saving; the disk asset reopened with the exact 69-bone hierarchy. Its SHA-256 remains `5C461476D6877DFBE3E6DF08FF48CDF5BB940BC8C883D3E3F43058331DCC186F`.

Only the two rig assets were intentionally changed after that checkpoint:

- `/Game/DiscGolf/Rigs/IK_DG_Master`
- `/Game/DiscGolf/Rigs/CR_DG_Master`

The final mirrored preferred-angle contract is:

| Bone | Preferred angle (X, Y, Z degrees) |
|---|---:|
| `lowerarm_l` | `(0, 0, -45)` |
| `lowerarm_r` | `(0, 0, +45)` |
| `calf_l` | `(+45, 0, 0)` |
| `calf_r` | `(+45, 0, 0)` |

Both rigs retain Free root behavior, stretch disabled, 20 iterations, 10 sub-iterations, zero global pull-chain alpha, and chain depth 2. Control Rig PBIK root, effector, and bone-setting FNames now persist as raw values such as `pelvis`; the earlier quoted export-text form (`"pelvis"`) caused a root-not-found initialization warning. A transient `ControlRigComponent` now executes the saved graph with 88 hierarchy keys and no PBIK initialization warning.

#### Compression and plant-foot acceptance

| Check | Result |
|---|---|
| Mirrored elbows | PASS — forward offsets 15.2225 / 15.8748 cm; mirror-position error 2.07484 cm; no inversion observed |
| Mirrored knees | PASS — forward offsets 27.2859 / 27.3133 cm; mirror-position error 0.11894 cm; no inversion observed |
| Reachable PBIK targets | PASS — maximum effector error 0.4252 cm |
| No-stretch contract | PASS — effectively zero segment-length change; unreachable extension targets remain short rather than stretching |
| Brace-foot stability | PASS — 133.5103 cm throwing-hand sweep with 0.017151 cm foot drift, 0 degrees rotation drift, and 0.017151 cm toe drift while the spine rotates 12.6821 degrees |

The screenshots show the intended mirrored bend direction, compressed and extended solver states, and one fixed plant marker across reachback, brace, and follow-through stress poses. The disconnected-looking rigid blocks are a limitation of the generated validation proxy, not production skinning or anatomy.

#### Provisional gameplay-disc grip check

The visual test used the same mesh as the native gameplay disc: `/Engine/BasicShapes/Cylinder.Cylinder`, scale `(0.21, 0.21, 0.015)`. It renders at 21 x 21 x 1.5 cm versus the 21.1 cm gameplay physics diameter. Disc local `+X` is release-forward and local `+Z` is the top/normal.

| Hand | Candidate local attachment transform |
|---|---|
| Left | location `(0,0,0)`, rotation `(0,0,0)`, scale `(0.21,0.21,0.015)` on `disc_grip_l` |
| Right | location `(0,0,0)`, rotation `(0,180,0)`, scale `(0.21,0.21,0.015)` on `disc_grip_r` |

The derived palm-local offsets are approximately left `(+3.500001,-5.9999998,-1.999998)` cm and right `(-3.500001,-5.9999998,-1.999998)` cm. Normal-Editor captures place the real Cylinder at the evaluated left/right grip positions with visible `+X/+Y/+Z` markers. This is a PASS for provisional mesh identity, scale, grip position, and axis convention. The capture renderers were transient components positioned from evaluated Control Rig bone coordinates; they were not an animated socket-follow implementation. Literal held-disc attachment/release is Session 3. The flat Cylinder has no authored rim or dome, and the proxy has block hands without deforming fingers, so production palm/rim ergonomics are not accepted here.

#### Shared profile foundation

ShortCompact, Baseline, and TallLongArms were loaded as three distinct fixtures and exercised against the same `SK_DG_Master`, `SKEL_DG_Master`, `IK_DG_Master`, and `CR_DG_Master`. The profile controls resolve correctly, but all three render the same geometry by design. Runtime silhouette/body-proportion deformation remains Session 4 work.

#### Final visual evidence

All eight 1920 x 1080 images are under `Saved/CharacterFramework/Screenshots/Session2_VisualCloseout/`:

- `01_PBIK_Compressed_MirroredElbowsKnees.png`
- `02_PBIK_Extended_NoStretch.png`
- `03_PlantFoot_Start_Reachback.png`
- `04_PlantFoot_Mid_Brace.png`
- `05_PlantFoot_End_FollowThrough.png`
- `06_Grip_Left_Cylinder_Axes.png`
- `07_Grip_Right_Cylinder_Axes.png`
- `08_ProfileFixtures_AllThree.png`

Capture manifest: `Saved/CharacterFramework/Screenshots/Session2_VisualCloseout/Session2_VisualCloseout_CaptureManifest.json`. The dedicated Editor process used `/Engine/Maps/Entry`, created only transient test actors/components, dirtied that startup map in memory, and exited without saving it. All eight Session 2 package hashes were identical before and after capture.

#### Final closeout regression

| Gate | Result | Evidence |
|---|---|---|
| UE Editor build | PASS | `DiscGolfTourEditor Win64 Development`, 90.80 s |
| UE runtime build | PASS | `DiscGolfTour Win64 Development`, 71.17 s |
| Framework reflection | PASS | `Saved/Logs/CharacterFramework_Session2_Closeout_Reflection.log`; 4 classes + 4 structs |
| Strict rig validation | PASS | `Saved/Logs/CharacterFramework_Session2_Closeout_Strict_Final.log`; 69 bones, 4 goals, 4 effectors, 3 profiles, `gameplay_wiring=NONE_SESSION_2`; 0 errors/warnings |
| Transient rig/grip/profile evidence | PASS | `Saved/Logs/CharacterFramework_Session2_Closeout_VisualDynamic_Final.log`; package mutation none |
| Authoring no-write rerun | PASS | `Saved/Logs/CharacterFramework_Session2_Closeout_NoWrite_Final.log`; validation-only, 8/8 package hashes unchanged |
| Full automation | PASS | `Saved/Logs/Automation_CharacterFramework_Session2_Closeout.log`; 102/102 succeeded |
| Three-hole gameplay smoke | PASS | `Saved/Logs/ThreeHoleRoundSmoke_CharacterFramework_Session2_Closeout.log`; 3/3 holes, 3 strokes on par 11 (-8) |
| Original Content seal | PASS | 605 files; digest `92D95B055243221174413EE6AEC2B0564E8A7331E77B6486183773513142A561` |
| Session 3 wiring scan | PASS | Exact eight-package set; no montage, throw/release notify, held gameplay disc, pawn/AnimBP assignment, framework throw component, release callback, or physics handoff |

Final rig SHA-256 values are `13D29A1D4B4F1E2A6F010D95E052D21A1E974A6789BBFE85AC5E4E260EE72958` for `IK_DG_Master` and `3F3A7BABE632C49BFE48070CD05AE73B75188DAA16369AB5E585633295D6F981` for `CR_DG_Master`. Session 2 is closed at the validation-proxy/provisional-disc level. Session 3 has not begun.

---

## Session 3 — first animated RHBH throw and existing-physics handoff

Date: 2026-08-15
Status: **SESSION 3 ACCEPTANCE: PASS**

This session followed only `CODEX/03_FIRST_THROW_AND_PHYSICS.md`. It implements one right-handed backhand drive from input through animation, one release notify, the existing gameplay launch/flight path, follow-through, and recovery. Forehand, putting, additional throw styles, character deformation, creator systems, outfits, MetaHuman work, production animation, and new course content were not started.

### Preflight and protected baseline

The starting validated checkpoint was `01a998bf51d551c7c2ae9c9ae75415d9c33c0418`. Seven intentional Session 2 visual-closeout files were separated from generated evidence and committed before Session 3 as `65be3f3fe370b685e00cf65ba44193aeb3cda0db` (`Session 2: close visual rig acceptance`). That dedicated commit contains only the validated `IK_DG_Master` and `CR_DG_Master` changes plus their closeout documentation and four Session 2 authoring/validation scripts.

The unrelated pre-existing untracked environment, imported-content, data, source-art, and editor-configuration roots were not staged, discarded, overwritten, or incorporated. Generated `Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` output remains uncommitted. The installed UE 5.8-compatible plugin remained the source of truth and has zero Session 3 source changes; no build-kit files replaced it. The behavior-neutral `FoliageBasicShapeHalfExtentCm` unity-build rename remains intact.

### Existing throw-path audit

The following existing project path remains authoritative:

`ADiscGolferPawn::InputThrow` -> `UThrowControllerComponent::HandleThrowPress` -> `ADiscGolfTourGameMode::RequestThrow` / Session 3 grip adapter -> `ADiscGolfTourGameMode::LaunchThrow` -> `DiscGolfCourseRules::ApplyLieEffects` -> `DiscGolfMath::ResolveThrowRelease` -> `ADiscActor::Throw` -> `UDiscFlightComponent::Launch`.

| Responsibility | Existing authority retained |
|---|---|
| Throw start | `ADiscGolferPawn::InputThrow`; the first press starts the existing timing state and the second produces `FThrowCommand` |
| Throw state | `UThrowControllerComponent::bTimingActive` before command creation; existing GameMode lifecycle gates for active disc/replay/flyover/hole/UI/lie-transition state; one project-owned Session 3 animation transaction only after a legal RHBH drive command exists |
| Disc choice | `UDiscBagComponent::GetSelectedMoldId` and `GetSelectedPlastic`; `UDiscCatalogSubsystem::ResolveDisc` resolves the definition at launch |
| Aim and style | Pawn world-forward direction plus the existing `UThrowControllerComponent` throw style and shot context |
| Power and angles | Existing ThrowController `Power01`, `HyzerDeg`, `NoseDeg`, and `LaunchAngleDeg` values |
| Release timing | Existing moving timing needle and `DiscGolfMath::NormalizeTimingError`; the animation does not recalculate timing |
| Lie effects | Existing `DiscGolfCourseRules::ApplyLieEffects`; GameMode remains authoritative for the current shot context and lie multipliers |
| Release speed, spin, aim error, and final release attitude | Existing `DiscGolfMath::ResolveThrowRelease` only |
| Gameplay disc | `ADiscGolfTourGameMode::LaunchThrow` spawns one `ADiscActor`, initializes it from the catalog and `AWindDirector`, then calls `ADiscActor::Throw(LastRelease)` |
| Flight, wind, collision, and ground play | Existing `UDiscFlightComponent` fixed-step solver and the existing wind/collision/ground systems |
| Settle, lie, score, and next action | Existing `HandleDiscSettled` / `HandleDiscHoledOut`, lie resolution, scoring, player relocation, and course flow |
| Camera | Existing `StartBroadcastCameraForShot`, broadcast tracking, settle transition, and `ReturnCameraToPlayer`; no character-framework camera authority was enabled |
| Input contexts | Existing Enhanced Input mapping remains authoritative. Aim/throw adjustment actions are temporarily gated by the active animation transaction rather than replaced with a parallel input context |

Before Session 3 there was no runtime held-disc attachment and no character-animation-to-gameplay release bridge. Direct/programmatic `RequestThrow` remains synchronous for regressions and non-Session-3 paths. Only a local `Backhand + Drive` command attempts the new montage; non-RHBH, putting, or unavailable-presentation cases retain the prior immediate path.

### Temporary RHBH animation and montage

| Asset | Verified contract |
|---|---|
| `/Game/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype` | 2.8 seconds at 60 FPS; 168 frames / 169 keys per track; 20 animated bone tracks; all 6 DG motion curves; 116.264-degree throwing-arm sweep; non-looping; in-place with only small validation shifts |
| `/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype` | One `A_DG_RHBH_Prototype` segment on `DefaultSlot`, one Default section, 2.8-second duration |
| `/Game/DiscGolf/Animation/ABP_DG_Player` | Parent remains `/Script/DiscGolfCharacterFramework.DiscGolfAnimInstance`; pose flow is `LocalRefPose -> DefaultSlot -> Root`; targets `SKEL_DG_Master` |

The non-production sequence contains setup, a compact x-step/run-up, reachback, plant/brace, acceleration, release, follow-through, and recovery. Visual QA exposed that the first draft stacked component-space pose intent as independent local rotations. The sequence authoring utility was corrected to convert component intent to local bone keys; the accepted sequence keeps parent-child length ratios at approximately 1.0, limits the sampled maximum component rotation to 88.043 degrees, preserves a 116.264-degree arm sweep and 66.756 cm grip sweep, and stays within a 173.965 cm sampled root-relative extent. Final SHA-256 values are `EA53E0460B958FFA7C8BC1DCACB5A4E6F1C6ABBACE9F177C68A1017C6783ECE6` for the sequence, `6BCD1C3256668D7F041FE6D33B6910052EE77FA4739A1EF4E60E689A787A8AF8` for the montage, and `87627AACC7F6E6696CE16395F2C1E66FACCEAAC5333856469B6CAE008F75EE4D` for the Animation Blueprint. Session 3 did not change the accepted `IK_DG_Master` or `CR_DG_Master` assets.

| Event | Frame | Time |
|---|---:|---:|
| Aim | 0 | 0.000000 s |
| RunUp | 12 | 0.200000 s |
| ReachBack | 48 | 0.800000 s |
| Plant | 70 | 1.166667 s |
| Acceleration | 84 | 1.400000 s |
| `DG Release Disc` | 96 | 1.600000 s |
| FollowThrough | 100 | 1.666667 s |
| Recovery | 132 | 2.200000 s |
| `DG Throw Finished` | 162 | 2.700000 s |

`DG Release Disc` uses `/Script/DiscGolfCharacterFramework.AnimNotify_DiscRelease` and exists exactly once. `DG Throw Finished` uses `/Script/DiscGolfCharacterFramework.AnimNotify_ThrowFinished` and exists exactly once. Phase entries use `/Script/DiscGolfCharacterFramework.AnimNotify_ThrowPhase`. The authoring utility and independent read-only validator both enforce the exact event count, timing, slot, skeleton, track, curve, and Animation Blueprint contracts.

### Provisional held disc

`ADiscGolferPawn` now creates `HeldDiscVisual`, using the existing provisional `/Engine/BasicShapes/Cylinder.Cylinder`, attached to `SkeletalGolferMesh` at `disc_grip_r`.

| Attachment field | Value |
|---|---|
| Local location | `(0, 0, 0)` cm |
| Local rotation | `(Pitch=0, Yaw=180, Roll=0)` degrees |
| Local scale | `(0.21, 0.21, 0.015)` |
| Collision | Disabled |
| Overlap events | Disabled |

The component becomes visible only for an active pre-release animation transaction. At release it is hidden before the synchronous gameplay-launch delegate executes, preventing a held/gameplay double-disc frame. The authoritative gameplay-disc mesh, physics orientation, and collision were not altered to fit the proxy hand. The Cylinder and block hand remain provisional validation geometry and do not establish final rim/palm ergonomics.

### Single-authority release adapter

The project-owned adapter is `UDiscGolfRHBHThrowAdapterComponent`. Its release path is:

`HandleFrameworkDiscRelease` -> `ADiscGolferPawn::HandleAnimatedRHBHRelease` -> `ADiscGolfTourGameMode::RequestThrowFromGrip` -> existing `LaunchThrow` -> existing `ResolveThrowRelease` -> existing gameplay disc and flight component.

The adapter snapshots the exact authoritative `FThrowCommand` produced by the ThrowController. From plugin `FDGReleaseData` it consumes only `GripWorldTransform`; framework-suggested speed, spin, grip velocity, and replacement release intent are ignored. `RequestThrowFromGrip` is a narrow C++-only seam: GameMode rejects regression/lie-transition activity, calls not originating from the actual player pawn's active and release-committed adapter transaction, cached-command mismatches, non-finite transforms, and grip positions more than 300 cm from the player. It then uses the accepted grip **location only** as the gameplay-disc spawn override. Direction, power, timing, hyzer, nose, launch angle, disc definition, lie effects, release speed, spin, wind, and every flight calculation continue through the existing project authorities listed above.

`FDiscGolfRHBHThrowTransaction` commits its state to Released before invoking external launch code. Each attempt has a monotonically increasing serial, exactly one permitted release commit, and stale-attempt rejection. A duplicate, blended, re-entrant, or replayed release notify cannot launch a second disc. The held visual hides before launch; a rejected authoritative launch is recorded and never retried.

Cancellation before release transitions directly to recovery, hides the held visual, cancels the framework presentation, and launches zero discs. A montage-start failure uses the same safe cancellation. An interruption after release recovers presentation state but cannot replace or relaunch the committed gameplay disc. `DG Throw Finished` performs normal recovery once; montage-end interruption provides a fallback; a token-aware 5.0-second watchdog prevents an indefinitely stuck pre- or post-release transaction. The pawn stops any stale montage during recovery so an old notify cannot affect a later attempt.

During the active animation transaction, aim adjustment, power, hyzer, nose, style, disc selection, and a second throw are rejected. At release, the existing GameMode camera transition begins. At finish/recovery, the animation lock clears; the existing flight continues independently; at settle the existing lie transition moves the player, stops the broadcast camera, blends back to the pawn, reapplies the shot context, and makes the next legal action available. Reset cancels an outstanding animated transaction before running the existing hole reset. No duplicate pawn, gameplay disc, replay actor, or input authority is created.

### Body-profile compatibility boundary

ShortCompact, Baseline, and TallLongArms all reference the same accepted `SK_DG_Master`, `SKEL_DG_Master`, `IK_DG_Master`, `CR_DG_Master`, `ABP_DG_Player`, and RHBH montage foundation. Their data assets and matching rig inputs remain valid. Runtime body-proportion deformation and profile-to-rig plumbing are deliberately still absent, so all three fixtures use the same proxy geometry in Session 3; this is a playback/event compatibility check, not Session 4 deformation acceptance.

The rendered compatibility harness played the same montage and event path on ShortCompact, Baseline, and TallLongArms. Each fixture started animation, retained a finite usable grip transform, fired one release callback, reached follow-through, and recovered through `ThrowFinished` without inversion, collapse, or a stuck transaction. The images intentionally identify profile deformation as Session 4 work; this pass proves shared-rig/playback compatibility only, not distinct body silhouettes.

### Files and assets changed

Modified:

- `Content/DiscGolf/Animation/ABP_DG_Player.uasset`
- `Source/DiscGolfTour/DiscGolfTour.Build.cs`
- `Source/DiscGolfTour/DiscGolfTourGameMode.cpp`
- `Source/DiscGolfTour/DiscGolfTourGameMode.h`
- `Source/DiscGolfTour/DiscGolferPawn.cpp`
- `Source/DiscGolfTour/DiscGolferPawn.h`
- `Docs/CODEX_CHARACTER_INTEGRATION_CHANGELOG.md`

Added:

- `Content/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.uasset`
- `Content/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.uasset`
- `Source/DiscGolfTour/DiscGolfRHBHThrowTransaction.h`
- `Source/DiscGolfTour/DiscGolfRHBHThrowAdapterComponent.h`
- `Source/DiscGolfTour/DiscGolfRHBHThrowAdapterComponent.cpp`
- `Source/DiscGolfTour/DiscGolfSession3SmokeRunner.h`
- `Source/DiscGolfTour/DiscGolfSession3SmokeRunner.cpp`
- `Source/DiscGolfTour/DiscGolfSession3VisualCaptureRunner.h`
- `Source/DiscGolfTour/DiscGolfSession3VisualCaptureRunner.cpp`
- `Source/DiscGolfTour/Tests/DiscGolfSession3ThrowTransactionTests.cpp`
- `Source/DiscGolfTourEditor/DiscGolfSession3AssetUtility.h`
- `Source/DiscGolfTourEditor/DiscGolfSession3AssetUtility.cpp`
- `Scripts/create_dg_character_session3_assets.py`
- `Scripts/diagnose_dg_character_session3_pose.py`
- `Scripts/validate_dg_character_session3_assets.py`
- `Scripts/validate_dg_character_session3_rig.py`
- `Scripts/validate_dg_character_session3_wiring.py`

The 24-file Session 3 source/content/documentation scope is the complete intended change set. Rendered screenshots, manifests, logs, binaries, intermediates, and other generated evidence remain under `Saved`/generated roots and are not project-source changes.

### Automated and regression verification

| Gate | Result | Evidence |
|---|---|---|
| Project static validator | PASS | `Scripts/validate_project.py` |
| Reference flight check | PASS | Existing release, flight, mirror, and ground envelopes unchanged |
| Trajectory artifact validation | PASS | Existing deterministic trajectory artifacts remain valid |
| UE Editor build | PASS | Final compiled source build: 4 actions / 13.68 s; final up-to-date verification: 0 actions / 2.69 s; `Result: Succeeded` |
| UE runtime build | PASS | `DiscGolfTour Win64 Development`; 9 actions / 148.76 s; linked `Binaries/Win64/DiscGolfTour.exe` |
| Framework reflection | PASS | `Saved/Logs/CharacterFramework_Session3_Final_Reflection.log`; 4 classes + 4 structs; exit 0; no fatal/assert/failure markers |
| Strict Session 3 asset validation/no-write gate | PASS | `Saved/Logs/CharacterFramework_Session3_Final_Assets.log` and `Saved/CharacterFramework/Session3AssetValidation.json`; exact release/finish counts and `disk_mutation=NONE` |
| Idempotent authoring rerun | PASS | `Saved/Logs/CharacterFramework_Session3_Final_NoWrite.log`; `PASS_ALREADY_CURRENT_NO_ASSET_WRITES`; all 10 package SHA-256 values unchanged |
| Session 3 wiring scan | PASS | `Saved/CharacterFramework/Session3WiringValidation.json`; 20/20 checks; exact 10-asset set; consumed release field `GripWorldTransform`; plugin changes 0 |
| Strict rig/authority validation | PASS | `Saved/Logs/CharacterFramework_Session3_Final_Rig.log`; 69 bones, 4 goals, 4 effectors, 3 profiles, release 1, finish 1, `SINGLE_EXISTING_FLIGHT_PATH` |
| Focused transaction automation | PASS | `Saved/Logs/Automation_CharacterFramework_Session3_Focused_Final.log`; 6/6 tests succeeded; process exit 0 |
| Full existing automation | PASS | `Saved/Logs/Automation_CharacterFramework_Session3_Full_Final.log`; 108/108 `DiscGolfTour.` tests succeeded; `TEST COMPLETE. EXIT CODE: 0` |
| Existing three-hole gameplay smoke | PASS | `Saved/Logs/ThreeHoleRoundSmoke_CharacterFramework_Session3_Final.log`; 3/3 holes, 3 strokes on par 11 (-8); manifest transitions, scoring, scorecard, and save snapshot active |
| Live end-to-end one-throw smoke | PASS | `Saved/Logs/Session3OneThrowSmoke_Final.log`; exact FollowThrough/Recovery phase observations, one `DG Throw Finished`, one original pawn, camera return, next-action begin/cancel; evaluated pre-release cancellation launched 0 discs through the former release time |
| Rendered visual evidence | PASS (validation proxy) | `Saved/Logs/CharacterFramework_Session3_VisualCapture_ComponentAnchored.log`; 8/8 1920x1080 images; one release, one stroke, three profile fixtures, actual held/gameplay mesh component anchors |

The final live one-throw flight produced 1,745 trajectory samples, 67.20 m airborne travel, 75.96 m final carry, a 2.17 m apex, four ground impacts, and a resolved fairway lie. The test observed animation before release, exactly one release commit and one gameplay disc, the authoritative `LastRelease` reaching the existing flight component with valid non-zero velocity, exactly one FollowThrough phase, exactly one Recovery phase, exactly one `DG Throw Finished`, settled-flight/lie completion, return of the PlayerController view target to the original pawn, and availability of the next legal action.

The six new deterministic transaction tests cover cached-command pass-through, cancel-before-release, duplicate/re-entrant release rejection, normal and watchdog recovery, one-authority release, and stale-attempt/post-release interruption rejection. Existing physics tests were not loosened.

### Visual evidence

The final capture manifest is `Saved/CharacterFramework/Screenshots/Session3_FirstThrow/Session3_FirstThrow_CaptureManifest.json`. It records 8/8 1920x1080 captures, one live release callback, one stroke, `ThrowFinished` recovery, no held visual after release, and successful playback/recovery checks for all three profile fixtures. The first three frames are anchored to the actual rendered components (`HeldDiscVisual`, then the authoritative `DiscMesh`) with zero focus-alignment error; the release log records zero mesh-alignment error.

- `01_HeldDisc_BeforeRelease.png`
- `02_Exact_DGReleaseDisc_Frame.png`
- `03_ImmediatePostRelease_OneGameplayDisc.png`
- `04_FollowThrough.png`
- `05_RecoveredGameplayState.png`
- `06_ShortCompact_Playback.png`
- `07_Baseline_Playback.png`
- `08_TallLongArms_Playback.png`

Manual review passed these images only as validation-proxy/provisional-Cylinder evidence. The white held Cylinder is nearly edge-on in the first frame, and the release frame overlaps the proxy hand; the paired manifest and callback log provide the exact state proof. These images are not production character-art, grip-ergonomics, environment-lighting, or UI-presentation acceptance.

### Remaining production limitations

- `A_DG_RHBH_Prototype` is mechanically coherent validation animation, not final mocap or production polish.
- `SK_DG_Master` is a rigid block proxy; the Cylinder has no authored rim or dome and cannot prove production grip ergonomics.
- Runtime ShortCompact/Baseline/TallLongArms deformation and profile plumbing remain Session 4 work.
- Only one RHBH drive is animated. Forehand and putting deliberately retain existing behavior and have no Session 3 animation coverage.
- No plugin camera, replay, course, physics, scoring, save, equipment, UI, or environment system was activated.
- No Session 4 work has begun.

---

## Session 4 -- one-skeleton character creator and profiled RHBH presentation

Date: 2026-08-15

Status: **SESSION 4 ACCEPTANCE: PASS.**

The final line of the Session 3 record above describes the state at Session 3 closeout. Session 4 now adds a bounded body-profile and throw-style presentation layer on the accepted character foundation. It does not replace the player, throw command, release transaction, flight solver, wind, collision, lie, scoring, course, camera, or replay authorities established before this session.

### Source-of-truth and authority boundaries

| Responsibility | Session 4 owner / boundary |
|---|---|
| Creator ranges and plugin-facing types | Installed UE 5.8-compatible `DiscGolfCharacterFramework` plugin and `Plugins/DiscGolfCharacterFramework/Config/DG_CharacterCreatorSchema.json` |
| Persistent player data | Project-owned primitive-only `FDiscGolfCharacterProfileSaveData` in `Source/DiscGolfTour/DiscGolfCharacterProfileRuntime.h` |
| Save authority | Existing `UDiscGolfTourGameInstance` / `UDiscGolfSaveGame` path; schema version 7 |
| Live preview | The already possessed `ADiscGolferPawn` and a transient duplicate of the default character profile |
| Pose deformation | Project-owned mutable Control Rig unit `FRigUnit_DGApplyCharacterProfile` |
| Full-body solve | The previously accepted `DGFullBodyIK` PBIK node and its unchanged settings |
| Throw playback | Existing `AM_DG_RHBH_Prototype` montage with its existing fixed timing, curves, release notify, and finish notify |
| Release and flight | Existing Session 3 release adapter and authoritative gameplay-disc launch path |
| Throw-style power/spin | Explicitly fixed to `1.0` / `1.0`; creator style is visual-only |
| Left-handed animation | Not authored. LHBH preview/save is supported, but animated RHBH is rejected before montage and the existing non-animated gameplay fallback remains authoritative |

No new skeleton, alternate pawn, duplicate gameplay disc, or parallel throw/flight calculation was introduced.

### Character profile schema and persistence

`FDiscGolfCharacterProfileSaveData` stores only project-owned primitive fields, so a player save does not serialize marketplace/plugin UObject references:

| Group | Persisted fields | Accepted range |
|---|---|---|
| Handedness | `bLeftHanded` | Right / Left |
| Body | `HeightCm` | 150-210 cm; default 183 |
| Body | `WingspanScale`, `ShoulderWidthScale` | 0.92-1.08; default 1.0 |
| Body | `TorsoLengthScale`, `LegLengthScale`, `HandScale` | 0.94-1.06; default 1.0 |
| Body | `MassKg` | 45-160 kg; default 82 |
| Throw presentation | `RunUpIntensity`, `ReachBackAmount`, `TorsoRotation`, `BraceIntensity`, `Explosiveness`, `FollowThrough` | 0.0-1.0 with schema defaults |

`Muscularity`, `BodyFat`, `Chest`, `Waist`, `Hips`, `Arms`, and `Legs` are persisted as a future body-build foundation but are intentionally not presented as production visual deformation in Session 4.

Save schema 7 sanitizes all character values against the installed creator schema. Schema 6 and earlier migrate to a sanitized default profile; unsupported future schemas are rejected. `UDiscGolfTourGameInstance::UpdateCharacterProfile` restores the previous profile if the existing save operation fails, so an unsuccessful Apply cannot replace the last valid saved character.

`DiscGolfTour.Character.Session4.Profile.DiskSlotRoundTrip` supplies the focused next-launch persistence proof. It writes a sanitized schema-7 profile to a GUID-named automation-only UE save slot, reloads it through `LoadGameFromSlot`, verifies all 21 primitive character fields (including handedness) plus schema version, then deletes the slot and verifies that it no longer exists. The project's normal startup continues to load its existing `DiscGolfTour_Profile_0` slot through the GameInstance, and pawn initialization reconstructs the single transient runtime profile from that loaded data.

At pawn initialization, `DA_DG_DefaultCharacter` is duplicated into transient `RuntimeCharacterProfile`. Saved or previewed values modify only that transient instance. The source data asset remains immutable.

### One-skeleton runtime rig architecture

The visible profile path uses the accepted assets:

- `/Game/DiscGolf/Characters/Meshes/SK_DG_Master`
- `/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master`
- `/Game/DiscGolf/Rigs/IK_DG_Master`
- `/Game/DiscGolf/Rigs/CR_DG_Master`
- `/Game/DiscGolf/Animation/ABP_DG_Player`
- `/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype`

`FRigUnit_DGApplyCharacterProfile` is implemented in:

- `Source/DiscGolfTour/DiscGolfCharacterRigUnits.h`
- `Source/DiscGolfTour/DiscGolfCharacterRigUnits.cpp`

Its five public inputs are `BodyProfile`, `ThrowStyle`, `Handedness`, `ThrowPhase`, and `bThrowActive`. It executes before the accepted PBIK:

`BeginExecution -> DGApplyCharacterProfile -> DGFullBodyIK`

The unit snapshots the source pose and derives bounded local/global transforms from that source rather than multiplying the previous evaluated pose. This prevents cumulative scale or transform drift. Baseline is a near no-op. Height, torso, leg, shoulder, arm, and hand changes are bounded to the creator schema; actor/component scale is not used as a body-proportion shortcut. The result is translated to an average ball/toe ground landmark before PBIK so the feet remain grounded across the supported range.

Existing `DG_FootPlant_L` / `DG_FootPlant_R` curves drive foot locks. The grip correction follows `disc_grip_r` with limited reachback influence. Both hand-IK alpha controls are explicitly set to zero before the active-throw branch, preventing an interrupted or cancelled reachback from leaving stale hand IK in idle. Throw-style inputs add restrained presentation-only offsets and rotations while preserving the montage, `DG Release Disc`, and `DG Throw Finished` timing.

`ABP_DG_Player` now evaluates:

`LocalRefPose -> DefaultSlot -> ControlRig -> Root`

The Control Rig node transfers the input pose and directly maps all five animation-instance properties to the five Control Rig public variables. The accepted PBIK contract remains pelvis Free, 20 iterations, 10 subiterations, global pull 0, stretch disabled, four effectors, and four bend settings.

The provisional held Cylinder still attaches to `disc_grip_r`. Its component uses `SetAbsolute(false, false, true)` with relative scale `(0.21, 0.21, 0.015)`. It therefore follows the profiled socket position and rotation without inheriting Control Rig socket scale. This removed the oversized tiled-plane artifact seen in early Session 4 captures without changing the authoritative gameplay disc mesh, physics scale, or launch orientation.

### Creator UI and gameplay flow

`UDiscGolfCharacterCreatorWidget` supplies the functional native Body/Throw Style UI. It exposes:

- live Body and Throw Style sliders;
- Right / Left handed selection;
- Baseline, ShortCompact, and TallLongArms presets;
- Reset, Apply, Cancel, Rotate Left, and Rotate Right;
- an explicit warning that animated LHBH is not authored.

`/Game/DiscGolf/UI/WBP_DG_CharacterCreator` is deliberately an empty WidgetBlueprint foundation parented to `/Script/DiscGolfTour.DiscGolfCharacterCreatorWidget`. It inherits the native UI and does not fabricate Face, Hair, Clothing, or Outfit tabs. If that WBP cannot be loaded, the PlayerController can instantiate the native class.

The existing PlayerController owns open/preview/apply/cancel and input focus. Opening is gated by `ADiscGolfTourGameMode::CanOpenCharacterCreator` and `ADiscGolferPawn::IsCharacterProfileChangeSafe` so it cannot begin during a live disc, replay, flyover, regression, lie transition, hole intro, or active animated throw. Normal HUD rendering is suppressed while the creator is open. Preview is transient on the possessed pawn; Apply uses the GameInstance save authority; Cancel restores the profile that was active when the creator opened. The implementation is event-driven and does not add a permanent character-creator Tick.

### Handedness and gameplay-authority acceptance

- Right-handed profiles continue through the single Session 3 RHBH montage and release adapter.
- A Left profile can be previewed and saved.
- `ADiscGolferPawn::TryStartAnimatedRHBHThrow` rejects a Left profile before montage playback or a release callback.
- Left-handed gameplay retains the pre-existing non-animated fallback; no mirrored montage or LHBH animation quality is claimed.
- `FDiscGolfCharacterProfileSaveData::ToThrowStyle` forces `PowerMultiplier = 1.0` and `SpinMultiplier = 1.0`.
- Body/style presentation never recalculates power, spin, hyzer, nose angle, aim, timing, or wind.
- The existing Session 3 transaction remains the only animated release authority, and the existing gameplay disc/flight component remains the only physics authority.
- No plugin camera, replay, course, scoring, save, equipment, environment, or physics subsystem was activated.

### Deterministic asset authoring and validation

`UDiscGolfSession4AssetUtility` exposes `AuthorSession4Assets()` and `ValidateSession4Assets()`. The Python wrappers are:

- `Scripts/create_dg_character_session4_assets.py`
- `Scripts/validate_dg_character_session4_assets.py`
- `Scripts/validate_dg_character_session4_wiring.py`
- `Scripts/run-session4-profile-smokes.py`

The utility owns only the exact Session 4 CR variables/unit, ABP Control Rig node/mappings, and empty creator WBP. It validates the accepted PBIK, skeleton, ABP node set, variable GUID/type/default contract, and existing WBP ownership before writing. Unexpected immutable content is refused rather than rewritten. UE 5.8 legacy Control Rig member-variable creation is followed by one synchronous full Blueprint compile before getter nodes are authored; recovery validation reads the generated CDO public property-bag variables. A current asset rerun reports `PASS_ALREADY_CURRENT_NO_ASSET_WRITES`.

`Scripts/validate_dg_character_session2.py` now accepts either the original direct `BeginExecution -> DGFullBodyIK` chain or the exact Session 4 two-hop chain above. All original PBIK type, root, settings, effector, bend, and hierarchy checks remain required. Its historical Session 2 authority gate still intentionally requires zero gameplay wiring, so the integrated Session 3/4 final strict gate is `Scripts/validate_dg_character_session3_rig.py`, which adds the exact release/finish and single-flight-authority checks.

### Session 4 source/content scope

Modified:

- `Content/DiscGolf/Animation/ABP_DG_Player.uasset`
- `Content/DiscGolf/Rigs/CR_DG_Master.uasset`
- `Scripts/validate_dg_character_session2.py`
- `Source/DiscGolfTour/DiscGolfHUD.cpp`
- `Source/DiscGolfTour/DiscGolfSaveGame.h`
- `Source/DiscGolfTour/DiscGolfSession3VisualCaptureRunner.cpp`
- `Source/DiscGolfTour/DiscGolfTour.Build.cs`
- `Source/DiscGolfTour/DiscGolfTourGameInstance.cpp`
- `Source/DiscGolfTour/DiscGolfTourGameInstance.h`
- `Source/DiscGolfTour/DiscGolfTourGameMode.cpp`
- `Source/DiscGolfTour/DiscGolfTourGameMode.h`
- `Source/DiscGolfTour/DiscGolfTourPlayerController.cpp`
- `Source/DiscGolfTour/DiscGolfTourPlayerController.h`
- `Source/DiscGolfTour/DiscGolferPawn.cpp`
- `Source/DiscGolfTour/DiscGolferPawn.h`
- `Source/DiscGolfTour/Tests/DiscGolfSaveSchemaTests.cpp`
- `Source/DiscGolfTourEditor/DiscGolfSession3AssetUtility.cpp`
- `Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs`
- `Docs/CODEX_CHARACTER_INTEGRATION_CHANGELOG.md`

Added:

- `Content/DiscGolf/UI/WBP_DG_CharacterCreator.uasset`
- `Scripts/create_dg_character_session4_assets.py`
- `Scripts/run-session4-profile-smokes.py`
- `Scripts/validate_dg_character_session4_assets.py`
- `Scripts/validate_dg_character_session4_wiring.py`
- `Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.cpp`
- `Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.h`
- `Source/DiscGolfTour/DiscGolfCharacterProfileRuntime.cpp`
- `Source/DiscGolfTour/DiscGolfCharacterProfileRuntime.h`
- `Source/DiscGolfTour/DiscGolfCharacterRigUnits.cpp`
- `Source/DiscGolfTour/DiscGolfCharacterRigUnits.h`
- `Source/DiscGolfTour/DiscGolfSession4VisualCaptureRunner.cpp`
- `Source/DiscGolfTour/DiscGolfSession4VisualCaptureRunner.h`
- `Source/DiscGolfTour/Tests/DiscGolfCharacterProfileRuntimeTests.cpp`
- `Source/DiscGolfTourEditor/DiscGolfSession4AssetUtility.cpp`
- `Source/DiscGolfTourEditor/DiscGolfSession4AssetUtility.h`

`SK_DG_Master`, `SKEL_DG_Master`, `IK_DG_Master`, `A_DG_RHBH_Prototype`, `AM_DG_RHBH_Prototype`, and the three existing authored profile data assets were reused without Session 4 replacement.

### Final verification evidence

| Gate | Recorded result | Evidence / qualification |
|---|---|---|
| Session 4 asset authoring recovery | PASS | `Saved/Logs/CharacterFramework_Session4_AssetAuthoring_Recovery2.log` |
| Final Editor target build | PASS | `DiscGolfTourEditor Win64 Development`; 4 actions; `Result: Succeeded`; 29.72 s |
| Final runtime target build | PASS | `DiscGolfTour Win64 Development`; 11 actions; `Result: Succeeded`; 155.22 s |
| Final source/static validators | PASS | `validate_project.py`, `reference_flight_check.py`, `validate_trajectory_artifacts.py`, Python compile, schema SHA-256, and `git diff --check`; Session 4 wiring 90/90 in `Saved/CharacterFramework/Session4WiringValidation.json` |
| Final framework reflection | PASS | `Saved/Logs/CharacterFramework_Session4_Final_Reflection.log`; 4 classes + 4 structs; 0 errors/warnings |
| Final strict rig/authority | PASS | `Saved/Logs/CharacterFramework_Session4_Final_StrictRig.log`; 69 bones, 4 goals, 4 effectors, 3 profiles, one release, one finish, single existing flight path; 0 errors/warnings |
| Final strict asset validation | PASS | `Saved/Logs/CharacterFramework_Session4_Final_Assets.log`; exact `Begin_Profile_PBIK`, `RefPose_DefaultSlot_ControlRig_Root`, project-owned WBP, and `disk_mutation=NONE` |
| Final idempotent author/no-write rerun | PASS | `Saved/Logs/CharacterFramework_Session4_Final_NoWrite.log`; `PASS_ALREADY_CURRENT_NO_ASSET_WRITES`; 0 errors/warnings |
| Full `DiscGolfTour.` automation | PASS | `Saved/Logs/Automation_CharacterFramework_Session4_Final.log`; 115/115 tests succeeded, zero non-success results, `TEST COMPLETE. EXIT CODE: 0`; includes the real disk-slot round-trip proof |
| Existing three-hole smoke | PASS | `Saved/Logs/ThreeHoleRoundSmoke_CharacterFramework_Session4_Final.log`; 3/3 holes, 3 strokes, par 11, -8; existing transitions, scoring, scorecard, and save snapshot active |
| Sequential profile one-throw smokes | PASS | `Saved/CharacterFramework/Session4ProfileSmokeReport.json`; 5/5: ShortCompact, Baseline, TallLongArms, SliderMin, and SliderMax; each process exit 0 with no timeout |
| Retry7 rendered visual acceptance | PASS | `Saved/Logs/CharacterFramework_Session4_VisualCapture_Retry7.log` and the capture manifest listed below |

Each final profile smoke proved aim/throw availability, active animation, one release, one authoritative gameplay disc with valid non-zero motion, the existing flight solver, FollowThrough, Recovery, `DG Throw Finished`, camera return, next-action availability, and cancel-before-release producing zero discs.

### Retry7 rendered visual evidence

Retry7 used UE 5.8.1 build CL 56057345 and produced the required 8/8 images at 1920x1080:

- `Saved/CharacterFramework/Screenshots/Session4_CharacterCreator/01_Neutral_Front_Short_Baseline_Tall.png`
- `Saved/CharacterFramework/Screenshots/Session4_CharacterCreator/02_Neutral_Side_Short_Baseline_Tall.png`
- `Saved/CharacterFramework/Screenshots/Session4_CharacterCreator/03_Maximum_Reachback_AllProfiles.png`
- `Saved/CharacterFramework/Screenshots/Session4_CharacterCreator/04_Plant_Brace_AllProfiles.png`
- `Saved/CharacterFramework/Screenshots/Session4_CharacterCreator/05_Release_AllProfiles.png`
- `Saved/CharacterFramework/Screenshots/Session4_CharacterCreator/06_FollowThrough_AllProfiles.png`
- `Saved/CharacterFramework/Screenshots/Session4_CharacterCreator/07_Live_CharacterCreator_ShortPreview.png`
- `Saved/CharacterFramework/Screenshots/Session4_CharacterCreator/08_Slider_Extremes_Min_Max.png`

The authoritative manifest is `Saved/CharacterFramework/Screenshots/Session4_CharacterCreator/Session4_CharacterCreator_CaptureManifest.json`. Every capture passed framing, camera-outside-subject, clear line-of-sight, finite-transform, and readability gates.

| Fixture | Head-to-ground landmark | Wingspan chain | Shoulder width | Avg. hand chain | Avg. foot-to-ball | Avg. hand-to-grip |
|---|---:|---:|---:|---:|---:|---:|
| ShortCompact | 123.60 cm | 148.80 cm | 30.25 cm | 15.13 cm | 14.26 cm | 6.32 cm |
| Baseline | 153.00 cm | 184.67 cm | 38.00 cm | 17.30 cm | 17.72 cm | 7.23 cm |
| TallLongArms | 179.12 cm | 219.38 cm | 44.70 cm | 19.23 cm | 20.84 cm | 8.03 cm |
| SliderMin | 118.61 cm | 141.45 cm | 28.66 cm | 14.72 cm | 13.65 cm | 6.15 cm |
| SliderMax | 185.04 cm | 226.53 cm | 47.10 cm | 19.64 cm | 21.55 cm | 8.21 cm |

These are proxy-skeleton landmarks, not claims about production anatomical stature.

Phase evidence:

- ReachBack target `0.9000 s`; captured `0.9034 s`; minimum `DG_ReachbackAlpha = 0.99921`.
- Plant target `1.1667 s`; captured `1.1733 s`; minimum brace/plant requirement `1.0` and contact gate passed.
- Release target `1.6000 s`; captured `1.6108 s`; minimum `DG_ReleaseApproachAlpha = 0.83725`; exactly three validation-only grip callbacks for the three profiles.
- FollowThrough target `1.8667 s`; captured `1.8788 s`; minimum `DG_FollowThroughAlpha = 0.99644`.

The live creator frame opened the inherited native/WBP UI on the possessed pawn, previewed ShortCompact plus a Left-handed draft, displayed the LHBH limitation, and confirmed Cancel restored the opening profile without saving. The handedness boundary check rejected animated RHBH before montage, generated zero release callbacks during that check, and left the primary profile asset unchanged.

The Retry7 gameplay-isolation guard recorded zero world discs and zero strokes both before and after capture. Its three release callbacks were explicitly bound validation callbacks and did not spawn gameplay discs. The persistent-write guard compared 616 initial package files and one existing save file; no `.uasset`, `.umap`, or save-game file changed, and no package/level save call occurred. Only the eight PNGs and the JSON manifest were allowed outputs.

### Remaining production-art and scope limitations

- `SK_DG_Master` remains a rigid, blocky validation proxy. Production topology, skinning, weight painting, twist distribution, joint volume, and deformation polish are not accepted.
- The provisional Cylinder proves attachment and scale isolation only. Final rim/palm fit, finger wrap, grip ergonomics, and production disc art remain deferred.
- Face, hair, clothing, outfits, and cosmetic authoring are absent by design. `WBP_DG_CharacterCreator` contains no fake future tabs.
- Persisted muscularity/body-fat/chest/waist/hips/arms/legs values are foundation data only and do not yet drive visible production meshes.
- Only the RHBH montage is animated. LHBH, forehand, and putting animation remain deferred; their existing gameplay behavior was not replaced.
- Throw-style differences are intentionally restrained and visual-only. They do not change release power, spin, flight, scoring, or competitive outcome.
- Retry7 accepts profile readability and rig behavior on the proxy, not final character-art silhouette quality.
- Final Fab forest visual acceptance remains pending imported marketplace assets and is outside Session 4.
- **SESSION 4 ACCEPTANCE: PASS.** One master skeleton supports all accepted profiles and slider extremes; creator preview/persistence, restrained presentation variation, profile-safe held-disc/release alignment, and the Session 3 single-authority throw invariant are all demonstrated. Session 5 and later character-art work have not begun.
