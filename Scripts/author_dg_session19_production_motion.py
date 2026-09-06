"""Guarded Unreal commandlet wrapper for project-authored procedural motion candidates.

This script must run inside UE's PythonScript commandlet with the explicit
``-DGAuthorProductionProceduralMotion`` switch. It does not import, duplicate,
retarget, or relabel the Session 3/5 synthetic fixtures. The native editor utility
authors three independent DGMaster sequences, their montages, and one library from
the source-controlled project recipe. Human animation/contact/product approval
remain deliberately false.
"""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import math
import re
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import unreal


AUTHOR_SWITCH = "-DGAuthorProductionProceduralMotion"
REQUIRED_USER_ROOT = Path(r"C:\DGTour_TestRuns\Session19ProductionMotion")
REPORT_RELATIVE = Path("Saved/ProductionMotion/AuthoringReport.json")
SAVE_RELATIVE = Path("Saved/SaveGames/DiscGolfTour_Profile_0.sav")
SAVE_IDENTITY = {
    "bytes": 5212,
    "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
}
PROTECTED_SYNTHETIC = {
    "Content/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.uasset": {
        "bytes": 321080,
        "sha256": "EA53E0460B958FFA7C8BC1DCACB5A4E6F1C6ABBACE9F177C68A1017C6783ECE6",
    },
    "Content/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.uasset": {
        "bytes": 20451,
        "sha256": "6BCD1C3256668D7F041FE6D33B6910052EE77FA4739A1EF4E60E689A787A8AF8",
    },
    "Content/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001.uasset": {
        "bytes": 414050,
        "sha256": "8CFC265B520542970328D4B0A25F4528D0CE480224A3109F0074CECECA62F1F1",
    },
    "Content/DiscGolf/Animation/Mocap/Production/AM_DG_RHBH_SyntheticPipelineTest_v001.uasset": {
        "bytes": 21131,
        "sha256": "707463D4ED96C7B9CD70B94E164292BEF6B8301390D3098E146F8BF9D6C2BFDB",
    },
    "Content/DiscGolf/Animation/Mocap/Production/DA_DG_AnimationLibrary.uasset": {
        "bytes": 2915,
        "sha256": "620098CD89FEBD991422DE012F34AC58F4AFE45420419737CECAB3D93AEE7955",
    },
}
V1_TARGET_OBJECTS = (
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/A_DG_RHBH_Drive_Procedural_v001.A_DG_RHBH_Drive_Procedural_v001",
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v001.AM_DG_RHBH_Drive_Procedural_v001",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/A_DG_RHBH_Approach_Procedural_v001.A_DG_RHBH_Approach_Procedural_v001",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v001.AM_DG_RHBH_Approach_Procedural_v001",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/A_DG_RHBH_Putt_Procedural_v001.A_DG_RHBH_Putt_Procedural_v001",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v001.AM_DG_RHBH_Putt_Procedural_v001",
    "/Game/DiscGolf/Animation/ProductionMotion/DA_DG_ProductionMotionLibrary.DA_DG_ProductionMotionLibrary",
)

V2_TARGET_OBJECTS = (
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/A_DG_RHBH_Drive_Procedural_v002.A_DG_RHBH_Drive_Procedural_v002",
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v002.AM_DG_RHBH_Drive_Procedural_v002",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/A_DG_RHBH_Approach_Procedural_v002.A_DG_RHBH_Approach_Procedural_v002",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v002.AM_DG_RHBH_Approach_Procedural_v002",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/A_DG_RHBH_Putt_Procedural_v002.A_DG_RHBH_Putt_Procedural_v002",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v002.AM_DG_RHBH_Putt_Procedural_v002",
    "/Game/DiscGolf/Animation/ProductionMotion/DA_DG_ProductionMotionLibrary_v002.DA_DG_ProductionMotionLibrary_v002",
)

V3_TARGET_OBJECTS = (
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/A_DG_RHBH_Drive_Procedural_v003.A_DG_RHBH_Drive_Procedural_v003",
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v003.AM_DG_RHBH_Drive_Procedural_v003",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/A_DG_RHBH_Approach_Procedural_v003.A_DG_RHBH_Approach_Procedural_v003",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v003.AM_DG_RHBH_Approach_Procedural_v003",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/A_DG_RHBH_Putt_Procedural_v003.A_DG_RHBH_Putt_Procedural_v003",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v003.AM_DG_RHBH_Putt_Procedural_v003",
    "/Game/DiscGolf/Animation/ProductionMotion/DA_DG_ProductionMotionLibrary_v003.DA_DG_ProductionMotionLibrary_v003",
)
V4_TARGET_OBJECTS = (
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/A_DG_RHBH_Drive_Procedural_v004.A_DG_RHBH_Drive_Procedural_v004",
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v004.AM_DG_RHBH_Drive_Procedural_v004",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/A_DG_RHBH_Approach_Procedural_v004.A_DG_RHBH_Approach_Procedural_v004",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v004.AM_DG_RHBH_Approach_Procedural_v004",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/A_DG_RHBH_Putt_Procedural_v004.A_DG_RHBH_Putt_Procedural_v004",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v004.AM_DG_RHBH_Putt_Procedural_v004",
    "/Game/DiscGolf/Animation/ProductionMotion/DA_DG_ProductionMotionLibrary_v004.DA_DG_ProductionMotionLibrary_v004",
)
V5_TARGET_OBJECTS = (
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/A_DG_RHBH_Drive_Procedural_v005.A_DG_RHBH_Drive_Procedural_v005",
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v005.AM_DG_RHBH_Drive_Procedural_v005",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/A_DG_RHBH_Approach_Procedural_v005.A_DG_RHBH_Approach_Procedural_v005",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v005.AM_DG_RHBH_Approach_Procedural_v005",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/A_DG_RHBH_Putt_Procedural_v005.A_DG_RHBH_Putt_Procedural_v005",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v005.AM_DG_RHBH_Putt_Procedural_v005",
    "/Game/DiscGolf/Animation/ProductionMotion/DA_DG_ProductionMotionLibrary_v005.DA_DG_ProductionMotionLibrary_v005",
)
V6_TARGET_OBJECTS = tuple(
    object_path.replace("v005", "v006") for object_path in V5_TARGET_OBJECTS
)
V7_TARGET_OBJECTS = tuple(
    object_path.replace("v006", "v007") for object_path in V6_TARGET_OBJECTS
)

VERSION_SPECS = {
    "v1": {
        "schema_version": 1,
        "asset_revision": "v001",
        "evidence_schema": (
            "DiscGolfTour.Session19ProductionMotionCandidateAuthoringEvidence.v1"
        ),
        "canonical_evidence": Path(
            "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence.json"
        ),
        "recipe": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v1.json"),
        "recipe_schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v1",
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V1",
        "targets": V1_TARGET_OBJECTS,
    },
    "v2": {
        "schema_version": 2,
        "asset_revision": "v002",
        "evidence_schema": (
            "DiscGolfTour.Session19ProductionMotionCandidateAuthoringEvidence.v2"
        ),
        "canonical_evidence": Path(
            "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v002.json"
        ),
        "recipe": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v2.json"),
        "recipe_schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v2",
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V2",
        "targets": V2_TARGET_OBJECTS,
    },
    "v3": {
        "schema_version": 3,
        "asset_revision": "v003",
        "evidence_schema": (
            "DiscGolfTour.Session19ProductionMotionCandidateAuthoringEvidence.v3"
        ),
        "canonical_evidence": Path(
            "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v003.json"
        ),
        "recipe": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v3.json"),
        "recipe_schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v3",
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V3",
        "targets": V3_TARGET_OBJECTS,
    },
    "v4": {
        "schema_version": 4,
        "asset_revision": "v004",
        "evidence_schema": (
            "DiscGolfTour.Session19ProductionMotionCandidateAuthoringEvidence.v4"
        ),
        "canonical_evidence": Path(
            "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v004.json"
        ),
        "recipe": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v4.json"),
        "recipe_schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v4",
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V4",
        "targets": V4_TARGET_OBJECTS,
    },
    "v5": {
        "schema_version": 5,
        "asset_revision": "v005",
        "evidence_schema": (
            "DiscGolfTour.Session19ProductionMotionCandidateAuthoringEvidence.v5"
        ),
        "canonical_evidence": Path(
            "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v005.json"
        ),
        "recipe": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v5.json"),
        "recipe_schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v5",
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V5",
        "targets": V5_TARGET_OBJECTS,
    },
    "v6": {
        "schema_version": 6,
        "asset_revision": "v006",
        "evidence_schema": (
            "DiscGolfTour.Session19ProductionMotionCandidateAuthoringEvidence.v6"
        ),
        "canonical_evidence": Path(
            "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v006.json"
        ),
        "recipe": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json"),
        "recipe_schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v6",
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V6",
        "targets": V6_TARGET_OBJECTS,
    },
    "v7": {
        "schema_version": 7,
        "asset_revision": "v007",
        "evidence_schema": (
            "DiscGolfTour.Session19ProductionMotionCandidateAuthoringEvidence.v7"
        ),
        "canonical_evidence": Path(
            "Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v007.json"
        ),
        "recipe": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v7.json"),
        "recipe_schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v7",
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V7",
        "targets": V7_TARGET_OBJECTS,
    },
}
VERSIONED_MOTION = {"v2", "v3", "v4", "v5", "v6", "v7"}
GRANULAR_MOTION = {"v3", "v4", "v5", "v6", "v7"}

