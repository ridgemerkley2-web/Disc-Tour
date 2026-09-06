"""Migrate the three v006 production montages to the Recovery blend contract.

This is an intentionally narrow, idempotent repair for already-authored v006
candidate assets. It changes only ``BlendOutTriggerTime`` on the Drive,
Approach, and Putt montages, preserves auto blend-out and its authored blend
shape, then runs the native v006 validator. It never touches sequences, the
production library, gameplay physics, release notifies, maps, or save data.

Run with UnrealEditor-Cmd ``-run=pythonscript`` while the project is closed.
"""

from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import uuid

import unreal


REPAIR_SWITCH = "-DGRepairProductionMotionV6BlendOut"
REQUIRED_USER_ROOT = Path("C:/DGTour_TestRuns/Session19ProductionMotion")
REPORT_RELATIVE = Path("Saved/ProductionMotion/V006BlendOutRepair.json")
EXPECTED_TRIGGER_SECONDS = 0.10
LEGACY_TRIGGER_SECONDS = -1.0
MONTAGE_PATHS = (
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v006",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v006",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v006",
)


class RepairError(RuntimeError):
    pass


def exact_switch(command_line: str, switch: str) -> bool:
    return bool(
        re.search(
            rf"(?i)(?:^|\s|\"){re.escape(switch)}(?=$|\s|\")",
            command_line,
        )
    )


def command_value(command_line: str, name: str) -> str:
    match = re.search(
        rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))',
        command_line,
    )
    return (match.group(1) or match.group(2)) if match else ""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def identity(path: Path) -> dict[str, object]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    return {
        "exists": True,
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def target_file(project_root: Path, package_path: str) -> Path:
    relative = package_path.removeprefix("/Game/")
    return project_root / "Content" / f"{relative}.uasset"


def dirty_packages() -> tuple[list[str], list[str]]:
    content = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    )
    maps = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )
    return content, maps


def validate_command_line(command_line: str, project_root: Path) -> Path:
    required = (REPAIR_SWITCH, "-unattended", "-nop4")
    missing = [item for item in required if not exact_switch(command_line, item)]
    if missing:
        raise RepairError(f"required Unreal command-line switches absent: {missing}")
    if "-run=pythonscript" not in command_line.casefold():
        raise RepairError("repair must run through PythonScript commandlet")
    if command_value(command_line, "DGProductionMotionVersion").casefold() != "v6":
        raise RepairError("exact -DGProductionMotionVersion=v6 is required")
    user_text = command_value(command_line, "UserDir")
    if not user_text:
        raise RepairError("absolute external -UserDir is required")
    user_dir = Path(user_text).resolve()
    try:
        relative = user_dir.relative_to(REQUIRED_USER_ROOT.resolve())
    except ValueError as exc:
        raise RepairError(
            f"UserDir is outside {REQUIRED_USER_ROOT.resolve()}: {user_dir}"
        ) from exc
    if len(relative.parts) != 1:
        raise RepairError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise RepairError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold():
        raise RepairError("UserDir UUID is not canonical lowercase-hyphen form")
    try:
        user_dir.relative_to(project_root)
    except ValueError:
        pass
    else:
        raise RepairError("UserDir must be external to the project")
    report_path = user_dir / REPORT_RELATIVE
    if report_path.exists():
        raise RepairError(f"fresh repair report already exists: {report_path}")
    return user_dir


def main() -> None:
    project_root = Path(unreal.Paths.project_dir()).resolve()
    command_line = unreal.SystemLibrary.get_command_line()
    user_dir = validate_command_line(command_line, project_root)
    report_path = user_dir / REPORT_RELATIVE
    dirty_content, dirty_maps = dirty_packages()
    if dirty_content or dirty_maps:
        raise RepairError(
            f"commandlet began with dirty packages: content={dirty_content}, maps={dirty_maps}"
        )

    rows: list[dict[str, object]] = []
    loaded: list[tuple[str, unreal.AnimMontage, Path, float]] = []
    for package_path in MONTAGE_PATHS:
        asset = unreal.load_asset(package_path)
        file_path = target_file(project_root, package_path)
        if not isinstance(asset, unreal.AnimMontage) or not file_path.is_file():
            raise RepairError(f"missing expected v006 AnimMontage: {package_path}")
        auto_blend = bool(asset.get_editor_property("enable_auto_blend_out"))
        trigger = float(asset.get_editor_property("blend_out_trigger_time"))
        if not auto_blend:
            raise RepairError(f"auto blend-out is disabled on {package_path}")
        if not math.isclose(trigger, LEGACY_TRIGGER_SECONDS, abs_tol=1e-5) and not math.isclose(
            trigger, EXPECTED_TRIGGER_SECONDS, abs_tol=1e-5
        ):
            raise RepairError(
                f"unexpected blend-out trigger on {package_path}: {trigger}"
            )
        before = identity(file_path)
        rows.append(
            {
                "asset": package_path,
                "file": file_path.relative_to(project_root).as_posix(),
                "auto_blend_out": auto_blend,
                "trigger_before_seconds": trigger,
                "identity_before": before,
            }
        )
        loaded.append((package_path, asset, file_path, trigger))

    for _, asset, _, trigger in loaded:
        if not math.isclose(trigger, EXPECTED_TRIGGER_SECONDS, abs_tol=1e-5):
            asset.set_editor_property(
                "blend_out_trigger_time", EXPECTED_TRIGGER_SECONDS
            )

    for index, (package_path, asset, file_path, trigger) in enumerate(loaded):
        changed = not math.isclose(
            trigger, EXPECTED_TRIGGER_SECONDS, abs_tol=1e-5
        )
        if changed and not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            raise RepairError(f"could not save repaired montage: {package_path}")
        actual_trigger = float(asset.get_editor_property("blend_out_trigger_time"))
        if not math.isclose(
            actual_trigger, EXPECTED_TRIGGER_SECONDS, abs_tol=1e-5
        ):
            raise RepairError(
                f"repaired trigger did not persist on {package_path}: {actual_trigger}"
            )
        rows[index].update(
            {
                "changed": changed,
                "trigger_after_seconds": actual_trigger,
                "identity_after": identity(file_path),
            }
        )

    native_text = (
        unreal.DiscGolfProductionMotionAuthoringUtility.
        validate_production_motion_assets_for_version("v6")
    )
    native_validation = json.loads(native_text)
    if not str(native_validation.get("status", "")).startswith("PASS"):
        raise RepairError(f"native v006 validation failed: {native_validation}")
    dirty_content_after, dirty_maps_after = dirty_packages()
    if dirty_content_after or dirty_maps_after:
        raise RepairError(
            "repair left dirty packages: "
            f"content={dirty_content_after}, maps={dirty_maps_after}"
        )

    report = {
        "schema": "DiscGolfTour.Session19ProductionMotionV6BlendOutRepair.v1",
        "status": "PASS",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "recipe_version": "v6",
        "asset_revision": "v006",
        "expected_blend_out_trigger_seconds": EXPECTED_TRIGGER_SECONDS,
        "target_count": len(MONTAGE_PATHS),
        "targets": rows,
        "native_validation": native_validation,
        "sequences_changed": False,
        "production_library_changed": False,
        "release_notifies_changed": False,
        "gameplay_physics_changed": False,
        "maps_or_save_data_changed": False,
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    unreal.log(
        "DG_PRODUCTION_MOTION_V6_BLEND_REPAIR: PASS "
        + json.dumps(report, sort_keys=True)
    )


main()
