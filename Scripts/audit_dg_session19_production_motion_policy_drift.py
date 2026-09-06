#!/usr/bin/env python3
"""Audit immutable production-motion policy bindings against the current tree.

This command is intentionally read-only and fail-closed.  A located current-work
provenance explains why a file changed; it does not approve the change, repair a
historical policy, or create candidate evidence.  Any identity drift remains a
blocker until a fresh Shipping candidate has its own append-only policy.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import tempfile
from typing import Any

import validate_dg_session19_production_motion as production_motion


ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / "Config/DG_Session19ProductionMotionAuthoringPolicy.json"


# These classifications describe the post-candidate workstream that owns a
# changed binding.  They are not an identity allowlist: every changed byte still
# blocks reuse of the historical policy and requires a fresh candidate policy.
CURRENT_WORK_CLASSIFICATIONS: dict[str, tuple[str, str]] = {
    "motionPipelineDocument": (
        "CURRENT_PIPELINE_DOCUMENTATION_EVOLUTION",
        "The current pipeline document gained later motion, quarantine, and "
        "retarget boundaries after the historical candidate.",
    ),
    "golferPawnSource": (
        "CURRENT_RUNTIME_MOTION_INTEGRATION",
        "The current pawn source contains the production-family selection and "
        "fail-closed throw-presentation integration added after the historical candidate.",
    ),
    "golferPawnHeader": (
        "CURRENT_RUNTIME_MOTION_INTEGRATION",
        "The current pawn interface evolved with its post-candidate production-motion "
        "and avatar-backend integration.",
    ),
    "metaHumanRetargetRuntimeSource": (
        "CURRENT_RUNTIME_RETARGET_INTEGRATION",
        "The current MetaHuman retarget runtime implementation contains later "
        "correction and presentation work.",
    ),
    "dgMasterSkeleton": (
        "CURRENT_SERIALIZED_RIG_REAUTHORING",
        "The DGMaster skeleton package was reauthored during later rig/retarget work; "
        "binary identity alone cannot approve its semantics.",
    ),
    "dgMasterControlRig": (
        "CURRENT_SERIALIZED_RIG_REAUTHORING",
        "The DGMaster Control Rig package was reauthored during later motion work; "
        "binary identity alone cannot approve its semantics.",
    ),
    "playerAnimationBlueprint": (
        "CURRENT_SERIALIZED_ANIMATION_REAUTHORING",
        "The player animation Blueprint package was regenerated during later motion "
        "integration; binary identity alone cannot approve its semantics.",
    ),
    "productionMotionRuntimePaths": (
        "CURRENT_RUNTIME_MOTION_REVISION_EVOLUTION",
        "The runtime motion contract advanced beyond the historical v001 candidate "
        "through immutable later revisions.",
    ),
    "productionMotionAuthoringUtilityHeader": (
        "CURRENT_AUTHORING_TOOL_EVOLUTION",
        "The guarded authoring interface advanced with later immutable motion revisions.",
    ),
    "productionMotionAuthoringUtilitySource": (
        "CURRENT_AUTHORING_TOOL_EVOLUTION",
        "The guarded authoring implementation advanced with later immutable motion "
        "revisions and additive repair support.",
    ),
    "productionMotionAuthorWrapper": (
        "CURRENT_AUTHORING_TOOL_EVOLUTION",
        "The guarded command wrapper advanced with later immutable motion revisions.",
    ),
    "productionMotionAssetValidator": (
        "CURRENT_VALIDATION_TOOL_EVOLUTION",
        "The native-asset contract validator advanced with later immutable motion revisions.",
    ),
}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _identity(path: Path, root: Path) -> dict[str, Any]:
    return {
        "path": path.relative_to(root).as_posix(),
        "bytes": path.stat().st_size,
        "sha256": _sha256(path),
    }


def audit_policy_bindings(
    policy: Any,
    root: Path,
    classifications: dict[str, tuple[str, str]] | None = None,
) -> dict[str, Any]:
    """Return a read-only drift report; drift is never converted into a pass."""
    root = root.resolve()
    classifications = (
        CURRENT_WORK_CLASSIFICATIONS if classifications is None else classifications
    )
    issues: list[str] = []
    rows: list[dict[str, Any]] = []
    bindings = policy.get("bindings") if type(policy) is dict else None
    if type(bindings) is not dict:
        issues.append("policy.bindings must be an object")
        bindings = {}
    else:
        actual_keys = set(bindings)
        if actual_keys != production_motion.BINDING_KEYS:
            missing = sorted(production_motion.BINDING_KEYS - actual_keys)
            extra = sorted(repr(key) for key in actual_keys - production_motion.BINDING_KEYS)
            issues.append(
                "policy.bindings keys differ: "
                f"missing={missing} extra={extra}"
            )

    for key, descriptor in bindings.items():
        label = f"policy.bindings.{key}"
        if type(key) is not str:
            issues.append(f"{label} key must be a string")
            continue
        if type(descriptor) is not dict or set(descriptor) != {
            "path", "bytes", "sha256"
        }:
            issues.append(f"{label} identity descriptor differs")
            continue
        relative = descriptor.get("path")
        descriptor_bytes = descriptor.get("bytes")
        descriptor_hash = descriptor.get("sha256")
        if (
            type(relative) is not str
            or type(descriptor_bytes) is not int
            or isinstance(descriptor_bytes, bool)
            or descriptor_bytes < 1
            or type(descriptor_hash) is not str
            or production_motion.SHA_RE.fullmatch(descriptor_hash) is None
        ):
            issues.append(f"{label} identity descriptor is invalid")
            continue
        path = production_motion.safe_project_path(relative, root, label, issues)
        if path is None:
            continue
        if not path.is_file() or path.is_symlink():
            issues.append(f"{label} target is missing or irregular")
            continue
        current = _identity(path, root)
        historical = {
            "path": relative,
            "bytes": descriptor_bytes,
            "sha256": descriptor_hash,
        }
        if current == historical:
            continue
        classification = classifications.get(key)
        if classification is None:
            issues.append(f"{label} drift has no located current-work provenance")
            classification_name = "UNCLASSIFIED_DRIFT"
            rationale = "No current development workstream is bound to this change."
            provenance_located = False
        else:
            classification_name, rationale = classification
            provenance_located = True
        rows.append({
            "binding": key,
            "path": relative,
            "historicalIdentity": historical,
            "currentIdentity": current,
            "classification": classification_name,
            "currentWorkProvenanceLocated": provenance_located,
            "semanticApprovalInferred": False,
            "historicalPolicyMutationAllowed": False,
            "freshCandidatePolicyRequired": True,
            "rationale": rationale,
        })

    if issues:
        state = "INVALID_AUDIT_INPUT_OR_UNCLASSIFIED_DRIFT"
    elif rows:
        state = "BLOCKED_HISTORICAL_POLICY_IDENTITY_DRIFT"
    else:
        state = "PASS_NO_BINDING_IDENTITY_DRIFT"
    return {
        "schema": "DiscGolfTour.Session19ProductionMotionPolicyDriftAudit.v1",
        "schemaVersion": 1,
        "state": state,
        "historicalPolicyMustRemainUnchanged": True,
        "freshCandidateContentReceiptRequiredBeforeRebind": True,
        "candidatePolicyGenerationCreatesReleaseEvidence": False,
        "driftCount": len(rows),
        "classifiedCurrentWorkCount": sum(
            1 for row in rows if row["currentWorkProvenanceLocated"]
        ),
        "unclassifiedDriftCount": sum(
            1 for row in rows if not row["currentWorkProvenanceLocated"]
        ),
        "drift": rows,
        "issues": issues,
        "claimBoundary": {
            "currentChangesSemanticallyApproved": False,
            "historicalCandidateRevalidated": False,
            "freshShippingCandidateExists": False,
            "releaseReady": False,
        },
    }


def run_self_test() -> tuple[bool, int, list[str]]:
    failures: list[str] = []
    cases = 0
    with tempfile.TemporaryDirectory(prefix="dgt-motion-policy-drift-") as name:
        root = Path(name).resolve()
        bindings: dict[str, dict[str, Any]] = {}
        for key in sorted(production_motion.BINDING_KEYS):
            target = root / "Bindings" / f"{key}.bin"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(f"historical-{key}".encode("utf-8"))
            bindings[key] = _identity(target, root)
        policy = {"bindings": bindings}
        target = root / bindings["motionPipelineDocument"]["path"]

        report = audit_policy_bindings(policy, root)
        cases += 1
        if report["state"] != "PASS_NO_BINDING_IDENTITY_DRIFT":
            failures.append("identical binding did not pass")

        target.write_bytes(b"current development")
        report = audit_policy_bindings(policy, root)
        cases += 1
        if (
            report["state"] != "BLOCKED_HISTORICAL_POLICY_IDENTITY_DRIFT"
            or report["driftCount"] != 1
            or not report["drift"][0]["currentWorkProvenanceLocated"]
            or report["drift"][0]["semanticApprovalInferred"]
            or report["drift"][0]["historicalPolicyMutationAllowed"]
            or not report["drift"][0]["freshCandidatePolicyRequired"]
        ):
            failures.append("classified drift was not retained as a blocking rebind")

        report = audit_policy_bindings(policy, root, classifications={})
        cases += 1
        if (
            report["state"] != "INVALID_AUDIT_INPUT_OR_UNCLASSIFIED_DRIFT"
            or report["unclassifiedDriftCount"] != 1
            or not report["issues"]
        ):
            failures.append("unclassified drift did not fail closed")

        incomplete = {"bindings": {}}
        report = audit_policy_bindings(incomplete, root)
        cases += 1
        if (
            report["state"] != "INVALID_AUDIT_INPUT_OR_UNCLASSIFIED_DRIFT"
            or not any("keys differ" in issue for issue in report["issues"])
        ):
            failures.append("incomplete binding set survived")

        extra_bindings = dict(bindings)
        extra_bindings["unexpectedBinding"] = bindings["motionPipelineDocument"]
        report = audit_policy_bindings({"bindings": extra_bindings}, root)
        cases += 1
        if (
            report["state"] != "INVALID_AUDIT_INPUT_OR_UNCLASSIFIED_DRIFT"
            or not any("keys differ" in issue for issue in report["issues"])
        ):
            failures.append("extra binding survived")

        malformed_bindings = dict(bindings)
        malformed_bindings["motionPipelineDocument"] = {
            "path": bindings["motionPipelineDocument"]["path"]
        }
        malformed = {"bindings": malformed_bindings}
        report = audit_policy_bindings(malformed, root)
        cases += 1
        if not report["issues"]:
            failures.append("malformed identity descriptor survived")

        zero_byte_bindings = dict(bindings)
        zero_byte_bindings["motionPipelineDocument"] = {
            "path": bindings["motionPipelineDocument"]["path"],
            "bytes": 0,
            "sha256": "0" * 64,
        }
        report = audit_policy_bindings({"bindings": zero_byte_bindings}, root)
        cases += 1
        if (
            report["state"] != "INVALID_AUDIT_INPUT_OR_UNCLASSIFIED_DRIFT"
            or not report["issues"]
        ):
            failures.append("non-positive historical byte count survived")

        traversal_bindings = dict(bindings)
        traversal_bindings["motionPipelineDocument"] = {
            "path": "../outside.md", "bytes": 1, "sha256": "0" * 64,
        }
        traversal = {
            "bindings": traversal_bindings,
        }
        report = audit_policy_bindings(traversal, root)
        cases += 1
        if not report["issues"]:
            failures.append("project-root traversal survived")

        missing_bindings = dict(bindings)
        missing_bindings["motionPipelineDocument"] = {
            "path": "Docs/missing.md", "bytes": 1, "sha256": "0" * 64,
        }
        missing = {
            "bindings": missing_bindings,
        }
        report = audit_policy_bindings(missing, root)
        cases += 1
        if not report["issues"]:
            failures.append("missing target survived")

        report = audit_policy_bindings([], root)
        cases += 1
        if not report["issues"]:
            failures.append("non-object policy survived")

    return not failures, cases, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--policy", type=Path, default=POLICY)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        passed, cases, failures = run_self_test()
        if not passed:
            print("SESSION 19 PRODUCTION MOTION POLICY DRIFT SELF-TEST FAILED")
            for failure in failures:
                print(f" - {failure}")
            return 1
        print(f"SESSION 19 PRODUCTION MOTION POLICY DRIFT SELF-TEST PASS cases={cases}")
        return 0

    root = args.root.resolve()
    policy_path = args.policy if args.policy.is_absolute() else root / args.policy
    policy_path = policy_path.resolve(strict=False)
    try:
        policy_path.relative_to(root)
    except ValueError:
        print(json.dumps({
            "state": "INVALID_AUDIT_INPUT_OR_UNCLASSIFIED_DRIFT",
            "issues": ["selected policy must be inside --root"],
        }, indent=2))
        return 2
    if not policy_path.is_file() or policy_path.is_symlink():
        print(json.dumps({
            "state": "INVALID_AUDIT_INPUT_OR_UNCLASSIFIED_DRIFT",
            "issues": ["selected policy is missing or irregular"],
        }, indent=2))
        return 2
    try:
        policy = production_motion.load_json(policy_path)
    except production_motion.StrictJsonError as exc:
        print(json.dumps({
            "state": "INVALID_AUDIT_INPUT_OR_UNCLASSIFIED_DRIFT",
            "issues": [f"selected policy is invalid: {exc}"],
        }, indent=2))
        return 2
    report = audit_policy_bindings(policy, root)
    report["historicalPolicy"] = _identity(policy_path, root)
    print(json.dumps(report, indent=2))
    return 0 if report["state"] == "PASS_NO_BINDING_IDENTITY_DRIFT" else 1


if __name__ == "__main__":
    raise SystemExit(main())
