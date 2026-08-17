"""Import and author the owned Session 7 proxy customization content in UE 5.8.

Run only after the Blender generator and ordinary-CPython source/brand audits
pass.  The first run imports one 69-bone skeletal head and seven head-local
static cosmetics, creates two project materials, 17 UDiscGolfCosmeticItem data
assets, and the canonical DA_DG_CosmeticCatalog.  Once all outputs exist, reruns
request no package save or import unless this owned utility must reconcile an
explicitly required material-usage flag on M_DG_HeadProxy.

This utility never edits the BuildKit donor, frozen master skeleton/body, rigs,
accepted animation, Session 6 outfit catalog, gameplay authority, or vendor
content.  It refuses partial/stale assets unless they carry the exact Session 7
ownership metadata.
"""

from __future__ import annotations

import hashlib
import json
import re
import runpy
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_ROOT = PROJECT_ROOT / "SourceArt/DiscGolf/Characters/Customization/Proxy"
SPEC_PATH = SOURCE_ROOT / "proxy_customization_catalog_spec.json"
MANIFEST_PATH = SOURCE_ROOT / "proxy_customization_source_manifest.json"
SOURCE_VALIDATOR = PROJECT_ROOT / "Scripts/validate_dg_character_session7_proxy_source.py"
REPORT_PATH = PROJECT_ROOT / "Saved/CharacterFramework/Session7CustomizationAssetCreation.json"

METADATA = {
    "DG_Session": "7",
    "DG_ContentStatus": "NON_PRODUCTION_PROXY",
    "DG_ShippingStatus": "DO_NOT_SHIP",
    "DG_BrandId": "dg_generic",
    "DG_BrandStatus": "GENERIC_UNBRANDED",
    "DG_SourceKind": "PROJECT_LOCAL_GENERATED_BLOCKOUT",
    "DG_ProductionApproved": "false",
}
SOURCE_MANIFEST_RELATIVE = (
    "SourceArt/DiscGolf/Characters/Customization/Proxy/"
    "proxy_customization_source_manifest.json"
)

# These package hashes are the accepted Session 6 boundary.  Head import may
# reference SKEL_DG_Master, but may not save/rewrite it or any throw/outfit
# authority required by the hat/hair regression.
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
    "Content/DiscGolf/Materials/Outfits/M_DG_OutfitProxy.uasset":
        "EBB8A8FF9F7455F74675052F61F4CF78B372D611523675196B3A324A965B441B",
    "Content/DiscGolf/Outfits/Data/DA_DG_OutfitCatalog.uasset":
        "F1ED9BF5D715D1C6C8685BBB404EA9943C293FB5A338E1CC8A950C875C8431CF",
    "Content/DiscGolf/Outfits/Headwear/SM_DG_Headwear_ProxyCap01.uasset":
        "92132258E256D01DE70CD4A3154F9990A11027AEAD1599AC4440F0C3AC5016C4",
    "Content/DiscGolf/Outfits/Headwear/SM_DG_Headwear_ProxyBeanie01.uasset":
        "68B8A11ADA91C92D3FA5975A2ECFBA1AF04904A9DA4BBF9DDDD80C9C7A27A5C3",
    "Content/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Headwear_Cap01.uasset":
        "A686CAD9C6D86DCF96077D224C8CA26A8BC64EA4112C2E27A788AE96A297021A",
    "Content/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Headwear_Beanie01.uasset":
        "E0C66EA90CD4F39725840D5E56EC2B7FFB9AE7185945446AC0BD3E7FBAF7EF7F",
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
                f"Protected Session 6 package changed before Session 7 authoring: "
                f"{relative} {actual} != {expected}")
        result[relative] = actual
    return result


def _load_spec() -> dict:
    return json.loads(SPEC_PATH.read_text(encoding="utf-8"))


def _package_path(path: str) -> str:
    leaf = path.rsplit("/", 1)[-1]
    return path.rsplit(".", 1)[0] if "." in leaf else path


def _object_path(path: str) -> str:
    package = _package_path(path)
    return f"{package}.{package.rsplit('/', 1)[-1]}"


def _asset_name(path: str) -> str:
    return _package_path(path).rsplit("/", 1)[-1]


def _asset_folder(path: str) -> str:
    return _package_path(path).rsplit("/", 1)[0]


def _uasset_path(path: str) -> Path:
    package = _package_path(path)
    if not package.startswith("/Game/"):
        raise RuntimeError(f"Session 7 output escaped /Game: {package}")
    return PROJECT_ROOT / "Content" / (package[len("/Game/"):] + ".uasset")


