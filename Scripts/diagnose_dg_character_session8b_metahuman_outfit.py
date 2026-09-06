"""Run the strict no-asset-write Session 8B MetaHuman Outfit diagnostic."""

from __future__ import annotations

import hashlib
import json
import math
import re
import tempfile
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import unreal


SCHEMA = "DiscGolfTour.Session8BMetaHumanOutfitDiagnosticEnvelope.v1"
DIAGNOSTIC_SWITCH = "-DGSession8BMetaHumanOutfitDiagnostic"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanOutfitDiagnostic.json"
)
EXPECTED_PROJECT_ROOT = Path(r"C:\DGTour").resolve()
EXPECTED_USER_ROOT = Path(r"C:\DGTour_TestRuns\Session8B").resolve()
GENERATED_BLUEPRINT = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/"
    "BP_MHC_DG_Golfer_Default.BP_MHC_DG_Golfer_Default"
)
OUTFIT_OBJECT = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/"
    "Clothing/MHC_DG_Golfer_Default_Outfits."
    "MHC_DG_Golfer_Default_Outfits"
)
CONTENT_EXPECTED = {
    "file_count": 953,
    "bytes": 2920024366,
    "manifest_sha256": (
        "95400E9857F222C4454587A4AB36E0D1993DEA64B9EF48EF24969E5F8320D651"
    ),
}
CONTENT_DIRECTORY_COUNT = 230
WRAPPER_RELATIVE = Path(
    "Content/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.uasset"
)
RETARGET_RELATIVE = Path(
    "Content/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.uasset"
)
PROFILE_RELATIVE = Path(
    "Content/DiscGolf/Characters/Avatar/Data/"
    "DA_DG_AvatarBackend_MetaHuman_Default.uasset"
)
CANONICAL_EXPECTED = {
    WRAPPER_RELATIVE.as_posix(): {
        "exists": True,
        "bytes": 225494,
        "sha256": (
            "C322CBFA8F9F3268D2A7F05E20FF942F5A3B51F2D2C6AF53ED74E24266DF3E2C"
        ),
    },
    RETARGET_RELATIVE.as_posix(): {
        "exists": True,
        "bytes": 22640,
        "sha256": (
            "6316D573F271EE40BD7D3C42258723F9C8669A5606A0FB0373016DF3EBFBEAE9"
        ),
    },
    PROFILE_RELATIVE.as_posix(): {
        "exists": True,
        "bytes": 2274,
        "sha256": (
            "8D2B6F25EE629CA04FFF630BC8A3CD9030CB34D161A5955866C1BB208217DC2D"
        ),
    },
}
PRODUCTION_SAVE_RELATIVE = Path("Saved/SaveGames/DiscGolfTour_Profile_0.sav")
ACCEPTED_BACKUP = Path(
    r"C:\DGTour_Backups\Session7_Accepted"
    r"\DiscGolfTour_Profile_0_Session7_Accepted.sav"
)
ACCEPTED_SAVE = {
    "exists": True,
    "bytes": 5212,
    "sha256": (
        "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
    ),
}
PACKAGE_SUFFIXES = (".uasset", ".uexp", ".ubulk", ".uptnl")
ANCHORS = {
    "Root",
    "pelvis",
    "spine_01",
    "hand_l",
    "hand_r",
    "calf_l",
    "calf_r",
    "foot_l",
    "foot_r",
}


class DiagnosticError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    return {"exists": True, "bytes": path.stat().st_size, "sha256": sha256(path)}


def snapshot(root: Path) -> dict[str, dict[str, Any]]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): record(path)
        for path in sorted(root.rglob("*"))
        if path.is_file()
    }


def snapshot_summary(files: dict[str, dict[str, Any]]) -> dict[str, Any]:
    digest = hashlib.sha256()
    total_bytes = 0
    for relative, identity in sorted(files.items()):
        size = int(identity["bytes"])
        total_bytes += size
        digest.update(
            f"{relative}\t{size}\t{identity['sha256']}\n".encode("utf-8")
        )
    return {
        "file_count": len(files),
        "bytes": total_bytes,
        "manifest_sha256": digest.hexdigest().upper(),
    }


