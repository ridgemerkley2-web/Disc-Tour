#!/usr/bin/env python3
"""Strict Session 6 brand, provenance, and acquisition-state audit.

This is a read-only ordinary-CPython audit.  It does not launch Blender,
Unreal, UBT, a browser, or a downloader.  The only file it writes is its JSON
report under ``Saved``.  The audit intentionally permits only the project-local
``dg_generic`` proxy fixture and proves that neither the unacquired clothing
pack nor unavailable Premium Disc Golf logo art entered the owned source or
live Session 6 package namespace.
"""

from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "DiscGolf" / "Outfits" / "Proxy"
SPEC_PATH = SOURCE_ROOT / "proxy_outfit_catalog_spec.json"
MANIFEST_PATH = SOURCE_ROOT / "proxy_outfit_source_manifest.json"
PURCHASE_REGISTER = (
    PROJECT_ROOT
    / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Assets/purchase_and_license_register.csv"
)
THIRD_PARTY_REGISTRY = (
    PROJECT_ROOT
    / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_ThirdPartyAssetRegistry.json"
)
BRAND_REGISTRY = (
    PROJECT_ROOT
    / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_BrandSafetyRegistry.json"
)
REPORT_PATH = (
    PROJECT_ROOT / "Saved/CharacterFramework/Session6BrandLicenseAudit.json"
)

# The proxy source and generated packages may not carry these real-company or
# vendor identifiers.  Keeping the strings in this validator (outside the
# scanned namespaces) makes the block list reviewable without introducing them
# into content metadata.
FORBIDDEN_TOKENS = (
    "adidas",
    "discraft",
    "discmania",
    "dynamic discs",
    "innova",
    "latitude 64",
    "mlindborg",
    "modern clothes",
    "mvp disc",
    "nike",
    "oakley",
    "premium disc golf",
    "prodigy disc",
    "under armour",
    "westside discs",
    "9cc9d3cf-d72e-491b-b076-0af3dccc6b82",
)

ALLOWED_OWNED_EXTENSIONS = {".json", ".py", ".blend", ".fbx"}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def _find_by_id(entries: list[dict], key: str, value: str) -> dict | None:
    return next((entry for entry in entries if entry.get(key) == value), None)


def _binary_token_hits(path: Path) -> list[str]:
    """Find forbidden ASCII or UTF-16LE strings without interpreting artwork."""
    payload = path.read_bytes().lower()
    hits = []
    for token in FORBIDDEN_TOKENS:
        ascii_token = token.encode("utf-8")
        utf16_token = token.encode("utf-16le")
        if ascii_token in payload or utf16_token in payload:
            hits.append(token)
    return hits


def _live_owned_files() -> list[Path]:
    roots = (
        PROJECT_ROOT / "Content/DiscGolf/Outfits",
        PROJECT_ROOT / "Content/DiscGolf/Materials/Outfits",
    )
    result: list[Path] = []
    for root in roots:
        if root.is_dir():
            result.extend(path for path in root.rglob("*") if path.is_file())
    return sorted(result)


