#!/usr/bin/env python3
"""Validate the fail-closed Session 19 production-motion candidate contract.

This lane pins the independently authored Drive, Approach, and Putt candidates,
their runtime/cook binding, provenance, and protected synthetic exclusions. It
does not claim human animation quality, disc contact, fresh packaged presence,
product approval, or release readiness.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19ProductionMotionAuthoringPolicy.json"
SHA_RE = re.compile(r"^[0-9A-F]{64}$")
CANDIDATE_ID = "S19_WindowsShipping_20260825T163212Z_5f703bd619b5"
CANDIDATE_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]{1,96}$")
CANDIDATE_CONTENT_RE = re.compile(
    r"^Evidence/Session19/CandidateContent-"
    r"(S19_WindowsShipping_[A-Za-z0-9_-]{1,96})\.json$"
)
TARGET_ROOT = "/Game/DiscGolf/Animation/ProductionMotion"
SYNTHETIC_ROOTS = (
    "/Game/DiscGolf/Animation/Mocap",
    "/Game/DiscGolf/Animation/Throws",
)
BINDING_KEYS = {
    "motionPipelineDocument", "frozenMotionRegistry",
    "session3SyntheticAuthoringUtility", "session5SyntheticPipelineUtility",
    "session8BMetaHumanRuntimeAuthor", "golferPawnSource", "golferPawnHeader",
    "metaHumanRetargetRuntimeSource", "runtimeCookManifest", "packagingSettings",
    "candidateContentAudit", "dgMasterSkeleton", "dgMasterControlRig",
    "playerAnimationBlueprint", "metaHumanRetargeter",
    "metaHumanRuntimeBlueprint", "metaHumanAvatarProfile",
    "productionMotionRuntimePaths", "productionMotionAuthoringUtilityHeader",
    "productionMotionAuthoringUtilitySource", "productionMotionAuthorWrapper",
    "productionMotionAssetValidator", "productionMotionRecipe",
    "productionMotionCandidateAuthoringEvidence",
}
REQUIRED_PHASES = [
    "Aim", "RunUp", "ReachBack", "Plant", "Acceleration",
    "FollowThrough", "Recovery",
]
REQUIRED_CURVES = [
    "DG_FootPlant_L", "DG_FootPlant_R", "DG_ReachbackAlpha",
    "DG_BraceAlpha", "DG_ReleaseApproachAlpha", "DG_FollowThroughAlpha",
]
V3_REQUIRED_CURVES = REQUIRED_CURVES + [
    "DG_WeightShiftAlpha", "DG_BraceCompressionAlpha", "DG_HipDriveAlpha",
    "DG_TorsoDriveAlpha", "DG_ShoulderDriveAlpha", "DG_ElbowLeadAlpha",
    "DG_WristLagAlpha", "DG_FingerReleaseAlpha",
    "DG_OffArmCounterbalanceAlpha", "DG_GazeTargetAlpha",
    "DG_DiscPlaneAlpha", "DG_BraceExtensionAlpha", "DG_RecoveryBeatAlpha",
]
FAMILY_SPECS = [
    {
        "id": "DG_PROCEDURAL_DRIVE_RHBH_V1",
        "family": "Drive",
        "throwType": "Backhand",
        "handedness": "Right",
        "sequenceObjectPath": f"{TARGET_ROOT}/Drive/A_DG_RHBH_Drive_Procedural_v001.A_DG_RHBH_Drive_Procedural_v001",
        "montageObjectPath": f"{TARGET_ROOT}/Drive/AM_DG_RHBH_Drive_Procedural_v001.AM_DG_RHBH_Drive_Procedural_v001",
        "durationSecondsMin": 1.8,
        "durationSecondsMax": 3.5,
        "recommendedPowerMin": 0.55,
        "recommendedPowerMax": 1.0,
        "requiredCoverage": [
            "SETUP_AIM", "RUN_UP", "REACH_BACK", "FOOT_PLANT",
            "ACCELERATION", "RELEASE", "FOLLOW_THROUGH", "RECOVERY",
        ],
        "assetState": "AUTHORED_PROCEDURAL_CANDIDATE_HUMAN_REVIEW_REQUIRED",
        "runtimeBindingState": "BOUND_TECHNICAL_CANDIDATE",
        "sequenceFile": {
            "path": "Content/DiscGolf/Animation/ProductionMotion/Drive/A_DG_RHBH_Drive_Procedural_v001.uasset",
            "bytes": 272668,
            "sha256": "43530C4802599684CA44CA62609C8759E7D06E5CE9C692E657C3120D4985C822",
        },
        "montageFile": {
            "path": "Content/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v001.uasset",
            "bytes": 21214,
            "sha256": "ED67CFF772682AB023F522AB4D0CD7FC535A3DDE1501A2CA17108D458604C2CE",
        },
    },
    {
        "id": "DG_PROCEDURAL_APPROACH_RHBH_V1",
        "family": "Approach",
        "throwType": "Backhand",
        "handedness": "Right",
        "sequenceObjectPath": f"{TARGET_ROOT}/Approach/A_DG_RHBH_Approach_Procedural_v001.A_DG_RHBH_Approach_Procedural_v001",
        "montageObjectPath": f"{TARGET_ROOT}/Approach/AM_DG_RHBH_Approach_Procedural_v001.AM_DG_RHBH_Approach_Procedural_v001",
        "durationSecondsMin": 1.2,
        "durationSecondsMax": 2.6,
        "recommendedPowerMin": 0.25,
        "recommendedPowerMax": 0.75,
        "requiredCoverage": [
            "SETUP_AIM", "CONTROLLED_STEP", "REACH_BACK", "FOOT_PLANT",
            "ACCELERATION", "RELEASE", "FOLLOW_THROUGH", "RECOVERY",
        ],
        "assetState": "AUTHORED_PROCEDURAL_CANDIDATE_HUMAN_REVIEW_REQUIRED",
        "runtimeBindingState": "BOUND_TECHNICAL_CANDIDATE",
        "sequenceFile": {
            "path": "Content/DiscGolf/Animation/ProductionMotion/Approach/A_DG_RHBH_Approach_Procedural_v001.uasset",
            "bytes": 183697,
            "sha256": "FF38F160DA70CD0A12B4E2FD335C128E3043E1F850901930A95AA65C24DEDDB1",
        },
        "montageFile": {
            "path": "Content/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v001.uasset",
            "bytes": 21244,
            "sha256": "96D06A1AF3AEE1C764C582B06D0E20CB029CE5E2F0CA511F59EF3B495787163C",
        },
    },
    {
        "id": "DG_PROCEDURAL_PUTT_RHBH_V1",
        "family": "Putt",
        "throwType": "Backhand",
        "handedness": "Right",
        "sequenceObjectPath": f"{TARGET_ROOT}/Putt/A_DG_RHBH_Putt_Procedural_v001.A_DG_RHBH_Putt_Procedural_v001",
        "montageObjectPath": f"{TARGET_ROOT}/Putt/AM_DG_RHBH_Putt_Procedural_v001.AM_DG_RHBH_Putt_Procedural_v001",
        "durationSecondsMin": 0.8,
        "durationSecondsMax": 2.0,
        "recommendedPowerMin": 0.05,
        "recommendedPowerMax": 0.45,
        "requiredCoverage": [
            "SETUP_AIM", "WEIGHT_SHIFT", "FOOT_PLANT", "ACCELERATION",
            "RELEASE", "FOLLOW_THROUGH", "RECOVERY",
        ],
        "assetState": "AUTHORED_PROCEDURAL_CANDIDATE_HUMAN_REVIEW_REQUIRED",
        "runtimeBindingState": "BOUND_TECHNICAL_CANDIDATE",
        "sequenceFile": {
            "path": "Content/DiscGolf/Animation/ProductionMotion/Putt/A_DG_RHBH_Putt_Procedural_v001.uasset",
            "bytes": 131591,
            "sha256": "E8924448E99D01FA2B0B7361FBC50DB31A76670F6E8A070FAAEB5844A5B83781",
        },
        "montageFile": {
            "path": "Content/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v001.uasset",
            "bytes": 21204,
            "sha256": "9148D82E4161790818542107C25C389718E1B4519390255EE6D08367B7F5EC1A",
        },
    },
]

VERSIONED_ASSET_IDENTITIES = {
    "v2": {
        "Drive": {
            "sequence": (430246, "4ABED0194663C99A0859EB533406BD75FCC406AC9351AC04582C65EC13DF7E32"),
            "montage": (21417, "F66134F9AFB001D0C55752C10AC4512EB78AA4686121854F6B4399CCFDF2E15F"),
        },
        "Approach": {
            "sequence": (320409, "9C613490E29BA475606C356E8DC743E26515C1C3EE6DFCF8247A132E35358BC1"),
            "montage": (21447, "00C6FD7BF53CF90C06D6EA23E67940B13780347CAC28FF61838CF757FDF73BA9"),
        },
        "Putt": {
            "sequence": (217541, "27DDA73C2C209E500F40B32D6BDB33BF10A73DC4E455A2F8C7301904791DC07E"),
            "montage": (21407, "DAAFCBE347F5C16D180F0BA14D31CDCD04C835C14280DEB03FE1946A705B5B8B"),
        },
        "library": (4477, "9E0F9876B4B106A204E3ACF5D4BA2A980E41C6E61F5F1A58147BB428F4753592"),
    },
    "v3": {
        "Drive": {
            "sequence": (460500, "3566C3CDEB794D7436090C81712F535362270B51C162C59B9966CD8A9FC637E3"),
            "montage": (21695, "D499FCE8882E3EC45ED5DA740F9362F2AD21F32E9CDCB127B9FB132B97F7C3FB"),
        },
        "Approach": {
            "sequence": (356486, "5833A56E566575D57703A6E5077D4BB3C08787937BEFD7D40F3530F2D97D8829"),
            "montage": (21725, "8BB5F4DB588DE6DB731ABE34534ACC4CEC768001F02D290F2C3DC27410231D29"),
        },
        "Putt": {
            "sequence": (252498, "D3862386FB04D966638019BFF88868C376A3D5130AD19D9A77D60B9BA2339E5F"),
            "montage": (21685, "6E2572E82B4D9F3766952FCA5D10395BE8E87990D0F0F956EAF75CC699C342E3"),
        },
        "library": (4755, "60AA39C6B51A6E774B7E9C53E8E6B0D5248EB07FCE795A1DC061666D44B95515"),
    },
    "v4": {
        "Drive": {
            "sequence": (534909, "88D82002DD58EEE87FE5A8263D59DE8D50CCC9053B32C04E9BB89CD77781A58E"),
            "montage": (21864, "B44DA35C0E95AB7DBF63A9403D264A9C5FEDCEE5230326D3C0D2E222971214AE"),
        },
        "Approach": {
            "sequence": (422927, "98FB3EB65AB18648E231D3BE7BEAE8368393B523343C136872F4D89C42EED970"),
            "montage": (21894, "4108863BB722C4FFE7F1FD1AF0551B2F76109C4098115D358BAAC1BE99732AAA"),
        },
        "Putt": {
            "sequence": (308731, "18BA8FDB17A8592D54E87B6C6515D63C7E36A79E31FA2FAB8FC9A072044E1218"),
            "montage": (21854, "1B40EAB152CFF21B41EF45EC0A205FDD3812B8885BFB176FB8E40924F285F899"),
        },
        "library": (4924, "EE50AA14E8A8C535582B5A67FD7DE363BEAC8D9E6F1D4E24B736C26CAF712F8D"),
    },
    "v6": {
        "Drive": {
            "sequence": (479441, "319CBA9E20966A5C6EA82816EC1B65F2E9400611B3E84CCA1B7D276BADD856E4"),
            "montage": (22076, "B0D3831BFD984CED1875C03DD1F97216F25131745EDEE2A417AF2C204E40372E"),
        },
        "Approach": {
            "sequence": (381283, "D96AD9020A5ECBB594A3D0AF9BF3060C3DBBBA66B425EC076D0E3BF7FDBC8211"),
            "montage": (22106, "ED971E2B7801228E6BFB01904B5B4FCD7000A4AD04365652FF6FFA08F563B897"),
        },
        "Putt": {
            "sequence": (276239, "D7F76D263D18EA67641292C70F337AA7C5907FCA001ACECE650688D1F189F5A3"),
            "montage": (22066, "A26218034AB52791C5233DF1AA3840A7825406323EEB7326E3EC1E8147385257"),
        },
        "library": (5136, "76C004ED999922253710B948F624E9CC72FC70DF25A85B7136ADB80819822622"),
    },
}
V6_BLEND_OUT_REPAIR_PATH = (
    "Evidence/Session19/ProductionMotionV006BlendOutRepair.json"
)
V6_BLEND_OUT_REPAIR_TARGETS = [
    {
        "asset_package": (
            "/Game/DiscGolf/Animation/ProductionMotion/Drive/"
            "AM_DG_RHBH_Drive_Procedural_v006"
        ),
        "file": (
            "Content/DiscGolf/Animation/ProductionMotion/Drive/"
            "AM_DG_RHBH_Drive_Procedural_v006.uasset"
        ),
        "changed": True,
        "identity_before": {
            "bytes": 22076,
            "sha256": "B0D3831BFD984CED1875C03DD1F97216F25131745EDEE2A417AF2C204E40372E",
        },
        "identity_after": {
            "bytes": 22133,
            "sha256": "9220D00C6D82325603065B1F8BAE70F29520190777446FF351A6D3C5C472B759",
        },
    },
    {
        "asset_package": (
            "/Game/DiscGolf/Animation/ProductionMotion/Approach/"
            "AM_DG_RHBH_Approach_Procedural_v006"
        ),
        "file": (
            "Content/DiscGolf/Animation/ProductionMotion/Approach/"
            "AM_DG_RHBH_Approach_Procedural_v006.uasset"
        ),
        "changed": True,
        "identity_before": {
            "bytes": 22106,
            "sha256": "ED971E2B7801228E6BFB01904B5B4FCD7000A4AD04365652FF6FFA08F563B897",
        },
        "identity_after": {
            "bytes": 22163,
            "sha256": "F10D9B1572AFC4AC5613EBF179299EDC41915F2605A36E639B2863298713D985",
        },
    },
    {
        "asset_package": (
            "/Game/DiscGolf/Animation/ProductionMotion/Putt/"
            "AM_DG_RHBH_Putt_Procedural_v006"
        ),
        "file": (
            "Content/DiscGolf/Animation/ProductionMotion/Putt/"
            "AM_DG_RHBH_Putt_Procedural_v006.uasset"
        ),
        "changed": True,
        "identity_before": {
            "bytes": 22066,
            "sha256": "A26218034AB52791C5233DF1AA3840A7825406323EEB7326E3EC1E8147385257",
        },
        "identity_after": {
            "bytes": 22123,
            "sha256": "10FA1BD79CA5CCFEF0301336754642B8E19574E55C59F61CB3DF0C7D431FA8FA",
        },
    },
]
VERSIONED_POWER_RANGES = {
    "v2": {"Drive": (0.62, 1.0), "Approach": (0.25, 0.72), "Putt": (0.05, 0.45)},
    "v3": {"Drive": (0.62, 1.0), "Approach": (0.25, 0.72), "Putt": (0.05, 0.45)},
    "v4": {"Drive": (0.62, 1.0), "Approach": (0.25, 0.72), "Putt": (0.05, 0.45)},
    "v6": {"Drive": (0.62, 1.0), "Approach": (0.25, 0.72), "Putt": (0.05, 0.45)},
}


def build_family_specs(version: str) -> list[dict[str, Any]]:
    if version == "v1":
        return copy.deepcopy(FAMILY_SPECS)
    revision = {
        "v2": "v002", "v3": "v003", "v4": "v004",
        "v6": "v006",
    }[version]
    numeric_version = {"v2": 2, "v3": 3, "v4": 4, "v6": 6}[version]
    result = copy.deepcopy(FAMILY_SPECS)
    for family in result:
        name = family["family"]
        family["id"] = f"DG_PROCEDURAL_{name.upper()}_RHBH_V{numeric_version}"
        prefix = f"A_DG_RHBH_{name}_Procedural_{revision}"
        montage_prefix = f"AM_DG_RHBH_{name}_Procedural_{revision}"
        family_root = f"{TARGET_ROOT}/{name}"
        family["sequenceObjectPath"] = f"{family_root}/{prefix}.{prefix}"
        family["montageObjectPath"] = (
            f"{family_root}/{montage_prefix}.{montage_prefix}"
        )
        family["recommendedPowerMin"], family["recommendedPowerMax"] = (
            VERSIONED_POWER_RANGES[version][name]
        )
        sequence_bytes, sequence_sha = (
            VERSIONED_ASSET_IDENTITIES[version][name]["sequence"]
        )
        montage_bytes, montage_sha = (
            VERSIONED_ASSET_IDENTITIES[version][name]["montage"]
        )
        family["sequenceFile"] = {
            "path": (
                f"Content/DiscGolf/Animation/ProductionMotion/{name}/"
                f"{prefix}.uasset"
            ),
            "bytes": sequence_bytes,
            "sha256": sequence_sha,
        }
        family["montageFile"] = {
            "path": (
                f"Content/DiscGolf/Animation/ProductionMotion/{name}/"
                f"{montage_prefix}.uasset"
            ),
            "bytes": montage_bytes,
            "sha256": montage_sha,
        }
    return result


def build_production_library(version: str) -> dict[str, Any]:
    if version == "v1":
        object_name = "DA_DG_ProductionMotionLibrary"
        file_identity_value = {
            "bytes": 4068,
            "sha256": "AAD0157BC480B5C572FFF3D5022AFBA9E8A4824DB5A2CF5287F0330FCCC23CD9",
        }
    else:
        revision = {
            "v2": "v002", "v3": "v003", "v4": "v004",
            "v6": "v006",
        }[version]
        object_name = f"DA_DG_ProductionMotionLibrary_{revision}"
        library_bytes, library_sha = VERSIONED_ASSET_IDENTITIES[version]["library"]
        file_identity_value = {"bytes": library_bytes, "sha256": library_sha}
    return {
        "objectPath": f"{TARGET_ROOT}/{object_name}.{object_name}",
        "assetState": "AUTHORED_PROCEDURAL_CANDIDATE_HUMAN_REVIEW_REQUIRED",
        "requiredEntryCount": 3,
        "defaultEntryAllowedBeforeAllValidationAndHumanApproval": False,
        "file": {
            "path": (
                "Content/DiscGolf/Animation/ProductionMotion/"
                f"{object_name}.uasset"
            ),
            **file_identity_value,
        },
    }


VERSION_POLICY_SPECS = {
    "v1": {
        "revision": "v001",
        "recipe": "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v1.json",
        "evidence": "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence.json",
        "required_curves": REQUIRED_CURVES,
        "families": build_family_specs("v1"),
        "library": build_production_library("v1"),
    },
    "v2": {
        "revision": "v002",
        "recipe": "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v2.json",
        "evidence": "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v002.json",
        "required_curves": REQUIRED_CURVES,
        "families": build_family_specs("v2"),
        "library": build_production_library("v2"),
    },
    "v3": {
        "revision": "v003",
        "recipe": "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v3.json",
        "evidence": "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v003.json",
        "required_curves": V3_REQUIRED_CURVES,
        "families": build_family_specs("v3"),
        "library": build_production_library("v3"),
    },
    "v4": {
        "revision": "v004",
        "recipe": "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v4.json",
        "evidence": "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v004.json",
        "required_curves": V3_REQUIRED_CURVES,
        "families": build_family_specs("v4"),
        "library": build_production_library("v4"),
    },
    "v6": {
        "revision": "v006",
        "recipe": "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json",
        "evidence": "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v006.json",
        "required_curves": V3_REQUIRED_CURVES,
        "families": build_family_specs("v6"),
        "library": build_production_library("v6"),
    },
}
SUMMARY = {
    "requiredMotionFamilyCount": 3,
    "authoredMotionFamilyCount": 3,
    "requiredAnimationSequenceCount": 3,
    "authoredAnimationSequenceCount": 3,
    "requiredMontageCount": 3,
    "authoredMontageCount": 3,
    "requiredProductionLibraryCount": 1,
    "authoredProductionLibraryCount": 1,
    "requiredSemanticCoverageCount": 8,
    "sourceControlledTechnicalContractComplete": True,
    "protectedSyntheticAssetReuseCount": 0,
    "runtimeBindingComplete": True,
    "shippingCookBindingComplete": True,
    "humanAnimationApproval": False,
    "humanDiscContactApproval": False,
    "releaseReady": False,
}
CLAIMS = {
    "technicalAuthoringContractAccepted": True,
    "protectedSyntheticAssetsRemainDoNotShip": True,
    "distinctProjectAuthoredMotionAssetsExist": True,
    "runtimeBindingAccepted": True,
    "shippingCookPresenceAccepted": False,
    "motionQualityAccepted": False,
    "discContactQualityAccepted": False,
    "performerReleaseAccepted": False,
    "legalApproval": False,
    "ownerApproval": False,
    "releaseApproval": False,
    "releaseReady": False,
}
RESIDUAL_BLOCKERS = [
    "PROFILE_MATRIX_META_HUMAN_RETARGET_AND_SINGLE_RELEASE_LIVE_VALIDATION_PENDING",
    "HUMAN_ANIMATION_DISC_CONTACT_AND_PRODUCT_ART_APPROVAL_PENDING",
    "FRESH_SHIPPING_CANDIDATE_WITH_PRODUCTION_MOTION_IDENTITIES_PENDING",
]


class StrictJsonError(ValueError):
    pass


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise StrictJsonError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _constant(value: str) -> None:
    raise StrictJsonError(f"non-finite JSON constant: {value}")


def _validate_numbers(value: Any, label: str = "root") -> None:
    if type(value) is int and not -(2**63) <= value <= 2**63 - 1:
        raise StrictJsonError(f"integer outside signed 64-bit range at {label}")
    if type(value) is float and not (-1.7976931348623157e308 <= value <= 1.7976931348623157e308):
        raise StrictJsonError(f"non-finite number at {label}")
    if type(value) is dict:
        for key, item in value.items():
            _validate_numbers(item, f"{label}.{key}")
    elif type(value) is list:
        for index, item in enumerate(value):
            _validate_numbers(item, f"{label}[{index}]")


def load_json(path: Path) -> Any:
    try:
        value = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_pairs,
            parse_constant=_constant,
        )
        _validate_numbers(value)
        return value
    except (OSError, UnicodeError, json.JSONDecodeError, StrictJsonError) as exc:
        raise StrictJsonError(f"{path}: {exc}") from exc


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def exact_keys(value: Any, keys: set[str], label: str, issues: list[str]) -> bool:
    if type(value) is not dict:
        issues.append(f"{label} must be an object")
        return False
    actual = set(value)
    if actual != keys:
        issues.append(
            f"{label} keys differ: missing={sorted(keys-actual)} extra={sorted(actual-keys)}"
        )
        return False
    return True


def expect(actual: Any, expected: Any, label: str, issues: list[str]) -> None:
    if type(actual) is not type(expected) or actual != expected:
        issues.append(f"{label} differs: expected {expected!r}, got {actual!r}")


def safe_project_path(value: Any, root: Path, label: str, issues: list[str]) -> Path | None:
    if type(value) is not str or not value:
        issues.append(f"{label} must be a non-empty project-relative path")
        return None
    pure = PurePosixPath(value)
    if pure.is_absolute() or ".." in pure.parts or "\\" in value or ":" in value:
        issues.append(f"{label} is unsafe: {value!r}")
        return None
    path = (root / value).resolve()
    try:
        path.relative_to(root)
    except ValueError:
        issues.append(f"{label} escapes the project root")
        return None
    return path


def verify_binding(
    binding: Any, root: Path, label: str, issues: list[str], *, check_file: bool
) -> Path | None:
    if not exact_keys(binding, {"path", "bytes", "sha256"}, label, issues):
        return None
    path = safe_project_path(binding["path"], root, f"{label}.path", issues)
    if type(binding["bytes"]) is not int or type(binding["bytes"]) is bool or binding["bytes"] < 1:
        issues.append(f"{label}.bytes must be a positive integer")
    if type(binding["sha256"]) is not str or not SHA_RE.fullmatch(binding["sha256"]):
        issues.append(f"{label}.sha256 must be uppercase SHA-256")
    if check_file and path is not None:
        if not path.is_file():
            issues.append(f"{label} is missing: {binding['path']}")
        elif path.stat().st_size != binding["bytes"] or sha256(path) != binding["sha256"]:
            issues.append(f"{label} identity differs")
    return path


def object_to_file(root: Path, object_path: str) -> Path:
    package, separator, _object = object_path.rpartition(".")
    if not separator or not package.startswith("/Game/"):
        raise ValueError(f"invalid project object path: {object_path}")
    return root / "Content" / f"{package.removeprefix('/Game/').replace('/', os.sep)}.uasset"


def validate_v6_blend_out_repair(
    receipt: Any, root: Path, *, check_files: bool,
) -> tuple[list[str], dict[str, Any]]:
    """Validate the additive v006 montage repair without rewriting authoring truth."""
    issues: list[str] = []
    observations: dict[str, Any] = {}
    root_keys = {
        "schema", "schema_version", "status", "generated_utc",
        "recipe_version", "asset_revision", "authority",
        "historical_authoring_evidence", "repair_script",
        "source_commandlet_report", "operation", "targets",
        "native_validation", "unchanged_contract", "claim_boundary",
    }
    if not exact_keys(receipt, root_keys, "v006 blend-out repair", issues):
        return issues, observations
    for key, expected in {
        "schema": "DiscGolfTour.Session19ProductionMotionV006BlendOutRepair.v1",
        "schema_version": 1,
        "status": "PASS_ADDITIVE_V006_MONTAGE_BLEND_OUT_REPAIR",
        "generated_utc": "2026-08-30T03:41:30.635976+00:00",
        "recipe_version": "v6",
        "asset_revision": "v006",
        "authority": "TECHNICAL_REPAIR_ONLY_NO_HUMAN_OR_RELEASE_APPROVAL",
    }.items():
        expect(receipt[key], expected, f"v006 blend-out repair.{key}", issues)

    expected_authoring_evidence = {
        "path": "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v006.json",
        "bytes": 44023,
        "sha256": "A6BC2C3BA4B37718C4CD64004DD3123708939EA43519CB61CD8006EC5AE06DA6",
    }
    expect(
        receipt["historical_authoring_evidence"], expected_authoring_evidence,
        "v006 blend-out repair historical authoring evidence", issues,
    )
    verify_binding(
        receipt["historical_authoring_evidence"], root,
        "v006 blend-out repair historical authoring evidence", issues,
        check_file=check_files,
    )
    expected_repair_script = {
        "path": "Scripts/repair_dg_session19_production_motion_v6_blend_out.py",
        "bytes": 9102,
        "sha256": "951071F08BC360053AF9CF8F6C44E11238B2F1379E4F235F40706E4AC7032CB1",
    }
    expect(
        receipt["repair_script"], expected_repair_script,
        "v006 blend-out repair script", issues,
    )
    verify_binding(
        receipt["repair_script"], root, "v006 blend-out repair script", issues,
        check_file=check_files,
    )
    expect(receipt["source_commandlet_report"], {
        "run_id": "8eea9f24-0bc9-45d8-b0bd-5a41acee5b8b",
        "external_relative_path": "Saved/ProductionMotion/V006BlendOutRepair.json",
        "bytes": 21064,
        "sha256": "39BEDA98E902CD3F2D446C1EB26E4353995246E2FDCA744D695717CEB3512F4A",
    }, "v006 blend-out repair source commandlet report", issues)
    expect(receipt["operation"], {
        "type": "SET_ANIM_MONTAGE_BLEND_OUT_TRIGGER_TIME",
        "property": "BlendOutTriggerTime",
        "legacy_value_seconds": -1.0,
        "requested_value_seconds": 0.1,
        "observed_value_seconds": 0.10000000149011612,
        "auto_blend_out_required": True,
        "target_scope": "V006_PRODUCTION_MONTAGES_ONLY",
        "target_count": 3,
    }, "v006 blend-out repair operation", issues)
    expect(
        receipt["targets"], V6_BLEND_OUT_REPAIR_TARGETS,
        "v006 blend-out repair exact target identities", issues,
    )
    expect(receipt["native_validation"], {
        "validator": (
            "DiscGolfProductionMotionAuthoringUtility."
            "validate_production_motion_assets_for_version"
        ),
        "status": "PASS_PROJECT_AUTHORED_PROCEDURAL_CANDIDATES",
        "schema": "DiscGolfTour.Session19ProductionMotionNativeReport.v6",
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V6",
        "recipe_md5": "0DA722DE95232CB0375373DC453138EB",
        "recipe_version": "v6",
        "asset_revision": "v006",
        "family_count": 3,
        "sequence_count": 3,
        "montage_count": 3,
        "library_entry_count": 3,
        "human_animation_approval": False,
        "human_disc_contact_approval": False,
        "release_ready": False,
    }, "v006 blend-out repair native validation", issues)
    expect(receipt["unchanged_contract"], {
        "original_authoring_report_rewritten": False,
        "recipe_rewritten": False,
        "sequences_changed": False,
        "production_library_changed": False,
        "release_notifies_changed": False,
        "gameplay_physics_changed": False,
        "maps_or_save_data_changed": False,
    }, "v006 blend-out repair unchanged contract", issues)
    expect(receipt["claim_boundary"], {
        "recovery_checkpoint_full_weight_contract": True,
        "human_animation_approval": False,
        "human_disc_contact_approval": False,
        "release_ready": False,
    }, "v006 blend-out repair claim boundary", issues)

    if check_files:
        for index, row in enumerate(V6_BLEND_OUT_REPAIR_TARGETS):
            path = safe_project_path(
                row["file"], root, f"v006 blend-out repair target[{index}]", issues,
            )
            if path is None:
                continue
            after = row["identity_after"]
            if not path.is_file():
                issues.append(f"v006 repaired montage is missing: {row['file']}")
            elif (
                path.stat().st_size != after["bytes"]
                or sha256(path) != after["sha256"]
            ):
                issues.append(f"v006 repaired montage identity differs: {row['file']}")
    observations.update({
        "status": receipt["status"],
        "target_count": len(receipt["targets"]) if type(receipt["targets"]) is list else 0,
        "trigger_seconds": receipt["operation"].get("requested_value_seconds")
        if type(receipt["operation"]) is dict else None,
        "native_validation": receipt["native_validation"].get("status")
        if type(receipt["native_validation"]) is dict else None,
    })
    return issues, observations


def validate_v6_blend_out_repair_path(
    root: Path, *, check_files: bool,
) -> tuple[list[str], dict[str, Any]]:
    path = (root / V6_BLEND_OUT_REPAIR_PATH).resolve()
    try:
        path.relative_to(root)
    except ValueError:
        return ["v006 blend-out repair path escapes project root"], {}
    if not path.is_file() or path.is_symlink():
        return [f"v006 blend-out repair receipt is missing or irregular: {path}"], {}
    try:
        receipt = load_json(path)
    except StrictJsonError as exc:
        return [f"v006 blend-out repair receipt is invalid: {exc}"], {}
    return validate_v6_blend_out_repair(receipt, root, check_files=check_files)


def infer_motion_version(policy: dict[str, Any], issues: list[str]) -> str | None:
    bindings = policy.get("bindings")
    recipe_binding = (
        bindings.get("productionMotionRecipe")
        if type(bindings) is dict else None
    )
    recipe_path = (
        recipe_binding.get("path") if type(recipe_binding) is dict else None
    )
    matches = [
        version for version, spec in VERSION_POLICY_SPECS.items()
        if recipe_path == spec["recipe"]
    ]
    if len(matches) != 1:
        issues.append(
            "policy.bindings.productionMotionRecipe.path does not select exactly "
            "one supported append-only motion revision"
        )
        return None
    version = matches[0]
    evidence_binding = bindings.get("productionMotionCandidateAuthoringEvidence")
    evidence_path = (
        evidence_binding.get("path")
        if type(evidence_binding) is dict else None
    )
    if evidence_path != VERSION_POLICY_SPECS[version]["evidence"]:
        issues.append(
            "policy production-motion recipe/evidence revisions differ: "
            f"expected {VERSION_POLICY_SPECS[version]['evidence']!r}, "
            f"got {evidence_path!r}"
        )
    return version


def validate_policy(
    policy: Any, root: Path, *, check_files: bool,
    expected_candidate_id: str | None = None,
) -> tuple[list[str], dict[str, Any]]:
    issues: list[str] = []
    observations: dict[str, Any] = {}
    root_keys = {
        "schema", "schemaVersion", "session", "milestone", "authority", "state",
        "bindings", "protectedSyntheticAssets", "authoredSourcePolicy",
        "technicalContract", "motionFamilies", "productionLibrary",
        "guardedAuthoringContract", "runtimeAndCookGate", "summary",
        "residualBlockers", "claimBoundary",
    }
    if not exact_keys(policy, root_keys, "policy", issues):
        return issues, observations
    for key, expected in {
        "schema": "DiscGolfTour.Session19ProductionMotionAuthoringPolicy.v2",
        "schemaVersion": 2,
        "session": 19,
        "milestone": "v0.5",
        "authority": "FAIL_CLOSED_PROJECT_AUTHORED_PROCEDURAL_MOTION_CANDIDATES_NOT_VISUAL_PERFORMANCE_OWNER_OR_RELEASE_APPROVAL",
        "state": "TECHNICAL_PROCEDURAL_CANDIDATES_AUTHORED_RUNTIME_AND_COOK_BOUND_HUMAN_REVIEW_AND_FRESH_CANDIDATE_PENDING",
    }.items():
        expect(policy[key], expected, f"policy.{key}", issues)

    paths: dict[str, Path] = {}
    if exact_keys(policy["bindings"], BINDING_KEYS, "policy.bindings", issues):
        for key, binding in policy["bindings"].items():
            path = verify_binding(
                binding, root, f"policy.bindings.{key}", issues, check_file=check_files
            )
            if path is not None:
                paths[key] = path
    motion_version = infer_motion_version(policy, issues)
    version_spec = VERSION_POLICY_SPECS[motion_version or "v1"]
    family_specs = version_spec["families"]
    production_library = version_spec["library"]
    if motion_version == "v6":
        repair_issues, repair_observations = validate_v6_blend_out_repair_path(
            root, check_files=check_files,
        )
        issues.extend(repair_issues)
        observations["v006BlendOutRepair"] = repair_observations
    candidate_content_binding = (
        policy.get("bindings", {}).get("candidateContentAudit")
        if type(policy.get("bindings")) is dict else None
    )
    candidate_content_token = (
        candidate_content_binding.get("path")
        if type(candidate_content_binding) is dict else None
    )
    candidate_match = (
        CANDIDATE_CONTENT_RE.fullmatch(candidate_content_token)
        if type(candidate_content_token) is str else None
    )
    bound_candidate_id = candidate_match.group(1) if candidate_match else None
    if bound_candidate_id is None:
        issues.append(
            "policy.bindings.candidateContentAudit.path is not a candidate-scoped "
            "Session 19 content receipt"
        )
    if (
        expected_candidate_id is not None
        and (
            CANDIDATE_RE.fullmatch(expected_candidate_id) is None
            or bound_candidate_id != expected_candidate_id
        )
    ):
        issues.append("selected production-motion policy candidate differs")

    protected = policy["protectedSyntheticAssets"]
    if type(protected) is not list or len(protected) != 5:
        issues.append("protectedSyntheticAssets must contain exactly five rows")
        protected = []
    protected_paths: set[str] = set()
    for index, row in enumerate(protected):
        label = f"protectedSyntheticAssets[{index}]"
        if not exact_keys(row, {
            "objectPath", "file", "classification", "usageStatus",
            "mayBeDuplicatedOrRelabeledForProduction",
        }, label, issues):
            continue
        if type(row["objectPath"]) is not str or not any(
            row["objectPath"].startswith(root_name + "/") for root_name in SYNTHETIC_ROOTS
        ):
            issues.append(f"{label}.objectPath is outside a protected synthetic root")
        protected_paths.add(row["objectPath"])
        expect(row["usageStatus"], "DO_NOT_SHIP", f"{label}.usageStatus", issues)
        expect(row["mayBeDuplicatedOrRelabeledForProduction"], False,
               f"{label}.reuse", issues)
        verify_binding(row["file"], root, f"{label}.file", issues, check_file=check_files)

    expect(policy["authoredSourcePolicy"], {
        "plannedSourceKind": "PROJECT_AUTHORED_PROCEDURAL",
        "plannedCreator": "DiscGolfTour project",
        "performer": "NOT_APPLICABLE_PROCEDURAL_NO_CAPTURE",
        "motionCaptureClaim": False,
        "externalPerformanceClaim": False,
        "plannedRightsStatus": "PROJECT_AUTHORED_SOURCE_PENDING_HUMAN_ART_AND_FINAL_PRODUCT_REVIEW",
        "plannedUsageStatus": "TECHNICAL_SHIPPING_ELIGIBLE_PENDING_HUMAN_ART_APPROVAL",
        "derivationFromProtectedSyntheticAssetsAllowed": False,
        "externalSourceAcquisitionRequired": False,
        "humanAnimationApprovalRequired": True,
        "humanDiscContactApprovalRequired": True,
    }, "authored source policy", issues)

    expect(policy["technicalContract"], {
        "targetRoot": TARGET_ROOT,
        "targetSkeleton": "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master",
        "targetControlRig": "/Game/DiscGolf/Rigs/CR_DG_Master.CR_DG_Master",
        "metaHumanRetargeter": "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.RTG_DGMaster_To_MetaHuman",
        "frameRate": 60,
        "rootMotionEnabled": False,
        "gameplayPawnOwnsWorldMotion": True,
        "animationOwnsGameplayRelease": False,
        "requiredPhaseNotifies": REQUIRED_PHASES,
        "requiredBranchingPointNotifies": ["DG Release Disc", "DG Throw Finished"],
        "requiredNotifyCardinality": {"DG Release Disc": 1, "DG Throw Finished": 1},
        "requiredCurves": version_spec["required_curves"],
        "requiredProfileMatrix": ["ShortCompact", "Baseline", "TallLongArms"],
        "requiredValidation": [
            "FINITE_BONE_TRANSFORMS", "BOUNDED_ROOT_DRIFT", "ROOT_MOTION_DISABLED",
            "THROWING_HAND_DISC_GRIP_ALIGNMENT", "PLANT_FOOT_TRANSLATION_AND_ROTATION",
            "BRACE_CONTINUITY", "SHOULDER_ELBOW_KNEE_BEND_PLAUSIBILITY",
            "FOLLOW_THROUGH_RECOVERY_CONTINUITY",
            "EXACT_RELEASE_AND_FINISH_NOTIFY_CARDINALITY",
            "SINGLE_AUTHORITATIVE_GAMEPLAY_DISC",
            "META_HUMAN_RETARGET_OUTPUT_FINITE_AND_BOUNDED",
        ],
    }, "technical contract", issues)
    expect(policy["motionFamilies"], family_specs, "motion families", issues)
    expect(policy["productionLibrary"], production_library,
           "production library", issues)
    authoring = policy["guardedAuthoringContract"]
    expect(authoring, {
        "requiredNewNativeUtilityFiles": [
            "Source/DiscGolfTourEditor/DiscGolfProductionMotionAuthoringUtility.h",
            "Source/DiscGolfTourEditor/DiscGolfProductionMotionAuthoringUtility.cpp",
        ],
        "requiredNewCommandletWrapper": "Scripts/author_dg_session19_production_motion.py",
        "requiredOriginalRecipe": version_spec["recipe"],
        "currentRequiredAuthoringFileCount": 4,
        "currentRequiredAuthoringFilesPresent": 4,
        "requiredSwitch": "-DGAuthorProductionProceduralMotion",
        "requiredExternalUserDirRoot": "C:/DGTour_TestRuns/Session19ProductionMotion",
        "requiresUnattended": True,
        "requiresNoP4": True,
        "refuseExistingTargetAssets": True,
        "rollbackPartialWrites": True,
        "protectSaveGameSha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
        "copyOrDuplicateProtectedSyntheticAssetAllowed": False,
        "authoringFeasibleNow": True,
        "authoringBlockedReason": "",
    }, "guarded authoring contract", issues)
    expect(policy["runtimeAndCookGate"], {
        "shippingProductionMotionBindingActive": True,
        "shippingFallback": "SYNCHRONOUS_GAMEPLAY_RELEASE_IF_CANDIDATE_LOAD_OR_PLAY_FAILS",
        "developmentSyntheticMontageCompiledOutOfShipping": True,
        "targetRootAlwaysCookConfigured": True,
        "protectedSyntheticRootsNeverCookConfigured": True,
        "candidateProductionMotionNamedIdentityCount": 7,
        "newShippingCandidateRequiredAfterMotionIntegration": True,
    }, "runtime and cook gate", issues)
    expect(policy["summary"], SUMMARY, "summary", issues)
    expect(policy["residualBlockers"], RESIDUAL_BLOCKERS, "residual blockers", issues)
    expect(policy["claimBoundary"], CLAIMS, "claim boundary", issues)

    target_object_paths = [
        item[key] for item in family_specs for key in ("sequenceObjectPath", "montageObjectPath")
    ] + [policy["productionLibrary"]["objectPath"]]
    expect(len(set(target_object_paths)), 7, "unique target object path count", issues)
    for object_path in target_object_paths:
        if object_path in protected_paths or not object_path.startswith(TARGET_ROOT + "/"):
            issues.append(f"target path overlaps or escapes the production-motion root: {object_path}")
        if check_files:
            try:
                disk_path = object_to_file(root, object_path)
            except ValueError as exc:
                issues.append(str(exc))
            else:
                if not disk_path.is_file():
                    issues.append(f"authored target asset is missing: {disk_path}")

    for index, family in enumerate(policy["motionFamilies"]):
        for kind in ("sequence", "montage"):
            file_key = f"{kind}File"
            object_key = f"{kind}ObjectPath"
            is_additively_repaired_v6_montage = (
                motion_version == "v6" and kind == "montage"
            )
            bound_path = verify_binding(
                family[file_key], root,
                f"motionFamilies[{index}].{file_key}", issues,
                check_file=check_files and not is_additively_repaired_v6_montage,
            )
            if bound_path is not None:
                try:
                    expected_path = object_to_file(root, family[object_key]).resolve()
                except ValueError as exc:
                    issues.append(str(exc))
                else:
                    if bound_path.resolve() != expected_path:
                        issues.append(
                            f"motionFamilies[{index}].{file_key} does not match "
                            f"{object_key}"
                        )
                if is_additively_repaired_v6_montage and check_files:
                    repair_row = next(
                        (
                            row for row in V6_BLEND_OUT_REPAIR_TARGETS
                            if row["file"] == family[file_key]["path"]
                        ),
                        None,
                    )
                    if repair_row is None:
                        issues.append(
                            f"motionFamilies[{index}].{file_key} lacks exact v006 "
                            "repair provenance"
                        )
                    else:
                        after = repair_row["identity_after"]
                        if (
                            not bound_path.is_file()
                            or bound_path.stat().st_size != after["bytes"]
                            or sha256(bound_path) != after["sha256"]
                        ):
                            issues.append(
                                f"motionFamilies[{index}].{file_key} repaired "
                                "identity differs"
                            )
    library_path = verify_binding(
        policy["productionLibrary"]["file"], root,
        "productionLibrary.file", issues, check_file=check_files,
    )
    if library_path is not None:
        try:
            expected_library_path = object_to_file(
                root, policy["productionLibrary"]["objectPath"]
            ).resolve()
        except ValueError as exc:
            issues.append(str(exc))
        else:
            if library_path.resolve() != expected_library_path:
                issues.append(
                    "productionLibrary.file does not match productionLibrary.objectPath"
                )

    required_authoring_paths = [
        *authoring["requiredNewNativeUtilityFiles"],
        authoring["requiredNewCommandletWrapper"], authoring["requiredOriginalRecipe"],
    ]
    present_authoring = 0
    for relative in required_authoring_paths:
        candidate = safe_project_path(relative, root, "required authoring path", issues)
        if candidate is not None and candidate.exists():
            present_authoring += 1
    expect(present_authoring, 4, "present authoring prerequisite count", issues)

    if check_files and not issues:
        authoring_evidence = load_json(
            paths["productionMotionCandidateAuthoringEvidence"]
        )
        numeric_version = int((motion_version or "v1").removeprefix("v"))
        expect(
            authoring_evidence.get("schema"),
            (
                "DiscGolfTour.Session19ProductionMotionCandidateAuthoringEvidence."
                f"v{numeric_version}"
            ),
            "production-motion authoring evidence schema", issues,
        )
        expect(
            authoring_evidence.get("schema_version"), numeric_version,
            "production-motion authoring evidence schema version", issues,
        )
        expect(
            authoring_evidence.get("recipe", {}).get("path"),
            version_spec["recipe"],
            "production-motion authoring evidence recipe", issues,
        )
        if motion_version in {"v2", "v3", "v4", "v5", "v6"}:
            expect(
                authoring_evidence.get("recipe_version"), motion_version,
                "production-motion authoring evidence recipe version", issues,
            )
            expect(
                authoring_evidence.get("asset_revision"), version_spec["revision"],
                "production-motion authoring evidence asset revision", issues,
            )
        expect(
            authoring_evidence.get("target_objects"), target_object_paths,
            "production-motion authoring evidence target objects", issues,
        )
        expect(
            authoring_evidence.get("human_animation_approval"), False,
            "production-motion authoring evidence human animation approval", issues,
        )
        expect(
            authoring_evidence.get("human_disc_contact_approval"), False,
            "production-motion authoring evidence human contact approval", issues,
        )
        expect(
            authoring_evidence.get("release_ready"), False,
            "production-motion authoring evidence release readiness", issues,
        )
        evidence_assets = {
            row.get("object_path"): row
            for row in authoring_evidence.get("authored_assets", [])
            if type(row) is dict
        }
        for object_path, file_spec in [
            *(
                (family[object_key], family[file_key])
                for family in family_specs
                for object_key, file_key in (
                    ("sequenceObjectPath", "sequenceFile"),
                    ("montageObjectPath", "montageFile"),
                )
            ),
            (production_library["objectPath"], production_library["file"]),
        ]:
            row = evidence_assets.get(object_path, {})
            expect(row.get("file"), file_spec["path"],
                   f"authoring evidence asset {object_path} path", issues)
            expect(row.get("bytes"), file_spec["bytes"],
                   f"authoring evidence asset {object_path} bytes", issues)
            expect(row.get("sha256"), file_spec["sha256"],
                   f"authoring evidence asset {object_path} SHA-256", issues)

        registry = load_json(paths["frozenMotionRegistry"])
        sources = registry.get("sources", [])
        synthetic = [
            item for item in sources
            if type(item) is dict and item.get("source_kind") == "SYNTHETIC_TEST"
        ]
        expect(len(synthetic), 2, "frozen synthetic registry row count", issues)
        for item in synthetic:
            expect(item.get("usage_status"), "DO_NOT_SHIP",
                   f"registry source {item.get('source_id')} usage", issues)
            expect(item.get("production_status"), "SYNTHETIC_TEST_DO_NOT_SHIP",
                   f"registry source {item.get('source_id')} production status", issues)
            expect(item.get("external_production_claim"), False,
                   f"registry source {item.get('source_id')} external claim", issues)
        if any(item.get("usage_status") == "CLEARED_FOR_PRODUCTION" for item in sources if type(item) is dict):
            issues.append("frozen motion registry unexpectedly claims a production-cleared source")

        pawn = paths["golferPawnSource"].read_text(encoding="utf-8")
        for token in [
            '#include "DiscGolfProductionMotion.h"',
            "#if DG_WITH_DEVELOPMENT_CONTENT",
            "IsPipelineRuntimeValidationRequested()",
            "ProductionDriveMontage = LoadObject<UAnimMontage>(",
            "ProductionApproachMontage = LoadObject<UAnimMontage>(",
            "ProductionPuttMontage = LoadObject<UAnimMontage>(",
            "PresentationComponent->GetAnimationFamily()",
            "RHBHThrowMontage = ProductionDriveMontage;",
            "ResolveRHBHThrowMontage(",
            "Entry.MotionFamilyId != ExpectedFamilyId",
            "FallbackMontage = ProductionApproachMontage;",
            "FallbackMontage = ProductionPuttMontage;",
        ]:
            if token not in pawn:
                issues.append(f"golfer pawn shipping/development motion seam missing: {token}")
        if not re.search(
            r"if\s*\(\s*!GM->RequestThrow\(Command\)\s*\)\s*\{\s*"
            r"CancelThrowPresentation\(\);\s*\}",
            pawn,
        ):
            issues.append(
                "golfer pawn fail-closed synchronous release seam missing"
            )
        if "FObjectFinder<UAnimMontage>" in pawn:
            issues.append("golfer pawn must not eagerly hard-find production montages on its CDO")

        runtime_paths = paths["productionMotionRuntimePaths"].read_text(encoding="utf-8")
        for object_path in target_object_paths:
            if object_path not in runtime_paths:
                issues.append(
                    f"production-motion runtime path header is missing: {object_path}"
                )

        game_ini = paths["packagingSettings"].read_text(encoding="utf-8")
        for synthetic_root in SYNTHETIC_ROOTS:
            expected = f'+DirectoriesToNeverCook=(Path="{synthetic_root}")'
            if expected not in game_ini:
                issues.append(f"protected synthetic never-cook root missing: {synthetic_root}")
        if f'+DirectoriesToAlwaysCook=(Path="{TARGET_ROOT}")' not in game_ini:
            issues.append("target production-motion AlwaysCook root is missing")
        if f'+DirectoriesToNeverCook=(Path="{TARGET_ROOT}")' in game_ini:
            issues.append("target production-motion root must not be a never-cook root")

        content = load_json(paths["candidateContentAudit"])
        expect(
            content.get("schema"),
            "DiscGolfTour.Session19CandidateContentAuditReceipt.v1",
            "candidate content schema", issues,
        )
        expect(content.get("runId"), bound_candidate_id,
               "candidate content run id", issues)
        expect(content.get("state"), "PASS_BOUNDED_STAGED_CONTENT_AUDIT",
               "candidate content state", issues)
        expect(content.get("issues"), [], "candidate content issues", issues)
        dev_category = content.get("forbiddenIdentityCategories", {}).get(
            "DEVELOPMENT_TEST_EDITOR", {}
        )
        expect(dev_category.get("passed"), True,
               "candidate development-content exclusion", issues)
        retained = {
            item.get("package"): item for item in content.get("requiredRetainedRuntimeAssets", [])
            if type(item) is dict
        }
        for package in [
            "/Game/DiscGolf/Animation/ABP_DG_Player",
            "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default",
            "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default",
            "/Game/DiscGolf/Rigs/CR_DG_Master",
        ]:
            row = retained.get(package, {})
            expect(row.get("presentInUfsManifest"), True,
                   f"candidate retained asset {package} UFS", issues)
            expect(row.get("presentInContainerInventory"), True,
                   f"candidate retained asset {package} container", issues)

    observations = {
        "motionRecipeVersion": motion_version,
        "motionAssetRevision": version_spec["revision"],
        "motionFamilyContracts": 3,
        "semanticCoverageCount": 8,
        "targetAssetCount": 7,
        "targetAssetsPresent": 7,
        "protectedSyntheticAssetCount": 5,
        "protectedSyntheticAssetReuseCount": 0,
        "authoringPrerequisiteFilesPresent": present_authoring,
        "shippingProductionMotionBindingActive": True,
        "targetRootAlwaysCookConfigured": True,
        "candidateProductionMotionNamedIdentityCount": 7,
        "freshShippingCandidateIncludesProductionMotion": False,
        "humanAnimationApproval": False,
        "humanDiscContactApproval": False,
        "releaseReady": False,
    }
    return issues, observations


Mutation = tuple[str, Callable[[dict[str, Any]], None]]


def run_self_test(policy: dict[str, Any], root: Path) -> tuple[bool, int, list[str]]:
    failures: list[str] = []
    # Self-test verifies the immutable schema/adversarial contract. Live binding
    # drift belongs to the normal validation path, not to this unit baseline.
    baseline, _ = validate_policy(policy, root, check_files=False)
    if baseline:
        failures.append("baseline failed: " + "; ".join(baseline[:5]))
    for version in ("v2", "v3", "v4", "v6"):
        versioned = copy.deepcopy(policy)
        selection_issues = apply_motion_version_contract(versioned, root, version)
        if selection_issues:
            failures.append(
                f"{version} setup failed: " + "; ".join(selection_issues[:5])
            )
            continue
        versioned_issues, _ = validate_policy(
            versioned, root, check_files=False
        )
        if versioned_issues:
            failures.append(
                f"{version} baseline failed: "
                + "; ".join(versioned_issues[:5])
            )
    mutations: list[Mutation] = [
        ("extra", lambda d: d.__setitem__("releaseApproved", True)),
        ("schema", lambda d: d.__setitem__("schema", "wrong")),
        ("version", lambda d: d.__setitem__("schemaVersion", 1)),
        ("authority", lambda d: d.__setitem__("authority", "PRODUCTION_APPROVED")),
        ("binding-hash", lambda d: d["bindings"]["dgMasterSkeleton"].__setitem__("sha256", "g" * 64)),
        ("synthetic-ship", lambda d: d["protectedSyntheticAssets"][0].__setitem__("usageStatus", "CLEARED_FOR_PRODUCTION")),
        ("synthetic-reuse", lambda d: d["protectedSyntheticAssets"][0].__setitem__("mayBeDuplicatedOrRelabeledForProduction", True)),
        ("performer", lambda d: d["authoredSourcePolicy"].__setitem__("performer", "UNKNOWN PERSON")),
        ("capture-claim", lambda d: d["authoredSourcePolicy"].__setitem__("motionCaptureClaim", True)),
        ("derivation", lambda d: d["authoredSourcePolicy"].__setitem__("derivationFromProtectedSyntheticAssetsAllowed", True)),
        ("animation-authority", lambda d: d["technicalContract"].__setitem__("animationOwnsGameplayRelease", True)),
        ("root-motion", lambda d: d["technicalContract"].__setitem__("rootMotionEnabled", True)),
        ("curve", lambda d: d["technicalContract"]["requiredCurves"].pop()),
        ("recipe-evidence-revision", lambda d: d["bindings"]["productionMotionCandidateAuthoringEvidence"].__setitem__("path", VERSION_POLICY_SPECS["v2"]["evidence"])),
        ("notify", lambda d: d["technicalContract"]["requiredNotifyCardinality"].__setitem__("DG Release Disc", 2)),
        ("family-missing", lambda d: d["motionFamilies"].pop()),
        ("family-target", lambda d: d["motionFamilies"][0].__setitem__("sequenceObjectPath", "/Game/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.A_DG_RHBH_Prototype")),
        ("family-authored", lambda d: d["motionFamilies"][0].__setitem__("assetState", "AUTHORED")),
        ("library-default", lambda d: d["productionLibrary"].__setitem__("defaultEntryAllowedBeforeAllValidationAndHumanApproval", True)),
        ("authoring-feasible", lambda d: d["guardedAuthoringContract"].__setitem__("authoringFeasibleNow", False)),
        ("copy-allowed", lambda d: d["guardedAuthoringContract"].__setitem__("copyOrDuplicateProtectedSyntheticAssetAllowed", True)),
        ("runtime-binding", lambda d: d["runtimeAndCookGate"].__setitem__("shippingProductionMotionBindingActive", False)),
        ("summary-authored", lambda d: d["summary"].__setitem__("authoredMotionFamilyCount", 0)),
        ("summary-release", lambda d: d["summary"].__setitem__("releaseReady", True)),
        ("claim-assets", lambda d: d["claimBoundary"].__setitem__("distinctProjectAuthoredMotionAssetsExist", False)),
        ("claim-shipping-cook", lambda d: d["claimBoundary"].__setitem__("shippingCookPresenceAccepted", True)),
        ("claim-release", lambda d: d["claimBoundary"].__setitem__("releaseReady", True)),
    ]
    for name, mutation in mutations:
        candidate = copy.deepcopy(policy)
        mutation(candidate)
        issues, _ = validate_policy(candidate, root, check_files=False)
        if not issues:
            failures.append(f"mutation survived: {name}")
    strict_cases = [
        ("duplicate", '{"schema":1,"schema":2}'),
        ("nan", '{"value":NaN}'),
        ("overflow", '{"value":1e309}'),
    ]
    for name, payload in strict_cases:
        try:
            value = json.loads(payload, object_pairs_hook=_pairs, parse_constant=_constant)
            _validate_numbers(value)
        except StrictJsonError:
            continue
        failures.append(f"strict JSON case survived: {name}")
    mismatch_issues, _ = validate_policy(
        policy, root, check_files=False,
        expected_candidate_id="S19_WindowsShipping_20990101T000000Z_mismatch",
    )
    if not mismatch_issues:
        failures.append("selected candidate mismatch survived")
    with tempfile.TemporaryDirectory(prefix="dgt-production-motion-output-") as temp_name:
        collision = Path(temp_name) / "receipt.json"
        collision.write_bytes(b"sentinel")
        try:
            write_bytes_exclusive(collision, b"must-not-overwrite")
        except FileExistsError:
            if collision.read_bytes() != b"sentinel":
                failures.append("exclusive output collision changed existing bytes")
        else:
            failures.append("exclusive output collision was accepted")
    unsealed_issues = apply_motion_version_contract(
        copy.deepcopy(policy), root, "v5"
    )
    if not unsealed_issues or "unsealed" not in unsealed_issues[0]:
        failures.append("unsealed v5 release policy was accepted")
    repair_mutations: list[Mutation] = [
        ("repair-extra-root-key", lambda d: d.__setitem__("approval", True)),
        (
            "repair-operation",
            lambda d: d["operation"].__setitem__("type", "REAUTHOR_MONTAGES"),
        ),
        (
            "repair-requested-trigger",
            lambda d: d["operation"].__setitem__("requested_value_seconds", 0.25),
        ),
        (
            "repair-observed-trigger",
            lambda d: d["operation"].__setitem__("observed_value_seconds", 0.25),
        ),
        ("repair-target-missing", lambda d: d["targets"].pop()),
        (
            "repair-target-extra",
            lambda d: d["targets"].append(copy.deepcopy(d["targets"][0])),
        ),
        (
            "repair-target-package",
            lambda d: d["targets"][0].__setitem__("asset_package", "/Game/Other"),
        ),
        (
            "repair-before-identity",
            lambda d: d["targets"][0]["identity_before"].__setitem__(
                "sha256", "0" * 64
            ),
        ),
        (
            "repair-after-identity",
            lambda d: d["targets"][0]["identity_after"].__setitem__("bytes", 1),
        ),
        (
            "repair-native-status",
            lambda d: d["native_validation"].__setitem__("status", "PASS"),
        ),
        (
            "repair-native-revision",
            lambda d: d["native_validation"].__setitem__("asset_revision", "v007"),
        ),
        (
            "repair-native-count",
            lambda d: d["native_validation"].__setitem__("montage_count", 2),
        ),
        (
            "repair-authoring-evidence",
            lambda d: d["historical_authoring_evidence"].__setitem__(
                "sha256", "0" * 64
            ),
        ),
        (
            "repair-script",
            lambda d: d["repair_script"].__setitem__("sha256", "0" * 64),
        ),
        (
            "repair-claims-recipe-change",
            lambda d: d["unchanged_contract"].__setitem__("recipe_rewritten", True),
        ),
        (
            "repair-release-claim",
            lambda d: d["claim_boundary"].__setitem__("release_ready", True),
        ),
    ]
    try:
        repair_receipt = load_json(root / V6_BLEND_OUT_REPAIR_PATH)
    except StrictJsonError as exc:
        failures.append(f"v006 repair self-test receipt load failed: {exc}")
    else:
        repair_baseline, _ = validate_v6_blend_out_repair(
            repair_receipt, root, check_files=False,
        )
        if repair_baseline:
            failures.append(
                "v006 repair self-test baseline failed: "
                + "; ".join(repair_baseline[:5])
            )
        for name, mutation in repair_mutations:
            candidate = copy.deepcopy(repair_receipt)
            mutation(candidate)
            repair_issues, _ = validate_v6_blend_out_repair(
                candidate, root, check_files=False,
            )
            if not repair_issues:
                failures.append(f"v006 repair mutation survived: {name}")
    return (
        not failures,
        len(mutations) + len(strict_cases) + len(repair_mutations) + 4,
        failures,
    )


def file_identity(path: Path, root: Path) -> dict[str, Any]:
    return {
        "path": path.relative_to(root).as_posix(),
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def apply_motion_version_contract(
    policy: dict[str, Any], root: Path, motion_version: str,
) -> list[str]:
    """Select one immutable recipe/assets revision without changing older assets."""
    issues: list[str] = []
    if motion_version == "v5":
        return [
            "v5 production-motion release policy is unsealed: its retained arm-spatial "
            "comparator gates fail; use the corrected immutable v006 candidate"
        ]
    spec = VERSION_POLICY_SPECS.get(motion_version)
    if spec is None:
        return [f"unsupported production-motion policy version: {motion_version!r}"]
    bindings = policy.get("bindings")
    if type(bindings) is not dict or set(bindings) != BINDING_KEYS:
        return ["production-motion policy bindings differ"]
    for key, relative in (
        ("productionMotionRecipe", spec["recipe"]),
        ("productionMotionCandidateAuthoringEvidence", spec["evidence"]),
    ):
        path = safe_project_path(relative, root, f"{motion_version} {key}", issues)
        if path is None:
            continue
        if not path.is_file() or path.is_symlink():
            issues.append(f"{motion_version} {key} is missing or irregular")
            continue
        bindings[key] = file_identity(path, root)
    if issues:
        return issues
    policy["technicalContract"]["requiredCurves"] = copy.deepcopy(
        spec["required_curves"]
    )
    policy["motionFamilies"] = copy.deepcopy(spec["families"])
    policy["productionLibrary"] = copy.deepcopy(spec["library"])
    policy["guardedAuthoringContract"]["requiredOriginalRecipe"] = spec["recipe"]
    return issues


def write_bytes_exclusive(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as handle:
        handle.write(payload)
        handle.flush()
        os.fsync(handle.fileno())


def write_receipt(
    path: Path, policy_path: Path, policy: dict[str, Any],
    observations: dict[str, Any], root: Path,
) -> None:
    receipt = {
        "schema": "DiscGolfTour.Session19ProductionMotionTechnicalCandidateEvidence.v2",
        "schemaVersion": 2,
        "session": 19,
        "milestone": "v0.5",
        "authority": policy["authority"],
        "state": "PASS_TECHNICAL_PROCEDURAL_CANDIDATES_AUTHORED_RUNTIME_AND_COOK_BOUND_HUMAN_REVIEW_AND_FRESH_CANDIDATE_PENDING",
        "policy": file_identity(policy_path, root),
        "bindings": policy["bindings"],
        "observations": observations,
        "residualBlockers": policy["residualBlockers"],
        "claimBoundary": policy["claimBoundary"],
        "issues": [],
    }
    payload = (json.dumps(receipt, indent=2) + "\n").encode("utf-8")
    try:
        write_bytes_exclusive(path, payload)
    except FileExistsError as exc:
        raise FileExistsError(
            f"evidence is immutable and already exists: {path}"
        ) from exc


def generate_candidate_policy(
    root: Path, candidate_id: str, destination: Path,
    motion_version: str = "v6",
) -> tuple[dict[str, Any] | None, list[str]]:
    issues: list[str] = []
    if CANDIDATE_RE.fullmatch(candidate_id or "") is None:
        return None, ["candidate ID is not canonical"]
    if candidate_id == CANDIDATE_ID:
        return None, ["candidate policy generation may not duplicate historical authority"]
    root = root.resolve()
    destination = destination.resolve(strict=False)
    try:
        relative = destination.relative_to(root).as_posix()
    except ValueError:
        return None, ["candidate policy destination must be inside the project root"]
    expected_relative = (
        "Config/DG_Session19ProductionMotionAuthoringPolicy-"
        f"{candidate_id}.json"
    )
    if relative != expected_relative:
        return None, [f"candidate policy destination must be {expected_relative}"]
    if os.path.lexists(destination):
        return None, ["candidate policy destination already exists; refusing overwrite"]
    historical_path = root / "Config/DG_Session19ProductionMotionAuthoringPolicy.json"
    try:
        policy = copy.deepcopy(load_json(historical_path))
    except StrictJsonError as exc:
        return None, [f"historical production-motion policy is invalid: {exc}"]
    bindings = policy.get("bindings")
    if type(bindings) is not dict or set(bindings) != BINDING_KEYS:
        return None, ["historical production-motion policy bindings differ"]
    for key, binding in bindings.items():
        if key == "candidateContentAudit":
            continue
        if type(binding) is not dict or set(binding) != {"path", "bytes", "sha256"}:
            issues.append(f"historical production-motion binding {key} differs")
            continue
        path = safe_project_path(
            binding.get("path"), root,
            f"historical production-motion binding {key}", issues,
        )
        if path is None:
            continue
        if not path.is_file() or path.is_symlink():
            issues.append(
                f"historical production-motion binding {key} is missing or irregular"
            )
            continue
        bindings[key] = file_identity(path, root)
    if issues:
        return None, issues

    issues.extend(apply_motion_version_contract(policy, root, motion_version))
    if issues:
        return None, issues

    candidate_relative = (
        f"Evidence/Session19/CandidateContent-{candidate_id}.json"
    )
    candidate_path = root / candidate_relative
    if not candidate_path.is_file() or candidate_path.is_symlink():
        return None, [f"candidate content receipt is missing or irregular: {candidate_relative}"]
    policy["bindings"]["candidateContentAudit"] = file_identity(candidate_path, root)
    validation_issues, _ = validate_policy(
        policy, root, check_files=True, expected_candidate_id=candidate_id,
    )
    if validation_issues:
        return None, validation_issues
    return policy, issues


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--policy", type=Path, default=POLICY_PATH)
    parser.add_argument("--candidate-id")
    parser.add_argument(
        "--motion-version", choices=("v1", "v2", "v3", "v4", "v5", "v6"),
        help=(
            "Motion revision selected for append-only candidate-policy generation; "
            "defaults to the current runtime-selected v6 revision."
        ),
    )
    parser.add_argument(
        "--generate-candidate-policy", type=Path,
        help=(
            "Append-only Config policy path; refreshes current project-file "
            "identities and binds the exact candidate-content receipt for "
            "--candidate-id without changing historical authority."
        ),
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--require-runtime-ready", action="store_true")
    parser.add_argument(
        "--validate-recipe-version", choices=("v1", "v2", "v3", "v4", "v5", "v6"),
        help=(
            "Validate only the selected immutable recipe/tooling contract. "
            "This path does not require or mutate the historical v001 policy/evidence."
        ),
    )
    args = parser.parse_args()
    root = args.root.resolve()
    if args.validate_recipe_version is not None:
        if (
            args.generate_candidate_policy is not None
            or args.candidate_id is not None
            or args.motion_version is not None
            or args.output is not None
            or args.self_test
            or args.require_runtime_ready
        ):
            parser.error(
                "--validate-recipe-version cannot be combined with policy, output, "
                "self-test, candidate, or runtime-ready modes"
            )
        from validate_dg_session19_production_motion_recipe import validate_path

        recipe_issues, recipe_observations = validate_path(
            root, args.validate_recipe_version
        )
        if args.validate_recipe_version == "v6":
            repair_issues, repair_observations = validate_v6_blend_out_repair_path(
                root, check_files=True,
            )
            recipe_issues.extend(repair_issues)
            recipe_observations["additive_v006_blend_out_repair"] = (
                repair_observations
            )
        if recipe_issues:
            print("SESSION 19 PRODUCTION MOTION RECIPE INVALID")
            for issue in recipe_issues:
                print(f" - {issue}")
            return 1
        print(
            "SESSION 19 PRODUCTION MOTION RECIPE PASS "
            + json.dumps(recipe_observations, sort_keys=True, separators=(",", ":"))
        )
        return 0
    if args.generate_candidate_policy is not None:
        if args.candidate_id is None:
            parser.error("--generate-candidate-policy requires --candidate-id")
        if args.self_test or args.output is not None or args.require_runtime_ready:
            parser.error(
                "--generate-candidate-policy cannot be combined with --self-test, "
                "--output, or --require-runtime-ready"
            )
        destination = (
            args.generate_candidate_policy
            if args.generate_candidate_policy.is_absolute()
            else root / args.generate_candidate_policy
        )
        generated, generation_issues = generate_candidate_policy(
            root, args.candidate_id, destination,
            args.motion_version or "v6",
        )
        if generated is None:
            print("SESSION 19 PRODUCTION MOTION CANDIDATE POLICY REFUSED")
            for issue in generation_issues:
                print(f" - {issue}")
            return 1
        payload = (json.dumps(generated, indent=2) + "\n").encode("utf-8")
        try:
            write_bytes_exclusive(destination, payload)
        except (FileExistsError, OSError) as exc:
            print(f"SESSION 19 PRODUCTION MOTION CANDIDATE POLICY REFUSED: {exc}")
            return 1
        print(
            "SESSION 19 PRODUCTION MOTION CANDIDATE POLICY GENERATED_APPEND_ONLY "
            f"candidate={args.candidate_id} path={destination.relative_to(root).as_posix()} "
            f"bytes={len(payload)} sha256={hashlib.sha256(payload).hexdigest().upper()}"
        )
        return 0
    if args.motion_version is not None:
        parser.error("--motion-version requires --generate-candidate-policy")
    policy_path = args.policy if args.policy.is_absolute() else root / args.policy
    policy_path = policy_path.resolve()
    try:
        policy_relative = policy_path.relative_to(root).as_posix()
    except ValueError:
        print("SESSION 19 PRODUCTION MOTION INVALID: selected policy must be inside --root")
        return 1
    historical_relative = "Config/DG_Session19ProductionMotionAuthoringPolicy.json"
    candidate_scoped_policy = policy_relative != historical_relative
    if candidate_scoped_policy:
        expected_policy_relative = (
            "Config/DG_Session19ProductionMotionAuthoringPolicy-"
            f"{args.candidate_id}.json"
            if args.candidate_id is not None else None
        )
        if expected_policy_relative is None or policy_relative != expected_policy_relative:
            print(
                "SESSION 19 PRODUCTION MOTION INVALID: candidate-scoped policy "
                "requires matching --candidate-id and canonical Config filename"
            )
            return 1
    try:
        policy = load_json(policy_path)
    except StrictJsonError as exc:
        print(f"SESSION 19 PRODUCTION MOTION INVALID: {exc}")
        return 1
    if type(policy) is not dict:
        print("SESSION 19 PRODUCTION MOTION INVALID: policy root must be an object")
        return 1
    if args.self_test:
        if args.candidate_id is not None and not candidate_scoped_policy:
            parser.error(
                "--self-test accepts --candidate-id only with its canonical "
                "candidate-scoped policy"
            )
        passed, count, failures = run_self_test(policy, root)
        if not passed:
            print("SESSION 19 PRODUCTION MOTION SELF-TEST FAILED")
            for failure in failures:
                print(f" - {failure}")
            return 1
        print(f"SESSION 19 PRODUCTION MOTION SELF-TEST PASS cases={count}")
        return 0
    issues, observations = validate_policy(
        policy, root, check_files=True, expected_candidate_id=args.candidate_id,
    )
    if issues:
        print("SESSION 19 PRODUCTION MOTION INVALID")
        for issue in issues:
            print(f" - {issue}")
        return 1
    if args.output is not None:
        output = args.output if args.output.is_absolute() else root / args.output
        output = output.resolve()
        if candidate_scoped_policy:
            expected_output = (
                root / "Evidence/Session19" /
                f"ProductionMotionTechnicalCandidateEvidence-{args.candidate_id}.json"
            ).resolve()
            if output != expected_output:
                print(
                    "SESSION 19 PRODUCTION MOTION OUTPUT FAILED: candidate-scoped "
                    f"--output must be {expected_output.relative_to(root).as_posix()}"
                )
                return 1
        try:
            write_receipt(output, policy_path, policy, observations, root)
        except (OSError, FileExistsError) as exc:
            print(f"SESSION 19 PRODUCTION MOTION OUTPUT FAILED: {exc}")
            return 1
    if args.require_runtime_ready:
        print(
            "SESSION 19 PRODUCTION MOTION RELEASE BLOCKED: technical candidates=7/7, "
            "families=3/3, runtime_binding=true, AlwaysCook=true; live profile-matrix/"
            "MetaHuman/single-release validation, human animation/disc-contact/product "
            "approval, and a fresh Shipping candidate containing all seven identities remain"
        )
        return 2
    print(
        "SESSION 19 PRODUCTION MOTION TECHNICAL CANDIDATE PASS families=3/3 "
        "semantic_coverage=8 target_assets=7/7 synthetic_reuse=0 "
        "authoring_prerequisites=4/4 runtime_binding=true AlwaysCook=true "
        "fresh_shipping_candidate=false human_motion=false "
        "human_contact=false release_ready=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
