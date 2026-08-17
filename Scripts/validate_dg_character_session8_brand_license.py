#!/usr/bin/env python3
"""Fail-closed Session 8 brand, acquisition, and vendor-content audit.

Session 8 adds only project-owned, generic backend/cook scaffolding. MetaHuman
may be named as a reserved optional Epic integration, but no MetaHuman, paid
Fab, Premium Disc Golf, or other vendor art may be imported or claimed. The
script launches no external process and writes only its JSON report under
``Saved/CharacterFramework``.
"""

from __future__ import annotations

import csv
from datetime import datetime, timezone
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILDKIT = ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5"
ACQUIRED_LOCK = BUILDKIT / "Config/DG_AcquiredAssetLock.json"
THIRD_PARTY_REGISTRY = BUILDKIT / "Config/DG_ThirdPartyAssetRegistry.json"
BRAND_REGISTRY = BUILDKIT / "Config/DG_BrandSafetyRegistry.json"
PURCHASE_REGISTER = BUILDKIT / "Assets/purchase_and_license_register.csv"
SESSION7_SPEC = (
    ROOT / "SourceArt/DiscGolf/Characters/Customization/Proxy/"
    "proxy_customization_catalog_spec.json"
)
COOK_SPEC = ROOT / "Config/DG_RuntimeCookManifest.json"
UPROJECT = ROOT / "DiscGolfTour.uproject"
REPORT = ROOT / "Saved/CharacterFramework/Session8BrandLicenseAudit.json"

EXPECTED_SESSION8_CONTENT_FILES = {
    "Content/DiscGolf/Cook/DA_DG_RuntimeCookManifest.uasset",
    (
        "Content/DiscGolf/Characters/Avatar/Data/"
        "DA_DG_AvatarBackend_DGMaster.uasset"
    ),
}
SESSION8_CONTENT_ROOTS = (
    ROOT / "Content/DiscGolf/Cook",
    ROOT / "Content/DiscGolf/Characters/Avatar",
    ROOT / "Content/DiscGolf/Characters/MetaHuman",
)
SESSION8_SOURCEART_ROOTS = (
    ROOT / "SourceArt/DiscGolf/Characters/Avatar",
    ROOT / "SourceArt/DiscGolf/Characters/MetaHuman",
)
SESSION8_OWNED_TEXT = (
    "Config/DG_RuntimeCookManifest.json",
    "Source/DiscGolfTour/DiscGolfAvatarBackendRuntime.h",
    "Source/DiscGolfTour/DiscGolfAvatarBackendRuntime.cpp",
    "Source/DiscGolfTour/DiscGolfMetaHumanVisualContract.h",
    "Source/DiscGolfTour/DiscGolfMetaHumanAvatarBackendComponent.h",
    "Source/DiscGolfTour/DiscGolfMetaHumanAvatarBackendComponent.cpp",
    "Source/DiscGolfTour/DiscGolfRuntimeCookManifest.h",
    "Source/DiscGolfTour/DiscGolfRuntimeCookManifest.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfAvatarBackendRuntimeTests.cpp",
    "Scripts/audit_dg_character_session8_availability.py",
    "Scripts/create_dg_character_session8_cook_assets.py",
    "Scripts/validate_dg_character_session8_cook_assets.py",
    "Scripts/validate_dg_character_session8_cook_no_write.py",
)

# MetaHuman is an allowed technical reservation because it is Epic's optional
# free integration. These are the paid candidates, listing IDs, and unrelated
# real brands that must not enter the Session 8-owned source/content boundary.
FORBIDDEN_VENDOR_TOKENS = (
    "ultra dynamic sky",
    "brushify",
    "modern clothes",
    "mlindborg",
    "easy waterscape",
    "fluid flux",
    "84fda27a-c79f-49c9-8458-82401fb37cfb",
    "9bfc58fe-b011-437f-b74c-44e23101e57f",
    "9cc9d3cf-d72e-491b-b076-0af3dccc6b82",
    "ddd28113-6f59-4ba3-bf60-ad7b1920a547",
    "196c70cd-1283-4249-bf6b-c3019d1cbe11",
    "adidas",
    "discraft",
    "discmania",
    "dynamic discs",
    "innova",
    "latitude 64",
    "mvp disc",
    "nike",
    "oakley",
    "prodigy disc",
    "under armour",
    "westside discs",
)
PREMIUM_LOGO_TOKENS = (
    "premium disc golf",
    "premium_disc_golf",
    "premiumdg",
    "premium-dg",
    "pdg_logo",
)
PACKAGE_SUFFIXES = {".uasset", ".uexp", ".ubulk", ".uptnl"}


