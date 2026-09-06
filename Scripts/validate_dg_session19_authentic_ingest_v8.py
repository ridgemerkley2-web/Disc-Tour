"""Fail-closed validator for the append-only v008 authentic-motion ingest receipt."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import re
import tempfile
from pathlib import Path
from typing import Any

ROOT = Path(__file__).absolute().parents[1].resolve()
SCHEMA = "DiscGolfTour.Session19AuthenticIngestReceipt.v1"
REVISION = "v008"
SHA256_RE = re.compile(r"[0-9A-F]{64}")
RATIONAL_RE = re.compile(r"[1-9][0-9]*/[1-9][0-9]*")


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _exact(obj: Any, fields: set[str], label: str, errors: list[str]) -> dict[str, Any] | None:
    if not isinstance(obj, dict):
        errors.append(f"{label} must be an object")
        return None
    if set(obj) != fields:
        errors.append(f"{label} fields differ: expected {sorted(fields)}")
        return None
    return obj


def _named(value: Any) -> bool:
    if not isinstance(value, str) or len(value.strip()) < 3:
        return False
    normalized = value.strip().upper()
    return normalized not in {"TBD", "TODO", "UNKNOWN", "NONE", "N/A", "PLACEHOLDER", "UNASSIGNED", "PENDING"} and not normalized.startswith(("UNVERIFIED", "PENDING_", "REPLACE"))


def _file(record: Any, label: str, root: Path, errors: list[str], allow_external: bool = False) -> Path | None:
    item = _exact(record, {"path", "bytes", "sha256"}, label, errors)
    if item is None:
        return None
    raw = item.get("path")
    if not isinstance(raw, str) or not raw or ".." in Path(raw).parts:
        errors.append(f"{label}.path must be a non-empty path without traversal")
        return None
    raw_path = Path(raw)
    path = raw_path.resolve() if raw_path.is_absolute() else (root / raw_path).resolve()
    if not allow_external:
        try:
            path.relative_to(root)
        except ValueError:
            errors.append(f"{label}.path must remain under project root")
            return None
    if not path.is_file():
        errors.append(f"{label}.path is missing")
        return None
    size = path.stat().st_size
    digest = _sha256(path)
    if type(item.get("bytes")) is not int or item["bytes"] <= 0 or item["bytes"] != size:
        errors.append(f"{label}.bytes does not match the file")
    if not isinstance(item.get("sha256"), str) or SHA256_RE.fullmatch(item["sha256"]) is None:
        errors.append(f"{label}.sha256 must be 64 uppercase hexadecimal characters")
    elif item["sha256"] != digest:
        errors.append(f"{label}.sha256 does not match the file")
    return path


def validate(data: Any, root: Path = ROOT) -> list[str]:
    errors: list[str] = []
    top = _exact(data, {"schema", "candidate_revision", "source_identity", "source_media", "ingested_asset", "rights", "solve", "approvals", "status"}, "receipt", errors)
    if top is None:
        return errors
    if top["schema"] != SCHEMA:
        errors.append("schema is not the v008 authentic-ingest schema")
    if top["candidate_revision"] != REVISION:
        errors.append("candidate_revision must be exactly v008")

    identity = _exact(top["source_identity"], {"source_id", "take_id", "performer_name", "capture_date", "source_file"}, "source_identity", errors)
    media = _exact(top["source_media"], {"codec", "width_pixels", "height_pixels", "frame_rate_fps", "frame_count", "time_base", "duration_seconds", "constant_frame_rate"}, "source_media", errors)
    rights = _exact(top["rights"], {"ownership_or_license", "derivative_animation_allowed", "game_distribution_allowed", "performer_release_verified", "evidence_file"}, "rights", errors)
    solve = _exact(top["solve"], {"completed", "solver", "solver_version", "settings_id", "output_file", "source_sha256", "frame_count", "time_base", "quality_review_passed", "reviewer", "receipt_file"}, "solve", errors)
    approvals = _exact(top["approvals"], {"rights_reviewer", "technical_reviewer", "approved_for_candidate_authoring"}, "approvals", errors)

    source_path = _file(identity.get("source_file") if identity else None, "source_identity.source_file", root, errors, allow_external=True)
    ingest_paths: list[Path] = []
    if not isinstance(top["ingested_asset"], list) or len(top["ingested_asset"]) != 3:
        errors.append("ingested_asset must contain exactly three Capture Manager asset records")
        ingested_items: list[Any] = []
    else:
        ingested_items = top["ingested_asset"]
    ingested_records: list[dict[str, Any]] = []
    for index, raw_ingested in enumerate(ingested_items):
        record = _exact(raw_ingested, {"asset_id", "role", "file", "derived_from_source_sha256", "import_tool", "import_settings_id"}, f"ingested_asset[{index}]", errors)
        if record:
            ingested_records.append(record)
            path = _file(record["file"], f"ingested_asset[{index}].file", root, errors)
            if path: ingest_paths.append(path)
    _file(rights.get("evidence_file") if rights else None, "rights.evidence_file", root, errors)
    solve_path = _file(solve.get("output_file") if solve else None, "solve.output_file", root, errors)
    _file(solve.get("receipt_file") if solve else None, "solve.receipt_file", root, errors)

    if identity:
        for key in ("source_id", "take_id", "performer_name", "capture_date"):
            if not _named(identity.get(key)):
                errors.append(f"source_identity.{key} must be named and non-placeholder")
    if media:
        if not _named(media.get("codec")):
            errors.append("source_media.codec is required")
        for key in ("width_pixels", "height_pixels", "frame_count"):
            if type(media.get(key)) is not int or media[key] <= 0:
                errors.append(f"source_media.{key} must be a positive integer")
        for key in ("frame_rate_fps", "duration_seconds"):
            value = media.get(key)
            if type(value) not in {int, float} or not math.isfinite(value) or value <= 0:
                errors.append(f"source_media.{key} must be finite and positive")
        if not isinstance(media.get("time_base"), str) or RATIONAL_RE.fullmatch(media["time_base"]) is None:
            errors.append("source_media.time_base must be a positive rational")
        if media.get("constant_frame_rate") is not True:
            errors.append("source_media.constant_frame_rate must be exactly true")
        if all(type(media.get(k)) in {int, float} and not isinstance(media.get(k), bool) and media[k] > 0 for k in ("frame_count", "frame_rate_fps", "duration_seconds")):
            expected = media["frame_count"] / media["frame_rate_fps"]
            if abs(expected - media["duration_seconds"]) > max(0.05, 1.5 / media["frame_rate_fps"]):
                errors.append("source_media frame count/fps/duration are inconsistent")
    source_hash = _sha256(source_path) if source_path else None
    roles: set[str] = set()
    for index, record in enumerate(ingested_records):
        if record.get("derived_from_source_sha256") != source_hash:
            errors.append(f"ingested_asset[{index}] is not hash-bound to source file")
        for key in ("asset_id", "role", "import_tool", "import_settings_id"):
            if not _named(record.get(key)):
                errors.append(f"ingested_asset[{index}].{key} is required")
        roles.add(str(record.get("role")))
    if roles != {"CaptureData", "ImageSequence", "SoundWave"}:
        errors.append("ingested_asset roles must be exactly CaptureData, ImageSequence, and SoundWave")
    if len(set(ingest_paths)) != len(ingest_paths):
        errors.append("ingested asset files must be distinct")
    if rights:
        if not _named(rights.get("ownership_or_license")):
            errors.append("rights.ownership_or_license must be explicit")
        for key in ("derivative_animation_allowed", "game_distribution_allowed", "performer_release_verified"):
            if rights.get(key) is not True:
                errors.append(f"rights.{key} must be exactly true")
    if solve:
        if solve.get("completed") is not True or solve.get("quality_review_passed") is not True:
            errors.append("solve completion and quality review must both be exactly true")
        for key in ("solver", "solver_version", "settings_id", "reviewer"):
            if not _named(solve.get(key)):
                errors.append(f"solve.{key} is required")
        if solve.get("source_sha256") != source_hash:
            errors.append("solve is not hash-bound to source file")
        if media and (solve.get("frame_count") != media.get("frame_count") or solve.get("time_base") != media.get("time_base")):
            errors.append("solve frame count/time base do not match source media")
    if solve_path and solve_path in ingest_paths:
        errors.append("ingested and solved outputs must be distinct files")
    if approvals:
        for key in ("rights_reviewer", "technical_reviewer"):
            if not _named(approvals.get(key)):
                errors.append(f"approvals.{key} must name a reviewer")
        if approvals.get("approved_for_candidate_authoring") is not True:
            errors.append("approvals.approved_for_candidate_authoring must be exactly true")
    if top.get("status") != "PASS_AUTHENTIC_INGEST_V008_CANDIDATE_ONLY":
        errors.append("status must be the exact candidate-only pass status")
    return errors


def _fixture(root: Path) -> dict[str, Any]:
    files = {"source.mov": b"source-video", "ingested.mov": b"capture-data", "ingested2.mov": b"image-sequence", "ingested3.mov": b"sound-wave", "rights.txt": b"rights", "solve.anim": b"animation", "solve.json": b"solve-receipt"}
    for name, payload in files.items():
        (root / name).write_bytes(payload)
    rec = lambda name: {"path": name, "bytes": len(files[name]), "sha256": hashlib.sha256(files[name]).hexdigest().upper()}
    source_hash = rec("source.mov")["sha256"]
    common = {"derived_from_source_sha256": source_hash, "import_tool": "Live Link Hub Capture Manager", "import_settings_id": "MonoVideo_v1"}
    ingested = [dict(common, asset_id="CD_AUTH_001", role="CaptureData", file=rec("ingested.mov")), dict(common, asset_id="IS_AUTH_001", role="ImageSequence", file=rec("ingested2.mov")), dict(common, asset_id="SW_AUTH_001", role="SoundWave", file=rec("ingested3.mov"))]
    return {"schema": SCHEMA, "candidate_revision": REVISION, "source_identity": {"source_id": "DG_AUTH_001", "take_id": "TAKE_001", "performer_name": "Test Performer", "capture_date": "2026-08-30", "source_file": rec("source.mov")}, "source_media": {"codec": "HEVC", "width_pixels": 1920, "height_pixels": 1080, "frame_rate_fps": 30.0, "frame_count": 120, "time_base": "1/600", "duration_seconds": 4.0, "constant_frame_rate": True}, "ingested_asset": ingested, "rights": {"ownership_or_license": "PROJECT_OWNED_PERFORMER_CAPTURE", "derivative_animation_allowed": True, "game_distribution_allowed": True, "performer_release_verified": True, "evidence_file": rec("rights.txt")}, "solve": {"completed": True, "solver": "MetaHuman Animator", "solver_version": "UE_5.8", "settings_id": "BodySolve_v1", "output_file": rec("solve.anim"), "source_sha256": source_hash, "frame_count": 120, "time_base": "1/600", "quality_review_passed": True, "reviewer": "Technical Reviewer", "receipt_file": rec("solve.json")}, "approvals": {"rights_reviewer": "Rights Reviewer", "technical_reviewer": "Technical Reviewer", "approved_for_candidate_authoring": True}, "status": "PASS_AUTHENTIC_INGEST_V008_CANDIDATE_ONLY"}


def self_test() -> int:
    failures: list[str] = []
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        valid = _fixture(root)
        if validate(valid, root): failures.append("valid fixture")
        cases = [
            ("rights", lambda x: x["rights"].__setitem__("game_distribution_allowed", False)),
            ("release", lambda x: x["rights"].__setitem__("performer_release_verified", False)),
            ("solve", lambda x: x["solve"].__setitem__("completed", False)),
            ("review", lambda x: x["solve"].__setitem__("quality_review_passed", False)),
            ("source hash", lambda x: x["source_identity"]["source_file"].__setitem__("sha256", "0" * 64)),
            ("binding", lambda x: x["ingested_asset"][0].__setitem__("derived_from_source_sha256", "0" * 64)),
            ("frames", lambda x: x["solve"].__setitem__("frame_count", 119)),
            ("timebase", lambda x: x["solve"].__setitem__("time_base", "1/30")),
            ("approval", lambda x: x["approvals"].__setitem__("approved_for_candidate_authoring", False)),
            ("extra", lambda x: x.__setitem__("unexpected", True)),
        ]
        for name, mutate in cases:
            candidate = copy.deepcopy(valid); mutate(candidate)
            if not validate(candidate, root): failures.append(name)
    if failures:
        print(f"FAIL: authentic-ingest v008 self-test: {failures}")
        return 1
    print(f"PASS: {len(cases) + 1} authentic-ingest v008 self-tests")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", nargs="?", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if args.receipt is None:
        parser.error("receipt is required unless --self-test is used")
    try:
        data = json.loads(args.receipt.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        print(f"FAIL: cannot read receipt: {exc}")
        return 1
    errors = validate(data)
    if errors:
        print("FAIL: authentic-ingest v008 receipt")
        for error in errors: print(f"- {error}")
        return 1
    print("PASS_AUTHENTIC_INGEST_V008_CANDIDATE_ONLY")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
