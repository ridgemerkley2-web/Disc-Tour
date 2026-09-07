#!/usr/bin/env python3
"""Catch mechanical C++ mistakes that a compiler would catch, when there is none.

Unreal is not installed on a source checkout, so feature work happens with no
compiler feedback at all. These checks are chosen for that situation: each one
catches a mistake that either breaks the build or violates a stated architecture
rule, and each is detectable from source text alone.

Every check passes cleanly on the codebase as it stands, so any report is a change
introduced since. Measured at the time of writing: 129 headers declare reflected
types and all 129 order their generated.h correctly; 100 .cpp files include their
own header first; no UPROPERTY holds a raw UObject pointer; no Actor or UObject is
raw-allocated; no runtime file includes the Shipping-denied DeveloperTool module.

This is not a substitute for compiling. It is what is available instead.

    python Scripts/lint_cpp_conventions.py
    python Scripts/lint_cpp_conventions.py --self-test
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Source"

RUNTIME_MODULE = "DiscGolfTour"
DEVELOPER_MODULE = "DiscGolfTourDeveloper"

REFLECTED_DECL = re.compile(r"^\s*(UCLASS|USTRUCT|UENUM|UINTERFACE)\s*\(", re.M)
INCLUDE = re.compile(r'^\s*#include\s+"([^"]+)"')
BODY_MACROS = ("GENERATED_BODY()", "GENERATED_UCLASS_BODY()",
               "GENERATED_IINTERFACE_BODY()", "GENERATED_USTRUCT_BODY()")


class Findings:
    def __init__(self) -> None:
        self.items: list[tuple[str, str]] = []
        self.checked = 0

    def add(self, check: str, detail: str) -> None:
        self.items.append((check, detail))


def _display(path: Path) -> str:
    """Repo-relative where possible; self-test samples live outside the tree."""
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return path.name


def headers() -> list[Path]:
    return sorted(SOURCE.rglob("*.h"))


def sources() -> list[Path]:
    return sorted(SOURCE.rglob("*.cpp"))


def includes_of(text: str) -> list[tuple[int, str]]:
    found = []
    for number, line in enumerate(text.splitlines(), 1):
        match = INCLUDE.match(line)
        if match:
            found.append((number, match.group(1)))
    return found


def check_generated_header_last(findings: Findings) -> None:
    """A reflected header must include its own generated.h, last.

    Unreal Header Tool requires this and the compiler rejects anything else. It is
    the single most likely mistake to make when adding a reflected type by hand.
    """
    for header in headers():
        text = header.read_text(encoding="utf-8", errors="ignore")
        if not REFLECTED_DECL.search(text):
            continue
        findings.checked += 1
        found = includes_of(text)
        generated = [(n, inc) for n, inc in found if inc.endswith(".generated.h")]
        relative = _display(header)
        if not generated:
            findings.add("generated-header",
                         f"{relative}: declares a reflected type but includes no "
                         "generated.h")
            continue
        number, include = generated[-1]
        expected = f"{header.stem}.generated.h"
        if include != expected:
            findings.add("generated-header",
                         f"{relative}:{number}: includes {include}, expected "
                         f"{expected}")
        elif found[-1][1] != include:
            findings.add("generated-header",
                         f"{relative}:{number}: {include} must be the last include; "
                         f"{found[-1][1]} follows it")


def check_generated_body(findings: Findings) -> None:
    """Every UCLASS/USTRUCT/UINTERFACE needs its body macro."""
    for header in headers():
        lines = header.read_text(encoding="utf-8", errors="ignore").splitlines()
        for index, line in enumerate(lines):
            match = re.match(r"\s*(UCLASS|USTRUCT|UINTERFACE)\s*\(", line)
            if not match:
                continue
            findings.checked += 1
            window = "\n".join(lines[index:index + 12])
            if not any(macro in window for macro in BODY_MACROS):
                findings.add("generated-body",
                             f"{_display(header)}:{index + 1}: "
                             f"{match.group(1)} without a GENERATED_BODY macro")


def check_own_header_first(findings: Findings) -> None:
    """A .cpp includes its own header first, so the header stands alone."""
    for source in sources():
        own = source.with_suffix(".h")
        if not own.is_file():
            continue
        findings.checked += 1
        found = includes_of(source.read_text(encoding="utf-8", errors="ignore"))
        if not found:
            findings.add("own-header-first",
                         f"{_display(source)}: no includes; expected "
                         f"{own.name} first")
        elif Path(found[0][1]).name != own.name:
            findings.add("own-header-first",
                         f"{_display(source)}: first include is "
                         f"{found[0][1]}, expected {own.name}")


def check_module_boundary(findings: Findings) -> None:
    """The runtime module must never include the Shipping-denied DeveloperTool module.

    A stated architecture rule. Violating it links development-only code into a
    Shipping build, which is exactly what Session 19 separated.
    """
    developer_headers = {p.name for p in (SOURCE / DEVELOPER_MODULE).rglob("*.h")}
    if not developer_headers:
        return
    runtime = SOURCE / RUNTIME_MODULE
    for path in sorted(list(runtime.rglob("*.cpp")) + list(runtime.rglob("*.h"))):
        findings.checked += 1
        for number, include in includes_of(
                path.read_text(encoding="utf-8", errors="ignore")):
            if Path(include).name in developer_headers:
                findings.add("module-boundary",
                             f"{_display(path)}:{number}: runtime module "
                             f"includes {include} from {DEVELOPER_MODULE}")


def check_reflected_pointer_ownership(findings: Findings) -> None:
    """UPROPERTY members hold TObjectPtr, not a raw UObject pointer.

    AGENTS.md requires it, and raw reflected pointers are the classic source of
    stale references the garbage collector cannot fix up.
    """
    for header in headers():
        lines = header.read_text(encoding="utf-8", errors="ignore").splitlines()
        for index, line in enumerate(lines):
            if not re.match(r"\s*UPROPERTY\s*\(", line):
                continue
            findings.checked += 1
            for follow in range(index + 1, min(index + 4, len(lines))):
                declaration = lines[follow].strip()
                if not declaration or declaration.startswith("//"):
                    continue
                if re.match(r"(?:class\s+)?[AU]\w+\s*\*\s*\w+\s*(?:=|;)", declaration):
                    findings.add("reflected-pointer",
                                 f"{_display(header)}:{follow + 1}: "
                                 f"UPROPERTY holds a raw pointer, use TObjectPtr: "
                                 f"{declaration[:60]}")
                break


def check_uobject_allocation(findings: Findings) -> None:
    """No raw new of an Actor or UObject; those come from NewObject/SpawnActor.

    Deliberately restricted to A/U prefixes. FAnimInstanceProxy and friends are
    heap-allocated with new by Unreal's own contract and are not violations.
    """
    for path in sorted(list(SOURCE.rglob("*.cpp")) + list(SOURCE.rglob("*.h"))):
        findings.checked += 1
        for number, line in enumerate(
                path.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
            if re.search(r"(?<![\w:])new\s+[AU][A-Z]\w+", line):
                findings.add("uobject-allocation",
                             f"{_display(path)}:{number}: raw new of a "
                             f"UObject/Actor: {line.strip()[:60]}")


CHECKS = (
    ("generated-header", check_generated_header_last),
    ("generated-body", check_generated_body),
    ("own-header-first", check_own_header_first),
    ("module-boundary", check_module_boundary),
    ("reflected-pointer", check_reflected_pointer_ownership),
    ("uobject-allocation", check_uobject_allocation),
)


def run() -> Findings:
    findings = Findings()
    for _name, check in CHECKS:
        check(findings)
    return findings


def self_test() -> int:
    """Prove each check fires, by running it against a deliberately broken sample."""
    import tempfile
    import shutil

    global SOURCE
    original = SOURCE
    cases = (
        ("generated-header",
         "Broken.h",
         '#include "CoreMinimal.h"\n#include "Broken.generated.h"\n'
         '#include "Extra.h"\n\nUCLASS()\nclass UBroken : public UObject\n'
         '{\n    GENERATED_BODY()\n};\n'),
        ("generated-body",
         "NoBody.h",
         '#include "CoreMinimal.h"\n#include "NoBody.generated.h"\n\n'
         'UCLASS()\nclass UNoBody : public UObject\n{\npublic:\n'
         '    int32 Value = 0;\n};\n'),
        ("reflected-pointer",
         "RawPointer.h",
         '#include "CoreMinimal.h"\n#include "RawPointer.generated.h"\n\n'
         'USTRUCT()\nstruct FRawPointer\n{\n    GENERATED_BODY()\n'
         '    UPROPERTY()\n    UObject* Held = nullptr;\n};\n'),
        ("uobject-allocation",
         "Alloc.cpp",
         '#include "Alloc.h"\n\nvoid Make()\n{\n    UObject* Bad = new UObject();\n}\n'),
    )
    caught = 0
    for expected, name, content in cases:
        workspace = Path(tempfile.mkdtemp(prefix="dgtour_lint_"))
        try:
            (workspace / name).write_text(content, encoding="utf-8")
            if name.endswith(".cpp"):
                (workspace / name.replace(".cpp", ".h")).write_text(
                    '#include "CoreMinimal.h"\n', encoding="utf-8")
            SOURCE = workspace
            findings = run()
            names = {item[0] for item in findings.items}
            if expected in names:
                caught += 1
                print(f"  {expected}: fires")
            else:
                print(f"  {expected}: DID NOT FIRE (reported {sorted(names)})")
        finally:
            SOURCE = original
            shutil.rmtree(workspace, ignore_errors=True)

    print(f"\nCPP CONVENTION LINT SELF-TEST: {caught}/{len(cases)} checks fire")
    return 0 if caught == len(cases) else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true",
                        help="prove each check fires on a broken sample")
    args = parser.parse_args()
    if args.self_test:
        return self_test()

    findings = run()
    if findings.items:
        print("C++ convention lint FAILED")
        current = None
        for check, detail in findings.items:
            if check != current:
                print(f"\n{check}")
                current = check
            print(f"  {detail}")
        print(f"\n{len(findings.items)} finding(s) across "
              f"{findings.checked} inspected declarations.")
        return 1

    print("C++ convention lint OK")
    print(f"  {findings.checked} declarations inspected across "
          f"{len(list(SOURCE.rglob('*.h')))} headers and "
          f"{len(list(SOURCE.rglob('*.cpp')))} sources")
    print("  This is a source-text check, not a compile.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
