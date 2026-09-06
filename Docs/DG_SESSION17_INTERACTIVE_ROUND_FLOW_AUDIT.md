# Session 17 Interactive Round-Flow Audit

## Result

Session 17 closes a bounded interaction defect in the existing v0.5 presentation
path. The native front end and scorecard/results surfaces now expose real mouse,
keyboard, and controller-focus targets instead of relying on Canvas text plus raw
key interception. This is a technical interaction milestone, not production UI,
visual-design, accessibility-certification, shipping, or public-release approval.
The exact fourteen inherited release blockers remain unresolved.

## Implementation

- `DiscGolfRoundFlowPresentation.*` projects authoritative GameMode/round data into
  validated value snapshots for `Hidden`, `FrontEnd`, `LiveScorecard`,
  `HoleComplete`, and `RoundResults`.
- Every screen has an exact fail-closed action whitelist and one initial-focus
  action. Contradictory visibility, stale totals, invalid rows, fabricated score,
  and extra commands are rejected atomically.
- `UDiscGolfRoundFlowWidget` builds a code-owned Slate/UMG surface with focusable
  buttons, authoritative score rows, bounded one-shot dispatch, and no scoring,
  save, throw, flight, or progression authority.
- `ADiscGolfTourPlayerController` owns widget reconciliation, GameAndUI focus,
  cursor state, gameplay-context suppression, settings-origin recovery, raw-input
  fallback, and guarded dispatch through the existing GameMode actions.
- `ADiscGolfHUD` retains Canvas as a fallback only when the native widget cannot be
  attached. Settings and the character creator take presentation precedence.
- The results actions reuse `AdvanceToNextHole`, `RestartRound`, and the new guarded
  `ReturnToMainMenu` path; they do not introduce a second round authority.

## Verification

- Fresh Editor and Game Development builds pass. Current build products are:
  - Editor DLL: 5,852,672 bytes, SHA-256
    `8B0A7F612C433DECF05CB35AE7B544902FAFBCF3D702378914BCA87CE8EBA6FE`.
  - Game executable: 341,386,752 bytes, SHA-256
    `81CFE36707976461E341A305CE1CE65ED20188501AA5715761336750FA7AD9A3`.
- Focused `DiscGolfTour.Session17.RoundFlow.*` automation passes exactly 9/9 with
  zero warning-successes, failures, or not-run tests. Report:
  `Saved/Automation/Session17RoundFlow_Focused_b7d25d40-9681-4503-8062-d0f57a6428c5/index.json`,
  SHA-256 `048752790533822108CF542E2D0E8532C1C69C1A5ABD31CC7FC54991064CFE5B`.
- An earlier exploratory commandlet run was rejected after a widget-structure test
  called `UUserWidget::TakeWidget()` without a live world/local player and crashed
  in UMG. The test was corrected to use a development-only direct native-Slate
  construction seam; the accepted rebuild and 9/9 rerun above are clean. This was
  test-harness misuse, not a runtime widget failure.
- Full `DiscGolfTour.*` automation passes exactly 249/249 with zero
  warning-successes, failures, or not-run tests. Report:
  `Saved/Automation/Session17RoundFlow_Full_150692cc-285d-43d1-8e4f-88cc483f82b2/index.json`,
  SHA-256 `325FDE0156EFC771B958BFFE64797DEEDACB3F04CB9FF6293EE0E55092436833`.
- Rendered Editor D3D12 front-end run
  `dba49552-ef41-49b1-a9b5-b30c20aa7951` passed widget/focus ownership,
  settings-origin recovery, Start/Continue, gameplay recovery, one player, and
  duplicate-action rejection. Its log SHA-256 is
  `6DAC37C94436C6866747767F07C41F4E31F37F5A42C97123288E84F3D4DD30C0`.
- Rendered Editor D3D12 results run
  `895ba4cb-97ff-405c-a3ed-06c0790aa1e4` completed the authoritative three-hole
  round, presented interactive final results, restored settings to results,
  restarted Hole 1 through the shared action, recovered gameplay, and rejected a
  duplicate action. Its log SHA-256 is
  `446519E34987BF137DDFF9E38DACB53F98ED20BE6C491FA01F5FE4F0393F8BC6`.

## Packaged proof

Fresh non-iterative archive
`C:\DGTour_Packages\S17_InteractiveRoundFlow_4b2b89de-408a-4874-915a-113393260190\Windows`
completed clean build, full cook, stage, Pak, IoStore, and archive with AutomationTool
exit 0 and `BUILD SUCCESSFUL`. IoStore contains 985 packages. The archive contains
54 files / 1,918,052,812 bytes; its sorted `relative-path<TAB>bytes<TAB>sha256`
manifest digest is
`B0B74533C0B96CEDCD2FDE0EB3194A488088DBEC2D27451E7F78D0D75D58F7DA`.
The inner executable matches the fresh Game build at 341,386,752 bytes / SHA-256
`81CFE36707976461E341A305CE1CE65ED20188501AA5715761336750FA7AD9A3`.

- Packaged front-end run `097bab94-679c-4caa-9619-6a262a32d489` emitted
  `DG_SESSION17_ROUND_FLOW_FRONT_END: PASS` and exited 0. Log SHA-256:
  `24029FD7A55EBA058CC312135E5F4C4D6794EAF6DBC21ED00D3D4FAD6BB3D1C8`.
- Packaged results run `05facc0d-e116-4459-93ac-0a1d20671209` emitted both
  `DG_SESSION17_ROUND_FLOW_RESULTS: PASS` and the existing three-hole PASS marker,
  then exited 0. Log SHA-256:
  `C31346D5F66C2EEF4ABD91D0925B9C6CF7A5BA0EC3925FC4627F8ED2E48638F5`.

All four live runs used disposable external GUID `-UserDir` roots. Project
SaveGames still contains exactly one file: the protected 5,212-byte profile with
SHA-256 `A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14`.
`validate_project.py`, `reference_flight_check.py`, the Session 16 validator's
159/159 self-test, normal Session 16 validation, and `git diff --check` pass.

## Evidence-boundary correction

The Session 16 validator previously compared its historical accepted Editor/Game
hash records to the mutable files under current `Binaries/Win64`. Any legitimate
later clean build therefore produced a false historical-closure failure. It now
keeps those accepted record values exact while allowing later current binaries to
differ. Retained Session 16 automation, live reports, package manifest/package,
frozen source records, and the protected profile remain live byte- and hash-checked.
The 159 mutation cases continue to reject altered evidence records.

## Remaining boundary

This slice resolves the concrete no-click-target/no-focus-target interaction gap.
It does not provide final art direction, responsive platform certification, screen
reader/localization completion, production motion/audio/environment acceptance,
brand/provenance clearance, signing, store readiness, or user visual approval.
No release blocker is removed or added by this milestone.
