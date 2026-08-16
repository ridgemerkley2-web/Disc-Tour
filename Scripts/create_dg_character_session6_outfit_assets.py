"""Import and author the owned Session 6 outfit proxy asset catalog in UE 5.8.

Run only after the Blender generator and source validator pass. The first run
imports exactly nine skeletal and six static FBXs, creates one shared material,
15 UDiscGolfOutfitItem assets, and DA_DG_OutfitCatalog. Once every output exists,
reruns are strict validation-only and request no package saves.

This script never edits the BuildKit donor, the accepted master mesh/skeleton,
rigs, animation assets, creator widget, gameplay code, or any vendor content.
"""

from __future__ import annotations

import hashlib
import json
import re
import runpy
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "DiscGolf" / "Outfits" / "Proxy"
SPEC_PATH = SOURCE_ROOT / "proxy_outfit_catalog_spec.json"
MANIFEST_PATH = SOURCE_ROOT / "proxy_outfit_source_manifest.json"
SOURCE_VALIDATOR = PROJECT_ROOT / "Scripts" / "validate_dg_character_session6_proxy_source.py"
ASSET_VALIDATOR = PROJECT_ROOT / "Scripts" / "validate_dg_character_session6_outfit_assets.py"
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session6OutfitAssetCreation.json"

METADATA = {
    "DG_Session": "6",
    "DG_ContentStatus": "NON_PRODUCTION_PROXY",
    "DG_ShippingStatus": "DO_NOT_SHIP",
    "DG_BrandId": "dg_generic",
    "DG_BrandStatus": "GENERIC_UNBRANDED",
    "DG_SourceKind": "PROJECT_LOCAL_GENERATED_BLOCKOUT",
    "DG_ProductionApproved": "false",
}

# Frozen accepted packages. Importing a follower against SKEL_DG_Master must not
# silently save or reauthor any one of these assets.
PROTECTED_ASSETS = {
    "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset":
        "5C461476D6877DFBE3E6DF08FF48CDF5BB940BC8C883D3E3F43058331DCC186F",
    "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset":
        "D40A0C4FE4BFE100A553910E01CCB5C00ACD9D1C927D4C1387F25894541493EA",
    "Content/DiscGolf/Rigs/IK_DG_Master.uasset":
        "13D29A1D4B4F1E2A6F010D95E052D21A1E974A6789BBFE85AC5E4E260EE72958",
    "Content/DiscGolf/Rigs/CR_DG_Master.uasset":
        "21BAA6E6C0C885F4F18BFF077FFCE3043916051DFF6675317004B09B2D9E10CF",
    "Content/DiscGolf/Animation/ABP_DG_Player.uasset":
        "833454B7FC2F5F759795FB641295E1F41DBF0F4F1AB888476FE222D2DFB9D379",
    "Content/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.uasset":
        "EA53E0460B958FFA7C8BC1DCACB5A4E6F1C6ABBACE9F177C68A1017C6783ECE6",
    "Content/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.uasset":
        "6BCD1C3256668D7F041FE6D33B6910052EE77FA4739A1EF4E60E689A787A8AF8",
    "Content/DiscGolf/UI/WBP_DG_CharacterCreator.uasset":
        "69D1879FA25B2480EC3A6FED461954E7E824DA58BFCE33A143F2B7CBB405F1B0",
    "Content/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.uasset":
        "A0EF3A40A6E1A25E96C1E7B0344681A40D22A672B650708B82ABD22DAEEB6A9B",
    "Content/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.uasset":
        "8F1421E28C286F42EE6881669648DF40A7EF0E4FCAC944198D1C9F17FF0842DF",
    "Content/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.uasset":
        "A57F8CF320B67FB48213517127F0D1AC35F1DA79D518DFBE7FE589B6E347327D",
}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _protected_hashes() -> dict[str, str]:
    result = {}
    for relative, expected in PROTECTED_ASSETS.items():
        path = PROJECT_ROOT / relative
        if not path.is_file():
            raise RuntimeError(f"Protected accepted package is missing: {path}")
        actual = _sha256(path)
        if actual != expected:
            raise RuntimeError(
                f"Protected accepted package hash changed before Session 6 authoring: "
                f"{relative} {actual} != {expected}")
        result[relative] = actual
    return result


