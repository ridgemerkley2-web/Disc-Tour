# Session 19 sustained Shipping performance certification scaffold

## Scope and claim boundary

This scaffold defines a future external technical-evidence lane. It does not run a
soak, modify the game or candidate, or turn the existing three-hole 30-second
rendered-performance result into release certification. A passing sustained
technical receipt still records all of these as false:

- human performance acceptance;
- product-owner approval;
- release approval;
- release readiness;
- cryptographic attestation or hostile-same-user forgery resistance.

The trust model is bounded operational evidence. Files, hashes, timestamps, and
tool exports are independently cross-checked, but an administrator controlling the
host can fabricate operational evidence. Human review remains a separate gate.

The source-controlled contract is
`Config/DG_Session19SustainedShippingPerformanceCertificationPolicy.json`. The
generator and validator are:

- `Scripts/generate_dg_session19_sustained_performance_declaration.py`;
- `Scripts/validate_dg_session19_sustained_performance.py`.

## Why predeclaration is separate

The declaration must exist before any capture artifact. It freezes one exact:

- Session 19 Windows Shipping candidate ID;
- live archive inventory, launcher, and inner Shipping executable identity;
- already validated candidate-bound three-hole performance receipt;
- lowercase UUIDv4 certification-run ID;
- 15–120 minute duration (30 minutes by default in the policy);
- host, sensor, PresentMon, and Unreal Insights evidence contract.

The generator revalidates the prior three-hole receipt against its original run and
the live archive, snapshots the archive and receipt twice, and publishes the
declaration append-only under an explicitly selected external root. It launches no
process. The existing guarded Shipping capture accepts exactly 30 seconds and is not
itself a sustained profiling lane; do not reuse its bounded receipt as a thermal or
thread-profile claim.

## Predeclare a future run

Use an external declaration path that does not already exist. Do not put it in the
project, candidate archive, original three-hole run, or a canonical project Evidence
directory.

```powershell
$Candidate = 'S19_WindowsShipping_<timestamp>_<nonce>'
$Archive = "C:\DGTour_Packages\$Candidate\Windows"
$PriorRun = 'C:\DGTourExternal\ShippingPerformance\<candidate>\<run-uuid>'
$PriorReceipt = 'C:\DGTourExternal\Receipts\ShippingPerformance-<candidate>.json'
$ExternalRoot = 'C:\DGTourExternal\SustainedPerformance'
$Declaration = "$ExternalRoot\Declarations\<new-name>.json"

python Scripts\generate_dg_session19_sustained_performance_declaration.py `
  --candidate-id $Candidate `
  --archive $Archive `
  --three-hole-receipt $PriorReceipt `
  --three-hole-run-root $PriorRun `
  --duration-seconds 1800 `
  --external-root $ExternalRoot `
  --output $Declaration
```

The command prints the generated certification-run UUID. A future runner must create
one new directory whose leaf is exactly that UUID and populate only the declared
files. The guarded runner is
`Scripts/run_dg_session19_sustained_performance.py`.

## Guarded capture runner

Always run the read-only preflight first:

```powershell
python Scripts\run_dg_session19_sustained_performance.py `
  --preflight-only `
  --declaration $Declaration `
  --archive $Archive `
  --three-hole-receipt $PriorReceipt `
  --three-hole-run-root $PriorRun `
  --candidate-id $Candidate `
  --external-root $ExternalRoot
```

Preflight revalidates the declaration, prior receipt, live archive, host power/AC/
storage state, `nvidia-smi`, and the installed PresentMon timed-capture switches. It
does not create directories or launch a capture.

The declared `nvidia-smi` field list is exact. In particular, the runner never
silently substitutes `enforced.power.limit` for a declaration that names
`power.limit`. If a laptop driver reports the declared field as unavailable,
preflight fails and the policy/declaration must be explicitly revised and regenerated
before capture; an alias chosen after predeclaration would invalidate the evidence.

The additive v2 policy is the explicit laptop-GPU lane. It names
`enforced.power.limit` in both `requiredNvidiaSmiQueryFields` and
`effectivePowerLimitQueryField`; the v1 policy remains unchanged for immutable v1
declarations. Generate a v2 declaration with
`DG_Session19SustainedShippingPerformanceCertificationPolicyV2.json`. Validators
select only the exact supported policy named and hashed by the declaration, so a v1
declaration cannot be reinterpreted under v2 telemetry semantics.

