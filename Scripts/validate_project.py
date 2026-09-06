#!/usr/bin/env python3
from pathlib import Path, PurePosixPath
import argparse
import ast
import hashlib
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
HISTORICAL_RELEASE_SCOPE_CONTRACT = Path(
    "Config/DG_Session19ReleaseScopeContract.json"
)
VALIDATION_SCOPES = ("historical", "v05-shipping")


def resolve_release_scope_contract(
        root, scope, selected, *, require_exists=True):
    """Resolve a scope authority without permitting implicit historical mutation."""
    if scope not in VALIDATION_SCOPES:
        raise ValueError(f"unknown project validation scope: {scope}")
    if scope == "historical":
        if selected is not None:
            raise ValueError(
                "--release-scope-contract is valid only with --scope v05-shipping"
            )
        candidate = root / HISTORICAL_RELEASE_SCOPE_CONTRACT
    else:
        if selected is None:
            raise ValueError(
                "--scope v05-shipping requires an explicit "
                "--release-scope-contract"
            )
        candidate = selected if selected.is_absolute() else root / selected

    try:
        resolved = candidate.resolve(strict=require_exists)
        relative = resolved.relative_to(root.resolve())
    except (OSError, ValueError) as exc:
        raise ValueError("release-scope contract must resolve inside the project") from exc

    if not relative.parts or relative.parts[0] != "Config":
        raise ValueError("release-scope contract must be a project Config file")
    historical = (root / HISTORICAL_RELEASE_SCOPE_CONTRACT).resolve()
    if scope == "v05-shipping" and resolved == historical:
        raise ValueError(
            "v05-shipping requires a candidate-scoped contract; the historical "
            "Session 19 contract is immutable"
        )
    if require_exists:
        if not resolved.is_file() or resolved.stat().st_size == 0:
            raise ValueError("selected release-scope contract is missing or empty")
        cursor = root.resolve()
        for part in relative.parts:
            cursor /= part
            if cursor.is_symlink():
                raise ValueError("selected release-scope contract uses a symlink")
    token = PurePosixPath(*relative.parts).as_posix()
    return resolved, token


def partition_release_excluded_findings(
        findings, predicate, *, authority_valid, scope_decision):
    """Classify only findings explicitly superseded by a valid selected authority."""
    if not authority_valid:
        return list(findings), []
    retained = []
    excluded = []
    for finding in findings:
        if predicate(finding):
            excluded.append({
                "finding": finding,
                "classification": "RELEASE_EXCLUDED_ROADMAP_WORK",
                "scopeDecision": scope_decision,
            })
        else:
            retained.append(finding)
    return retained, excluded


def candidate_scope_identity_issues(
        contract, contract_token, historical_candidate_id):
    issues = []
    candidate = contract.get("currentShippingCandidateEvidence", {})
    candidate_id = candidate.get("candidateId") if isinstance(candidate, dict) else None
    if not isinstance(candidate_id, str) or not candidate_id:
        issues.append("selected candidate-scoped contract has no candidateId")
        return issues
    if candidate_id == historical_candidate_id:
        issues.append(
            "selected candidate-scoped contract still names the historical candidate"
        )
    if candidate_id not in Path(contract_token).name:
        issues.append(
            "selected candidate-scoped contract filename must contain its candidateId"
        )
    return issues


def run_scope_self_test():
    checks = 0
    failures = []

    def expect_pass(name, action):
        nonlocal checks
        checks += 1
        try:
            action()
        except Exception as exc:  # pragma: no cover - emitted by the harness
            failures.append(f"{name}: {exc}")

    def expect_fail(name, action):
        nonlocal checks
        checks += 1
        try:
            action()
        except ValueError:
            return
        failures.append(f"{name}: unexpectedly passed")

    expect_pass(
        "historical default",
        lambda: resolve_release_scope_contract(
            ROOT, "historical", None, require_exists=True),
    )
    expect_fail(
        "historical explicit selection",
        lambda: resolve_release_scope_contract(
            ROOT, "historical", HISTORICAL_RELEASE_SCOPE_CONTRACT,
            require_exists=True),
    )
    expect_fail(
        "candidate selection required",
        lambda: resolve_release_scope_contract(
            ROOT, "v05-shipping", None, require_exists=False),
    )
    expect_fail(
        "historical contract cannot masquerade as candidate scope",
        lambda: resolve_release_scope_contract(
            ROOT, "v05-shipping", HISTORICAL_RELEASE_SCOPE_CONTRACT,
            require_exists=True),
    )
    expect_fail(
        "outside-root contract rejected",
        lambda: resolve_release_scope_contract(
            ROOT, "v05-shipping", ROOT.parent / "outside.json",
            require_exists=False),
    )
    expect_fail(
        "missing candidate contract rejected",
        lambda: resolve_release_scope_contract(
            ROOT, "v05-shipping",
            Path("Config/DG_Session19ReleaseScopeContract-MISSING.json"),
            require_exists=True),
    )
    expect_pass(
        "candidate Config token",
        lambda: resolve_release_scope_contract(
            ROOT, "v05-shipping",
            Path(
                "Config/DG_Session19ReleaseScopeContract-"
                "S19_WindowsShipping_SELFTEST.json"
            ),
            require_exists=False),
    )

    sample = ["legacy missing", "current technical failure"]
    retained, excluded = partition_release_excluded_findings(
        sample, lambda finding: finding.startswith("legacy"),
        authority_valid=True, scope_decision="SELF_TEST_EXCLUSION",
    )
    expect_pass(
        "valid authority partitions only named roadmap finding",
        lambda: None if (
            retained == ["current technical failure"]
            and [entry["finding"] for entry in excluded] == ["legacy missing"]
        ) else (_ for _ in ()).throw(ValueError("partition mismatch")),
    )
    retained, excluded = partition_release_excluded_findings(
        sample, lambda finding: True,
        authority_valid=False, scope_decision="UNTRUSTED",
    )
    expect_pass(
        "invalid authority cannot suppress findings",
        lambda: None if retained == sample and excluded == []
        else (_ for _ in ()).throw(ValueError("fail-closed partition mismatch")),
    )

    fresh_id = "S19_WindowsShipping_20990101T000000Z_0123456789ab"
    fresh_contract = {"currentShippingCandidateEvidence": {"candidateId": fresh_id}}
    expect_pass(
        "fresh candidate identity binding",
        lambda: None if not candidate_scope_identity_issues(
            fresh_contract,
            f"Config/DG_Session19ReleaseScopeContract-{fresh_id}.json",
            "S19_WindowsShipping_HISTORICAL",
        ) else (_ for _ in ()).throw(ValueError("fresh binding rejected")),
    )
    expect_pass(
        "historical candidate identity rejected",
        lambda: None if len(candidate_scope_identity_issues(
            fresh_contract,
            f"Config/DG_Session19ReleaseScopeContract-{fresh_id}.json",
            fresh_id,
        )) == 1 else (_ for _ in ()).throw(ValueError("historical identity accepted")),
    )
    expect_pass(
        "candidate filename binding required",
        lambda: None if len(candidate_scope_identity_issues(
            fresh_contract, "Config/DG_Session19ReleaseScopeContract-other.json",
            "S19_WindowsShipping_HISTORICAL",
        )) == 1 else (_ for _ in ()).throw(ValueError("filename mismatch accepted")),
    )
    return checks, failures


parser = argparse.ArgumentParser(
    description=(
        "Validate project source or an explicitly selected v0.5 Shipping "
        "candidate/release-scope authority."
    )
)
parser.add_argument("--scope", choices=VALIDATION_SCOPES, default="historical")
parser.add_argument(
    "--release-scope-contract", type=Path,
    help=(
        "Candidate-scoped Config contract; required only for --scope "
        "v05-shipping."
    ),
)
parser.add_argument("--self-test", action="store_true")
args = parser.parse_args()

if args.self_test:
    checks, scope_self_test_failures = run_scope_self_test()
    if scope_self_test_failures:
        print(f"PROJECT VALIDATION SCOPE SELF-TEST FAILED: {checks} cases")
        for failure in scope_self_test_failures:
            print(" -", failure)
        sys.exit(1)
    print(f"PROJECT VALIDATION SCOPE SELF-TEST PASS: {checks}/{checks}")
    sys.exit(0)

try:
    selected_release_scope_path, selected_release_scope_token = (
        resolve_release_scope_contract(
            ROOT, args.scope, args.release_scope_contract, require_exists=True)
    )
except ValueError as exc:
    print(f"Project validation INVALID SCOPE: {exc}")
    sys.exit(1)

SESSION19_THROWLAB_RELOCATIONS = {
    "Source/DiscGolfTour/DiscThrowLabTypes.h":
        "Source/DiscGolfTourDeveloper/DiscThrowLabTypes.h",
    "Source/DiscGolfTour/DiscThrowLabSaveGame.h":
        "Source/DiscGolfTourDeveloper/DiscThrowLabSaveGame.h",
    "Source/DiscGolfTour/DiscThrowLabSubsystem.h":
        "Source/DiscGolfTourDeveloper/DiscThrowLabSubsystem.h",
    "Source/DiscGolfTour/DiscThrowLabSubsystem.cpp":
        "Source/DiscGolfTourDeveloper/DiscThrowLabSubsystem.cpp",
    "Source/DiscGolfTour/Tests/DiscThrowLabSession11Tests.cpp":
        "Source/DiscGolfTourDeveloper/Tests/DiscThrowLabSession11Tests.cpp",
}

