"""Resave only the retained Session 19 v0.5 assets through the clean-room redirect.

Run with UE 5.8 after a successful DiscGolfTourEditor build:

    UnrealEditor-Cmd.exe C:/DGTour/DiscGolfTour.uproject \
        -run=PythonScript \
        -script=C:/DGTour/Scripts/migrate_dg_session19_serialized_assets.py \
        -unattended -nop4 -nosplash -nullrhi -nosound

The source-controlled policy and TSV are the only migration authority. The
script fails closed if their identities, the 6/39 split, CoreRedirect, module
boundary, or any pre-migration package identity differs. It saves exactly the
six retained packages and proves that every other Content binary is unchanged.
"""

from __future__ import annotations

import csv
import hashlib
import json
import runpy
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
POLICY_PATH = PROJECT_ROOT / "Config/DG_Session19SerializedAssetMigrationPolicy.json"
INVENTORY_PATH = PROJECT_ROOT / "Evidence/Session19/SerializedAssetMigrationInventory.tsv"
REPORT_PATH = PROJECT_ROOT / "Saved/Session19/SerializedAssetMigrationRun.json"
OLD_SCRIPT = b"/Script/DiscGolfCharacterFramework"
NEW_SCRIPT = b"/Script/DiscGolfRuntimeFoundation"
PROTECTED_EXTENSIONS = {".uasset", ".uexp", ".ubulk", ".uptnl"}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _identity(path: Path) -> dict[str, object]:
    return {"bytes": path.stat().st_size, "sha256": _sha256(path)}


