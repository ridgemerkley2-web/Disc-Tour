"""Strict read-only validation for the live Session 7 customization assets.

The validator proves the exact project-owned generic catalog, skeletal head,
static head-local cosmetics, material parameter contract, frozen master
skeleton boundary, and package namespace.  It never imports, creates, edits,
recompiles, saves, renames, deletes, or checks out an Unreal asset.  Its only
write is the JSON report below ``Saved/CharacterFramework``.
"""

from __future__ import annotations

import hashlib
import json
import math
import re
import runpy
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_ROOT = PROJECT_ROOT / "SourceArt/DiscGolf/Characters/Customization/Proxy"
SPEC_PATH = SOURCE_ROOT / "proxy_customization_catalog_spec.json"
MANIFEST_PATH = SOURCE_ROOT / "proxy_customization_source_manifest.json"
AUTHOR_SCRIPT = PROJECT_ROOT / "Scripts/create_dg_character_session7_customization_assets.py"
SOURCE_VALIDATOR = PROJECT_ROOT / "Scripts/validate_dg_character_session7_proxy_source.py"
REPORT_PATH = (
    PROJECT_ROOT / "Saved/CharacterFramework/Session7CustomizationAssetValidation.json"
)
SOURCE_MANIFEST_RELATIVE = (
    "SourceArt/DiscGolf/Characters/Customization/Proxy/"
    "proxy_customization_source_manifest.json"
)

EXPECTED_IDS = [
    "hair_none", "hair_short", "hair_medium", "hair_mohawk",
    "facialhair_none", "facialhair_stubble", "facialhair_beard",
    "brow_default", "brow_alt", "scar_none", "scar_proxy",
    "tattoo_none", "tattoo_proxy", "voice_default", "voice_alt",
    "pronouns_default", "pronouns_they_them",
]
EXPECTED_VISIBLE_MORPHS = [
    "DG_Face_HeadWidth", "DG_Face_HeadHeight", "DG_Face_CheekFullness",
    "DG_Face_JawWidth", "DG_Face_ChinLength",
]
EXPECTED_DEFERRED_MORPHS = [
    "DG_Face_BrowHeight", "DG_Face_BrowDepth", "DG_Face_EyeSize",
    "DG_Face_EyeSpacing", "DG_Face_EyeDepth", "DG_Face_NoseWidth",
    "DG_Face_NoseLength", "DG_Face_NoseBridge", "DG_Face_CheekWidth",
    "DG_Face_JawHeight", "DG_Face_ChinWidth", "DG_Face_MouthWidth",
    "DG_Face_LipFullness", "DG_Face_EarSize", "DG_Face_EarAngle",
]
EXPECTED_METADATA = {
    "DG_Session": "7",
    "DG_ContentStatus": "NON_PRODUCTION_PROXY",
    "DG_ShippingStatus": "DO_NOT_SHIP",
    "DG_BrandId": "dg_generic",
    "DG_BrandStatus": "GENERIC_UNBRANDED",
    "DG_SourceKind": "PROJECT_LOCAL_GENERATED_BLOCKOUT",
    "DG_ProductionApproved": "false",
}
FORBIDDEN_BRAND_TOKENS = (
    "adidas", "discraft", "discmania", "dynamic discs", "innova",
    "latitude 64", "metahuman", "mvp disc", "nike", "oakley",
    "premium disc golf", "prodigy disc", "under armour", "westside discs",
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _package_path(object_or_package_path: str) -> str:
    leaf = object_or_package_path.rsplit("/", 1)[-1]
    return (object_or_package_path.rsplit(".", 1)[0]
            if "." in leaf else object_or_package_path)


def _object_path(object_or_package_path: str) -> str:
    package = _package_path(object_or_package_path)
    return f"{package}.{package.rsplit('/', 1)[-1]}"


def _uasset_path(object_or_package_path: str) -> Path:
    package = _package_path(object_or_package_path)
    if not package.startswith("/Game/"):
        raise RuntimeError(f"Expected /Game package path, found {package}")
    return PROJECT_ROOT / "Content" / (package[len("/Game/"):] + ".uasset")


def _load_asset(path: str, expected_type, errors: list[str]):
    asset = unreal.EditorAssetLibrary.load_asset(_package_path(path))
    if not isinstance(asset, expected_type):
        errors.append(
            f"Expected {expected_type.__name__} at {_object_path(path)}, found {asset}")
        return None
    return asset


def _soft_path(value) -> str:
    if value is None:
        return ""
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "to_soft_object_path"):
        value = value.to_soft_object_path()
    text = str(value)
    match = re.search(r"(/Game/[^'\" ]+)", text)
    return match.group(1) if match else text