Capture requires the explicit `--capture` switch. There is intentionally no default
long-running action:

```powershell
python Scripts\run_dg_session19_sustained_performance.py `
  --capture `
  --declaration $Declaration `
  --archive $Archive `
  --three-hole-receipt $PriorReceipt `
  --three-hole-run-root $PriorRun `
  --candidate-id $Candidate `
  --external-root $ExternalRoot
```

The runner launches the exact bound inner Shipping executable with only Pine Ridge
Hole 1, 1920×1080, DX12, profile-load/write suppression, and a fresh external
`UserDir`. It never adds `-trace`, console execution, logging, benchmark, fixed-time,
NullRHI, or other diagnostic flags. The player/operator must keep representative
rendered gameplay active throughout the declared window. After the timed capture,
the runner stops only the child processes it created.

Capture mode writes the PRE/POST snapshots, one-second NVIDIA series and metadata,
and normalized PresentMon artifacts. Raw PresentMon output and a non-authoritative
capture state remain under `Working/<run-id>`, outside the exact run directory.

### Unreal trace is a required separate step

The installed Unreal Insights executable does not expose a reliable CLI that can
externally attach to this already-running Shipping process and export the four
required timing roles. Adding Unreal `-trace` launch flags would change the frozen
Shipping invocation and was not authorized by the declaration. The runner therefore
does not improvise them and does not fabricate a trace, timing export, thread
profile, or manifest. Capture mode ends with
`CAPTURE_COMPLETE_REAL_UNREAL_TRACE_AND_TIMING_EXPORT_REQUIRED` and every acceptance
claim false.

If a real trace was collected by a separately approved method during the same
capture window, export its exact four-role normalized timing CSV, then finalize with
explicit observed timestamps and channels:

```powershell
python Scripts\run_dg_session19_sustained_performance.py `
  --finalize-only `
  --declaration $Declaration `
  --archive $Archive `
  --three-hole-receipt $PriorReceipt `
  --three-hole-run-root $PriorRun `
  --candidate-id $Candidate `
  --external-root $ExternalRoot `
  --unreal-trace C:\DGTourExternal\ProfileExports\capture.utrace `
  --unreal-timing-csv C:\DGTourExternal\ProfileExports\timing.csv `
  --profile-started-utc 2026-01-01T00:05:00.000Z `
  --profile-finished-utc 2026-01-01T00:06:00.000Z `
  --trace-channel cpu --trace-channel frame --trace-channel gpu
```

Finalization validates and copies the real trace/timing files, computes the exact
four role summaries, binds the selected `UnrealInsights.exe`, and publishes
`RunManifest.json` last. It reports only that the exact flat manifest is ready for
the independent validator; technical validation, human acceptance, and release
readiness remain false.

## Required run directory

The run directory is flat and exact. Extra files, directories, links, junctions,
missing files, or renamed files fail closed.

```text
<certification-run-uuid>/
  RunManifest.json
  ArchiveSnapshot.Pre.json
  ArchiveSnapshot.Post.json
  HostSnapshot.Pre.json
  HostSnapshot.Post.json
  NvidiaSensorCapture.json
  NvidiaSensors.csv
  PresentMonCapture.json
  PresentMon.csv
  ThreadProfile.json
  UnrealInsights.utrace
  UnrealInsightsTiming.csv