# Existing v001 outputs are immutable inputs while v002 is authored. These
# identities deliberately fail closed instead of treating "present" as enough.
PROTECTED_V1 = {
    V1_TARGET_OBJECTS[0]: {"bytes": 272668, "sha256": "43530C4802599684CA44CA62609C8759E7D06E5CE9C692E657C3120D4985C822"},
    V1_TARGET_OBJECTS[1]: {"bytes": 21214, "sha256": "ED67CFF772682AB023F522AB4D0CD7FC535A3DDE1501A2CA17108D458604C2CE"},
    V1_TARGET_OBJECTS[2]: {"bytes": 183697, "sha256": "FF38F160DA70CD0A12B4E2FD335C128E3043E1F850901930A95AA65C24DEDDB1"},
    V1_TARGET_OBJECTS[3]: {"bytes": 21244, "sha256": "96D06A1AF3AEE1C764C582B06D0E20CB029CE5E2F0CA511F59EF3B495787163C"},
    V1_TARGET_OBJECTS[4]: {"bytes": 131591, "sha256": "E8924448E99D01FA2B0B7361FBC50DB31A76670F6E8A070FAAEB5844A5B83781"},
    V1_TARGET_OBJECTS[5]: {"bytes": 21204, "sha256": "9148D82E4161790818542107C25C389718E1B4519390255EE6D08367B7F5EC1A"},
    V1_TARGET_OBJECTS[6]: {"bytes": 4068, "sha256": "AAD0157BC480B5C572FFF3D5022AFBA9E8A4824DB5A2CF5287F0330FCCC23CD9"},
}

PROTECTED_V2 = {
    V2_TARGET_OBJECTS[0]: {"bytes": 430246, "sha256": "4ABED0194663C99A0859EB533406BD75FCC406AC9351AC04582C65EC13DF7E32"},
    V2_TARGET_OBJECTS[1]: {"bytes": 21417, "sha256": "F66134F9AFB001D0C55752C10AC4512EB78AA4686121854F6B4399CCFDF2E15F"},
    V2_TARGET_OBJECTS[2]: {"bytes": 320409, "sha256": "9C613490E29BA475606C356E8DC743E26515C1C3EE6DFCF8247A132E35358BC1"},
    V2_TARGET_OBJECTS[3]: {"bytes": 21447, "sha256": "00C6FD7BF53CF90C06D6EA23E67940B13780347CAC28FF61838CF757FDF73BA9"},
    V2_TARGET_OBJECTS[4]: {"bytes": 217541, "sha256": "27DDA73C2C209E500F40B32D6BDB33BF10A73DC4E455A2F8C7301904791DC07E"},
    V2_TARGET_OBJECTS[5]: {"bytes": 21407, "sha256": "DAAFCBE347F5C16D180F0BA14D31CDCD04C835C14280DEB03FE1946A705B5B8B"},
    V2_TARGET_OBJECTS[6]: {"bytes": 4477, "sha256": "9E0F9876B4B106A204E3ACF5D4BA2A980E41C6E61F5F1A58147BB428F4753592"},
}