def _enum_token(value) -> str:
    name = getattr(value, "name", None)
    if callable(name):
        name = name()
    text = str(name) if name else str(value).rsplit(".", 1)[-1].split(":", 1)[0]
    return re.sub(r"[^A-Za-z0-9]", "", text).casefold()


def _name_text(value) -> str:
    text = str(value)
    return "" if text.casefold() == "none" else text


def _expected_enum_token(display_name: str) -> str:
    return re.sub(r"[^A-Za-z0-9]", "", display_name).casefold()


def _near(actual, expected, tolerance: float = 1.0e-5) -> bool:
    try:
        value = float(actual)
    except (TypeError, ValueError):
        return False
    return math.isfinite(value) and abs(value - float(expected)) <= tolerance


def _color_matches(color, expected: list[float]) -> bool:
    return all(_near(actual, target) for actual, target in zip(
        (color.r, color.g, color.b, color.a), expected))


def _transform_is_identity(transform) -> bool:
    translation = transform.translation
    rotation = transform.rotation
    scale = transform.scale3d
    return (
        _near(translation.x, 0.0) and _near(translation.y, 0.0)
        and _near(translation.z, 0.0)
        and _near(rotation.x, 0.0) and _near(rotation.y, 0.0)
        and _near(rotation.z, 0.0)
        and abs(abs(float(rotation.w)) - 1.0) <= 1.0e-5
        and _near(scale.x, 1.0) and _near(scale.y, 1.0) and _near(scale.z, 1.0)
    )


def _metadata(asset, errors: list[str], item_id: str = "") -> None:
    for key, expected in EXPECTED_METADATA.items():
        actual = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key))
        if actual != expected:
            errors.append(
                f"{asset.get_path_name()} metadata {key}={actual!r}, expected {expected!r}")
    source = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, "DG_SourceManifest"))
    if source != SOURCE_MANIFEST_RELATIVE:
        errors.append(
            f"{asset.get_path_name()} source-manifest metadata differs: {source!r}")
    if item_id:
        actual = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, "DG_ItemId"))
        if actual != item_id:
            errors.append(
                f"{asset.get_path_name()} ItemId metadata {actual!r} != {item_id!r}")


def _expand_hierarchy(contract: dict) -> list[tuple[str, str | None]]:
    hierarchy = [
        (str(name), parent if parent is None else str(parent))
        for name, parent in contract["hierarchy"]
    ]
    pattern = contract["finger_pattern"]
    for side in ("l", "r"):
        for finger in pattern["each_hand"]:
            parent = f"hand_{side}"
            for segment in range(1, int(pattern["segments"]) + 1):
                name = pattern["name_template"].format(
                    finger=finger, segment=segment, side=side)
                hierarchy.append((name, parent))
                parent = name
    return hierarchy


def _protected_hashes(contract: dict, errors: list[str]) -> dict[str, str]:
    result = {}
    for relative, expected in contract["PROTECTED_ASSETS"].items():
        path = PROJECT_ROOT / relative
        if not path.is_file():
            errors.append(f"Protected accepted package missing: {relative}")
            continue
        actual = _sha256(path)
        result[relative] = actual
        if actual != expected:
            errors.append(
                f"Protected accepted package hash changed: {relative} {actual} != {expected}")
    return result


