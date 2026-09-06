# Session 12 Camera, Replay, Input, UI, and Audio Audit

Status: **TECHNICAL RUNTIME IMPLEMENTATION ACCEPTED; PRODUCTION READINESS PENDING; PUBLIC RELEASE BLOCKED ON ELEVEN EXPLICIT CLOSURES.**

## Acceptance boundary

Session 12 is technically accepted as a bounded project-owned presentation layer. This acceptance covers the implemented runtime paths, focused and full automation, live Editor-game evidence, fresh non-iterative packaging, and packaged-live evidence. It is not a public-production acceptance, a content-rights clearance, a platform-certification result, or permission to ship.

Session 12 presents and orchestrates accepted project-owned state. It does not create a second throw, release, gameplay disc, flight, trajectory, course, rules, scoring, equipment, settings, or save authority. The fixed-step 240 Hz SI solver remains unchanged. Replay and tracer presentation consume immutable native trajectory samples. The schema-10 production profile and the Session 9-11 contracts and validators remain frozen.

The installed plugin and `_BuildKit` remain reference/value sources only. Plugin camera, replay, tracer, audio, settings, and UI-flow components are forbidden as runtime authorities. Gameplay Cameras, CommonUI, full engine replay, and the MetaSound Builder API remain optional and are not hard compile or shipping dependencies. Project Canvas UI and project Enhanced Input/runtime mapping fallback remain valid fallback paths.

## Technically accepted runtime paths

### Camera ownership and live-shot presentation

- `DiscGolfCameraViewContract` is the project-owned exclusive view lifecycle. Generation-stamped tokens bind Aim/Tee, FollowDisc/Landing/Basket, ShotReplay, CharacterCreator, FreeCamera, and CourseFlyover modes to their required project owner. A different active owner cannot preempt the view, and a stale callback cannot release a newer view.
- `ADiscBroadcastCameraDirector` and `DiscGolfBroadcastCameraMath` remain the read-only live-shot camera authority. They consume the active disc, release, hole, and authored camera-anchor state without steering, colliding with, or replacing the gameplay disc.
- `ADiscGolfTourGameMode` acquires and releases the project view token around live-shot, replay, and flyover lifecycles. Failure to acquire or initialize fails closed and returns the view to the existing project player-camera flow.
- `bAutoFollowDisc` gates automatic broadcast follow. Reduced motion removes the live-shot blend and replay camera lag; it does not change gameplay or replay samples.
- Gameplay Cameras remains optional and is not wired as authority.

### Immutable replay and tracer presentation

- `DiscGolfPresentationMath::BuildBoundedActualReplaySamples` validates finite, ordered native trajectory and ground-transition data, then selects source indices only. Endpoints, state/contact/surface boundaries, and the closest actual sample to each explicit transition are retained; no synthetic trajectory sample is created.
- The default capture policy bounds source history, output to 2,400 actual samples, sampling density to 60 Hz, and duration to 120 seconds. `ADiscReplayActor` independently rejects invalid, unordered, oversized, over-duration, or non-finite replay input before showing or ticking the actor.
- `ADiscReplayActor` remains a presentation-only ghost. It interpolates display frames over immutable accepted samples and never invokes simulation, collision, rules, scoring, history, or export authority.
- Project replay controls now support pause/resume, bounded seek, playback-rate cycling, and tracking/tee camera cycling. Reduced motion disables replay camera lag. Replay completion and cancellation release the exact replay view token before returning control.
- The project tracer continues to render accepted replay capture state. The normalized tracer-color preset is applied by the project presentation path; it does not alter the authoritative trajectory.

### Input routes, remapping, and controller fallback

- `DiscGolfInputRoutePolicy` explicitly resolves the five project routes with `CharacterCreator > UI > Replay > AimThrow > Gameplay` precedence. Gameplay may enter AimThrow, but once another route owns input it accepts only its own actions; UI, creator, and replay input cannot pass through into gameplay.
- Replay-route-only controls are Space/Gamepad Face Bottom for pause, Left/Right or D-pad Left/Right for one-second seek, and Up/Gamepad Face Top for playback-rate cycling. Consumed replay input does not fall through to shot or menu actions.
- Existing project remapping remains authoritative. The controller reset-all branch is reachable, and the settings/control selection indices are independent so page changes do not corrupt selection.
- The project runtime Enhanced Input fallback retains its complete mapping set and accepts normalized Default/Southpaw plus controller dead zone. It rebuilds safely when either setting changes. Southpaw swaps the controller stick roles only; character throwing handedness remains an independent character/profile choice.
- Canvas and Enhanced Input fallbacks remain available when authored UI/input assets are absent; no CommonUI or plugin input-flow dependency was introduced.

### UI, settings, and accessibility

- `FDiscGolfPlayerSettings` was extended in place without a main save-schema bump. Normalized persisted settings now cover subtitles, full high-contrast UI, color-vision mode, tracer-color preset, auto-follow, replay speed, ambience and voice volume, controller dead zone, Southpaw controller, aim assist, timing-window scale, shot-shape guide, and optional flight-preview intent.
- The existing settings Canvas exposes 32 normalized rows in a bounded three-column layout. The HUD, scorecard, hole intro, replay chrome, creator, and controls presentation remain read-only consumers of project gameplay/save authorities.
- High-contrast UI affects project HUD accents, reduced motion bypasses hole-intro fades and camera smoothing, and the shot-shape guide gates the aiming guide. The tracer preset is applied to the project tracer. Color-vision mode, subtitles, optional flight preview, ambience volume, and voice volume are normalized and exposed hooks where corresponding authored production content is not yet present.
- Aim assist and timing-window scale are copied into `UThrowControllerComponent` only at throw-command capture. Aim assist is bounded to a maximum six-degree correction toward the basket direction; timing-window scale is clamped to 0.5-2.0. Defaults of zero aim assist and 1.0 timing scale retain parity with the pre-Session-12 command. Equipment, release, solver, and handedness authorities are unchanged.

