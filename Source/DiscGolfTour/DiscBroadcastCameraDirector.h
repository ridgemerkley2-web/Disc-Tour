#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfBroadcastCameraMath.h"
#include "DiscBroadcastCameraDirector.generated.h"

class ADiscActor;
class ADiscGolfCourseFeatureActor;
class UCameraComponent;
class USceneComponent;

/** Presentation-only live-shot camera. It reads recorded samples and never writes gameplay state. */
UCLASS()
class DISCGOLFTOUR_API ADiscBroadcastCameraDirector : public AActor
{
    GENERATED_BODY()

public:
    ADiscBroadcastCameraDirector();
    virtual void Tick(float DeltaSeconds) override;

    bool InitializeForShot(
        ADiscActor* InDisc,
        const FVector& InReleaseLocationCm,
        const FVector& InBasketLocationCm,
        EDiscShotContext InShotContext);
    void StopTracking();

    bool IsTrackingShot() const { return TrackedDisc != nullptr; }
    EDiscBroadcastCameraMode GetCameraMode() const { return CameraMode; }
    FString GetStatusText() const;
    bool WasLineOfSightAdjusted() const { return bLineOfSightAdjusted; }

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<ADiscActor> TrackedDisc;

    FVector ReleaseLocationCm = FVector::ZeroVector;
    FVector BasketLocationCm = FVector::ZeroVector;
    EDiscShotContext ShotContext = EDiscShotContext::Drive;
    EDiscBroadcastCameraMode CameraMode = EDiscBroadcastCameraMode::Launch;
    bool bLineOfSightAdjusted = false;
    bool bUsingAuthoredAnchor = false;
    float CurrentFieldOfViewDeg = 68.0f;
    TArray<FBox> VisualExclusionBounds;
    UPROPERTY() TArray<TWeakObjectPtr<ADiscGolfCourseFeatureActor>> AuthoredCameraAnchors;

    bool BuildInputFromRecordedSamples(FDiscBroadcastCameraInput& OutInput) const;
    FVector ResolveLineOfSight(
        const FVector& DesiredLocationCm,
        const FVector& LookAtWorldCm,
        const FVector& ShotSide,
        bool& bOutAdjusted) const;
    bool IsInsideVisibleStaticGeometry(const FVector& LocationCm) const;
    void CacheVisibleStaticGeometry();
    void CacheAuthoredCameraAnchors();
    bool ApplyAuthoredAnchor(FDiscBroadcastCameraPlan& InOutPlan, EDiscBroadcastCameraMode Mode) const;
    void ApplyInitialPlan(const FDiscBroadcastCameraInput& Input);
};
