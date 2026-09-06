# Session 14 Career, AI, Settings, and Automation Audit

Status: **BOUNDED TECHNICAL PASS; PUBLIC RELEASE REMAINS BLOCKED.**

Session 14 implements a narrow foundation proof, not a finished career mode or production opponent system. It preserves the existing schema-10 player profile, normalized player-settings persistence, project round/scoring authority, equipment catalog, `ADiscGolfTourGameMode::RequestThrow`, and `UDiscFlightComponent` fixed-step flight path.

## Tournament and career proof

The project-owned fallback event is the original generic `PineRidgeChampionship`, presented by `dg_generic`. It contains exactly one three-hole StrokePlay round on Championship tees with pars 3/4/4. Unknown brands, event drift, extra rounds, non-contiguous holes, unsupported formats, and invalid values fail closed.

`FDiscGolfRoundState` remains score authority. Its `Strokes` field already includes penalties, while framework scorecards add base strokes and penalties. Conversion therefore writes framework base strokes as project strokes minus project penalties, retains the penalty separately, and crosschecks framework total/to-par against the project totals. This prevents double-counting.

Career progress uses the isolated schema-v1 `UDiscGolfCareerProgressSaveGame` domain and default development slot `DGT_Career_Dev_v1`; it is not folded into the protected profile. Validation bounds completed events and round history to 128 and sponsorships to 32, rejects duplicate event/results and future schemas, and commits or loads atomically. The live proof saves, reloads, validates, and deletes one disposable GUID result without passing the active gameplay round to career code.

## Measured-flight AI proof

One transient `dg_generic_ai_proof` profile contains all six finite framework skills and a validated copy of the existing generic starter loadout. The adapter accepts only completed real-flight measurements, validates their commands, equipment identities, contexts, telemetry, and finite values, then deterministically stable-sorts the candidates. It delegates utility selection to `UDiscGolfAIShotPlannerComponent` and translates the winner back to the existing `FThrowCommand` without changing outputs on failure.

The tick-free live runner evaluates four detached `ADiscActor` previews in the loaded Pine Ridge world. Each preview uses `DiscGolfMath::ResolveThrowRelease`, the resolved player-owned disc instance, current wind, course collision, and the real `UDiscFlightComponent`; no GameMode settle/hole-out delegate is bound, so previews cannot score or present a shot. Only the selected measured command enters `ADiscGolfTourGameMode::RequestThrow`, exactly once.

Both canonical Editor and packaged runs measured four previews, selected candidate 4 (zero-based index 3), recorded one authoritative stroke, and completed with 1,856 authoritative samples over 7.725 seconds. The packaged report proves `event_id=PineRidgeChampionship`, `presenting_brand_id=dg_generic`, one career history result, an exact save/load round trip, and disposable slot deletion.

StateTree remains optional and is not a module dependency. No analytic alternate solver, fake score path, parallel inventory, parallel settings subsystem, plugin player-profile authority, large field, full season, economy, ranking simulation, sponsorship system, or production opponent strategy is claimed.

## Automated smoke foundation

Seventeen Session 14 tests cover the implementation and donor-requested fast foundations:

- `DiscGolfTour.Session14.CompetitionCareer.*` passes 7/7 for generic event validation, penalty-safe scorecard conversion, atomic failure, bounded history, future-schema rejection, duplicate rejection, and isolated save round-trip.
- `DiscGolfTour.Session14.AI.*` passes 5/5 for the generic profile, measured outcome conversion, invalid-input atomicity, deterministic selection/ties, and selected stable identity/enums.
- `DiscGolfTour.Session14.Smoke.*` passes 5/5 for unique catalog IDs/plastics, starter-bag resolution, score/penalty parity, Pine Ridge course validation, and schema-v1 career serialization.

No donor flight range is enforced because the provided CSV remains explicitly uncalibrated. The existing approved 84.5 m reference-flight envelope still passes.

## Save-boundary correction

The first exploratory live invocation omitted `-UserDir` and therefore allowed ordinary bootstrap persistence to touch the project save domain. The run was rejected as closure evidence. The unexpected 5,747-byte profile and isolated equipment file were preserved in `C:\DGTour_Backups\Session14_AccidentalLive_20260824_122618`; the protected profile was restored only from the independently verified 5,212-byte accepted external backup whose SHA-256 was exactly `A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`.

