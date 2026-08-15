# Research Sources

Primary/authoritative references used to shape this project:

## Unreal Engine
- Unreal Engine 5.8 documentation: https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-documentation
- Gameplay Framework: https://dev.epicgames.com/documentation/unreal-engine/gameplay-framework-in-unreal-engine
- Programming with C++: https://dev.epicgames.com/documentation/unreal-engine/programming-with-cplusplus-in-unreal-engine
- Enhanced Input: https://dev.epicgames.com/documentation/unreal-engine/enhanced-input-in-unreal-engine
- Data Assets: https://dev.epicgames.com/documentation/unreal-engine/data-assets-in-unreal-engine
- Asset Management: https://dev.epicgames.com/documentation/unreal-engine/asset-management-in-unreal-engine
- Scalability Reference: https://dev.epicgames.com/documentation/unreal-engine/scalability-reference-for-unreal-engine

## Disc golf equipment/course design
- PDGA Course Development: https://www.pdga.com/course-development
- PDGA Course Design: https://www.pdga.com/course-development/design
- PDGA Technical Standards: https://www.pdga.com/technical-standards
- PDGA Approved Discs: https://www.pdga.com/technical-standards/equipment-certification/discs
- PDGA Par Guidelines: https://www.pdga.com/documents/par-guidelines

## Flight dynamics
- UC Davis Biosport — Frisbee Flight Simulation and Throw Biomechanics: https://research.engineering.ucdavis.edu/biosport/sample-page/test-page-1/frisbee-flight-simulation-and-throw-biomechanics/
- UC Davis Biosport publications page, including Hummel/Hubbard flying-disc work: https://research.engineering.ucdavis.edu/biosport/sample-page/publications/

## Environment assets

- Poly Haven license (CC0): https://polyhaven.com/license
- Boulder 01: https://polyhaven.com/a/boulder_01
- Shrub 04: https://polyhaven.com/a/shrub_04
- Weathered Planks: https://polyhaven.com/a/weathered_planks

The download script, exact selected files, upstream URLs, sizes, and MD5 checksums are recorded in `SourceArt/PineRidge/PolyHaven/asset_manifest.json`. See `Docs/PINE_RIDGE_ENVIRONMENT_ASSETS.md` for the runtime ownership and import policy.

## Open-source references evaluated
- FrisPy (MIT): https://github.com/tmcclintock/FrisPy — useful as an independent SI-unit trajectory/calibration reference for aerodynamic coefficients and equations of motion. Do not substitute its adaptive SciPy solver directly for the game's fixed-step runtime solver.
- Disc-Golf-VR (MIT): https://github.com/Zertigan/Disc-Golf-VR — an older Unreal Engine 4/VR prototype; potentially useful for interaction ideas, but not a production code dependency for the Unreal 5.8 project.
- Project_Disc: https://github.com/Tinos-Vafias/Project_Disc — Unreal Engine 5 prototype with no repository license found during review; do not copy or integrate its code/assets unless licensing is clarified.

## Interpretation policy
Research references inform architecture and calibration. They are not a license to label current fallback coefficients as experimentally validated modern golf-disc coefficients. Keep source claims precise.