def directory_snapshot(root: Path) -> set[str]:
    if not root.is_dir():
        return set()
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_dir()
    }


def write_report(path: Path, state: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f"{path.name}.tmp")
    temporary.write_text(
        json.dumps(state, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    temporary.replace(path)


def strict_json_text(text: str, label: str) -> dict[str, Any]:
    def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise DiagnosticError(f"duplicate JSON key {key!r} in {label}")
            result[key] = value
        return result

    def no_constant(value: str) -> Any:
        raise DiagnosticError(f"non-finite JSON number {value!r} in {label}")

    try:
        value = json.loads(
            text,
            object_pairs_hook=no_duplicates,
            parse_constant=no_constant,
        )
    except json.JSONDecodeError as exc:
        raise DiagnosticError(f"invalid JSON returned by {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise DiagnosticError(f"{label} JSON root is not an object")
    return value


def exact_switch(command_line: str, switch: str) -> bool:
    return bool(
        re.search(rf'(?i)(?:^|\s|"){re.escape(switch)}(?=$|\s|")', command_line)
    )


def command_value(command_line: str, name: str) -> str:
    match = re.search(
        rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))',
        command_line,
    )
    return (match.group(1) or match.group(2)) if match else ""


def validate_command_line(command_line: str, project_root: Path) -> Path:
    required = (DIAGNOSTIC_SWITCH, NO_PORTAL_SWITCH, "-unattended", "-nop4")
    missing = [switch for switch in required if not exact_switch(command_line, switch)]
    if missing:
        raise DiagnosticError(f"required command-line switches absent: {missing}")
    run_tokens = re.findall(r"(?i)(?:^|\s)-run=pythonscript(?=$|\s)", command_line)
    if len(run_tokens) != 1 or re.search(
        r"(?i)(?:^|\s)-executepythonscript(?:=|\s|$)", command_line
    ):
        raise DiagnosticError("diagnostic requires exactly one PythonScript commandlet")
    user_dir_text = command_value(command_line, "UserDir")
    if not user_dir_text:
        raise DiagnosticError("absolute external -UserDir is required")
    user_dir = Path(user_dir_text).resolve()
    try:
        relative = user_dir.relative_to(EXPECTED_USER_ROOT)
    except ValueError as exc:
        raise DiagnosticError(f"UserDir is outside Session8B root: {user_dir}") from exc
    if len(relative.parts) != 1:
        raise DiagnosticError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise DiagnosticError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold():
        raise DiagnosticError("UserDir UUID is not canonical")
    try:
        user_dir.relative_to(project_root)
    except ValueError:
        pass
    else:
        raise DiagnosticError("UserDir must be external to the project")
    return user_dir


def dirty_names() -> tuple[list[str], list[str]]:
    content = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    )
    maps = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )
    return content, maps


def resolve_helper() -> Callable[[], str]:
    utility = unreal.DiscGolfSession8BMetaHumanUtility
    names = (
        "diagnose_session8_b_meta_human_outfit",
        "diagnose_session8b_meta_human_outfit",
    )
    matches = [name for name in names if hasattr(utility, name)]
    if len(matches) != 1:
        raise DiagnosticError(f"expected exactly one diagnostic spelling: {matches}")
    return getattr(utility, matches[0])


def require_exact(payload: dict[str, Any], expected: dict[str, Any], label: str) -> None:
    for key, value in expected.items():
        if payload.get(key) != value:
            raise DiagnosticError(
                f"{label}.{key}={payload.get(key)!r}, expected {value!r}"
            )


def finite_number(value: Any, label: str) -> float:
    if (
        isinstance(value, bool)
        or not isinstance(value, (int, float))
        or not math.isfinite(float(value))
    ):
        raise DiagnosticError(f"{label} is not a finite number: {value!r}")
    return float(value)


