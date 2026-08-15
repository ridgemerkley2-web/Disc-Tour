"""Run the non-mutating environment asset scan, proposal, and validation stages.

Usage:
  UnrealEditor-Cmd.exe DiscGolfTour.uproject -run=pythonscript \
    -script=Scripts/run-environment-asset-binding-workflow.py -unattended -NullRHI
"""

import os

import unreal


ASSET_SET_PATH = "/Game/Environment/Forest/DA_TemperateMountainForest_Assets"
PLANNED_MESH_VENDOR_ROOTS = [
    "/Game/Environment/Vendors/ProjectNature/SpruceForest",
    "/Game/Environment/Vendors/Megascans/EuropeanBeech",
    "/Game/Environment/Vendors/ProjectNature/ConiferBushesSaplings01",
    "/Game/Environment/Vendors/ProjectNature/FernCollection",
    "/Game/Environment/Vendors/ProjectNature/FoliageCollection",
    "/Game/Environment/Vendors/Shadowmire/RockCollection04",
    "/Game/Environment/Vendors/GreenBugGames/StumpScanned",
]
VERIFIED_IMPORTED_MESH_VENDOR_ROOTS = [
    "/Game/PN_interactiveSpruceForest",
    "/Game/Stump_Scanned",
]
MANUAL_INTEGRATION_VENDOR_ROOTS = [
    "/Game/Environment/Vendors/ProjectNature/ForestLandscapeMaterials01",
    "/Game/Environment/Vendors/tharlevfx/WaterMaterials",
    "/Game/WaterMaterials",
]
EXTRA_VENDOR_ROOTS_ENV_VAR = "DISC_GOLF_ENVIRONMENT_VENDOR_ROOTS"


def normalize_vendor_root(root):
    normalized = root.strip().replace("\\", "/").rstrip("/")
    if not normalized.startswith("/Game/") or ".." in normalized.split("/"):
        return None
    return normalized


def is_same_or_child_root(root, parent_root):
    root_key = root.casefold()
    parent_key = parent_root.casefold()
    return root_key == parent_key or root_key.startswith(parent_key + "/")


def is_manual_integration_root(root, manual_roots):
    return any(is_same_or_child_root(root, manual_root) for manual_root in manual_roots)


def configured_existing_vendor_roots():
    """Return mesh-scan roots and log material-only/manual integration roots separately."""
    manual_roots = [
        root
        for configured_root in MANUAL_INTEGRATION_VENDOR_ROOTS
        if (root := normalize_vendor_root(configured_root))
    ]
    configured = (
        list(PLANNED_MESH_VENDOR_ROOTS)
        + list(VERIFIED_IMPORTED_MESH_VENDOR_ROOTS)
    )
    extras = os.environ.get(EXTRA_VENDOR_ROOTS_ENV_VAR, "")
    if extras:
        configured.extend(
            root for line in extras.splitlines() for root in line.split(";")
        )

    unique_roots = []
    seen = set()
    for configured_root in configured:
        root = normalize_vendor_root(configured_root)
        if not root:
            unreal.log_warning(
                f"ENVIRONMENT_VENDOR_ROOT_REJECTED: {configured_root!r}"
            )
            continue
        if is_manual_integration_root(root, manual_roots):
            unreal.log_warning(
                "ENVIRONMENT_VENDOR_ROOT_MANUAL_ONLY_REJECTED: "
                f"{configured_root!r}"
            )
            continue
        key = root.casefold()
        if key not in seen:
            seen.add(key)
            unique_roots.append(root)

    existing_roots = [
        root
        for root in unique_roots
        if unreal.EditorAssetLibrary.does_directory_exist(root)
    ]
    missing_roots = [root for root in unique_roots if root not in existing_roots]
    unreal.log(
        "ENVIRONMENT_VENDOR_ROOTS: "
        f"configured={len(unique_roots)} existing={len(existing_roots)} "
        f"missing={len(missing_roots)} extra_env={EXTRA_VENDOR_ROOTS_ENV_VAR}"
    )
    if missing_roots:
        unreal.log(
            "ENVIRONMENT_VENDOR_ROOTS_PENDING: " + ", ".join(missing_roots)
        )
    if not existing_roots:
        unreal.log_warning(
            "ENVIRONMENT_VENDOR_ROOTS_EMPTY: proposal will contain no imported candidates"
        )

    manual_existing = [
        root
        for root in manual_roots
        if unreal.EditorAssetLibrary.does_directory_exist(root)
    ]
    unreal.log(
        "ENVIRONMENT_MANUAL_INTEGRATION_ROOTS: "
        f"configured={len(manual_roots)} existing={len(manual_existing)} "
        "excluded_from_static_mesh_candidates=true roots="
        + ", ".join(manual_roots)
    )
    return existing_roots


def candidate_count(scan):
    return sum(len(proposal.candidates) for proposal in scan.proposals)


asset_set = unreal.EditorAssetLibrary.load_asset(ASSET_SET_PATH)
if not asset_set:
    raise RuntimeError(f"Environment asset set is missing: {ASSET_SET_PATH}")

vendor_roots = configured_existing_vendor_roots()

scan = unreal.DiscGolfEnvironmentAssetBinder.scan_environment_assets(
    vendor_roots, asset_set
)
unreal.log(
    "ENVIRONMENT_SCAN_OK: "
    f"roots={len(scan.vendor_content_roots)} "
    f"slots={len(scan.proposals)} candidates={candidate_count(scan)} "
    f"report={scan.report_path}"
)

proposal = unreal.DiscGolfEnvironmentAssetBinder.propose_bindings(
    vendor_roots, asset_set
)
unreal.log(
    "ENVIRONMENT_PROPOSAL_OK: "
    f"roots={len(proposal.vendor_content_roots)} "
    f"slots={len(proposal.proposals)} candidates={candidate_count(proposal)} "
    f"report={proposal.report_path} applied=false"
)

validation = (
    unreal.DiscGolfEnvironmentAssetBinder.validate_environment_asset_readiness(asset_set)
)
if validation.report_path == proposal.report_path:
    raise RuntimeError(
        "Candidate proposal and assigned-readiness reports must remain separate"
    )
if not validation.report_generated:
    raise RuntimeError("Environment asset validation report generation failed")
if not validation.structurally_complete:
    raise RuntimeError("Environment asset validation is missing one or more category slots")
if not validation.production_ready:
    unreal.log_warning(
        "ENVIRONMENT_PRODUCTION_READINESS_PENDING: inspect slot statuses before approval"
    )

unreal.log(
    "ENVIRONMENT_VALIDATION_OK: "
    f"slots=16 report={validation.report_path} "
    f"report_generated={str(validation.report_generated).lower()} "
    f"production_ready={str(validation.production_ready).lower()} applied=false"
)
