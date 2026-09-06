#!/usr/bin/env python3
"""Fail-closed v008 Unreal extraction -> adapter -> biomechanics lane."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

import adapt_dg_v008_unreal_telemetry as adapter
import validate_dg_v008_world_biomechanics as validator


ROOT = Path(__file__).absolute().parents[1]
EXPECTED_ASSET_PATH = "/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED"
EXPECTED_SOCKETS = {"DG_HeelContact_L": "foot_l", "DG_ToeContact_L": "ball_l", "DG_HeelContact_R": "foot_r", "DG_ToeContact_R": "ball_r"}
EXPECTED_CURVES = ["DG_COM_X_CM", "DG_COM_Y_CM", "DG_COM_Z_CM", "DG_SupportPolygonMarginCm"]
ASSET_FILE = ROOT / "Content/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED.uasset"
CONTRACT = ROOT / "SourceArt/DiscGolf/Motion/V008PostRetargetTelemetryContract.json"
TEMPLATE = ROOT / "SourceArt/DiscGolf/Motion/V008PostRetargetTelemetryContract.template.json"
EXTRACTOR = ROOT / "Scripts/extract_dg_v008_post_retarget_unreal.py"
DEFAULT_EDITOR = Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")


def validate_contract(payload):
    failures = []
    if not isinstance(payload, dict) or payload.get("schema") != "DiscGolfTour.V008TelemetryExtractionContract.v1": failures.append("contract schema differs")
    if isinstance(payload, dict) and payload.get("asset_path") != EXPECTED_ASSET_PATH: failures.append("contract asset_path is not the exact accepted normalized derivative")
    if isinstance(payload, dict) and payload.get("sample_rate_hz") != 60: failures.append("contract sample_rate_hz must equal 60")
    if isinstance(payload, dict) and payload.get("required_sockets") != EXPECTED_SOCKETS: failures.append("contract required_sockets differ")
    if isinstance(payload, dict) and payload.get("required_curves") != EXPECTED_CURVES: failures.append("contract required_curves differ")
    phases = payload.get("phases") if isinstance(payload, dict) else None
    names = ("reachback", "plant", "pocket", "release", "followthrough", "recovery", "settle")
    if not isinstance(phases, dict) or set(phases) != set(names): failures.append("contract phase set differs")
    else:
        values = [phases[name] for name in names]
        if any(isinstance(v, bool) or not isinstance(v, int) or v < 0 for v in values) or values != sorted(values) or len(set(values)) != len(values): failures.append("contract phases must be unique ordered nonnegative frames")
    review = payload.get("review") if isinstance(payload, dict) else None
    if not isinstance(review, dict) or review.get("status") != "APPROVED_FOR_TECHNICAL_DIAGNOSTIC_EXTRACTION": failures.append("contract review status is not approved")
    else:
        reviewer = review.get("reviewer")
        if not isinstance(reviewer, str) or reviewer.strip().upper() in ("", "UNASSIGNED", "REPLACE"): failures.append("contract reviewer is not named")
        timestamp = review.get("reviewed_at_utc")
        if not isinstance(timestamp, str) or not timestamp.endswith("Z"): failures.append("contract reviewed_at_utc is absent")
        if review.get("phase_basis") != "MANUALLY_REVIEWED_DGMASTER_NORMALIZED_FRAME_INDICES": failures.append("contract phase basis differs")
    return failures


def preflight(asset=ASSET_FILE, contract=CONTRACT, editor=DEFAULT_EDITOR):
    blockers = []
    if not asset.is_file(): blockers.append(f"BLOCKED_EXPECTED_DGMASTER_NORMALIZED_ASSET_MISSING: {asset}")
    if not contract.is_file(): blockers.append(f"BLOCKED_EXTRACTION_CONTRACT_MISSING: {contract} (copy/review {TEMPLATE})")
    else:
        try: contract_payload = json.loads(contract.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as exc: blockers.append(f"BLOCKED_EXTRACTION_CONTRACT_UNREADABLE: {exc}")
        else: blockers.extend(f"BLOCKED_EXTRACTION_CONTRACT_INVALID: {item}" for item in validate_contract(contract_payload))
    if not editor.is_file(): blockers.append(f"BLOCKED_UNREAL_EDITOR_CMD_MISSING: {editor}")
    return blockers


def process_export(source):
    converted = adapter.adapt(source)
    report = validator.validate(converted)
    return converted, report


def self_test():
    converted, report = process_export(adapter._source_fixture())
    checks = [report["status"] == "PASS", converted["schema"] == validator.SCHEMA]
    with tempfile.TemporaryDirectory() as folder:
        base = Path(folder)
        blockers = preflight(base / "missing.uasset", base / "missing.json", base / "missing.exe")
        checks.extend([len(blockers) == 3, blockers[0].startswith("BLOCKED_EXPECTED_DGMASTER_NORMALIZED_ASSET_MISSING")])
        asset, editor, contract = base / "asset.uasset", base / "editor.exe", base / "contract.json"
        asset.touch(); editor.touch()
        approved = {
            "schema": "DiscGolfTour.V008TelemetryExtractionContract.v1", "asset_path": EXPECTED_ASSET_PATH, "sample_rate_hz": 60,
            "phases": {"reachback": 44, "plant": 64, "pocket": 79, "release": 84, "followthrough": 94, "recovery": 118, "settle": 132},
            "required_sockets": EXPECTED_SOCKETS, "required_curves": EXPECTED_CURVES,
            "review": {"status": "APPROVED_FOR_TECHNICAL_DIAGNOSTIC_EXTRACTION", "reviewer": "Test Reviewer", "reviewed_at_utc": "2026-08-31T00:00:00Z", "phase_basis": "MANUALLY_REVIEWED_DGMASTER_NORMALIZED_FRAME_INDICES"}}
        contract.write_text(json.dumps(approved), encoding="utf-8")
        checks.append(preflight(asset, contract, editor) == [])
        approved["review"]["status"] = "TEMPLATE_NOT_REVIEWED"; contract.write_text(json.dumps(approved), encoding="utf-8")
        checks.append(any("review status" in item for item in preflight(asset, contract, editor)))
    malformed = adapter._source_fixture()
    del malformed["frames"][0]["transforms"]["hand_r"]
    try:
        process_export(malformed)
    except adapter.ContractError:
        checks.append(True)
    else:
        checks.append(False)
    return sum(checks), len(checks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--preflight", action="store_true")
    parser.add_argument("--unreal-export", type=Path, help="validate an existing extractor output without launching Unreal")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "Saved/AnimationFluidity/V008PostRetargetTelemetry")
    parser.add_argument("--editor", type=Path, default=DEFAULT_EDITOR)
    args = parser.parse_args()
    if args.self_test:
        passed, total = self_test(); print(f"SELF_TEST {passed}/{total}"); return 0 if passed == total else 1
    if args.preflight:
        blockers = preflight(editor=args.editor)
        print(json.dumps({"status": "PASS_PREFLIGHT" if not blockers else "BLOCKED", "blockers": blockers}, indent=2))
        return 0 if not blockers else 1
    args.output_dir.mkdir(parents=True, exist_ok=True)
    unreal_export = args.unreal_export or args.output_dir / "v008_post_retarget_unreal.json"
    if args.unreal_export is None:
        blockers = preflight(editor=args.editor)
        if blockers:
            print(json.dumps({"status": "BLOCKED", "blockers": blockers}, indent=2)); return 1
        env = os.environ.copy(); env["DG_V008_EXTRACTION_CONTRACT"] = str(CONTRACT); env["DG_V008_UNREAL_EXPORT"] = str(unreal_export)
        command = [str(args.editor), str(ROOT / "DiscGolfTour.uproject"), "-unattended", "-nop4", "-nosplash", "-ExecutePythonScript=" + str(EXTRACTOR)]
        result = subprocess.run(command, cwd=ROOT, env=env, check=False)
        if result.returncode != 0 or not unreal_export.is_file():
            print(json.dumps({"status": "FAIL", "blockers": [f"UNREAL_EXTRACTION_FAILED exit={result.returncode}"]}, indent=2)); return 1
    try:
        source = json.loads(unreal_export.read_text(encoding="utf-8"))
        converted, report = process_export(source)
    except (OSError, UnicodeError, json.JSONDecodeError, adapter.ContractError) as exc:
        print(json.dumps({"status": "FAIL", "blockers": [str(exc)]}, indent=2)); return 1
    (args.output_dir / "v008_world_biomechanics_input.json").write_text(json.dumps(converted, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (args.output_dir / "v008_world_biomechanics_report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