SESSION19_CAREER_AI_RELOCATIONS = {
    "Source/DiscGolfTour/DiscGolfCompetitionRuntime.h":
        "Source/DiscGolfTourDeveloper/DiscGolfCompetitionRuntime.h",
    "Source/DiscGolfTour/DiscGolfCompetitionRuntime.cpp":
        "Source/DiscGolfTourDeveloper/DiscGolfCompetitionRuntime.cpp",
    "Source/DiscGolfTour/DiscGolfCareerProgressSaveGame.h":
        "Source/DiscGolfTourDeveloper/DiscGolfCareerProgressSaveGame.h",
    "Source/DiscGolfTour/DiscGolfCareerSubsystem.h":
        "Source/DiscGolfTourDeveloper/DiscGolfCareerSubsystem.h",
    "Source/DiscGolfTour/DiscGolfCareerSubsystem.cpp":
        "Source/DiscGolfTourDeveloper/DiscGolfCareerSubsystem.cpp",
    "Source/DiscGolfTour/DiscGolfAIPlannerAdapter.h":
        "Source/DiscGolfTourDeveloper/DiscGolfAIPlannerAdapter.h",
    "Source/DiscGolfTour/DiscGolfAIPlannerAdapter.cpp":
        "Source/DiscGolfTourDeveloper/DiscGolfAIPlannerAdapter.cpp",
    "Source/DiscGolfTour/DiscGolfSession14AISmokeRunner.h":
        "Source/DiscGolfTourDeveloper/DiscGolfSession14AISmokeRunner.h",
    "Source/DiscGolfTour/DiscGolfSession14AISmokeRunner.cpp":
        "Source/DiscGolfTourDeveloper/DiscGolfSession14AISmokeRunner.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession14CompetitionCareerTests.cpp":
        "Source/DiscGolfTourDeveloper/Tests/DiscGolfSession14CompetitionCareerTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession14AITests.cpp":
        "Source/DiscGolfTourDeveloper/Tests/DiscGolfSession14AITests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession14SmokeFoundationTests.cpp":
        "Source/DiscGolfTourDeveloper/Tests/DiscGolfSession14SmokeFoundationTests.cpp",
}

SESSION19_VERTICAL_SLICE_RELOCATIONS = {
    "Source/DiscGolfTour/DiscGolfSession15VerticalSliceContract.h":
        "Source/DiscGolfTourDeveloper/DiscGolfSession15VerticalSliceContract.h",
    "Source/DiscGolfTour/DiscGolfSession15VerticalSliceContract.cpp":
        "Source/DiscGolfTourDeveloper/DiscGolfSession15VerticalSliceContract.cpp",
    "Source/DiscGolfTour/DiscGolfSession15VerticalSliceRunner.h":
        "Source/DiscGolfTourDeveloper/DiscGolfSession15VerticalSliceRunner.h",
    "Source/DiscGolfTour/DiscGolfSession15VerticalSliceRunner.cpp":
        "Source/DiscGolfTourDeveloper/DiscGolfSession15VerticalSliceRunner.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession15VerticalSliceTests.cpp":
        "Source/DiscGolfTourDeveloper/Tests/DiscGolfSession15VerticalSliceTests.cpp",
}

SESSION19_DEVELOPER_RELOCATIONS = {
    **SESSION19_THROWLAB_RELOCATIONS,
    **SESSION19_CAREER_AI_RELOCATIONS,
    **SESSION19_VERTICAL_SLICE_RELOCATIONS,
}


def build_rule_dependencies(source):
    dependencies = {"Public": set(), "Private": set()}
    pattern = re.compile(
        r"(Public|Private)DependencyModuleNames\.Add(?:Range)?\s*\((.*?)\);",
        re.DOTALL,
    )
    for match in pattern.finditer(source):
        dependencies[match.group(1)].update(
            re.findall(r'"([^"\r\n]+)"', match.group(2))
        )
    return dependencies

required = [
    "DiscGolfTour.uproject",
    "AGENTS.md",
    "Source/DiscGolfTour/DiscGolfTour.Build.cs",
    "Source/DiscGolfTour/DiscGolfQualityAdapter.h",
    "Source/DiscGolfTour/DiscGolfQualityAdapter.cpp",
    "Source/DiscGolfTour/DiscGolfBuiltInEnvironmentProvider.h",
    "Source/DiscGolfTour/DiscGolfBuiltInEnvironmentProvider.cpp",
    "Source/DiscGolfTour/DevCourseBootstrap.h",
    "Source/DiscGolfTour/DevCourseBootstrap.cpp",
    "Source/DiscGolfTour/DiscGolfTourGameMode.h",
    "Source/DiscGolfTour/DiscFlightComponent.cpp",
    "Source/DiscGolfTour/DiscReplayActor.cpp",
    "Source/DiscGolfTour/DiscReplayActor.h",
    "Source/DiscGolfTour/DiscGolfPresentationMath.h",
    "Source/DiscGolfTour/DiscBroadcastCameraDirector.cpp",
    "Source/DiscGolfTour/DiscBroadcastCameraDirector.h",
    "Source/DiscGolfTour/DiscGolfBroadcastCameraMath.h",
    "Source/DiscGolfTour/DiscEquipmentDataAssets.cpp",
    "Source/DiscGolfTour/DiscEquipmentDataAssets.h",
    "Source/DiscGolfTour/DiscCatalogSubsystem.cpp",
    "Source/DiscGolfTour/DiscCatalogSubsystem.h",
    "Source/DiscGolfTour/DiscGolfCourseRules.cpp",
    "Source/DiscGolfTour/DiscGolfCourseRules.h",
    "Source/DiscGolfTour/DiscGolfCourseSurfaceActor.cpp",
    "Source/DiscGolfTour/DiscGolfCourseSurfaceActor.h",
    "Source/DiscGolfTour/DiscGolfCourseDefinition.cpp",
    "Source/DiscGolfTour/DiscGolfCourseDefinition.h",
    "Source/DiscGolfTour/DiscGolfRouteTelemetry.cpp",
    "Source/DiscGolfTour/DiscGolfRouteTelemetry.h",
    "Source/DiscGolfTour/DiscGolfCoursePresentationDefinition.cpp",
    "Source/DiscGolfTour/DiscGolfCoursePresentationDefinition.h",
    "Source/DiscGolfTour/DiscGolfFoliagePresentationActor.cpp",
    "Source/DiscGolfTour/DiscGolfFoliagePresentationActor.h",
    "Source/DiscGolfTour/DiscGolfWaterPresentationActor.cpp",
    "Source/DiscGolfTour/DiscGolfWaterPresentationActor.h",
    "Source/DiscGolfTour/DiscGolferPresentationComponent.cpp",
    "Source/DiscGolfTour/DiscGolferPresentationComponent.h",
    "Source/DiscGolfTour/DiscGolfRoundState.cpp",
    "Source/DiscGolfTour/DiscGolfRoundState.h",
    "Source/DiscGolfTour/DiscGolfCourseFeatureActor.cpp",
    "Source/DiscGolfTour/DiscGolfCourseFeatureActor.h",
    "Source/DiscGolfTour/DiscGolfWindZoneActor.cpp",
    "Source/DiscGolfTour/DiscGolfWindZoneActor.h",
    "Source/DiscGolfTour/DiscGolfFlyoverRouteActor.cpp",
    "Source/DiscGolfTour/DiscGolfFlyoverRouteActor.h",
    "Source/DiscGolfTour/Tests/DiscGolfCourseDefinitionTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfQualityAdapterTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfBuiltInEnvironmentProviderTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfRouteTelemetryTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfRoundStateTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfPresentationTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfBroadcastCameraTests.cpp",
    "Source/DiscGolfTour/Tests/DiscCatalogTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfCourseRulesTests.cpp",
    "Source/DiscGolfTour/DiscGolfInputConfig.cpp",
    "Source/DiscGolfTour/DiscGolfInputConfig.h",
    "Source/DiscGolfTour/DiscGolfMath.h",
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfReleaseTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfNonAuthorityThrowIngressTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfThrowIngressTests.cpp",
    "Docs/ARCHITECTURE.md",
    "Scripts/validate_presentation_capture.py",
    "Scripts/validate_route_telemetry.py",
    "Scripts/generate_disc_data_assets.py",
    "Scripts/download-pine-ridge-cc0-assets.ps1",
    "Scripts/import-pine-ridge-assets.py",
    "Scripts/run-environment-asset-binding-workflow.py",
    "Source/DiscGolfTourEditor/DiscGolfEnvironmentAssetBinder.h",
    "Source/DiscGolfTourEditor/DiscGolfEnvironmentAssetBinder.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfEnvironmentAssetBinderTests.cpp",
    "Data/PineRidgeHole1.json",
    "Data/PineRidgeHole2.json",
    "Data/PineRidgeHole3.json",
    "Data/PineRidgeCourse.json",
    "Data/PineRidgePresentation.json",
    "Config/DG_BrandLicenseContract.json",
    "Docs/DG_BRAND_ASSET_AUDIT.md",
    "Scripts/validate_dg_session9_brand_license.py",
    "Source/DiscGolfTour/Tests/DiscGolfBrandLicenseTests.cpp",
    "Config/DG_Session10EnvironmentContract.json",
    "Docs/DG_SESSION10_ENVIRONMENT_AUDIT.md",
    "Scripts/validate_dg_session10_environment.py",
    "Source/DiscGolfTour/Tests/DiscGolfSession10EnvironmentContractTests.cpp",
    "Config/DG_Session11EquipmentThrowLabContract.json",
    "Docs/DG_SESSION11_EQUIPMENT_THROW_LAB_AUDIT.md",
    "Scripts/validate_dg_session11_equipment_throw_lab.py",
    "Source/DiscGolfTour/DiscThrowLabTypes.h",
    "Source/DiscGolfTour/DiscThrowLabSaveGame.h",
    "Source/DiscGolfTour/DiscEquipmentSaveGame.h",
    "Source/DiscGolfTour/DiscThrowLabSubsystem.h",
    "Source/DiscGolfTour/DiscThrowLabSubsystem.cpp",
    "Source/DiscGolfTour/Tests/DiscEquipmentSession11Tests.cpp",
    "Source/DiscGolfTour/Tests/DiscThrowLabSession11Tests.cpp",
    "Config/DG_Session12PresentationContract.json",
    "Docs/DG_SESSION12_PRESENTATION_AUDIT.md",
    "Scripts/validate_dg_session12_presentation.py",
    "Config/DG_Session13CourseAuthoringPcgContract.json",
    "Docs/DG_SESSION13_COURSE_AUTHORING_PCG_AUDIT.md",
    "Scripts/validate_dg_session13_course_authoring_pcg.py",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringActors.h",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringActors.cpp",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringValidation.h",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringValidation.cpp",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringExporter.h",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringExporter.cpp",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringPcgAdapter.h",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringPcgAdapter.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfCourseAuthoringActorsTests.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfCourseAuthoringValidationTests.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfCourseAuthoringExporterTests.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfCourseAuthoringPcgAdapterTests.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfSession13CourseAuthoringFixtureTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession13CourseJsonTests.cpp",
    "Config/DG_Session15VerticalSliceContract.json",
    "Scripts/validate_dg_session15_vertical_slice.py",
    "Config/DG_Session18PolyHavenProvenanceContract.json",
    "SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json",
    "Evidence/Session18/PolyHavenReimport.log",
    "Evidence/Session18/PineRidgeSemanticValidation.json",
    "Evidence/Session18/BuildCookRun.log",
    "Evidence/Session18/Manifest_UFSFiles_Win64.txt",
    "Evidence/Session18/ArchiveFiles.tsv",
    "Evidence/Session18/PackagedPineRidgePlaySmoke.log",
    "Evidence/Session18/PolyHavenPackageIdentityReceipt.json",
    "Scripts/validate-pine-ridge-provenance-semantics.py",
    "Scripts/validate_dg_session18_poly_haven_provenance.py",
    "Docs/DG_SESSION18_POLY_HAVEN_PROVENANCE_AUDIT.md",
    "Config/DG_Session19ReleaseScopeContract.json",
    "Config/DG_Session19ShippingContentPolicy.json",
    "Config/DG_Session19StagedProvenancePolicy.json",
    "Config/DG_Session19BakedPcgPolicy.json",
    "Evidence/Session19/BakedPcgSourceAuthorityReceipt.json",
    "Evidence/Session19/UnusedFabExternalQuarantineReceipt.json",
    "Evidence/Session19/QuarantinedFabBeforeMove.tsv",
    "Evidence/Session19/CharacterFrameworkBeforeMove-S19_Framework_20260825T031617Z_c68ee2e4e6be.tsv",
    "Evidence/Session19/CharacterFrameworkExternalQuarantine-S19_Framework_20260825T031617Z_c68ee2e4e6be.json",
    "Config/DG_Session19ManualReleaseReviewPolicy.json",
    "Scripts/validate_dg_session19_manual_release_review.py",
    "Docs/DG_SESSION19_MANUAL_RELEASE_REVIEW.md",
    "Evidence/Session19/ManualReleaseReviewInventory.json",
    "Evidence/Session19/ManualReleaseDecisionRecord.template.json",
    "Config/DG_PhysicsMeasuredReferencePolicy.json",
    "Scripts/validate_dg_physics_measured_reference.py",
    "Evidence/Session19/PhysicsMeasuredReferenceDataset.template.json",
    "Scripts/validate_dg_session19_release_scope.py",
    "Scripts/generate_dg_session19_staged_provenance.py",
    "Scripts/validate_dg_session19_baked_pcg.py",
    "Scripts/run-session15-hole1-acceptance.py",
    "Docs/DG_SESSION15_VERTICAL_SLICE_AUDIT.md",
]

