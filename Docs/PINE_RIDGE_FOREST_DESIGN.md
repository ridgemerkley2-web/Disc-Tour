# Pine Ridge Persistent Forest Design

## Course-scale rule

Pine Ridge is one persistent course property, not three disposable hole maps. All three holes, terrain beds, forests, fixtures, wind zones, Gallery Lake, baskets, cameras, and presentation actors coexist in one Unreal world. Advancing the round changes the active hole and moves play to the next tee; it must not destroy and rebuild the course.

`Data/PineRidgeCourse.json` owns the stable shared-world placement:

| Hole | World origin (cm) | Yaw | Forest pattern |
|---|---:|---:|---|
| 1 Pine Ridge Opening | 0, 0, 0 | 0° | `OpeningBroadTreeLine` |
| 2 Needle Gate | 14,500, 4,000, 100 | 8° | `NeedleCanopyCompression` |
| 3 Gallery Lake | 36,000, 10,500, -250 | -12° | `GalleryLakeFrame` |

The placement forms one roughly 580 m-long property. `Docs/PINE_RIDGE_GROUND_GRASS.md` now owns the continuous presentation height field and the two graded visual connector trails between the previous green and next tee. These trails do not change hole-local competitive geometry or introduce new gameplay collision.

## DGPT design references

These are design references, not geometry to copy.

- [Brewster Ridge / Green Mountain Championship](https://www.dgpt.com/event/2026-green-mountain-championship/): tight wooded fairways reward line hitting and scrambling. Pine Ridge Opening adapts this as a broad, legible tree-lined opening that tightens downrange and releases around the green.
- [Northwood Black / Ledgestone](https://www.dgpt.com/news/how-to-watch-2023-ledgestone-open/): the course is entirely wooded and forces gap control and navigation. [DGPT's broadcast engineering account](https://www.dgpt.com/news/dgn-expands-ledgestone-open-coverage-thanks-to-new-cellular-technology-investments-cbrs/) also describes its extremely dense woods and underbrush. Needle Gate therefore has the highest canopy target, shallowest route-edge buffer, and strongest visual compression.
- [Idlewild](https://www.dgpt.com/event/2025-lws-open-at-idlewild/): long technical holes combine dense woods, water, elevation, scrambling, and shot shaping. Gallery Lake adapts that relationship by using dense wooded framing while keeping the lake carry, right-shore bailout, landing zone, and green visible.
- [DGPT Dream 18, Hole 4](https://www.dgpt.com/news/dgpt-dream-18-course-what-is-the-best-hole-4-on-tour/): Brewster's defined wooded fairways and Idlewild's pond/sloped-green pressure reinforce the principle that forest density should clarify strategic choices instead of becoming random visual noise.

## Runtime density contract

At the current High foliage tier:

| Hole | Decorative HISM trees | Authoritative trunks | Intent |
|---|---:|---:|---|
| Opening | 476 | 12 | generous first gap, dense outer walls, green release |
| Needle Gate | 580 | 18 | narrowest visual corridor and deepest compression |
| Gallery Lake | 518 | 14 | lake amphitheater frame with open carry and bailout reads |
| Total | 1,574 | 44 | one persistent course |

Decorative trees use deterministic HISM placement and are always `NoCollision`, generate no overlaps, and never affect navigation. The 44 authored trunk fixtures remain the only tree-contact authority. Low/Medium/High may alter visual density and culling but may not change trunks, routes, lies, penalties, physics, or scoring.

The current High tier culls decorative firs from 87.5 m through 260 m. Decorative instances closest to the
playable corridor retain movable canopy shadows; deep-forest components keep the same visual population but
do not duplicate the dynamic shadow pass. All 44 visuals associated with authoritative trunks remain on the
shadowed near-tree components. This is presentation partitioning only and does not change instance transforms
or collision authority.

The generator must reject candidates inside:

- every Primary, Risk/reward, and Bailout corridor plus the hole-specific edge buffer;
- tee and green clearings;
- Gallery Lake and its shore buffer;
- authored camera pockets;
- spectator/gallery lanes;
- minimum spacing around another accepted decorative instance.

## Asset and realism boundary

The current forest uses the three imported CC0 Poly Haven Fir Sapling variants with larger mature-canopy scaling
and a smaller understory subset. Each source retains full close LOD0, then uses fixed 20%, 5%, and 1.25% triangle
reductions at 0.22, 0.085, and 0.030 screen-size thresholds. The resulting chains are:

| Variant | LOD0 | LOD1 | LOD2 | LOD3 |
|---|---:|---:|---:|---:|
| Fir A | 157,402 | 31,481 | 7,871 | 1,967 |
| Fir B | 150,876 | 30,176 | 7,544 | 1,886 |
| Fir C | 124,743 | 24,949 | 6,238 | 1,559 |

The shoreline boulder likewise retains 123,976 close triangles and reduces to 22,316 / 4,338 triangles.
The reproducible import pipeline reapplies these chains, preventing a reimport from silently restoring one-LOD
source scans. This is adequate for the dense-forest layout and current Omen performance gates, but it is not
the final mature-tree asset set.

[Poly Haven Fir Tree 01](https://polyhaven.com/a/fir_tree_01) is a suitable CC0 visual reference with three tall fir variants, but its source mesh is approximately eight million triangles. Do not add it directly to the runtime forest. A production import requires deliberate LOD/Nanite preparation, reduced texture tiers, wind strategy, shadow/cull profiling, and a fresh three-hole packaged Omen gate before it can replace or supplement the current instances.

## Acceptance

Run `-DenseForestSmokeTest`. It must find three persistent hole actors, all three named forest patterns, the exact tier target for each foliage actor, 44 valid trunk proxies, and collision-invariant decorative components.

Then capture real-D3D tee views for all three holes. Human review must confirm:

- the forest reads as continuous mass rather than isolated saplings;
- every intended first-shot gap remains visible;
- Needle Gate is visibly the most compressed hole;
- Gallery Lake remains readable as a water hole;
- no tree occupies the tee, green, water, camera, or gallery exclusion;
- close branches remain detailed while distant trees transition without visibly hollowing the corridor;
- the scene does not claim final mature-conifer fidelity until the replacement asset pass is complete.
