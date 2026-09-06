# DGTour Game Bible

_Master Vision, Current Build, and Full Project Roadmap_

> Canonical product/design direction adopted 2026-08-23 from the user-provided master bible. This document contains enduring vision plus the historical baseline it was written against. Volatile acceptance truth is maintained in [CODEX_CHARACTER_INTEGRATION_CHANGELOG.md](C:/DGTour/Docs/CODEX_CHARACTER_INTEGRATION_CHANGELOG.md) and [CODEX_LONG_RUN_STATE.md](C:/DGTour/Docs/CODEX_LONG_RUN_STATE.md); those evidence records take precedence over historical “current status” passages below.

### Live evidence status at adoption

Session 8B has advanced materially beyond the historical pending baseline in this document: Core Data, plugin enablement, a project-owned original MetaHuman, Optimized Medium assembly, DGMaster visual retargeting, creator/backend persistence, cook closure, packaged switching, authoritative release preservation, and nine-frame visual collection have all been implemented and evidenced. The garment/pelvis failure was corrected by disabling the inappropriate retarget Root Motion operation while preserving existing gameplay authority. The accepted project-owned procedural Control Rig presentation removes the literal idle/reference T-pose and dual-horizontal-arm throw silhouettes without changing gameplay, release, disc, flight, montage, notify, asset, or save authority. Final packaged v3.3 run `5508308e-88e2-4b04-b1e1-2a3944674bc6` passed all four phases and all nine capture gates; aggregate SHA-256 is `A72BFE7D37FE1E1BF3A82533B7145BE463C95D69EFD644F98B487487CAEBFB9F`. Creator/gameplay bounded p95 values are 7.849802/14.675301 ms, raw p95 values are 7.233199/14.057398 ms, and both segments have zero hitches under the unchanged 22 ms contract. Focused automation passed 6/6 and full automation passed 140/140 on the final source. Manual inspection confirms relaxed idle arms, a tucked non-throwing arm, visible grip/release contact, an across-body follow-through with the disc airborne, compact outfit geometry, and no new hair, framing, or deformation defect. This is original procedural presentation, not captured or licensed production motion. The specific T-pose/horizontal-arm blocker is resolved and Session 8B is machine-accepted; full-body footwork, weight transfer, cadence, and final animation-art approval remain an explicit art/user judgment. At that checkpoint, Session 9 had not begun.

### Live evidence update — Session 9

Session 9 is technically complete as a fail-closed brand, rights, and provenance audit. `Config/DG_BrandLicenseContract.json` is now the project-level source of truth, `Scripts/validate_dg_session9_brand_license.py` validates it and its local evidence, and `Docs/DG_BRAND_ASSET_AUDIT.md` records the findings. The development contract passes as `PASS_CONTRACT_RELEASE_BLOCKED`; public distribution remains intentionally blocked by eight named clearance/provenance issues, including unresolved framework distribution rights, development-only cooked content, incomplete derived-asset and staged-package provenance, pending public-name clearance, and manual logo/trade-dress review. This is a technical audit result, not legal advice or a shipping approval.

### Live evidence update — Session 10

Session 10 is technically complete for its bounded quality/environment-adapter scope. A pure quality resolver now maps the player presets into course and environment tiers; Medium deliberately maps to the environment's Performance tier because that preset has no serialized Medium value, while the frozen Omen capture profile resolves to High/High. The built-in provider supplies visual-only Clear and Overcast lighting, with unsupported precipitation falling back to Overcast without affecting wind, disc flight, collision, traction, lie, or scoring. Automatic environment-to-flight wind mutation is removed and defaults false. Production readiness now requires all 16 environment asset categories plus the PCG graph, and the editor binder rejects quarantined roots and unapproved provenance before mutation.

Fresh Editor and Game Development builds passed, focused environment automation passed 20/20, and full DiscGolfTour automation passed 152/152. Fresh Development archive `5ea6982a-a89f-4948-a49c-212c4a889207` completed cook, stage, Pak, IoStore, and archive with exactly four `/Game/Environment/Forest` packages: `DA_TemperateMountainForest_Assets`, `DA_TemperateMountainForest`, `Materials/MPC_EnvironmentWind`, and `PCG/PCG_TemperateMountainForest`. All three quarantined vendor roots are absent. This closure does not claim production environment art, legal/public-release clearance, shipping approval, or visual acceptance: `assetsReady` remains false, PCG generation remains dormant, and public release remains blocked by the eight inherited Session 9 issues plus `SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY`.

### Live evidence update — Session 12

Session 12 is technically complete as a bounded project-owned camera, replay, input, UI/settings/accessibility, and semantic-audio presentation layer. Exclusive generation-stamped view ownership prevents stale or competing camera callbacks; replay and tracer use bounded actual native trajectory samples without resimulation; input routes fail closed; and the project Canvas/Enhanced Input fallbacks remain available without hard Gameplay Cameras or CommonUI authority. The no-tick audio router preserves the richer project semantic contract and supports an honest silent-asset fallback because no licensed production audio content is accepted. Focused automation passed 16/16, full automation passed 180/180, and both live Editor and fresh packaged evidence passed. This is not shipping UI/audio, platform input certification, content-rights clearance, or public-production acceptance; Session 12 adds `SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING` as the eleventh ordered release blocker.

### Live evidence update — Session 13