def _ensure_folder(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"Could not create owned content folder: {path}")


def _load_asset(path: str, expected_type):
    asset = unreal.EditorAssetLibrary.load_asset(_package_path(path))
    if not isinstance(asset, expected_type):
        raise RuntimeError(
            f"Expected {expected_type.__name__} at {_object_path(path)}, found {asset}")
    return asset


def _mark_owned(asset, item_id: str = "") -> None:
    for key, value in METADATA.items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, value)
    unreal.EditorAssetLibrary.set_metadata_tag(
        asset, "DG_SourceManifest", SOURCE_MANIFEST_RELATIVE)
    if item_id:
        unreal.EditorAssetLibrary.set_metadata_tag(asset, "DG_ItemId", item_id)


def _require_owned(asset, item_id: str = "") -> None:
    for key, expected in METADATA.items():
        actual = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key))
        if actual != expected:
            raise RuntimeError(
                f"Refusing to modify stale/non-owned Session 7 asset "
                f"{asset.get_path_name()}: {key}={actual!r}, expected {expected!r}")
    source = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, "DG_SourceManifest"))
    if source != SOURCE_MANIFEST_RELATIVE:
        raise RuntimeError(f"Refusing Session 7 source-manifest mismatch: {asset.get_path_name()}")
    if item_id:
        actual = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, "DG_ItemId"))
        if actual != item_id:
            raise RuntimeError(
                f"Refusing Session 7 ItemId ownership mismatch: {actual} != {item_id}")


def _enum_member(enum_type, display_name: str):
    token = re.sub(r"(?<!^)(?=[A-Z])", "_", display_name).upper()
    try:
        return getattr(enum_type, token)
    except AttributeError as error:
        raise RuntimeError(f"Could not resolve {enum_type.__name__}.{token}") from error


def _all_output_paths(spec: dict) -> list[str]:
    paths = [
        spec["head_material_asset"], spec["hair_material_asset"],
        spec["head_mesh_asset"], spec["catalog_asset"],
    ]
    paths.extend(item["mesh_asset"] for item in spec["items"] if item["mesh_kind"] == "Static")
    paths.extend(item["data_asset"] for item in spec["items"])
    return paths


def _output_hashes(spec: dict) -> dict[str, str]:
    result = {}
    for path in _all_output_paths(spec):
        disk_path = _uasset_path(path)
        if not disk_path.is_file():
            raise RuntimeError(f"Session 7 output is missing: {disk_path}")
        result[path] = _sha256(disk_path)
    return result


def _reference_pose_signature(skeleton) -> tuple:
    pose = skeleton.get_reference_pose()
    result = []
    for bone_name in pose.get_bone_names():
        transform = pose.get_bone_pose(bone_name, unreal.AnimPoseSpaces.LOCAL)
        t, r, s = transform.translation, transform.rotation, transform.scale3d
        result.append((
            str(bone_name), float(t.x), float(t.y), float(t.z),
            float(r.x), float(r.y), float(r.z), float(r.w),
            float(s.x), float(s.y), float(s.z),
        ))
    return tuple(result)


def _source_ready() -> dict:
    namespace = runpy.run_path(str(SOURCE_VALIDATOR), run_name="dg_session7_author_source")
    result = namespace["validate"]()
    if result.get("status") != "PASS" or not result.get("acceptance_ready"):
        raise RuntimeError(
            "Session 7 generated source is not acceptance-ready: "
            f"status={result.get('status')} source={result.get('source_status')}")
    return result


def _import_head_options(skeleton):
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("override_full_name", True)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("create_physics_asset", False)
    options.set_editor_property("skeleton", skeleton)
    data = options.get_editor_property("skeletal_mesh_import_data")
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("convert_scene_unit", True)
    data.set_editor_property("import_meshes_in_bone_hierarchy", False)
    data.set_editor_property("preserve_smoothing_groups", True)
    data.set_editor_property("update_skeleton_reference_pose", False)
    data.set_editor_property("use_t0_as_ref_pose", False)
    data.set_editor_property("import_morph_targets", True)
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    return options


def _import_static_options():
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("override_full_name", True)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("create_physics_asset", False)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("convert_scene_unit", True)
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    return options


