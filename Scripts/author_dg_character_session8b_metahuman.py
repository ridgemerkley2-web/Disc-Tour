"""Fail-closed Session 8B authoring and assembly for the canonical MetaHuman.

This script must run through Unreal's PythonScript commandlet.  It has two
explicit modes:

* ``-DGSession8BMetaHumanPreflightOnly`` performs no cloud request and no asset
  mutation.
* ``-DGSession8BMetaHumanAuthor`` applies the frozen generic design, makes the
  two blocking MetaHuman Cloud requests, assembles UE Optimized Medium, and
  saves the exact package set only after every validation gate succeeds.

Both modes require a fresh external UUID ``-UserDir``, ``-unattended``,
``-nop4``, ``-NoMetaHumanAccountPortalLoginFallback``, real offscreen D3D12
commandlet rendering, and the process-local TextureGraph commandlet opt-in.
Consequently a missing persistent Epic login fails instead of opening
AccountPortal, while MetaHuman material baking cannot run under NullRHI or an
uninitialized TextureGraph device manager.
``-Session8BSourceAuditReport=<absolute external JSON path>`` must point
to the completed read-only source audit from the preceding isolated run.

The source asset and generated roots are intentionally dedicated to this one
transaction.  On failure, no save is requested before assembly succeeds.  If
the final package save itself is partial, the dedicated MetaHuman tree is
restored byte-for-byte to its pre-run state before the commandlet exits.
"""

from __future__ import annotations

import hashlib
import json
import math
import re
import tempfile
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import unreal


SCHEMA = "DiscGolfTour.Session8BMetaHumanAuthor.v1"
DESIGN_ID = "dg_original_generic_golfer_v1"

CHARACTER_ASSET_NAME = "MHC_DG_Golfer_Default"
SOURCE_PACKAGE_ROOT = "/Game/DiscGolf/Characters/MetaHuman/Source"
SOURCE_PACKAGE_NAME = f"{SOURCE_PACKAGE_ROOT}/{CHARACTER_ASSET_NAME}"
SOURCE_OBJECT_PATH = f"{SOURCE_PACKAGE_NAME}.{CHARACTER_ASSET_NAME}"

GENERATED_ROOT = "/Game/DiscGolf/Characters/MetaHuman/Generated"
GENERATED_CHARACTER_ROOT = f"{GENERATED_ROOT}/{CHARACTER_ASSET_NAME}"
COMMON_ROOT = "/Game/DiscGolf/Characters/MetaHuman/Common"
EXPECTED_GENERATED_BP = (
    f"{GENERATED_CHARACTER_ROOT}/BP_{CHARACTER_ASSET_NAME}."
    f"BP_{CHARACTER_ASSET_NAME}"
)

# The runtime wrapper, backend profile, and retarget assets are deliberately
# outside this authoring transaction.  They are subsequent integration work.
RUNTIME_WRAPPER_OBJECT_PATH = (
    "/Game/DiscGolf/Characters/MetaHuman/"
    "BP_DG_MetaHuman_Default.BP_DG_MetaHuman_Default"
)

CREATE_REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanCreateOpen.json"
)
SOURCE_AUDIT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanSourceAudit.json"
)
AUTHOR_REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanAuthor.json"
)

PRODUCTION_SAVE_RELATIVE = Path("Saved/SaveGames/DiscGolfTour_Profile_0.sav")
ACCEPTED_BACKUP = Path(
    r"C:\DGTour_Backups\Session7_Accepted\DiscGolfTour_Profile_0_Session7_Accepted.sav"
)
ACCEPTED_SAVE_BYTES = 5212
ACCEPTED_SAVE_SHA256 = (
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
)

PREFLIGHT_SWITCH = "-DGSession8BMetaHumanPreflightOnly"
AUTHOR_SWITCH = "-DGSession8BMetaHumanAuthor"
NO_ACCOUNT_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
ALLOW_COMMANDLET_RENDERING_SWITCH = "-AllowCommandletRendering"
RENDER_OFFSCREEN_SWITCH = "-RenderOffscreen"
D3D12_SWITCH = "-d3d12"
TEXTUREGRAPH_COMMANDLET_OVERRIDE = (
    "-ini:Engine:[ConsoleVariables]:TextureGraph.AllowCommandlets=True"
)
SOURCE_AUDIT_ARGUMENT = "Session8BSourceAuditReport"
EXPECTED_SOURCE_PACKAGE_BYTES = 757356
EXPECTED_SOURCE_PACKAGE_SHA256 = (
    "45C25ED6FADC26B06079DB903E0700F35DC03E5DAD276317DD3B486B8B006BA0"
)
EXPECTED_SOURCE_AUDIT_BYTES = 9594
EXPECTED_SOURCE_AUDIT_SHA256 = (
    "F9B1B646B489E6CC4D35DB1F000F0BF200FA5C298950E5B808D696DC8BECE252"
)
EXPECTED_FACE_LANDMARK_COUNT = 79
EXPECTED_BODY_CONSTRAINT_COUNT = 30
EXPECTED_HAIR_WARDROBE_COUNT = 38
EXPECTED_BEARD_WARDROBE_COUNT = 10
EXPECTED_CLOTHING_WARDROBE_COUNT = 1
EXPECTED_PERSISTENT_GENERATED_OUTPUT_COUNT = 261
TRANSIENT_DISPOSITION = "EPIC_DELETED_FACE_BAKE_INTERMEDIATE"

WARDROBE_SELECTIONS = (
    (
        "Hair",
        "/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/"
        "WI_Hair_S_Clean.WI_Hair_S_Clean",
    ),
    (
        "Beard",
        "/MetaHumanCharacter/Optional/Grooms/Bindings/Beards/"
        "WI_Beard_M_Stubble.WI_Beard_M_Stubble",
    ),
    (
        "Outfits",
        "/MetaHumanCharacter/Optional/Clothing/"
        "WI_DefaultGarment.WI_DefaultGarment",
    ),
)

# UE Optimized Medium creates these two groom-bake textures as temporary public
# assets, bakes their data into the final skin materials, then deliberately
# calls AssetDeleted and clears RF_Public | RF_Standalone. Their packages stay
# dirty in memory even though Epic's pipeline has made them empty/non-assets.
# They must be proven exact and discarded at commandlet exit, never
# re-registered or persisted as runtime outputs.
EXPECTED_DISCARDED_TRANSIENT_CLASSES = {
    (
        f"{GENERATED_CHARACTER_ROOT}/T_PreBakedGroom_Color_LOD3"
    ): unreal.Texture2D,
    (
        f"{GENERATED_CHARACTER_ROOT}/T_PreBakedGroom_NSR_LOD3"
    ): unreal.Texture2D,
}

# Targets are fractions of each installed parametric model's advertised range,
# rather than copied measurements from a real person.  The restrained values
# produce a moderately athletic, non-extreme generic golfer.
BODY_CONSTRAINT_RATIOS = {
    "Height": 0.54,
    "Across Shoulder": 0.57,
    "Chest": 0.52,
    "Waist": 0.44,
    "Hip": 0.48,
    "Muscularity": 0.58,
    "Fat": 0.40,
    "Upper Arm Length": 0.52,
}

# A small centroid-relative anisotropic change is original local geometry, not
# a preset or likeness transfer.  It is intentionally subtle and bounded.
FACE_LANDMARK_SCALES = (1.018, 0.991, 1.006)
MAX_FACE_LANDMARK_DELTA = 2.0

ALLOWED_PACKAGE_FILE_SUFFIXES = {".uasset", ".uexp", ".ubulk", ".uptnl"}
FORBIDDEN_CREDENTIAL_TOKENS = (
    "metahuman.cloud.config.useremail",
    "metahuman.cloud.config.password",
    "metahuman.cloud.config.exchangecode",
)