Session 13 is technically complete for its bounded course-authoring, validation, and PCG-mapping seam. Six visible editor-only, tick-free actors convert deterministically into a supplementary framework course definition, while project-owned JSON plus C++ fallback remains runtime course authority. Strict validation and export fail without mutating the target, and all ten gameplay-zone types map into a deterministic value-only decoration plan. PCG remains `GenerateOnDemand`, has no runtime generation consumer, owns no competitive collision, and has no profiled or visually approved generated content. Focused automation passed 19/19 plus 9/9 course-authoring tests, full automation passed 206/206, and fresh packaged Pine Ridge play evidence passed. Session 13 adds `SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING` as the twelfth ordered release blocker.

### Live evidence update — Session 14

Session 14 is technically complete as a bounded competition, career-save, measured-flight AI, and smoke-automation proof—not a finished career mode or production opponent system. The original generic `PineRidgeChampionship` fallback contains one three-hole StrokePlay round; scorecard conversion preserves project scoring authority and prevents penalty double-counting. Career results persist atomically in the isolated schema-v1 `DGT_Career_Dev_v1` domain rather than the protected schema-10 profile. One transient generic AI profile evaluates four completed real-flight previews, selects through the framework planner, and sends only the winner through the existing GameMode release and fixed-step flight path. Session 14 automation passed 17/17 and the full suite passed 223/223; fresh Editor and packaged proofs both selected one measured candidate, executed exactly one authoritative throw, and deleted their disposable career result. An exploratory run that omitted `-UserDir` was rejected, preserved for audit, and the protected profile was restored only from its independently verified byte-identical backup; all accepted closures used enforced external GUID save roots and finished with the protected 5,212-byte/A999 profile exact. Session 14 adds `SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING`, bringing the ordered public-release blocker count to thirteen.

### Live evidence update — Session 15

Session 15 is technically complete as a bounded Pine Ridge Hole 1 integration proof—not a polished vertical slice or production/public-release approval. Editor acceptance `cf218615-ed96-46b9-8f55-f0976780eecf` and packaged acceptance `6875a60a-5875-4de0-8988-9d8d0ffe2613` each completed Setup, Drive, Finish, and Verify at D3D12 1920x1080. Both proved creator/profile and outfit continuity, Apex/Tour drive identity, sampled nonzero wind, exactly one authoritative RHBH release, natural Circle 2 continuation, stable Touch/Base identity through two putts, exactly three strokes, actual-sample replay and Throw Lab, project scoring, normalized settings, and save/load continuity. Focused automation passed 8/8, the full suite passed 231/231, and final Editor/Game builds passed. Fresh package `18d49b6c-2efc-4d38-b5e0-6f5d400aabc9` cooked 985 packages with zero incremental skips and retained 54 manifest-bound files. Editor and packaged 600-sample performance records measured 9.578798 and 9.580903 ms p95 with zero hitches. Project Content, SaveGames, staged-package bytes, and the protected 5,212-byte/A999 profile remained unchanged. Session 15 adds `SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING`, bringing the exact ordered public-release blocker count to fourteen.

### Live evidence update — Session 16

Session 16 is technically complete for the inherited core-playability gate. Fresh Editor and packaged acceptances each passed all 44 blocking checks across Smoke, CoreLoop, Round, and Persistence, while focused/full automation passed 9/9 and 240/240. This proves that continued polish may proceed on the existing authoritative gameplay path. It does not approve production content, provenance, brand/legal clearance, shipping, or public release; the exact fourteen inherited blockers remain unresolved.

### Live evidence update — Session 17

Session 17 resolves the concrete visible-but-noninteractive round-flow defect. The front end, live scorecard, hole-complete state, and final results now use a code-owned focusable/clickable widget backed by exact authoritative snapshots and action whitelists. Settings returns to its originating screen, visible UI suppresses gameplay input, Canvas remains a fallback, and Continue/restart/menu commands reuse existing GameMode and round authority. Fresh Editor/Game builds, 9/9 focused and 249/249 full automation, rendered Editor gates, fresh 985-package archive `4b2b89de-408a-4874-915a-113393260190`, and both packaged interaction gates pass. This is technical interaction closure, not final UI/art, accessibility certification, or release approval; the fourteen-blocker ledger is unchanged.

### Live evidence update — Session 18

Session 18 closes exactly the Poly Haven derived-runtime receipt blocker. A controlled UE 5.8.1 reimport binds 35 verified CC0 source files to 55 derived Pine Ridge packages and identifies five project-original packages as the disjoint remainder of the exact 60-package root. The durable receipt SHA-256 is `13ACC125F5B442BE75DE405DBD76D43AA9381436624E12BDDBA3C7BA4F90D858`; independent semantic validation passes all 60 packages with report SHA-256 `1C3D6960E342B58143D55B42D701B18D19E17BFE8D118862BE38A93A15B74035`. Focused/full automation passes 3/3 and 252/252. Fresh archive `63624166-aa24-49d0-a89f-74a125a36be2` contains all 60 package identities in 54 files / 1,918,214,604 bytes; its sorted tab-line manifest SHA-256 is `0877C04FF8911F0903077F8C68093BF8A783FB6939538D2BFD1E2888D35B1870`, and its inner executable SHA-256 is `6051C15D6DCF14D4EE92B34215B79B7AA7BCADC5D8F6F5CA8AB24807E89D4346`. This is identity/provenance evidence for that bounded root only; actual staged-package provenance and exactly twelve other blockers remain, leaving thirteen current blockers and no release-ready claim.

