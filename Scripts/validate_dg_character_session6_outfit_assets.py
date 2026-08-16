"""Strict read-only validation for the live Session 6 outfit proxy assets.

The module exposes ``validate()`` for the authoring utility and runs it from
``__main__`` for commandlet validation. It never calls a package save, Modify,
PostEditChange, material recompile, import, delete, rename, or asset factory.
"""

from __future__ import annotations

import hashlib
import importlib.util
import json
import math
import re
import runpy
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "DiscGolf" / "Outfits" / "Proxy"
SPEC_PATH = SOURCE_ROOT / "proxy_outfit_catalog_spec.json"
MANIFEST_PATH = SOURCE_ROOT / "proxy_outfit_source_manifest.json"
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session6OutfitAssetValidation.json"
AUTHOR_SCRIPT = PROJECT_ROOT / "Scripts" / "create_dg_character_session6_outfit_assets.py"
SOURCE_VALIDATOR = PROJECT_ROOT / "Scripts" / "validate_dg_character_session6_proxy_source.py"

EXPECTED_METADATA = {
    "DG_Session": "6",
    "DG_ContentStatus": "NON_PRODUCTION_PROXY",
    "DG_ShippingStatus": "DO_NOT_SHIP",
    "DG_BrandId": "dg_generic",
    "DG_BrandStatus": "GENERIC_UNBRANDED",
    "DG_SourceKind": "PROJECT_LOCAL_GENERATED_BLOCKOUT",
    "DG_ProductionApproved": "false",
}

FORBIDDEN_BRAND_TOKENS = (
    "adidas", "discraft", "discmania", "dynamic discs", "innova", "latitude 64",
    "mvp disc", "nike", "oakley", "prodigy disc", "premium disc golf",
    "under armour", "westside discs",
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
    return object_or_package_path.rsplit(".", 1)[0] if "." in leaf else object_or_package_path


def _object_path(object_or_package_path: str) -> str:
    package = _package_path(object_or_package_path)
    return f"{package}.{package.rsplit('/', 1)[-1]}"


def _uasset_path(object_or_package_path: str) -> Path:
    package = _package_path(object_or_package_path)
    return PROJECT_ROOT / "Content" / (package[len("/Game/"):] + ".uasset")


def _load_asset(path: str, expected_type, errors: list[str]):
    asset = unreal.EditorAssetLibrary.load_asset(_package_path(path))
    if not isinstance(asset, expected_type):
        errors.append(
            f"Expected {expected_type.__name__} at {_object_path(path)}, found {asset}")
        return None
    return asset


def _enum_token(value) -> str:
    # UE 5.8's Python repr for reflected enum values is, for example,
    # ``<DGOutfitSlot.HEADWEAR: 0>`` rather than just ``HEADWEAR``.  Compare
    # the symbolic member name and deliberately discard the numeric value.
    name = getattr(value, "name", None)
    if callable(name):
        name = name()
    text = str(name) if name else str(value).rsplit(".", 1)[-1].split(":", 1)[0]
    return re.sub(r"[^A-Za-z0-9]", "", text).casefold()


def _expected_enum_token(display_name: str) -> str:
    return display_name.replace("_", "").casefold()


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


def _near(a, b, tolerance=1.0e-5) -> bool:
    return math.isfinite(float(a)) and abs(float(a) - float(b)) <= tolerance


def _color_matches(color, values) -> bool:
    return all(_near(component, expected) for component, expected in zip(
        (color.r, color.g, color.b, color.a), values))


def _transform_is_identity(transform) -> bool:
    translation = transform.translation
    rotation = transform.rotation
    scale = transform.scale3d
    return (
        _near(translation.x, 0.0) and _near(translation.y, 0.0)
        and _near(translation.z, 0.0)
        and _near(rotation.x, 0.0) and _near(rotation.y, 0.0)
        and _near(rotation.z, 0.0) and abs(abs(float(rotation.w)) - 1.0) <= 1.0e-5
        and _near(scale.x, 1.0) and _near(scale.y, 1.0) and _near(scale.z, 1.0)
    )


def _metadata(asset, errors: list[str], item_id: str = "") -> None:
    for key, expected in EXPECTED_METADATA.items():
        actual = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key))
        if actual != expected:
            errors.append(
                f"{asset.get_path_name()} metadata {key}={actual!r}, expected {expected!r}")
    source = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, "DG_SourceManifest"))
    if source != "SourceArt/DiscGolf/Outfits/Proxy/proxy_outfit_source_manifest.json":
        errors.append(f"{asset.get_path_name()} has wrong source-manifest metadata: {source}")
    if item_id:
        actual = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, "DG_ItemId"))
        if actual != item_id:
            errors.append(f"{asset.get_path_name()} ItemId metadata {actual} != {item_id}")