def _all_output_paths(spec: dict) -> list[str]:
    result = [
        spec["head_material_asset"], spec["hair_material_asset"],
        spec["head_mesh_asset"], spec["catalog_asset"],
    ]
    result.extend(
        item["mesh_asset"] for item in spec["items"] if item["mesh_kind"] == "Static")
    result.extend(item["data_asset"] for item in spec["items"])
    return result


def _output_hashes(spec: dict, errors: list[str]) -> dict[str, str]:
    result = {}
    for object_path in _all_output_paths(spec):
        disk = _uasset_path(object_path)
        if not disk.is_file():
            errors.append(f"Expected Session 7 package missing: {disk}")
            continue
        result[_object_path(object_path)] = _sha256(disk)
    return result


def _parameter_names(material, kind: str) -> list[str]:
    getter = getattr(unreal.MaterialEditingLibrary, f"get_{kind}_parameter_names")
    return [str(name) for name in getter(material)]


def _material_slot_paths(mesh, skeletal: bool) -> list[str]:
    property_name = "materials" if skeletal else "static_materials"
    return [
        _soft_path(slot.get_editor_property("material_interface"))
        for slot in mesh.get_editor_property(property_name)
    ]


def _morph_names(mesh, errors: list[str]) -> list[str]:
    # UE 5.8 exposes K2_GetAllMorphTargetNames through ScriptName as the first
    # spelling.  Keep the native wrapper spelling as a version-local fallback;
    # failing both is an error because metadata alone is not morph evidence.
    for method_name in ("get_all_morph_target_names", "k2_get_all_morph_target_names"):
        method = getattr(mesh, method_name, None)
        if callable(method):
            return [str(name) for name in method()]
    errors.append("UE 5.8 did not expose actual skeletal-mesh morph target names")
    return []


def _validate_materials(spec: dict, errors: list[str]) -> tuple[object, object]:
    head = _load_asset(spec["head_material_asset"], unreal.Material, errors)
    hair = _load_asset(spec["hair_material_asset"], unreal.Material, errors)
    if head:
        _metadata(head, errors)
        if not unreal.MaterialEditingLibrary.has_material_usage(
                head, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH):
            errors.append("M_DG_HeadProxy lacks required SkeletalMesh material usage")
        if not unreal.MaterialEditingLibrary.has_material_usage(
                head, unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS):
            errors.append("M_DG_HeadProxy lacks required MorphTargets material usage")
        metadata_parameters = str(unreal.EditorAssetLibrary.get_metadata_tag(
            head, "DG_MaterialParameters"))
        if metadata_parameters != "|".join(spec["head_material_parameters"]):
            errors.append("M_DG_HeadProxy exact parameter metadata differs")
        surface_mask = str(unreal.EditorAssetLibrary.get_metadata_tag(
            head, "DG_SurfaceMask"))
        if surface_mask != "DG_SurfaceMask:R=SkinVsEye|G=Freckles|B=Scar|A=TattooHook":
            errors.append("M_DG_HeadProxy surface-mask metadata differs")
        expected_vector = {"DG_SkinTone", "DG_EyeColor"}
        expected_scalar = {
            "DG_Complexion", "DG_Freckles", "DG_SunExposure",
            "DG_ScarProxy", "DG_TattooProxy",
        }
        if set(_parameter_names(head, "vector")) != expected_vector:
            errors.append("M_DG_HeadProxy vector parameter set differs")
        if set(_parameter_names(head, "scalar")) != expected_scalar:
            errors.append("M_DG_HeadProxy scalar parameter set differs")
        if _parameter_names(head, "texture") or _parameter_names(head, "static_switch"):
            errors.append("M_DG_HeadProxy unexpectedly owns texture/static-switch parameters")
        for name in expected_vector:
            value = unreal.MaterialEditingLibrary.get_material_default_vector_parameter_value(
                head, name)
            if not _color_matches(value, spec["material_defaults"][name]):
                errors.append(f"M_DG_HeadProxy vector default differs: {name}")
        for name in expected_scalar:
            value = unreal.MaterialEditingLibrary.get_material_default_scalar_parameter_value(
                head, name)
            if not _near(value, spec["material_defaults"][name]):
                errors.append(f"M_DG_HeadProxy scalar default differs: {name}")
    if hair:
        _metadata(hair, errors)
        metadata_parameters = str(unreal.EditorAssetLibrary.get_metadata_tag(
            hair, "DG_MaterialParameters"))
        if metadata_parameters != "DG_HairColor":
            errors.append("M_DG_HairProxy exact parameter metadata differs")
        if set(_parameter_names(hair, "vector")) != {"DG_HairColor"} \
                or _parameter_names(hair, "scalar") \
                or _parameter_names(hair, "texture") \
                or _parameter_names(hair, "static_switch"):
            errors.append("M_DG_HairProxy exact parameter set differs")
        value = unreal.MaterialEditingLibrary.get_material_default_vector_parameter_value(
            hair, "DG_HairColor")
        if not _color_matches(value, spec["material_defaults"]["DG_HairColor"]):
            errors.append("M_DG_HairProxy DG_HairColor default differs")
    return head, hair


