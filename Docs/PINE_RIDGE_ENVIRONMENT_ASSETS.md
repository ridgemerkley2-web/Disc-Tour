# Pine Ridge Environment Asset Annex

## License and provenance

The current Pine Ridge environment source set comes from Poly Haven under CC0. Commercial use and modification are allowed without attribution, but this project still retains upstream provenance for auditability. The upstream machine-readable authority is `SourceArt/PineRidge/PolyHaven/asset_manifest.json`, which records the selected URL, local path, byte size, and MD5 checksum for every downloaded file. The accepted derived-runtime authority is `SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json`, SHA-256 `13ACC125F5B442BE75DE405DBD76D43AA9381436624E12BDDBA3C7BA4F90D858`.

Session 18 binds the exact 35 source files to 55 derived Unreal packages and separately classifies five project-original packages, forming a complete 60-package partition of `/Game/Presentation/Course/PineRidge`. Independent semantic validation passed all 60 packages; its report SHA-256 is `1C3D6960E342B58143D55B42D701B18D19E17BFE8D118862BE38A93A15B74035`.

Focused `DiscGolfTour.Session18.*` automation passes 3/3, and full `DiscGolfTour.*` automation passes 252/252. Fresh archive `C:\DGTour_Packages\S18_PolyHavenProvenance_63624166-aa24-49d0-a89f-74a125a36be2\Windows` contains all 55 derived and five excluded project-original package identities in 54 files / 1,918,214,604 bytes. Its sorted tab-line manifest SHA-256 is `0877C04FF8911F0903077F8C68093BF8A783FB6939538D2BFD1E2888D35B1870`, and its inner executable SHA-256 is `6051C15D6DCF14D4EE92B34215B79B7AA7BCADC5D8F6F5CA8AB24807E89D4346`. The packaged Pine Ridge play smoke exits 0 with 1,968 samples, authored camera, local wind, and ground contact. Source-controlled evidence is retained under `Evidence/Session18`. This is identity inclusion evidence only. Session 18 closes only the Poly Haven derived-runtime receipt blocker; whole staged-package provenance, Fab receipts, final art, and twelve other public-release blockers remain, leaving exactly thirteen and no release-ready claim.

The reproducible fetch step is `Scripts/download-pine-ridge-cc0-assets.ps1`. It must remain checksum-valid and idempotent. Do not hand-replace a downloaded source file without updating the scripted selection and regenerating the manifest.

## Selected assets

| Poly Haven asset | Resolution | Pine Ridge use |
|---|---:|---|
| Forest Ground 01 | 2K | rough and forest-floor PBR surface material |
| Leafy Grass | 2K | fairway/light-rough PBR surface material |
| Grass Path 2 | 2K | tee/path PBR surface material |
| Fir Sapling | 1K FBX/PBR | current collision-free forest mass, understory, and authored-tree presentation; not final mature-conifer art |
| Boulder 01 | 1K FBX/PBR | component-fitted rock fixture presentation |
| Shrub 04 | 1K FBX/PBR/alpha | deterministic masked dense-grass HISM presentation |
| Weathered Planks | 1K PBR | material input for the original Pine Ridge sign assembly |

Upstream pages:

- https://polyhaven.com/a/boulder_01
- https://polyhaven.com/a/shrub_04
- https://polyhaven.com/a/weathered_planks
- https://polyhaven.com/license

## Import and ownership rules

Run `Scripts/import-pine-ridge-assets.py` with `UnrealEditor.exe -ExecutePythonScript=...` after a clean download. UE 5.8 commandlet mode does not expose `StaticMeshEditorSubsystem`, so the pipeline rejects `UnrealEditor-Cmd.exe` before reimporting anything. The full-editor import creates course materials and mesh assets under `/Game/Presentation/Course/PineRidge`, reapplies the fixed fir four-LOD and boulder three-LOD reduction chains, writes the transient `Saved/PineRidgeAssetImportReceipt.json`, and writes the source-controlled post-save `SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json`. Source validation requires the exact seven asset IDs and 35 manifest files before an engine build.

After import, run `Scripts/validate-pine-ridge-provenance-semantics.py` through the full Editor and run `Scripts/validate_dg_session18_poly_haven_provenance.py` plus its self-test. Reject any changed source/runtime hash, missing or extra package, duplicate classification, dependency/import-source mismatch, altered texture/LOD setting, or weakened release ledger. A fresh package must contain all 55 derived and five project-original identities, but that inclusion check remains distinct from whole-stage provenance.

Imported art is presentation only:

- PBR ground and procedural terrain presentation never own lies or collision.
- Fir meshes never replace deterministic tree-trunk proxies.
- Boulder art is fit inside the existing typed rock proxy.
- Shrub instances are contained by the dense-grass overlap fixture.
- The original sign assembly covers but never replaces the typed sign proxy.
- The generated Gallery Lake material and procedural mesh never replace the authored water hazard, lie, collision, or penalty authority.
- Every fixture visual uses `NoCollision`, ignores every response channel, generates no overlap events, and cannot affect navigation.
- Decorative forest placement follows `Docs/PINE_RIDGE_FOREST_DESIGN.md`: all strategy corridors, tee/green clearings, Gallery Lake, camera pockets, spectator lanes, and minimum neighbor spacing are exclusions.
- A foliage actor is accepted only when all three fir variants load and its complete deterministic instance set configures. Partial failure leaves authoritative trunk proxies visible.
- The current scaled saplings establish course composition only. Their imported LOD0 detail is preserved up close while deterministic reductions supply 20%/5%/1.25% fir levels; the boulder uses 18%/3.5% reductions. Any mature-conifer replacement needs prepared LOD/Nanite meshes, texture/wind tiers, shadow/cull profiling, and a rendered performance capture before runtime adoption.
- The shared course ground uses Forest Ground 01, Leafy Grass, and Grass Path 2 through generated PBR materials. `M_PineRidgeFairwayBlend` and `M_PineRidgeTrailBlend` consume procedural vertex alpha to feather complete PBR sets; `M_PineRidgeGrassBlade` and `M_PineRidgeLeafLitter` use original generated alpha masks, per-instance shade variation, and stable two-sided masked responses. Grass adds root-anchored wind and component-level species tint/strength parameters.
- Course grass is separate from authored dense-grass gameplay fixtures. Three grass and two litter HISM components distribute only along route/connector edges, align to the course normal, enforce slope/protected-area limits, use quality-tier culling, and have no collision. Exact counts and limits are documented in `Docs/PINE_RIDGE_GROUND_GRASS.md`.
- Gallery Lake reuses the scanned boulder, terrain/fir material families, and engine cylinder only as collision-free HISM dressing: fitted rocks, thin textured reed stems, and rounded bark deadfall. Connector wear reuses Grass Path 2 through a dedicated material instance. All placement remains deterministic and outside the route/fixture exclusion contract.

Low, Medium, and High may change visual density and culling. They may not change fixture transforms, collision types, solver coefficients, route data, rules, or the `PineRidgeCompetitiveV2_Fixtures` identity.

## QA contract

After any source, import, material, mesh, pivot, scale, alpha-mask, or fixture-presentation change:

1. Run source validation and the independent reference-flight envelope.
2. Build `DiscGolfTourEditor Win64 Development`.
3. Run all `DiscGolfTour.` automation tests.
4. Run `-CourseSmokeTest`, `-FixtureCollisionSmokeTest`, and `-RegressionSuiteSmokeTest`.
5. Independently validate the latest fixture and trajectory artifacts.
6. Capture `-FixturePresentationGallery -VisualQAScreenshot=<name>` with real D3D12 and inspect rock relief, shrub masking, sign legibility, scale, and pivot placement.
7. After any lake material, mesh, basin, or shoreline change, run `-GalleryLakeWaterSmokeTest` and capture `-GalleryLakeWaterVisualQA -VisualQAScreenshot=<name>` with real D3D12; reject material fallback, shader compilation errors, lost hazard identity, or changed hazard collision.
8. After any forest density, exclusion, asset, or transform change, run `-DenseForestSmokeTest` and capture all three final tee views with real D3D12. Reject wrong pattern/count identity, any decorative collision, missing trunks, blocked route reads, forest inside water/camera/gallery clearings, or a disconnected per-hole map architecture.
9. After any shared-ground, fairway, trail, shoreline, grass/litter, material, height, or exclusion change, run `-GroundGrassSmokeTest`, capture all three tee views with real rendering, and capture the Gallery Lake bank view. Reject per-hole terrain actors, incorrect geometry counts, visible blockout sidewalls, water intrusion, hard biome seams, material fallback, dressing in protected areas, or any competitive collision change.
10. After any meaningful terrain, forest, ground-cover, water, lighting, camera, character, UI, or effects cost increase, build the packaged Development target and repeat the exact 1920x1080 D3D12 Omen capture for all three holes. Validate every schema-v2 artifact; Needle Gate's 15.63 ms P95 is the current worst-case comparison point.

The automated gates prove collision invariance and deterministic implementation. They do not replace human review of visual realism, contact plausibility, route readability, or sustained thermal/GPU profiling on the target Omen hardware.
