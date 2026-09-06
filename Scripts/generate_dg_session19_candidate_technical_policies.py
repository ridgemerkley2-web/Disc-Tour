#!/usr/bin/env python3
"""Create fresh Session 10 v4 or Session 12 v3 candidate policies append-only.

The historical v3/v2 policy files are immutable templates.  A generation mode
derives one candidate-scoped trusted-runtime-journal policy from explicit live
archive, external-run, and UserDir inputs, invokes the existing validator in
memory, then publishes the complete Config JSON through exclusive hard-link
creation.  Session 12 additionally requires explicit post-change focused and
full-suite automation logs; their stable identities replace the historical
template bindings.  It never creates the corresponding evidence receipt.
"""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import tempfile
from typing import Any, Callable

import validate_dg_session10_environment_shipping as session10
import validate_dg_session19_session12_presentation_technical as session12


ROOT = Path(__file__).resolve().parents[1]
SESSION10_TEMPLATE = Path("Config/DG_Session10EnvironmentShippingTechnicalPolicy.json")
SESSION12_TEMPLATE = Path("Config/DG_Session19Session12PresentationTechnicalPolicy.json")
FRESH_CANDIDATE_RE = re.compile(
    r"^S19_WindowsShipping_(?P<utc>\d{8}T\d{6}Z)_(?P<nonce>[0-9a-f]{12})$"
)
UNREAL_LOG_UTC_RE = re.compile(
    r"\[(?P<utc>\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}):\d{3}\]"
)
ABSLOG_PREFIX_RE = re.compile(r"(?i)(?<![A-Za-z0-9_-])-abslog=")
UUID_RE = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
)
SESSION10_SELECTED_SCREENSHOTS = [
    ("HOLE_1_TEE_ENVIRONMENT_RENDERED", "Screenshots/002_hole1_tee.png"),
    (
        "HOLE_1_RECOVERED_LIE_ENVIRONMENT_RENDERED",
        "Screenshots/003_hole1_recovered_lie.png",
    ),
    ("HOLE_3_TEE_ENVIRONMENT_RENDERED", "Screenshots/008_hole3_tee.png"),
]


class GenerationError(RuntimeError):
    """Fail-closed generation error; no candidate policy may be published."""


def _is_reparse(path: Path) -> bool:
    try:
        attributes = getattr(path.lstat(), "st_file_attributes", 0)
    except OSError:
        return False
    return path.is_symlink() or bool(
        attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    )


def _contains_reparse(path: Path) -> bool:
    absolute = path.absolute()
    if not absolute.parts:
        return False
    cursor = Path(absolute.parts[0])
    for part in absolute.parts[1:]:
        cursor /= part
        if os.path.lexists(cursor) and _is_reparse(cursor):
            return True
    return False


def _refuse_tree_reparse(root: Path, label: str) -> None:
    """Reject descendant reparses before a live directory becomes authority."""
    try:
        resolved_root = root.resolve(strict=True)
    except OSError as exc:
        raise GenerationError(f"{label} is missing: {exc}") from exc
    pending = [root]
    while pending:
        current = pending.pop()
        try:
            with os.scandir(current) as entries:
                for entry in entries:
                    path = Path(entry.path)
                    if _is_reparse(path):
                        raise GenerationError(
                            f"{label} contains a descendant reparse point: {path}"
                        )
                    if entry.is_dir(follow_symlinks=False):
                        try:
                            path.resolve(strict=True).relative_to(resolved_root)
                        except (OSError, ValueError) as exc:
                            raise GenerationError(
                                f"{label} contains a directory outside its boundary: {path}"
                            ) from exc
                        pending.append(path)
                    elif not entry.is_file(follow_symlinks=False):
                        raise GenerationError(
                            f"{label} contains an irregular descendant: {path}"
                        )
        except GenerationError:
            raise
        except OSError as exc:
            raise GenerationError(f"{label} tree inspection failed: {exc}") from exc


def _canonical_relative(value: Any, label: str) -> str:
    if type(value) is not str or not value:
        raise GenerationError(f"{label} must be a non-empty relative path")
    pure = PurePosixPath(value)
    if (
        pure.is_absolute()
        or any(part in {"", ".", ".."} for part in pure.parts)
        or "\\" in value
        or ":" in value
        or value != pure.as_posix()
    ):
        raise GenerationError(f"{label} is not a canonical project-relative path")
    return value


def _project_path(value: Any, label: str, *, must_exist: bool) -> tuple[Path, str]:
    relative = _canonical_relative(value, label)
    lexical = ROOT.joinpath(*PurePosixPath(relative).parts)
    cursor = ROOT.resolve()
    for part in PurePosixPath(relative).parts:
        cursor /= part
        if os.path.lexists(cursor) and _is_reparse(cursor):
            raise GenerationError(f"{label} contains a reparse point")
    try:
        path = lexical.resolve(strict=must_exist)
        path.relative_to(ROOT.resolve())
    except (OSError, ValueError) as exc:
        raise GenerationError(f"{label} escapes the project or is missing") from exc
    if must_exist and (not path.is_file() or _is_reparse(path)):
        raise GenerationError(f"{label} must be an existing regular file")
    return path, relative


def _absolute_host(
    value: Path,
    label: str,
    *,
    directory: bool,
) -> Path:
    if not value.is_absolute():
        raise GenerationError(f"{label} must be an absolute path")
    if ".." in value.parts:
        raise GenerationError(f"{label} must not contain parent traversal")
    if _contains_reparse(value):
        raise GenerationError(f"{label} contains a reparse point")
    try:
        path = value.resolve(strict=True)
    except OSError as exc:
        raise GenerationError(f"{label} is missing: {exc}") from exc
    if directory and not path.is_dir():
        raise GenerationError(f"{label} must be an existing directory")
    if not directory and not path.is_file():
        raise GenerationError(f"{label} must be an existing file")
    if directory:
        _refuse_tree_reparse(path, label)
    return path


def _stable_snapshot(path: Path) -> tuple[bytes, os.stat_result]:
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    if (
        before.st_size != after.st_size
        or before.st_mtime_ns != after.st_mtime_ns
        or before.st_ctime_ns != after.st_ctime_ns
        or before.st_dev != after.st_dev
        or before.st_ino != after.st_ino
        or len(data) != after.st_size
    ):
        raise GenerationError(f"file changed while read: {path}")
    return data, after


def _stat_fingerprint(value: os.stat_result) -> tuple[int, int, int, int, int, int]:
    return (
        value.st_mode,
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
        value.st_dev,
        value.st_ino,
    )


