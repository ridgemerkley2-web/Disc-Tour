#!/usr/bin/env python3
"""Strict disk/report validation for the accepted Session 8B MH assembly run."""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SCHEMA = "DiscGolfTour.Session8BMetaHumanAssemblyFileValidation.v1"
AUTHOR_STATUS = "PASS_AUTHORED_ASSEMBLED_AND_SAVED"
AUTHOR_REPORT_BYTES = 230148
AUTHOR_REPORT_SHA256 = (
    "A0021E5160F0505481166D4F6634D78CF95FBF3004EB13E8C4D394F06FFE3BCA"
)
AUTHOR_LOG_BYTES = 672356
AUTHOR_LOG_SHA256 = (
    "5C418869BC3C98774C65C6A22CDEE360CD0912F3F28975660A8EBDB12705FEF6"
)
EXPECTED_SOURCE_RELATIVE = "Source/MHC_DG_Golfer_Default.uasset"
EXPECTED_SOURCE_BYTES = 112982127
EXPECTED_SOURCE_SHA256 = (
    "1AA1EFBFDD882308959D1429D562DD2D92F64317219660003063D0AEF17BCFAB"
)
EXPECTED_GENERATED_COUNT = 56
EXPECTED_COMMON_COUNT = 205
EXPECTED_OUTPUT_COUNT = 261
EXPECTED_SAVE_COUNT = 262
EXPECTED_CONTENT_FILE_COUNT = 950
EXPECTED_TOMBSTONES = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/"
    "T_PreBakedGroom_Color_LOD3",
    "/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/"
    "T_PreBakedGroom_NSR_LOD3",
)
ACCEPTED_SAVE_BYTES = 5212
ACCEPTED_SAVE_SHA256 = (
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
)


class ValidationError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    return {
        "exists": True,
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def strict_json(path: Path) -> dict[str, Any]:
    def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValidationError(f"duplicate JSON key {key!r}: {path}")
            result[key] = value
        return result

