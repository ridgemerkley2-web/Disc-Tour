#!/usr/bin/env python3
"""Predeclare one external sustained Shipping performance evidence run.

The declaration binds an exact candidate archive and the already validated
three-hole rendered-performance receipt before any sustained capture starts.
It never launches the game, starts profiling tools, writes into the candidate
archive/project/evidence run, or grants performance/release approval.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import stat
import sys
import tempfile
from typing import Any, Callable, Optional, Sequence
import uuid

import audit_dg_session19_performance_certification_readiness as readiness
import validate_dg_session19_shipping_performance as shipping


ROOT = Path(__file__).resolve().parents[1]
POLICY_V1_PATH = ROOT / "Config/DG_Session19SustainedShippingPerformanceCertificationPolicy.json"
POLICY_V2_PATH = ROOT / "Config/DG_Session19SustainedShippingPerformanceCertificationPolicyV2.json"
POLICY_PATH = POLICY_V2_PATH
POLICY_SCHEMA = "DiscGolfTour.Session19SustainedShippingPerformanceCertificationPolicy.v2"
DECLARATION_SCHEMA = "DiscGolfTour.Session19SustainedShippingPerformanceDeclaration.v2"
SUPPORTED_POLICY_PATHS = {
    POLICY_V1_PATH.name: POLICY_V1_PATH,
    POLICY_V2_PATH.name: POLICY_V2_PATH,
}
DECLARATION_STATE = "PREDECLARED_EXTERNAL_CAPTURE_NOT_STARTED_NO_APPROVAL_CLAIM"
CANDIDATE_RE = re.compile(
    r"^S19_WindowsShipping_\d{8}T\d{6}Z_[0-9a-f]{12}$"
)


class DeclarationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise DeclarationError(message)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def is_reparse(path: Path) -> bool:
    if path.is_symlink():
        return True
    try:
        attributes = getattr(path.lstat(), "st_file_attributes", 0)
    except OSError:
        return False
    return bool(attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def path_chain_has_reparse(path: Path) -> bool:
    absolute = path.absolute()
    if not absolute.parts:
        return False
    cursor = Path(absolute.parts[0])
    for part in absolute.parts[1:]:
        cursor /= part
        if os.path.lexists(cursor) and is_reparse(cursor):
            return True
    return False


def stable_binding(path: Path, label: str) -> dict[str, Any]:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise DeclarationError(
            f"{label} cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(resolved.is_file() and not is_reparse(resolved), f"{label} is not a regular file")
    before = resolved.stat()
    binding = shipping.file_binding(resolved)
    after = resolved.stat()
    fingerprint = lambda value: (
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
        value.st_dev,
        value.st_ino,
    )
    require(fingerprint(before) == fingerprint(after), f"{label} changed while hashed")
    return binding


def policy_version(policy: dict[str, Any]) -> int:
    version = policy.get("schemaVersion")
    require(type(version) is int and version in {1, 2}, "policy version differs")
    return version


def resolve_bound_policy_path(binding: Any) -> Path:
    require(type(binding) is dict and set(binding) == {"fileName", "bytes", "sha256"},
            "policy binding shape differs")
    path = SUPPORTED_POLICY_PATHS.get(binding.get("fileName"))
    require(path is not None, "policy binding names an unsupported policy")
    require(stable_binding(path, "certification policy") == binding,
            "predeclaration policy binding differs")
    return path


def resolve_supported_policy_path(path: Path) -> Path:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise DeclarationError(
            f"certification policy cannot be resolved ({exc.__class__.__name__})"
        ) from None
    for supported in SUPPORTED_POLICY_PATHS.values():
        if resolved == supported.resolve(strict=True):
            return supported
    raise DeclarationError("certification policy path is not an exact supported policy")


def load_policy(path: Path = POLICY_PATH) -> dict[str, Any]:
    policy = shipping.load_json(path)
    version = policy_version(policy)
    require(policy.get("schema") ==
            f"DiscGolfTour.Session19SustainedShippingPerformanceCertificationPolicy.v{version}",
            "policy schema differs")
    require(policy.get("session") == 19, "policy session differs")
    target = policy.get("target")
    require(type(target) is dict, "policy target is missing")
    require(target.get("platform") == "Windows"
            and target.get("configuration") == "Shipping"
            and target.get("archiveLeaf") == "Windows"
            and target.get("shippingExecutablePath") == shipping.EXECUTABLE_RELATIVE,
            "policy Shipping target differs")
    require(target.get("candidateIdPattern") ==
            r"^S19_WindowsShipping_[0-9]{8}T[0-9]{6}Z_[0-9a-f]{12}$",
            "policy candidate grammar differs")
    pre = policy.get("predeclaration")
    require(type(pre) is dict, "policy predeclaration is missing")
    require(pre.get("minimumDurationSeconds") == 900, "minimum duration was weakened")
    require(pre.get("maximumDurationSeconds") == 7200, "maximum duration differs")
    require(pre.get("declarationMustPrecedeEveryCapturedArtifact") is True,
            "predeclaration chronology was weakened")
    sensor = policy.get("nvidiaSensorContract")
    require(type(sensor) is dict, "sensor contract is missing")
    require(sensor.get("nominalIntervalMilliseconds") == 1000,
            "sensor interval must remain exactly one second")
    require(sensor.get("minimumIntervalMilliseconds") == 500
            and sensor.get("maximumIntervalMilliseconds") == 1500,
            "sensor continuity bounds differ")
    require(sensor.get("minimumSampleCoverageRatio") >= 0.98,
            "sensor coverage ratio was weakened")
    require(sensor.get("requiredCsvColumns") == [
        "sampleIndex", "timestampUtc", "gpuUuid", "gpuName", "driverVersion",
        "temperatureC", "powerDrawW", "powerLimitW", "utilizationGpuPercent",
        "graphicsClockMHz", "memoryClockMHz", "pstate",
    ], "sensor columns differ")
    power_limit_field = "power.limit" if version == 1 else "enforced.power.limit"
    require(sensor.get("requiredNvidiaSmiQueryFields") == [
        "uuid", "name", "driver_version", "temperature.gpu", "power.draw",
        power_limit_field, "utilization.gpu", "clocks.current.graphics",
        "clocks.current.memory", "pstate",
    ], "nvidia-smi query fields differ")
    if version == 1:
        require("effectivePowerLimitQueryField" not in sensor,
                "v1 policy may not retrofit a power-limit alias")
    else:
        require(sensor.get("effectivePowerLimitQueryField") == power_limit_field,
                "v2 effective power-limit field differs")
    host = policy.get("hostSnapshotContract")
    require(type(host) is dict, "host snapshot contract is missing")
    for key in (
        "requireSameGpuUuid", "requireSameGpuDriver", "requireSameActivePowerScheme",
        "requireAcOnline", "requireBatterySaverOff", "requireGpuPowerDrawAndLimit",
    ):
        require(host.get(key) is True, f"host requirement {key} was weakened")
    require(host.get("minimumFreeBytes", 0) >= 20 * 1024**3,
            "host free-space requirement was weakened")
    present = policy.get("presentMonContract")
    require(type(present) is dict, "PresentMon contract is missing")
    require(present.get("requiredCsvColumns") == [
        "Application", "ProcessID", "TimeInSeconds", "MsBetweenPresents",
        "PresentMode", "Dropped",
    ], "PresentMon columns differ")
    require(type(present.get("maximumContinuityGapMilliseconds")) in (int, float)
            and present["maximumContinuityGapMilliseconds"] <= 1000.0,
            "PresentMon continuity bound was weakened")
    require(type(present.get("minimumPresentRateHz")) in (int, float)
            and present["minimumPresentRateHz"] >= 10.0,
            "PresentMon minimum rate was weakened")
    require(present.get("requiredApplication") == "DiscGolfTour-Win64-Shipping.exe",
            "PresentMon application differs")
    require(type(present.get("maximumDroppedRatio")) in (int, float)
            and present["maximumDroppedRatio"] <= 0.01,
            "PresentMon dropped ratio was weakened")
    profile = policy.get("profileContract")
    require(type(profile) is dict, "profile contract is missing")
    require(profile.get("requiredThreadRoles") == [
        "GPU", "GameThread", "RenderThread", "RHIThread"
    ], "required GPU/game/render/RHI roles differ")
    require(set(profile.get("requiredTraceChannels", [])) >= {"cpu", "frame", "gpu"},
            "required Unreal Insights channels differ")
    require(profile.get("minimumProfileDurationSeconds", 0) >= 60
            and profile.get("minimumEventsPerRole", 0) >= 60
            and profile.get("minimumTraceBytes", 0) >= 1024 * 1024,
            "profile artifact minimums were weakened")
    require(profile.get("requiredTraceMagicAscii") == ["2CRT", "ECRT"],
            "Unreal trace magic contract differs")
    require(profile.get("minimumDistinctTraceByteValues", 0) >= 32
            and profile.get("minimumTraceNonZeroRatio", 0) >= 0.05,
            "profile placeholder rejection was weakened")
    chronology = policy.get("chronologyContract")
    require(type(chronology) is dict, "chronology contract is missing")
    for key in (
        "declarationMustPrecedePreSnapshots", "preSnapshotsMustNotFollowRunStart",
        "postSnapshotsMustNotPrecedeRunFinish", "manifestMustFollowPostSnapshots",
    ):
        require(chronology.get(key) is True, f"chronology requirement {key} was weakened")
    boundary = policy.get("claimBoundary")
    require(type(boundary) is dict, "claim boundary is missing")
    for key in (
        "humanPerformanceAcceptance",
        "productOwnerApproval",
        "releaseApproval",
        "releaseReady",
        "cryptographicProcessAttestation",
        "hostileSameUserForgeryResistance",
    ):
        require(boundary.get(key) is False, f"policy may not grant {key}")
    artifacts = policy.get("requiredArtifacts")
    require(artifacts == {
        "runManifest": "RunManifest.json",
        "archivePre": "ArchiveSnapshot.Pre.json",
        "archivePost": "ArchiveSnapshot.Post.json",
        "hostPre": "HostSnapshot.Pre.json",
        "hostPost": "HostSnapshot.Post.json",
        "nvidiaSensorCapture": "NvidiaSensorCapture.json",
        "nvidiaSensors": "NvidiaSensors.csv",
        "presentMonCapture": "PresentMonCapture.json",
        "presentMonCsv": "PresentMon.csv",
        "threadProfile": "ThreadProfile.json",
        "unrealInsightsTrace": "UnrealInsights.utrace",
        "unrealInsightsTimingCsv": "UnrealInsightsTiming.csv",
    }, "required artifact set differs")
    require(policy.get("schemas") == {
        "declaration":
            f"DiscGolfTour.Session19SustainedShippingPerformanceDeclaration.v{version}",
        "runManifest": "DiscGolfTour.Session19SustainedShippingPerformanceRunManifest.v1",
        "archiveSnapshot": "DiscGolfTour.Session19SustainedShippingPerformanceArchiveSnapshot.v1",
        "hostSnapshot": "DiscGolfTour.Session19SustainedShippingPerformanceHostSnapshot.v1",
        "nvidiaSensorCapture": "DiscGolfTour.Session19SustainedShippingNvidiaSensorCapture.v1",
        "presentMonCapture": "DiscGolfTour.Session19SustainedShippingPresentMonCapture.v1",
        "threadProfile": "DiscGolfTour.Session19SustainedShippingThreadProfile.v1",
        "technicalReceipt": "DiscGolfTour.Session19SustainedShippingPerformanceTechnicalReceipt.v1",
    }, "policy artifact schemas differ")
    require(policy.get("technicalPassState") ==
            "PASS_SUSTAINED_EXTERNAL_SHIPPING_PERFORMANCE_EVIDENCE_HUMAN_APPROVALS_PENDING",
            "policy technical pass state differs")
    return policy


def archive_identity(archive: Path, candidate_id: str) -> dict[str, Any]:
    manifest = shipping.build_archive_manifest(archive, candidate_id)
    return {
        "inventorySha256": manifest["inventorySha256"],
        "fileCount": manifest["fileCount"],
        "totalBytes": manifest["totalBytes"],
        "launcher": manifest["launcher"],
        "shippingExecutable": manifest["shippingExecutable"],
    }


def receipt_identity(path: Path, receipt: dict[str, Any]) -> dict[str, Any]:
    binding = stable_binding(path, "three-hole performance receipt")
    return {
        **binding,
        "schema": receipt.get("schema"),
        "state": receipt.get("state"),
        "runId": receipt.get("runId"),
        "claimBoundary": receipt.get("claimBoundary"),
    }


def declaration_claim_boundary() -> dict[str, Any]:
    return {
        "predeclaredOnly": True,
        "captureExecuted": False,
        "technicalEvidenceAccepted": False,
        "sustainedPerformanceCertified": False,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }


def compose_declaration(
    *,
    policy: dict[str, Any],
    policy_binding: dict[str, Any],
    candidate_id: str,
    certification_run_id: str,
    duration_seconds: int,
    archive: dict[str, Any],
    performance_receipt: dict[str, Any],
    declared_utc: str,
) -> dict[str, Any]:
    require(CANDIDATE_RE.fullmatch(candidate_id) is not None, "candidate ID is invalid")
    try:
        parsed_run_id = uuid.UUID(certification_run_id)
    except (ValueError, AttributeError):
        raise DeclarationError("certification run ID is invalid") from None
    require(
        str(parsed_run_id) == certification_run_id
        and parsed_run_id.version == 4
        and parsed_run_id.variant == uuid.RFC_4122,
        "certification run ID must be lowercase RFC 4122 UUIDv4",
    )
    pre = policy["predeclaration"]
    require(type(duration_seconds) is int, "duration must be an integer")
    require(
        pre["minimumDurationSeconds"] <= duration_seconds <= pre["maximumDurationSeconds"],
        "duration is outside the predeclared policy range",
    )
    require(performance_receipt.get("state") == readiness.EXPECTED_PASS_STATE,
            "three-hole receipt is not the accepted rendered-performance state")
    require(performance_receipt.get("claimBoundary") == readiness.EXPECTED_RECEIPT_CLAIMS,
            "three-hole receipt claim boundary differs")
    version = policy_version(policy)
    return {
        "schema": policy["schemas"]["declaration"],
        "schemaVersion": version,
        "session": 19,
        "candidateId": candidate_id,
        "certificationRunId": certification_run_id,
        "declaredUtc": declared_utc,
        "declaredDurationSeconds": duration_seconds,
        "state": DECLARATION_STATE,
        "policy": policy_binding,
        "archive": archive,
        "threeHolePerformanceReceipt": performance_receipt,
        "requiredArtifacts": policy["requiredArtifacts"],
        "schemas": policy["schemas"],
        "hostSnapshotContract": policy["hostSnapshotContract"],
        "nvidiaSensorContract": policy["nvidiaSensorContract"],
        "presentMonContract": policy["presentMonContract"],
        "profileContract": policy["profileContract"],
        "chronologyContract": policy["chronologyContract"],
        "technicalPassState": policy["technicalPassState"],
        "futureTechnicalReceiptClaimBoundary": policy["claimBoundary"],
        "claimBoundary": declaration_claim_boundary(),
    }


def validate_output_path(
    output: Path,
    external_root: Path,
    archive: Path,
    three_hole_run_root: Path,
) -> Path:
    require(output.is_absolute() and external_root.is_absolute(),
            "output and external root must be absolute")
    require(".." not in output.parts and ".." not in external_root.parts,
            "output paths cannot contain parent traversal")
    require(not path_chain_has_reparse(external_root), "external root contains a reparse point")
    prospective_external = external_root.resolve(strict=False)
    prospective_output = output.resolve(strict=False)
    require(not shipping.is_same_or_under(prospective_external, ROOT),
            "external root must be outside the project")
    require(not shipping.is_same_or_under(prospective_external, archive),
            "external root must be outside the candidate archive")
    require(not shipping.is_same_or_under(prospective_external, three_hole_run_root),
            "external root must be outside the rendered-performance run")
    require(shipping.is_same_or_under(prospective_output, prospective_external),
            "declaration output must remain under the external root")
    require(not shipping.is_same_or_under(prospective_output, ROOT),
            "declaration output must remain outside the project")
    require(not shipping.is_same_or_under(prospective_output, archive),
            "declaration output must remain outside the candidate archive")
    require(not shipping.is_same_or_under(prospective_output, three_hole_run_root),
            "declaration output must remain outside the rendered-performance run")
    external_root.mkdir(parents=True, exist_ok=True)
    resolved_external = external_root.resolve(strict=True)
    resolved_output = output.resolve(strict=False)
    require(shipping.is_same_or_under(resolved_output, resolved_external),
            "declaration output must remain under the external root")
    require(not output.exists(), "refusing to overwrite an existing declaration")
    require(not path_chain_has_reparse(output.parent), "declaration parent contains a reparse point")
    return resolved_output


ReceiptVerifier = Callable[[Path, Path, Path, str], tuple[dict[str, Any], dict[str, Any]]]


def generate(
    *,
    candidate_id: str,
    archive: Path,
    receipt_path: Path,
    three_hole_run_root: Path,
    duration_seconds: int,
    external_root: Path,
    output: Path,
    policy_path: Path = POLICY_PATH,
    receipt_verifier: ReceiptVerifier = readiness.validate_receipt_against_run,
) -> dict[str, Any]:
    require(CANDIDATE_RE.fullmatch(candidate_id) is not None, "candidate ID is invalid")
    archive = shipping.validate_candidate_archive_path(archive, candidate_id)
    try:
        three_hole_run_root = three_hole_run_root.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise DeclarationError(
            f"three-hole run root cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(three_hole_run_root.is_dir() and not is_reparse(three_hole_run_root),
            "three-hole run root is not a regular directory")
    output = validate_output_path(output, external_root, archive, three_hole_run_root)
    policy_path = resolve_supported_policy_path(policy_path)
    policy = load_policy(policy_path)
    policy_binding = stable_binding(policy_path, "certification policy")
    try:
        receipt, _ = receipt_verifier(
            receipt_path, three_hole_run_root, archive, candidate_id
        )
    except (readiness.AuditError, shipping.EvidenceError, OSError) as exc:
        raise DeclarationError(f"three-hole receipt revalidation failed: {exc}") from None
    before_archive = archive_identity(archive, candidate_id)
    before_receipt = receipt_identity(receipt_path, receipt)
    run_id = str(uuid.uuid4())
    declaration = compose_declaration(
        policy=policy,
        policy_binding=policy_binding,
        candidate_id=candidate_id,
        certification_run_id=run_id,
        duration_seconds=duration_seconds,
        archive=before_archive,
        performance_receipt=before_receipt,
        declared_utc=utc_now(),
    )
    after_archive = archive_identity(archive, candidate_id)
    try:
        after_receipt, _ = receipt_verifier(
            receipt_path, three_hole_run_root, archive, candidate_id
        )
    except (readiness.AuditError, shipping.EvidenceError, OSError) as exc:
        raise DeclarationError(
            f"three-hole receipt post-composition validation failed: {exc}"
        ) from None
    require(after_archive == before_archive, "candidate archive changed during declaration")
    require(receipt_identity(receipt_path, after_receipt) == before_receipt,
            "three-hole receipt changed during declaration")
    shipping.write_exclusive_json(output, declaration)
    require(shipping.load_json(output) == declaration,
            "published declaration does not round-trip exactly")
    return declaration


def run_self_test() -> int:
    checks = 0

    def check(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    policy_v1 = load_policy(POLICY_V1_PATH)
    policy = load_policy()
    policy_binding = stable_binding(POLICY_PATH, "self-test policy")
    check(policy_version(policy_v1) == 1, "v1 policy version differs")
    check(policy_version(policy) == 2, "v2 policy version differs")
    check(policy_v1["nvidiaSensorContract"]["requiredNvidiaSmiQueryFields"][5]
          == "power.limit", "v1 power-limit field differs")
    check(policy["nvidiaSensorContract"]["effectivePowerLimitQueryField"]
          == "enforced.power.limit", "v2 power-limit field differs")
    check(resolve_bound_policy_path(policy_binding) == POLICY_PATH,
          "bound v2 policy resolution differs")
    receipt = {
        "fileName": "ShippingPerformance.json",
        "bytes": 100,
        "sha256": "A" * 64,
        "schema": shipping.RECEIPT_SCHEMA,
        "state": readiness.EXPECTED_PASS_STATE,
        "runId": "11111111-2222-4333-8444-555555555555",
        "claimBoundary": readiness.EXPECTED_RECEIPT_CLAIMS,
    }
    archive = {
        "inventorySha256": "B" * 64,
        "fileCount": 2,
        "totalBytes": 200,
        "launcher": {"relativePath": shipping.LAUNCHER_RELATIVE},
        "shippingExecutable": {"relativePath": shipping.EXECUTABLE_RELATIVE},
    }
    run_id = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
    declaration = compose_declaration(
        policy=policy,
        policy_binding=policy_binding,
        candidate_id="S19_WindowsShipping_20990101T000000Z_abcdef123456",
        certification_run_id=run_id,
        duration_seconds=1800,
        archive=archive,
        performance_receipt=receipt,
        declared_utc="2099-01-01T00:00:00.000Z",
    )
    check(declaration["certificationRunId"] == run_id, "run ID differs")
    check(declaration["declaredDurationSeconds"] == 1800, "duration differs")
    check(declaration["archive"] == archive, "archive identity differs")
    check(declaration["threeHolePerformanceReceipt"] == receipt, "receipt differs")
    check(declaration["claimBoundary"]["predeclaredOnly"] is True,
          "declaration must remain predeclared only")
    check(declaration["claimBoundary"]["captureExecuted"] is False,
          "generator cannot claim a capture")
    check(declaration["claimBoundary"]["humanPerformanceAcceptance"] is False,
          "generator cannot grant performance approval")
    check(declaration["claimBoundary"]["releaseReady"] is False,
          "generator cannot grant release readiness")
    for bad_duration in (899, 7201, True):
        try:
            compose_declaration(
                policy=policy,
                policy_binding=policy_binding,
                candidate_id=declaration["candidateId"],
                certification_run_id=run_id,
                duration_seconds=bad_duration,
                archive=archive,
                performance_receipt=receipt,
                declared_utc=declaration["declaredUtc"],
            )
        except DeclarationError:
            rejected = True
        else:
            rejected = False
        check(rejected, f"invalid duration {bad_duration!r} must be rejected")
    overclaim = dict(receipt)
    overclaim["claimBoundary"] = dict(readiness.EXPECTED_RECEIPT_CLAIMS)
    overclaim["claimBoundary"]["releaseReady"] = True
    try:
        compose_declaration(
            policy=policy,
            policy_binding=policy_binding,
            candidate_id=declaration["candidateId"],
            certification_run_id=run_id,
            duration_seconds=1800,
            archive=archive,
            performance_receipt=overclaim,
            declared_utc=declaration["declaredUtc"],
        )
    except DeclarationError:
        rejected = True
    else:
        rejected = False
    check(rejected, "overclaiming rendered receipt must be rejected")
    with tempfile.TemporaryDirectory(prefix="dg-s19-sustained-declaration-") as temporary:
        external = Path(temporary).resolve()
        fake_archive = external / "Archive"
        fake_run = external / "PriorRun"
        fake_archive.mkdir()
        fake_run.mkdir()
        output = external / "Declarations" / "Declaration.json"
        resolved = validate_output_path(output, external, fake_archive, fake_run)
        check(resolved == output.resolve(), "external output resolution differs")
        try:
            validate_output_path(ROOT / "Config/forbidden.json", ROOT, fake_archive, fake_run)
        except DeclarationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "project-local declaration must be rejected")
        weakened_host = json.loads(json.dumps(policy))
        weakened_host["hostSnapshotContract"]["requireAcOnline"] = False
        weakened_host_path = external / "weakened-host.json"
        shipping.write_exclusive_json(weakened_host_path, weakened_host)
        try:
            resolve_supported_policy_path(weakened_host_path)
        except DeclarationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "declaration generation must reject an unapproved policy path")
        try:
            load_policy(weakened_host_path)
        except DeclarationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "policy may not weaken AC-line evidence")
        weakened_profile = json.loads(json.dumps(policy))
        weakened_profile["profileContract"]["requiredThreadRoles"].remove("RHIThread")
        weakened_profile_path = external / "weakened-profile.json"
        shipping.write_exclusive_json(weakened_profile_path, weakened_profile)
        try:
            load_policy(weakened_profile_path)
        except DeclarationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "policy may not remove the RHI-thread profile role")
    print(
        f"Session 19 sustained performance declaration generator self-test OK "
        f"({checks} assertions; no process launched)."
    )
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--three-hole-receipt", type=Path)
    parser.add_argument("--three-hole-run-root", type=Path)
    parser.add_argument("--duration-seconds", type=int)
    parser.add_argument("--external-root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--policy", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    evidence_args = (
        args.candidate_id,
        args.archive,
        args.three_hole_receipt,
        args.three_hole_run_root,
        args.duration_seconds,
        args.external_root,
        args.output,
        args.policy,
    )
    if args.self_test:
        if any(value is not None for value in evidence_args):
            print("--self-test accepts no declaration arguments", file=sys.stderr)
            return 2
        try:
            return run_self_test()
        except (AssertionError, DeclarationError, shipping.EvidenceError, OSError) as exc:
            print(f"Sustained performance declaration self-test FAILED: {exc}", file=sys.stderr)
            return 1
    if any(value is None for value in evidence_args[:-1]):
        print(
            "--candidate-id, --archive, --three-hole-receipt, --three-hole-run-root, "
            "--duration-seconds, --external-root, and --output are required",
            file=sys.stderr,
        )
        return 2
    try:
        declaration = generate(
            candidate_id=args.candidate_id,
            archive=args.archive,
            receipt_path=args.three_hole_receipt,
            three_hole_run_root=args.three_hole_run_root,
            duration_seconds=args.duration_seconds,
            external_root=args.external_root,
            output=args.output,
            policy_path=args.policy or POLICY_PATH,
        )
    except (DeclarationError, shipping.EvidenceError, OSError) as exc:
        print(f"Sustained performance declaration failed closed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({
        "state": declaration["state"],
        "candidateId": declaration["candidateId"],
        "certificationRunId": declaration["certificationRunId"],
        "declaredDurationSeconds": declaration["declaredDurationSeconds"],
        "captureExecuted": False,
        "humanPerformanceAcceptance": False,
        "releaseReady": False,
    }, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
