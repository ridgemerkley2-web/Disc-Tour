# Pine Ridge source art

## Production sources

The Pine Ridge environment uses the following Poly Haven assets as its first production-quality source set:

- [Forest Ground 01](https://polyhaven.com/a/forrest_ground_01) for pine-needle hazard, forest floor, and deep rough.
- [Leafy Grass](https://polyhaven.com/a/leafy_grass) for fairway and light rough.
- [Grass Path 2](https://polyhaven.com/a/grass_path_2) for dirt paths, worn tee surrounds, and exposed ground.
- [Fir Sapling](https://polyhaven.com/a/fir_sapling) for non-colliding understory and authored-tree presentation prototypes.

Poly Haven publishes these assets under CC0 1.0. Commercial use, modification, and redistribution are permitted without attribution. Attribution remains in this project for provenance. See the [Poly Haven asset license](https://polyhaven.com/license).

Run `Scripts/download-pine-ridge-cc0-assets.ps1` to download the chosen Omen-baseline files and verify their published MD5 hashes. The script writes a machine-readable `PolyHaven/asset_manifest.json` containing every source URL, file size, and checksum.

## Selection policy

- Ground uses 2K diffuse, DirectX normal, roughness, and ambient-occlusion maps. The source pages also provide up to 8K, but 2K is the baseline until measured captures justify more texture memory.
- Fir sapling uses the 1K FBX and 1K texture set. Visual instances must have collision disabled; the authored Pine Ridge trunk proxies remain the competitive collision authority.
- Source art is imported into `/Game/Presentation/Course/PineRidge`. Generated Unreal assets may be rebuilt from this folder using `Scripts/import_pine-ridge-assets.py`.
- [Project Nature temperate Vegetation: Spruce Forest](https://www.fab.com/listings/f8044501-17a2-498f-b198-5f1bc71ee87a) is the preferred mature-tree candidate: Fab lists 15 growth-stage meshes, high/low variants, shader wind, and landscape impostors. It is free but must be added through the project Epic account before evaluation/import.
- [Project Nature temperate Vegetation: Foliage Collection](https://www.fab.com/listings/6a5ae8db-d80f-4b23-b276-87da390cfe56) is the preferred ground-cover candidate. Do not copy either Fab pack into source control until acquisition and its applicable Fab Standard License are recorded in the asset manifest.