class Session8BAuthorError(RuntimeError):
    """Raised when a fail-closed Session 8B gate does not hold."""


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def _file_record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    data = path.read_bytes()
    return {
        "exists": True,
        "bytes": len(data),
        "sha256": _sha256_bytes(data),
    }


def _directory_snapshot(root: Path) -> dict[str, dict[str, Any]]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): _file_record(path)
        for path in sorted(candidate for candidate in root.rglob("*") if candidate.is_file())
    }


def _directory_bytes(root: Path) -> dict[str, bytes]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): path.read_bytes()
        for path in sorted(candidate for candidate in root.rglob("*") if candidate.is_file())
    }


def _records_from_bytes(files: dict[str, bytes]) -> dict[str, dict[str, Any]]:
    return {
        relative: {
            "exists": True,
            "bytes": len(data),
            "sha256": _sha256_bytes(data),
        }
        for relative, data in sorted(files.items())
    }


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def _strict_json(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise Session8BAuthorError(f"required report is absent: {path}")

    def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise Session8BAuthorError(
                    f"duplicate JSON key {key!r} in {path}"
                )
            result[key] = value
        return result

    try:
        payload = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=reject_duplicate_keys,
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise Session8BAuthorError(f"invalid JSON report {path}: {exc}") from exc
    if not isinstance(payload, dict):
        raise Session8BAuthorError(f"JSON report root must be an object: {path}")
    return payload


def _write_report(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def _has_exact_switch(command_line: str, switch: str) -> bool:
    token = re.escape(switch)
    return bool(
        re.search(
            rf"(?i)(?:^|\s|\")({token})(?=$|\s|\")",
            command_line,
        )
    )


def _extract_user_dir(command_line: str) -> Path:
    patterns = (
        r'(?i)(?:^|\s)"-userdir=([^\"]+)"',
        r'(?i)(?:^|\s)-userdir="([^\"]+)"',
        r"(?i)(?:^|\s)-userdir=([^\s\"]+)",
    )
    matches: list[str] = []
    for pattern in patterns:
        matches.extend(match.group(1) for match in re.finditer(pattern, command_line))
    unique = sorted(set(matches), key=str.casefold)
    if len(unique) != 1:
        raise Session8BAuthorError(
            "command line must contain exactly one absolute -UserDir=<UUID path>"
        )
    raw = Path(unique[0]).expanduser()
    if not raw.is_absolute():
        raise Session8BAuthorError("-UserDir must be absolute")
    return raw.resolve()


def _extract_path_argument(command_line: str, argument_name: str) -> Path:
    escaped = re.escape(argument_name)
    patterns = (
        rf'(?i)(?:^|\s)"-{escaped}=([^\"]+)"',
        rf'(?i)(?:^|\s)-{escaped}="([^\"]+)"',
        rf"(?i)(?:^|\s)-{escaped}=([^\s\"]+)",
    )
    matches: list[str] = []
    for pattern in patterns:
        matches.extend(match.group(1) for match in re.finditer(pattern, command_line))
    unique = sorted(set(matches), key=str.casefold)
    if len(unique) != 1:
        raise Session8BAuthorError(
            f"command line must contain exactly one absolute -{argument_name}=<path>"
        )
    path = Path(unique[0]).expanduser()
    if not path.is_absolute():
        raise Session8BAuthorError(f"-{argument_name} must be absolute")
    return path.resolve()


def _validate_command_line(
    command_line: str, project_root: Path
) -> tuple[str, Path, Path]:
    lower = command_line.casefold()
    is_preflight = _has_exact_switch(command_line, PREFLIGHT_SWITCH)
    is_author = _has_exact_switch(command_line, AUTHOR_SWITCH)
    if is_preflight == is_author:
        raise Session8BAuthorError(
            f"command line must contain exactly one of {PREFLIGHT_SWITCH} or {AUTHOR_SWITCH}"
        )
    required_switches = (
        "-unattended",
        "-nop4",
        NO_ACCOUNT_PORTAL_SWITCH,
        ALLOW_COMMANDLET_RENDERING_SWITCH,
        RENDER_OFFSCREEN_SWITCH,
        D3D12_SWITCH,
        TEXTUREGRAPH_COMMANDLET_OVERRIDE,
    )
    missing = [
        switch
        for switch in required_switches
        if not _has_exact_switch(command_line, switch)
    ]
    if not re.search(
        r'(?i)(?:^|\s|\")-run=pythonscript(?=$|\s|\")', command_line
    ):
        missing.append("-run=PythonScript")
    if missing:
        raise Session8BAuthorError(
            "missing mandatory unattended command-line tokens: " + ", ".join(missing)
        )
    if "-executepythonscript" in lower:
        raise Session8BAuthorError(
            "interactive -ExecutePythonScript is forbidden; use -run=PythonScript"
        )
    if not unreal.SystemLibrary.get_console_variable_bool_value(
        "TextureGraph.AllowCommandlets"
    ):
        raise Session8BAuthorError(
            "TextureGraph.AllowCommandlets must be true before any cloud or build work"
        )
    credential_hits = [token for token in FORBIDDEN_CREDENTIAL_TOKENS if token in lower]
    if credential_hits:
        raise Session8BAuthorError(
            "credential-bearing MetaHuman Cloud arguments are forbidden: "
            + ", ".join(credential_hits)
        )

    user_dir = _extract_user_dir(command_line)
    if not user_dir.is_dir():
        raise Session8BAuthorError(f"external -UserDir must already exist: {user_dir}")
    try:
        parsed_uuid = uuid.UUID(user_dir.name)
    except ValueError as exc:
        raise Session8BAuthorError(
            "external -UserDir final directory name must be a UUID"
        ) from exc
    if str(parsed_uuid) != user_dir.name.casefold():
        raise Session8BAuthorError(
            "external -UserDir must use the canonical hyphenated UUID form"
        )
    if _is_within(user_dir, project_root) or _is_within(project_root, user_dir):
        raise Session8BAuthorError("-UserDir must be external to the project tree")
    source_audit_path = _extract_path_argument(command_line, SOURCE_AUDIT_ARGUMENT)
    if _is_within(source_audit_path, project_root):
        raise Session8BAuthorError("completed source audit must remain external")
    if _is_within(source_audit_path, user_dir):
        raise Session8BAuthorError(
            "completed source audit must come from a prior isolated UUID run"
        )
    if source_audit_path.name != SOURCE_AUDIT_RELATIVE.name:
        raise Session8BAuthorError(
            "completed source-audit filename is not canonical: "
            f"{source_audit_path.name}"
        )
    if (
        source_audit_path.parent.name != "CharacterFramework"
        or source_audit_path.parent.parent.name != "Saved"
    ):
        raise Session8BAuthorError(
            "completed source audit must use <UUID>/Saved/CharacterFramework"
        )
    try:
        audit_uuid = uuid.UUID(source_audit_path.parents[2].name)
    except ValueError as exc:
        raise Session8BAuthorError(
            "completed source audit is not under an external UUID run root"
        ) from exc
    if str(audit_uuid) != source_audit_path.parents[2].name.casefold():
        raise Session8BAuthorError(
            "completed source-audit run root is not a canonical UUID"
        )
    return (
        "preflight" if is_preflight else "author",
        user_dir,
        source_audit_path,
    )


def _asset_paths(root: str) -> list[str]:
    return sorted(
        str(path)
        for path in unreal.EditorAssetLibrary.list_assets(
            root, recursive=True, include_folder=False
        )
    )


def _package_from_asset_path(asset_path: str) -> str:
    return asset_path.split(".", 1)[0]


def _package_from_content_file(content_root: Path, path: Path) -> str:
    relative = path.relative_to(content_root)
    if path.suffix.casefold() not in ALLOWED_PACKAGE_FILE_SUFFIXES:
        raise Session8BAuthorError(f"unexpected package file suffix: {path}")
    without_suffix = relative.with_suffix("").as_posix()
    return f"/Game/{without_suffix}"


def _dirty_content_packages() -> list[Any]:
    return list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())


def _dirty_map_packages() -> list[Any]:
    return list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())