### Semantic audio routing

- `DiscGolfPresentationAudio` remains the project semantic authority for release, flight, ground, basket, penalty, hole flow, round completion, replay, and flyover events. It validates finite context, clamps intensity/pitch, suppresses regression emission, deduplicates adjacent identical events, and retains a maximum 64-event semantic trace.
- `UDiscGolfPresentationAudioRouterComponent` is a project-owned, no-tick adapter. `ADiscGolfTourGameMode` calls it only after an event passes semantic acceptance and is appended to the bounded project trace. The route receives normalized `MasterVolume * EffectsVolume`; routing cannot change gameplay or semantic event meaning.
- The router sanitizes the coarse provider payload, preserves project semantic identity/pitch/replay/listener/gain data, rejects an invalid semantic contract atomically, suppresses immediate duplicate route keys, and bounds its diagnostic trace to 64 entries.
- Optional `USoundBase` fallbacks are primed asynchronously. No per-event synchronous load exists. The Blueprint provider route is invoked with or without an asset, and an empty fallback table is an explicit successful `SILENT ASSET FALLBACK` technical path.
- The installed plugin bridge remains a value vocabulary only and is not runtime authority. Its DTO cannot preserve every project semantic field, so the richer project route remains canonical.
- No production audio content is currently present. The source inventory found zero supported raw audio files, zero sound-named project assets, and zero project assets with SoundWave, SoundCue, or MetaSound signatures. Ambience and voice settings are hooks only because no corresponding semantic categories/assets or licensed final mix have been accepted.

## Verification and current evidence

The technical contract is `TECHNICAL_RUNTIME_IMPLEMENTATION_ACCEPTED_PRODUCTION_READINESS_PENDING`. All seven implementation capabilities are `TECHNICAL_PASS`, including the live/fresh-package closure capability.

- Final Win64 Development builds passed for both Editor and Game: `Saved/Logs/Session12EditorBuildClosure_8f79bfcd-5783-4e2f-9bd2-fcf14359b4a5.log` and `Saved/Logs/Session12GameBuildClosure_8f79bfcd-5783-4e2f-9bd2-fcf14359b4a5.log`.
- Final focused `DiscGolfTour.Session12.*` automation passed 16/16 with zero failed or not-run tests: `Saved/Automation/Session12FocusedClosure_db263248-c975-4058-be62-87669bc17d00/index.json`.
- Final full `DiscGolfTour.` automation passed 180/180 with zero failed or not-run tests: `Saved/Automation/Session12FullClosure_67b0de32-7266-4b9d-ab18-539ac11fc267/index.json`.
- `validate_dg_session12_presentation.py` exits 0 in normal mode with no pending capability and eleven release blockers. `--require-release` correctly exits 2 with `BLOCKED_PENDING_SESSION9_TO_SESSION12_PRODUCTION_CLOSURE`.
- `validate_project.py` passed. `reference_flight_check.py` passed with the accepted Apex reference carry of 84.5 m. The protected schema-10 profile remains 5,212 bytes with SHA-256 `A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`.
- Live Editor-game evidence passed in `Saved/Logs/Session12LiveClosure_d9a4b071-53e3-4804-b851-b0f8d98ca388.log`: broadcast transitions reached Launch, Fairway Track, and Basket/Landing; seven semantic events routed; and the Session 12 smoke passed with 492 bounded actual samples, replay progress 0.168, exclusive view ownership, verified pause/seek/rate/camera changes, and real post-resume timer advancement.
- Fresh non-iterative package creation passed with AutomationTool exit 0, `BUILD SUCCESSFUL`, 984 cooked packages, and zero incrementally skipped packages in `Saved/Logs/Session12PackageClosure_cdb72579-35bf-4463-b3db-1dac5cfd7d3d.log`. The archived build is `C:/DGTour_Packages/S12_Presentation_Closure_cdb72579-35bf-4463-b3db-1dac5cfd7d3d/Windows`.
- Packaged-live evidence passed in `Saved/Logs/Session12PackagedClosureFinal_0c92b43a-266d-4c7f-920c-232b64c8befd.log` with the same 492-sample replay, exclusive view ownership, replay controls, seven sanitized audio routes, and successful process exit.

The focused contract tests cover exclusive/stale-token camera ownership, strict actual-sample selection, replay validation and controls, semantic audio mapping/sanitization/silent fallback, input-route isolation, normalized settings, Southpaw/dead-zone fallback, accessibility command capture, HUD state, and fail-closed release-state ordering. The validator continues to freeze the accepted Session 9-11 contract/validator bytes, protected profile, and relevant donor references, and rejects plugin authority wiring, synchronous route loads, or prohibited hard engine dependencies.

## Exact production-readiness blockers

The ordered blockers are:

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
11. `SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING`

The eleventh blocker no longer means that the bounded runtime implementation or evidence is incomplete. It now covers remaining production acceptance: shipping UI/accessibility review, platform input certification, final camera/replay presentation polish, authored and licensed audio content, production mix/voice/ambience coverage, content provenance, and public-release approval.

## Release state

Technical exit is accepted as `TECHNICAL_EXIT_ACCEPTED_PRODUCTION_READINESS_PENDING`. Public production readiness remains blocked: `releaseBlocked` is true, `release_ready` and `release_use_allowed` are false, and production-readiness approval is false. No technical pass, live smoke, package, or packaged-live run overrides any of the eleven blockers or authorizes public release.
