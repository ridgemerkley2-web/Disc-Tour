#!/usr/bin/env python3
"""Read-only audit of an authoring host for the trees this repository cannot carry.

The published history is complete for everything git tracks, but three classes of
material live outside it and exist only on the machine that ran the sessions:

  1. the two externally quarantined trees, moved out of the project root on
     2026-08-25 with their destinations deliberately unrecorded;
  2. the ``Saved/`` evidence tree and the ``_BuildKit`` donor kit, both gitignored,
     which every frozen session gate hashes;
  3. the packaged archives under the recorded package root.

Run this ON the authoring host. It locates those trees, verifies whatever it finds
against the manifests committed to this repository, and reports what is provably
intact, what has drifted, and what is absent. It also reports whether the authoring
git repository holds work the published history never received.

Nothing is written outside the report path, and nothing outside this repository is
modified, moved, or deleted. Pass --json to emit a machine-readable report.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]

FRAMEWORK_MANIFEST = (
    ROOT / "Evidence/Session19"
    / "CharacterFrameworkBeforeMove-S19_Framework_20260825T031617Z_c68ee2e4e6be.tsv"
)
FAB_MANIFEST = ROOT / "Evidence/Session19/QuarantinedFabBeforeMove.tsv"

# Scripts/quarantine-session19-character-framework.ps1 declares this root, and the
# receipt's recovery token supplies the run id below it. The receipt itself records
# hostPathRecorded: false, so this is the only surviving pointer to the destination.
FRAMEWORK_QUARANTINE_ROOT = Path(r"C:\DGTour_Quarantine")
FRAMEWORK_RUN_ID = "S19_Framework_20260825T031617Z_c68ee2e4e6be"
FRAMEWORK_ROOT_DIRS = {
    "project_plugin": "ProjectPlugin/DiscGolfCharacterFramework",
    "buildkit_plugin": "BuildKitPlugin/DiscGolfCharacterFramework",
}

# The Fab receipt's recovery token carries a trailing "Content" segment, and its
# recoveryRelativePath values sit directly beneath it.
FAB_RUN_ID = "Session19ReleaseScope_20260824_01/Content"
FAB_ROOT_DIRS = {
    "fab_project_nature_spruce_forest": "PN_interactiveSpruceForest",
    "fab_greenbuggames_stump_scanned": "Stump_Scanned",
    "fab_tharlevfx_water_materials": "WaterMaterials",
}

# Recorded in the Session 19 evidence as the authoring project and package roots.
AUTHORING_PROJECT_ROOT = Path(r"C:\DGTour")
PACKAGE_ROOT = Path(r"C:\DGTour_Packages")

# Gitignored trees that every frozen gate hashes. Relative to the authoring project.
UNVERSIONED_TREES = ("Saved", "_BuildKit")

PUBLISHED_HEAD = "4873cb1efb3a5c105fdfa05e472251005e5edadc"


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


MANIFEST_FIELDS = ("rootId", "relativePath", "bytes", "sha256")


def read_manifest(path: Path) -> list[dict[str, str]]:
    """Read a before-move manifest: rootId, relativePath, bytes, sha256.

    The two committed manifests disagree on format -- the character-framework one
    carries a quoted header row, the Fab one has no header at all -- so detect
    rather than assume.
    """
    if not path.is_file():
        return []
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.reader(handle, delimiter="\t"))
    if not rows:
        return []
    if [cell.strip('"') for cell in rows[0]] == list(MANIFEST_FIELDS):
        rows = rows[1:]
    return [
        dict(zip(MANIFEST_FIELDS, [cell.strip('"') for cell in row]))
        for row in rows if len(row) >= len(MANIFEST_FIELDS)
    ]


def verify_tree(base: Path, rows: list[dict[str, str]], root_dirs: dict[str, str]):
    """Compare a recovered tree against its manifest rows.

    A file counts as intact only when its SHA-256 matches. Line-ending
    normalization is tolerated in both directions, because a file that round-trips
    through a text-normalizing transport is unchanged in content even though its
    digest moves.
    """
    result = {
        "base": str(base),
        "basePresent": base.is_dir(),
        "roots": {},
        "intact": 0,
        "drifted": [],
        "absent": [],
        "unexpected": [],
    }
    if not base.is_dir():
        result["absent"] = [r["relativePath"] for r in rows]
        return result

    by_root: dict[str, list[dict[str, str]]] = {}
    for row in rows:
        by_root.setdefault(row["rootId"], []).append(row)

    for root_id, rows_for_root in sorted(by_root.items()):
        sub = root_dirs.get(root_id)
        root_base = base / sub if sub else base
        seen = set()
        root_result = {
            "path": str(root_base),
            "present": root_base.is_dir(),
            "expectedFiles": len(rows_for_root),
            "expectedBytes": sum(int(r["bytes"]) for r in rows_for_root),
            "intact": 0,
        }
        for row in rows_for_root:
            target = root_base / row["relativePath"]
            seen.add(target.resolve() if target.exists() else target)
            if not target.is_file():
                result["absent"].append(f"{root_id}:{row['relativePath']}")
                continue
            want = row["sha256"].upper()
            raw = target.read_bytes()
            got = hashlib.sha256(raw).hexdigest().upper()
            if got != want:
                lf = raw.replace(b"\r\n", b"\n")
                crlf = lf.replace(b"\n", b"\r\n")
                if want in {
                    hashlib.sha256(lf).hexdigest().upper(),
                    hashlib.sha256(crlf).hexdigest().upper(),
                }:
                    got = want  # same content, different line endings in transit
            if got == want:
                root_result["intact"] += 1
                result["intact"] += 1
            else:
                result["drifted"].append({
                    "file": f"{root_id}:{row['relativePath']}",
                    "expectedBytes": int(row["bytes"]),
                    "actualBytes": len(raw),
                })
        if root_base.is_dir():
            for found in root_base.rglob("*"):
                if found.is_file() and found not in seen:
                    result["unexpected"].append(str(found.relative_to(base)))
        result["roots"][root_id] = root_result
    return result


def git(repo: Path, *args: str) -> str | None:
    try:
        done = subprocess.run(
            ["git", "-C", str(repo), *args],
            capture_output=True, text=True, check=False,
        )
    except OSError:
        return None
    return done.stdout.strip() if done.returncode == 0 else None


def audit_authoring_repo(repo: Path) -> dict:
    """Report work the authoring repository holds that the published history lacks."""
    result = {"path": str(repo), "present": repo.is_dir(), "isGitRepo": False}
    if not repo.is_dir():
        return result
    head = git(repo, "rev-parse", "HEAD")
    if head is None:
        return result
    result.update({
        "isGitRepo": True,
        "head": head,
        "headIsPublished": head == PUBLISHED_HEAD,
        "uncommittedChanges": len([
            line for line in (git(repo, "status", "--porcelain") or "").splitlines()
            if line.strip()
        ]),
        "stashes": len((git(repo, "stash", "list") or "").splitlines()),
        "branches": (git(repo, "branch", "--format=%(refname:short)") or "").splitlines(),
    })
    unpublished = git(repo, "log", "--oneline", f"{PUBLISHED_HEAD}..HEAD")
    result["commitsNotInPublishedHistory"] = (
        unpublished.splitlines() if unpublished else []
    )
    return result


def describe_tree(path: Path) -> dict:
    """Count a tree without hashing it -- these are logs and archives, not evidence."""
    if not path.is_dir():
        return {"path": str(path), "present": False}
    files = 0
    total = 0
    for entry in path.rglob("*"):
        try:
            if entry.is_file():
                files += 1
                total += entry.stat().st_size
        except OSError:
            continue
    return {"path": str(path), "present": True, "files": files, "bytes": total}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project-root", type=Path, default=AUTHORING_PROJECT_ROOT,
        help="the authoring project tree (default: the recorded C:\\DGTour)",
    )
    parser.add_argument(
        "--quarantine-root", type=Path, default=FRAMEWORK_QUARANTINE_ROOT,
        help="quarantine root declared by the Session 19 move script",
    )
    parser.add_argument(
        "--package-root", type=Path, default=PACKAGE_ROOT,
        help="packaged archive root recorded in the Session 19 evidence",
    )
    parser.add_argument("--json", type=Path, help="write the full report here")
    args = parser.parse_args()

    framework_rows = read_manifest(FRAMEWORK_MANIFEST)
    fab_rows = read_manifest(FAB_MANIFEST)
    if not framework_rows:
        print(f"FAIL: manifest is missing or empty: {FRAMEWORK_MANIFEST}")
        return 2

    framework_base = args.quarantine_root / FRAMEWORK_RUN_ID
    fab_base = args.quarantine_root / FAB_RUN_ID

    report = {
        "authoringRepo": audit_authoring_repo(args.project_root),
        "characterFramework": verify_tree(
            framework_base, framework_rows, FRAMEWORK_ROOT_DIRS),
        "quarantinedFab": verify_tree(fab_base, fab_rows, FAB_ROOT_DIRS)
        if fab_rows else {"base": str(fab_base), "basePresent": False,
                          "note": "manifest absent from this checkout"},
        "unversionedTrees": {
            name: describe_tree(args.project_root / name)
            for name in UNVERSIONED_TREES
        },
        "packageRoot": describe_tree(args.package_root),
    }

    def summarize(title: str, block: dict, rows: list[dict[str, str]]) -> None:
        expected = len(rows)
        intact = block.get("intact", 0)
        drifted = len(block.get("drifted", []))
        absent = len(block.get("absent", []))
        state = "ABSENT" if not block.get("basePresent") else (
            "INTACT" if intact == expected else "PARTIAL")
        print(f"{title}: {state}")
        print(f"  location        {block.get('base')}")
        print(f"  verified intact {intact}/{expected}")
        if drifted:
            print(f"  drifted         {drifted}")
            for entry in block["drifted"][:10]:
                print(f"    {entry['file']} "
                      f"(recorded {entry['expectedBytes']}B, found {entry['actualBytes']}B)")
            if drifted > 10:
                print(f"    ... and {drifted - 10} more")
        if absent:
            print(f"  absent          {absent}")
        if block.get("unexpected"):
            print(f"  not in manifest {len(block['unexpected'])}")

    print(f"Authoring-host recovery audit (read-only)\nRepository: {ROOT}\n")

    repo = report["authoringRepo"]
    if not repo["present"]:
        print(f"Authoring project: ABSENT at {repo['path']}")
        print("  This is not the authoring host, or the tree moved. "
              "Re-run with --project-root.")
    elif not repo["isGitRepo"]:
        print(f"Authoring project: present at {repo['path']}, but not a git repository")
    else:
        print(f"Authoring project: present at {repo['path']}")
        print(f"  HEAD {repo['head'][:12]}"
              f"{' (matches published history)' if repo['headIsPublished'] else ''}")
        print(f"  uncommitted changes {repo['uncommittedChanges']}")
        print(f"  stashes             {repo['stashes']}")
        unpublished = repo["commitsNotInPublishedHistory"]
        print(f"  commits not published {len(unpublished)}")
        for line in unpublished[:10]:
            print(f"    {line}")
    print()

    summarize("Character framework quarantine", report["characterFramework"],
              framework_rows)
    print()
    if fab_rows:
        summarize("Fab content quarantine", report["quarantinedFab"], fab_rows)
        print()

    for name, block in report["unversionedTrees"].items():
        if block["present"]:
            print(f"{name}/: present, {block['files']} files, {block['bytes']:,} bytes")
        else:
            print(f"{name}/: ABSENT at {block['path']}")
    pkg = report["packageRoot"]
    print(f"Package root: "
          + (f"present, {pkg['files']} files, {pkg['bytes']:,} bytes"
             if pkg["present"] else f"ABSENT at {pkg['path']}"))

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"\nReport written to {args.json}")

    recoverable = (
        report["characterFramework"].get("basePresent")
        or report["unversionedTrees"]["Saved"]["present"]
    )
    return 0 if recoverable else 1


if __name__ == "__main__":
    sys.exit(main())