errors = []
release_excluded_roadmap_findings = []
selected_release_scope_binding = None
selected_release_scope_candidate_id = None
selected_release_scope_release_blockers = []
for rel in required:
    if not (ROOT / rel).exists():
        errors.append(f"missing required file: {rel}")

try:
    math_source = (ROOT / "Source/DiscGolfTour/DiscGolfMath.h").read_text(
        encoding="utf-8"
    )
    game_mode_source = (
        ROOT / "Source/DiscGolfTour/DiscGolfTourGameMode.cpp"
    ).read_text(encoding="utf-8")
    game_mode_header_source = (
        ROOT / "Source/DiscGolfTour/DiscGolfTourGameMode.h"
    ).read_text(encoding="utf-8")
    release_test_source = (
        ROOT / "Source/DiscGolfTour/Tests/DiscGolfReleaseTests.cpp"
    ).read_text(encoding="utf-8")
    non_authority_ingress_test_source = (
        ROOT / "Source/DiscGolfTour/Tests/DiscGolfNonAuthorityThrowIngressTests.cpp"
    ).read_text(encoding="utf-8")
    golfer_pawn_source = (
        ROOT / "Source/DiscGolfTour/DiscGolferPawn.cpp"
    ).read_text(encoding="utf-8")
    rhbh_adapter_source = (
        ROOT / "Source/DiscGolfTour/DiscGolfRHBHThrowAdapterComponent.cpp"
    ).read_text(encoding="utf-8")
    throw_ingress_test_source = (
        ROOT / "Source/DiscGolfTour/Tests/DiscGolfThrowIngressTests.cpp"
    ).read_text(encoding="utf-8")
    aim_rebase_helper = "TryRebaseReleaseDirectionToPreserveAimPoint"
    release_origin_preflight = "TryPrepareReleaseForOriginOverride"

    if math_source.count(f"inline bool {aim_rebase_helper}(") != 1:
        errors.append("grip-origin aim rebase helper topology is not singular")
    if game_mode_source.count(f"DiscGolfMath::{aim_rebase_helper}(") != 1:
        errors.append("game mode grip-origin aim rebase call topology is not singular")
    if game_mode_header_source.count(
        f"static bool {release_origin_preflight}("
    ) != 1:
        errors.append("release-origin preflight seam declaration is not singular")
    if game_mode_source.count(
        f"ADiscGolfTourGameMode::{release_origin_preflight}("
    ) != 1:
        errors.append("release-origin preflight seam definition is not singular")

    preflight_definition_index = game_mode_source.find(
        f"ADiscGolfTourGameMode::{release_origin_preflight}("
    )
    null_guard_index = game_mode_source.find(
        "if (!ReleaseLocationOverrideCm)", preflight_definition_index
    )
    rebase_call_index = game_mode_source.find(
        f"DiscGolfMath::{aim_rebase_helper}(", null_guard_index
    )
    rebase_assignment_index = game_mode_source.find(
        "InOutRelease.Direction = AimPreservingDirection;",
        rebase_call_index,
    )
    launch_release_index = game_mode_source.find(
        "FThrowRelease CandidateRelease =", rebase_assignment_index
    )
    preflight_call_index = game_mode_source.find(
        f"{release_origin_preflight}(", launch_release_index
    )
    release_validation_index = game_mode_source.find(
        "DiscGolfMath::IsThrowReleaseValid(CandidateRelease",
        preflight_call_index,
    )
    spawn_index = game_mode_source.find(
        "SpawnActor<ADiscActor>", release_validation_index
    )
    if not (
        0 <= preflight_definition_index < null_guard_index < rebase_call_index
        < rebase_assignment_index < launch_release_index < preflight_call_index
        < release_validation_index < spawn_index
    ):
        errors.append(
            "release-origin preflight is not null-safe, atomic, and ordered before spawn"
        )
    for invariant_token in (
        "MaximumAnimatedGripOriginDistanceCm = 300.0f",
        "const FVector AimLineOriginCm = CandidateThrowStartLieLocation;",
        "AimLineOriginCm, ActiveHole->BasketLocation",
        "FVector::DistSquared(",
        "AimReferenceDistanceCm <= SMALL_NUMBER",
        "DiscGolfMath::TryRebaseReleaseDirectionToPreserveAimPoint(",
    ):
        if invariant_token not in game_mode_header_source + game_mode_source:
            errors.append(
                f"missing animated pre-animation origin/range invariant: {invariant_token}"
            )

    if game_mode_source.count("LaunchThrow(Command, &GripLocation)") != 1:
        errors.append("animated grip override ingress topology changed")
    if game_mode_source.count("return LaunchThrow(Command);") != 1:
        errors.append("synchronous throw fallback topology changed")
    if game_mode_source.count("LaunchThrow(Command, nullptr, true)") != 1:
        errors.append("trusted regression throw fallback topology changed")

    for test_token in (
        "DiscGolfTour.Physics.ReleaseOrigin.AimPointPreservation",
        "DiscGolfTour.Physics.ReleaseOrigin.FailClosed",
    ):
        if test_token not in release_test_source:
            errors.append(f"missing grip-origin release regression test: {test_token}")
    if "DiscGolfTour.Gameplay.ThrowIngress.ReleaseOriginPreflight" \
            not in throw_ingress_test_source:
        errors.append("missing GameMode release-origin preflight integration test")
    for routing_token in (
        "Circle 1 right-backhand putt is eligible for authored motion",
        "Circle 2 right-backhand putt is eligible for authored motion",
        "Circle 1 right-backhand putt begins the guarded transaction",
        "Circle 2 right-backhand putt begins the guarded transaction",
        "A valid right-backhand command cannot cross shot contexts",
        "Forehand is outside the animated RHBH slice",
        "Left-handed command cannot start RHBH presentation",
    ):
        if routing_token not in non_authority_ingress_test_source:
            errors.append(
                f"missing guarded authored-RHBH routing coverage: {routing_token}"
            )
    for forbidden_gate in (
        "AuthoritativeCommand.ShotContext != EDiscShotContext::Drive",
        "Command.ShotContext != EDiscShotContext::Drive",
    ):
        if forbidden_gate in golfer_pawn_source:
            errors.append(
                f"pawn still hard-gates authored RHBH motion to Drive: {forbidden_gate}"
            )
    if "Command.ShotContext == EDiscShotContext::Drive" in rhbh_adapter_source:
        errors.append("RHBH adapter still hard-gates authored motion to Drive")
    for routing_source_token in (
        "Command.ShotContext == ExpectedShotContext",
        "Command.Handedness == EDGHandedness::Right",
        "CurrentProfileHandedness == EDGHandedness::Right",
    ):
        if routing_source_token not in rhbh_adapter_source:
            errors.append(
                f"RHBH adapter fail-closed routing seam missing: {routing_source_token}"
            )
    for montage_source_token in (
        "UAnimMontage* ADiscGolferPawn::ResolveRHBHThrowMontage(",
        "const FName ExpectedFamilyId = MotionFamilyId(AnimationFamily);",
        "Entry.MotionFamilyId != ExpectedFamilyId",
        "FallbackMontage = ProductionDriveMontage;",
        "FallbackMontage = ProductionApproachMontage;",
        "FallbackMontage = ProductionPuttMontage;",
        "return IsUsableMontage(FallbackMontage) ? FallbackMontage : nullptr;",
    ):
        if montage_source_token not in golfer_pawn_source:
            errors.append(
                f"pawn authored motion-family selection missing: {montage_source_token}"
            )