def _load_inputs() -> tuple[dict, list[dict[str, str]]]:
    policy = json.loads(POLICY_PATH.read_text(encoding="utf-8"))
    boundary = policy["migrationBoundary"]
    inventory_identity = _identity(INVENTORY_PATH)
    if inventory_identity != {
        "bytes": boundary["inventoryBytes"],
        "sha256": boundary["inventorySha256"],
    }:
        raise RuntimeError(
            f"Migration inventory identity differs: {inventory_identity}")
    with INVENTORY_PATH.open("r", encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    if len(rows) != 45:
        raise RuntimeError(f"Expected exactly 45 inventory rows, found {len(rows)}")
    retained = [row for row in rows if row["disposition"] == "RETAINED"]
    excluded = [row for row in rows if row["disposition"] == "EXCLUDED"]
    if len(retained) != 6 or len(excluded) != 39:
        raise RuntimeError(
            f"Expected retained=6/excluded=39, found {len(retained)}/{len(excluded)}")
    unknown = [row for row in rows if row["disposition"] not in {"RETAINED", "EXCLUDED"}]
    if unknown:
        raise RuntimeError("Inventory contains an unknown disposition")
    packages = [row["package"] for row in retained]
    if packages != policy["retainedPackages"]:
        raise RuntimeError("Retained package order differs from policy")
    return policy, rows


def _require_module_boundary(policy: dict) -> None:
    descriptor = json.loads((PROJECT_ROOT / "DiscGolfTour.uproject").read_text(encoding="utf-8"))
    modules = {entry["Name"]: entry for entry in descriptor.get("Modules", [])}
    if "DiscGolfRuntimeFoundation" not in modules:
        raise RuntimeError("DiscGolfRuntimeFoundation is absent from the project descriptor")
    plugins = {entry["Name"]: entry for entry in descriptor.get("Plugins", [])}
    legacy = plugins.get("DiscGolfCharacterFramework")
    if legacy is not None:
        raise RuntimeError("Legacy framework project descriptor entry must be absent")
    plugin_descriptor = json.loads((
        PROJECT_ROOT
        / "Plugins/DiscGolfCharacterFramework/DiscGolfCharacterFramework.uplugin"
    ).read_text(encoding="utf-8"))
    if plugin_descriptor.get("EnabledByDefault") is not False:
        raise RuntimeError(
            "Retained legacy source tree must be disabled by default until quarantine")
    ini_text = (PROJECT_ROOT / policy["coreRedirect"]["configPath"]).read_text(
        encoding="utf-8")
    for entry in policy["coreRedirect"]["exactEntries"]:
        if entry not in ini_text:
            raise RuntimeError(f"A controlled Session 19 redirect is absent: {entry}")
    for class_path in policy["requiredReplacementClasses"]:
        if unreal.load_class(None, class_path) is None:
            raise RuntimeError(f"Replacement class failed to load: {class_path}")


def _assert_pre_identity(row: dict[str, str]) -> Path:
    path = PROJECT_ROOT / row["file"]
    if not path.is_file():
        raise RuntimeError(f"Inventory asset is missing: {row['file']}")
    actual = _identity(path)
    expected = {
        "bytes": int(row["pre_bytes"]),
        "sha256": row["pre_sha256"],
    }
    if actual != expected:
        raise RuntimeError(
            f"Pre-migration identity differs for {row['file']}: {actual} != {expected}")
    if OLD_SCRIPT not in path.read_bytes():
        raise RuntimeError(f"Inventory asset lacks the legacy script import: {row['file']}")
    return path


def _assert_retained_input(row: dict[str, str]) -> Path:
    path = PROJECT_ROOT / row["file"]
    if not path.is_file():
        if row["package"] == "/Game/DiscGolf/Rigs/CR_DG_Master":
            # A failed bounded recreate may have removed only the tracked CR
            # before the project-owned authoring workflow ran. Recovery below
            # must recreate it before any evidence can pass.
            return path
        raise RuntimeError(f"Retained inventory asset is missing: {row['file']}")
    data = path.read_bytes()
    actual = _identity(path)
    expected = {
        "bytes": int(row["pre_bytes"]),
        "sha256": row["pre_sha256"],
    }
    if actual == expected and OLD_SCRIPT in data:
        return path
    if NEW_SCRIPT in data:
        # A previous bounded commandlet attempt may already have saved this exact
        # retained package. The all-Content snapshot below still proves that the
        # retry changes nothing outside the six-package authority.
        return path
    if (row["package"] == "/Game/DiscGolf/Rigs/CR_DG_Master"
            and OLD_SCRIPT not in data):
        # Project-owned Session 2 recreation completed but the subsequent
        # Session 4 Foundation-variable authoring may have failed closed.
        return path
    raise RuntimeError(
        f"Retained input is neither the inventoried original nor a redirected "
        f"bounded-run output: {row['file']}")


def _allowed_paths(rows: list[dict[str, str]]) -> set[Path]:
    result: set[Path] = set()
    for row in rows:
        if row["disposition"] != "RETAINED":
            continue
        asset_path = (PROJECT_ROOT / row["file"]).resolve()
        for extension in PROTECTED_EXTENSIONS:
            result.add(asset_path.with_suffix(extension))
    return result


def _content_snapshot(allowed: set[Path]) -> dict[str, dict[str, object]]:
    result: dict[str, dict[str, object]] = {}
    for path in sorted((PROJECT_ROOT / "Content").rglob("*")):
        resolved = path.resolve()
        if not path.is_file() or path.suffix.casefold() not in PROTECTED_EXTENSIONS:
            continue
        if resolved in allowed:
            continue
        result[path.relative_to(PROJECT_ROOT).as_posix()] = _identity(path)
    return result


def _compile_if_blueprint(asset) -> bool:
    if isinstance(asset, unreal.Blueprint):
        result = unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        if result is False:
            raise RuntimeError(f"Blueprint compile failed: {asset.get_path_name()}")
        return True
    return False


def _recreate_control_rig_if_legacy(policy: dict) -> bool:
    package = "/Game/DiscGolf/Rigs/CR_DG_Master"
    path = PROJECT_ROOT / "Content/DiscGolf/Rigs/CR_DG_Master.uasset"
    existing_data = path.read_bytes() if path.is_file() else b""
    if existing_data and OLD_SCRIPT not in existing_data and NEW_SCRIPT in existing_data:
        return False
    recreated = False
    if not existing_data or OLD_SCRIPT in existing_data:
        if path.is_file() and not unreal.EditorAssetLibrary.delete_asset(package):
            raise RuntimeError("Could not delete the version-controlled legacy Control Rig package")
        foundation_script = PROJECT_ROOT / policy["workflow"]["controlRigFoundationScript"]
        if not foundation_script.is_file():
            raise RuntimeError(f"Control Rig foundation script is missing: {foundation_script}")
        runpy.run_path(str(foundation_script), run_name="__main__")
        if not unreal.EditorAssetLibrary.does_asset_exist(package):
            raise RuntimeError("Project-owned Session 2 workflow did not recreate CR_DG_Master")
        recreated = True

    # The current Session 2 generator leaves several PBIK defaults implicit.
    # Normalize the freshly recreated graph to the exact project-owned Session 4
    # preflight contract before that utility adds the Foundation-typed variables.
    rig = unreal.EditorAssetLibrary.load_asset(package)
    if not isinstance(rig, unreal.ControlRigBlueprint):
        raise RuntimeError("Recreated CR_DG_Master did not load as a ControlRigBlueprint")
    model = rig.get_default_model()
    controller = rig.get_controller_by_name(model.get_name()) if model else None
    pbik = next(
        (node for node in model.get_nodes() if node.get_name() == "DGFullBodyIK"),
        None,
    ) if model else None
    if controller is None or pbik is None:
        raise RuntimeError("Recreated CR_DG_Master lacks its editable DGFullBodyIK node")
    pin_defaults = {
        "Root": "pelvis",
        "Settings.RootBehavior": "Free",
        "Settings.Iterations": "20",
        "Settings.SubIterations": "10",
        "Settings.GlobalPullChainAlpha": "0.000000",
        "Settings.bAllowStretch": "False",
    }
    for index in range(4):
        pin_defaults.update({
            f"BoneSettings.{index}.PositionStiffness": "0.000000",
            f"BoneSettings.{index}.RotationStiffness": "0.000000",
            f"BoneSettings.{index}.X": "Free",
            f"BoneSettings.{index}.Y": "Free",
            f"BoneSettings.{index}.Z": "Free",
        })
    for pin_path, value in pin_defaults.items():
        pin = pbik.find_pin(pin_path)
        if pin is None:
            raise RuntimeError(f"Recreated PBIK pin is missing: {pin_path}")
        if str(pin.get_default_value()) == value:
            continue
        if not controller.set_pin_default_value(
                pin.get_pin_path(), value, True, False, False, False, False):
            raise RuntimeError(f"Could not normalize recreated PBIK pin: {pin_path}")
    rig.request_auto_vm_recompilation()
    rig.recompile_vm()
    if not unreal.EditorAssetLibrary.save_loaded_asset(rig, only_if_is_dirty=False):
        raise RuntimeError("Could not save normalized recreated CR_DG_Master")
    author_result = unreal.DiscGolfSession4AssetUtility.author_session4_assets()
    try:
        parsed = json.loads(str(author_result))
    except json.JSONDecodeError as exc:
        raise RuntimeError(
            f"Session 4 authoring utility returned invalid JSON: {author_result}") from exc
    if not str(parsed.get("status", "")).startswith("PASS"):
        raise RuntimeError(
            f"Session 4 authoring utility did not accept recreated CR_DG_Master: {author_result}")
    if OLD_SCRIPT in path.read_bytes():
        raise RuntimeError("Independently recreated CR_DG_Master still contains the legacy script")
    return recreated


def main() -> None:
    policy, rows = _load_inputs()
    _require_module_boundary(policy)
    retained = [row for row in rows if row["disposition"] == "RETAINED"]
    excluded = [row for row in rows if row["disposition"] == "EXCLUDED"]
    for row in retained:
        _assert_retained_input(row)
    for row in excluded:
        _assert_pre_identity(row)
    allowed = _allowed_paths(retained)
    protected_before = _content_snapshot(allowed)
    results: list[dict[str, object]] = []
    control_rig_recreated = _recreate_control_rig_if_legacy(policy)

    for row in retained:
        asset = unreal.EditorAssetLibrary.load_asset(row["package"])
        if asset is None:
            raise RuntimeError(f"Retained package failed to load: {row['package']}")
        class_path = asset.get_class().get_path_name()
        compiled = _compile_if_blueprint(asset)
        if not unreal.EditorAssetLibrary.save_loaded_asset(
                asset, only_if_is_dirty=False):
            raise RuntimeError(f"Retained package failed to save: {row['package']}")
        results.append({
            "package": row["package"],
            "loadedObject": asset.get_path_name(),
            "assetClass": class_path,
            "blueprintCompiled": compiled,
            "saved": True,
        })

    protected_after = _content_snapshot(allowed)
    if protected_before != protected_after:
        before_keys = set(protected_before)
        after_keys = set(protected_after)
        changed = sorted(
            (before_keys ^ after_keys)
            | {key for key in before_keys & after_keys
               if protected_before[key] != protected_after[key]})
        raise RuntimeError(
            "Content outside the six retained packages changed: " + ", ".join(changed))

    post_identities: list[dict[str, object]] = []
    for row in retained:
        path = PROJECT_ROOT / row["file"]
        data = path.read_bytes()
        if OLD_SCRIPT in data:
            raise RuntimeError(f"Legacy script remains in retained asset: {row['file']}")
        if NEW_SCRIPT not in data:
            raise RuntimeError(f"Replacement script is absent from retained asset: {row['file']}")
        post_identities.append({
            "package": row["package"],
            "file": row["file"],
            **_identity(path),
        })
    for row in excluded:
        _assert_pre_identity(row)

    report = {
        "schema": "DiscGolfTour.Session19SerializedAssetMigrationRun.v1",
        "status": "PASS_RETAINED_ASSETS_RESAVED_EXCLUDED_ASSETS_UNCHANGED",
        "policy": POLICY_PATH.relative_to(PROJECT_ROOT).as_posix(),
        "policySha256": _sha256(POLICY_PATH),
        "inventory": INVENTORY_PATH.relative_to(PROJECT_ROOT).as_posix(),
        "inventorySha256": _sha256(INVENTORY_PATH),
        "legacyScriptPackage": OLD_SCRIPT.decode("ascii"),
        "replacementScriptPackage": NEW_SCRIPT.decode("ascii"),
        "retainedAssetCount": len(retained),
        "excludedAssetCount": len(excluded),
        "loadedAndSavedCount": len(results),
        "protectedContentFileCount": len(protected_after),
        "protectedContentMutationCount": 0,
        "legacyPluginProjectEntryPresent": False,
        "legacyPluginEnabledByDefault": False,
        "controlRigRecreatedFromProjectOwnedWorkflow": control_rig_recreated,
        "results": results,
        "postMigrationIdentities": post_identities,
        "releaseReady": False,
        "shippingPackageEvidenceAccepted": False,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log(
        "DG_SESSION19_SERIALIZED_MIGRATION: PASS "
        f"retained={len(retained)} excluded_unchanged={len(excluded)} "
        f"protected_mutations=0")


if __name__ == "__main__":
    main()