def main() -> int:
    errors: list[str] = []
    required = (SPEC_PATH, PURCHASE_REGISTER, THIRD_PARTY_REGISTRY, BRAND_REGISTRY)
    for path in required:
        _require(path.is_file(), f"Required authority file missing: {path}", errors)
    if errors:
        raise RuntimeError("; ".join(errors))

    spec = _load_json(SPEC_PATH)
    third_party = _load_json(THIRD_PARTY_REGISTRY)
    brands = _load_json(BRAND_REGISTRY)
    with PURCHASE_REGISTER.open("r", encoding="utf-8-sig", newline="") as stream:
        purchase_rows = list(csv.DictReader(stream))

    modern_purchase = _find_by_id(
        purchase_rows, "integration_id", "modern_clothes_pack")
    modern_registry = _find_by_id(
        third_party.get("assets", []), "integration_id", "modern_clothes_pack")
    generic_brand = _find_by_id(brands.get("brands", []), "brand_id", "dg_generic")
    premium_brand = _find_by_id(
        brands.get("brands", []), "brand_id", "premium_disc_golf")

    _require(modern_purchase is not None,
             "Purchase register lacks modern_clothes_pack", errors)
    if modern_purchase:
        _require(modern_purchase.get("status") == "NOT_ACQUIRED",
                 "modern_clothes_pack is not frozen at NOT_ACQUIRED", errors)
        acquisition_fields = (
            "purchase_date", "license_tier", "order_or_receipt_ref",
            "imported_project_version", "source_version",
        )
        _require(all(not modern_purchase.get(field) for field in acquisition_fields),
                 "modern_clothes_pack has incomplete/unconfirmed acquisition fields", errors)
    _require(modern_registry is not None,
             "Third-party registry lacks modern_clothes_pack", errors)
    if modern_registry:
        _require(modern_registry.get("status") == "NOT_ACQUIRED"
                 and modern_registry.get("purchase_required") is True,
                 "Third-party registry does not block the unacquired clothing pack", errors)
    _require(generic_brand is not None and generic_brand.get("status") == "APPROVED",
             "dg_generic is not approved by the installed brand registry", errors)
    _require(premium_brand is not None
             and premium_brand.get("logo_asset_status") == "NOT_BUNDLED",
             "Premium Disc Golf logo availability is not frozen at NOT_BUNDLED", errors)
    _require(brands.get("default_policy") == "BLOCK_UNLESS_EXPLICITLY_APPROVED",
             "Installed brand registry is not block-by-default", errors)

    _require(spec.get("content_status") == "NON_PRODUCTION_PROXY",
             "Proxy spec lost NON_PRODUCTION_PROXY status", errors)
    _require(spec.get("shipping_status") == "DO_NOT_SHIP",
             "Proxy spec lost DO_NOT_SHIP status", errors)
    _require(spec.get("brand_id") == "dg_generic"
             and spec.get("brand_status") == "GENERIC_UNBRANDED",
             "Proxy spec is not generic/unbranded", errors)
    _require(spec.get("external_sources") == [] and spec.get("logos") == [],
             "Proxy spec declares external sources or logos", errors)
    _require(spec.get("material_asset") ==
             "/Game/DiscGolf/Materials/Outfits/M_DG_OutfitProxy",
             "Proxy catalog no longer uses the one owned shared material", errors)

    source_files = sorted(
        path for path in SOURCE_ROOT.rglob("*")
        if path.is_file() and path.suffix.casefold() in ALLOWED_OWNED_EXTENSIONS
    )
    unexpected_source = sorted(
        path for path in SOURCE_ROOT.rglob("*")
        if path.is_file()
        and "__pycache__" not in path.parts
        and path.suffix.casefold() not in ALLOWED_OWNED_EXTENSIONS
    )
    _require(not unexpected_source,
             "Unexpected file type in project-owned proxy source: "
             + ", ".join(str(path) for path in unexpected_source), errors)

    source_token_hits: dict[str, list[str]] = {}
    for path in source_files:
        hits = _binary_token_hits(path)
        if hits:
            relative = path.relative_to(PROJECT_ROOT).as_posix()
            source_token_hits[relative] = hits
            errors.append(f"Forbidden brand/vendor token in owned source {relative}: {hits}")

    generated_state = "GENERATION_PENDING"
    manifest_summary: dict = {}
    if MANIFEST_PATH.is_file():
        generated_state = "GENERATED_SOURCE_AUDITED"
        manifest = _load_json(MANIFEST_PATH)
        _require(manifest.get("content_status") == "NON_PRODUCTION_PROXY"
                 and manifest.get("shipping_status") == "DO_NOT_SHIP",
                 "Generated manifest lost non-production shipping boundary", errors)
        _require(manifest.get("brand_id") == "dg_generic"
                 and manifest.get("brand_status") == "GENERIC_UNBRANDED",
                 "Generated manifest is not generic/unbranded", errors)
        _require(manifest.get("external_sources") == []
                 and manifest.get("logos") == [],
                 "Generated manifest declares an external source or logo", errors)
        _require(manifest.get("production_license_claim") == "NONE",
                 "Generated proxy source makes a production-license claim", errors)
        _require(str(manifest.get("rights_status", "")).startswith(
                     "PROJECT_LOCAL_GENERATED_TEST_FIXTURE"),
                 "Generated manifest provenance is not project-local test-fixture only", errors)
        manifest_summary = {
            "path": MANIFEST_PATH.relative_to(PROJECT_ROOT).as_posix(),
            "sha256": _sha256(MANIFEST_PATH),
            "rights_status": manifest.get("rights_status"),
            "production_license_claim": manifest.get("production_license_claim"),
        }

    live_files = _live_owned_files()
    live_token_hits: dict[str, list[str]] = {}
    for path in live_files:
        hits = _binary_token_hits(path)
        if hits:
            relative = path.relative_to(PROJECT_ROOT).as_posix()
            live_token_hits[relative] = hits
            errors.append(f"Forbidden brand/vendor token in live Session 6 package: {relative}: {hits}")

    result = {
        "schema": "DiscGolfTour.Session6BrandLicenseAudit.v1",
        "status": "PASS" if not errors else "FAIL",
        "source_status": generated_state,
        "acceptance_ready": (
            not errors
            and generated_state == "GENERATED_SOURCE_AUDITED"
            and len(live_files) > 0),
        "content_status": spec.get("content_status"),
        "shipping_status": spec.get("shipping_status"),
        "brand_id": spec.get("brand_id"),
        "brand_policy": brands.get("default_policy"),
        "premium_logo_status": (
            premium_brand.get("logo_asset_status") if premium_brand else None),
        "modern_clothes_purchase_status": (
            modern_purchase.get("status") if modern_purchase else None),
        "modern_clothes_registry_status": (
            modern_registry.get("status") if modern_registry else None),
        "external_sources": spec.get("external_sources"),
        "logos": spec.get("logos"),
        "shared_material": spec.get("material_asset"),
        "owned_source_file_count": len(source_files),
        "live_owned_file_count": len(live_files),
        "source_brand_token_hits": source_token_hits,
        "live_brand_token_hits": live_token_hits,
        "manifest": manifest_summary,
        "network_access": "NONE",
        "asset_acquisition": "NONE",
        "unreal_launched": False,
        "ubt_launched": False,
        "blender_launched": False,
        "errors": errors,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if errors:
        print("SESSION 6 BRAND/LICENSE AUDIT FAIL")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(
        "SESSION 6 BRAND/LICENSE AUDIT PASS: "
        f"brand=dg_generic vendor=NOT_ACQUIRED source={generated_state}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
