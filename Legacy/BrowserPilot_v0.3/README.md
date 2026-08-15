# Pine Ridge Disc Golf — Local Pilot 0.3

A dependency-free browser vertical slice for testing the gameplay direction before the full Unreal build.

## Windows — easiest
1. Extract the ZIP.
2. Double-click `run-local.bat`.
3. Your browser opens to `http://localhost:8765`.
4. Leave the server window open while you play.

No Node, npm, Unreal Engine, or Python is required on Windows; the included PowerShell server is the primary launcher.

## Mac / Linux
Open a terminal in this folder and run:

```bash
./run-local.sh
```

Then visit `http://localhost:8765`.

## Throwing
A throw uses a two-tap release:
1. Press **SPACE** to start the release meter.
2. Press **SPACE** again when the needle is in the green. The narrow center is a Perfect release.

The player now performs a visible throw animation before the disc releases. Early/late timing changes aim, speed and spin. Rough lies narrow the clean timing window and reduce available power. Inside about 105 ft the game automatically enters a putting setup.

## Disc bag
0.3 expands the bag to five fictional molds:
- **1 — Apex:** 12 / 5 / -1 / 3 distance driver
- **2 — Vector:** 9 / 5 / -2 / 2 control driver
- **3 — Line:** 7 / 5 / -1 / 2 fairway driver
- **4 — Compass:** 5 / 5 / 0 / 1 midrange
- **5 — Touch:** 2 / 3 / 0 / 1 putt & approach

Each mold can be used in three fictional plastics:
- **Crystal:** slightly more stable with stronger skips
- **Tour:** balanced grip, glide and ground play
- **Base:** slightly less stable, grippier, and much less skippy

Plastic selection changes the actual flight/ground calculations; it is not cosmetic only.

## Controls
- A / D — aim
- W / S — power
- Q / E — hyzer / anhyzer
- F — RHBH / RHFH
- 1–5 — select disc
- Space — start timing / release
- P — replay the previous completed shot
- C — cycle Broadcast / Chase / Tee / Pin cameras
- R — reset current hole

## New in 0.3
- Visible player with backhand/forehand throw animation and follow-through
- Five-disc fictional bag instead of three generic prototypes
- Crystal / Tour / Base plastics with stability, glide, grip and skip modifiers
- Gusting/variable wind instead of one fixed wind vector
- Fairway, light rough, deep rough, Circle 1 and Circle 2 lie classification
- Rough-lie power and timing penalties
- Surface-dependent skips, slides and edge rolls
- Instant recorded shot replay with a broadcast replay bug
- Richer course rendering with distant mountains, mixed tree shapes, brush, rocks and water detail
- Tournament ropes, spectators and basket hole signage on the showcase hole
- Expanded telemetry with surface/lie information
- Improved disc information panel and broadcast lower thirds

## What is intentionally still prototype-level
This is still a browser gameplay pilot. The terrain and player are procedural canvas art rather than final 3D assets, there is no character creator or career mode yet, and the physics still need calibration against measured real-world throws before being considered simulation-grade.

The next major step after the 0.3 feel/presentation test is the Unreal vertical slice: one highly detailed championship hole, a real 3D player rig/animation set, proper foliage/terrain, audio, and the same disc-flight concepts moved into the Unreal physics/gameplay layer.
