#!/usr/bin/env python3
"""Create an immutable candidate-bound v0.5 equipment policy from two fresh logs."""

from __future__ import annotations

import argparse
import copy
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile

import validate_dg_session19_v05_equipment_shipping as equipment


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TEMPLATE = ROOT / "Config/DG_Session19V05EquipmentShippingTechnicalPolicy.json"


def candidate_not_before(candidate_id: str) -> datetime:
    return equipment.candidate_build_utc(candidate_id)


def bind_log(
    path: Path, candidate_id: str, not_before: datetime,
) -> tuple[dict[str, object], str]:
    resolved = path.resolve(strict=True)
    data, observed_stat = equipment.read_stable_regular_file(resolved)
    if not equipment.candidate_log_name_matches(resolved.name, candidate_id):
        raise ValueError(f"automation log filename is not candidate-scoped: {resolved.name}")
    if observed_stat.st_mtime < not_before.timestamp():
        raise ValueError(f"automation log predates candidate build: {resolved.name}")
    return (
        {
            "fileName": resolved.name,
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest().upper(),
        },
        data.decode("utf-8-sig", errors="replace"),
    )


def build_policy(template: dict, candidate_id: str, logs: list[Path]) -> dict:
    if len(logs) != 2:
        raise ValueError("exactly two --automation-log arguments are required")
    not_before = candidate_not_before(candidate_id)
    snapshots = [bind_log(path, candidate_id, not_before) for path in logs]
    bindings = [binding for binding, _ in snapshots]
    if len({item["fileName"].casefold() for item in bindings}) != 2:
        raise ValueError("automation log filenames must be distinct")
    if len({item["sha256"] for item in bindings}) != 2:
        raise ValueError("automation log hashes must be distinct")
    for path, (_, text) in zip(logs, snapshots):
        _, problems = equipment.parse_automation_text(
            text,
            template["automation"]["requiredTests"] + equipment.POST_FIX_REQUIRED_TESTS,
            template["automation"]["minimumSuccessCountPerLog"],
        )
        if problems:
            raise ValueError(f"automation log is not a current passing full suite: {path.name}: {', '.join(problems)}")
    policy = copy.deepcopy(template)
    policy["candidateId"] = candidate_id
    policy["automation"]["candidateLogsNotBeforeUtc"] = not_before.isoformat().replace("+00:00", "Z")
    policy["automation"]["candidateFreshnessRequiredTests"] = equipment.POST_FIX_REQUIRED_TESTS
    policy["automation"]["candidateLogBindings"] = bindings
    problems = equipment.validate_policy(policy, require_candidate_bindings=True)
    if problems:
        raise ValueError("generated policy is invalid: " + ", ".join(problems))
    return policy


def write_immutable(path: Path, policy: dict) -> None:
    if path.exists() or path.is_symlink():
        raise FileExistsError(f"refusing to overwrite immutable output: {path}")
    payload = (json.dumps(policy, indent=2, sort_keys=True) + "\n").encode("utf-8")
    descriptor, temporary_name = tempfile.mkstemp(prefix=".dgt-equipment-policy-", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.link(temporary_name, path)
    finally:
        Path(temporary_name).unlink(missing_ok=True)


def run_self_test() -> None:
    template = equipment.load_json(DEFAULT_TEMPLATE)
    candidate = "S19_WindowsShipping_20000101T000000Z_000000000001"
    with tempfile.TemporaryDirectory(prefix="dgt-equipment-policy-selftest-") as name:
        root = Path(name)
        paths = [root / f"Automation_{candidate}_{suffix}.log" for suffix in ("A", "B")]
        for index, path in enumerate(paths):
            path.write_text(
                equipment.make_automation_fixture(
                    template["automation"]["requiredTests"] + equipment.POST_FIX_REQUIRED_TESTS
                ) + f"\nFixture.{index}", encoding="utf-8"
            )
        policy = build_policy(template, candidate, paths)
        assert not equipment.validate_policy(policy, require_candidate_bindings=True)
        try:
            build_policy(template, candidate, [paths[0], paths[0]])
        except ValueError:
            pass
        else:
            raise AssertionError("same log was accepted twice")
        predecessor = root / "Automation_S19_WindowsShipping_PREDECESSOR_A.log"
        predecessor.write_bytes(paths[0].read_bytes())
        try:
            build_policy(template, candidate, [predecessor, paths[1]])
        except ValueError:
            pass
        else:
            raise AssertionError("predecessor filename was accepted")
        try:
            build_policy(template, candidate, [root / f"Automation_{candidate}_missing.log", paths[1]])
        except (OSError, ValueError):
            pass
        else:
            raise AssertionError("missing log was accepted")
        future_candidate = "S19_WindowsShipping_20990101T000000Z_000000000002"
        future_paths = [root / f"Automation_{future_candidate}_{suffix}.log" for suffix in ("A", "B")]
        for index, path in enumerate(future_paths):
            path.write_bytes(paths[index].read_bytes())
        try:
            build_policy(template, future_candidate, future_paths)
        except ValueError:
            pass
        else:
            raise AssertionError("stale logs were accepted")
        legacy_paths = [root / f"Automation_{candidate}_legacy_{suffix}.log" for suffix in ("A", "B")]
        for index, path in enumerate(legacy_paths):
            path.write_text(
                equipment.make_automation_fixture(template["automation"]["requiredTests"])
                + f"\nLegacy.{index}", encoding="utf-8"
            )
        try:
            build_policy(template, candidate, legacy_paths)
        except ValueError:
            pass
        else:
            raise AssertionError("pre-fix full-suite logs were accepted")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-id")
    parser.add_argument("--automation-log", action="append", type=Path, default=[])
    parser.add_argument("--template", type=Path, default=DEFAULT_TEMPLATE)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            run_self_test()
            print("SELF_TEST PASS")
            return 0
        if not args.candidate_id or args.output is None:
            raise ValueError("--candidate-id and --output are required")
        template = equipment.load_json(args.template)
        policy = build_policy(template, args.candidate_id, args.automation_log)
        write_immutable(args.output, policy)
        print(f"PASS output={args.output}")
        return 0
    except (OSError, ValueError, AssertionError) as exc:
        print(f"FAIL: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