def _load_spec() -> dict:
    return json.loads(SPEC_PATH.read_text(encoding="utf-8"))


def _package_path(object_or_package_path: str) -> str:
    leaf = object_or_package_path.rsplit("/", 1)[-1]
    return object_or_package_path.rsplit(".", 1)[0] if "." in leaf else object_or_package_path


def _object_path(object_or_package_path: str) -> str:
    package = _package_path(object_or_package_path)
    return f"{package}.{package.rsplit('/', 1)[-1]}"


def _asset_name(object_or_package_path: str) -> str:
    return _package_path(object_or_package_path).rsplit("/", 1)[-1]


def _asset_folder(object_or_package_path: str) -> str:
    return _package_path(object_or_package_path).rsplit("/", 1)[0]


def _uasset_path(object_or_package_path: str) -> Path:
    package = _package_path(object_or_package_path)
    if not package.startswith("/Game/"):
        raise RuntimeError(f"Session 6 asset is outside /Game: {package}")
    return PROJECT_ROOT / "Content" / (package[len("/Game/"):] + ".uasset")


def _load_asset(path: str, expected_type):
    asset = unreal.EditorAssetLibrary.load_asset(_package_path(path))
    if not isinstance(asset, expected_type):
        raise RuntimeError(
            f"Expected {expected_type.__name__} at {_object_path(path)}, found {asset}")
    return asset


def _ensure_folder(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"Could not create content folder: {path}")


def _mark_owned(asset, item_id: str = "") -> None:
    for key, value in METADATA.items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, value)
    unreal.EditorAssetLibrary.set_metadata_tag(
        asset, "DG_SourceManifest", "SourceArt/DiscGolf/Outfits/Proxy/proxy_outfit_source_manifest.json")
    if item_id:
        unreal.EditorAssetLibrary.set_metadata_tag(asset, "DG_ItemId", item_id)


def _require_owned(asset, item_id: str = "") -> None:
    for key, value in METADATA.items():
        actual = unreal.EditorAssetLibrary.get_metadata_tag(asset, key)
        if str(actual) != value:
            raise RuntimeError(
                f"Refusing to modify non-owned/stale Session 6 asset {asset.get_path_name()}: "
                f"metadata {key}={actual!r}, expected {value!r}")
    if item_id:
        actual = unreal.EditorAssetLibrary.get_metadata_tag(asset, "DG_ItemId")
        if str(actual) != item_id:
            raise RuntimeError(
                f"Refusing ItemId ownership mismatch {asset.get_path_name()}: {actual} != {item_id}")


def _enum_member(enum_type, display_name: str):
    token = re.sub(r"(?<!^)(?=[A-Z])", "_", display_name).upper()
    try:
        return getattr(enum_type, token)
    except AttributeError as error:
        raise RuntimeError(f"Could not resolve {enum_type.__name__}.{token}") from error


def _all_output_paths(spec: dict) -> list[str]:
    return (
        [spec["material_asset"], spec["catalog_asset"]]
        + [item["mesh_asset"] for item in spec["items"]]
        + [item["data_asset"] for item in spec["items"]]
    )


def _run_asset_validator() -> dict:
    namespace = runpy.run_path(str(ASSET_VALIDATOR), run_name="dg_session6_asset_validation")
    result = namespace["validate"]()
    if result.get("status") != "PASS_NO_DISK_MUTATION":
        raise RuntimeError("Session 6 strict asset validation failed: " + "; ".join(result["errors"]))
    return result


