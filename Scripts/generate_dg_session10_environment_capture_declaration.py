#!/usr/bin/env python3
"""Predeclare a guarded external Session 10 machine environment recapture.

The declaration is optional because the selected candidate already has a valid
three-hole rendered-performance run.  It exists so a future recapture can bind
the exact candidate, baseline receipt, command contract, and claim boundary
before execution.  This program never launches the game.  ``--preflight-only``
performs no writes; ``--emit-declaration`` writes one new external JSON file with
exclusive-create semantics and cannot overwrite prior evidence.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
from typing import Any, Optional, Sequence
import uuid

import audit_dg_session10_environment_capture_readiness as readiness
import validate_dg_session19_shipping_performance as performance


ROOT = Path(__file__).resolve().parents[1]
DECLARATION_SCHEMA = "DiscGolfTour.Session10EnvironmentMachineCaptureDeclaration.v1"
DECLARATION_STATE = "PREDECLARED_EXTERNAL_SESSION10_MACHINE_CAPTURE_NOT_STARTED"
NAMESPACE = Path("Session10EnvironmentCapture")


class DeclarationError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def is_same_or_under(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except (OSError, RuntimeError, ValueError):
        return False


def is_reparse(path: Path) -> bool:
    if path.is_symlink():
        return True
    try:
        attributes = getattr(path.lstat(), "st_file_attributes", 0)
    except OSError:
        return False
    return bool(attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def existing_chain_has_reparse(path: Path, stop: Path) -> bool:
    """Reject a reparse entry below an already accepted external root."""
    try:
        relative = path.absolute().relative_to(stop.absolute())
    except ValueError:
        return True
    cursor = stop.absolute()
    for part in relative.parts:
        cursor /= part
        if os.path.lexists(cursor) and is_reparse(cursor):
            return True
    return False


def validate_external_root(external_root: Path, archive: Path) -> Path:
    try:
        resolved = external_root.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise DeclarationError(
            f"external root must already exist ({exc.__class__.__name__})"
        ) from None
    if not resolved.is_dir() or is_reparse(resolved):
        raise DeclarationError("external root must be a regular non-reparse directory")
    if is_same_or_under(resolved, ROOT) or is_same_or_under(ROOT, resolved):
        raise DeclarationError("external root must be outside the project root")
    if is_same_or_under(resolved, archive) or is_same_or_under(archive, resolved):
        raise DeclarationError("external root must be outside the candidate archive")
    return resolved


def validate_output_path(
    output: Path,
    external_root: Path,
    archive: Path,
    candidate_id: str,
) -> Path:
    root = validate_external_root(external_root, archive)
    allowed = (root / NAMESPACE / "Declarations").absolute()
    selected = output.absolute()
    try:
        selected.relative_to(allowed)
    except ValueError:
        raise DeclarationError(
            "declaration output must be inside the external "
            "Session10EnvironmentCapture/Declarations directory"
        ) from None
    if selected.parent != allowed:
        raise DeclarationError("declaration output may not create nested directories")
    if selected.suffix.casefold() != ".json" \
            or not selected.name.startswith(f"EnvironmentCaptureDeclaration-{candidate_id}-"):
        raise DeclarationError("declaration output name does not match the candidate contract")
    if selected.exists() or os.path.lexists(selected):
        raise DeclarationError("refusing to overwrite an existing declaration")
    if existing_chain_has_reparse(selected.parent, root):
        raise DeclarationError("declaration output path contains a reparse point")
    if is_same_or_under(selected, ROOT) or is_same_or_under(selected, archive):
        raise DeclarationError("declaration output cannot be inside protected roots")
    return selected


def write_exclusive_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("xb") as handle:
            handle.write(
                (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")
            )
    except FileExistsError:
        raise DeclarationError("refusing to overwrite an existing declaration") from None


def repo_identity(relative: str, label: str) -> dict[str, Any]:
    path = readiness.safe_relative_path(ROOT, relative, label)
    return {"path": relative, **readiness.stable_file_identity(path, label)}


def build_declaration(
    policy_path: Path,
    policy: dict[str, Any],
    audit: dict[str, Any],
    archive: Path,
    external_root: Path,
    declaration_id: str,
    generated_utc: str,
) -> dict[str, Any]:
    candidate = policy["candidateId"]
    try:
        parsed_id = uuid.UUID(declaration_id)
    except (ValueError, AttributeError):
        raise DeclarationError("declaration ID must be a UUID") from None
    if str(parsed_id) != declaration_id or parsed_id.version != 4:
        raise DeclarationError("declaration ID must be a canonical lowercase UUIDv4")
    performance_contract = policy["shippingPerformanceEvidence"]
    declaration_contract = policy["captureDeclarationContract"]
    archive_manifest = performance.build_archive_manifest(archive, candidate)
    policy_relative = policy_path.resolve().relative_to(ROOT.resolve()).as_posix()
    policy_identity = {
        "path": policy_relative,
        **readiness.stable_file_identity(policy_path, "machine evidence policy"),
    }
    runner = repo_identity(declaration_contract["runner"], "capture runner")
    validator = repo_identity(declaration_contract["validator"], "capture validator")
    sanitized_launches: list[dict[str, Any]] = []
    for hole in performance_contract["requiredHoleNumbers"]:
        example_uuid = f"00000000-0000-4000-8000-{hole:012d}"
        sanitized_launches.append({
            "holeNumber": hole,
            "executableRelativePath": performance.EXECUTABLE_RELATIVE,
            "sanitizedArguments": performance.required_arguments(
                hole, example_uuid, True
            ),
            "userDirRule": "FRESH_EXTERNAL_UUID_GENERATED_BY_RUNNER",
        })
    bindings = policy["bindings"]
    return {
        "schema": DECLARATION_SCHEMA,
        "schemaVersion": 1,
        "session": 10,
        "milestone": "v0.5",
        "candidateId": candidate,
        "declarationId": declaration_id,
        "declaredUtc": generated_utc,
        "state": DECLARATION_STATE,
        "authority": (
            "PREDECLARATION_ONLY_NO_CAPTURE_EXECUTION_ASSET_POPULATION_VISUAL_"
            "PROVENANCE_LEGAL_OWNER_PERFORMANCE_OR_RELEASE_APPROVAL"
        ),
        "policy": policy_identity,
        "baselineBindings": {
            "session10TechnicalPolicy": bindings["session10TechnicalPolicy"],
            "session10TechnicalReceipt": bindings["session10TechnicalReceipt"],
            "categoryTechnicalPlan": bindings["categoryTechnicalPlan"],
            "shippingPerformanceReceipt": bindings["shippingPerformanceReceipt"],
        },
        "candidateArchive": {
            "recoveryLocationToken": policy["candidateArchive"][
                "archiveRecoveryLocationToken"
            ],
            "fileCount": archive_manifest["fileCount"],
            "bytes": archive_manifest["totalBytes"],
            "inventorySha256": archive_manifest["inventorySha256"],
            "launcher": archive_manifest["launcher"],
            "shippingExecutable": archive_manifest["shippingExecutable"],
            "mustRemainByteIdentical": True,
        },
        "baselineMachineEvidence": {
            "sourceRunId": performance_contract["runId"],
            "actualRenderedRhiTechnicalEvidenceAccepted": audit["machineEvidence"][
                "actualRenderedRhiTechnicalEvidenceAccepted"
            ],
            "threeHoleEnvironmentTraversalTechnicalEvidenceAccepted": audit[
                "machineEvidence"
            ]["threeHoleEnvironmentTraversalTechnicalEvidenceAccepted"],
            "boundedThreeHolePerformanceTechnicalEvidenceAccepted": audit[
                "machineEvidence"
            ]["boundedThreeHolePerformanceTechnicalEvidenceAccepted"],
            "worstP95FrameMs": audit["machineEvidence"]["worstP95FrameMs"],
            "lowestAverageFps": audit["machineEvidence"]["lowestAverageFps"],
            "totalHitchCount": audit["machineEvidence"]["totalHitchCount"],
        },
        "captureContract": {
            "runner": runner,
            "validator": validator,
            "externalRootToken": "DGTourExternal",
            "outputNamespace": "ShippingPerformance/<candidateId>/<runnerGeneratedRunId>",
            "requiredHoleNumbers": performance_contract["requiredHoleNumbers"],
            "durationSecondsPerHole": performance_contract[
                "requiredDurationSecondsPerHole"
            ],
            "freshExternalUserDirPerHole": True,
            "archivePrePostInventoryRequired": True,
            "launches": sanitized_launches,
            "captureNotStartedByThisDeclaration": True,
        },
        "categoryBaseline": {
            "populatedCategoryCount": audit["categoryCoverage"][
                "populatedCategoryCount"
            ],
            "missingCategoryCount": audit["categoryCoverage"]["missingCategoryCount"],
            "productionReadyCategoryCount": audit["categoryCoverage"][
                "productionReadyCategoryCount"
            ],
            "missingCategories": audit["categoryCoverage"]["missingCategories"],
        },
        "externalOutput": {
            "declarationNamespace": NAMESPACE.as_posix(),
            "externalRootResolvedForPreflight": str(external_root),
            "appendOnly": True,
            "protectedProjectWrites": False,
            "protectedArchiveWrites": False,
            "protectedEvidenceRunWrites": False,
        },
        "claimBoundary": {
            "declarationOnly": True,
            "captureStarted": False,
            "captureCompleted": False,
            "newTechnicalEvidenceAccepted": False,
            "allSixteenProductionSlotsBound": False,
            "allSixteenProductionSlotsReady": False,
            "visualQualityAccepted": False,
            "sustainedPerformanceCertified": False,
            "humanPerformanceAcceptance": False,
            "provenanceApproval": False,
            "legalApproval": False,
            "ownerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }


def preflight(
    policy_path: Path,
    archive: Path,
    external_root: Path,
    declaration_id: str,
    output: Optional[Path],
) -> tuple[dict[str, Any], Path]:
    policy = readiness.strict_load(policy_path, "machine evidence policy")
    shape_issues = readiness.validate_policy_shape(policy)
    if shape_issues:
        raise DeclarationError("policy failed closed: " + "; ".join(shape_issues))
    candidate = policy["candidateId"]
    archive_resolved = performance.validate_candidate_archive_path(archive, candidate)
    external_resolved = validate_external_root(external_root, archive_resolved)
    audit = readiness.audit_selected(policy_path, archive_resolved, external_resolved)
    if audit.get("passed") is not True:
        raise DeclarationError(
            "read-only baseline audit failed: " + "; ".join(audit.get("issues", []))
        )
    name = f"EnvironmentCaptureDeclaration-{candidate}-{declaration_id}.json"
    selected_output = output if output is not None else (
        external_resolved / NAMESPACE / "Declarations" / name
    )
    selected_output = validate_output_path(
        selected_output, external_resolved, archive_resolved, candidate
    )
    declaration = build_declaration(
        policy_path,
        policy,
        audit,
        archive_resolved,
        external_resolved,
        declaration_id,
        utc_now(),
    )
    return declaration, selected_output


def run_self_test() -> tuple[int, dict[str, Any]]:
    checks = 0
    failures: list[dict[str, str]] = []

    def check(name: str, condition: bool) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            failures.append({"case": name, "error": "assertion failed"})

    try:
        policy = readiness.strict_load(
            readiness.DEFAULT_POLICY_PATH, "self-test machine evidence policy"
        )
        candidate = policy["candidateId"]
        synthetic_audit = {
            "categoryCoverage": {
                "populatedCategoryCount": 3,
                "missingCategoryCount": 13,
                "productionReadyCategoryCount": 0,
                "missingCategories": policy["categoryBaseline"]["missingCategories"],
            },
            "machineEvidence": {
                "actualRenderedRhiTechnicalEvidenceAccepted": True,
                "threeHoleEnvironmentTraversalTechnicalEvidenceAccepted": True,
                "boundedThreeHolePerformanceTechnicalEvidenceAccepted": True,
                "worstP95FrameMs": 9.62,
                "lowestAverageFps": 122.9,
                "totalHitchCount": 0,
            },
        }
        with tempfile.TemporaryDirectory(prefix="dg-s10-environment-declaration-") as temp:
            base = Path(temp)
            archive_parent = base / "Packages" / candidate
            archive = archive_parent / "Windows"
            (archive / Path(performance.EXECUTABLE_RELATIVE).parent).mkdir(parents=True)
            (archive / performance.LAUNCHER_RELATIVE).write_bytes(b"launcher")
            (archive / performance.EXECUTABLE_RELATIVE).write_bytes(b"shipping")
            external = base / "External"
            external.mkdir()
            declaration_id = str(uuid.uuid4())
            declaration = build_declaration(
                readiness.DEFAULT_POLICY_PATH,
                policy,
                synthetic_audit,
                archive,
                external,
                declaration_id,
                "2099-01-01T00:00:00.000Z",
            )
            check("declaration-schema", declaration["schema"] == DECLARATION_SCHEMA)
            check("candidate-bound", declaration["candidateId"] == candidate)
            check("three-launches", len(declaration["captureContract"]["launches"]) == 3)
            check("capture-not-started", declaration["claimBoundary"]["captureStarted"] is False)
            check("visual-false", declaration["claimBoundary"]["visualQualityAccepted"] is False)
            check("legal-false", declaration["claimBoundary"]["legalApproval"] is False)
            check("owner-false", declaration["claimBoundary"]["ownerApproval"] is False)
            check("release-false", declaration["claimBoundary"]["releaseReady"] is False)
            output = external / NAMESPACE / "Declarations" / (
                f"EnvironmentCaptureDeclaration-{candidate}-{declaration_id}.json"
            )
            selected = validate_output_path(output, external, archive, candidate)
            check("external-output-selected", selected == output.absolute())
            write_exclusive_json(selected, declaration)
            check("exclusive-write-created", selected.is_file())
            try:
                write_exclusive_json(selected, declaration)
            except DeclarationError:
                overwrite_rejected = True
            else:
                overwrite_rejected = False
            check("overwrite-rejected", overwrite_rejected)
            try:
                validate_output_path(
                    ROOT / output.name, external, archive, candidate
                )
            except DeclarationError:
                project_output_rejected = True
            else:
                project_output_rejected = False
            check("project-output-rejected", project_output_rejected)
            try:
                validate_external_root(archive, archive)
            except DeclarationError:
                archive_root_rejected = True
            else:
                archive_root_rejected = False
            check("archive-output-root-rejected", archive_root_rejected)
    except (DeclarationError, readiness.AuditError, performance.EvidenceError,
            OSError, KeyError, TypeError, ValueError) as exc:
        failures.append({"case": "self-test-exception", "error": str(exc)})
    result = {
        "schema": "DiscGolfTour.Session10EnvironmentCaptureDeclarationSelfTest.v1",
        "passed": not failures,
        "assertionCount": checks,
        "failures": failures,
        "gameProcessLaunched": False,
        "longCaptureStarted": False,
        "protectedEvidenceWritten": False,
    }
    return (0 if not failures else 1), result


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, default=readiness.DEFAULT_POLICY_PATH)
    parser.add_argument("--archive", type=Path, default=readiness.DEFAULT_ARCHIVE)
    parser.add_argument("--external-root", type=Path, default=readiness.DEFAULT_EXTERNAL_ROOT)
    parser.add_argument("--declaration-id")
    parser.add_argument("--output", type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--preflight-only", action="store_true")
    mode.add_argument("--emit-declaration", action="store_true")
    mode.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        if args.declaration_id is not None or args.output is not None \
                or args.policy != readiness.DEFAULT_POLICY_PATH \
                or args.archive != readiness.DEFAULT_ARCHIVE \
                or args.external_root != readiness.DEFAULT_EXTERNAL_ROOT:
            print("--self-test accepts no path or declaration overrides", file=sys.stderr)
            return 2
        code, result = run_self_test()
        print(json.dumps(result, indent=2, sort_keys=True))
        return code
    if not args.preflight_only and not args.emit_declaration:
        print("select --preflight-only or --emit-declaration", file=sys.stderr)
        return 2
    declaration_id = args.declaration_id or str(uuid.uuid4())
    try:
        declaration, output = preflight(
            args.policy.resolve(),
            args.archive.resolve(),
            args.external_root.resolve(),
            declaration_id,
            args.output,
        )
        written = False
        if args.emit_declaration:
            write_exclusive_json(output, declaration)
            written = True
    except (DeclarationError, readiness.AuditError, performance.EvidenceError,
            OSError, KeyError, TypeError, ValueError) as exc:
        print(f"Session 10 environment capture declaration failed closed: {exc}",
              file=sys.stderr)
        return 1
    result = {
        "state": declaration["state"],
        "candidateId": declaration["candidateId"],
        "declarationId": declaration["declarationId"],
        "preflightPassed": True,
        "declarationWritten": written,
        "output": str(output),
        "gameProcessLaunched": False,
        "longCaptureStarted": False,
        "releaseReady": False,
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
