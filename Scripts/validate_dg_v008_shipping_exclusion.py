#!/usr/bin/env python3
"""Fail-closed Shipping boundary for authentic v008 diagnostic assets."""
from pathlib import Path
import argparse

ROOT = Path(__file__).resolve().parents[1]
LINE = '+DirectoriesToNeverCook=(Path="/Game/DiscGolf/Animation/Authentic/v008/Diagnostic")'
OUTPUT = ROOT / "Content/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_RAW.uasset"

def validate(text: str) -> list[str]:
    errors = []
    if text.count(LINE) != 1: errors.append("diagnostic NeverCook root must occur exactly once")
    if '+DirectoriesToAlwaysCook=(Path="/Game/DiscGolf/Animation/Authentic' in text:
        errors.append("authentic diagnostic hierarchy must not be AlwaysCook")
    if not OUTPUT.is_file(): errors.append("exact append-only RAW output is missing")
    return errors

def self_test(text: str) -> None:
    cases = [("pass", text, False), ("missing", text.replace(LINE, ""), True),
             ("duplicate", text + "\n" + LINE, True),
             ("conflict", text + '\n+DirectoriesToAlwaysCook=(Path="/Game/DiscGolf/Animation/Authentic")', True)]
    for name, value, should_fail in cases:
        if bool(validate(value)) != should_fail: raise RuntimeError(f"self-test failed: {name}")

def main() -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(); text = (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    if args.self_test: self_test(text)
    errors = validate(text)
    print("PASS_V008_DIAGNOSTIC_SHIPPING_EXCLUDED" if not errors else "FAIL: " + "; ".join(errors))
    return 1 if errors else 0

if __name__ == "__main__": raise SystemExit(main())
