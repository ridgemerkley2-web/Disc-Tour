# Session 18 Poly Haven Derived-Runtime Provenance Audit

## Result

Session 18 closes one narrowly defined technical provenance blocker:
`POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE`. A controlled full-
Editor UE 5.8.1 reimport now emits a source-controlled, post-save receipt that
cryptographically binds all 35 CC0 source files to 55 Poly Haven-derived Unreal
packages and separately classifies the five project-original packages in the same
Pine Ridge runtime root. Exactly thirteen public-release blockers remain.

This result does not close whole-package provenance, the three quarantined Fab
receipt gates, development `DO_NOT_SHIP` content, character-framework distribution
rights, brand/legal review, production-art approval, shipping, or public release.

## Durable closure

- Source manifest:
  `SourceArt/PineRidge/PolyHaven/asset_manifest.json`, 35/35 files, SHA-256
  `880C79DD7F9151DAD0D8344C0E505CBB1E3AC98D06C955E9DACD6E305BCBD250`.
- Importer: `Scripts/import-pine-ridge-assets.py`, 108,495 bytes, SHA-256
  `A0B2A0F39F0921ED0AC2E3E884C793428CA0FAF192635B125A0EF227A929A8D6`.
- Engine identity: Unreal Engine 5.8.1, changelist 56057345, compatible
  changelist 55116800, branch `++UE5+Release-5.8`.
- Accepted controlled reimport: `1a0b2284-5196-487c-9a65-17b288ed0768`.
  The source-controlled, user-path-sanitized
  `Evidence/Session18/PolyHavenReimport.log` is 535,486 bytes with SHA-256
  `9683A9518A9D034E0C4CCD2F705C54D330F8F312314D203FA9480C964BB40F01`.
  The contract also commits to the 535,552-byte raw-log SHA-256
  `082FC4212BCA3FDCD3AF4019C8BEFE03F1BFD55CE375C1D40F2D4C3B04C8433A`.
  The retained log records the exact importer/engine run, receipt hash, success
  marker, and clean exit.
- Durable receipt:
  `SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json`, 88,601 bytes,
  SHA-256
  `13ACC125F5B442BE75DE405DBD76D43AA9381436624E12BDDBA3C7BA4F90D858`.

The receipt records 32 direct texture imports, five direct static-mesh imports,
and 18 dependent material/material-instance packages. Each record pins the live
file size and SHA-256, object path, Unreal class, observed scoped Asset Registry
dependencies, direct import source where applicable, transitive source closure,
and selected semantic settings. The 55 derived packages total 89,090,646 bytes.

Five project-original packages are explicitly disjoint from the Poly Haven set:
`M_GalleryLakeWater`, `M_PineRidgeGrassBlade`, `M_PineRidgeLeafLitter`, and the
two generated alpha-mask textures. Their own input/dependency/settings records
total 107,489 bytes. The two classes form an exact, non-overlapping 60-package /
89,198,135-byte partition of `Content/Presentation/Course/PineRidge`; no scoped
package is omitted or double-classified.

## Independent semantic validation

`Scripts/validate-pine-ridge-provenance-semantics.py` independently loads all 60
packages through Unreal, compares their classes, direct import paths, live project-
scoped Asset Registry dependencies, direct-source MD5 bindings, texture settings,
and mesh LOD counts to the receipt. Run
`a4e81657-0ebc-42a2-a6e9-06858e10b1d3` passed all 60 packages. Its 34,478-byte
source-controlled report at
`Evidence/Session18/PineRidgeSemanticValidation.json` has SHA-256
`1C3D6960E342B58143D55B42D701B18D19E17BFE8D118862BE38A93A15B74035`.

`Scripts/validate_dg_session18_poly_haven_provenance.py` uses strict duplicate-
key and non-finite-number rejection, exact schemas and path containment, live
source/runtime hashing, exact source/output mappings, JSON type-confusion and
overflow rejection, valid canonical UTC checking, case-insensitive uniqueness,
the complete runtime-root partition, immutable continuity records, and the exact
one-resolved/thirteen-remaining blocker ledger. Its mutation suite passes 53/53.
Normal validation passes; `--require-release-ready` deliberately exits 2.

## Package boundary

Fresh Session 18 Development package run
`63624166-aa24-49d0-a89f-74a125a36be2` completed a full non-iterative cook and
IoStore archive with 985 cooked packages, zero incrementally skipped, seven
platform-skipped, 992 total, 985 IoStore-ordered packages, and 2,895 written
chunks. The archive contains 54 physical files totaling 1,918,214,604 bytes.
`Evidence/Session18/ArchiveFiles.tsv` records all 54 canonical path/size/SHA-256
rows; its 6,753-byte file and canonical-row digest are both
`0877C04FF8911F0903077F8C68093BF8A783FB6939538D2BFD1E2888D35B1870`.

The byte-bound, 3,052-line UFS manifest at
`Evidence/Session18/Manifest_UFSFiles_Win64.txt` has SHA-256
`C94C9CE041B3E82813439818204B7A1DE2560120E6FEB845F864110EC612F747`.
It contains all 55 derived identities, all five project-original identities, and
no package below the three quarantined import roots. Their disjoint union exactly
equals the 60 Pine Ridge `.uasset` identities; the separately counted 37 Pine
Ridge `.ubulk` rows each bind by stem to one of those identities. The canonical
derived, original, and union path digests are respectively `CA6FE3E1...9DA69`,
`C593C252...16B3`, and `9EDB069B...069`.

This proves receipt-to-UFS package-identity inclusion and exact Pine Ridge root
partition only. It does not bind uncooked source `.uasset` hashes to cooked
IoStore chunk bytes; classify every other staged file, dependency, sidecar,
engine/plugin payload, or anonymous chunk; establish vendor acquisition rights;
or close `ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED`.

## Verification

- Editor and Game Development targets build successfully under UE 5.8.1.
- Focused Session 18 automation passes 3/3; retained report SHA-256 is
  `F6BA395C85B86D8A2CB768BB9BB58FAFC5CBDD1BA440693843F0F42142535092`.
- Full project automation passes 252/252; retained report SHA-256 is
  `724898C6E3901D76FE7600F86F7AF3FDE54FC090737D7A660F83215F528D92A6`.
- The fresh packaged game executable is 341,401,088 bytes with SHA-256
  `6051C15D6DCF14D4EE92B34215B79B7AA7BCADC5D8F6F5CA8AB24807E89D4346`.
- Packaged Pine Ridge play smoke run `65f4b822-7c11-4ca3-a63e-caa4e10fb857`
  exits 0 with 1,968 samples, 98.1 m final carry, one ground contact, authored
  camera, and local wind active. Its canonical UTF-8 evidence log SHA-256 is
  `5A5D13A776B9BF337B12FF1580E6E474799F6ADAE4AEF3E0B5E01EBB55176251`.
- `validate_project.py`, historical Session 9 and Session 16 validation, the
  reference-flight check, and `git diff --check` pass.

## Release ledger

Sessions 9-17 remain immutable historical records of the fourteen blockers that
were open when those gates ran. The additive Session 18 contract supersedes only
the current state of the Poly Haven derived-runtime receipt blocker. The remaining
ordered ledger contains exactly thirteen blockers, including
`ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED`.
