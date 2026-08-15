# Performance Budgets

## Purpose

Production art must not silently erase the low-spec development path or change gameplay. These presentation budgets are an early warning contract for the HP Omen development target; they do not feed the flight solver, collision, scoring, wind, or rules.

## Provisional Omen gate

- Target average frame time: 16.67 ms / 60 FPS.
- Warning threshold: P95 frame time above 22.0 ms.
- Failure threshold: P95 frame time above 33.34 ms.
- Hitch: any frame at or above 50.0 ms.
- Failure threshold: more than 1% hitches in the bounded sample window.
- Process physical-memory warning: above 3.5 GiB.
- Process physical-memory failure: above 4.0 GiB.
- Warm-up: at least 120 render-frame samples.
- Rolling window: at most 600 samples.

The runtime HUD reports average FPS, P95 frame time, physical process memory, and PASS/WARN/FAIL. P95 and average calculations are refreshed twice per second; physical memory is sampled at the same interval. The bounded tracker does no physics work and its summary sorting is not performed every frame.

## Repeatable rendered capture

`-PerformanceCaptureSeconds=N` applies the source-controlled
`OmenGameplay1080pHighFoliageV1` preset before the persistent course is built. It uses
100% resolution, quality level 2 for view distance/AA/shadows/GI/reflections/post/textures/effects/shading,
and High foliage population. The capture then traverses the active hole's authored flyover for a
10-second residency warm-up before resetting telemetry and sampling the requested duration.

Launch the packaged Development build with an explicit course/hole, exact backbuffer, and capture duration:

```powershell
DiscGolfTour.exe -Course=PineRidge -Hole=1 -PerformanceCaptureSeconds=30 `
  -ResX=1920 -ResY=1080 -ForceRes -RenderOffscreen -dx12 -unattended
```

The duration is clamped to 5-300 seconds. The process writes a schema-v2 timestamped JSON artifact plus
`Saved/PerformanceCaptures/LatestPerformance.json`, logs the final state, and exits non-zero unless the
sample passes. Local practice saves are ignored during this gate. Schema v2 records and validates the
capture profile, continuous authored camera route, warm-up, requested duration, rendered/NullRHI state,
actual RHI adapter, resolution, runtime mode, and every scalability level. The independent validator rejects
NullRHI, resized windows, the wrong GPU/RHI identity, the wrong preset, or a missing continuous flyover.

Capture all three authored holes at the intended resolution and scalability preset after each substantial terrain, foliage, lighting, character, UI, or effects integration. A gray-box or NullRHI result is useful for code regression but is not production-art evidence.

## Current packaged Omen result

The August 13, 2026 packaged Development gate ran on the NVIDIA GeForce RTX 5060 Laptop GPU at exact
1920x1080 D3D12. Each hole used the same persistent property, High foliage population, 10-second warm-up,
30-second continuous authored flyover, and 600-frame rolling window:

| Hole | Average FPS | P95 | Max | Hitches | Process memory | Result |
|---|---:|---:|---:|---:|---:|---|
| Pine Ridge Opening | 112.6 | 11.10 ms | 12.23 ms | 0 | 1.27 GiB | PASS |
| Needle Gate | 95.9 | 15.63 ms | 18.08 ms | 0 | 1.23 GiB | PASS |
| Gallery Lake | 120.2 | 9.77 ms | 11.49 ms | 0 | 1.24 GiB | PASS |

Evidence is preserved as `Saved/PerformanceCaptures/PackagedFinal_Hole1_1080p.json` through
`PackagedFinal_Hole3_1080p.json`, with matching logs under `Saved/Logs/Performance_Packaged_Final_*`.

## Interpretation limits

The lightweight monitor observes game tick delta and process resident memory. It is a regression tripwire,
not a replacement for Unreal Insights, `stat unit`, `stat gpu`, RenderDoc, thread/GPU split timing, thermal-soak
testing, or release-package certification. GPU, game-thread, render-thread, RHI-thread, streaming-pool, and
asset-residency budgets remain a later profiling expansion.
