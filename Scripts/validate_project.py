#!/usr/bin/env python3
from pathlib import Path
import ast
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
required = [
    "DiscGolfTour.uproject",
    "AGENTS.md",
    "Source/DiscGolfTour/DiscGolfTour.Build.cs",
    "Source/DiscGolfTour/DiscFlightComponent.cpp",
    "Source/DiscGolfTour/DiscReplayActor.cpp",
    "Source/DiscGolfTour/DiscReplayActor.h",
    "Source/DiscGolfTour/DiscGolfPresentationMath.h",
    "Source/DiscGolfTour/DiscBroadcastCameraDirector.cpp",
    "Source/DiscGolfTour/DiscBroadcastCameraDirector.h",
    "Source/DiscGolfTour/DiscGolfBroadcastCameraMath.h",
    "Source/DiscGolfTour/DiscEquipmentDataAssets.cpp",
    "Source/DiscGolfTour/DiscEquipmentDataAssets.h",
    "Source/DiscGolfTour/DiscCatalogSubsystem.cpp",
    "Source/DiscGolfTour/DiscCatalogSubsystem.h",
    "Source/DiscGolfTour/DiscGolfCourseRules.cpp",
    "Source/DiscGolfTour/DiscGolfCourseRules.h",
    "Source/DiscGolfTour/DiscGolfCourseSurfaceActor.cpp",
    "Source/DiscGolfTour/DiscGolfCourseSurfaceActor.h",
    "Source/DiscGolfTour/DiscGolfCourseDefinition.cpp",
    "Source/DiscGolfTour/DiscGolfCourseDefinition.h",
    "Source/DiscGolfTour/DiscGolfRouteTelemetry.cpp",
    "Source/DiscGolfTour/DiscGolfRouteTelemetry.h",
    "Source/DiscGolfTour/DiscGolfCoursePresentationDefinition.cpp",
    "Source/DiscGolfTour/DiscGolfCoursePresentationDefinition.h",
    "Source/DiscGolfTour/DiscGolfFoliagePresentationActor.cpp",
    "Source/DiscGolfTour/DiscGolfFoliagePresentationActor.h",
    "Source/DiscGolfTour/DiscGolfWaterPresentationActor.cpp",
    "Source/DiscGolfTour/DiscGolfWaterPresentationActor.h",
    "Source/DiscGolfTour/DiscGolferPresentationComponent.cpp",
    "Source/DiscGolfTour/DiscGolferPresentationComponent.h",
    "Source/DiscGolfTour/DiscGolfRoundState.cpp",
    "Source/DiscGolfTour/DiscGolfRoundState.h",
    "Source/DiscGolfTour/DiscGolfCourseFeatureActor.cpp",
    "Source/DiscGolfTour/DiscGolfCourseFeatureActor.h",
    "Source/DiscGolfTour/DiscGolfWindZoneActor.cpp",
    "Source/DiscGolfTour/DiscGolfWindZoneActor.h",
    "Source/DiscGolfTour/DiscGolfFlyoverRouteActor.cpp",
    "Source/DiscGolfTour/DiscGolfFlyoverRouteActor.h",
    "Source/DiscGolfTour/Tests/DiscGolfCourseDefinitionTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfRouteTelemetryTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfRoundStateTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfPresentationTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfBroadcastCameraTests.cpp",
    "Source/DiscGolfTour/Tests/DiscCatalogTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfCourseRulesTests.cpp",
    "Source/DiscGolfTour/DiscGolfInputConfig.cpp",
    "Source/DiscGolfTour/DiscGolfInputConfig.h",
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
    "Docs/ARCHITECTURE.md",
    "Scripts/validate_presentation_capture.py",
    "Scripts/validate_route_telemetry.py",
    "Scripts/generate_disc_data_assets.py",
    "Scripts/download-pine-ridge-cc0-assets.ps1",
    "Scripts/import-pine-ridge-assets.py",
    "Data/PineRidgeHole1.json",
    "Data/PineRidgeHole2.json",
    "Data/PineRidgeHole3.json",
    "Data/PineRidgeCourse.json",
    "Data/PineRidgePresentation.json",
]

errors = []
for rel in required:
    if not (ROOT / rel).exists():
        errors.append(f"missing required file: {rel}")

