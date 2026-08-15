#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfFlyoverRouteActor.generated.h"

class UCameraComponent;
class USplineComponent;
class APlayerController;
class ADiscGolfFlyoverRouteActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDiscGolfFlyoverFinished, ADiscGolfFlyoverRouteActor*, Route);

/** Authored preview route with a presentation-only camera. */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfFlyoverRouteActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfFlyoverRouteActor();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(BlueprintAssignable) FOnDiscGolfFlyoverFinished OnFlyoverFinished;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flyover") float PreviewDurationSeconds = 9.0f;

    void Configure(const TArray<FVector>& WorldPointsCm, const FVector& InLookAtWorldCm);
    bool StartPreview(APlayerController* PlayerController);
    void StopPreview();
    bool IsPreviewing() const { return bPreviewing; }
    int32 GetPointCount() const;
    float GetRouteLengthCm() const;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USplineComponent> RouteSpline;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> PreviewCamera;
    UPROPERTY() TObjectPtr<APlayerController> PreviewController;
    FVector LookAtWorldCm = FVector::ZeroVector;
    float PreviewElapsedSeconds = 0.0f;
    bool bPreviewing = false;
};