def _load_json(path: Path, label: str, errors: list[str]) -> dict:
    if not path.is_file():
        errors.append(f"{label} missing: {path}")
        return {}
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"{label} invalid: {exc}")
        return {}
    if not isinstance(value, dict):
        errors.append(f"{label} root is not an object")
        return {}
    return value


def _purchase_rows(errors: list[str]) -> list[dict[str, str]]:
    if not PURCHASE_REGISTER.is_file():
        errors.append(f"Purchase/license register missing: {PURCHASE_REGISTER}")
        return []
    try:
        with PURCHASE_REGISTER.open(
            "r", encoding="utf-8-sig", newline=""
        ) as stream:
            return list(csv.DictReader(stream))
    except OSError as exc:
        errors.append(f"Purchase/license register unreadable: {exc}")
        return []


def _find(entries: object, key: str, value: str) -> dict | None:
    if not isinstance(entries, list):
        return None
    return next(
        (
            entry for entry in entries
            if isinstance(entry, dict) and entry.get(key) == value
        ),
        None,
    )


def _files_under(roots: tuple[Path, ...]) -> list[Path]:
    return sorted(
        path
        for root in roots if root.is_dir()
        for path in root.rglob("*") if path.is_file()
    )


def _token_hits(path: Path, tokens: tuple[str, ...]) -> list[str]:
    payload = path.read_bytes().lower()
    return [
        token for token in tokens
        if token.encode("utf-8") in payload
        or token.encode("utf-16le") in payload
    ]