except Exception as exc:
    errors.append(f"invalid grip-origin aim rebase topology: {exc}")

session19_supersedes_internal_fab_quarantine = False
session19_developer_relocation_valid = False
session19_foundation_runtime_migration_valid = False
session19_supersedes_developer_relocation_paths = False
session19_supersedes_core_throwlab_entrypoints = False
session19_supersedes_framework_dependency_expectation = False
session19_framework_external_quarantine_valid = False
session19_supersedes_runtime_cook_manifest_always_cook = False
session14_only_relocation_issues = False
session14_only_superseded_issues = False
session14_issues = []
session15_only_relocation_issues = False
session19_errors = []
session19_summary = {}
try:
    binding_runner_path = ROOT / "Scripts/run-environment-asset-binding-workflow.py"
    binding_runner_source = binding_runner_path.read_text(encoding="utf-8")
    binding_runner_tree = ast.parse(binding_runner_source)
    literal_lists = {}
    for node in binding_runner_tree.body:
        if (isinstance(node, ast.Assign) and len(node.targets) == 1
                and isinstance(node.targets[0], ast.Name)
                and isinstance(node.value, (ast.List, ast.Tuple))):
            literal_lists[node.targets[0].id] = ast.literal_eval(node.value)

    approved_roots = literal_lists.get("APPROVED_RUNTIME_MESH_ROOTS", [])
    expected_approved_roots = ["/Game/Presentation/Course/PineRidge"]
    quarantine_roots = {
        "/Game/PN_interactiveSpruceForest",
        "/Game/Stump_Scanned",
        "/Game/WaterMaterials",
    }
    runnable_literal_roots = {
        value
        for name, values in literal_lists.items()
        if "ROOT" in name
        for value in values
        if isinstance(value, str)
    }
    if approved_roots != expected_approved_roots:
        errors.append("environment binder runner approved roots differ from the single project runtime root")
    if runnable_literal_roots & quarantine_roots:
        errors.append("quarantined package roots leaked into environment binder runnable root lists")
    if any(root in binding_runner_source for root in quarantine_roots):
        errors.append("quarantined package roots leaked into environment binder source")
    if "os.environ" in binding_runner_source or "os.getenv" in binding_runner_source:
        errors.append("environment binder runner allows an environment root override")
    if "scan_environment_assets" not in binding_runner_source or "propose_bindings" not in binding_runner_source:
        errors.append("environment binder runner is missing scan/proposal stages")
    if "if not scan.provenance_accepted:" not in binding_runner_source:
        errors.append("environment binder scan does not fail closed on provenance rejection")
    if "if not proposal.provenance_accepted:" not in binding_runner_source:
        errors.append("environment binder proposal does not fail closed on provenance rejection")
    if "apply_approved_bindings" in binding_runner_source.casefold():
        errors.append("non-mutating environment binder runner invokes asset approval")
except Exception as exc:
    errors.append(f"invalid environment asset binding runner configuration: {exc}")

try:
    project = json.loads((ROOT / "DiscGolfTour.uproject").read_text(encoding="utf-8"))
    module_entries = project.get("Modules", [])
    modules = {m.get("Name") for m in module_entries}
    if "DiscGolfTour" not in modules:
        errors.append("DiscGolfTour module missing from .uproject")

    developer_entries = [
        entry for entry in module_entries
        if entry.get("Name") == "DiscGolfTourDeveloper"
    ]
    foundation_entries = [
        entry for entry in module_entries
        if entry.get("Name") == "DiscGolfRuntimeFoundation"
    ]
    developer_relocation_issues = []
    if len(developer_entries) != 1:
        developer_relocation_issues.append(
            "DiscGolfTourDeveloper must have exactly one .uproject module entry"
        )
    else:
        developer_entry = developer_entries[0]
        if developer_entry.get("Type") != "DeveloperTool":
            developer_relocation_issues.append(
                "DiscGolfTourDeveloper module type must be DeveloperTool"
            )
        if developer_entry.get("TargetConfigurationDenyList") != ["Shipping"]:
            developer_relocation_issues.append(
                "DiscGolfTourDeveloper must have the exact Shipping configuration deny-list"
            )

    developer_build_path = (
        ROOT / "Source/DiscGolfTourDeveloper/DiscGolfTourDeveloper.Build.cs"
    )
    developer_module_path = (
        ROOT / "Source/DiscGolfTourDeveloper/DiscGolfTourDeveloperModule.cpp"
    )
    runtime_build_path = ROOT / "Source/DiscGolfTour/DiscGolfTour.Build.cs"
    if not developer_build_path.is_file() or developer_build_path.stat().st_size == 0:
        developer_relocation_issues.append(
            "DiscGolfTourDeveloper module rules are missing or empty"
        )
    if not developer_module_path.is_file() or developer_module_path.stat().st_size == 0:
        developer_relocation_issues.append(
            "DiscGolfTourDeveloper module implementation is missing or empty"
        )

    runtime_build_source = runtime_build_path.read_text(encoding="utf-8")
    runtime_dependencies = build_rule_dependencies(runtime_build_source)
    all_runtime_dependencies = (
        runtime_dependencies["Public"] | runtime_dependencies["Private"]
    )
    if "DiscGolfTourDeveloper" in all_runtime_dependencies:
        developer_relocation_issues.append(
            "DiscGolfTour runtime module must not depend on DiscGolfTourDeveloper"
        )

    if developer_build_path.is_file():
        developer_build_source = developer_build_path.read_text(encoding="utf-8")
        developer_dependencies = build_rule_dependencies(developer_build_source)
        all_developer_dependencies = (
            developer_dependencies["Public"] | developer_dependencies["Private"]
        )
        for dependency in ("DiscGolfTour", "DiscGolfRuntimeFoundation"):
            if dependency not in all_developer_dependencies:
                developer_relocation_issues.append(
                    f"DiscGolfTourDeveloper must depend on {dependency}"
                )

    for historical_path, developer_path in SESSION19_DEVELOPER_RELOCATIONS.items():
        relocated_file = ROOT / developer_path
        if not relocated_file.is_file() or relocated_file.stat().st_size == 0:
            developer_relocation_issues.append(
                f"required relocated DeveloperTool file is missing or empty: {developer_path}"
            )
        if (ROOT / historical_path).exists():
            developer_relocation_issues.append(
                f"relocated DeveloperTool file remains in the runtime module: {historical_path}"
            )

    session19_developer_relocation_valid = not developer_relocation_issues
    errors.extend(
        f"Session 19 DeveloperTool relocation: {issue}"
        for issue in developer_relocation_issues
    )

    foundation_migration_issues = []
    if len(foundation_entries) != 1:
        foundation_migration_issues.append(
            "DiscGolfRuntimeFoundation must have exactly one .uproject module entry"
        )
    elif foundation_entries[0].get("Type") != "Runtime":
        foundation_migration_issues.append(
            "DiscGolfRuntimeFoundation module type must be Runtime"
        )
    foundation_build_path = (
        ROOT / "Source/DiscGolfRuntimeFoundation/DiscGolfRuntimeFoundation.Build.cs"
    )
    if not foundation_build_path.is_file() or foundation_build_path.stat().st_size == 0:
        foundation_migration_issues.append(
            "DiscGolfRuntimeFoundation module rules are missing or empty"
        )
    if "DiscGolfRuntimeFoundation" not in runtime_dependencies["Public"]:
        foundation_migration_issues.append(
            "DiscGolfTour must publicly depend on DiscGolfRuntimeFoundation"
        )
    if "DiscGolfCharacterFramework" in all_runtime_dependencies:
        foundation_migration_issues.append(
            "DiscGolfTour retains a legacy DiscGolfCharacterFramework dependency"
        )

    session19_foundation_runtime_migration_valid = not foundation_migration_issues
    errors.extend(
        f"Session 19 runtime-foundation migration: {issue}"
        for issue in foundation_migration_issues
    )
except Exception as exc:
    errors.append(f"invalid .uproject JSON: {exc}")

try:
    course = json.loads((ROOT / "Data/PineRidgeHole1.json").read_text(encoding="utf-8"))
    if course.get("schema") != "disc_golf_hole_blockout" or course.get("schemaVersion") != 1:
        errors.append("Pine Ridge course schema identity/version is invalid")
    expected_counts = {
        "surfaces": 11,
        "trees": 12,
        "landingZones": 2,
        "cameraAnchors": 3,
        "spectatorBoundaries": 3,
        "windZones": 2,
        "flyoverPointsCm": 6,
    }
    for field, expected_count in expected_counts.items():
        if len(course.get(field, [])) != expected_count:
            errors.append(f"Pine Ridge {field} count is not {expected_count}")
    ids = []
    for field in ("surfaces", "landingZones", "cameraAnchors", "spectatorBoundaries", "windZones"):
        ids.extend(item.get("id") for item in course.get(field, []))
    if None in ids or len(ids) != len(set(ids)):
        errors.append("Pine Ridge feature IDs must be present and globally unique")
    if {anchor.get("mode") for anchor in course.get("cameraAnchors", [])} != {"Launch", "Fairway", "Finish"}:
        errors.append("Pine Ridge camera anchors must cover launch, fairway, and finish")
except Exception as exc:
    errors.append(f"invalid Pine Ridge course JSON: {exc}")