try:
    binding_runner_path = ROOT / "Scripts/run-environment-asset-binding-workflow.py"
    binding_runner_source = binding_runner_path.read_text(encoding="utf-8")
    binding_runner_tree = ast.parse(binding_runner_source)
    literal_lists = {}
    function_names = {
        node.name for node in binding_runner_tree.body
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
    }
    for node in binding_runner_tree.body:
        if (isinstance(node, ast.Assign) and len(node.targets) == 1
                and isinstance(node.targets[0], ast.Name)
                and isinstance(node.value, (ast.List, ast.Tuple))):
            literal_lists[node.targets[0].id] = ast.literal_eval(node.value)

    planned_mesh_roots = set(literal_lists.get("PLANNED_MESH_VENDOR_ROOTS", []))
    imported_mesh_roots = set(
        literal_lists.get("VERIFIED_IMPORTED_MESH_VENDOR_ROOTS", []))
    manual_roots = set(literal_lists.get("MANUAL_INTEGRATION_VENDOR_ROOTS", []))
    expected_imported_mesh_roots = {
        "/Game/PN_interactiveSpruceForest",
        "/Game/Stump_Scanned",
    }
    expected_manual_roots = {
        "/Game/Environment/Vendors/ProjectNature/ForestLandscapeMaterials01",
        "/Game/Environment/Vendors/tharlevfx/WaterMaterials",
        "/Game/WaterMaterials",
    }
    if not expected_imported_mesh_roots.issubset(imported_mesh_roots):
        errors.append("environment binder runner is missing verified imported mesh roots")
    if not expected_manual_roots.issubset(manual_roots):
        errors.append("environment binder runner is missing manual material/water roots")
    if (planned_mesh_roots | imported_mesh_roots) & manual_roots:
        errors.append("manual material/water roots leaked into environment mesh scanning")
    if "is_manual_integration_root" not in function_names:
        errors.append("environment binder runner does not guard manual-only root descendants")
    if "ENVIRONMENT_VENDOR_ROOT_MANUAL_ONLY_REJECTED" not in binding_runner_source:
        errors.append("environment binder runner does not report rejected manual-only mesh roots")
    if "apply_approved_bindings" in binding_runner_source.casefold():
        errors.append("non-mutating environment binder runner invokes asset approval")
except Exception as exc:
    errors.append(f"invalid environment asset binding runner configuration: {exc}")

try:
    project = json.loads((ROOT / "DiscGolfTour.uproject").read_text(encoding="utf-8"))
    modules = {m.get("Name") for m in project.get("Modules", [])}
    if "DiscGolfTour" not in modules:
        errors.append("DiscGolfTour module missing from .uproject")
except Exception as exc:
    errors.append(f"invalid .uproject JSON: {exc}")

try:
    course = json.loads((ROOT / "Data/PineRidgeHole1.json").read_text(encoding="utf-8"))
    if course.get("schema") != "disc_golf_hole_blockout" or course.get("schemaVersion") != 1:
        errors.append("Pine Ridge course schema identity/version is invalid")
    expected_counts = {
        "surfaces": 11,
        "trees": 12,
        "landingZones": 2,
        "cameraAnchors": 3,
        "spectatorBoundaries": 3,
        "windZones": 2,
        "flyoverPointsCm": 6,
    }
    for field, expected_count in expected_counts.items():
        if len(course.get(field, [])) != expected_count:
            errors.append(f"Pine Ridge {field} count is not {expected_count}")
    ids = []
    for field in ("surfaces", "landingZones", "cameraAnchors", "spectatorBoundaries", "windZones"):
        ids.extend(item.get("id") for item in course.get(field, []))
    if None in ids or len(ids) != len(set(ids)):
        errors.append("Pine Ridge feature IDs must be present and globally unique")
    if {anchor.get("mode") for anchor in course.get("cameraAnchors", [])} != {"Launch", "Fairway", "Finish"}:
        errors.append("Pine Ridge camera anchors must cover launch, fairway, and finish")
except Exception as exc:
    errors.append(f"invalid Pine Ridge course JSON: {exc}")

