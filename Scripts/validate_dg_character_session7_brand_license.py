#!/usr/bin/env python3
"""Read-only Session 7 proxy brand, provenance, and acquisition audit.

Only the project-local ``dg_generic`` blockout is permitted.  The audit proves
that no optional vendor character pack, MetaHuman content, real-player likeness,
company mark, or Premium Disc Golf artwork entered the owned Session 7 source
or live content namespace.  It launches no external program and writes only its
JSON report under Saved.
"""

from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt/DiscGolf/Characters/Customization/Proxy"
SPEC_PATH = SOURCE_ROOT / "proxy_customization_catalog_spec.json"
MANIFEST_PATH = SOURCE_ROOT / "proxy_customization_source_manifest.json"
PURCHASE_REGISTER = (
    PROJECT_ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Assets/purchase_and_license_register.csv")
THIRD_PARTY_REGISTRY = (
    PROJECT_ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_ThirdPartyAssetRegistry.json")
BRAND_REGISTRY = (
    PROJECT_ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_BrandSafetyRegistry.json")
REPORT_PATH = PROJECT_ROOT / "Saved/CharacterFramework/Session7BrandLicenseAudit.json"
FROZEN_CATALOG_SPEC_SHA256 = (
    "27E6B2C74AA5F1F254CC499627FF8A2347F39727E823A4D0B4F769213D59FE45"
)

FORBIDDEN_TOKENS = (
    "adidas", "discraft", "discmania", "dynamic discs", "innova", "latitude 64",
    "metahuman", "mlindborg", "modern clothes", "mvp disc", "nike", "oakley",
    "premium disc golf", "prodigy disc", "under armour", "westside discs",
    "9cc9d3cf-d72e-491b-b076-0af3dccc6b82",
)
ALLOWED_SOURCE_EXTENSIONS = {".json", ".py", ".blend", ".fbx"}
LIVE_ROOTS = (
    PROJECT_ROOT / "Content/DiscGolf/Characters/Customization",
    PROJECT_ROOT / "Content/DiscGolf/Materials/CharacterCustomization",
)
PACKAGE_SUFFIXES = {".uasset", ".uexp", ".ubulk", ".uptnl"}


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


def _find(entries: list[dict], key: str, value: str) -> dict | None:
    return next((entry for entry in entries if entry.get(key) == value), None)


def _token_hits(path: Path) -> list[str]:
    payload = path.read_bytes().lower()
    hits = []
    for token in FORBIDDEN_TOKENS:
        if token.encode("utf-8") in payload or token.encode("utf-16le") in payload:
            hits.append(token)
    return hits


def _source_files() -> list[Path]:
    return sorted(path for path in SOURCE_ROOT.rglob("*") if path.is_file())


def _live_files() -> list[Path]:
    files = []
    for root in LIVE_ROOTS:
        if root.is_dir():
            files.extend(path for path in root.rglob("*")
                         if path.is_file() and path.suffix.casefold() in PACKAGE_SUFFIXES)
    return sorted(files)


def _purchase_rows() -> list[dict]:
    with PURCHASE_REGISTER.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def main() -> int:
    errors: list[str] = []
    _require(SPEC_PATH.is_file(), f"Session 7 catalog spec missing: {SPEC_PATH}", errors)
    if SPEC_PATH.is_file():
        _require(_sha256(SPEC_PATH) == FROZEN_CATALOG_SPEC_SHA256,
                 "Frozen Session 7 catalog spec byte hash differs", errors)
    _require(PURCHASE_REGISTER.is_file(), "Purchase/license register missing", errors)
    _require(THIRD_PARTY_REGISTRY.is_file(), "Third-party registry missing", errors)
    _require(BRAND_REGISTRY.is_file(), "Brand registry missing", errors)
    spec = _load_json(SPEC_PATH) if SPEC_PATH.is_file() else {}
    third_party = _load_json(THIRD_PARTY_REGISTRY) if THIRD_PARTY_REGISTRY.is_file() else {}
    brands = _load_json(BRAND_REGISTRY) if BRAND_REGISTRY.is_file() else {}
    purchases = _purchase_rows() if PURCHASE_REGISTER.is_file() else []

    _require(spec.get("content_status") == "NON_PRODUCTION_PROXY",
             "Proxy lost NON_PRODUCTION_PROXY status", errors)
    _require(spec.get("shipping_status") == "DO_NOT_SHIP",
             "Proxy lost DO_NOT_SHIP status", errors)
    _require(spec.get("brand_id") == "dg_generic"
             and spec.get("brand_status") == "GENERIC_UNBRANDED",
             "Only the generic/unbranded proxy is permitted", errors)
    _require(spec.get("external_sources") == [] and spec.get("logos") == [],
             "External sources/logos must remain empty", errors)
    _require(spec.get("production_license_claim") == "NONE",
             "Generated test fixture may not claim a production asset license", errors)
    _require(spec.get("rights_status") ==
             "PROJECT_LOCAL_GENERATED_TEST_FIXTURE_DERIVED_FROM_ACCEPTED_PROJECT_PROXY",
             "Generated proxy rights status differs", errors)

    modern_purchase = _find(purchases, "integration_id", "modern_clothes_pack")
    modern_registry = _find(third_party.get("assets", []), "integration_id", "modern_clothes_pack")
    metahuman_registry = _find(third_party.get("assets", []), "integration_id", "metahuman")
    generic_brand = _find(brands.get("brands", []), "brand_id", "dg_generic")
    premium_brand = _find(brands.get("brands", []), "brand_id", "premium_disc_golf")
    _require(modern_purchase is not None and modern_purchase.get("status") == "NOT_ACQUIRED",
             "Modern clothes purchase state is not explicitly NOT_ACQUIRED", errors)
    _require(modern_registry is not None and modern_registry.get("status") == "NOT_ACQUIRED",
             "Modern clothes registry state is not explicitly NOT_ACQUIRED", errors)
    _require(metahuman_registry is not None,
             "MetaHuman registry entry is missing", errors)
    _require(generic_brand is not None and generic_brand.get("status") == "APPROVED",
             "Generic/unbranded brand is not approved", errors)
    _require(premium_brand is not None
             and premium_brand.get("logo_asset_status") == "NOT_BUNDLED",
             "Premium Disc Golf logo state is not NOT_BUNDLED", errors)

    source_files = _source_files()
    allowed_names = {
        "generate_session7_proxy_customization.py",
        "proxy_customization_catalog_spec.json",
        "proxy_customization_source_manifest.json",
        "DG_Session7_ProxyCustomization.blend",
        "SK_DG_Head_Proxy.fbx",
        "SM_DG_Hair_Short_Proxy.fbx",
        "SM_DG_Hair_Medium_Proxy.fbx",
        "SM_DG_Hair_Mohawk_Proxy.fbx",
        "SM_DG_FacialHair_Stubble_Proxy.fbx",
        "SM_DG_FacialHair_Beard_Proxy.fbx",
        "SM_DG_Brow_Default_Proxy.fbx",
        "SM_DG_Brow_Alt_Proxy.fbx",
    }
    source_token_hits: dict[str, list[str]] = {}
    for path in source_files:
        relative = path.relative_to(PROJECT_ROOT).as_posix()
        _require(path.suffix.casefold() in ALLOWED_SOURCE_EXTENSIONS,
                 f"Unexpected source extension: {relative}", errors)
        _require(path.name in allowed_names,
                 f"Unexpected source file in owned Session 7 namespace: {relative}", errors)
        hits = _token_hits(path)
        if hits:
            source_token_hits[relative] = hits
            errors.append(f"Forbidden brand/vendor token in Session 7 source: {relative}: {hits}")

    manifest_summary = None
    generated_state = "INPUTS_READY_NOT_GENERATED"
    if MANIFEST_PATH.is_file():
        manifest = _load_json(MANIFEST_PATH)
        generated_state = "GENERATED_SOURCE_AUDITED"
        _require(manifest.get("status") == "PASS_GENERATED_SOURCE_FIXTURE",
                 "Generated manifest status differs", errors)
        _require(manifest.get("external_sources") == [] and manifest.get("logos") == []
                 and manifest.get("production_license_claim") == "NONE",
                 "Generated manifest gained external/license/logo claims", errors)
        _require(manifest.get("rights_status") == spec.get("rights_status"),
                 "Generated manifest rights status differs", errors)
        _require(manifest.get("network_access") == "NONE"
                 and manifest.get("asset_acquisition") == "NONE",
                 "Generated manifest reports external access/acquisition", errors)
        manifest_summary = {
            "path": MANIFEST_PATH.relative_to(PROJECT_ROOT).as_posix(),
            "sha256": _sha256(MANIFEST_PATH),
            "rights_status": manifest.get("rights_status"),
            "production_license_claim": manifest.get("production_license_claim"),
        }

    live_files = _live_files()
    live_token_hits: dict[str, list[str]] = {}
    for path in live_files:
        hits = _token_hits(path)
        if hits:
            relative = path.relative_to(PROJECT_ROOT).as_posix()
            live_token_hits[relative] = hits
            errors.append(f"Forbidden brand/vendor token in live Session 7 content: {relative}: {hits}")

    result = {
        "schema": "DiscGolfTour.Session7BrandLicenseAudit.v1",
        "status": "PASS" if not errors else "FAIL",
        "source_status": generated_state,
        "acceptance_ready": (
            not errors and generated_state == "GENERATED_SOURCE_AUDITED" and bool(live_files)),
        "content_status": spec.get("content_status"),
        "shipping_status": spec.get("shipping_status"),
        "brand_id": spec.get("brand_id"),
        "brand_policy": brands.get("default_policy"),
        "premium_logo_status": premium_brand.get("logo_asset_status") if premium_brand else None,
        "modern_clothes_purchase_status": modern_purchase.get("status") if modern_purchase else None,
        "modern_clothes_registry_status": modern_registry.get("status") if modern_registry else None,
        "metahuman_integration_status": "NOT_STARTED_SESSION8_BOUNDARY",
        "external_sources": spec.get("external_sources"),
        "logos": spec.get("logos"),
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
        print("SESSION 7 BRAND/LICENSE AUDIT FAIL")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(
        "SESSION 7 BRAND/LICENSE AUDIT PASS: "
        f"brand=dg_generic vendor=NOT_ACQUIRED source={generated_state}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