def _import_meshes(spec: dict, skeleton) -> tuple[object, dict[str, object], list[str], int]:
    definitions = [{
        "key": "__head__", "source_fbx": spec["head_source_fbx"],
        "mesh_asset": spec["head_mesh_asset"], "mesh_kind": "Skeletal",
    }]
    definitions.extend(item for item in spec["items"] if item["mesh_kind"] == "Static")
    tasks = []
    task_definitions = []
    for definition in definitions:
        if unreal.EditorAssetLibrary.does_asset_exist(_package_path(definition["mesh_asset"])):
            continue
        source = SOURCE_ROOT / definition["source_fbx"]
        if not source.is_file():
            raise RuntimeError(f"Generated source FBX missing: {source}")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", _asset_folder(definition["mesh_asset"]))
        task.set_editor_property("destination_name", _asset_name(definition["mesh_asset"]))
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("factory", unreal.FbxFactory())
        task.set_editor_property(
            "options", _import_head_options(skeleton)
            if definition["mesh_kind"] == "Skeletal" else _import_static_options())
        tasks.append(task)
        task_definitions.append(definition)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    expected_imports = {_object_path(definition["mesh_asset"]) for definition in task_definitions}
    actual_imports = set()
    for task in tasks:
        actual_imports.update(str(path) for path in task.get_editor_property("imported_object_paths"))
    if tasks and actual_imports != expected_imports:
        raise RuntimeError(
            f"Unexpected import side products; expected={sorted(expected_imports)} "
            f"actual={sorted(actual_imports)}")

    writes = []
    head = _load_asset(spec["head_mesh_asset"], unreal.SkeletalMesh)
    if _uasset_path(spec["head_mesh_asset"]).is_file():
        _require_owned(head)
    else:
        _mark_owned(head)
        writes.append(_object_path(spec["head_mesh_asset"]))
    meshes = {}
    for item in spec["items"]:
        if item["mesh_kind"] != "Static":
            continue
        mesh = _load_asset(item["mesh_asset"], unreal.StaticMesh)
        if _uasset_path(item["mesh_asset"]).is_file():
            _require_owned(mesh, item["item_id"])
        else:
            _mark_owned(mesh, item["item_id"])
            writes.append(_object_path(item["mesh_asset"]))
        meshes[item["item_id"]] = mesh
    return head, meshes, writes, len(tasks)


def _parameter(material, expression_type, name, default, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_type, x, y)
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", default)
    return expression


def _constant3(material, color, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, x, y)
    expression.set_editor_property("constant", unreal.LinearColor(*color))
    return expression


