#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfShotTracerComponent.generated.h"

class USplineComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGOnTracerUpdated,
    TArray<FVector>,
    WorldPoints
);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfShotTracerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfShotTracerComponent();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Tracer")
    float MinimumPointDistanceCm = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Tracer")
    int32 MaximumPoints = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Tracer")
    FLinearColor TracerColor = FLinearColor(0.65f, 0.82f, 0.20f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Tracer")
    bool bVisible = true;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Tracer")
    TArray<FVector> WorldPoints;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Tracer")
    FDGOnTracerUpdated OnTracerUpdated;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Tracer")
    void BeginTracer(FVector ReleaseLocationCm);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Tracer")
    bool AddTracerPoint(FVector WorldLocationCm);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Tracer")
    void EndTracer();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Tracer")
    void ClearTracer();

    UFUNCTION(BlueprintPure, Category="Disc Golf|Tracer")
    USplineComponent* GetTracerSpline() const { return TracerSpline; }

    // Use Niagara, spline meshes, or another project-specific visual.
    UFUNCTION(BlueprintImplementableEvent, Category="Disc Golf|Tracer")
    void RebuildTracerVisual(
        const TArray<FVector>& Points,
        FLinearColor Color,
        bool bShouldBeVisible
    );

private:
    UPROPERTY(Transient)
    TObjectPtr<USplineComponent> TracerSpline;

    void EnsureSpline();
};
