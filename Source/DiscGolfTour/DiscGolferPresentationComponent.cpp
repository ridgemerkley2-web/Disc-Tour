#include "DiscGolferPresentationComponent.h"

EGolferAnimationFamily DiscGolferPresentation::ResolveFamily(
    EDiscShotContext ShotContext,
    float DistanceToBasketMeters)
{
    if (ShotContext != EDiscShotContext::Drive) return EGolferAnimationFamily::Putt;
    return FMath::IsFinite(DistanceToBasketMeters) && DistanceToBasketMeters <= 55.0f
        ? EGolferAnimationFamily::Approach
        : EGolferAnimationFamily::Drive;
}

float DiscGolferPresentation::ReleaseDurationSeconds(EGolferAnimationFamily Family)
{
    switch (Family)
    {
        case EGolferAnimationFamily::Approach: return 0.18f;
        case EGolferAnimationFamily::Putt: return 0.16f;
        default: return 0.22f;
    }
}

float DiscGolferPresentation::FollowThroughDurationSeconds(EGolferAnimationFamily Family)
{
    switch (Family)
    {
        case EGolferAnimationFamily::Approach: return 0.65f;
        case EGolferAnimationFamily::Putt: return 0.50f;
        default: return 0.85f;
    }
}

FString DiscGolferPresentation::FamilyName(EGolferAnimationFamily Family)
{
    switch (Family)
    {
        case EGolferAnimationFamily::Approach: return TEXT("APPROACH");
        case EGolferAnimationFamily::Putt: return TEXT("PUTT");
        default: return TEXT("DRIVE");
    }
}

FString DiscGolferPresentation::PhaseName(EGolferAnimationPhase Phase)
{
    switch (Phase)
    {
        case EGolferAnimationPhase::Windup: return TEXT("WINDUP");
        case EGolferAnimationPhase::Release: return TEXT("RELEASE");
        case EGolferAnimationPhase::FollowThrough: return TEXT("FOLLOW THROUGH");
        default: return TEXT("SETUP");
    }
}

UDiscGolferPresentationComponent::UDiscGolferPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDiscGolferPresentationComponent::SetShotContext(
    EDiscShotContext ShotContext,
    float DistanceToBasketMeters)
{
    AnimationFamily = DiscGolferPresentation::ResolveFamily(ShotContext, DistanceToBasketMeters);
    AnimationPhase = EGolferAnimationPhase::Setup;
    PhaseElapsedSeconds = 0.0f;
    PhaseNormalizedTime = 0.0f;
    SetComponentTickEnabled(false);
}

void UDiscGolferPresentationComponent::BeginTiming(EThrowStyle ThrowStyle)
{
    PresentationThrowStyle = ThrowStyle;
    AnimationPhase = EGolferAnimationPhase::Windup;
    PhaseElapsedSeconds = 0.0f;
    PhaseNormalizedTime = 0.0f;
    SetComponentTickEnabled(true);
}

void UDiscGolferPresentationComponent::CancelTiming()
{
    AnimationPhase = EGolferAnimationPhase::Setup;
    PhaseElapsedSeconds = 0.0f;
    PhaseNormalizedTime = 0.0f;
    SetComponentTickEnabled(false);
}

void UDiscGolferPresentationComponent::CommitAuthoritativeRelease(const FThrowRelease& Release)
{
    if (Release.ShotContext != EDiscShotContext::Drive)
    {
        AnimationFamily = EGolferAnimationFamily::Putt;
    }
    else if (AnimationFamily == EGolferAnimationFamily::Putt)
    {
        AnimationFamily = EGolferAnimationFamily::Drive;
    }
    PresentationThrowStyle = Release.ThrowStyle;
    PresentationReleaseGrade = Release.Grade;
    PresentationReleaseTiming = Release.Timing;
    AnimationPhase = EGolferAnimationPhase::Release;
    PhaseElapsedSeconds = 0.0f;
    PhaseNormalizedTime = 0.0f;
    SetComponentTickEnabled(true);
}

void UDiscGolferPresentationComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvancePresentation(DeltaTime);
}

void UDiscGolferPresentationComponent::AdvancePresentation(float DeltaTime)
{
    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f) return;

    PhaseElapsedSeconds += DeltaTime;
    if (AnimationPhase == EGolferAnimationPhase::Windup)
    {
        const float WindupLoopSeconds = AnimationFamily == EGolferAnimationFamily::Drive
            ? 0.90f : AnimationFamily == EGolferAnimationFamily::Approach ? 0.75f : 0.65f;
        PhaseNormalizedTime = FMath::Fmod(PhaseElapsedSeconds / WindupLoopSeconds, 1.0f);
        return;
    }
    if (AnimationPhase == EGolferAnimationPhase::Release)
    {
        const float Duration = DiscGolferPresentation::ReleaseDurationSeconds(AnimationFamily);
        PhaseNormalizedTime = FMath::Clamp(PhaseElapsedSeconds / Duration, 0.0f, 1.0f);
        if (PhaseElapsedSeconds >= Duration)
        {
            AnimationPhase = EGolferAnimationPhase::FollowThrough;
            PhaseElapsedSeconds = 0.0f;
            PhaseNormalizedTime = 0.0f;
        }
        return;
    }
    if (AnimationPhase == EGolferAnimationPhase::FollowThrough)
    {
        const float Duration = DiscGolferPresentation::FollowThroughDurationSeconds(AnimationFamily);
        PhaseNormalizedTime = FMath::Clamp(PhaseElapsedSeconds / Duration, 0.0f, 1.0f);
        if (PhaseElapsedSeconds >= Duration) CancelTiming();
    }
}

FString UDiscGolferPresentationComponent::GetStatusText() const
{
    return FString::Printf(TEXT("%s // %s %s"),
        bSkeletalAssetsReady ? TEXT("SKELETAL") : TEXT("PLACEHOLDER"),
        *DiscGolferPresentation::FamilyName(AnimationFamily),
        *DiscGolferPresentation::PhaseName(AnimationPhase));
}
