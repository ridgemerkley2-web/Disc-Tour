#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfEnvironmentTypes.h"
#include "DiscGolfEnvironmentController.generated.h"

class UBoxComponent;
class UPCGComponent;
class UDiscGolfForestPreset;
class AWindDirector;
class ADiscGolfEnvironmentZoneActor;

/** Owns one course-scale PCG forest and resolves designer zones into biome density. */
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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    TObjectPtr<UPCGComponent> PCGComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    TSoftObjectPtr<UDiscGolfForestPreset> ForestPreset;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    EDiscGolfEnvironmentQuality Quality = EDiscGolfEnvironmentQuality::High;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    FVector CourseExtentCm = FVector(90000.0f, 60000.0f, 10000.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    bool bSynchronizeDiscFlightWind = true;

    UFUNCTION(BlueprintCallable, CallInEditor, Category="Environment")
    void GenerateForest();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Environment")
    void CleanupForest();
    UFUNCTION(BlueprintCallable, Category="Environment")
    void ApplyPreset();
    UFUNCTION(BlueprintCallable, Category="Environment")
    void SynchronizeWindDirector(AWindDirector* WindDirector) const;
    UFUNCTION(BlueprintPure, Category="Environment")
    float EvaluateDensity(
        const FVector& WorldLocation,
        EDiscGolfEnvironmentAssetCategory Category) const;
    /** PCG batch path: reuses one preset load and one zone query for every sampled point. */
    float EvaluateDensityFromZones(
        const FVector& WorldLocation,
        EDiscGolfEnvironmentAssetCategory Category,
        const UDiscGolfForestPreset* Preset,
        const TArray<const ADiscGolfEnvironmentZoneActor*>& Zones) const;
    UFUNCTION(BlueprintPure, Category="Environment")
    bool HasProductionConfiguration() const;
};