def _reference_pose_signature(skeleton) -> tuple:
    """Capture the accepted local reference pose before follower import."""
    pose = skeleton.get_reference_pose()
    result = []
    for bone_name in pose.get_bone_names():
        transform = pose.get_bone_pose(bone_name, unreal.AnimPoseSpaces.LOCAL)
        translation = transform.translation
        rotation = transform.rotation
        scale = transform.scale3d
        result.append((
            str(bone_name),
            float(translation.x), float(translation.y), float(translation.z),
            float(rotation.x), float(rotation.y), float(rotation.z), float(rotation.w),
            float(scale.x), float(scale.y), float(scale.z),
        ))
    return tuple(result)


def _import_options(item: dict, skeleton):
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("override_full_name", True)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("create_physics_asset", False)
    if item["mesh_kind"] == "Skeletal":
        options.set_editor_property("import_as_skeletal", True)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        options.set_editor_property("skeleton", skeleton)
        data = options.get_editor_property("skeletal_mesh_import_data")
        data.set_editor_property("convert_scene", True)
        data.set_editor_property("convert_scene_unit", True)
        data.set_editor_property("import_meshes_in_bone_hierarchy", False)
        data.set_editor_property("preserve_smoothing_groups", True)
        data.set_editor_property("update_skeleton_reference_pose", False)
        data.set_editor_property("use_t0_as_ref_pose", False)
        data.set_editor_property("import_morph_targets", False)
        data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    else:
        options.set_editor_property("import_as_skeletal", False)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_STATIC_MESH)
        data = options.get_editor_property("static_mesh_import_data")
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("generate_lightmap_u_vs", False)
        data.set_editor_property("auto_generate_collision", False)
        data.set_editor_property("convert_scene", True)
        data.set_editor_property("convert_scene_unit", True)
        data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    return options


def _import_meshes(spec: dict, skeleton) -> tuple[dict[str, object], list[str]]:
    tasks = []
    item_for_task = []
    for item in spec["items"]:
        if unreal.EditorAssetLibrary.does_asset_exist(_package_path(item["mesh_asset"])):
            continue
        source = SOURCE_ROOT / item["source_fbx"]
        if not source.is_file():
            raise RuntimeError(f"Generated proxy FBX is missing: {source}")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", _asset_folder(item["mesh_asset"]))
        task.set_editor_property("destination_name", _asset_name(item["mesh_asset"]))
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("factory", unreal.FbxFactory())
        task.set_editor_property("options", _import_options(item, skeleton))
        tasks.append(task)
        item_for_task.append(item)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    meshes = {}
    writes = []
    for item in spec["items"]:
        expected_type = unreal.SkeletalMesh if item["mesh_kind"] == "Skeletal" else unreal.StaticMesh
        mesh = _load_asset(item["mesh_asset"], expected_type)
        if _uasset_path(item["mesh_asset"]).is_file():
            _require_owned(mesh)
        else:
            _mark_owned(mesh)
            writes.append(_object_path(item["mesh_asset"]))
        meshes[item["item_id"]] = mesh

    # Each task may return only the mesh object path. Reject accidental Skeleton,
    # PhysicsAsset, material, or texture side products before any package save.
    expected_imports = {_object_path(item["mesh_asset"]) for item in item_for_task}
    actual_imports = set()
    for task in tasks:
        actual_imports.update(str(path) for path in task.get_editor_property("imported_object_paths"))
    if tasks and actual_imports != expected_imports:
        raise RuntimeError(
            f"Unexpected import outputs; expected={sorted(expected_imports)} actual={sorted(actual_imports)}")
    return meshes, writes


def _material_parameter(material, expression_type, name, default, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_type, x, y)
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", default)
    return expression