def _multiply(material, a, a_output, b, b_output, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, x, y)
    unreal.MaterialEditingLibrary.connect_material_expressions(a, a_output, expression, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(b, b_output, expression, "B")
    return expression


def _lerp(material, a, b, alpha, alpha_output, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLinearInterpolate, x, y)
    unreal.MaterialEditingLibrary.connect_material_expressions(a, "", expression, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(b, "", expression, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(alpha, alpha_output, expression, "Alpha")
    return expression


def _create_head_material(path: str, spec: dict):
    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        _asset_name(path), _asset_folder(path), unreal.Material, factory)
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Could not create M_DG_HeadProxy")
    defaults = spec["material_defaults"]
    skin = _parameter(material, unreal.MaterialExpressionVectorParameter, "DG_SkinTone",
                      unreal.LinearColor(*defaults["DG_SkinTone"]), -1100, -340)
    eyes = _parameter(material, unreal.MaterialExpressionVectorParameter, "DG_EyeColor",
                      unreal.LinearColor(*defaults["DG_EyeColor"]), -1100, -220)
    complexion = _parameter(material, unreal.MaterialExpressionScalarParameter, "DG_Complexion",
                            defaults["DG_Complexion"], -1100, 460)
    freckles = _parameter(material, unreal.MaterialExpressionScalarParameter, "DG_Freckles",
                          defaults["DG_Freckles"], -1100, 20)
    sun = _parameter(material, unreal.MaterialExpressionScalarParameter, "DG_SunExposure",
                     defaults["DG_SunExposure"], -1100, 140)
    scar = _parameter(material, unreal.MaterialExpressionScalarParameter, "DG_ScarProxy",
                      defaults["DG_ScarProxy"], -1100, 260)
    tattoo = _parameter(material, unreal.MaterialExpressionScalarParameter, "DG_TattooProxy",
                        defaults["DG_TattooProxy"], -1100, 380)
    vertex = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -1100, -80)

    skin_eye = _lerp(material, eyes, skin, vertex, "R", -760, -280)
    dark = _constant3(material, (0.16, 0.07, 0.035, 1.0), -760, -120)
    freckle_alpha = _multiply(material, freckles, "", vertex, "G", -760, 20)
    freckled = _lerp(material, skin_eye, dark, freckle_alpha, "", -480, -220)
    scar_color = _constant3(material, (0.22, 0.055, 0.045, 1.0), -760, 100)
    scar_alpha = _multiply(material, scar, "", vertex, "B", -480, 80)
    scarred = _lerp(material, freckled, scar_color, scar_alpha, "", -190, -180)
    tan_color = _constant3(material, (0.36, 0.16, 0.07, 1.0), -480, 210)
    sun_tinted = _lerp(material, scarred, tan_color, sun, "", 100, -120)
    tattoo_color = _constant3(material, (0.025, 0.035, 0.055, 1.0), -190, 250)
    tattoo_alpha = _multiply(material, tattoo, "", vertex, "A", 100, 160)
    final_color = _lerp(material, sun_tinted, tattoo_color, tattoo_alpha, "", 380, -80)
    unreal.MaterialEditingLibrary.connect_material_property(
        final_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    negative_roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -480, 420)
    negative_roughness.set_editor_property("r", -0.22)
    roughness_delta = _multiply(
        material, complexion, "", negative_roughness, "", -190, 410)
    roughness_base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -190, 520)
    roughness_base.set_editor_property("r", 0.72)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 100, 440)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roughness_base, "", roughness, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roughness_delta, "", roughness, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    _ensure_head_material_usages(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    _mark_owned(material)
    unreal.EditorAssetLibrary.set_metadata_tag(
        material, "DG_MaterialParameters", "|".join(spec["head_material_parameters"]))
    unreal.EditorAssetLibrary.set_metadata_tag(
        material, "DG_SurfaceMask", "DG_SurfaceMask:R=SkinVsEye|G=Freckles|B=Scar|A=TattooHook")
    return material


def _ensure_head_material_usages(material) -> bool:
    """Reconcile only the two renderer usages required by the proxy head."""
    changed = False
    for usage, label in (
        (unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, "SkeletalMesh"),
        (unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS, "MorphTargets"),
    ):
        if not unreal.MaterialEditingLibrary.has_material_usage(material, usage):
            unreal.MaterialEditingLibrary.set_base_material_usage(
                material, usage, True)
            changed = True
        if not unreal.MaterialEditingLibrary.has_material_usage(material, usage):
            raise RuntimeError(f"M_DG_HeadProxy failed {label} usage")
    return changed


def _create_hair_material(path: str, spec: dict):
    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        _asset_name(path), _asset_folder(path), unreal.Material, factory)
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Could not create M_DG_HairProxy")
    color = _parameter(
        material, unreal.MaterialExpressionVectorParameter, "DG_HairColor",
        unreal.LinearColor(*spec["material_defaults"]["DG_HairColor"]), -300, -80)
    unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -300, 100)
    roughness.set_editor_property("r", 0.62)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    _mark_owned(material)
    unreal.EditorAssetLibrary.set_metadata_tag(
        material, "DG_MaterialParameters", "DG_HairColor")
    return material


def _create_materials(spec: dict) -> tuple[object, object, list[str]]:
    writes = []
    materials = []
    for path, creator in (
        (spec["head_material_asset"], _create_head_material),
        (spec["hair_material_asset"], _create_hair_material),
    ):
        existing = (unreal.EditorAssetLibrary.load_asset(_package_path(path))
                    if unreal.EditorAssetLibrary.does_asset_exist(_package_path(path)) else None)
        if existing is not None:
            if not isinstance(existing, unreal.Material):
                raise RuntimeError(f"Material path has wrong class: {_object_path(path)}")
            _require_owned(existing)
            materials.append(existing)
        else:
            material = creator(path, spec)
            materials.append(material)
            writes.append(_object_path(path))
    return materials[0], materials[1], writes


