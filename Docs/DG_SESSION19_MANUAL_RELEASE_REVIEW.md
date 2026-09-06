# Session 19 Manual and External Release Review Packet

Date: 2026-08-24  
Authority: `Config/DG_Session19ManualReleaseReviewPolicy.json`  
Objective inventory: `Evidence/Session19/ManualReleaseReviewInventory.json`  
Validator: `Scripts/validate_dg_session19_manual_release_review.py`

## Result

The manual review packet is complete enough to identify exactly who must decide what,
which candidate must be reviewed, and which artifacts must exist. It is not a release
approval, legal opinion, license grant, visual approval, physics calibration, or product
owner sign-off. All eight manual gates remain pending.

The user's blanket authorization approves the engineering closure strategy and allows
the team to prepare evidence. It does not establish title clearance, trademark or trade
dress clearance, audio or motion rights, or an absent named reviewer's judgment.

Normal validation means `PASS_REVIEW_PACKET_VALID_HUMAN_APPROVALS_PENDING`.
`--require-human-approval` must return exit code 2 until a separate sanitized decision
record is bound to one exact Windows Shipping candidate and all required named reviewers
have recorded dispositions.

## Objective narrowing

| Area | Repository-observable result | What it does not prove |
|---|---|---|
| Public title | `Disc Golf Tour`, `DiscGolfTour`, and `DGTour` are inventoried as working names; final public title remains `null`. | Owner title lock or legal clearance in any market. |
| Public display names | Eight equipment names and four course/hole names are enumerated with stable IDs and source paths. | Trademark/name clearance or approval to expose any row publicly. |
| Logos/trade dress | Active runtime brand is the generic project namespace and declares zero authorized logos. | Visual originality, absence of copied artwork, or non-confusing trade dress. |
| Final visuals | Ten exact candidate-bound PNGs document front end, tee/recovered-lie states, Hole 1/2 completion, and the 3/3-hole final scorecard. They are technical observations and do not satisfy the required 18-view, two-resolution human capture set. | Final-candidate visual quality, safe zones, readability, accessibility, or owner approval. |
| Audio | Twelve original generated SoundWave packages are source-bound and proven present in IoStore and the packaged Asset Registry for the current candidate. Audible runtime playback and the human mix review remain unproven. | Rights approval, audible playback, mix quality, accessibility, product-owner approval, or release acceptance. |
| Character/motion | Seven production motion identities are runtime-bound and proven present in IoStore and the packaged Asset Registry; protected synthetic identities are absent from the cook. | Likeness/performer approval, garment/mesh quality, animation quality, disc-contact quality, performance, soak, or owner approval. |
| Environment | The current candidate proves three required runtime environment assets present and the editor PCG graph absent from Shipping; environment and Pine Ridge roots remain cryptographically summarized. | Approval of route readability, proxies, wind, LODs, camera clearance, final RHI visuals, performance, or owner approval. |
| Physics/play feel | The latest local engineering regression is inventoried separately from the incomplete route-telemetry session. | Real-world calibration, representative player feel, or product approval. |

## Exact public-name review inventory

The title reviewer must choose and lock one exact public title; no candidate is currently
accepted. A rename must cover package metadata, executable/file-description surfaces,
front end, HUD, scorecard, settings, credits, crash/report surfaces, screenshots, store
copy, and any installer/uninstaller strings. Stable internal IDs may remain internal only
when the bound runtime inventory proves they are not unintentionally public.

Every one of these current display-name rows requires an explicit `APPROVE`, `REPLACE`,
or `NOT_PUBLIC_IN_SCOPE` disposition:

- Molds: `Apex`, `Vector`, `Line`, `Compass`, `Touch`.
- Plastics: `Base`, `Tour`, `Crystal`.
- Course: `Pine Ridge Championship`.
- Holes: `Pine Ridge Opening`, `Needle Gate`, `Gallery Lake`.

