#!/usr/bin/env python3
"""Fail-closed validation for a Session 19 technical-origin proposal.

Validation regenerates the deterministic proposal from the CLI-bound source draft
and compares the complete document.  A pass is technical triage only; it cannot be
used as reviewed classification, legal clearance, shipping approval, or release
readiness.
"""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import sys
import tempfile
from typing import Any

import generate_dg_session19_technical_origin_proposal as origin


def typed_equal(actual: Any, expected: Any) -> bool:
    if type(actual) is not type(expected):
        return False
    if type(expected) is dict:
        return set(actual) == set(expected) and all(
            typed_equal(actual[key], expected[key]) for key in expected
        )
    if type(expected) is list:
        return len(actual) == len(expected) and all(
            typed_equal(left, right) for left, right in zip(actual, expected)
        )
    return actual == expected


def first_difference(actual: Any, expected: Any, path: str = "$") -> str | None:
    if type(actual) is not type(expected):
        return f"{path}: type differs ({type(actual).__name__} vs {type(expected).__name__})"
    if type(expected) is dict:
        if set(actual) != set(expected):
            missing = sorted(set(expected) - set(actual))
            extra = sorted(set(actual) - set(expected))
            return f"{path}: keys differ missing={missing} extra={extra}"
        for key in expected:
            difference = first_difference(actual[key], expected[key], f"{path}.{key}")
            if difference is not None:
                return difference
        return None
    if type(expected) is list:
        if len(actual) != len(expected):
            return f"{path}: list length differs ({len(actual)} vs {len(expected)})"
        for index, (left, right) in enumerate(zip(actual, expected)):
            difference = first_difference(left, right, f"{path}[{index}]")
            if difference is not None:
                return difference
        return None
    if actual != expected:
        return f"{path}: value differs ({actual!r} vs {expected!r})"
    return None


def security_problems(document: Any) -> list[str]:
    problems: list[str] = []
    if type(document) is not dict:
        return ["proposal root must be an object"]
    if set(document) != origin.PROPOSAL_ROOT_KEYS:
        problems.append("proposal root keys differ from the isolated proposal schema")
        return problems
    if document.get("schema") != origin.PROPOSAL_SCHEMA:
        problems.append("proposal schema differs")
    if document.get("state") != origin.PROPOSAL_STATE:
        problems.append("proposal state differs")
    if document.get("purpose") != origin.PROPOSAL_PURPOSE:
        problems.append("proposal purpose differs")
    for key in (
        "reviewed", "shippingApproved", "legalReviewed", "legalApproved",
        "licenseApproved", "distributionApproved", "authoritativeClassificationCompatible",
    ):
        if document.get(key) is not False:
            problems.append(f"root {key} must be false")

    records = document.get("records")
    if type(records) is not list:
        problems.append("proposal records must be an array")
        return problems
    seen: set[tuple[str, str, str]] = set()
    for index, record in enumerate(records):
        if type(record) is not dict or set(record) != origin.PROPOSAL_RECORD_KEYS:
            problems.append(f"record {index} keys or type differ")
            continue
        forbidden_authoritative_keys = {"classification", "authorityId", "licenseId"} & set(record)
        if forbidden_authoritative_keys:
            problems.append(f"record {index} contains authoritative classification fields")
        for key in (
            "reviewed", "shippingApproved", "legalReviewed", "legalApproved",
            "licenseApproved", "distributionApproved",
        ):
            if record[key] is not False:
                problems.append(f"record {index} {key} must be false")
        if record["licenseStatus"] != origin.LICENSE_STATUS:
            problems.append(f"record {index} licenseStatus differs")
        key = (
            str(record["scope"]), str(record["container"] or "").casefold(),
            str(record["identity"]).casefold(),
        )
        if key in seen:
            problems.append(f"record {index} duplicates or case-collides")
        seen.add(key)
        scope = record["scope"]
        disposition = record["proposalDisposition"]
        if scope == "UFS_ENTRY" and disposition != origin.DIAGNOSTIC_DISPOSITION:
            problems.append(f"record {index} promotes diagnostic UFS evidence")
        if (
            scope == "CONTAINER_ANONYMOUS_CHUNK"
            and disposition != origin.ANONYMOUS_DISPOSITION
        ):
            problems.append(f"record {index} classifies an anonymous chunk")
        if disposition != origin.TECHNICAL_DISPOSITION:
            if any(record[key] is not None for key in (
                "technicalOriginCategory", "ruleId", "confidence",
            )):
                problems.append(f"record {index} unresolved/diagnostic fields are not null")
        elif (
            record["technicalOriginCategory"] is None
            or record["ruleId"] not in origin.RULE_BY_ID
            or record["confidence"] is None
        ):
            problems.append(f"record {index} technical proposal fields are incomplete")

    boundary = document.get("releaseBoundary")
    if type(boundary) is not dict:
        problems.append("release boundary is malformed")
    else:
        required_false = (
            "mayBeUsedAsAuthoritativeClassification", "independentReviewComplete",
            "legalReviewComplete", "distributionClearanceComplete",
            "shippingApprovalGranted", "releaseReady", "releaseUseAllowed", "blockerClosed",
        )
        if boundary.get("nonAuthoritativeTechnicalProposalOnly") is not True:
            problems.append("non-authoritative proposal boundary must be true")
        for key in required_false:
            if boundary.get(key) is not False:
                problems.append(f"release boundary {key} must be false")
    return problems


