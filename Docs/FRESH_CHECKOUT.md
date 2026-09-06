# Working from a fresh checkout

The repository is not self-contained. Several gates verify evidence that
`.gitignore` deliberately excludes, and one accepted policy requires files to be
absent. Read this before "repairing" anything a validator reports missing.

## What a clean clone can and cannot prove

`python Scripts/reference_flight_check.py` passes on a clean clone and is a real
result: the flight envelope, mirrored RHBH/LHBH and RHFH/LHFH behaviour, release
model, and ground model are all reproduced from source.

`python Scripts/validate_project.py` fails on a clean clone and cannot pass
there. It ends with a checkout note counting three groups:

- **results naming unversioned evidence** - gates that hash build logs, packaged
  archives under `C:\DGTour_Packages`, or `Saved/SaveGames/DiscGolfTour_Profile_0.sav`.
  `Saved/` and `_BuildKit/` are gitignored, so these can only be evaluated on the
  authoring tree.
- **results naming paths Session 19 requires to be absent** - see below.
- **remaining results** - everything else. These are the ones worth reading.

Nothing is suppressed; the counts are diagnostic only.

## Paths that must stay absent

Two Session 19 decisions make frozen gates name paths that no longer exist. Both
are accepted policy, not damage.

**The character framework plugin.** Session 19 moved
`Plugins/DiscGolfCharacterFramework` and its `_BuildKit` copy outside the project
root: 513 files, 106,364,819 bytes, recoverable, host path deliberately not
recorded, recovery token `DGTOUR_EXTERNAL_QUARANTINE/S19_Framework_20260825T031617Z_c68ee2e4e6be`.
The receipt is `Evidence/Session19/CharacterFrameworkExternalQuarantine-S19_Framework_20260825T031617Z_c68ee2e4e6be.json`
(`projectSourceRemovalAccepted: true`, `mustBeOutsideProjectRoot: true`,
`deleted: false`). The runtime no longer references the plugin module; its
replacement script package is `/Script/DiscGolfRuntimeFoundation`.

Restoring the plugin into the tree clears eight Session 9/14 errors and then
fails the Session 9 brand gate, because the last committed copy predates the
Session 9 genericization sweep and still carries `premium_*` brand identifiers.
Do not restore it. `shippingPackageAbsenceSatisfied` and `blockerClosed` are both
`false`; that closure needs the quarantined copy and a rights decision, not a
`git checkout` of an old revision.

**Relocated DeveloperTool files.** Session 19 moved the Throw Lab, career/AI, and
vertical-slice sources from `Source/DiscGolfTour/` to
`Source/DiscGolfTourDeveloper/`, which is denied in Shipping. Frozen gates still
name the old paths; `SESSION19_DEVELOPER_RELOCATIONS` in `validate_project.py`
maps them, and the supersession that clears them needs the unversioned Session 19
evidence to run.

## Line endings on digest-verified evidence

Frozen receipts record the SHA-256 and byte count of evidence files as the
authoring run wrote them. `.gitattributes` normalizes line endings in both
directions - `*.json text eol=lf` forces LF, and the bare `* text=auto` rule
yields CRLF for `.tsv`/`.log`/`.txt` on Windows - so a normalized file can no
longer reproduce its recorded digest even though its content is intact.

Ninety-two such paths are now pinned with `-text` and stored in the byte form
their digests describe. **When you add a file whose SHA-256 is recorded in a
contract or receipt, pin it the same way**, or the digest becomes unverifiable
from any clone.

Two Session 18 logs remain unresolved: `Evidence/Session18/PolyHavenReimport.log`
and `Evidence/Session18/PackagedPineRidgePlaySmoke.log` are 6 and 2 bytes larger
than their recorded digests describe, and the difference is not a whole-file
line-ending transform. Both contracts also record a separate larger "raw" byte
count, so the recorded values are a processed form whose exact derivation is not
reproducible from this tree.

## Original authoring tree

Session 19 evidence records the authoring project root as `C:\DGTour` and its
packages as `C:\DGTour_Packages\...`. The sessions ran on a **separate machine**;
a clone alone never has them, and neither did the machine this documentation was
written from. Confirmed there on 2026-09-06 by whole-disk search: no `C:\DGTour`,
no `C:\DGTour_Quarantine`, no `.uplugin` file anywhere on the disk, no Unreal
Engine installation, and no agent session touching this project before the day
the repository was published.

The published history is complete for everything git tracks. Three classes of
material stay on the authoring host:

1. the two quarantined trees — the character framework
   (`Evidence/Session19/CharacterFrameworkBeforeMove-*.tsv`, 513 files) and the
   unused Fab content (`Evidence/Session19/QuarantinedFabBeforeMove.tsv`, 533
   files / 2.34 GB), both moved under the `C:\DGTour_Quarantine` root that
   `Scripts/quarantine-session19-character-framework.ps1` declares;
2. `Saved/` and `_BuildKit/`, gitignored, hashed by every frozen gate;
3. the packaged archives under `C:\DGTour_Packages`.

Run `python Scripts/audit_dg_authoring_host_recovery.py` **on the authoring host**
to verify all of it against the committed manifests. It is read-only, reports
exactly which files are intact, drifted, or absent, and also reports whether that
machine's repository holds commits, stashes, or uncommitted work the published
history never received.

### What the quarantined plugin is actually worth

The 106,181,459-byte figure is mostly build output. By the manifest: 318 files /
37.5 MB of `Intermediate/`, 3 files / 68.4 MB of `Binaries/` (a 67.5 MB `.pdb`),
and **96 files / 209,178 bytes of authored source** — the only part a rebuild
cannot regenerate. Of those 96, **89 are recoverable from this repository's own
history**: 82 are byte-identical to commit `48982c0`, and 7 more match after
applying the Session 9 genericization (`premium_disc_golf` → `dg_generic`,
`premium_default` → `dg_generic_default`, `premium_bag_default` →
`dg_generic_bag_default`), verified by digest against the manifest.

Seven files carry later edits that exist nowhere in git: the `.uplugin`,
`DiscGolfAvatarBackendComponent.cpp` and `.h`, `DiscGolfPlayabilityLibrary.cpp`,
`DiscGolfPlayabilityMonitorComponent.cpp` and `.h`, and
`DiscGolfCustomizationTypes.h`. Six of the seven have cleanroom successors under
`Source/DiscGolfRuntimeFoundation/`, so the behaviour survives even where those
exact revisions do not. Recovering the revisions themselves needs the authoring
host.