def _embedded_abslog_values(text: str, label: str) -> list[str]:
    """Parse every Unreal ``-abslog=`` assignment without substring matching."""
    prefixes = list(ABSLOG_PREFIX_RE.finditer(text))
    if not prefixes:
        raise GenerationError(f"{label} contains no embedded -abslog assignment")
    values: list[str] = []
    for prefix in prefixes:
        start = prefix.end()
        if start >= len(text):
            raise GenerationError(f"{label} contains an empty -abslog assignment")
        if text[start] == '"':
            end = text.find('"', start + 1)
            if end < 0:
                raise GenerationError(f"{label} contains an unterminated quoted -abslog")
            value = text[start + 1:end]
            trailer = end + 1
        else:
            end = start
            while end < len(text) and not text[end].isspace() and text[end] != '"':
                end += 1
            value = text[start:end]
            trailer = end
        while trailer < len(text) and text[trailer] == '"':
            trailer += 1
        if trailer < len(text) and not text[trailer].isspace():
            raise GenerationError(f"{label} contains a malformed -abslog delimiter")
        if not value or "\x00" in value or "'" in value:
            raise GenerationError(f"{label} contains an invalid -abslog value")
        values.append(value)
    return values


def _validate_embedded_abslog_identity(
    text: str,
    selected: Path,
    observed_fingerprint: tuple[int, int, int, int, int, int],
    label: str,
) -> None:
    """Bind all embedded aliases to one stable, selected regular file."""
    try:
        selected_resolved = selected.resolve(strict=True)
    except OSError as exc:
        raise GenerationError(f"{label} selected log is missing: {exc}") from exc
    if (
        selected_resolved != selected
        or not selected.is_file()
        or selected.is_symlink()
        or _is_reparse(selected)
    ):
        raise GenerationError(f"{label} selected log is not an exact regular file")
    try:
        before = selected.stat()
    except OSError as exc:
        raise GenerationError(f"{label} selected log cannot be inspected: {exc}") from exc
    if _stat_fingerprint(before) != observed_fingerprint:
        raise GenerationError(f"{label} selected log changed after its byte snapshot")

    embedded_paths: list[Path] = []
    for value in _embedded_abslog_values(text, label):
        embedded = Path(value)
        if not embedded.is_absolute() or ".." in embedded.parts:
            raise GenerationError(f"{label} embedded -abslog is not an absolute direct path")
        try:
            resolved = embedded.resolve(strict=True)
        except OSError as exc:
            raise GenerationError(f"{label} embedded -abslog target is missing: {exc}") from exc
        if (
            resolved != selected_resolved
            or not embedded.is_file()
            or embedded.is_symlink()
            or _is_reparse(embedded)
        ):
            raise GenerationError(
                f"{label} embedded -abslog is foreign, ambiguous, or irregular"
            )
        embedded_paths.append(embedded)

    try:
        after = selected.stat()
        aliases_still_exact = all(
            path.resolve(strict=True) == selected_resolved for path in embedded_paths
        )
    except OSError as exc:
        raise GenerationError(f"{label} log path changed during validation: {exc}") from exc
    if _stat_fingerprint(after) != observed_fingerprint or not aliases_still_exact:
        raise GenerationError(f"{label} log identity changed during provenance validation")


def _stable_bytes(path: Path) -> bytes:
    data, _ = _stable_snapshot(path)
    return data


