#!/usr/bin/env python3
"""Run the four-process Session 8B packaged MetaHuman acceptance lane.

The launcher owns one fresh external UUID UserDir across all four processes so
the real default profile can be applied and reloaded without touching the
production project save. It copies evidence out, validates pixels/metadata and
then removes only the marker-protected UUID directory it created.
"""

from __future__ import annotations

import argparse
import binascii
import copy
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
import uuid
import zlib


ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / "Content"
PROJECT_SAVE_DIR = ROOT / "Saved/SaveGames"
PRODUCTION_SAVE = PROJECT_SAVE_DIR / "DiscGolfTour_Profile_0.sav"
DEFAULT_ACCEPTED_BACKUP = Path(
    "C:/DGTour_Backups/Session7_Accepted/"
    "DiscGolfTour_Profile_0_Session7_Accepted.sav"
)
DEFAULT_REPORT = (
    ROOT / "Saved/CharacterFramework/"
    "Session8BMetaHumanPackagedAcceptance.json"
)
DEFAULT_EVIDENCE_ROOT = (
    ROOT / "Saved/CharacterFramework/"
    "S8BPkg"
)
DEFAULT_LOG_ROOT = ROOT / "Saved/Logs"
EXPECTED_USER_DIR_ROOT = Path(
    "C:/DGTour_TestRuns/Session8B_Packaged"
).resolve()
EXPECTED_ACCEPTED_SAVE_BYTES = 5212
EXPECTED_ACCEPTED_SAVE_SHA256 = (
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
)
SCHEMA = "DiscGolfTour.Session8BMetaHumanPackagedAcceptance.v3.4"
UNIT_WORLD_SCALE_TOLERANCE = 0.001
HAND_CORRECTION_MODE = "TARGET_RIGHT_ARM_TWO_BONE_TO_SOURCE_HAND_R"
EXPECTED_HAIR_GROOM = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/"
    "MHC_DG_Golfer_Default/Grooms/Hair_S_Clean.Hair_S_Clean"
)
EXPECTED_OUTFIT_MESH = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/"
    "MHC_DG_Golfer_Default/Clothing/MHC_DG_Golfer_Default_Outfits."
    "MHC_DG_Golfer_Default_Outfits"
)
EXPECTED_BODY_MESH = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/"
    "MHC_DG_Golfer_Default/Body/"
    "SKM_MHC_DG_Golfer_Default_BodyMesh."
    "SKM_MHC_DG_Golfer_Default_BodyMesh"
)
EXPECTED_RETARGET_ANIM_INSTANCE = (
    "/Script/DiscGolfTour.DiscGolfMetaHumanRetargetAnimInstance"
)
EXPECTED_SHIRT_MATERIAL_SLOT = "M_DG_bodyShapeD_Shirt"
EXPECTED_SHORT_MATERIAL_SLOT = "M_DG_bodyShapeD_Short"
EXPECTED_SHIRT_MATERIAL = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/"
    "MHC_DG_Golfer_Default/Clothing/"
    "MI_WI_DefaultGarment_M_DG_bodyShapeD_Shirt."
    "MI_WI_DefaultGarment_M_DG_bodyShapeD_Shirt"
)
EXPECTED_SHORT_MATERIAL = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/"
    "MHC_DG_Golfer_Default/Clothing/"
    "MI_WI_DefaultGarment_M_DG_bodyShapeD_Short."
    "MI_WI_DefaultGarment_M_DG_bodyShapeD_Short"
)
OUTFIT_SKIN_THRESHOLDS = {
    "shirt_minimum_knee_clearance_cm": 5.0,
    "short_minimum_knee_clearance_cm": -5.0,
    "garment_minimum_foot_clearance_cm": 20.0,
    "shirt_maximum_height_cm": 85.0,
    "short_maximum_height_cm": 60.0,
    "below_knee_tolerance_cm": 5.0,
    "below_foot_tolerance_cm": 1.0,
}
OUTFIT_SKIN_STRING_FIELDS = frozenset({
    "pose_sync_mode",
    "structural_reason",
    "semantic_reason",
    "fixed_function_support_reason",
    "active_morph_target_summary",
    "active_mesh_deformer_name",
    "active_mesh_deformer_path",
    "body_post_process_class_path",
    "renderer_fallback_bone_list",
    "renderer_fallback_summary",
    "renderer_fallback_reason",
})
OUTFIT_SKIN_FLOAT_FIELDS = frozenset({
    "knee_plane_body_z_cm",
    "foot_plane_body_z_cm",
    "knee_to_foot_span_cm",
})
OUTFIT_SKIN_INTEGER_FIELDS = frozenset({
    "actual_rendered_lod",
    "predicted_lod",
    "desired_sync_lod",
    "best_available_lod",
    "force_rendered_lod",
    "force_streamed_lod",
    "forced_lod_legacy_one_based",
    "computed_min_lod",
    "asset_min_lod",
    "current_first_lod",
    "pending_first_lod",
    "vertex_count",
    "section_count",
    "outfit_reference_bone_count",
    "body_reference_bone_count",
    "body_component_space_transform_count",
    "leader_bone_map_count",
    "used_outfit_bone_count",
    "missing_leader_mapping_count",
    "leader_name_mismatch_count",
    "checked_vertex_count",
    "checked_influence_count",
    "non_finite_vertex_count",
    "invalid_influence_count",
    "non_normalized_weight_vertex_count",
    "invalid_section_bone_map_count",
    "inactive_used_outfit_bone_count",
    "hidden_leader_bone_count",
    "invalid_leader_pose_bone_count",
    "renderer_fallback_bone_count",
    "morph_target_map_entry_count",
    "active_morph_target_count",
    "non_zero_morph_target_weight_count",
    "non_zero_morph_curve_count",
    "invalid_active_morph_target_count",
    "non_finite_morph_weight_count",
    "active_external_morph_target_count",
    "non_finite_external_morph_weight_count",
    "clothing_simulation_count",
    "body_bone_revision_before",
    "body_bone_revision_after",
})
OUTFIT_SKIN_BOOLEAN_FIELDS = frozenset({
    "mesh_object_present",
    "mesh_object_dynamic_data_valid",
    "position_cpu_access_requested",
    "position_data_present",
    "skin_weight_cpu_access_requested",
    "skin_weight_data_present",
    "skin_weight_lookup_present",
    "skin_weight_vertex_count_matches",
    "skin_weight_profile_pending",
    "using_skin_weight_profile",
    "every_vertex_covered_exactly_once",
    "ref_pose_override_present",
    "leader_safe_pose_validation_enabled",
    "leader_valid_mesh_pose_array_present",
    "body_post_process_disabled",
    "body_post_process_should_evaluate",
    "has_active_morph_targets",
    "has_active_external_morph_targets",
    "has_mesh_deformer",
    "has_clothing_simulation",
    "has_section_clothing_data",
    "has_world_position_offset_material",
    "fixed_function_path_complete",
    "structurally_valid",
    "semantic_accepted",
})
OUTFIT_SKIN_KEYS = (
    OUTFIT_SKIN_STRING_FIELDS
    | OUTFIT_SKIN_FLOAT_FIELDS
    | OUTFIT_SKIN_INTEGER_FIELDS
    | OUTFIT_SKIN_BOOLEAN_FIELDS
    | {"semantic_thresholds", "sections"}
)
OUTFIT_SKIN_SECTION_STRING_FIELDS = frozenset({
    "material_slot",
    "material_path",
    "semantic_reason",
    "worst_vertex_influences",
})
OUTFIT_SKIN_SECTION_VECTOR_FIELDS = frozenset({
    "bounds_minimum_body_cm",
    "bounds_maximum_body_cm",
    "worst_vertex_body_cm",
})
OUTFIT_SKIN_SECTION_FLOAT_FIELDS = frozenset({
    "height_cm",
    "knee_clearance_cm",
    "foot_clearance_cm",
})
OUTFIT_SKIN_SECTION_INTEGER_FIELDS = frozenset({
    "section_index",
    "authored_material_index",
    "resolved_material_index",
    "base_vertex_index",
    "vertex_count",
    "triangle_count",
    "checked_vertex_count",
    "below_knee_vertex_count",
    "below_foot_vertex_count",
    "worst_vertex_index",
})
OUTFIT_SKIN_SECTION_BOOLEAN_FIELDS = frozenset({
    "enabled",
    "has_clothing_data",
    "material_uses_world_position_offset",
    "semantic_accepted",
})
OUTFIT_SKIN_SECTION_KEYS = (
    OUTFIT_SKIN_SECTION_STRING_FIELDS
    | OUTFIT_SKIN_SECTION_VECTOR_FIELDS
    | OUTFIT_SKIN_SECTION_FLOAT_FIELDS
    | OUTFIT_SKIN_SECTION_INTEGER_FIELDS
    | OUTFIT_SKIN_SECTION_BOOLEAN_FIELDS
)
EXPECTED_CLOTHING_POST_PROCESS = (
    "/Game/DiscGolf/Characters/MetaHuman/Common/Animation/"
    "ABP_Clothing_PostProcess.ABP_Clothing_PostProcess_C"
)
EXPECTED_BODY_POST_PROCESS = (
    "/Game/DiscGolf/Characters/MetaHuman/Common/Body/"
    "ABP_Body_PostProcess.ABP_Body_PostProcess_C"
)
CAPTURE_STRING_FIELDS = frozenset({
    "filename",
    "scene",
    "backend_id",
    "active_visual_actor",
    "throw_phase",
    "presentation_hand_correction_mode",
})
CAPTURE_INTEGER_FIELDS = frozenset({
    "bytes",
    "width",
    "height",
    "strokes",
    "world_disc_count",
    "release_commit_count",
    "expected_strokes",
    "expected_world_disc_count",
    "expected_release_commit_count",
    "creator_semantic_point_count",
    "source_bone_revision_at_pre_update",
    "target_bone_revision_before_evaluate",
    "target_bone_revision_at_capture",
    "source_sample_frame_counter",
    "target_correction_frame_counter",
    "release_callback_frame_counter",
    "capture_freeze_frame_counter",
    "release_callback_event_order",
    "capture_freeze_event_order",
})
CAPTURE_FLOAT_FIELDS = frozenset({
    "disc_to_grip_distance_cm",
    "visible_disc_to_metahuman_hand_distance_cm",
    "source_hand_r_to_disc_grip_r_cm",
    "metahuman_hand_r_pre_to_source_hand_r_cm",
    "metahuman_hand_r_post_to_source_hand_r_cm",
})
CAPTURE_VECTOR3_FIELDS = frozenset({
    "grip_world_cm",
    "gameplay_disc_world_cm",
    "metahuman_hand_world_cm",
    "visible_disc_world_cm",
    "source_hand_r_world_cm",
    "source_disc_grip_r_world_cm",
    "metahuman_hand_r_pre_correction_world_cm",
    "metahuman_hand_r_post_correction_world_cm",
})
CAPTURE_VECTOR2_FIELDS = frozenset({
    "creator_semantic_min_px",
    "creator_semantic_max_px",
})
CAPTURE_BOOLEAN_FIELDS = frozenset({
    "creator_open",
    "dg_proxy_visible",
    "metahuman_ready",
    "held_disc_visible",
    "gameplay_disc_active",
    "release_evidence_collected",
    "immutable_release_grip_valid",
    "gameplay_disc_location_finite",
    "disc_to_grip_distance_finite",
    "released_disc_identity_verified",
    "flight_component_identity_verified",
    "flight_is_flying",
    "flight_tick_snapshot_present",
    "flight_tick_was_enabled_before_pause",
    "flight_tick_suspended",
    "world_paused_for_capture",
    "visible_hand_alignment_evidence_collected",
    "visible_hand_location_finite",
    "visible_disc_location_finite",
    "visible_hand_distance_finite",
    "presentation_hand_correction_snapshot_valid",
    "presentation_hand_correction_reachable",
    "presentation_hand_correction_applied",
    "creator_framing_evidence_collected",
    "creator_framing_accepted",
    "metahuman_render_evidence_collected",
    "metahuman_hair_render_evidence_collected",
    "metahuman_outfit_render_evidence_collected",
    "metahuman_outfit_live_skin_evidence_collected",
})
CAPTURE_COMMON_KEYS = (
    CAPTURE_STRING_FIELDS
    | CAPTURE_INTEGER_FIELDS
    | CAPTURE_FLOAT_FIELDS
    | CAPTURE_VECTOR3_FIELDS
    | CAPTURE_VECTOR2_FIELDS
    | CAPTURE_BOOLEAN_FIELDS
)
CAPTURE_METAHUMAN_OBJECT_KEYS = frozenset({
    "metahuman_body_render",
    "metahuman_head_render",
    "metahuman_outfit_render",
    "metahuman_hair_render",
    "metahuman_outfit_live_skin",
    "metahuman_presentation_policy",
})
CAPTURE_METAHUMAN_SCALE_BOOLEAN_FIELDS = frozenset({
    "metahuman_visual_root_uses_absolute_scale",
})
CAPTURE_METAHUMAN_SCALE_VECTOR3_FIELDS = frozenset({
    "metahuman_visual_root_world_scale",
    "metahuman_body_world_scale",
    "metahuman_head_world_scale",
    "metahuman_outfit_world_scale",
})
CAPTURE_METAHUMAN_SCALE_KEYS = (
    CAPTURE_METAHUMAN_SCALE_BOOLEAN_FIELDS
    | CAPTURE_METAHUMAN_SCALE_VECTOR3_FIELDS
)
PRESENTATION_POLICY_KEYS = frozenset({
    "quality_profile_id",
    "requested_policy",
    "verified_policy",
    "policy_verified",
    "policy_status",
    "active_visual_actor",
    "animation_source_component",
    "verified_forced_lod",
    "counters",
    "lod_sync",
    "skeletal_components",
    "groom_components",
    "outfit_required_bones_helper",
})
PRESENTATION_POLICY_COUNTER_KEYS = frozenset({
    "transition_success_count",
    "transition_failure_count",
    "rollback_success_count",
    "rollback_failure_count",
})
PRESENTATION_POLICY_LOD_SYNC_KEYS = frozenset({
    "component_path",
    "contract_status",
    "contract_valid",
    "num_lods",
    "min_lod",
    "forced_lod",
    "registered",
    "tick_enabled",
    "tick_even_when_paused",
    "components_to_sync",
    "custom_lod_mapping",
    "missing_declared_components",
})
PRESENTATION_POLICY_SYNC_ENTRY_KEYS = frozenset({
    "name", "sync_option", "component_present",
})
PRESENTATION_POLICY_MAPPING_KEYS = frozenset({"name", "mapping"})
PRESENTATION_POLICY_SKELETAL_KEYS = frozenset({
    "name",
    "component_path",
    "skeletal_mesh_path",
    "visibility_tick_option",
    "lod_count",
    "expected_mapped_lod",
    "forced_lod_legacy_one_based",
    "force_rendered_lod",
    "force_streamed_lod",
    "actual_rendered_lod",
    "predicted_lod",
    "desired_sync_lod",
    "best_available_lod",
    "registered",
    "visible_presentation",
    "tick_enabled",
    "tick_even_when_paused",
    "update_rate_optimizations",
    "source_tick_prerequisite",
})
PRESENTATION_POLICY_GROOM_KEYS = frozenset({
    "name",
    "component_path",
    "groom_asset_path",
    "expected_mapped_lod",
    "forced_lod",
    "force_rendered_lod",
    "force_streamed_lod",
    "desired_sync_lod",
    "best_available_lod",
    "mapped_by_lod_sync",
    "registered",
    "tick_enabled",
    "tick_even_when_paused",
})
PRESENTATION_POLICY_HELPER_KEYS = frozenset({
    "component_path",
    "leader_pose_component_path",
    "configured_actual_outfit_lod",
    "configured_predicted_outfit_lod",
    "configured_outfit_bone_count",
    "configured_ready_outfit_lod_count",
    "required_leader_bone_count",
    "mapped_outfit_used_leader_bone_count",
    "registered",
    "configured_for_body_and_outfit",
    "assetless",
    "visible",
    "hidden_in_game",
    "should_render",
    "render_in_main_pass",
    "visible_in_scene_capture_only",
    "can_ever_tick",
    "tick_enabled",
    "collision_disabled",
    "generate_overlap_events",
    "registered_as_body_follower",
})
PRESENTATION_POLICY_PROBE_KEYS = frozenset({
    "name",
    "classification",
    "invocation",
    "call_result",
    "expected_call_result",
    "actor_identity_preserved",
    "helper_identity_preserved",
    "stable_policy_state_preserved",
    "counter_delta",
    "before",
    "after",
})
EXPECTED_PRESENTATION_POLICY_SYNC_ENTRIES = (
    ("Body", "Drive", True),
    ("Face", "Drive", True),
    ("SkeletalMesh", "Passive", True),
    ("SkeletalMesh1", "Passive", False),
    ("SkeletalMesh2", "Passive", False),
    ("Hair", "Passive", True),
    ("Eyebrows", "Passive", True),
    ("Mustache", "Passive", True),
    ("Beard", "Passive", True),
)
EXPECTED_PRESENTATION_POLICY_MAPPINGS = (
    ("Hair", [3, 5, 7]),
    ("Beard", [3, 5, 7]),
    ("Mustache", [3, 5, 7]),
    ("Eyebrows", [3, 5, 7]),
    ("SkeletalMesh", [1, 2, 3]),
    ("SkeletalMesh1", [1, 2, 3]),
    ("SkeletalMesh2", [1, 2, 3]),
)
EXPECTED_PRESENTATION_POLICY_BY_CAPTURE = {
    "02_MH_Creator_Curated_FullBody.png": "CharacterCreator",
    "03_MH_Creator_CleanShaven_Hair_Closeup.png": "CharacterCreator",
    "05_MH_Gameplay_After_Cancel.png": "GameplayPerformance",
    "07_MH_RHBH_Grip.png": "GameplayPerformance",
    "08_MH_RHBH_Release.png": "GameplayPerformance",
    "09_MH_RHBH_FollowThrough.png": "GameplayPerformance",
}
PASS_PREFIX = "DG_SESSION8B_METAHUMAN_PACKAGED_ACCEPTANCE: PASS"
FAIL_PREFIX = "DG_SESSION8B_METAHUMAN_PACKAGED_ACCEPTANCE: FAIL"
AGGREGATE_PASS = "PASS_PACKAGED_METAHUMAN_RUNTIME_VISUAL_ACCEPTANCE"
ISOLATED_SAVE_RELATIVE = "Saved/SaveGames/DiscGolfTour_Profile_0.sav"


