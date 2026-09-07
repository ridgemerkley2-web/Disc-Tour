#!/usr/bin/env python3
"""Keep digest-verified files byte-stable against .gitattributes normalization.

Frozen contracts and receipts record the SHA-256 of files as the authoring run
wrote them. `.gitattributes` rewrites line endings on checkout in both directions
-- `*.json text eol=lf` and friends force LF, while the bare `* text=auto` rule
yields CRLF on Windows for any extension without an explicit rule -- so a file can
reach a clone with different bytes than its recorded digest describes. The content
is intact; the digest simply cannot be reproduced, and the gate that hashes it
reports drift that no source change explains.

This audit finds every tracked file whose recorded digest matches only after
reconstructing the other line-ending form, and reports it as needing a `-text`
pin. With --fix it writes the file back in the recorded byte form and appends the
pin. Run it after adding any file whose SHA-256 a contract records.

Exits 0 when nothing needs pinning.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
GITATTRIBUTES = ROOT / ".gitattributes"
DIGEST_RE = re.compile(r"\b([0-9A-Fa-f]{64})\b")

# Files larger than this are packaged archives or art, never hand-authored text
# whose line endings git would rewrite.
SIZE_LIMIT = 8 * 1024 * 1024
LFS_POINTER_PREFIX = b"version https://git-lfs"

PIN_HEADER = (
    "# Digest-pinned evidence. Frozen session receipts record the SHA-256 and byte",
    "# count of these files as they were written by the authoring run. The rules above",
    "# rewrite line endings on checkout (json/py/md to LF, tsv/log/txt to CRLF via",
    "# `text=auto`), which changes their bytes and makes the recorded digests",
    "# unverifiable from a fresh clone. Store them verbatim instead.",
)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def continuation_lf(raw: bytes) -> bytes:
    """Undo CRLF promotion of the interior line breaks in a multi-line message.

    Unreal terminates each log message with CRLF but writes the message body
    verbatim, so a multi-line message carries bare LF inside it. `text=auto`
    round-trips that lossily: the blob is stored fully LF and checkout promotes
    every LF back to CRLF, gaining one byte per interior break. Neither whole-file
    transform can express the original, so a mixed-ending file is invisible to an
    LF/CRLF flip -- which is why the two Session 18 logs looked for a while like
    content drift rather than a normalization artefact.

    Continuation lines are the ones beginning with a tab or a space.
    """
    parts = raw.split(b"\r\n")
    out = bytearray()
    for index, segment in enumerate(parts[:-1]):
        out += segment
        out += b"\n" if parts[index + 1][:1] in (b"\t", b" ") else b"\r\n"
    out += parts[-1]
    return bytes(out)


def tracked_files() -> list[str]:
    done = subprocess.run(
        ["git", "-C", str(ROOT), "ls-files", "-z"],
        capture_output=True, check=True,
    )
    return [p for p in done.stdout.decode("utf-8").split("\0") if p]


def recorded_digests(paths: list[str]) -> set[str]:
    """Every SHA-256 literal written anywhere in the tree."""
    digests: set[str] = set()
    for rel in paths:
        path = ROOT / rel
        try:
            if not path.is_file() or path.stat().st_size > SIZE_LIMIT:
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        digests.update(match.upper() for match in DIGEST_RE.findall(text))
    return digests


def pinned_paths() -> set[str]:
    if not GITATTRIBUTES.is_file():
        return set()
    pinned = set()
    for line in GITATTRIBUTES.read_text(encoding="utf-8").splitlines():
        if line.endswith(" -text") and "filter=lfs" not in line:
            pinned.add(line[: -len(" -text")])
    return pinned


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fix", action="store_true",
        help="rewrite affected files to their recorded byte form and pin them",
    )
    args = parser.parse_args()

    paths = tracked_files()
    digests = recorded_digests(paths)
    already = pinned_paths()
    print(f"Tracked files: {len(paths)}")
    print(f"Recorded SHA-256 literals: {len(digests)}")
    print(f"Already pinned: {len(already)}\n")

    verified, needs_pin, unpinned_ok = [], [], []
    for rel in paths:
        path = ROOT / rel
        try:
            if not path.is_file() or path.stat().st_size > SIZE_LIMIT:
                continue
            raw = path.read_bytes()
        except OSError:
            continue
        if raw.startswith(LFS_POINTER_PREFIX):
            continue
        if sha(raw) in digests:
            verified.append(rel)
            continue
        lf = raw.replace(b"\r\n", b"\n")
        crlf = lf.replace(b"\n", b"\r\n")
        for form in (lf, crlf, continuation_lf(raw)):
            if form != raw and sha(form) in digests:
                (unpinned_ok if rel in already else needs_pin).append((rel, form))
                break

    print(f"Files verifying as checked out: {len(verified)}")
    print(f"Files needing a -text pin:      {len(needs_pin)}")
    if unpinned_ok:
        print(f"Pinned but still not verbatim:  {len(unpinned_ok)} "
              f"(the pin is set but the stored bytes are the wrong form)")

    todo = needs_pin + unpinned_ok
    if not todo:
        print("\nAll digest-verified files are byte-stable.")
        return 0

    for rel, _ in todo:
        print(f"  {rel}")

    if not args.fix:
        print("\nRe-run with --fix to restore the recorded byte form and pin these.")
        return 1

    for rel, form in todo:
        (ROOT / rel).write_bytes(form)
    fresh = sorted(rel for rel, _ in needs_pin)
    if fresh:
        text = GITATTRIBUTES.read_text(encoding="utf-8").rstrip("\n")
        if PIN_HEADER[0] not in text:
            text += "\n" + "\n".join(("",) + PIN_HEADER)
        text += "\n" + "\n".join(f"{rel} -text" for rel in fresh) + "\n"
        GITATTRIBUTES.write_text(text, encoding="utf-8")
    print(f"\nRestored {len(todo)} files; pinned {len(fresh)} new paths.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