def _strict_json(data: bytes, label: str) -> Any:
    def pairs(values: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in values:
            if key in result:
                raise GenerationError(f"duplicate JSON key in {label}: {key}")
            result[key] = value
        return result

    try:
        return json.loads(
            data.decode("utf-8-sig"),
            object_pairs_hook=pairs,
            parse_constant=lambda value: (_ for _ in ()).throw(
                GenerationError(f"non-finite JSON value in {label}: {value}")
            ),
        )
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise GenerationError(f"invalid JSON {label}: {exc}") from exc


def _load_project_json(value: Any, label: str) -> dict[str, Any]:
    path, _ = _project_path(value, label, must_exist=True)
    document = _strict_json(_stable_bytes(path), label)
    if type(document) is not dict:
        raise GenerationError(f"{label} root must be an object")
    return document


def _file_identity(relative: str, label: str) -> dict[str, Any]:
    path, canonical = _project_path(relative, label, must_exist=True)
    data = _stable_bytes(path)
    return {
        "path": canonical,
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest().upper(),
    }


def _candidate_build_utc(candidate_id: str) -> datetime:
    match = FRESH_CANDIDATE_RE.fullmatch(candidate_id)
    if match is None:
        raise GenerationError("candidate ID must contain exact UTC timestamp and lowercase nonce")
    try:
        parsed = datetime.strptime(match.group("utc"), "%Y%m%dT%H%M%SZ")
    except ValueError as exc:
        raise GenerationError("candidate ID UTC timestamp is invalid") from exc
    return parsed.replace(tzinfo=timezone.utc)


def _candidate_log_name(relative: str, candidate_id: str, label: str) -> None:
    expected_suffix = f"-{candidate_id}.log"
    if not PurePosixPath(relative).name.endswith(expected_suffix):
        raise GenerationError(
            f"{label} filename must end with the candidate scope {expected_suffix}"
        )


def _validate_candidate_log_observation(
    *,
    candidate_id: str,
    relative: str,
    absolute: Path,
    observed_stat: os.stat_result,
    text: str,
    label: str,
) -> None:
    build_utc = _candidate_build_utc(candidate_id)
    _candidate_log_name(relative, candidate_id, label)
    build_time_ns = int(build_utc.timestamp()) * 1_000_000_000
    if observed_stat.st_mtime_ns < build_time_ns:
        raise GenerationError(f"{label} modified before the candidate build timestamp")
    _validate_embedded_abslog_identity(
        text, absolute, _stat_fingerprint(observed_stat), label,
    )
    observed_log_times: list[datetime] = []
    for match in UNREAL_LOG_UTC_RE.finditer(text):
        try:
            observed = datetime.strptime(
                match.group("utc"), "%Y.%m.%d-%H.%M.%S"
            ).replace(tzinfo=timezone.utc)
        except ValueError as exc:
            raise GenerationError(f"{label} contains an invalid Unreal UTC timestamp") from exc
        observed_log_times.append(observed)
    if not observed_log_times:
        raise GenerationError(f"{label} contains no Unreal UTC timestamps")
    if min(observed_log_times) < build_utc:
        raise GenerationError(f"{label} contains pre-candidate Unreal log content")


def _candidate_automation_log(
    value: Path, label: str, candidate_id: str,
) -> tuple[dict[str, Any], str]:
    relative = _canonical_relative(value.as_posix(), label)
    pure = PurePosixPath(relative)
    if len(pure.parts) < 3 or pure.parts[:2] != ("Saved", "Logs") or pure.suffix != ".log":
        raise GenerationError(f"{label} must be a .log beneath Saved/Logs")
    _candidate_build_utc(candidate_id)
    _candidate_log_name(relative, candidate_id, label)
    path, canonical = _project_path(relative, label, must_exist=True)
    data, observed_stat = _stable_snapshot(path)
    text = data.decode("utf-8-sig", errors="replace")
    _validate_candidate_log_observation(
        candidate_id=candidate_id,
        relative=canonical,
        absolute=path,
        observed_stat=observed_stat,
        text=text,
        label=label,
    )
    identity = {
        "path": canonical,
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest().upper(),
    }
    return identity, text


def _validate_session12_log_selection(
    template: dict[str, Any],
    candidate_id: str,
    focused_binding: dict[str, Any],
    focused_text: str,
    full_binding: dict[str, Any],
    full_text: str,
) -> dict[str, dict[str, Any]]:
    _candidate_build_utc(candidate_id)
    template_bindings = template.get("projectBindings")
    if type(template_bindings) is not list or any(
        type(item) is not dict for item in template_bindings
    ):
        raise GenerationError("Session 12 template project bindings are malformed")
    template_by_role: dict[str, dict[str, Any]] = {}
    for item in template_bindings:
        role = item.get("role")
        if type(role) is not str or role in template_by_role:
            raise GenerationError("Session 12 template project binding roles are malformed")
        template_by_role[role] = item
    selected = {
        "FOCUS_INTRO_RELEASE_LOG": focused_binding,
        "CURRENT_FULL_AUTOMATION_LOG": full_binding,
    }
    for role, binding in selected.items():
        if role not in template_by_role:
            raise GenerationError(f"Session 12 template lacks automation role: {role}")
        if type(binding) is not dict or set(binding) != {"path", "bytes", "sha256"}:
            raise GenerationError(f"explicit Session 12 automation identity is malformed: {role}")
        relative = _canonical_relative(binding.get("path"), f"explicit {role} path")
        _candidate_log_name(relative, candidate_id, f"explicit {role}")
        pure = PurePosixPath(relative)
        if (
            len(pure.parts) < 3
            or pure.parts[:2] != ("Saved", "Logs")
            or pure.suffix != ".log"
            or type(binding.get("bytes")) is not int
            or binding["bytes"] <= 0
            or type(binding.get("sha256")) is not str
            or re.fullmatch(r"[0-9A-F]{64}", binding["sha256"]) is None
        ):
            raise GenerationError(f"explicit Session 12 automation identity differs: {role}")
        historical_path = template_by_role[role].get("path")
        if type(historical_path) is not str:
            raise GenerationError(f"Session 12 template automation path is malformed: {role}")
        if relative.casefold() == historical_path.casefold():
            raise GenerationError(
                f"explicit Session 12 automation log reuses historical template path: {role}"
            )
    if (
        focused_binding["path"].casefold() == full_binding["path"].casefold()
        or focused_binding["sha256"] == full_binding["sha256"]
    ):
        raise GenerationError("explicit Session 12 focused/full logs are not distinct")
    try:
        session12._validate_log(
            focused_text,
            session12.EXPLICIT_CANDIDATE_FOCUSED_LOG_TESTS,
            "explicit candidate intro focus automation",
            len(session12.EXPLICIT_CANDIDATE_FOCUSED_LOG_TESTS),
        )
        if focused_text.count("**** TEST COMPLETE. EXIT CODE: 0 ****") != 1:
            raise session12.ContractError(
                "explicit candidate intro focus automation did not exit cleanly"
            )
        session12._validate_full_automation_log(
            full_text, session12.EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS,
        )
    except session12.ContractError as exc:
        raise GenerationError(f"explicit Session 12 automation log invalid: {exc}") from exc
    return copy.deepcopy(selected)


def _session12_candidate_automation_bindings(
    template: dict[str, Any], candidate_id: str, focused_log: Path, full_log: Path,
) -> dict[str, dict[str, Any]]:
    focused_binding, focused_text = _candidate_automation_log(
        focused_log, "Session 12 focused automation log", candidate_id,
    )
    full_binding, full_text = _candidate_automation_log(
        full_log, "Session 12 full automation log", candidate_id,
    )
    return _validate_session12_log_selection(
        template, candidate_id,
        focused_binding, focused_text, full_binding, full_text,
    )


def _json_bytes(value: Any) -> bytes:
    try:
        return (
            json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
        ).encode("utf-8")
    except (TypeError, ValueError) as exc:
        raise GenerationError(f"policy is not strict JSON: {exc}") from exc


def _exclusive_publish(path: Path, payload: bytes) -> None:
    if _contains_reparse(path.parent) or not path.parent.is_dir():
        raise GenerationError("policy output parent is missing or contains a reparse point")
    temporary_name: str | None = None
    try:
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=".dgtour-candidate-policy-", suffix=".tmp", dir=path.parent,
        )
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        temporary = Path(temporary_name)
        if (
            temporary.stat().st_size != len(payload)
            or hashlib.sha256(_stable_bytes(temporary)).digest()
            != hashlib.sha256(payload).digest()
        ):
            raise GenerationError("staged policy identity verification failed")
        # Same-directory hard-link publication is atomic and exclusive.  It
        # cannot replace or truncate an existing candidate policy.
        os.link(temporary, path)
    except FileExistsError as exc:
        raise GenerationError(f"immutable candidate policy already exists: {path}") from exc
    except OSError as exc:
        raise GenerationError(f"exclusive candidate policy publication failed: {exc}") from exc
    finally:
        if temporary_name is not None:
            try:
                Path(temporary_name).unlink(missing_ok=True)
            except OSError:
                pass


def _publish_validated(path: Path, document: Any, issues: list[str]) -> dict[str, Any]:
    if issues:
        raise GenerationError("candidate policy validation failed: " + "; ".join(issues))
    payload = _json_bytes(document)
    _exclusive_publish(path, payload)
    try:
        displayed_path = path.relative_to(ROOT).as_posix()
    except ValueError:
        displayed_path = path.name
    return {
        "path": displayed_path,
        "bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest().upper(),
    }


def _replace_token(value: Any, old: str, new: str) -> Any:
    if type(value) is str:
        return value.replace(old, new)
    if type(value) is list:
        return [_replace_token(item, old, new) for item in value]
    if type(value) is dict:
        return {key: _replace_token(item, old, new) for key, item in value.items()}
    return copy.deepcopy(value)


def _candidate_id(value: str) -> str:
    _candidate_build_utc(value)
    return value


