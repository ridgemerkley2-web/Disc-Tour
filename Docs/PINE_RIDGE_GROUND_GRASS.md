# Pine Ridge Persistent Ground, Biomes, and Grass

## Course-scale contract

Pine Ridge owns one continuous presentation ground for the complete three-hole property. It is not allowed to generate a separate terrain bed when a hole becomes active. The shared actor is built once after all three world-space hole definitions have been assembled and persists through the entire round.

The current runtime geometry at High foliage quality is:

| Layer | Count | Purpose |
|---|---:|---|
| Shared terrain height field | 37,810 triangles | continuous forest floor across the property |
| Fairway biome ribbons | 1,152 triangles | three five-rail route-following PBR blends |
| Connector biome trails | 512 triangles | feathered links from each green to the next tee |
| Connector wear | 36 patches / 288 triangles | irregular PBR foot-traffic breakup on the two visual trails |
| Gallery Lake shoreline | 576 triangles | graded water/soil/forest transition band |
| Gallery Lake dressing | 42 rocks / 96 reed clusters / 384 stems / 14 deadfall | three deterministic HISM accent families outside competitive corridors |
| Grass clusters | 7,150 | deterministic, terrain-normal-aligned tall-edge vegetation |
| Grass cards | 14,300 HISM instances / 28,600 triangles | two masked cards per cluster across three species variants |
| Forest-floor litter | 1,980 clusters / 3,960 HISM-card triangles | two masked leaf-and-needle variants |

The height field samples the three Primary routes and the two connector splines, blends restrained low-frequency relief away from play, and grades Gallery Lake into the same course surface. Fairways and trails use five rails with zero-alpha outer rails and a full-strength center, so their PBR biome changes feather into the forest floor without changing gameplay geometry. Gallery Lake uses four elliptical rails to rise from below the water surface through exposed soil and back into the course grade.

## Materials

The generated material set uses the existing CC0 Poly Haven source textures:

- Forest Ground 01 for the continuous off-fairway floor;
- Leafy Grass for groomed fairway ribbons;
- Grass Path 2 for connector trails;
- `MI_PineRidgeTrailWear`, a darker, tighter-tiled Grass Path instance for localized connector scuffing;
- `MI_Boulder01`, `MI_PineRidgeShoreReeds`, and `MI_FirSaplingBranches` for fitted shoreline rocks, thin textured reed stems, and rounded deadfall;
- `M_PineRidgeFairwayBlend` and `M_PineRidgeTrailBlend`, generated PBR materials that use procedural-mesh vertex alpha to blend base color, normal, roughness, and ambient occlusion;
- `M_PineRidgeGrassBlade`, a generated two-sided masked PBR material with per-instance shade variation, species tint parameters, root-anchored world-position-offset wind, and an original generated alpha silhouette;
- `M_PineRidgeLeafLitter`, a generated two-sided masked rough material with per-instance shade variation and a separate original generated alpha silhouette for small forest-floor leaf and needle cards.

The import script remains the only authority for rebuilding these generated Unreal materials and their alpha-mask textures. The reproducible mask sources are checked in under `SourceArt/PineRidge/Generated` as Base64; the script materializes their PNGs before import. Color, masking, and wind tuning must be reviewed under the real D3D12 runtime lights, not accepted from scalar values alone.

## Grass placement

Grass density scales through the existing Low/Medium/High presentation tiers. The current targets are 2,275, 4,875, and 7,150 clusters. Placement is deterministic, concentrated beyond fairway and connector edges, and aligned to the sampled course normal. Grass rejects slopes steeper than 32 degrees. The three HISM species components vary tint, card proportions, and wind response without introducing per-instance collision. It also rejects candidates inside:

- tee and basket clearings;
- Gallery Lake and its shoreline buffer;
- authored camera pockets;
- spectator/gallery lanes.

Course grass is presentation only. Dense-grass gameplay fixtures remain separate authored overlap volumes with their existing speed and spin response. Ground-cover instance culling scales with the presentation tier: Low uses 67.5-180 m, Medium 90-240 m, and High 112.5-300 m start/end distances.

Forest-floor litter scales with the same presentation tier. High quality targets 1,980 small deterministic cards split between leaf and needle HISM components beyond playable route edges. It follows the sampled terrain normal, rejects slopes steeper than 38 degrees, and uses the same protected-area rejection as grass. The litter is visual breakup only; it is not a lie surface or a collision fixture.

## Trail wear and shoreline dressing

Each connector receives 18 deterministic eight-triangle wear patches. Their irregular radial footprints sit slightly above the connector material, follow the shared course height/normal, and use the existing CC0 path PBR set. They communicate foot traffic without creating a new surface type or navigation authority.

Gallery Lake distributes 42 component-fitted scanned rocks, 96 four-stem reed clusters, and 14 rounded bark-covered deadfall pieces between the graded bank and forest edge. Placement rejects slopes above 34 degrees and remains at least 1,050 cm from every authored Primary, Risk/reward, and Bailout route; it also excludes tee/green clearings, camera pockets, spectator lanes, and all existing collision fixtures. These accents do not resize or replace the water hazard.

## Gameplay authority

The shared ground, biome ribbons, connector trails, wear patches, shoreline, and all eight grass/litter/shoreline HISM components use `NoCollision`, emit no overlaps, and cannot affect navigation. The existing authored course-surface actors remain present and retain all trace, lie, surface-response, out-of-bounds, hazard, water, and penalty authority. Their fairway/rough/dirt blockout render meshes are hidden only after the complete shared presentation configures successfully.

The connector trails are visually continuous but do not yet add player-navigation collision beyond the sealed competitive surfaces. A future production Landscape or virtual-heightfield pass may reconcile that ownership only through an explicit gameplay-geometry revision and full regression gate.

## Acceptance

Run `-GroundGrassSmokeTest` at High foliage quality. It must find exactly one ready ground actor serving all three persistent holes, 37,810 terrain triangles, 1,152 fairway triangles, 512 connector triangles, 36 wear patches / 288 triangles, 576 shoreline triangles, 42 rocks, 96 reed clusters / 384 stems, 14 deadfall pieces, at least 1,050 cm of route clearance, 7,150 grass clusters / 14,300 HISM cards / 28,600 triangles across three species, 1,980 litter clusters / 3,960 triangles across two HISM variants, a 112.5-300 m cull range, a sampled maximum slope no greater than 38 degrees, and collision-invariant components.

Then capture real-D3D tee views for all three holes. Reject:

- disconnected terrain beds or visible blockout sidewalls;
- missing connector or fairway ribbons;
- grass inside water, tee, green, camera, or gallery exclusions;
- upright cards that visibly ignore the supporting terrain slope, or grass placed above the 32-degree limit;
- a hard fairway/trail material seam or a broken/exposed lake bank;
- rectangular wear decals, oversized reeds, box-shaped deadfall, shoreline accents in a strategy corridor, or dressing that obscures the lake-carry read;
- a blocked first-shot corridor;
- rectangular card silhouettes, synchronized wind, material fallback, shader compile errors, or bright untextured grass;
- any change to competitive collision, flight, lies, or scoring.

## Remaining realism work

This is the first slope-aware instanced ground/shoreline composition baseline, not the final Landscape/PCG tier.
The packaged three-hole 1080p Omen gate now passes with this complete population. Remaining work includes
authored multi-blade/species atlases or meshes with richer normals, camera-distance dither/fade, broader
forest-floor microsurface breakup, and more irregular erosion geometry. Any density, shadow, or replacement-
mesh increase must repeat the packaged performance gate.