def _assign_mesh_materials(head, static_meshes: dict[str, object], head_material,
                           hair_material, writes: list[str], spec: dict) -> None:
    if _object_path(spec["head_mesh_asset"]) in writes:
        slots = list(head.get_editor_property("materials"))
        if len(slots) != 1:
            raise RuntimeError(f"Head import must expose one material slot, found {len(slots)}")
        slots[0].set_editor_property("material_interface", head_material)
        head.set_editor_property("materials", slots)
        unreal.EditorAssetLibrary.set_metadata_tag(
            head, "DG_VisibleMorphs", "|".join(spec["visible_morphs"].values()))
        unreal.EditorAssetLibrary.set_metadata_tag(
            head, "DG_DeferredVisualMorphs", "|".join(spec["deferred_visual_morphs"].values()))
    for item in spec["items"]:
        if item["mesh_kind"] != "Static" or _object_path(item["mesh_asset"]) not in writes:
            continue
        mesh = static_meshes[item["item_id"]]
        slots = list(mesh.get_editor_property("static_materials"))
        if len(slots) != 1:
            raise RuntimeError(
                f"Static cosmetic import must expose one material slot: {item['item_id']}")
        mesh.set_material(0, hair_material)


def _create_items(spec: dict, static_meshes: dict[str, object]) -> tuple[list, list[str]]:
    result = []
    writes = []
    for definition in spec["items"]:
        path = definition["data_asset"]
        item = (unreal.EditorAssetLibrary.load_asset(_package_path(path))
                if unreal.EditorAssetLibrary.does_asset_exist(_package_path(path)) else None)
        if item is not None:
            if not isinstance(item, unreal.DiscGolfCosmeticItem):
                raise RuntimeError(f"Cosmetic item path has wrong class: {_object_path(path)}")
            _require_owned(item, definition["item_id"])
            result.append(item)
            continue
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.DiscGolfCosmeticItem.static_class())
        item = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            _asset_name(path), _asset_folder(path), unreal.DiscGolfCosmeticItem, factory)
        if not isinstance(item, unreal.DiscGolfCosmeticItem):
            raise RuntimeError(f"Could not create cosmetic item: {_object_path(path)}")
        item.set_editor_property("item_id", definition["item_id"])
        item.set_editor_property(
            "display_name", unreal.TextLibrary.conv_string_to_text(definition["display_name"]))
        item.set_editor_property(
            "kind", _enum_member(unreal.DGCosmeticKind, definition["kind"]))
        item.set_editor_property("skeletal_mesh", None)
        item.set_editor_property(
            "static_mesh", static_meshes.get(definition["item_id"]))
        item.set_editor_property("attach_socket", definition["attach_socket"])
        item.set_editor_property("relative_attachment_transform", unreal.Transform())
        item.set_editor_property("material_variant_id", definition["material_variant_id"])
        item.set_editor_property("min_height_cm", float(spec["height_range_cm"][0]))
        item.set_editor_property("max_height_cm", float(spec["height_range_cm"][1]))
        _mark_owned(item, definition["item_id"])
        result.append(item)
        writes.append(_object_path(path))
    return result, writes


def _create_catalog(spec: dict, items: list) -> tuple[object, bool]:
    path = spec["catalog_asset"]
    catalog = (unreal.EditorAssetLibrary.load_asset(_package_path(path))
               if unreal.EditorAssetLibrary.does_asset_exist(_package_path(path)) else None)
    if catalog is not None:
        if not isinstance(catalog, unreal.DiscGolfCosmeticCatalog):
            raise RuntimeError(f"Cosmetic catalog path has wrong class: {_object_path(path)}")
        _require_owned(catalog)
        return catalog, False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DiscGolfCosmeticCatalog.static_class())
    catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        _asset_name(path), _asset_folder(path), unreal.DiscGolfCosmeticCatalog, factory)
    if not isinstance(catalog, unreal.DiscGolfCosmeticCatalog):
        raise RuntimeError("Could not create DA_DG_CosmeticCatalog")
    catalog.set_editor_property("items", items)
    _mark_owned(catalog)
    return catalog, True


def _save_outputs(writes: list[str]) -> None:
    for object_path in dict.fromkeys(writes):
        asset = unreal.EditorAssetLibrary.load_asset(_package_path(object_path))
        if asset is None:
            raise RuntimeError(f"Owned output disappeared before save: {object_path}")
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=True):
            raise RuntimeError(f"Could not save owned Session 7 asset: {object_path}")