def _validate_head(spec: dict, contract: dict, errors: list[str]):
    master_skeleton = _load_asset(spec["master_skeleton_asset"], unreal.Skeleton, errors)
    master_mesh = _load_asset(spec["master_mesh_asset"], unreal.SkeletalMesh, errors)
    head = _load_asset(spec["head_mesh_asset"], unreal.SkeletalMesh, errors)
    hierarchy = _expand_hierarchy(_load_json(PROJECT_ROOT / spec["skeleton_contract"]))
    expected_bones = [name for name, _parent in hierarchy]
    if len(expected_bones) != 69:
        errors.append(f"Frozen skeleton contract expands to {len(expected_bones)}, expected 69")
    if master_skeleton:
        actual_bones = [
            str(name) for name in master_skeleton.get_reference_pose().get_bone_names()]
        if len(actual_bones) != 69 or set(actual_bones) != set(expected_bones):
            errors.append("Accepted master skeleton exact 69-bone name set differs")
    if master_mesh and master_skeleton:
        if _soft_path(master_mesh.get_editor_property("skeleton")) != \
                _object_path(spec["master_skeleton_asset"]):
            errors.append("Accepted master mesh no longer uses SKEL_DG_Master")
        mesh_editor = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        if mesh_editor is None:
            errors.append("SkeletalMeshEditorSubsystem unavailable for hierarchy validation")
        else:
            for bone_name, expected_parent in hierarchy:
                actual_parent = str(mesh_editor.get_bone_parent(master_mesh, bone_name))
                if actual_parent.casefold() in {"", "none"}:
                    actual_parent = None
                if actual_parent != expected_parent:
                    errors.append(
                        f"Accepted master parent differs: {bone_name} "
                        f"{actual_parent!r} != {expected_parent!r}")
    if not head:
        return None
    _metadata(head, errors)
    if _soft_path(head.get_editor_property("skeleton")) != \
            _object_path(spec["master_skeleton_asset"]):
        errors.append("Proxy head does not use the accepted SKEL_DG_Master")
    if head.get_editor_property("physics_asset"):
        errors.append("Proxy head unexpectedly has a PhysicsAsset")
    if not head.has_vertex_colors():
        errors.append("Proxy head lost DG_SurfaceMask vertex-color data")
    if _material_slot_paths(head, True) != [_object_path(spec["head_material_asset"])]:
        errors.append("Proxy head material slot differs")
    actual_morphs = _morph_names(head, errors)
    if actual_morphs != EXPECTED_VISIBLE_MORPHS:
        errors.append(
            f"Proxy head actual morph ledger differs: {actual_morphs} != "
            f"{EXPECTED_VISIBLE_MORPHS}")
    visible_metadata = str(unreal.EditorAssetLibrary.get_metadata_tag(
        head, "DG_VisibleMorphs"))
    deferred_metadata = str(unreal.EditorAssetLibrary.get_metadata_tag(
        head, "DG_DeferredVisualMorphs"))
    if visible_metadata != "|".join(EXPECTED_VISIBLE_MORPHS):
        errors.append("Proxy head visible-morph metadata differs")
    if deferred_metadata != "|".join(EXPECTED_DEFERRED_MORPHS):
        errors.append("Proxy head deferred-morph metadata differs")
    bounds = head.get_bounds().box_extent
    # Import converts meters to centimeters.  The shell must be large enough
    # to hide the accepted block head, but remain a head-scale proxy.
    if not (14.0 <= float(bounds.x) <= 30.0
            and 13.0 <= float(bounds.y) <= 30.0
            and 17.0 <= float(bounds.z) <= 35.0):
        errors.append(
            "Proxy head bounds do not satisfy the enclosing head-scale gate: "
            f"{float(bounds.x)}, {float(bounds.y)}, {float(bounds.z)}")
    return head