PROTECTED_V3 = {
    V3_TARGET_OBJECTS[0]: {"bytes": 460500, "sha256": "3566C3CDEB794D7436090C81712F535362270B51C162C59B9966CD8A9FC637E3"},
    V3_TARGET_OBJECTS[1]: {"bytes": 21695, "sha256": "D499FCE8882E3EC45ED5DA740F9362F2AD21F32E9CDCB127B9FB132B97F7C3FB"},
    V3_TARGET_OBJECTS[2]: {"bytes": 356486, "sha256": "5833A56E566575D57703A6E5077D4BB3C08787937BEFD7D40F3530F2D97D8829"},
    V3_TARGET_OBJECTS[3]: {"bytes": 21725, "sha256": "8BB5F4DB588DE6DB731ABE34534ACC4CEC768001F02D290F2C3DC27410231D29"},
    V3_TARGET_OBJECTS[4]: {"bytes": 252498, "sha256": "D3862386FB04D966638019BFF88868C376A3D5130AD19D9A77D60B9BA2339E5F"},
    V3_TARGET_OBJECTS[5]: {"bytes": 21685, "sha256": "6E2572E82B4D9F3766952FCA5D10395BE8E87990D0F0F956EAF75CC699C342E3"},
    V3_TARGET_OBJECTS[6]: {"bytes": 4755, "sha256": "60AA39C6B51A6E774B7E9C53E8E6B0D5248EB07FCE795A1DC061666D44B95515"},
}
PROTECTED_V3_EVIDENCE = {
    "path": Path("Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v003.json"),
    "bytes": 18695,
    "sha256": "17D5972B20840F3792F6BB8C63345FC40D4DE3BA65BFBD6FAC4186223FFE1C60",
}
PROTECTED_V4 = {
    V4_TARGET_OBJECTS[0]: {"bytes": 534909, "sha256": "88D82002DD58EEE87FE5A8263D59DE8D50CCC9053B32C04E9BB89CD77781A58E"},
    V4_TARGET_OBJECTS[1]: {"bytes": 21864, "sha256": "B44DA35C0E95AB7DBF63A9403D264A9C5FEDCEE5230326D3C0D2E222971214AE"},
    V4_TARGET_OBJECTS[2]: {"bytes": 422927, "sha256": "98FB3EB65AB18648E231D3BE7BEAE8368393B523343C136872F4D89C42EED970"},
    V4_TARGET_OBJECTS[3]: {"bytes": 21894, "sha256": "4108863BB722C4FFE7F1FD1AF0551B2F76109C4098115D358BAAC1BE99732AAA"},
    V4_TARGET_OBJECTS[4]: {"bytes": 308731, "sha256": "18BA8FDB17A8592D54E87B6C6515D63C7E36A79E31FA2FAB8FC9A072044E1218"},
    V4_TARGET_OBJECTS[5]: {"bytes": 21854, "sha256": "1B40EAB152CFF21B41EF45EC0A205FDD3812B8885BFB176FB8E40924F285F899"},
    V4_TARGET_OBJECTS[6]: {"bytes": 4924, "sha256": "EE50AA14E8A8C535582B5A67FD7DE363BEAC8D9E6F1D4E24B736C26CAF712F8D"},
}
PROTECTED_V4_EVIDENCE = {
    "path": Path("Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v004.json"),
    "bytes": 21537,
    "sha256": "14FF4C4FD36C47A410203F4796ADE6C4F761189F4477F44E6528BD663423C89F",
}
PROTECTED_V5 = {
    V5_TARGET_OBJECTS[0]: {"bytes": 470161, "sha256": "16EB4356D8BAAA0341D3C404658130CDC706D83887818F9184E5AA2F698C948E"},
    V5_TARGET_OBJECTS[1]: {"bytes": 22076, "sha256": "CD5397AA9F3A1DD9A37D075AC3BD86EEC458B9AE056E73C3514FC1D59183CF4D"},
    V5_TARGET_OBJECTS[2]: {"bytes": 374307, "sha256": "B847FE8BBF371CCD762A5E1E4A6F12B318BA77194D9169FD2A4BEAA89F71D738"},
    V5_TARGET_OBJECTS[3]: {"bytes": 22106, "sha256": "D300B6C359EBE01BA16772DFB0871F4692F370A8DBA1C9FF460D4C856CE20B1F"},
    V5_TARGET_OBJECTS[4]: {"bytes": 271567, "sha256": "4F2C7DA0E3A0716ED1883851710472558A56CEC1D5F892B89181151450E9AA2B"},
    V5_TARGET_OBJECTS[5]: {"bytes": 22066, "sha256": "C42CE15E494D2694CE0BBD777C958E9D7390F813904CC5E5A818784081B716B1"},
    V5_TARGET_OBJECTS[6]: {"bytes": 5136, "sha256": "614F4674E99A1FEE3FC35F2232992B6FE82B19A30459D3530205624B1C843644"},
}
PROTECTED_V5_EVIDENCE = {
    "path": Path("Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v005.json"),
    "bytes": 25821,
    "sha256": "88E93F390032BD90A47B278C08F71CD9576CEB230B243C05F660F03CB6095F33",
}
PROTECTED_V6 = {
    V6_TARGET_OBJECTS[0]: {"bytes": 479441, "sha256": "319CBA9E20966A5C6EA82816EC1B65F2E9400611B3E84CCA1B7D276BADD856E4"},
    V6_TARGET_OBJECTS[1]: {"bytes": 22133, "sha256": "9220D00C6D82325603065B1F8BAE70F29520190777446FF351A6D3C5C472B759"},
    V6_TARGET_OBJECTS[2]: {"bytes": 381283, "sha256": "D96AD9020A5ECBB594A3D0AF9BF3060C3DBBBA66B425EC076D0E3BF7FDBC8211"},
    V6_TARGET_OBJECTS[3]: {"bytes": 22163, "sha256": "F10D9B1572AFC4AC5613EBF179299EDC41915F2605A36E639B2863298713D985"},
    V6_TARGET_OBJECTS[4]: {"bytes": 276239, "sha256": "D7F76D263D18EA67641292C70F337AA7C5907FCA001ACECE650688D1F189F5A3"},
    V6_TARGET_OBJECTS[5]: {"bytes": 22123, "sha256": "10FA1BD79CA5CCFEF0301336754642B8E19574E55C59F61CB3DF0C7D431FA8FA"},
    V6_TARGET_OBJECTS[6]: {"bytes": 5136, "sha256": "76C004ED999922253710B948F624E9CC72FC70DF25A85B7136ADB80819822622"},
}
PROTECTED_V6_EVIDENCE = {
    "path": Path("Evidence/Session19/ProductionMotionCandidateAuthoringEvidence-v006.json"),
    "bytes": 44023,
    "sha256": "A6BC2C3BA4B37718C4CD64004DD3123708939EA43519CB61CD8006EC5AE06DA6",
}
V7_SOURCE_MOTION_DESIGN_CONTRACT = {
    "schema": "DiscGolfTour.ProductionMotionV7SourceDesignContract.v1",
    "source_recipe": "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json",
    "mutation_scope": (
        "DENSE_DRIVE_BILATERAL_CLAVICLE_UPPERARM_LOWERARM_HAND_ROTATIONS_ONLY"
    ),
    "source_metric_basis": (
        "EXACT_PROBED_DGMASTER_REFERENCE_CHAIN_STATIC_RECONSTRUCTION"
    ),
    "dense_drive_sampling_policy": {
        "frames": [0, 144],
        "frame_rate": 60,
        "required_pose_count": 145,
        "non_arm_source_sampling": (
            "V006_CATMULL_ROM_INTEGER_FRAME_SAMPLES_ROUNDED_6_DECIMALS"
        ),
        "throwing_component_reshape_frames": [66, 94],
        "release_shoulder_circle_sample": 115,
    },
    "support_arm_named_phase_gate": {
        "frames": [44, 64, 84, 118],
        "maximum_reach_ratio": 0.75,
        "minimum_elbow_degrees": 65.0,
        "maximum_elbow_degrees": 105.0,
    },
    "throwing_arm_power_pocket_gate": {
        "frames": [79, 80, 81],
        "minimum_reach_ratio": 0.70,
        "maximum_reach_ratio": 0.82,
        "minimum_elbow_degrees": 85.0,
        "maximum_elbow_degrees": 105.0,
        "maximum_absolute_torso_lateral_cm": 46.0,
        "minimum_torso_forward_cm": -45.0,
        "maximum_torso_forward_cm": -20.0,
        "minimum_torso_vertical_cm": 5.0,
        "maximum_torso_vertical_cm": 25.0,
    },
    "throwing_arm_release_gate": {
        "frame": 84,
        "minimum_reach_ratio": 0.95,
        "maximum_reach_ratio": 0.99,
        "minimum_elbow_degrees": 145.0,
        "maximum_elbow_degrees": 165.0,
    },
    "throwing_arm_followthrough_gate": {
        "frame": 94,
        "minimum_reach_ratio": 0.95,
        "maximum_reach_ratio": 1.0,
        "minimum_elbow_degrees": 155.0,
        "maximum_elbow_degrees": 175.0,
    },
    "throwing_arm_recovery_gate": {
        "frame": 118,
        "maximum_reach_ratio": 0.75,
        "minimum_elbow_degrees": 75.0,
        "maximum_elbow_degrees": 110.0,
        "maximum_absolute_torso_lateral_cm": 35.0,
        "maximum_torso_vertical_cm": -15.0,
    },
    "release_component_grip_invariant": {
        "frame": 84,
        "maximum_translation_error_cm": 0.1,
        "maximum_rotation_error_degrees": 0.1,
        "reference_local_translation_cm": [-3.5, -6.0, -2.0],
        "reference_local_rotation_xyzw": [
            5.329638952389359e-10,
            3.781419621873283e-12,
            1.809875339875422e-11,
            -1.0,
        ],
    },
    "release_proximal_stability_gate": {
        "frame": 84,
        "maximum_shoulder_origin_delta_from_v006_cm": 11.5,
        "maximum_clavicle_relative_to_spine_delta_from_v006_degrees": 42.0,
        "maximum_fixed_hand_wrist_correction_degrees": 20.0,
    },
    "source_continuity_gate": {
        "frame_rate": 60,
        "gate_interpretation": (
            "PROCEDURAL_60HZ_ANTI_POP_REGRESSION_NOT_HUMAN_ANATOMICAL_MAXIMUM"
        ),
        "lowerarm_exception_basis": (
            "24_DEGREES_PER_FRAME_AND_18_DEGREES_PER_FRAME_SQUARED_"
            "ARE_SAMPLING_STYLE_CAPS_NOT_ANATOMY_MAXIMA"
        ),
        "high_rate_capture_validation_required": True,
        "maximum_hand_translation_cm_per_frame": 12.25,
        "maximum_grip_translation_cm_per_frame": 14.25,
        "maximum_hand_translation_acceleration_cm_per_frame_squared": 10.0,
        "maximum_grip_translation_acceleration_cm_per_frame_squared": 10.25,
        "maximum_torso_local_shoulder_translation_cm_per_frame": 2.25,
        "maximum_torso_local_shoulder_acceleration_cm_per_frame_squared": 3.0,
        "maximum_torso_local_elbow_translation_cm_per_frame": 10.0,
        "maximum_torso_local_elbow_acceleration_cm_per_frame_squared": 8.0,
        "maximum_clavicle_relative_to_spine_log_vector_rate_degrees_per_frame": 8.0,
        "maximum_clavicle_relative_to_spine_log_vector_acceleration_degrees_per_frame_squared": 6.0,
        "maximum_throwing_upperarm_component_log_vector_rate_degrees_per_frame": 18.0,
        "maximum_throwing_upperarm_component_log_vector_acceleration_degrees_per_frame_squared": 15.0,
        "maximum_throwing_lowerarm_component_log_vector_rate_degrees_per_frame": 24.0,
        "maximum_throwing_lowerarm_component_log_vector_acceleration_degrees_per_frame_squared": 18.0,
        "maximum_elbow_delta_degrees_per_frame": 45.0,
        "maximum_elbow_bend_plane_delta_degrees_per_frame": 60.0,
        "maximum_pre_release_disc_plane_delta_degrees_per_frame": 15.0,
        "maximum_throwing_wrist_additive_degrees": 25.0,
        "quaternion_log_vector_convention": (
            "SPATIAL_DELTA_CURRENT_TIMES_INVERSE_PREVIOUS_SHORTEST_"
            "W_NONNEGATIVE_LOG_VECTOR_DEGREES;_ACCELERATION_IS_NORM_"
            "OF_CONSECUTIVE_LOG_VECTOR_DIFFERENCE"
        ),
    },
    "static_source_geometry_approval": False,
    "unreal_assets_authored": False,
    "runtime_source_pose_approval": False,
    "metahuman_deformation_approval": False,
    "human_animation_approval": False,
    "human_disc_contact_approval": False,
    "release_approval": False,
    "status": "CANDIDATE_REQUIRES_FAIL_CLOSED_V7_VALIDATION",
}
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