Internal project name: DiscGolfTour / DGTour
Engine: Unreal Engine 5.8.1
Current project root: C:\DGTour
Historical accepted checkpoint at adoption: 48982c011cdf2008c945af52b17c20c58aebb8be
Current development stage: Character/gameplay foundation and Session 8B machine acceptance complete; Sessions 9–18 are technically complete within their bounded scopes; the one-hole integration proof, core-playability gate, interactive round flow, and Poly Haven derived-runtime receipt are accepted; public release remains blocked on thirteen explicit closures, with actual staged-package provenance and production acceptance next.
This bible separates three things clearly:
Built: Implemented and validated inside the real Unreal project.
Prepared: Designed and included in the build kit, but not yet integrated into the live project.
Planned: Agreed direction that still needs implementation and tuning.
## 1. The Game We Are Building
DGTour is intended to be a realistic, presentation-heavy disc-golf sports game with the accessibility and polish of a major golf title.
The core fantasy is:
Create your own golfer, assemble a bag, walk onto a beautiful course, evaluate the lie and wind, choose a disc and throwing stance, execute the shot, and watch a physically believable disc fly through an accurately designed environment.

It should combine:
Realistic disc flight
A satisfying skill-based throw mechanic
Authentic course design
Deep player customization
Equipment and bag strategy
Tournament and career progression
Broadcast-style cameras, overlays, tracers, and replays
Highly distinct course biomes
Fair but meaningful penalties for bad lies, difficult footing, weather, and obstacles
The project should feel like a disc-golf sports simulation, not merely a disc-throwing sandbox.
## 2. Current Brand and Title Policy
DGTour and DiscGolfTour are internal working names.
The final public-facing title has not been locked.
Premium Disc Golf was explored as a possible partner or internal brand, but you later asked to remove it from the active presentation for now. Therefore:
Current menus and prototype content should remain generic. DGTour remains an internal working name pending public-title clearance.
No real disc manufacturer, apparel company, course brand, or product name should appear without explicit approval.
Premium Disc Golf remains an optional dormant integration path.
Premium branding should only be restored when you explicitly approve it and provide authorized artwork.
No logo should be scraped or reconstructed from online images.
Generic fictional development discs and apparel remain the default safe option.
The game has a block-by-default brand policy:
Explicitly approved brand
→ permitted under documented scope

Unknown real brand
→ blocked

Generic fictional content
→ permitted
## 3. Core Design Pillars
Authentic Disc Flight
The disc must respond believably to:
Release velocity
Spin
Hyzer and anhyzer
Nose angle
Launch elevation
Wind
Disc shape
Disc mass
Plastic
Wear
Ground interaction
Trees and obstacles
The existing project flight system remains authoritative. Character animation contributes the release transform but does not secretly rewrite the flight after launch.
Skill-Based Throwing
The player should control:
Aim
Disc selection
Shot type
Stance
Run-up
Power
Release timing
Hyzer or anhyzer
Nose angle
Launch height
The game should reward controlled execution instead of applying arbitrary random dispersion.
Meaningful Lies and Scrambling
A tee-pad drive, a flat fairway standstill, and a throw from wet roots on a steep sidehill should not feel identical.
Surface, slope, clearance, stance, and shot difficulty should alter:
Maximum safe power
Accuracy-window size
Aim stability
Run-up availability
Slip risk
Animation and body position
Distinct Courses
Each course must have its own:
Biome
Tree species
Understory
Ground materials
Rock family
Foliage density
Elevation character
Weather personality
Course-management decisions
Signature holes
Courses should not look like the same Marketplace forest with different baskets.
Player Identity
Players should be able to create a recognizable golfer through:
Body proportions
Face
Hair
Appearance
Throw style
Clothing
Accessories
Bag
Equipment
The system must preserve one stable gameplay skeleton and animation architecture.
Sports Presentation
The game should eventually support:
Tee cameras
Follow-disc cameras
Landing cameras
Basket cameras
Shot tracers
Slow-motion replay
Score overlays
Tournament presentation
Player cards
Career results
Broadcast-style graphics
Safe, Measured Production
Every major system must pass:
Compilation
Automation
Regression
Save protection
Cook/package checks
Visual acceptance
Core playability gates
No feature is considered complete merely because it looks correct in the Editor.
## 4. The Complete Player Loop
The intended moment-to-moment loop is:
Enter course
→ review hole
→ evaluate wind and lie
→ select disc
→ select throw and stance
→ aim
→ set power
→ execute timing/release
→ character throws
→ disc flies through authoritative physics
→ camera follows flight
→ disc impacts or settles
→ rules evaluate OB/water/mando/basket
→ next lie is established
→ score updates
→ repeat
→ complete hole
→ advance
→ complete round
→ results and progression
A successful core-loop build must never leave the player wondering what to do next.
Every resolved shot should end in one of these outcomes:
Legal next lie
Penalty/drop-zone lie
Hole completed
Round completed
Visible recoverable error
A disc that flies forever, a camera that never returns, or a hole that cannot advance is a blocking playability defect.
## 5. The Throwing System
Existing Authority
The current project already has authoritative systems for:
Aim
Power
Timing
Hyzer
Nose angle
Spin
Wind
Disc launch
Flight
Collision
Ground play
Lie
Scoring
Camera
Replay
The character framework was integrated around these systems.
Character-to-Physics Contract
The accepted throw invariant is:
one input throw
→ one animation
→ one release event
→ one authoritative gameplay disc
→ one completed flight
→ one recovery to playable control
At the exact release frame:
The animation reads the world transform of disc_grip_r.
The held visual disc is released or hidden.
The adapter calls the existing authoritative launch path once.
Existing gameplay values determine the actual throw.
Existing flight physics takes over.
Animation does not independently recalculate:
Power
Spin
Aim
Hyzer
Nose angle
Wind
Aerodynamics
Current RHBH Prototype
Built and accepted:
Duration: 2.8 seconds
Release: frame 96 / 1.600 seconds
Throw finished: frame 162 / 2.700 seconds
Exactly one release
Exactly one finish event
Guarded against stale or duplicate notifies
Pre-release cancellation launches zero discs
Post-release interruption cannot spawn a replacement
Recovery watchdog protects against permanent throw-state lock
The current animation is technically valid but still prototype-quality.
Future Throws
Planned:
Production RHBH
Left-handed backhand
Forehand
Standstill
Approach
Putt
Jump putt
Straddle putt
Patent-pending stance
Kneeling or low-ceiling stance
Roller
Overhand shots
These should reuse the same release and physics authority.
## 6. Footing, Lie, and Shot-Difficulty System
This is a planned core gameplay system and should be implemented before the final vertical-slice gate.
Three Separate Difficulty Values
Power Demand
How much of the golfer’s available power the intended shot requires.
Accuracy Demand
How precise the aim, timing, angle, and landing zone must be.
Footing Difficulty
How physically difficult it is to execute the throw from the current lie.
The HUD could show:
POWER DEMAND        82%
ACCURACY DEMAND     64%
FOOTING DIFFICULTY  37%
Lie Evaluation Inputs
The evaluator should inspect:
Surface material
Surface grip
Firmness
Wetness
Unevenness
Ground slope
Sidehill direction
Height difference between feet
Roots
Rocks
Mud
Sand
Pine needles
Available run-up space
Reachback clearance
Follow-through clearance
Nearby trees and shrubs
Legal stance area
Example Surface Profiles
Surface	Grip	Stability	Run-up suitability
Concrete tee	100%	100%	100%
Dry grass	90%	90%	90%
Pine needles	75%	80%	75%
Wet grass	60%	75%	55%
Roots	65%	45%	30%
Mud	40%	50%	25%
Loose sand	45%	50%	20%


