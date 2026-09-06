#pragma once

#include "CoreMinimal.h"
#include "DiscGolfAvatarBackendProfile.h"
#include "Engine/DataAsset.h"
#include "DiscGolfRuntimeCookManifest.generated.h"

/**
 * Explicit cook root for the accepted project runtime closure.
 *
 * This asset owns discovery only. It never owns player, throw, release, save,
 * inventory, flight, or visual-backend selection authority.
 */
UCLASS(BlueprintType)
class DISCGOLFTOUR_API UDiscGolfRuntimeCookManifest : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UDiscGolfRuntimeCookManifest();

    static const FPrimaryAssetType PrimaryAssetType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    FName ManifestId = TEXT("dg_runtime_v1");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    FString SourceSpecRelativePath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    FString SourceSpecSha256;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    FString ContentStatus =
        TEXT("ACCEPTED_TECHNICAL_PIPELINE_WITH_NON_PRODUCTION_PROXY_VISUALS");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    FString ShippingStatus = TEXT("DO_NOT_CLAIM_SHIPPING_ART_APPROVAL");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    FString Authority = TEXT("COOK_DISCOVERY_ONLY_NO_GAMEPLAY_OR_RELEASE_AUTHORITY");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    int32 ExpectedRuntimePackageCount = 69;

    /** Accepted Sessions 8B MetaHuman closure; never folded into the legacy 69. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    int32 ExpectedMetaHumanRuntimePackageCount = 264;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    int32 ExpectedAvatarBackendProfileCount = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    int32 ExpectedExcludedPackageCount = 12;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    int32 ExpectedExcludedMetaHumanPackageCount = 1;

    /** SHA-256 of sorted MetaHuman package names, each followed by a newline. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    FString MetaHumanRuntimePackageListSha256;

    /** Soft package dependencies included by the Runtime asset bundle. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook", meta=(AssetBundles="Runtime"))
    TArray<TSoftObjectPtr<UObject>> RuntimeAssets;

    /** Separate exact MetaHuman package closure: assembly, wrapper, retarget and target IK. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook", meta=(AssetBundles="Runtime"))
    TArray<TSoftObjectPtr<UObject>> MetaHumanRuntimeAssets;

    /** Exact stable visual-backend entry points: DGMaster fallback then MetaHuman. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook", meta=(AssetBundles="Runtime"))
    TArray<TSoftObjectPtr<UDiscGolfAvatarBackendProfile>> AvatarBackendProfiles;

    /** Strings by design: exclusions must not become soft cook dependencies. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    TArray<FString> ExplicitlyExcludedPackages;

    /** Editor-only source MHC packages; strings so they never become cook roots. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    TArray<FString> ExplicitlyExcludedMetaHumanPackages;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Cook")
    TArray<FSoftObjectPath> GetRuntimeAssetPaths() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Cook")
    TArray<FSoftObjectPath> GetMetaHumanRuntimeAssetPaths() const;

    /** C++/test guard used by strict validators after the asset is authored. */
    bool ValidateRuntimeContract(FString& OutError) const;
};