The runner now also fail-closes unless exactly one active absolute `-UserDir` exists at `C:\DGTour_TestRuns\Session14\<GUID>`, outside the project. The wrapper remains the primary pre-bootstrap boundary. Both accepted Editor and packaged closures used fresh external GUID domains; their disposable career slots were deleted, their isolated run roots were removed from the active test location after evidence collection, and the restored protected profile remained exact A999 afterward.

## Closure evidence

Final Editor and Game Development builds pass. `DiscGolfTour.Session14.*` passes 17/17 and full `DiscGolfTour.*` automation passes 223/223 with unique paths, zero warning-successes, zero failures, and zero not-run tests. Project validation and the reference flight pass.

Fresh archive `C:\DGTour_Packages\S14_CareerAI_c84db54d-7d6e-490e-a24b-9f0b0a20f413` completes clean rebuild, full non-iterative cook, stage, Pak, IoStore, and archive with 985 cooked, zero incrementally skipped, seven platform-skipped, and 992 total packages. Its 54 files / 1,916,110,012 bytes are individually bound by `Saved/Session14Reports/Session14PackageManifest_c84db54d-7d6e-490e-a24b-9f0b0a20f413.json`.

The packaged executable repeats the four actual-flight previews, one framework-planner selection, one existing-path authoritative throw, and isolated career result round trip, emits both exact PASS markers, and requests exit status 0. The protected profile finishes exactly 5,212 bytes with the accepted A999 SHA-256, and no additional file remains in the project SaveGames directory.

Canonical closure bindings:

- Contract: `Config/DG_Session14CareerAiTestingContract.json`, 16,762 bytes, SHA-256 `1BB296E1C1EF4B1DA91AD9EBA80637C02333C02573BA9F40CBEFF7723DC1307C`.
- Validator: `Scripts/validate_dg_session14_career_ai.py`, 34,967 bytes, SHA-256 `0D7CC0BF9522C2AF4F920B92B3F19093630D4D1382B7FDF3DDC9F2FBEC8765F3`.
- Canonical validator report: `Saved/CareerReports/DG_Session14CareerAiTestingAudit.json`, 1,315 bytes, SHA-256 `16A0DA1B6C04A7E9A3FEE8885CF53A826E5FB5175347C8447911D9514762FF9A`.
- Focused automation: `Saved/Automation/Session14FocusedFinal_e06b4a59-f773-4ca6-9457-4cc57b0486fa/index.json`, 7,681 bytes, SHA-256 `09FD3116BE695F2300EA06067E8739E2414FF16316D995CD5807E2406696E14C`.
- Full automation: `Saved/Automation/Session14FullFinal_133d78f7-5f28-4a0a-b104-865540d4e3d8/index.json`, 89,753 bytes, SHA-256 `F266591AC4BC297903B4C43A1626350409403E7CE98C88337FD29F8640406E3F`.
- Packaged smoke report: `Saved/Session14Reports/Session14AIGolferPackagedSmoke_f9872bf4-8319-4625-8426-224fed282076.json`, 4,041 bytes, SHA-256 `91E62B3F4D00CF00D5FE1A0E1E86F9A26B341BBAE20FCABFC7BD752E2395D95A`.
- Package manifest: `Saved/Session14Reports/Session14PackageManifest_c84db54d-7d6e-490e-a24b-9f0b0a20f413.json`, 11,115 bytes, SHA-256 `D88146A3EB83BD911F50A7556C18714192F27B29B12874877ED53AE627397D66`.
- Protected profile: `Saved/SaveGames/DiscGolfTour_Profile_0.sav`, 5,212 bytes, SHA-256 `A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`.

## Release status

Session 14 appends `SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING` without weakening or reordering the twelve inherited blockers. Full career/season/economy/ranking design, production AI strategy and field simulation, shipping UI/presentation, calibration, content clearance, provenance, and public-release approval remain pending. The exact ordered public-release blocker count is thirteen.