These are initial design values, not final tuning.
Power Ceiling
A difficult lie can reduce safe maximum power:
Available Power =
Player Power
× Stance Modifier
× Footing Modifier
× Clearance Modifier
The meter can show:
0% ─────────── 68% ───────────── 100%
                SAFE              OVERPOWER
The player may choose to overpower, but doing so increases:
Slip risk
Timing error
Nose-angle error
Hyzer-angle error
Balance failure
Reduced spin efficiency
Accuracy Window
Bad footing and demanding lines narrow the meter’s sweet spot.
A standstill may sacrifice power but improve control.
Stance Choices
Planned stance system:
Full run-up
Shortened run-up
Standstill
Straddle
Patent-pending
Kneeling
Low-ceiling stance
The character animation and Control Rig should visibly respond to the selected stance and terrain.
Fairness Rules
Lie evaluation is deterministic.
Opening and closing menus cannot reroll footing.
The HUD explains the main penalties.
Difficult lies always provide at least one legal recovery option.
Accessibility settings can widen timing windows.
The lie modifies release quality, not post-release physics.
## 7. Disc Physics and Equipment
Current State
The existing flight system is playable and remains authoritative.
Session 11 integrates a bounded project-owned equipment layer into the live project. Five deterministic generic disc instances use stable GUIDs, Tour plastic, generic stamps, 175 g defaults, selection/favorite state, and isolated schema-v1 persistence. The selected instance identity is captured before a legal throw and resolved once through the existing catalog and launch path. The plugin supplies value-only DTOs, never bag, release, or flight authority.
Planned Disc Definition
Each disc model should define:
Stable ID
Approved fictional or licensed brand ID
Class
Speed
Glide
Turn
Fade
Diameter
Height
Rim width
Rim depth
Dome
Mass range
Aerodynamic coefficients
Mesh
Stamp options
Planned Plastic Definition
Each plastic should define:
Grip
Durability
Stiffness
Stability offset
Ground friction
Skip tendency
Rolling resistance
Appearance
Individual Disc Instances
A player-owned disc can contain:
Unique ID
Disc model
Plastic
Weight
Wear
Color
Stamp
Nickname
Favorite status
Disc Wear
Wear should gradually affect the actual disc instance.
A seasoned disc might:
Turn more
Fade less
Skip differently
Feel different on the ground
Wear must be calibrated through the real physics system rather than inferred from display numbers alone.
The Session 11 technical implementation therefore keeps wear as metadata-only. Non-baseline weight scales mass and both inertias, but no calibrated aerodynamic or wear behavior is claimed.
Starter Development Equipment
The live development bag contains five generic project instances backed by the existing generic Apex, Vector, Line, Compass, and Touch catalog definitions and Base, Tour, and Crystal plastic definitions. Premium branding remains dormant and blocked.
No real manufacturer product data is required.
## 8. Character System
One Stable Gameplay Skeleton
Built:
SK_DG_Master
SKEL_DG_Master
69 bones
Six animation curves
Full finger structure
Twist bones
IK helpers
disc_grip_r
disc_grip_l
Every created golfer uses the same gameplay hierarchy.
The system does not create a new skeleton per body type.
IK and Control Rig
Built and accepted:
IK_DG_Master
Four IK goals
Four effectors
CR_DG_Master
Full-body/PBIK foundation
Elbow direction validated
Knee direction validated
Compression validated
Representative plant-foot stability validated
Disc-grip axes validated
Animation Blueprint
Built:
ABP_DG_Player
It receives:
Throw state
Body profile
Throw-style data
Animation playback
Control Rig correction
IK correction
Release state
Body Profiles
Built and accepted:
ShortCompact
Baseline
TallLongArms
Slider extremes
Visible body-proportion differences work through the same skeleton and rig.
## 9. Full Character Creator
Current State
Built and accepted for technical proxy scope.
The creator contains seven working tabs:
Identity
Body
Face
Hair
Appearance
Throw Style
Outfit
Identity
Supports:
Display name
Throwing handedness
Voice ID placeholder
Pronoun-set ID placeholder
Left-handed identity persists, but the newly animated gameplay throw remains RHBH.
Body
Supports:
Height
Wingspan
Shoulder width
Torso length
Leg length
Hand size
Body mass/build foundation
Muscularity
Body-fat appearance
Chest
Waist
Hips
Arms
Legs
These are visual identity values and do not secretly alter gameplay power.
Face
Twenty controls are mapped:
Head width and height
Brows
Eyes
Nose
Cheeks
Jaw
Chin
Mouth and lips
Ears
Current proxy support:
5 visibly demonstrated morphs
15 correctly marked as deferred
The project does not pretend unsupported proxy morphs are working.
Face Presets
Supports generic presets:
Default
Square
Narrow
Round
Hair and Facial Hair
Stable cosmetic catalog supports:
Hair
Facial hair
Eyebrows
Recoloring
Hat/hair restoration
Missing-item fallback
Appearance
Supports technical parameters for:
Skin tone
Eye color
Complexion
Freckles
Sun exposure
Scar hooks
Tattoo hooks
Appearance shading is proven on the modular head, not yet on a finished production body.
Outfit
Built and accepted:
Modular outfit slots
Apply and Cancel
Live preview
Color/material variants
Schema-8/9 reconstruction
Missing-item fallback
Full profile compatibility
Accepted RHBH compatibility
Current clothing is generic DO_NOT_SHIP blockout art.
Creator Operations
Supports:
Live preview
Presets
Apply
Cancel
Reset
Randomize
Locks
Save/reload
Migration
Missing-content fallback
Persistence
Current schema:
Schema 10
Explicit schema-8 and schema-9 migration is implemented.
The character system preserves:
Body
Face
Hair
Appearance
Throw style
Outfit
Identity
## 10. MetaHuman Production Visual Backend
Intended Architecture
The proxy/DG character remains the gameplay source.
MetaHuman becomes the high-quality visible layer:
DGMaster gameplay source
→ accepted throw animation
→ exact release
→ authoritative disc physics