def require_recursive_finiteness(value: object, context: str) -> None:
    if type(value) is float and not math.isfinite(value):
        raise ValueError(f"non-finite JSON number at {context}")
    if isinstance(value, dict):
        for key, child in value.items():
            require_recursive_finiteness(child, f"{context}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            require_recursive_finiteness(child, f"{context}[{index}]")


def strict_json_loads(text: str) -> dict[str, object]:
    def reject_duplicate_keys(
        pairs: list[tuple[str, object]],
    ) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON object key: {key!r}")
            result[key] = value
        return result

    def reject_nonfinite_constant(token: str) -> object:
        raise ValueError(f"non-finite JSON numeric token: {token}")

    parsed = json.loads(
        text,
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=reject_nonfinite_constant,
    )
    if not isinstance(parsed, dict):
        raise ValueError("phase report root was not a JSON object")
    require_recursive_finiteness(parsed, "$")
    return parsed

PHASES = (
    (
        "apply_metahuman",
        "PASS_DG_TO_METAHUMAN_APPLY",
        (
            "01_DG_Creator_Sentinel_Baseline.png",
            "02_MH_Creator_Curated_FullBody.png",
            "03_MH_Creator_CleanShaven_Hair_Closeup.png",
        ),
    ),
    (
        "reload_cancel_failure_switch_dg",
        "PASS_RELOAD_CANCEL_FAILURE_ATOMICITY_AND_MH_TO_DG",
        (
            "04_DG_Creator_Preview_From_MH.png",
            "05_MH_Gameplay_After_Cancel.png",
        ),
    ),
    (
        "reload_dg_restore_metahuman_visual_throw",
        "PASS_RELOAD_DG_RESTORE_METAHUMAN_AND_RHBH",
        (
            "06_DG_Gameplay_Fresh_Reload.png",
            "07_MH_RHBH_Grip.png",
            "08_MH_RHBH_Release.png",
            "09_MH_RHBH_FollowThrough.png",
        ),
    ),
    (
        "metahuman_performance",
        "PASS_METAHUMAN_RENDERED_PERFORMANCE",
        (),
    ),
)
EXPECTED_CAPTURE_SCENES = {
    "01_DG_Creator_Sentinel_Baseline.png":
        "dg_creator_proxy_sentinel_baseline",
    "02_MH_Creator_Curated_FullBody.png":
        "metahuman_creator_curated_fixed_preset_full_body",
    "03_MH_Creator_CleanShaven_Hair_Closeup.png":
        "metahuman_creator_clean_shaven_hair_closeup",
    "04_DG_Creator_Preview_From_MH.png":
        "dg_creator_preview_from_saved_metahuman",
    "05_MH_Gameplay_After_Cancel.png":
        "metahuman_gameplay_after_byte_exact_cancel",
    "06_DG_Gameplay_Fresh_Reload.png":
        "dg_gameplay_fresh_process_reload",
    "07_MH_RHBH_Grip.png": "metahuman_rhbh_grip_before_release",
    "08_MH_RHBH_Release.png":
        "metahuman_rhbh_release_authoritative_disc_at_grip",
    "09_MH_RHBH_FollowThrough.png":
        "metahuman_rhbh_follow_through",
}

REQUIRED_REPORT_TOKENS = {
    "save_semantics": "isolated_profile_save_only",
    "backend_profile_id": "metahuman_assembled",
    "backend_preferred_quality_profile_id": "GameplayPerformance",
    "assembly_pipeline": "UE_OPTIMIZED",
    "assembly_optimization_level": "MEDIUM",
    "runtime_scalability_profile": "OmenGameplay1080pHighFoliageV1",
    "performance_route": "metahuman_creator_closeup_then_rhbh_gameplay",
    "customization_semantics": "CURATED_FIXED_PRESET",
    "facial_hair_semantics": "CLEAN_SHAVEN",
    "proxy_value_semantics": "PROXY_ONLY_VALUES_PRESERVED_NOT_APPLIED",
    "backend_failure_semantics": "FAILED_CANDIDATE_RETAINED_ACTIVE_VISUAL",
    "cosmetic_failure_semantics": (
        "UNAVAILABLE_CUSTOMIZATION_REJECTED_BEFORE_MUTATION"
    ),
    "release_authority": "DiscGolfTourGameMode.RequestThrowFromGrip",
    "flight_authority": "DiscActor.DiscFlightComponent",
    "animation_authority": "dg_master_animation_source_preserved",
    "avatar_scope": "metahuman_presentation_only",
}
PRESERVED_PROXY_FIELDS = {
    "preserved_proxy_face_preset_id": "face_square",
    "preserved_proxy_hair_style_id": "hair_medium",
    "preserved_proxy_facial_hair_id": "facialhair_beard",
    "preserved_proxy_eyebrow_id": "brow_alt",
    "preserved_proxy_scar_id": "scar_proxy",
    "preserved_proxy_tattoo_count": 1,
    "preserved_proxy_tattoo_id": "tattoo_proxy",
    "preserved_proxy_headwear_item_id": "proxy_s6_headwear_cap_01",
    "preserved_proxy_headwear_variant_id": "Default",
}
PERFORMANCE_CONTRACT = {
    "accepted_p95_frame_ms": 22.0,
    "accepted_hitch_count": 0,
    "hitch_frame_ms": 50.0,
    "accepted_used_physical_bytes": 3758096384,
    # C++ stores 33.34f, then SetNumberField promotes that float32 value to
    # double and UE's JSON writer emits it with %.17g.
    "hard_failure_p95_frame_ms": 33.34000015258789,
    "hard_failure_hitch_rate_percent": 1.0,
    "hard_failure_used_physical_bytes": 4294967296,
    "minimum_samples": 120,
    "maximum_samples": 600,
    "residency_warmup_seconds": 10.0,
    "segment_seconds": 15.0,
}
PERFORMANCE_CONTRACT_KEYS = frozenset(PERFORMANCE_CONTRACT)
PERFORMANCE_CONTRACT_INTEGER_KEYS = frozenset({
    "accepted_hitch_count",
    "accepted_used_physical_bytes",
    "hard_failure_used_physical_bytes",
    "minimum_samples",
    "maximum_samples",
})
PERFORMANCE_CONTRACT_NUMBER_KEYS = (
    PERFORMANCE_CONTRACT_KEYS - PERFORMANCE_CONTRACT_INTEGER_KEYS
)
PERFORMANCE_SEGMENT_KEYS = frozenset({
    "segment",
    "status",
    "duration_seconds",
    "sample_count",
    "average_frame_ms",
    "p95_frame_ms",
    "max_frame_ms",
    "hitch_count",
    "hitch_rate_percent",
    "used_physical_bytes",
    "sample_reduction",
    "timing_clock",
    "raw_frame_count",
    "raw_average_frame_ms",
    "raw_p95_frame_ms",
    "raw_max_frame_ms",
    "raw_hitch_count",
    "raw_hitch_rate_percent",
    "metahuman_presentation_policy_begin",
    "metahuman_presentation_policy_end",
})
PERFORMANCE_SEGMENT_STRING_KEYS = frozenset({
    "segment", "status", "sample_reduction", "timing_clock",
})
PERFORMANCE_SEGMENT_INTEGER_KEYS = frozenset({
    "sample_count",
    "hitch_count",
    "used_physical_bytes",
    "raw_frame_count",
    "raw_hitch_count",
})
PERFORMANCE_SEGMENT_NUMBER_KEYS = frozenset({
    "duration_seconds",
    "average_frame_ms",
    "p95_frame_ms",
    "max_frame_ms",
    "hitch_rate_percent",
    "raw_average_frame_ms",
    "raw_p95_frame_ms",
    "raw_max_frame_ms",
    "raw_hitch_rate_percent",
})
OUTFIT_REQUIRED_BONES_HELPER_NAME_RE = re.compile(
    r"DG_MetaHumanOutfitRequiredBones(?:_[0-9]+)?"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--self-test", action="store_true",
        help="Run fail-closed packaged scale-evidence validator tests and exit.",
    )
    parser.add_argument(
        "--executable",
        help="Packaged inner Binaries/Win64 DiscGolfTour executable.",
    )
    parser.add_argument(
        "--user-dir",
        help=(
            "Fresh C:/DGTour_TestRuns/Session8B_Packaged/<uuid> "
            "directory shared by the four phase processes."
        ),
    )
    parser.add_argument(
        "--accepted-backup", default=str(DEFAULT_ACCEPTED_BACKUP),
    )
    parser.add_argument("--report", default=str(DEFAULT_REPORT))
    parser.add_argument(
        "--evidence-root", default=str(DEFAULT_EVIDENCE_ROOT),
    )
    parser.add_argument("--log-root", default=str(DEFAULT_LOG_ROOT))
    parser.add_argument("--timeout-seconds", type=float, default=420.0)
    options = parser.parse_args()
    if not options.self_test:
        missing = [
            name for name in ("executable", "user_dir")
            if not getattr(options, name)
        ]
        if missing:
            parser.error(
                "the following arguments are required unless --self-test is used: "
                + ", ".join(f"--{name.replace('_', '-')}" for name in missing)
            )
    return options


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def file_record(path: Path) -> dict[str, object]:
    if not path.is_file():
        return {"present": False}
    stat = path.stat()
    return {
        "present": True,
        "bytes": stat.st_size,
        "mtime_ns": stat.st_mtime_ns,
        "sha256": sha256(path),
    }


def directory_snapshot(root: Path) -> dict[str, dict[str, object]]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): file_record(path)
        for path in sorted(root.rglob("*")) if path.is_file()
    }


def directory_topology(root: Path) -> list[str]:
    if not root.is_dir():
        return []
    return [
        path.relative_to(root).as_posix()
        for path in sorted(root.rglob("*")) if path.is_dir()
    ]


def changed_paths(
    before: dict[str, dict[str, object]],
    after: dict[str, dict[str, object]],
) -> list[str]:
    return sorted(
        key for key in before.keys() | after.keys()
        if before.get(key) != after.get(key)
    )


def snapshot_manifest_sha256(
    snapshot: dict[str, dict[str, object]],
) -> str:
    digest = hashlib.sha256()
    for relative_path in sorted(snapshot):
        record = snapshot[relative_path]
        digest.update(
            f"{relative_path}\t{record.get('bytes')}\t"
            f"{record.get('sha256')}\n".encode("utf-8")
        )
    return digest.hexdigest().upper()


def is_within(path: Path, directory: Path) -> bool:
    return path == directory or directory in path.parents


def _paeth(left: int, up: int, upper_left: int) -> int:
    predictor = left + up - upper_left
    left_distance = abs(predictor - left)
    up_distance = abs(predictor - up)
    upper_left_distance = abs(predictor - upper_left)
    if left_distance <= up_distance and left_distance <= upper_left_distance:
        return left
    if up_distance <= upper_left_distance:
        return up
    return upper_left


