#!/usr/bin/env python3
"""Catch mechanical C++ mistakes that a compiler would catch, when there is none.

Unreal is not installed on a source checkout, so feature work happens with no
compiler feedback at all. Every check here catches a mistake that breaks the build
or violates a stated architecture rule, and each is detectable from source text.

Detection runs against preprocessed views of each file rather than raw text, which
matters more than it sounds: a macro name inside a comment or inside a TEXT("...")
literal is not a declaration, and an earlier version of this linter that scanned
raw text could be fooled in both directions. `_blank_comments` produces two
offset-preserving views -- one with comments blanked and literals intact (include
paths live in literals), one with both blanked (declarations live in neither).
Newlines survive in both, so reported line numbers stay exact.

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

INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*[<"]([^">]+)[">]', re.M)
REFLECTED_DECL = re.compile(
    r"^[ \t]*(UCLASS|USTRUCT|UENUM|UINTERFACE|UPROPERTY|UFUNCTION|UDELEGATE)[ \t]*\(",
    re.M)
REFLECTED_TYPE = re.compile(r"\b(UCLASS|USTRUCT|UINTERFACE)\s*\(")
BODY_MACRO = re.compile(
    r"\bGENERATED_(BODY|UCLASS_BODY|USTRUCT_BODY|IINTERFACE_BODY|UINTERFACE_BODY)\s*\(")
CPP_REFLECTION = re.compile(r"\b(UCLASS|USTRUCT|UENUM|UINTERFACE)\s*\(")


def _blank(text: str) -> tuple[str, str]:
    """Return (comments blanked, comments and literal interiors blanked).

    Offsets and line numbers are preserved: blanked characters become spaces and
    newlines are kept, so a match position in either view maps back to the source.
    """
    no_comments = list(text)
    no_literals = list(text)
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        nxt = text[index + 1] if index + 1 < length else ""
        if char == "/" and nxt == "/":
            while index < length and text[index] != "\n":
                no_comments[index] = no_literals[index] = " "
                index += 1
        elif char == "/" and nxt == "*":
            while index < length and not (text[index] == "*" and
                                          index + 1 < length and
                                          text[index + 1] == "/"):
                if text[index] != "\n":
                    no_comments[index] = no_literals[index] = " "
                index += 1
            for _ in range(2):
                if index < length:
                    no_comments[index] = no_literals[index] = " "
                    index += 1
        elif char in "\"'":
            quote = char
            index += 1
            while index < length and text[index] != quote:
                if text[index] == "\\":
                    if text[index] != "\n":
                        no_literals[index] = " "
                    index += 1
                if index < length:
                    if text[index] != "\n":
                        no_literals[index] = " "
                    index += 1
            index += 1
        else:
            index += 1
    return "".join(no_comments), "".join(no_literals)


def _line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def _match_parens(text: str, open_index: int) -> int:
    """Index just past the paren group starting at open_index, or -1."""
    depth = 0
    for index in range(open_index, len(text)):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return index + 1
    return -1


class Findings:
    def __init__(self) -> None:
        self.items: list[tuple[str, str]] = []
        self.checked = 0

    def add(self, check: str, detail: str) -> None:
        self.items.append((check, detail))


def _display(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return path.name


def headers() -> list[Path]:
    return sorted(SOURCE.rglob("*.h"))


def sources() -> list[Path]:
    return sorted(SOURCE.rglob("*.cpp"))


def check_generated_header(findings: Findings) -> None:
    """A reflected header includes exactly one generated.h, named for it, last.

    Unreal Header Tool requires this and the compiler rejects anything else. A
    header declaring nothing reflected must not include one at all, because UHT
    never emits that file and the include cannot resolve.

    Known limitation, deliberately not fixed: no preprocessor evaluation, so an
    include inside a block UHT skips, placed after the generated.h, will report.
    The remedy is the same one-line move, so it is worth living with.
    """
    for header in headers():
        text = header.read_text(encoding="utf-8", errors="ignore")
        no_comments, no_literals = _blank(text)
        includes = [(m.group(1), _line_of(text, m.start()))
                    for m in INCLUDE.finditer(no_comments)]
        generated = [(inc, line) for inc, line in includes
                     if inc.endswith(".generated.h")]
        reflected = REFLECTED_DECL.search(no_literals) is not None
        name = _display(header)
        if not reflected:
            for inc, line in generated:
                findings.add("generated-header",
                             f"{name}:{line}: includes {inc} but declares nothing "
                             "reflected; UHT emits no such file")
            continue
        findings.checked += 1
        expected = f"{header.stem}.generated.h"
        if not generated:
            findings.add("generated-header",
                         f"{name}: declares a reflected type but includes no "
                         f"{expected}")
            continue
        for inc, line in generated[:-1]:
            findings.add("generated-header",
                         f"{name}:{line}: a second generated.h ({inc}); only "
                         f"{expected} belongs here")
        last_generated, last_line = generated[-1]
        if last_generated != expected and not last_generated.endswith("/" + expected):
            findings.add("generated-header",
                         f"{name}:{last_line}: includes {last_generated}, expected "
                         f"{expected}")
        elif includes[-1][0] != last_generated:
            findings.add("generated-header",
                         f"{name}:{includes[-1][1]}: {includes[-1][0]} follows the "
                         f"generated.h; move it above")


def check_reflection_macro_in_cpp(findings: Findings) -> None:
    """UHT parses headers only, so a reflected declaration in a .cpp cannot link."""
    for source in sources():
        text = source.read_text(encoding="utf-8", errors="ignore")
        _, no_literals = _blank(text)
        findings.checked += 1
        for match in CPP_REFLECTION.finditer(no_literals):
            findings.add("reflection-in-cpp",
                         f"{_display(source)}:{_line_of(text, match.start())}: "
                         f"{match.group(1)} in a .cpp; UHT parses headers only, so "
                         "the paired GENERATED_BODY has no definition")


def _reflected_bodies(no_literals: str):
    """Yield (kind, open_brace, close_brace) for each reflected type body."""
    for match in REFLECTED_TYPE.finditer(no_literals):
        start = match.start()
        if start > 0 and (no_literals[start - 1] == "_"
                          or no_literals[start - 1].isalnum()):
            continue
        after = _match_parens(no_literals, match.end() - 1)
        if after < 0:
            continue
        brace = no_literals.find("{", after)
        semicolon = no_literals.find(";", after)
        if brace < 0 or (0 <= semicolon < brace):
            continue
        depth = 0
        end = -1
        for index in range(brace, len(no_literals)):
            if no_literals[index] == "{":
                depth += 1
            elif no_literals[index] == "}":
                depth -= 1
                if depth == 0:
                    end = index
                    break
        if end < 0:
            continue
        yield match.group(1), start, brace, end


def check_generated_body(findings: Findings) -> None:
    """Exactly one GENERATED_*_BODY per reflected type, at its own brace depth.

    The depth restriction is what makes this work: a nested plain struct's body
    macro would otherwise be counted for the outer type, so an outer class missing
    its own macro reads as correct and the defect stays invisible.
    """
    for path in headers() + sources():
        text = path.read_text(encoding="utf-8", errors="ignore")
        _, no_literals = _blank(text)
        for kind, start, brace, end in _reflected_bodies(no_literals):
            findings.checked += 1
            count = 0
            depth = 0
            for index in range(brace, end):
                if no_literals[index] == "{":
                    depth += 1
                elif no_literals[index] == "}":
                    depth -= 1
                elif depth == 1 and no_literals.startswith("GENERATED_", index):
                    if BODY_MACRO.match(no_literals, index):
                        count += 1
            if count != 1:
                findings.add("generated-body",
                             f"{_display(path)}:{_line_of(text, start)}: {kind} has "
                             f"{count} GENERATED_*_BODY macros at its own scope, "
                             "expected exactly 1")


def check_uenum_blueprint_type(findings: Findings) -> None:
    """A Blueprint-exposed enum class must declare ': uint8'.

    UHT requires the uint8 underlying type; ': int32' is a build break. Only uint8
    is accepted, because accepting any sized integer lets int32 through silently.
    """
    for path in headers():
        text = path.read_text(encoding="utf-8", errors="ignore")
        _, no_literals = _blank(text)
        for match in re.finditer(r"\bUENUM\s*\(", no_literals):
            after = _match_parens(no_literals, match.end() - 1)
            if after < 0:
                continue
            findings.checked += 1
            args = no_literals[match.end():after - 1]
            brace = no_literals.find("{", after)
            semicolon = no_literals.find(";", after)
            if brace < 0 or (0 <= semicolon < brace):
                continue
            declaration = no_literals[after:brace]
            if ("BlueprintType" in args
                    and re.search(r"\benum\s+class\b", declaration)
                    and not re.search(r":\s*uint8\b", declaration)):
                findings.add("uenum-underlying-type",
                             f"{_display(path)}:{_line_of(text, match.start())}: "
                             "UENUM(BlueprintType) enum class must declare ': uint8'")


def check_reflected_pointer_ownership(findings: Findings) -> None:
    """UPROPERTY members hold TObjectPtr, not a raw UObject pointer."""
    for header in headers():
        text = header.read_text(encoding="utf-8", errors="ignore")
        _, no_literals = _blank(text)
        lines = no_literals.splitlines()
        for index, line in enumerate(lines):
            if not re.match(r"\s*UPROPERTY\s*\(", line):
                continue
            findings.checked += 1
            for follow in range(index + 1, min(index + 4, len(lines))):
                declaration = lines[follow].strip()
                if not declaration:
                    continue
                if re.match(r"(?:class\s+)?[AU]\w+\s*\*\s*\w+\s*(?:=|;)", declaration):
                    findings.add("reflected-pointer",
                                 f"{_display(header)}:{follow + 1}: UPROPERTY holds "
                                 f"a raw pointer, use TObjectPtr: {declaration[:60]}")
                break


def check_unreflected_tobjectptr(findings: Findings) -> None:
    """A TObjectPtr member of a reflected type must carry UPROPERTY.

    Without it the collector cannot see the reference, so the object can be
    collected while the member still points at it. Scoped to bodies containing a
    GENERATED_BODY macro, which is what keeps template, static and FGCObject-tracked
    members -- all legal without UPROPERTY -- out of the population.
    """
    member = re.compile(r"^\s*TObjectPtr\s*<[^>]+>\s+\w+\s*(?:=|;)")
    for path in headers():
        text = path.read_text(encoding="utf-8", errors="ignore")
        _, no_literals = _blank(text)
        for _kind, _start, brace, end in _reflected_bodies(no_literals):
            body = no_literals[brace:end]
            body_lines = body.splitlines()
            first_line = _line_of(text, brace)
            if not any(BODY_MACRO.search(line) for line in body_lines):
                continue

            # Depth of each body line, so only the reflected type's OWN members are
            # considered. A nested plain struct is not a reflected type: its
            # TObjectPtr members cannot carry UPROPERTY and are not GC roots of this
            # class. Scanning the whole body flags them and the linter gets ignored.
            depth_of_line = []
            depth = 0
            for line in body_lines:
                depth_of_line.append(depth)
                depth += line.count("{") - line.count("}")

            for offset, line in enumerate(body_lines):
                if depth_of_line[offset] != 1 or not member.match(line):
                    continue
                findings.checked += 1
                if "UPROPERTY" in line:
                    continue
                previous = body_lines[offset - 1] if offset else ""
                if "UPROPERTY" in previous:
                    continue
                findings.add("unreflected-tobjectptr",
                             f"{_display(path)}:{first_line + offset}: TObjectPtr "
                             "member of a reflected type without UPROPERTY; the "
                             f"collector cannot see it: {line.strip()[:60]}")


def check_own_header_first(findings: Findings) -> None:
    """A .cpp includes its own header first, so the header stands alone."""
    for source in sources():
        own = source.with_suffix(".h")
        if not own.is_file():
            continue
        findings.checked += 1
        text = source.read_text(encoding="utf-8", errors="ignore")
        no_comments, _ = _blank(text)
        includes = [(m.group(1), _line_of(text, m.start()))
                    for m in INCLUDE.finditer(no_comments)]
        if not includes:
            findings.add("own-header-first",
                         f"{_display(source)}: no includes; expected {own.name} first")
        elif Path(includes[0][0]).name != own.name:
            findings.add("own-header-first",
                         f"{_display(source)}: first include is {includes[0][0]}, "
                         f"expected {own.name}")


def check_module_boundary(findings: Findings) -> None:
    """The runtime module must never include the Shipping-denied DeveloperTool module."""
    developer_headers = {p.name for p in (SOURCE / DEVELOPER_MODULE).rglob("*.h")}
    if not developer_headers:
        return
    runtime = SOURCE / RUNTIME_MODULE
    for path in sorted(list(runtime.rglob("*.cpp")) + list(runtime.rglob("*.h"))):
        findings.checked += 1
        text = path.read_text(encoding="utf-8", errors="ignore")
        no_comments, _ = _blank(text)
        for match in INCLUDE.finditer(no_comments):
            if Path(match.group(1)).name in developer_headers:
                findings.add("module-boundary",
                             f"{_display(path)}:{_line_of(text, match.start())}: "
                             f"runtime module includes {match.group(1)} from "
                             f"{DEVELOPER_MODULE}")


def check_uobject_allocation(findings: Findings) -> None:
    """No raw new of an Actor or UObject; those come from NewObject/SpawnActor.

    Restricted to A/U prefixes. FAnimInstanceProxy and friends are heap-allocated
    with new by Unreal's own contract and are not violations.
    """
    for path in headers() + sources():
        findings.checked += 1
        text = path.read_text(encoding="utf-8", errors="ignore")
        _, no_literals = _blank(text)
        for match in re.finditer(r"(?<![\w:])new\s+[AU][A-Z]\w+", no_literals):
            line = _line_of(text, match.start())
            findings.add("uobject-allocation",
                         f"{_display(path)}:{line}: raw new of a UObject/Actor: "
                         f"{text.splitlines()[line - 1].strip()[:60]}")


CHECKS = (
    check_generated_header,
    check_reflection_macro_in_cpp,
    check_generated_body,
    check_uenum_blueprint_type,
    check_reflected_pointer_ownership,
    check_unreflected_tobjectptr,
    check_own_header_first,
    check_module_boundary,
    check_uobject_allocation,
)


def run() -> Findings:
    findings = Findings()
    for check in CHECKS:
        check(findings)
    return findings


SELF_TEST_CASES = (
    ("generated-header", "Late.h",
     '#include "CoreMinimal.h"\n#include "Late.generated.h"\n#include "Extra.h"\n\n'
     'UCLASS()\nclass ULate : public UObject\n{\n    GENERATED_BODY()\n};\n'),
    ("generated-header", "Spurious.h",
     '#include "CoreMinimal.h"\n#include "Spurious.generated.h"\n\n'
     'struct FPlain\n{\n    int32 Value = 0;\n};\n'),
    ("reflection-in-cpp", "InCpp.cpp",
     '#include "InCpp.h"\n\nUSTRUCT()\nstruct FInCpp\n{\n    GENERATED_BODY()\n};\n'),
    ("generated-body", "NoBody.h",
     '#include "CoreMinimal.h"\n#include "NoBody.generated.h"\n\nUCLASS()\n'
     'class UNoBody : public UObject\n{\npublic:\n    struct FNested\n    {\n'
     '        int32 Inner = 0;\n    };\n    int32 Value = 0;\n};\n'),
    ("uenum-underlying-type", "Enum.h",
     '#include "CoreMinimal.h"\n#include "Enum.generated.h"\n\n'
     'UENUM(BlueprintType)\nenum class EBad : int32\n{\n    None,\n};\n'),
    ("reflected-pointer", "RawPointer.h",
     '#include "CoreMinimal.h"\n#include "RawPointer.generated.h"\n\nUSTRUCT()\n'
     'struct FRawPointer\n{\n    GENERATED_BODY()\n    UPROPERTY()\n'
     '    UObject* Held = nullptr;\n};\n'),
    ("unreflected-tobjectptr", "Unreflected.h",
     '#include "CoreMinimal.h"\n#include "Unreflected.generated.h"\n\nUCLASS()\n'
     'class UUnreflected : public UObject\n{\n    GENERATED_BODY()\n'
     '    TObjectPtr<UActorComponent> Held;\n};\n'),
    ("uobject-allocation", "Alloc.cpp",
     '#include "Alloc.h"\n\nvoid Make()\n{\n    UObject* Bad = new UObject();\n}\n'),
)

# Legal shapes an earlier raw-text version of this linter got wrong. Each must
# produce no finding at all.
SELF_TEST_LEGAL = (
    ("Comment.h",
     '#include "CoreMinimal.h"\n// UCLASS() in a comment is not a declaration\n'
     '/* USTRUCT() either */\nstruct FPlain\n{\n    int32 Value = 0;\n};\n'),
    ("Literal.cpp",
     '#include "Literal.h"\n\nvoid Log()\n{\n'
     '    const TCHAR* Text = TEXT("UCLASS() appears here as text");\n}\n'),
    ("Nested.h",
     '#include "CoreMinimal.h"\n#include "Nested.generated.h"\n\nUCLASS()\n'
     'class UOuter : public UObject\n{\n    GENERATED_BODY()\npublic:\n'
     '    struct FInner\n    {\n        int32 Value = 0;\n    };\n};\n'),
    # A TObjectPtr inside a nested plain struct cannot carry UPROPERTY and is not a
    # GC root of the enclosing class. An earlier version of the unreflected-tobjectptr
    # check scanned the whole body and reported three of these in real code.
    ("NestedPointer.h",
     '#include "CoreMinimal.h"\n#include "NestedPointer.generated.h"\n\n'
     'UCLASS()\nclass UHolder : public UObject\n{\n    GENERATED_BODY()\n'
     'public:\n    struct FSnapshot\n    {\n'
     '        TObjectPtr<UObject> Captured;\n    };\n'
     '    UPROPERTY() TObjectPtr<UObject> Owned;\n};\n'),
)


def self_test() -> int:
    import shutil
    import tempfile

    global SOURCE
    original = SOURCE
    caught = 0
    for expected, name, content in SELF_TEST_CASES:
        workspace = Path(tempfile.mkdtemp(prefix="dgtour_lint_"))
        try:
            (workspace / name).write_text(content, encoding="utf-8")
            if name.endswith(".cpp"):
                (workspace / name.replace(".cpp", ".h")).write_text(
                    '#include "CoreMinimal.h"\n', encoding="utf-8")
            SOURCE = workspace
            reported = {item[0] for item in run().items}
            if expected in reported:
                caught += 1
                print(f"  fires   {expected:24} ({name})")
            else:
                print(f"  MISSED  {expected:24} ({name}) reported {sorted(reported)}")
        finally:
            SOURCE = original
            shutil.rmtree(workspace, ignore_errors=True)

    clean = 0
    for name, content in SELF_TEST_LEGAL:
        workspace = Path(tempfile.mkdtemp(prefix="dgtour_lint_"))
        try:
            (workspace / name).write_text(content, encoding="utf-8")
            if name.endswith(".cpp"):
                (workspace / name.replace(".cpp", ".h")).write_text(
                    '#include "CoreMinimal.h"\n', encoding="utf-8")
            SOURCE = workspace
            items = run().items
            if not items:
                clean += 1
                print(f"  quiet   legal construct        ({name})")
            else:
                print(f"  FALSE POSITIVE on {name}: {items}")
        finally:
            SOURCE = original
            shutil.rmtree(workspace, ignore_errors=True)

    print(f"\nCPP CONVENTION LINT SELF-TEST: {caught}/{len(SELF_TEST_CASES)} checks "
          f"fire, {clean}/{len(SELF_TEST_LEGAL)} legal constructs stay quiet")
    return 0 if caught == len(SELF_TEST_CASES) and clean == len(SELF_TEST_LEGAL) else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true",
                        help="prove each check fires, and stays quiet on legal code")
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
        print(f"\n{len(findings.items)} finding(s) across {findings.checked} "
              "inspected declarations.")
        return 1

    print("C++ convention lint OK")
    print(f"  {findings.checked} declarations inspected across "
          f"{len(headers())} headers and {len(sources())} sources")
    print("  This is a source-text check, not a compile.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