DGMaster evaluated pose
→ IK Retargeter
→ visible MetaHuman
Session 8A
Built and accepted:
Dormant avatar-backend adapter hardened
Exactly two cook assets authored
Current DGMaster cook closure proven
UAT build/cook/stage/archive passed
69 required runtime packages/exports proven
No MetaHuman evidence fabricated
Current MetaHuman Status
Session 8B machine acceptance is complete. A project-owned original MetaHuman, Optimized Medium assembly, DGMaster visual retarget, backend switching, creator persistence, packaged reconstruction, held-disc alignment, authoritative release preservation, and the unchanged performance budget have all been proven in the final packaged v3.3 lane. The accepted procedural presentation corrects the prior idle/reference and horizontal-arm silhouettes without claiming licensed or captured production motion. Lower-body footwork, weight transfer, cadence, and final animation-art approval remain art decisions rather than missing machine evidence.
## 11. Mocap and Animation Pipeline
Technical Pipeline
Built and accepted:
source motion
→ source IK
→ retargeter
→ DG master
→ cleanup
→ production-stage test
The system supports:
Source import
Retargeting
Cleanup
Phase metadata
Release validation
Profile compatibility
Source/license tracking
Current Shipping Status
No legitimate owned or licensed production RHBH source has been found.
Therefore:
Synthetic fixture: DO_NOT_SHIP
Current Session 3 RHBH: gameplay fallback
Production mocap: blocked pending approved source
This is intentional and honest.
## 12. Course and Foliage Bible
Course Philosophy
A course should be designed as a disc-golf challenge first and a forest second.
The course system should define:
Tee
Basket
Primary fairway
Secondary fairway
Rough
Deep rough
Green
OB
Water
Mandos
Drop zones
Spectator zones
No-spawn zones
Then the environment system builds around those rules.
Recommended Foliage Stack
Course gameplay zones
→ course biome profile
→ landscape materials
→ landscape grass
→ PCG canopy
→ PCG understory
→ PCG ground details
→ manual hero pass
Per-Course Biomes
Each course should have a dedicated biome data asset defining:
Tree species
Species percentages
Growth stages
Understory
Ground cover
Deadfall
Rocks
Soil
Moisture
Slope response
Elevation response
Density
Season
Wind
Collision
Quality settings
Possible future biome families:
Sierra pine
Pacific Northwest
Midwest parkland
High desert
Southeast pine
Mountain meadow
Coastal
Alpine
Manual Hero Pass
Manual placement remains essential for:
Tee framing
Signature trees
Basket guardians
Landing zones
Scenic overlooks
Tournament areas
Designed throwing gaps
Foliage Collision Tiers
Full collision
Trunks
Major limbs
Boulders
Buildings
Major deadfall
Basket
Simplified collision
Medium branches
Rigid shrubs
Logs
Signs
Visual only
Grass
Ferns
Flowers
Leaves
Tiny twigs
Most debris
Perfect visuals should never create unfair microscopic collision.
Current Foliage Status
The project contains substantial unrelated environment asset roots, including forest, stump, water, presentation, and Pine Ridge content.
These have been intentionally preserved, but the advanced per-course biome/PCG production system has not yet been integrated.
Current policy:
Prove gameplay with current assets.
Audit existing foliage.
Build course-biome rules.
Identify visual gaps.
Upgrade only where quality materially improves.
## 13. Course Authoring and Validation
The v1.4/v1.5 build kit prepares a future course-definition system for:
Hole IDs
Hole numbers
Par
Tee transforms
Basket transforms
Published distance
Elevation
Drop zones
Mandos
Gameplay zones
The prepared validator can detect:
Missing IDs
Duplicate holes
Invalid par
Tee/basket placement problems
Distance mismatches
Invalid zones
Invalid mandos
Missing drop zones
Project-specific future checks should add:
Polygon self-intersections
Fairway obstruction
Safe player spawn
Basket and tee collision
Spectator safety
Performance
Lighting
Course completion path
This system is prepared but not yet integrated.
## 14. Camera, Replay, and Shot Tracer
Existing State
The existing camera and replay systems remain authoritative and have continued passing regressions.
Planned Presentation Layer
Prepared systems include:
Aim camera
Tee camera
Follow-disc camera
Landing camera
Basket camera
Creator camera
Replay camera
Free camera
Shot tracer
Telemetry replay
Optional full Unreal replay
The tracer should display the actual sampled flight, not a fake post-release curve.
Possible tracer features:
Color selection
Apex marker
Landing marker
Distance marks
Fade
Replay comparison
Accessibility-safe colors
These advanced systems are prepared in the build kit but not fully integrated into the live game.
## 15. Throw Lab and Telemetry
The Session 11 Throw Lab records completed authoritative native captures for development use:
Disc
Plastic
Weight
Wear
Release speed
Spin
Hyzer
Nose
Launch height
Wobble
Wind
Full trajectory
Impacts
Apex
Carry
Ground play
Total distance
Flight time
Lateral deviation
OB
Holed out
This is the primary bounded development adapter for:
Physics calibration
Disc comparison
Lie-system tuning
Footing-system tuning
Animation-release validation
Regression testing
The active implementation retains at most 64 records, 2,400 actual replay samples per record at nominal 60 Hz, and 512 ground transitions. It supports selection, pinning, deletion, exact two-record comparison, actual-sample replay through the existing replay actor, and isolated schema-v1 save/load. It does not create an alternate simulation, telemetry, replay, release, rules, or scoring authority. Shipping UI and production calibration remain pending.
## 16. Audio
The planned audio architecture is event driven.
Events include:
Throw release
Disc-flight loop
Tree hit
Grass hit
Dirt hit
Rock hit
Water hit
Basket chains
Basket cage
Footsteps
Crowd reaction
Forest ambience
Weather
Audio parameters may respond to:
Speed
Spin
Wobble
Surface
Wetness
Intensity
Listener distance
MetaSounds are preferred for responsive authored audio, but normal sound assets remain the fallback.
No licensed commercial sound pack is currently bundled.
## 17. Input, UI, and Accessibility
Planned Input Contexts
Gameplay
Aim and throw
Character creator
Replay
UI
Planned Control Support
Keyboard and mouse
Controller
Default controller preset
Southpaw controller preset
Remapping
Sensitivity
Dead zones
Invert Y
Hold or toggle aim
Throwing handedness and controller layout are separate settings.
Accessibility
Planned settings include:
Subtitles
UI scale
High contrast
Color-vision modes
Tracer color
Camera-shake scale
Reduced motion
Aim assist
Timing-window scale
Shot-shape guide
Optional flight preview
Accessibility modifies presentation and execution difficulty, not equipment statistics.
## 18. Competition, Career, and AI
Competition Contracts
Prepared:
Stroke play
Match play
Skins
Best-shot doubles
Scorecards
Penalty strokes
Multi-round events
Payouts
Career Contracts
Prepared:
Season
Currency
Player rating
World rank
Completed events
Round history
Sponsorship progression
AI Golfer Contracts
Prepared AI profiles include:
Power
Accuracy
Putting
Consistency
Aggression
Course management
Bag
AI should evaluate candidate shots using:
Expected progress
Success probability
OB risk
Obstacle risk
Landing error
Visible AI golfers should eventually use the same discs, release inputs, and physics as the user.
These are data and architecture contracts—not completed game modes.
## 19. Save and Data Safety
Current Character Save
Current accepted production profile:
Size: 5,212 bytes
SHA-256:
A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14
The external Session 7 backup is byte-identical.
All tests have been instructed to use disposable external UUID save directories.
Save Principles
Stable IDs
Explicit schema versions
Explicit migration
Missing-content fallback
No display-name-only references
No raw vendor asset path as sole identity
Separate global settings where appropriate
Production save protected from automation
## 20. Testing and Quality Philosophy
The project uses unusually strict acceptance gates.
Latest accepted Session 8A results include:
Editor build: pass
Runtime build: pass
UAT cook/package: pass for current DGMaster scope
Automation: 134/134
Session 3 throw: pass
Session 4 profile matrix: pass
Session 5 fallback and profiles: pass
Session 6 outfit matrix: pass
Session 7 full-character matrix: pass
Three-hole smoke: pass
Protected save: unchanged
Tests generally verify:
Reflection
Asset structure
Rig structure
Notify counts
Release authority
Profile compatibility
Outfit compatibility
Save migration
Missing-item fallback
Cook closure
Package contents
No-write behavior
Brand/provenance
Gameplay regression
No test should be weakened merely to obtain a green result.
## 21. Core Playability Gates
The v1.5 build kit defines four future blocking gates.
Smoke Gate
boot
→ menu
→ profile
→ course
→ tee
→ bag
→ aim
→ throw
→ release
→ disc launches
Core Loop Gate
flight
→ resolve
→ legal lie
→ next throw
→ basket
→ score
→ hole complete
Round Gate
multiple holes
→ scores retained
→ round complete
→ results
Persistence Gate
save
→ reload
→ character
→ bag
→ settings
→ round/career
→ safe missing-content recovery
A blocking Smoke or Core Loop failure stops visual polish work.
These gates are prepared but have not yet been executed as the final integrated v1.5 playability pass.
## 22. What Has Actually Been Built So Far
Fully Integrated and Accepted
Sessions 1–2
UE 5.8.1-compatible plugin
69-bone master skeleton
IK Rig
Control Rig
PBIK
Animation Blueprint
Three body fixtures
Disc grip bones
Manual rig acceptance
Session 3
2.8-second RHBH prototype
Exact release and finish events
Single-authority release adapter
Held provisional disc
Cancellation and duplicate protection
Recovery to gameplay
Session 4
Body creator
Throw-style creator
Live preview
Presets
Apply and Cancel
Persistence
Multiple body profiles
Session 5
Motion-source pipeline
IK retargeting
Cleanup stage
Phase/release validation
License/source manifest
Synthetic fixture marked DO_NOT_SHIP
Session 6
Modular outfit creator
Stable outfit catalog
Material variants
Live preview
Save/reload
Missing-item fallback
RHBH compatibility
Session 7
Seven-tab full creator
Schema 10 with explicit schema-8/schema-9 migration
Modular head
Face mappings
Hair and facial hair
Appearance parameters
Scar/tattoo hooks
Randomization and locks
Hat/hair restoration
Full-character test matrix
Session 8A
Avatar-backend adapter hardened
DGMaster cook closure
UAT build/cook/stage/archive
Current runtime dependency closure
Honest MetaHuman blocker report
Session 8B Accepted Scoped Integration
MetaHuman Creator Core Data and project plugin: verified
Real MetaHuman visual backend: packaged and machine-accepted
MetaHuman performance/cook evidence: accepted under the unchanged v3.3 contract
Remaining qualification: procedural presentation is not captured production motion; final animation-art judgment remains separate
Remaining Prepared or Planned Beyond the Bounded Proofs
Production camera, UI, accessibility, and audio content approval
Production PCG consumer, generated-content profiling, and visual approval
Full career, season, economy, ranking, and sponsorship systems
Production AI strategy and tournament fields
Production-calibrated flight-regression cases
Final vertical-slice gate
Core playability gate
## 23. Session-by-Session Project Phases
Session	Scope	Status
1	Audit, plugin integration, UE 5.8 compatibility	Complete
2	Master skeleton, IK, Control Rig, AnimBP	Complete
3	First RHBH and authoritative release	Complete
4	Body and throw-style creator	Complete
5	Mocap/retarget pipeline	Technical pass; production source blocked
6	Outfit customization	Complete for proxy scope
7	Full proxy character customizer	Complete for proxy scope
8A	Avatar adapter and DGMaster cook closure	Complete
8B	Real MetaHuman visual backend	Complete — machine accepted; final animation-art judgment remains
9	Brand and licensing audit	Complete — contract passes; public release blocked on eight explicit closures
10	Quality and environment adapters	Complete — bounded technical pass; production environment bindings and public release remain blocked
11	Disc equipment and Throw Lab	Complete — technical runtime pass; production physics/readiness and public release remain blocked
12	Camera, replay, input, UI, audio	Complete — bounded technical pass; production presentation/audio/input certification remains blocked
13	Course authoring, validation, PCG foliage	Complete — bounded technical pass; production PCG consumer, bindings, profiling, and visual approval remain blocked
14	Career, AI, settings, automation	Complete — bounded technical proof; full career and production AI readiness remain blocked
15	Final one-hole vertical slice	Complete — bounded technical pass; production readiness remains blocked
16	Core playability gates	Complete — 44/44 Editor and packaged technical gate
17	Interactive round flow	Complete — focusable/clickable Editor and packaged interaction path
18	Poly Haven derived-runtime provenance	Complete — one provenance blocker resolved; thirteen release blockers remain