def _package_names(packages: list[Any]) -> list[str]:
    return sorted(str(package.get_name()) for package in packages)


def _require_clean_editor() -> None:
    dirty_content = _package_names(_dirty_content_packages())
    dirty_maps = _package_names(_dirty_map_packages())
    if dirty_content or dirty_maps:
        raise Session8BAuthorError(
            "isolated commandlet began with dirty packages: "
            f"content={dirty_content}, maps={dirty_maps}"
        )


def _require_only_frozen_source_load_dirty(
    source_file: Path,
    metahuman_root: Path,
    baseline_meta_records: dict[str, dict[str, Any]],
) -> list[str]:
    """Accept UE's observed load-time dirtiness for this source package only.

    UE 5.8.1 initializes some transient/internal MetaHuman state on load and
    marks the owning source package dirty even during the read-only audit.  The
    exception stays safe only while the disk artifact and full owned tree are
    still the exact frozen pre-load bytes and no map or second content package
    is dirty.
    """

    dirty_content = _package_names(_dirty_content_packages())
    dirty_maps = _package_names(_dirty_map_packages())
    if dirty_maps:
        raise Session8BAuthorError(
            f"MetaHuman source load dirtied map packages: {dirty_maps}"
        )
    if not set(dirty_content).issubset({SOURCE_PACKAGE_NAME}):
        raise Session8BAuthorError(
            "MetaHuman source load dirtied packages outside the one-file allowlist: "
            f"{dirty_content}"
        )
    source_record = _file_record(source_file)
    if (
        source_record["bytes"] != EXPECTED_SOURCE_PACKAGE_BYTES
        or source_record["sha256"] != EXPECTED_SOURCE_PACKAGE_SHA256
    ):
        raise Session8BAuthorError(
            "load-time source dirtiness coincides with frozen package disk drift"
        )
    if _directory_snapshot(metahuman_root) != baseline_meta_records:
        raise Session8BAuthorError(
            "load-time source dirtiness coincides with MetaHuman disk-tree drift"
        )
    return dirty_content


def _validate_accepted_save(path: Path, label: str) -> dict[str, Any]:
    record = _file_record(path)
    if (
        record["bytes"] != ACCEPTED_SAVE_BYTES
        or record["sha256"] != ACCEPTED_SAVE_SHA256
    ):
        raise Session8BAuthorError(
            f"{label} is not the frozen accepted Session 7 A999 baseline: {record}"
        )
    return record


def _validate_create_report(
    path: Path, source_file: Path, source_record: dict[str, Any]
) -> dict[str, Any]:
    payload = _strict_json(path)
    if payload.get("schema_version") != 1:
        raise Session8BAuthorError("source create/open report schema is not 1")
    if payload.get("status") != "PASS_CREATED_AND_OPENED":
        raise Session8BAuthorError(
            "canonical source must come from the accepted fresh-create proof"
        )
    if payload.get("character_object_path") != SOURCE_OBJECT_PATH:
        raise Session8BAuthorError("create/open report names a different source asset")
    if payload.get("character_class") != "/Script/MetaHumanCharacter.MetaHumanCharacter":
        raise Session8BAuthorError("create/open report character class is not MetaHumanCharacter")
    if payload.get("created") is not True or payload.get("assembled") is not False:
        raise Session8BAuthorError("create/open report does not prove a fresh unassembled source")
    if payload.get("cloud_auto_rig_requested") is not False:
        raise Session8BAuthorError("create/open report already requested auto-rigging")
    if payload.get("cloud_texture_sources_requested") is not False:
        raise Session8BAuthorError("create/open report already requested texture sources")
    if payload.get("qualification") != (
        "SOURCE_ASSET_ONLY_NOT_RIGGED_NOT_TEXTURE_DOWNLOADED_NOT_ASSEMBLED"
    ):
        raise Session8BAuthorError("create/open report qualification is not source-only")
    if payload.get("package_size") != source_record["bytes"]:
        raise Session8BAuthorError("source package size drifted after create/open proof")
    if str(payload.get("package_sha256", "")).upper() != source_record["sha256"]:
        raise Session8BAuthorError("source package hash drifted after create/open proof")
    if path.stat().st_mtime_ns < source_file.stat().st_mtime_ns:
        raise Session8BAuthorError("create/open report is older than the source package")
    if (
        source_record["bytes"] != EXPECTED_SOURCE_PACKAGE_BYTES
        or source_record["sha256"] != EXPECTED_SOURCE_PACKAGE_SHA256
    ):
        raise Session8BAuthorError("source package is not the frozen post-create artifact")
    return payload


def _validate_source_audit(
    path: Path, source_file: Path
) -> dict[str, Any]:
    report_record = _file_record(path)
    if (
        report_record["bytes"] != EXPECTED_SOURCE_AUDIT_BYTES
        or report_record["sha256"] != EXPECTED_SOURCE_AUDIT_SHA256
    ):
        raise Session8BAuthorError(
            "source audit report is not the frozen completed external artifact"
        )
    payload = _strict_json(path)
    if payload.get("schema_version") != 1:
        raise Session8BAuthorError("source audit report schema is not 1")
    if payload.get("status") != "PASS_READ_ONLY_SOURCE_AUDIT":
        raise Session8BAuthorError("source audit did not pass read-only")
    if payload.get("character_object_path") != SOURCE_OBJECT_PATH:
        raise Session8BAuthorError("source audit names a different character")
    if payload.get("character_class") != "/Script/MetaHumanCharacter.MetaHumanCharacter":
        raise Session8BAuthorError("source audit class is not MetaHumanCharacter")
    if payload.get("has_high_resolution_textures") is not False:
        raise Session8BAuthorError("source audit already reports high-resolution textures")
    if payload.get("can_build") is not False:
        raise Session8BAuthorError("source audit already reports a buildable character")
    if payload.get("writes") != []:
        raise Session8BAuthorError("source audit was not a zero-write audit")
    if path.stat().st_mtime_ns < source_file.stat().st_mtime_ns:
        raise Session8BAuthorError("source audit is stale relative to the source package")

    landmarks = payload.get("face_landmark_count")
    constraints = payload.get("body_constraints")
    if landmarks != EXPECTED_FACE_LANDMARK_COUNT:
        raise Session8BAuthorError(
            f"source audit landmark count is not {EXPECTED_FACE_LANDMARK_COUNT}"
        )
    if payload.get("body_constraint_count") != EXPECTED_BODY_CONSTRAINT_COUNT:
        raise Session8BAuthorError(
            f"source audit constraint count is not {EXPECTED_BODY_CONSTRAINT_COUNT}"
        )
    if not isinstance(constraints, list) or len(constraints) != (
        EXPECTED_BODY_CONSTRAINT_COUNT
    ):
        raise Session8BAuthorError("source audit body-constraint evidence is malformed")
    constraint_names = [entry.get("name") for entry in constraints if isinstance(entry, dict)]
    if len(constraint_names) != len(constraints) or len(set(constraint_names)) != len(
        constraint_names
    ):
        raise Session8BAuthorError("source audit body constraints are missing or duplicated")
    missing_constraints = sorted(set(BODY_CONSTRAINT_RATIOS) - set(constraint_names))
    if missing_constraints:
        raise Session8BAuthorError(
            "source audit lacks frozen body constraints: "
            + ", ".join(missing_constraints)
        )
    for entry in constraints:
        if entry.get("active") is not False:
            raise Session8BAuthorError(
                f"source audit constraint is already active: {entry.get('name')}"
            )
        values = (entry.get("minimum"), entry.get("target"), entry.get("maximum"))
        if not all(isinstance(value, (int, float)) and math.isfinite(value) for value in values):
            raise Session8BAuthorError(
                f"source audit constraint has non-finite evidence: {entry.get('name')}"
            )
        if not float(values[0]) <= float(values[1]) <= float(values[2]):
            raise Session8BAuthorError(
                f"source audit constraint target is out of range: {entry.get('name')}"
            )

    for key, expected_count in (
        ("available_hair_wardrobe_items", EXPECTED_HAIR_WARDROBE_COUNT),
        ("available_beard_wardrobe_items", EXPECTED_BEARD_WARDROBE_COUNT),
        ("available_clothing_wardrobe_items", EXPECTED_CLOTHING_WARDROBE_COUNT),
    ):
        values = payload.get(key)
        if not isinstance(values, list) or len(values) != expected_count:
            raise Session8BAuthorError(
                f"source audit {key} count is not the frozen {expected_count}"
            )

    audit_asset_lists = {
        path
        for key in (
            "available_hair_wardrobe_items",
            "available_beard_wardrobe_items",
            "available_clothing_wardrobe_items",
        )
        for path in payload.get(key, [])
        if isinstance(path, str)
    }
    missing_wardrobe = sorted(
        asset_path
        for _, asset_path in WARDROBE_SELECTIONS
        if asset_path not in audit_asset_lists
    )
    if missing_wardrobe:
        raise Session8BAuthorError(
            "source audit lacks frozen wardrobe assets: " + ", ".join(missing_wardrobe)
        )
    return payload