```

`RunManifest.json` hash-binds every other run file and the declaration. Its process
ID must agree with PresentMon and the thread profile. It may claim only that capture
completed; it must leave technical validation and every human/release decision
false.

### PRE and POST snapshots

Both archive snapshots carry the exact predeclared canonical archive inventory
identity, launcher binding, and inner executable binding. The validator recomputes
the live archive before and after all other checks.

Both host snapshots record:

- GPU name, UUID, driver version, instantaneous power draw, and power limit;
- active Windows power-scheme GUID and name;
- AC-line state and battery-saver state;
- storage volume identity and free bytes.

The GPU, driver, active power scheme, and storage volume must remain the same; AC
must be online, battery saver off, and at least 20 GiB free both times.

### NVIDIA one-second series

`NvidiaSensorCapture.json` binds the exact `nvidia-smi.exe` identity/version,
query-field list, one-second interval, UTC window, and CSV identity.
`NvidiaSensors.csv` uses the exact declared columns, contiguous zero-based indexes,
one GPU/driver identity, and UTC timestamps. Samples nominally occur every 1000 ms;
every gap must remain between 500 and 1500 ms, coverage must be at least 98% of the
declared one-second count, the series must span the declared duration, and it must
stay inside the run window. Temperature, utilization, clocks, draw, limit, and
performance state must be finite and policy-bounded. The current provisional
temperature ceiling is 87 °C.

### PresentMon evidence

`PresentMonCapture.json` binds the PresentMon executable identity/version, target
process ID, UTC window, and `PresentMon.csv`. The CSV has this exact normalized
column order:

```text
Application,ProcessID,TimeInSeconds,MsBetweenPresents,PresentMode,Dropped
```

The independent validator requires strict monotonic chronology, agreement between
successive time deltas and `MsBetweenPresents`, at least ten presents per declared
second, no continuity gap over 1000 ms, at most 1% dropped rows, the exact Shipping
application name, and the same process ID as the run manifest.

If the installed PresentMon version emits a wider raw schema, retain its raw output
outside this exact run directory and produce this six-column lossless projection for
validation. Do not manufacture or interpolate rows. The bound PresentMon executable
and original raw output should be retained alongside the review packet even though
only the exact projection belongs in this run directory.

### GPU/game/render/RHI profile

`ThreadProfile.json` binds all of these to the same process and capture window:

- an actual `UnrealInsights.utrace` containing `cpu`, `frame`, and `gpu` channels;
- the Unreal Insights executable identity/version used for analysis;
- `UnrealInsightsTiming.csv` exported from that trace;
- recomputed summaries for exactly `GPU`, `GameThread`, `RenderThread`, and
  `RHIThread`.

The timing CSV order is:

```text
ThreadRole,ThreadName,StartSeconds,DurationMilliseconds
```

Every role needs at least 60 positive-duration events spanning at least the declared
60-second profile window. The trace must carry an Unreal trace magic header, be at
least 1 MiB, and pass conservative nonzero/byte-diversity checks so an arbitrary,
empty, or zero-filled placeholder cannot satisfy the contract. The validator does
not claim that these structural checks are a cryptographic proof that the export
came from the trace.

## Validate read-only, then optionally publish externally

First validate without `--output`. This writes nothing:

```powershell
python Scripts\validate_dg_session19_sustained_performance.py `
  --declaration $Declaration `
  --run-root "$ExternalRoot\Runs\<certification-run-uuid>" `
  --archive $Archive `
  --three-hole-receipt $PriorReceipt `
  --three-hole-run-root $PriorRun `
  --candidate-id $Candidate
```

After a read-only pass, an optional append-only technical receipt can be written to
a new path under the selected external root and outside the run directory:

```powershell
python Scripts\validate_dg_session19_sustained_performance.py `
  --declaration $Declaration `
  --run-root "$ExternalRoot\Runs\<certification-run-uuid>" `
  --archive $Archive `
  --three-hole-receipt $PriorReceipt `
  --three-hole-run-root $PriorRun `
  --candidate-id $Candidate `
  --external-root $ExternalRoot `
  --output "$ExternalRoot\Receipts\<new-technical-receipt-name>.json"
```

An existing output is never overwritten. A failure never emits a PASS receipt.

## Safe scaffold checks

These tests use temporary synthetic files only. They do not launch the game, run a
soak, attach PresentMon, or start Unreal Insights.

```powershell
python Scripts\generate_dg_session19_sustained_performance_declaration.py --self-test
python Scripts\validate_dg_session19_sustained_performance.py --self-test
python Scripts\run_dg_session19_sustained_performance.py --self-test
python -m py_compile `
  Scripts\generate_dg_session19_sustained_performance_declaration.py `
  Scripts\validate_dg_session19_sustained_performance.py `
  Scripts\run_dg_session19_sustained_performance.py
```
