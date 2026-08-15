# HP Omen Development and Performance Setup

The Omen is both the primary development machine and the current minimum measured gameplay target. It is not the cinematic ceiling. Production art must preserve the packaged gameplay gate below before it is accepted into the persistent course.

## Recorded hardware floor

Measured August 13, 2026:

- CPU: AMD Ryzen 7 8745HX;
- GPU: NVIDIA GeForce RTX 5060 Laptop GPU with 7,899 MB dedicated VRAM reported by Unreal D3D12;
- system RAM: 15.05 GiB;
- internal display: 2560x1600;
- free system SSD capacity at the gate: approximately 289 GiB;
- Unreal Engine: 5.8.1;
- gameplay gate: packaged Development build, D3D12, exact 1920x1080, `OmenGameplay1080pHighFoliageV1`.

The Windows video-controller API may report only 4 GiB for this adapter. Use Unreal's D3D12 adapter line in the packaged log as the VRAM authority.

## Install
1. Epic Games Launcher + Unreal Engine 5.8.
2. Visual Studio 2022.
3. In Visual Studio Installer, enable **Game development with C++** plus the Windows SDK and Unreal/Visual Studio integration components offered for your installation.
4. Python 3 for the source-only validation/flight-envelope scripts.
5. Git and Git LFS before the project starts accumulating binary Unreal assets.

## Storage
Unreal projects grow quickly because of Derived Data Cache, intermediate builds, source art, and cooked output. Keep substantial free SSD capacity. Generated folders are excluded from Git.

## Development graphics philosophy

Physics and gameplay should be tested at stable frame rates. The accepted gameplay preset uses 100% resolution, quality level 2 for view distance, anti-aliasing, shadows, global illumination, reflections, post-processing, textures, effects, and shading, plus quality level 3 for the complete High foliage population.

Preset direction:
- **Development/Low:** cheap lighting, reduced foliage, simple spectators.
- **Medium:** representative gameplay quality.
- **High:** complete gameplay foliage and the representative packaged Omen target.
- **Broadcast/Cinematic:** replay/photo mode only; not the baseline gameplay requirement.

## Acceptance command

Run every authored hole from `Saved/PackagedPerformanceGate/Windows/DiscGolfTour.exe`:

```powershell
DiscGolfTour.exe -Course=PineRidge -Hole=1 -PerformanceCaptureSeconds=30 `
  -ResX=1920 -ResY=1080 -ForceRes -RenderOffscreen -dx12 -unattended
```

Repeat with `-Hole=2` and `-Hole=3`. Each run includes a 10-second warm-up and one continuous authored flyover before writing schema-v2 evidence. `Scripts/validate_performance_capture.py` must pass every artifact. NullRHI, editor, resized-window, wrong-preset, and single-hole results are diagnostic only.

Current packaged results are 112.6 / 95.9 / 120.2 average FPS and 11.10 / 15.63 / 9.77 ms P95 for Holes 1/2/3, with zero hitches. Needle Gate is the current worst-case guardrail.

Re-run the full three-hole gate after any meaningful terrain, foliage, water, lighting, camera, character, UI, or effects cost increase. Details and thresholds are authoritative in `Docs/PERFORMANCE_BUDGETS.md`.

## Recheck before release

Record GPU driver, free SSD space, Windows power mode, wall-power state, and thermals before a release certification pass. The current numbers are a development regression baseline, not a substitute for a sustained thermal soak or Unreal Insights GPU/thread profiling.