def _validate_source_tree(
    content_root: Path,
    metahuman_root: Path,
    source_file: Path,
) -> tuple[dict[str, bytes], dict[str, dict[str, Any]]]:
    if not source_file.is_file():
        raise Session8BAuthorError(f"canonical source package is absent: {source_file}")
    baseline_bytes = _directory_bytes(metahuman_root)
    baseline_records = _records_from_bytes(baseline_bytes)
    source_relative_stem = (
        Path("Source") / CHARACTER_ASSET_NAME
    ).as_posix().casefold()
    unexpected = []
    for relative in baseline_bytes:
        candidate = Path(relative)
        stem = candidate.with_suffix("").as_posix().casefold()
        if stem != source_relative_stem or candidate.suffix.casefold() not in (
            ALLOWED_PACKAGE_FILE_SUFFIXES
        ):
            unexpected.append(relative)
    if unexpected:
        raise Session8BAuthorError(
            "MetaHuman tree contains pre-existing non-source files: "
            + ", ".join(sorted(unexpected))
        )

    generated_disk = content_root / "DiscGolf/Characters/MetaHuman/Generated"
    common_disk = content_root / "DiscGolf/Characters/MetaHuman/Common"
    for label, root in (("Generated", generated_disk), ("Common", common_disk)):
        files = [path for path in root.rglob("*") if path.is_file()] if root.exists() else []
        if files:
            raise Session8BAuthorError(
                f"dedicated {label} root must be empty before authoring: {files}"
            )
    if _asset_paths(GENERATED_ROOT) or _asset_paths(COMMON_ROOT):
        raise Session8BAuthorError(
            "Generated/Common asset-registry roots must be empty before authoring"
        )
    if unreal.EditorAssetLibrary.does_asset_exist(EXPECTED_GENERATED_BP):
        raise Session8BAuthorError("expected generated Blueprint already exists")
    if unreal.EditorAssetLibrary.does_asset_exist(RUNTIME_WRAPPER_OBJECT_PATH):
        raise Session8BAuthorError(
            "runtime wrapper already exists; it is outside this author transaction"
        )
    return baseline_bytes, baseline_records


def _clean_empty_rollback_residue(metahuman_root: Path) -> list[str]:
    """Remove only empty directories below the two dedicated author roots."""
    removed: list[str] = []
    for root_name in ("Generated", "Common"):
        dedicated = metahuman_root / root_name
        if not dedicated.exists():
            continue
        if not _is_within(dedicated, metahuman_root):
            raise Session8BAuthorError(
                f"rollback residue root escaped the owned MetaHuman tree: {dedicated}"
            )
        directories = sorted(
            (candidate for candidate in dedicated.rglob("*") if candidate.is_dir()),
            key=lambda candidate: len(candidate.parts),
            reverse=True,
        )
        directories.append(dedicated)
        for directory in directories:
            if any(directory.iterdir()):
                continue
            relative = directory.relative_to(metahuman_root).as_posix()
            directory.rmdir()
            removed.append(relative)
    return sorted(removed)


def _validate_live_source(
    audit: dict[str, Any]
) -> tuple[Any, Any, dict[str, Any]]:
    character = unreal.load_asset(SOURCE_OBJECT_PATH)
    if not isinstance(character, unreal.MetaHumanCharacter):
        raise Session8BAuthorError(
            f"canonical object is not a MetaHumanCharacter: {SOURCE_OBJECT_PATH}"
        )
    if bool(character.get_editor_property("has_high_resolution_textures")):
        raise Session8BAuthorError("canonical source already has high-resolution textures")

    subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if bool(subsystem.can_build_meta_human(character=character, log_error=False)):
        raise Session8BAuthorError("canonical source is already buildable")
    if not subsystem.try_add_object_to_edit(character=character):
        raise Session8BAuthorError(
            "canonical source could not be acquired for isolated editing"
        )
    try:
        landmarks = list(subsystem.get_face_landmarks(character=character))
        constraints = list(subsystem.get_body_constraints(character=character))
        if len(landmarks) != audit["face_landmark_count"]:
            raise Session8BAuthorError(
                "live face landmark count differs from the completed source audit"
            )
        if len(constraints) != audit["body_constraint_count"]:
            raise Session8BAuthorError(
                "live body constraint count differs from the completed source audit"
            )
        slot_data = list(
            character.internal_collection.default_instance.get_slot_selection_data()
        )
        slot_names = sorted(str(entry.selection.slot_name) for entry in slot_data)
        if slot_names != ["Character"]:
            raise Session8BAuthorError(
                "fresh source must have only its Character slot selected; got "
                f"{slot_names}"
            )
        live = {
            "face_landmark_count": len(landmarks),
            "body_constraint_count": len(constraints),
            "initial_slot_names": slot_names,
            "has_high_resolution_textures": False,
            "can_build": False,
        }
    finally:
        if subsystem.is_object_added_for_editing(character=character):
            subsystem.remove_object_to_edit(character=character)
    return character, subsystem, live


def _landmark_digest(landmarks: list[Any]) -> str:
    canonical = [
        [round(float(point.x), 6), round(float(point.y), 6), round(float(point.z), 6)]
        for point in landmarks
    ]
    return _sha256_bytes(
        json.dumps(canonical, separators=(",", ":")).encode("ascii")
    )


