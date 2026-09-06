# Session 16 Core Playability Audit

## Current checkpoint

Session 16 has passed its **bounded technical core-playability gate**. Fresh Editor
acceptance `d64ee8c4-e1a6-4b49-9d73-d9bb76c834e2` and fresh packaged acceptance
`07bb2b2a-be53-4686-bf67-088158e3da5c` each passed all 44 blocking checks: Smoke
11/11, CoreLoop 16/16, Round 7/7, and Persistence 10/10. Both emitted
`PASS_CORE_PLAYABILITY_GATE_RELEASE_BLOCKED`, preserved project boundaries, and used
isolated external user directories that were moved recoverably after evidence
collection. This is technical closure only, not production-readiness approval,
shipping approval, legal clearance, or public-release approval.

Authority: `Config/DG_Session16CorePlayabilityContract.json`

Validator: `Scripts/validate_dg_session16_core_playability.py`

Runtime source lane: `Source/DiscGolfTour/DiscGolfSession16CorePlayabilityContract.h/.cpp`

Focused tests: `Source/DiscGolfTour/Tests/DiscGolfSession16CorePlayabilityTests.cpp`

The dual-state source contract preserves the original preparation definition through
strict schema and Session 9-15 continuity checks while carrying the verified technical
closure evidence. Its completed technical result is
`PASS_CORE_PLAYABILITY_TECHNICAL_CLOSURE_RELEASE_BLOCKED`. Technical closure does not
set `release_ready`: release-required validation remains blocked by the exact
inherited fourteen blockers.

## Canonical 44-check mapping

The contract preserves the exact v1.5 build-kit order and count. Every check is
blocking and maps to an existing project authority; the Session 16 layer observes
those authorities and does not replace them.

| Gate | Checks | Existing authorities |
| --- | ---: | --- |
| Smoke | 11 | GameMode/PlayerController startup, schema-10 profile, persistent course bootstrap, project bag, pawn input, accepted release path, fixed-step flight |
| CoreLoop | 16 | fixed-step flight, course rules and lie state, GameMode basket/score completion, round state, replay/camera/input recovery, single-player/single-disc lifecycle gate |
| Round | 7 | persistent course activation, `FDiscGolfRoundState`, GameMode completion, HUD scorecard/results presentation |
| Persistence | 10 | schema-10 player profile, isolated equipment and career domains, normalized settings, full-character and optional-backend fallback |

The exact prepared checks are:

- Smoke: `boot_game`, `main_menu_available`, `player_profile_available`,
  `course_load`, `spawn_at_tee`, `bag_available`, `select_disc`, `enter_aim`,
  `start_throw`, `release_notify_fires`, and `disc_launches`.
- CoreLoop: `physics_advances`, `disc_settles_or_holes_out`, `lie_updates`,
  `next_throw_available`, `ob_penalty_if_triggered`,
  `water_penalty_if_triggered`, `basket_detects_completion`, `score_updates`,
  `hole_completes`, `pause_resume`, `camera_recovers`,
  `input_context_recovers`, `replay_exits_cleanly`, `no_duplicate_player`,
  `no_duplicate_disc`, and `no_soft_lock`.
- Round: `advance_to_next_hole`, `spawn_next_hole`,
  `scorecard_persists_between_holes`, `multiple_holes_complete`,
  `round_completes`, `results_screen_available`, and
  `return_to_menu_or_continue`.
- Persistence: `save_player`, `save_bag`, `save_settings`,
  `save_round_or_career`, `load_save`, `loaded_character_matches`,
  `loaded_bag_matches`, `loaded_settings_match`,
  `missing_cosmetic_falls_back`, and
  `missing_optional_asset_does_not_block_play`.

Failure-code mappings are explicit and may be shared where the underlying failure is
the same. For example, all four save checks map to `SaveFailed`, restored character,
bag, and settings checks map to `SaveMismatch`, and round continuation reuses
`PlayerCouldNotContinue`. A missing check, reordered check, unknown authority,
changed failure code, or premature closure claim fails validation.

## Focused automation contract

The focused prefix is `DiscGolfTour.Session16.*` with exactly nine expected tests:

1. `DiscGolfTour.Session16.Catalog.ExactRequiredChecks`
2. `DiscGolfTour.Session16.Gates.SmokePass`
3. `DiscGolfTour.Session16.Gates.CoreLoopPass`
4. `DiscGolfTour.Session16.Gates.RoundPass`
5. `DiscGolfTour.Session16.Gates.PersistencePass`
6. `DiscGolfTour.Session16.Gates.RejectIncompleteEvidence`
7. `DiscGolfTour.Session16.Contract.FailureAndReportSemantics`
8. `DiscGolfTour.Session16.Watchdog.ExplicitLifecycle`
9. `DiscGolfTour.Session16.Persistence.OptionalAssetFallback`