Replacement display text must not silently change stable save/catalog IDs. The final
Shipping runtime/asset scan must bind the reviewed display text to the candidate.

### Candidate-bound technical inventory for gates 1 and 2

After the replacement Shipping candidate and its verification receipt exist, generate
the two technical artifacts with the exact archive leaf and matching Unreal Engine tool
set:

```powershell
python Scripts\generate_dg_session19_shipping_title_name_inventory.py --generate `
  --candidate-id <S19_WindowsShipping_candidate> `
  --archive <exact-candidate-package-root>\Windows `
  --shipping-verification Evidence\Session19\ShippingCandidateVerification-<S19_WindowsShipping_candidate>.json `
  --unrealpak "<engine>\Engine\Binaries\Win64\UnrealPak.exe" `
  --unreal-editor-cmd "<engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
```

The create-new record is
`Evidence/Session19/ShippingTitleRuntimeNameInventory-<candidate>.json`. Validate it
against the unchanged live archive with the same arguments, replacing `--generate`
with `--validate-record <record-path>`. The record supplies only
`BOUND_SHIPPING_TITLE_SURFACE_INVENTORY` and `BOUND_SHIPPING_RUNTIME_NAME_SCAN`.
It binds the archive manifest, Shipping executable, verification receipt, tools,
packaged config, PE metadata, loose text, container namespaces/payloads, cooked
equipment packages, Asset Registry objects, and all twelve stable-ID/name rows.

Unobservable runtime, rendered, installer, store, external-service, capture, and legal
surfaces remain explicitly pending. A passing technical inventory does not lock a
title, clear a display name, authenticate a reviewer, close either manual gate, or
approve release. The generator refuses to overwrite an existing record and validation
fails closed if the candidate, archive, receipt, tool, policy, source, artifact hash,
stable ID, name row, or path differs.

## Exact final capture set

The accepted capture manifest must bind every image to the final Shipping archive
manifest and executable SHA-256. Development, Editor, historical, or differently hashed
package captures cannot substitute. The policy requires the following 18 views at both
1280x720 and 1920x1080:

1. Front-end title.
2. Settings and accessibility.
3. Hole 1 intro, route, and tee.
4. Hole 1 flight, lie, and score.
5. Hole 2 intro, route, and tee.
6. Hole 2 flight, lie, and score.
7. Hole 3 intro, route, and tee.
8. Hole 3 flight, lie, and score.
9. Scorecard.
10. Round complete.
11. Character setup.
12. Character drive, release, and follow-through.
13. Character approach.
14. Character putt.
15. Environment fairway close view.
16. Environment forest route gap.
17. Gallery Lake environment view.
18. Every logo, signage, sponsor, stamp, and brand-bearing surface.

### External capture-manifest workflow

Use a dedicated directory outside both `C:\DGTour` and the exact candidate archive.
It must contain only the 36 PNG files, with no subdirectories or extra sidecars. Name
each file `<viewId>__<resolution>.png`, using the policy identifiers verbatim. For
example:

```text
front_end_title__1280x720.png
front_end_title__1920x1080.png
...
all_logo_signage_sponsor_and_brand_surfaces__1920x1080.png
```

Before capture, emit the authoritative read-only 36-file plan directly from the live
policy and generator. This command does not inspect a candidate, write evidence, or
make an approval claim:

```powershell
python Scripts\generate_dg_session19_final_shipping_capture_manifest.py `
  --print-required-captures
```

After capturing from the exact final Shipping executable, generate a new UUIDv4 and
run the create-new manifest command. `--capture-tool` must point to the actual external
capture executable or script used for the session; the tool is hashed, not copied.

```powershell
$captureSession = [guid]::NewGuid().ToString().ToLowerInvariant()
python Scripts\generate_dg_session19_final_shipping_capture_manifest.py --generate `
  --candidate-id <S19_WindowsShipping_candidate> `
  --archive <exact-candidate-package-root>\Windows `
  --shipping-verification Evidence\Session19\ShippingCandidateVerification-<S19_WindowsShipping_candidate>.json `
  --capture-root <absolute-external-capture-directory> `
  --capture-tool <absolute-capture-tool-path> `
  --capture-session-id $captureSession `
  --capture-method WINDOWS_GRAPHICS_CAPTURE