def _apply_original_face_design(subsystem: Any, character: Any) -> dict[str, Any]:
    before = list(subsystem.get_face_landmarks(character=character))
    if not before:
        raise Session8BAuthorError("editable character returned no face landmarks")
    coordinates = [
        (float(point.x), float(point.y), float(point.z)) for point in before
    ]
    if not all(math.isfinite(value) for point in coordinates for value in point):
        raise Session8BAuthorError("face landmarks contain non-finite values")
    centroid = tuple(
        sum(point[axis] for point in coordinates) / len(coordinates)
        for axis in range(3)
    )
    deltas = []
    delta_magnitudes = []
    for point in coordinates:
        delta = tuple(
            (point[axis] - centroid[axis]) * (FACE_LANDMARK_SCALES[axis] - 1.0)
            for axis in range(3)
        )
        magnitude = math.sqrt(sum(value * value for value in delta))
        if not math.isfinite(magnitude) or magnitude > MAX_FACE_LANDMARK_DELTA:
            raise Session8BAuthorError(
                f"frozen face edit exceeded its safety bound: {magnitude}"
            )
        deltas.append(unreal.Vector(x=delta[0], y=delta[1], z=delta[2]))
        delta_magnitudes.append(magnitude)
    if max(delta_magnitudes) <= 1.0e-5:
        raise Session8BAuthorError("frozen face edit would be visually null")

    subsystem.translate_face_landmarks(
        character=character,
        landmark_indices=list(range(len(deltas))),
        deltas=deltas,
    )
    subsystem.commit_face_state(character=character)
    after = list(subsystem.get_face_landmarks(character=character))
    if len(after) != len(before):
        raise Session8BAuthorError("face landmark count changed during local sculpt")
    before_digest = _landmark_digest(before)
    after_digest = _landmark_digest(after)
    if before_digest == after_digest:
        raise Session8BAuthorError("face commit did not change the editable state")
    return {
        "operation": "CENTROID_RELATIVE_ANISOTROPIC_LOCAL_SCULPT",
        "landmark_count": len(before),
        "scales": list(FACE_LANDMARK_SCALES),
        "centroid": [round(value, 6) for value in centroid],
        "maximum_requested_delta": max(delta_magnitudes),
        "before_sha256": before_digest,
        "after_sha256": after_digest,
    }


def _apply_original_body_design(subsystem: Any, character: Any) -> dict[str, Any]:
    constraints = list(subsystem.get_body_constraints(character=character))
    by_name = {str(constraint.name): constraint for constraint in constraints}
    missing = sorted(set(BODY_CONSTRAINT_RATIOS) - set(by_name))
    if missing:
        raise Session8BAuthorError(
            "live parametric body lacks frozen constraints: " + ", ".join(missing)
        )

    requested: dict[str, dict[str, Any]] = {}
    for name, ratio in BODY_CONSTRAINT_RATIOS.items():
        constraint = by_name[name]
        minimum = float(constraint.min_measurement)
        maximum = float(constraint.max_measurement)
        if not math.isfinite(minimum) or not math.isfinite(maximum) or maximum <= minimum:
            raise Session8BAuthorError(
                f"invalid body-constraint range for {name}: {minimum}..{maximum}"
            )
        target = minimum + (maximum - minimum) * ratio
        constraint.is_active = True
        constraint.target_measurement = target
        requested[name] = {
            "ratio": ratio,
            "minimum": minimum,
            "maximum": maximum,
            "target": target,
        }

    subsystem.set_body_constraints(character, constraints)
    subsystem.commit_body_state(character=character)
    committed = {
        str(constraint.name): constraint
        for constraint in subsystem.get_body_constraints(character=character)
    }
    for name, evidence in requested.items():
        actual = committed.get(name)
        if actual is None or not bool(actual.is_active):
            raise Session8BAuthorError(f"body constraint {name} was not committed active")
        if not math.isclose(
            float(actual.target_measurement),
            float(evidence["target"]),
            rel_tol=0.0,
            abs_tol=1.0e-3,
        ):
            raise Session8BAuthorError(f"body constraint {name} target did not commit")
    return {
        "operation": "PARAMETRIC_RANGE_FRACTIONS_NO_REAL_PERSON_MEASUREMENTS",
        "constraints": requested,
    }


def _apply_frozen_wardrobe(character: Any) -> dict[str, str]:
    selected_keys: dict[str, Any] = {}
    selected_paths: dict[str, str] = {}
    for slot_name, asset_path in WARDROBE_SELECTIONS:
        wardrobe_item = unreal.load_asset(asset_path)
        if not isinstance(wardrobe_item, unreal.MetaHumanWardrobeItem):
            raise Session8BAuthorError(
                f"frozen {slot_name} wardrobe item is absent or wrong class: {asset_path}"
            )
        item_key = character.internal_collection.try_add_item_from_wardrobe_item(
            slot_name=slot_name,
            wardrobe_item=wardrobe_item,
        )
        if item_key is None:
            raise Session8BAuthorError(
                f"failed to add frozen wardrobe item for slot {slot_name}: {asset_path}"
            )
        selection = unreal.MetaHumanPipelineSlotSelection(
            slot_name=slot_name,
            selected_item=item_key,
        )
        if not character.internal_collection.default_instance.try_add_slot_selection(
            selection=selection
        ):
            raise Session8BAuthorError(
                f"failed to select frozen wardrobe item for slot {slot_name}"
            )
        selected_keys[slot_name] = item_key
        selected_paths[slot_name] = asset_path

    live_slots: dict[str, Any] = {}
    for entry in character.internal_collection.default_instance.get_slot_selection_data():
        selection = entry.selection
        live_slots[str(selection.slot_name)] = selection.selected_item
    if set(live_slots) != {"Character", *selected_keys.keys()}:
        raise Session8BAuthorError(
            f"unexpected slots after frozen wardrobe selection: {sorted(live_slots)}"
        )
    for slot_name, expected_key in selected_keys.items():
        if not live_slots[slot_name].references_same_asset(expected_key):
            raise Session8BAuthorError(
                f"wardrobe selection verification failed for {slot_name}"
            )
    return selected_paths


def _output_package_allowed(package_name: str) -> bool:
    return (
        package_name == SOURCE_PACKAGE_NAME
        or package_name.startswith(GENERATED_CHARACTER_ROOT + "/")
        or package_name.startswith(COMMON_ROOT + "/")
    )


def _verify_build_outputs() -> tuple[list[str], set[str]]:
    generated_assets = _asset_paths(GENERATED_CHARACTER_ROOT)
    common_assets = _asset_paths(COMMON_ROOT)
    if EXPECTED_GENERATED_BP not in generated_assets:
        raise Session8BAuthorError(
            f"assembled Blueprint is absent from generated outputs: {EXPECTED_GENERATED_BP}"
        )
    generated_bp = unreal.load_asset(EXPECTED_GENERATED_BP)
    if not isinstance(generated_bp, unreal.Blueprint):
        raise Session8BAuthorError("expected assembled output is not a Blueprint")
    if len(generated_assets) < 2:
        raise Session8BAuthorError("assembly produced an implausibly small Generated set")
    if not common_assets:
        raise Session8BAuthorError("assembly produced no dedicated Common assets")
    assets = sorted(set(generated_assets + common_assets))
    packages = {_package_from_asset_path(path) for path in assets}
    if not packages or any(not _output_package_allowed(name) for name in packages):
        raise Session8BAuthorError("assembled package set escaped the dedicated roots")
    if (
        len(assets) != EXPECTED_PERSISTENT_GENERATED_OUTPUT_COUNT
        or len(packages) != EXPECTED_PERSISTENT_GENERATED_OUTPUT_COUNT
    ):
        raise Session8BAuthorError(
            "persistent Generated/Common output partition changed: "
            f"assets={len(assets)} packages={len(packages)} "
            f"expected={EXPECTED_PERSISTENT_GENERATED_OUTPUT_COUNT}"
        )
    return assets, packages


