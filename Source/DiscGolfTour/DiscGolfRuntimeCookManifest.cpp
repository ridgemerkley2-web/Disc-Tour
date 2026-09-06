#include "DiscGolfRuntimeCookManifest.h"

#include "Misc/SecureHash.h"

namespace
{
#if DG_WITH_DEVELOPMENT_CONTENT
constexpr const TCHAR* ExpectedMetaHumanPackageListSha256 =
    TEXT("003A7683069A2353819421CD71ECA82A2A0F8AE2E7786F56E04EA18DB11AB3B9");
constexpr const TCHAR* ExpectedMetaHumanPackageListMd5 =
    TEXT("F4E2693BB42AC260A33C7952E5655F57");
constexpr const TCHAR* DGMasterProfilePackage =
    TEXT("/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_DGMaster");
constexpr const TCHAR* MetaHumanProfilePackage =
    TEXT("/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default");
constexpr const TCHAR* MetaHumanWrapperPackage =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default");
constexpr const TCHAR* MetaHumanRetargetPackage =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman");
constexpr const TCHAR* MetaHumanTargetIKPackage =
    TEXT("/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig");
constexpr const TCHAR* MetaHumanSourcePackage =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default");

FString GetSortedPackageFingerprint(
    const TArray<TSoftObjectPtr<UObject>>& Assets)
{
    TArray<FString> Packages;
    Packages.Reserve(Assets.Num());
    for (const TSoftObjectPtr<UObject>& Asset : Assets)
    {
        Packages.Add(Asset.ToSoftObjectPath().GetLongPackageName());
    }
    Packages.Sort([](const FString& Left, const FString& Right)
    {
        return Left.Compare(Right, ESearchCase::CaseSensitive) < 0;
    });
    FString Payload;
    for (const FString& Package : Packages)
    {
        Payload += Package;
        Payload += TEXT("\n");
    }
    // Package names are ASCII. The source/editor validator separately freezes
    // the SHA-256; this always-available runtime fingerprint catches any
    // same-count/path substitution without an experimental crypto dependency.
    return FMD5::HashAnsiString(*Payload).ToUpper();
}
#endif
}

const FPrimaryAssetType UDiscGolfRuntimeCookManifest::PrimaryAssetType(
    TEXT("DGRuntimeCookManifest"));

UDiscGolfRuntimeCookManifest::UDiscGolfRuntimeCookManifest()
{
#if DG_WITH_DEVELOPMENT_CONTENT
    SourceSpecRelativePath = TEXT("Config/DG_RuntimeCookManifest.json");
#endif
}

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

TArray<FSoftObjectPath>
UDiscGolfRuntimeCookManifest::GetMetaHumanRuntimeAssetPaths() const
{
    TArray<FSoftObjectPath> Paths;
    Paths.Reserve(MetaHumanRuntimeAssets.Num());
    for (const TSoftObjectPtr<UObject>& Asset : MetaHumanRuntimeAssets)
    {
        Paths.Add(Asset.ToSoftObjectPath());
    }
    return Paths;
}

bool UDiscGolfRuntimeCookManifest::ValidateRuntimeContract(FString& OutError) const
{
#if !DG_WITH_DEVELOPMENT_CONTENT
    OutError = TEXT("Runtime cook-manifest validation is unavailable in Shipping.");
    return false;
#else
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
    if (MetaHumanRuntimeAssets.Num() != ExpectedMetaHumanRuntimePackageCount
        || ExpectedMetaHumanRuntimePackageCount != 264)
    {
        OutError = TEXT(
            "Runtime cook manifest must contain exactly 264 separately counted MetaHuman runtime assets.");
        return false;
    }
    if (ExplicitlyExcludedPackages.Num() != ExpectedExcludedPackageCount
        || ExpectedExcludedPackageCount != 12)
    {
        OutError = TEXT("Runtime cook manifest must name exactly 12 excluded packages.");
        return false;
    }
    if (ExplicitlyExcludedMetaHumanPackages.Num()
            != ExpectedExcludedMetaHumanPackageCount
        || ExpectedExcludedMetaHumanPackageCount != 1
        || ExplicitlyExcludedMetaHumanPackages[0] != MetaHumanSourcePackage)
    {
        OutError = TEXT(
            "Runtime cook manifest must name the exact editor-only source MHC exclusion.");
        return false;
    }
    if (AvatarBackendProfiles.Num() != ExpectedAvatarBackendProfileCount
        || ExpectedAvatarBackendProfileCount != 2
        || AvatarBackendProfiles[0].ToSoftObjectPath().GetLongPackageName()
            != DGMasterProfilePackage
        || AvatarBackendProfiles[1].ToSoftObjectPath().GetLongPackageName()
            != MetaHumanProfilePackage)
    {
        OutError = TEXT(
            "Backend cook entries must be exactly DGMaster then the assembled MetaHuman profile.");
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

    TSet<FString> MetaHumanSet;
    int32 CommonPackageCount = 0;
    int32 GeneratedPackageCount = 0;
    bool bHasWrapper = false;
    bool bHasRetargeter = false;
    bool bHasTargetIK = false;
    for (const TSoftObjectPtr<UObject>& Asset : MetaHumanRuntimeAssets)
    {
        const FSoftObjectPath Path = Asset.ToSoftObjectPath();
        const FString PackageName = Path.GetLongPackageName();
        const bool bCommon = PackageName.StartsWith(
            TEXT("/Game/DiscGolf/Characters/MetaHuman/Common/"));
        const bool bGenerated = PackageName.StartsWith(
            TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/"));
        const bool bWrapper = PackageName == MetaHumanWrapperPackage;
        const bool bRetargeter = PackageName == MetaHumanRetargetPackage;
        const bool bTargetIK = PackageName == MetaHumanTargetIKPackage;
        if (Path.IsNull() || MetaHumanSet.Contains(PackageName)
            || RuntimeSet.Contains(PackageName) || ExcludedSet.Contains(PackageName)
            || PackageName == MetaHumanSourcePackage
            || (!bCommon && !bGenerated && !bWrapper && !bRetargeter && !bTargetIK)
            || PackageName.Contains(TEXT("/Source/"), ESearchCase::IgnoreCase)
            || PackageName.Contains(TEXT("Beard"), ESearchCase::IgnoreCase)
            || PackageName.Contains(TEXT("Mustache"), ESearchCase::IgnoreCase)
            || PackageName.Contains(TEXT("Stubble"), ESearchCase::IgnoreCase))
        {
            OutError = TEXT(
                "MetaHuman runtime assets must match the accepted assembly/wrapper/retarget/target-IK boundary and exclude authoring/facial-hair packages.");
            return false;
        }
        CommonPackageCount += bCommon ? 1 : 0;
        GeneratedPackageCount += bGenerated ? 1 : 0;
        bHasWrapper |= bWrapper;
        bHasRetargeter |= bRetargeter;
        bHasTargetIK |= bTargetIK;
        MetaHumanSet.Add(PackageName);
    }
    if (CommonPackageCount != 205 || GeneratedPackageCount != 56
        || !bHasWrapper || !bHasRetargeter || !bHasTargetIK
        || MetaHumanRuntimePackageListSha256
            != ExpectedMetaHumanPackageListSha256
        || GetSortedPackageFingerprint(MetaHumanRuntimeAssets)
            != ExpectedMetaHumanPackageListMd5)
    {
        OutError = TEXT(
            "The separately counted MetaHuman runtime package inventory/count/hash differs from the frozen contract.");
        return false;
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
#endif
}