def _create_material(spec: dict):
    path = spec["material_asset"]
    package = _package_path(path)
    existing = (unreal.EditorAssetLibrary.load_asset(package)
                if unreal.EditorAssetLibrary.does_asset_exist(package) else None)
    if existing is not None:
        if not isinstance(existing, unreal.Material):
            raise RuntimeError(f"Outfit material path has wrong class: {_object_path(path)}")
        _require_owned(existing)
        return existing, False

    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        _asset_name(path), _asset_folder(path), unreal.Material, factory)
    if not isinstance(material, unreal.Material):
        raise RuntimeError("MaterialFactoryNew failed for M_DG_OutfitProxy")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    defaults = spec["variants"][0]
    primary = _material_parameter(
        material, unreal.MaterialExpressionVectorParameter, "DG_PrimaryColor",
        unreal.LinearColor(*defaults["primary_color"]), -900, -260)
    secondary = _material_parameter(
        material, unreal.MaterialExpressionVectorParameter, "DG_SecondaryColor",
        unreal.LinearColor(*defaults["secondary_color"]), -900, -80)
    accent = _material_parameter(
        material, unreal.MaterialExpressionVectorParameter, "DG_AccentColor",
        unreal.LinearColor(*defaults["accent_color"]), -900, 100)
    roughness_bias = _material_parameter(
        material, unreal.MaterialExpressionScalarParameter, "DG_RoughnessBias",
        float(defaults["roughness_bias"]), -650, 400)
    vertex = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -900, 260)

    primary_mul = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -560, -240)
    secondary_mul = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -560, -60)
    accent_mul = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -560, 120)
    color_add_a = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -300, -150)
    color_add_b = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -80, -80)
    unreal.MaterialEditingLibrary.connect_material_expressions(primary, "", primary_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex, "R", primary_mul, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(secondary, "", secondary_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex, "G", secondary_mul, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(accent, "", accent_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex, "B", accent_mul, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(primary_mul, "", color_add_a, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(secondary_mul, "", color_add_a, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(color_add_a, "", color_add_b, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(accent_mul, "", color_add_b, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        color_add_b, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness_base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -650, 300)
    roughness_base.set_editor_property("r", 0.65)
    roughness_add = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -300, 330)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roughness_base, "", roughness_add, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roughness_bias, "", roughness_add, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness_add, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.set_base_material_usage(
        material,
        unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH,
        True,
    )
    if not unreal.MaterialEditingLibrary.has_material_usage(
            material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH):
        raise RuntimeError("M_DG_OutfitProxy failed to enable SkeletalMesh usage")
    unreal.MaterialEditingLibrary.recompile_material(material)
    _mark_owned(material)
    unreal.EditorAssetLibrary.set_metadata_tag(
        material,
        "DG_MaterialParameters",
        "DG_PrimaryColor|DG_SecondaryColor|DG_AccentColor|DG_RoughnessBias",
    )
    return material, True


def _variant_structs(spec: dict, material) -> list:
    result = []
    for definition in spec["variants"]:
        variant = unreal.DGOutfitVariant()
        variant.set_editor_property("variant_id", definition["variant_id"])
        variant.set_editor_property("material_overrides", [material])
        variant.set_editor_property("vector_parameters", {
            "DG_PrimaryColor": unreal.LinearColor(*definition["primary_color"]),
            "DG_SecondaryColor": unreal.LinearColor(*definition["secondary_color"]),
            "DG_AccentColor": unreal.LinearColor(*definition["accent_color"]),
        })
        variant.set_editor_property("scalar_parameters", {
            "DG_RoughnessBias": float(definition["roughness_bias"]),
        })
        result.append(variant)
    return result


def _create_items(spec: dict, meshes: dict[str, object], material) -> tuple[list, list[str]]:
    items = []
    writes = []
    for definition in spec["items"]:
        path = definition["data_asset"]
        package = _package_path(path)
        item = (unreal.EditorAssetLibrary.load_asset(package)
                if unreal.EditorAssetLibrary.does_asset_exist(package) else None)
        if item is not None:
            if not isinstance(item, unreal.DiscGolfOutfitItem):
                raise RuntimeError(f"Outfit item path has wrong class: {_object_path(path)}")
            _require_owned(item, definition["item_id"])
            items.append(item)
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.DiscGolfOutfitItem.static_class())
        item = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            _asset_name(path), _asset_folder(path), unreal.DiscGolfOutfitItem, factory)
        if not isinstance(item, unreal.DiscGolfOutfitItem):
            raise RuntimeError(f"DataAssetFactory failed for {_object_path(path)}")

        item.set_editor_property("item_id", definition["item_id"])
        item.set_editor_property(
            "display_name", unreal.TextLibrary.conv_string_to_text(definition["display_name"]))
        item.set_editor_property(
            "slot", _enum_member(unreal.DGOutfitSlot, definition["slot"]))
        mesh = meshes[definition["item_id"]]
        if definition["mesh_kind"] == "Skeletal":
            item.set_editor_property("skeletal_mesh", mesh)
            item.set_editor_property("static_mesh", None)
            item.set_editor_property("attach_socket", "")
            item.set_editor_property("use_leader_pose", True)
        else:
            item.set_editor_property("skeletal_mesh", None)
            item.set_editor_property("static_mesh", mesh)
            item.set_editor_property("attach_socket", definition["attach_socket"])
            item.set_editor_property("use_leader_pose", False)
        item.set_editor_property("relative_attachment_transform", unreal.Transform())
        item.set_editor_property("bind_cloth_to_leader_pose", False)
        item.set_editor_property("covered_body_regions", [
            _enum_member(unreal.DGBodyRegion, region)
            for region in definition["covered_regions"]
        ])
        item.set_editor_property("conflicting_slots", [])
        item.set_editor_property("min_height_cm", float(spec["height_range_cm"][0]))
        item.set_editor_property("max_height_cm", float(spec["height_range_cm"][1]))
        item.set_editor_property("variants", _variant_structs(spec, material))
        _mark_owned(item, definition["item_id"])
        items.append(item)
        writes.append(_object_path(path))
    return items, writes


def _create_catalog(spec: dict, items: list):
    path = spec["catalog_asset"]
    package = _package_path(path)
    catalog = (unreal.EditorAssetLibrary.load_asset(package)
               if unreal.EditorAssetLibrary.does_asset_exist(package) else None)
    if catalog is not None:
        if not isinstance(catalog, unreal.DiscGolfOutfitCatalog):
            raise RuntimeError(f"Outfit catalog path has wrong class: {_object_path(path)}")
        _require_owned(catalog)
        return catalog, False

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DiscGolfOutfitCatalog.static_class())
    catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        _asset_name(path), _asset_folder(path), unreal.DiscGolfOutfitCatalog, factory)
    if not isinstance(catalog, unreal.DiscGolfOutfitCatalog):
        raise RuntimeError("DataAssetFactory failed for DA_DG_OutfitCatalog")
    catalog.set_editor_property("items", items)
    _mark_owned(catalog)
    return catalog, True


def _save_new_assets(spec: dict, writes: list[str]) -> None:
    for object_path in writes:
        asset = unreal.EditorAssetLibrary.load_asset(_package_path(object_path))
        if asset is None:
            raise RuntimeError(f"Owned output disappeared before save: {object_path}")
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=True):
            raise RuntimeError(f"Could not save owned Session 6 asset: {object_path}")
    for path in _all_output_paths(spec):
        if not _uasset_path(path).is_file():
            raise RuntimeError(f"Expected Session 6 package was not persisted: {_uasset_path(path)}")