class AuthorError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def identity(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    return {
        "exists": True,
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
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


def validate_command_line(
    command_line: str, project_root: Path
) -> tuple[Path, str, dict[str, Any]]:
    required = (AUTHOR_SWITCH, "-unattended", "-nop4")
    missing = [item for item in required if not exact_switch(command_line, item)]
    if missing:
        raise AuthorError(f"required Unreal command-line switches absent: {missing}")
    if "-run=pythonscript" not in command_line.casefold():
        raise AuthorError("authoring must run through PythonScript commandlet")
    version = command_value(command_line, "DGProductionMotionVersion").casefold()
    if version not in VERSION_SPECS:
        raise AuthorError(
            "exact -DGProductionMotionVersion=v1, v2, v3, v4, v5, v6, or v7 is required"
        )
    user_text = command_value(command_line, "UserDir")
    if not user_text:
        raise AuthorError("absolute external -UserDir is required")
    user_dir = Path(user_text).resolve()
    expected_root = REQUIRED_USER_ROOT.resolve()
    try:
        relative = user_dir.relative_to(expected_root)
    except ValueError as exc:
        raise AuthorError(f"UserDir is outside {expected_root}: {user_dir}") from exc
    if len(relative.parts) != 1:
        raise AuthorError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise AuthorError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold():
        raise AuthorError("UserDir UUID is not canonical lowercase-hyphen form")
    try:
        user_dir.relative_to(project_root)
    except ValueError:
        pass
    else:
        raise AuthorError("UserDir must be external to the project")
    report = user_dir / REPORT_RELATIVE
    if report.exists():
        raise AuthorError(f"fresh authoring report already exists: {report}")
    return user_dir, version, VERSION_SPECS[version]


def strict_json(text: str, label: str) -> dict[str, Any]:
    def pairs(values: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in values:
            if key in result:
                raise AuthorError(f"duplicate JSON key {key!r} in {label}")
            result[key] = value
        return result

    try:
        value = json.loads(text, object_pairs_hook=pairs)
    except json.JSONDecodeError as exc:
        raise AuthorError(f"invalid JSON from {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise AuthorError(f"{label} JSON root is not an object")
    return value


def validate_v7_append_only_scope(
    source: dict[str, Any], candidate: dict[str, Any]
) -> None:
    """Fail unless v007 is a dense v006 bake plus Drive arm rotations only."""
    arm_bones = (
        "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
        "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
    )

    def families(value: dict[str, Any]) -> dict[str, dict[str, Any]]:
        rows = value.get("families")
        if not isinstance(rows, list) or any(not isinstance(row, dict) for row in rows):
            raise AuthorError("v7 append-only comparison lacks exact family objects")
        result = {str(row.get("family")): row for row in rows}
        if set(result) != {"Drive", "Approach", "Putt"} or len(result) != len(rows):
            raise AuthorError("v7 append-only comparison family identity differs")
        return result

    def poses(family: dict[str, Any]) -> dict[int, dict[str, Any]]:
        rows = family.get("pose_keys")
        if not isinstance(rows, list) or any(not isinstance(row, dict) for row in rows):
            raise AuthorError("v7 append-only comparison lacks exact pose keys")
        result: dict[int, dict[str, Any]] = {}
        for row in rows:
            frame = row.get("frame")
            if not isinstance(frame, int) or isinstance(frame, bool) or frame in result:
                raise AuthorError("v7 append-only comparison pose frame differs")
            result[frame] = row
        return result

    def catmull_rom_scalar(
        first: float,
        second: float,
        third: float,
        fourth: float,
        alpha: float,
    ) -> float:
        squared = alpha * alpha
        cubed = squared * alpha
        return 0.5 * (
            2.0 * second
            + (-first + third) * alpha
            + (2.0 * first - 5.0 * second + 4.0 * third - fourth) * squared
            + (-first + 3.0 * second - 3.0 * third + fourth) * cubed
        )

    def sampled_scalar(
        ordered_frames: list[int], values: dict[int, float], frame: int
    ) -> float:
        upper = 1
        while upper < len(ordered_frames) and ordered_frames[upper] < frame:
            upper += 1
        upper = min(max(upper, 1), len(ordered_frames) - 1)
        earlier = ordered_frames[upper - 1]
        later = ordered_frames[upper]
        alpha = (frame - earlier) / max(1, later - earlier)
        return catmull_rom_scalar(
            values[ordered_frames[max(0, upper - 2)]],
            values[earlier],
            values[later],
            values[ordered_frames[min(len(ordered_frames) - 1, upper + 1)]],
            alpha,
        )

    def dense_v6_poses(family: dict[str, Any]) -> list[dict[str, Any]]:
        source_poses = family.get("pose_keys")
        frame_count = family.get("frame_count")
        if (
            not isinstance(source_poses, list)
            or not source_poses
            or not isinstance(frame_count, int)
            or isinstance(frame_count, bool)
        ):
            raise AuthorError("v7 dense append-only comparison source is malformed")
        ordered_frames = [pose.get("frame") for pose in source_poses]
        if (
            any(not isinstance(frame, int) or isinstance(frame, bool)
                for frame in ordered_frames)
            or ordered_frames != sorted(set(ordered_frames))
            or ordered_frames[0] != 0
            or ordered_frames[-1] != frame_count
        ):
            raise AuthorError("v7 dense append-only comparison frame set differs")
        rotations = {
            pose["frame"]: pose.get("rotation_degrees") for pose in source_poses
        }
        grips = {
            pose["frame"]: float(pose.get("throwing_hand_grip_alpha"))
            for pose in source_poses
        }
        roots = {
            pose["frame"]: pose.get("translation_cm", {}).get("root")
            for pose in source_poses
        }
        first_rotations = rotations[ordered_frames[0]]
        if not isinstance(first_rotations, dict):
            raise AuthorError("v7 dense append-only comparison rotations are absent")
        bones = tuple(first_rotations)
        result: list[dict[str, Any]] = []
        for frame in range(frame_count + 1):
            result.append({
                "frame": frame,
                "throwing_hand_grip_alpha": round(
                    sampled_scalar(ordered_frames, grips, frame), 6
                ),
                "translation_cm": {
                    "root": [
                        round(sampled_scalar(
                            ordered_frames,
                            {
                                pose_frame: float(translation[axis])
                                for pose_frame, translation in roots.items()
                            },
                            frame,
                        ), 6)
                        for axis in range(3)
                    ],
                },
                "rotation_degrees": {
                    bone: [
                        round(sampled_scalar(
                            ordered_frames,
                            {
                                pose_frame: float(values[bone][axis])
                                for pose_frame, values in rotations.items()
                            },
                            frame,
                        ), 6)
                        for axis in range(3)
                    ]
                    for bone in bones
                },
            })
        return result

    comparison = copy.deepcopy(source)
    comparison_families = families(comparison)
    comparison_families["Drive"]["pose_keys"] = dense_v6_poses(
        comparison_families["Drive"]
    )
    normalized = copy.deepcopy(candidate)
    normalized.pop("v7_source_motion_design_contract", None)
    for key in ("schema", "schema_version", "recipe_id", "recipe_version", "asset_revision"):
        normalized[key] = source.get(key)
    normalized_families = families(normalized)
    for family_name, family in normalized_families.items():
        source_family = comparison_families[family_name]
        for key in (
            "motion_id", "sequence_object_path", "montage_object_path", "style_id"
        ):
            family[key] = source_family.get(key)
        if family_name == "Drive":
            source_poses = poses(source_family)
            for frame, pose in poses(family).items():
                source_pose = source_poses.get(frame)
                rotations = pose.get("rotation_degrees")
                source_rotations = (
                    source_pose.get("rotation_degrees")
                    if isinstance(source_pose, dict) else None
                )
                if not isinstance(rotations, dict) or not isinstance(source_rotations, dict):
                    raise AuthorError("v7 Drive arm rotation payload is malformed")
                for bone in arm_bones:
                    if bone not in rotations or bone not in source_rotations:
                        raise AuthorError(f"v7 Drive arm payload omits {bone} at frame {frame}")
                    rotations[bone] = copy.deepcopy(source_rotations[bone])
    if normalized != comparison:
        raise AuthorError(
            "v7 differs from the per-frame v006 bake outside version identity "
            "and Drive bilateral arm rotations"
        )

    source_drive = families(source)["Drive"]
    candidate_drive = families(candidate)["Drive"]
    if (
        source_drive.get("release_frame") != 84
        or candidate_drive.get("release_frame") != 84
        or source_drive.get("biomechanical_events", {}).get("release_frame") != 84
        or candidate_drive.get("biomechanical_events", {}).get("release_frame") != 84
    ):
        raise AuthorError("v7 release frame differs from immutable v006 frame 84")
    source_release = poses(source_drive).get(84)
    candidate_release = poses(candidate_drive).get(84)
    if not isinstance(source_release, dict) or not isinstance(candidate_release, dict):
        raise AuthorError("v7 release pose is absent")
    finger_prefixes = ("thumb_", "index_", "middle_", "ring_", "pinky_")
    source_rotations = source_release.get("rotation_degrees", {})
    candidate_rotations = candidate_release.get("rotation_degrees", {})
    source_fingers = {
        key: value for key, value in source_rotations.items()
        if key.startswith(finger_prefixes)
    }
    candidate_fingers = {
        key: value for key, value in candidate_rotations.items()
        if key.startswith(finger_prefixes)
    }
    if (
        source_release.get("throwing_hand_grip_alpha")
            != candidate_release.get("throwing_hand_grip_alpha")
        or source_release.get("translation_cm") != candidate_release.get("translation_cm")
        or source_fingers != candidate_fingers
        or source_drive.get("curves", {}).get("DG_FingerReleaseAlpha")
            != candidate_drive.get("curves", {}).get("DG_FingerReleaseAlpha")
        or source_drive.get("curves", {}).get("DG_WristLagAlpha")
            != candidate_drive.get("curves", {}).get("DG_WristLagAlpha")
    ):
        raise AuthorError("v7 frame-84 release/grip payload differs from v006")


def validate_v7_source_lane(
    project_root: Path,
    source: dict[str, Any],
    candidate: dict[str, Any],
) -> dict[str, Any]:
    """Run the deterministic v007 source validator before any package mutation."""
    contract = candidate.get("v7_source_motion_design_contract")
    if contract != V7_SOURCE_MOTION_DESIGN_CONTRACT:
        raise AuthorError(
            "v7 authoring remains locked unless the complete final source-motion "
            "design contract matches the fixed authoring authority"
        )
    scripts_dir = project_root / "Scripts"
    validator_path = scripts_dir / "validate_dg_session19_production_motion_v7.py"
    if not validator_path.is_file():
        raise AuthorError("v7 deterministic source validator is absent")
    scripts_text = str(scripts_dir)
    if scripts_text not in sys.path:
        sys.path.insert(0, scripts_text)
    module_spec = importlib.util.spec_from_file_location(
        "dg_session19_v7_source_validator_for_authoring", validator_path
    )
    if module_spec is None or module_spec.loader is None:
        raise AuthorError("v7 deterministic source validator could not be loaded")
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    validate = getattr(module, "validate_source_candidate", None)
    if not callable(validate):
        raise AuthorError("v7 deterministic source validator entry point is absent")
    issues, observations = validate(source, candidate)
    if issues:
        raise AuthorError(
            "v7 deterministic source validation failed: " + "; ".join(issues[:5])
        )
    if not isinstance(observations, dict):
        raise AuthorError("v7 deterministic source observations are malformed")
    validate_v7_source_observations(observations)
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


def validate_v7_source_observations(observations: dict[str, Any]) -> None:
    """Bind authoring to exact v007 proof fields, limits, and math convention."""
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
        raise AuthorError("v7 dense Drive identity proof is incomplete or differs")

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
        raise AuthorError("v7 bilateral arm mutation-scope proof is not exact")

    grip = observations.get("frame_84_grip_preservation")
    if not isinstance(grip, dict):
        raise AuthorError("v7 frame-84 grip/proximal evidence is absent")
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
            11.5,
        ),
        (
            "release_clavicle_relative_to_spine_delta_from_v006_degrees",
            "maximum_release_clavicle_relative_to_spine_delta_from_v006_degrees",
            42.0,
        ),
        (
            "fixed_hand_wrist_correction_degrees",
            "maximum_fixed_hand_wrist_correction_degrees",
            20.0,
        ),
    )
    if any(
        not _v7_observed_gate_passes(grip, actual, limit, expected)
        for actual, limit, expected in grip_gates
    ):
        raise AuthorError("v7 frame-84 grip/proximal evidence exceeds fixed limits")

    continuity = observations.get("source_continuity_guards")
    expected_convention = V7_SOURCE_MOTION_DESIGN_CONTRACT[
        "source_continuity_gate"
    ]["quaternion_log_vector_convention"]
    if (
        not isinstance(continuity, dict)
        or continuity.get("frame_rate") != 60
        or continuity.get("quaternion_log_vector_convention")
            != expected_convention
    ):
        raise AuthorError(
            "v7 continuity proof is absent or uses a different quaternion convention"
        )

    nested_gates = {
        "torso_local_shoulder": (
            (
                "maximum_translation_cm_per_frame",
                "maximum_translation_limit_cm_per_frame",
                2.25,
            ),
            (
                "maximum_acceleration_cm_per_frame_squared",
                "maximum_acceleration_limit_cm_per_frame_squared",
                3.0,
            ),
        ),
        "torso_local_elbow": (
            (
                "maximum_translation_cm_per_frame",
                "maximum_translation_limit_cm_per_frame",
                10.0,
            ),
            (
                "maximum_acceleration_cm_per_frame_squared",
                "maximum_acceleration_limit_cm_per_frame_squared",
                8.0,
            ),
        ),
        "clavicle_relative_to_spine": (
            (
                "maximum_log_vector_rate_degrees_per_frame",
                "maximum_log_vector_rate_limit_degrees_per_frame",
                8.0,
            ),
            (
                "maximum_log_vector_acceleration_degrees_per_frame_squared",
                "maximum_log_vector_acceleration_limit_degrees_per_frame_squared",
                6.0,
            ),
        ),
        "throwing_upperarm_component": (
            (
                "maximum_log_vector_rate_degrees_per_frame",
                "maximum_log_vector_rate_limit_degrees_per_frame",
                18.0,
            ),
            (
                "maximum_log_vector_acceleration_degrees_per_frame_squared",
                "maximum_log_vector_acceleration_limit_degrees_per_frame_squared",
                15.0,
            ),
        ),
        "throwing_lowerarm_component": (
            (
                "maximum_log_vector_rate_degrees_per_frame",
                "maximum_log_vector_rate_limit_degrees_per_frame",
                24.0,
            ),
            (
                "maximum_log_vector_acceleration_degrees_per_frame_squared",
                "maximum_log_vector_acceleration_limit_degrees_per_frame_squared",
                18.0,
            ),
        ),
    }
    for section_name, gates in nested_gates.items():
        section = continuity.get(section_name)
        if not isinstance(section, dict) or any(
            not _v7_observed_gate_passes(section, actual, limit, expected)
            for actual, limit, expected in gates
        ):
            raise AuthorError(
                f"v7 {section_name} continuity evidence exceeds fixed limits"
            )

    flat_gates = (
        (
            "maximum_hand_translation_cm_per_frame",
            "maximum_hand_translation_limit_cm_per_frame",
            12.25,
        ),
        (
            "maximum_grip_translation_cm_per_frame",
            "maximum_grip_translation_limit_cm_per_frame",
            14.25,
        ),
        (
            "maximum_hand_acceleration_cm_per_frame_squared",
            "maximum_hand_acceleration_limit_cm_per_frame_squared",
            10.0,
        ),
        (
            "maximum_grip_acceleration_cm_per_frame_squared",
            "maximum_grip_acceleration_limit_cm_per_frame_squared",
            10.25,
        ),
        (
            "maximum_elbow_delta_degrees_per_frame",
            "maximum_elbow_delta_limit_degrees_per_frame",
            45.0,
        ),
        (
            "maximum_elbow_bend_plane_delta_degrees_per_frame",
            "maximum_elbow_bend_plane_delta_limit_degrees_per_frame",
            60.0,
        ),
        (
            "maximum_pre_release_disc_plane_delta_degrees_per_frame",
            "maximum_pre_release_disc_plane_delta_limit_degrees",
            15.0,
        ),
        (
            "maximum_throwing_wrist_additive_degrees",
            "maximum_throwing_wrist_additive_limit_degrees",
            25.0,
        ),
    )
    if any(
        not _v7_observed_gate_passes(continuity, actual, limit, expected)
        for actual, limit, expected in flat_gates
    ):
        raise AuthorError("v7 full-path continuity evidence exceeds fixed limits")


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


def validate_v7_native_arm_gates(native_validation: dict[str, Any]) -> None:
    families = native_validation.get("families")
    if not isinstance(families, list):
        raise AuthorError("native v7 bilateral arm evidence is absent")
    by_name = {
        family.get("family"): family
        for family in families
        if isinstance(family, dict)
    }
    if set(by_name) != {"Drive", "Approach", "Putt"} or len(by_name) != 3:
        raise AuthorError("native v7 family identity differs")
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
        raise AuthorError("native v7 Drive bilateral arm gate identity differs")
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
        "v7_release_reach_ratio",
        "v7_release_elbow_angle_degrees",
        "v7_followthrough_reach_ratio",
        "v7_followthrough_elbow_angle_degrees",
        "v7_recovery_reach_ratio",
        "v7_recovery_elbow_angle_degrees",
        "v7_recovery_absolute_torso_lateral_cm",
        "v7_recovery_torso_vertical_cm",
    )
    if any(
        not isinstance(drive.get(field), (int, float))
        or isinstance(drive.get(field), bool)
        or not math.isfinite(float(drive[field]))
        for field in metric_fields
    ):
        raise AuthorError("native v7 Drive bilateral arm metrics are non-finite")
    t = V7_NATIVE_DRIVE_ARM_THRESHOLDS
    pocket_maximum_elbow_passed, release_minimum_elbow_passed = (
        v7_native_boundary_elbow_checks(drive)
    )
    if (
        float(drive["v7_support_arm_maximum_reach_ratio"])
            > t["support_maximum_reach_ratio"]
        or not t["support_minimum_elbow_angle_degrees"]
            <= float(drive["v7_support_arm_minimum_elbow_angle_degrees"])
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
        raise AuthorError(
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
    v6_thresholds = {
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
                family.get("arm_spatial_gate_thresholds"), v6_thresholds
            )
        ):
            raise AuthorError(
                f"native v7 {name} immutable v6 spatial contract differs"
            )


def target_file(project_root: Path, object_path: str) -> Path:
    package = object_path.split(".", 1)[0].removeprefix("/Game/")
    return project_root / "Content" / f"{package}.uasset"


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


def resolve_native(name_options: tuple[str, ...]) -> Callable[..., str]:
    utility = unreal.DiscGolfProductionMotionAuthoringUtility
    matches = [name for name in name_options if hasattr(utility, name)]
    if len(matches) != 1:
        raise AuthorError(
            f"expected one reflected native method from {name_options}, got {matches}"
        )
    return getattr(utility, matches[0])


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    try:
        temporary.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        temporary.replace(path)
    finally:
        if temporary.exists():
            temporary.unlink()


def rollback_targets(
    project_root: Path, target_objects: tuple[str, ...]
) -> dict[str, Any]:
    deleted_assets: list[str] = []
    deleted_files: list[str] = []
    errors: list[str] = []
    for object_path in reversed(target_objects):
        package = object_path.split(".", 1)[0]
        try:
            if unreal.EditorAssetLibrary.does_asset_exist(package):
                if unreal.EditorAssetLibrary.delete_asset(package):
                    deleted_assets.append(package)
                else:
                    errors.append(f"EditorAssetLibrary refused delete: {package}")
        except Exception as exc:
            errors.append(f"asset delete failed for {package}: {exc}")
    for object_path in reversed(target_objects):
        path = target_file(project_root, object_path)
        if not path.exists():
            continue
        try:
            path.unlink()
            deleted_files.append(str(path))
        except OSError as exc:
            errors.append(f"disk delete failed for {path}: {exc}")
    restored = all(
        not target_file(project_root, path).exists() for path in target_objects
    )
    return {
        "attempted": True,
        "deleted_assets": deleted_assets,
        "deleted_files": deleted_files,
        "errors": errors,
        "target_absence_restored": restored,
    }


def main() -> None:
    project_root = Path(unreal.Paths.project_dir()).resolve()
    command_line = unreal.SystemLibrary.get_command_line()
    user_dir, version, spec = validate_command_line(command_line, project_root)
    target_objects = spec["targets"]
    report_path = user_dir / REPORT_RELATIVE
    canonical_evidence = project_root / spec["canonical_evidence"]
    state: dict[str, Any] = {
        "schema": spec["evidence_schema"],
        "schema_version": spec["schema_version"],
        "recipe_version": version,
        "asset_revision": spec["asset_revision"],
        "generated_utc": utc_now(),
        "status": "FAIL_NOT_STARTED",
        "phase": "PREFLIGHT",
        "source_kind": "PROJECT_AUTHORED_PROCEDURAL",
        "performer": "NOT_APPLICABLE_PROCEDURAL_NO_CAPTURE",
        "derived_from_synthetic_fixture": False,
        "motion_capture_claim": False,
        "human_animation_approval": False,
        "human_disc_contact_approval": False,
        "product_art_approval": False,
        "release_ready": False,
        "target_objects": list(target_objects),
        "errors": [],
    }
    transaction_started = False
    try:
        if canonical_evidence.exists():
            raise AuthorError(f"immutable canonical evidence already exists: {canonical_evidence}")
        dirty_content, dirty_maps = dirty_packages()
        if dirty_content or dirty_maps:
            raise AuthorError(
                f"commandlet began with dirty packages: content={dirty_content}, maps={dirty_maps}"
            )
        recipe_path = project_root / spec["recipe"]
        recipe = strict_json(recipe_path.read_text(encoding="utf-8"), "motion recipe")
        if recipe.get("schema") != spec["recipe_schema"]:
            raise AuthorError("recipe schema differs from selected version")
        if recipe.get("recipe_id") != spec["recipe_id"]:
            raise AuthorError("recipe id differs from selected version")
        if (
            version == "v7"
            and recipe.get("v7_source_motion_design_contract")
            != V7_SOURCE_MOTION_DESIGN_CONTRACT
        ):
            raise AuthorError(
                "v7 source-motion design contract differs from the fixed "
                "source-only gate and fail-closed approval boundary"
            )
        if version == "v7":
            v6_recipe_path = (
                project_root
                / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json"
            )
            v6_recipe = strict_json(
                v6_recipe_path.read_text(encoding="utf-8"),
                "immutable v006 motion recipe",
            )
            validate_v7_append_only_scope(v6_recipe, recipe)
            state["v7_source_validation"] = validate_v7_source_lane(
                project_root, v6_recipe, recipe
            )
        if version in VERSIONED_MOTION:
            versioned_policy = {
                "schema_version": spec["schema_version"],
                "recipe_version": version,
                "asset_revision": spec["asset_revision"],
                "interpolation_mode": (
                    "CATMULL_ROM_BIOMECHANICAL_PHASE_SHAPED"
                    if version in GRANULAR_MOTION
                    else "CATMULL_ROM_PHASE_SHAPED"
                ),
                "world_motion_authority": (
                    "GAMEPLAY_PAWN_CAPSULE_PRESENTATION_ROOT_ONLY"
                ),
                "presentation_root_trajectory_enabled": True,
                "root_motion_enabled": False,
                "external_source_used": False,
                "motion_capture_claim": False,
                "human_animation_approval": False,
                "human_disc_contact_approval": False,
            }
            if version in GRANULAR_MOTION:
                versioned_policy.update({
                    "biomechanical_model": "RHBH_KINETIC_CHAIN_GRANULAR_V1",
                    "disc_contact_model": (
                        "REACHBACK_PLANE_WRIST_LAG_FINGER_RELEASE"
                    ),
                    "recovery_model": (
                        "THREE_BEAT_DECELERATION_RECENTER_SETTLE"
                    ),
                })
            if version == "v4":
                versioned_policy.update({
                    "rotation_space": (
                        "COMPONENT_SPACE_ADDITIVE_TO_REFERENCE"
                    ),
                })
            if version in {"v5", "v6", "v7"}:
                versioned_policy.update({
                    "rotation_space": (
                        "MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_ADDITIVE_TO_REFERENCE"
                    ),
                    "root_track_policy": (
                        "TRANSLATION_ONLY_REFERENCE_ROTATION_SCALE"
                    ),
                    "component_rotation_bones": [
                        "pelvis", "spine_01", "spine_02", "spine_03",
                        "spine_04", "neck_01", "head",
                    ],
                    "local_rotation_bones": [
                        "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
                        "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
                        "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r",
                        "calf_r", "foot_r", "ball_r", "thumb_01_r",
                        "thumb_02_r", "thumb_03_r", "index_01_r",
                        "index_02_r", "index_03_r", "middle_01_r",
                        "middle_02_r", "middle_03_r", "ring_01_r",
                        "ring_02_r", "ring_03_r", "pinky_01_r",
                        "pinky_02_r", "pinky_03_r",
                    ],
                })
            for key, expected in versioned_policy.items():
                if recipe.get(key) != expected:
                    raise AuthorError(
                        f"{version} recipe {key}={recipe.get(key)!r}, expected {expected!r}"
                    )
        if recipe.get("source_kind") != "PROJECT_AUTHORED_PROCEDURAL":
            raise AuthorError("recipe source kind differs")
        if recipe.get("derived_from_synthetic_fixture") is not False:
            raise AuthorError("recipe synthetic-derivation boundary differs")
        state["recipe"] = {
            "path": spec["recipe"].as_posix(),
            **identity(recipe_path),
            "recipe_id": recipe.get("recipe_id"),
            "recipe_version": version,
            "asset_revision": spec["asset_revision"],
        }
        for relative, expected in PROTECTED_SYNTHETIC.items():
            actual = identity(project_root / relative)
            if actual != {"exists": True, **expected}:
                raise AuthorError(f"protected synthetic identity differs: {relative}")
        save_actual = identity(project_root / SAVE_RELATIVE)
        if save_actual != {"exists": True, **SAVE_IDENTITY}:
            raise AuthorError("protected production save identity differs")
        if version in VERSIONED_MOTION:
            for object_path, expected in PROTECTED_V1.items():
                actual = identity(target_file(project_root, object_path))
                if actual != {"exists": True, **expected}:
                    raise AuthorError(
                        f"protected v001 production identity differs: {object_path}"
                    )
        if version in GRANULAR_MOTION:
            for object_path, expected in PROTECTED_V2.items():
                actual = identity(target_file(project_root, object_path))
                if actual != {"exists": True, **expected}:
                    raise AuthorError(
                        f"protected v002 production identity differs: {object_path}"
                    )
        if version in {"v4", "v5", "v6", "v7"}:
            for object_path, expected in PROTECTED_V3.items():
                actual = identity(target_file(project_root, object_path))
                if actual != {"exists": True, **expected}:
                    raise AuthorError(
                        f"protected v003 production identity differs: {object_path}"
                    )
            evidence_actual = identity(
                project_root / PROTECTED_V3_EVIDENCE["path"]
            )
            evidence_expected = {
                "exists": True,
                "bytes": PROTECTED_V3_EVIDENCE["bytes"],
                "sha256": PROTECTED_V3_EVIDENCE["sha256"],
            }
            if evidence_actual != evidence_expected:
                raise AuthorError("protected v003 authoring evidence identity differs")
        if version in {"v5", "v6", "v7"}:
            for object_path, expected in PROTECTED_V4.items():
                actual = identity(target_file(project_root, object_path))
                if actual != {"exists": True, **expected}:
                    raise AuthorError(
                        f"protected v004 production identity differs: {object_path}"
                    )
            evidence_actual = identity(
                project_root / PROTECTED_V4_EVIDENCE["path"]
            )
            evidence_expected = {
                "exists": True,
                "bytes": PROTECTED_V4_EVIDENCE["bytes"],
                "sha256": PROTECTED_V4_EVIDENCE["sha256"],
            }
            if evidence_actual != evidence_expected:
                raise AuthorError("protected v004 authoring evidence identity differs")
        if version in {"v6", "v7"}:
            for object_path, expected in PROTECTED_V5.items():
                actual = identity(target_file(project_root, object_path))
                if actual != {"exists": True, **expected}:
                    raise AuthorError(
                        f"protected v005 production identity differs: {object_path}"
                    )
            evidence_actual = identity(
                project_root / PROTECTED_V5_EVIDENCE["path"]
            )
            evidence_expected = {
                "exists": True,
                "bytes": PROTECTED_V5_EVIDENCE["bytes"],
                "sha256": PROTECTED_V5_EVIDENCE["sha256"],
            }
            if evidence_actual != evidence_expected:
                raise AuthorError("protected v005 authoring evidence identity differs")
        if version == "v7":
            for object_path, expected in PROTECTED_V6.items():
                actual = identity(target_file(project_root, object_path))
                if actual != {"exists": True, **expected}:
                    raise AuthorError(
                        f"protected v006 production identity differs: {object_path}"
                    )
            evidence_actual = identity(
                project_root / PROTECTED_V6_EVIDENCE["path"]
            )
            evidence_expected = {
                "exists": True,
                "bytes": PROTECTED_V6_EVIDENCE["bytes"],
                "sha256": PROTECTED_V6_EVIDENCE["sha256"],
            }
            if evidence_actual != evidence_expected:
                raise AuthorError("protected v006 authoring evidence identity differs")
        for object_path in target_objects:
            package = object_path.split(".", 1)[0]
            if unreal.EditorAssetLibrary.does_asset_exist(package):
                raise AuthorError(f"target asset already exists: {package}")
            if target_file(project_root, object_path).exists():
                raise AuthorError(f"target file already exists: {target_file(project_root, object_path)}")

        state["phase"] = "NATIVE_AUTHOR"
        transaction_started = True
        author = resolve_native(("author_production_motion_assets_for_version",))
        native_author = strict_json(author(version), "native author")
        state["native_author"] = native_author
        if not str(native_author.get("status", "")).startswith("PASS"):
            raise AuthorError(f"native author failed: {native_author}")
        if native_author.get("created_asset_count") != 7:
            raise AuthorError("native author did not create exactly seven assets")

        state["phase"] = "NATIVE_VALIDATE"
        validate = resolve_native(("validate_production_motion_assets_for_version",))
        native_validation = strict_json(validate(version), "native validation")
        state["native_validation"] = native_validation
        if not str(native_validation.get("status", "")).startswith("PASS"):
            raise AuthorError(f"native validation failed: {native_validation}")
        exact_counts = {
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
        for key, expected in exact_counts.items():
            if native_validation.get(key) != expected:
                raise AuthorError(
                    f"native validation {key}={native_validation.get(key)!r}, expected {expected!r}"
                )
        native_version = {
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
            "asset_revision": spec["asset_revision"],
            "recipe_id": spec["recipe_id"],
            "world_motion_authority": "GAMEPLAY_PAWN_CAPSULE",
            "presentation_root_trajectory_enabled": version in VERSIONED_MOTION,
        }
        for key, expected in native_version.items():
            if native_validation.get(key) != expected:
                raise AuthorError(
                    f"native validation {key}={native_validation.get(key)!r}, "
                    f"expected {expected!r}"
                )
        if version in GRANULAR_MOTION:
            for key, expected in {
                "interpolation_mode": "CATMULL_ROM_BIOMECHANICAL_PHASE_SHAPED",
                "biomechanical_model": "RHBH_KINETIC_CHAIN_GRANULAR_V1",
                "disc_contact_model": "REACHBACK_PLANE_WRIST_LAG_FINGER_RELEASE",
                "recovery_model": "THREE_BEAT_DECELERATION_RECENTER_SETTLE",
            }.items():
                if native_validation.get(key) != expected:
                    raise AuthorError(
                        f"native validation granular field differs: {key}"
                    )
            expected_metrics = {
                "Drive": (
                    (145, 1, 45, 23, 19)
                    if version == "v7"
                    else (35, 6, 11, 23, 19)
                ),
                "Approach": (36, 5, 10, 23, 19),
                "Putt": (30, 4, 10, 23, 19),
            }
            families = native_validation.get("families")
            if not isinstance(families, list) or len(families) != 3:
                raise AuthorError(
                    "native validation granular family metrics are absent"
                )
            for family in families:
                name = family.get("family")
                expected = expected_metrics.get(name)
                actual = (
                    family.get("pose_key_count"),
                    family.get("maximum_pose_key_gap_frames"),
                    family.get("recovery_pose_key_count"),
                    family.get("minimum_authored_rotation_channels_per_pose"),
                    family.get("curve_count"),
                )
                if expected is None or actual != expected:
                    raise AuthorError(
                        f"native validation granular density metrics differ for {name}"
                    )
        if version == "v4":
            for key, expected in {
                "rotation_space": "COMPONENT_SPACE_ADDITIVE_TO_REFERENCE",
                "local_track_construction": (
                    "COMPONENT_INTENT_CONVERTED_TO_LOCAL_BONE_TRACKS"
                ),
            }.items():
                if native_validation.get(key) != expected:
                    raise AuthorError(f"native validation v4 field differs: {key}")
            for family in native_validation.get("families", []):
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
                    or family.get("maximum_component_delta_degrees", 999.0)
                        > 95.0
                ):
                    raise AuthorError(
                        f"native validation v4 component metrics differ for "
                        f"{family.get('family')}"
                    )
        if version in {"v5", "v6", "v7"}:
            for key, expected in {
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
                if native_validation.get(key) != expected:
                    raise AuthorError(
                        f"native validation {version} field differs: {key}"
                    )
            for family in native_validation.get("families", []):
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
                    or not 0.998 <= family.get(
                        "minimum_joint_segment_ratio", 0.0
                    ) <= 1.002
                    or not 0.998 <= family.get(
                        "maximum_joint_segment_ratio", 0.0
                    ) <= 1.002
                    or family.get(
                        "maximum_component_intent_error_degrees", 999.0
                    ) > 0.05
                    or family.get("maximum_local_intent_error_degrees", 999.0)
                        > 0.05
                    or family.get("maximum_root_rotation_error_degrees", 999.0)
                        > 0.01
                    or maximum_local_frame_delta
                        > maximum_local_frame_delta_limit
                    or family.get("maximum_finger_frame_delta_degrees", 999.0)
                        > 20.0
                    or family.get("maximum_component_delta_degrees", 999.0)
                        > 95.0
                    or family.get("named_phase_knee_sample_count") != 16
                    or not 130.0 <= family.get(
                        "minimum_named_phase_knee_angle_degrees", 0.0
                    ) <= 176.0
                    or not 130.0 <= family.get(
                        "maximum_named_phase_knee_angle_degrees", 999.0
                    ) <= 176.0
                ):
                    raise AuthorError(
                        f"native validation {version} mixed-space metrics differ for "
                        f"{family.get('family')}"
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
            for family in native_validation.get("families", []):
                if (
                    family.get("arm_spatial_sample_count") != 3
                    or family.get("arm_spatial_gate_applied") is not True
                    or family.get("arm_spatial_gate_passed") is not True
                ):
                    raise AuthorError(
                        f"native validation v6 arm spatial gate differs for "
                        f"{family.get('family')}"
                    )
                if any(
                    not isinstance(family.get(field), (int, float))
                    or isinstance(family.get(field), bool)
                    or not math.isfinite(float(family[field]))
                    for field in scalar_fields
                ):
                    raise AuthorError(
                        f"native validation v6 arm spatial scalar differs for "
                        f"{family.get('family')}"
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
                        raise AuthorError(
                            f"native validation v6 arm direction differs for "
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
                        raise AuthorError(
                            f"native validation v6 hand position differs for "
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
                reachback, release, followthrough = hands
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
                    raise AuthorError(
                        f"native validation v6 spatial threshold evidence differs for "
                        f"{family.get('family')}"
                    )
        if version == "v7":
            validate_v7_native_arm_gates(native_validation)

        state["phase"] = "POSTCONDITIONS"
        assets: list[dict[str, Any]] = []
        for object_path in target_objects:
            package = object_path.split(".", 1)[0]
            path = target_file(project_root, object_path)
            if not unreal.EditorAssetLibrary.does_asset_exist(package) or not path.is_file():
                raise AuthorError(f"authored target is missing: {object_path}")
            assets.append(
                {
                    "object_path": object_path,
                    "package": package,
                    "file": path.relative_to(project_root).as_posix(),
                    **identity(path),
                }
            )
        state["authored_assets"] = assets
        for relative, expected in PROTECTED_SYNTHETIC.items():
            if identity(project_root / relative) != {"exists": True, **expected}:
                raise AuthorError(f"protected synthetic changed during authoring: {relative}")
        if identity(project_root / SAVE_RELATIVE) != {"exists": True, **SAVE_IDENTITY}:
            raise AuthorError("protected production save changed during authoring")
        if version in VERSIONED_MOTION:
            for object_path, expected in PROTECTED_V1.items():
                if identity(target_file(project_root, object_path)) != {
                    "exists": True,
                    **expected,
                }:
                    raise AuthorError(
                        f"protected v001 production changed during authoring: {object_path}"
                    )
        if version in GRANULAR_MOTION:
            for object_path, expected in PROTECTED_V2.items():
                if identity(target_file(project_root, object_path)) != {
                    "exists": True,
                    **expected,
                }:
                    raise AuthorError(
                        f"protected v002 production changed during authoring: {object_path}"
                    )
        if version in {"v4", "v5", "v6", "v7"}:
            for object_path, expected in PROTECTED_V3.items():
                if identity(target_file(project_root, object_path)) != {
                    "exists": True,
                    **expected,
                }:
                    raise AuthorError(
                        f"protected v003 production changed during authoring: {object_path}"
                    )
            evidence_actual = identity(
                project_root / PROTECTED_V3_EVIDENCE["path"]
            )
            evidence_expected = {
                "exists": True,
                "bytes": PROTECTED_V3_EVIDENCE["bytes"],
                "sha256": PROTECTED_V3_EVIDENCE["sha256"],
            }
            if evidence_actual != evidence_expected:
                raise AuthorError("protected v003 authoring evidence changed")
        if version in {"v5", "v6", "v7"}:
            for object_path, expected in PROTECTED_V4.items():
                if identity(target_file(project_root, object_path)) != {
                    "exists": True,
                    **expected,
                }:
                    raise AuthorError(
                        f"protected v004 production changed during authoring: {object_path}"
                    )
            evidence_actual = identity(
                project_root / PROTECTED_V4_EVIDENCE["path"]
            )
            evidence_expected = {
                "exists": True,
                "bytes": PROTECTED_V4_EVIDENCE["bytes"],
                "sha256": PROTECTED_V4_EVIDENCE["sha256"],
            }
            if evidence_actual != evidence_expected:
                raise AuthorError("protected v004 authoring evidence changed")
        if version in {"v6", "v7"}:
            for object_path, expected in PROTECTED_V5.items():
                if identity(target_file(project_root, object_path)) != {
                    "exists": True,
                    **expected,
                }:
                    raise AuthorError(
                        f"protected v005 production changed during authoring: {object_path}"
                    )
            evidence_actual = identity(
                project_root / PROTECTED_V5_EVIDENCE["path"]
            )
            evidence_expected = {
                "exists": True,
                "bytes": PROTECTED_V5_EVIDENCE["bytes"],
                "sha256": PROTECTED_V5_EVIDENCE["sha256"],
            }
            if evidence_actual != evidence_expected:
                raise AuthorError("protected v005 authoring evidence changed")
        if version == "v7":
            for object_path, expected in PROTECTED_V6.items():
                if identity(target_file(project_root, object_path)) != {
                    "exists": True,
                    **expected,
                }:
                    raise AuthorError(
                        f"protected v006 production changed during authoring: {object_path}"
                    )
            evidence_actual = identity(
                project_root / PROTECTED_V6_EVIDENCE["path"]
            )
            evidence_expected = {
                "exists": True,
                "bytes": PROTECTED_V6_EVIDENCE["bytes"],
                "sha256": PROTECTED_V6_EVIDENCE["sha256"],
            }
            if evidence_actual != evidence_expected:
                raise AuthorError("protected v006 authoring evidence changed")
        dirty_content_after, dirty_maps_after = dirty_packages()
        if dirty_content_after or dirty_maps_after:
            raise AuthorError(
                "authoring left dirty packages: "
                f"content={dirty_content_after}, maps={dirty_maps_after}"
            )
        state["protected_synthetic_asset_count"] = len(PROTECTED_SYNTHETIC)
        state["protected_synthetic_asset_reuse_count"] = 0
        state["protected_save"] = {
            "path": SAVE_RELATIVE.as_posix(),
            **identity(project_root / SAVE_RELATIVE),
        }
        state["runtime_binding_scope"] = "DRIVE_APPROACH_PUTT_TECHNICAL_CANDIDATE_SELECTION"
        state["cook_scope"] = "/Game/DiscGolf/Animation/ProductionMotion"
        state["world_motion_authority"] = "GAMEPLAY_PAWN_CAPSULE"
        state["presentation_root_trajectory_enabled"] = version in VERSIONED_MOTION
        state["fresh_shipping_candidate_inclusion_verified"] = False
        state["profile_matrix_live_validation_complete"] = False
        state["metahuman_live_validation_complete"] = False
        state["single_release_live_validation_complete"] = False
        state["status"] = (
            "PASS_AUTHORED_PROCEDURAL_CANDIDATES_HUMAN_REVIEW_REQUIRED"
        )
        state["phase"] = "COMPLETE"
        state["errors"] = []
        # The external run report is written first. If that write fails, the
        # exception path can still roll back the new packages without leaving a
        # canonical PASS beside missing assets. The immutable project evidence
        # is the final commit point; nothing mutating follows it.
        write_json(report_path, state)
        write_json(canonical_evidence, state)
        unreal.log(
            "DG_SESSION19_PRODUCTION_MOTION: PASS authored=7 families=3 "
            "synthetic_reuse=0 human_animation=false human_contact=false release_ready=false"
        )
    except Exception as exc:
        state["errors"].append(f"{type(exc).__name__}: {exc}")
        state["status"] = "FAIL_ROLLED_BACK" if transaction_started else "FAIL_PREFLIGHT"
        state["phase"] = "ROLLBACK" if transaction_started else state["phase"]
        if transaction_started:
            state["rollback"] = rollback_targets(project_root, target_objects)
        write_json(report_path, state)
        unreal.log_error(f"DG_SESSION19_PRODUCTION_MOTION: {state['status']}: {exc}")
        raise


if __name__ == "__main__":
    main()