The footing/lie difficulty system should be integrated during the course/gameplay phase before Session 15’s final vertical-slice acceptance.
## 24. Higher-Level Production Phases
Phase A — Technical Foundation
Sessions 1–4
Goal:
Stable project
Character skeleton
Rig
First throw
Basic creator
Existing gameplay preserved
Status: complete.
Phase B — Content Pipeline
Sessions 5–7
Goal:
Motion pipeline
Outfits
Full creator
Save and fallback architecture
Status: complete for proxy/technical scope.
Phase C — Production Character
Session 8
Goal:
MetaHuman visual backend
Real-time assembly
Retargeted gameplay
Honest creator mapping
Cook/package performance
Status: 8A and 8B complete for machine acceptance; final animation-art judgment remains explicit.
Phase D — Production Environment and Rules
Sessions 9–13
Goal:
Brand safety
Materials and quality profiles
Course biome system
Foliage
Course authoring
Lie and footing
Disc equipment
Cameras
Audio
Status: Sessions 9–13 are integrated and fail-closed within their bounded technical scopes. Session 10's PCG graph remains packaged but dormant, Session 13 adds a value-only mapping plan with no runtime generation consumer, production environment art remains unapproved, and equipment, presentation, audio, input certification, generated-content profiling, and visual acceptance remain behind explicit readiness blockers.
Phase E — Game Structure
Session 14
Goal:
Saves
Scorecards
Career proof
AI proof
Automated tests
Status: bounded technical proof complete. One original generic event, isolated schema-v1 career result, measured-flight AI selection, existing-path authoritative throw, and automation foundation are accepted. Full career/season/economy/ranking, production AI fields and strategy, shipping presentation, and public-release readiness remain deferred.
Phase F — Vertical Slice
Session 15
Goal:
One finished hole with:
High-quality player
Character creator
Outfit
Disc selection
Excellent RHBH
Realistic flight
Course foliage
Footing
Cameras
Tracer
Audio
Replay
Save/load
Performance
Status: bounded technical pass. One controlled Pine Ridge Hole 1 loop is accepted in fresh Editor and packaged evidence with 8/8 focused and 231/231 full automation. Production character/motion/environment/audio, calibrated-equipment breadth, manual visual approval, provenance/legal clearance, broad playability, and public-release readiness remain deferred behind fourteen explicit blockers.
Phase G — Playability Validation
Session 16
Goal:
Smoke
One-hole loop
Multi-hole round
Persistence
Soft-lock prevention
Status: complete. Fresh Editor and packaged acceptances passed 44/44 blocking checks; later interaction and provenance work preserved this gameplay authority.
Phase H — Alpha Production
After the vertical slice passes:
Expand to more holes
Add more shot types
Add production equipment
Replace proxy content
Build course tools for designers
Begin AI and tournament production
Phase I — Content Scale
Multiple courses
Distinct biomes
More clothing
More discs
Career events
AI fields
Commentary/presentation
Seasonal and weather variants
Phase J — Beta and Polish
Optimization
Accessibility audit
Save migration
Input certification
Balance
QA
Crash recovery
Brand/legal audit
Platform packaging
Phase K — Launch
Launch scope, platform list, price, licensing, and content count remain TBD.
## 25. Immediate Next Milestone
The immediate milestone remains the ordered production/provenance path. Session 16 proves the inherited core loop across 44/44 blocking checks, Session 17 proves the native round-flow interaction path, and Session 18 durably closes the Poly Haven source-to-runtime receipt without claiming whole-stage coverage. The highest-value next work is actual staged-package provenance, followed by remaining production character/motion/environment/audio/UI acceptance. Preserve the single authoritative release/flight path, actual-sample-only replay and Throw Lab boundaries, project round/scoring authority, protected schema-10 profile, isolated development saves, visual-only environment boundary, dormant/on-demand PCG state, quarantines, generic identity, and all thirteen remaining release blockers. Full career, large AI fields, multiplayer, many courses, licensed equipment, and content-scale expansion remain out of scope until the appropriate production gates are resolved.
## 26. Definition of the First Finished Vertical Slice
The first major product milestone is one hole that demonstrates:
Boot
→ create/load golfer
→ choose outfit
→ choose disc
→ load beautiful course
→ evaluate wind and footing
→ select stance
→ aim
→ execute RHBH
→ character visibly releases correctly
→ disc flies realistically
→ camera and tracer follow
→ disc collides and settles
→ lie updates
→ repeat
→ basket detects completion
→ score updates
→ replay works
→ save and reload
→ acceptable performance
That single hole should be good enough to answer:
Is this game genuinely fun and worth scaling?