def main() -> None:
    source_validation = _source_ready()
    spec = _load_spec()
    for folder in (
        "/Game/DiscGolf/Characters/Customization",
        "/Game/DiscGolf/Characters/Customization/Head",
        "/Game/DiscGolf/Characters/Customization/Cosmetics",
        "/Game/DiscGolf/Characters/Customization/Data",
        "/Game/DiscGolf/Characters/Customization/Data/Items",
        "/Game/DiscGolf/Materials/CharacterCustomization",
    ):
        _ensure_folder(folder)
    protected_before = _protected_hashes()

    all_outputs = _all_output_paths(spec)
    if all(unreal.EditorAssetLibrary.does_asset_exist(_package_path(path))
           and _uasset_path(path).is_file() for path in all_outputs):
        outputs_before = _output_hashes(spec)
        head_material = _load_asset(spec["head_material_asset"], unreal.Material)
        _require_owned(head_material)
        material_reconciled = _ensure_head_material_usages(head_material)
        writes = ([ _object_path(spec["head_material_asset"]) ]
                  if material_reconciled else [])
        if writes:
            _save_outputs(writes)
        outputs_after = _output_hashes(spec)
        changed_outputs = sorted(
            path for path in all_outputs
            if outputs_after[path] != outputs_before[path])
        expected_changes = (
            [spec["head_material_asset"]] if material_reconciled else [])
        if changed_outputs != expected_changes:
            raise RuntimeError(
                "Session 7 material reconciliation changed unexpected outputs: "
                f"{changed_outputs} != {expected_changes}")
        protected_after = _protected_hashes()
        if protected_after != protected_before:
            raise RuntimeError("Protected package hash changed on validation-only authoring path")
        status = ("PASS_RECONCILED_HEAD_MATERIAL_USAGE"
                  if material_reconciled
                  else "PASS_ALREADY_CURRENT_NO_ASSET_WRITES")
        report = {
            "schema": "DiscGolfTour.Session7CustomizationAssetCreation.v1",
            "status": status,
            "source_validation": source_validation.get("source_status"),
            "catalog": spec["catalog_asset"],
            "head": spec["head_mesh_asset"],
            "catalog_item_count": 17,
            "static_mesh_count": 7,
            "visible_morph_count": 5,
            "deferred_visual_morph_count": 15,
            "asset_import_calls": 0,
            "asset_save_calls": len(writes),
            "asset_factory_calls": 0,
            "asset_delete_calls": 0,
            "writes": writes,
            "changed_output_packages": changed_outputs,
            "head_material_skeletal_mesh_usage": True,
            "head_material_morph_targets_usage": True,
            "protected_assets": protected_after,
            "strict_validation_status": "NOT_RUN_AWAITING_CLEARANCE",
        }
        REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        unreal.log(
            "DG_SESSION7_CUSTOMIZATION_AUTHOR: " + status
            + f" saves={len(writes)}")
        return

    skeleton = _load_asset(spec["master_skeleton_asset"], unreal.Skeleton)
    reference_pose_before = _reference_pose_signature(skeleton)
    head, static_meshes, mesh_writes, import_calls = _import_meshes(spec, skeleton)
    head_material, hair_material, material_writes = _create_materials(spec)
    writes = material_writes + mesh_writes
    _assign_mesh_materials(
        head, static_meshes, head_material, hair_material, writes, spec)
    items, item_writes = _create_items(spec, static_meshes)
    _catalog, catalog_created = _create_catalog(spec, items)
    writes.extend(item_writes)
    if catalog_created:
        writes.append(_object_path(spec["catalog_asset"]))

    if _reference_pose_signature(skeleton) != reference_pose_before:
        raise RuntimeError("Accepted SKEL_DG_Master reference pose changed in memory")
    _save_outputs(writes)
    protected_after = _protected_hashes()
    if protected_after != protected_before:
        raise RuntimeError("Protected accepted package changed during Session 7 authoring")
    report = {
        "schema": "DiscGolfTour.Session7CustomizationAssetCreation.v1",
        "status": "PASS_AUTHORED",
        "source_validation": source_validation.get("source_status"),
        "catalog": spec["catalog_asset"],
        "head": spec["head_mesh_asset"],
        "catalog_item_count": 17,
        "static_mesh_count": 7,
        "visible_morph_count": 5,
        "deferred_visual_morph_count": 15,
        "asset_import_calls": import_calls,
        "asset_save_calls": len(list(dict.fromkeys(writes))),
        "writes": list(dict.fromkeys(writes)),
        "protected_assets": protected_after,
        "strict_validation_status": "NOT_RUN_AWAITING_CLEARANCE",
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log(
        "DG_SESSION7_CUSTOMIZATION_AUTHOR: PASS_AUTHORED "
        f"items=17 static=7 imports={import_calls} saves={report['asset_save_calls']}")


if __name__ == "__main__":
    main()