def _candidate_host_inputs(
    candidate_id: str,
    archive_value: Path,
    external_run_value: Path,
    user_dir_value: Path,
) -> tuple[Path, Path, Path, str]:
    archive = _absolute_host(archive_value, "archive", directory=True)
    external_run = _absolute_host(external_run_value, "external run", directory=True)
    user_dir = _absolute_host(user_dir_value, "UserDir", directory=True)
    if archive.name != "Windows" or archive.parent.name != candidate_id:
        raise GenerationError("archive must be the exact <candidateId>/Windows directory")
    prefix = candidate_id + "_"
    if not external_run.name.startswith(prefix):
        raise GenerationError("external run must be the exact candidate/token directory")
    token = external_run.name[len(prefix):]
    if UUID_RE.fullmatch(token) is None:
        raise GenerationError("external run token must be a canonical lowercase UUID")
    if user_dir.name != token:
        raise GenerationError("UserDir leaf must equal the external-run token")
    return archive, external_run, user_dir, token


def _policy_output(relative: str) -> Path:
    path, _ = _project_path(relative, "candidate policy output", must_exist=False)
    if os.path.lexists(path):
        raise GenerationError(f"immutable candidate policy already exists: {relative}")
    return path


def _refuse_existing_receipt(relative: str) -> None:
    path, _ = _project_path(relative, "candidate receipt output", must_exist=False)
    if os.path.lexists(path):
        raise GenerationError(
            f"candidate receipt already exists before its selected policy: {relative}"
        )


def _load_external_manifest(external_run: Path) -> tuple[dict[str, Any], str]:
    manifest_path = external_run / "technical-evidence-manifest.json"
    if _contains_reparse(manifest_path) or not manifest_path.is_file():
        raise GenerationError("external technical-evidence manifest is missing or irregular")
    data = _stable_bytes(manifest_path)
    manifest = _strict_json(data, "external technical-evidence manifest")
    if type(manifest) is not dict:
        raise GenerationError("external technical-evidence manifest root must be an object")
    return manifest, hashlib.sha256(data).hexdigest().upper()


def _upgrade_session10_policy(
    template: dict[str, Any],
    candidate_id: str,
    archive_identity: dict[str, Any],
    executable_identity: dict[str, Any],
    token: str,
    manifest_sha256: str,
    manifest_screenshots: list[dict[str, Any]],
    binding_resolver: Callable[[str, str], dict[str, Any]],
) -> dict[str, Any]:
    if (
        template.get("schema") != session10.LEGACY_POLICY_SCHEMA
        or template.get("schemaVersion") != 3
    ):
        raise GenerationError("Session 10 template must be the historical v3 policy")
    old_id = template.get("candidateId")
    if type(old_id) is not str or session10.CANDIDATE_ID_RE.fullmatch(old_id) is None:
        raise GenerationError("Session 10 template candidate is malformed")
    policy = _replace_token(template, old_id, candidate_id)
    policy["schema"] = session10.TRUSTED_POLICY_SCHEMA
    policy["schemaVersion"] = 4
    policy["trustedRuntimeJournalContract"] = copy.deepcopy(
        session10.TRUSTED_RUNTIME_JOURNAL_CONTRACT
    )
    policy["bindings"]["freshUserDirValidation"] = {
        "path": f"Evidence/Session19/FreshUserDir-{candidate_id}.json",
        "bytes": 1,
        "sha256": "0" * 64,
    }
    policy["bindings"] = {
        key: binding_resolver(binding["path"], f"Session 10 binding {key}")
        for key, binding in policy["bindings"].items()
    }
    policy["candidate"] = {
        "archiveRecoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
        "archiveFileCount": archive_identity["fileCount"],
        "archiveBytes": archive_identity["bytes"],
        "archiveCanonicalManifestSha256": archive_identity[
            "canonicalManifestSha256"
        ],
        "shippingExecutableRelativePath": executable_identity["relativePath"],
        "shippingExecutableBytes": executable_identity["bytes"],
        "shippingExecutableSha256": executable_identity["sha256"],
    }
    screenshot_by_path = {
        item.get("relativePath"): item
        for item in manifest_screenshots if type(item) is dict
    }
    selected: list[dict[str, Any]] = []
    for milestone, relative in SESSION10_SELECTED_SCREENSHOTS:
        item = screenshot_by_path.get(relative)
        if type(item) is not dict:
            raise GenerationError(f"external manifest lacks Session 10 screenshot: {relative}")
        selected.append({
            "milestone": milestone,
            "relativePath": relative,
            "bytes": item.get("bytes"),
            "sha256": str(item.get("sha256", "")).upper(),
            "width": item.get("width"),
            "height": item.get("height"),
        })
    policy["externalRuntimeEvidence"] = {
        "manifestFileName": "technical-evidence-manifest.json",
        "manifestSha256": manifest_sha256,
        "externalUserDirToken": token,
        "requiredScreenshotCount": len(manifest_screenshots),
        "selectedEnvironmentScreenshots": selected,
        "technicalArtifactCollectionOnly": True,
        "actualRenderedRhiAttested": False,
        "visualQualityApproved": False,
    }
    return policy


