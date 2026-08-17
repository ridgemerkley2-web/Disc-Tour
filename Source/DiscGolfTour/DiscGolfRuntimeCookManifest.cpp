#include "DiscGolfRuntimeCookManifest.h"

const FPrimaryAssetType UDiscGolfRuntimeCookManifest::PrimaryAssetType(
    TEXT("DGRuntimeCookManifest"));

FPrimaryAssetId UDiscGolfRuntimeCookManifest::GetPrimaryAssetId() const
{
    return ManifestId.IsNone()
        ? FPrimaryAssetId()
        : FPrimaryAssetId(PrimaryAssetType, ManifestId);
}

TArray<FSoftObjectPath> UDiscGolfRuntimeCookManifest::GetRuntimeAssetPaths() const
{
    TArray<FSoftObjectPath> Paths;
    Paths.Reserve(RuntimeAssets.Num());
    for (const TSoftObjectPtr<UObject>& Asset : RuntimeAssets)
    {
        Paths.Add(Asset.ToSoftObjectPath());
    }
    return Paths;
}

bool UDiscGolfRuntimeCookManifest::ValidateRuntimeContract(FString& OutError) const
{
    OutError.Reset();
    if (ManifestId != FName(TEXT("dg_runtime_v1")))
    {
        OutError = TEXT("Runtime cook manifest ID is not canonical.");
        return false;
    }
    if (RuntimeAssets.Num() != ExpectedRuntimePackageCount
        || ExpectedRuntimePackageCount != 69)
    {
        OutError = TEXT("Runtime cook manifest must contain exactly 69 runtime assets.");
        return false;
    }
    if (ExplicitlyExcludedPackages.Num() != ExpectedExcludedPackageCount
        || ExpectedExcludedPackageCount != 12)
    {
        OutError = TEXT("Runtime cook manifest must name exactly 12 excluded packages.");
        return false;
    }
    if (AvatarBackendProfiles.Num() != 1
        || AvatarBackendProfiles[0].ToSoftObjectPath().GetLongPackageName()
            != TEXT("/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_DGMaster"))
    {
        OutError = TEXT("The only initial backend cook entry must be the DGMaster profile.");
        return false;
    }

    TSet<FString> ExcludedSet;
    for (const FString& ExcludedPackage : ExplicitlyExcludedPackages)
    {
        if (!ExcludedPackage.StartsWith(TEXT("/Game/DiscGolf/"))
            || ExcludedSet.Contains(ExcludedPackage))
        {
            OutError = TEXT("Excluded package paths must be unique /Game/DiscGolf packages.");
            return false;
        }
        ExcludedSet.Add(ExcludedPackage);
    }

    TSet<FString> RuntimeSet;
    for (const TSoftObjectPtr<UObject>& Asset : RuntimeAssets)
    {
        const FSoftObjectPath Path = Asset.ToSoftObjectPath();
        const FString PackageName = Path.GetLongPackageName();
        if (Path.IsNull() || !PackageName.StartsWith(TEXT("/Game/DiscGolf/"))
            || RuntimeSet.Contains(PackageName) || ExcludedSet.Contains(PackageName))
        {
            OutError = TEXT(
                "Runtime assets must be unique, non-null, in /Game/DiscGolf, and not excluded.");
            return false;
        }
        RuntimeSet.Add(PackageName);
    }

    if (SourceSpecRelativePath != TEXT("Config/DG_RuntimeCookManifest.json")
        || SourceSpecSha256.Len() != 64
        || ContentStatus
            != TEXT("ACCEPTED_TECHNICAL_PIPELINE_WITH_NON_PRODUCTION_PROXY_VISUALS")
        || ShippingStatus != TEXT("DO_NOT_CLAIM_SHIPPING_ART_APPROVAL")
        || Authority != TEXT("COOK_DISCOVERY_ONLY_NO_GAMEPLAY_OR_RELEASE_AUTHORITY"))
    {
        OutError = TEXT("Runtime cook manifest provenance/authority fields differ.");
        return false;
    }
    return true;
}