Only after that answer is yes should the project spend heavily on premium foliage, materials, courses, clothing, and licensed content.
## 27. Immutable Project Rules
These should remain permanent development principles:
Existing authoritative gameplay is preserved unless deliberately replaced through a tested migration.
One throw creates one release and one gameplay disc.
Animation changes release presentation; physics determines flight.
One stable gameplay skeleton supports every created golfer.
MetaHuman is visual, not a second gameplay authority.
Unsupported creator controls must not pretend to work.
Unlicensed real brands remain blocked.
Proxy and synthetic assets remain DO_NOT_SHIP.
Production saves are protected from automation.
Missing optional cosmetics or premium assets cannot block playability.
Course gameplay zones drive foliage placement.
Tiny decorative foliage does not create unfair collision.
Lie and footing change execution difficulty, not post-release physics.
Uncalibrated flight cases cannot be treated as regression truth.
Cooked/package behavior matters more than Editor-only appearance.
A feature is not complete until its regressions pass.
The vertical slice comes before large-scale content production.
A beautiful soft-locked game is still a failed build.
## Current One-Sentence Status
DGTour has a validated playable foundation, an accepted packaged MetaHuman presentation backend, protected single-authority release and flight, fail-closed brand/license and environment controls, technically accepted Sessions 12–15 integration work, a 44/44 Session 16 core-playability closure, a packaged focusable/clickable Session 17 round-flow path, and a durable Session 18 Poly Haven source-to-runtime receipt; public release remains blocked on thirteen explicit closures, and the next provenance gate is whole staged-package classification.

The logical permanent home for this is:
C:\DGTour\Docs\DG_GAME_BIBLE.md
with the current build status maintained separately in:
C:\DGTour\Docs\CODEX_CHARACTER_INTEGRATION_CHANGELOG.md
C:\DGTour\Docs\CODEX_LONG_RUN_STATE.md