def _upgrade_session12_policy(
    template: dict[str, Any],
    candidate_id: str,
    archive_identity: dict[str, Any],
    executable_identity: dict[str, Any],
    binding_resolver: Callable[[str, str], dict[str, Any]],
    automation_bindings: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    if (
        template.get("schema") != session12.LEGACY_POLICY_SCHEMA
        or template.get("schemaVersion") != 2
    ):
        raise GenerationError("Session 12 template must be the historical v2 policy")
    candidate = template.get("candidate")
    old_id = candidate.get("candidateId") if type(candidate) is dict else None
    if type(old_id) is not str or session12.CANDIDATE_ID_RE.fullmatch(old_id) is None:
        raise GenerationError("Session 12 template candidate is malformed")
    if template.get("requiredCurrentLogTests") != session12.HISTORICAL_CURRENT_LOG_TESTS:
        raise GenerationError("Session 12 template current automation test set differs")
    if set(automation_bindings) != {
        "FOCUS_INTRO_RELEASE_LOG", "CURRENT_FULL_AUTOMATION_LOG",
    }:
        raise GenerationError("Session 12 explicit automation binding roles differ")
    policy = _replace_token(template, old_id, candidate_id)
    policy["schema"] = session12.TRUSTED_POLICY_SCHEMA
    policy["schemaVersion"] = 3
    policy["trustedRuntimeJournalContract"] = copy.deepcopy(
        session12.TRUSTED_RUNTIME_JOURNAL_CONTRACT
    )
    policy["candidate"] = {
        "candidateId": candidate_id,
        "archiveLeaf": "Windows",
        "externalRunTokenPattern": "{candidateId}_{uuid}",
        "archiveFileCount": archive_identity["fileCount"],
        "archiveBytes": archive_identity["bytes"],
        "archiveCanonicalManifestSha256": archive_identity[
            "canonicalManifestSha256"
        ],
        "shippingExeRelativePath": executable_identity["relativePath"],
        "shippingExeBytes": executable_identity["bytes"],
        "shippingExeSha256": executable_identity["sha256"],
    }
    refreshed_project_bindings: list[dict[str, Any]] = []
    for item in policy["projectBindings"]:
        role = item["role"]
        binding = automation_bindings.get(role)
        if binding is None:
            binding = binding_resolver(
                item["path"], f"Session 12 project binding {role}",
            )
        refreshed_project_bindings.append({"role": role, **copy.deepcopy(binding)})
    policy["projectBindings"] = refreshed_project_bindings
    policy["requiredCurrentLogTests"] = copy.deepcopy(
        session12.EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS
    )
    policy["sourceBindings"] = [
        {
            **binding_resolver(
                item["path"], f"Session 12 source binding {item['path']}",
            ),
            "markers": copy.deepcopy(item["markers"]),
        }
        for item in policy["sourceBindings"]
    ]
    return policy


def _archive_executable(archive: Path) -> tuple[Path, dict[str, Any]]:
    relative = "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
    executable = _absolute_host(
        archive.joinpath(*PurePosixPath(relative).parts),
        "Shipping executable",
        directory=False,
    )
    data = _stable_bytes(executable)
    return executable, {
        "relativePath": relative,
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest().upper(),
    }


def _session10_policy(args: argparse.Namespace) -> tuple[dict[str, Any], Path, str]:
    candidate_id = _candidate_id(args.candidate_id)
    archive, external_run, user_dir, token = _candidate_host_inputs(
        candidate_id, args.archive, args.external_run, args.user_dir,
    )
    unrealpak = _absolute_host(args.unrealpak, "UnrealPak", directory=False)
    if unrealpak.name.casefold() != "unrealpak.exe":
        raise GenerationError("UnrealPak filename must be UnrealPak.exe")
    output_relative = (
        "Config/DG_Session10EnvironmentShippingTechnicalPolicy-"
        f"{candidate_id}.json"
    )
    output = _policy_output(output_relative)
    receipt_relative = (
        f"Evidence/Session10/EnvironmentShippingTechnicalV2-{candidate_id}.json"
    )
    _refuse_existing_receipt(receipt_relative)
    template = _load_project_json(args.template.as_posix(), "Session 10 template")
    content_relative = f"Evidence/Session19/CandidateContent-{candidate_id}.json"
    content = _load_project_json(content_relative, "candidate content audit")
    archive_identity = content.get("archive")
    if type(archive_identity) is not dict:
        raise GenerationError("candidate content audit lacks archive identity")
    _, executable_identity = _archive_executable(archive)
    manifest, manifest_sha256 = _load_external_manifest(external_run)
    manifest_evidence = manifest.get("evidence")
    if type(manifest_evidence) is not dict:
        raise GenerationError("external manifest evidence must be an object")
    screenshots = manifest_evidence.get("screenshots")
    if type(screenshots) is not list:
        raise GenerationError("external manifest screenshots must be an array")
    policy = _upgrade_session10_policy(
        template,
        candidate_id,
        archive_identity,
        executable_identity,
        token,
        manifest_sha256,
        screenshots,
        _file_identity,
    )
    policy_issues, _ = session10.validate_policy(policy, ROOT, check_files=True)
    if policy_issues:
        raise GenerationError("Session 10 policy invalid: " + "; ".join(policy_issues))
    live_issues, _ = session10.validate_live(
        policy, ROOT, archive, unrealpak, external_run, user_dir,
    )
    if live_issues:
        raise GenerationError("Session 10 live validation failed: " + "; ".join(live_issues))
    return policy, output, receipt_relative


def _session12_policy(args: argparse.Namespace) -> tuple[dict[str, Any], Path, str]:
    candidate_id = _candidate_id(args.candidate_id)
    archive, external_run, user_dir, _ = _candidate_host_inputs(
        candidate_id, args.archive, args.external_run, args.user_dir,
    )
    output_relative = (
        "Config/DG_Session19Session12PresentationTechnicalPolicy-"
        f"{candidate_id}.json"
    )
    output = _policy_output(output_relative)
    receipt_relative = (
        "Evidence/Session19/Session12PresentationTechnicalAudioCoverage-"
        f"{candidate_id}.json"
    )
    _refuse_existing_receipt(receipt_relative)
    template = _load_project_json(args.template.as_posix(), "Session 12 template")
    automation_bindings = _session12_candidate_automation_bindings(
        template, candidate_id,
        args.focused_automation_log, args.full_automation_log,
    )
    content = _load_project_json(
        f"Evidence/Session19/CandidateContent-{candidate_id}.json",
        "candidate content audit",
    )
    archive_identity = content.get("archive")
    if type(archive_identity) is not dict:
        raise GenerationError("candidate content audit lacks archive identity")
    _, executable_identity = _archive_executable(archive)
    policy = _upgrade_session12_policy(
        template,
        candidate_id,
        archive_identity,
        executable_identity,
        _file_identity,
        automation_bindings,
    )
    try:
        session12._validate_policy(policy)
    except session12.ContractError as exc:
        raise GenerationError(f"Session 12 policy invalid: {exc}") from exc
    live_archive_identity = session12._collect_archive(archive)
    live_archive_summary = {
        key: live_archive_identity[key]
        for key in ("fileCount", "bytes", "canonicalManifestSha256")
    }
    content_archive_summary = {
        key: archive_identity.get(key)
        for key in ("fileCount", "bytes", "canonicalManifestSha256")
    }
    if live_archive_summary != content_archive_summary:
        raise GenerationError("live archive identity differs from candidate content audit")
    payload = _json_bytes(policy)
    overrides = {output.resolve(): payload}
    try:
        validator_result = session12._run_session12_validator()
        # This constructs but deliberately does not publish the v3 receipt.
        session12._candidate_receipt(
            candidate_id,
            archive,
            external_run,
            validator_result,
            live_archive_identity,
            overrides=overrides,
            policy_path=output,
            user_dir=user_dir,
        )
    except (
        session12.ContractError,
        FileNotFoundError,
        OSError,
        UnicodeError,
    ) as exc:
        raise GenerationError(f"Session 12 live validation failed: {exc}") from exc
    return policy, output, receipt_relative


def _dummy_binding(relative: str, _label: str) -> dict[str, Any]:
    return {"path": relative, "bytes": 1, "sha256": "A" * 64}


def run_self_test() -> tuple[bool, int, list[str]]:
    failures: list[str] = []
    checks = 0

    def check(name: str, action: Callable[[], bool]) -> None:
        nonlocal checks
        checks += 1
        try:
            passed = action()
        except Exception as exc:
            failures.append(f"{name}: {exc.__class__.__name__}: {exc}")
            return
        if not passed:
            failures.append(f"{name}: condition was false")

    candidate = "S19_WindowsShipping_20990101T000000Z_deadbeefcafe"
    archive_identity = {
        "fileCount": 31,
        "bytes": 123456,
        "canonicalManifestSha256": "B" * 64,
    }
    executable_identity = {
        "relativePath": "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe",
        "bytes": 98765,
        "sha256": "C" * 64,
    }
    session10_template = _load_project_json(
        SESSION10_TEMPLATE.as_posix(), "Session 10 self-test template",
    )
    fake_screenshots = [
        {
            "relativePath": relative,
            "bytes": 1000 + index,
            "sha256": f"{index + 1:064X}",
            "width": 1920,
            "height": 1080,
        }
        for index, (_, relative) in enumerate(SESSION10_SELECTED_SCREENSHOTS)
    ]
    # The policy contract requires an exact ten-image capture even though it
    # binds three environment milestones directly.
    fake_screenshots.extend({
        "relativePath": f"Screenshots/unused_{index}.png",
        "bytes": 2000 + index,
        "sha256": f"{index + 10:064X}",
        "width": 1920,
        "height": 1080,
    } for index in range(7))
    policy10 = _upgrade_session10_policy(
        session10_template,
        candidate,
        archive_identity,
        executable_identity,
        "12345678-1234-4abc-8def-1234567890ab",
        "D" * 64,
        fake_screenshots,
        _dummy_binding,
    )
    issues10, _ = session10.validate_policy(policy10, ROOT, check_files=False)
    check("Session 10 v4 policy", lambda: not issues10)
    check(
        "Session 10 candidate paths",
        lambda: (
            policy10["candidateId"] == candidate
            and policy10["bindings"]["freshUserDirValidation"]["path"]
            == f"Evidence/Session19/FreshUserDir-{candidate}.json"
        ),
    )
    forged10 = copy.deepcopy(policy10)
    forged10["trustedRuntimeJournalContract"]["runtimeJournalValidationState"] = "PASS_FORGED"
    forged10_issues, _ = session10.validate_policy(forged10, ROOT, check_files=False)
    check("Session 10 forged journal contract", lambda: bool(forged10_issues))

    session12_template = _load_project_json(
        SESSION12_TEMPLATE.as_posix(), "Session 12 self-test template",
    )
    def automation_success(test: str, index: int) -> str:
        return (
            "Test Completed. Result={Success} "
            f"Name={{Synthetic{index}}} Path={{{test}}}"
        )

    focused_text = "\n".join([
        f"Found {len(session12.EXPLICIT_CANDIDATE_FOCUSED_LOG_TESTS)} automation tests",
        *(
            automation_success(test, index)
            for index, test in enumerate(
                session12.EXPLICIT_CANDIDATE_FOCUSED_LOG_TESTS
            )
        ),
        "**** TEST COMPLETE. EXIT CODE: 0 ****",
    ])
    full_lines = [
        automation_success(test, index)
        for index, test in enumerate(session12.EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS)
    ]
    full_lines.extend(
        automation_success(f"DiscGolfTour.Synthetic.Full.Case{index}", index + 100)
        for index in range(255 - len(full_lines))
    )
    full_text = "\n".join([
        "Found 255 automation tests",
        *full_lines,
        "**** TEST COMPLETE. EXIT CODE: 0 ****",
    ])

    def synthetic_log_binding(path: str, text: str) -> dict[str, Any]:
        data = text.encode("utf-8")
        return {
            "path": path,
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest().upper(),
        }

    focused_binding = synthetic_log_binding(
        f"Saved/Logs/Session12Focused-{candidate}.log", focused_text,
    )
    full_binding = synthetic_log_binding(
        f"Saved/Logs/Session12Full-{candidate}.log", full_text,
    )
    automation_bindings = _validate_session12_log_selection(
        session12_template,
        candidate,
        focused_binding,
        focused_text,
        full_binding,
        full_text,
    )

    def candidate_id_refused(value: str) -> bool:
        try:
            _candidate_build_utc(value)
        except GenerationError:
            return True
        return False

    check(
        "candidate UTC derivation",
        lambda: _candidate_build_utc(candidate)
        == datetime(2099, 1, 1, tzinfo=timezone.utc),
    )
    for name, malformed_candidate in [
        ("candidate legacy synthetic token", "S19_WindowsShipping_Synthetic_20990101T000000Z_deadbeefcafe"),
        ("candidate missing timestamp", "S19_WindowsShipping_deadbeefcafe"),
        ("candidate invalid calendar", "S19_WindowsShipping_20990230T000000Z_deadbeefcafe"),
        ("candidate uppercase nonce", "S19_WindowsShipping_20990101T000000Z_DEADBEEFCAFE"),
        ("candidate suffix", "S19_WindowsShipping_20990101T000000Z_deadbeefcafe_extra"),
    ]:
        check(name, lambda value=malformed_candidate: candidate_id_refused(value))

    observation_candidate = "S19_WindowsShipping_20000101T000000Z_deadbeefcafe"
    observation_relative = (
        f"Saved/Logs/Session12Focused-{observation_candidate}.log"
    )
    observation_directory = tempfile.TemporaryDirectory(
        prefix="dgtour-abslog-generator-test-"
    )
    observed_absolute = (
        Path(observation_directory.name) / "selected-candidate.log"
    ).resolve()
    provenance_text = (
        f"-abslog={observed_absolute}\n"
        f'-abslog="{observed_absolute}"\n'
        "[2000.01.01-00.00.01:000][  0] fresh candidate automation"
    )
    observed_absolute.write_text(provenance_text, encoding="utf-8")
    _, observed_stat = _stable_snapshot(observed_absolute)

    def observation_refused(
        *,
        selected_candidate: str = observation_candidate,
        relative: str = observation_relative,
        absolute: Path = observed_absolute,
        snapshot_stat: os.stat_result = observed_stat,
        text: str = provenance_text,
    ) -> bool:
        try:
            _validate_candidate_log_observation(
                candidate_id=selected_candidate,
                relative=relative,
                absolute=absolute,
                observed_stat=snapshot_stat,
                text=text,
                label="synthetic candidate automation",
            )
        except GenerationError:
            return True
        return False

    check(
        "candidate log observation baseline",
        lambda: not observation_refused(),
    )
    check(
        "candidate log pre-build mtime",
        lambda: observation_refused(
            selected_candidate=candidate,
            relative=focused_binding["path"],
        ),
    )
    check(
        "candidate log copied stale internal time",
        lambda: observation_refused(
            text=provenance_text.replace(
                "[2000.01.01-00.00.01:000]",
                "[1999.12.31-23.59.59:999]",
            )
        ),
    )
    check(
        "candidate log renamed abslog mismatch",
        lambda: observation_refused(
            text=provenance_text.replace(
                str(observed_absolute), str(observed_absolute.with_name("stale.log"))
            )
        ),
    )
    check(
        "candidate log foreign filename scope",
        lambda: observation_refused(
            relative="Saved/Logs/Session12Focused-"
            "S19_WindowsShipping_20000101T000000Z_aaaaaaaaaaaa.log"
        ),
    )

    lexical_script = Path(__file__).absolute()
    selected_script = lexical_script.resolve(strict=True)
    selected_script_fingerprint = _stat_fingerprint(selected_script.stat())

    def abslog_identity_refused(
        text: str,
        fingerprint: tuple[int, int, int, int, int, int] = selected_script_fingerprint,
    ) -> bool:
        try:
            _validate_embedded_abslog_identity(
                text,
                selected_script,
                fingerprint,
                "synthetic alias provenance",
            )
        except GenerationError:
            return True
        return False

    check(
        "junction alias resolves to selected file",
        lambda: not abslog_identity_refused(
            f"-abslog={lexical_script}\n-abslog=\"{lexical_script}\"\n"
        ),
    )
    foreign_script = (ROOT / "Scripts/validate_dg_session19_session12_presentation_technical.py").resolve()
    check(
        "mixed abslog aliases are ambiguous",
        lambda: abslog_identity_refused(
            f"-abslog={lexical_script}\n-abslog={foreign_script}\n"
        ),
    )
    check(
        "foreign reparse-root abslog escape",
        lambda: abslog_identity_refused(
            "-abslog=C:\\DGTour\\Scripts\\"
            "validate_dg_session19_session12_presentation_technical.py\n"
        ),
    )
    check(
        "relative abslog path",
        lambda: abslog_identity_refused("-abslog=Saved/Logs/selected.log\n"),
    )
    check(
        "malformed quoted abslog",
        lambda: abslog_identity_refused(f'-abslog="{lexical_script}\n'),
    )
    stale_fingerprint = list(selected_script_fingerprint)
    stale_fingerprint[2] -= 1
    check(
        "abslog selected-file mutation",
        lambda: abslog_identity_refused(
            f"-abslog={lexical_script}\n", tuple(stale_fingerprint),
        ),
    )
    observation_directory.cleanup()

    class SyntheticSnapshotStat:
        def __init__(self, *, mtime_ns: int) -> None:
            self.st_size = 4
            self.st_mtime_ns = mtime_ns
            self.st_ctime_ns = 20
            self.st_dev = 30
            self.st_ino = 40

    class SyntheticSnapshotPath:
        def __init__(self, *, drift: bool) -> None:
            self.drift = drift
            self.stat_count = 0
            self.read_count = 0

        def stat(self) -> SyntheticSnapshotStat:
            self.stat_count += 1
            return SyntheticSnapshotStat(
                mtime_ns=10 + (1 if self.drift and self.stat_count > 1 else 0)
            )

        def read_bytes(self) -> bytes:
            self.read_count += 1
            return b"test"

        def __str__(self) -> str:
            return "synthetic-snapshot.log"

    stable_path = SyntheticSnapshotPath(drift=False)
    stable_data, stable_stat = _stable_snapshot(stable_path)  # type: ignore[arg-type]
    check(
        "single stable byte snapshot",
        lambda: (
            stable_data == b"test"
            and stable_stat.st_mtime_ns == 10
            and stable_path.read_count == 1
            and stable_path.stat_count == 2
        ),
    )

    def unstable_snapshot_refused() -> bool:
        try:
            _stable_snapshot(  # type: ignore[arg-type]
                SyntheticSnapshotPath(drift=True)
            )
        except GenerationError:
            return True
        return False

    check("unstable byte snapshot", unstable_snapshot_refused)
    policy12 = _upgrade_session12_policy(
        session12_template,
        candidate,
        archive_identity,
        executable_identity,
        _dummy_binding,
        automation_bindings,
    )
    try:
        session12._validate_policy(policy12)
    except session12.ContractError as exc:
        failures.append(f"Session 12 v3 policy: {exc}")
    checks += 1
    check(
        "Session 12 refreshed source identities",
        lambda: all(
            item["bytes"] == 1 and item["sha256"] == "A" * 64
            for item in policy12["sourceBindings"]
        ),
    )
    check(
        "Session 12 explicit automation identities",
        lambda: (
            next(
                item for item in policy12["projectBindings"]
                if item["role"] == "FOCUS_INTRO_RELEASE_LOG"
            ) == {"role": "FOCUS_INTRO_RELEASE_LOG", **focused_binding}
            and next(
                item for item in policy12["projectBindings"]
                if item["role"] == "CURRENT_FULL_AUTOMATION_LOG"
            ) == {"role": "CURRENT_FULL_AUTOMATION_LOG", **full_binding}
            and policy12["requiredCurrentLogTests"]
            == session12.EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS
        ),
    )

    def selection_refused(
        focus: dict[str, Any] = focused_binding,
        focus_log: str = focused_text,
        full: dict[str, Any] = full_binding,
        full_log: str = full_text,
    ) -> bool:
        try:
            _validate_session12_log_selection(
                session12_template, candidate,
                focus, focus_log, full, full_log,
            )
        except GenerationError:
            return True
        return False

    stale_focus = copy.deepcopy(focused_binding)
    stale_focus["path"] = session12.HISTORICAL_TEMPLATE_AUTOMATION_LOGS[
        "FOCUS_INTRO_RELEASE_LOG"
    ]
    check(
        "Session 12 stale focused template path",
        lambda: selection_refused(focus=stale_focus),
    )
    stale_full = copy.deepcopy(full_binding)
    stale_full["path"] = session12.HISTORICAL_TEMPLATE_AUTOMATION_LOGS[
        "CURRENT_FULL_AUTOMATION_LOG"
    ]
    check(
        "Session 12 stale full template path",
        lambda: selection_refused(full=stale_full),
    )
    duplicate_path = copy.deepcopy(full_binding)
    duplicate_path["path"] = focused_binding["path"]
    check(
        "Session 12 duplicate log path",
        lambda: selection_refused(full=duplicate_path),
    )
    duplicate_hash = copy.deepcopy(full_binding)
    duplicate_hash["sha256"] = focused_binding["sha256"]
    check(
        "Session 12 duplicate log hash",
        lambda: selection_refused(full=duplicate_hash),
    )
    malformed_focus = copy.deepcopy(focused_binding)
    malformed_focus["bytes"] = True
    check(
        "Session 12 malformed focused identity",
        lambda: selection_refused(focus=malformed_focus),
    )
    missing_lifecycle_focus = focused_text.replace(
        automation_success(
            "DiscGolfTour.Session19.Input.HoleIntroThrowTimingLifecycle", 1,
        ),
        "lifecycle success removed",
    )
    check(
        "Session 12 focused lifecycle omission",
        lambda: selection_refused(focus_log=missing_lifecycle_focus),
    )
    missing_camera_full = full_text.replace(
        automation_success(
            "DiscGolfTour.Presentation.PersistentSemanticProxyCollisionContract",
            len(session12.EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS) - 1,
        ),
        "camera success removed",
    )
    check(
        "Session 12 full camera omission",
        lambda: selection_refused(full_log=missing_camera_full),
    )
    failed_full = full_text.replace(
        "Test Completed. Result={Success}",
        "Test Completed. Result={Fail}",
        1,
    )
    check(
        "Session 12 failed full result",
        lambda: selection_refused(full_log=failed_full),
    )
    forged12 = copy.deepcopy(policy12)
    forged12["trustedRuntimeJournalContract"]["runtimeJournalValidationState"] = "PASS_FORGED"
    try:
        session12._validate_policy(forged12)
    except session12.ContractError:
        forged12_rejected = True
    else:
        forged12_rejected = False
    check("Session 12 forged journal contract", lambda: forged12_rejected)

    with tempfile.TemporaryDirectory(prefix="dgtour-policy-generator-test-") as temporary:
        root = Path(temporary)
        created = root / "policy.json"
        result = _publish_validated(created, policy10, [])
        original = created.read_bytes()
        check(
            "exclusive create",
            lambda: result["bytes"] == len(original),
        )

        def collision_refused() -> bool:
            try:
                _publish_validated(created, policy12, [])
            except GenerationError:
                return created.read_bytes() == original
            return False

        check("collision refuses overwrite", collision_refused)
        tamper_output = root / "tampered.json"

        def tamper_no_output() -> bool:
            try:
                _publish_validated(tamper_output, forged10, forged10_issues)
            except GenerationError:
                return not os.path.lexists(tamper_output)
            return False

        check("tamper has no partial output", tamper_no_output)
        strict_output = root / "strict.json"

        def strict_no_output() -> bool:
            try:
                _publish_validated(strict_output, {"value": float("nan")}, [])
            except GenerationError:
                return not os.path.lexists(strict_output)
            return False

        check("serialization has no partial output", strict_no_output)

        def traversal_refused() -> bool:
            try:
                _absolute_host(
                    root / "untrusted" / ".." / "input.json",
                    "self-test input",
                    directory=False,
                )
            except GenerationError as exc:
                return "parent traversal" in str(exc)
            return False

        check("host traversal", traversal_refused)
        clean_tree = root / "clean-tree"
        clean_tree.mkdir()
        (clean_tree / "nested").mkdir()
        check(
            "clean host tree",
            lambda: _absolute_host(
                clean_tree, "self-test clean tree", directory=True,
            ) == clean_tree.resolve(),
        )

        def descendant_reparse_refused() -> bool:
            flagged = clean_tree / "nested"
            original_is_reparse = globals()["_is_reparse"]

            def simulated_is_reparse(path: Path) -> bool:
                return path == flagged or original_is_reparse(path)

            globals()["_is_reparse"] = simulated_is_reparse
            try:
                _absolute_host(
                    clean_tree, "self-test reparse tree", directory=True,
                )
            except GenerationError as exc:
                return "descendant reparse point" in str(exc)
            finally:
                globals()["_is_reparse"] = original_is_reparse
            return False

        check("descendant host reparse", descendant_reparse_refused)
        check(
            "temporary cleanup",
            lambda: not list(root.glob(".dgtour-candidate-policy-*.tmp")),
        )
    return not failures, checks, failures


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    subparsers = parser.add_subparsers(dest="command")
    policy10 = subparsers.add_parser("session10")
    policy10.add_argument("--candidate-id", required=True)
    policy10.add_argument("--archive", type=Path, required=True)
    policy10.add_argument("--external-run", type=Path, required=True)
    policy10.add_argument("--user-dir", type=Path, required=True)
    policy10.add_argument("--unrealpak", type=Path, required=True)
    policy10.add_argument("--template", type=Path, default=SESSION10_TEMPLATE)
    policy10.add_argument("--preflight-only", action="store_true")
    policy12 = subparsers.add_parser("session12")
    policy12.add_argument("--candidate-id", required=True)
    policy12.add_argument("--archive", type=Path, required=True)
    policy12.add_argument("--external-run", type=Path, required=True)
    policy12.add_argument("--user-dir", type=Path, required=True)
    policy12.add_argument("--template", type=Path, default=SESSION12_TEMPLATE)
    policy12.add_argument(
        "--focused-automation-log", type=Path, required=True,
        help=(
            "fresh project-relative Saved/Logs file containing the exact "
            "HoleIntroConfirmIsolation and HoleIntroThrowTimingLifecycle run; "
            "filename must end -<candidate-id>.log and the run must postdate it"
        ),
    )
    policy12.add_argument(
        "--full-automation-log", type=Path, required=True,
        help=(
            "fresh project-relative Saved/Logs full DiscGolfTour automation log "
            "covering the candidate camera and intro regressions; filename must "
            "end -<candidate-id>.log and the run must postdate it"
        ),
    )
    policy12.add_argument("--preflight-only", action="store_true")
    return parser


def main() -> int:
    parser = _parser()
    args = parser.parse_args()
    if args.self_test:
        if args.command is not None:
            parser.error("--self-test cannot be combined with a generation command")
        passed, checks, failures = run_self_test()
        if not passed:
            print("SESSION 19 CANDIDATE TECHNICAL POLICY GENERATOR SELF-TEST FAILED")
            for failure in failures:
                print(f" - {failure}")
            return 1
        print(
            "SESSION 19 CANDIDATE TECHNICAL POLICY GENERATOR SELF-TEST PASS "
            f"checks={checks}"
        )
        return 0
    if args.command is None:
        parser.error("choose session10 or session12 (or use --self-test)")
    try:
        policy, output, receipt_relative = (
            _session10_policy(args)
            if args.command == "session10"
            else _session12_policy(args)
        )
        payload = _json_bytes(policy)
        identity = {
            "path": output.relative_to(ROOT).as_posix(),
            "bytes": len(payload),
            "sha256": hashlib.sha256(payload).hexdigest().upper(),
        }
        if not args.preflight_only:
            # Close the long live-audit window as far as possible: a receipt
            # created while validation ran prevents policy publication.
            _refuse_existing_receipt(receipt_relative)
            identity = _publish_validated(output, policy, [])
    except (GenerationError, OSError, ValueError, KeyError, TypeError) as exc:
        print(f"SESSION 19 CANDIDATE TECHNICAL POLICY GENERATION REFUSED: {exc}")
        return 1
    state = "PREFLIGHT PASS" if args.preflight_only else "APPEND-ONLY CREATE PASS"
    print(
        f"SESSION 19 CANDIDATE TECHNICAL POLICY {state} "
        f"lane={args.command} candidate={args.candidate_id} "
        f"path={identity['path']} bytes={identity['bytes']} "
        f"sha256={identity['sha256']} release_ready=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