The final post-package run passed all nine tests with zero warning-successes,
failures, not-run, or in-process tests. Its report is
`Saved/Automation/Session16Focused_PostPackage_5eeeebc6-969b-4d99-a342-e62aeff522f6/index.json`,
SHA-256 `2703FB605C565C68A00403E49F883F236749B39003B51C491CEACEB7C53D4272`.

## Fail-closed validation

The validator uses strict JSON parsing and rejects duplicate keys, non-finite numbers,
unknown or missing fields, order/count drift, relaxed policy flags, changed build-kit
sources, unsafe or absent authority paths, incomplete authority coverage, premature
evidence claims, continuity drift, blocker drift, and release/closure claims.

Its mutation self-test exercises 159 baseline, structural, semantic, continuity,
per-check failure-code, per-check authority, evidence, and malformed-JSON cases.
Normal, `--require-prepared`, and `--require-closure` modes validate the preserved
source preparation plus the populated technical-closure evidence. The contract keeps
`release_ready=false`, does not claim production or release readiness, and
`--require-release-ready` remains blocked.

Commands:

```powershell
python Scripts/validate_dg_session16_core_playability.py
python Scripts/validate_dg_session16_core_playability.py --require-prepared
python Scripts/validate_dg_session16_core_playability.py --self-test
python Scripts/validate_dg_session16_core_playability.py --require-closure
python Scripts/validate_dg_session16_core_playability.py --require-release-ready
```

## Frozen continuity

The contract pins the exact byte counts and SHA-256 digests of the Session 9 through
Session 15 contracts and validators. It does not rewrite historical evidence. It also
requires the project SaveGames directory to contain only
`Saved/SaveGames/DiscGolfTour_Profile_0.sav`, exactly 5,212 bytes with SHA-256
`A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`.

The source contract validates those boundaries read-only. Both accepted live runs
used an isolated external GUID `-UserDir` and proved the project Content, project
saves, and protected profile remained unchanged; packaged acceptance also proved the
staged package remained unchanged.

## Accepted closure evidence

All eight technical closure requirements are satisfied:

1. Editor Development build passed.
2. Game Development build passed.
3. Focused Session 16 automation passed exactly 9/9 with zero warnings, failures, or
   not-run tests:
   `Saved/Automation/Session16Focused_PostPackage_5eeeebc6-969b-4d99-a342-e62aeff522f6/index.json`,
   SHA-256 `2703FB605C565C68A00403E49F883F236749B39003B51C491CEACEB7C53D4272`.
4. Full `DiscGolfTour.*` automation passed exactly 240/240 with zero warnings,
   failures, or not-run tests:
   `Saved/Automation/FullRegression_Session16_PostPackage_d38a9e79-59d3-4852-bb95-81bd8f3097a6/index.json`,
   SHA-256 `1D3B3E2F21C944F02BCF469B11FDB2FC5525C327D550C807226645F3F0A4F570`.
5. Fresh isolated Editor acceptance passed 44/44:
   `Saved/Session16Reports/Acceptance_editor_d64ee8c4-e1a6-4b49-9d73-d9bb76c834e2/PlayabilityReport.json`,
   42,321 bytes, SHA-256
   `303EE590BBDD8F3B3A3222782D4AE83B987528258440654D12E89FEB292CC4A4`.
   The report ran from `2026-08-24T23:04:38.892894+00:00` through
   `2026-08-24T23:08:40.306767+00:00`.
6. Fresh non-iterative build/cook/stage/Pak/IoStore/archive completed. The retained
   package contains 54 manifest-bound files.
7. Fresh isolated packaged acceptance passed 44/44:
   `Saved/Session16Reports/Acceptance_packaged_07bb2b2a-be53-4686-bf67-088158e3da5c/PlayabilityReport.json`.
8. Both acceptance reports confirm unchanged project boundaries. The packaged run
   also confirms the package manifest remained unchanged, and the protected project
   profile remains exactly 5,212 bytes with SHA-256
   `A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`.

The packaged manifest is retained at
`Saved/Session16Reports/Acceptance_packaged_07bb2b2a-be53-4686-bf67-088158e3da5c/RuntimeEvidence/PackageManifest.json`.

## Blockers and result semantics

`SESSION16_CORE_PLAYABILITY_CLOSURE_PENDING` was a temporary development-gate
blocker, not a fifteenth public-release blocker. It is absent from the completed
technical contract and accepted live gate. The inherited public-release blocker
array remains the exact existing fourteen:

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
12. `SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING`
13. `SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING`
14. `SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING`

Passing Session 16 proves that the accepted Editor and packaged builds completed the
defined 44-check core-playability path without a blocking failure or soft lock. It
does not close any of the fourteen inherited production, content, provenance, brand,
legal, shipping, or public-release blockers.
