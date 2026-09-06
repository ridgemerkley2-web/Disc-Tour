#!/usr/bin/env python3
"""Retired compatibility gate for the obsolete baked-PCG runtime closure.

PCG generation now lives exclusively in the Editor module. Shipping consumes
the deterministic 44-tree authored JSON payload, compiles no project runtime
PCG dependency or generation API, and NeverCooks the project PCG graph root.
Candidate cook proof belongs to validate_dg_session19_shipping_pcg_separation;
this gate validates source/prebuild facts only and never emits a candidate
receipt.
"""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import sys
from typing import Any, Callable

import validate_dg_session19_baked_pcg as baked_pcg
import validate_dg_session19_shipping_pcg_separation as shipping_separation


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19BakedPcgRuntimeClosurePolicy.json"
SEPARATION_POLICY_PATH = ROOT / "Config/DG_Session19ShippingPcgSeparationPolicy.json"
STATE = "PASS_RETIRED_RUNTIME_CLOSURE_SOURCE_PREBUILD_SEPARATION"
RELEASE_STATE = "BLOCKED_FRESH_SHIPPING_COOK_HUMAN_PERFORMANCE_OWNER_AND_RELEASE_GATES"

EXPECTED_POLICY: dict[str, Any] = {
    "schema": "DiscGolfTour.Session19BakedPcgRuntimeClosureRetirementPolicy.v2",
    "schemaVersion": 2,
    "session": 19,
    "policyId": "retired_candidate_runtime_pcg_closure_source_separation_bridge_v2",
    "authority": "SOURCE_PREBUILD_TECHNICAL_EVIDENCE_ONLY_NOT_CANDIDATE_OR_RELEASE_APPROVAL",
    "state": "RETIRED_RUNTIME_CLOSURE_DELEGATED_TO_EDITOR_AUTHORING_AND_SHIPPING_SEPARATION",
    "strategy": "SHIP_EXACT_AUTHORED_JSON_TREE_OUTPUT_AND_KEEP_PCG_AUTHORING_CODE_AND_GRAPH_OUT_OF_SHIPPING",
    "successors": {
        "sourceAuthority": {
            "policy": "Config/DG_Session19BakedPcgPolicy.json",
            "validator": "Scripts/validate_dg_session19_baked_pcg.py",
            "receipt": "Evidence/Session19/BakedPcgSourceAuthorityReceipt.json",
            "requiredState": "SOURCE_AUTHORITY_AND_SHIPPING_PCG_SOURCE_SEPARATION_VERIFIED_COOK_PROOF_PENDING",
        },
        "shippingSeparation": {
            "policy": "Config/DG_Session19ShippingPcgSeparationPolicy.json",
            "validator": "Scripts/validate_dg_session19_shipping_pcg_separation.py",
            "requiredSourceState": "PASS_SOURCE_CONFIG_SHIPPING_PCG_SEPARATION_COOK_PROOF_PENDING",
            "candidateReceiptPattern": "Evidence/Session19/ShippingPcgSeparation-{candidateId}.json",
        },
    },
    "retiredCandidateEvidence": {
        "historicalReceiptPattern": "Evidence/Session19/BakedPcgRuntimeClosure-{candidateId}.json",
        "historicalReceiptsRemainHistorical": True,
        "newCandidateReceiptEmissionAllowed": False,
        "candidateCookProofDelegatedToShippingSeparation": True,
        "prebuildSourceClosureDelegatedToBakedPcgAuthority": True,
    },
    "compileBoundary": {
        "runtimeModuleRules": "Source/DiscGolfTour/DiscGolfTour.Build.cs",
        "runtimeControllerHeader": "Source/DiscGolfTour/DiscGolfEnvironmentController.h",
        "runtimeControllerSource": "Source/DiscGolfTour/DiscGolfEnvironmentController.cpp",
        "runtimeForbiddenModuleDependencies": ["PCG"],
        "runtimeForbiddenSymbols": [
            "UPCG", "PCGComponent", "GenerateForest", "GenerateLocal", "SetGraph"
        ],
        "editorModuleRules": "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs",
        "editorRequiredModuleDependency": "PCG",
        "editorAuthoringController": "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.cpp",
        "editorAuthoringHeader": "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.h",
        "editorGenerateForestDefinitionCount": 1,
        "pcgPluginTargetAllowList": ["Editor"],
    },
    "assetBoundary": {
        "forestGraphDiskPath": "Content/Environment/Forest/PCG/PCG_TemperateMountainForest.uasset",
        "forestGraphPackagePath": "/Game/Environment/Forest/PCG/PCG_TemperateMountainForest",
        "shippingNeverCookDirectory": "/Game/Environment/Forest/PCG",
        "forestGraphPropertyEditorOnly": True,
        "runtimeAuthoredTreeSource": "ORDERED_EXPLICIT_TREES_ARRAYS_IN_AUTHORED_HOLE_JSON",
        "runtimeAuthoredTreeCount": 44,
        "runtimeAuthoredTreePayloadSha256": "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6",
    },
    "acceptedSourceProof": [
        "RUNTIME_MODULE_HAS_NO_PCG_DEPENDENCY",
        "RUNTIME_SOURCE_HAS_NO_PCG_GENERATION_TYPE_OR_API",
        "PCG_PLUGIN_IS_EDITOR_TARGET_ONLY",
        "FOREST_GRAPH_PROPERTY_IS_EDITOR_ONLY_AND_GENERIC",
        "EDITOR_MODULE_OWNS_GRAPH_CONTROLLER_AND_CUSTOM_NODE",
        "SHIPPING_COOK_CONFIGURATION_EXCLUDES_PCG_GRAPH_DIRECTORY",
        "AUTHORED_JSON_44_TREE_AUTHORITY_UNCHANGED",
    ],
    "remainingEvidence": [
        "FRESH_REPLACEMENT_WINDOWS_SHIPPING_COOK_PCG_ABSENCE",
        "THREE_HOLE_COLLISION_VISUAL_ACCEPTANCE",
        "QUANTITATIVE_PERFORMANCE_AND_SOAK_ACCEPTANCE",
        "PROVENANCE_LEGAL_DISTRIBUTION_AND_OWNER_APPROVAL",
    ],
    "claimBoundary": {
        "legacyCandidateRuntimeClosureRetired": True,
        "sourceAuthorityAccepted": True,
        "sourceConfigSeparationProven": True,
        "freshShippingCookAbsenceProven": False,
        "unrealPcgBakeSaveReopenProven": False,
        "collisionVisualApproval": False,
        "performanceAcceptance": False,
        "soakAcceptance": False,
        "manualGameplayAcceptance": False,
        "humanPlayFeelApproval": False,
        "accessibilityApproval": False,
        "provenanceApproval": False,
        "legalApproval": False,
        "distributionClearance": False,
        "ownerApproval": False,
        "releaseApproval": False,
        "session13BlockerClosed": False,
        "releaseReady": False,
    },
    "evidence": {
        "validator": "Scripts/validate_dg_session19_baked_pcg_runtime_closure.py",
        "normalState": STATE,
        "selfTestMinimumMutationCount": 16,
    },
    "session13BlockerClosed": False,
    "releaseReady": False,
}