def decode_png_rgb_stats(path: Path) -> dict[str, object]:
    """Strictly decode an 8-bit RGB/RGBA PNG and return nonblank evidence."""
    payload = path.read_bytes()
    if payload[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG: {path}")
    offset = 8
    width = height = bit_depth = color_type = -1
    compression = filtering = interlace = -1
    idat = bytearray()
    saw_ihdr = False
    saw_iend = False
    while offset < len(payload):
        if offset + 12 > len(payload):
            raise ValueError(f"truncated PNG chunk: {path}")
        length = struct.unpack(">I", payload[offset:offset + 4])[0]
        chunk_type = payload[offset + 4:offset + 8]
        data_start = offset + 8
        data_end = data_start + length
        crc_end = data_end + 4
        if crc_end > len(payload):
            raise ValueError(f"truncated PNG chunk payload: {path}")
        chunk_data = payload[data_start:data_end]
        expected_crc = struct.unpack(">I", payload[data_end:crc_end])[0]
        actual_crc = binascii.crc32(chunk_type + chunk_data) & 0xFFFFFFFF
        if expected_crc != actual_crc:
            raise ValueError(f"PNG chunk CRC mismatch: {path}")
        if chunk_type == b"IHDR":
            if saw_ihdr or offset != 8 or length != 13:
                raise ValueError(f"invalid PNG IHDR: {path}")
            (
                width, height, bit_depth, color_type,
                compression, filtering, interlace,
            ) = struct.unpack(">IIBBBBB", chunk_data)
            saw_ihdr = True
        elif chunk_type == b"IDAT":
            if not saw_ihdr or saw_iend:
                raise ValueError(f"out-of-order PNG IDAT: {path}")
            idat.extend(chunk_data)
        elif chunk_type == b"IEND":
            if length != 0:
                raise ValueError(f"invalid PNG IEND: {path}")
            saw_iend = True
            offset = crc_end
            break
        offset = crc_end
    if not saw_ihdr or not saw_iend or offset != len(payload) or not idat:
        raise ValueError(f"incomplete PNG structure: {path}")
    if width != 1920 or height != 1080 \
            or bit_depth != 8 or color_type not in (2, 6) \
            or compression != 0 or filtering != 0 or interlace != 0:
        raise ValueError(
            f"unsupported PNG encoding {width}x{height} depth={bit_depth} "
            f"color={color_type} interlace={interlace}: {path}"
        )

    bytes_per_pixel = 3 if color_type == 2 else 4
    stride = width * bytes_per_pixel
    decoded = zlib.decompress(bytes(idat))
    expected_decoded_bytes = height * (stride + 1)
    if len(decoded) != expected_decoded_bytes:
        raise ValueError(
            f"PNG decoded byte count {len(decoded)} != "
            f"{expected_decoded_bytes}: {path}"
        )

    previous = bytearray(stride)
    rgb_digest = hashlib.sha256()
    luminance_sum = 0.0
    luminance_square_sum = 0.0
    luminance_min = 255.0
    luminance_max = 0.0
    dark_pixels = 0
    bright_pixels = 0
    decoded_offset = 0
    for _ in range(height):
        filter_type = decoded[decoded_offset]
        decoded_offset += 1
        filtered_row = decoded[decoded_offset:decoded_offset + stride]
        decoded_offset += stride
        row = bytearray(stride)
        for index, filtered_value in enumerate(filtered_row):
            left = row[index - bytes_per_pixel] \
                if index >= bytes_per_pixel else 0
            up = previous[index]
            upper_left = previous[index - bytes_per_pixel] \
                if index >= bytes_per_pixel else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            elif filter_type == 4:
                predictor = _paeth(left, up, upper_left)
            else:
                raise ValueError(f"unsupported PNG filter {filter_type}: {path}")
            row[index] = (filtered_value + predictor) & 0xFF

        rgb_row = bytearray(width * 3)
        for pixel_index in range(width):
            source_index = pixel_index * bytes_per_pixel
            target_index = pixel_index * 3
            red = row[source_index]
            green = row[source_index + 1]
            blue = row[source_index + 2]
            rgb_row[target_index:target_index + 3] = bytes((red, green, blue))
            luminance = (54.0 * red + 183.0 * green + 19.0 * blue) / 256.0
            luminance_sum += luminance
            luminance_square_sum += luminance * luminance
            luminance_min = min(luminance_min, luminance)
            luminance_max = max(luminance_max, luminance)
            dark_pixels += int(luminance <= 16.0)
            bright_pixels += int(luminance >= 239.0)
        rgb_digest.update(rgb_row)
        previous = row

    pixel_count = width * height
    luminance_mean = luminance_sum / pixel_count
    luminance_variance = max(
        0.0, luminance_square_sum / pixel_count - luminance_mean ** 2
    )
    luminance_stddev = math.sqrt(luminance_variance)
    dynamic_range = luminance_max - luminance_min
    dark_fraction = dark_pixels / pixel_count
    bright_fraction = bright_pixels / pixel_count
    if not 3.0 <= luminance_mean <= 252.0 \
            or luminance_stddev < 5.0 \
            or dynamic_range < 32.0 \
            or dark_fraction >= 0.99 \
            or bright_fraction >= 0.99:
        raise ValueError(
            f"decoded PNG is blank/degenerate mean={luminance_mean:.3f} "
            f"stddev={luminance_stddev:.3f} range={dynamic_range:.3f} "
            f"dark={dark_fraction:.6f} bright={bright_fraction:.6f}: {path}"
        )
    return {
        "width": width,
        "height": height,
        "bit_depth": bit_depth,
        "color_type": color_type,
        "pixel_count": pixel_count,
        "rgb_sha256": rgb_digest.hexdigest().upper(),
        "luminance_mean": luminance_mean,
        "luminance_stddev": luminance_stddev,
        "luminance_min": luminance_min,
        "luminance_max": luminance_max,
        "luminance_dynamic_range": dynamic_range,
        "dark_pixel_fraction": dark_fraction,
        "bright_pixel_fraction": bright_fraction,
        "nonblank_validation": "PASS_DECODED_RGB_NONBLANK",
    }


def terminate(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    if sys.platform == "win32":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    else:
        process.kill()
    try:
        process.wait(timeout=10.0)
    except subprocess.TimeoutExpired:
        process.kill()


def validate_paths(
    options: argparse.Namespace,
) -> tuple[Path, Path, Path, Path, Path, Path]:
    executable = Path(options.executable).expanduser().resolve()
    user_input = Path(options.user_dir).expanduser()
    if not user_input.is_absolute():
        raise ValueError("--user-dir must be absolute")
    user_dir = user_input.resolve()
    backup = Path(options.accepted_backup).expanduser().resolve()
    report = Path(options.report).expanduser().resolve()
    evidence_root = Path(options.evidence_root).expanduser().resolve()
    log_root = Path(options.log_root).expanduser().resolve()

    forbidden = ("unrealeditor", "unrealbuildtool", "automationtool", "runuat")
    executable_name = executable.name.casefold()
    if not executable.is_file() or executable.suffix.casefold() != ".exe":
        raise FileNotFoundError(f"packaged executable not found: {executable}")
    if not executable_name.startswith("discgolftour") or any(
        token in executable_name for token in forbidden
    ):
        raise ValueError("--executable must be a packaged DiscGolfTour game")
    if is_within(executable, ROOT) \
            or executable.parent.name.casefold() != "win64" \
            or executable.parent.parent.name.casefold() != "binaries":
        raise ValueError(
            "--executable must be the packaged inner Binaries/Win64 game exe"
        )
    packaged_game_root = executable.parents[2]
    paks = packaged_game_root / "Content/Paks"
    if not paks.is_dir() or not any(paks.glob("*.utoc")) \
            or not any(paks.glob("*.ucas")):
        raise ValueError("packaged executable has no adjacent IoStore content")

    try:
        run_uuid = uuid.UUID(user_dir.name)
    except ValueError as exc:
        raise ValueError("--user-dir final name must be a UUID") from exc
    if str(run_uuid) != user_dir.name.casefold():
        raise ValueError("--user-dir must use canonical lower-case UUID text")
    if user_dir.parent != EXPECTED_USER_DIR_ROOT:
        raise ValueError(
            "--user-dir must be exactly beneath "
            "C:/DGTour_TestRuns/Session8B_Packaged"
        )
    if user_dir.exists():
        raise FileExistsError(f"fresh --user-dir already exists: {user_dir}")
    if not user_dir.parent.is_dir():
        raise FileNotFoundError(
            f"external --user-dir parent must already exist: {user_dir.parent}"
        )
    if is_within(user_dir, ROOT) or is_within(user_dir, packaged_game_root):
        raise ValueError("--user-dir must be external to source and package")
    if is_within(executable, user_dir):
        raise ValueError("packaged executable may not be inside --user-dir")
    if is_within(backup, ROOT):
        raise ValueError("accepted backup must remain outside the repository")
    safe_character_framework_root = (
        ROOT / "Saved/CharacterFramework"
    ).resolve()
    safe_log_root = (ROOT / "Saved/Logs").resolve()
    if report.suffix.casefold() != ".json" \
            or report == safe_character_framework_root \
            or evidence_root == safe_character_framework_root \
            or not is_within(report, safe_character_framework_root) \
            or not is_within(evidence_root, safe_character_framework_root) \
            or not is_within(log_root, safe_log_root):
        raise ValueError(
            "report/evidence/log outputs must stay in the project Saved/"
            "CharacterFramework and Saved/Logs evidence roots"
        )
    for output in (report, evidence_root, log_root):
        if is_within(output, CONTENT) or is_within(output, PROJECT_SAVE_DIR) \
                or is_within(output, user_dir) \
                or is_within(output, packaged_game_root) \
                or output in (executable, backup):
            raise ValueError(
                "reports/logs overlap a protected Content, save, package, "
                "backup or UserDir boundary"
            )
    if report == evidence_root or is_within(report, evidence_root) \
            or is_within(evidence_root, report) \
            or report == log_root or is_within(report, log_root) \
            or is_within(log_root, report) \
            or is_within(evidence_root, log_root) \
            or is_within(log_root, evidence_root):
        raise ValueError("report, evidence and log outputs may not alias")
    if not 60.0 <= options.timeout_seconds <= 1800.0:
        raise ValueError("timeout must be between 60 and 1800 seconds per phase")

    if sys.platform == "win32":
        stable_run_dir = evidence_root / user_dir.name
        stable_outputs = [
            stable_run_dir / phase / filename
            for phase, _status, captures in PHASES
            for filename in (
                "LauncherConsole.log", "PhaseReport.json", *captures,
            )
        ]
        longest_output = max(stable_outputs, key=lambda path: len(str(path)))
        if len(str(longest_output)) >= 260:
            raise ValueError(
                "stable evidence path exceeds the Windows MAX_PATH boundary: "
                f"{len(str(longest_output))} characters: {longest_output}"
            )
    return executable, user_dir, backup, report, evidence_root, log_root


def require_exact_object(
    value: object,
    expected_keys: frozenset[str],
    context: str,
) -> dict[str, object]:
    if not isinstance(value, dict) or set(value) != expected_keys:
        actual_keys = set(value) if isinstance(value, dict) else set()
        raise ValueError(
            f"{context} exact keys drifted: "
            f"missing={sorted(expected_keys - actual_keys)!r} "
            f"extra={sorted(actual_keys - expected_keys)!r}"
        )
    return value


def require_exact_integer(value: object, context: str) -> int:
    if type(value) is not int:
        raise ValueError(f"{context} was not an exact JSON integer")
    return value


def require_exact_finite_number(
    value: object,
    context: str,
) -> int | float:
    if type(value) is int:
        return value
    if type(value) is float and math.isfinite(value):
        return value
    raise ValueError(f"{context} was not a finite JSON number")


def validate_performance_contract(
    value: object,
    context: str,
) -> dict[str, object]:
    contract = require_exact_object(
        value, PERFORMANCE_CONTRACT_KEYS, context,
    )
    require_recursive_finiteness(contract, context)
    for key in PERFORMANCE_CONTRACT_INTEGER_KEYS:
        require_exact_integer(contract[key], f"{context}.{key}")
    for key in PERFORMANCE_CONTRACT_NUMBER_KEYS:
        require_exact_finite_number(contract[key], f"{context}.{key}")
    if any(contract[key] != expected
           for key, expected in PERFORMANCE_CONTRACT.items()):
        raise ValueError(f"{context} threshold values drifted")
    return contract


def validate_performance_segment_shape(
    value: object,
    expected_segment: str,
    context: str,
) -> dict[str, object]:
    row = require_exact_object(value, PERFORMANCE_SEGMENT_KEYS, context)
    require_recursive_finiteness(row, context)
    if any(type(row[key]) is not str
           for key in PERFORMANCE_SEGMENT_STRING_KEYS):
        raise ValueError(f"{context} string field types drifted")
    for key in PERFORMANCE_SEGMENT_INTEGER_KEYS:
        require_exact_integer(row[key], f"{context}.{key}")
    for key in PERFORMANCE_SEGMENT_NUMBER_KEYS:
        require_exact_finite_number(row[key], f"{context}.{key}")
    if row["segment"] != expected_segment \
            or row["status"] != "PASS" \
            or any(row[key] < 0 for key in (
                *PERFORMANCE_SEGMENT_INTEGER_KEYS,
                *PERFORMANCE_SEGMENT_NUMBER_KEYS,
            )) \
            or any(row[key] <= 0 for key in (
                "average_frame_ms",
                "p95_frame_ms",
                "max_frame_ms",
                "raw_average_frame_ms",
                "raw_p95_frame_ms",
                "raw_max_frame_ms",
            )) \
            or row["duration_seconds"] < 15.0 \
            or not 120 <= row["sample_count"] <= 600 \
            or row["sample_reduction"] \
                != "CONTIGUOUS_BUCKET_MAXIMA_FULL_SEGMENT" \
            or row["timing_clock"] \
                != "FPlatformTime::Seconds_MONOTONIC" \
            or row["raw_frame_count"] < row["sample_count"] \
            or row["average_frame_ms"] > row["max_frame_ms"] \
            or row["p95_frame_ms"] > row["max_frame_ms"] \
            or row["p95_frame_ms"] > 22.0 \
            or row["hitch_count"] != 0 \
            or row["hitch_rate_percent"] != 0 \
            or row["max_frame_ms"] >= PERFORMANCE_CONTRACT["hitch_frame_ms"] \
            or row["raw_average_frame_ms"] > row["raw_max_frame_ms"] \
            or row["raw_p95_frame_ms"] > row["raw_max_frame_ms"] \
            or row["raw_p95_frame_ms"] > 22.0 \
            or row["raw_hitch_count"] != 0 \
            or row["raw_hitch_rate_percent"] != 0 \
            or row["raw_max_frame_ms"] \
                >= PERFORMANCE_CONTRACT["hitch_frame_ms"] \
            or row["used_physical_bytes"] > 3758096384:
        raise ValueError(f"{context} performance budget failed: {row!r}")
    return row


def validate_policy_counters(
    value: object,
    context: str,
    *,
    canonical_snapshot: bool = True,
) -> dict[str, int]:
    counters = require_exact_object(
        value, PRESENTATION_POLICY_COUNTER_KEYS, context,
    )
    result = {
        key: require_exact_integer(counters[key], f"{context}.{key}")
        for key in PRESENTATION_POLICY_COUNTER_KEYS
    }
    if any(count < 0 for count in result.values()):
        raise ValueError(f"{context} contained a negative counter")
    if canonical_snapshot and any(result[key] != 0 for key in (
        "transition_failure_count",
        "rollback_success_count",
        "rollback_failure_count",
    )):
        raise ValueError(
            f"{context} claimed a canonical policy failure/rollback: {result!r}"
        )
    return result


def validate_presentation_policy_snapshot(
    value: object,
    expected_policy: str,
    context: str,
    *,
    require_live_render_lods: bool,
    capture: dict[str, object] | None = None,
) -> dict[str, object]:
    policy = require_exact_object(value, PRESENTATION_POLICY_KEYS, context)
    if expected_policy not in {"CharacterCreator", "GameplayPerformance"}:
        raise ValueError(f"{context} requested an unknown expected policy")
    creator = expected_policy == "CharacterCreator"
    expected_forced_lod = 0 if creator else 2
    string_fields = (
        "quality_profile_id",
        "requested_policy",
        "verified_policy",
        "policy_status",
        "active_visual_actor",
        "animation_source_component",
    )
    if any(type(policy[key]) is not str for key in string_fields) \
            or type(policy["policy_verified"]) is not bool \
            or type(policy["verified_forced_lod"]) is not int:
        raise ValueError(f"{context} scalar types drifted")
    status = str(policy["policy_status"])
    if policy["quality_profile_id"] != "GameplayPerformance" \
            or policy["requested_policy"] != expected_policy \
            or policy["verified_policy"] != expected_policy \
            or policy["policy_verified"] is not True \
            or policy["verified_forced_lod"] != expected_forced_lod \
            or not policy["active_visual_actor"] \
            or not policy["animation_source_component"] \
            or not status.startswith(f"{expected_policy} policy state applied:") \
            or f"{expected_policy} policy verified:" not in status:
        raise ValueError(f"{context} public profile/mode/status state was not exact")
    validate_policy_counters(policy["counters"], f"{context}.counters")

    lod_sync = require_exact_object(
        policy["lod_sync"],
        PRESENTATION_POLICY_LOD_SYNC_KEYS,
        f"{context}.lod_sync",
    )
    lod_sync_string_fields = ("component_path", "contract_status")
    lod_sync_bool_fields = (
        "contract_valid", "registered", "tick_enabled", "tick_even_when_paused",
    )
    lod_sync_int_fields = ("num_lods", "min_lod", "forced_lod")
    if any(type(lod_sync[key]) is not str for key in lod_sync_string_fields) \
            or any(type(lod_sync[key]) is not bool for key in lod_sync_bool_fields) \
            or any(type(lod_sync[key]) is not int for key in lod_sync_int_fields):
        raise ValueError(f"{context}.lod_sync scalar types drifted")
    if not lod_sync["component_path"] \
            or lod_sync["contract_valid"] is not True \
            or not str(lod_sync["contract_status"]).startswith(
                "GameplayPerformance LOD sync schema verified:"
            ) \
            or lod_sync["registered"] is not True \
            or lod_sync["num_lods"] != 3 \
            or lod_sync["min_lod"] != 0 \
            or lod_sync["forced_lod"] != expected_forced_lod \
            or lod_sync["tick_enabled"] is not True \
            or lod_sync["tick_even_when_paused"] is not creator:
        raise ValueError(f"{context}.lod_sync effective policy was not exact")
    sync_entries = lod_sync["components_to_sync"]
    if not isinstance(sync_entries, list) \
            or len(sync_entries) != len(EXPECTED_PRESENTATION_POLICY_SYNC_ENTRIES):
        raise ValueError(f"{context}.lod_sync sync-entry count drifted")
    actual_sync_entries: list[tuple[str, str, bool]] = []
    for index, entry_value in enumerate(sync_entries):
        entry = require_exact_object(
            entry_value,
            PRESENTATION_POLICY_SYNC_ENTRY_KEYS,
            f"{context}.lod_sync.components_to_sync[{index}]",
        )
        if type(entry["name"]) is not str \
                or type(entry["sync_option"]) is not str \
                or type(entry["component_present"]) is not bool:
            raise ValueError(f"{context}.lod_sync sync-entry types drifted")
        actual_sync_entries.append((
            str(entry["name"]),
            str(entry["sync_option"]),
            bool(entry["component_present"]),
        ))
    if tuple(actual_sync_entries) != EXPECTED_PRESENTATION_POLICY_SYNC_ENTRIES:
        raise ValueError(f"{context}.lod_sync ordered sync schema drifted")
    mappings = lod_sync["custom_lod_mapping"]
    if not isinstance(mappings, list) \
            or len(mappings) != len(EXPECTED_PRESENTATION_POLICY_MAPPINGS):
        raise ValueError(f"{context}.lod_sync mapping count drifted")
    actual_mappings: list[tuple[str, list[int]]] = []
    for index, mapping_value in enumerate(mappings):
        mapping = require_exact_object(
            mapping_value,
            PRESENTATION_POLICY_MAPPING_KEYS,
            f"{context}.lod_sync.custom_lod_mapping[{index}]",
        )
        lods = mapping["mapping"]
        if type(mapping["name"]) is not str or not isinstance(lods, list) \
                or any(type(lod) is not int for lod in lods):
            raise ValueError(f"{context}.lod_sync mapping types drifted")
        actual_mappings.append((str(mapping["name"]), list(lods)))
    if tuple(actual_mappings) != EXPECTED_PRESENTATION_POLICY_MAPPINGS \
            or lod_sync["missing_declared_components"] != [
                "SkeletalMesh1", "SkeletalMesh2",
            ]:
        raise ValueError(f"{context}.lod_sync exact mappings/missing set drifted")

    skeletal = policy["skeletal_components"]
    expected_skeletal_names = ("Body", "Face", "SkeletalMesh")
    expected_skeletal_lods = (
        (0, 0, 1) if creator else (2, 2, 3)
    )
    if not isinstance(skeletal, list) or len(skeletal) != 3:
        raise ValueError(f"{context}.skeletal_components count drifted")
    expected_tick_option = (
        "AlwaysTickPoseAndRefreshBones"
        if creator else "OnlyTickPoseWhenRendered"
    )
    for index, mesh_value in enumerate(skeletal):
        mesh = require_exact_object(
            mesh_value,
            PRESENTATION_POLICY_SKELETAL_KEYS,
            f"{context}.skeletal_components[{index}]",
        )
        string_keys = (
            "name", "component_path", "skeletal_mesh_path", "visibility_tick_option",
        )
        integer_keys = (
            "lod_count", "expected_mapped_lod",
            "forced_lod_legacy_one_based", "force_rendered_lod",
            "force_streamed_lod", "actual_rendered_lod", "predicted_lod",
            "desired_sync_lod", "best_available_lod",
        )
        bool_keys = (
            "registered", "visible_presentation", "tick_enabled",
            "tick_even_when_paused", "update_rate_optimizations",
            "source_tick_prerequisite",
        )
        if any(type(mesh[key]) is not str for key in string_keys) \
                or any(type(mesh[key]) is not int for key in integer_keys) \
                or any(type(mesh[key]) is not bool for key in bool_keys):
            raise ValueError(f"{context} skeletal row types drifted")
        expected_lod = expected_skeletal_lods[index]
        if mesh["name"] != expected_skeletal_names[index] \
                or not mesh["component_path"] \
                or not mesh["skeletal_mesh_path"] \
                or mesh["lod_count"] <= expected_lod \
                or mesh["expected_mapped_lod"] != expected_lod \
                or mesh["forced_lod_legacy_one_based"] != expected_lod + 1 \
                or mesh["force_rendered_lod"] != expected_lod \
                or mesh["force_streamed_lod"] != expected_lod \
                or not 0 <= mesh["best_available_lod"] <= expected_lod \
                or mesh["registered"] is not True \
                or mesh["visible_presentation"] is not True \
                or mesh["tick_enabled"] is not True \
                or mesh["tick_even_when_paused"] is not creator \
                or mesh["update_rate_optimizations"] is not False \
                or mesh["source_tick_prerequisite"] is not True \
                or mesh["visibility_tick_option"] != expected_tick_option:
            raise ValueError(
                f"{context} skeletal policy was not exact for "
                f"{expected_skeletal_names[index]}"
            )
        if require_live_render_lods and (
            not 0 <= mesh["actual_rendered_lod"] <= expected_lod
            or not 0 <= mesh["predicted_lod"] <= expected_lod
        ):
            raise ValueError(
                f"{context} {mesh['name']} actual/predicted LOD was unavailable "
                "or worse than its forced mapped tier"
            )

    grooms = policy["groom_components"]
    expected_groom_names = (
        "Hair", "Eyebrows", "Fuzz", "Eyelashes", "Mustache", "Beard",
    )
    mapped_grooms = {"Hair", "Eyebrows", "Mustache", "Beard"}
    expected_groom_lod = 3 if creator else 7
    if not isinstance(grooms, list) or len(grooms) != len(expected_groom_names):
        raise ValueError(f"{context}.groom_components count drifted")
    for index, groom_value in enumerate(grooms):
        groom = require_exact_object(
            groom_value,
            PRESENTATION_POLICY_GROOM_KEYS,
            f"{context}.groom_components[{index}]",
        )
        string_keys = ("name", "component_path", "groom_asset_path")
        integer_keys = (
            "expected_mapped_lod", "forced_lod", "force_rendered_lod",
            "force_streamed_lod", "desired_sync_lod", "best_available_lod",
        )
        bool_keys = (
            "mapped_by_lod_sync", "registered", "tick_enabled",
            "tick_even_when_paused",
        )
        if any(type(groom[key]) is not str for key in string_keys) \
                or any(type(groom[key]) is not int for key in integer_keys) \
                or any(type(groom[key]) is not bool for key in bool_keys):
            raise ValueError(f"{context} Groom row types drifted")
        name = expected_groom_names[index]
        mapped = name in mapped_grooms
        if groom["name"] != name or not groom["component_path"] \
                or groom["mapped_by_lod_sync"] is not mapped \
                or groom["registered"] is not True \
                or groom["tick_enabled"] is not True \
                or groom["tick_even_when_paused"] is not creator:
            raise ValueError(f"{context} Groom tick/mapping state failed for {name}")
        if mapped and (
            groom["expected_mapped_lod"] != expected_groom_lod
            or groom["forced_lod"] != expected_groom_lod
            or groom["force_rendered_lod"] != expected_groom_lod
            or groom["force_streamed_lod"] != -1
        ):
            raise ValueError(f"{context} mapped Groom LOD failed for {name}")
        if not mapped and groom["expected_mapped_lod"] != -1:
            raise ValueError(f"{context} {name} was not explicitly unmapped")

    helper = require_exact_object(
        policy["outfit_required_bones_helper"],
        PRESENTATION_POLICY_HELPER_KEYS,
        f"{context}.outfit_required_bones_helper",
    )
    helper_string_keys = ("component_path", "leader_pose_component_path")
    helper_integer_keys = (
        "configured_actual_outfit_lod",
        "configured_predicted_outfit_lod",
        "configured_outfit_bone_count",
        "configured_ready_outfit_lod_count",
        "required_leader_bone_count",
        "mapped_outfit_used_leader_bone_count",
    )
    helper_bool_keys = tuple(PRESENTATION_POLICY_HELPER_KEYS - set(
        helper_string_keys + helper_integer_keys
    ))
    if any(type(helper[key]) is not str for key in helper_string_keys) \
            or any(type(helper[key]) is not int for key in helper_integer_keys) \
            or any(type(helper[key]) is not bool for key in helper_bool_keys):
        raise ValueError(f"{context} helper scalar types drifted")
    body_path = str(skeletal[0]["component_path"])
    _helper_owner_path, helper_separator, helper_name = str(
        helper["component_path"]
    ).rpartition(".")
    if helper_separator != "." \
            or OUTFIT_REQUIRED_BONES_HELPER_NAME_RE.fullmatch(helper_name) is None \
            or helper["leader_pose_component_path"] != body_path \
            or helper["configured_actual_outfit_lod"] < -1 \
            or helper["configured_predicted_outfit_lod"] < 0 \
            or any(helper[key] <= 0 for key in (
                "configured_outfit_bone_count",
                "configured_ready_outfit_lod_count",
                "required_leader_bone_count",
                "mapped_outfit_used_leader_bone_count",
            )) \
            or helper["registered"] is not True \
            or helper["configured_for_body_and_outfit"] is not True \
            or helper["assetless"] is not True \
            or helper["visible"] is not False \
            or helper["hidden_in_game"] is not True \
            or helper["should_render"] is not False \
            or helper["render_in_main_pass"] is not False \
            or helper["visible_in_scene_capture_only"] is not False \
            or helper["can_ever_tick"] is not False \
            or helper["tick_enabled"] is not False \
            or helper["collision_disabled"] is not True \
            or helper["generate_overlap_events"] is not False \
            or helper["registered_as_body_follower"] is not True:
        raise ValueError(f"{context} helper identity/config/render/tick state failed")

    if capture is not None:
        if policy["active_visual_actor"] != capture["active_visual_actor"]:
            raise ValueError(f"{context} active actor contradicted its capture")
        render_links = (
            (skeletal[0], capture["metahuman_body_render"]),
            (skeletal[1], capture["metahuman_head_render"]),
            (skeletal[2], capture["metahuman_outfit_render"]),
        )
        if any(not isinstance(render, dict) for _mesh, render in render_links):
            raise ValueError(f"{context} render cross-link was not an object")
        if any(
            mesh["component_path"] != render.get("component_path")
            or mesh["skeletal_mesh_path"] != render.get("skeletal_mesh_path")
            for mesh, render in render_links
        ):
            raise ValueError(f"{context} skeletal identity contradicted render evidence")
        hair = grooms[0]
        hair_render = capture["metahuman_hair_render"]
        if not isinstance(hair_render, dict) \
                or hair["component_path"] != hair_render.get("component_path") \
                or hair["groom_asset_path"] != hair_render.get("resource_path"):
            raise ValueError(f"{context} Hair identity contradicted render evidence")
    return policy


def presentation_policy_identity_projection(
    policy: dict[str, object],
) -> tuple[object, ...]:
    lod_sync = policy["lod_sync"]
    skeletal = policy["skeletal_components"]
    grooms = policy["groom_components"]
    helper = policy["outfit_required_bones_helper"]
    assert isinstance(lod_sync, dict)
    assert isinstance(skeletal, list)
    assert isinstance(grooms, list)
    assert isinstance(helper, dict)
    return (
        policy["quality_profile_id"],
        policy["active_visual_actor"],
        policy["animation_source_component"],
        lod_sync["component_path"],
        lod_sync["num_lods"],
        lod_sync["min_lod"],
        json.dumps(lod_sync["components_to_sync"], sort_keys=True),
        json.dumps(lod_sync["custom_lod_mapping"], sort_keys=True),
        json.dumps(lod_sync["missing_declared_components"], sort_keys=True),
        tuple(
            (row["name"], row["component_path"], row["skeletal_mesh_path"])
            for row in skeletal
        ),
        tuple(
            (row["name"], row["component_path"], row["groom_asset_path"])
            for row in grooms
        ),
        helper["component_path"],
        helper["leader_pose_component_path"],
    )


def presentation_policy_stable_projection(
    policy: dict[str, object],
) -> tuple[object, ...]:
    lod_sync = policy["lod_sync"]
    skeletal = policy["skeletal_components"]
    grooms = policy["groom_components"]
    helper = policy["outfit_required_bones_helper"]
    assert isinstance(lod_sync, dict)
    assert isinstance(skeletal, list)
    assert isinstance(grooms, list)
    assert isinstance(helper, dict)
    return (
        presentation_policy_identity_projection(policy),
        policy["requested_policy"],
        policy["verified_policy"],
        policy["policy_verified"],
        policy["verified_forced_lod"],
        policy["policy_status"],
        policy["counters"],
        lod_sync["forced_lod"],
        lod_sync["registered"],
        lod_sync["tick_enabled"],
        lod_sync["tick_even_when_paused"],
        tuple(
            (
                row["expected_mapped_lod"],
                row["forced_lod_legacy_one_based"],
                row["force_rendered_lod"],
                row["force_streamed_lod"],
                row["registered"],
                row["visible_presentation"],
                row["tick_enabled"],
                row["tick_even_when_paused"],
                row["visibility_tick_option"],
                row["update_rate_optimizations"],
                row["source_tick_prerequisite"],
            )
            for row in skeletal
        ),
        tuple(
            (
                row["mapped_by_lod_sync"],
                row["expected_mapped_lod"],
                row["forced_lod"],
                row["force_rendered_lod"],
                row["force_streamed_lod"],
                row["registered"],
                row["tick_enabled"],
                row["tick_even_when_paused"],
            )
            for row in grooms
        ),
        json.dumps(helper, sort_keys=True),
    )


def policy_counter_delta(
    before: dict[str, object],
    after: dict[str, object],
) -> dict[str, int]:
    before_counters = require_exact_object(
        before["counters"],
        PRESENTATION_POLICY_COUNTER_KEYS,
        "policy counter delta before",
    )
    after_counters = require_exact_object(
        after["counters"],
        PRESENTATION_POLICY_COUNTER_KEYS,
        "policy counter delta after",
    )
    return {
        key: require_exact_integer(
            after_counters[key], f"policy counter delta after.{key}"
        ) - require_exact_integer(
            before_counters[key], f"policy counter delta before.{key}"
        )
        for key in PRESENTATION_POLICY_COUNTER_KEYS
    }


def validate_presentation_policy_probe(
    value: object,
    expected_name: str,
    expected_classification: str,
    expected_invocation: str,
    expected_before_policy: str,
    expected_after_policy: str,
    expected_call_result: bool,
    expected_counter_delta: dict[str, int],
    expected_stable_preserved: bool,
    context: str,
) -> dict[str, object]:
    probe = require_exact_object(value, PRESENTATION_POLICY_PROBE_KEYS, context)
    string_keys = ("name", "classification", "invocation")
    bool_keys = (
        "call_result", "expected_call_result", "actor_identity_preserved",
        "helper_identity_preserved", "stable_policy_state_preserved",
    )
    if any(type(probe[key]) is not str for key in string_keys) \
            or any(type(probe[key]) is not bool for key in bool_keys):
        raise ValueError(f"{context} probe scalar types drifted")
    before = validate_presentation_policy_snapshot(
        probe["before"], expected_before_policy, f"{context}.before",
        require_live_render_lods=False,
    )
    after = validate_presentation_policy_snapshot(
        probe["after"], expected_after_policy, f"{context}.after",
        require_live_render_lods=False,
    )
    recorded_delta = validate_policy_counters(
        probe["counter_delta"],
        f"{context}.counter_delta",
        canonical_snapshot=False,
    )
    actual_delta = policy_counter_delta(before, after)
    if probe["name"] != expected_name \
            or probe["classification"] != expected_classification \
            or probe["invocation"] != expected_invocation \
            or probe["call_result"] is not expected_call_result \
            or probe["expected_call_result"] is not expected_call_result \
            or probe["actor_identity_preserved"] is not True \
            or probe["helper_identity_preserved"] is not True \
            or probe["stable_policy_state_preserved"] \
                is not expected_stable_preserved \
            or recorded_delta != expected_counter_delta \
            or actual_delta != expected_counter_delta:
        raise ValueError(f"{context} result/identity/counter contract failed")
    if presentation_policy_identity_projection(before) \
            != presentation_policy_identity_projection(after):
        raise ValueError(f"{context} changed visual/component/helper identity")
    stable_equal = presentation_policy_stable_projection(before) \
        == presentation_policy_stable_projection(after)
    if stable_equal is not expected_stable_preserved:
        raise ValueError(f"{context} stable policy-state classification was false")
    return probe


def validate_performance_policy_chain(
    transition_probes: list[object],
    performance_policies: list[
        tuple[dict[str, object], dict[str, object]]
    ],
    transition_success_delta: dict[str, int],
) -> None:
    if len(transition_probes) != 4 or len(performance_policies) != 2:
        raise ValueError("phase 4 performance policy-chain cardinality drifted")
    probes = [
        require_exact_object(
            value,
            PRESENTATION_POLICY_PROBE_KEYS,
            f"phase 4 transition probe {index}",
        )
        for index, value in enumerate(transition_probes)
    ]
    creator_begin = performance_policies[0][0]
    creator_end = performance_policies[0][1]
    gameplay_begin = performance_policies[1][0]
    chain_values = (
        probes[0]["after"],
        probes[1]["before"],
        probes[1]["after"],
        probes[2]["before"],
        probes[2]["after"],
        probes[3]["before"],
        probes[3]["after"],
    )
    if any(not isinstance(value, dict) for value in chain_values):
        raise ValueError("phase 4 performance policy-chain state was not an object")
    (
        entry_after,
        creator_no_op_before,
        creator_no_op_after,
        exit_before,
        exit_after,
        gameplay_no_op_before,
        gameplay_no_op_after,
    ) = chain_values
    between_delta = policy_counter_delta(creator_end, gameplay_begin)
    if presentation_policy_identity_projection(creator_end) \
            != presentation_policy_identity_projection(gameplay_begin) \
            or between_delta != transition_success_delta \
            or presentation_policy_stable_projection(entry_after) \
            != presentation_policy_stable_projection(creator_no_op_before) \
            or presentation_policy_stable_projection(creator_no_op_after) \
            != presentation_policy_stable_projection(creator_begin) \
            or presentation_policy_stable_projection(creator_end) \
            != presentation_policy_stable_projection(exit_before) \
            or presentation_policy_stable_projection(exit_after) \
            != presentation_policy_stable_projection(gameplay_no_op_before) \
            or presentation_policy_stable_projection(gameplay_no_op_after) \
            != presentation_policy_stable_projection(gameplay_begin) \
            or presentation_policy_stable_projection(gameplay_begin) \
            != presentation_policy_stable_projection(exit_after):
        raise ValueError(
            "phase 4 entry/no-op/performance/exit chain did not preserve "
            "adjacent policy state with exactly one creator-to-gameplay "
            "transition outside both timed windows"
        )


def validate_metahuman_unit_world_scale_evidence(
    capture: dict[str, object],
    filename: str,
    *,
    expects_metahuman: bool,
) -> None:
    """Require exact fixed-preset scale evidence only on MetaHuman captures."""
    present_keys = CAPTURE_METAHUMAN_SCALE_KEYS & set(capture)
    if not expects_metahuman:
        if present_keys:
            raise ValueError(
                f"DG capture {filename} unexpectedly included MetaHuman "
                f"scale evidence: {sorted(present_keys)!r}"
            )
        return
    if present_keys != CAPTURE_METAHUMAN_SCALE_KEYS:
        missing = sorted(CAPTURE_METAHUMAN_SCALE_KEYS - present_keys)
        raise ValueError(
            f"MetaHuman capture {filename} omitted fixed-preset scale "
            f"evidence: {missing!r}"
        )
    if capture["metahuman_visual_root_uses_absolute_scale"] is not True:
        raise ValueError(
            f"MetaHuman capture {filename} did not isolate inherited root scale"
        )
    for key in CAPTURE_METAHUMAN_SCALE_VECTOR3_FIELDS:
        value = capture[key]
        if not isinstance(value, dict) or set(value) != {"x", "y", "z"}:
            raise ValueError(
                f"MetaHuman capture {filename} {key} was not an exact xyz object"
            )
        components: list[float] = []
        for axis in ("x", "y", "z"):
            component = value[axis]
            if type(component) not in (int, float):
                raise ValueError(
                    f"MetaHuman capture {filename} {key}.{axis} was not numeric"
                )
            numeric = float(component)
            if not math.isfinite(numeric):
                raise ValueError(
                    f"MetaHuman capture {filename} {key}.{axis} was not finite"
                )
            components.append(numeric)
        if any(
            not math.isclose(
                component,
                1.0,
                rel_tol=0.0,
                abs_tol=UNIT_WORLD_SCALE_TOLERANCE,
            )
            for component in components
        ):
            raise ValueError(
                f"MetaHuman capture {filename} {key} was not unit world scale: "
                f"{tuple(components)!r}"
            )


def validate_phase_report(
    report: dict[str, object],
    phase: str,
    status: str,
    run_id: str,
    expected_captures: tuple[str, ...],
) -> None:
    if report.get("schema") != SCHEMA \
            or report.get("status") != status \
            or report.get("phase_status") != status \
            or report.get("phase") != phase \
            or report.get("run_id") != run_id:
        raise ValueError(f"phase report identity/status mismatch for {phase}")
    if report.get("runtime_mode") != "packaged" \
            or report.get("fallback_used") is not False \
            or report.get("manual_visual_review_required") is not True \
            or report.get("manual_visual_review_required_int") != 1 \
            or report.get("practice_snapshot_save_suppressed") is not True:
        raise ValueError(f"phase report runtime/safety boundary failed for {phase}")
    expected_user_dir = (EXPECTED_USER_DIR_ROOT / run_id).resolve()
    expected_save_path = (
        expected_user_dir / ISOLATED_SAVE_RELATIVE
    ).resolve()
    if Path(str(report.get("user_dir", ""))).resolve() != expected_user_dir \
            or Path(str(report.get("isolated_save_path", ""))).resolve() \
            != expected_save_path \
            or report.get("resolution_x") != 1920 \
            or report.get("resolution_y") != 1080 \
            or "d3d12" not in str(report.get("rhi", "")).casefold() \
            or not report.get("gpu_brand") \
            or any(token in str(report.get("gpu_brand", "")).casefold()
                   for token in ("software", "microsoft basic render")):
        raise ValueError(f"phase {phase} rendered/UserDir boundary was false")
    expected_save_mutation = phase != PHASES[3][0]
    if report.get("save_mutated_by_phase") is not expected_save_mutation:
        raise ValueError(
            f"phase {phase} save-mutation disclosure did not match "
            f"{expected_save_mutation}"
        )
    for key, value in REQUIRED_REPORT_TOKENS.items():
        if report.get(key) != value:
            raise ValueError(f"phase {phase} report token {key!r} was not {value!r}")
    for key, value in PRESERVED_PROXY_FIELDS.items():
        if report.get(key) != value:
            raise ValueError(
                f"phase {phase} preserved proxy field {key!r} "
                f"was not {value!r}"
            )
    captures = report.get("captures")
    if not isinstance(captures, list):
        raise ValueError(f"phase {phase} capture list missing")
    names = tuple(
        row.get("filename") for row in captures if isinstance(row, dict)
    )
    if names != expected_captures:
        raise ValueError(
            f"phase {phase} capture order {names!r} != {expected_captures!r}"
        )
    for row in captures:
        if not isinstance(row, dict):
            raise ValueError(f"phase {phase} contained non-object capture metadata")
        filename = str(row.get("filename", ""))
        if row.get("scene") != EXPECTED_CAPTURE_SCENES.get(filename):
            raise ValueError(f"capture {filename} scene token was not frozen")
        expects_mh = filename.startswith(("02_", "03_", "05_", "07_", "08_", "09_"))
        expected_capture_keys = CAPTURE_COMMON_KEYS | (
            CAPTURE_METAHUMAN_OBJECT_KEYS | CAPTURE_METAHUMAN_SCALE_KEYS
            if expects_mh else frozenset()
        )
        if set(row) != expected_capture_keys:
            missing = sorted(expected_capture_keys - set(row))
            extra = sorted(set(row) - expected_capture_keys)
            raise ValueError(
                f"capture {filename} v3.4 keys drifted: "
                f"missing={missing!r} extra={extra!r}"
            )
        if any(type(row[key]) is not str for key in CAPTURE_STRING_FIELDS) \
                or any(type(row[key]) is not int
                       for key in CAPTURE_INTEGER_FIELDS) \
                or any(type(row[key]) not in (int, float)
                       for key in CAPTURE_FLOAT_FIELDS) \
                or any(not math.isfinite(float(row[key]))
                       for key in CAPTURE_FLOAT_FIELDS) \
                or any(type(row[key]) is not bool
                       for key in CAPTURE_BOOLEAN_FIELDS):
            raise ValueError(f"capture {filename} v3.4 scalar types drifted")
        validate_metahuman_unit_world_scale_evidence(
            row,
            filename,
            expects_metahuman=expects_mh,
        )

        def exact_finite_vector(
            field_name: str,
            axes: tuple[str, ...],
        ) -> tuple[float, ...]:
            value = row[field_name]
            if not isinstance(value, dict) or set(value) != set(axes) \
                    or any(type(value[axis]) not in (int, float)
                           for axis in axes):
                raise ValueError(
                    f"capture {filename} {field_name} was not an exact vector"
                )
            result = tuple(float(value[axis]) for axis in axes)
            if not all(math.isfinite(component) for component in result):
                raise ValueError(
                    f"capture {filename} {field_name} was not finite"
                )
            return result

        capture_vectors3 = {
            key: exact_finite_vector(key, ("x", "y", "z"))
            for key in CAPTURE_VECTOR3_FIELDS
        }
        for key in CAPTURE_VECTOR2_FIELDS:
            exact_finite_vector(key, ("x", "y"))

        expects_hand_alignment = filename.startswith(("07_", "08_"))
        if not expects_hand_alignment:
            correction_defaults_exact = (
                row["presentation_hand_correction_mode"] == "INACTIVE"
                and row["presentation_hand_correction_snapshot_valid"] is False
                and row["presentation_hand_correction_reachable"] is False
                and row["presentation_hand_correction_applied"] is False
                and all(row[key] == -1 for key in (
                    "source_hand_r_to_disc_grip_r_cm",
                    "metahuman_hand_r_pre_to_source_hand_r_cm",
                    "metahuman_hand_r_post_to_source_hand_r_cm",
                ))
                and all(row[key] == 0 for key in (
                    "source_bone_revision_at_pre_update",
                    "target_bone_revision_before_evaluate",
                    "target_bone_revision_at_capture",
                    "source_sample_frame_counter",
                    "target_correction_frame_counter",
                    "release_callback_frame_counter",
                    "capture_freeze_frame_counter",
                    "release_callback_event_order",
                    "capture_freeze_event_order",
                ))
                and all(capture_vectors3[key] == (0.0, 0.0, 0.0)
                        for key in (
                            "source_hand_r_world_cm",
                            "source_disc_grip_r_world_cm",
                            "metahuman_hand_r_pre_correction_world_cm",
                            "metahuman_hand_r_post_correction_world_cm",
                        ))
            )
            if not correction_defaults_exact:
                raise ValueError(
                    f"capture {filename} claimed hand correction outside 07/08"
                )
        expected_backend = "metahuman_assembled" if expects_mh else "dg_master"
        if row.get("backend_id") != expected_backend \
                or row.get("metahuman_ready") is not expects_mh \
                or row.get("dg_proxy_visible") is expects_mh \
                or bool(row.get("active_visual_actor")) is not expects_mh:
            raise ValueError(f"capture {filename} backend/proxy scene metadata failed")
        if expects_mh:
            expected_policy = EXPECTED_PRESENTATION_POLICY_BY_CAPTURE.get(filename)
            if expected_policy is None:
                raise ValueError(
                    f"capture {filename} lacked an explicit frozen policy mode"
                )
            capture_policy = validate_presentation_policy_snapshot(
                row.get("metahuman_presentation_policy"),
                expected_policy,
                f"capture {filename}.metahuman_presentation_policy",
                require_live_render_lods=True,
                capture=row,
            )
            if row.get("metahuman_render_evidence_collected") is not True:
                raise ValueError(
                    f"capture {filename} omitted live MetaHuman render evidence"
                )
            if row.get("metahuman_outfit_render_evidence_collected") is not True:
                raise ValueError(
                    f"capture {filename} omitted live MetaHuman outfit evidence"
                )
            if row.get("metahuman_outfit_live_skin_evidence_collected") is not True:
                raise ValueError(
                    f"capture {filename} omitted the live Outfit skin diagnostic"
                )
            body_render_post_process_path = ""
            for mesh_key in (
                "metahuman_body_render",
                "metahuman_head_render",
                "metahuman_outfit_render",
            ):
                mesh = row.get(mesh_key)
                if not isinstance(mesh, dict):
                    raise ValueError(
                        f"capture {filename} omitted {mesh_key} evidence"
                    )
                required_true = (
                    "registered",
                    "visible",
                    "should_render",
                    "render_state_created",
                    "recently_rendered",
                    "render_in_main_pass",
                    "bounds_finite",
                    "pose_finite",
                    "projected_viewport_overlap",
                    "supported_pose_source",
                )
                required_false = (
                    "hidden_in_game",
                    "owner_no_see",
                    "only_owner_see",
                    "visible_in_scene_capture_only",
                )
                if any(mesh.get(key) is not True for key in required_true) \
                        or any(mesh.get(key) is not False for key in required_false):
                    raise ValueError(
                        f"capture {filename} {mesh_key} did not prove a live "
                        "main-view renderable primitive"
                    )
                lod_count = int(mesh.get("lod_count", 0))
                material_count = int(mesh.get("material_count", 0))
                reference_bones = int(mesh.get("reference_bone_count", 0))
                pose_bones = int(mesh.get("component_space_transform_count", 0))
                radius = float(mesh.get("bounds_radius_cm", -1.0))
                is_outfit = mesh_key == "metahuman_outfit_render"
                if not mesh.get("component_path") \
                        or not mesh.get("skeletal_mesh_path") \
                        or lod_count <= 0 \
                        or material_count <= 0 \
                        or reference_bones <= 0 \
                        or (not is_outfit and pose_bones != reference_bones) \
                        or not math.isfinite(radius) \
                        or radius <= 0.0:
                    raise ValueError(
                        f"capture {filename} {mesh_key} resource/pose counts "
                        "were not complete"
                    )
                if mesh_key == "metahuman_body_render":
                    component_path = mesh.get("component_path")
                    animation_instance = mesh.get(
                        "animation_instance_class_path"
                    )
                    post_process_path = mesh.get("post_process_class_path")
                    pose_source_path = mesh.get("pose_source_component_path")
                    if mesh.get("skeletal_mesh_path") != EXPECTED_BODY_MESH \
                            or animation_instance \
                            != EXPECTED_RETARGET_ANIM_INSTANCE \
                            or not isinstance(post_process_path, str) \
                            or mesh.get("leader_pose_component_path") != "" \
                            or pose_source_path != component_path \
                            or mesh.get("pose_sync_mode") != "DIRECT":
                        raise ValueError(
                            f"capture {filename} Body did not use the exact "
                            "retargeted fixed-preset runtime seam"
                        )
                    body_render_post_process_path = post_process_path
                if is_outfit:
                    leader = str(mesh.get("leader_pose_component_path", ""))
                    anim_instance = str(
                        mesh.get("animation_instance_class_path", "")
                    )
                    post_process = str(mesh.get("post_process_class_path", ""))
                    pose_source = str(mesh.get("pose_source_component_path", ""))
                    pose_mode = str(mesh.get("pose_sync_mode", ""))
                    body_path = str(
                        row["metahuman_body_render"].get("component_path", "")
                    )
                    leader_branch = (
                        leader == body_path
                        and not anim_instance
                        and not post_process
                        and pose_source == body_path
                        and pose_mode == "LEADER_POSE_BODY"
                    )
                    post_process_branch = (
                        not leader
                        and not anim_instance
                        and post_process == EXPECTED_CLOTHING_POST_PROCESS
                        and pose_source == body_path
                        and pose_mode
                        == "POST_PROCESS_COPY_POSE_FROM_ATTACHED_BODY"
                    )
                    anchor_distance = float(mesh.get(
                        "maximum_shared_anchor_distance_cm", math.nan
                    ))
                    if mesh.get("skeletal_mesh_path") != EXPECTED_OUTFIT_MESH \
                            or not (leader_branch or post_process_branch) \
                            or mesh.get("shared_anchors_converged") is not True \
                            or int(mesh.get("shared_anchor_count", 0)) != 5 \
                            or not math.isfinite(anchor_distance) \
                            or not 0.0 <= anchor_distance <= 5.0:
                        raise ValueError(
                            f"capture {filename} outfit did not use the exact "
                            "generated mesh, supported pose-sync branch, and "
                            "converged body anchors"
                        )
            outfit_skin = row.get("metahuman_outfit_live_skin")
            if not isinstance(outfit_skin, dict):
                raise ValueError(
                    f"capture {filename} omitted metahuman_outfit_live_skin"
                )
            if set(outfit_skin) != OUTFIT_SKIN_KEYS \
                    or any(type(outfit_skin[key]) is not str
                           for key in OUTFIT_SKIN_STRING_FIELDS) \
                    or any(type(outfit_skin[key]) is not int
                           for key in OUTFIT_SKIN_INTEGER_FIELDS) \
                    or any(
                        type(outfit_skin[key]) not in (int, float)
                        for key in OUTFIT_SKIN_FLOAT_FIELDS
                    ) \
                    or any(type(outfit_skin[key]) is not bool
                           for key in OUTFIT_SKIN_BOOLEAN_FIELDS):
                raise ValueError(
                    f"capture {filename} live Outfit skin schema/types drifted"
                )
            required_skin_true = (
                "mesh_object_present",
                "mesh_object_dynamic_data_valid",
                "position_cpu_access_requested",
                "position_data_present",
                "skin_weight_cpu_access_requested",
                "skin_weight_data_present",
                "skin_weight_lookup_present",
                "skin_weight_vertex_count_matches",
                "every_vertex_covered_exactly_once",
                "structurally_valid",
            )
            if any(outfit_skin.get(key) is not True for key in required_skin_true):
                raise ValueError(
                    f"capture {filename} live Outfit skin structure was incomplete"
                )
            actual_lod = int(outfit_skin.get("actual_rendered_lod", -1))
            vertex_count = int(outfit_skin.get("vertex_count", 0))
            section_count = int(outfit_skin.get("section_count", 0))
            outfit_bones = int(outfit_skin.get("outfit_reference_bone_count", 0))
            body_bones = int(outfit_skin.get("body_reference_bone_count", 0))
            body_pose = int(outfit_skin.get(
                "body_component_space_transform_count", 0
            ))
            leader_map = int(outfit_skin.get("leader_bone_map_count", 0))
            knee_z = float(outfit_skin.get("knee_plane_body_z_cm", math.nan))
            foot_z = float(outfit_skin.get("foot_plane_body_z_cm", math.nan))
            knee_foot_span = float(outfit_skin.get(
                "knee_to_foot_span_cm", math.nan
            ))
            ref_pose_override_present = outfit_skin.get(
                "ref_pose_override_present"
            )
            body_post_process_disabled = outfit_skin.get(
                "body_post_process_disabled"
            )
            body_post_process_should_evaluate = outfit_skin.get(
                "body_post_process_should_evaluate"
            )
            body_post_process_class_path = outfit_skin.get(
                "body_post_process_class_path"
            )
            leader_safe_pose_validation_enabled = outfit_skin.get(
                "leader_safe_pose_validation_enabled"
            )
            leader_valid_mesh_pose_array_present = outfit_skin.get(
                "leader_valid_mesh_pose_array_present"
            )
            if not isinstance(ref_pose_override_present, bool) \
                    or not isinstance(body_post_process_disabled, bool) \
                    or not isinstance(body_post_process_should_evaluate, bool) \
                    or not isinstance(body_post_process_class_path, str) \
                    or not isinstance(
                        leader_safe_pose_validation_enabled, bool
                    ) \
                    or not isinstance(
                        leader_valid_mesh_pose_array_present, bool
                    ):
                raise ValueError(
                    f"capture {filename} omitted canonical runtime input state"
                )
            if body_post_process_class_path != body_render_post_process_path:
                raise ValueError(
                    f"capture {filename} contradicted the Body post-process path"
                )
            canonical_runtime_inputs = (
                ref_pose_override_present is False
                and body_post_process_disabled is False
                and body_post_process_should_evaluate is True
                and body_post_process_class_path == EXPECTED_BODY_POST_PROCESS
            )
            expected_noncanonical_tokens: list[str] = []
            if ref_pose_override_present:
                expected_noncanonical_tokens.append(
                    "NONCANONICAL_REF_POSE_OVERRIDE_PRESENT"
                )
            if body_post_process_disabled:
                expected_noncanonical_tokens.append(
                    "NONCANONICAL_BODY_POST_PROCESS_DISABLED"
                )
            if not body_post_process_should_evaluate:
                expected_noncanonical_tokens.append(
                    "NONCANONICAL_BODY_POST_PROCESS_NOT_EVALUATING"
                )
            if body_post_process_class_path != EXPECTED_BODY_POST_PROCESS:
                expected_noncanonical_tokens.append(
                    "NONCANONICAL_BODY_POST_PROCESS_CLASS "
                    f"expected={EXPECTED_BODY_POST_PROCESS} "
                    f"actual={body_post_process_class_path}"
                )
            hidden_leader_bones = int(outfit_skin.get(
                "hidden_leader_bone_count", -1
            ))
            invalid_leader_pose_bones = int(outfit_skin.get(
                "invalid_leader_pose_bone_count", -1
            ))
            renderer_fallback_bones = int(outfit_skin.get(
                "renderer_fallback_bone_count", -1
            ))
            used_outfit_bones = int(outfit_skin.get(
                "used_outfit_bone_count", 0
            ))
            fallback_bone_text = outfit_skin.get(
                "renderer_fallback_bone_list"
            )
            fallback_summary_text = outfit_skin.get(
                "renderer_fallback_summary"
            )
            fallback_reason = outfit_skin.get("renderer_fallback_reason")
            fallback_bone_items = (
                [] if not fallback_bone_text else fallback_bone_text.split(",")
            )
            fallback_summary_items = (
                [] if not fallback_summary_text
                else fallback_summary_text.split(",")
            )
            renderer_checks_valid_pose = (
                leader_safe_pose_validation_enabled
                and leader_valid_mesh_pose_array_present
            )
            fallback_identity_valid = all(
                item.endswith("]") and "[" in item
                and item.rsplit("[", 1)[0]
                and item.rsplit("[", 1)[1][:-1].isdigit()
                for item in fallback_bone_items
            )
            fallback_summary_valid = all(
                summary.startswith(bone + ">")
                and (
                    summary.endswith(":REF_LOCAL_ROOT")
                    or ":REF_LOCAL_PARENT=" in summary
                )
                for bone, summary in zip(
                    fallback_bone_items, fallback_summary_items, strict=True
                )
            ) if len(fallback_bone_items) == len(fallback_summary_items) else False
            if hidden_leader_bones < 0 \
                    or invalid_leader_pose_bones < 0 \
                    or renderer_fallback_bones < 0 \
                    or hidden_leader_bones > used_outfit_bones \
                    or invalid_leader_pose_bones > used_outfit_bones \
                    or renderer_fallback_bones > outfit_bones \
                    or (
                        not leader_valid_mesh_pose_array_present
                        and invalid_leader_pose_bones != 0
                    ) \
                    or (
                        not renderer_checks_valid_pose
                        and renderer_fallback_bones != 0
                    ) \
                    or (
                        renderer_checks_valid_pose
                        and renderer_fallback_bones
                        < invalid_leader_pose_bones
                    ) \
                    or len(fallback_bone_items) != renderer_fallback_bones \
                    or len(fallback_summary_items) != renderer_fallback_bones \
                    or len(set(fallback_bone_items)) \
                    != renderer_fallback_bones \
                    or not fallback_identity_valid \
                    or not fallback_summary_valid \
                    or fallback_reason != (
                        "NONE" if renderer_fallback_bones == 0
                        else "LEADER_VALID_MESH_POSE_FALSE_SAFE_MODE"
                    ):
                raise ValueError(
                    f"capture {filename} renderer pose-fallback evidence drifted"
                )
            canonical_runtime_inputs = not expected_noncanonical_tokens
            structural_reason = outfit_skin.get("structural_reason")
            if outfit_skin.get("pose_sync_mode") != "LEADER_POSE_BODY" \
                    or structural_reason not in {
                        "PASS",
                        "PASS_DIAGNOSTIC_CPU_FIXED_FUNCTION_BASE_ONLY",
                    } \
                    or not 0 <= actual_lod < 4 \
                    or vertex_count <= 0 \
                    or section_count != 2 \
                    or outfit_bones != 342 \
                    or body_bones != 342 \
                    or body_pose != body_bones \
                    or leader_map != outfit_bones \
                    or int(outfit_skin.get("missing_leader_mapping_count", -1)) != 0 \
                    or int(outfit_skin.get("leader_name_mismatch_count", -1)) != 0 \
                    or int(outfit_skin.get("invalid_section_bone_map_count", -1)) != 0 \
                    or int(outfit_skin.get(
                        "inactive_used_outfit_bone_count", -1
                    )) != 0 \
                    or used_outfit_bones <= 0 \
                    or int(outfit_skin.get("invalid_influence_count", -1)) != 0 \
                    or int(outfit_skin.get(
                        "non_normalized_weight_vertex_count", -1
                    )) != 0 \
                    or int(outfit_skin.get("non_finite_vertex_count", -1)) != 0 \
                    or outfit_skin.get("skin_weight_profile_pending") is not False \
                    or outfit_skin.get("using_skin_weight_profile") is not False \
                    or not all(math.isfinite(value) for value in (
                        knee_z, foot_z, knee_foot_span,
                    )) \
                    or not 10.0 <= knee_foot_span <= 100.0:
                raise ValueError(
                    f"capture {filename} live Outfit skin identity/pose was invalid"
                )
            thresholds = outfit_skin.get("semantic_thresholds")
            if not isinstance(thresholds, dict) \
                    or set(thresholds) != set(OUTFIT_SKIN_THRESHOLDS) \
                    or any(type(value) not in (int, float)
                           for value in thresholds.values()) \
                    or any(
                not math.isclose(
                    float(thresholds.get(key, math.nan)), value,
                    rel_tol=0.0, abs_tol=1e-6,
                )
                for key, value in OUTFIT_SKIN_THRESHOLDS.items()
            ):
                raise ValueError(
                    f"capture {filename} Outfit silhouette thresholds drifted"
                )
            gpu_only_flags = (
                "has_active_morph_targets",
                "has_active_external_morph_targets",
                "has_mesh_deformer",
                "has_clothing_simulation",
                "has_section_clothing_data",
                "has_world_position_offset_material",
            )
            gpu_only_active = any(
                outfit_skin.get(key) is True for key in gpu_only_flags
            )
            if any(not isinstance(outfit_skin.get(key), bool)
                   for key in gpu_only_flags):
                raise ValueError(
                    f"capture {filename} omitted GPU-only deformation flags"
                )
            morph_map_entries = int(outfit_skin.get(
                "morph_target_map_entry_count", -1
            ))
            active_morph_count = int(outfit_skin.get(
                "active_morph_target_count", -1
            ))
            nonzero_morph_weights = int(outfit_skin.get(
                "non_zero_morph_target_weight_count", -1
            ))
            nonzero_morph_curves = int(outfit_skin.get(
                "non_zero_morph_curve_count", -1
            ))
            active_external_morphs = int(outfit_skin.get(
                "active_external_morph_target_count", -1
            ))
            clothing_simulations = int(outfit_skin.get(
                "clothing_simulation_count", -1
            ))
            if min(
                morph_map_entries, active_morph_count,
                nonzero_morph_weights, nonzero_morph_curves,
                active_external_morphs, clothing_simulations,
            ) < 0 \
                    or active_morph_count > morph_map_entries \
                    or int(outfit_skin.get(
                        "invalid_active_morph_target_count", -1
                    )) != 0 \
                    or int(outfit_skin.get(
                        "non_finite_morph_weight_count", -1
                    )) != 0 \
                    or int(outfit_skin.get(
                        "non_finite_external_morph_weight_count", -1
                    )) != 0 \
                    or outfit_skin.get("has_active_morph_targets") \
                    is not (active_morph_count > 0
                            or nonzero_morph_weights > 0
                            or nonzero_morph_curves > 0) \
                    or outfit_skin.get("has_active_external_morph_targets") \
                    is not (active_external_morphs > 0) \
                    or outfit_skin.get("has_clothing_simulation") \
                    is not (clothing_simulations > 0):
                raise ValueError(
                    f"capture {filename} GPU deformation counts were inconsistent"
                )
            if active_morph_count > 0 \
                    and not str(outfit_skin.get("active_morph_target_summary", "")):
                raise ValueError(
                    f"capture {filename} omitted active morph target identities"
                )
            mesh_deformer_active = outfit_skin.get("has_mesh_deformer") is True
            if mesh_deformer_active is not bool(
                outfit_skin.get("active_mesh_deformer_path")
            ):
                raise ValueError(
                    f"capture {filename} mesh-deformer identity was inconsistent"
                )
            fixed_complete = outfit_skin.get("fixed_function_path_complete")
            semantic_accepted = outfit_skin.get("semantic_accepted")
            fixed_reason = str(outfit_skin.get(
                "fixed_function_support_reason", ""
            ))
            unsupported_reason_parts = [
                token for key, token in (
                    ("has_active_morph_targets", "ACTIVE_MORPH_TARGETS"),
                    (
                        "has_active_external_morph_targets",
                        "ACTIVE_EXTERNAL_MORPH_TARGETS",
                    ),
                    ("has_mesh_deformer", "MESH_DEFORMER"),
                    ("has_clothing_simulation", "CLOTHING_SIMULATION"),
                    ("has_section_clothing_data", "SECTION_CLOTHING_DATA"),
                    (
                        "has_world_position_offset_material",
                        "WORLD_POSITION_OFFSET_MATERIAL",
                    ),
                )
                if outfit_skin.get(key) is True
            ]
            expected_fixed_reason = (
                "CPU_FIXED_FUNCTION_COMPLETE"
                if not unsupported_reason_parts
                else "CPU_FIXED_FUNCTION_UNSUPPORTED_"
                    + "+".join(unsupported_reason_parts)
            )
            if fixed_complete is not (not gpu_only_active) \
                    or not isinstance(semantic_accepted, bool) \
                    or fixed_reason != expected_fixed_reason \
                    or structural_reason != (
                        "PASS" if fixed_complete
                        else "PASS_DIAGNOSTIC_CPU_FIXED_FUNCTION_BASE_ONLY"
                    ) \
                    or not str(outfit_skin.get("semantic_reason", "")):
                raise ValueError(
                    f"capture {filename} Outfit diagnostic classification was false"
                )
            sections = outfit_skin.get("sections")
            if not isinstance(sections, list) or len(sections) != 2:
                raise ValueError(
                    f"capture {filename} omitted the two Outfit render sections"
                )
            expected_sections = {
                (EXPECTED_SHIRT_MATERIAL_SLOT, EXPECTED_SHIRT_MATERIAL),
                (EXPECTED_SHORT_MATERIAL_SLOT, EXPECTED_SHORT_MATERIAL),
            }
            observed_sections: set[tuple[str, str]] = set()
            section_semantics: list[bool] = []
            section_failure_reasons: list[str] = []
            observed_section_indices: set[int] = set()
            observed_vertex_ranges: list[tuple[int, int]] = []
            section_vertex_total = 0
            section_has_clothing_data = False
            section_has_wpo = False

            def finite_vector3(
                value: object,
                field_name: str,
            ) -> tuple[float, float, float]:
                if not isinstance(value, dict) \
                        or set(value) != {"x", "y", "z"}:
                    raise ValueError(
                        f"capture {filename} {field_name} was not an exact vector"
                    )
                if any(type(value[axis]) not in (int, float)
                       for axis in ("x", "y", "z")):
                    raise ValueError(
                        f"capture {filename} {field_name} components were not numbers"
                    )
                result = tuple(float(value[axis]) for axis in ("x", "y", "z"))
                if not all(math.isfinite(component) for component in result):
                    raise ValueError(
                        f"capture {filename} {field_name} was not finite"
                    )
                return result

            for section in sections:
                if not isinstance(section, dict):
                    raise ValueError(
                        f"capture {filename} had a non-object Outfit section"
                    )
                if set(section) != OUTFIT_SKIN_SECTION_KEYS \
                        or any(type(section[key]) is not str
                               for key in OUTFIT_SKIN_SECTION_STRING_FIELDS) \
                        or any(type(section[key]) is not int
                               for key in OUTFIT_SKIN_SECTION_INTEGER_FIELDS) \
                        or any(
                            type(section[key]) not in (int, float)
                            for key in OUTFIT_SKIN_SECTION_FLOAT_FIELDS
                        ) \
                        or any(type(section[key]) is not bool
                               for key in OUTFIT_SKIN_SECTION_BOOLEAN_FIELDS):
                    raise ValueError(
                        f"capture {filename} Outfit section schema/types drifted"
                    )
                identity = (
                    str(section.get("material_slot", "")),
                    str(section.get("material_path", "")),
                )
                observed_sections.add(identity)
                section_index = int(section.get("section_index", -1))
                observed_section_indices.add(section_index)
                base_vertex = int(section.get("base_vertex_index", -1))
                section_vertex_count = int(section.get("vertex_count", 0))
                section_vertex_total += section_vertex_count
                observed_vertex_ranges.append(
                    (base_vertex, base_vertex + section_vertex_count)
                )
                section_has_clothing_data = section_has_clothing_data or (
                    section.get("has_clothing_data") is True
                )
                if not isinstance(
                    section.get("material_uses_world_position_offset"), bool
                ):
                    raise ValueError(
                        f"capture {filename} Outfit section omitted WPO capability"
                    )
                section_has_wpo = section_has_wpo or (
                    section.get("material_uses_world_position_offset") is True
                )
                if section.get("enabled") is not True \
                        or not isinstance(section.get("has_clothing_data"), bool) \
                        or base_vertex < 0 \
                        or section_vertex_count <= 0 \
                        or int(section.get("triangle_count", 0)) <= 0:
                    raise ValueError(
                        f"capture {filename} Outfit section was not live/complete"
                    )
                section_semantic = section.get("semantic_accepted")
                if not isinstance(section_semantic, bool):
                    raise ValueError(
                        f"capture {filename} Outfit section omitted semantic result"
                    )
                section_semantics.append(section_semantic)
                bounds_minimum = finite_vector3(
                    section.get("bounds_minimum_body_cm"),
                    "Outfit section bounds minimum",
                )
                bounds_maximum = finite_vector3(
                    section.get("bounds_maximum_body_cm"),
                    "Outfit section bounds maximum",
                )
                worst_vertex = finite_vector3(
                    section.get("worst_vertex_body_cm"),
                    "Outfit section worst vertex",
                )
                numeric_fields = (
                    "height_cm", "knee_clearance_cm", "foot_clearance_cm",
                )
                section_reason = section.get("semantic_reason")
                worst_vertex_index = int(section.get("worst_vertex_index", -1))
                if int(section.get("checked_vertex_count", -1)) \
                        != section_vertex_count \
                        or any(not math.isfinite(float(
                            section.get(key, math.nan)
                        )) for key in numeric_fields) \
                        or not base_vertex <= worst_vertex_index \
                        < base_vertex + section_vertex_count \
                        or not section.get("worst_vertex_influences") \
                        or not isinstance(section_reason, str) \
                        or not section_reason:
                    raise ValueError(
                        f"capture {filename} Outfit section skin result was incomplete"
                    )
                height = float(section["height_cm"])
                knee_clearance = float(section["knee_clearance_cm"])
                foot_clearance = float(section["foot_clearance_cm"])
                below_knee = int(section.get("below_knee_vertex_count", -1))
                below_foot = int(section.get("below_foot_vertex_count", -1))
                if any(bounds_minimum[index] > bounds_maximum[index]
                       for index in range(3)) \
                        or any(
                            worst_vertex[index] < bounds_minimum[index] - 1e-4
                            or worst_vertex[index] > bounds_maximum[index] + 1e-4
                            for index in range(3)
                        ) \
                        or not math.isclose(
                            worst_vertex[2], bounds_minimum[2],
                            rel_tol=0.0, abs_tol=1e-4,
                        ) \
                        or not math.isclose(
                            height, bounds_maximum[2] - bounds_minimum[2],
                            rel_tol=0.0, abs_tol=1e-4,
                        ) \
                        or not math.isclose(
                            knee_clearance, bounds_minimum[2] - knee_z,
                            rel_tol=0.0, abs_tol=1e-4,
                        ) \
                        or not math.isclose(
                            foot_clearance, bounds_minimum[2] - foot_z,
                            rel_tol=0.0, abs_tol=1e-4,
                        ) \
                        or not 0 <= below_knee <= section_vertex_count \
                        or not 0 <= below_foot <= section_vertex_count:
                    raise ValueError(
                        f"capture {filename} Outfit section bounds/derived metrics "
                        "were internally inconsistent"
                    )
                if identity[0] == EXPECTED_SHIRT_MATERIAL_SLOT:
                    metric_semantic = (
                        knee_clearance >= 5.0
                        and foot_clearance >= 20.0
                        and height <= 85.0
                        and below_knee == 0
                        and below_foot == 0
                    )
                else:
                    metric_semantic = (
                        knee_clearance >= -5.0
                        and foot_clearance >= 20.0
                        and height <= 60.0
                        and below_knee == 0
                        and below_foot == 0
                    )
                recomputed_semantic = (
                    fixed_complete
                    and canonical_runtime_inputs
                    and metric_semantic
                )
                if section_semantic is not recomputed_semantic:
                    raise ValueError(
                        f"capture {filename} Outfit section semantic predicate "
                        "did not independently recompute"
                    )
                expected_input_reason = (
                    "CANONICAL_RUNTIME_INPUTS"
                    if not expected_noncanonical_tokens
                    else "+".join(expected_noncanonical_tokens)
                )
                expected_section_reason_prefix = (
                    ("PASS" if section_semantic else "REJECT")
                    + f" support={fixed_reason} inputs={expected_input_reason} "
                )
                if not section_reason.startswith(expected_section_reason_prefix):
                    raise ValueError(
                        f"capture {filename} Outfit section reason token drifted"
                    )
                if any(token not in section_reason
                       for token in expected_noncanonical_tokens):
                    raise ValueError(
                        f"capture {filename} Outfit section omitted noncanonical "
                        "runtime-input evidence"
                    )
                if not section_semantic:
                    section_failure_reasons.append(
                        f"section_{section_index}_{identity[0]}: {section_reason}"
                    )
            if observed_sections != expected_sections:
                raise ValueError(
                    f"capture {filename} Outfit section/material identity drifted"
                )
            sorted_vertex_ranges = sorted(observed_vertex_ranges)
            if observed_section_indices != {0, 1} \
                    or len(sorted_vertex_ranges) != 2 \
                    or sorted_vertex_ranges[0][0] != 0 \
                    or sorted_vertex_ranges[0][1] \
                    != sorted_vertex_ranges[1][0] \
                    or sorted_vertex_ranges[1][1] != vertex_count \
                    or section_vertex_total != vertex_count \
                    or outfit_skin.get("has_section_clothing_data") \
                    is not section_has_clothing_data \
                    or outfit_skin.get("has_world_position_offset_material") \
                    is not section_has_wpo:
                raise ValueError(
                    f"capture {filename} Outfit section topology/cloth flags drifted"
                )
            semantic_reason = str(outfit_skin.get("semantic_reason", ""))
            expected_semantic_reason = (
                "PASS"
                if not section_failure_reasons
                else (
                    ("" if fixed_complete else fixed_reason + " | ")
                    + " | ".join(section_failure_reasons)
                )
            )
            if int(outfit_skin.get("checked_vertex_count", -1)) \
                    != vertex_count \
                    or int(outfit_skin.get("checked_influence_count", 0)) <= 0 \
                    or int(outfit_skin.get("body_bone_revision_before", -1)) \
                    != int(outfit_skin.get("body_bone_revision_after", -2)) \
                    or semantic_accepted is not all(section_semantics) \
                    or semantic_reason != expected_semantic_reason \
                    or any(token not in semantic_reason
                           for token in expected_noncanonical_tokens):
                raise ValueError(
                    f"capture {filename} Outfit semantic aggregate was inconsistent"
                )
            if not fixed_complete and semantic_accepted is not False:
                raise ValueError(
                    f"capture {filename} unsupported GPU path falsely passed CPU semantics"
                )
            if row.get("metahuman_hair_render_evidence_collected") is not True:
                raise ValueError(
                    f"capture {filename} omitted live MetaHuman hair evidence"
                )
            hair = row.get("metahuman_hair_render")
            if not isinstance(hair, dict):
                raise ValueError(
                    f"capture {filename} omitted metahuman_hair_render evidence"
                )
            hair_required_true = (
                "registered",
                "visible",
                "should_render",
                "render_state_created",
                "recently_rendered",
                "render_in_main_pass",
                "bounds_finite",
                "projected_viewport_overlap",
                "asset_valid",
                "asset_groups_valid",
                "lod_sync_registered",
                "lod_sync_tick_enabled",
            )
            hair_required_false = (
                "hidden_in_game",
                "owner_no_see",
                "only_owner_see",
                "visible_in_scene_capture_only",
                "compiling",
            )
            hair_radius = float(hair.get("bounds_radius_cm", -1.0))
            hair_asset_groups = int(hair.get("asset_group_count", 0))
            hair_component_groups = int(hair.get("component_group_count", 0))
            hair_lod_count = int(hair.get("lod_count", 0))
            hair_best_lod = int(hair.get("best_available_lod", -1))
            hair_forced_lod = int(hair.get("forced_lod", -1))
            if any(hair.get(key) is not True for key in hair_required_true) \
                    or any(hair.get(key) is not False for key in hair_required_false) \
                    or hair.get("tick_even_when_paused") \
                    is not (expected_policy == "CharacterCreator") \
                    or hair.get("lod_sync_tick_even_when_paused") \
                    is not (expected_policy == "CharacterCreator") \
                    or hair.get("resource_path") != EXPECTED_HAIR_GROOM \
                    or not hair.get("component_path") \
                    or int(hair.get("material_count", 0)) <= 0 \
                    or hair_asset_groups <= 0 \
                    or hair_component_groups != hair_asset_groups \
                    or hair_lod_count <= 0 \
                    or hair_best_lod < 0 \
                    or not 0 <= hair_forced_lod < hair_lod_count \
                    or hair_forced_lod < hair_best_lod \
                    or not math.isfinite(hair_radius) \
                    or hair_radius <= 0.0:
                raise ValueError(
                    f"capture {filename} did not prove the live Hair_S_Clean groom"
                )
            expects_creator_framing = filename.startswith(("02_", "03_"))
            if row.get("creator_framing_evidence_collected") \
                    is not expects_creator_framing \
                    or row.get("creator_framing_accepted") \
                    is not expects_creator_framing:
                raise ValueError(
                    f"capture {filename} creator framing evidence was not exact"
                )
            if expects_creator_framing:
                def finite_point(key: str) -> tuple[float, float]:
                    value = row.get(key)
                    if not isinstance(value, dict):
                        raise ValueError(
                            f"capture {filename} omitted {key}"
                        )
                    point = (float(value.get("x", math.nan)),
                             float(value.get("y", math.nan)))
                    if not all(math.isfinite(component) for component in point):
                        raise ValueError(
                            f"capture {filename} {key} was not finite"
                        )
                    return point

                semantic_count = int(row.get("creator_semantic_point_count", 0))
                semantic_min = finite_point("creator_semantic_min_px")
                semantic_max = finite_point("creator_semantic_max_px")
                body = row["metahuman_body_render"]
                head = row["metahuman_head_render"]
                outfit = row["metahuman_outfit_render"]
                preview_min_x = 1920.0 * 0.5 + 20.0
                max_x = 1920.0 - 32.0
                max_y = 1080.0 - 32.0
                if filename.startswith("02_"):
                    framing_exact = (
                        semantic_count == 5
                        and semantic_min[0] >= preview_min_x
                        and semantic_min[1] >= 32.0
                        and semantic_max[0] <= max_x
                        and semantic_max[1] <= max_y
                        and semantic_max[1] - semantic_min[1] >= 300.0
                        and all(
                            evidence.get("projected_viewport_contained") is True
                            for evidence in (body, head, outfit, hair)
                        )
                    )
                else:
                    hair_min = hair.get("projected_min_px")
                    hair_max = hair.get("projected_max_px")
                    if not isinstance(hair_min, dict) \
                            or not isinstance(hair_max, dict):
                        raise ValueError(
                            f"capture {filename} omitted projected hair bounds"
                        )
                    hair_min_xy = (
                        float(hair_min.get("x", math.nan)),
                        float(hair_min.get("y", math.nan)),
                    )
                    hair_max_xy = (
                        float(hair_max.get("x", math.nan)),
                        float(hair_max.get("y", math.nan)),
                    )
                    hair_height = hair_max_xy[1] - hair_min_xy[1]
                    framing_exact = (
                        semantic_count == 1
                        and all(math.isfinite(value) for value in (
                            *hair_min_xy, *hair_max_xy, hair_height,
                        ))
                        and semantic_min == hair_min_xy
                        and semantic_max == hair_max_xy
                        and hair_min_xy[0] >= preview_min_x
                        and hair_min_xy[1] >= 32.0
                        and hair_max_xy[0] <= max_x
                        and hair_max_xy[1] <= max_y
                        and 180.0 <= hair_height <= 500.0
                        and head.get("projected_viewport_contained") is True
                        and hair.get("projected_viewport_contained") is True
                    )
                if not framing_exact:
                    raise ValueError(
                        f"capture {filename} did not prove contained creator framing"
                    )
            expects_hand_alignment = filename.startswith(("07_", "08_"))
            hand_distance = float(row.get(
                "visible_disc_to_metahuman_hand_distance_cm", math.nan
            ))
            if row.get("visible_hand_alignment_evidence_collected") \
                    is not expects_hand_alignment:
                raise ValueError(
                    f"capture {filename} visible hand/disc evidence partition failed"
                )
            if expects_hand_alignment:
                def vector_distance(
                    left: tuple[float, float, float],
                    right: tuple[float, float, float],
                ) -> float:
                    return math.sqrt(sum(
                        (left[index] - right[index]) ** 2
                        for index in range(3)
                    ))

                source_hand = capture_vectors3["source_hand_r_world_cm"]
                source_grip = capture_vectors3[
                    "source_disc_grip_r_world_cm"
                ]
                pre_hand = capture_vectors3[
                    "metahuman_hand_r_pre_correction_world_cm"
                ]
                post_hand = capture_vectors3[
                    "metahuman_hand_r_post_correction_world_cm"
                ]
                source_grip_distance = float(
                    row["source_hand_r_to_disc_grip_r_cm"]
                )
                pre_source_distance = float(
                    row["metahuman_hand_r_pre_to_source_hand_r_cm"]
                )
                post_source_distance = float(
                    row["metahuman_hand_r_post_to_source_hand_r_cm"]
                )
                derived_distances_exact = (
                    math.isclose(
                        source_grip_distance,
                        vector_distance(source_hand, source_grip),
                        rel_tol=0.0,
                        abs_tol=0.002,
                    )
                    and math.isclose(
                        pre_source_distance,
                        vector_distance(pre_hand, source_hand),
                        rel_tol=0.0,
                        abs_tol=0.002,
                    )
                    and math.isclose(
                        post_source_distance,
                        vector_distance(post_hand, source_hand),
                        rel_tol=0.0,
                        abs_tol=0.002,
                    )
                )
                source_frame = row["source_sample_frame_counter"]
                target_frame = row["target_correction_frame_counter"]
                release_frame = row["release_callback_frame_counter"]
                freeze_frame = row["capture_freeze_frame_counter"]
                release_order = row["release_callback_event_order"]
                freeze_order = row["capture_freeze_event_order"]
                exact_release_order = (
                    release_frame == 0
                    and release_order == 0
                    and freeze_order == 1
                ) if filename.startswith("07_") else (
                    release_frame == freeze_frame
                    and release_order == 2
                    and freeze_order == 3
                )
                correction_exact = (
                    row["presentation_hand_correction_mode"]
                    == HAND_CORRECTION_MODE
                    and row["presentation_hand_correction_snapshot_valid"]
                    is True
                    and row["presentation_hand_correction_reachable"] is True
                    and row["presentation_hand_correction_applied"] is True
                    and derived_distances_exact
                    and 0.0 <= source_grip_distance <= 25.0
                    and 0.0 <= post_source_distance <= 0.05
                    and source_frame > 0
                    and source_frame == target_frame == freeze_frame
                    and row["source_bone_revision_at_pre_update"] > 0
                    and row["target_bone_revision_at_capture"]
                    != row["target_bone_revision_before_evaluate"]
                    and exact_release_order
                )
                if not correction_exact:
                    raise ValueError(
                        f"capture {filename} did not prove the exact same-frame "
                        "no-stretch target-arm correction and release ordering"
                    )
            if expects_hand_alignment and (
                row.get("visible_hand_location_finite") is not True
                or row.get("visible_disc_location_finite") is not True
                or row.get("visible_hand_distance_finite") is not True
                or not math.isfinite(hand_distance)
                or not 0.0 <= hand_distance <= 25.0
            ):
                raise ValueError(
                    f"capture {filename} visible MetaHuman hand did not converge "
                    "with the authoritative disc"
                )
        elif row.get("metahuman_render_evidence_collected") is not False \
                or row.get("metahuman_hair_render_evidence_collected") is not False \
                or row.get("metahuman_outfit_render_evidence_collected") is not False \
                or row.get("metahuman_outfit_live_skin_evidence_collected") is not False \
                or row.get("visible_hand_alignment_evidence_collected") is not False \
                or row.get("creator_framing_evidence_collected") is not False \
                or "metahuman_body_render" in row \
                or "metahuman_head_render" in row \
                or "metahuman_outfit_render" in row \
                or "metahuman_outfit_live_skin" in row \
                or "metahuman_hair_render" in row:
            raise ValueError(
                f"DG capture {filename} unexpectedly claimed MetaHuman render evidence"
            )
        expects_creator = filename.startswith(("01_", "02_", "03_", "04_"))
        if row.get("creator_open") is not expects_creator:
            raise ValueError(f"capture {filename} creator state metadata failed")
        if filename.startswith("07_") and (
            row.get("throw_phase") != "ReachBack"
            or row.get("held_disc_visible") is not True
            or row.get("gameplay_disc_active") is not False
        ):
            raise ValueError("grip capture scene metadata failed")
        if filename.startswith("08_") and (
            row.get("held_disc_visible") is not False
            or row.get("gameplay_disc_active") is not True
            or row.get("release_evidence_collected") is not True
            or row.get("immutable_release_grip_valid") is not True
            or row.get("gameplay_disc_location_finite") is not True
            or row.get("disc_to_grip_distance_finite") is not True
            or row.get("released_disc_identity_verified") is not True
            or row.get("flight_component_identity_verified") is not True
            or row.get("flight_is_flying") is not True
            or row.get("flight_tick_snapshot_present") is not True
            or row.get("flight_tick_was_enabled_before_pause") is not True
            or row.get("flight_tick_suspended") is not True
            or row.get("world_paused_for_capture") is not True
            or int(row.get("strokes", -1))
                != int(row.get("expected_strokes", -2))
            or int(row.get("world_disc_count", -1))
                != int(row.get("expected_world_disc_count", -2))
            or int(row.get("release_commit_count", -1))
                != int(row.get("expected_release_commit_count", -2))
            or not math.isfinite(
                float(row.get("disc_to_grip_distance_cm", math.nan)))
            or not 0.0 <= float(
                row.get("disc_to_grip_distance_cm", -1.0)) <= 25.0
        ):
            raise ValueError("release capture scene metadata failed")
        if filename.startswith("09_") and row.get("throw_phase") != "FollowThrough":
            raise ValueError("follow-through capture scene metadata failed")
    candidate_isolation_probes = report.get(
        "presentation_policy_candidate_isolation_probes"
    )
    transition_probes = report.get("presentation_policy_transition_probes")
    if not isinstance(candidate_isolation_probes, list) \
            or not isinstance(transition_probes, list) \
            or type(report.get("gameplay_candidate_isolation_probe_passed")) \
            is not bool \
            or type(report.get("presentation_policy_transition_probes_passed")) \
            is not bool:
        raise ValueError(f"phase {phase} policy-probe schema was absent")
    zero_counter_delta = {
        "transition_success_count": 0,
        "transition_failure_count": 0,
        "rollback_success_count": 0,
        "rollback_failure_count": 0,
    }
    if phase == PHASES[1][0]:
        if report["gameplay_candidate_isolation_probe_passed"] is not True \
                or len(candidate_isolation_probes) != 1:
            raise ValueError(
                "phase 2 did not report its sole gameplay candidate-isolation probe"
            )
        validate_presentation_policy_probe(
            candidate_isolation_probes[0],
            "phase2_gameplay_invalid_candidate",
            "PREVALIDATION_CANDIDATE_REJECTION",
            "BuildVisualBackend_duplicate_body_head_tag",
            "GameplayPerformance",
            "GameplayPerformance",
            False,
            zero_counter_delta,
            True,
            "phase 2 candidate isolation",
        )
    elif report["gameplay_candidate_isolation_probe_passed"] is not False \
            or candidate_isolation_probes:
        raise ValueError(
            f"non-phase-2 report {phase} claimed candidate-isolation evidence"
        )

    if phase == PHASES[3][0]:
        if report["presentation_policy_transition_probes_passed"] is not True \
                or len(transition_probes) != 4:
            raise ValueError("phase 4 did not report exactly four policy probes")
        transition_success_delta = dict(zero_counter_delta)
        transition_success_delta["transition_success_count"] = 1
        expected_probes = (
            (
                "phase4_creator_entry", "COMMITTED_ACTIVE_TRANSITION",
                "OpenCharacterCreator_BeginCharacterCreatorPreview",
                "GameplayPerformance", "CharacterCreator", True,
                transition_success_delta, False,
            ),
            (
                "phase4_creator_redundant_no_op", "VERIFIED_SAME_MODE_NO_OP",
                "SetPresentationPolicy",
                "CharacterCreator", "CharacterCreator", True,
                zero_counter_delta, True,
            ),
            (
                "phase4_gameplay_exit", "COMMITTED_ACTIVE_TRANSITION",
                "CancelCharacterCreator_EndCharacterCreatorPreview",
                "CharacterCreator", "GameplayPerformance", True,
                transition_success_delta, False,
            ),
            (
                "phase4_gameplay_redundant_no_op", "VERIFIED_SAME_MODE_NO_OP",
                "SetPresentationPolicy",
                "GameplayPerformance", "GameplayPerformance", True,
                zero_counter_delta, True,
            ),
        )
        for index, expected in enumerate(expected_probes):
            validate_presentation_policy_probe(
                transition_probes[index], *expected,
                context=f"phase 4 transition probe {index}",
            )
    elif report["presentation_policy_transition_probes_passed"] is not False \
            or transition_probes:
        raise ValueError(f"non-phase-4 report {phase} claimed transition probes")

    if phase == PHASES[0][0] \
            and report.get("proxy_values_preserved") is not True:
        raise ValueError("phase 1 did not prove preserved proxy-only values")
    if phase == PHASES[0][0]:
        creator_policies = [
            row["metahuman_presentation_policy"]
            for row in captures
            if str(row.get("filename", "")).startswith(("02_", "03_"))
        ]
        if len(creator_policies) != 2 \
                or presentation_policy_stable_projection(creator_policies[0]) \
                != presentation_policy_stable_projection(creator_policies[1]):
            raise ValueError(
                "phase 1 creator captures changed policy state or counters"
            )
    if phase == PHASES[1][0] and (
        report.get("cosmetic_failure_probe_passed") is not True
        or report.get("backend_failure_probe_passed") is not True
        or report.get("backend_failure_adapter_status")
        != "Distinct visual body/head component tags are required."
    ):
        raise ValueError("phase 2 did not prove both no-write failure probes")
    if phase == PHASES[2][0]:
        if report.get("duplicate_release_no_op_passed") is not True \
                or report.get("stable_throw_presentation_verified") is not True \
                or report.get("stable_throw_retarget_verified") is not True \
                or report.get("release_callback_count") != 1 \
                or report.get("recovery_callback_count") != 1:
            raise ValueError(
                "phase 3 did not prove duplicate release no-op and a stable "
                "MetaHuman actor/body/head tuple"
            )
        throw_actor_paths = [
            row.get("active_visual_actor") for row in captures
            if isinstance(row, dict)
            and str(row.get("filename", "")).startswith(("07_", "08_", "09_"))
        ]
        if len(throw_actor_paths) != 3 \
                or not throw_actor_paths[0] \
                or len(set(throw_actor_paths)) != 1 \
                or report.get("stable_throw_visual_actor") != throw_actor_paths[0] \
                or not report.get("stable_throw_visual_body") \
                or not report.get("stable_throw_visual_head"):
            raise ValueError("phase 3 throw captures changed the visual actor tuple")
        throw_policies = [
            row["metahuman_presentation_policy"]
            for row in captures
            if str(row.get("filename", "")).startswith(("07_", "08_", "09_"))
        ]
        if len(throw_policies) != 3 \
                or any(
                    presentation_policy_stable_projection(policy)
                    != presentation_policy_stable_projection(throw_policies[0])
                    for policy in throw_policies[1:]
                ):
            raise ValueError(
                "phase 3 throw captures changed gameplay policy state or counters"
            )
        capture_by_name = {
            str(row.get("filename")): row for row in captures
            if isinstance(row, dict)
        }
        grip = capture_by_name["07_MH_RHBH_Grip.png"]
        release = capture_by_name["08_MH_RHBH_Release.png"]
        if release.get("release_commit_count") \
                != int(grip.get("release_commit_count", -1)) + 1 \
                or release.get("strokes") \
                != int(grip.get("strokes", -1)) + 1 \
                or release.get("world_disc_count") \
                != int(grip.get("world_disc_count", -1)) + 1:
            raise ValueError(
                "phase 3 release frame did not add exactly one commit/stroke/disc"
            )
    segments = report.get("performance_segments")
    if phase == PHASES[3][0]:
        if not isinstance(segments, list) or len(segments) != 2:
            raise ValueError("phase 4 did not report exactly two performance segments")
        if report.get("performance_saw_authoritative_flight") is not True:
            raise ValueError("phase 4 did not observe the authoritative flight component")
        trajectory_fields = (
            "trajectory_export_deferred_at_settlement",
            "trajectory_summary_ready_before_flush",
            "trajectory_export_flushed_after_segment",
            "trajectory_export_flush_passed",
        )
        if any(report.get(key) is not True for key in trajectory_fields) \
                or not report.get("deferred_trajectory_capture_id") \
                or report.get("flushed_trajectory_capture_id") \
                != report.get("deferred_trajectory_capture_id"):
            raise ValueError(
                "phase 4 did not defer trajectory file export until after sampling"
            )
        if report.get("stable_throw_presentation_verified") is not True \
                or report.get("stable_throw_retarget_verified") is not True \
                or report.get("release_callback_count") != 1 \
                or report.get("recovery_callback_count") != 1 \
                or not report.get("stable_throw_visual_actor") \
                or not report.get("stable_throw_visual_body") \
                or not report.get("stable_throw_visual_head"):
            raise ValueError("phase 4 did not retain a stable MetaHuman actor tuple")
        validate_performance_contract(
            report.get("performance_contract"),
            "phase 4 performance_contract",
        )
        performance_policies: list[
            tuple[dict[str, object], dict[str, object]]
        ] = []
        expected_segments = (
            ("creator_closeup", "CharacterCreator"),
            ("rhbh_gameplay", "GameplayPerformance"),
        )
        for index, (expected_segment, expected_policy) in enumerate(
            expected_segments
        ):
            row = validate_performance_segment_shape(
                segments[index],
                expected_segment,
                f"phase 4 performance_segments[{index}]",
            )
            begin_policy = validate_presentation_policy_snapshot(
                row["metahuman_presentation_policy_begin"],
                expected_policy,
                f"phase 4 {row['segment']} policy begin",
                require_live_render_lods=True,
            )
            end_policy = validate_presentation_policy_snapshot(
                row["metahuman_presentation_policy_end"],
                expected_policy,
                f"phase 4 {row['segment']} policy end",
                require_live_render_lods=True,
            )
            if presentation_policy_stable_projection(begin_policy) \
                    != presentation_policy_stable_projection(end_policy):
                raise ValueError(
                    f"phase 4 {row['segment']} changed policy state/counters "
                    "inside the timed window"
                )
            performance_policies.append((begin_policy, end_policy))

        validate_performance_policy_chain(
            transition_probes,
            performance_policies,
            transition_success_delta,
        )
    elif segments not in ([], None):
        raise ValueError(f"non-performance phase {phase} reported performance samples")


def run_scale_evidence_self_test() -> int:
    def fixture() -> dict[str, object]:
        unit = {"x": 1.0, "y": 1.0, "z": 1.0}
        return {
            "metahuman_visual_root_uses_absolute_scale": True,
            **{
                key: copy.deepcopy(unit)
                for key in CAPTURE_METAHUMAN_SCALE_VECTOR3_FIELDS
            },
        }

    baseline = fixture()
    cases: list[tuple[str, dict[str, object], bool, bool]] = [
        ("valid_metahuman", copy.deepcopy(baseline), True, True),
        ("valid_dg_absence", {}, False, True),
    ]

    missing_flag = copy.deepcopy(baseline)
    del missing_flag["metahuman_visual_root_uses_absolute_scale"]
    cases.append(("missing_absolute_flag", missing_flag, True, False))

    relative_root = copy.deepcopy(baseline)
    relative_root["metahuman_visual_root_uses_absolute_scale"] = False
    cases.append(("relative_root", relative_root, True, False))

    numeric_absolute_flag = copy.deepcopy(baseline)
    numeric_absolute_flag["metahuman_visual_root_uses_absolute_scale"] = 1
    cases.append(("numeric_absolute_flag", numeric_absolute_flag, True, False))

    for field, bad_value in (
        ("metahuman_visual_root_world_scale", 1.01),
        ("metahuman_body_world_scale", 0.82),
        ("metahuman_head_world_scale", 1.15),
        ("metahuman_outfit_world_scale", 0.0),
    ):
        non_unit = copy.deepcopy(baseline)
        assert isinstance(non_unit[field], dict)
        non_unit[field]["x"] = bad_value
        cases.append((f"non_unit_{field}", non_unit, True, False))

    nonfinite = copy.deepcopy(baseline)
    assert isinstance(nonfinite["metahuman_head_world_scale"], dict)
    nonfinite["metahuman_head_world_scale"]["y"] = float("nan")
    cases.append(("nonfinite_head_scale", nonfinite, True, False))

    missing_axis = copy.deepcopy(baseline)
    assert isinstance(missing_axis["metahuman_body_world_scale"], dict)
    del missing_axis["metahuman_body_world_scale"]["z"]
    cases.append(("missing_vector_axis", missing_axis, True, False))

    extra_axis = copy.deepcopy(baseline)
    assert isinstance(extra_axis["metahuman_outfit_world_scale"], dict)
    extra_axis["metahuman_outfit_world_scale"]["w"] = 1.0
    cases.append(("extra_vector_axis", extra_axis, True, False))

    boolean_axis = copy.deepcopy(baseline)
    assert isinstance(boolean_axis["metahuman_visual_root_world_scale"], dict)
    boolean_axis["metahuman_visual_root_world_scale"]["z"] = True
    cases.append(("boolean_vector_axis", boolean_axis, True, False))

    cases.append(("dg_scale_leak", copy.deepcopy(baseline), False, False))

    failures: list[str] = []
    for name, capture, expects_metahuman, should_pass in cases:
        accepted = True
        try:
            validate_metahuman_unit_world_scale_evidence(
                capture,
                f"{name}.png",
                expects_metahuman=expects_metahuman,
            )
        except ValueError:
            accepted = False
        if accepted != should_pass:
            failures.append(
                f"{name}: accepted={accepted} expected={should_pass}"
            )

    if failures:
        for failure in failures:
            print(f"SELF_TEST_ERROR: {failure}", file=sys.stderr)
        print(
            f"FAIL_SELF_TEST: {len(cases) - len(failures)}/{len(cases)}",
            file=sys.stderr,
        )
        return 1
    print(
        f"PASS_SELF_TEST: {len(cases)}/{len(cases)} packaged v3.4 "
        "MetaHuman unit-world-scale evidence cases",
        flush=True,
    )
    return 0


def main() -> int:
    options = parse_args()
    if options.self_test:
        return run_scale_evidence_self_test()
    (
        executable,
        user_dir,
        accepted_backup,
        report_path,
        evidence_root,
        log_root,
    ) = validate_paths(options)
    packaged_game_root = executable.parents[2]
    run_id = user_dir.name
    owned_marker = user_dir / ".dg_session8b_packaged_acceptance_owned"
    stable_run_dir = evidence_root / run_id
    if stable_run_dir.exists():
        raise FileExistsError(f"evidence run directory already exists: {stable_run_dir}")

    production_before = file_record(PRODUCTION_SAVE)
    backup_before = file_record(accepted_backup)
    project_saves_before = directory_snapshot(PROJECT_SAVE_DIR)
    project_save_topology_before = directory_topology(PROJECT_SAVE_DIR)
    if production_before.get("bytes") != EXPECTED_ACCEPTED_SAVE_BYTES \
            or production_before.get("sha256") != EXPECTED_ACCEPTED_SAVE_SHA256:
        raise RuntimeError("production save is not the accepted A999 baseline")
    if backup_before.get("bytes") != EXPECTED_ACCEPTED_SAVE_BYTES \
            or backup_before.get("sha256") != EXPECTED_ACCEPTED_SAVE_SHA256:
        raise RuntimeError("external backup is not the accepted A999 baseline")

    print("Hashing Content before Session 8B packaged acceptance...", flush=True)
    content_before = directory_snapshot(CONTENT)
    content_topology_before = directory_topology(CONTENT)
    print("Hashing packaged game before Session 8B acceptance...", flush=True)
    package_before = directory_snapshot(packaged_game_root)
    package_topology_before = directory_topology(packaged_game_root)
    package_protected_before = {
        path: record for path, record in package_before.items()
        if Path(path).suffix.casefold() in {".exe", ".utoc", ".ucas", ".pak"}
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    stable_run_dir.mkdir(parents=True)
    log_root.mkdir(parents=True, exist_ok=True)

    errors: list[str] = []
    phase_records: list[dict[str, object]] = []
    capture_records: list[dict[str, object]] = []
    isolated_save_records: list[dict[str, object]] = []
    external_inventory: dict[str, dict[str, object]] = {}
    cleanup_error = ""
    started_utc = datetime.now(timezone.utc).isoformat()
    started_monotonic = time.monotonic()

    try:
        user_dir.mkdir()
        owned_marker.write_text(run_id + "\n", encoding="ascii")
        for phase, expected_status, expected_captures in PHASES:
            log_path = log_root / f"Session8B_MetaHumanPackaged_{phase}_{run_id}.log"
            log_path.unlink(missing_ok=True)
            command = [
                str(executable),
                "-Session8BMetaHumanPackagedAcceptance",
                f"-Session8BMetaHumanPhase={phase}",
                "-Session8BPracticeSnapshotSaveSuppressed",
                f"-Session8BRunId={run_id}",
                f"-UserDir={user_dir}",
                "-Course=PineRidge",
                "-Hole=1",
                "-SkipHoleIntro",
                "-ResX=1920",
                "-ResY=1080",
                "-ForceRes",
                "-RenderOffscreen",
                "-d3d12",
                "-unattended",
                "-nop4",
                "-nosplash",
                "-UTF8Output",
                "-stdout",
                "-FullStdOutLogOutput",
                f"-abslog={log_path}",
            ]
            print(f"Running Session 8B phase: {phase}", flush=True)
            process: subprocess.Popen[str] | None = None
            console = ""
            timed_out = False
            phase_started = time.monotonic()
            try:
                process = subprocess.Popen(
                    command,
                    cwd=executable.parent,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    creationflags=(
                        subprocess.CREATE_NEW_PROCESS_GROUP
                        if sys.platform == "win32" else 0
                    ),
                )
                try:
                    console, _ = process.communicate(
                        timeout=options.timeout_seconds
                    )
                except subprocess.TimeoutExpired as exc:
                    timed_out = True
                    partial = exc.output or ""
                    if isinstance(partial, bytes):
                        partial = partial.decode("utf-8", errors="replace")
                    terminate(process)
                    tail, _ = process.communicate()
                    console = partial + (tail or "")
            finally:
                if process is not None and process.poll() is None:
                    terminate(process)
            return_code = process.returncode if process is not None else None
            log_text = log_path.read_text(
                encoding="utf-8", errors="replace"
            ) if log_path.is_file() else ""
            evidence_text = log_text if log_text else console
            external_phase_dir = (
                user_dir / "Saved/CharacterFramework/"
                "Session8BMetaHumanPackagedAcceptance" / phase
            )
            stable_phase_dir = stable_run_dir / phase
            stable_phase_dir.mkdir(exist_ok=True)
            (stable_phase_dir / "LauncherConsole.log").write_text(
                console, encoding="utf-8"
            )
            if external_phase_dir.is_dir():
                for source in external_phase_dir.iterdir():
                    if source.is_file() and source.suffix.casefold() in {
                        ".json", ".png",
                    }:
                        shutil.copy2(source, stable_phase_dir / source.name)
            warning_lines = sorted({
                line.strip() for line in evidence_text.splitlines()
                if ": warning:" in line.casefold()
            })
            external_report = external_phase_dir / "PhaseReport.json"
            stable_report = stable_phase_dir / "PhaseReport.json"
            isolated_save = user_dir / ISOLATED_SAVE_RELATIVE
            external_saves = sorted(user_dir.rglob("*.sav"))
            isolated_record = {
                "phase": phase,
                **file_record(isolated_save),
            }
            if external_saves == [isolated_save] and isolated_save.is_file():
                isolated_save_records.append(isolated_record)
            phase_record: dict[str, object] = {
                "phase": phase,
                "expected_status": expected_status,
                "status": "UNPARSED" if stable_report.is_file() else "MISSING",
                "return_code": return_code,
                "duration_seconds": time.monotonic() - phase_started,
                "command": command,
                "log": str(log_path),
                "log_record": file_record(log_path),
                "phase_report": str(stable_report),
                "phase_report_record": file_record(stable_report),
                "isolated_save": isolated_record,
                "warning_count": len(warning_lines),
                "warning_lines": warning_lines,
            }
            phase_records.append(phase_record)
            phase_report: dict[str, object] = {}
            if external_report.is_file():
                phase_report = strict_json_loads(
                    stable_report.read_text(encoding="utf-8")
                )
                phase_record["status"] = phase_report.get("status", "MISSING")
                phase_record["phase_report_record"] = file_record(stable_report)
            if timed_out:
                raise RuntimeError(f"phase {phase} timed out")
            if return_code != 0:
                raise RuntimeError(f"phase {phase} returned {return_code}")
            pass_lines = [
                line for line in evidence_text.splitlines()
                if PASS_PREFIX in line
            ]
            fail_lines = [
                line for line in evidence_text.splitlines()
                if FAIL_PREFIX in line
            ]
            if len(pass_lines) != 1 or fail_lines \
                    or f"status={expected_status}" not in pass_lines[0]:
                raise RuntimeError(f"phase {phase} log marker contract failed")
            forbidden_log_tokens = (
                ": Error:", "Fatal error:",
                "EXCEPTION_ACCESS_VIOLATION", "Assertion failed:",
                "ensure condition failed", "LogRHI: Error:",
            )
            if any(token.casefold() in evidence_text.casefold()
                   for token in forbidden_log_tokens):
                raise RuntimeError(f"phase {phase} emitted a fatal/error diagnostic")
            if not external_report.is_file():
                raise FileNotFoundError(f"phase report missing: {external_report}")
            validate_phase_report(
                phase_report, phase, expected_status, run_id, expected_captures
            )
            runtime_capture_rows = {
                row["filename"]: row
                for row in phase_report.get("captures", [])
                if isinstance(row, dict) and isinstance(row.get("filename"), str)
            }
            for filename in expected_captures:
                source = external_phase_dir / filename
                if not source.is_file() or source.stat().st_size < 32 * 1024:
                    raise ValueError(f"capture missing or undersized: {source}")
                pixel_evidence = decode_png_rgb_stats(source)
                width = int(pixel_evidence["width"])
                height = int(pixel_evidence["height"])
                if (width, height) != (1920, 1080):
                    raise ValueError(
                        f"capture {filename} is {width}x{height}, not 1920x1080"
                    )
                runtime_scene = runtime_capture_rows.get(filename)
                if not isinstance(runtime_scene, dict) \
                        or runtime_scene.get("width") != width \
                        or runtime_scene.get("height") != height \
                        or runtime_scene.get("bytes") != source.stat().st_size \
                        or not runtime_scene.get("scene"):
                    raise ValueError(
                        f"runtime scene metadata did not match capture {filename}"
                    )
                target = stable_phase_dir / filename
                shutil.copy2(source, target)
                capture_records.append({
                    "phase": phase,
                    "filename": filename,
                    "bytes": target.stat().st_size,
                    "width": width,
                    "height": height,
                    "sha256": sha256(target),
                    "decoded_pixel_evidence": pixel_evidence,
                    "stable_path": str(target),
                    "runtime_scene_metadata": runtime_scene,
                })

            if external_saves != [isolated_save] or not isolated_save.is_file():
                raise ValueError(
                    f"phase {phase} external save inventory was not exactly "
                    f"{ISOLATED_SAVE_RELATIVE}: {external_saves!r}"
                )

        if len(capture_records) != 9:
            raise ValueError("the four phases did not produce exactly nine captures")
        capture_names = tuple(row["filename"] for row in capture_records)
        expected_names = tuple(
            name for _, _, names in PHASES for name in names
        )
        if capture_names != expected_names:
            raise ValueError("nine-capture filename/order contract failed")
        rgb_hash_by_name = {
            str(row["filename"]):
                str(row["decoded_pixel_evidence"]["rgb_sha256"])
            for row in capture_records
        }
        allowed_redundant_pair = frozenset({
            "01_DG_Creator_Sentinel_Baseline.png",
            "04_DG_Creator_Preview_From_MH.png",
        })
        for left_index, left_name in enumerate(expected_names):
            for right_name in expected_names[left_index + 1:]:
                if rgb_hash_by_name[left_name] == rgb_hash_by_name[right_name] \
                        and frozenset({left_name, right_name}) \
                        != allowed_redundant_pair:
                    raise ValueError(
                        "unexpected pixel-identical capture pair: "
                        f"{left_name}, {right_name}"
                    )
        if len(set(rgb_hash_by_name.values())) < 8:
            raise ValueError(
                "nine captures did not contain at least eight distinct RGB states"
            )
        save_hashes = [row.get("sha256") for row in isolated_save_records]
        if save_hashes[0] == save_hashes[1] \
                or save_hashes[0] != save_hashes[2] \
                or save_hashes[0] != save_hashes[3]:
            raise ValueError(
                "isolated save transitions were not exact MH1 -> distinct DG2 "
                "-> byte-identical MH3 -> unchanged MH4"
            )
        rejected_outfit_frames = [
            str(row["filename"])
            for row in capture_records
            if isinstance(row.get("runtime_scene_metadata"), dict)
            and str(row.get("filename", "")).startswith(
                ("02_", "03_", "05_", "07_", "08_", "09_")
            )
            and row["runtime_scene_metadata"]
                ["metahuman_outfit_live_skin"].get("semantic_accepted") is not True
        ]
        if rejected_outfit_frames:
            raise ValueError(
                "live Outfit skin diagnostic rejected frames after complete "
                f"four-phase capture: {rejected_outfit_frames!r}"
            )
    except Exception as exc:  # aggregate the truthful failure before cleanup
        errors.append(f"{type(exc).__name__}: {exc}")
    finally:
        if user_dir.is_dir():
            external_inventory = directory_snapshot(user_dir)
            marker_ok = owned_marker.is_file() \
                and owned_marker.read_text(encoding="ascii").strip() == run_id \
                and user_dir.parent == EXPECTED_USER_DIR_ROOT \
                and user_dir.name == run_id
            if not marker_ok:
                cleanup_error = (
                    "refused cleanup because the exact owned marker/path "
                    "contract was not intact"
                )
            else:
                try:
                    shutil.rmtree(user_dir)
                except OSError as exc:
                    cleanup_error = f"{type(exc).__name__}: {exc}"

    print("Hashing Content after Session 8B packaged acceptance...", flush=True)
    content_after = directory_snapshot(CONTENT)
    content_topology_after = directory_topology(CONTENT)
    print("Hashing packaged game after Session 8B acceptance...", flush=True)
    package_after = directory_snapshot(packaged_game_root)
    package_topology_after = directory_topology(packaged_game_root)
    package_protected_after = {
        path: record for path, record in package_after.items()
        if Path(path).suffix.casefold() in {".exe", ".utoc", ".ucas", ".pak"}
    }
    project_saves_after = directory_snapshot(PROJECT_SAVE_DIR)
    project_save_topology_after = directory_topology(PROJECT_SAVE_DIR)
    production_after = file_record(PRODUCTION_SAVE)
    backup_after = file_record(accepted_backup)
    content_changes = changed_paths(content_before, content_after)
    package_changes = changed_paths(package_before, package_after)
    project_save_changes = changed_paths(
        project_saves_before, project_saves_after
    )
    external_residue = user_dir.exists()
    if content_changes:
        errors.append(f"Content changed: {content_changes!r}")
    if package_changes:
        errors.append(f"packaged game changed: {package_changes!r}")
    if project_save_changes:
        errors.append(f"project SaveGames changed: {project_save_changes!r}")
    if project_save_topology_after != project_save_topology_before:
        errors.append("project SaveGames directory topology changed")
    if content_topology_after != content_topology_before:
        errors.append("Content directory topology changed")
    if package_topology_after != package_topology_before:
        errors.append("packaged game directory topology changed")
    if production_after != production_before:
        errors.append("accepted production save changed")
    if backup_after != backup_before:
        errors.append("accepted external backup changed")
    if cleanup_error:
        errors.append(f"cleanup failed: {cleanup_error}")
    if external_residue:
        errors.append(f"external UUID UserDir residue remains: {user_dir}")

    status = AGGREGATE_PASS if not errors else "FAIL"
    isolated_profile_save_only_verified = (
        len(isolated_save_records) == len(phase_records)
        and len(phase_records) > 0
        and all(record.get("present") is True for record in isolated_save_records)
    )
    protected_boundaries_unchanged = (
        not content_changes
        and not package_changes
        and content_topology_after == content_topology_before
        and package_topology_after == package_topology_before
        and not project_save_changes
        and project_save_topology_after == project_save_topology_before
        and production_after == production_before
        and backup_after == backup_before
    )
    aggregate = {
        "schema": SCHEMA,
        "captured_utc": datetime.now(timezone.utc).isoformat(),
        "started_utc": started_utc,
        "status": status,
        "run_id": run_id,
        "manual_visual_review_required": 1,
        "manual_visual_review_status": "PENDING",
        "phase1_user_dir_fresh_before_launch": True,
        "isolated_profile_save_only": isolated_profile_save_only_verified,
        "protected_boundaries_unchanged": protected_boundaries_unchanged,
        "phase_count": len(phase_records),
        "capture_count": len(capture_records),
        "unique_capture_file_sha256_count": len({
            row["sha256"] for row in capture_records
        }),
        "unique_capture_rgb_sha256_count": len({
            row["decoded_pixel_evidence"]["rgb_sha256"]
            for row in capture_records
        }),
        "phases": phase_records,
        "captures": capture_records,
        "outfit_live_skin_diagnostic_collected_count": sum(
            1 for row in capture_records
            if isinstance(row.get("runtime_scene_metadata"), dict)
            and row["runtime_scene_metadata"].get(
                "metahuman_outfit_live_skin_evidence_collected"
            ) is True
        ),
        "outfit_live_skin_semantic_rejected_filenames": [
            str(row.get("filename", "")) for row in capture_records
            if isinstance(row.get("runtime_scene_metadata"), dict)
            and isinstance(row["runtime_scene_metadata"].get(
                "metahuman_outfit_live_skin"
            ), dict)
            and row["runtime_scene_metadata"]["metahuman_outfit_live_skin"].get(
                "semantic_accepted"
            ) is not True
        ],
        "isolated_save_transitions": isolated_save_records,
        "external_inventory_before_cleanup": external_inventory,
        "external_user_dir_removed": not external_residue,
        "content_file_count": len(content_after),
        "content_manifest_sha256_before":
            snapshot_manifest_sha256(content_before),
        "content_manifest_sha256_after":
            snapshot_manifest_sha256(content_after),
        "content_changed_paths": content_changes,
        "content_topology_before": content_topology_before,
        "content_topology_after": content_topology_after,
        "packaged_game_root": str(packaged_game_root),
        "packaged_game_file_count": len(package_after),
        "packaged_game_manifest_sha256_before":
            snapshot_manifest_sha256(package_before),
        "packaged_game_manifest_sha256_after":
            snapshot_manifest_sha256(package_after),
        "packaged_executable_iostore_records_before":
            package_protected_before,
        "packaged_executable_iostore_records_after":
            package_protected_after,
        "packaged_game_changed_paths": package_changes,
        "packaged_game_topology_before": package_topology_before,
        "packaged_game_topology_after": package_topology_after,
        "project_save_changed_paths": project_save_changes,
        "project_save_manifest_sha256_before":
            snapshot_manifest_sha256(project_saves_before),
        "project_save_manifest_sha256_after":
            snapshot_manifest_sha256(project_saves_after),
        "project_save_topology_before": project_save_topology_before,
        "project_save_topology_after": project_save_topology_after,
        "production_save_before": production_before,
        "production_save_after": production_after,
        "accepted_backup_before": backup_before,
        "accepted_backup_after": backup_after,
        "duration_seconds": time.monotonic() - started_monotonic,
        "errors": errors,
    }
    report_path.write_text(
        json.dumps(
            aggregate, indent=2, sort_keys=True, allow_nan=False,
        ) + "\n",
        encoding="utf-8",
    )
    print(f"{status}: {report_path}", flush=True)
    if status == AGGREGATE_PASS:
        print(
            "Machine acceptance passed; nine frames still require manual "
            "visual review.",
            flush=True,
        )
        return 0
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
