# AGENTS.md — Disc Golf Tour

## Mission
Build a realistic, scalable disc-golf simulation in Unreal Engine 5.8 with original tour/broadcast presentation. Prioritize **throw feel, flight credibility, course playability, performance, and maintainable systems** before final art polish.

## Read first
Before making substantial changes, read:
1. `README.md`
2. `Docs/ARCHITECTURE.md`
3. `Docs/PHYSICS_CALIBRATION.md`
4. `ROADMAP.md`
5. The relevant file in `Tasks/`

If you are working from a clone rather than the authoring tree, read
`Docs/FRESH_CHECKOUT.md` first. It explains which validator results a checkout
without `Saved/` cannot pass, and which paths accepted policy requires to stay
absent.

`Legacy/BrowserPilot_v0.3` is a read-only behavioral reference. Do not turn it back into the production game.

## Non-negotiable architecture rules
- Keep the production game in Unreal C++ with Blueprint/content layers on top.
- The disc flight solver remains a **custom fixed-step simulation**, not Unreal rigid-body flight with arbitrary forces added per render frame.
- Use SI units inside the flight solver. Unreal centimeters are converted only at the world-motion boundary.
- Do not make disc flight depend on frame rate.
- RHBH/RHFH behavior must mirror through spin/handedness rather than maintaining two unrelated physics implementations.
- High-speed turn and low-speed fade are opposing stability effects. Never collapse them into the same-sign canned curve.
- Mold/plastic/wear behavior should become data-driven. Avoid hard-coding new molds into gameplay classes once Primary Data Assets/Data Registry are introduced.
- Preserve a low-spec development path. Do not require Lumen, dense Nanite foliage, MetaHumans, or large marketplace packs for basic gameplay testing.
- Gameplay systems must work without final art.
- Never copy EA Sports/PGA Tour proprietary UI, assets, commentary, branding, or course data. We want comparable broadcast quality and production value, not infringement.

## Physics discipline
- Treat current aerodynamic coefficients as calibration seeds.
- Do not claim the fallback profiles physically describe a named real disc.
- When changing flight behavior, update `Docs/PHYSICS_CALIBRATION.md` with what changed and why.
- Prefer parameter changes backed by measured trajectories over hand-tuned special cases.
- Add deterministic or automated reference tests when practical.
- Keep wind in meters/second.
- Expose telemetry needed to compare simulated and real throws: time, position, velocity, spin, angle of attack, bank/attitude when available.

## Course discipline
- Course geometry and gameplay routes should follow the design principles documented in `Docs/COURSE_DESIGN.md`.
- Safety rules matter for real course references even though the game can exaggerate spectacle.
- Every championship hole should have a clear intended scoring decision, not just length plus trees.
- Keep tee, basket, OB/hazard, landing zones, spectator areas, and camera anchors as explicit data/actors.

## Performance discipline
Target a smooth development experience on a mid/high-tier HP Omen before final visual quality:
- Avoid per-frame world searches from the flight solver.
- Avoid spawning hundreds of individual Actors for production foliage; the runtime primitive course is development-only.
- Profile before adding expensive effects.
- Build graphics presets. Gameplay and physics must not change when visual quality changes.
- Final target can use Nanite/Lumen where justified, but they are not baseline gameplay dependencies.

## Coding conventions
- Follow Unreal naming/macros and Epic-style C++ conventions.
- Prefer small focused ActorComponents/subsystems over god classes.
- Keep public API minimal.
- Use `TObjectPtr` for reflected UObject references.
- Use forward declarations in headers where possible.
- Avoid raw `new/delete` for UObjects/Actors.
- Avoid tick when a system can be event-driven. Fixed-step flight and gusting wind are intentional tick users.
- Comment **why**, especially for coordinate/sign conventions and physics, not obvious syntax.

## Verification after every code change
Run every source-only check at once, which reports all of them rather than
stopping at the first failure:

```text
python Scripts/run_source_checks.py
```

On a checkout without `Saved/` and `_BuildKit/` the evidence-bound gates cannot
pass; `Docs/FRESH_CHECKOUT.md` explains which and why. The individual checks:

```text
python Scripts/validate_project.py
python Scripts/reference_flight_check.py
```

Do not run these under `-O`, `-OO` or `PYTHONOPTIMIZE`: the reference check enforces
through `assert` and refuses to run with assertions disabled rather than report a
false pass. After changing the flight, release or ground models, also run the
mutation harness, which proves the reference check still rejects known-bad solver
behaviour:

```text
python Scripts/mutation_test_reference_flight.py
```

The reference check is a mirror of the C++ solver, so it can pass while the two
drift apart. After editing either side, confirm they still state the same
conventions:

```text
python Scripts/validate_solver_convention_parity.py
```

When Unreal is not installed there is no compiler feedback, so run the source-only
C++ lint after editing any header or source. It checks the mechanical rules a
compiler would otherwise catch -- generated.h ordering, GENERATED_BODY presence,
own-header-first includes, the runtime/DeveloperTool module boundary, TObjectPtr on
reflected pointers, and raw UObject allocation:

```text
python Scripts/lint_cpp_conventions.py
```

If Unreal Engine is available locally, also run an editor build. On Windows PowerShell:

```powershell
./Scripts/build-unreal.ps1
```

If the script cannot find UE automatically, pass the engine root:

```powershell
./Scripts/build-unreal.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
```

When the editor build succeeds, run Unreal Automation tests containing `DiscGolfTour.` when feasible.

## Git behavior
If the directory is a Git repository:
- Check `git status` before edits.
- Keep changes scoped to the current task.
- Do not discard user changes.
- Run validation/build before finalizing.
- Summarize files changed, tests run, and remaining risks.

## Binary assets
Do not generate large binary `.uasset`/`.umap` changes unless the task explicitly requires editor content. When binary content becomes necessary:
- Git LFS must be enabled.
- Keep source art or generation notes where licensing allows.
- Prefer original or properly licensed assets.

## Definition of done for a gameplay task
A gameplay task is not done merely because code was written. It should:
1. Compile in the targeted Unreal version when an engine is available.
2. Pass lightweight repo validation.
3. Be testable in the primitive dev hole or a dedicated test map.
4. Preserve or improve frame-rate-independent behavior.
5. Document material physics/design changes.