def _validate_items_and_meshes(spec: dict, errors: list[str]) -> tuple[list, list]:
    item_objects = []
    mesh_objects = []
    hair_material_path = _object_path(spec["hair_material_asset"])
    static_definitions = {
        item["item_id"]: item for item in spec["items"] if item["mesh_kind"] == "Static"
    }
    static_meshes = {}
    for item_id, definition in static_definitions.items():
        mesh = _load_asset(definition["mesh_asset"], unreal.StaticMesh, errors)
        if not mesh:
            continue
        static_meshes[item_id] = mesh
        mesh_objects.append(mesh)
        _metadata(mesh, errors, item_id)
        if _material_slot_paths(mesh, False) != [hair_material_path]:
            errors.append(f"Static cosmetic material slot differs: {item_id}")
        bounds = mesh.get_bounds().box_extent
        if not all(math.isfinite(float(value)) and 0.05 < float(value) < 100.0
                   for value in (bounds.x, bounds.y, bounds.z)):
            errors.append(
                f"Static cosmetic has invalid head-local bounds: {item_id} "
                f"{float(bounds.x)}, {float(bounds.y)}, {float(bounds.z)}")

    searchable = []
    for definition in spec["items"]:
        item_id = definition["item_id"]
        item = _load_asset(definition["data_asset"], unreal.DiscGolfCosmeticItem, errors)
        if not item:
            continue
        item_objects.append(item)
        _metadata(item, errors, item_id)
        actual_id = str(item.get_editor_property("item_id"))
        if actual_id != item_id:
            errors.append(f"ItemId differs: {actual_id!r} != {item_id!r}")
        if str(item.get_editor_property("display_name")) != definition["display_name"]:
            errors.append(f"DisplayName differs: {item_id}")
        if _enum_token(item.get_editor_property("kind")) != \
                _expected_enum_token(definition["kind"]):
            errors.append(f"Cosmetic kind differs: {item_id}")
        if _soft_path(item.get_editor_property("skeletal_mesh")):
            errors.append(f"Catalog item unexpectedly owns a skeletal mesh: {item_id}")
        actual_static = _soft_path(item.get_editor_property("static_mesh"))
        expected_static = (
            _object_path(definition["mesh_asset"])
            if definition["mesh_kind"] == "Static" else "")
        if actual_static != expected_static:
            errors.append(
                f"Static mesh assignment differs: {item_id} "
                f"{actual_static!r} != {expected_static!r}")
        if _name_text(item.get_editor_property("attach_socket")) != \
                definition["attach_socket"]:
            errors.append(f"AttachSocket differs: {item_id}")
        if definition["mesh_kind"] == "Static" and definition["attach_socket"] != "head":
            errors.append(f"Static cosmetic escaped the head attachment: {item_id}")
        if not _transform_is_identity(
                item.get_editor_property("relative_attachment_transform")):
            errors.append(f"Relative attachment is not Identity: {item_id}")
        if _name_text(item.get_editor_property("material_variant_id")) != \
                definition["material_variant_id"]:
            errors.append(f"MaterialVariantId differs: {item_id}")
        if not _near(item.get_editor_property("min_height_cm"), spec["height_range_cm"][0]) \
                or not _near(item.get_editor_property("max_height_cm"),
                             spec["height_range_cm"][1]):
            errors.append(f"Height compatibility range differs: {item_id}")
        searchable.extend((
            item_id, definition["display_name"], definition["data_asset"],
            definition["mesh_asset"], definition["material_variant_id"],
        ))

    searchable_text = "\n".join(searchable).casefold()
    for token in FORBIDDEN_BRAND_TOKENS:
        if token in searchable_text:
            errors.append(f"Forbidden brand/vendor token in live proxy catalog: {token}")
    return item_objects, mesh_objects


