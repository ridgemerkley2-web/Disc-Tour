"""Read-only Unreal validation for the authored production-motion candidates."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import math
import re
import sys
import uuid
from pathlib import Path
from typing import Any

import unreal


VALIDATE_SWITCH = "-DGValidateProductionProceduralMotion"
USER_ROOT = Path(r"C:\DGTour_TestRuns\Session19ProductionMotionValidation")
REPORT_RELATIVE = Path("Saved/ProductionMotion/ValidationReport.json")
SAVE_RELATIVE = Path("Saved/SaveGames/DiscGolfTour_Profile_0.sav")
TARGET_ROOT = Path("Content/DiscGolf/Animation/ProductionMotion")
PROTECTED_ROOTS = (
    Path("Content/DiscGolf/Animation/Throws"),
    Path("Content/DiscGolf/Animation/Mocap"),
)
PROTECTED_V6_FILES = {
    "Content/DiscGolf/Animation/ProductionMotion/Drive/A_DG_RHBH_Drive_Procedural_v006.uasset": {
        "bytes": 479441,
        "sha256": "319CBA9E20966A5C6EA82816EC1B65F2E9400611B3E84CCA1B7D276BADD856E4",
    },
    "Content/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v006.uasset": {
        "bytes": 22133,
        "sha256": "9220D00C6D82325603065B1F8BAE70F29520190777446FF351A6D3C5C472B759",
    },
    "Content/DiscGolf/Animation/ProductionMotion/Approach/A_DG_RHBH_Approach_Procedural_v006.uasset": {
        "bytes": 381283,
        "sha256": "D96AD9020A5ECBB594A3D0AF9BF3060C3DBBBA66B425EC076D0E3BF7FDBC8211",
    },
    "Content/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v006.uasset": {
        "bytes": 22163,
        "sha256": "F10D9B1572AFC4AC5613EBF179299EDC41915F2605A36E639B2863298713D985",
    },
    "Content/DiscGolf/Animation/ProductionMotion/Putt/A_DG_RHBH_Putt_Procedural_v006.uasset": {
        "bytes": 276239,
        "sha256": "D7F76D263D18EA67641292C70F337AA7C5907FCA001ACECE650688D1F189F5A3",
    },
    "Content/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v006.uasset": {
        "bytes": 22123,
        "sha256": "10FA1BD79CA5CCFEF0301336754642B8E19574E55C59F61CB3DF0C7D431FA8FA",
    },
    "Content/DiscGolf/Animation/ProductionMotion/DA_DG_ProductionMotionLibrary_v006.uasset": {
        "bytes": 5136,
        "sha256": "76C004ED999922253710B948F624E9CC72FC70DF25A85B7136ADB80819822622",
    },
    "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v006.json": {
        "bytes": 44023,
        "sha256": "A6BC2C3BA4B37718C4CD64004DD3123708939EA43519CB61CD8006EC5AE06DA6",
    },
}
V7_TARGET_OBJECTS = (
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/A_DG_RHBH_Drive_Procedural_v007.A_DG_RHBH_Drive_Procedural_v007",
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v007.AM_DG_RHBH_Drive_Procedural_v007",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/A_DG_RHBH_Approach_Procedural_v007.A_DG_RHBH_Approach_Procedural_v007",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v007.AM_DG_RHBH_Approach_Procedural_v007",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/A_DG_RHBH_Putt_Procedural_v007.A_DG_RHBH_Putt_Procedural_v007",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v007.AM_DG_RHBH_Putt_Procedural_v007",
    "/Game/DiscGolf/Animation/ProductionMotion/DA_DG_ProductionMotionLibrary_v007.DA_DG_ProductionMotionLibrary_v007",
)
V7_AUTHORING_EVIDENCE = Path(
    "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v007.json"
)
VERSION_SPECS = {
    "v1": {
        "asset_revision": "v001",
        "presentation_root_trajectory_enabled": False,
    },
    "v2": {
        "asset_revision": "v002",
        "presentation_root_trajectory_enabled": True,
    },
    "v3": {
        "asset_revision": "v003",
        "presentation_root_trajectory_enabled": True,
    },
    "v4": {
        "asset_revision": "v004",
        "presentation_root_trajectory_enabled": True,
    },
    "v5": {
        "asset_revision": "v005",
        "presentation_root_trajectory_enabled": True,
    },
    "v6": {
        "asset_revision": "v006",
        "presentation_root_trajectory_enabled": True,
    },
    "v7": {
        "asset_revision": "v007",
        "presentation_root_trajectory_enabled": True,
    },
}
GRANULAR_MOTION = {"v3", "v4", "v5", "v6", "v7"}
V7_NATIVE_DRIVE_ARM_THRESHOLDS = {
    "support_maximum_reach_ratio": 0.75,
    "support_minimum_elbow_angle_degrees": 65.0,
    "support_maximum_elbow_angle_degrees": 105.0,
    "power_pocket_minimum_reach_ratio": 0.70,
    "power_pocket_maximum_reach_ratio": 0.82,
    "power_pocket_minimum_elbow_angle_degrees": 85.0,
    "power_pocket_maximum_elbow_angle_degrees": 105.0,
    "power_pocket_maximum_absolute_torso_lateral_cm": 46.0,
    "power_pocket_minimum_torso_forward_cm": -45.0,
    "power_pocket_maximum_torso_forward_cm": -20.0,
    "power_pocket_minimum_torso_vertical_cm": 5.0,
    "power_pocket_maximum_torso_vertical_cm": 25.0,
    "release_minimum_reach_ratio": 0.95,
    "release_maximum_reach_ratio": 0.99,
    "release_minimum_elbow_angle_degrees": 145.0,
    "release_maximum_elbow_angle_degrees": 165.0,
    "followthrough_minimum_reach_ratio": 0.95,
    "followthrough_maximum_reach_ratio": 1.0,
    "followthrough_minimum_elbow_angle_degrees": 155.0,
    "followthrough_maximum_elbow_angle_degrees": 175.0,
    "recovery_maximum_reach_ratio": 0.75,
    "recovery_minimum_elbow_angle_degrees": 75.0,
    "recovery_maximum_elbow_angle_degrees": 110.0,
    "recovery_maximum_absolute_torso_lateral_cm": 35.0,
    "recovery_maximum_torso_vertical_cm": -15.0,
}
# Native data-model rotations are FQuat4f. The derived v007 pocket maximum
# and release minimum elbow angles deliberately land on their semantic bounds,
# so accept only one-millidegree representation noise at those two boundaries.
V7_NATIVE_ARM_BOUNDARY_TOLERANCE_DEGREES = 0.001


def v7_native_boundary_elbow_checks(
    drive: dict[str, Any],
) -> tuple[bool, bool]:
    thresholds = V7_NATIVE_DRIVE_ARM_THRESHOLDS
    tolerance = V7_NATIVE_ARM_BOUNDARY_TOLERANCE_DEGREES
    return (
        float(drive["v7_power_pocket_maximum_elbow_angle_degrees"])
            <= thresholds["power_pocket_maximum_elbow_angle_degrees"]
                + tolerance,
        float(drive["v7_release_elbow_angle_degrees"])
            >= thresholds["release_minimum_elbow_angle_degrees"]
                - tolerance,
    )


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def snapshot(root: Path) -> dict[str, dict[str, Any]]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): {
            "bytes": path.stat().st_size,
            "sha256": sha256(path),
        }
        for path in sorted(item for item in root.rglob("*") if item.is_file())
    }


def require_exact_files(
    project_root: Path,
    expected: dict[str, dict[str, Any]],
    label: str,
) -> dict[str, dict[str, Any]]:
    observed: dict[str, dict[str, Any]] = {}
    for relative, identity in expected.items():
        path = project_root / relative
        if not path.is_file():
            raise RuntimeError(f"{label} file is absent: {relative}")
        actual = {"bytes": path.stat().st_size, "sha256": sha256(path)}
        if actual != identity:
            raise RuntimeError(f"{label} identity differs: {relative}")
        observed[relative] = actual
    return observed


def object_file(object_path: str) -> Path:
    package = object_path.split(".", 1)[0].removeprefix("/Game/")
    return Path("Content") / f"{package}.uasset"


def require_v7_authoring_evidence(project_root: Path) -> dict[str, Any]:
    evidence_path = project_root / V7_AUTHORING_EVIDENCE
    if not evidence_path.is_file():
        raise RuntimeError("immutable v007 authoring evidence is absent")
    evidence = strict_json(evidence_path.read_text(encoding="utf-8"))
    for key, expected in {
        "schema": "DiscGolfTour.Session19ProductionMotionCandidateAuthoringEvidence.v7",
        "schema_version": 7,
        "recipe_version": "v7",
        "asset_revision": "v007",
        "status": "PASS_AUTHORED_PROCEDURAL_CANDIDATES_HUMAN_REVIEW_REQUIRED",
        "human_animation_approval": False,
        "human_disc_contact_approval": False,
        "product_art_approval": False,
        "release_ready": False,
    }.items():
        if evidence.get(key) != expected:
            raise RuntimeError(f"v007 authoring evidence field differs: {key}")
    recipe_path = project_root / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v7.json"
    recipe_identity = {
        "exists": True,
        "bytes": recipe_path.stat().st_size,
        "sha256": sha256(recipe_path),
        "path": "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v7.json",
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V7",
        "recipe_version": "v7",
        "asset_revision": "v007",
    }
    if evidence.get("recipe") != recipe_identity:
        raise RuntimeError("v007 authoring evidence recipe identity differs")
    assets = evidence.get("authored_assets")
    if not isinstance(assets, list) or len(assets) != len(V7_TARGET_OBJECTS):
        raise RuntimeError("v007 authoring evidence asset set differs")
    by_object = {
        item.get("object_path"): item for item in assets if isinstance(item, dict)
    }
    if set(by_object) != set(V7_TARGET_OBJECTS) or len(by_object) != len(assets):
        raise RuntimeError("v007 authoring evidence object identity differs")
    verified_assets: dict[str, dict[str, Any]] = {}
    for object_path in V7_TARGET_OBJECTS:
        relative = object_file(object_path)
        record = by_object[object_path]
        path = project_root / relative
        actual = {
            "exists": path.is_file(),
            "bytes": path.stat().st_size if path.is_file() else 0,
            "sha256": sha256(path) if path.is_file() else "",
        }
        expected_record = {
            "object_path": object_path,
            "package": object_path.split(".", 1)[0],
            "file": relative.as_posix(),
            **actual,
        }
        if record != expected_record or not actual["exists"]:
            raise RuntimeError(
                f"v007 authoring evidence asset identity differs: {object_path}"
            )
        verified_assets[object_path] = {
            "file": relative.as_posix(),
            "bytes": actual["bytes"],
            "sha256": actual["sha256"],
        }
    native = evidence.get("native_validation")
    if (
        not isinstance(native, dict)
        or not str(native.get("status", "")).startswith("PASS")
        or native.get("schema")
            != "DiscGolfTour.Session19ProductionMotionNativeReport.v7"
        or native.get("recipe_version") != "v7"
        or native.get("asset_revision") != "v007"
        or native.get("release_ready") is not False
    ):
        raise RuntimeError("v007 authoring evidence native validation differs")
    return {
        "path": V7_AUTHORING_EVIDENCE.as_posix(),
        "bytes": evidence_path.stat().st_size,
        "sha256": sha256(evidence_path),
        "status": evidence["status"],
        "verified_assets": verified_assets,
    }


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


def strict_json(text: str) -> dict[str, Any]:
    def pairs(values: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in values:
            if key in result:
                raise RuntimeError(f"duplicate JSON key: {key}")
            result[key] = value
        return result

    value = json.loads(text, object_pairs_hook=pairs)
    if not isinstance(value, dict):
        raise RuntimeError("native validation JSON root is not an object")
    return value


def threshold_contract_matches(
    actual: Any,
    expected: dict[str, float | bool],
) -> bool:
    """Compare reflected UE float thresholds without demanding bit identity."""
    if not isinstance(actual, dict) or set(actual) != set(expected):
        return False
    for key, expected_value in expected.items():
        actual_value = actual.get(key)
        if isinstance(expected_value, bool):
            if actual_value is not expected_value:
                return False
        elif (
            not isinstance(actual_value, (int, float))
            or isinstance(actual_value, bool)
            or not math.isfinite(float(actual_value))
            or not math.isclose(
                float(actual_value), float(expected_value), abs_tol=1e-5
            )
        ):
            return False
    return True


def validate_v7_native_arm_gates(native: dict[str, Any]) -> None:
    families = native.get("families")
    if not isinstance(families, list):
        raise RuntimeError("native v7 bilateral arm evidence is absent")
    by_name = {
        family.get("family"): family
        for family in families
        if isinstance(family, dict)
    }
    if set(by_name) != {"Drive", "Approach", "Putt"} or len(by_name) != 3:
        raise RuntimeError("native v7 family identity differs")
    drive = by_name["Drive"]
    if (
        drive.get("arm_spatial_gate_applied") is not True
        or drive.get("arm_spatial_gate_passed") is not True
        or drive.get("v7_drive_arm_gate_applied") is not True
        or drive.get("arm_spatial_gate_contract")
            != "V7_DRIVE_BILATERAL_POWER_POCKET_RELEASE_RECOVERY"
        or drive.get("v7_support_arm_sample_count") != 4
        or drive.get("v7_power_pocket_sample_count") != 3
        or not threshold_contract_matches(
            drive.get("v7_drive_arm_gate_thresholds"),
            V7_NATIVE_DRIVE_ARM_THRESHOLDS,
        )
    ):
        raise RuntimeError("native v7 Drive bilateral arm gate identity differs")
    metric_fields = (
        "v7_support_arm_maximum_reach_ratio",
        "v7_support_arm_minimum_elbow_angle_degrees",
        "v7_support_arm_maximum_elbow_angle_degrees",
        "v7_power_pocket_minimum_reach_ratio",
        "v7_power_pocket_maximum_reach_ratio",
        "v7_power_pocket_minimum_elbow_angle_degrees",
        "v7_power_pocket_maximum_elbow_angle_degrees",
        "v7_power_pocket_maximum_absolute_torso_lateral_cm",
        "v7_power_pocket_minimum_torso_forward_cm",
        "v7_power_pocket_maximum_torso_forward_cm",
        "v7_power_pocket_minimum_torso_vertical_cm",
        "v7_power_pocket_maximum_torso_vertical_cm",
        "v7_release_reach_ratio", "v7_release_elbow_angle_degrees",
        "v7_followthrough_reach_ratio",
        "v7_followthrough_elbow_angle_degrees",
        "v7_recovery_reach_ratio", "v7_recovery_elbow_angle_degrees",
        "v7_recovery_absolute_torso_lateral_cm",
        "v7_recovery_torso_vertical_cm",
    )
    if any(
        not isinstance(drive.get(field), (int, float))
        or isinstance(drive.get(field), bool)
        or not math.isfinite(float(drive[field]))
        for field in metric_fields
    ):
        raise RuntimeError("native v7 Drive bilateral arm metrics are non-finite")
    t = V7_NATIVE_DRIVE_ARM_THRESHOLDS
    pocket_maximum_elbow_passed, release_minimum_elbow_passed = (
        v7_native_boundary_elbow_checks(drive)
    )
    if (
        float(drive["v7_support_arm_maximum_reach_ratio"])
            > t["support_maximum_reach_ratio"]
        or float(drive["v7_support_arm_minimum_elbow_angle_degrees"])
            < t["support_minimum_elbow_angle_degrees"]
        or float(drive["v7_support_arm_maximum_elbow_angle_degrees"])
            > t["support_maximum_elbow_angle_degrees"]
        or float(drive["v7_power_pocket_minimum_reach_ratio"])
            < t["power_pocket_minimum_reach_ratio"]
        or float(drive["v7_power_pocket_maximum_reach_ratio"])
            > t["power_pocket_maximum_reach_ratio"]
        or float(drive["v7_power_pocket_minimum_elbow_angle_degrees"])
            < t["power_pocket_minimum_elbow_angle_degrees"]
        or not pocket_maximum_elbow_passed
        or float(drive["v7_power_pocket_maximum_absolute_torso_lateral_cm"])
            > t["power_pocket_maximum_absolute_torso_lateral_cm"]
        or float(drive["v7_power_pocket_minimum_torso_forward_cm"])
            < t["power_pocket_minimum_torso_forward_cm"]
        or float(drive["v7_power_pocket_maximum_torso_forward_cm"])
            > t["power_pocket_maximum_torso_forward_cm"]
        or float(drive["v7_power_pocket_minimum_torso_vertical_cm"])
            < t["power_pocket_minimum_torso_vertical_cm"]
        or float(drive["v7_power_pocket_maximum_torso_vertical_cm"])
            > t["power_pocket_maximum_torso_vertical_cm"]
        or not t["release_minimum_reach_ratio"]
            <= float(drive["v7_release_reach_ratio"])
            <= t["release_maximum_reach_ratio"]
        or not release_minimum_elbow_passed
        or float(drive["v7_release_elbow_angle_degrees"])
            > t["release_maximum_elbow_angle_degrees"]
        or not t["followthrough_minimum_reach_ratio"]
            <= float(drive["v7_followthrough_reach_ratio"])
            <= t["followthrough_maximum_reach_ratio"]
        or not t["followthrough_minimum_elbow_angle_degrees"]
            <= float(drive["v7_followthrough_elbow_angle_degrees"])
            <= t["followthrough_maximum_elbow_angle_degrees"]
        or float(drive["v7_recovery_reach_ratio"])
            > t["recovery_maximum_reach_ratio"]
        or not t["recovery_minimum_elbow_angle_degrees"]
            <= float(drive["v7_recovery_elbow_angle_degrees"])
            <= t["recovery_maximum_elbow_angle_degrees"]
        or float(drive["v7_recovery_absolute_torso_lateral_cm"])
            > t["recovery_maximum_absolute_torso_lateral_cm"]
        or float(drive["v7_recovery_torso_vertical_cm"])
            > t["recovery_maximum_torso_vertical_cm"]
    ):
        raise RuntimeError(
            "native v7 Drive bilateral arm threshold evidence differs; "
            f"pocket_max_elbow="
            f"{float(drive['v7_power_pocket_maximum_elbow_angle_degrees']):.9f} "
            f"<= {t['power_pocket_maximum_elbow_angle_degrees']:.3f}+"
            f"{V7_NATIVE_ARM_BOUNDARY_TOLERANCE_DEGREES:.3f}: "
            f"{pocket_maximum_elbow_passed}; release_min_elbow="
            f"{float(drive['v7_release_elbow_angle_degrees']):.9f} "
            f">= {t['release_minimum_elbow_angle_degrees']:.3f}-"
            f"{V7_NATIVE_ARM_BOUNDARY_TOLERANCE_DEGREES:.3f}: "
            f"{release_minimum_elbow_passed}"
        )
    legacy_thresholds = {
        "minimum_elbow_angle_degrees": 110.0,
        "minimum_arm_extension_ratio": 0.82,
        "reachback_throwing_side_directional_dominance": True,
        "followthrough_throwing_side_directional_dominance": True,
        "minimum_reachback_radial_reach_cm": 64.0,
        "minimum_hand_vertical_cm": 0.0,
        "minimum_release_follow_lateral_clearance_cm": 60.0,
        "maximum_direction_frame_delta_degrees": 8.0,
    }
    for name in ("Approach", "Putt"):
        family = by_name[name]
        if (
            family.get("arm_spatial_gate_applied") is not True
            or family.get("arm_spatial_gate_passed") is not True
            or family.get("arm_spatial_gate_contract")
                != "V6_THROWING_ARM_NAMED_PHASE_SPATIAL"
            or not threshold_contract_matches(
                family.get("arm_spatial_gate_thresholds"), legacy_thresholds
            )
        ):
            raise RuntimeError(
                f"native v7 {name} immutable v6 spatial contract differs"
            )


def validate_v7_source_lane(project_root: Path) -> dict[str, Any]:
    scripts_dir = project_root / "Scripts"
    validator_path = scripts_dir / "validate_dg_session19_production_motion_v7.py"
    if not validator_path.is_file():
        raise RuntimeError("v7 deterministic source validator is absent")
    scripts_text = str(scripts_dir)
    if scripts_text not in sys.path:
        sys.path.insert(0, scripts_text)
    module_spec = importlib.util.spec_from_file_location(
        "dg_session19_v7_source_validator_for_asset_validation",
        validator_path,
    )
    if module_spec is None or module_spec.loader is None:
        raise RuntimeError("v7 deterministic source validator could not be loaded")
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    validate = getattr(module, "validate_source_candidate", None)
    expected_sections = getattr(module, "_expected_contract_sections", None)
    if not callable(validate) or not callable(expected_sections):
        raise RuntimeError(
            "v7 deterministic source validator contract entry points are absent"
        )
    source = strict_json(
        (project_root / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json")
        .read_text(encoding="utf-8")
    )
    candidate = strict_json(
        (project_root / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v7.json")
        .read_text(encoding="utf-8")
    )
    expected_contract = {
        "schema": "DiscGolfTour.ProductionMotionV7SourceDesignContract.v1",
        "source_recipe": (
            "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json"
        ),
        "mutation_scope": (
            "DENSE_DRIVE_BILATERAL_CLAVICLE_UPPERARM_LOWERARM_HAND_ROTATIONS_ONLY"
        ),
        "source_metric_basis": (
            "EXACT_PROBED_DGMASTER_REFERENCE_CHAIN_STATIC_RECONSTRUCTION"
        ),
        **expected_sections(),
        "static_source_geometry_approval": False,
        "unreal_assets_authored": False,
        "runtime_source_pose_approval": False,
        "metahuman_deformation_approval": False,
        "human_animation_approval": False,
        "human_disc_contact_approval": False,
        "release_approval": False,
        "status": "CANDIDATE_REQUIRES_FAIL_CLOSED_V7_VALIDATION",
    }
    if candidate.get("v7_source_motion_design_contract") != expected_contract:
        raise RuntimeError(
            "v7 asset validation remains locked unless the complete final "
            "source-motion design contract matches the deterministic authority"
        )
    issues, observations = validate(source, candidate)
    if issues:
        raise RuntimeError(
            "v7 deterministic source validation failed: " + "; ".join(issues[:5])
        )
    if not isinstance(observations, dict):
        raise RuntimeError("v7 deterministic source observations are malformed")
    validate_v7_source_observations(observations, expected_contract)
    return observations


def _v7_finite_nonnegative(value: Any) -> bool:
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(float(value))
        and float(value) >= 0.0
    )


def _v7_observed_gate_passes(
    evidence: dict[str, Any],
    actual_field: str,
    limit_field: str,
    expected_limit: float,
) -> bool:
    actual = evidence.get(actual_field)
    reported_limit = evidence.get(limit_field)
    return (
        _v7_finite_nonnegative(actual)
        and _v7_finite_nonnegative(reported_limit)
        and math.isclose(
            float(reported_limit), expected_limit, rel_tol=0.0, abs_tol=1e-9
        )
        and float(actual) <= expected_limit + 1e-6
    )


def validate_v7_source_observations(
    observations: dict[str, Any], expected_contract: dict[str, Any]
) -> None:
    """Require exact dense/proximal/continuity proof before reading v007 assets."""
    dense = observations.get("dense_drive_identity")
    if not isinstance(dense, dict) or any(
        (
            dense.get("required_frame_range") != [0, 144],
            dense.get("required_pose_count") != 145,
            dense.get("observed_pose_count") != 145,
            dense.get("frame_set_and_order_exact") is not True,
            dense.get(
                "non_arm_channels_equal_v006_integer_frame_catmull_samples"
            ) is not True,
            dense.get("non_arm_mismatch_count") != 0,
            dense.get(
                "approach_and_putt_payload_exact_after_version_identity"
            ) is not True,
        )
    ):
        raise RuntimeError("v7 dense Drive identity proof is incomplete or differs")

    mutation = observations.get("arm_mutation_scope")
    if (
        not isinstance(mutation, dict)
        or mutation.get("exact") is not True
        or not isinstance(mutation.get("changed_channel_count"), int)
        or isinstance(mutation.get("changed_channel_count"), bool)
        or mutation.get("changed_channel_count") <= 0
        or mutation.get("changed_channel_count")
            != mutation.get("expected_changed_channel_count")
    ):
        raise RuntimeError("v7 bilateral arm mutation-scope proof is not exact")

    grip = observations.get("frame_84_grip_preservation")
    if not isinstance(grip, dict):
        raise RuntimeError("v7 frame-84 grip/proximal evidence is absent")
    proximal = expected_contract["release_proximal_stability_gate"]
    grip_gates = (
        (
            "hand_origin_delta_cm",
            "maximum_component_translation_error_cm",
            0.1,
        ),
        (
            "hand_orientation_delta_degrees",
            "maximum_component_rotation_error_degrees",
            0.1,
        ),
        (
            "disc_grip_origin_delta_cm",
            "maximum_component_translation_error_cm",
            0.1,
        ),
        (
            "disc_grip_orientation_delta_degrees",
            "maximum_component_rotation_error_degrees",
            0.1,
        ),
        (
            "release_shoulder_origin_delta_from_v006_cm",
            "maximum_release_shoulder_origin_delta_from_v006_cm",
            proximal["maximum_shoulder_origin_delta_from_v006_cm"],
        ),
        (
            "release_clavicle_relative_to_spine_delta_from_v006_degrees",
            "maximum_release_clavicle_relative_to_spine_delta_from_v006_degrees",
            proximal[
                "maximum_clavicle_relative_to_spine_delta_from_v006_degrees"
            ],
        ),
        (
            "fixed_hand_wrist_correction_degrees",
            "maximum_fixed_hand_wrist_correction_degrees",
            proximal["maximum_fixed_hand_wrist_correction_degrees"],
        ),
    )
    if any(
        not _v7_observed_gate_passes(grip, actual, limit, expected)
        for actual, limit, expected in grip_gates
    ):
        raise RuntimeError("v7 frame-84 grip/proximal evidence exceeds fixed limits")

    continuity = observations.get("source_continuity_guards")
    continuity_contract = expected_contract["source_continuity_gate"]
    if (
        not isinstance(continuity, dict)
        or continuity.get("frame_rate") != 60
        or continuity.get("quaternion_log_vector_convention")
            != continuity_contract["quaternion_log_vector_convention"]
    ):
        raise RuntimeError(
            "v7 continuity proof is absent or uses a different quaternion convention"
        )

    nested_gates = {
        "torso_local_shoulder": (
            (
                "maximum_translation_cm_per_frame",
                "maximum_translation_limit_cm_per_frame",
                continuity_contract[
                    "maximum_torso_local_shoulder_translation_cm_per_frame"
                ],
            ),
            (
                "maximum_acceleration_cm_per_frame_squared",
                "maximum_acceleration_limit_cm_per_frame_squared",
                continuity_contract[
                    "maximum_torso_local_shoulder_acceleration_cm_per_frame_squared"
                ],
            ),
        ),
        "torso_local_elbow": (
            (
                "maximum_translation_cm_per_frame",
                "maximum_translation_limit_cm_per_frame",
                continuity_contract[
                    "maximum_torso_local_elbow_translation_cm_per_frame"
                ],
            ),
            (
                "maximum_acceleration_cm_per_frame_squared",
                "maximum_acceleration_limit_cm_per_frame_squared",
                continuity_contract[
                    "maximum_torso_local_elbow_acceleration_cm_per_frame_squared"
                ],
            ),
        ),
        "clavicle_relative_to_spine": (
            (
                "maximum_log_vector_rate_degrees_per_frame",
                "maximum_log_vector_rate_limit_degrees_per_frame",
                continuity_contract[
                    "maximum_clavicle_relative_to_spine_log_vector_rate_degrees_per_frame"
                ],
            ),
            (
                "maximum_log_vector_acceleration_degrees_per_frame_squared",
                "maximum_log_vector_acceleration_limit_degrees_per_frame_squared",
                continuity_contract[
                    "maximum_clavicle_relative_to_spine_log_vector_acceleration_degrees_per_frame_squared"
                ],
            ),
        ),
        "throwing_upperarm_component": (
            (
                "maximum_log_vector_rate_degrees_per_frame",
                "maximum_log_vector_rate_limit_degrees_per_frame",
                continuity_contract[
                    "maximum_throwing_upperarm_component_log_vector_rate_degrees_per_frame"
                ],
            ),
            (
                "maximum_log_vector_acceleration_degrees_per_frame_squared",
                "maximum_log_vector_acceleration_limit_degrees_per_frame_squared",
                continuity_contract[
                    "maximum_throwing_upperarm_component_log_vector_acceleration_degrees_per_frame_squared"
                ],
            ),
        ),
        "throwing_lowerarm_component": (
            (
                "maximum_log_vector_rate_degrees_per_frame",
                "maximum_log_vector_rate_limit_degrees_per_frame",
                continuity_contract[
                    "maximum_throwing_lowerarm_component_log_vector_rate_degrees_per_frame"
                ],
            ),
            (
                "maximum_log_vector_acceleration_degrees_per_frame_squared",
                "maximum_log_vector_acceleration_limit_degrees_per_frame_squared",
                continuity_contract[
                    "maximum_throwing_lowerarm_component_log_vector_acceleration_degrees_per_frame_squared"
                ],
            ),
        ),
    }
    for section_name, gates in nested_gates.items():
        section = continuity.get(section_name)
        if not isinstance(section, dict) or any(
            not _v7_observed_gate_passes(section, actual, limit, expected)
            for actual, limit, expected in gates
        ):
            raise RuntimeError(
                f"v7 {section_name} continuity evidence exceeds fixed limits"
            )

    flat_gates = (
        (
            "maximum_hand_translation_cm_per_frame",
            "maximum_hand_translation_limit_cm_per_frame",
            continuity_contract["maximum_hand_translation_cm_per_frame"],
        ),
        (
            "maximum_grip_translation_cm_per_frame",
            "maximum_grip_translation_limit_cm_per_frame",
            continuity_contract["maximum_grip_translation_cm_per_frame"],
        ),
        (
            "maximum_hand_acceleration_cm_per_frame_squared",
            "maximum_hand_acceleration_limit_cm_per_frame_squared",
            continuity_contract[
                "maximum_hand_translation_acceleration_cm_per_frame_squared"
            ],
        ),
        (
            "maximum_grip_acceleration_cm_per_frame_squared",
            "maximum_grip_acceleration_limit_cm_per_frame_squared",
            continuity_contract[
                "maximum_grip_translation_acceleration_cm_per_frame_squared"
            ],
        ),
        (
            "maximum_elbow_delta_degrees_per_frame",
            "maximum_elbow_delta_limit_degrees_per_frame",
            continuity_contract["maximum_elbow_delta_degrees_per_frame"],
        ),
        (
            "maximum_elbow_bend_plane_delta_degrees_per_frame",
            "maximum_elbow_bend_plane_delta_limit_degrees_per_frame",
            continuity_contract[
                "maximum_elbow_bend_plane_delta_degrees_per_frame"
            ],
        ),
        (
            "maximum_pre_release_disc_plane_delta_degrees_per_frame",
            "maximum_pre_release_disc_plane_delta_limit_degrees",
            continuity_contract[
                "maximum_pre_release_disc_plane_delta_degrees_per_frame"
            ],
        ),
        (
            "maximum_throwing_wrist_additive_degrees",
            "maximum_throwing_wrist_additive_limit_degrees",
            continuity_contract["maximum_throwing_wrist_additive_degrees"],
        ),
    )
    if any(
        not _v7_observed_gate_passes(continuity, actual, limit, expected)
        for actual, limit, expected in flat_gates
    ):
        raise RuntimeError("v7 full-path continuity evidence exceeds fixed limits")


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    if not all(
        exact_switch(command_line, switch)
        for switch in (VALIDATE_SWITCH, "-unattended", "-nop4")
    ):
        raise RuntimeError("read-only validation switches are incomplete")
    if "-run=pythonscript" not in command_line.casefold():
        raise RuntimeError("validation must run through PythonScript commandlet")
    version = command_value(command_line, "DGProductionMotionVersion").casefold()
    if version not in VERSION_SPECS:
        raise RuntimeError(
            "exact -DGProductionMotionVersion=v1, v2, v3, v4, v5, v6, or v7 is required"
        )
    version_spec = VERSION_SPECS[version]
    user_text = command_value(command_line, "UserDir")
    if not user_text:
        raise RuntimeError("validation requires an external -UserDir")
    user_dir = Path(user_text).resolve()
    relative = user_dir.relative_to(USER_ROOT.resolve())
    if len(relative.parts) != 1 or str(uuid.UUID(relative.name)) != relative.name.casefold():
        raise RuntimeError("validation UserDir must be one canonical UUID child")
    report_path = user_dir / REPORT_RELATIVE
    if report_path.exists():
        raise RuntimeError("validation report already exists")

    project_root = Path(unreal.Paths.project_dir()).resolve()
    v7_source_validation = (
        validate_v7_source_lane(project_root) if version == "v7" else None
    )
    protected_v6_before = (
        require_exact_files(
            project_root,
            PROTECTED_V6_FILES,
            "immutable v006 production history",
        )
        if version == "v7"
        else None
    )
    v7_authoring_evidence_before = (
        require_v7_authoring_evidence(project_root) if version == "v7" else None
    )
    target_before = snapshot(project_root / TARGET_ROOT)
    protected_before = {
        root.as_posix(): snapshot(project_root / root) for root in PROTECTED_ROOTS
    }
    save_path = project_root / SAVE_RELATIVE
    save_before = {"bytes": save_path.stat().st_size, "sha256": sha256(save_path)}
    dirty_content_before = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    )
    dirty_maps_before = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )
    if dirty_content_before or dirty_maps_before:
        raise RuntimeError("read-only validation began with dirty packages")

    utility = unreal.DiscGolfProductionMotionAuthoringUtility
    method_name = "validate_production_motion_assets_for_version"
    if not hasattr(utility, method_name):
        raise RuntimeError(f"required reflected native method is absent: {method_name}")
    raw = getattr(utility, method_name)(version)
    native = strict_json(raw)
    if not str(native.get("status", "")).startswith("PASS"):
        raise RuntimeError(f"native validation failed: {native}")
    expected = {
        "family_count": 3,
        "sequence_count": 3,
        "montage_count": 3,
        "library_entry_count": 3,
        "derived_from_synthetic_fixture": False,
        "external_performance_claim": False,
        "human_animation_approval": False,
        "human_disc_contact_approval": False,
        "release_ready": False,
    }
    for key, value in expected.items():
        if native.get(key) != value:
            raise RuntimeError(f"native validation field differs: {key}")
    expected_versioned = {
        "schema": (
            "DiscGolfTour.Session19ProductionMotionNativeReport.v7"
            if version == "v7"
            else (
                "DiscGolfTour.Session19ProductionMotionNativeReport.v6"
                if version == "v6"
                else (
                    "DiscGolfTour.Session19ProductionMotionNativeReport.v5"
                    if version == "v5"
                    else (
                        "DiscGolfTour.Session19ProductionMotionNativeReport.v4"
                        if version == "v4"
                        else (
                            "DiscGolfTour.Session19ProductionMotionNativeReport.v3"
                            if version == "v3"
                            else "DiscGolfTour.Session19ProductionMotionNativeReport.v2"
                        )
                    )
                )
            )
        ),
        "recipe_version": version,
        "asset_revision": version_spec["asset_revision"],
        "world_motion_authority": "GAMEPLAY_PAWN_CAPSULE",
        "presentation_root_trajectory_enabled": version_spec[
            "presentation_root_trajectory_enabled"
        ],
    }
    for key, value in expected_versioned.items():
        if native.get(key) != value:
            raise RuntimeError(
                f"native versioned validation field differs: {key}"
            )
    if version in GRANULAR_MOTION:
        for key, value in {
            "interpolation_mode": "CATMULL_ROM_BIOMECHANICAL_PHASE_SHAPED",
            "biomechanical_model": "RHBH_KINETIC_CHAIN_GRANULAR_V1",
            "disc_contact_model": "REACHBACK_PLANE_WRIST_LAG_FINGER_RELEASE",
            "recovery_model": "THREE_BEAT_DECELERATION_RECENTER_SETTLE",
        }.items():
            if native.get(key) != value:
                raise RuntimeError(f"native granular field differs: {key}")
        expected_metrics = {
            "Drive": (
                (145, 1, 45, 23, 19)
                if version == "v7"
                else (35, 6, 11, 23, 19)
            ),
            "Approach": (36, 5, 10, 23, 19),
            "Putt": (30, 4, 10, 23, 19),
        }
        families = native.get("families")
        if not isinstance(families, list) or len(families) != 3:
            raise RuntimeError("native granular family metrics are absent")
        for family in families:
            name = family.get("family")
            expected_family = expected_metrics.get(name)
            if expected_family is None:
                raise RuntimeError("native granular family metric identity differs")
            actual = (
                family.get("pose_key_count"),
                family.get("maximum_pose_key_gap_frames"),
                family.get("recovery_pose_key_count"),
                family.get("minimum_authored_rotation_channels_per_pose"),
                family.get("curve_count"),
            )
            if actual != expected_family:
                raise RuntimeError(f"native granular density metrics differ for {name}")
    if version == "v4":
        for key, value in {
            "rotation_space": "COMPONENT_SPACE_ADDITIVE_TO_REFERENCE",
            "local_track_construction": (
                "COMPONENT_INTENT_CONVERTED_TO_LOCAL_BONE_TRACKS"
            ),
        }.items():
            if native.get(key) != value:
                raise RuntimeError(f"native v4 field differs: {key}")
        for family in native.get("families", []):
            if (
                family.get("rotation_space")
                    != "COMPONENT_SPACE_ADDITIVE_TO_REFERENCE"
                or family.get("local_track_construction")
                    != "COMPONENT_INTENT_CONVERTED_TO_LOCAL_BONE_TRACKS"
                or family.get("component_validation_sampled_frames")
                    != family.get("frames") + 1
                or not 0.998 <= family.get(
                    "minimum_joint_segment_ratio", 0.0
                ) <= 1.002
                or not 0.998 <= family.get(
                    "maximum_joint_segment_ratio", 0.0
                ) <= 1.002
                or family.get(
                    "maximum_component_intent_error_degrees", 999.0
                ) > 0.05
                or family.get("maximum_component_delta_degrees", 999.0) > 95.0
            ):
                raise RuntimeError(
                    f"native v4 component metrics differ for {family.get('family')}"
                )
    if version in {"v5", "v6", "v7"}:
        for key, value in {
            "rotation_space": (
                "MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_ADDITIVE_TO_REFERENCE"
            ),
            "root_track_policy": "TRANSLATION_ONLY_REFERENCE_ROTATION_SCALE",
            "local_track_construction": (
                "MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_BONE_TRACKS"
            ),
            "component_rotation_bone_count": 7,
            "local_rotation_bone_count": 31,
        }.items():
            if native.get(key) != value:
                raise RuntimeError(f"native {version} field differs: {key}")
        for family in native.get("families", []):
            maximum_local_frame_delta = family.get(
                "maximum_local_frame_delta_degrees", 999.0
            )
            maximum_local_frame_delta_limit = (
                24.0
                if version == "v7" and family.get("family") == "Drive"
                else 20.0
            )
            if (
                family.get("rotation_space")
                    != "MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_ADDITIVE_TO_REFERENCE"
                or family.get("root_track_policy")
                    != "TRANSLATION_ONLY_REFERENCE_ROTATION_SCALE"
                or family.get("local_track_construction")
                    != "MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_BONE_TRACKS"
                or family.get("component_rotation_bone_count") != 7
                or family.get("local_rotation_bone_count") != 31
                or family.get("component_validation_sampled_frames")
                    != family.get("frames") + 1
                or not 0.998 <= family.get("minimum_joint_segment_ratio", 0.0) <= 1.002
                or not 0.998 <= family.get("maximum_joint_segment_ratio", 0.0) <= 1.002
                or family.get("maximum_component_intent_error_degrees", 999.0) > 0.05
                or family.get("maximum_local_intent_error_degrees", 999.0) > 0.05
                or family.get("maximum_root_rotation_error_degrees", 999.0) > 0.01
                or maximum_local_frame_delta > maximum_local_frame_delta_limit
                or family.get("maximum_finger_frame_delta_degrees", 999.0) > 20.0
                or family.get("maximum_component_delta_degrees", 999.0) > 95.0
                or family.get("named_phase_knee_sample_count") != 16
                or not 130.0 <= family.get(
                    "minimum_named_phase_knee_angle_degrees", 0.0
                ) <= 176.0
                or not 130.0 <= family.get(
                    "maximum_named_phase_knee_angle_degrees", 999.0
                ) <= 176.0
            ):
                raise RuntimeError(
                    f"native {version} mixed-space metrics differ for {family.get('family')}"
                )
    if version == "v6":
        scalar_fields = (
            "reachback_throwing_elbow_angle_degrees",
            "release_throwing_elbow_angle_degrees",
            "followthrough_throwing_elbow_angle_degrees",
            "reachback_throwing_arm_reach_ratio",
            "release_throwing_arm_reach_ratio",
            "followthrough_throwing_arm_reach_ratio",
            "reachback_throwing_hand_torso_clearance_cm",
            "release_throwing_hand_torso_clearance_cm",
            "followthrough_throwing_hand_torso_clearance_cm",
            "reachback_to_release_arm_direction_delta_degrees",
            "release_to_followthrough_arm_direction_delta_degrees",
            "reachback_to_followthrough_arm_direction_delta_degrees",
            "reachback_to_followthrough_hand_travel_cm",
            "reachback_throwing_arm_collapse_ratio",
            "release_throwing_arm_collapse_ratio",
            "followthrough_throwing_arm_collapse_ratio",
            "reachback_throwing_hand_radial_reach_cm",
            "minimum_throwing_hand_vertical_cm",
            "minimum_release_follow_throwing_hand_lateral_clearance_cm",
            "maximum_throwing_hand_direction_frame_delta_degrees",
        )
        direction_fields = (
            "reachback_throwing_arm_direction_torso_local",
            "release_throwing_arm_direction_torso_local",
            "followthrough_throwing_arm_direction_torso_local",
        )
        for family in native.get("families", []):
            if (
                family.get("arm_spatial_sample_count") != 3
                or family.get("arm_spatial_gate_applied") is not True
                or family.get("arm_spatial_gate_passed") is not True
            ):
                raise RuntimeError(
                    f"native v6 arm spatial gate differs for {family.get('family')}"
                )
            if any(
                not isinstance(family.get(field), (int, float))
                or isinstance(family.get(field), bool)
                or not math.isfinite(float(family[field]))
                for field in scalar_fields
            ):
                raise RuntimeError(
                    f"native v6 arm spatial scalar differs for {family.get('family')}"
                )
            for field in direction_fields:
                direction = family.get(field)
                if (
                    not isinstance(direction, list)
                    or len(direction) != 3
                    or any(
                        not isinstance(value, (int, float))
                        or isinstance(value, bool)
                        or not math.isfinite(float(value))
                        for value in direction
                    )
                    or not 0.999 <= math.sqrt(sum(
                        float(value) ** 2 for value in direction
                    )) <= 1.001
                ):
                    raise RuntimeError(
                        f"native v6 arm direction differs for "
                        f"{family.get('family')}:{field}"
                    )
            hand_fields = (
                "reachback_throwing_hand_torso_local_cm",
                "release_throwing_hand_torso_local_cm",
                "followthrough_throwing_hand_torso_local_cm",
            )
            hands = []
            for field in hand_fields:
                hand = family.get(field)
                if (
                    not isinstance(hand, list)
                    or len(hand) != 3
                    or any(
                        not isinstance(value, (int, float))
                        or isinstance(value, bool)
                        or not math.isfinite(float(value))
                        for value in hand
                    )
                ):
                    raise RuntimeError(
                        f"native v6 hand position differs for "
                        f"{family.get('family')}:{field}"
                    )
                hands.append([float(value) for value in hand])
            expected_thresholds = {
                "minimum_elbow_angle_degrees": 110.0,
                "minimum_arm_extension_ratio": 0.82,
                "reachback_throwing_side_directional_dominance": True,
                "followthrough_throwing_side_directional_dominance": True,
                "minimum_reachback_radial_reach_cm": 64.0,
                "minimum_hand_vertical_cm": 0.0,
                "minimum_release_follow_lateral_clearance_cm": 60.0,
                "maximum_direction_frame_delta_degrees": 8.0,
            }
            reachback, _release, followthrough = hands
            if (
                not threshold_contract_matches(
                    family.get("arm_spatial_gate_thresholds"),
                    expected_thresholds,
                )
                or min(
                    float(family["reachback_throwing_elbow_angle_degrees"]),
                    float(family["release_throwing_elbow_angle_degrees"]),
                    float(family["followthrough_throwing_elbow_angle_degrees"]),
                ) < 110.0
                or min(
                    float(family["reachback_throwing_arm_reach_ratio"]),
                    float(family["release_throwing_arm_reach_ratio"]),
                    float(family["followthrough_throwing_arm_reach_ratio"]),
                ) < 0.82
                or abs(reachback[0]) < abs(reachback[2])
                or abs(followthrough[0]) < abs(followthrough[2])
                or float(family["reachback_throwing_hand_radial_reach_cm"])
                    < 64.0
                or float(family["minimum_throwing_hand_vertical_cm"]) < 0.0
                or float(family[
                    "minimum_release_follow_throwing_hand_lateral_clearance_cm"
                ]) < 60.0
                or float(family[
                    "maximum_throwing_hand_direction_frame_delta_degrees"
                ]) > 8.0
                or abs(float(family[
                    "reachback_throwing_arm_collapse_ratio"
                ]) - (1.0 - float(family[
                    "reachback_throwing_arm_reach_ratio"
                ]))) > 1e-4
                or abs(float(family[
                    "release_throwing_arm_collapse_ratio"
                ]) - (1.0 - float(family[
                    "release_throwing_arm_reach_ratio"
                ]))) > 1e-4
                or abs(float(family[
                    "followthrough_throwing_arm_collapse_ratio"
                ]) - (1.0 - float(family[
                    "followthrough_throwing_arm_reach_ratio"
                ]))) > 1e-4
            ):
                raise RuntimeError(
                    f"native v6 spatial threshold evidence differs for "
                    f"{family.get('family')}"
                )
    if version == "v7":
        validate_v7_native_arm_gates(native)

    target_after = snapshot(project_root / TARGET_ROOT)
    protected_after = {
        root.as_posix(): snapshot(project_root / root) for root in PROTECTED_ROOTS
    }
    save_after = {"bytes": save_path.stat().st_size, "sha256": sha256(save_path)}
    dirty_content_after = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    )
    dirty_maps_after = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )
    if target_after != target_before:
        raise RuntimeError("target assets changed during read-only validation")
    if protected_after != protected_before:
        raise RuntimeError("protected synthetic roots changed during validation")
    if save_after != save_before:
        raise RuntimeError("protected save changed during validation")
    if dirty_content_after or dirty_maps_after:
        raise RuntimeError("read-only validation left dirty packages")
    protected_v6_after = (
        require_exact_files(
            project_root,
            PROTECTED_V6_FILES,
            "immutable v006 production history",
        )
        if version == "v7"
        else None
    )
    if protected_v6_after != protected_v6_before:
        raise RuntimeError("immutable v006 production history changed during validation")
    v7_authoring_evidence_after = (
        require_v7_authoring_evidence(project_root) if version == "v7" else None
    )
    if v7_authoring_evidence_after != v7_authoring_evidence_before:
        raise RuntimeError("immutable v007 authoring evidence changed during validation")

    report = {
        "schema": (
            "DiscGolfTour.Session19ProductionMotionAssetValidation.v7"
            if version == "v7"
            else (
                "DiscGolfTour.Session19ProductionMotionAssetValidation.v6"
                if version == "v6"
                else (
                    "DiscGolfTour.Session19ProductionMotionAssetValidation.v5"
                    if version == "v5"
                    else (
                        "DiscGolfTour.Session19ProductionMotionAssetValidation.v4"
                        if version == "v4"
                        else (
                            "DiscGolfTour.Session19ProductionMotionAssetValidation.v3"
                            if version == "v3"
                            else "DiscGolfTour.Session19ProductionMotionAssetValidation.v2"
                        )
                    )
                )
            )
        ),
        "status": "PASS_NO_PROJECT_MUTATION",
        "recipe_version": version,
        "asset_revision": version_spec["asset_revision"],
        "native_validation": native,
        "target_files": target_after,
        "target_file_count": len(target_after),
        "protected_synthetic_roots_unchanged": True,
        "protected_save": {"path": SAVE_RELATIVE.as_posix(), **save_after},
        "dirty_content_packages_before": dirty_content_before,
        "dirty_map_packages_before": dirty_maps_before,
        "dirty_content_packages_after": dirty_content_after,
        "dirty_map_packages_after": dirty_maps_after,
        "human_animation_approval": False,
        "human_disc_contact_approval": False,
        "release_ready": False,
    }
    if version == "v7":
        report["v7_source_validation"] = v7_source_validation
        report["protected_v006_production_history"] = protected_v6_after
        report["v007_authoring_evidence"] = v7_authoring_evidence_after
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    unreal.log(
        "DG_SESSION19_PRODUCTION_MOTION_VALIDATE: PASS_NO_PROJECT_MUTATION "
        "families=3 assets=7 human_animation=false human_contact=false"
    )


if __name__ == "__main__":
    main()