def _load_author_contract():
    spec = importlib.util.spec_from_file_location("dg_session6_author_contract", AUTHOR_SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load authoring contract: {AUTHOR_SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _protected_hashes(contract, errors: list[str]) -> dict[str, str]:
    result = {}
    for relative, expected in contract.PROTECTED_ASSETS.items():
        path = PROJECT_ROOT / relative
        if not path.is_file():
            errors.append(f"Protected accepted package missing: {relative}")
            continue
        actual = _sha256(path)
        result[relative] = actual
        if actual != expected:
            errors.append(f"Protected accepted package hash changed: {relative}")
    return result


def _output_hashes(spec: dict, errors: list[str]) -> dict[str, str]:
    paths = (
        [spec["material_asset"], spec["catalog_asset"]]
        + [item["mesh_asset"] for item in spec["items"]]
        + [item["data_asset"] for item in spec["items"]]
    )
    result = {}
    for object_path in paths:
        disk = _uasset_path(object_path)
        if not disk.is_file():
            errors.append(f"Expected Session 6 package missing: {disk}")
            continue
        result[_object_path(object_path)] = _sha256(disk)
    return result


def validate() -> dict:
    errors: list[str] = []
    spec = _load_json(SPEC_PATH)
    manifest = _load_json(MANIFEST_PATH) if MANIFEST_PATH.is_file() else {}
    if not MANIFEST_PATH.is_file():
        errors.append(f"Generated source manifest is missing: {MANIFEST_PATH}")
    try:
        source_namespace = runpy.run_path(
            str(SOURCE_VALIDATOR), run_name="dg_session6_asset_source_validation")
        source_namespace["main"]()
    except Exception as error:
        errors.append(f"Strict proxy source validation failed: {type(error).__name__}: {error}")
    author_contract = _load_author_contract()
    protected_before = _protected_hashes(author_contract, errors)
    outputs_before = _output_hashes(spec, errors)

    if spec.get("catalog_asset") != \
            "/Game/DiscGolf/Outfits/Data/DA_DG_OutfitCatalog.DA_DG_OutfitCatalog":
        errors.append("Catalog object path differs from runtime authority")
    if manifest.get("shipping_status") != "DO_NOT_SHIP" \
            or manifest.get("brand_id") != "dg_generic" \
            or manifest.get("external_sources") != [] \
            or manifest.get("logos") != []:
        errors.append("Generated source provenance is not generic DO_NOT_SHIP")

    material = _load_asset(spec["material_asset"], unreal.Material, errors)
    if material:
        _metadata(material, errors)
        if not unreal.MaterialEditingLibrary.has_material_usage(
                material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH):
            errors.append("M_DG_OutfitProxy lacks required SkeletalMesh material usage")
        actual_parameters = str(unreal.EditorAssetLibrary.get_metadata_tag(
            material, "DG_MaterialParameters"))
        if actual_parameters != \
                "DG_PrimaryColor|DG_SecondaryColor|DG_AccentColor|DG_RoughnessBias":
            errors.append("M_DG_OutfitProxy parameter metadata differs")
        defaults = spec["variants"][0]
        for name, field in (
            ("DG_PrimaryColor", "primary_color"),
            ("DG_SecondaryColor", "secondary_color"),
            ("DG_AccentColor", "accent_color"),
        ):
            actual = unreal.MaterialEditingLibrary.get_material_default_vector_parameter_value(
                material, name)
            if not _color_matches(actual, defaults[field]):
                errors.append(f"Material default vector parameter differs: {name}")
        roughness = unreal.MaterialEditingLibrary.get_material_default_scalar_parameter_value(
            material, "DG_RoughnessBias")
        if not _near(roughness, defaults["roughness_bias"]):
            errors.append("Material default scalar parameter differs: DG_RoughnessBias")

    master_skeleton = _load_asset(spec["master_skeleton_asset"], unreal.Skeleton, errors)
    master_bones = set()
    if master_skeleton:
        master_bones = {
            str(name) for name in master_skeleton.get_reference_pose().get_bone_names()}
        if len(master_bones) != 69:
            errors.append(f"Accepted master skeleton has {len(master_bones)} bones, expected 69")

    item_objects = []
    searchable_strings = []
    for definition in spec["items"]:
        item = _load_asset(definition["data_asset"], unreal.DiscGolfOutfitItem, errors)
        if not item:
            continue
        item_objects.append(item)
        _metadata(item, errors, definition["item_id"])
        if str(item.get_editor_property("item_id")) != definition["item_id"]:
            errors.append(f"ItemId differs: {definition['item_id']}")
        if str(item.get_editor_property("display_name")) != definition["display_name"]:
            errors.append(f"DisplayName differs: {definition['item_id']}")
        if _enum_token(item.get_editor_property("slot")) != \
                _expected_enum_token(definition["slot"]):
            errors.append(f"Slot differs: {definition['item_id']}")
        if not _near(item.get_editor_property("min_height_cm"), 150.0) \
                or not _near(item.get_editor_property("max_height_cm"), 210.0):
            errors.append(f"Height range differs: {definition['item_id']}")
        if list(item.get_editor_property("conflicting_slots")):
            errors.append(f"Proxy conflicts must remain empty: {definition['item_id']}")
        if bool(item.get_editor_property("bind_cloth_to_leader_pose")):
            errors.append(f"Proxy cloth binding must remain disabled: {definition['item_id']}")
        if not _transform_is_identity(
                item.get_editor_property("relative_attachment_transform")):
            errors.append(f"Relative attachment is not Identity: {definition['item_id']}")

        actual_coverage = [
            _enum_token(value) for value in item.get_editor_property("covered_body_regions")]
        expected_coverage = [
            _expected_enum_token(value) for value in definition["covered_regions"]]
        if actual_coverage != expected_coverage:
            errors.append(
                f"Coverage differs: {definition['item_id']} {actual_coverage} != {expected_coverage}")

        skeletal_path = _soft_path(item.get_editor_property("skeletal_mesh"))
        static_path = _soft_path(item.get_editor_property("static_mesh"))
        if definition["mesh_kind"] == "Skeletal":
            if skeletal_path != _object_path(definition["mesh_asset"]) or static_path:
                errors.append(f"Skeletal/static mesh assignment differs: {definition['item_id']}")
            if str(item.get_editor_property("attach_socket")) not in ("", "None"):
                errors.append(f"Skeletal proxy has a static socket: {definition['item_id']}")
            if not bool(item.get_editor_property("use_leader_pose")):
                errors.append(f"Skeletal proxy disabled Leader Pose: {definition['item_id']}")
        else:
            if static_path != _object_path(definition["mesh_asset"]) or skeletal_path:
                errors.append(f"Static/skeletal mesh assignment differs: {definition['item_id']}")
            socket = str(item.get_editor_property("attach_socket"))
            if socket != definition["attach_socket"] or socket not in master_bones:
                errors.append(f"Static attach target differs/missing: {definition['item_id']}")
            if socket in {"disc_grip_l", "disc_grip_r"}:
                errors.append(
                    f"Cosmetic proxy attached to gameplay grip bone: {definition['item_id']}")
            if bool(item.get_editor_property("use_leader_pose")):
                errors.append(f"Static proxy incorrectly enabled Leader Pose: {definition['item_id']}")

        variants = list(item.get_editor_property("variants"))
        if [str(variant.get_editor_property("variant_id")) for variant in variants] \
                != ["Default", "Graphite", "Teal"]:
            errors.append(f"Variant IDs/order differ: {definition['item_id']}")
        for variant, expected in zip(variants, spec["variants"]):
            overrides = list(variant.get_editor_property("material_overrides"))
            if len(overrides) != 1 or _soft_path(overrides[0]) != _object_path(spec["material_asset"]):
                errors.append(
                    f"Material override differs: {definition['item_id']} {expected['variant_id']}")
            vectors = dict(variant.get_editor_property("vector_parameters"))
            scalars = dict(variant.get_editor_property("scalar_parameters"))
            expected_vectors = {
                "DG_PrimaryColor": expected["primary_color"],
                "DG_SecondaryColor": expected["secondary_color"],
                "DG_AccentColor": expected["accent_color"],
            }
            if {str(key) for key in vectors} != set(expected_vectors):
                errors.append(
                    f"Vector parameter names differ: {definition['item_id']} {expected['variant_id']}")
            for name, expected_value in expected_vectors.items():
                actual_value = next((value for key, value in vectors.items()
                                     if str(key) == name), None)
                if actual_value is None or not _color_matches(actual_value, expected_value):
                    errors.append(
                        f"Vector parameter differs: {definition['item_id']} "
                        f"{expected['variant_id']} {name}")
            actual_bias = next((value for key, value in scalars.items()
                                if str(key) == "DG_RoughnessBias"), None)
            if {str(key) for key in scalars} != {"DG_RoughnessBias"} \
                    or actual_bias is None \
                    or not _near(actual_bias, expected["roughness_bias"]):
                errors.append(
                    f"Scalar parameter differs: {definition['item_id']} {expected['variant_id']}")
        searchable_strings.extend((
            definition["item_id"], definition["display_name"],
            definition["data_asset"], definition["mesh_asset"],
        ))

    meshes = []
    for definition in spec["items"]:
        expected_type = (
            unreal.SkeletalMesh if definition["mesh_kind"] == "Skeletal" else unreal.StaticMesh)
        mesh = _load_asset(definition["mesh_asset"], expected_type, errors)
        if not mesh:
            continue
        meshes.append(mesh)
        _metadata(mesh, errors)
        bounds = mesh.get_bounds()
        extents = (bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z)
        if not all(math.isfinite(float(value)) and 0.1 < float(value) < 250.0
                   for value in extents):
            errors.append(f"Proxy mesh bounds are invalid: {mesh.get_path_name()} {extents}")
        if definition["mesh_kind"] == "Skeletal":
            if len(mesh.get_editor_property("materials")) != 1:
                errors.append(
                    f"Proxy skeletal mesh must expose one material slot: {mesh.get_path_name()}")
            if not mesh.has_vertex_colors():
                errors.append(
                    f"Proxy skeletal mesh lost its role vertex colors: {mesh.get_path_name()}")
            skeleton = mesh.get_editor_property("skeleton")
            if not skeleton or skeleton.get_path_name() != _object_path(spec["master_skeleton_asset"]):
                errors.append(f"Follower skeleton differs: {mesh.get_path_name()}")
            physics_asset = mesh.get_editor_property("physics_asset")
            if physics_asset:
                errors.append(f"Proxy skeletal mesh unexpectedly has PhysicsAsset: {mesh.get_path_name()}")
        elif len(mesh.get_editor_property("static_materials")) != 1:
            errors.append(
                f"Proxy static mesh must expose one material slot: {mesh.get_path_name()}")

    catalog = _load_asset(spec["catalog_asset"], unreal.DiscGolfOutfitCatalog, errors)
    if catalog:
        _metadata(catalog, errors)
        actual_catalog_paths = [
            _soft_path(value) for value in catalog.get_editor_property("items")]
        expected_catalog_paths = [
            _object_path(item["data_asset"]) for item in spec["items"]]
        if actual_catalog_paths != expected_catalog_paths:
            errors.append("Catalog item order/path set differs from frozen spec")
        resolved_ids = []
        for definition in spec["items"]:
            found = catalog.find_item_by_id(definition["item_id"])
            resolved_ids.append(str(found.get_editor_property("item_id")) if found else "")
        if resolved_ids != [definition["item_id"] for definition in spec["items"]]:
            errors.append("Catalog FindItemById does not resolve every stable ItemId exactly")

    expected_outfit_assets = {
        _object_path(spec["catalog_asset"]),
        *(_object_path(item["mesh_asset"]) for item in spec["items"]),
        *(_object_path(item["data_asset"]) for item in spec["items"]),
    }
    actual_outfit_assets = {
        str(path) for path in unreal.EditorAssetLibrary.list_assets(
            "/Game/DiscGolf/Outfits", recursive=True, include_folder=False)}
    if actual_outfit_assets != expected_outfit_assets:
        errors.append(
            "Unexpected/missing /Game/DiscGolf/Outfits assets: "
            f"missing={sorted(expected_outfit_assets - actual_outfit_assets)} "
            f"unexpected={sorted(actual_outfit_assets - expected_outfit_assets)}")

    searchable = "\n".join(searchable_strings).casefold()
    for token in FORBIDDEN_BRAND_TOKENS:
        if token in searchable:
            errors.append(f"Forbidden brand token in live proxy catalog: {token}")

    protected_after = _protected_hashes(author_contract, errors)
    outputs_after = _output_hashes(spec, errors)
    disk_mutation = protected_after != protected_before or outputs_after != outputs_before
    if disk_mutation:
        errors.append("Read-only validation observed a protected/output package hash change")

    result = {
        "schema": "DiscGolfTour.Session6OutfitAssetValidation.v1",
        "status": "PASS_NO_DISK_MUTATION" if not errors else "FAIL",
        "content_status": spec.get("content_status"),
        "shipping_status": spec.get("shipping_status"),
        "brand_id": spec.get("brand_id"),
        "catalog": spec.get("catalog_asset"),
        "item_count": len(item_objects),
        "skeletal_mesh_count": sum(isinstance(mesh, unreal.SkeletalMesh) for mesh in meshes),
        "static_mesh_count": sum(isinstance(mesh, unreal.StaticMesh) for mesh in meshes),
        "variant_ids": [entry["variant_id"] for entry in spec.get("variants", [])],
        "slot_conflict_policy": "ONE_PER_SLOT;NO_CROSS_SLOT_PROXY_CONFLICTS;TOP_PLUS_OUTERWEAR_ALLOWED",
        "body_coverage_policy": "METADATA_UNION_ONLY_PROXY_BODY_HAS_NO_REGION_MASK_ART",
        "bag_boundary": "STATIC_COSMETIC;spine_04;IDENTITY_LOCAL;NO_INVENTORY_AUTHORITY",
        "protected_assets": protected_after,
        "output_package_hashes": outputs_after,
        "disk_mutation": "NONE" if not disk_mutation else "DETECTED",
        "asset_save_calls": 0,
        "asset_import_calls": 0,
        "errors": errors,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main() -> None:
    result = validate()
    if result["status"] != "PASS_NO_DISK_MUTATION":
        raise RuntimeError(
            "Session 6 strict outfit validation failed: " + "; ".join(result["errors"]))
    unreal.log(
        "DG_SESSION6_OUTFIT_VALIDATION: PASS_NO_DISK_MUTATION "
        f"items={result['item_count']} skeletal={result['skeletal_mesh_count']} "
        f"static={result['static_mesh_count']}")


if __name__ == "__main__":
    main()
