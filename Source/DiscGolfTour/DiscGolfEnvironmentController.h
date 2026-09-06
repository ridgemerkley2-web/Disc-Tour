#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfEnvironmentTypes.h"
#include "DiscGolfEnvironmentController.generated.h"

class UBoxComponent;
class UDiscGolfForestPreset;
class AWindDirector;
class ADiscGolfEnvironmentZoneActor;

/** Resolves designer zones, environment quality, and optional shared wind data. */
UCLASS(BlueprintType)
class DISCGOLFTOUR_API ADiscGolfEnvironmentController : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfEnvironmentController();
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    TObjectPtr<UBoxComponent> GenerationBounds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    TSoftObjectPtr<UDiscGolfForestPreset> ForestPreset;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    EDiscGolfEnvironmentQuality Quality = EDiscGolfEnvironmentQuality::High;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    FVector CourseExtentCm = FVector(90000.0f, 60000.0f, 10000.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    bool bSynchronizeDiscFlightWind = false;

    UFUNCTION(BlueprintCallable, Category="Environment")
    bool SynchronizeWindDirector(AWindDirector* WindDirector) const;
    UFUNCTION(BlueprintPure, Category="Environment")
    float EvaluateDensity(
        const FVector& WorldLocation,
        EDiscGolfEnvironmentAssetCategory Category) const;
    /** Authoring batch path: reuses one preset load and one zone query for sampled points. */
    float EvaluateDensityFromZones(
        const FVector& WorldLocation,
        EDiscGolfEnvironmentAssetCategory Category,
        const UDiscGolfForestPreset* Preset,
        const TArray<const ADiscGolfEnvironmentZoneActor*>& Zones) const;
    UFUNCTION(BlueprintPure, Category="Environment")
    bool HasProductionConfiguration() const;

    /** Pure authoring-completeness predicate kept asset-load-free for contract validation. */
    static bool IsProductionConfigurationComplete(
        int32 BindingSlotCount,
        int32 PopulatedSlotCount,
        bool bHasAuthoringGraph);
};