def validate_matrix(value: Any, label: str) -> None:
    if not isinstance(value, list) or len(value) != 4:
        raise DiagnosticError(f"{label} is not a 4x4 matrix")
    for row_index, row in enumerate(value):
        if not isinstance(row, list) or len(row) != 4:
            raise DiagnosticError(f"{label}[{row_index}] is not a four-value row")
        for column_index, item in enumerate(row):
            finite_number(item, f"{label}[{row_index}][{column_index}]")


def validate_bounds(value: Any, label: str) -> None:
    if not isinstance(value, dict) or value.get("valid") is not True:
        raise DiagnosticError(f"{label} is not a valid bounds object")
    for field in ("minimum_cm", "maximum_cm", "extent_cm"):
        vector = value.get(field)
        if not isinstance(vector, dict) or set(vector) != {"x", "y", "z"}:
            raise DiagnosticError(f"{label}.{field} is not an xyz vector")
        for axis, item in vector.items():
            finite_number(item, f"{label}.{field}.{axis}")


def validate_helper(payload: dict[str, Any]) -> None:
    require_exact(
        payload,
        {
            "schema_version": 2,
            "session": "8B",
            "operation": "OUTFIT_DIAGNOSTIC",
            "status": "PASS",
            "disk_mutation": "NONE",
            "writes": [],
            "attempted_writes": [],
            "errors": [],
            "generated_blueprint": GENERATED_BLUEPRINT,
            "outfit_mesh": OUTFIT_OBJECT,
            "diagnostic_schema": (
                "DiscGolfTour.Session8BMetaHumanOutfitDiagnostic.v1"
            ),
            "rendered_lod_observation_scope": (
                "ASSET_RENDER_DATA_ONLY_NO_LIVE_RENDER_COMPONENT"
            ),
            "actual_rendered_lod": -1,
            "asset_save_attempted": False,
            "body_component_identity_relative": True,
            "outfit_component_identity_relative": True,
            "outfit_component_count": 1,
            "all_anchor_comparisons_present": True,
            "all_bone_probe_completed": True,
            "all_skin_bones_mapped_to_body": True,
            "all_synthetic_ref_to_local_matrices_finite": True,
            "all_lods_probe_completed": True,
            "dirty_package_baseline_preserved": True,
            "diagnostic_completed": True,
        },
        "helper",
    )
    bone_count = payload.get("outfit_raw_bone_count")
    comparisons = payload.get("all_bone_comparisons")
    if (
        isinstance(bone_count, bool)
        or not isinstance(bone_count, int)
        or bone_count <= 0
        or payload.get("all_bone_comparison_count") != bone_count
        or not isinstance(comparisons, list)
        or len(comparisons) != bone_count
    ):
        raise DiagnosticError("all-bone comparison cardinality differs")
    for expected_index, bone in enumerate(comparisons):
        if not isinstance(bone, dict):
            raise DiagnosticError(f"bone comparison {expected_index} is not an object")
        if (
            bone.get("outfit_bone_index") != expected_index
            or not isinstance(bone.get("bone_name"), str)
            or not bone["bone_name"]
            or isinstance(bone.get("body_bone_index"), bool)
            or not isinstance(bone.get("body_bone_index"), int)
            or not isinstance(bone.get("name_mapped"), bool)
            or bone.get("outfit_inverse_bind_finite") is not True
        ):
            raise DiagnosticError(f"bone comparison {expected_index} is incomplete")
        validate_matrix(
            bone.get("outfit_inverse_bind_matrix"),
            f"bone[{expected_index}].outfit_inverse_bind_matrix",
        )
        if bone["name_mapped"]:
            if (
                bone["body_bone_index"] < 0
                or bone.get("inverse_bind_finite") is not True
            ):
                raise DiagnosticError(
                    f"mapped bone comparison {expected_index} is incomplete"
                )
            validate_matrix(
                bone.get("body_inverse_bind_matrix"),
                f"bone[{expected_index}].body_inverse_bind_matrix",
            )
            for prefix in ("local_ref_delta", "component_ref_delta"):
                for suffix in (
                    "translation_cm",
                    "rotation_degrees",
                    "scale_maximum",
                ):
                    finite_number(
                        bone.get(f"{prefix}_{suffix}"),
                        f"bone[{expected_index}].{prefix}_{suffix}",
                    )
            finite_number(
                bone.get("inverse_bind_matrix_maximum_absolute_delta"),
                f"bone[{expected_index}].inverse_bind_delta",
            )
        elif bone["body_bone_index"] != -1:
            raise DiagnosticError(
                f"unmapped bone comparison {expected_index} has a body index"
            )
    anchors = payload.get("required_anchor_comparisons")
    if not isinstance(anchors, dict) or set(anchors) != ANCHORS:
        raise DiagnosticError(f"required anchor set differs: {anchors!r}")

    lod_count = payload.get("outfit_lod_count")
    lods = payload.get("outfit_render_lods")
    if (
        isinstance(lod_count, bool)
        or not isinstance(lod_count, int)
        or lod_count <= 0
        or not isinstance(lods, list)
        or len(lods) != lod_count
    ):
        raise DiagnosticError("Outfit render-LOD cardinality differs")
    for field in (
        "render_data_current_first_lod",
        "render_data_pending_first_lod",
        "render_data_inlined_lod_count",
        "render_data_non_optional_lod_count",
        "component_compute_min_lod",
        "asset_min_lod",
    ):
        finite_number(payload.get(field), f"helper.{field}")
    for lod_index, lod in enumerate(lods):
        if not isinstance(lod, dict):
            raise DiagnosticError(f"LOD {lod_index} is not an object")
        require_exact(
            lod,
            {
                "lod_index": lod_index,
                "data_ready": True,
                "position_cpu_access_requested": True,
                "position_data_available": True,
                "position_cpu_accessible": True,
                "skin_weight_cpu_access_requested": True,
                "skin_weight_data_available": True,
                "skin_weight_lookup_available": True,
                "skin_weight_cpu_accessible": True,
                "skin_weight_vertex_count_matches": True,
                "maximum_bone_influence_count_valid": True,
                "leader_state_safe_for_cpu_skinning": True,
                "section_bone_maps_valid": True,
                "every_vertex_covered_exactly_once": True,
                "uncovered_vertex_count": 0,
                "overlapping_vertex_count": 0,
                "all_influence_mappings_valid": True,
                "all_skin_weight_sums_normalized": True,
                "invalid_influence_bone_map_index_count": 0,
                "invalid_influence_mesh_bone_index_count": 0,
                "invalid_influence_body_mapping_count": 0,
                "invalid_influence_count_vertex_count": 0,
                "invalid_influence_span_vertex_count": 0,
                "non_normalized_weight_vertex_count": 0,
                "probe_ready": True,
                "skinned_position_counts_match": True,
                "non_finite_self_skinned_vertex_count": 0,
                "non_finite_body_driven_vertex_count": 0,
            },
            f"LOD {lod_index}",
        )
        vertex_count = lod.get("vertex_count")
        maximum_influences = lod.get("maximum_bone_influences")
        sections = lod.get("sections")
        if (
            isinstance(vertex_count, bool)
            or not isinstance(vertex_count, int)
            or vertex_count <= 0
            or isinstance(maximum_influences, bool)
            or not isinstance(maximum_influences, int)
            or maximum_influences <= 0
            or not isinstance(sections, list)
            or not sections
            or lod.get("section_count") != len(sections)
        ):
            raise DiagnosticError(f"LOD {lod_index} geometry metadata is invalid")
        validate_bounds(lod.get("self_skinned_bounds"), f"LOD {lod_index}.self")
        validate_bounds(lod.get("body_driven_bounds"), f"LOD {lod_index}.body")
        for section_index, section in enumerate(sections):
            if not isinstance(section, dict):
                raise DiagnosticError(
                    f"LOD {lod_index} section {section_index} is not an object"
                )
            require_exact(
                section,
                {
                    "section_index": section_index,
                    "vertex_range_valid": True,
                    "bone_map_valid": True,
                    "invalid_bone_map_entry_count": 0,
                    "invalid_influence_bone_map_index_count": 0,
                    "invalid_influence_mesh_bone_index_count": 0,
                    "invalid_influence_body_mapping_count": 0,
                    "invalid_influence_count_vertex_count": 0,
                    "invalid_influence_span_vertex_count": 0,
                    "non_normalized_weight_vertex_count": 0,
                },
                f"LOD {lod_index} section {section_index}",
            )
            section_vertices = section.get("vertex_count")
            if (
                isinstance(section_vertices, bool)
                or not isinstance(section_vertices, int)
                or section_vertices <= 0
                or section.get("checked_skin_weight_vertex_count")
                != section_vertices
                or section.get("checked_influence_slot_count")
                != section_vertices * maximum_influences
            ):
                raise DiagnosticError(
                    f"LOD {lod_index} section {section_index} weight coverage differs"
                )
            tolerance = finite_number(
                lod.get("normalized_weight_tolerance"),
                f"LOD {lod_index}.normalized_weight_tolerance",
            )
            for field in (
                "minimum_normalized_weight_sum",
                "maximum_normalized_weight_sum",
            ):
                weight_sum = finite_number(
                    section.get(field),
                    f"LOD {lod_index} section {section_index}.{field}",
                )
                if abs(weight_sum - 1.0) > tolerance:
                    raise DiagnosticError(
                        f"LOD {lod_index} section {section_index}.{field} "
                        f"is not normalized: {weight_sum}"
                    )
            if section.get("enabled") is True and not section.get("material_path"):
                raise DiagnosticError(
                    f"LOD {lod_index} enabled section {section_index} has no material"
                )
            bone_map = section.get("bone_map")
            if (
                not isinstance(bone_map, list)
                or not bone_map
                or section.get("bone_map_count") != len(bone_map)
                or any(
                    not isinstance(entry, dict)
                    or isinstance(entry.get("body_bone_index"), bool)
                    or not isinstance(entry.get("body_bone_index"), int)
                    or entry["body_bone_index"] < 0
                    for entry in bone_map
                )
            ):
                raise DiagnosticError(
                    f"LOD {lod_index} section {section_index} BoneMap differs"
                )
            validate_bounds(
                section.get("self_skinned_bounds"),
                f"LOD {lod_index} section {section_index}.self",
            )
            validate_bounds(
                section.get("body_driven_bounds"),
                f"LOD {lod_index} section {section_index}.body",
            )


