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
    static const FPrimaryAssetType PrimaryAssetType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    FName ManifestId = TEXT("dg_runtime_v1");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    FString SourceSpecRelativePath = TEXT("Config/DG_RuntimeCookManifest.json");

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    int32 ExpectedExcludedPackageCount = 12;

    /** Soft package dependencies included by the Runtime asset bundle. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook", meta=(AssetBundles="Runtime"))
    TArray<TSoftObjectPtr<UObject>> RuntimeAssets;

    /** Stable visual-backend entry points; initially only the DGMaster fallback. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook", meta=(AssetBundles="Runtime"))
    TArray<TSoftObjectPtr<UDiscGolfAvatarBackendProfile>> AvatarBackendProfiles;

    /** Strings by design: exclusions must not become soft cook dependencies. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cook")
    TArray<FString> ExplicitlyExcludedPackages;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Cook")
    TArray<FSoftObjectPath> GetRuntimeAssetPaths() const;

    /** C++/test guard used by strict validators after the asset is authored. */
    bool ValidateRuntimeContract(FString& OutError) const;
};
