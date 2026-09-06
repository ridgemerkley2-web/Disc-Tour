# Session 19 clean-room runtime-foundation receipt

Recorded: 2026-08-25T02:28:46Z

## Scope and method

`Source/DiscGolfRuntimeFoundation` is a new project-owned Shipping compatibility
module. Its short C++ type names and callable surface were derived from project
consumer includes/member use, project-owned tests and contracts, UnrealHeaderTool
and compiler diagnostics, and the current v0.5 Shipping requirements. The
implementation lane did not inspect or transplant the contents of either legacy
`DiscGolfCharacterFramework` source tree while authoring the replacement.

After implementation, a read-only mechanical audit compared the completed module
with both legacy trees. The comparison program emitted aggregate counts, hashes,
and filenames only; it did not emit legacy source lines. One implementation was
restructured after the first aggregate result, without exposing legacy text, and
the final audit below was then run.

This receipt documents engineering process and reproducible comparison results.
It is not a legal opinion, a license determination, or clearance to redistribute
any legacy source or serialized asset.

## Source identity

Tree digests cover sorted `.h`, `.cpp`, and `.cs` files. Each tree row is the
forward-slash relative path, a NUL byte, and the lowercase SHA-256 file digest;
rows are joined with LF and SHA-256 hashed.

| Tree | Files | SHA-256 |
|---|---:|---|
| `Source/DiscGolfRuntimeFoundation` | 46 | `f250fefec7b2792aeec99ee417b62c705f0f61ccc559f5bcedfcd40f7f95e735` |
| `Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework` | 92 | `b76b4e6b05d7d853f32ae3629b2a27eee0f850ce1f3a2e60a2251c4238b95843` |
| `_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework` | 92 | `eba0dd0cbb5e7eab91dad810b0d5c40f62a7c9802810408576aa7c3eb0d20f76` |

## Mechanical exact-similarity audit

The audit collapses internal whitespace, trims each source line, and excludes
blank/short lines (under 24 characters), comments, preprocessor directives,
brace-only lines, and reflection-macro-only lines. It then measures exact file
hashes, unique normalized line intersection, exact five-line windows within a
single file, and the longest contiguous run of filtered lines.

The replacement has 380 filtered lines and 342 unique filtered lines.

| Compared legacy tree | Exact files | Exact unique lines | Exact per-file five-line windows | Longest filtered run |
|---|---:|---:|---:|---:|
| Installed plugin | 0 | 116/342 (33.92%) | 8/246 | 7 |
| Build-kit copy | 0 | 102/342 (29.82%) | 8/246 | 7 |

For both comparisons, the longest run is between the two
`Public/DiscGolfPlayabilityTypes.h` files and has SHA-256
`64825e5605bee865040fbdb141a033cc1763fe070e6da2aaf0ced2049db3f98a`.
The required reflected compatibility surface and common Unreal declarations are
expected contributors to exact-line overlap, but the audit does not classify or
excuse those matches and makes no conclusion about substantial similarity.

## Shipping compile checkpoint

Command:

```text
C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat DiscGolfTour Win64 Shipping C:\DGTour\DiscGolfTour.uproject -WaitMutex -NoHotReload
```

Result: succeeded; linked
`Binaries/Win64/DiscGolfTour-Win64-Shipping.exe`.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `Binaries/Win64/DiscGolfTour-Win64-Shipping.exe` | 172296192 | `8b362e7242866af94343a216cdf50146c797175bf08d91071f1129919314e27f` |

A raw binary fixed-string scan found zero occurrences of
`DiscGolfCharacterFramework` and one occurrence of `DiscGolfRuntimeFoundation`.
This establishes a compile/link checkpoint only; it is not package, cook, load,
or playthrough evidence.

## Outstanding serialized migration

The legacy script package is still imported by 45 `.uasset` files: four animation
assets, three avatar assets, the customization catalog plus 17 items, the outfit
catalog plus 15 items, the default character data asset, the master control rig,
and two test profiles. These must be independently recreated or legitimately
migrated and resaved against the project-owned module before the legacy plugin can
be quarantined from all targets.

Live non-binary bindings also remain in `Config/DefaultGame.ini` (the avatar
backend primary-asset class), `Config/DG_RuntimeCookManifest.json`, its binary
`Content/DiscGolf/Cook/DA_DG_RuntimeCookManifest.uasset` mirror, and editor utility
animation/notify paths. A clean cook/package and runtime asset-load audit remain
release blockers.