try:
    manifest = json.loads((ROOT / "Data/PineRidgeCourse.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != "disc_golf_course_manifest" or manifest.get("schemaVersion") != 1:
        errors.append("Pine Ridge manifest schema identity/version is invalid")
    holes = manifest.get("holes", [])
    if [entry.get("holeNumber") for entry in holes] != [1, 2, 3]:
        errors.append("Pine Ridge manifest must contain contiguous holes 1-3")
    if [entry.get("definitionFile") for entry in holes] != [
        "Data/PineRidgeHole1.json", "Data/PineRidgeHole2.json", "Data/PineRidgeHole3.json"
    ]:
        errors.append("Pine Ridge manifest hole paths are not stable")
    expected_origins = [(0, 0, 0), (14500, 4000, 100), (36000, 10500, -250)]
    actual_origins = [tuple(entry.get("worldOriginCm", {}).get(axis) for axis in ("x", "y", "z"))
                      for entry in holes]
    if actual_origins != expected_origins:
        errors.append("Pine Ridge persistent-world hole origins are not the approved contiguous layout")
    if [entry.get("worldYawDeg") for entry in holes] != [0, 8, -12]:
        errors.append("Pine Ridge persistent-world hole yaws are not stable")

    expected = {
        1: {"par": 3, "surfaces": 11, "trees": 12, "flyoverPointsCm": 6},
        2: {"par": 4, "surfaces": 12, "trees": 18, "flyoverPointsCm": 7},
        3: {"par": 4, "surfaces": 12, "trees": 14, "flyoverPointsCm": 7},
    }
    total_par = 0
    for hole_number, counts in expected.items():
        definition = json.loads((ROOT / f"Data/PineRidgeHole{hole_number}.json").read_text(encoding="utf-8"))
        if definition.get("courseId") != manifest.get("courseId") or definition.get("layoutId") != manifest.get("layoutId"):
            errors.append(f"Pine Ridge hole {hole_number} identity does not match the manifest")
        if definition.get("holeNumber") != hole_number or definition.get("par") != counts["par"]:
            errors.append(f"Pine Ridge hole {hole_number} metadata is invalid")
        total_par += definition.get("par", 0)
        for field in ("surfaces", "trees", "flyoverPointsCm"):
            if len(definition.get(field, [])) != counts[field]:
                errors.append(f"Pine Ridge hole {hole_number} {field} count is not {counts[field]}")
        for field, minimum in (("landingZones", 2), ("cameraAnchors", 3), ("spectatorBoundaries", 3), ("windZones", 2)):
            if len(definition.get(field, [])) < minimum:
                errors.append(f"Pine Ridge hole {hole_number} {field} coverage is incomplete")
        feature_ids = []
        for field in ("surfaces", "landingZones", "cameraAnchors", "spectatorBoundaries", "windZones"):
            feature_ids.extend(item.get("id") for item in definition.get(field, []))
        if None in feature_ids or len(feature_ids) != len(set(feature_ids)):
                errors.append(f"Pine Ridge hole {hole_number} feature IDs must be globally unique")
        if hole_number == 2:
            routes = definition.get("shotRoutes", [])
            if ({route.get("id") for route in routes}
                    != {"NeedlePlacement", "LateCrosswindAttack", "LeftPitchOut"}):
                errors.append("Needle Gate telemetry route IDs are not stable")
    if total_par != 11:
        errors.append("Pine Ridge three-hole layout par is not 11")
except Exception as exc:
    errors.append(f"invalid Pine Ridge course manifest or hole set: {exc}")

try:
    presentation = json.loads((ROOT / "Data/PineRidgePresentation.json").read_text(encoding="utf-8"))
    if presentation.get("schema") != "disc_golf_course_presentation" or presentation.get("schemaVersion") != 1:
        errors.append("Pine Ridge presentation schema identity/version is invalid")
    if presentation.get("courseId") != "PineRidgeChampionship" or presentation.get("layoutId") != "Championship":
        errors.append("Pine Ridge presentation identity does not match the course")
    if not presentation.get("collisionInvariantAcrossQuality"):
        errors.append("Pine Ridge presentation quality must preserve competitive collision")
    collision_profile = presentation.get("collisionProfileId")
    quality_tiers = presentation.get("qualityTiers", [])
    if {tier.get("id") for tier in quality_tiers} != {"Low", "Medium", "High"}:
        errors.append("Pine Ridge presentation must define Low, Medium, and High tiers")
    for tier in quality_tiers:
        if tier.get("affectsCollision") or tier.get("collisionProfileId") != collision_profile:
            errors.append(f"Pine Ridge {tier.get('id')} quality tier changes competitive collision")
    presentation_holes = presentation.get("holes", [])
    if [hole.get("holeNumber") for hole in presentation_holes] != [1, 2, 3]:
        errors.append("Pine Ridge presentation must cover contiguous holes 1-3")
    if [hole.get("forestReferenceId") for hole in presentation_holes] != [
        "BrewsterRidgeTreeLine", "NorthwoodBlackCompression", "IdlewildLakeFrame"
    ]:
        errors.append("Pine Ridge forest-design references are incomplete or reordered")
    if any(hole.get("forestDepthCm", 0) < 4000 or hole.get("canopyDensityScale", 0) < 1.1
           for hole in presentation_holes):
        errors.append("Every Pine Ridge hole must retain the dense-forest depth/density floor")
    for path in presentation.get("assets", {}).values():
        if not isinstance(path, str) or not path.startswith("/Game/") or " " in path:
            errors.append("Pine Ridge presentation asset paths must be stable /Game paths")
except Exception as exc:
    errors.append(f"invalid Pine Ridge presentation JSON: {exc}")

input_config = (ROOT / "Config/DefaultInput.ini").read_text(encoding="utf-8")
if "+ActionMappings=" in input_config or "+AxisMappings=" in input_config:
    errors.append("legacy action/axis mappings returned to DefaultInput.ini")
if "EnhancedInput.EnhancedPlayerInput" not in input_config or "EnhancedInput.EnhancedInputComponent" not in input_config:
    errors.append("Enhanced Input classes missing from DefaultInput.ini")

game_config = (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8")
for asset_type in ("DiscMold", "DiscPlastic"):
    if f'PrimaryAssetType="{asset_type}"' not in game_config or "CookRule=AlwaysCook" not in game_config:
        errors.append(f"{asset_type} Asset Manager scan/cook rule missing from DefaultGame.ini")
if 'DirectoriesToAlwaysCook=(Path="/Game/Data/Discs")' not in game_config:
    errors.append("disc data packaging directory is not explicitly staged for cooking")
if 'DirectoriesToAlwaysCook=(Path="/Game/Presentation/Course/PineRidge")' not in game_config:
    errors.append("Pine Ridge presentation directory is not explicitly staged for cooking")

expected_assets = {
    "Content/Data/Discs/Molds": 5,
    "Content/Data/Discs/Plastics": 3,
}
for rel, expected_count in expected_assets.items():
    folder = ROOT / rel
    actual = len(list(folder.glob("*.uasset"))) if folder.exists() else 0
    if actual != expected_count:
        errors.append(f"{rel} contains {actual} assets; expected {expected_count}")

environment_assets = [
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeTerrain.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeGrassBlade.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeFairwayBlend.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeTrailBlend.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeLeafLitter.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeTrailWear.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeShoreReeds.uasset",
    "Content/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_grass_card_alpha.uasset",
    "Content/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_litter_card_alpha.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_GalleryLakeWater.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeFairway.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeForestFloor.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgePath.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_Boulder01.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_Shrub04.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeSignWood.uasset",
    "Content/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_a.uasset",
    "Content/Presentation/Course/PineRidge/Fixtures/PolyHaven/Boulder01/boulder_01_1k.uasset",
    "Content/Presentation/Course/PineRidge/Fixtures/PolyHaven/Shrub04/shrub_04_1k.uasset",
]
for rel in environment_assets:
    if not (ROOT / rel).exists():
        errors.append(f"missing imported Pine Ridge environment asset: {rel}")

try:
    source_manifest = json.loads((
        ROOT / "SourceArt/PineRidge/PolyHaven/asset_manifest.json"
    ).read_text(encoding="utf-8-sig"))
    source_files = source_manifest.get("files", [])
    required_source_assets = {
        "forrest_ground_01", "leafy_grass", "grass_path_2", "fir_sapling",
        "boulder_01", "shrub_04", "weathered_planks",
    }
    if (source_manifest.get("license") != "CC0-1.0" or len(source_files) != 35
            or {entry.get("assetId") for entry in source_files} != required_source_assets):
        errors.append("Pine Ridge CC0 source manifest is incomplete")
    for entry in source_files:
        if entry.get("license") != "CC0-1.0" or not entry.get("sourceUrl", "").startswith("https://"):
            errors.append("Pine Ridge source manifest contains an unlicensed or unstable entry")
            break
        if not (ROOT / entry.get("relativePath", "")).exists():
            errors.append(f"missing Pine Ridge source file: {entry.get('relativePath')}")
except Exception as exc:
    errors.append(f"invalid Pine Ridge source manifest: {exc}")

# Lightweight source hygiene that can run without Unreal installed.
for path in (ROOT / "Source").rglob("*.cpp"):
    text = path.read_text(encoding="utf-8")
    if "TODO(CRITICAL)" in text:
        errors.append(f"critical TODO left in {path.relative_to(ROOT)}")

if errors:
    print("Project validation FAILED")
    for e in errors:
        print(" -", e)
    sys.exit(1)

print("Project validation OK")
print(f"Root: {ROOT}")
print(f"C++ files: {len(list((ROOT / 'Source').rglob('*.cpp')))}")
print(f"Headers: {len(list((ROOT / 'Source').rglob('*.h')))}")
