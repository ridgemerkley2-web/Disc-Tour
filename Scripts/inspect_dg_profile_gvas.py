#!/usr/bin/env python3
"""Read-only, fail-closed inspection of the DiscGolfTour schema-10 profile GVAS.

The helper intentionally implements only the UE 5.8 save/header and tagged
property subset written by UDiscGolfSaveGame. It never writes the input or a
receipt file: the JSON receipt is emitted to stdout.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable


RECEIPT_SCHEMA = "DiscGolfTour.GvasTechnicalAcceptance.v1"
PASS_STATE = "PASS_GVAS_TECHNICAL_ACCEPTANCE"
FAIL_STATE = "FAIL_GVAS_TECHNICAL_ACCEPTANCE"
EXPECTED_CLASS = "/Script/DiscGolfTour.DiscGolfSaveGame"
EXPECTED_SCHEMA_VERSION = 10
EXPECTED_SAVE_GAME_FILE_VERSION = 3
EXPECTED_PACKAGE_UE4_VERSION = 522
EXPECTED_PACKAGE_UE5_VERSION = 1018
EXPECTED_ENGINE_MAJOR = 5
EXPECTED_ENGINE_MINOR = 8
EXPECTED_CUSTOM_VERSION_FORMAT = 3

MAX_FILE_BYTES = 16 * 1024 * 1024
MAX_STRING_CHARS = 4096
MAX_CUSTOM_VERSIONS = 512
MAX_PROPERTIES = 512
MAX_ARRAY_ITEMS = 64
MAX_TYPE_NODES = 16
MAX_DEPTH = 8

FLAG_HAS_ARRAY_INDEX = 0x01
FLAG_HAS_PROPERTY_GUID = 0x02
FLAG_HAS_PROPERTY_EXTENSIONS = 0x04
FLAG_BINARY_OR_NATIVE = 0x08
FLAG_BOOL_TRUE = 0x10
FLAG_SKIPPED = 0x20
KNOWN_FLAGS = 0x3F


class ParseFailure(Exception):
    """A stable fail-closed parse/validation failure."""

    def __init__(self, code: str, message: str, offset: int | None = None):
        super().__init__(message)
        self.code = code
        self.message = message
        self.offset = offset


class Reader:
    def __init__(self, data: bytes, *, base_offset: int = 0):
        self.data = data
        self.pos = 0
        self.base_offset = base_offset

    @property
    def remaining(self) -> int:
        return len(self.data) - self.pos

    @property
    def absolute_offset(self) -> int:
        return self.base_offset + self.pos

    def fail(self, code: str, message: str) -> None:
        raise ParseFailure(code, message, self.absolute_offset)

    def take(self, size: int) -> bytes:
        if size < 0:
            self.fail("NEGATIVE_SIZE", f"negative read size {size}")
        end = self.pos + size
        if end > len(self.data):
            self.fail(
                "TRUNCATED_DATA",
                f"need {size} byte(s), only {self.remaining} remain",
            )
        value = self.data[self.pos:end]
        self.pos = end
        return value

    def unpack(self, fmt: str) -> Any:
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.take(size))[0]

    def u8(self) -> int:
        return self.unpack("<B")

    def u16(self) -> int:
        return self.unpack("<H")

    def u32(self) -> int:
        return self.unpack("<I")

    def i32(self) -> int:
        return self.unpack("<i")

    def f32(self) -> float:
        return self.unpack("<f")

    def f64(self) -> float:
        return self.unpack("<d")

    def fstring(self) -> str:
        length_offset = self.absolute_offset
        length = self.i32()
        if length == 0:
            return ""
        if length == -(2**31):
            raise ParseFailure("INVALID_STRING", "invalid FString length", length_offset)

        wide = length < 0
        char_count = -length if wide else length
        if char_count < 1 or char_count > MAX_STRING_CHARS + 1:
            raise ParseFailure(
                "STRING_LIMIT",
                f"FString character count {char_count} is outside 1..{MAX_STRING_CHARS + 1}",
                length_offset,
            )
        raw = self.take(char_count * (2 if wide else 1))
        terminator = b"\x00\x00" if wide else b"\x00"
        if not raw.endswith(terminator):
            self.fail("INVALID_STRING", "FString lacks its null terminator")
        payload = raw[: -len(terminator)]
        try:
            return payload.decode("utf-16-le" if wide else "utf-8", errors="strict")
        except UnicodeDecodeError as exc:
            raise ParseFailure(
                "INVALID_STRING_ENCODING",
                f"FString decoding failed: {exc}",
                length_offset,
            ) from exc

    def bounded(self, size: int, label: str) -> "Reader":
        start = self.absolute_offset
        if size < 0 or size > self.remaining:
            self.fail(
                "INVALID_PROPERTY_SIZE",
                f"{label} size {size} exceeds remaining {self.remaining}",
            )
        return Reader(self.take(size), base_offset=start)

    def require_end(self, label: str) -> None:
        if self.remaining:
            self.fail(
                "UNCONSUMED_DATA",
                f"{label} has {self.remaining} unconsumed byte(s)",
            )


@dataclass(frozen=True)
class TypeNode:
    name: str
    inner_count: int


@dataclass(frozen=True)
class PropertyTag:
    name: str
    type_nodes: tuple[TypeNode, ...]
    size: int
    flags: int
    value: Reader

    @property
    def root_type(self) -> str:
        return self.type_nodes[0].name

    @property
    def type_display(self) -> str:
        return _format_type_nodes(self.type_nodes)


def _format_type_nodes(nodes: tuple[TypeNode, ...]) -> str:
    index = 0

    def visit() -> str:
        nonlocal index
        if index >= len(nodes):
            return "<malformed>"
        node = nodes[index]
        index += 1
        children = [visit() for _ in range(node.inner_count)]
        return node.name + ("(" + ",".join(children) + ")" if children else "")

    return visit()


def _short_type(name: str) -> str:
    return name.rsplit(".", 1)[-1].rsplit("/", 1)[-1]


def _has_type_node(tag: PropertyTag, expected: str) -> bool:
    return any(_short_type(node.name) == expected for node in tag.type_nodes[1:])


def _read_type_name(reader: Reader) -> tuple[TypeNode, ...]:
    nodes: list[TypeNode] = []
    remaining = 1
    while remaining:
        if len(nodes) >= MAX_TYPE_NODES:
            reader.fail("TYPE_NODE_LIMIT", "property type name exceeds node limit")
        name = reader.fstring()
        inner_count = reader.i32()
        if not name or inner_count < 0 or inner_count > MAX_TYPE_NODES:
            reader.fail(
                "INVALID_TYPE_NAME",
                f"invalid type node name={name!r}, innerCount={inner_count}",
            )
        remaining += inner_count - 1
        if remaining < 0 or remaining > MAX_TYPE_NODES:
            reader.fail("INVALID_TYPE_NAME", "malformed property type node arity")
        nodes.append(TypeNode(name, inner_count))
    return tuple(nodes)


def _read_property_tag(reader: Reader) -> PropertyTag | None:
    name = reader.fstring()
    if name == "None":
        return None
    if not name:
        reader.fail("INVALID_PROPERTY_NAME", "empty property name")
    type_nodes = _read_type_name(reader)
    size = reader.i32()
    flags = reader.u8()
    if flags & ~KNOWN_FLAGS:
        reader.fail("UNKNOWN_TAG_FLAGS", f"property {name} has unknown flags 0x{flags:02X}")
    if flags & FLAG_HAS_ARRAY_INDEX:
        array_index = reader.i32()
        if array_index != 0:
            reader.fail(
                "UNSUPPORTED_ARRAY_INDEX",
                f"property {name} has array index {array_index}",
            )
    if flags & FLAG_HAS_PROPERTY_GUID:
        reader.take(16)
        reader.fail("UNSUPPORTED_PROPERTY_GUID", f"property {name} has a property GUID")
    if flags & FLAG_HAS_PROPERTY_EXTENSIONS:
        reader.fail("UNSUPPORTED_PROPERTY_EXTENSIONS", f"property {name} has extensions")
    if flags & FLAG_SKIPPED:
        reader.fail("SKIPPED_PROPERTY", f"property {name} was serialized as skipped")
    if flags & FLAG_BINARY_OR_NATIVE and _short_type(type_nodes[0].name) != "StructProperty":
        reader.fail(
            "UNSUPPORTED_NATIVE_PROPERTY",
            f"property {name} has unexpected binary/native serialization",
        )
    if flags & FLAG_BOOL_TRUE and _short_type(type_nodes[0].name) != "BoolProperty":
        reader.fail("INVALID_BOOL_FLAG", f"non-bool property {name} has BoolTrue")
    value = reader.bounded(size, f"property {name}")
    return PropertyTag(name, type_nodes, size, flags, value)


def _parse_tagged_struct(
    reader: Reader,
    *,
    label: str,
    allowed: dict[str, Callable[[PropertyTag, int], Any] | None],
    depth: int,
) -> dict[str, Any]:
    if depth > MAX_DEPTH:
        reader.fail("DEPTH_LIMIT", f"{label} nesting exceeds {MAX_DEPTH}")
    result: dict[str, Any] = {}
    for _ in range(MAX_PROPERTIES):
        tag = _read_property_tag(reader)
        if tag is None:
            return result
        if tag.name not in allowed:
            reader.fail(
                "UNKNOWN_PROPERTY",
                f"{label} contains unexpected property {tag.name!r}",
            )
        if tag.name in result:
            reader.fail("DUPLICATE_PROPERTY", f"{label} repeats property {tag.name!r}")
        parser = allowed[tag.name]
        result[tag.name] = parser(tag, depth + 1) if parser else None
        tag.value.require_end(f"property {label}.{tag.name}")
    reader.fail("PROPERTY_LIMIT", f"{label} exceeds {MAX_PROPERTIES} properties")
    raise AssertionError("unreachable")


def _expect_root(tag: PropertyTag, expected: str) -> None:
    actual = _short_type(tag.root_type)
    if actual != expected:
        tag.value.fail(
            "PROPERTY_TYPE_MISMATCH",
            f"property {tag.name} is {tag.type_display}, expected {expected}",
        )


def _parse_int(tag: PropertyTag, _depth: int) -> int:
    _expect_root(tag, "IntProperty")
    if tag.size != 4 or tag.flags & FLAG_BOOL_TRUE:
        tag.value.fail("PROPERTY_SIZE_MISMATCH", f"int property {tag.name} must be 4 bytes")
    return tag.value.i32()


def _parse_float(tag: PropertyTag, _depth: int) -> float:
    _expect_root(tag, "FloatProperty")
    if tag.size != 4:
        tag.value.fail("PROPERTY_SIZE_MISMATCH", f"float property {tag.name} must be 4 bytes")
    value = tag.value.f32()
    if not math.isfinite(value):
        tag.value.fail("NONFINITE_NUMBER", f"float property {tag.name} is not finite")
    return value


def _parse_bool(tag: PropertyTag, _depth: int) -> bool:
    _expect_root(tag, "BoolProperty")
    if tag.size != 0:
        tag.value.fail("PROPERTY_SIZE_MISMATCH", f"bool property {tag.name} must have size 0")
    return bool(tag.flags & FLAG_BOOL_TRUE)


def _parse_name(tag: PropertyTag, _depth: int) -> str:
    _expect_root(tag, "NameProperty")
    return tag.value.fstring()


def _parse_enum(tag: PropertyTag, _depth: int) -> str:
    _expect_root(tag, "EnumProperty")
    return tag.value.fstring()


def _parse_expected_enum(expected: str) -> Callable[[PropertyTag, int], str]:
    def parse(tag: PropertyTag, depth: int) -> str:
        if not _has_type_node(tag, expected):
            tag.value.fail(
                "PROPERTY_TYPE_MISMATCH",
                f"property {tag.name} is {tag.type_display}, expected enum {expected}",
            )
        return _parse_enum(tag, depth)

    return parse


def _parse_vector(tag: PropertyTag, _depth: int) -> dict[str, float]:
    _expect_root(tag, "StructProperty")
    if not _has_type_node(tag, "Vector") or tag.size != 24:
        tag.value.fail(
            "PROPERTY_TYPE_MISMATCH",
            f"property {tag.name} must be a 24-byte FVector, got {tag.type_display}/{tag.size}",
        )
    values = (tag.value.f64(), tag.value.f64(), tag.value.f64())
    if not all(math.isfinite(value) for value in values):
        tag.value.fail("NONFINITE_NUMBER", f"vector property {tag.name} is not finite")
    return {"x": values[0], "y": values[1], "z": values[2]}


def _parse_effects(tag: PropertyTag, depth: int) -> dict[str, Any]:
    _expect_root(tag, "StructProperty")
    if not _has_type_node(tag, "LieEffectProfile"):
        tag.value.fail("PROPERTY_TYPE_MISMATCH", f"unexpected effects type {tag.type_display}")
    return _parse_tagged_struct(
        tag.value,
        label="PracticeLieState.Effects",
        allowed={
            "ProfileId": _parse_name,
            "PowerMultiplier": _parse_float,
            "TimingErrorMultiplier": _parse_float,
        },
        depth=depth,
    )


def _parse_lie(tag: PropertyTag, depth: int) -> dict[str, Any]:
    _expect_root(tag, "StructProperty")
    if not _has_type_node(tag, "DiscGolfLieState"):
        tag.value.fail("PROPERTY_TYPE_MISMATCH", f"unexpected lie type {tag.type_display}")
    return _parse_tagged_struct(
        tag.value,
        label="PracticeLieState",
        allowed={
            "SchemaVersion": _parse_int,
            "SurfaceAtRest": _parse_expected_enum("ECourseSurfaceType"),
            "PlayingSurface": _parse_expected_enum("ECourseSurfaceType"),
            "LieType": _parse_expected_enum("ELieType"),
            "ShotContext": _parse_expected_enum("EDiscShotContext"),
            "PenaltyType": _parse_expected_enum("EDiscGolfPenaltyType"),
            "ReliefRule": _parse_expected_enum("EDiscGolfReliefRule"),
            "RawDiscLocationCm": _parse_vector,
            "LieLocationCm": _parse_vector,
            "DistanceToBasketMeters": _parse_float,
            "PenaltyStrokes": _parse_int,
            "Effects": _parse_effects,
        },
        depth=depth,
    )


def _skip_text(tag: PropertyTag, _depth: int) -> None:
    _expect_root(tag, "TextProperty")
    tag.value.pos = len(tag.value.data)
    return None


def _skip_string(tag: PropertyTag, _depth: int) -> None:
    _expect_root(tag, "StrProperty")
    tag.value.fstring()
    return None


def _skip_known_array(tag: PropertyTag, _depth: int) -> None:
    _expect_root(tag, "ArrayProperty")
    if not _has_type_node(tag, "NameProperty"):
        tag.value.fail(
            "PROPERTY_TYPE_MISMATCH",
            f"property {tag.name} is {tag.type_display}, expected an FName array",
        )
    tag.value.pos = len(tag.value.data)
    return None


def _parse_hole_score_value(reader: Reader, depth: int) -> dict[str, Any]:
    return _parse_tagged_struct(
        reader,
        label="PracticeRoundState.HoleScores[]",
        allowed={
            "HoleNumber": _parse_int,
            "HoleName": _skip_text,
            "Par": _parse_int,
            "Strokes": _parse_int,
            "PenaltyStrokes": _parse_int,
            "bCompleted": _parse_bool,
        },
        depth=depth,
    )


def _parse_hole_scores(tag: PropertyTag, depth: int) -> list[dict[str, Any]]:
    _expect_root(tag, "ArrayProperty")
    if not _has_type_node(tag, "DiscGolfRoundHoleScore"):
        tag.value.fail("PROPERTY_TYPE_MISMATCH", f"unexpected hole-score array {tag.type_display}")
    count = tag.value.i32()
    if count < 1 or count > MAX_ARRAY_ITEMS:
        tag.value.fail(
            "ARRAY_LIMIT",
            f"hole-score count {count} is outside 1..{MAX_ARRAY_ITEMS}",
        )
    return [_parse_hole_score_value(tag.value, depth + 1) for _ in range(count)]


def _parse_round(tag: PropertyTag, depth: int) -> dict[str, Any]:
    _expect_root(tag, "StructProperty")
    if not _has_type_node(tag, "DiscGolfRoundState"):
        tag.value.fail("PROPERTY_TYPE_MISMATCH", f"unexpected round type {tag.type_display}")
    return _parse_tagged_struct(
        tag.value,
        label="PracticeRoundState",
        allowed={
            "CourseId": _parse_name,
            "LayoutId": _parse_name,
            "CourseName": _skip_text,
            "CurrentHoleIndex": _parse_int,
            "HoleScores": _parse_hole_scores,
            "bRoundComplete": _parse_bool,
        },
        depth=depth,
    )


def _skip_expected_struct(expected: str) -> Callable[[PropertyTag, int], None]:
    def parse(tag: PropertyTag, _depth: int) -> None:
        _expect_root(tag, "StructProperty")
        if not _has_type_node(tag, expected):
            tag.value.fail(
                "PROPERTY_TYPE_MISMATCH",
                f"property {tag.name} is {tag.type_display}, expected struct {expected}",
            )
        tag.value.pos = len(tag.value.data)
        return None

    return parse


TOP_LEVEL_PROPERTIES: dict[str, Callable[[PropertyTag, int], Any] | None] = {
    "PlayerName": _skip_string,
    "UnlockedMolds": _skip_known_array,
    "PreferredGraphicsPreset": _parse_int,
    "SaveSchemaVersion": _parse_int,
    "bHasPracticeRoundSnapshot": _parse_bool,
    "PracticeStrokes": _parse_int,
    "PracticePenaltyStrokes": _parse_int,
    "bPracticeHoleComplete": _parse_bool,
    "PracticeLieState": _parse_lie,
    "PracticeCourseId": _parse_name,
    "PracticeLayoutId": _parse_name,
    "PracticeHoleNumber": _parse_int,
    "PracticeRoundState": _parse_round,
    "PracticeMoldId": _parse_name,
    "PracticePlastic": _parse_expected_enum("EDiscPlastic"),
    "PlayerSettings": _skip_expected_struct("DiscGolfPlayerSettings"),
    "CharacterCustomization": _skip_expected_struct("DGFullCharacterCustomization"),
    "CharacterProfile": _skip_expected_struct("DiscGolfCharacterProfileSaveData"),
    "OutfitLoadout": _skip_expected_struct("DGOutfitLoadout"),
}


REQUIRED_TOP_LEVEL = {
    "bHasPracticeRoundSnapshot",
    "PracticeLieState",
    "PracticeCourseId",
    "PracticeLayoutId",
    "PracticeRoundState",
    "PracticeMoldId",
}

REQUIRED_LIE: set[str] = set()

REQUIRED_ROUND = {
    "CourseId",
    "LayoutId",
    "HoleScores",
}

REQUIRED_SCORE: set[str] = set()


def _require_fields(value: dict[str, Any], required: set[str], label: str) -> None:
    missing = sorted(required - value.keys())
    if missing:
        raise ParseFailure("MISSING_PROPERTY", f"{label} is missing: {', '.join(missing)}")


def _apply_defaults(
    value: dict[str, Any],
    defaults: dict[str, Any],
    label: str,
    resolved: list[str],
) -> None:
    for name, default in defaults.items():
        if name not in value:
            value[name] = default
            resolved.append(f"{label}.{name}")


def _enum_short(value: str) -> str:
    return value.rsplit("::", 1)[-1]


def _parse_header(reader: Reader) -> dict[str, Any]:
    if reader.take(4) != b"GVAS":
        raise ParseFailure("UNKNOWN_MAGIC", "input is not a GVAS save", 0)
    save_game_version = reader.i32()
    package_ue4 = reader.i32()
    package_ue5 = reader.i32()
    engine = {
        "major": reader.u16(),
        "minor": reader.u16(),
        "patch": reader.u16(),
        "changelist": reader.u32(),
        "branch": reader.fstring(),
    }
    custom_format = reader.i32()
    custom_count = reader.i32()
    if custom_count < 0 or custom_count > MAX_CUSTOM_VERSIONS:
        reader.fail(
            "CUSTOM_VERSION_LIMIT",
            f"custom version count {custom_count} is outside 0..{MAX_CUSTOM_VERSIONS}",
        )
    custom_versions: list[dict[str, Any]] = []
    for _ in range(custom_count):
        guid = reader.take(16)
        version = reader.i32()
        custom_versions.append({"guidHex": guid.hex().upper(), "version": version})
    save_class = reader.fstring()

    expected = (
        (save_game_version, EXPECTED_SAVE_GAME_FILE_VERSION, "save-game file version"),
        (package_ue4, EXPECTED_PACKAGE_UE4_VERSION, "UE4 package version"),
        (package_ue5, EXPECTED_PACKAGE_UE5_VERSION, "UE5 package version"),
        (engine["major"], EXPECTED_ENGINE_MAJOR, "engine major"),
        (engine["minor"], EXPECTED_ENGINE_MINOR, "engine minor"),
        (custom_format, EXPECTED_CUSTOM_VERSION_FORMAT, "custom-version format"),
        (save_class, EXPECTED_CLASS, "save class"),
    )
    for actual, wanted, label in expected:
        if actual != wanted:
            reader.fail("UNKNOWN_HEADER_VERSION", f"{label} {actual!r}, expected {wanted!r}")
    return {
        "saveGameFileVersion": save_game_version,
        "packageFileVersionUE4": package_ue4,
        "packageFileVersionUE5": package_ue5,
        "engineVersion": engine,
        "customVersionFormat": custom_format,
        "customVersionCount": custom_count,
        "customVersionsSha256": hashlib.sha256(
            b"".join(bytes.fromhex(item["guidHex"]) + struct.pack("<i", item["version"]) for item in custom_versions)
        ).hexdigest().upper(),
        "saveGameClass": save_class,
    }


def parse_gvas(data: bytes) -> tuple[dict[str, Any], dict[str, Any]]:
    if not data:
        raise ParseFailure("EMPTY_INPUT", "save is empty", 0)
    if len(data) > MAX_FILE_BYTES:
        raise ParseFailure(
            "FILE_LIMIT",
            f"save is {len(data)} bytes; maximum is {MAX_FILE_BYTES}",
            0,
        )
    reader = Reader(data)
    header = _parse_header(reader)
    # UE 5.8 writes one root-UClass serialization-control-extension byte
    # before the first tagged property. Struct payloads do not have it.
    serialization_control_extensions = reader.u8()
    if serialization_control_extensions != 0:
        reader.fail(
            "UNKNOWN_SERIALIZATION_CONTROL",
            "root serialization-control extensions are not supported",
        )
    header["serializationControlExtensions"] = serialization_control_extensions
    values = _parse_tagged_struct(
        reader,
        label="DiscGolfSaveGame",
        allowed=TOP_LEVEL_PROPERTIES,
        depth=0,
    )
    object_guid_present = reader.u32()
    if object_guid_present not in (0, 1):
        reader.fail(
            "UNKNOWN_OBJECT_GUID_MARKER",
            f"object GUID marker is {object_guid_present}, expected 0 or 1",
        )
    header["objectGuidPresent"] = bool(object_guid_present)
    if object_guid_present:
        header["objectGuidHex"] = reader.take(16).hex().upper()
    reader.require_end("GVAS file")
    default_resolved: list[str] = []
    _apply_defaults(
        values,
        {
            "SaveSchemaVersion": 10,
            "bHasPracticeRoundSnapshot": False,
            "PracticeStrokes": 0,
            "PracticePenaltyStrokes": 0,
            "bPracticeHoleComplete": False,
            "PracticeCourseId": "RegressionCourse",
            "PracticeLayoutId": "Practice",
            "PracticeHoleNumber": 1,
            "PracticeMoldId": "Apex",
            "PracticePlastic": "EDiscPlastic::Tour",
        },
        "DiscGolfSaveGame",
        default_resolved,
    )
    _require_fields(values, REQUIRED_TOP_LEVEL, "DiscGolfSaveGame")
    if values["SaveSchemaVersion"] != EXPECTED_SCHEMA_VERSION:
        raise ParseFailure(
            "UNKNOWN_GAME_SCHEMA",
            f"schema {values['SaveSchemaVersion']}, expected {EXPECTED_SCHEMA_VERSION}",
        )
    if values["bHasPracticeRoundSnapshot"] is not True:
        raise ParseFailure("NO_PRACTICE_SNAPSHOT", "profile has no practice-round snapshot")

    lie = values["PracticeLieState"]
    round_state = values["PracticeRoundState"]
    _apply_defaults(
        lie,
        {
            "SchemaVersion": 1,
            "RawDiscLocationCm": {"x": 0.0, "y": 0.0, "z": 0.0},
            "LieLocationCm": {"x": 0.0, "y": 0.0, "z": 0.0},
            "DistanceToBasketMeters": 0.0,
            "PenaltyStrokes": 0,
        },
        "PracticeLieState",
        default_resolved,
    )
    _apply_defaults(
        round_state,
        {"CurrentHoleIndex": 0, "bRoundComplete": False},
        "PracticeRoundState",
        default_resolved,
    )
    _require_fields(lie, REQUIRED_LIE, "PracticeLieState")
    _require_fields(round_state, REQUIRED_ROUND, "PracticeRoundState")
    scores = round_state["HoleScores"]
    for index, score in enumerate(scores):
        _apply_defaults(
            score,
            {
                "HoleNumber": 1,
                "Par": 3,
                "Strokes": 0,
                "PenaltyStrokes": 0,
                "bCompleted": False,
            },
            f"PracticeRoundState.HoleScores[{index}]",
            default_resolved,
        )
        _require_fields(score, REQUIRED_SCORE, f"HoleScores[{index}]")

    if lie["SchemaVersion"] != 1:
        raise ParseFailure("UNKNOWN_LIE_SCHEMA", f"lie schema {lie['SchemaVersion']}, expected 1")
    if lie["DistanceToBasketMeters"] < 0:
        raise ParseFailure("INVALID_LIE", "distance to basket is negative")
    if lie["PenaltyStrokes"] < 0:
        raise ParseFailure("INVALID_LIE", "lie penalty strokes are negative")

    practice_hole = values["PracticeHoleNumber"]
    current_index = round_state["CurrentHoleIndex"]
    if current_index < 0 or current_index >= len(scores):
        raise ParseFailure("INVALID_ROUND", f"current hole index {current_index} is out of range")
    if practice_hole != current_index + 1:
        raise ParseFailure(
            "INCONSISTENT_CURRENT_HOLE",
            f"PracticeHoleNumber {practice_hole} != CurrentHoleIndex+1 {current_index + 1}",
        )
    if values["PracticeCourseId"] != round_state["CourseId"]:
        raise ParseFailure("INCONSISTENT_COURSE", "practice and round course IDs differ")
    if values["PracticeLayoutId"] != round_state["LayoutId"]:
        raise ParseFailure("INCONSISTENT_LAYOUT", "practice and round layout IDs differ")
    if values["PracticeStrokes"] < 0 or values["PracticePenaltyStrokes"] < 0:
        raise ParseFailure("INVALID_SCORE", "practice stroke count is negative")

    for index, score in enumerate(scores):
        if score["HoleNumber"] != index + 1:
            raise ParseFailure("INVALID_ROUND", "hole-score numbers must be contiguous from 1")
        if score["Par"] <= 0 or score["Strokes"] < 0 or score["PenaltyStrokes"] < 0:
            raise ParseFailure("INVALID_SCORE", f"invalid numeric score at hole {index + 1}")
        if index < current_index and not score["bCompleted"]:
            raise ParseFailure("INVALID_ROUND", f"hole {index + 1} before current hole is incomplete")
        if index > current_index and score["bCompleted"]:
            raise ParseFailure("INVALID_ROUND", f"future hole {index + 1} is completed")

    active = scores[current_index]
    if active["HoleNumber"] != practice_hole:
        raise ParseFailure("INCONSISTENT_CURRENT_HOLE", "active score hole number differs")
    if active["bCompleted"] != values["bPracticeHoleComplete"]:
        raise ParseFailure("INCONSISTENT_HOLE_COMPLETION", "active completion flags differ")
    if values["bPracticeHoleComplete"] and (
        active["Strokes"] != values["PracticeStrokes"]
        or active["PenaltyStrokes"] != values["PracticePenaltyStrokes"]
    ):
        raise ParseFailure("INCONSISTENT_SCORE", "completed active-hole scores differ")

    completed = [score for score in scores if score["bCompleted"]]
    all_complete = len(completed) == len(scores)
    if round_state["bRoundComplete"] != all_complete:
        raise ParseFailure("INCONSISTENT_ROUND_COMPLETION", "round completion flag differs from scores")

    mold = values["PracticeMoldId"]
    if not mold or mold == "None":
        raise ParseFailure("INVALID_DISC_SELECTION", "practice mold is empty")
    plastic = _enum_short(values["PracticePlastic"])
    if plastic not in {"Base", "Tour", "Crystal"}:
        raise ParseFailure("UNKNOWN_DISC_PLASTIC", f"unknown disc plastic {values['PracticePlastic']!r}")

    output_scores = [
        {
            "holeNumber": score["HoleNumber"],
            "par": score["Par"],
            "strokes": score["Strokes"],
            "penaltyStrokes": score["PenaltyStrokes"],
            "completed": score["bCompleted"],
            "scoreToPar": score["Strokes"] - score["Par"] if score["bCompleted"] else None,
        }
        for score in scores
    ]
    profile = {
        "saveSchemaVersion": values["SaveSchemaVersion"],
        "hasPracticeRoundSnapshot": values["bHasPracticeRoundSnapshot"],
        "courseId": values["PracticeCourseId"],
        "layoutId": values["PracticeLayoutId"],
        "currentHoleNumber": practice_hole,
        "currentHoleIndex": current_index,
        "practiceStrokes": values["PracticeStrokes"],
        "practicePenaltyStrokes": values["PracticePenaltyStrokes"],
        "practiceHoleComplete": values["bPracticeHoleComplete"],
        "roundComplete": round_state["bRoundComplete"],
        "completedHoleCount": len(completed),
        "completedStrokes": sum(score["Strokes"] for score in completed),
        "completedPenaltyStrokes": sum(score["PenaltyStrokes"] for score in completed),
        "completedScoreToPar": sum(score["Strokes"] - score["Par"] for score in completed),
        "holeScores": output_scores,
        "rawLieCm": lie["RawDiscLocationCm"],
        "lieLocationCm": lie["LieLocationCm"],
        "distanceToBasketMeters": lie["DistanceToBasketMeters"],
        "liePenaltyStrokes": lie["PenaltyStrokes"],
        "lieType": _enum_short(lie["LieType"]) if "LieType" in lie else None,
        "discSelection": {"moldId": mold, "plastic": plastic},
    }
    checks = {
        "exactGvasHeaderKnown": True,
        "exactSaveClassKnown": True,
        "exactGameSchemaKnown": True,
        "requiredTechnicalFieldsResolved": True,
        "propertyPayloadFullyConsumed": True,
        "roundStateConsistent": True,
        "lieFiniteAndNonnegative": True,
        "discSelectionKnown": True,
        "defaultResolvedPropertyCount": len(default_resolved),
        "defaultResolvedProperties": default_resolved,
    }
    return header, {"profile": profile, "checks": checks}


def make_receipt(path: Path, data: bytes) -> dict[str, Any]:
    source = {
        "path": str(path.resolve()),
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest().upper(),
        "readOnlyInspection": True,
    }
    try:
        header, body = parse_gvas(data)
        return {
            "schema": RECEIPT_SCHEMA,
            "schemaVersion": 1,
            "state": PASS_STATE,
            "source": source,
            "gvas": header,
            **body,
            "acceptanceBoundary": {
                "technicalSaveAcceptanceOnly": True,
                "gameplayAcceptanceClaimed": False,
                "releaseReadinessClaimed": False,
                "humanOrLegalApprovalClaimed": False,
            },
        }
    except ParseFailure as exc:
        error: dict[str, Any] = {"code": exc.code, "message": exc.message}
        if exc.offset is not None:
            error["offset"] = exc.offset
        return {
            "schema": RECEIPT_SCHEMA,
            "schemaVersion": 1,
            "state": FAIL_STATE,
            "source": source,
            "error": error,
            "acceptanceBoundary": {
                "technicalSaveAcceptanceOnly": True,
                "gameplayAcceptanceClaimed": False,
                "releaseReadinessClaimed": False,
                "humanOrLegalApprovalClaimed": False,
            },
        }


def _pack_fstring(value: str) -> bytes:
    raw = value.encode("utf-8") + b"\x00"
    return struct.pack("<i", len(raw)) + raw


def _pack_type(nodes: list[tuple[str, int]]) -> bytes:
    return b"".join(_pack_fstring(name) + struct.pack("<i", inner) for name, inner in nodes)


def _pack_tag(
    name: str,
    nodes: list[tuple[str, int]],
    value: bytes,
    *,
    flags: int = 0,
) -> bytes:
    return _pack_fstring(name) + _pack_type(nodes) + struct.pack("<iB", len(value), flags) + value


def _pack_int(name: str, value: int) -> bytes:
    return _pack_tag(name, [("IntProperty", 0)], struct.pack("<i", value))


def _pack_bool(name: str, value: bool) -> bytes:
    return _pack_tag(name, [("BoolProperty", 0)], b"", flags=FLAG_BOOL_TRUE if value else 0)


def _pack_name(name: str, value: str) -> bytes:
    return _pack_tag(name, [("NameProperty", 0)], _pack_fstring(value))


def _pack_enum(name: str, enum_type: str, value: str) -> bytes:
    return _pack_tag(
        name,
        [("EnumProperty", 2), (enum_type, 0), ("ByteProperty", 0)],
        _pack_fstring(value),
    )


def _pack_struct(name: str, struct_type: str, value: bytes) -> bytes:
    return _pack_tag(name, [("StructProperty", 1), (struct_type, 0)], value)


def _end_struct() -> bytes:
    return _pack_fstring("None")


def _synthetic_fixture() -> bytes:
    score = b"".join(
        [
            _pack_int("HoleNumber", 1),
            _pack_int("Par", 3),
            _pack_int("Strokes", 0),
            _pack_int("PenaltyStrokes", 0),
            _pack_bool("bCompleted", False),
            _end_struct(),
        ]
    )
    score_array = struct.pack("<i", 1) + score
    round_state = b"".join(
        [
            _pack_name("CourseId", "PineRidge"),
            _pack_name("LayoutId", "Main"),
            _pack_int("CurrentHoleIndex", 0),
            _pack_tag(
                "HoleScores",
                [
                    ("ArrayProperty", 1),
                    ("StructProperty", 1),
                    ("DiscGolfRoundHoleScore", 0),
                ],
                score_array,
            ),
            _pack_bool("bRoundComplete", False),
            _end_struct(),
        ]
    )
    vector_type = [("StructProperty", 1), ("Vector", 0)]
    lie = b"".join(
        [
            _pack_int("SchemaVersion", 1),
            _pack_enum("LieType", "ELieType", "ELieType::Fairway"),
            _pack_tag("RawDiscLocationCm", vector_type, struct.pack("<ddd", 1.0, 2.0, 3.0)),
            _pack_tag("LieLocationCm", vector_type, struct.pack("<ddd", 1.0, 2.0, 3.0)),
            _pack_tag("DistanceToBasketMeters", [("FloatProperty", 0)], struct.pack("<f", 10.0)),
            _pack_int("PenaltyStrokes", 0),
            _end_struct(),
        ]
    )
    payload = b"".join(
        [
            _pack_int("SaveSchemaVersion", 10),
            _pack_bool("bHasPracticeRoundSnapshot", True),
            _pack_int("PracticeStrokes", 0),
            _pack_int("PracticePenaltyStrokes", 0),
            _pack_bool("bPracticeHoleComplete", False),
            _pack_struct("PracticeLieState", "DiscGolfLieState", lie),
            _pack_name("PracticeCourseId", "PineRidge"),
            _pack_name("PracticeLayoutId", "Main"),
            _pack_int("PracticeHoleNumber", 1),
            _pack_struct("PracticeRoundState", "DiscGolfRoundState", round_state),
            _pack_name("PracticeMoldId", "Apex"),
            _pack_enum("PracticePlastic", "EDiscPlastic", "EDiscPlastic::Tour"),
            _end_struct(),
        ]
    )
    branch = _pack_fstring("++UE5+Release-5.8")
    custom_entries = b"".join(bytes(16) + struct.pack("<i", 0) for _ in range(92))
    header = b"".join(
        [
            b"GVAS",
            struct.pack("<iii", 3, 522, 1018),
            struct.pack("<HHHI", 5, 8, 1, 0),
            branch,
            struct.pack("<ii", 3, 92),
            custom_entries,
            _pack_fstring(EXPECTED_CLASS),
            b"\x00",
        ]
    )
    return header + payload + struct.pack("<I", 0)


def run_self_test() -> dict[str, Any]:
    fixture = _synthetic_fixture()
    cases: list[tuple[str, bool]] = []

    header, body = parse_gvas(fixture)
    cases.append(("known fixture passes", body["profile"]["currentHoleNumber"] == 1))
    cases.append(("fixture header is bound", header["customVersionCount"] == 92))

    mutations: list[tuple[str, bytes, str]] = [
        ("bad magic rejected", b"NOPE" + fixture[4:], "UNKNOWN_MAGIC"),
        ("unknown engine minor rejected", fixture[:18] + struct.pack("<H", 9) + fixture[20:], "UNKNOWN_HEADER_VERSION"),
        ("trailing byte rejected", fixture + b"X", "UNCONSUMED_DATA"),
        (
            "unknown game schema rejected",
            fixture.replace(
                _pack_int("SaveSchemaVersion", 10),
                _pack_int("SaveSchemaVersion", 11),
                1,
            ),
            "UNKNOWN_GAME_SCHEMA",
        ),
        (
            "unknown tag flags rejected",
            fixture.replace(
                _pack_int("SaveSchemaVersion", 10),
                _pack_tag(
                    "SaveSchemaVersion",
                    [("IntProperty", 0)],
                    struct.pack("<i", 10),
                    flags=0x40,
                ),
                1,
            ),
            "UNKNOWN_TAG_FLAGS",
        ),
        (
            "unknown object GUID marker rejected",
            fixture[:-4] + struct.pack("<I", 2),
            "UNKNOWN_OBJECT_GUID_MARKER",
        ),
    ]
    for label, mutated, expected_code in mutations:
        try:
            parse_gvas(mutated)
            cases.append((label, False))
        except ParseFailure as exc:
            cases.append((label, exc.code == expected_code))

    # Every strict prefix near the structural tail must reject truncation.
    tail_points = sorted({len(fixture) - amount for amount in (1, 2, 4, 8, 16, 31)})
    truncation_ok = True
    for point in tail_points:
        try:
            parse_gvas(fixture[:point])
            truncation_ok = False
        except ParseFailure:
            pass
    cases.append(("truncated tails rejected", truncation_ok))

    # Inject a structurally valid but unknown top-level tag before None.
    unknown_tag = _pack_int("FutureUnknownField", 7)
    root_end_offset = len(fixture) - 4 - len(_end_struct())
    unknown = (
        fixture[:root_end_offset]
        + unknown_tag
        + _end_struct()
        + fixture[-4:]
    )
    try:
        parse_gvas(unknown)
        cases.append(("unknown property rejected", False))
    except ParseFailure as exc:
        cases.append(("unknown property rejected", exc.code == "UNKNOWN_PROPERTY"))

    passed = sum(1 for _, ok in cases if ok)
    return {
        "schema": "DiscGolfTour.GvasTechnicalAcceptance.SelfTest.v1",
        "state": "PASS" if passed == len(cases) else "FAIL",
        "passed": passed,
        "total": len(cases),
        "cases": [{"name": name, "passed": ok} for name, ok in cases],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Emit a read-only technical JSON receipt for a DiscGolfTour profile GVAS."
    )
    parser.add_argument("save", nargs="?", type=Path, help="DiscGolfTour_Profile_0.sav")
    parser.add_argument("--self-test", action="store_true", help="run bounded parser self-tests")
    args = parser.parse_args(argv)

    if args.self_test:
        receipt = run_self_test()
        print(json.dumps(receipt, indent=2, sort_keys=True))
        return 0 if receipt["state"] == "PASS" else 1
    if args.save is None:
        parser.error("save is required unless --self-test is used")
    try:
        data = args.save.read_bytes()
    except OSError as exc:
        receipt = {
            "schema": RECEIPT_SCHEMA,
            "schemaVersion": 1,
            "state": FAIL_STATE,
            "source": {"path": str(args.save), "readOnlyInspection": True},
            "error": {"code": "INPUT_READ_FAILED", "message": str(exc)},
        }
        print(json.dumps(receipt, indent=2, sort_keys=True))
        return 1
    receipt = make_receipt(args.save, data)
    print(json.dumps(receipt, indent=2, sort_keys=True))
    return 0 if receipt["state"] == PASS_STATE else 1


if __name__ == "__main__":
    raise SystemExit(main())