def _validate_catalog_and_namespace(spec: dict, errors: list[str]):
    catalog = _load_asset(spec["catalog_asset"], unreal.DiscGolfCosmeticCatalog, errors)
    if catalog:
        _metadata(catalog, errors)
        actual_paths = [_soft_path(value) for value in catalog.get_editor_property("items")]
        expected_paths = [_object_path(item["data_asset"]) for item in spec["items"]]
        if actual_paths != expected_paths:
            errors.append("Cosmetic catalog exact item order/path ledger differs")
        resolved = []
        for item_id in EXPECTED_IDS:
            item = catalog.find_by_id(item_id)
            resolved.append(str(item.get_editor_property("item_id")) if item else "")
        if resolved != EXPECTED_IDS:
            errors.append("Cosmetic catalog FindById does not resolve every stable ID exactly")
        if catalog.find_by_id("not_a_session7_cosmetic") is not None:
            errors.append("Cosmetic catalog FindById resolves an unknown ID")

    expected_customization = {
        _object_path(spec["head_mesh_asset"]), _object_path(spec["catalog_asset"]),
        *(_object_path(item["data_asset"]) for item in spec["items"]),
        *(_object_path(item["mesh_asset"]) for item in spec["items"]
          if item["mesh_kind"] == "Static"),
    }
    actual_customization = {
        str(path) for path in unreal.EditorAssetLibrary.list_assets(
            "/Game/DiscGolf/Characters/Customization", recursive=True,
            include_folder=False)
    }
    if actual_customization != expected_customization:
        errors.append(
            "Unexpected/missing customization namespace assets: "
            f"missing={sorted(expected_customization - actual_customization)} "
            f"unexpected={sorted(actual_customization - expected_customization)}")
    expected_materials = {
        _object_path(spec["head_material_asset"]),
        _object_path(spec["hair_material_asset"]),
    }
    actual_materials = {
        str(path) for path in unreal.EditorAssetLibrary.list_assets(
            "/Game/DiscGolf/Materials/CharacterCustomization", recursive=True,
            include_folder=False)
    }
    if actual_materials != expected_materials:
        errors.append(
            "Unexpected/missing customization material assets: "
            f"missing={sorted(expected_materials - actual_materials)} "
            f"unexpected={sorted(actual_materials - expected_materials)}")
    return catalog, actual_customization, actual_materials