    try:
        payload = json.loads(
            path.read_text(encoding="utf-8"), object_pairs_hook=no_duplicates
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValidationError(f"invalid JSON {path}: {exc}") from exc
    if not isinstance(payload, dict):
        raise ValidationError(f"JSON root is not an object: {path}")
    return payload


def package_file(content_root: Path, package_name: str) -> Path:
    if not package_name.startswith("/Game/"):
        raise ValidationError(f"non-project package in author report: {package_name}")
    return content_root / f"{package_name.removeprefix('/Game/')}.uasset"


def validate(root: Path, author_report: Path, author_log: Path) -> dict[str, Any]:
    content = root / "Content"
    meta = content / "DiscGolf/Characters/MetaHuman"
    production = root / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
    backup = Path(
        r"C:\DGTour_Backups\Session7_Accepted\DiscGolfTour_Profile_0_Session7_Accepted.sav"
    )
    report_record = record(author_report)
    log_record = record(author_log)
    if report_record != {
        "exists": True,
        "bytes": AUTHOR_REPORT_BYTES,
        "sha256": AUTHOR_REPORT_SHA256,
    }:
        raise ValidationError(f"author report identity changed: {report_record}")
    if log_record != {
        "exists": True,
        "bytes": AUTHOR_LOG_BYTES,
        "sha256": AUTHOR_LOG_SHA256,
    }:
        raise ValidationError(f"author log identity changed: {log_record}")

    payload = strict_json(author_report)
    if payload.get("status") != AUTHOR_STATUS or payload.get("phase") != "COMPLETE":
        raise ValidationError("author report is not the accepted complete PASS")
    if payload.get("errors") != []:
        raise ValidationError(f"author report contains errors: {payload.get('errors')}")
    saved = payload.get("saved_packages")
    allowlist = payload.get("exact_save_allowlist")
    attempted = payload.get("save_attempted_packages")
    assets = payload.get("assembly", {}).get("asset_object_paths")
    if not all(isinstance(value, list) for value in (saved, allowlist, attempted, assets)):
        raise ValidationError("author report package/asset lists are malformed")
    if not (
        len(saved) == len(set(saved)) == EXPECTED_SAVE_COUNT
        and saved == allowlist == attempted
    ):
        raise ValidationError("saved/allowlist/attempted partition is not exact 262")
    if len(assets) != len(set(assets)):
        raise ValidationError("assembled asset object paths are duplicated")
    if len(assets) != EXPECTED_OUTPUT_COUNT:
        raise ValidationError(f"assembled output count is {len(assets)}, expected 261")
    output_packages = sorted(path.split(".", 1)[0] for path in assets)
    if len(output_packages) != len(set(output_packages)):
        raise ValidationError("assembled assets do not map one-to-one to packages")
    source_package = "/Game/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default"
    if saved != sorted(output_packages + [source_package]):
        raise ValidationError("persistent saved set is not outputs plus exact source")
    common = [name for name in output_packages if "/MetaHuman/Common/" in name]
    generated = [name for name in output_packages if "/MetaHuman/Generated/" in name]
    if len(common) != EXPECTED_COMMON_COUNT or len(generated) != EXPECTED_GENERATED_COUNT:
        raise ValidationError(
            f"output partition changed: common={len(common)} generated={len(generated)}"
        )

    disk_files = payload.get("disk_delta", {}).get("files")
    if not isinstance(disk_files, dict) or len(disk_files) != EXPECTED_SAVE_COUNT:
        raise ValidationError("author disk snapshot is not the exact 262 files")
    actual_meta_files = sorted(path for path in meta.rglob("*") if path.is_file())
    if len(actual_meta_files) != EXPECTED_SAVE_COUNT:
        raise ValidationError(f"MetaHuman disk file count changed: {len(actual_meta_files)}")
    current_records: dict[str, dict[str, Any]] = {}
    for path in actual_meta_files:
        relative = path.relative_to(meta).as_posix()
        current_records[relative] = record(path)
    if current_records != disk_files:
        raise ValidationError("MetaHuman disk bytes differ from accepted author report")
    if current_records.get(EXPECTED_SOURCE_RELATIVE) != {
        "exists": True,
        "bytes": EXPECTED_SOURCE_BYTES,
        "sha256": EXPECTED_SOURCE_SHA256,
    }:
        raise ValidationError("canonical source identity differs from accepted build")

    tombstones = payload.get("transient_tombstones")
    dirty_exit = payload.get("dirty_content_packages_at_exit")
    if not isinstance(tombstones, list) or len(tombstones) != 2:
        raise ValidationError("transient tombstone ledger is not exact two")
    tombstone_packages = sorted(entry.get("package") for entry in tombstones)
    if tombstone_packages != sorted(EXPECTED_TOMBSTONES):
        raise ValidationError("transient tombstone package identities changed")
    if dirty_exit != sorted(EXPECTED_TOMBSTONES):
        raise ValidationError("commandlet dirty-at-exit set is not exact tombstones")
    if payload.get("dirty_map_packages_at_exit") != [] or payload.get(
        "unexpected_dirty_packages_at_exit"
    ) != []:
        raise ValidationError("unexpected content/map packages were dirty at exit")
    if payload.get("commandlet_exit_discards_transients") is not True:
        raise ValidationError("commandlet transient-discard contract was not recorded")
    for entry in tombstones:
        if entry.get("asset_data_valid") is not False or entry.get(
            "registry_present"
        ) is not False or entry.get("listed_as_output_asset") is not False:
            raise ValidationError(f"tombstone registry contract failed: {entry}")
        package = entry["package"]
        if package in saved or package in output_packages:
            raise ValidationError(f"tombstone leaked into persistent outputs: {package}")
        stem = content / package.removeprefix("/Game/")
        for suffix in (".uasset", ".uexp", ".ubulk", ".uptnl"):
            if Path(f"{stem}{suffix}").exists():
                raise ValidationError(f"tombstone sidecar exists: {stem}{suffix}")

    content_files = sum(1 for path in content.rglob("*") if path.is_file())
    if content_files != EXPECTED_CONTENT_FILE_COUNT:
        raise ValidationError(f"project Content count changed: {content_files}")
    expected_save = {
        "exists": True,
        "bytes": ACCEPTED_SAVE_BYTES,
        "sha256": ACCEPTED_SAVE_SHA256,
    }
    production_record = record(production)
    backup_record = record(backup)
    if production_record != expected_save or backup_record != expected_save:
        raise ValidationError("production save or accepted backup changed")
    if production.read_bytes() != backup.read_bytes():
        raise ValidationError("production save and accepted backup are not byte-identical")

    log_text = author_log.read_text(encoding="utf-8", errors="replace")
    pass_marker = "DG_SESSION8B_METAHUMAN_AUTHOR: PASS_AUTHORED_ASSEMBLED_AND_SAVED"
    fail_marker = "DG_SESSION8B_METAHUMAN_AUTHOR: FAIL"
    if log_text.count(pass_marker) != 1 or fail_marker in log_text:
        raise ValidationError("author log PASS/FAIL marker contract failed")
    return {
        "schema": SCHEMA,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS_NO_DISK_MUTATION",
        "author_report": {"path": str(author_report), **report_record},
        "author_log": {"path": str(author_log), **log_record},
        "persistent_generated_common_packages": EXPECTED_OUTPUT_COUNT,
        "persistent_source_packages": 1,
        "persistent_save_packages": EXPECTED_SAVE_COUNT,
        "common_packages": len(common),
        "generated_packages": len(generated),
        "transient_tombstones": tombstones,
        "content_file_count": content_files,
        "metahuman_files": current_records,
        "production_save": production_record,
        "accepted_backup": backup_record,
        "production_backup_byte_identical": True,
        "qualifications": [
            "UE_OPTIMIZED_MEDIUM_TECHNICAL_ASSEMBLY",
            "CANONICAL_RUNTIME_WRAPPER_RETARGET_PROFILE_NOT_YET_AUTHORED",
            "NO_PACKAGED_RUNTIME_VISUAL_OR_PERFORMANCE_ACCEPTANCE_YET",
        ],
        "errors": [],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--author-report", type=Path, required=True)
    parser.add_argument("--author-log", type=Path, required=True)
    parser.add_argument(
        "--report",
        type=Path,
        default=Path("Saved/CharacterFramework/Session8BMetaHumanAssemblyValidation.json"),
    )
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.report if args.report.is_absolute() else root / args.report
    try:
        result = validate(root, args.author_report.resolve(), args.author_log.resolve())
    except Exception as exc:
        result = {
            "schema": SCHEMA,
            "generated_utc": datetime.now(timezone.utc).isoformat(),
            "status": "FAIL",
            "errors": [f"{type(exc).__name__}: {exc}"],
        }
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"SESSION 8B METAHUMAN ASSEMBLY FILE VALIDATION FAIL: {exc}")
        return 1
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        "SESSION 8B METAHUMAN ASSEMBLY FILE VALIDATION PASS_NO_DISK_MUTATION: "
        "261 outputs + source; 2 tombstones discarded"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
