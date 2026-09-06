# Session 9 Brand and Asset Audit

Date: 2026-08-23  
Authority: `Config/DG_BrandLicenseContract.json`  
Validator: `Scripts/validate_dg_session9_brand_license.py`

## Result semantics

This is a technical provenance gate, not legal advice. The normal expected result is
`PASS_CONTRACT_RELEASE_BLOCKED`: the project matches the contract's scoped declared
inventories, runtime-facing defaults are generic, quarantined imports are configured as
NeverCook, and every unresolved release issue is explicit. It does **not** prove complete
staged-package provenance or mean that the project is ready for public distribution.

`--require-release-ready` is intentionally fail-closed while any release blocker remains.
No receipt, account identifier, order number, or other private acquisition record belongs
in the project contract or a packaged build. The contract stores only stable public
listing identities and opaque evidence-document paths.

## Active presentation policy

- `DGTour`, `DiscGolfTour`, and `Disc Golf Tour` remain internal working names. No final
  public title is locked or cleared.
- `dg_generic` is the only active runtime brand ID. Framework defaults, stamp defaults,
  and bag defaults use the generic namespace.
- Premium Disc Golf remains dormant and blocked pending a new explicit approval and
  authorized artwork. The historical `_BuildKit` approval record is not current authority.
- Public creator labels say `REALISTIC / CURATED`; `MetaHuman` remains an internal
  technical asset, class, plugin, and diagnostics identifier.
- Pine Ridge runtime IDs are project-original functional identifiers. Real courses may
  remain cited in design research, but are not runtime identity strings.
- Stable disc IDs remain unchanged for save and physics compatibility. Their current
  display names, and `Pine Ridge Championship`, remain Development-only pending public
  name clearance.

## Approved external and platform sources

### Poly Haven Pine Ridge set

The local project source manifest contains 35 files across seven CC0 source assets. Session 9
checks every recorded byte size and MD5, the manifest SHA-256, the CC0 declaration, and
the source URLs. Poly Haven states that its assets are CC0 and may be used commercially
without required attribution: <https://polyhaven.com/license>.

This clears the local **source bytes**, not the complete derived runtime chain. The current
ignored `Saved/PineRidgeAssetImportReceipt.json` does not pin the input-manifest hash,
tool and engine versions, import options, source-to-output mapping, or output hashes. A
sanitized durable receipt (or asset-depot equivalent) is still required before the derived
Pine Ridge `.uasset` set can be called release-provenanced.

### Epic MetaHuman

The assembled character is an approved UE-only technical dependency, not an approved
player-facing brand. The validator pins the accepted Common/Generated inventory counts,
the 264-package runtime contract, and exclusion of the source MHC from the cook closure.
Epic's Content EULA governs MetaHuman Content and identifies it as UE-Only Content:
<https://www.unrealengine.com/eula/content>. The contract also preserves the no-AI-
training/database restriction and requires a separate decision before public technology
branding.

## Quarantined local imports

| Source | Local root | Files | Bytes | State |
|---|---|---:|---:|---|
| Project Nature Spruce Forest | `Content/PN_interactiveSpruceForest` | 363 | 1,565,612,902 | receipt evidence pending |
| GreenBugGames Stump Scanned | `Content/Stump_Scanned` | 63 | 680,263,631 | receipt evidence pending |
| tharlevfx Water Materials | `Content/WaterMaterials` | 107 | 91,003,274 | receipt evidence pending |

These roots are local inspection sources only. They are not approved bindings, are absent
from the declarative runtime cook manifest, and are explicitly listed in
`DirectoriesToNeverCook`. Actual release-stage exclusion remains part of the pending
staged-package provenance closure.
Their presence proves neither acquisition nor redistribution rights. A future approval
requires the owner's receipt evidence, reviewed scope, approved project-owned bindings,
render/collision/performance acceptance, and a contract revision.

Fab's Standard License generally permits commercial use and distribution as part of a
project while prohibiting standalone redistribution, but the project must retain its own
acquisition evidence for each product: <https://www.fab.com/eula>.

## Development-only content

- Session 6 proxy outfits are project-local technical fixtures marked `DO_NOT_SHIP`.
- Session 7 proxy customization is project-local technical fixture art marked
  `DO_NOT_SHIP`.
- The motion registry contains two project-authored synthetic fixtures and seven
  unverified vendor-demo motions. None is cleared production motion.
- The runtime cook contract already says `DO_NOT_CLAIM_SHIPPING_ART_APPROVAL`.

These assets may remain in a Development, `ForDistribution=False` package. A Shipping or
distribution-ready audit must reject them until production replacements and rights
evidence exist.

## Framework source-rights boundary

The enabled `DiscGolfCharacterFramework` plugin arrived in the user-supplied build kit.
Neither the supplied archive nor the installed plugin contains a `LICENSE`, `COPYING`, or
`NOTICE` file or another documented redistribution grant. Local possession, compilation,
and a `CreatedBy` descriptor do not establish source or binary redistribution rights.
The Session 9 contract therefore records this as an unresolved distribution blocker. Do
not invent or attach an open-source license; the owner must document project authorship
or obtain an appropriate written grant.

## Release blockers

1. Final public title is not locked or cleared.
2. Equipment and course display names await clearance.
3. Character-framework source/binary distribution rights are unresolved.
4. Development `DO_NOT_SHIP` motion, outfit, and customization content remains in the
   technical runtime closure.
5. Three locally imported Fab products remain receipt-evidence pending.
6. Poly Haven source bytes are verified, but the derived runtime asset receipt is not
   durable or cryptographically closed.
7. No release gate yet maps every file from an actual staged UFS/IoStore manifest to an
   approved provenance class, including transitive engine/plugin dependencies.
8. A human visual review for logos, likeness, copied artwork, and trade dress is still
   required; string and manifest scans cannot prove visual originality.

## Final automated evidence

The strict validator self-test passed 15 adversarial cases. Normal validation passed with
`contract_valid=true`, `release_ready=false`, `errors=0`, and `blockers=8`; the
release-required lane returned its expected exit code 2. Fresh Editor and Game Development
builds succeeded. Focused `DiscGolfTour.BrandLicense.` automation passed 1/1, and the full
`DiscGolfTour.` suite passed 141/141, with zero non-success, automation-error, fatal,
assert, or critical result. These results validate the scoped technical contract only;
they do not close actual staged-package provenance, legal clearance, or distribution
approval.

## Commands

```powershell
python Scripts/validate_dg_session9_brand_license.py --self-test
python Scripts/validate_dg_session9_brand_license.py
python Scripts/validate_dg_session9_brand_license.py --require-release-ready
```

The first two commands must pass. The third must remain nonzero until every declared
release blocker is closed and the contract is deliberately revised.
