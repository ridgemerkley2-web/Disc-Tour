#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfEnvironmentTypes.h"
#include "DiscGolfEnvironmentDataAssets.generated.h"

/** Asset-path abstraction consumed by editor authoring. External paths never enter runtime logic. */
UCLASS(BlueprintType)
class DISCGOLFTOUR_API UDiscGolfEnvironmentAssetSet : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UDiscGolfEnvironmentAssetSet();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment Assets")
    FName AssetSetId = TEXT("UnassignedForestAssets");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment Assets")
    TArray<FDiscGolfEnvironmentAssetSlot> Slots;

    UFUNCTION(BlueprintPure, Category="Environment Assets")
    bool HasPopulatedSlot(EDiscGolfEnvironmentAssetCategory Category) const;

    UFUNCTION(BlueprintPure, Category="Environment Assets")
    int32 GetPopulatedSlotCount() const;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

/** Reusable biome/course preset. The first authored instance is DA_TemperateMountainForest. */
UCLASS(BlueprintType)
class DISCGOLFTOUR_API UDiscGolfForestPreset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UDiscGolfForestPreset();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Forest")
    FName PresetId = TEXT("TemperateMountainForest");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Forest")
    TSoftObjectPtr<UDiscGolfEnvironmentAssetSet> AssetSet;

#if WITH_EDITORONLY_DATA
    /** Editor-only graph reference. It is stripped from every cooked runtime preset. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Authoring",
        meta=(AllowedClasses="/Script/PCG.PCGGraphInterface"))
    TSoftObjectPtr<UObject> ForestGraph;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course")
    FDiscGolfEnvironmentCourseClearance Clearance;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course")
    TArray<FDiscGolfEnvironmentZoneRules> ZoneRules;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Forest")
    TArray<FDiscGolfEnvironmentSpeciesWeight> SpeciesMix;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
    FDiscGolfEnvironmentMaterialSlots Materials;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind")
    FDiscGolfEnvironmentWindSettings Wind;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Interaction")
    FDiscGolfEnvironmentInteractionProfile LightCanopy;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Interaction")
    FDiscGolfEnvironmentInteractionProfile DenseCanopy;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Interaction")
    FDiscGolfEnvironmentInteractionProfile Shrub;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scalability")
    FDiscGolfEnvironmentQualitySettings Performance;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scalability")
    FDiscGolfEnvironmentQualitySettings High;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scalability")
    FDiscGolfEnvironmentQualitySettings Cinematic;

    UFUNCTION(BlueprintPure, Category="Environment")
    FDiscGolfEnvironmentQualitySettings ResolveQuality(EDiscGolfEnvironmentQuality Quality) const;

    UFUNCTION(BlueprintPure, Category="Environment")
    FDiscGolfEnvironmentZoneRules ResolveZoneRules(EDiscGolfEnvironmentZoneType ZoneType) const;

    UFUNCTION(BlueprintPure, Category="Environment")
    float ResolveSpeciesWeight(EDiscGolfEnvironmentAssetCategory Category) const;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