def _validate_exact_discarded_transients(
    package_objects: dict[str, Any],
    registry_output_packages: set[str],
    content_root: Path,
) -> tuple[list[dict[str, Any]], set[str]]:
    discarded_packages = (
        set(package_objects) - set(registry_output_packages) - {SOURCE_PACKAGE_NAME}
    )
    expected_packages = set(EXPECTED_DISCARDED_TRANSIENT_CLASSES)
    if discarded_packages != expected_packages:
        raise Session8BAuthorError(
            "discarded transient package set is not the exact Epic Optimized "
            "Medium groom-bake contract: "
            f"missing={sorted(expected_packages - discarded_packages)}, "
            f"unexpected={sorted(discarded_packages - expected_packages)}"
        )

    listed_asset_paths = set(_asset_paths(GENERATED_CHARACTER_ROOT))
    records: list[dict[str, Any]] = []
    for package_name in sorted(discarded_packages):
        if not _output_package_allowed(package_name):
            raise Session8BAuthorError(
                f"discarded transient escaped dedicated roots: {package_name}"
            )
        object_path = f"{package_name}.{package_name.rsplit('/', 1)[-1]}"
        expected_class = EXPECTED_DISCARDED_TRANSIENT_CLASSES[package_name]
        asset = unreal.find_object(None, object_path, expected_class.static_class())
        if not isinstance(asset, expected_class):
            raise Session8BAuthorError(
                "discarded transient is absent or has the wrong class: "
                f"{object_path} expected={expected_class.__name__}"
            )
        if str(asset.get_path_name()) != object_path:
            raise Session8BAuthorError(
                f"discarded transient path mismatch: {asset.get_path_name()} != {object_path}"
            )
        object_asset_data = unreal.AssetRegistryHelpers.create_asset_data(asset, False)
        if unreal.AssetRegistryHelpers.is_valid(object_asset_data):
            raise Session8BAuthorError(
                f"discarded transient still reports valid asset data: {object_path}"
            )
        registry_matches = list(
            unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_package_name(
                package_name
            )
        )
        if registry_matches:
            raise Session8BAuthorError(
                f"discarded transient remains in the asset registry: {object_path}"
            )
        if object_path in listed_asset_paths:
            raise Session8BAuthorError(
                f"discarded transient appears in the registry asset list: {object_path}"
            )
        package_relative = package_name.removeprefix("/Game/")
        unexpected_files = [
            str(content_root / f"{package_relative}{suffix}")
            for suffix in sorted(ALLOWED_PACKAGE_FILE_SUFFIXES)
            if (content_root / f"{package_relative}{suffix}").is_file()
        ]
        if unexpected_files:
            raise Session8BAuthorError(
                "discarded transient unexpectedly exists on disk: "
                + ", ".join(unexpected_files)
            )
        records.append(
            {
                "package": package_name,
                "object_path": object_path,
                "class": expected_class.__name__,
                "asset_data_valid": False,
                "registry_present": False,
                "editor_asset_library_presence_check": (
                    "NOT_AUTHORITATIVE_FOR_LIVE_NON_ASSET_OBJECT"
                ),
                "listed_as_output_asset": False,
                "disk_files_before": [],
                "disk_files_after": [],
                "dirty_at_commandlet_exit": True,
                "disposition": TRANSIENT_DISPOSITION,
                "asset_registry_action": "NO_RENOTIFICATION",
            }
        )
    return records, discarded_packages


def _save_exact_packages(
    package_objects: dict[str, Any],
    output_package_names: set[str],
    discarded_package_names: set[str],
    content_root: Path,
    state: dict[str, Any],
) -> list[str]:
    expected_saved = set(output_package_names) | {SOURCE_PACKAGE_NAME}
    expected_dirty = expected_saved | set(discarded_package_names)
    actual_dirty = set(package_objects)
    if actual_dirty != expected_dirty:
        raise Session8BAuthorError(
            "dirty package set is not the exact save allowlist: "
            f"missing={sorted(expected_dirty - actual_dirty)}, "
            f"unexpected={sorted(actual_dirty - expected_dirty)}"
        )
    if _dirty_map_packages():
        raise Session8BAuthorError("assembly dirtied a map package")

    output_packages = [package_objects[name] for name in sorted(output_package_names)]
    state["save_called"] = True
    state["save_attempted_packages"] = sorted(output_package_names)
    if not unreal.EditorLoadingAndSavingUtils.save_packages(output_packages, False):
        raise Session8BAuthorError("saving the exact Generated/Common package set failed")

    remaining = {
        str(package.get_name()): package for package in _dirty_content_packages()
    }
    expected_after_outputs = {SOURCE_PACKAGE_NAME} | set(discarded_package_names)
    if set(remaining) != expected_after_outputs:
        raise Session8BAuthorError(
            "unexpected dirty packages remained after output save: "
            f"{sorted(remaining)}"
        )
    state["save_attempted_packages"] = sorted(expected_saved)
    if not unreal.EditorLoadingAndSavingUtils.save_packages(
        [remaining[SOURCE_PACKAGE_NAME]], False
    ):
        raise Session8BAuthorError("saving the canonical source package failed")
    final_dirty = set(_package_names(_dirty_content_packages()))
    if final_dirty != set(discarded_package_names) or _dirty_map_packages():
        raise Session8BAuthorError(
            "post-save dirty set is not the exact intentionally discarded "
            f"transient set: actual={sorted(final_dirty)}"
        )
    for package_name in sorted(discarded_package_names):
        package_relative = package_name.removeprefix("/Game/")
        for suffix in sorted(ALLOWED_PACKAGE_FILE_SUFFIXES):
            path = content_root / f"{package_relative}{suffix}"
            if path.is_file():
                raise Session8BAuthorError(
                    f"discarded transient was persisted unexpectedly: {path}"
                )
    state["intentionally_discarded_dirty_packages_at_exit"] = sorted(final_dirty)
    state["dirty_content_packages_at_exit"] = sorted(final_dirty)
    state["dirty_map_packages_at_exit"] = []
    state["unexpected_dirty_packages_at_exit"] = []
    state["commandlet_exit_discards_transients"] = True
    return sorted(expected_saved)


def _restore_metahuman_tree(
    metahuman_root: Path,
    baseline_bytes: dict[str, bytes],
) -> dict[str, Any]:
    removed: list[str] = []
    restored: list[str] = []
    errors: list[str] = []
    if not metahuman_root.exists():
        metahuman_root.mkdir(parents=True, exist_ok=True)
    current_files = sorted(
        candidate for candidate in metahuman_root.rglob("*") if candidate.is_file()
    )
    for path in current_files:
        relative = path.relative_to(metahuman_root).as_posix()
        try:
            if relative not in baseline_bytes:
                path.unlink()
                removed.append(relative)
        except OSError as exc:
            errors.append(f"remove {relative}: {exc}")
    for relative, expected in sorted(baseline_bytes.items()):
        path = metahuman_root / Path(relative)
        try:
            actual = path.read_bytes() if path.is_file() else None
            if actual != expected:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(expected)
                restored.append(relative)
        except OSError as exc:
            errors.append(f"restore {relative}: {exc}")

    removed_empty_directories: list[str] = []
    for directory in sorted(
        (candidate for candidate in metahuman_root.rglob("*") if candidate.is_dir()),
        key=lambda candidate: len(candidate.parts),
        reverse=True,
    ):
        try:
            if not any(directory.iterdir()):
                relative = directory.relative_to(metahuman_root).as_posix()
                directory.rmdir()
                removed_empty_directories.append(relative)
        except OSError as exc:
            errors.append(f"clean empty directory {directory}: {exc}")

    after = _directory_snapshot(metahuman_root)
    expected_records = _records_from_bytes(baseline_bytes)
    return {
        "removed_new_files": removed,
        "removed_empty_directories": sorted(removed_empty_directories),
        "restored_baseline_files": restored,
        "errors": errors,
        "byte_exact": after == expected_records and not errors,
        "after": after,
    }