def validate() -> dict:
    errors: list[str] = []
    spec = _load_json(SPEC_PATH) if SPEC_PATH.is_file() else {}
    manifest = _load_json(MANIFEST_PATH) if MANIFEST_PATH.is_file() else {}
    if not SPEC_PATH.is_file():
        errors.append(f"Catalog spec missing: {SPEC_PATH}")
    if not MANIFEST_PATH.is_file():
        errors.append(f"Generated source manifest missing: {MANIFEST_PATH}")
    source_result = {}
    try:
        namespace = runpy.run_path(
            str(SOURCE_VALIDATOR), run_name="dg_session7_asset_source_validation")
        source_result = namespace["validate"]()
        if source_result.get("status") != "PASS" \
                or not source_result.get("acceptance_ready"):
            errors.append(
                "Generated proxy source is not acceptance-ready: "
                f"{source_result.get('status')} {source_result.get('source_status')}")
    except Exception as error:
        errors.append(
            f"Strict proxy source validation failed: {type(error).__name__}: {error}")

    author_contract = runpy.run_path(
        str(AUTHOR_SCRIPT), run_name="dg_session7_asset_author_contract")
    protected_before = _protected_hashes(author_contract, errors)
    outputs_before = _output_hashes(spec, errors) if spec else {}

    if [item.get("item_id") for item in spec.get("items", [])] != EXPECTED_IDS:
        errors.append("Frozen 17-item catalog stable-ID order differs")
    if list(spec.get("visible_morphs", {}).values()) != EXPECTED_VISIBLE_MORPHS:
        errors.append("Frozen exact five visible morphs differ")
    if list(spec.get("deferred_visual_morphs", {}).values()) != EXPECTED_DEFERRED_MORPHS:
        errors.append("Frozen explicit 15 deferred morphs differ")
    if manifest.get("status") != "PASS_GENERATED_SOURCE_FIXTURE" \
            or manifest.get("shipping_status") != "DO_NOT_SHIP" \
            or manifest.get("brand_id") != "dg_generic" \
            or manifest.get("external_sources") != [] \
            or manifest.get("logos") != [] \
            or manifest.get("production_license_claim") != "NONE":
        errors.append("Generated source provenance is not exact generic DO_NOT_SHIP")

    _head_material, _hair_material = _validate_materials(spec, errors)
    head = _validate_head(spec, author_contract, errors)
    items, static_meshes = _validate_items_and_meshes(spec, errors)
    catalog, customization_assets, material_assets = \
        _validate_catalog_and_namespace(spec, errors)

    protected_after = _protected_hashes(author_contract, errors)
    outputs_after = _output_hashes(spec, errors) if spec else {}
    disk_mutation = protected_before != protected_after or outputs_before != outputs_after
    if disk_mutation:
        errors.append("Read-only validation observed a protected/output package hash change")

    result = {
        "schema": "DiscGolfTour.Session7CustomizationAssetValidation.v1",
        "status": "PASS_NO_DISK_MUTATION" if not errors else "FAIL",
        "content_status": spec.get("content_status"),
        "shipping_status": spec.get("shipping_status"),
        "brand_id": spec.get("brand_id"),
        "catalog": spec.get("catalog_asset"),
        "catalog_item_ids": EXPECTED_IDS,
        "catalog_item_count": len(items),
        "head_mesh_count": 1 if head else 0,
        "static_mesh_count": len(static_meshes),
        "visible_morphs": EXPECTED_VISIBLE_MORPHS,
        "deferred_visual_morphs": EXPECTED_DEFERRED_MORPHS,
        "head_surface_scope": "VISIBLE_PROXY_ACCEPTANCE_HEAD_ONLY",
        "body_surface_scope": "DEFERRED_PRODUCTION_MESH_MATERIAL_REQUIRED",
        "tattoo_none_persistence": spec.get("tattoo_none_persistence"),
        "catalog_loaded": catalog is not None,
        "customization_namespace_asset_count": len(customization_assets),
        "material_namespace_asset_count": len(material_assets),
        "source_validation_status": source_result.get("source_status"),
        "protected_assets": protected_after,
        "output_package_hashes": outputs_after,
        "disk_mutation": "NONE" if not disk_mutation else "DETECTED",
        "asset_save_calls": 0,
        "asset_import_calls": 0,
        "asset_factory_calls": 0,
        "asset_delete_calls": 0,
        "errors": errors,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main() -> None:
    result = validate()
    if result["status"] != "PASS_NO_DISK_MUTATION":
        raise RuntimeError(
            "Session 7 strict customization validation failed: "
            + "; ".join(result["errors"]))
    unreal.log(
        "DG_SESSION7_CUSTOMIZATION_VALIDATION: PASS_NO_DISK_MUTATION "
        f"items={result['catalog_item_count']} head=1 static={result['static_mesh_count']} "
        "visible=5 deferred=15")


if __name__ == "__main__":
    main()
