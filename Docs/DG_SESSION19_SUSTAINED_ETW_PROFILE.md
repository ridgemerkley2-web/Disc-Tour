# Session 19 Sustained Shipping ETW Profile Lane

## Purpose

This additive v3 lane collects candidate-bound Windows Performance Recorder CPU/GPU ETW evidence for the sealed Shipping candidate without changing the frozen sustained-performance v1/v2 policies, declarations, or candidate archive.

It does not certify performance or approve release. A technical pass still leaves human performance acceptance, product-owner approval, legal approval, and release approval false.

## Current state

The current host is `CAPTURE_READY_EXPORT_BLOCKED`:

- WPR is installed, idle, and exposes the required CPU/GPU profiles.
- `tracerpt.exe` is installed and policy-bound for trusted ETL parsing and provider/PID proof.
- `WPAExporter.exe` is not installed or selected.
- No exact `.wpaProfile` is selected for the required raw CPU/GPU exports.
- No v3 declaration, run directory, ETL, export, or technical manifest exists.
- Capture is therefore forbidden.

The ordinary sealed Shipping target does not compile a full Unreal CPU/GPU trace. This lane uses external Windows ETW evidence and must not fabricate or substitute an Unreal trace.

## Authority files

- `Config/DG_Session19SustainedShippingEtwProfilePolicyV3-S19_WindowsShipping_20260827T023919Z_51a1c132f837.json`
- `Scripts/audit_dg_session19_shipping_profile_lane.py`
- `Scripts/generate_dg_session19_sustained_etw_profile_declaration.py`
- `Scripts/run_dg_session19_sustained_etw_profile.py`
- `Scripts/validate_dg_session19_sustained_etw_profile.py`

The policy pins the exact candidate, archive inventory, Shipping executable, three-hole rendered-performance receipt, WPR identity, and tracerpt identity. After trusted exporter installation, the policy must be deliberately revised and resealed with the exact exporter and `.wpaProfile` identities before declaration or capture.

## Safe verification

These commands do not launch the game, start WPR capture, run tracerpt, run WPAExporter, or write evidence:

```powershell
python Scripts\audit_dg_session19_shipping_profile_lane.py --self-test
python Scripts\validate_dg_session19_sustained_etw_profile.py --self-test
python Scripts\generate_dg_session19_sustained_etw_profile_declaration.py --self-test
python Scripts\run_dg_session19_sustained_etw_profile.py --self-test

python Scripts\validate_dg_session19_sustained_etw_profile.py `
  --policy Config\DG_Session19SustainedShippingEtwProfilePolicyV3-S19_WindowsShipping_20260827T023919Z_51a1c132f837.json `
  --archive C:\DGTour_Packages\S19_WindowsShipping_20260827T023919Z_51a1c132f837\Windows `
  --external-root C:\DGTourExternal

python Scripts\generate_dg_session19_sustained_etw_profile_declaration.py `
  --preflight-only `
  --policy Config\DG_Session19SustainedShippingEtwProfilePolicyV3-S19_WindowsShipping_20260827T023919Z_51a1c132f837.json `
  --archive C:\DGTour_Packages\S19_WindowsShipping_20260827T023919Z_51a1c132f837\Windows `
  --external-root C:\DGTourExternal `
  --engine-root "C:\Program Files\Epic Games\UE_5.8\Engine"
```

While the exporter/profile bindings are absent, the validator and preflight must exit 2. Expected states are `BASELINE_VALID_CAPTURE_AND_EXPORT_EVIDENCE_NOT_PRESENT` and `PREDECLARED_CAPTURE_READY_EXPORT_BLOCKED_NO_CAPTURE_STARTED`.

## Unblock sequence

1. Provide a trusted `WPAExporter.exe` and an exact project-owned `.wpaProfile` that emits the required raw CPU and GPU tables.
2. Revise and reseal the v3 policy with their exact path, byte length, SHA-256, and tool version. Re-run all adversarial self-tests.
3. Run `--preflight-only`; capture must remain forbidden unless every live tool/profile/status binding is exact.
4. Explicitly emit one append-only declaration. Never overwrite or reuse a declaration/run UUID.
5. Run explicit capture with both candidate-ID and profile-run-ID confirmations. The runner records the Shipping process ID, process-creation identity, executable hash, capture window, archive snapshots, WPR commands, and ETL envelope.
6. Run explicit export. Trusted tracerpt provider/PID proof and an actual exact WPAExporter invocation/output inventory are mandatory.
7. Run the independent validator with both `--declaration` and `--run-root` before finalization.
8. Finalize only after the independent read-only validator recomputes the complete run successfully.

## Technical acceptance boundary

A technical pass requires all of the following from one declared run:

- unchanged candidate archive before and after capture;
- real WPR CPU/GPU capture chronology;
- ETL byte envelope plus trusted tracerpt parsing, required providers, and captured-game PID evidence;
- exact WPAExporter executable/profile/invocation/input/output bindings;
- raw CPU/GPU events for the captured Shipping process;
- independently recomputed GPU, GameThread, RenderThread, and RHIThread metrics;
- each role spanning the declared sustained duration within the policy tolerance;
- an append-only manifest matching the independent validator's recomputation.

High-entropy bytes alone are never accepted as a real ETL. CSV or normalized JSON alone is never accepted as proof that WPAExporter ran. The evidence model is bounded operational process evidence, not cryptographic attestation or hostile same-user forgery resistance.