def validate_document(document: Any, expected: dict[str, Any]) -> list[str]:
    problems = security_problems(document)
    if not typed_equal(document, expected):
        problems.append(first_difference(document, expected) or "proposal differs")
    return problems


def deterministic_encoding_problems(
    data: bytes, expected: dict[str, Any],
) -> list[str]:
    if data == origin.proposal_bytes(expected):
        return []
    return ["proposal bytes differ from the deterministic generator encoding"]


def write_test_json(path: Path, document: dict[str, Any]) -> str:
    data = origin.proposal_bytes(document)
    path.write_bytes(data)
    return origin.sha256_bytes(data)


def run_self_test() -> int:
    candidate_id = "S19_ValidatorTest"
    with tempfile.TemporaryDirectory(prefix="dgt_origin_validator_") as folder:
        draft_path = Path(folder) / "draft.json"
        digest = write_test_json(draft_path, origin.test_source_document(candidate_id))
        source, binding = origin.load_source_draft(draft_path, candidate_id, digest)
        expected = origin.build_proposal(source, binding)

        cases: list[tuple[str, Any, bool]] = [("valid", expected, True)]
        mutations = (
            lambda value: value.__setitem__("reviewed", True),
            lambda value: value.__setitem__("shippingApproved", True),
            lambda value: value.__setitem__("legalReviewed", True),
            lambda value: value.__setitem__("legalApproved", True),
            lambda value: value.__setitem__("licenseApproved", True),
            lambda value: value.__setitem__("distributionApproved", True),
            lambda value: value.__setitem__("authoritativeClassificationCompatible", True),
            lambda value: value["records"][2].__setitem__(
                "proposalDisposition", origin.TECHNICAL_DISPOSITION
            ),
            lambda value: value["records"][3].__setitem__(
                "proposalDisposition", origin.TECHNICAL_DISPOSITION
            ),
            lambda value: value["records"].pop(),
            lambda value: value["releaseBoundary"].__setitem__("releaseReady", True),
            lambda value: value.__setitem__(
                "schema", origin.SOURCE_SCHEMA
            ),
            lambda value: value["summary"].__setitem__(
                "technicalOriginProposedCount", 999
            ),
        )
        for index, mutate in enumerate(mutations):
            value = copy.deepcopy(expected)
            mutate(value)
            cases.append((f"mutation-{index}", value, False))

        passed = 0
        for name, value, should_pass in cases:
            observed_pass = not validate_document(value, expected)
            if observed_pass == should_pass:
                passed += 1
            else:
                print(f"self-test case differed: {name}")
        encoding_cases = (
            (origin.proposal_bytes(expected), True),
            (b" " + origin.proposal_bytes(expected), False),
        )
        for data, should_pass in encoding_cases:
            if (not deterministic_encoding_problems(data, expected)) == should_pass:
                passed += 1
        total = len(cases) + len(encoding_cases)
        if passed != total:
            print(f"FAIL technical-origin proposal validator self-test {passed}/{total}")
            return 1
        print(f"PASS technical-origin proposal validator self-test {passed}/{total}")
        return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--proposal", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--source-draft", type=Path)
    parser.add_argument("--source-draft-sha256")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()
    missing = [
        name for name, value in (
            ("--proposal", args.proposal),
            ("--candidate-id", args.candidate_id),
            ("--source-draft", args.source_draft),
            ("--source-draft-sha256", args.source_draft_sha256),
        ) if value is None
    ]
    if missing:
        parser.error("required arguments: " + ", ".join(missing))

    try:
        source, binding = origin.load_source_draft(
            args.source_draft.resolve(), args.candidate_id, args.source_draft_sha256
        )
        expected = origin.build_proposal(source, binding)
        proposal_path = args.proposal.resolve()
        document = origin.load_json_strict(proposal_path)
        problems = validate_document(document, expected)
        data = proposal_path.read_bytes()
        problems.extend(deterministic_encoding_problems(data, expected))
    except (origin.ProposalError, OSError) as exc:
        print(f"FAIL_NON_AUTHORITATIVE_TECHNICAL_ORIGIN_PROPOSAL: {exc}", file=sys.stderr)
        return 1
    if problems:
        for problem in problems[:20]:
            print(f"FAIL_NON_AUTHORITATIVE_TECHNICAL_ORIGIN_PROPOSAL: {problem}", file=sys.stderr)
        if len(problems) > 20:
            print(
                f"FAIL_NON_AUTHORITATIVE_TECHNICAL_ORIGIN_PROPOSAL: "
                f"{len(problems) - 20} additional problem(s)", file=sys.stderr,
            )
        return 1

    summary = document["summary"]
    print(
        "PASS_NON_AUTHORITATIVE_TECHNICAL_ORIGIN_PROPOSAL_VALIDATED "
        f"candidate={document['candidateId']} records={summary['sourceRecordCount']} "
        f"proposed={summary['technicalOriginProposedCount']} "
        f"anonymous_unresolved={summary['unresolvedAnonymousChunkCount']} "
        f"ufs_diagnostic={summary['diagnosticNonAuthoritativeRecordCount']} "
        f"proposal_bytes={len(data)} proposal_sha256={origin.sha256_bytes(data)} "
        "reviewed=false shippingApproved=false legalApproved=false releaseReady=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