def _changed_meta_files(
    before: dict[str, dict[str, Any]],
    after: dict[str, dict[str, Any]],
) -> tuple[list[str], list[str], list[str]]:
    before_names = set(before)
    after_names = set(after)
    created = sorted(after_names - before_names)
    deleted = sorted(before_names - after_names)
    changed = sorted(
        name for name in before_names & after_names if before[name] != after[name]
    )
    return created, changed, deleted


def _validate_successful_disk_delta(
    content_root: Path,
    metahuman_root: Path,
    baseline: dict[str, dict[str, Any]],
    saved_package_names: set[str],
) -> dict[str, Any]:
    after = _directory_snapshot(metahuman_root)
    created, changed, deleted = _changed_meta_files(baseline, after)
    if deleted:
        raise Session8BAuthorError(
            "successful authoring deleted baseline MetaHuman files: " + ", ".join(deleted)
        )
    touched = created + changed
    if not touched:
        raise Session8BAuthorError("successful authoring produced no package file delta")
    touched_packages = set()
    for relative in touched:
        absolute = metahuman_root / Path(relative)
        touched_packages.add(_package_from_content_file(content_root, absolute))
    if touched_packages != saved_package_names:
        raise Session8BAuthorError(
            "disk delta does not exactly equal the saved-package allowlist: "
            f"missing={sorted(saved_package_names - touched_packages)}, "
            f"unexpected={sorted(touched_packages - saved_package_names)}"
        )
    missing_package_files = []
    for package_name in sorted(saved_package_names):
        uasset = content_root / (package_name.removeprefix("/Game/") + ".uasset")
        if not uasset.is_file():
            missing_package_files.append(str(uasset))
    if missing_package_files:
        raise Session8BAuthorError(
            "saved allowlist packages lack .uasset files: "
            + ", ".join(missing_package_files)
        )
    return {
        "created_files": created,
        "changed_files": changed,
        "deleted_files": deleted,
        "touched_packages": sorted(touched_packages),
        "files": after,
    }