```

The only accepted output location is
`Evidence/Session19/FinalShippingCaptureManifest-<candidate>-<captureSession>.json`, and
the generator refuses to overwrite it. Before binding that file to a decision record,
revalidate the unchanged archive, receipt, tool, capture root, and manifest:

```powershell
python Scripts\generate_dg_session19_final_shipping_capture_manifest.py `
  --validate-record Evidence\Session19\FinalShippingCaptureManifest-<candidate>-<captureSession>.json `
  --candidate-id <S19_WindowsShipping_candidate> `
  --archive <exact-candidate-package-root>\Windows `
  --shipping-verification Evidence\Session19\ShippingCandidateVerification-<candidate>.json `
  --capture-root <absolute-external-capture-directory> `
  --capture-tool <absolute-capture-tool-path> `
  --capture-session-id <captureSession> `
  --capture-method WINDOWS_GRAPHICS_CAPTURE
```

The validator recomputes the complete live archive manifest, Shipping executable and
receipt bindings, policy and tool identities, every PNG SHA-256, CRC/decode structure,
and exact dimensions. It rejects missing or extra files, duplicate pixels, hard links,
project/archive-contained roots, symlinks, junctions/reparse points, path escapes, and
candidate/tool/archive drift. The capture method remains an operator declaration: these
mechanical checks cannot authenticate pixel provenance or substitute for human review.
The manifest therefore hard-codes every art, accessibility, logo/trade-dress, legal,
product-owner, and release approval field to `false`.

The product owner, art reviewer, and accessibility reviewer must disposition readability,
safe zones, clipping, substitutions, visible development content, route clarity, result
clarity, score clarity, and every open visual defect. Logo and trade-dress review is a
separate legal decision over the same bound captures; art approval does not replace it.

## Exact authored-audio closure

The semantic router covers ThrowRelease, AirborneFlight, GroundContact, GroundState,
BasketOutcome, Penalty, HoleStart, HoleCompletion, HoleTransition, RoundCompletion,
Replay, and Flyover. The router's silent fallback is an engineering fail-safe, not
production coverage.

Before audio approval is valid, provide:

1. A source-to-runtime manifest for every Shipping sound, including source identity,
   author/publisher, license or project-authorship basis, permitted distribution scope,
   source hash, derived runtime identity, and derived hash.
2. A row for each of the 12 semantic categories recording the bound runtime assets or an
   explicit product-owner decision that silence is intentional.
3. A Shipping-candidate mix review covering loudness/dynamic range, loop seams,
   spatialization, distance behavior, replay behavior, voice/music/ambience sliders,
   mute behavior, and accessibility expectations.
4. Sanitized audio-owner, license-reviewer, and product-owner dispositions.

There are currently no accepted authored-media or mix-review artifacts. Audio is therefore
both objectively content-blocked and subsequently human-blocked.

## Exact character and motion closure

Retained technical packages, a Control Rig, an Animation Blueprint, and a MetaHuman-based
visual do not by themselves constitute production animation. Prototype and synthetic
motion under NeverCook roots may not be used as approval evidence.

Before character approval is valid, provide:

1. A production motion source/license manifest covering every Shipping drive, approach,
   putt, idle/setup, release, grip, follow-through, recovery, and transition clip.
2. Candidate-bound captures for setup, drive/release/follow-through, approach, and putt.
3. An animation review of release timing, planted-foot behavior, root motion, hand/disc
   attachment, mirrored handedness, contacts, looping, transitions, and recovery.
4. An art review of mesh, skin, hair, garments, cloth intersections, deformation, LODs,
   silhouette, camera-distance quality, and likeness/identity boundary.
5. Sanitized animation-owner, art-reviewer, and product-owner dispositions with every
   exception either fixed or explicitly rejected for release.

## Exact environment acceptance

Technical environment evidence narrows this gate but does not accept it. The final review
must use the exact Shipping candidate and all three holes. Required decisions cover:

- intended route gaps, landing zones, bailout readability, water-carry readability, and
  connector navigation;
- competitive collision/proxy alignment and vegetation interaction boundaries;
- ground, forest, shoreline, water, dressing, lighting, wind response, LOD/Nanite,
  culling, shadows, camera clearance, and quality-tier consistency;
- real-RHI 1920x1080 performance, memory, hitching, visual defects, and any final
  substitutions;
- environment provenance in the actual staged-package receipt.

The environment owner, level-design owner, and product owner must approve the bound
capture/performance set. A passing deterministic tree-placement receipt or historical
performance capture cannot make the final visual judgment.

## Exact physics and play-feel closure

The 240 Hz regression suite proves repeatability and guards accepted engineering
behavior. It is not a measured-accuracy claim. Before calibration approval is valid:

1. Freeze a source-provenanced measured/reference dataset and predeclare comparison
   tolerances before evaluating it.
2. Repeat the bounded Shipping catalog across representative mold, plastic, handedness,
   power, angle, wind, ground, fixture, and Circle 1/Circle 2 putting scenarios.
3. Record carry, lateral displacement over time, apex/time, turn onset, fade onset,
   landing angle/speed, ground response, and outlier disposition.
4. Run a structured, predeclared playtest protocol that separates physical credibility,
   learnability, timing feel, bag differentiation, putting pace, ground/fixture response,
   and course strategy readability.
5. Complete the intended route-telemetry dataset or explicitly revise and approve its
   protocol; the current local report is incomplete with zero attempts.
6. Obtain sanitized physics-owner, playtest-owner, and product-owner dispositions.

No accepted measured dataset or structured play-feel result is currently bound, so this
gate remains human-blocked after engineering stability checks.

## Decision-record procedure

Copy `Evidence/Session19/ManualReleaseDecisionRecord.template.json` only when named
human review begins for the current candidate. Bind it to these four values:

- Shipping archive receipt path.
- Shipping archive manifest SHA-256.
- Shipping executable SHA-256.
- Final capture-set manifest SHA-256.

For every gate, each required role must record a reviewer ID, UTC review time, decision,
and review artifact ID. Store only sanitized identifiers and dispositions in the
repository; confidential legal analysis, receipts, account data, and acquisition secrets
remain outside it. A gate with a missing reviewer, unbound artifact, open exception, or
`PENDING`/`REJECTED` disposition is not approved.

Supplying a completed decision record also requires live strict capture inputs. The
manual validator invokes the external capture validator again; a JSON-only 36-row
manifest is insufficient:

```powershell
python Scripts\validate_dg_session19_manual_release_review.py `
  --require-human-approval `
  --decision-record <repository-relative-completed-record> `
  --title-name-inventory <repository-relative-bound-title-name-inventory> `
  --capture-manifest Evidence/Session19/FinalShippingCaptureManifest-<candidate>-<captureSession>.json `
  --shipping-archive <absolute-candidate-root>\Windows `
  --external-capture-root <absolute-external-capture-directory> `
  --capture-tool <absolute-capture-tool-path> `
  --capture-session-id <captureSession> `
  --capture-method WINDOWS_GRAPHICS_CAPTURE
```

## Validation

```powershell
python Scripts/generate_dg_session19_final_shipping_capture_manifest.py --self-test
python Scripts/validate_dg_session19_manual_release_review.py --self-test
python Scripts/validate_dg_session19_manual_release_review.py --write-evidence
python Scripts/validate_dg_session19_manual_release_review.py
python Scripts/validate_dg_session19_manual_release_review.py --require-human-approval
```

The first four commands must pass. The final command must return exit code 2 while any
of the eight review gates remains pending.