def canonical_records(project_root: Path) -> dict[str, dict[str, Any]]:
    return {
        relative: record(project_root / relative)
        for relative in CANONICAL_EXPECTED
    }


def canonical_sidecars(project_root: Path) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for relative in (WRAPPER_RELATIVE, RETARGET_RELATIVE, PROFILE_RELATIVE):
        base = (project_root / relative).with_suffix("")
        for suffix in PACKAGE_SUFFIXES[1:]:
            path = Path(f"{base}{suffix}")
            result[path.relative_to(project_root).as_posix()] = record(path)
    return result


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    project_root = Path(unreal.Paths.project_dir()).resolve()
    content_root = Path(unreal.Paths.project_content_dir()).resolve()
    invalid_report = (
        Path(tempfile.gettempdir()) / "Session8BMetaHumanOutfitDiagnostic.invalid.json"
    )
    report_path = invalid_report
    state: dict[str, Any] = {
        "schema": SCHEMA,
        "generated_utc": utc_now(),
        "status": "FAIL_NOT_STARTED",
        "phase": "COMMAND_LINE",
        "disk_mutation": "NONE",
        "project_asset_mutation": "NONE",
        "writes": [],
        "attempted_writes": [],
        "asset_save_call_count": 0,
        "errors": [],
        "generated_blueprint": GENERATED_BLUEPRINT,
        "outfit_object": OUTFIT_OBJECT,
    }
    user_dir: Path | None = None
    content_before: dict[str, dict[str, Any]] | None = None
    directories_before: set[str] | None = None
    saves_before: dict[str, dict[str, Any]] | None = None
    save_directories_before: set[str] | None = None
    sidecars_before: dict[str, dict[str, Any]] | None = None
    production = project_root / PRODUCTION_SAVE_RELATIVE
    saves_root = production.parent
    try:
        if project_root != EXPECTED_PROJECT_ROOT:
            raise DiagnosticError(f"unexpected project root: {project_root}")
        user_dir = validate_command_line(command_line, project_root)
        candidate_report = user_dir / REPORT_RELATIVE
        if candidate_report.exists():
            raise DiagnosticError(f"fresh UUID already contains report: {candidate_report}")
        if list(user_dir.rglob("*.sav")):
            raise DiagnosticError("fresh UUID already contains a save file")
        report_path = candidate_report
        state["external_user_dir"] = str(user_dir)
        state["report_path"] = str(report_path)

        state["phase"] = "IMMUTABLE_BASELINES"
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise DiagnosticError(
                f"commandlet began dirty: content={dirty_content}, maps={dirty_maps}"
            )
        content_before = snapshot(content_root)
        content_summary_before = snapshot_summary(content_before)
        if content_summary_before != CONTENT_EXPECTED:
            raise DiagnosticError(
                f"Content baseline changed: {content_summary_before}"
            )
        directories_before = directory_snapshot(content_root)
        if len(directories_before) != CONTENT_DIRECTORY_COUNT:
            raise DiagnosticError(
                f"Content directory count={len(directories_before)}, "
                f"expected {CONTENT_DIRECTORY_COUNT}"
            )
        canonical_before = canonical_records(project_root)
        if canonical_before != CANONICAL_EXPECTED:
            raise DiagnosticError(f"canonical identities changed: {canonical_before}")
        sidecars_before = canonical_sidecars(project_root)
        if any(identity["exists"] for identity in sidecars_before.values()):
            raise DiagnosticError("a canonical package sidecar exists")
        saves_before = snapshot(saves_root)
        save_directories_before = directory_snapshot(saves_root)
        if saves_before != {production.name: ACCEPTED_SAVE}:
            raise DiagnosticError(f"project SaveGames baseline differs: {saves_before}")
        if save_directories_before:
            raise DiagnosticError("project SaveGames contains unexpected directories")
        if record(ACCEPTED_BACKUP) != ACCEPTED_SAVE:
            raise DiagnosticError("accepted backup identity changed")
        if production.read_bytes() != ACCEPTED_BACKUP.read_bytes():
            raise DiagnosticError("production save and accepted backup bytes differ")
        state["before"] = {
            "content": content_summary_before,
            "content_directory_count": len(directories_before),
            "canonical_assets": canonical_before,
            "canonical_sidecars": sidecars_before,
            "project_savegames": saves_before,
            "accepted_backup": record(ACCEPTED_BACKUP),
            "dirty_content_packages": dirty_content,
            "dirty_map_packages": dirty_maps,
        }

        state["phase"] = "READ_ONLY_CPP_DIAGNOSTIC"
        helper_payload = strict_json_text(
            resolve_helper()(), "C++ Outfit diagnostic helper"
        )
        state["helper_result"] = helper_payload
        validate_helper(helper_payload)

        state["phase"] = "PROVE_NO_PROJECT_MUTATION"
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise DiagnosticError(
                f"diagnostic changed dirty packages: {dirty_content}, {dirty_maps}"
            )
        content_after = snapshot(content_root)
        directories_after = directory_snapshot(content_root)
        canonical_after = canonical_records(project_root)
        sidecars_after = canonical_sidecars(project_root)
        saves_after = snapshot(saves_root)
        save_directories_after = directory_snapshot(saves_root)
        external_saves = sorted(
            path.relative_to(user_dir).as_posix() for path in user_dir.rglob("*.sav")
        )
        if content_after != content_before:
            raise DiagnosticError("Content file identities changed during diagnostic")
        if snapshot_summary(content_after) != CONTENT_EXPECTED:
            raise DiagnosticError("post-diagnostic Content manifest differs")
        if directories_after != directories_before:
            raise DiagnosticError("Content directory topology changed")
        if canonical_after != CANONICAL_EXPECTED:
            raise DiagnosticError("canonical asset identities changed")
        if sidecars_after != sidecars_before:
            raise DiagnosticError("canonical sidecar identities changed")
        if saves_after != saves_before or save_directories_after != save_directories_before:
            raise DiagnosticError("project SaveGames changed")
        if external_saves:
            raise DiagnosticError(f"external save files appeared: {external_saves}")
        if record(ACCEPTED_BACKUP) != ACCEPTED_SAVE:
            raise DiagnosticError("accepted backup identity changed after diagnostic")
        if production.read_bytes() != ACCEPTED_BACKUP.read_bytes():
            raise DiagnosticError("accepted production save bytes changed")

        state.update(
            {
                "status": "PASS_NO_DISK_MUTATION_DIAGNOSTIC_COMPLETE",
                "phase": "COMPLETE",
                "disk_mutation": "EXTERNAL_ATOMIC_REPORT_ONLY",
                "project_asset_mutation": "NONE",
                "content_before": content_summary_before,
                "content_after": snapshot_summary(content_after),
                "content_unchanged": True,
                "content_directories_unchanged": True,
                "canonical_assets_after": canonical_after,
                "canonical_assets_unchanged": True,
                "canonical_sidecars_unchanged": True,
                "project_savegames_after": saves_after,
                "project_savegames_unchanged": True,
                "accepted_backup_unchanged": True,
                "production_save_unchanged": True,
                "external_save_files": external_saves,
                "dirty_content_packages_after": dirty_content,
                "dirty_map_packages_after": dirty_maps,
                "completed_utc": utc_now(),
            }
        )
        write_report(report_path, state)
    except Exception as exc:
        state["errors"].append(f"{type(exc).__name__}: {exc}")
        state["status"] = "FAIL_NO_PROJECT_MUTATION_CLAIMED"
        state["failed_utc"] = utc_now()
        try:
            if (
                content_before is not None
                and directories_before is not None
                and saves_before is not None
                and save_directories_before is not None
                and sidecars_before is not None
            ):
                state["failure_boundary_unchanged"] = (
                    snapshot(content_root) == content_before
                    and directory_snapshot(content_root) == directories_before
                    and canonical_records(project_root) == CANONICAL_EXPECTED
                    and canonical_sidecars(project_root) == sidecars_before
                    and snapshot(saves_root) == saves_before
                    and directory_snapshot(saves_root) == save_directories_before
                    and record(ACCEPTED_BACKUP) == ACCEPTED_SAVE
                    and production.read_bytes() == ACCEPTED_BACKUP.read_bytes()
                    and (user_dir is None or not list(user_dir.rglob("*.sav")))
                )
            write_report(report_path, state)
        except Exception as report_error:
            unreal.log_error(
                "DG_SESSION8B_OUTFIT_DIAGNOSTIC report write failed: "
                f"{report_error}"
            )
        unreal.log_error(
            "DG_SESSION8B_OUTFIT_DIAGNOSTIC: "
            f"{state['status']} phase={state['phase']} report={report_path}: {exc}"
        )
        raise

    unreal.log(
        "DG_SESSION8B_OUTFIT_DIAGNOSTIC: "
        f"{state['status']} report={report_path}"
    )


if __name__ == "__main__":
    main()