class ContractError(ValueError):
    pass


def _strict_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ContractError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _load_policy(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(
            path.read_text(encoding="utf-8-sig"),
            object_pairs_hook=_strict_pairs,
            parse_constant=lambda token: (_ for _ in ()).throw(
                ContractError(f"non-finite JSON value: {token}")
            ),
        )
    except (OSError, UnicodeError, json.JSONDecodeError, ContractError) as exc:
        raise ContractError(f"invalid policy {path}: {exc}") from exc
    if type(value) is not dict:
        raise ContractError("policy root must be an object")
    return value


def _policy_errors(policy: dict[str, Any]) -> list[str]:
    if policy == EXPECTED_POLICY:
        return []
    errors: list[str] = []
    expected_keys = set(EXPECTED_POLICY)
    actual_keys = set(policy)
    if actual_keys != expected_keys:
        errors.append(
            "policy root keys differ: "
            f"missing={sorted(expected_keys - actual_keys)} "
            f"extra={sorted(actual_keys - expected_keys)}"
        )
    for key in sorted(expected_keys & actual_keys):
        if policy[key] != EXPECTED_POLICY[key]:
            errors.append(f"policy.{key} differs from the retired source-separation contract")
    return errors


def _source_result(policy: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    errors = _policy_errors(policy)
    errors.extend(f"baked source authority: {item}" for item in baked_pcg.audit())
    separation_facts: dict[str, Any] = {}
    try:
        separation_policy = shipping_separation.load_json(SEPARATION_POLICY_PATH)
        separation_facts = shipping_separation.source_audit(separation_policy)
    except (OSError, ValueError, shipping_separation.ValidationError) as exc:
        errors.append(f"Shipping PCG separation: {exc}")

    expected_facts = {
        "runtimePcgModuleDependencyCount": 0,
        "runtimePcgGenerationSymbolCount": 0,
        "pcgPluginTargets": ["Editor"],
        "forestGraphPropertyEditorOnly": True,
        "shippingNeverCookGraphDirectory": True,
        "editorAuthoringControllerPresent": True,
        "editorCustomNodePresent": True,
        "authoredTreePayload": {
            "treeCount": 44,
            "bytes": 2982,
            "sha256": "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6",
        },
    }
    if separation_facts and separation_facts != expected_facts:
        errors.append("Shipping PCG separation source facts differ")

    result = {
        "schema": "DiscGolfTour.Session19BakedPcgRuntimeClosureRetirementReceipt.v1",
        "schemaVersion": 1,
        "session": 19,
        "candidateId": None,
        "state": STATE if not errors else "FAIL_RETIRED_RUNTIME_CLOSURE_SOURCE_PREBUILD_SEPARATION",
        "mode": "SOURCE_PREBUILD_ONLY_NO_CANDIDATE_RECEIPT",
        "sourceAuthorityState": policy.get("successors", {}).get(
            "sourceAuthority", {}
        ).get("requiredState"),
        "shippingSeparationState": policy.get("successors", {}).get(
            "shippingSeparation", {}
        ).get("requiredSourceState"),
        "sourceFacts": separation_facts,
        "retiredCandidateEvidence": copy.deepcopy(
            policy.get("retiredCandidateEvidence", {})
        ),
        "claimBoundary": copy.deepcopy(policy.get("claimBoundary", {})),
        "failures": errors,
        "session13BlockerClosed": False,
        "releaseReady": False,
    }
    return result, errors


def _mutated_policy(operation: Callable[[dict[str, Any]], None]) -> dict[str, Any]:
    candidate = copy.deepcopy(EXPECTED_POLICY)
    operation(candidate)
    return candidate


def _self_test() -> tuple[int, list[str]]:
    try:
        live_policy = _load_policy(POLICY_PATH)
    except ContractError as exc:
        return 0, [str(exc)]
    _, baseline_errors = _source_result(live_policy)
    if baseline_errors:
        return 0, ["baseline invalid before self-test", *baseline_errors]

    mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
        ("schema", lambda value: value.__setitem__("schemaVersion", 1)),
        ("state", lambda value: value.__setitem__("state", "PASS")),
        ("strategy", lambda value: value.__setitem__("strategy", "RUNTIME_PCG")),
        ("source successor", lambda value: value["successors"]["sourceAuthority"].__setitem__("validator", "wrong.py")),
        ("cook successor", lambda value: value["successors"]["shippingSeparation"].__setitem__("validator", "wrong.py")),
        ("candidate emission", lambda value: value["retiredCandidateEvidence"].__setitem__("newCandidateReceiptEmissionAllowed", True)),
        ("runtime dependency", lambda value: value["compileBoundary"]["runtimeForbiddenModuleDependencies"].clear()),
        ("runtime symbol", lambda value: value["compileBoundary"]["runtimeForbiddenSymbols"].pop()),
        ("editor dependency", lambda value: value["compileBoundary"].__setitem__("editorRequiredModuleDependency", "Engine")),
        ("plugin target", lambda value: value["compileBoundary"]["pcgPluginTargetAllowList"].append("Runtime")),
        ("NeverCook", lambda value: value["assetBoundary"].__setitem__("shippingNeverCookDirectory", "/Game")),
        ("tree hash", lambda value: value["assetBoundary"].__setitem__("runtimeAuthoredTreePayloadSha256", "0" * 64)),
        ("fresh cook overclaim", lambda value: value["claimBoundary"].__setitem__("freshShippingCookAbsenceProven", True)),
        ("performance overclaim", lambda value: value["claimBoundary"].__setitem__("performanceAcceptance", True)),
        ("owner overclaim", lambda value: value["claimBoundary"].__setitem__("ownerApproval", True)),
        ("release overclaim", lambda value: value["claimBoundary"].__setitem__("releaseReady", True)),
        ("blocker overclaim", lambda value: value.__setitem__("session13BlockerClosed", True)),
        ("mutation minimum", lambda value: value["evidence"].__setitem__("selfTestMinimumMutationCount", 1)),
    ]
    caught = 0
    failures: list[str] = []
    for name, operation in mutations:
        candidate = _mutated_policy(operation)
        if _policy_errors(candidate):
            caught += 1
        else:
            failures.append(f"policy mutation escaped detection: {name}")

    baked_caught, baked_failures = baked_pcg.self_test()
    separation_policy = shipping_separation.load_json(SEPARATION_POLICY_PATH)
    separation_caught, separation_failures = shipping_separation.self_test(
        separation_policy
    )
    caught += baked_caught + separation_caught
    failures.extend(f"baked successor: {item}" for item in baked_failures)
    failures.extend(f"separation successor: {item}" for item in separation_failures)
    return caught, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--emit-json", action="store_true")
    parser.add_argument("--release-required", action="store_true")
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive")
    parser.add_argument("--unrealpak")
    parser.add_argument("--unreal-editor-cmd")
    parser.add_argument("--expected-receipt")
    args = parser.parse_args()

    retired_arguments = (
        args.candidate_id,
        args.archive,
        args.unrealpak,
        args.unreal_editor_cmd,
        args.expected_receipt,
    )
    if any(retired_arguments):
        parser.error(
            "candidate-bound runtime closure is retired; use "
            "validate_dg_session19_shipping_pcg_separation.py for fresh cook proof"
        )
    if args.self_test and any((args.emit_json, args.release_required)):
        parser.error("--self-test does not accept output or release arguments")

    if args.self_test:
        caught, failures = _self_test()
        minimum = EXPECTED_POLICY["evidence"]["selfTestMinimumMutationCount"]
        if failures or caught < minimum:
            print(
                "Session 19 retired baked-PCG runtime-closure self-test: "
                f"FAIL ({caught} mutations caught)"
            )
            for failure in failures:
                print(f"- {failure}")
            return 1
        print(
            "Session 19 retired baked-PCG runtime-closure self-test: "
            f"PASS ({caught} mutations caught)"
        )
        return 0

    try:
        policy = _load_policy(POLICY_PATH)
    except ContractError as exc:
        print(f"Session 19 retired baked-PCG runtime closure: FAIL\n- {exc}")
        return 1
    result, errors = _source_result(policy)
    if args.emit_json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    elif errors:
        print("Session 19 retired baked-PCG runtime closure: FAIL")
        for error in errors:
            print(f"- {error}")
    else:
        print(f"Session 19 retired baked-PCG runtime closure: {STATE}")
        print(
            "Runtime PCG generation is absent; Editor owns authoring and the "
            "project PCG graph root is NeverCook."
        )
        print("Fresh replacement-candidate cook and human/release gates remain pending.")
    if errors:
        return 1
    if args.release_required:
        if not args.emit_json:
            print(f"Session 19 retired baked-PCG runtime closure release state: {RELEASE_STATE}")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
