"""Run the non-mutating environment asset scan, proposal, and validation stages.

Usage:
  UnrealEditor-Cmd.exe DiscGolfTour.uproject -run=pythonscript \
    -script=Scripts/run-environment-asset-binding-workflow.py -unattended -NullRHI
"""

import unreal


ASSET_SET_PATH = "/Game/Environment/Forest/DA_TemperateMountainForest_Assets"
APPROVED_RUNTIME_MESH_ROOTS = [
    "/Game/Presentation/Course/PineRidge",
]


def normalize_vendor_root(root):
    normalized = root.strip().replace("\\", "/").rstrip("/")
    if not normalized.startswith("/Game/") or ".." in normalized.split("/"):
        return None
    return normalized


def configured_existing_runtime_roots():
    """Return only source-controlled runtime roots cleared for this workflow."""
    unique_roots = []
    seen = set()
    for configured_root in APPROVED_RUNTIME_MESH_ROOTS:
        root = normalize_vendor_root(configured_root)
        if not root:
            raise RuntimeError(
                f"Approved environment runtime root is malformed: {configured_root!r}"
            )
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
        "ENVIRONMENT_APPROVED_RUNTIME_ROOTS: "
        f"configured={len(unique_roots)} existing={len(existing_roots)} "
        f"missing={len(missing_roots)}"
    )
    if missing_roots:
        unreal.log(
            "ENVIRONMENT_APPROVED_RUNTIME_ROOTS_MISSING: "
            + ", ".join(missing_roots)
        )
    if not existing_roots:
        raise RuntimeError(
            "ENVIRONMENT_APPROVED_RUNTIME_ROOTS_EMPTY: no approved content is available"
        )
    return existing_roots


def candidate_count(scan):
    return sum(len(proposal.candidates) for proposal in scan.proposals)


asset_set = unreal.EditorAssetLibrary.load_asset(ASSET_SET_PATH)
if not asset_set:
    raise RuntimeError(f"Environment asset set is missing: {ASSET_SET_PATH}")

runtime_roots = configured_existing_runtime_roots()

scan = unreal.DiscGolfEnvironmentAssetBinder.scan_environment_assets(
    runtime_roots, asset_set
)
if not scan.provenance_accepted:
    raise RuntimeError(f"Environment scan provenance rejected: {scan.policy_error}")
unreal.log(
    "ENVIRONMENT_SCAN_OK: "
    f"roots={len(scan.vendor_content_roots)} "
    f"slots={len(scan.proposals)} candidates={candidate_count(scan)} "
    f"report={scan.report_path}"
)

proposal = unreal.DiscGolfEnvironmentAssetBinder.propose_bindings(
    runtime_roots, asset_set
)
if not proposal.provenance_accepted:
    raise RuntimeError(
        f"Environment proposal provenance rejected: {proposal.policy_error}"
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