def main() -> None:
    if not MANIFEST_PATH.is_file():
        raise RuntimeError(
            "Session 6 source manifest is missing; run Blender generation and source validation first")
    runpy.run_path(str(SOURCE_VALIDATOR), run_name="__main__")
    protected_before = _protected_hashes()
    spec = _load_spec()

    for folder in sorted({_asset_folder(path) for path in _all_output_paths(spec)}):
        _ensure_folder(folder)

    existing = [
        path for path in _all_output_paths(spec)
        if unreal.EditorAssetLibrary.does_asset_exist(_package_path(path))
    ]
    if existing and len(existing) != len(_all_output_paths(spec)):
        # Partial reruns are allowed only for assets carrying our exact metadata.
        for path in existing:
            asset = unreal.EditorAssetLibrary.load_asset(_package_path(path))
            item_id = next((item["item_id"] for item in spec["items"]
                            if _package_path(item["data_asset"]) == _package_path(path)), "")
            _require_owned(asset, item_id)

    if len(existing) == len(_all_output_paths(spec)):
        material = _load_asset(spec["material_asset"], unreal.Material)
        _require_owned(material)
        skeletal_usage = unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH
        material_repaired = False
        if not unreal.MaterialEditingLibrary.has_material_usage(material, skeletal_usage):
            unreal.MaterialEditingLibrary.set_base_material_usage(
                material, skeletal_usage, True)
            if not unreal.MaterialEditingLibrary.has_material_usage(
                    material, skeletal_usage):
                raise RuntimeError(
                    "M_DG_OutfitProxy failed to reconcile SkeletalMesh usage")
            _save_new_assets(spec, [_object_path(spec["material_asset"])])
            material_repaired = True

        protected_after = _protected_hashes()
        if protected_after != protected_before:
            raise RuntimeError(
                "Protected accepted package changed during material usage reconciliation")
        validation = _run_asset_validator()
        report = {
            "schema": "DiscGolfTour.Session6OutfitAssetCreation.v1",
            "status": (
                "PASS_REPAIRED_SKELETAL_MATERIAL_USAGE"
                if material_repaired
                else "PASS_ALREADY_CURRENT_NO_ASSET_WRITES"
            ),
            "asset_writes": (
                [_object_path(spec["material_asset"])] if material_repaired else []
            ),
            "validation": validation,
            "protected_assets": protected_after,
        }
        REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        unreal.log(
            "DG_SESSION6_OUTFITS: PASS_REPAIRED_SKELETAL_MATERIAL_USAGE"
            if material_repaired
            else "DG_SESSION6_OUTFITS: PASS_ALREADY_CURRENT_NO_ASSET_WRITES")
        return

    skeleton = _load_asset(spec["master_skeleton_asset"], unreal.Skeleton)
    reference_pose_before = _reference_pose_signature(skeleton)
    meshes, mesh_writes = _import_meshes(spec, skeleton)
    if _reference_pose_signature(skeleton) != reference_pose_before:
        raise RuntimeError(
            "Accepted SKEL_DG_Master reference pose changed in memory during follower import")
    material, material_created = _create_material(spec)
    items, item_writes = _create_items(spec, meshes, material)
    catalog, catalog_created = _create_catalog(spec, items)

    writes = list(mesh_writes)
    if material_created:
        writes.append(_object_path(spec["material_asset"]))
    writes.extend(item_writes)
    if catalog_created:
        writes.append(_object_path(spec["catalog_asset"]))
    expected_new = {
        _object_path(path) for path in _all_output_paths(spec)
        if not _uasset_path(path).is_file()
    }
    if set(writes) != expected_new:
        raise RuntimeError(
            f"Owned write set differs before save: writes={sorted(writes)} "
            f"expected={sorted(expected_new)}")

    _save_new_assets(spec, writes)
    protected_after = _protected_hashes()
    if protected_after != protected_before:
        raise RuntimeError("Protected accepted package changed during Session 6 authoring")

    validation = _run_asset_validator()
    report = {
        "schema": "DiscGolfTour.Session6OutfitAssetCreation.v1",
        "status": "PASS_AUTHORED_AND_VALIDATED",
        "content_status": "NON_PRODUCTION_PROXY",
        "shipping_status": "DO_NOT_SHIP",
        "catalog": catalog.get_path_name(),
        "item_count": len(items),
        "skeletal_mesh_count": sum(
            definition["mesh_kind"] == "Skeletal" for definition in spec["items"]),
        "static_mesh_count": sum(
            definition["mesh_kind"] == "Static" for definition in spec["items"]),
        "asset_writes": writes,
        "protected_assets": protected_after,
        "validation": validation,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log(
        f"DG_SESSION6_OUTFITS: PASS assets={len(writes)} items={len(items)} "
        "skeletal=9 static=6 DO_NOT_SHIP")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(json.dumps({
            "schema": "DiscGolfTour.Session6OutfitAssetCreation.v1",
            "status": "FAIL",
            "error": f"{type(error).__name__}: {error}",
        }, indent=2) + "\n", encoding="utf-8")
        raise