try:
    manifest = json.loads((ROOT / "Data/PineRidgeCourse.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != "disc_golf_course_manifest" or manifest.get("schemaVersion") != 1:
        errors.append("Pine Ridge manifest schema identity/version is invalid")
    holes = manifest.get("holes", [])
    if [entry.get("holeNumber") for entry in holes] != [1, 2, 3]:
        errors.append("Pine Ridge manifest must contain contiguous holes 1-3")
    if [entry.get("definitionFile") for entry in holes] != [
        "Data/PineRidgeHole1.json", "Data/PineRidgeHole2.json", "Data/PineRidgeHole3.json"
    ]:
        errors.append("Pine Ridge manifest hole paths are not stable")
    expected_origins = [(0, 0, 0), (14500, 4000, 100), (36000, 10500, -250)]
    actual_origins = [tuple(entry.get("worldOriginCm", {}).get(axis) for axis in ("x", "y", "z"))
                      for entry in holes]
    if actual_origins != expected_origins:
        errors.append("Pine Ridge persistent-world hole origins are not the approved contiguous layout")
    if [entry.get("worldYawDeg") for entry in holes] != [0, 8, -12]:
        errors.append("Pine Ridge persistent-world hole yaws are not stable")

    expected = {
        1: {"par": 3, "surfaces": 11, "trees": 12, "flyoverPointsCm": 6},
        2: {"par": 4, "surfaces": 12, "trees": 18, "flyoverPointsCm": 7},
        3: {"par": 4, "surfaces": 12, "trees": 14, "flyoverPointsCm": 7},
    }
    total_par = 0
    for hole_number, counts in expected.items():
        definition = json.loads((ROOT / f"Data/PineRidgeHole{hole_number}.json").read_text(encoding="utf-8"))
        if definition.get("courseId") != manifest.get("courseId") or definition.get("layoutId") != manifest.get("layoutId"):
            errors.append(f"Pine Ridge hole {hole_number} identity does not match the manifest")
        if definition.get("holeNumber") != hole_number or definition.get("par") != counts["par"]:
            errors.append(f"Pine Ridge hole {hole_number} metadata is invalid")
        total_par += definition.get("par", 0)
        for field in ("surfaces", "trees", "flyoverPointsCm"):
            if len(definition.get(field, [])) != counts[field]:
                errors.append(f"Pine Ridge hole {hole_number} {field} count is not {counts[field]}")
        for field, minimum in (("landingZones", 2), ("cameraAnchors", 3), ("spectatorBoundaries", 3), ("windZones", 2)):
            if len(definition.get(field, [])) < minimum:
                errors.append(f"Pine Ridge hole {hole_number} {field} coverage is incomplete")
        feature_ids = []
        for field in ("surfaces", "landingZones", "cameraAnchors", "spectatorBoundaries", "windZones"):
            feature_ids.extend(item.get("id") for item in definition.get(field, []))
        if None in feature_ids or len(feature_ids) != len(set(feature_ids)):
                errors.append(f"Pine Ridge hole {hole_number} feature IDs must be globally unique")
        if hole_number == 2:
            routes = definition.get("shotRoutes", [])
            if ({route.get("id") for route in routes}
                    != {"NeedlePlacement", "LateCrosswindAttack", "LeftPitchOut"}):
                errors.append("Needle Gate telemetry route IDs are not stable")
    if total_par != 11:
        errors.append("Pine Ridge three-hole layout par is not 11")
except Exception as exc:
    errors.append(f"invalid Pine Ridge course manifest or hole set: {exc}")

try:
    presentation = json.loads((ROOT / "Data/PineRidgePresentation.json").read_text(encoding="utf-8"))
    if presentation.get("schema") != "disc_golf_course_presentation" or presentation.get("schemaVersion") != 1:
        errors.append("Pine Ridge presentation schema identity/version is invalid")
    if presentation.get("courseId") != "PineRidgeChampionship" or presentation.get("layoutId") != "Championship":
        errors.append("Pine Ridge presentation identity does not match the course")
    if not presentation.get("collisionInvariantAcrossQuality"):
        errors.append("Pine Ridge presentation quality must preserve competitive collision")
    collision_profile = presentation.get("collisionProfileId")
    quality_tiers = presentation.get("qualityTiers", [])
    if {tier.get("id") for tier in quality_tiers} != {"Low", "Medium", "High"}:
        errors.append("Pine Ridge presentation must define Low, Medium, and High tiers")
    for tier in quality_tiers:
        if tier.get("affectsCollision") or tier.get("collisionProfileId") != collision_profile:
            errors.append(f"Pine Ridge {tier.get('id')} quality tier changes competitive collision")
    presentation_holes = presentation.get("holes", [])
    if [hole.get("holeNumber") for hole in presentation_holes] != [1, 2, 3]:
        errors.append("Pine Ridge presentation must cover contiguous holes 1-3")
    if [hole.get("forestReferenceId") for hole in presentation_holes] != [
        "OpeningBroadTreeLine", "NeedleCanopyCompression", "GalleryLakeFrame"
    ]:
        errors.append("Pine Ridge forest-design references are incomplete or reordered")
    if any(hole.get("forestDepthCm", 0) < 4000 or hole.get("canopyDensityScale", 0) < 1.1
           for hole in presentation_holes):
        errors.append("Every Pine Ridge hole must retain the dense-forest depth/density floor")
    for path in presentation.get("assets", {}).values():
        if not isinstance(path, str) or not path.startswith("/Game/") or " " in path:
            errors.append("Pine Ridge presentation asset paths must be stable /Game paths")
except Exception as exc:
    errors.append(f"invalid Pine Ridge presentation JSON: {exc}")

input_config = (ROOT / "Config/DefaultInput.ini").read_text(encoding="utf-8")
if "+ActionMappings=" in input_config or "+AxisMappings=" in input_config:
    errors.append("legacy action/axis mappings returned to DefaultInput.ini")
if "EnhancedInput.EnhancedPlayerInput" not in input_config or "EnhancedInput.EnhancedInputComponent" not in input_config:
    errors.append("Enhanced Input classes missing from DefaultInput.ini")

game_config = (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8")
for asset_type in ("DiscMold", "DiscPlastic"):
    if f'PrimaryAssetType="{asset_type}"' not in game_config or "CookRule=AlwaysCook" not in game_config:
        errors.append(f"{asset_type} Asset Manager scan/cook rule missing from DefaultGame.ini")
if 'DirectoriesToAlwaysCook=(Path="/Game/Data/Discs")' not in game_config:
    errors.append("disc data packaging directory is not explicitly staged for cooking")
if 'DirectoriesToAlwaysCook=(Path="/Game/Presentation/Course/PineRidge")' not in game_config:
    errors.append("Pine Ridge presentation directory is not explicitly staged for cooking")

expected_assets = {
    "Content/Data/Discs/Molds": 5,
    "Content/Data/Discs/Plastics": 3,
}
for rel, expected_count in expected_assets.items():
    folder = ROOT / rel
    actual = len(list(folder.glob("*.uasset"))) if folder.exists() else 0
    if actual != expected_count:
        errors.append(f"{rel} contains {actual} assets; expected {expected_count}")

environment_assets = [
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeTerrain.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeGrassBlade.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeFairwayBlend.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeTrailBlend.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeLeafLitter.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeTrailWear.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeShoreReeds.uasset",
    "Content/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_grass_card_alpha.uasset",
    "Content/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_litter_card_alpha.uasset",
    "Content/Presentation/Course/PineRidge/Materials/M_GalleryLakeWater.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeFairway.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeForestFloor.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgePath.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_Boulder01.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_Shrub04.uasset",
    "Content/Presentation/Course/PineRidge/Materials/MI_PineRidgeSignWood.uasset",
    "Content/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_a.uasset",
    "Content/Presentation/Course/PineRidge/Fixtures/PolyHaven/Boulder01/boulder_01_1k.uasset",
    "Content/Presentation/Course/PineRidge/Fixtures/PolyHaven/Shrub04/shrub_04_1k.uasset",
]
for rel in environment_assets:
    if not (ROOT / rel).exists():
        errors.append(f"missing imported Pine Ridge environment asset: {rel}")

try:
    source_manifest = json.loads((
        ROOT / "SourceArt/PineRidge/PolyHaven/asset_manifest.json"
    ).read_text(encoding="utf-8-sig"))
    source_files = source_manifest.get("files", [])
    required_source_assets = {
        "forrest_ground_01", "leafy_grass", "grass_path_2", "fir_sapling",
        "boulder_01", "shrub_04", "weathered_planks",
    }
    if (source_manifest.get("license") != "CC0-1.0" or len(source_files) != 35
            or {entry.get("assetId") for entry in source_files} != required_source_assets):
        errors.append("Pine Ridge CC0 source manifest is incomplete")
    for entry in source_files:
        if entry.get("license") != "CC0-1.0" or not entry.get("sourceUrl", "").startswith("https://"):
            errors.append("Pine Ridge source manifest contains an unlicensed or unstable entry")
            break
        if not (ROOT / entry.get("relativePath", "")).exists():
            errors.append(f"missing Pine Ridge source file: {entry.get('relativePath')}")
except Exception as exc:
    errors.append(f"invalid Pine Ridge source manifest: {exc}")

try:
    from validate_dg_session9_brand_license import validate_repository

    brand_errors, _, _ = validate_repository(ROOT)
    errors.extend(
        f"Session 9 brand/license gate: {error}"
        for error in brand_errors
    )
except Exception as exc:
    errors.append(f"Session 9 brand/license gate could not run: {exc}")

try:
    from validate_dg_session10_environment import validate_repository as validate_session10_environment

    environment_errors, _, environment_blockers = validate_session10_environment(ROOT)
    errors.extend(
        f"Session 10 environment gate: {error}"
        for error in environment_errors
    )
    if environment_blockers != [
        "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
        "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
        "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
        "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
        "QUARANTINED_IMPORT_RECEIPTS_PENDING",
        "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
        "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
        "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
        "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
    ]:
        errors.append("Session 10 environment gate blocker continuity differs from the exact 9-blocker contract")
except Exception as exc:
    errors.append(f"Session 10 environment gate could not run: {exc}")

try:
    from validate_dg_session11_equipment_throw_lab import validate as validate_session11_equipment_throw_lab

    session11_report = validate_session11_equipment_throw_lab(
        ROOT, ROOT / "Config/DG_Session11EquipmentThrowLabContract.json")
    errors.extend(
        f"Session 11 equipment/Throw Lab gate: {error}"
        for error in session11_report.get("technicalErrors", [])
    )
    if session11_report.get("releaseBlockers") != [
        "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
        "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
        "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
        "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
        "QUARANTINED_IMPORT_RECEIPTS_PENDING",
        "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
        "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
        "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
        "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
        "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING",
    ]:
        errors.append("Session 11 equipment/Throw Lab blocker continuity differs from the exact 10-blocker contract")
except Exception as exc:
    errors.append(f"Session 11 equipment/Throw Lab gate could not run: {exc}")

try:
    from validate_dg_session12_presentation import audit as validate_session12_presentation

    session12_report = validate_session12_presentation(
        ROOT, Path("Config/DG_Session12PresentationContract.json"))
    errors.extend(
        f"Session 12 presentation gate: {error}"
        for error in session12_report.get("issues", [])
    )
    expected_session12_blockers = [
        "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
        "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
        "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
        "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
        "QUARANTINED_IMPORT_RECEIPTS_PENDING",
        "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
        "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
        "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
        "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
        "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING",
        "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING",
    ]
    if session12_report.get("releaseBlockers") != expected_session12_blockers:
        errors.append("Session 12 presentation blocker continuity differs from the exact 11-blocker contract")
    if not session12_report.get("technicalPass"):
        errors.append("Session 12 presentation technical gate did not pass")
    if session12_report.get("releaseReady") is not False:
        errors.append("Session 12 presentation gate must remain release-not-ready")
    if session12_report.get("releaseBlocked") is not True:
        errors.append("Session 12 presentation gate must remain release-blocked")
    if session12_report.get("pendingCapabilities") != []:
        errors.append("Session 12 presentation gate still reports pending capabilities")
except Exception as exc:
    errors.append(f"Session 12 presentation gate could not run: {exc}")

try:
    from validate_dg_session13_course_authoring_pcg import audit as validate_session13_course_authoring_pcg

    session13_report = validate_session13_course_authoring_pcg(
        ROOT, Path("Config/DG_Session13CourseAuthoringPcgContract.json"))
    errors.extend(
        f"Session 13 course-authoring/PCG gate: {error}"
        for error in session13_report.get("issues", [])
    )
    expected_session13_blockers = [
        "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
        "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
        "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
        "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
        "QUARANTINED_IMPORT_RECEIPTS_PENDING",
        "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
        "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
        "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
        "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
        "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING",
        "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING",
        "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING",
    ]
    if session13_report.get("releaseBlockers") != expected_session13_blockers:
        errors.append("Session 13 course-authoring/PCG blocker continuity differs from the exact 12-blocker contract")
    if not session13_report.get("technicalPass"):
        errors.append("Session 13 course-authoring/PCG technical gate did not pass")
    if session13_report.get("releaseReady") is not False:
        errors.append("Session 13 course-authoring/PCG gate must remain release-not-ready")
    if session13_report.get("releaseBlocked") is not True:
        errors.append("Session 13 course-authoring/PCG gate must remain release-blocked")
    if session13_report.get("pendingCapabilities") != []:
        errors.append("Session 13 course-authoring/PCG gate still reports pending capabilities")
except Exception as exc:
    errors.append(f"Session 13 course-authoring/PCG gate could not run: {exc}")

try:
    from validate_dg_session14_career_ai import audit as validate_session14_career_ai

    session14_report = validate_session14_career_ai(
        ROOT, Path("Config/DG_Session14CareerAiTestingContract.json"))
    session14_issues = session14_report.get("issues", [])
    expected_session14_relocation_issues = {
        f"required file is missing: {path}"
        for path in SESSION19_CAREER_AI_RELOCATIONS
    }
    session14_only_relocation_issues = (
        len(session14_issues) == len(expected_session14_relocation_issues)
        and set(session14_issues) == expected_session14_relocation_issues
    )
    errors.extend(
        f"Session 14 career/AI/testing gate: {error}"
        for error in session14_issues
    )
    expected_session14_blockers = [
        "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
        "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
        "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
        "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
        "QUARANTINED_IMPORT_RECEIPTS_PENDING",
        "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
        "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
        "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
        "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
        "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING",
        "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING",
        "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING",
        "SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING",
    ]
    if session14_report.get("releaseBlockers") != expected_session14_blockers:
        errors.append("Session 14 career/AI/testing blocker continuity differs from the exact 13-blocker contract")
    if not session14_report.get("technicalPass"):
        errors.append("Session 14 career/AI/testing technical gate did not pass")
    if session14_report.get("releaseReady") is not False:
        errors.append("Session 14 career/AI/testing gate must remain release-not-ready")
    if session14_report.get("releaseBlocked") is not True:
        errors.append("Session 14 career/AI/testing gate must remain release-blocked")
    if session14_report.get("pendingCapabilities") != []:
        errors.append("Session 14 career/AI/testing gate still reports pending capabilities")
except Exception as exc:
    errors.append(f"Session 14 career/AI/testing gate could not run: {exc}")

try:
    from validate_dg_session15_vertical_slice import audit as validate_session15_vertical_slice

    session15_report = validate_session15_vertical_slice(
        ROOT, Path("Config/DG_Session15VerticalSliceContract.json"))
    session15_issues = session15_report.get("issues", [])
    expected_session15_relocation_issues = {
        f"completed implementation file is missing: {path}"
        for path in SESSION19_VERTICAL_SLICE_RELOCATIONS
    }
    session15_only_relocation_issues = (
        len(session15_issues) == len(expected_session15_relocation_issues)
        and set(session15_issues) == expected_session15_relocation_issues
    )
    errors.extend(
        f"Session 15 vertical-slice gate: {error}"
        for error in session15_issues
    )
    expected_session15_blockers = [
        "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
        "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
        "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
        "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
        "QUARANTINED_IMPORT_RECEIPTS_PENDING",
        "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
        "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
        "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
        "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
        "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING",
        "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING",
        "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING",
        "SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING",
        "SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING",
    ]
    if session15_report.get("releaseBlockers") != expected_session15_blockers:
        errors.append("Session 15 vertical-slice blocker continuity differs from the exact 14-blocker contract")
    if session15_report.get("contractValid") is not True:
        errors.append("Session 15 vertical-slice contract is invalid")
    technical_pass = session15_report.get("technicalPass") is True
    technical_incomplete = session15_report.get("technicalIncomplete") is True
    if technical_pass == technical_incomplete:
        errors.append("Session 15 gate must be exactly technical-pass or valid technical-incomplete")
    if technical_pass and session15_report.get("pendingCapabilities") != []:
        errors.append("Session 15 technical-pass gate still reports pending capabilities")
    if technical_incomplete and (
            session15_report.get("evidenceState") != "PENDING"
            or len(session15_report.get("pendingCapabilities", [])) != 16):
        errors.append("Session 15 pre-evidence gate is not the exact 16-capability technical-incomplete state")
    if session15_report.get("releaseReady") is not False:
        errors.append("Session 15 vertical-slice gate must remain release-not-ready")
    if session15_report.get("releaseBlocked") is not True:
        errors.append("Session 15 vertical-slice gate must remain release-blocked")
except Exception as exc:
    errors.append(f"Session 15 vertical-slice gate could not run: {exc}")

try:
    from validate_dg_session16_core_playability import validate_contract as validate_session16_core_playability

    session16_contract = json.loads(
        (ROOT / "Config/DG_Session16CorePlayabilityContract.json").read_text(
            encoding="utf-8"
        )
    )
    session16_errors = validate_session16_core_playability(session16_contract, ROOT)
    errors.extend(
        f"Session 16 core-playability gate: {error}"
        for error in session16_errors
    )
    if len(session16_contract.get("prepared_checks", [])) != 44:
        errors.append("Session 16 core-playability gate lost its exact 44-check catalog")
    inherited_session15_blockers = json.loads(
        (ROOT / "Config/DG_Session15VerticalSliceContract.json").read_text(
            encoding="utf-8"
        )
    ).get("release_blockers")
    if session16_contract.get("release_blockers") != inherited_session15_blockers:
        errors.append("Session 16 core-playability release continuity differs from the inherited exact 14 blockers")
    if session16_contract.get("policy", {}).get("release_ready") is not False:
        errors.append("Session 16 core-playability gate must remain release-not-ready")
except Exception as exc:
    errors.append(f"Session 16 core-playability gate could not run: {exc}")

try:
    from validate_dg_session18_poly_haven_provenance import (
        load_json as load_session18_json,
        validate_project as validate_session18_poly_haven_provenance,
    )

    session18_contract = load_session18_json(
        ROOT / "Config/DG_Session18PolyHavenProvenanceContract.json"
    )
    session18_receipt = load_session18_json(
        ROOT / "SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json"
    )
    session18_errors, session18_summary = validate_session18_poly_haven_provenance(
        session18_contract, session18_receipt
    )
    errors.extend(
        f"Session 18 Poly Haven provenance gate: {error}"
        for error in session18_errors
    )
    if session18_summary.get("source_file_count") != 35:
        errors.append("Session 18 provenance gate lost its 35-file source closure")
    if session18_summary.get("derived_runtime_artifact_count") != 55:
        errors.append("Session 18 provenance gate lost its 55-artifact derived closure")
    if session18_contract.get("remainingReleaseBlockers") is None or len(
            session18_contract["remainingReleaseBlockers"]) != 13:
        errors.append("Session 18 provenance gate must retain exactly 13 release blockers")
except Exception as exc:
    errors.append(f"Session 18 Poly Haven provenance gate could not run: {exc}")

try:
    from validate_dg_physics_measured_reference import (
        load_json as load_measured_reference_json,
        validate as validate_measured_reference,
    )

    measured_reference_policy = load_measured_reference_json(
        ROOT / "Config/DG_PhysicsMeasuredReferencePolicy.json"
    )
    measured_reference_template = load_measured_reference_json(
        ROOT / "Evidence/Session19/PhysicsMeasuredReferenceDataset.template.json"
    )
    measured_reference_errors = validate_measured_reference(
        measured_reference_policy, measured_reference_template, False
    )
    errors.extend(
        f"Measured-reference capture protocol: {error}"
        for error in measured_reference_errors
    )
    if not validate_measured_reference(
            measured_reference_policy, measured_reference_template, True):
        errors.append(
            "Measured-reference capture template must fail closed when complete evidence is required"
        )
except Exception as exc:
    errors.append(f"Measured-reference capture protocol could not run: {exc}")

try:
    from validate_dg_session19_baked_pcg import audit as validate_session19_baked_pcg

    session19_baked_pcg_errors = validate_session19_baked_pcg()
    errors.extend(
        f"Session 19 baked-PCG source authority: {error}"
        for error in session19_baked_pcg_errors
    )
except Exception as exc:
    errors.append(f"Session 19 baked-PCG source-authority gate could not run: {exc}")

try:
    from validate_dg_session19_release_scope import (
        load_json_strict as load_session19_json,
        validate_contract as validate_session19_release_scope,
    )

    session19_contract_bytes = selected_release_scope_path.read_bytes()
    session19_contract = load_session19_json(selected_release_scope_path)
    selected_release_scope_binding = {
        "path": selected_release_scope_token,
        "bytes": len(session19_contract_bytes),
        "sha256": hashlib.sha256(session19_contract_bytes).hexdigest().upper(),
    }
    session19_errors, session19_summary = validate_session19_release_scope(
        session19_contract,
        ROOT,
        check_files=True,
        authority_contract=session19_contract,
        contract_binding=selected_release_scope_binding,
    )
    selected_release_scope_candidate_id = session19_summary.get("candidateId")
    declared_release_blockers = session19_contract.get("remainingReleaseBlockers")
    if (
            isinstance(declared_release_blockers, list)
            and all(isinstance(value, str) for value in declared_release_blockers)
    ):
        selected_release_scope_release_blockers = list(declared_release_blockers)
    if args.scope == "v05-shipping":
        historical_contract = load_session19_json(
            ROOT / HISTORICAL_RELEASE_SCOPE_CONTRACT
        )
        historical_candidate_id = historical_contract.get(
            "currentShippingCandidateEvidence", {}
        ).get("candidateId")
        session19_errors.extend(candidate_scope_identity_issues(
            session19_contract,
            selected_release_scope_token,
            historical_candidate_id,
        ))
    errors.extend(
        f"Session 19 release-scope authority: {error}"
        for error in session19_errors
    )
    if session19_summary.get("remainingBlockerCount") != 13:
        errors.append("Session 19 release-scope authority must retain all 13 blockers")
    if session19_summary.get("releaseReady") is not False:
        errors.append("Session 19 release-scope authority must remain release-not-ready")
    session19_supersedes_internal_fab_quarantine = (
        not session19_errors
        and session19_contract.get("unusedFabExternalQuarantine", {}).get(
            "currentState"
        ) == (
            "EXTERNAL_QUARANTINE_AND_CURRENT_CANDIDATE_SHIPPING_ABSENCE_"
            "TECHNICAL_PASS_PROVENANCE_AND_OWNER_CLOSURE_PENDING"
        )
        and session19_contract.get("unusedFabExternalQuarantine", {}).get(
            "externalQuarantineEvidenceAccepted"
        ) is True
        and session19_contract.get("unusedFabExternalQuarantine", {}).get(
            "blockerClosed"
        ) is False
    )
    session19_shipping_policy = load_session19_json(
        ROOT / "Config/DG_Session19ShippingContentPolicy.json"
    )
    session19_feature_scope = session19_contract.get("featureScope", {})
    session19_feature_exclusions = session19_shipping_policy.get(
        "featureExclusions", {}
    )
    session19_compile_definitions = session19_shipping_policy.get(
        "compileDefinitions", {}
    )
    session19_framework_quarantine = session19_contract.get(
        "characterFrameworkExternalQuarantine", {}
    )
    session19_framework_external_quarantine_valid = (
        not session19_errors
        and session19_framework_quarantine.get("runId")
        == "S19_Framework_20260825T031617Z_c68ee2e4e6be"
        and session19_framework_quarantine.get("projectRoots") == [
            "Plugins/DiscGolfCharacterFramework",
            "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Plugins/DiscGolfCharacterFramework",
        ]
        and session19_framework_quarantine.get("mustBeOutsideProjectRoot") is True
        and session19_framework_quarantine.get("hostPathsRecorded") is False
        and session19_framework_quarantine.get("projectSourceRemovalAccepted") is True
        and session19_framework_quarantine.get("shippingPackageAbsenceAccepted") is True
        and session19_framework_quarantine.get("replacementRuntimeAcceptanceAccepted") is False
        and session19_framework_quarantine.get("blockerClosed") is False
    )
    session19_supersedes_developer_relocation_paths = (
        not session19_errors
        and session19_developer_relocation_valid
        and session19_feature_scope.get("career")
        == "EXCLUDED_FROM_V05_SHIPPING_TECHNICAL_EVIDENCE_PASS_PUBLIC_CLAIM_ACCEPTANCE_PENDING"
        and session19_feature_scope.get("aiOpponents")
        == "EXCLUDED_FROM_V05_SHIPPING_TECHNICAL_EVIDENCE_PASS_PUBLIC_CLAIM_ACCEPTANCE_PENDING"
        and session19_feature_scope.get("throwLab")
        == "DEVELOPMENT_ONLY_SHIPPING_TECHNICAL_EXCLUSION_PASS_PUBLIC_CLAIM_ACCEPTANCE_PENDING"
        and session19_feature_exclusions.get("career", {}).get(
            "shippingEnabled"
        ) is False
        and session19_feature_exclusions.get("aiOpponents", {}).get(
            "shippingEnabled"
        ) is False
        and session19_feature_exclusions.get("throwLab", {}).get(
            "shippingEnabled"
        ) is False
        and session19_compile_definitions.get("DG_WITH_CAREER_AI") == 0
        and session19_compile_definitions.get("DG_WITH_THROW_LAB") == 0
    )
    session19_supersedes_framework_dependency_expectation = (
        session19_framework_external_quarantine_valid
        and session19_foundation_runtime_migration_valid
        and session19_feature_scope.get("characterFramework")
        == (
            "CLEANROOM_SOURCE_RETAINED_ASSET_MIGRATION_AND_SHIPPING_ABSENCE_"
            "TECHNICAL_PASS_RIGHTS_AND_RUNTIME_ACCEPTANCE_PENDING"
        )
    )
    default_game_text = (ROOT / "Config/DefaultGame.ini").read_text(
        encoding="utf-8-sig"
    )
    session19_never_cook_roots = re.findall(
        r'^\+DirectoriesToNeverCook=\(Path="([^"]+)"\)\s*$',
        default_game_text,
        re.MULTILINE,
    )
    session19_supersedes_runtime_cook_manifest_always_cook = (
        not session19_errors
        and session19_never_cook_roots.count("/Game/DiscGolf/Cook") == 1
        and 'PrimaryAssetType="DGRuntimeCookManifest"' not in default_game_text
        and "/Game/DiscGolf/Cook" in session19_shipping_policy.get(
            "neverCookRoots", []
        )
        and "/DiscGolf/Cook/DA_DG_RuntimeCookManifest"
        in session19_shipping_policy.get("forbiddenShippingPathTokens", [])
    )
    expected_session14_framework_quarantine_issues = {
        "required file is missing: "
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
        "DiscGolfCompetitionTypes.h",
        "required file is missing: "
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
        "DiscGolfTournamentDefinition.h",
        "required file is missing: "
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
        "DiscGolfAIGolferProfile.h",
        "required file is missing: "
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
        "DiscGolfAIShotPlannerComponent.h",
    }
    expected_session14_superseded_issues = {
        f"required file is missing: {path}"
        for path in SESSION19_CAREER_AI_RELOCATIONS
    } | expected_session14_framework_quarantine_issues
    session14_only_superseded_issues = (
        session19_framework_external_quarantine_valid
        and len(session14_issues) == len(expected_session14_superseded_issues)
        and set(session14_issues) == expected_session14_superseded_issues
    )
    game_mode_header = (
        ROOT / "Source/DiscGolfTour/DiscGolfTourGameMode.h"
    ).read_text(encoding="utf-8")
    hud_source = (
        ROOT / "Source/DiscGolfTour/DiscGolfHUD.cpp"
    ).read_text(encoding="utf-8")
    removed_throwlab_entrypoints = (
        "DGT_ToggleThrowLab",
        "DGT_ThrowLabPrevious",
        "DGT_ThrowLabNext",
        "DGT_ThrowLabPin",
        "DGT_ThrowLabDelete",
        "DGT_ThrowLabCompareLatest",
        "DGT_ThrowLabReplay",
        "DGT_ThrowLabSave",
        "DGT_ThrowLabLoad",
        "GetThrowLabRecordCount",
        "GetThrowLabComparisonText",
    )
    session19_supersedes_core_throwlab_entrypoints = (
        session19_supersedes_developer_relocation_paths
        and all(token not in game_mode_header for token in removed_throwlab_entrypoints)
        and "THROW LAB // DEVELOPMENT" not in hud_source
        and "GetThrowLabStatusText" not in hud_source
        and "GetThrowLabComparisonText" not in hud_source
        and "DGT_ThrowLab" not in hud_source
    )
except Exception as exc:
    errors.append(f"Session 19 release-scope authority could not run: {exc}")

if session19_supersedes_internal_fab_quarantine:
    superseded_historical_error_fragments = (
        "Session 9 brand/license gate: DefaultGame never-cook roots must be exactly ordered",
        "Session 9 brand/license gate: fab_project_nature_spruce_forest directory is missing",
        "Session 9 brand/license gate: fab_project_nature_spruce_forest inventory differs: files=0 bytes=0",
        "Session 9 brand/license gate: fab_greenbuggames_stump_scanned directory is missing",
        "Session 9 brand/license gate: fab_greenbuggames_stump_scanned inventory differs: files=0 bytes=0",
        "Session 9 brand/license gate: fab_tharlevfx_water_materials directory is missing",
        "Session 9 brand/license gate: fab_tharlevfx_water_materials inventory differs: files=0 bytes=0",
        "Session 10 environment gate: quarantine_exact_never_cook_roots:",
    )
    errors, newly_excluded = partition_release_excluded_findings(
        errors,
        lambda error: any(
            error.startswith(fragment)
            for fragment in superseded_historical_error_fragments
        ),
        authority_valid=not session19_errors,
        scope_decision="V05_UNUSED_FAB_IMPORTS_EXTERNALLY_QUARANTINED",
    )
    release_excluded_roadmap_findings.extend(newly_excluded)

if session19_supersedes_framework_dependency_expectation:
    # Frozen validators retain exact legacy-plugin source/dependency
    # expectations. Session 19 may supersede only these named results after its
    # bound 513-file receipt/manifest pass and both live roots are absent, while
    # the runtime module is independently proven to use the foundation.
    superseded_framework_errors = {
        "Session 9 brand/license gate: generic framework defaults are missing or altered",
        "Session 9 brand/license gate: scan root is missing: Plugins/DiscGolfCharacterFramework/Source",
        "Session 9 brand/license gate: runtime reference root "
        "Plugins/DiscGolfCharacterFramework/Source directory is missing: "
        "Plugins\\DiscGolfCharacterFramework\\Source",
        "Session 9 brand/license gate: DiscGolfCharacterFramework directory is missing: "
        "Plugins\\DiscGolfCharacterFramework",
        "Session 9 brand/license gate: framework content inventory differs after "
        "excluding Binaries and Intermediate: files=0 expected=96",
        "Session 10 environment gate: runtime_integration_public_module_dependencies: "
        "runtime module publicly depends on framework, Engine, and PCG",
    }
    superseded_framework_errors.update(
        "Session 14 career/AI/testing gate: required file is missing: " + path
        for path in (
            "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfCompetitionTypes.h",
            "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfTournamentDefinition.h",
            "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfAIGolferProfile.h",
            "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfAIShotPlannerComponent.h",
        )
    )
    if session14_only_superseded_issues:
        superseded_framework_errors.add(
            "Session 14 career/AI/testing technical gate did not pass"
        )
    errors, newly_excluded = partition_release_excluded_findings(
        errors,
        lambda error: error in superseded_framework_errors,
        authority_valid=not session19_errors,
        scope_decision="V05_LEGACY_CHARACTER_FRAMEWORK_EXTERNALLY_QUARANTINED",
    )
    release_excluded_roadmap_findings.extend(newly_excluded)

if session19_supersedes_runtime_cook_manifest_always_cook:
    # The retired development cook-manifest asset is exactly NeverCook and
    # forbidden by the bound Shipping policy. Preserve frozen Session 8
    # contracts while suppressing only their obsolete aggregate request to
    # restore the conflicting AlwaysCook scan.
    superseded_runtime_cook_errors = {
        "Session 8 availability gate: Asset Manager cook config tokens missing: "
        "['PrimaryAssetType=\"DGRuntimeCookManifest\"', "
        "'AssetBaseClass=\"/Script/DiscGolfTour.DiscGolfRuntimeCookManifest\"', "
        "'AssetBaseClass=\"/Script/DiscGolfCharacterFramework.DiscGolfAvatarBackendProfile\"']",
    }
    errors, newly_excluded = partition_release_excluded_findings(
        errors,
        lambda error: error in superseded_runtime_cook_errors,
        authority_valid=not session19_errors,
        scope_decision="V05_DEVELOPMENT_COOK_MANIFEST_SHIPPING_EXCLUDED",
    )
    release_excluded_roadmap_findings.extend(newly_excluded)

if session19_supersedes_developer_relocation_paths:
    # Frozen Sessions 11 and 14-16 retain their original project-core paths.
    # Suppress only the exact stale path results after every approved file is
    # present in the Shipping-denied DeveloperTool module and runtime has no
    # dependency on that module.
    superseded_relocation_errors = {
        f"missing required file: {path}"
        for path in SESSION19_THROWLAB_RELOCATIONS
    }
    superseded_relocation_errors.update(
        f"Session 11 equipment/Throw Lab gate: Session 11 implementation is "
        f"partial; missing implementation artifact: {path}"
        for path in SESSION19_THROWLAB_RELOCATIONS
    )
    superseded_relocation_errors.update(
        f"Session 14 career/AI/testing gate: required file is missing: {path}"
        for path in SESSION19_CAREER_AI_RELOCATIONS
    )
    superseded_relocation_errors.update(
        f"Session 15 vertical-slice gate: completed implementation file is "
        f"missing: {path}"
        for path in SESSION19_VERTICAL_SLICE_RELOCATIONS
    )
    session16_relocated_authority_paths = (
        "Source/DiscGolfTour/DiscGolfCareerSubsystem.h",
        "Source/DiscGolfTour/DiscGolfCareerSubsystem.cpp",
        "Source/DiscGolfTour/DiscGolfCareerProgressSaveGame.h",
    )
    superseded_relocation_errors.update(
        "Session 16 core-playability gate: authority path is unsafe, missing, "
        f"or empty: '{path}'"
        for path in session16_relocated_authority_paths
    )
    if session14_only_relocation_issues:
        superseded_relocation_errors.add(
            "Session 14 career/AI/testing technical gate did not pass"
        )
    if session15_only_relocation_issues:
        superseded_relocation_errors.update({
            "Session 15 vertical-slice contract is invalid",
            "Session 15 gate must be exactly technical-pass or valid technical-incomplete",
        })
    errors, newly_excluded = partition_release_excluded_findings(
        errors,
        lambda error: error in superseded_relocation_errors,
        authority_valid=not session19_errors,
        scope_decision=(
            "V05_CAREER_AI_THROW_LAB_VERTICAL_SLICE_DEVELOPERTOOL_"
            "SHIPPING_EXCLUDED"
        ),
    )
    release_excluded_roadmap_findings.extend(newly_excluded)

if session19_supersedes_core_throwlab_entrypoints:
    # Session 11 remains immutable historical evidence. Session 19 intentionally
    # removes its project-core Throw Lab console/HUD entrypoints from the approved
    # Shipping SKU while retaining the isolated development subsystem and tests.
    superseded_throwlab_errors = {
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['DGT_ToggleThrowLab']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['DGT_ThrowLabPrevious']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['DGT_ThrowLabNext']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['DGT_ThrowLabPin']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['DGT_ThrowLabDelete']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['DGT_ThrowLabCompareLatest']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['DGT_ThrowLabReplay']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['DGT_ThrowLabSave']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['DGT_ThrowLabLoad']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['GetThrowLabRecordCount']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfTourGameMode.h: one of ['GetThrowLabComparisonText']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfHUD.cpp: one of ['THROW LAB // DEVELOPMENT']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfHUD.cpp: one of ['GetThrowLabStatusText']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfHUD.cpp: one of ['GetThrowLabComparisonText']",
        "Session 11 equipment/Throw Lab gate: Session 11 implementation token group missing in Source/DiscGolfTour/DiscGolfHUD.cpp: one of ['DGT_ThrowLab']",
    }
    errors, newly_excluded = partition_release_excluded_findings(
        errors,
        lambda error: error in superseded_throwlab_errors,
        authority_valid=not session19_errors,
        scope_decision="V05_THROW_LAB_RUNTIME_ENTRYPOINTS_SHIPPING_EXCLUDED",
    )
    release_excluded_roadmap_findings.extend(newly_excluded)

# Lightweight source hygiene that can run without Unreal installed.
for path in (ROOT / "Source").rglob("*.cpp"):
    text = path.read_text(encoding="utf-8")
    if "TODO(CRITICAL)" in text:
        errors.append(f"critical TODO left in {path.relative_to(ROOT)}")

if errors:
    if args.scope == "v05-shipping":
        print("Project validation FAILED (v0.5 Shipping technical scope)")
        if selected_release_scope_binding is not None:
            print(
                "Selected release-scope authority: "
                f"candidate={selected_release_scope_candidate_id} "
                f"contract={selected_release_scope_binding['path']} "
                f"contract_sha256={selected_release_scope_binding['sha256']}"
            )
        print("Current v0.5 technical blockers:")
    else:
        print("Project validation FAILED")
    for e in errors:
        print(" -", e)
    if args.scope == "v05-shipping" and release_excluded_roadmap_findings:
        print("Release-excluded roadmap work (non-blocking for selected v0.5 scope):")
        for entry in release_excluded_roadmap_findings:
            print(
                f" - [{entry['scopeDecision']}] {entry['finding']}"
            )
    if (
            args.scope == "v05-shipping"
            and not session19_errors
            and selected_release_scope_release_blockers
    ):
        print("Release/public-approval blockers retained by selected authority:")
        for blocker in selected_release_scope_release_blockers:
            print(" -", blocker)
    sys.exit(1)

if args.scope == "v05-shipping":
    print("Project validation OK (v0.5 Shipping technical scope; release blocked)")
    print(
        "Selected release-scope authority: "
        f"candidate={selected_release_scope_candidate_id} "
        f"contract={selected_release_scope_binding['path']} "
        f"contract_sha256={selected_release_scope_binding['sha256']}"
    )
    print(
        "Release-excluded roadmap findings: "
        f"{len(release_excluded_roadmap_findings)}"
    )
    print(
        "Release/public-approval blockers retained: "
        f"{len(selected_release_scope_release_blockers)}"
    )
else:
    print("Project validation OK")
print(f"Root: {ROOT}")
print(f"C++ files: {len(list((ROOT / 'Source').rglob('*.cpp')))}")
print(f"Headers: {len(list((ROOT / 'Source').rglob('*.h')))}")
