#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfEnvironmentTypes.h"
#include "DiscGolfEnvironmentZoneActor.generated.h"

class USceneComponent;
class USplineComponent;
class UBoxComponent;

/**
 * Designer-authored vegetation zone/exclusion. Fairways use a spline corridor;
 * tees and greens normally use radial zones; semi/deep rough and OB use boxes.
 */
UCLASS(BlueprintType)
class DISCGOLFTOUR_API ADiscGolfEnvironmentZoneActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfEnvironmentZoneActor();
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment Zone")
    TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment Zone")
    TObjectPtr<USplineComponent> Spline;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment Zone")
    TObjectPtr<UBoxComponent> BoxPreview;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone")
    FName ZoneId = TEXT("EnvironmentZone");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone")
    EDiscGolfEnvironmentZoneType ZoneType = EDiscGolfEnvironmentZoneType::Fairway;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone")
    EDiscGolfEnvironmentZoneShape Shape = EDiscGolfEnvironmentZoneShape::SplineCorridor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone")
    int32 Priority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone")
    bool bHardExclusion = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone|Dimensions", meta=(ClampMin="200.0"))
    float WidthCm = 1200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone|Dimensions", meta=(ClampMin="100.0"))
    float RadiusCm = 1000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone|Dimensions")
    FVector BoxExtentCm = FVector(3000.0f, 3000.0f, 1500.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone|Dimensions", meta=(ClampMin="0.0"))
    float TreeSetbackCm = 550.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone|Dimensions", meta=(ClampMin="0.0"))
    float BrushSetbackCm = 250.0f;
    /** Density feathers across this outer band; collision/exclusion logic remains deterministic. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment Zone|Dimensions", meta=(ClampMin="0.0"))
    float BlendFalloffCm = 0.0f;

    UFUNCTION(BlueprintPure, Category="Environment Zone")
    bool ContainsForCategory(
        const FVector& WorldLocation,
        EDiscGolfEnvironmentAssetCategory Category) const;

    UFUNCTION(BlueprintPure, Category="Environment Zone")
    float GetInfluenceForCategory(
        const FVector& WorldLocation,
        EDiscGolfEnvironmentAssetCategory Category) const;

    UFUNCTION(BlueprintPure, Category="Environment Zone")
    float GetDistanceToSpline2D(const FVector& WorldLocation) const;
};