def main() -> int:
    errors: list[str] = []
    acquired = _load_json(ACQUIRED_LOCK, "Acquired asset lock", errors)
    third_party = _load_json(
        THIRD_PARTY_REGISTRY, "Third-party registry", errors
    )
    brands = _load_json(BRAND_REGISTRY, "Brand registry", errors)
    session7_spec = _load_json(SESSION7_SPEC, "Session 7 proxy spec", errors)
    cook_spec = _load_json(COOK_SPEC, "Session 8 cook spec", errors)
    project = _load_json(UPROJECT, "Project descriptor", errors)
    purchases = _purchase_rows(errors)

    acquired_entries = acquired.get("acquired")
    if acquired_entries != []:
        errors.append(
            f"Acquired asset lock must remain exactly empty: {acquired_entries!r}"
        )

    paid_registry = sorted(
        (
            entry for entry in third_party.get("assets", [])
            if isinstance(entry, dict)
            and entry.get("purchase_required") is True
        ),
        key=lambda entry: str(entry.get("integration_id", "")),
    )
    purchases = sorted(
        purchases,
        key=lambda row: str(row.get("integration_id", "")),
    )
    invalid_paid_registry = [
        {
            "integration_id": entry.get("integration_id"),
            "status": entry.get("status"),
        }
        for entry in paid_registry if entry.get("status") != "NOT_ACQUIRED"
    ]
    if invalid_paid_registry:
        errors.append(
            f"Paid registry candidates are not all NOT_ACQUIRED: {invalid_paid_registry}"
        )
    invalid_purchase_rows = [
        {"integration_id": row.get("integration_id"), "status": row.get("status")}
        for row in purchases if row.get("status") != "NOT_ACQUIRED"
    ]
    if invalid_purchase_rows:
        errors.append(
            f"Purchase-register candidates are not all NOT_ACQUIRED: "
            f"{invalid_purchase_rows}"
        )

    metahuman_registry = _find(
        third_party.get("assets"), "integration_id", "metahuman"
    )
    if not metahuman_registry \
            or metahuman_registry.get("status") != "AVAILABLE_OPTIONAL" \
            or metahuman_registry.get("purchase_required") is not False:
        errors.append(
            "MetaHuman must remain AVAILABLE_OPTIONAL and purchase_required=false"
        )

    generic_brand = _find(brands.get("brands"), "brand_id", "dg_generic")
    premium_brand = _find(
        brands.get("brands"), "brand_id", "premium_disc_golf"
    )
    if brands.get("default_policy") != "BLOCK_UNLESS_EXPLICITLY_APPROVED":
        errors.append("Brand registry lost its fail-closed default policy")
    if not generic_brand or generic_brand.get("status") != "APPROVED":
        errors.append("Generic/unbranded project art is not explicitly approved")
    premium_logo_qualification = (
        "PREMIUM_DISC_GOLF_ART_OUT_OF_SESSION8_SCOPE_AND_NOT_BUNDLED"
        if premium_brand
        and premium_brand.get("logo_asset_status") != "BUNDLED_AUTHORIZED"
        else None
    )

    if session7_spec.get("brand_id") != "dg_generic" \
            or session7_spec.get("brand_status") != "GENERIC_UNBRANDED":
        errors.append("Authoritative Session 7 proxy is no longer generic/unbranded")
    if session7_spec.get("external_sources") != [] \
            or session7_spec.get("logos") != [] \
            or session7_spec.get("production_license_claim") != "NONE":
        errors.append("Session 7 proxy gained an external source/logo/license claim")
    if session7_spec.get("shipping_status") != "DO_NOT_SHIP":
        errors.append("Authoritative proxy lost DO_NOT_SHIP status")

    outputs = cook_spec.get("asset_outputs", {})
    dgmaster = outputs.get("dg_master_backend_profile", {})
    reserved = cook_spec.get("reserved_optional_metahuman_entry_points", {})
    if cook_spec.get("content_status") \
            != "ACCEPTED_TECHNICAL_PIPELINE_WITH_NON_PRODUCTION_PROXY_VISUALS" \
            or cook_spec.get("shipping_status") \
            != "DO_NOT_CLAIM_SHIPPING_ART_APPROVAL":
        errors.append("Cook spec overclaims production/shipping art readiness")
    if dgmaster.get("backend_id") != "dg_master" \
            or dgmaster.get("visual_actor_class") is not None \
            or dgmaster.get("retarget_asset") is not None \
            or dgmaster.get("runtime_face_sculpting") is not False:
        errors.append("Initial DGMaster profile is not the generic dormant fallback")
    if reserved.get("created_by_session8_cook_author") is not False \
            or reserved.get("authoring_status") \
            != "BLOCKED_UNTIL_REAL_PROJECT_METAHUMAN_CONTENT_EXISTS":
        errors.append("Reserved MetaHuman entry points no longer fail closed")

    session8_content_files = _files_under(SESSION8_CONTENT_ROOTS)
    session8_content_relatives = {
        path.relative_to(ROOT).as_posix() for path in session8_content_files
        if path.suffix.casefold() in PACKAGE_SUFFIXES
    }
    unexpected_session8_content = sorted(
        session8_content_relatives - EXPECTED_SESSION8_CONTENT_FILES
    )
    if unexpected_session8_content:
        errors.append(
            "Unexpected/vendor asset packages entered the Session 8 avatar/cook "
            f"namespace: {unexpected_session8_content}"
        )
    metahuman_project_packages = sorted(
        relative for relative in session8_content_relatives
        if "/MetaHuman/" in f"/{relative}/"
        or "AvatarBackend_MetaHuman" in relative
    )
    if metahuman_project_packages:
        errors.append(
            f"MetaHuman project assets were fabricated/imported: {metahuman_project_packages}"
        )
    sourceart_files = _files_under(SESSION8_SOURCEART_ROOTS)
    if sourceart_files:
        errors.append(
            "Vendor/MetaHuman source art entered Session 8-owned roots: "
            + str([path.relative_to(ROOT).as_posix() for path in sourceart_files])
        )

    missing_owned_text = [
        relative for relative in SESSION8_OWNED_TEXT
        if not (ROOT / relative).is_file()
    ]
    if missing_owned_text:
        errors.append(f"Session 8 owned source files missing: {missing_owned_text}")
    vendor_token_hits: dict[str, list[str]] = {}
    for relative in SESSION8_OWNED_TEXT:
        path = ROOT / relative
        if not path.is_file():
            continue
        hits = _token_hits(path, FORBIDDEN_VENDOR_TOKENS)
        if hits:
            vendor_token_hits[relative] = hits
            errors.append(f"Vendor token in Session 8-owned source: {relative}: {hits}")

    discgolf_roots = (ROOT / "Content/DiscGolf", ROOT / "SourceArt/DiscGolf")
    premium_logo_hits: dict[str, list[str]] = {}
    for path in _files_under(discgolf_roots):
        filename = path.name.casefold()
        hits = [
            token for token in PREMIUM_LOGO_TOKENS
            if token in filename or token.replace(" ", "_") in filename
        ]
        if path.suffix.casefold() in PACKAGE_SUFFIXES | {
            ".json", ".txt", ".csv", ".ini", ".md"
        }:
            hits.extend(_token_hits(path, PREMIUM_LOGO_TOKENS))
        hits = sorted(set(hits))
        if hits:
            relative = path.relative_to(ROOT).as_posix()
            premium_logo_hits[relative] = hits
            errors.append(f"Premium Disc Golf logo/art token found: {relative}: {hits}")

    explicitly_enabled_plugins = sorted(
        entry.get("Name") for entry in project.get("Plugins", [])
        if isinstance(entry, dict) and entry.get("Enabled") is True
    )
    forbidden_enabled_plugins = sorted(
        name for name in explicitly_enabled_plugins
        if isinstance(name, str) and (
            name.casefold().startswith("metahuman")
            or name in {"HairStrands", "RigLogic"}
        )
    )
    if forbidden_enabled_plugins:
        errors.append(
            "Optional MetaHuman/vendor plugins were enabled without content: "
            f"{forbidden_enabled_plugins}"
        )

    result = {
        "schema": "DiscGolfTour.Session8BrandLicenseAudit.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if not errors else "FAIL",
        "brand_scope": "DG_GENERIC_UNBRANDED_ONLY",
        "authoritative_visual": "SESSION7_NON_PRODUCTION_PROXY",
        "authoritative_visual_shipping_status": session7_spec.get(
            "shipping_status"
        ),
        "acquired_asset_count": (
            len(acquired_entries) if isinstance(acquired_entries, list) else None
        ),
        "paid_registry_statuses": [
            {
                "integration_id": entry.get("integration_id"),
                "status": entry.get("status"),
            }
            for entry in paid_registry
        ],
        "purchase_register_statuses": [
            {
                "integration_id": row.get("integration_id"),
                "status": row.get("status"),
            }
            for row in purchases
        ],
        "metahuman_registry_status": (
            metahuman_registry.get("status") if metahuman_registry else None
        ),
        "metahuman_project_content_status": "ABSENT_BLOCKED",
        "premium_logo_status": (
            premium_brand.get("logo_asset_status") if premium_brand else None
        ),
        "qualifications": (
            [premium_logo_qualification] if premium_logo_qualification else []
        ),
        "expected_session8_content_files": sorted(
            EXPECTED_SESSION8_CONTENT_FILES
        ),
        "present_session8_content_files": sorted(session8_content_relatives),
        "unexpected_session8_content_files": unexpected_session8_content,
        "metahuman_project_packages": metahuman_project_packages,
        "session8_sourceart_files": [
            path.relative_to(ROOT).as_posix() for path in sourceart_files
        ],
        "vendor_token_hits": vendor_token_hits,
        "premium_logo_hits": premium_logo_hits,
        "explicitly_enabled_plugins": explicitly_enabled_plugins,
        "forbidden_enabled_plugins": forbidden_enabled_plugins,
        "vendor_assets_integrated_by_session8": [],
        "network_access": "NONE",
        "asset_acquisition": "NONE",
        "unreal_launched": False,
        "ubt_launched": False,
        "errors": errors,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(
        f"SESSION 8 BRAND/LICENSE {result['status']}: "
        f"brand=dg_generic acquired={result['acquired_asset_count']} "
        f"vendor_assets=0 premium_logo={result['premium_logo_status']} "
        f"report={REPORT}"
    )
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