def _run_author(
    character: Any,
    subsystem: Any,
    content_root: Path,
    metahuman_root: Path,
    baseline_meta_records: dict[str, dict[str, Any]],
    state: dict[str, Any],
) -> dict[str, Any]:
    if not subsystem.try_add_object_to_edit(character=character):
        raise Session8BAuthorError("failed to reacquire source for isolated authoring")
    editing = True
    try:
        state["phase"] = "APPLY_ORIGINAL_EDITABLE_DESIGN"
        face = _apply_original_face_design(subsystem, character)
        body = _apply_original_body_design(subsystem, character)
        wardrobe = _apply_frozen_wardrobe(character)
        if bool(character.get_editor_property("has_high_resolution_textures")):
            raise Session8BAuthorError("source became high-resolution before cloud requests")
        if bool(subsystem.can_build_meta_human(character=character, log_error=False)):
            raise Session8BAuthorError("source became buildable before cloud requests")

        state["phase"] = "BLOCKING_JOINTS_ONLY_AUTO_RIG"
        rig_request = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rig_request.blocking = True
        rig_request.report_progress = False
        rig_request.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        state["cloud_auto_rig_requested"] = True
        subsystem.request_auto_rigging(character, rig_request)

        state["phase"] = "BLOCKING_TEXTURE_SOURCES"
        texture_request = unreal.MetaHumanCharacterTextureRequestParams()
        texture_request.blocking = True
        texture_request.report_progress = False
        state["cloud_texture_sources_requested"] = True
        subsystem.request_texture_sources(character, texture_request)

        has_high_resolution_textures = bool(
            character.get_editor_property("has_high_resolution_textures")
        )
        can_build = bool(
            subsystem.can_build_meta_human(character=character, log_error=False)
        )
        if not has_high_resolution_textures:
            raise Session8BAuthorError(
                "blocking texture-source request did not produce high-resolution textures"
            )
        if not can_build:
            raise Session8BAuthorError(
                "source is not buildable after blocking JointsOnly rig and texture requests"
            )

        state["phase"] = "UE_OPTIMIZED_MEDIUM_ASSEMBLY"
        build_params = unreal.MetaHumanCharacterEditorBuildParameters()
        build_params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        build_params.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
        build_params.absolute_build_path = GENERATED_ROOT
        build_params.common_folder_path = COMMON_ROOT
        build_params.enable_wardrobe_item_validation = True
        subsystem.build_meta_human(character=character, params=build_params)
        output_assets, output_packages = _verify_build_outputs()
    finally:
        if editing and subsystem.is_object_added_for_editing(character=character):
            subsystem.remove_object_to_edit(character=character)

    state["phase"] = "EXACT_PACKAGE_ALLOWLIST"
    dirty_packages = _dirty_content_packages()
    package_objects = {
        str(package.get_name()): package for package in dirty_packages
    }
    if len(package_objects) != len(dirty_packages):
        raise Session8BAuthorError("dirty package names are duplicated")
    discarded_records, discarded_packages = _validate_exact_discarded_transients(
        package_objects=package_objects,
        registry_output_packages=output_packages,
        content_root=content_root,
    )
    state["transient_tombstones"] = discarded_records
    state["discarded_transient_packages"] = {
        package_name: EXPECTED_DISCARDED_TRANSIENT_CLASSES[package_name].__name__
        for package_name in sorted(discarded_packages)
    }
    state["exact_save_allowlist"] = sorted(output_packages | {SOURCE_PACKAGE_NAME})
    state["persistent_generated_output_count"] = len(output_packages)
    state["persistent_save_allowlist_count"] = len(state["exact_save_allowlist"])
    state["transient_tombstone_count"] = len(discarded_packages)

    state["phase"] = "SAVE_EXACT_ALLOWLIST"
    saved_packages = _save_exact_packages(
        package_objects=package_objects,
        output_package_names=output_packages,
        discarded_package_names=discarded_packages,
        content_root=content_root,
        state=state,
    )
    state["saved_packages"] = saved_packages

    state["phase"] = "POST_SAVE_DISK_AUDIT"
    disk_delta = _validate_successful_disk_delta(
        content_root=content_root,
        metahuman_root=metahuman_root,
        baseline=baseline_meta_records,
        saved_package_names=set(saved_packages),
    )
    return {
        "design": {
            "id": DESIGN_ID,
            "intent": "ORIGINAL_GENERIC_NO_REAL_PERSON_OR_PRESET_REFERENCE",
            "face": face,
            "body": body,
        },
        "wardrobe": wardrobe,
        "cloud": {
            "auto_rig": "BLOCKING_JOINTS_ONLY_COMPLETED",
            "texture_sources": "BLOCKING_HIGH_RESOLUTION_COMPLETED",
            "account_portal_fallback": "DISABLED_BY_COMMAND_LINE",
        },
        "post_cloud": {
            "has_high_resolution_textures": True,
            "can_build": True,
        },
        "assembly": {
            "pipeline": "UE_OPTIMIZED",
            "quality": "MEDIUM",
            "absolute_build_path": GENERATED_ROOT,
            "common_folder_path": COMMON_ROOT,
            "expected_blueprint": EXPECTED_GENERATED_BP,
            "asset_object_paths": output_assets,
        },
        "disk_delta": disk_delta,
    }


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    project_root = Path(unreal.Paths.project_dir()).resolve()
    content_root = Path(unreal.Paths.project_content_dir()).resolve()
    metahuman_root = content_root / "DiscGolf/Characters/MetaHuman"
    source_file = metahuman_root / f"Source/{CHARACTER_ASSET_NAME}.uasset"
    production_save = project_root / PRODUCTION_SAVE_RELATIVE

    # Use an OS temp fallback only to preserve a failure report when -UserDir is
    # itself malformed.  Valid runs always write under their external UUID root.
    fallback_report = (
        Path(tempfile.gettempdir())
        / "DiscGolfTour"
        / "Session8BMetaHumanAuthor.invalid-command-line.json"
    )
    report_path = fallback_report
    state: dict[str, Any] = {
        "schema": SCHEMA,
        "generated_utc": _utc_now(),
        "status": "FAIL_NOT_STARTED",
        "phase": "COMMAND_LINE_PREFLIGHT",
        "mode": "unknown",
        "source_object_path": SOURCE_OBJECT_PATH,
        "generated_root": GENERATED_ROOT,
        "generated_character_root": GENERATED_CHARACTER_ROOT,
        "common_root": COMMON_ROOT,
        "expected_generated_blueprint": EXPECTED_GENERATED_BP,
        "runtime_wrapper_created": False,
        "runtime_profile_created": False,
        "retarget_asset_created": False,
        "cloud_auto_rig_requested": False,
        "cloud_texture_sources_requested": False,
        "save_called": False,
        "save_attempted_packages": [],
        "saved_packages": [],
        "exact_save_allowlist": [],
        "transient_tombstones": [],
        "discarded_transient_packages": {},
        "intentionally_discarded_dirty_packages_at_exit": [],
        "dirty_content_packages_at_exit": [],
        "dirty_map_packages_at_exit": [],
        "unexpected_dirty_packages_at_exit": [],
        "commandlet_exit_discards_transients": False,
        "persistent_generated_output_count": 0,
        "persistent_save_allowlist_count": 0,
        "transient_tombstone_count": 0,
        "rollback_residue_cleanup": {
            "scope": ["Generated", "Common"],
            "removed_empty_directories": [],
        },
        "unreal_launched_by_script": False,
        "ubt_launched_by_script": False,
        "account_portal_fallback": "REQUIRED_DISABLED",
        "errors": [],
    }

    baseline_meta_bytes: dict[str, bytes] | None = None
    baseline_meta_records: dict[str, dict[str, Any]] | None = None
    production_before: dict[str, Any] | None = None
    backup_before: dict[str, Any] | None = None
    try:
        mode, user_dir, source_audit_path = _validate_command_line(
            command_line, project_root
        )
        report_path = user_dir / AUTHOR_REPORT_RELATIVE
        if _is_within(report_path, project_root) or not _is_within(report_path, user_dir):
            raise Session8BAuthorError("author report path is not external and user-dir scoped")
        if report_path.exists():
            raise Session8BAuthorError(
                f"fresh external UUID run already has an author report: {report_path}"
            )
        preexisting_external_saves = sorted(
            str(path) for path in user_dir.rglob("*.sav") if path.is_file()
        )
        if preexisting_external_saves:
            raise Session8BAuthorError(
                "fresh external UUID run already contains save files: "
                + ", ".join(preexisting_external_saves)
            )
        state["mode"] = mode
        state["external_user_dir"] = str(user_dir)
        state["external_report_path"] = str(report_path)
        state["command_line_contract"] = {
            "python_commandlet": True,
            "unattended": True,
            "nop4": True,
            "no_account_portal_login_fallback": True,
            "allow_commandlet_rendering": True,
            "render_offscreen": True,
            "d3d12_requested": True,
            "texturegraph_commandlets_enabled": True,
            "credential_arguments_present": False,
            "external_source_audit_argument": True,
        }

        state["phase"] = "IMMUTABLE_BASELINES"
        _require_clean_editor()
        production_before = _validate_accepted_save(production_save, "production save")
        backup_before = _validate_accepted_save(ACCEPTED_BACKUP, "accepted backup")
        state["rollback_residue_cleanup"]["removed_empty_directories"] = (
            _clean_empty_rollback_residue(metahuman_root)
        )
        baseline_meta_bytes, baseline_meta_records = _validate_source_tree(
            content_root=content_root,
            metahuman_root=metahuman_root,
            source_file=source_file,
        )
        source_record = _file_record(source_file)

        state["phase"] = "COMPLETED_SOURCE_AUDIT"
        create_report_path = project_root / CREATE_REPORT_RELATIVE
        create_report = _validate_create_report(
            create_report_path, source_file, source_record
        )
        source_audit = _validate_source_audit(source_audit_path, source_file)
        character, subsystem, live_source = _validate_live_source(source_audit)
        live_source["load_time_dirty_packages"] = (
            _require_only_frozen_source_load_dirty(
                source_file=source_file,
                metahuman_root=metahuman_root,
                baseline_meta_records=baseline_meta_records,
            )
        )

        state["preflight"] = {
            "create_report": str(create_report_path),
            "create_report_status": create_report["status"],
            "source_audit_report": str(source_audit_path),
            "source_audit_report_file": _file_record(source_audit_path),
            "source_audit_status": source_audit["status"],
            "source_package": source_record,
            "live_source": live_source,
            "production_save": production_before,
            "accepted_backup": backup_before,
            "generated_assets_before": [],
            "common_assets_before": [],
        }

        if mode == "preflight":
            state["phase"] = "PREFLIGHT_COMPLETE"
            if _file_record(production_save) != production_before:
                raise Session8BAuthorError("preflight changed the production save")
            if _file_record(ACCEPTED_BACKUP) != backup_before:
                raise Session8BAuthorError("preflight changed the accepted backup")
            state["status"] = "PASS_PREFLIGHT_NO_CLOUD_NO_ASSET_WRITES"
        else:
            author_result = _run_author(
                character=character,
                subsystem=subsystem,
                content_root=content_root,
                metahuman_root=metahuman_root,
                baseline_meta_records=baseline_meta_records,
                state=state,
            )
            state.update(author_result)
            if _file_record(production_save) != production_before:
                raise Session8BAuthorError("authoring changed the production save")
            if _file_record(ACCEPTED_BACKUP) != backup_before:
                raise Session8BAuthorError("authoring changed the accepted backup")
            state["production_save_unchanged"] = True
            state["accepted_backup_unchanged"] = True
            state["phase"] = "COMPLETE"
            state["status"] = "PASS_AUTHORED_ASSEMBLED_AND_SAVED"

    except Exception as exc:
        state["errors"].append(f"{type(exc).__name__}: {exc}")
        rollback: dict[str, Any] = {
            "attempted": False,
            "byte_exact": baseline_meta_bytes is None,
            "errors": [],
        }
        if baseline_meta_bytes is not None:
            rollback = _restore_metahuman_tree(
                metahuman_root=metahuman_root,
                baseline_bytes=baseline_meta_bytes,
            )
            rollback["attempted"] = True
        state["rollback"] = rollback

        production_unchanged = (
            production_before is None
            or _file_record(production_save) == production_before
        )
        backup_unchanged = (
            backup_before is None
            or _file_record(ACCEPTED_BACKUP) == backup_before
        )
        state["production_save_unchanged"] = production_unchanged
        state["accepted_backup_unchanged"] = backup_unchanged
        if rollback.get("byte_exact") and production_unchanged and backup_unchanged:
            state["status"] = (
                "FAIL_ROLLED_BACK_NO_PERSISTENT_ASSET_WRITES"
                if baseline_meta_bytes is not None
                else "FAIL_PREFLIGHT_NO_ASSET_WRITES"
            )
        else:
            state["status"] = "FAIL_ROLLBACK_INCOMPLETE"
        state["failed_utc"] = _utc_now()
        _write_report(report_path, state)
        unreal.log_error(
            "DG_SESSION8B_METAHUMAN_AUTHOR: FAIL "
            f"status={state['status']} phase={state['phase']} report={report_path}: {exc}"
        )
        raise

    state["completed_utc"] = _utc_now()
    _write_report(report_path, state)
    unreal.log(
        "DG_SESSION8B_METAHUMAN_AUTHOR: "
        f"{state['status']} mode={state['mode']} report={report_path}"
    )


if __name__ == "__main__":
    main()
