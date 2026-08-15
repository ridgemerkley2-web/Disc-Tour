#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfTypes.h"
#include "DiscGolferPresentationComponent.generated.h"

UENUM(BlueprintType)
enum class EGolferAnimationFamily : uint8
{
    Drive,
    Approach,
    Putt
};

UENUM(BlueprintType)
enum class EGolferAnimationPhase : uint8
{
    Setup,
    Windup,
    Release,
    FollowThrough
};

namespace DiscGolferPresentation
{
    DISCGOLFTOUR_API EGolferAnimationFamily ResolveFamily(
        EDiscShotContext ShotContext,
        float DistanceToBasketMeters);
    DISCGOLFTOUR_API float ReleaseDurationSeconds(EGolferAnimationFamily Family);
    DISCGOLFTOUR_API float FollowThroughDurationSeconds(EGolferAnimationFamily Family);
    DISCGOLFTOUR_API FString FamilyName(EGolferAnimationFamily Family);
    DISCGOLFTOUR_API FString PhaseName(EGolferAnimationPhase Phase);
}

/** Animation-facing state only. Authoritative release happens before CommitAuthoritativeRelease is called. */
UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOUR_API UDiscGolferPresentationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolferPresentationComponent();
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    void SetShotContext(EDiscShotContext ShotContext, float DistanceToBasketMeters);
    void BeginTiming(EThrowStyle ThrowStyle);
    void CancelTiming();
    void CommitAuthoritativeRelease(const FThrowRelease& Release);
    /** Advances animation-only state without requiring component registration. */
    void AdvancePresentation(float DeltaTime);
    void SetSkeletalAssetsReady(bool bReady) { bSkeletalAssetsReady = bReady; }

    UFUNCTION(BlueprintPure) EGolferAnimationFamily GetAnimationFamily() const { return AnimationFamily; }
    UFUNCTION(BlueprintPure) EGolferAnimationPhase GetAnimationPhase() const { return AnimationPhase; }
    UFUNCTION(BlueprintPure) EThrowStyle GetPresentationThrowStyle() const { return PresentationThrowStyle; }
    UFUNCTION(BlueprintPure) EReleaseGrade GetPresentationReleaseGrade() const { return PresentationReleaseGrade; }
    UFUNCTION(BlueprintPure) EReleaseTiming GetPresentationReleaseTiming() const { return PresentationReleaseTiming; }
    UFUNCTION(BlueprintPure) float GetPhaseNormalizedTime() const { return PhaseNormalizedTime; }
    UFUNCTION(BlueprintPure) bool HasSkeletalAssets() const { return bSkeletalAssetsReady; }
    UFUNCTION(BlueprintPure) FString GetStatusText() const;

private:
    UPROPERTY(VisibleAnywhere, Category="Golfer Presentation")
    EGolferAnimationFamily AnimationFamily = EGolferAnimationFamily::Drive;
    UPROPERTY(VisibleAnywhere, Category="Golfer Presentation")
    EGolferAnimationPhase AnimationPhase = EGolferAnimationPhase::Setup;
    UPROPERTY(VisibleAnywhere, Category="Golfer Presentation")
    EThrowStyle PresentationThrowStyle = EThrowStyle::Backhand;
    UPROPERTY(VisibleAnywhere, Category="Golfer Presentation")
    EReleaseGrade PresentationReleaseGrade = EReleaseGrade::Perfect;
    UPROPERTY(VisibleAnywhere, Category="Golfer Presentation")
    EReleaseTiming PresentationReleaseTiming = EReleaseTiming::OnTime;
    UPROPERTY(VisibleAnywhere, Category="Golfer Presentation")
    float PhaseNormalizedTime = 0.0f;
    UPROPERTY(VisibleAnywhere, Category="Golfer Presentation")
    bool bSkeletalAssetsReady = false;

    float PhaseElapsedSeconds = 0.0f;
};
