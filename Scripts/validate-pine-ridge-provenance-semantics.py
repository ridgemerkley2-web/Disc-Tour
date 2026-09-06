"""Unreal-side semantic validation for the durable Pine Ridge provenance receipt."""

from pathlib import Path
import hashlib
import json
import unreal


ROOT = Path(unreal.Paths.project_dir())
ROOT_RESOLVED = ROOT.resolve()
CONTENT_ROOT = "/Game/Presentation/Course/PineRidge"
RECEIPT_PATH = ROOT / "SourceArt" / "PineRidge" / "PolyHaven" / "derived_runtime_receipt.json"
OUTPUT = ROOT / "Saved" / "Provenance" / "Session18PineRidgeSemanticValidation.json"


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def md5_file(path):
    digest = hashlib.md5()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def package_from_object_path(object_path):
    return object_path.rsplit(".", 1)[0]


def content_file_for_package(package_path):
    value = str(package_path)
    return f"Content/{value[len('/Game/'): ]}.uasset"


def canonical_import_sources(asset, issues, object_path):
    filenames = []
    try:
        import_data = asset.get_editor_property("asset_import_data")
        if import_data:
            filenames = list(import_data.extract_filenames())
    except Exception:
        filenames = []
    result = []
    for filename in filenames:
        source = Path(str(filename))
        try:
            result.append(source.resolve().relative_to(ROOT_RESOLVED).as_posix())
        except ValueError:
            issues.append(f"import source escapes project for {object_path}: {source}")
    return sorted(result)


receipt = json.loads(RECEIPT_PATH.read_text(encoding="utf-8-sig"))
source_by_path = {entry["relativePath"]: entry for entry in receipt["sourceFiles"]}
derived_by_object = {entry["objectPath"]: entry for entry in receipt["derivedRuntimeArtifacts"]}
excluded_by_object = {
    entry["objectPath"]: entry
    for entry in receipt["excludedProjectOriginalRuntimeArtifacts"]
}
declared_by_object = dict(derived_by_object)
declared_by_object.update(excluded_by_object)

registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions()
options.set_editor_property("include_soft_package_references", True)
options.set_editor_property("include_hard_package_references", True)
options.set_editor_property("include_searchable_names", False)
options.set_editor_property("include_soft_management_references", False)
options.set_editor_property("include_hard_management_references", False)

observed_objects = sorted(
    unreal.EditorAssetLibrary.list_assets(CONTENT_ROOT, recursive=True, include_folder=False)
)
issues = []
if set(observed_objects) != set(declared_by_object):
    issues.append(
        f"runtime object partition differs: undeclared={sorted(set(observed_objects) - set(declared_by_object))} "
        f"missing={sorted(set(declared_by_object) - set(observed_objects))}"
    )

rows = []
for object_path in observed_objects:
    declaration = declared_by_object.get(object_path)
    if declaration is None:
        continue
    asset = unreal.EditorAssetLibrary.load_asset(object_path)
    if asset is None:
        issues.append(f"asset does not load: {object_path}")
        continue
    actual_class = asset.get_class().get_name()
    if actual_class != declaration["assetClass"]:
        issues.append(
            f"asset class differs for {object_path}: expected={declaration['assetClass']} actual={actual_class}"
        )

    package_name = package_from_object_path(object_path)
    scoped_dependencies = sorted(
        content_file_for_package(value)
        for value in registry.get_dependencies(package_name, options)
        if str(value).startswith(CONTENT_ROOT + "/")
    )
    expected_dependencies = declaration.get("runtimeDependencies")
    if expected_dependencies is not None and scoped_dependencies != expected_dependencies:
        issues.append(
            f"scoped dependencies differ for {object_path}: "
            f"expected={expected_dependencies} actual={scoped_dependencies}"
        )

    import_sources = canonical_import_sources(asset, issues, object_path)
    expected_sources = (
        declaration.get("directSourceRelativePaths", [])
        if object_path in derived_by_object else
        declaration.get("directProjectSourceRelativePaths", [])
    )
    if import_sources != expected_sources:
        issues.append(
            f"import sources differ for {object_path}: expected={expected_sources} actual={import_sources}"
        )
    for source_relative in import_sources:
        source_record = source_by_path.get(source_relative)
        if source_record is None and object_path in derived_by_object:
            issues.append(f"import source is not manifest-bound for {object_path}: {source_relative}")
            continue
        source_path = ROOT / source_relative
        if source_record is not None and md5_file(source_path) != source_record["md5"]:
            issues.append(f"import source MD5 differs for {object_path}: {source_relative}")
        if object_path in excluded_by_object:
            project_inputs = {
                item["relativePath"]: item
                for item in declaration.get("projectInputs", [])
            }
            input_record = project_inputs.get(source_relative)
            if input_record is None or sha256_file(source_path) != input_record.get("sha256"):
                issues.append(
                    f"project import source is not hash-bound for {object_path}: {source_relative}"
                )

    observed_semantics = {}
    if isinstance(asset, unreal.Texture2D):
        observed_semantics = {
            "compression": str(asset.get_editor_property("compression_settings")),
            "srgb": asset.get_editor_property("srgb"),
        }
        if "filter" in declaration.get("semanticSettings", {}):
            observed_semantics.update({
                "filter": str(asset.get_editor_property("filter")),
                "mipGenSettings": str(asset.get_editor_property("mip_gen_settings")),
            })
    elif isinstance(asset, unreal.StaticMesh):
        observed_semantics = {"lodCount": asset.get_num_lods()}
    expected_semantics = declaration.get("semanticSettings")
    if expected_semantics is not None and observed_semantics != expected_semantics:
        issues.append(
            f"semantic settings differ for {object_path}: "
            f"expected={expected_semantics} actual={observed_semantics}"
        )

    rows.append({
        "objectPath": object_path,
        "assetClass": actual_class,
        "scopedDependencies": scoped_dependencies,
        "importSourceRelativePaths": import_sources,
        "semanticSettings": observed_semantics,
    })

report = {
    "schema": "DiscGolfTour.PineRidgeProvenanceSemanticValidation.v1",
    "receiptRelativePath": RECEIPT_PATH.relative_to(ROOT).as_posix(),
    "receiptSha256": sha256_file(RECEIPT_PATH),
    "engineVersion": "5.8.1",
    "engineChangelist": 56057345,
    "passed": not issues,
    "assetCount": len(rows),
    "derivedAssetCount": len(derived_by_object),
    "excludedProjectOriginalCount": len(excluded_by_object),
    "assets": rows,
    "issues": issues,
}
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
OUTPUT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
if issues:
    for issue in issues:
        unreal.log_error(f"PINE_RIDGE_PROVENANCE_SEMANTICS: {issue}")
    raise RuntimeError(f"Pine Ridge provenance semantic validation failed: {len(issues)} issues")
unreal.log(
    f"PINE_RIDGE_PROVENANCE_SEMANTICS: PASS assets={len(rows)} "
    f"receiptSha256={report['receiptSha256']} output={OUTPUT}"
)
